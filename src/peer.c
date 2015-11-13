/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#define _BSD_SOURCE

#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rudp/endpoint.h>
#include <rudp/rudp.h>
#include <rudp/address.h>
#include <rudp/peer.h>
#include "rudp_list.h"
#include "rudp_packet.h"

/* Declarations */

static void peer_post_ack(struct rudp_peer *peer);
static rudp_error_t peer_send_raw(
    struct rudp_peer *peer,
    const void *data, size_t len);
static int peer_handle_ack(struct rudp_peer *peer, uint16_t ack);

static void peer_service(struct rudp_peer *peer);
static void _peer_service(struct ela_event_source *src,
                          int fd, uint32_t mask, void *data);
static void peer_service_schedule(struct rudp_peer *peer);

#define ACTION_TIMEOUT 5000
#define DROP_TIMEOUT (ACTION_TIMEOUT * 2)
#define MAX_RTO 3000

enum peer_state
{
    PEER_NEW,
    PEER_RUN,
    PEER_CONNECTING,
    PEER_DEAD,
};

/* Object management */

void rudp_peer_reset(struct rudp_peer *peer)
{
    struct rudp_packet_chain *pc, *tmp;
    rudp_list_for_each_safe(pc, tmp, &peer->sendq, chain_item)
    {
        rudp_list_remove(&pc->chain_item);
        rudp_packet_chain_free(peer->rudp, pc);
    }

    if ( peer->scheduled )
        ela_remove(peer->rudp->el, peer->service_source);
    peer->scheduled = 0;

    peer->abs_timeout_deadline = rudp_timestamp() + DROP_TIMEOUT;
    peer->in_seq_reliable = (uint16_t)-1;
    peer->in_seq_unreliable = 0;
    peer->out_seq_reliable = rudp_random(peer->rudp);
    peer->out_seq_unreliable = 0;
    peer->out_seq_acked = peer->out_seq_reliable - 1;
    peer->state = PEER_NEW;
    peer->last_out_time = rudp_timestamp();
    peer->srtt = 100;
    peer->rttvar = peer->srtt / 2;
    peer->rto = MAX_RTO;
    peer->must_ack = 0;
    peer->sendto_err = 0;
}

void rudp_peer_init(
    struct rudp_peer *peer,
    struct rudp *rudp,
    const struct rudp_peer_handler *handler,
    struct rudp_endpoint *endpoint)
{
    rudp_list_init(&peer->sendq);
    rudp_address_init(&peer->address, rudp);
    peer->endpoint = endpoint;
    peer->rudp = rudp;
    peer->handler = handler;
    ela_source_alloc(rudp->el, _peer_service, peer, &peer->service_source);
    peer->scheduled = 0;

    rudp_peer_reset(peer);

    peer_service_schedule(peer);
}

void rudp_peer_deinit(struct rudp_peer *peer)
{
    rudp_peer_reset(peer);
    rudp_address_deinit(&peer->address);
    ela_source_free(peer->rudp->el, peer->service_source);
    peer->service_source = NULL;
}

static void peer_update_rtt(struct rudp_peer *peer, rudp_time_t last_rtt)
{
    peer->rttvar = (3 * peer->rttvar + labs(peer->srtt - last_rtt)) / 4;
    peer->srtt = (7 * peer->srtt + last_rtt) / 8;
    peer->rto = peer->srtt;
    if ( peer->rto > MAX_RTO )
        peer->rto = MAX_RTO;

    rudp_log_printf(peer->rudp, RUDP_LOG_INFO,
                    "Timeout state: rttvar %d srtt %d rto %d\n",
                    (int)peer->rttvar, (int)peer->srtt, (int)peer->rto);
}

static void peer_rto_backoff(struct rudp_peer *peer)
{
    peer->rto *= 2;
    if ( peer->rto > MAX_RTO )
        peer->rto = MAX_RTO;

    rudp_log_printf(peer->rudp, RUDP_LOG_INFO,
                    "Timeout state: rttvar %d srtt %d rto %d\n",
                    (int)peer->rttvar, (int)peer->srtt, (int)peer->rto);
}

void rudp_peer_from_sockaddr(
    struct rudp_peer *peer,
    struct rudp *rudp,
    const struct sockaddr_storage *addr,
    const struct rudp_peer_handler *handler,
    struct rudp_endpoint *endpoint)
{
    rudp_peer_init(peer, rudp, handler, endpoint);
    rudp_address_set(&peer->address, (struct sockaddr *) addr, sizeof (*addr));
}

/* Sync handling */

enum packet_state
{
    SEQUENCED,
    UNSEQUENCED,
    RETRANSMITTED,
};

static
enum packet_state peer_analyse_reliable(
    struct rudp_peer *peer,
    uint16_t reliable_seq)
{
    if ( peer->in_seq_reliable == reliable_seq )
        return RETRANSMITTED;

    if ( (uint16_t)(peer->in_seq_reliable + 1) != reliable_seq ) {
        rudp_log_printf(peer->rudp, RUDP_LOG_WARN,
                        "%s unsequenced last seq %04x packet %04x\n",
                        __FUNCTION__, peer->in_seq_reliable, reliable_seq);
        return UNSEQUENCED;
    }

    peer->in_seq_reliable = reliable_seq;
    peer->in_seq_unreliable = 0;

    return SEQUENCED;
}

static
enum packet_state peer_analyse_unreliable(
    struct rudp_peer *peer,
    uint16_t reliable_seq,
    uint16_t unreliable_seq)
{
    rudp_log_printf(peer->rudp, RUDP_LOG_IO,
                    "%s rel %04x == %04x, unrel %04x >= %04x\n",
                    __FUNCTION__,
                    peer->in_seq_reliable, reliable_seq,
                    unreliable_seq, peer->in_seq_unreliable);

    if ( peer->in_seq_reliable != reliable_seq )
        return UNSEQUENCED;

    int16_t unreliable_delta = unreliable_seq - peer->in_seq_unreliable;

    if ( unreliable_delta <= 0 )
        return UNSEQUENCED;

    peer->in_seq_unreliable = unreliable_seq;

    return SEQUENCED;
}

/* Send function */

static void peer_ping(struct rudp_peer *peer)
{
    struct rudp_packet_chain *pc = rudp_packet_chain_alloc(
        peer->rudp,
        sizeof(struct rudp_packet_header) + sizeof(rudp_time_t)
        );
    struct rudp_packet_data *data = &pc->packet->data;

    rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                    "%s pushing PING\n", __FUNCTION__);

    rudp_time_t now = rudp_timestamp();
    data->header.command = RUDP_CMD_PING;
    memcpy(data->data, &now, sizeof(now));

    rudp_peer_send_reliable(peer, pc);
}

/* Receiver functions */

static
void peer_handle_ping(
    struct rudp_peer *peer,
    const struct rudp_packet_chain *in)
{
    /*
      We cant take RTT stats from retransmitted packets.
      Generic calling code still generates an ACK.
    */
    if ( in->packet->header.opt & RUDP_OPT_RETRANSMITTED )
        return;

    struct rudp_packet_chain *out =
        rudp_packet_chain_alloc(peer->rudp, in->len);
    struct rudp_packet_header *header = &out->packet->header;

    header->command = RUDP_CMD_PONG;
    header->opt = 0;

    rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                    "%s answering to ping\n", __FUNCTION__);

    memcpy(&out->packet->data.data[0],
           &in->packet->data.data[0],
           in->len - sizeof(struct rudp_packet_header));

    rudp_peer_send_unreliable(peer, out);
}

static
void peer_handle_pong(
    struct rudp_peer *peer,
    const struct rudp_packet_chain *pc)
{
    rudp_time_t orig, delta;
    memcpy(&orig, pc->packet->data.data, sizeof(orig));

    delta = rudp_timestamp() - orig;

    peer_update_rtt(peer, delta);
}

static void peer_service_schedule(struct rudp_peer *peer)
{
    // If nothing in sendq: reschedule service for later
    rudp_time_t delta = ACTION_TIMEOUT;

    // just abuse for_each to get head, if it exists
    struct rudp_packet_chain *head;
    rudp_list_for_each(head, &peer->sendq, chain_item)
    {
        struct rudp_packet_header *header = &head->packet->header;

        if ( header->opt & RUDP_OPT_RETRANSMITTED )
            // already transmitted head, wait for rto
            delta = rudp_timestamp() - peer->last_out_time + peer->rto;
        else
            // transmit asap
            delta = 0;

        // We dont really want to iterate after head
        break;
    }

    rudp_time_t to_delta = peer->abs_timeout_deadline - rudp_timestamp();

    if ( to_delta < delta )
        delta = to_delta;

    if ( delta <= 0 )
        delta = 1;

    rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                    "%s:%d Idle, service scheduled for %d\n",
                    __FUNCTION__, __LINE__,
                    (int)delta);

    struct timeval tv;
    rudp_timestamp_to_timeval(&tv, delta);

    ela_set_timeout(peer->rudp->el, peer->service_source, &tv, ELA_EVENT_ONCE);
    ela_add(peer->rudp->el, peer->service_source);
    peer->scheduled = 1;
}

static
void peer_handle_connreq(
    struct rudp_peer *peer,
    const struct rudp_packet_header *header)
{
    struct rudp_packet_chain *pc =
        rudp_packet_chain_alloc(peer->rudp,
                                sizeof(struct rudp_packet_conn_rsp));
    struct rudp_packet_conn_rsp *response = &pc->packet->conn_rsp;

    response->header.command = RUDP_CMD_CONN_RSP;
    response->header.opt = 0;
    response->accepted = htonl(1);

    rudp_log_printf(peer->rudp, RUDP_LOG_INFO,
                    "%s answering to connreq\n", __FUNCTION__);

    rudp_peer_send_unreliable(peer, pc);

    peer_service_schedule(peer);
}

/*
  - socket watcher
     - endpoint packet reader
        - server packet handler
           - peer packet handler <===
 */
rudp_error_t rudp_peer_incoming_packet(
    struct rudp_peer *peer, struct rudp_packet_chain *pc)
{
    const struct rudp_packet_header *header = &pc->packet->header;

    rudp_log_printf(peer->rudp, RUDP_LOG_IO,
                    "<<< incoming [%d] %s %s (%d) %04x:%04x\n",
                    peer->state,
                    (header->opt & RUDP_OPT_RELIABLE)
                        ? "reliable" : "unreliable",
                    rudp_command_name(header->command), header->command,
                    ntohs(header->reliable), ntohs(header->unreliable));

    if ( header->opt & RUDP_OPT_ACK ) {
        rudp_log_printf(peer->rudp, RUDP_LOG_IO,
                        "    has ACK flag, %04x\n",
                        (int)ntohs(header->reliable_ack));
        int broken = peer_handle_ack(peer, ntohs(header->reliable_ack));
        if ( broken ) {
            rudp_log_printf(peer->rudp, RUDP_LOG_WARN,
                            "    broken ACK flag, ignoring packet\n");
            return EINVAL;
        }
    }

    enum packet_state state;

    if ( header->opt & RUDP_OPT_RELIABLE )
        state = peer_analyse_reliable(peer, ntohs(header->reliable));
    else
        state = peer_analyse_unreliable(peer, ntohs(header->reliable),
                                        ntohs(header->unreliable));

    switch ( state ) {
    case UNSEQUENCED:
        if (peer->state == PEER_NEW
            && header->command == RUDP_CMD_CONN_REQ) {
            // Server side, handling new client
            peer_handle_connreq(peer, header);
            peer->in_seq_reliable = ntohs(header->reliable);
            peer->state = PEER_RUN;
        } else if (peer->state == PEER_CONNECTING
                   && header->command == RUDP_CMD_CONN_RSP) {
            // Client side, handling new server
            peer->in_seq_reliable = ntohs(header->reliable);
            peer_handle_ack(peer, ntohs(header->reliable_ack));
            peer->state = PEER_RUN;
        } else {
            rudp_log_printf(peer->rudp, RUDP_LOG_WARN,
                            "    unsequenced packet in state %d, ignored\n",
                            peer->state);
        }
        break;
//        return RUDP_EINVAL;

    case RETRANSMITTED:
        peer->abs_timeout_deadline = rudp_timestamp() + DROP_TIMEOUT;
        break;

    case SEQUENCED:
        peer->abs_timeout_deadline = rudp_timestamp() + DROP_TIMEOUT;

        switch ( header->command )
        {
        case RUDP_CMD_CLOSE:
            peer->state = PEER_DEAD;
            peer->handler->dropped(peer);
            rudp_log_printf(peer->rudp, RUDP_LOG_INFO,
                            "      peer dropped\n");
            return 0;

            break;

        case RUDP_CMD_PING:
            if ( peer->state == PEER_RUN ) {
                rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                                "       ping\n");
                peer_handle_ping(peer, pc);
            } else {
                rudp_log_printf(peer->rudp, RUDP_LOG_WARN,
                                "       ping while not running\n");
            }
            break;

        case RUDP_CMD_PONG:
            if ( peer->state == PEER_RUN ) {
                rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                                "       pong\n");
                peer_handle_pong(peer, pc);
            } else {
                rudp_log_printf(peer->rudp, RUDP_LOG_WARN,
                                "       pong while not running\n");
            }
            break;

        case RUDP_CMD_NOOP:
        case RUDP_CMD_CONN_REQ:
        case RUDP_CMD_CONN_RSP:
             break;

        default:
            if ( peer->state != PEER_RUN ) {
                rudp_log_printf(peer->rudp, RUDP_LOG_WARN,
                                "       user payload while not running\n");
                break;
            }

            if ( header->command >= RUDP_CMD_APP )
                peer->handler->handle_packet(peer, pc);
        }
    }

    if ( header->opt & RUDP_OPT_RELIABLE ) {
        rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                        "       reliable packet, posting ack\n");
        peer_post_ack(peer);
    }
    
    peer_service_schedule(peer);

    return 0;
}


/* Ack handling function */


static
int peer_handle_ack(struct rudp_peer *peer, uint16_t ack)
{
    int16_t ack_delta = (ack - peer->out_seq_acked);
    int16_t adv_delta = (ack - peer->out_seq_reliable);

    if ( ack_delta < 0 )
        // ack in past
        return 0;

    if ( adv_delta > 0 )
        // packet acking an unsent seq no -- broken packet
        return 1;

    rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                    "%s acked seqno is now %04x\n", __FUNCTION__, ack);

    peer->out_seq_acked = ack;

    struct rudp_packet_chain *pc, *tmp;
    rudp_list_for_each_safe(pc, tmp, &peer->sendq, chain_item)
    {
        struct rudp_packet_header *header = &pc->packet->header;
        uint16_t seqno = ntohs(header->reliable);
        int16_t delta = (seqno - ack);

        // not transmitted yet:
        // - unreliable packets, if they are still here
        // - reliable packet not marked retransmitted
        if ( ! (header->opt & RUDP_OPT_RELIABLE)
             || ! (header->opt & RUDP_OPT_RETRANSMITTED) )
            break;

        rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                        "%s (ack=%04x) considering unqueueing"
                        " packet id %04x, delta %d: %s\n",
                        __FUNCTION__,
                        ack, seqno, delta, delta > 0 ? "no": "yes");

        if ( delta > 0 )
            break;

        rudp_list_remove(&pc->chain_item);
        rudp_packet_chain_free(peer->rudp, pc);
    }

    rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                    "%s left in queue:\n",
                    __FUNCTION__);
    rudp_list_for_each(pc, &peer->sendq, chain_item) {
        struct rudp_packet_header *header = &pc->packet->header;
        rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                        "%s   - %04x:%04x\n",
                        __FUNCTION__,
                        ntohs(header->reliable),
                        ntohs(header->unreliable));
    }
    rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                    "%s ---\n",
                    __FUNCTION__);

    return 0;
}


/*
  Ack field is present in all headers.  Therefore any packet can be an
  ack.  If the send queue is empty, we cant afford to wait for a new
  one, so we send a new NOOP.
 */
static
void peer_post_ack(struct rudp_peer *peer)
{
    peer->must_ack = 1;

    if ( ! rudp_list_empty(&peer->sendq) ) {
        return;
    }

    struct rudp_packet_chain *pc = rudp_packet_chain_alloc(
        peer->rudp, sizeof(struct rudp_packet_conn_rsp));
    struct rudp_packet_header *header = &pc->packet->header;

    rudp_log_printf(peer->rudp, RUDP_LOG_DEBUG,
                    "%s pushing NOOP ACK\n", __FUNCTION__);

    header->command = RUDP_CMD_NOOP;

    rudp_peer_send_unreliable(peer, pc);

    peer_service_schedule(peer);
}


/* Sender functions */

rudp_error_t rudp_peer_send_unreliable(
    struct rudp_peer *peer,
    struct rudp_packet_chain *pc)
{
    pc->packet->header.opt = 0;
    pc->packet->header.reliable = htons(peer->out_seq_reliable);
    pc->packet->header.unreliable = htons(++(peer->out_seq_unreliable));

    rudp_log_printf(peer->rudp, RUDP_LOG_IO,
                    ">>> outgoing unreliable %s (%d) %04x:%04x\n",
                    rudp_command_name(pc->packet->header.command),
                    pc->packet->header.command,
                    ntohs(pc->packet->header.reliable),
                    ntohs(pc->packet->header.unreliable));

    rudp_list_append(&peer->sendq, &pc->chain_item);
    peer_service_schedule(peer);
    return peer->sendto_err;
}

rudp_error_t rudp_peer_send_reliable(
    struct rudp_peer *peer,
    struct rudp_packet_chain *pc)
{
    pc->packet->header.opt = RUDP_OPT_RELIABLE;
    pc->packet->header.reliable = htons(++(peer->out_seq_reliable));
    pc->packet->header.unreliable = 0;
    peer->out_seq_unreliable = 0;

    rudp_log_printf(peer->rudp, RUDP_LOG_IO,
                    ">>> outgoing reliable %s (%d) %04x:%04x\n",
                    rudp_command_name(pc->packet->header.command),
                    pc->packet->header.command,
                    ntohs(pc->packet->header.reliable),
                    ntohs(pc->packet->header.unreliable));

    rudp_list_append(&peer->sendq, &pc->chain_item);
    peer_service_schedule(peer);
    return peer->sendto_err;
}

static
rudp_error_t peer_send_raw(
    struct rudp_peer *peer,
    const void *data, size_t len)
{
    peer->sendto_err = rudp_endpoint_send(
        peer->endpoint, &peer->address, data, len);

    peer->last_out_time = rudp_timestamp();
    return peer->sendto_err;
}

rudp_error_t rudp_peer_send_connect(struct rudp_peer *peer)
{
    struct rudp_packet_chain *pc = rudp_packet_chain_alloc(
        peer->rudp, sizeof(struct rudp_packet_conn_req));
    struct rudp_packet_conn_req *conn_req = &pc->packet->conn_req;

    conn_req->header.command = RUDP_CMD_CONN_REQ;
    conn_req->data = 0;

    peer->state = PEER_CONNECTING;

    return rudp_peer_send_reliable(peer, pc);
}

rudp_error_t rudp_peer_send_close_noqueue(struct rudp_peer *peer)
{
    struct rudp_packet_header header = {
        .command = RUDP_CMD_CLOSE,
        .opt = 0,
    };

    header.opt = 0;
    header.reliable = htons(peer->out_seq_reliable);
    header.unreliable = htons(++(peer->out_seq_unreliable));

    rudp_log_printf(peer->rudp, RUDP_LOG_IO,
                    ">>> outgoing noqueue %s (%d) %04x:%04x\n",
                    rudp_command_name(header.command),
                    ntohs(header.reliable),
                    header.reliable,
                    ntohs(header.unreliable));

    return peer_send_raw(peer, &header, sizeof(header));
}

/* Worker functions */

static void peer_send_queue(struct rudp_peer *peer)
{
    struct rudp_packet_chain *pc, *tmp;
    rudp_list_for_each_safe(pc, tmp, &peer->sendq, chain_item)
    {
        struct rudp_packet_header *header = &pc->packet->header;

        if ( peer->must_ack ) {
            header->opt |= RUDP_OPT_ACK;
            header->reliable_ack = htons(peer->in_seq_reliable);
//            peer->must_ack = 0;
        } else {
            header->reliable_ack = 0;
        }

        rudp_log_printf(peer->rudp, RUDP_LOG_IO,
                        ">>>>>> %ssend %sreliable %s %04x:%04x %s %04x\n",
                        header->opt & RUDP_OPT_RETRANSMITTED ? "RE" : "",
                        header->opt & RUDP_OPT_RELIABLE ? "" : "un",
                        rudp_command_name(pc->packet->header.command),
                        ntohs(pc->packet->header.reliable),
                        ntohs(pc->packet->header.unreliable),
                        header->opt & RUDP_OPT_ACK ? "ack" : "noack",
                        ntohs(pc->packet->header.reliable_ack));

        peer_send_raw(peer, header, pc->len);

        if ( (header->opt & RUDP_OPT_RELIABLE)
             && (header->opt & RUDP_OPT_RETRANSMITTED) ) {
            peer_rto_backoff(peer);
            break;
        }

        if ( header->opt & RUDP_OPT_RELIABLE ) {
            header->opt |= RUDP_OPT_RETRANSMITTED;
//            break;
        } else {
            rudp_list_remove(&pc->chain_item);
            rudp_packet_chain_free(peer->rudp, pc);
        }
    }
}



/*
  Two reasons may bring us here:

  - There are some (un)reliable items in the send queue, either
    - we are retransmitting
    - we just enqueued something and we need to send

  - There is nothing in the send queue and we want to ensure the peer
    is still up
 */
static void peer_service(struct rudp_peer *peer)
{
    peer->scheduled = 0;

    if ( peer->abs_timeout_deadline < rudp_timestamp() ) {
        peer->handler->dropped(peer);
        return;
    }

    if ( rudp_list_empty(&peer->sendq) ) {
        /*
          Nothing was in the send queue, so we may be in a timeout
          situation. Handle retries and final timeout.
        */
        rudp_time_t out_delta = rudp_timestamp() - peer->last_out_time;
        if ( out_delta > ACTION_TIMEOUT )
            peer_ping(peer);
    }

    peer_send_queue(peer);

    peer_service_schedule(peer);
}

static void _peer_service(struct ela_event_source *src,
                          int fd, uint32_t mask, void *data)
{
    return peer_service((struct rudp_peer *)data);
}

int rudp_peer_address_compare(const struct rudp_peer *peer,
                              const struct sockaddr_storage *addr)
{
    return rudp_address_compare(&peer->address, addr);
}
