/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_PACKET_H_
/** @hidden */
#define RUDP_PACKET_H_

/**
   @file
   @module{Packet}
   @short Network packet structure

   Librudp packet format is designed to support the @xref {features}
   {features of the protocol}. All packets contain a fixed header
   containing:
   @list
     @item a command
     @item some flags
     @item a field for acking received packets sequence numbers
     @item two fields for outgoing sequence numbers.
   @end list

   Data follows the header, in an arbitrary format.  Some commands
   require fixed data. See relevant @ref {rudp_command} {command
   definitions}.

   As the 1-byte command code is under-used, user can use any code
   equal or above @ref RUDP_CMD_APP.
 */

#include <stdint.h>
#include <stdlib.h>
#include <rudp/list.h>
#include <rudp/compiler.h>

/**
   Command definition codes
 */
enum rudp_command
{
    /**
       @table 2
       @item @item
       @item Relevant field @item none
       @item Semantic @item useless payload
       @item Notes @item May not be RELIABLE. Useful for keepalives (NAT, etc).
       @end table
     */
    RUDP_CMD_NOOP = 0,

    /**
       @table 2
       @item @item
       @item Relevant field @item none.
       @item Semantic @item Close the session
       @item Expected answer @item None (ACK does the job)
       @item Notes @item May be RELIABLE.
       @end table
     */
    RUDP_CMD_CLOSE = 1,

    /**
       @table 2
       @item @item
       @item Relevant field @item conn_req.
       @item Semantic @item
       @item Expected answer @item conn_rsp
       @item Notes @item Must be RELIABLE
       @end table
     */
    RUDP_CMD_CONN_REQ = 2,

    /**
       @table 2
       @item @item
       @item Relevant field @item conn_rsp.
       @item Semantic @item Acknoledges CONN_REQ
       @item Expected answer @item None
       @item Notes @item Must not be RELIABLE
       @end table
     */
    RUDP_CMD_CONN_RSP = 3,

    /**
       @table 2
       @item @item
       @item Relevant field @item data.
       @item Semantic @item Must answer
       @item Expected answer @item PONG packet with same data
       @item Notes @item May be RELIABLE. Must not be answered for
                         if it was retransmitted. Must still be
                         acknoledged if reliable.
       @end table
     */
    RUDP_CMD_PING = 4,

    /**
       @table 2
       @item @item
       @item Relevant field @item data.
       @item Semantic @item PING answer
       @item Expected answer @item None
       @item Notes @item Must not be RELIABLE.
       @end table
     */
    RUDP_CMD_PONG = 5,

    /**
       @table 2
       @item @item
       @item Relevant field @item data
       @item Semantic @item Normal data packet
       @item Notes @item May be RELIABLE or not, if RELIABLE, must be acked.
              Any packet command number >= RUDP_CMD_APP is permitted
       @end table
     */
    RUDP_CMD_APP = 0x10,
};

/** @mgroup{Flags}
    Packet delivery is reliable. If emission fails, retransmit is
    performed. */
#define RUDP_OPT_RELIABLE 1

/** @mgroup{Flags}
    Packet also contains acknoledge of received packet. ack sequence
    id is in reliable_ack. */
#define RUDP_OPT_ACK 2

/** @mgroup{Flags}
    Packet was retransmitted at least once. */
#define RUDP_OPT_RETRANSMITTED 4

#define RUDP_CMD_APP_MAX (0xff - RUDP_CMD_APP)

/**
   Packet header structure. All fields in this structure should be
   transmitted in network order.
 */
struct rudp_packet_header
{
    uint8_t command;
    uint8_t opt;
    uint16_t reliable_ack;
    uint16_t reliable;
    uint16_t unreliable;
};

/**
   Connection request packet (@xref {protocol}).
 */
struct rudp_packet_conn_req
{
    struct rudp_packet_header header;
    uint32_t data;
};

/**
   Connection response packet (@xref {protocol}).
 */
struct rudp_packet_conn_rsp
{
    struct rudp_packet_header header;
    uint32_t accepted;
};

/**
   Data packet (@xref {protocol}).
 */
struct rudp_packet_data
{
    struct rudp_packet_header header;
    uint8_t data[0];
};

/**
   Structure factoring all the possible packet types.
 */
struct rudp_packet
{
    union {
        struct rudp_packet_header header;
        struct rudp_packet_conn_req conn_req;
        struct rudp_packet_conn_rsp conn_rsp;
        struct rudp_packet_data data;
    };
};

/**
   Packet chain structure
 */
struct rudp_packet_chain
{
    struct rudp_list chain_item;
    struct rudp_packet *packet;
    size_t alloc_size;
    size_t len;
};

/**
   @this retrieves a string matching a command type. This is
   guaranteed to return a valid string even for undefined or user
   command codes.

   @param cmd A command value
   @returns a command name string
 */
RUDP_EXPORT
const char *rudp_command_name(enum rudp_command cmd);

#endif
