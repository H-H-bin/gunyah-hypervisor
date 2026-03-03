// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#if defined(ARCH_ARM_FEAT_SME) || defined(ARCH_ARM_FEAT_SVE)

#include <hypregisters.h>

#if defined(ARCH_ARM_FEAT_SME)
#include <arm_fgt.h>
#endif

#include <atomic.h>
#include <compiler.h>
#include <cpulocal.h>
#include <ipi.h>
#include <log.h>
#include <object.h>
#include <panic.h>
#include <partition.h>
#include <partition_alloc.h>
#include <partition_init.h>
#include <platform_features.h>
#include <preempt.h>
#include <qcbor.h>
#include <scheduler.h>
#include <thread.h>
#include <trace.h>
#include <util.h>
#include <vcpu.h>

#include <asm/barrier.h>
#include <asm/system_registers.h>
#include <asm/vfp_helpers.h>

#include "event_handlers.h"
#include "vfp_save_load.h"

// Design for SVE / SME for all VMs
//
// Context switching:
// The SVE register state and PSTATE.SM are always context switched normally.
// The only state that is lazily context switched is the SME-only state, which
// consists of PSTATE.ZA, ZA storage and SMCR_EL1.
//
// If the next thread has its PSTATE.SM == 1, then it is considered to be using
// SME and the SMEN trap will be disabled, which will also require the SME-only
// state to be eagerly context switched. Otherwise, if its PSTATE.SM == 0, the
// SMEN trap will be enabled.
//
// Since the SVE register state is context switched normally, threads which are
// allowed to use SVE are always considered to be using SVE and will therefore
// always have the ZEN trap disabled.
//
// FIXME: In the future, we can avoid having to context switch the entire SVE
// register state by inspecting the value of CPACR_EL1.
//
// Trap handling:
// The ZEN trap is only enabled on threads that aren't allowed to use SVE, so
// the only thing the trap handler does is kill the thread.
//
// The SMEN trap controls access to SME and therefore the SME-only state. If the
// thread is allowed to use SME then the SME-only state is context switched and
// the SMEN trap is disabled for the remainder of its timeslice.
//
// VL:
// The NSVL and SVL provided to the threads are configured at compile-time and
// cannot be changed.

#if defined(ARCH_ARM_FEAT_SVE)
static_assert(util_is_p2(PLATFORM_SVE_REG_SIZE) &&
		      (16U <= PLATFORM_SVE_REG_SIZE) &&
		      (PLATFORM_SVE_REG_SIZE <= 256U),
	      "sve: invalid SVE register size");
#endif

#if defined(ARCH_ARM_FEAT_SME)
static_assert(util_is_p2(PLATFORM_SME_REG_SIZE) &&
		      (16U <= PLATFORM_SME_REG_SIZE) &&
		      (PLATFORM_SME_REG_SIZE <= 256U),
	      "sme: invalid SME register size");
#endif

#if defined(ARCH_ARM_FEAT_SME)
CPULOCAL_DECLARE_STATIC(thread_t *, vfp_lazy_sme_owner);
#endif
static bool vfp_sve_disabled = true;
static bool vfp_sme_disabled = true;
#if defined(ARCH_ARM_FEAT_SME_FA64)
static bool vfp_sme_fa64_disabled = true;
static bool vfp_streaming_has_ffr = false;
#endif

#if defined(ARCH_ARM_FEAT_SVE)
bool
vfp_sve_implemented(void)
{
	ID_AA64PFR0_EL1_t pfr0 = register_ID_AA64PFR0_EL1_read();

	return ID_AA64PFR0_EL1_get_SVE(&pfr0) != 0U;
}

register_t
vfp_rdvl(asm_ordering_dummy_t *ordering_var)
{
	register_t vl;

	__asm__ volatile(".arch_extension sve;"
			 "rdvl %[vl], 1;"
			 ".arch_extension nosve;"
			 : [vl] "=r"(vl), "+m"(*ordering_var));

	return vl;
}
#endif

#if defined(ARCH_ARM_FEAT_SME)
bool
vfp_sme_implemented(void)
{
	ID_AA64PFR1_EL1_t pfr1 = register_ID_AA64PFR1_EL1_read();

	return ID_AA64PFR1_EL1_get_SME(&pfr1) != 0U;
}

register_t
vfp_rdsvl(asm_ordering_dummy_t *ordering_var)
{
	register_t vl;

	__asm__ volatile(".arch_extension sme;"
			 "rdsvl %[vl], 1;"
			 ".arch_extension nosme;"
			 : [vl] "=r"(vl), "+m"(*ordering_var));

	return vl;
}

static void
vfp_smstart_sm(asm_ordering_dummy_t *ordering_var)
{
	__asm__ volatile(".arch_extension sme;"
			 "smstart sm;"
			 ".arch_extension nosme;"
			 : "+m"(*ordering_var));
}

static void
vfp_smstop_sm(asm_ordering_dummy_t *ordering_var)
{
	__asm__ volatile(".arch_extension sme;"
			 "smstop sm;"
			 ".arch_extension nosme;"
			 : "+m"(*ordering_var));
}

static void
vfp_smstop_all(asm_ordering_dummy_t *ordering_var)
{
	__asm__ volatile(".arch_extension sme;"
			 "smstop;"
			 ".arch_extension nosme;"
			 : "+m"(*ordering_var));
}
#endif

#if defined(ARCH_ARM_FEAT_SVE)
static void
vfp_enable_sve_access(CPTR_EL2_E2H1_t *cptr)
{
	CPTR_EL2_E2H1_set_ZEN(cptr, CPTR_ZEN_TRAP_NONE);
}

static void
vfp_disable_sve_access(CPTR_EL2_E2H1_t *cptr)
{
	CPTR_EL2_E2H1_set_ZEN(cptr, CPTR_ZEN_TRAP_ALL);
}
#endif

#if defined(ARCH_ARM_FEAT_SME)
static void
vfp_enable_sme_access(CPTR_EL2_E2H1_t *cptr)
{
	CPTR_EL2_E2H1_set_SMEN(cptr, CPTR_SMEN_TRAP_NONE);
}

static void
vfp_disable_sme_access(CPTR_EL2_E2H1_t *cptr)
{
	CPTR_EL2_E2H1_set_SMEN(cptr, CPTR_SMEN_TRAP_ALL);
}

static void
vfp_enable_hw_sme_access(void)
{
	CPTR_EL2_E2H1_t cptr =
		register_CPTR_EL2_E2H1_read_ordered(&asm_ordering);

	vfp_enable_sme_access(&cptr);
	register_CPTR_EL2_E2H1_write_ordered(cptr, &asm_ordering);
	asm_context_sync_ordered(&asm_ordering);
}
#endif

CPTR_EL2_E2H1_t
vfp_enable_hw_access(asm_ordering_dummy_t *ordering_var)
{
	CPTR_EL2_E2H1_t cptr =
		register_CPTR_EL2_E2H1_read_ordered(ordering_var);
	CPTR_EL2_E2H1_t old_cptr = cptr;

#if defined(ARCH_ARM_FEAT_SVE)
	vfp_enable_sve_access(&cptr);
#endif
#if defined(ARCH_ARM_FEAT_SME)
	vfp_enable_sme_access(&cptr);
#endif
	register_CPTR_EL2_E2H1_write_ordered(cptr, ordering_var);
	asm_context_sync_ordered(ordering_var);

	return old_cptr;
}

void
vfp_restore_cptr(CPTR_EL2_E2H1_t cptr, asm_ordering_dummy_t *ordering_var)
{
	register_CPTR_EL2_E2H1_write_ordered(cptr, ordering_var);
	asm_context_sync_ordered(ordering_var);
}

#if defined(ARCH_ARM_FEAT_SVE)
static void
vfp_check_hardware_nsvl(void)
{
#if defined(VERBOSE) && VERBOSE
	ZCR_EL2_t zcr = ZCR_EL2_default();

	// For SVE, if a certain VL is supported, then all smaller VLs are
	// guaranteed to be supported as well, so we can find out all available
	// VLs by requesting the maximum.
	ZCR_EL2_set_LEN(&zcr, 0xFU);
	register_ZCR_EL2_write_ordered(zcr, &asm_ordering);
	register_t max_vl = vfp_rdvl(&asm_ordering);

	// On verbose builds, take the time to print the maximum and configured
	// VL.
	if (max_vl == PLATFORM_SVE_REG_SIZE) {
		TRACE_AND_LOG(DEBUG, INFO, "SVE: using VL = {:#x}",
			      PLATFORM_SVE_REG_SIZE);
	} else {
		TRACE_AND_LOG(ERROR, WARN,
			      "SVE: maximum VL = {:#x}, configured VL = {:#x}",
			      max_vl, PLATFORM_SVE_REG_SIZE);
	}
#endif
	// If the maximum VL happens to be less than the configured VL, the
	// arrays used to store the SVE register state will be larger than
	// necessary and will have incorrect values when inspected through a
	// debugger.
}
#endif

#if defined(ARCH_ARM_FEAT_SME)
static void
vfp_check_hardware_svl(void)
{
	SMCR_EL2_t smcr = SMCR_EL2_default();

	// For SME, there are no such guarantees so we have to query each VL
	// separately.
#if defined(VERBOSE) && VERBOSE
	// On verbose builds, take the time to check the maximum supported SVL.
	SMCR_EL2_set_LEN(&smcr, 0xFU);
	register_SMCR_EL2_write_ordered(smcr, &asm_ordering);
	register_t max_vl = vfp_rdsvl(&asm_ordering);

	if (max_vl == PLATFORM_SME_REG_SIZE) {
		TRACE_AND_LOG(DEBUG, INFO, "SME: maximum VL = {:#x}", max_vl);
	} else {
		TRACE_AND_LOG(ERROR, WARN,
			      "SME: maximum VL = {:#x}, configured VL = {:#x}",
			      max_vl, PLATFORM_SME_REG_SIZE);
	}
#endif

	// Request the configured VL, and check what VL we actually get.
	SMCR_EL2_set_LEN(&smcr, (PLATFORM_SME_REG_SIZE >> 4) - 1U);
	register_SMCR_EL2_write_ordered(smcr, &asm_ordering);
	register_t hw_svl = vfp_rdsvl(&asm_ordering);

	if (hw_svl != PLATFORM_SME_REG_SIZE) {
#if defined(VERBOSE) && VERBOSE
		TRACE_AND_LOG(ERROR, WARN,
			      "SME: actual VL = {:#x}, configured VL = {:#x}",
			      hw_svl, PLATFORM_SME_REG_SIZE);
#endif
		if (hw_svl > PLATFORM_SME_REG_SIZE) {
			panic("SME: hardware VL is greater than configured VL");
		}

		// If the actual VL happens to be less than the configured VL,
		// the arrays used to store the streaming SVE register state and
		// ZA storage will be larger than necessary and will have
		// incorrect values when inspected through a debugger.
#if defined(VERBOSE) && VERBOSE
	} else {
		TRACE_AND_LOG(DEBUG, INFO, "SME: using VL = {:#x}",
			      PLATFORM_SME_REG_SIZE);
#endif
	}
}
#endif

void
vfp_handle_boot_cold_init(void)
{
	platform_cpu_features_t features = platform_get_cpu_features();

#if defined(ARCH_ARM_FEAT_SVE)
	vfp_sve_disabled = !vfp_sve_implemented() ||
			   platform_cpu_features_get_sve_disable(&features);
#endif
#if defined(ARCH_ARM_FEAT_SME)
	// SME requires FEAT_HCX and FEAT_FGT as well.
	vfp_sme_disabled = !vfp_sme_implemented() ||
			   platform_cpu_features_get_sme_disable(&features) ||
			   platform_cpu_features_get_hcx_disable(&features) ||
			   !arm_fgt_is_allowed();

#if defined(ARCH_ARM_FEAT_SME_FA64)
	// SME_FA64 is only useful if we have SME support.
	vfp_sme_fa64_disabled =
		vfp_sme_disabled ||
		platform_cpu_features_get_sme_fa64_disable(&features);

	vfp_streaming_has_ffr = !vfp_sme_fa64_disabled;
#endif
#endif
}

void
vfp_handle_boot_cpu_cold_init(void)
{
#if defined(ARCH_ARM_FEAT_SME)
	CPULOCAL(vfp_lazy_sme_owner) = NULL;
#endif
}

#if defined(ARCH_ARM_FEAT_SVE)
static void
vfp_cpu_warm_init_sve(void)
{
	ZCR_EL2_t zcr = ZCR_EL2_default();

	// Initialise ZCR_EL2. ZCR_EL2 is not context switched and all threads
	// will get the same value.
	ZCR_EL2_set_LEN(&zcr, (PLATFORM_SVE_REG_SIZE >> 4) - 1U);
	register_ZCR_EL2_write_ordered(zcr, &asm_ordering);
}
#endif

#if defined(ARCH_ARM_FEAT_SME)
static void
vfp_cpu_warm_init_sme(void)
{
	SMCR_EL2_t smcr = SMCR_EL2_default();

	// Initialise SMCR_EL2. SMCR_EL2 is not context switched and all threads
	// will get the same value.
	SMCR_EL2_set_LEN(&smcr, (PLATFORM_SME_REG_SIZE >> 4) - 1U);
#if defined(ARCH_ARM_FEAT_SME_FA64)
	SMCR_EL2_set_FA64(&smcr, !vfp_sme_fa64_disabled);
#endif
	register_SMCR_EL2_write_ordered(smcr, &asm_ordering);

	// Reset SMPRI_EL1 to the default value. All threads which are allowed
	// to use SME will read this value.
	register_SMPRI_EL1_write(SMPRI_EL1_default());

	// Check whether streaming priority is implemented.
	SMIDR_EL1_t smidr = register_SMIDR_EL1_read();
	if (SMIDR_EL1_get_SMPS(&smidr)) {
		// Enable priority mapping.
		HCRX_EL2_t hcrx = register_HCRX_EL2_read();
		HCRX_EL2_set_SMPME(&hcrx, true);
		register_HCRX_EL2_write(hcrx);

		// Map all priorities to 0.
		register_SMPRIMAP_EL2_write(SMPRIMAP_EL2_default());
	}
}
#endif

void
vfp_handle_boot_cpu_warm_init(void)
{
	(void)vfp_enable_hw_access(&asm_ordering);

#if defined(ARCH_ARM_FEAT_SVE)
	if (!vfp_sve_disabled) {
		static atomic_bool nsvl_checked = false;
		if (!atomic_exchange_explicit(&nsvl_checked, true,
					      memory_order_relaxed)) {
			vfp_check_hardware_nsvl();
		}
		vfp_cpu_warm_init_sve();
	}
#endif
#if defined(ARCH_ARM_FEAT_SME)
	if (!vfp_sme_disabled) {
		static atomic_bool svl_checked = false;
		if (!atomic_exchange_explicit(&svl_checked, true,
					      memory_order_relaxed)) {
			vfp_check_hardware_svl();
		}
		vfp_cpu_warm_init_sme();
	}
#endif
}

void
vfp_handle_rootvm_init(qcbor_enc_ctxt_t *qcbor_enc_ctxt)
{
	assert(qcbor_enc_ctxt != NULL);

	QCBOREncode_AddBoolToMap(qcbor_enc_ctxt, "sve_supported",
				 !vfp_sve_disabled);
	QCBOREncode_AddBoolToMap(qcbor_enc_ctxt, "sme_supported",
				 !vfp_sme_disabled);
}

static void_ptr_result_t
vfp_alloc_zeroed_ctx(partition_t *partition, size_t size, size_t min_alignment)
{
	void_ptr_result_t ret = partition_alloc(partition, size, min_alignment);
	if (ret.e == OK) {
		(void)memset_s(ret.r, size, 0, size);
	}

	return ret;
}

static void
vfp_free_ctx(partition_t *partition, size_t size, void *ctx)
{
	if (ctx != NULL) {
		partition_free(partition, ctx, size);
	}
}

#if defined(ARCH_ARM_FEAT_SME)
static bool
vfp_alloc_za_storage(thread_t *thread, partition_t *partition)
{
	void_ptr_result_t result = vfp_alloc_zeroed_ctx(
		partition, PLATFORM_SME_REG_SIZE * PLATFORM_SME_REG_SIZE,
		PLATFORM_SME_REG_SIZE);
	if (result.e == OK) {
		thread->vfp.sme.za_storage = result.r;
	}

	return result.e == OK;
}

static void
vfp_free_za_storage(thread_t *thread, partition_t *partition)
{
	vfp_free_ctx(partition, PLATFORM_SME_REG_SIZE * PLATFORM_SME_REG_SIZE,
		     thread->vfp.sme.za_storage);
	thread->vfp.sme.za_storage = NULL;
}
#endif

static bool
vfp_alloc_vector_regs(thread_t *thread, partition_t *partition)
{
	count_t size = thread->vfp.max_vl;

	void_ptr_result_t result =
		vfp_alloc_zeroed_ctx(partition, size * 32U, size);
	if (result.e == OK) {
		thread->vfp.vector_regs = result.r;
	}

	return result.e == OK;
}

static void
vfp_free_vector_regs(thread_t *thread, partition_t *partition)
{
	count_t size = thread->vfp.max_vl;

	vfp_free_ctx(partition, size * 32U, thread->vfp.vector_regs);
	thread->vfp.vector_regs = NULL;
}

static bool
vfp_alloc_predicate_regs(thread_t *thread, partition_t *partition)
{
	count_t size = thread->vfp.max_vl >> 3;

	// Allocate enough space for the FFR register, even if it doesn't end up
	// being used.
	void_ptr_result_t result =
		vfp_alloc_zeroed_ctx(partition, size * 17U, size);
	if (result.e == OK) {
		thread->vfp.predicate_regs = result.r;
	}

	return result.e == OK;
}

static void
vfp_free_predicate_regs(thread_t *thread, partition_t *partition)
{
	count_t size = thread->vfp.max_vl >> 3;

	vfp_free_ctx(partition, size * 17U, thread->vfp.predicate_regs);
	thread->vfp.predicate_regs = NULL;
}

static bool
vfp_alloc_zpffr(thread_t *thread, partition_t *partition)
{
	bool success;
	bool alloc_pffr;

	if (thread->vfp.max_vl == 0U) {
		alloc_pffr	   = false;
		thread->vfp.max_vl = 16U; // Size of V register
	} else {
		alloc_pffr = true;
	}

	if (!vfp_alloc_vector_regs(thread, partition)) {
		success = false;
		goto out;
	}

	if (!alloc_pffr) {
		success = true;
		goto out;
	}

	if (!vfp_alloc_predicate_regs(thread, partition)) {
		vfp_free_vector_regs(thread, partition);
		success = false;
		goto out;
	}

	success = true;
out:
	return success;
}

static void
vfp_free_zpffr(thread_t *thread, partition_t *partition)
{
	vfp_free_vector_regs(thread, partition);
	vfp_free_predicate_regs(thread, partition);
}

bool
vfp_handle_vcpu_activate_thread(thread_t *thread, vcpu_option_flags_t options)
{
	bool	     success;
	partition_t *partition = thread->header.partition;

#if defined(ARCH_ARM_FEAT_SVE)
	if (!vfp_sve_disabled && vcpu_option_flags_get_sve_allowed(&options)) {
		vcpu_option_flags_set_sve_allowed(&thread->vcpu_options, true);
		vfp_enable_sve_access(&thread->vcpu_regs_el2.cptr_el2);
		thread->vfp.max_vl = PLATFORM_SVE_REG_SIZE;
	} else {
		vfp_disable_sve_access(&thread->vcpu_regs_el2.cptr_el2);
	}
#endif
#if defined(ARCH_ARM_FEAT_SME)
	// Enable TID1 traps for SMIDR_EL1.
	HCR_EL2_set_TID1(&thread->vcpu_regs_el2.hcr_el2, true);
	// Disable SME access by default.
	vfp_disable_sme_access(&thread->vcpu_regs_el2.cptr_el2);

	if (!vfp_sme_disabled && vcpu_option_flags_get_sme_allowed(&options)) {
		vcpu_option_flags_set_sme_allowed(&thread->vcpu_options, true);
		thread->vfp.max_vl =
			util_max(thread->vfp.max_vl, PLATFORM_SME_REG_SIZE);

		HFGRTR_EL2_set_nTPIDR2_EL0(&thread->vcpu_regs_el2.hfgrtr_el2,
					   true);
		HFGWTR_EL2_set_nTPIDR2_EL0(&thread->vcpu_regs_el2.hfgwtr_el2,
					   true);
		// Disable the read trap for SMPRI_EL1 but leave the write trap
		// enabled.
		HFGRTR_EL2_set_nSMPRI_EL1(&thread->vcpu_regs_el2.hfgrtr_el2,
					  true);

		if (!vfp_alloc_za_storage(thread, partition)) {
			success = false;
			goto out;
		}
	}
#endif

	success = vfp_alloc_zpffr(thread, partition);
#if defined(ARCH_ARM_FEAT_SME)
	if (!success) {
		vfp_free_za_storage(thread, partition);
	}
out:
#endif
	return success;
}

void
vfp_handle_object_deactivate_thread(thread_t *thread)
{
	partition_t *partition = thread->header.partition;

#if defined(ARCH_ARM_FEAT_SME)
	vfp_free_za_storage(thread, partition);
#endif
	vfp_free_zpffr(thread, partition);
}

#if defined(ARCH_ARM_FEAT_SME)
static void
vfp_unblock_thread(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
	// The atomicity guarantees that this function will decide to unblock
	// the thread if and only if the affinity changed event handler decides
	// to block the thread.
	//
	// The ordering of the blocking and unblocking is guaranteed by the
	// fact that the affinity changed event handler is called with the
	// scheduler lock acquired.
	vfp_lazy_state_t old = atomic_exchange_explicit(
		&thread->vfp.state, VFP_LAZY_STATE_NONE, memory_order_acq_rel);

	if (old == VFP_LAZY_STATE_BLOCKED) {
		scheduler_lock_nopreempt(thread);
		bool need_trigger =
			scheduler_unblock(thread, SCHEDULER_BLOCK_VFP_SAVE);
		scheduler_unlock_nopreempt(thread);

		if (need_trigger) {
			scheduler_trigger();
		}
	}
}

static void
vfp_discard_lazy_sme_state(void) REQUIRE_PREEMPT_DISABLED
{
	thread_t *thread = CPULOCAL(vfp_lazy_sme_owner);

	object_put_thread(thread);
	CPULOCAL(vfp_lazy_sme_owner) = NULL;

	vfp_unblock_thread(thread);
}

static void
vfp_save_lazy_sme_state(void) REQUIRE_PREEMPT_DISABLED
{
	thread_t *thread = CPULOCAL(vfp_lazy_sme_owner);

	thread->vcpu_regs_el1.smcr_el1 =
		register_SMCR_EL1_read_ordered(&asm_ordering);

	// The whole SVCR register should have already been eagerly saved in the
	// save state handler. This means we can read the current value of the
	// ZA bit from the saved value.
	//
	// We must check the ZA bit because this function may have been called
	// from the save state handler.
	if (SVCR_get_ZA(&thread->vfp.sme.svcr)) {
		vfp_save_za_storage(thread->vfp.sme.za_storage, &asm_ordering);
	}

	object_put_thread(thread);
	CPULOCAL(vfp_lazy_sme_owner) = NULL;

	vfp_unblock_thread(thread);
}

static void
vfp_load_lazy_sme_state(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
	register_SMCR_EL1_write_ordered(thread->vcpu_regs_el1.smcr_el1,
					&asm_ordering);

	// If we are being called from the load state handler, then the saved
	// SVCR needs to be loaded. Otherwise, we are being called from the
	// SMEN trap handler and we need to load the ZA bit only.
	//
	// In the latter case, both the current and saved SM must be 0, so we
	// can avoid reading the current SVCR and just load the saved SVCR
	// directly in both cases.
	SVCR_t svcr = thread->vfp.sme.svcr;
	register_SVCR_write_ordered(svcr, &asm_ordering);

	if (SVCR_get_ZA(&svcr)) {
		vfp_load_za_storage(thread->vfp.sme.za_storage, &asm_ordering);
	}

	object_get_thread_additional(thread);
	CPULOCAL(vfp_lazy_sme_owner) = thread;

	// The affinity changed event handler cannot be running at this point
	// because `thread` is current on this cpu.
	//
	// FIXME: Is it guaranteed that the store is visible to the event
	// handler when it ends up running?
	atomic_store_relaxed(&thread->vfp.state, VFP_LAZY_STATE_ACTIVE);
}

static void
vfp_sync(thread_t *thread, bool enter_streaming_mode) REQUIRE_PREEMPT_DISABLED
{
	thread_t *owner = CPULOCAL(vfp_lazy_sme_owner);

	if (thread == owner) {
		// If the owner is the same, then the hardware state is already
		// synced with the thread's state; we just need to enter
		// streaming mode if requested.
		if (enter_streaming_mode) {
			vfp_smstart_sm(&asm_ordering);
		}
	} else {
		// Otherwise, save and load the lazy SME state.
		if (owner != NULL) {
			vfp_save_lazy_sme_state();
		}
		vfp_load_lazy_sme_state(thread);
	}
}

static void
vfp_sync_smstop(void) REQUIRE_PREEMPT_DISABLED
{
	if (CPULOCAL(vfp_lazy_sme_owner) != NULL) {
		CPTR_EL2_E2H1_t cptr = vfp_enable_hw_access(&asm_ordering);
		vfp_save_lazy_sme_state();
		vfp_smstop_all(&asm_ordering);
		vfp_restore_cptr(cptr, &asm_ordering);
	}
	// If there is no lazy sme owner, then SM and ZA should already be 0.
}

static void
vfp_save_streaming_sve_register_state(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
#if defined(ARCH_ARM_FEAT_SME_FA64)
	if (vfp_streaming_has_ffr) {
		vfp_save_streaming_pffr_regs(
			&thread->vfp.predicate_regs->pffr_streaming);
	} else {
		vfp_save_streaming_p_regs(
			&thread->vfp.predicate_regs->p_streaming);
	}
#else
	vfp_save_streaming_p_regs(&thread->vfp.predicate_regs->p_streaming,
				  &asm_ordering);
#endif

#if PLATFORM_SME_REG_SIZE == 16U
	vfp_save_v_regs(&thread->vfp.vector_regs->v, &asm_ordering);
#else
	vfp_save_streaming_z_regs(&thread->vfp.vector_regs->z_streaming,
				  &asm_ordering);
#endif
}

static void
vfp_load_streaming_sve_register_state(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
#if defined(ARCH_ARM_FEAT_SME_FA64)
	if (vfp_streaming_has_ffr) {
		vfp_load_streaming_pffr_regs(
			&thread->vfp.predicate_regs->pffr_streaming);
	} else {
		vfp_load_streaming_p_regs(
			&thread->vfp.predicate_regs->p_streaming);
	}
#else
	vfp_load_streaming_p_regs(&thread->vfp.predicate_regs->p_streaming,
				  &asm_ordering);
#endif

#if PLATFORM_SME_REG_SIZE == 16U
	vfp_load_v_regs(&thread->vfp.vector_regs->v, &asm_ordering);
#else
	vfp_load_streaming_z_regs(&thread->vfp.vector_regs->z_streaming,
				  &asm_ordering);
#endif
}
#endif

#if defined(ARCH_ARM_FEAT_SVE)
static void
vfp_save_sve_register_state(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
	vfp_save_sve_pffr_regs(&thread->vfp.predicate_regs->pffr_sve,
			       &asm_ordering);

#if PLATFORM_SVE_REG_SIZE == 16U
	vfp_save_v_regs(&thread->vfp.vector_regs->v, &asm_ordering);
#else
	vfp_save_sve_z_regs(&thread->vfp.vector_regs->z_sve, &asm_ordering);
#endif
}

static void
vfp_load_sve_register_state(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
	vfp_load_sve_pffr_regs(&thread->vfp.predicate_regs->pffr_sve,
			       &asm_ordering);

#if PLATFORM_SVE_REG_SIZE == 16U
	vfp_load_v_regs(&thread->vfp.vector_regs->v, &asm_ordering);
#else
	vfp_load_sve_z_regs(&thread->vfp.vector_regs->z_sve, &asm_ordering);
#endif
}
#endif

static void
vfp_vcpu_thread_save_state(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
	// FPSR must be saved before any potential changes to PSTATE.SM.
	thread->vcpu_regs_fpr.fpcr = register_FPCR_read();
	thread->vcpu_regs_fpr.fpsr = register_FPSR_read_ordered(&asm_ordering);

#if defined(ARCH_ARM_FEAT_SVE)
	bool sve_allowed =
		vcpu_option_flags_get_sve_allowed(&thread->vcpu_options);
	if (sve_allowed) {
		thread->vcpu_regs_el1.zcr_el1 =
			register_ZCR_EL1_read_ordered(&asm_ordering);
	}
#endif
#if defined(ARCH_ARM_FEAT_SME)
	if (vcpu_option_flags_get_sme_allowed(&thread->vcpu_options)) {
		thread->vcpu_regs_el1.tpidr2_el0 =
			register_TPIDR2_EL0_read_volatile();

		// A thread's SME state can only have changed if it was given
		// SME access. This is not only an optimisation; there may be
		// lazily switched context belonging to a different thread that
		// should not be saved.
		if (thread->vfp.sme_access) {
			// Save the entire SVCR register. We should already have
			// hardware SME access.
			thread->vfp.sme.svcr =
				register_SVCR_read_ordered(&asm_ordering);
			bool sm = SVCR_get_SM(&thread->vfp.sme.svcr);
			bool za = SVCR_get_ZA(&thread->vfp.sme.svcr);

			// Check if the PE is in streaming mode. If it is, then
			// save the streaming SVE register state. We also
			// enable or disable SME access here so we can load the
			// required CPTR_EL2 straight away in the load state
			// handler.
			if (sm) {
				vfp_save_streaming_sve_register_state(thread);
				vfp_enable_sme_access(
					&thread->vcpu_regs_el2.cptr_el2);
			} else {
				vfp_disable_sme_access(
					&thread->vcpu_regs_el2.cptr_el2);
				thread->vfp.sme_access = false;
			}

			// If the thread is not using ZA, eagerly save the
			// state to avoid delaying thread migration.
			if (!za && (CPULOCAL(vfp_lazy_sme_owner) != NULL)) {
				vfp_save_lazy_sme_state();
			}

			if (sm) {
				// It's better to leave streaming mode here
				// while we still have hardware SME access.
				vfp_smstop_sm(&asm_ordering);
				goto out;
			}
			// If the PE is not in streaming mode, fall through to
			// the SVE register state check.
		}
	}
#endif

	// Otherwise, if the thread is allowed to use SVE, then save the SVE
	// register state.
#if defined(ARCH_ARM_FEAT_SVE)
	if (sve_allowed) {
		vfp_save_sve_register_state(thread);
		goto out;
	}
#endif

	// Otherwise, the thread is not allowed to use SVE, or SVE is not
	// implemented. In this case, we only need to save the V registers.
	vfp_save_v_regs(&thread->vfp.vector_regs->v, &asm_ordering);
out:
	return;
}

static void
vfp_vcpu_thread_load_state(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
	vfp_restore_cptr(thread->vcpu_regs_el2.cptr_el2, &asm_ordering);

#if defined(ARCH_ARM_FEAT_SVE)
	bool sve_allowed =
		vcpu_option_flags_get_sve_allowed(&thread->vcpu_options);
	if (sve_allowed) {
		register_ZCR_EL1_write_ordered(thread->vcpu_regs_el1.zcr_el1,
					       &asm_ordering);
	}
#endif
#if defined(ARCH_ARM_FEAT_SME)
	if (vcpu_option_flags_get_sme_allowed(&thread->vcpu_options)) {
		register_TPIDR2_EL0_write(thread->vcpu_regs_el1.tpidr2_el0);

		// The thread's SM bit is always loaded, but its ZA bit is only
		// loaded if SM == 1.
		if (SVCR_get_SM(&thread->vfp.sme.svcr)) {
			vfp_sync(thread, true);
			vfp_load_streaming_sve_register_state(thread);
			goto out;
		} // We always leave streaming mode after saving the state, so
		  // it is unnecessary to do it here.
	}
#endif

#if defined(ARCH_ARM_FEAT_SVE)
	if (sve_allowed) {
		vfp_load_sve_register_state(thread);
		goto out;
	}
#endif

	vfp_load_v_regs(&thread->vfp.vector_regs->v, &asm_ordering);
out:
	register_FPCR_write(thread->vcpu_regs_fpr.fpcr);
	register_FPSR_write_ordered(thread->vcpu_regs_fpr.fpsr, &asm_ordering);
}

void
vfp_handle_vcpu_save_state(void)
{
	thread_t *thread = thread_get_self();

	vfp_vcpu_thread_save_state(thread);
}

void
vfp_handle_vcpu_load_state(void)
{
	thread_t *thread = thread_get_self();

	vfp_vcpu_thread_load_state(thread);
}

void
vfp_handle_vcpu_disable_state(void)
{
#if defined(ARCH_ARM_FEAT_SME)
	vfp_sync_smstop();
#endif
}

#if defined(ARCH_ARM_FEAT_SME)
void
vfp_handle_scheduler_affinity_changed(thread_t *thread, cpu_index_t prev_cpu)
{
	if (!cpulocal_index_valid(prev_cpu) || (!vcpu_is_vcpu(thread)) ||
	    vcpu_option_flags_get_pinned(&thread->vcpu_options)) {
		goto out;
	}

	vfp_lazy_state_t expected = VFP_LAZY_STATE_ACTIVE;
	vfp_lazy_state_t desired  = VFP_LAZY_STATE_BLOCKED;

	if (atomic_compare_exchange_strong_explicit(
		    &thread->vfp.state, &expected, desired,
		    memory_order_acq_rel, memory_order_acquire)) {
		scheduler_block(thread, SCHEDULER_BLOCK_VFP_SAVE);
		ipi_one(IPI_REASON_VFP_SAVE, prev_cpu);
	}

out:
	return;
}

static bool
vfp_thread_is_blocked(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
	bool blocked = false;

	if (thread != NULL) {
		// Relaxed load, since the ipi handler already synchronises
		// with sending the ipi.
		blocked = atomic_load_relaxed(&thread->vfp.state) ==
			  VFP_LAZY_STATE_BLOCKED;
	}

	return blocked;
}

bool
vfp_handle_ipi_vfp_save(void)
{
	if (vfp_thread_is_blocked(CPULOCAL(vfp_lazy_sme_owner))) {
		// It should be impossible for the SME-only state to belong to a
		// blocked thread (with reason VFP_SAVE) and the current thread
		// at the same time, since a thread is only blocked with reason
		// VFP_SAVE when the thread has stopped running and is being
		// migrated.
		//
		// The PE cannot be in streaming mode, because if it were, then
		// the current thread would be the owner of the SME-only state
		// (which is blocked). Therefore, it is save to reset the SM
		// bit since it is a noop.
		//
		// The ZA bit can obviously be reset since it doesn't belong to
		// the current thread.
		vfp_sync_smstop();
	}

	return false;
}

error_t
vfp_handle_power_cpu_suspend(bool may_poweroff)
{
	if (may_poweroff) {
		vfp_sync_smstop();
	}

	return OK;
}

static void
vfp_discard_sme_state_if_current(thread_t *thread) REQUIRE_PREEMPT_DISABLED
{
	if (thread == CPULOCAL(vfp_lazy_sme_owner)) {
		vfp_discard_lazy_sme_state();
		CPTR_EL2_E2H1_t cptr = vfp_enable_hw_access(&asm_ordering);
		vfp_smstop_all(&asm_ordering);

		if (thread->vfp.sme_access) {
			// We rely on the thread being the owner of the SME-only
			// state if has SME access, so if we are discarding the
			// state we need to disable SME access again.
			thread->vfp.sme_access = false;
			vfp_disable_sme_access(&cptr);
		}

		// Update the thread's saved state to reflect the fact that it
		// has been discarded.
		thread->vfp.sme.svcr = SVCR_default();
		vfp_disable_sme_access(&thread->vcpu_regs_el2.cptr_el2);

		vfp_restore_cptr(cptr, &asm_ordering);
	}
}

void
vfp_handle_vcpu_stopped(void)
{
	thread_t *current = thread_get_self();

	vfp_discard_sme_state_if_current(current);
}

void
vfp_handle_vcpu_warm_reset(thread_t *vcpu)
{
	if (vcpu_option_flags_get_sme_allowed(&vcpu->vcpu_options)) {
		preempt_disable();
		vfp_discard_sme_state_if_current(vcpu);
		preempt_enable();
	}
}
#endif

#if defined(ARCH_ARM_FEAT_SME)
static vcpu_trap_result_t
vfp_handle_smtc_access_trap(void)
{
	preempt_disable();

	vcpu_trap_result_t ret	  = VCPU_TRAP_RESULT_UNHANDLED;
	thread_t	  *thread = thread_get_self();

	if (!vcpu_option_flags_get_sme_allowed(&thread->vcpu_options)) {
		goto out;
	}

	// Sync the lazily context switched SME state.
	(void)vfp_enable_hw_sme_access();
	vfp_sync(thread, false);

	// Allow SME access for this thread and retry the instruction.
	thread->vfp.sme_access = true;
	ret		       = VCPU_TRAP_RESULT_RETRY;
out:
	preempt_enable();
	return ret;
}

vcpu_trap_result_t
vfp_handle_vcpu_trap_sme_access(ESR_EL2_ISS_SME_t iss)
{
	vcpu_trap_result_t ret = VCPU_TRAP_RESULT_UNHANDLED;

	if (!vfp_sme_disabled) {
		switch (ESR_EL2_ISS_SME_get_SMTC(&iss)) {
		case ISS_SME_SMTC_ACCESS_TRAP:
			ret = vfp_handle_smtc_access_trap();
			break;
		case ISS_SME_SMTC_STREAMING:
		case ISS_SME_SMTC_NON_STREAMING:
		case ISS_SME_SMTC_INACTIVE_ZA:
		default:
			ret = VCPU_TRAP_RESULT_UNHANDLED;
			break;
		}
	}

	return ret;
}

static register_t
sys_aa64smfr0_read(void)
{
	ID_AA64SMFR0_EL1_t smfr0 = register_ID_AA64SMFR0_EL1_read();

	smfr0 = ID_AA64SMFR0_EL1_clean(smfr0);

#if defined(ARCH_ARM_FEAT_SME_FA64)
	if (vfp_sme_fa64_disabled) {
		ID_AA64SMFR0_EL1_set_FA64(&smfr0, false);
	}
#endif

	// Set SME2 fields to 'not implemented'.
	ID_AA64SMFR0_EL1_set_SMEver(&smfr0, 0U);
	ID_AA64SMFR0_EL1_set_I16I32(&smfr0, 0U);
	ID_AA64SMFR0_EL1_set_BI32I32(&smfr0, false);

	return ID_AA64SMFR0_EL1_raw(smfr0);
}

static register_t
sys_smidr_read(const thread_t *thread)
{
	SMIDR_EL1_t smidr = register_SMIDR_EL1_read();

	smidr = SMIDR_EL1_clean(smidr);

	// Tell all threads that streaming mode priority is not supported.
	SMIDR_EL1_set_SMPS(&smidr, false);

	if (!vcpu_option_flags_get_pinned(&thread->vcpu_options)) {
		// Tell non pinned threads that they share an SMCU.
		SMIDR_EL1_set_Affinity(&smidr, 0U);
		SMIDR_EL1_set_SH(&smidr, SMIDR_SH_SHARED);
		SMIDR_EL1_set_Revision(&smidr, 0U);
		SMIDR_EL1_set_Implementer(&smidr, 0U);
		SMIDR_EL1_set_Affinity2(&smidr, 0U);
	}

	return SMIDR_EL1_raw(smidr);
}

// ID_AA64SMFR0_EL1 is trapped through HCR_EL2.TID3
// SMIDR_EL1 is trapped through HCR_EL2.TID1
vcpu_trap_result_t
vfp_sme_handle_vcpu_trap_sysreg_read(ESR_EL2_ISS_MSR_MRS_t iss)
{
	register_t	   reg_val = 0ULL;
	vcpu_trap_result_t ret	   = VCPU_TRAP_RESULT_EMULATED;
	thread_t	  *thread  = thread_get_self();

	// Assert this is a read
	assert(ESR_EL2_ISS_MSR_MRS_get_Direction(&iss));

	uint8_t reg_num = ESR_EL2_ISS_MSR_MRS_get_Rt(&iss);

	// Remove the fields that are not used in the comparison
	ESR_EL2_ISS_MSR_MRS_t temp_iss = iss;
	ESR_EL2_ISS_MSR_MRS_set_Rt(&temp_iss, 0U);
	ESR_EL2_ISS_MSR_MRS_set_Direction(&temp_iss, false);

	switch (ESR_EL2_ISS_MSR_MRS_raw(temp_iss)) {
	case ISS_MRS_MSR_ID_AA64SMFR0_EL1:
		if (vcpu_option_flags_get_sme_allowed(&thread->vcpu_options)) {
			reg_val = sys_aa64smfr0_read();
		}
		break;
	case ISS_MRS_MSR_SMIDR_EL1:
		if (vcpu_option_flags_get_sme_allowed(&thread->vcpu_options)) {
			reg_val = sys_smidr_read(thread);
		} else {
			ret = VCPU_TRAP_RESULT_UNHANDLED;
		}
		break;
	default:
		ret = VCPU_TRAP_RESULT_UNHANDLED;
		break;
	}

	if (ret == VCPU_TRAP_RESULT_EMULATED) {
		vcpu_gpr_write(thread, reg_num, reg_val);
	}

	return ret;
}

// SMPRI_EL1 is trapped through HFGWTR_EL2.nSMPRI_EL1
vcpu_trap_result_t
vfp_sme_handle_vcpu_trap_sysreg_write(ESR_EL2_ISS_MSR_MRS_t iss)
{
	vcpu_trap_result_t ret;
	thread_t	  *thread = thread_get_self();

	ESR_EL2_ISS_MSR_MRS_t temp_iss = iss;
	ESR_EL2_ISS_MSR_MRS_set_Rt(&temp_iss, 0U);
	ESR_EL2_ISS_MSR_MRS_set_Direction(&temp_iss, false);

	switch (ESR_EL2_ISS_MSR_MRS_raw(temp_iss)) {
	case ISS_MRS_MSR_SMPRI_EL1:
		ret = vcpu_option_flags_get_sme_allowed(&thread->vcpu_options)
			      ? VCPU_TRAP_RESULT_EMULATED
			      : VCPU_TRAP_RESULT_UNHANDLED;
		break;
	default:
		ret = VCPU_TRAP_RESULT_UNHANDLED;
		break;
	}

	return ret;
}

#endif
#endif
