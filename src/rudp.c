/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#include <rudp/rudp.h>
#include <rudp/packet.h>
#include <rudp/time.h>
#include "rudp_list.h"
#include "rudp_rudp.h"

#include <stdlib.h>

rudp_error_t rudp_init(
    struct rudp *rudp,
    struct ela_el *el,
    const struct rudp_handler *handler)
{
    rudp->handler = handler;
    rudp->el = el;

    rudp_list_init(&rudp->free_packet_list);
    rudp->free_packets = 0;
    rudp->allocated_packets = 0;

    rudp->seed = rudp_timestamp();
    rudp_random(rudp);
    rudp_random(rudp);
    rudp_random(rudp);

    return 0;
}

static
void *_rudp_default_alloc(struct rudp *rudp, size_t len)
{
    return malloc(len);
}

static
void _rudp_default_free(struct rudp *rudp, void *buffer)
{
    return free(buffer);
}

RUDP_EXPORT
const struct rudp_handler rudp_handler_default =
{
    .log = NULL,
    .mem_alloc = _rudp_default_alloc,
    .mem_free = _rudp_default_free,
};

void rudp_deinit(struct rudp *rudp)
{
    struct rudp_packet_chain *pc, *tmp;
    rudp_list_for_each_safe(pc, tmp, &rudp->free_packet_list, chain_item) {
        rudp_list_remove(&pc->chain_item);
        rudp_free(rudp, pc);
    }
}

uint16_t rudp_random(struct rudp *rudp)
{
    return rand_r(&rudp->seed);
}
