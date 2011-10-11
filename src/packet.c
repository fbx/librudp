/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#include <rudp/packet.h>
#include "rudp_packet.h"
#include "rudp_list.h"

const char *rudp_command_name(enum rudp_command cmd)
{
    switch (cmd) {
    case RUDP_CMD_NOOP: return "RUDP_CMD_NOOP";
    case RUDP_CMD_CLOSE: return "RUDP_CMD_CLOSE";
    case RUDP_CMD_CONN_REQ: return "RUDP_CMD_CONN_REQ";
    case RUDP_CMD_CONN_RSP: return "RUDP_CMD_CONN_RSP";
    case RUDP_CMD_PING: return "RUDP_CMD_PING";
    case RUDP_CMD_PONG: return "RUDP_CMD_PONG";
    case RUDP_CMD_APP: return "RUDP_CMD_APP";
    default:
        if ( (int) cmd < RUDP_CMD_APP )
            return "RUDP_CMD_invalid";
        else
            return "RUDP_CMD_APP_CUSTOM";
    }
}

#define DEFAULT_ALLOC_SIZE (RUDP_RECV_BUFFER_SIZE)
#define FREE_PACKET_POOL 10

struct rudp_packet_chain *rudp_packet_chain_alloc(
    struct rudp *rudp,
    size_t asked)
{
    size_t alloc = asked;
    struct rudp_packet_chain *pc;

    if ( alloc < DEFAULT_ALLOC_SIZE )
        alloc = DEFAULT_ALLOC_SIZE;

    if ( alloc == DEFAULT_ALLOC_SIZE ) {
        // We'll pass this is the list is empty as well
        rudp_list_for_each(pc, &rudp->free_packet_list, chain_item) {
            rudp_list_remove(&pc->chain_item);
            rudp->free_packets--;
            goto found;
        }
    }

    pc = rudp_alloc(rudp, sizeof(*pc)+alloc);
    if ( pc == NULL )
        return NULL;

    rudp->allocated_packets++;

    pc->packet = (void*)(pc+1);
    pc->alloc_size = alloc;

found:
    pc->len = asked;
    return pc;
}

void rudp_packet_chain_free(struct rudp *rudp, struct rudp_packet_chain *pc)
{
    if ( pc->alloc_size == DEFAULT_ALLOC_SIZE ) {
        rudp_list_insert(&rudp->free_packet_list, &pc->chain_item);
        rudp->free_packets++;
    } else {
        rudp_free(rudp, pc);
        rudp->allocated_packets--;
    }

    while ( rudp->free_packets > FREE_PACKET_POOL ) {
        rudp_list_for_each(pc, &rudp->free_packet_list, chain_item) {
            rudp_list_remove(&pc->chain_item);
            rudp->free_packets--;
            rudp->allocated_packets--;
            rudp_free(rudp, pc);
            break;
        }
    }
}
