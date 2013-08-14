/*
 *  Copyright 2014 Czech Technical University in Prague
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

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include "common.h"
#include "aes_keywrap.h"
#ifdef TC1798
#include "can_frame.h"
#include "Std_Types.h"
#include "Mcu.h"
#include "Port.h"
#include "Can.h"
#include "EcuM.h"
#include "Test_Print.h"
#include "Os.h"
#include "she.h"
#else
#include <unistd.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <nettle/aes.h>
#include "aes_cmac.h"
#endif /* TC1798 */
#include "macan.h"
#include "macan_config.h"

/* ToDo
 *   implement groups
 *   some error processing
 */

#ifdef TC1798
# define NODE_ID 2
# define NODE_OTHER 3
# define CAN_IF 2 /* Hth on TC1798, iface name on pc */
#endif

#define WRITE_DELAY 0.5
#define NODE_KS 0
#define NODE_TS 1
#ifndef NODE_ID
# error NODE_TS or NODE_OTHER not defined
#endif /* NODE_ID */
#ifndef CAN_IF
# error CAN_IF not specified
#endif /* CAN_IF */

uint8_t recv_skey_pending = 0;
#define NODE_HAS_KEY 1

uint8_t skey_queue[NODE_MAX + 1] = {0};

/* ltk stands for long term key; it is a key shared with the key server */
uint8_t ltk[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

/* ToDo: aggregate to struct */
uint8_t g_chg[6];

void can_recv_cb(int s, struct can_frame *cf)
{
	struct crypt_frame *cryf = (struct crypt_frame *)cf->data;
	int fwd;

	/* ToDo: macan or plain can */
	/* ToDo: crypto frame or else */

	if (cryf->dst_id != NODE_ID)
		return;

	switch (cryf->flags) {
	case 1:
		receive_challenge(s, cf);
		break;
	case 2:
		if (cf->can_id == NODE_KS) {
			fwd = receive_skey(cf);

			if (fwd != -1) {
				send_ack(s, fwd);
			}
			break;
		}

		if (receive_ack(cf))
			send_ack(s, cf->can_id);
		break;
	case 3:
		if (cf->can_dlc == 7)
			receive_auth_req(cf);
		else
			receive_sig(cf);
		break;
	}
}

uint32_t recv_signal_tab[SIG_MAX];

void recv_sig_cb(uint8_t sid, uint32_t sval)
{
	recv_signal_tab[sid] = sval;
}

void read_signals()
{

}

void operate_ecu(int s)
{
	while(1) {
#ifndef TC1798
		read_can_main(s);
#endif /* TC1798 */
		read_signals();

		/* operate_ecu(); */
		macan_init(s);

		send_sig(s, ENGINE, 55);
		send_sig(s, BRAKE, 66);

		//send_auth_req(s, NODE_OTHER, ENGINE, 0);
#ifndef TC1798
		usleep(250);
#endif /* TC1798 */
	}
}

int main(int argc, char *argv[])
{
	int s;

	s = init();
	operate_ecu(s);

	return 0;
}

