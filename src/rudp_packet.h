/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_PACKET_IMPL_H
#define RUDP_PACKET_IMPL_H

#include <stdlib.h>
#include <rudp/list.h>
#include <rudp/packet.h>
#include "rudp_rudp.h"
#include "rudp_packet.h"

#define RUDP_RECV_BUFFER_SIZE 4096

struct rudp_packet_chain *rudp_packet_chain_alloc(
    struct rudp *rudp,
    size_t alloc);

void rudp_packet_chain_free(
    struct rudp *rudp,
    struct rudp_packet_chain *pc);

#endif
