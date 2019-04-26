/*
 * Copyright 2019 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <debug.h>
#include <ddrc.h>
#include <dram.h>
#include <mmio.h>
#include <spinlock.h>
#include <imx_sip.h>

/* TODO: fine grained lock */
spinlock_t scmi_lock;

enum scmi_std_protocol {
	SCMI_PRO_BASE = 0x10,
	SCMI_PRO_POWER = 0x11,
	SCMI_PRO_CLK = 0x14,
};

#define MSG_ID(m)	((m) & 0xff)
#define MSG_TYPE(m)	(((m) >> 8) & 0x3)
#define MSG_PRO_ID(m)	(((m) >> 10) & 0xff)
#define MSG_TOKEN(m)	(((m) >> 18) & 0x3ff)

enum {
	SCMI_POWER_DOMAIN_PROTOCOL	= 0x11,
	SCMI_SYS_PWR_DOMAIN_PROTOCOL	= 0x12,
	SCMI_PER_DOMAIN_PROTOCOL	= 0x13,
	SCMI_CLK_DOMAIN_PROTOCOL	= 0x14,
	SCMI_SENSOR_PROTOCOL		= 0x15,
};

uint8_t protocol[] = { 0x11, 0x14 };

int scmi_handler(uint32_t smc_fid, u_register_t x1, u_register_t x2, u_register_t x3)
{
	NOTICE("%s %x\n", __func__, smc_fid);
	unsigned int *shmem = (unsigned int *)0x93f000;
	struct scmi_shared_mem *mem = (struct scmi_shared_mem *)0x93f000;
	struct response *response = (struct response *)&mem->msg_payload[0];
	uint32_t msg_header;
	uint32_t msg_id, msg_pro_id;
	switch(smc_fid) {
	case FSL_SIP_SCMI_1:
		NOTICE("%x %x %x %x\n", *shmem, *(shmem + 1), *(shmem + 2), *(shmem + 3));
		NOTICE("%x %x %x %x\n", *(shmem + 4), *(shmem + 5), *(shmem + 6), *(shmem + 7));
		msg_header = mem->msg_header;
		msg_id = MSG_ID(msg_header);
		//msg_type = MSG_TYPE(msg_header);
		msg_pro_id = MSG_PRO_ID(msg_header);
		//msg_token = MSG_TOKEN(msg_header);

		NOTICE("pro_id msg_id %x %x\n", msg_pro_id, msg_id);
		if (msg_pro_id == SCMI_PRO_BASE) {
			/* PROTOCAL_VERSION */
			if (msg_id == 0) {
				response->status = 0;
				response->data[0] = 0x10000;
				mem->length = 12;
			} else if (msg_id == 1) {
				response->status = 0;
				/* TODO: only POWER/SYSTEM/CLK now */
				response->data[0]= 3;
				mem->length = 12;
			} else if (msg_id == 3) {
				response->status = 0;
				memcpy(&response->data[0], "NXP", strlen("NXP") + 1);
				mem->length = 2;
			} else if (msg_id == 4) {
				response->status = 0;
				memcpy(&response->data[0], "IMX", strlen("IMX") + 1);
				mem->length = 12;
			} else if (msg_id == 5) {
				response->status = 0;
				/* VERSION 1.00 */
				response->data[0]= 0x100;
				mem->length = 12;
			} else if (msg_id == 6) {
				uint32_t num_skip = ((uint32_t *)&mem->msg_payload[0])[0];
				if (num_skip > 0)
					response->status = -2;
				else
					response->status = 0;
				/**/
				mem->length = 16;
				response->data[0]= sizeof(protocol);
				response->data[1]= (0x14 << 8) | 0x11;
			} else {
				NOTICE("gggggggggggg\n");
				mem->channel_status = 1;
				response->status = -1;
				return 0;
			}

			mem->channel_status = 1;
			return 0;
		} else if (msg_pro_id == SCMI_PRO_POWER) {
			return scmi_pd_handler(msg_id, response, mem);
		} else if (msg_pro_id == SCMI_PRO_CLK) {
			return scmi_clk_handler(msg_id, response, mem);
		}

		NOTICE("msg_id %x not support\n", msg_id);
		mem->channel_status = 1;
		response->status = -1;
		return 0;
	case FSL_SIP_SCMI_2:
		asm volatile("b .\r\n");
	}

	return 0;
}
