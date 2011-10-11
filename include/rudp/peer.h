/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_PEER_H_
/** @hidden */
#define RUDP_PEER_H_

/**
   @file
   @module {Peer}
   @short Peer implementation

   Peer is the symmetrical implementation of the protocol.  A Peer
   context handles the low-level protocol (with retransmits and ACKs).

   Peer context does not directly handle the transport and is abstract
   from the network code.

   Peer contexts use the same @xref {olc} {life cycle} as other
   complex objects of the library.  Before use, peer contexts must be
   initialized with @ref rudp_peer_init, and after use, they must be
   cleaned with @ref rudp_peer_deinit.

   Either because of a connection timeout or because of relevant
   command is received, disconnection can happend and the @tt
   rudp_peer_handler::dropped handler is called back.
 */

#include <rudp/time.h>
#include <rudp/list.h>
#include <rudp/address.h>
#include <rudp/compiler.h>

struct rudp_peer;
struct rudp_endpoint;
struct rudp_link_info;
struct rudp_packet_header;
struct rudp_packet_chain;

/**
   Peer handler code callbacks
 */
struct rudp_peer_handler
{
    /**
       @this is called on packet reception. Packet is already decoded
       and is ensured to be an application packet (@ref RUDP_CMD_APP
       command or equivalent).

       Packet chain ownership is not given to handler, handler must
       copy data and forget the chain afterwards.

       @param peer Peer context
       @param packet Packet descriptor structure
     */
    void (*handle_packet)(
        struct rudp_peer *peer,
        struct rudp_packet_chain *packet);

    /**
       @this is called anytime when link statistics are updated.

       @param peer Peer context
       @param info Link quality updated information
     */
    void (*link_info)(struct rudp_peer *peer, struct rudp_link_info *info);

    /**
       @this is called when connection to the peer drops, either
       explicitely or because of a timeout.

       After this handler is called, peer is left in the initialized
       state.  Handler code can either resend packets (which will use
       reset sequence numbers) or call @ref rudp_peer_deinit.

       @param peer Peer context
     */
    void (*dropped)(struct rudp_peer *peer);
};

/**
   @this is a peer context structure.  User must not use its fields
   directly.

   Peers are objects with the same @xref {olc} {life cycle} as
   most other complex objects of the library.  Before use, peer
   contexts must be initialized with @ref rudp_peer_init, and after
   use, they must be cleaned with @ref rudp_peer_deinit.

   An initialized peer context can send packets to its peer address,
   and can handle incoming packets according to the protocol.  It
   handles all the timeout and retransmit logic.

   @hidecontent
 */
struct rudp_peer
{
    const struct rudp_peer_handler *handler;
    struct rudp_address address;
    struct rudp_endpoint *endpoint;
    rudp_time_t abs_timeout_deadline;
    rudp_time_t last_out_time;
    rudp_time_t srtt;
    rudp_time_t rttvar;
    rudp_time_t rto;
    uint16_t in_seq_reliable;
    uint16_t in_seq_unreliable;
    uint16_t out_seq_reliable;
    uint16_t out_seq_unreliable;
    uint16_t out_seq_acked;
    uint8_t must_ack:1;
    uint8_t scheduled:1;
    uint8_t state;
    struct rudp_list sendq;
    struct rudp *rudp;
    struct ela_event_source *service_source;
    rudp_error_t sendto_err;
};

/**
   @this initializes a peer structure.

   @param peer Peer structure to initialize
   @param rudp A valid rudp context
   @param handler A peer handler descriptor
   @param endpoint Associated endpoint to send the outgoing packets
                   through
 */
RUDP_EXPORT
void rudp_peer_init(
    struct rudp_peer *peer,
    struct rudp *rudp,
    const struct rudp_peer_handler *handler,
    struct rudp_endpoint *endpoint);

/**
   @this initializes a peer structure knowning its remote
   address. This function is an alternative to calling @ref
   rudp_peer_init.

   @param peer Peer structure to initialize
   @param rudp A valid rudp context
   @param addr Peer's address
   @param handler A peer handler descriptor
   @param endpoint Associated endpoint to send the outgoing packets
                   through
 */
RUDP_EXPORT
void rudp_peer_from_sockaddr(
    struct rudp_peer *peer,
    struct rudp *rudp,
    const struct sockaddr_storage *addr,
    const struct rudp_peer_handler *handler,
    struct rudp_endpoint *endpoint);

/**
   @this frees all data associated to a peer structure

   @param peer Peer context to deinit
 */
RUDP_EXPORT
void rudp_peer_deinit(struct rudp_peer *peer);

/**
   @this resets a peer context.

   This resets sequence numbers, state, timers and send queue.
   Resetting a peer can be useful on lost connection, in order to
   start over.

   @param peer Peer context to reset
 */
RUDP_EXPORT
void rudp_peer_reset(struct rudp_peer *peer);

/**
   @this compares the peer address against another address. @see
   rudp_address_compare.

   @param peer Peer to compare
   @param addr Address to compare
   @returns 0 if they match, another value otherwise
 */
RUDP_EXPORT
int rudp_peer_address_compare(const struct rudp_peer *peer,
                              const struct sockaddr_storage *addr);

/**
   @this passes an incoming packet to the peer handler code.

   @param peer Peer context
   @param pc Packet descriptor structure
   @returns a possible error value
 */
RUDP_EXPORT
rudp_error_t rudp_peer_incoming_packet(
    struct rudp_peer *peer,
    struct rudp_packet_chain *pc);

/**
   @this sends unreliable data to a peer.

   Calling code doesn't have to initialize the packet header.

   @param peer Destination peer
   @param pc Packet chain element

   Peer handling code becomes owner of the @tt pc pointer, and may
   free it when needed.
 */
RUDP_EXPORT
rudp_error_t rudp_peer_send_unreliable(
    struct rudp_peer *peer,
    struct rudp_packet_chain *pc);

/**
   @this sends reliable data to a peer.

   Calling code doesn't have to initialize the packet header.

   @param peer Destination peer
   @param pc Packet chain element

   Peer handling code becomes owner of the @tt pc pointer, and may
   free it when needed.
 */
RUDP_EXPORT
rudp_error_t rudp_peer_send_reliable(
    struct rudp_peer *peer,
    struct rudp_packet_chain *pc);

/**
   @this sends a reliable connect packet to a peer.

   @param peer Destination peer
 */
RUDP_EXPORT
rudp_error_t rudp_peer_send_connect(struct rudp_peer *peer);

/**
   @this immediately sends an unreliable @ref RUDP_CMD_CLOSE command,
   bypassing the current retransmit queue.

   @param peer Destination peer
 */
RUDP_EXPORT
rudp_error_t rudp_peer_send_close_noqueue(struct rudp_peer *peer);

#endif
