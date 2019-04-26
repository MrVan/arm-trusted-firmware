/*
 * Copyright 2019 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <mmio.h>
#include <spinlock.h>
#include <string.h>
#include <imx_sip.h>
#include <utils_def.h>
#include <errno.h>
#include <limits_.h>

#include "imx8mm-clock.h"

enum imx_clk_type {
	CLK_INTPLL		= 0x1,
	CLK_MUX			= 0x1 << 1,
	CLK_FRACPLL		= 0x1 << 2,
	CLK_DIVIDER		= 0x1 << 3,
	CLK_FIXED		= 0x1 << 4,
	CLK_HAS_GATE		= 0x1 << 5,
	CLK_FIXED_DIVIDER_20	= 0x1 << 6,
	CLK_FIXED_DIVIDER_10	= 0x1 << 7,
	CLK_FIXED_DIVIDER_8	= 0x1 << 8,
	CLK_FIXED_DIVIDER_6	= 0x1 << 9,
	CLK_FIXED_DIVIDER_5	= 0x1 << 10,
	CLK_FIXED_DIVIDER_4	= 0x1 << 11,
	CLK_FIXED_DIVIDER_3	= 0x1 << 12,
	CLK_FIXED_DIVIDER_2	= 0x1 << 13,
	CLK_FIXED_DIVIDER_1	= 0x1 << 14,
	CLK_BUS			= 0x1 << 15,
	CLK_COMPOSITE		= 0x1 << 16,
	CLK_CRITICAL		= 0x1 << 17,
	CLK_BASIC		= 0x1 << 18,
	CLK_PLL_DIV		= 0x1 << 19,
	CLK_IP			= 0x1 << 20,
	CLK_CCGR		= 0x1 << 21,
};

struct imx_clk {
	int array_index;
	char *name;
	uint32_t flags;
	uint32_t freq;
	/*addr or gate*/
	uint32_t addr;
	bool enabled;
	uint32_t index;
	uint32_t use_cnt;
	uint32_t parent[8];
};

#define IMX_CLK_GATE
#define IMX_CLK_MUX
#define IMX_CLK_REPARENT
#define IMX_CLK_DIVIDER
#define IMX_CLK_CRITICAL

#define IMX_CLK_FACTOR_POWER_OF_TWO
#define IMX_CLK_FACTOR_FIXED

#define PROTOCOL_VERSION		0
#define PROTOCOL_ATTRIBUTES		1
#define PROTOCOL_MESSAGE_ATTRIBUTES	2
#define CLOCK _ATTRIBUTES	3
#define CLOCK_DESCRIBE_RATES	4
#define CLOCK_RATE_SET		5
#define CLOCK_RATE_GET		6
#define CLOCK_CONFIG_SET	7

#define SUCCESS			0
#define NOT_SUPPORTED		-1
#define INVALID_PARAMETERS	-2
#define DENIED			-3
#define NOT_FOUND		-4
#define OUT_OF_RANGE		-5
#define BUSY			-6
#define COMMS_ERROR		-7
#define GENERIC_ERROR		-8
#define HARDWARE_ERROR		-9
#define PROTOCOL_ERROR		-10

#define CLK_P8(ARRAY_INDEX, CLK_NAME, CLK_FLAGS, ADDR, ENABLED, INDEX, USE_CNT, P0, P1, P2, P3, P4, P5, P6, P7)	\
	{	\
		.array_index = ARRAY_INDEX,	\
		.name = CLK_NAME,		\
		.flags = CLK_FLAGS,		\
		.addr = ADDR,			\
		.enabled = ENABLED,		\
		.index = INDEX,			\
		.use_cnt = USE_CNT,		\
		.parent[0] = P0,		\
		.parent[1] = P1,		\
		.parent[2] = P2,		\
		.parent[3] = P3,		\
		.parent[4] = P4,		\
		.parent[5] = P5,		\
		.parent[6] = P6,		\
		.parent[7] = P7,		\
	}

#define CLK_P1(ARRAY_INDEX, CLK_NAME, CLK_FLAGS, ADDR, ENABLED, INDEX, USE_CNT, P0)	\
	{	\
		.array_index = ARRAY_INDEX,	\
		.name = CLK_NAME,		\
		.flags = CLK_FLAGS,		\
		.addr = ADDR,			\
		.enabled = ENABLED,		\
		.index = INDEX,			\
		.use_cnt = USE_CNT,		\
		.parent[0] = P0,		\
		.parent[1] = -1,		\
		.parent[2] = -1,		\
		.parent[3] = -1,		\
		.parent[4] = -1,		\
		.parent[5] = -1,		\
		.parent[6] = -1,		\
		.parent[7] = -1,		\
	}

#define CLK_P2(ARRAY_INDEX, CLK_NAME, CLK_FLAGS, ADDR, ENABLED, INDEX, USE_CNT, P0, P1)	\
	{	\
		.array_index = ARRAY_INDEX,	\
		.name = CLK_NAME,		\
		.flags = CLK_FLAGS,		\
		.addr = ADDR,			\
		.enabled = ENABLED,		\
		.index = INDEX,			\
		.use_cnt = USE_CNT,		\
		.parent[0] = P0,		\
		.parent[1] = P1,		\
		.parent[2] = -1,		\
		.parent[3] = -1,		\
		.parent[4] = -1,		\
		.parent[5] = -1,		\
		.parent[6] = -1,		\
		.parent[7] = -1,		\
	}

#define COMPOSITE_MUX_SHIFT	24
#define COMPOSITE_MUX_MASK	0x7

#define COMPOSITE_POST_SHIFT	0
#define COMPOSITE_POST_MASK	0x3F

#define COMPOSITE_PRE_SHIFT	16
#define COMPOSITE_PRE_MASK	0x7

#define COMPOSITE_GATE_SHIFT	28

/* We bind the gate the pll_ref together */
struct imx_clk imx_anatop_clk[] = {
	CLK_P1(0, "osc_24m1", CLK_BASIC | CLK_FIXED , 0, false, IMX8MM_CLK_24M, 0, -1),
	CLK_P2(1, "audio_pll1_out1", CLK_FRACPLL | CLK_HAS_GATE, 0x30360000, false, IMX8MM_AUDIO_PLL1, 0, 0, 13),
	CLK_P2(2, "audio_pll2_out1", CLK_FRACPLL | CLK_HAS_GATE, 0x30360014, false, IMX8MM_AUDIO_PLL2, 0, 0, 13),
	CLK_P2(3, "video_pll1_out1", CLK_FRACPLL | CLK_HAS_GATE, 0x30360028, false, IMX8MM_VIDEO_PLL1, 0, 0, 13),
	CLK_P2(4, "dram_pll_out1", CLK_FRACPLL | CLK_HAS_GATE, 0x30360050, false, IMX8MM_DRAM_PLL, 0, 0, 13),
	CLK_P2(5, "gpu_pll_out1", CLK_INTPLL | CLK_HAS_GATE, 0x30360064, false, IMX8MM_GPU_PLL, 0, 0, 11),
	CLK_P2(6, "vpu_pll_out1", CLK_INTPLL | CLK_HAS_GATE, 0x30360074, false, IMX8MM_VPU_PLL, 0, 0, 11),
	CLK_P2(7, "arm_pll_out1", CLK_INTPLL | CLK_HAS_GATE, 0x30360084, false, IMX8MM_ARM_PLL, 0, 0, 11),
	CLK_P2(8, "sys_pll1_800m1", CLK_INTPLL | CLK_HAS_GATE, 0x30360094, false, IMX8MM_SYS_PLL1, 0, 0, 11),
	CLK_P2(9, "sys_pll2_1000m1", CLK_INTPLL | CLK_HAS_GATE, 0x30360104, false, IMX8MM_SYS_PLL2, 0, 0, 11),
	CLK_P2(10, "sys_pll3_out1", CLK_INTPLL | CLK_HAS_GATE, 0x30360114, false, IMX8MM_SYS_PLL3, 0, 0, 11),

	/* there is gate actually, but we enabled them all */
	CLK_P2(11, "sys_pll1_40m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_20, 0x30360094, false, IMX8MM_SYS_PLL1_40M, 0, 8, 27),
	CLK_P2(12, "sys_pll1_80m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_10, 0x30360094, false, IMX8MM_SYS_PLL1_80M, 0, 8, 25),
	CLK_P2(13, "sys_pll1_100m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_8, 0x30360094, false, IMX8MM_SYS_PLL1_100M, 0, 8, 23),
	CLK_P2(14, "sys_pll1_133m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_6, 0x30360094, false, IMX8MM_SYS_PLL1_133M, 0, 8, 21),
	CLK_P2(15, "sys_pll1_160m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_5, 0x30360094, false, IMX8MM_SYS_PLL1_160M, 0, 8, 19),
	CLK_P2(16, "sys_pll1_200m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_4, 0x30360094, false, IMX8MM_SYS_PLL1_200M, 0, 8, 17),
	CLK_P2(17, "sys_pll1_266m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_3, 0x30360094, false, IMX8MM_SYS_PLL1_266M, 0, 8, 15),
	CLK_P2(18, "sys_pll1_400m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_2, 0x30360094, false, IMX8MM_SYS_PLL1_400M, 0, 8, 13),

	CLK_P2(20, "sys_pll2_50m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_20, 0x30360104, false, IMX8MM_SYS_PLL2_50M, 0, 9, 27),
	CLK_P2(21, "sys_pll2_100m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_10, 0x30360104, false, IMX8MM_SYS_PLL2_100M, 0, 9, 25),
	CLK_P2(22, "sys_pll2_125m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_8, 0x30360104, false, IMX8MM_SYS_PLL2_125M, 0, 9, 23),
	CLK_P2(23, "sys_pll2_166m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_6, 0x30360104, false, IMX8MM_SYS_PLL2_166M, 0, 9, 21),
	CLK_P2(24, "sys_pll2_200m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_5, 0x30360104, false, IMX8MM_SYS_PLL2_200M, 0, 9, 19),
	CLK_P2(25, "sys_pll2_250m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_4, 0x30360104, false, IMX8MM_SYS_PLL2_250M, 0, 9, 17),
	CLK_P2(26, "sys_pll2_333m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_3, 0x30360104, false, IMX8MM_SYS_PLL2_333M, 0, 9, 15),
	CLK_P2(27, "sys_pll2_500m1", CLK_PLL_DIV | CLK_HAS_GATE | CLK_FIXED_DIVIDER_2, 0x30360104, false, IMX8MM_SYS_PLL2_500M, 0, 9, 13),
};

/* Parent is in anatop */
struct imx_clk imx_ccm_clk[] = {
	CLK_P8(0, "nand_usdhc_bus1", CLK_BUS | CLK_CRITICAL | CLK_COMPOSITE, 0x30388900, false, IMX8MM_CLK_NAND_USDHC_BUS, 0, 0, 17, 8, 23, 14, 10, 24, 1),
	CLK_P8(1, "usdhc31", CLK_IP | CLK_COMPOSITE, 0x3038bc80, false, IMX8MM_CLK_USDHC3, 0, 0, 18, 8, 27, 10, 17, 2, 13),
	CLK_P8(2, "usdhc21", CLK_IP | CLK_COMPOSITE, 0x3038ac80, false, IMX8MM_CLK_USDHC2, 0, 0, 18, 8, 27, 10, 17, 2, 13),
};

/* Name is max 16 bytes including NULL */
struct imx_clk imx_ccgr_clk[] = {
	CLK_P1(0, "usdhc3_root", CLK_CCGR, 0x303845e0, false, IMX8MM_CLK_USDHC3_ROOT, 0, 1),
	CLK_P1(1, "usdhc2_root", CLK_CCGR, 0x30384520, false, IMX8MM_CLK_USDHC2_ROOT, 0, 2),
};

/* sys_pll1 sys_pll2 are always enabled */

int clk_get_parent(struct imx_clk *clk)
{
	return 0;
}

#define GNRL_CTL	0x0
#define DIV_CTL		0x4
#define LOCK_STATUS	BIT(31)
#define LOCK_SEL_MASK	BIT(29)
#define CLKE_MASK	BIT(11)
#define RST_MASK	BIT(9)
#define BYPASS_MASK	BIT(4)
#define MDIV_SHIFT	12
#define MDIV_MASK	GENMASK(21, 12)
#define PDIV_SHIFT	4
#define PDIV_MASK	GENMASK(9, 4)
#define SDIV_SHIFT	0
#define SDIV_MASK	GENMASK(2, 0)
#define KDIV_SHIFT	0
#define KDIV_MASK	GENMASK(15, 0)

#define LOCK_TIMEOUT_US		10000

int clk_get_rate(struct imx_clk *clk, uint64_t *rate)
{
	uint32_t val, mux, pre_div, post_div, gate;
	struct imx_clk *p_clk;
	uint64_t p_rate = 0;
	int ret;
	uint32_t pll_gnrl_ctl, pll_div_ctl0, pll_div_ctl1;
	uint32_t mdiv, pdiv, sdiv, kdiv, pll_gnrl, pll_div;
	uint64_t fvco;
	
	*rate = 0;

	if (!clk)
		return 0;

	/* clk gate, return parent's clk */
	if (clk->flags & CLK_FRACPLL) {
		/*Parent 1 will be used as the gate shift */
		pll_gnrl_ctl = mmio_read_32(clk->addr);
#if 1
		if (!(pll_gnrl_ctl & (1 << clk->parent[1])))
			return 0;
#if 0
		if ((pll_gnrl_ctl & RST_MASK) == 0)
			return 0;
#endif
#endif
		if (pll_gnrl_ctl & BYPASS_MASK) {
			*rate = 24000000;
			return 0;
		}

		pll_div_ctl0 = mmio_read_32(clk->addr + 4);
		pll_div_ctl1 = mmio_read_32(clk->addr + 8);

		mdiv = (pll_div_ctl0 & MDIV_MASK) >> MDIV_SHIFT;
		pdiv = (pll_div_ctl0 & PDIV_MASK) >> PDIV_SHIFT;
		sdiv = (pll_div_ctl0 & SDIV_MASK) >> SDIV_SHIFT;
		kdiv = pll_div_ctl1 & KDIV_MASK;

		fvco = 24000000;
		/* fvco = (m * 65536 + k) * Fin / (p * 65536) */
		fvco *= (mdiv * 65536 + kdiv);
		pdiv *= 65536;

		*rate = fvco / (pdiv << sdiv);
	} else if (clk->flags & CLK_INTPLL) {
		/*Parent 1 will be used as the gate shift */
		pll_gnrl = mmio_read_32(clk->addr);
#if 1
		if (!(pll_gnrl & (1 << clk->parent[1])))
			return 0;
#if 0
		if ((pll_gnrl & RST_MASK) == 0)
			return 0;
#endif
#endif
		if (pll_gnrl & BYPASS_MASK) {
			*rate = 24000000;
			return 0;
		}

		pll_div = mmio_read_32(clk->addr + 4);

		mdiv = (pll_div & MDIV_MASK) >> MDIV_SHIFT;
		pdiv = (pll_div & PDIV_MASK) >> PDIV_SHIFT;
		sdiv = (pll_div & SDIV_MASK) >> SDIV_SHIFT;

		fvco = 24000000;
		fvco *= mdiv;
		*rate = fvco / (pdiv << sdiv);
	} else if (clk->flags & CLK_MUX) {
		/* calculate mux and get parent rate */
	} else if (clk->flags & CLK_COMPOSITE) {
		val = mmio_read_32(clk->addr);
		mux = (val >> COMPOSITE_MUX_SHIFT) & COMPOSITE_MUX_MASK;
		pre_div = (val >> COMPOSITE_PRE_SHIFT) & COMPOSITE_PRE_MASK;
		post_div = (val >> COMPOSITE_POST_SHIFT) & COMPOSITE_POST_MASK;
		gate = !!(val & (1 << COMPOSITE_GATE_SHIFT));
		p_clk = &imx_anatop_clk[clk->parent[mux]];
		printf("=== %s %x %x\n", p_clk->name, val, gate);
		/*ignore gate when get rate */
		if (true) {
			ret = clk_get_rate(p_clk, &p_rate);
			if (!ret)
				*rate = p_rate / (pre_div + 1) / (post_div + 1);
		}
	} else if (clk->flags & CLK_BASIC) {
		*rate = 24000000;
		/* top source clk, directly return freq  */
	} else if (clk->flags & CLK_PLL_DIV) {
		uint32_t div;

		if (clk->flags & CLK_FIXED_DIVIDER_1)
			div = 1;
		else if (clk->flags & CLK_FIXED_DIVIDER_2)
			div = 2;
		else if (clk->flags & CLK_FIXED_DIVIDER_3)
			div = 3;
		else if (clk->flags & CLK_FIXED_DIVIDER_4)
			div = 4;
		else if (clk->flags & CLK_FIXED_DIVIDER_5)
			div = 5;
		else if (clk->flags & CLK_FIXED_DIVIDER_6)
			div = 6;
		else if (clk->flags & CLK_FIXED_DIVIDER_8)
			div = 8;
		else if (clk->flags & CLK_FIXED_DIVIDER_10)
			div = 10;
		else if (clk->flags & CLK_FIXED_DIVIDER_20)
			div = 20;
		else
			panic();

		NOTICE("===== %s %x %x\n", clk->name, mmio_read_32(clk->addr), 1 << clk->parent[1]);
		if (clk->flags & CLK_HAS_GATE) {
			if (!(mmio_read_32(clk->addr) & (1 << clk->parent[1]))) {
				NOTICE("===== %s %x %x\n", clk->name, mmio_read_32(clk->addr), 1 << clk->parent[1]);
				*rate = 0;
				return 0;
			}
		}

		p_clk = &imx_anatop_clk[clk->parent[0]];
		ret = clk_get_rate(p_clk, &p_rate);
		NOTICE("p_rate %lu\n", (unsigned long)p_rate);
		if (!ret)
			*rate = p_rate / div;
	} else if (clk->flags & CLK_CCGR) {
#if 0
		val = mmio_read_32(clk->addr);
		/* Ignore gate when get rate */
		if (!(val & 0x3)) {
			*rate = 0;
			NOTICE("rate %lu %s\n", (unsigned long)*rate, clk->name);
		} else {
#endif
		{
			p_clk = &imx_ccm_clk[clk->parent[0]];
			ret = clk_get_rate(p_clk, &p_rate);
			NOTICE("p_rate %lu\n", (unsigned long)p_rate);
			*rate = p_rate;
		}
	}

	NOTICE("%s: %s %lu\n", __func__, clk->name, (unsigned long)*rate);
	return 0;
}

int clk_composite_rate_get(struct imx_clk *clk, uint64_t *rate)
{
	struct imx_clk *parent_clk;
	uint32_t val, post_div, pre_div, mux;
	bool enabled;
	uint64_t p_rate;
	int ret;

	val = mmio_read_32(clk->addr);

	post_div = val & COMPOSITE_POST_MASK;
	pre_div = (val >> COMPOSITE_PRE_SHIFT);
	mux = (val >> COMPOSITE_MUX_SHIFT) & COMPOSITE_MUX_MASK;
	enabled = !!(val & COMPOSITE_GATE_SHIFT);

	if (!enabled)
		*rate = 0;

	if (clk->parent[mux] != -1) {
		parent_clk = &imx_anatop_clk[clk->parent[mux]];
	} else
		parent_clk = NULL;

	ret = clk_get_rate(parent_clk, &p_rate);
	if (ret)
		return 0;

	*rate = p_rate / (pre_div + 1) / (post_div + 1);

	return 0;
}

int clk_config_set(struct imx_clk *clk, bool enable, bool use_cnt)
{
	struct imx_clk *p_clk;
	uint32_t val, mux;
	int ret;

	NOTICE("%s %s %d %d\n", __func__, clk->name, clk->use_cnt, enable);
	/* Already in use */
	if (clk->use_cnt && enable)
		return 0;

	if (clk->flags & CLK_COMPOSITE) {
		if (clk->flags & CLK_CRITICAL) {
			if (!enable) {
				NOTICE("Not support disable\n");
				return -ENOTSUP;
			}
		}

		val = mmio_read_32(clk->addr);
		mux = (val >> COMPOSITE_MUX_SHIFT) & COMPOSITE_MUX_MASK;
		p_clk = &imx_anatop_clk[clk->parent[mux]];
		ret = clk_config_set(p_clk, enable, false);
		if (ret)
			return -ENOTSUP;

		if (enable) {
			val |= (1 << COMPOSITE_GATE_SHIFT);
			if (use_cnt)
				clk->use_cnt++;
		} else {
			val &= ~(1 << COMPOSITE_GATE_SHIFT);
			if (use_cnt)
				clk->use_cnt--;
		}

		mmio_write_32(clk->addr, val);

		return 0;
	} else if (clk->flags & CLK_CCGR) {
		/* TODO: refine to OPS_PARENT_ENABLE */
		p_clk = &imx_ccm_clk[clk->parent[0]];
		ret = clk_config_set(p_clk, true, false);
		if (ret)
			return -ENOTSUP;

		val = mmio_read_32(clk->addr);
		if (enable) {
			val |= 0x3;
			if (use_cnt)
				clk->use_cnt++;
		} else {
			val &= ~0x3;
			if (use_cnt)
				clk->use_cnt--;
		}
		mmio_write_32(clk->addr, val);

		p_clk = &imx_ccm_clk[clk->parent[0]];
		ret = clk_config_set(p_clk, enable, false);
		if (ret)
			return -ENOTSUP;
	}

	return 0;
	/* TODO */
}

#define PCG_PREDIV_SHIFT	16
#define PCG_PREDIV_WIDTH	3
#define PCG_PREDIV_MAX		8

#define PCG_DIV_SHIFT		0
#define PCG_DIV_WIDTH		6
#define PCG_DIV_MAX		64

#define PCG_PCS_SHIFT		24
#define PCG_PCS_MASK		0x7

#define PCG_CGC_SHIFT		28

static int clk_composite_compute_dividers(unsigned long rate,
					  unsigned long parent_rate,
					  int *prediv,
					  int *postdiv)
{
	int div1, div2;
	int error = INT_MAX;
	int ret = -EINVAL;

	*prediv = 1;
	*postdiv = 1;

	for (div1 = 1; div1 <= PCG_PREDIV_MAX; div1++) {
		for (div2 = 1; div2 <= PCG_DIV_MAX; div2++) {
			int new_error = ((parent_rate / div1) / div2) - rate;

			if (new_error < 0)
				new_error = 0 - new_error;
			if (new_error < error) {
				*prediv = div1;
				*postdiv = div2;
				error = new_error;
				ret = 0;
			}
		}
	}
	return ret;
}

/* TODO: refine OPS parent enable case */
int clk_set_rate(struct imx_clk *clk, uint64_t rate)
{
	struct imx_clk *p_clk;
	uint64_t p_rate;
	int prediv, div;
	int ret, i;
	uint32_t val, mux;

	NOTICE("%s %s %d %lu\n", __func__, clk->name, clk->use_cnt, (unsigned long)rate);
	if (clk->use_cnt) {
		NOTICE("%s: %s in use, cnt %d\n", __func__, clk->name, clk->use_cnt);
		return 0;
	}

	if (clk->flags & CLK_CCGR) {
		p_clk = &imx_ccm_clk[clk->parent[0]];
		ret = clk_config_set(p_clk, true, false);
		if (ret)
			return -ENOTSUP;
		ret = clk_set_rate(p_clk, rate);
		if (ret)
			return -ENOTSUP;
	} else if(clk->flags & CLK_COMPOSITE) {
		/* Not prograte to parent */
		val = mmio_read_32(clk->addr);
		mux = (val >> COMPOSITE_MUX_SHIFT) & COMPOSITE_MUX_MASK;
		p_clk = &imx_anatop_clk[clk->parent[mux]];
		ret = clk_get_rate(p_clk, &p_rate);
		if (ret)
			return ret;

		NOTICE("%lu %lu %d\n", (unsigned long)p_rate, (unsigned long)rate, mux);
		if (p_rate < rate) {
			/* TODO: parent number */
			for (i = 0; i < 8; i++) {
				p_clk = &imx_anatop_clk[clk->parent[i]];
				ret = clk_get_rate(p_clk, &p_rate);
				if (ret)
					return ret;
				if (p_rate >= rate)
					break;
			}
			mux = i;
		}

		NOTICE("h1\n");
		ret = clk_composite_compute_dividers(rate, p_rate, &prediv, &div);
		if (ret)
			return ret;

		val &= ~(0x3f | (0x7 << 16));
		val &= ~(COMPOSITE_MUX_MASK << COMPOSITE_MUX_SHIFT);
		val |= (mux << COMPOSITE_MUX_SHIFT);
		val |= (prediv - 1) << 16;
		val |= (div - 1) << PCG_DIV_SHIFT;
		NOTICE("%x\n", val);
		mmio_write_32(clk->addr, val);
		/* TODO */
		return 0;
	}

	return 0;
}

struct imx_clk *find_clk(uint32_t clk_id)
{
	struct imx_clk *clk;
	uint32_t i;

	clk = &imx_anatop_clk[0];

	for (i = 0; i < ARRAY_SIZE(imx_anatop_clk); i++) {
		if (clk_id == imx_anatop_clk[i].index)
			break;
	}

	if (i == ARRAY_SIZE(imx_anatop_clk)) {
		for (i = 0; i < ARRAY_SIZE(imx_ccm_clk); i++) {
			if (clk_id == imx_ccm_clk[i].index)
				break;
		}

		if (i == ARRAY_SIZE(imx_ccm_clk)) {
			clk = NULL;
			for (i = 0; i < ARRAY_SIZE(imx_ccgr_clk); i++) {
				if (clk_id == imx_ccgr_clk[i].index)
					break;
			}
			if (i == ARRAY_SIZE(imx_ccgr_clk))
				return NULL;
			else
				clk = &imx_ccgr_clk[i];
		} else {
			clk = &imx_ccm_clk[i];
		}
	} else {
		clk = &imx_anatop_clk[i];
	}

	return clk;
}

int scmi_clk_handler(uint32_t msg_id, struct response *response, struct scmi_shared_mem *mem)
{
	uint32_t clk_id, attributes, flags;
	struct imx_clk *clk;
	uint64_t rate;

	switch (msg_id) {
	case 0:
		response->status = 0;
		response->data[0] = 0x10000;
		mem->length = 12;
		break;
	case 1: 
		response->status = 0;
		/* TODO: attributes */
		response->data[0] = 0x100ff;
		mem->length = 12;
		break;
	case 3:
		clk_id = *(uint32_t *)&mem->msg_payload[0];

		NOTICE("get clk_id %d\n", clk_id);

		clk = find_clk(clk_id);
		if (!clk) {
			response->status = NOT_FOUND;
			mem->channel_status = 1;
			return 0;
		}

		response->status = 0;

		response->data[0] = clk->enabled;
		memcpy(&response->data[1], clk->name, strlen(clk->name) + 1);
		mem->length = 12 + strlen(clk->name) + 1;

		break;
	case CLOCK_DESCRIBE_RATES:
		clk_id = *(uint32_t *)&mem->msg_payload[0];
		/*rate_index = *((uint32_t *)&mem->msg_payload[0] + 1);*/
		response->status = 0;
		/* TODO */
		response->data[0]= 0;
		response->data[1]= 400000000;
		response->data[2]= 0;
		mem->length = 20;
		break;
	case CLOCK_RATE_SET:
		flags = *(uint32_t *)&mem->msg_payload[0];
		clk_id = *((uint32_t *)&mem->msg_payload[0] + 1);
		rate = *((uint32_t *)&mem->msg_payload[0] + 3);
		rate = rate << 32 | *((uint32_t *)&mem->msg_payload[0] + 2);

		NOTICE("set %p %p\n", mem->msg_payload + 8, mem->msg_payload + 12);
		NOTICE("set clk_id %d rate %u %u  %lu %d\n", clk_id, *(((uint32_t *)&mem->msg_payload[0]) + 2), *(((uint32_t *)&mem->msg_payload[0]) + 3), (unsigned long)rate, flags);
		clk = find_clk(clk_id);
		if (!clk) {
			response->status = NOT_FOUND;
			mem->channel_status = 1;
			return 0;
		}

		clk_set_rate(clk, rate);

		NOTICE("=== %lu\n", (unsigned long)rate);
		response->status = 0;
		mem->length = 8;
		break;
	case CLOCK_RATE_GET:
		clk_id = *(uint32_t *)&mem->msg_payload[0];

		NOTICE("get clk_id %d\n", clk_id);
		clk = find_clk(clk_id);
		if (!clk) {
			response->status = NOT_FOUND;
			mem->channel_status = 1;
			return 0;
		}

		clk_get_rate(clk, &rate);

		NOTICE("=== %lu\n", (unsigned long)rate);
		response->data[0]= rate & 0xffffffff;
		response->data[1]= rate >> 32;
		response->status = 0;
		mem->length = 16;
		break;
	case CLOCK_CONFIG_SET:
		clk_id = *(uint32_t *)&mem->msg_payload[0];
		attributes = *((uint32_t *)&mem->msg_payload[0] + 1);

		NOTICE("get clk_id %d\n", clk_id);
		clk = find_clk(clk_id);
		if (!clk) {
			response->status = NOT_FOUND;
			mem->channel_status = 1;
			return 0;
		}
		
		/* Enable clock */
		if (attributes & BIT_32(0)) {
			clk_config_set(clk, true, true);
		} else {
			if (!clk->use_cnt)
				panic();
			clk_config_set(clk, false, true);
		}

		response->status = 0;
		mem->length = 8;
		break;
	default:
		response->status = NOT_SUPPORTED;
		break;
	}

	mem->channel_status = 1;
	return 0;
}
