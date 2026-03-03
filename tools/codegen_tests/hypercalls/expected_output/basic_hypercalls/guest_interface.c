// Gunyah Hypervisor hypercall C bindings.
//
// Automatically generated. Do not modify.

// Hypervisor Call C Types
#include <guest_types.h>
// Hypervisor Call definitions
#include <guest_interface.h>

error_code_t
gunyah_hyp_test_hypercall_1(interrupt_control_register_t in0)
{
	const register register_t in_x0_ __asm__("x0") = (register_t)in0.bf[0];
	register error_code_t	  out_x0_ __asm__("x0");

	__asm__ volatile("hvc 0x6000"
			 : "=r"(out_x0_)
			 : "r"(in_x0_)
			 : "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9",
			   "x10", "x11", "x12", "x13", "x14", "x15", "x16",
			   "x17");

	return (error_code_t)out_x0_;
}

error_code_t
gunyah_hyp_test_hypercall_2(uint64_t in0, count_t in1,
			    interrupt_control_register_t in2)
{
	const register register_t in_x0_ __asm__("x0") = (register_t)in0;
	register register_t	  in_x1_ __asm__("x1") = (register_t)in1;
	register register_t	  in_x2_ __asm__("x2") = (register_t)in2.bf[0];
	register error_code_t	  out_x0_ __asm__("x0");

	__asm__ volatile("hvc 0x6001"
			 : "=r"(out_x0_), "+r"(in_x1_), "+r"(in_x2_)
			 : "r"(in_x0_)
			 : "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
			   "x11", "x12", "x13", "x14", "x15", "x16", "x17");

	return (error_code_t)out_x0_;
}

gunyah_hyp_test_hypercall_3_result_t
gunyah_hyp_test_hypercall_3(uint64_t in0)
{
	const register register_t in_x0_ __asm__("x0") = (register_t)in0;
	register uint64_t	  out_x0_ __asm__("x0");
	register uint32_t	  out_x1_ __asm__("x1");
	register error_code_t	  out_x2_ __asm__("x2");

	__asm__ volatile("hvc 0x6002"
			 : "=r"(out_x0_), "=r"(out_x1_), "=r"(out_x2_)
			 : "r"(in_x0_)
			 : "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10",
			   "x11", "x12", "x13", "x14", "x15", "x16", "x17");

	return (gunyah_hyp_test_hypercall_3_result_t){
		.out0  = (uint64_t)out_x0_,
		.out1  = (count_t)out_x1_,
		.error = (error_code_t)out_x2_,
	};
}
