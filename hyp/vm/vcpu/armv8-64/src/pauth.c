// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#include <hyptypes.h>

#include <hypregisters.h>

#if defined(ARCH_ARM_FEAT_PAuth)

#include <compiler.h>
#include <qcbor.h>
#include <thread.h>
#include <vcpu.h>

#include "event_handlers.h"

// The hypervisor only uses APIAKeyLo_EL1 and APIAKeyHi_EL1 and switching it
// needs to be done in the EL2 entry and exit assembly.
// To save kernel entry / exit time, we context switch the other vcpu key
// registers here.

void
vcpu_pauth_save_state(void)
{
	thread_t *vcpu = thread_get_self();

	aarch64_pauth_key_t da, db, ib, ga;

	da.lo = register_APDAKeyLo_EL1_read();
	da.hi = register_APDAKeyHi_EL1_read();

	vcpu->vcpu_regs_pauth.da = da;

	db.lo = register_APDBKeyLo_EL1_read();
	db.hi = register_APDBKeyHi_EL1_read();

	vcpu->vcpu_regs_pauth.db = db;

	ib.lo = register_APIBKeyLo_EL1_read();
	ib.hi = register_APIBKeyHi_EL1_read();

	vcpu->vcpu_regs_pauth.ib = ib;

	ga.lo = register_APGAKeyLo_EL1_read();
	ga.hi = register_APGAKeyHi_EL1_read();

	vcpu->vcpu_regs_pauth.ga = ga;
}

void
vcpu_pauth_load_state(void)
{
	thread_t *vcpu = thread_get_self();

	aarch64_pauth_key_t da, db, ib, ga;

	da = vcpu->vcpu_regs_pauth.da;
	register_APDAKeyLo_EL1_write(da.lo);
	register_APDAKeyHi_EL1_write(da.hi);

	db = vcpu->vcpu_regs_pauth.db;
	register_APDBKeyLo_EL1_write(db.lo);
	register_APDBKeyHi_EL1_write(db.hi);

	ib = vcpu->vcpu_regs_pauth.ib;
	register_APIBKeyLo_EL1_write(ib.lo);
	register_APIBKeyHi_EL1_write(ib.hi);

	ga = vcpu->vcpu_regs_pauth.ga;
	register_APGAKeyLo_EL1_write(ga.lo);
	register_APGAKeyHi_EL1_write(ga.hi);
}

void
vcpu_pauth_disable_state(void)
{
	aarch64_pauth_key_t nul = { 0U };

	register_APDAKeyLo_EL1_write(nul.lo);
	register_APDAKeyHi_EL1_write(nul.hi);

	register_APDBKeyLo_EL1_write(nul.lo);
	register_APDBKeyHi_EL1_write(nul.hi);

	register_APIBKeyLo_EL1_write(nul.lo);
	register_APIBKeyHi_EL1_write(nul.hi);

	register_APGAKeyLo_EL1_write(nul.lo);
	register_APGAKeyHi_EL1_write(nul.hi);
}
#endif
