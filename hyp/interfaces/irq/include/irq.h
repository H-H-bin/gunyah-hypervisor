// © 2021 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

// Routines for handling hardware-triggered IRQs.

#if !defined(IRQ_NULL)
// Enable a hardware IRQ, which must not be per-CPU.
//
// This function always immediately enables the IRQ, regardless of how many
// times irq_disable_*() has been called; there is no nesting count. The caller
// is responsible for counting disables if necessary.
//
// Note that newly registered IRQs are always disabled, and IRQs are
// automatically disabled when they are deregistered.
void
irq_enable_shared(hwirq_t *hwirq);

// Enable a hardware per-CPU IRQ on the local CPU.
//
// The behaviour is the same as for irq_enable_shared(), but affects only the
// calling CPU.
void
irq_enable_local(hwirq_t *hwirq) REQUIRE_PREEMPT_DISABLED;

// Disable a hardware IRQ, which must not be per-CPU, and wait until any running
// handlers on remote CPUs have completed.
//
// This function might block the calling thread, so cannot be called with
// preemption disabled.
void
irq_disable_shared_sync(hwirq_t *hwirq) EXCLUDE_PREEMPT_DISABLED;

// Disable a hardware IRQ, which must not be per-CPU, without waiting for
// handlers on remote CPUs to complete.
//
// This may be called with preemption disabled.
void
irq_disable_shared_nosync(hwirq_t *hwirq);

// Disable a hardware per-CPU IRQ on the local CPU.
//
// This may be called with preemption disabled.
void
irq_disable_local(hwirq_t *hwirq) REQUIRE_PREEMPT_DISABLED;

// Disable a hardware per-CPU IRQ on the local CPU, without waiting for the
// physical interrupt controller to acknowledge the disable (which may allow
// spurious interrupts to occur the call).
//
// This may be called with preemption disabled.
void
irq_disable_local_nowait(hwirq_t *hwirq) REQUIRE_PREEMPT_DISABLED;

// Deactivate an interrupt that has been handled after returning false from
// the irq_received handler. This is called automatically if the handler returns
// true.
void
irq_deactivate(hwirq_t *hwirq);

// Deactivate a forwarded IRQ which has been disabled
// with irq_disable_shared_nosync during IRQ unbinding.
void
irq_deactivate_forwarded(hwirq_t *hwirq);

// Register a range of HW IRQs provided by the platform.
//
// This should be called at boot time by the platform and/or interrupt
// controller driver code. It defines a range of IRQ numbers in which hwirq
// objects may be registered. The caller is responsible for creating and
// activating the hwirq objects afterwards.
//
// A caller that needs to represent IRQs in other ways should call
// irq_range_add() instead.
error_t
irq_range_add_hwirq(irq_t base, count_t size);

// Obtain the HW IRQ structure for a specific IRQ number.
//
// Must be called from an RCU critical section. No reference is taken to the
// result. Returns NULL if the specified IRQ is not in a registered hwirq range.
hwirq_t *
irq_lookup_hwirq(irq_t irq) REQUIRE_RCU_READ;

// Register a range of IRQ numbers.
//
// This is used internally by irq_range_add_hwirq(), but can be called by any
// other module that wants to customise the handling for a specific range of IRQ
// numbers. This is typically used for handling MSIs, which may be dynamically
// allocated by the hypervisor.
//
// If this succeeds, then any IRQ in this range that is subsequently returned by
// platform_irq_acknowledge() will be dispatched by calling an irq_dispatch
// event handler selected by range_type and passing the range pointer.
//
// The specified range must be a non-null pointer allocated by the caller, must
// not already be registered, and must not be released while it is registered.
// The caller should treat the irq_range_t structure as opaque; typically it
// will be contained in a larger structure defined by the caller.
error_t
irq_range_add(irq_range_t *range, irq_range_type_t range_type, irq_t base,
	      count_t size);

// Remove a registered range of IRQ numbers.
//
// If this succeeds, then any newly acknowledged IRQs will not be dispatched to
// the range after the call returns. IRQ handlers already dispatched to the
// range are guaranteed to have completed after an RCU grace period has elapsed.
//
// The range structure must not be freed or reused until after an RCU grace
// period has elapsed. If it is reused, it should be initialised to zero with
// memset() after the required RCU grace period has elapsed.
error_t
irq_range_remove(irq_range_t *range);

// Obtain the IRQ range for a specific IRQ number.
//
// Must be called from an RCU critical section. Returns NULL if the specified
// IRQ is not in a registered IRQ range.
irq_range_t *
irq_lookup_range(irq_t irq) REQUIRE_RCU_READ;

#endif // !defined(IRQ_NULL)

// Handle an interrupt exception on the current CPU.
//
// Returns true if rescheduling is needed.
bool
irq_interrupt_dispatch(void) REQUIRE_PREEMPT_DISABLED;
