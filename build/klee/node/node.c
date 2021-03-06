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


#include "common.h"
#include "helper.h"
#include "macan.h"

#include "macan_ev.h"

#define MACAN_CONFIG_LTKX(NODE_ID) macan_ltk_node ## NODE_ID
#define MACAN_CONFIG_LTK(NODE_ID) MACAN_CONFIG_LTKX(NODE_ID)

enum sig_id {
	SIGNAL_A,
	SIGNAL_B,
	SIG_COUNT
};

enum node_id {
	KEY_SERVER,
	TIME_SERVER,
	NODE1,
	NODE2,
	NODE_COUNT
};

#define TIME_EMIT_SIG 1000000

#define NODE_ID 2

const struct macan_key  macan_ltk_node1 = {
	.data = { 0x00, 0x01, 0x02, 0x09, 0x04, 0x05, 0x06, 0x07,
  	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}
};

struct macan_sig_spec demo_sig_spec[] = {
	[SIGNAL_A]    = {.can_nsid = 0,   .can_sid = 0x200, .src_id = NODE1, .dst_id = NODE2, .presc = 1},
	[SIGNAL_B]    = {.can_nsid = 0,   .can_sid = 0x201, .src_id = NODE2, .dst_id = NODE1, .presc = 1}
};

const struct macan_can_ids demo_can_ids = {
	.time = 0x000,
	.ecu = (struct macan_ecu[]){
		[KEY_SERVER]  = {0x100, "KS"},
		[TIME_SERVER] = {0x101, "TS"},
		[NODE1]          = {0x102, "N1"},
		[NODE2]          = {0x103, "N2"},
	},
};

struct macan_config config = {
	.sig_count         = SIG_COUNT,
	.sigspec           = demo_sig_spec,
	.node_count        = 4,
	.canid         	   = &demo_can_ids,
	.key_server_id     = KEY_SERVER,
	.time_server_id    = TIME_SERVER,
	.time_div          = 1000000,
	.skey_validity     = 60000000,
	.skey_chg_timeout  = 5000000,
	.time_req_sep      = 1000000,
	.time_delta        = 1000000,
};

struct macan_node_config node = {
	.node_id = NODE_ID,
	.ltk = &macan_ltk_node1,
};


int main(){

	macan_ev_loop *loop = MACAN_EV_DEFAULT;

	struct macan_ctx* macan_ctx = macan_alloc_mem(&config, &node);
	macan_init(macan_ctx, loop, 0 /*KLEE doesn't use fd.*/);

	macan_ev_run(loop);

	return 0;
}
