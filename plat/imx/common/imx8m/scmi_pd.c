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

#define POWER_STATE_ON	(0 << 30)
#define POWER_STATE_OFF	(1 << 30)

struct power_domain {
	char *name;
	uint32_t name_len;
	uint32_t state;
	bool has_child;
	uint32_t use_cnt;
	int reg;
	int parent;
	/* TODO: add clocks */
};

static struct power_domain pwr_domain[] = {
	{
		.name =  "HSIO_PD",
		.state = POWER_STATE_OFF,
		.reg = 0,
		.has_child = true,
	},
	{
		.name =  "PCIE0_PD",
		.state = POWER_STATE_OFF,
		.reg = 1,
		.parent = 0,
	},
	{
		.name =  "USB_OTG1_PD",
		.reg = 2,
		.state = POWER_STATE_OFF,
		.parent = 0,
		
	},
	{
		.name =  "USB_OTG2_PD",
		.state = POWER_STATE_OFF,
		.reg = 3,
		.parent = 0,
	},
	{
		.name =  "GPUMIX_PD",
		.state = POWER_STATE_OFF,
		.reg = 4,
		.has_child = true,
	},
	{
		.name =  "VPUMIX_PD",
		.state = POWER_STATE_OFF,
		.reg = 5,
		.has_child = true,
	},
	{
		.name =  "VPU_G1_PD",
		.state = POWER_STATE_OFF,
		.reg = 6,
		.parent = 5,
	},
	{
		.name =  "VPG_G2_PD",
		.state = POWER_STATE_OFF,
		.reg = 7,
		.parent = 5,
	},
	{
		.name =  "VPG_H1_PD",
		.state = POWER_STATE_OFF,
		.reg = 8,
		.parent = 5,
	},
	{
		.name =  "DISPMIX_PD",
		.state = POWER_STATE_OFF,
		.reg = 9,
		.has_child = true,
	},
	{
		.name =  "MIPI_PD",
		.state = POWER_STATE_OFF,
		.reg = 10,
		.parent = 9,
	}
};

int scmi_pd_handler(uint32_t msg_id, struct response *response, struct scmi_shared_mem *mem)
{
	if (msg_id == 0) {
		response->status = 0;
		response->data[0] = 0x10000;
		mem->length = 12;
	} else if (msg_id == 1) {
		response->status = 0;
		/* 11 domains, should we shrink to only leaf domain? */
		response->data[0] = 11;
		response->data[1] = 0x93fc00;
		response->data[2] = 0x0;
		response->data[3] = 0x400;
		mem->length = 24;
	} else if (msg_id == 3) {
		uint32_t domain_id =  *(uint16_t *)&mem->msg_payload[0];
		NOTICE("get domain_id %d\n", domain_id);
		response->status = 0;
		response->data[0] = 1 << 29;
		/* Fix according to each domain name*/
		mem->length = 32;
		if (domain_id < 10)
			memcpy(&response->data[1], pwr_domain[domain_id].name, strlen(pwr_domain[domain_id].name) + 1);
		else {
			response->status = -1;
			NOTICE("error domain id\n");
		}
	} else if (msg_id == 4) {
		uint32_t flags =  *(uint32_t *)&mem->msg_payload[0];
		uint32_t domain_id = *((uint32_t *)&mem->msg_payload[0] + 1);
		uint32_t power_state = *((uint32_t *)&mem->msg_payload[0] + 2);

		NOTICE("get flags %d domain_id %d power_state %d\n", flags, domain_id, power_state);
		/* TODO */
		response->status = 0;
		mem->length = 8;
	} else if (msg_id == 5) {
		uint32_t domain_id =  *(uint32_t *)&mem->msg_payload[0];
		NOTICE("get domain_id %d\n", domain_id);
		response->status = 0;
		response->data[0] = pwr_domain[domain_id].state;
		mem->length = 12;
	} else {
		NOTICE("kkkkkk\n");
		mem->channel_status = 1;
		response->status = -1;
		return 0;
	}

	mem->channel_status = 1;
	return 0;
}
