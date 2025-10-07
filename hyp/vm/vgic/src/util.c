// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <assert.h>
#include <hyptypes.h>

#include <atomic.h>
#include <preempt.h>
#include <spinlock.h>
#include <thread.h>
#include <util.h>

#include "internal.h"

static_assert(!(VGIC_HAS_EXT_PPIS && (!VGIC_HAS_EXT_SPIS)),
	      "vgic has extended PPIs enabled without extended SPIs enabled");

#if defined(VGIC_SPI_EXT_NUM)
static_assert((VGIC_SPI_EXT_NUM <= GIC_SPI_EXT_NUM) &&
		      ((VGIC_SPI_EXT_NUM % 32U) == 0U),
	      "Invalid configuration for VGIC_SPI_EXT_NUM");
#endif

vgic_irq_type_t
vgic_get_irq_type(virq_t irq)
{
	vgic_irq_type_t type;

	switch (irq) {
	case util_case_range(GIC_SGI_BASE,
			     (virq_t)(GIC_SGI_BASE + (GIC_SGI_NUM - 1U))):
		type = VGIC_IRQ_TYPE_SGI;
		break;
	case util_case_range(GIC_PPI_BASE,
			     (virq_t)(GIC_PPI_BASE + (GIC_PPI_NUM - 1U))):
		type = VGIC_IRQ_TYPE_PPI;
		break;
	case util_case_range(GIC_SPI_BASE,
			     (virq_t)(GIC_SPI_BASE + (GIC_SPI_NUM - 1U))):
		type = VGIC_IRQ_TYPE_SPI;
		break;
#if VGIC_HAS_EXT_PPIS
	case util_case_range(
		GIC_PPI_EXT_BASE,
		(virq_t)(GIC_PPI_EXT_BASE + (GIC_PPI_EXT_NUM - 1U))):
		type = VGIC_IRQ_TYPE_PPI_EXT;
		break;
#endif
#if VGIC_HAS_EXT_SPIS
	case util_case_range(
		GIC_SPI_EXT_BASE,
		(virq_t)(GIC_SPI_EXT_BASE + (VGIC_SPI_EXT_NUM - 1U))):
		type = VGIC_IRQ_TYPE_SPI_EXT;
		break;
#endif
#if VGIC_HAS_LPI
	case util_case_range(GIC_LPI_BASE,
			     (virq_t)(GIC_LPI_BASE + (VGIC_LPI_NUM - 1U))):
		type = VGIC_IRQ_TYPE_LPI;
		break;
#endif
	default:
		type = VGIC_IRQ_TYPE_RESERVED;
		break;
	}

	return type;
}

bool
vgic_irq_is_private(virq_t virq)
{
	bool result;
	switch (vgic_get_irq_type(virq)) {
	case VGIC_IRQ_TYPE_SGI:
	case VGIC_IRQ_TYPE_PPI:
#if VGIC_HAS_EXT_PPIS
	case VGIC_IRQ_TYPE_PPI_EXT:
#endif
		result = true;
		break;
	case VGIC_IRQ_TYPE_SPI:
#if VGIC_HAS_EXT_SPIS
	case VGIC_IRQ_TYPE_SPI_EXT:
#endif
	case VGIC_IRQ_TYPE_RESERVED:
#if VGIC_HAS_LPI && GICV3_HAS_VLPI_V4_1
	case VGIC_IRQ_TYPE_LPI:
#endif
	default:
		result = false;
		break;
	}
	return result;
}

bool
vgic_irq_is_spi(virq_t virq)
{
	bool result;
	switch (vgic_get_irq_type(virq)) {
	case VGIC_IRQ_TYPE_SPI:
#if VGIC_HAS_EXT_SPIS
	case VGIC_IRQ_TYPE_SPI_EXT:
#endif
		result = true;
		break;
	case VGIC_IRQ_TYPE_SGI:
	case VGIC_IRQ_TYPE_PPI:
#if VGIC_HAS_EXT_PPIS
	case VGIC_IRQ_TYPE_PPI_EXT:
#endif
	case VGIC_IRQ_TYPE_RESERVED:
#if VGIC_HAS_LPI && GICV3_HAS_VLPI_V4_1
	case VGIC_IRQ_TYPE_LPI:
#endif
	default:
		result = false;
		break;
	}
	return result;
}

bool
vgic_irq_is_ppi(virq_t virq)
{
	bool result;
	switch (vgic_get_irq_type(virq)) {
	case VGIC_IRQ_TYPE_PPI:
#if VGIC_HAS_EXT_PPIS
	case VGIC_IRQ_TYPE_PPI_EXT:
#endif
		result = true;
		break;
	case VGIC_IRQ_TYPE_SGI:
	case VGIC_IRQ_TYPE_SPI:
#if VGIC_HAS_EXT_SPIS
	case VGIC_IRQ_TYPE_SPI_EXT:
#endif
	case VGIC_IRQ_TYPE_RESERVED:
#if VGIC_HAS_LPI && GICV3_HAS_VLPI_V4_1
	case VGIC_IRQ_TYPE_LPI:
#endif
	default:
		result = false;
		break;
	}
	return result;
}

// Find the target of a given VIRQ source, if it is directly routed or private.
//
// No routing decisions are made by this function; it returns NULL for 1-of-N
// SPIs.
thread_t *
vgic_find_target(vic_t *vic, virq_source_t *source)
{
	thread_t *ret;

	if (source->is_private) {
		if (source->vgic_gicr_index < vic->gicr_count) {
			ret = atomic_load_consume(
				&vic->gicr_vcpus[source->vgic_gicr_index]);
		} else {
			ret = NULL;
		}
	} else {
		_Atomic vgic_delivery_state_t *dstate =
			vgic_find_dstate(vic, NULL, source->virq);
		vgic_delivery_state_t current_dstate =
			atomic_load_relaxed(dstate);

#if VGIC_HAS_1N
		if (vgic_delivery_state_get_route_1n(&current_dstate)) {
			ret = NULL;
			goto out;
		}
#endif

		index_t route_index =
			vgic_delivery_state_get_route(&current_dstate);
		if (route_index < vic->gicr_count) {
			ret = atomic_load_consume(
				&vic->gicr_vcpus[route_index]);
		} else {
			ret = NULL;
		}
	}

#if VGIC_HAS_1N
out:
#endif
	return ret;
}

virq_source_t *_Atomic *
vgic_find_source_ptr(vic_t *vic, thread_t *vcpu, virq_t virq)
{
	_Atomic(virq_source_t *) *source_ptr;

	switch (vgic_get_irq_type(virq)) {
	case VGIC_IRQ_TYPE_SPI:
		assert(vic != NULL);
		if (virq < (vic->sources_count + GIC_SPI_BASE)) {
			source_ptr = &vic->sources[virq - GIC_SPI_BASE];
		} else {
			source_ptr = NULL;
		}
		break;
	case VGIC_IRQ_TYPE_PPI:
		assert(vcpu != NULL);
		source_ptr = &vcpu->vgic_sources[virq - GIC_PPI_BASE];
		break;
#if VGIC_HAS_EXT_SPIS
	case VGIC_IRQ_TYPE_SPI_EXT:
		assert(vic != NULL);
		if (virq < (vic->ext_sources_count + GIC_SPI_EXT_BASE)) {
			source_ptr = &vic->ext_sources[virq - GIC_SPI_EXT_BASE];
		} else {
			source_ptr = NULL;
		}
		break;
#if VGIC_HAS_EXT_PPIS
	case VGIC_IRQ_TYPE_PPI_EXT:
		assert(vcpu != NULL);
		source_ptr = &vcpu->vgic_ext_sources[virq - GIC_PPI_EXT_BASE];
		break;
#endif
#endif
	case VGIC_IRQ_TYPE_SGI:
	case VGIC_IRQ_TYPE_RESERVED:
#if VGIC_HAS_LPI && GICV3_HAS_VLPI_V4_1
	case VGIC_IRQ_TYPE_LPI:
#endif
	default:
		source_ptr = NULL;
		break;
	}
	return source_ptr;
}

virq_source_t *
vgic_find_source(vic_t *vic, thread_t *vcpu, virq_t virq)
{
	virq_source_t		 *source = NULL;
	_Atomic(virq_source_t *) *source_ptr =
		vgic_find_source_ptr(vic, vcpu, virq);
	if (source_ptr != NULL) {
		source = atomic_load_acquire(source_ptr);
	}
	return source;
}

vgic_delivery_state_t _Atomic *
vgic_find_dstate(vic_t *vic, thread_t *vcpu, virq_t virq)
{
	_Atomic vgic_delivery_state_t *dstate;

	switch (vgic_get_irq_type(virq)) {
	case VGIC_IRQ_TYPE_SGI:
	case VGIC_IRQ_TYPE_PPI:
		assert_debug(vcpu != NULL);
		assert_debug(virq < util_array_size(vcpu->vgic_private_states));

		dstate = &vcpu->vgic_private_states[virq];
		break;
	case VGIC_IRQ_TYPE_SPI:
		assert_debug(vic != NULL);
		assert_debug(virq >= GIC_SPI_BASE);
		assert_debug((virq - GIC_SPI_BASE) <
			     ((virq_t)util_array_size(vic->spi_states)));

		dstate = &vic->spi_states[virq - GIC_SPI_BASE];
		break;
#if VGIC_HAS_EXT_PPIS
	case VGIC_IRQ_TYPE_PPI_EXT:
		assert_debug(vcpu != NULL);
		assert_debug(virq >= GIC_PPI_EXT_BASE);
		assert_debug((virq - GIC_PPI_EXT_BASE) <
			     util_array_size(vcpu->vgic_ext_private_states));

		dstate =
			&vcpu->vgic_ext_private_states[virq - GIC_PPI_EXT_BASE];
		break;
#endif
#if VGIC_HAS_EXT_SPIS
	case VGIC_IRQ_TYPE_SPI_EXT:
		assert_debug(vic != NULL);
		assert_debug(virq >= GIC_SPI_EXT_BASE);
		assert_debug((virq - GIC_SPI_EXT_BASE) <
			     ((virq_t)util_array_size(vic->ext_spi_states)));

		dstate = &vic->ext_spi_states[virq - GIC_SPI_EXT_BASE];
		break;
#endif
	case VGIC_IRQ_TYPE_RESERVED:
#if VGIC_HAS_LPI && GICV3_HAS_VLPI_V4_1
	case VGIC_IRQ_TYPE_LPI:
#endif
	default:
		// Invalid IRQ number
		dstate = NULL;
		break;
	}
	return dstate;
}

bool
vgic_delivery_state_is_level_asserted(const vgic_delivery_state_t *x)
{
	return vgic_delivery_state_get_level_sw(x) ||
	       vgic_delivery_state_get_level_msg(x) ||
	       vgic_delivery_state_get_level_src(x);
}

bool
vgic_delivery_state_is_pending(const vgic_delivery_state_t *x)
{
	return vgic_delivery_state_get_cfg_is_edge(x)
		       ? vgic_delivery_state_get_edge(x)
		       : vgic_delivery_state_is_level_asserted(x);
}

cpu_index_t
vgic_lr_owner_lock(thread_t *vcpu)
{
	preempt_disable();
	return vgic_lr_owner_lock_nopreempt(vcpu);
}

cpu_index_t
vgic_lr_owner_lock_nopreempt(thread_t *vcpu) LOCK_IMPL
{
	cpu_index_t remote_cpu;
	if ((vcpu != NULL) && (vcpu != thread_get_self())) {
		spinlock_acquire_nopreempt(&vcpu->vgic_lr_owner_lock.lock);
		remote_cpu =
			atomic_load_relaxed(&vcpu->vgic_lr_owner_lock.owner);
	} else {
		remote_cpu = CPU_INDEX_INVALID;
	}
	return remote_cpu;
}

void
vgic_lr_owner_unlock(thread_t *vcpu)
{
	vgic_lr_owner_unlock_nopreempt(vcpu);
	preempt_enable();
}

void
vgic_lr_owner_unlock_nopreempt(thread_t *vcpu) LOCK_IMPL
{
	if ((vcpu != NULL) && (vcpu != thread_get_self())) {
		spinlock_release_nopreempt(&vcpu->vgic_lr_owner_lock.lock);
	}
}
