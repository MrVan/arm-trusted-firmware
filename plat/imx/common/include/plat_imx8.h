/*
 * Copyright (c) 2015-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __PLAT_IMX8_H__
#define __PLAT_IMX8_H__

#include <gicv3.h>
#include <psci.h>

struct plat_gic_ctx {
	gicv3_redist_ctx_t rdist_ctx[PLATFORM_CORE_COUNT];
	gicv3_dist_ctx_t dist_ctx;
};

unsigned int plat_calc_core_pos(uint64_t mpidr);
void imx_mailbox_init(uintptr_t base_addr);
void plat_gic_driver_init(void);
void plat_gic_init(void);
void plat_gic_cpuif_enable(void);
void plat_gic_cpuif_disable(void);
void plat_gic_pcpu_init(void);
void plat_gic_save(unsigned int proc_num, struct plat_gic_ctx *ctx);
void plat_gic_restore(unsigned int proc_num, struct plat_gic_ctx *ctx);

void __dead2 imx_system_off(void);
void __dead2 imx_system_reset(void);
int imx_validate_power_state(unsigned int power_state,
			psci_power_state_t *req_state);
void imx_get_sys_suspend_power_state(psci_power_state_t *req_state);
bool imx_is_wakeup_src_irqsteer(void);
void __dead2 imx_pwr_domain_pwr_down_wfi(const psci_power_state_t *target_state);
int imx_get_cpu_rev(uint32_t *cpu_id, uint32_t *cpu_rev);
#endif /*__PLAT_IMX8_H__ */
