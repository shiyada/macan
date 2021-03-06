/*
 *  Copyright 2014, 2015 Czech Technical University in Prague
 *
 *  Authors: Michal Sojka <sojkam1@fel.cvut.cz>
 *           Radek Matějka <radek.matejka@gmail.com>
 *           Ondřej Kulatý <kulatond@fel.cvut.cz>
 *
 *  This file is part of MaCAN.
 *
 *  MaCAN is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  MaCAN is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with MaCAN.	If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <macan.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "cryptlib.h"
#include "macan_ev.h"
#include "macan_private.h"

static
void send_time_auth(struct macan_ctx *ctx, macan_ecuid dst_id, uint8_t challenge[6])
{
	struct com_part *cp = ctx->cpart[dst_id];
	uint8_t plain[12];
	struct can_frame canf = {0};

	memcpy(plain, &ctx->ts.bcast_time, 4);
	memcpy(plain + 4, challenge, 6);
	memcpy(plain + 10, &CANID(ctx, ctx->config->time_server_id), 2);

	canf.can_id = ctx->config->canid->time;
	canf.can_dlc = 8;
	memcpy(canf.data, &ctx->ts.bcast_time, 4);
	macan_sign(&cp->skey, canf.data + 4, plain, 12);

	print_msg(ctx, MSG_INFO,"sending signed time to #%d\n", dst_id);

	ctx->ts.auth_req[dst_id].pending = false;

	macan_send(ctx, &canf);
}

static void skey_received(struct macan_ctx *ctx, macan_ecuid dst_id)
{
	if (ctx->ts.auth_req[dst_id].pending) {
		ctx->ts.auth_req[dst_id].pending = false;
		ctx->cpart[dst_id]->skey_callback = NULL;
		send_time_auth(ctx, dst_id, ctx->ts.auth_req[dst_id].chg);
	}
}

static
void ts_receive_challenge(struct macan_ctx *ctx, struct can_frame *cf)
{
	struct macan_challenge *ch = (struct macan_challenge *)cf->data;
	macan_ecuid dst_id;

	if (!macan_canid2ecuid(ctx->config, cf->can_id, &dst_id))
		return;

	if (is_skey_ready(ctx, dst_id))
		send_time_auth(ctx, dst_id, ch->chg);
	else if (ctx->cpart[dst_id]) {
		ctx->cpart[dst_id]->skey_callback = skey_received;
		ctx->ts.auth_req[dst_id].pending = true;
		memcpy(ctx->ts.auth_req[dst_id].chg, ch->chg, sizeof(ch->chg));
	}
}

static
void macan_rx_cb_ts(macan_ev_loop *loop, macan_ev_can *w, int revents)
{
	(void)loop; (void)revents; /* suppress warnings */
	struct macan_ctx *ctx = w->data;
	struct can_frame cf;

	while (macan_read(ctx, &cf)) {
		enum macan_process_status status;

		status = macan_process_frame(ctx, &cf);

		if (status == MACAN_FRAME_CHALLENGE)
			ts_receive_challenge(ctx, &cf);
	}
}

static void
time_broadcast_cb(macan_ev_loop *loop, macan_ev_timer *w, int revents)
{
	(void)loop; (void)revents;
	struct macan_ctx *ctx = w->data;
	struct can_frame cf = {0};
	uint64_t usec;

	usec = read_time();
	ctx->ts.bcast_time = usec / ctx->config->time_div;

	cf.can_id = ctx->config->canid->time;
	cf.can_dlc = 4;
	memcpy(cf.data, &ctx->ts.bcast_time, 4);

	macan_send(ctx, &cf);
}


int macan_init_ts(struct macan_ctx *ctx, macan_ev_loop *loop, int sockfd)
{
	assert(ctx->node->node_id == ctx->config->time_server_id);

	read_time(); /* Ensure that MaCAN time starts before "event loop time" */

	__macan_init(ctx, loop, sockfd);

	macan_ev_canrx_setup(ctx, &ctx->can_watcher, macan_rx_cb_ts);
	macan_ev_timer_setup(ctx, &ctx->housekeeping, macan_housekeeping_cb, 0, 1000);
	macan_ev_timer_setup(ctx, &ctx->ts.time_bcast, time_broadcast_cb, 0, ctx->config->time_div / 1000);

	return 0;
}
