// Copyright © Qualcomm Technologies, Inc. and/or its subsidiaries.
//
// SPDX-License-Identifier: BSD-3-Clause

#if defined(UNIT_TESTS) && GICV3_EXT_IRQS
#include <hyptypes.h>

#include <hypconstants.h>
#include <hypregisters.h>

#include <atomic.h>
#include <compiler.h>
#include <log.h>
#include <object.h>
#include <panic.h>
#include <partition.h>
#include <partition_alloc.h>
#include <trace.h>

#include "event_handlers.h"
#include "gicv3.h"

void
gicv3_handle_tests_init(void)
{
	// Test extended SPI
	gicv3_irq_set_trigger_shared(GIC_SPI_EXT_BASE, IRQ_TRIGGER_EDGE_RISING);

	hwirq_create_t params = {
		.irq	= GIC_SPI_EXT_BASE,
		.action = HWIRQ_ACTION_GICV3_EXT_SPI_TEST_ACTION,
	};

	hwirq_ptr_result_t ret =
		partition_allocate_hwirq(partition_get_private(), params);

	if (ret.e != OK) {
		panic("Failed to create GICv3 extended SPI test IRQ");
	}

	if (object_activate_hwirq(ret.r) != OK) {
		panic("Failed to activate GICv3 extended SPI test IRQ");
	}

	gicv3_irq_enable_shared(ret.r->irq);
}

bool
gicv3_handle_tests_start(void)
{
	// Trigger extended SPI test
	gicd_t			   *gicd = gicv3_get_gicd_pointer();
	GICD_CLRSPI_SETSPI_NSR_SR_t gic_spi_ext_base;
	gic_spi_ext_base.bf[0] = GIC_SPI_EXT_BASE;
	atomic_store_release(&gicd->setspi_nsr, gic_spi_ext_base);

	return false;
}

bool
gicv3_handle_irq_received_ext_spi_test_action(const hwirq_t *hwirq)
{
	// Extended SPI Test Handler
	LOG(DEBUG, INFO, "Extended SPI test handler for received irq {:d}\n",
	    (uint32_t)hwirq->irq);

	return true;
}
#else

extern char unused;

#endif
