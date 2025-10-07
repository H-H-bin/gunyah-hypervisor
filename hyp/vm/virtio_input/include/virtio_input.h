// © 2023 Qualcomm Innovation Center, Inc. All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

error_t
virtio_input_set_data_sel_ev_bits(count_t		  ev_bits_count,
				  virtio_input_ev_bits_t *ev_bits,
				  uint32_t subsel, uint32_t size,
				  vmaddr_t data);
error_t
virtio_input_set_data_sel_abs_info(count_t		   absinfo_count,
				   virtio_input_absinfo_t *absinfo,
				   uint32_t subsel, uint32_t size,
				   vmaddr_t data);
