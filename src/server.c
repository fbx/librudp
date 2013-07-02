/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */


#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rudp/packet.h>
#include <rudp/server.h>
#include <rudp/peer.h>
#include "rudp_list.h"
#include "rudp_packet.h"
#include "rudp_rudp.h"

struct server_peer
{
    struct rudp_peer base;
    struct rudp_list server_item;
    struct rudp_server *server;
    void *user_data;
};

static const struct rudp_endpoint_handler server_endpoint_handler;

rudp_error_t rudp_server_init(
    struct rudp_server *server,
    struct rudp *rudp,
    const struct rudp_server_handler *handler)
{
    rudp_endpoint_init(&server->endpoint, rudp, &server_endpoint_handler);
    rudp_list_init(&server->peer_list);
    server->handler = handler;
    server->rudp = rudp;
    return 0;
}

rudp_error_t rudp_server_bind(struct rudp_server *server)
{
    rudp_error_t err = rudp_endpoint_bind(&server->endpoint);

    if ( err )
        rudp_log_printf(server->endpoint.rudp, RUDP_LOG_ERROR,
                        "Binding of server to %s failed\n",
                        rudp_address_text(&server->endpoint.addr));
    else
        rudp_log_printf(server->endpoint.rudp, RUDP_LOG_INFO,
                        "Bound server to %s\n",
                        rudp_address_text(&server->endpoint.addr));

    return err;
}

static void server_peer_forget(struct rudp_server *server,
                               struct server_peer *peer)
{
    rudp_list_remove(&peer->server_item);
    rudp_peer_deinit(&peer->base);
    rudp_free(server->rudp, peer);
}

void rudp_server_client_close(struct rudp_server *server,
                            struct rudp_peer *_peer)
{
    struct server_peer *peer = (struct server_peer *)_peer;

    server_peer_forget(server, peer);
}

rudp_error_t rudp_server_close(struct rudp_server *server)
{
    struct server_peer *peer, *tmp;
    rudp_list_for_each_safe(peer, tmp, &server->peer_list, server_item)
    {
        peer->server->handler->peer_dropped(peer->server, &peer->base);
        rudp_server_client_close(server, &peer->base);
    }

    rudp_endpoint_close(&server->endpoint);
    return 0;
}

rudp_error_t rudp_server_deinit(struct rudp_server *server)
{
    rudp_endpoint_deinit(&server->endpoint);
    rudp_list_init(&server->peer_list);
    return 0;
}

static
struct server_peer *rudp_server_peer_lookup(struct rudp_server *server,
                                          const struct sockaddr_storage *addr)
{
    struct server_peer *peer;
    rudp_list_for_each(peer, &server->peer_list, server_item)
    {
        if ( ! rudp_peer_address_compare(&peer->base, addr) )
            return peer;
    }
    return NULL;
}

static
void server_handle_data_packet(struct rudp_peer *_peer,
                               struct rudp_packet_chain *pc)
{
    struct server_peer *peer = (struct server_peer *)_peer;
    struct rudp_packet_data *header = &pc->packet->data;

    peer->server->handler->handle_packet(
        peer->server, &peer->base,
        header->header.command - RUDP_CMD_APP,
        header->data, pc->len - sizeof(header));
}

static
void server_link_info(struct rudp_peer *_peer,
                      struct rudp_link_info *info)
{
    struct server_peer *peer = (struct server_peer *)_peer;

    peer->server->handler->link_info(peer->server, _peer, info);
}

static
void server_peer_dropped(struct rudp_peer *_peer)
{
    struct server_peer *peer = (struct server_peer *)_peer;

    rudp_log_printf(peer->base.rudp, RUDP_LOG_INFO, "Peer dropped\n");

    peer->server->handler->peer_dropped(peer->server, _peer);

    server_peer_forget(peer->server, peer);
}

static const struct rudp_peer_handler server_peer_handler = {
    .handle_packet = server_handle_data_packet,
    .link_info = server_link_info,
    .dropped = server_peer_dropped,
};

static struct server_peer *server_peer_new(struct rudp_server *server,
                                           const struct sockaddr_storage *addr)
{
    struct server_peer *peer = rudp_alloc(server->rudp, sizeof(*peer));

    if ( peer == NULL )
        return NULL;

    rudp_peer_from_sockaddr(
        &peer->base, server->rudp,
        addr, &server_peer_handler,
        &server->endpoint);

    rudp_log_printf(server->rudp, RUDP_LOG_INFO, "New connection\n");

    rudp_list_insert(&server->peer_list, &peer->server_item);

    peer->server = server;
    peer->user_data = NULL;

    return peer;
}

/*
  - socket watcher
     - endpoint packet reader
        - server packet handler <===
           - new peer
           - peer packet handler

  If we can't find the source address, packet may be from a new peer
  or maybe garbage data.  We wont know until we pass the handshaking.
  Create a peer and answer unless we are absolutely sure it is
  garbage.
 */
static
void server_handle_endpoint_packet(struct rudp_endpoint *endpoint,
                                           const struct sockaddr_storage *addr,
                                           struct rudp_packet_chain *pc)
{
    struct rudp_server *server = __container_of(endpoint, server, endpoint);
    struct server_peer *peer = rudp_server_peer_lookup(server, addr);
    rudp_error_t err;

    if ( peer != NULL ) {
        rudp_peer_incoming_packet(&peer->base, pc);
        return;
    }

    struct rudp_packet_header *header = &pc->packet->header;

    if ( pc->len != sizeof(struct rudp_packet_conn_req)
         || header->command != RUDP_CMD_CONN_REQ )
        goto garbage;

    peer = server_peer_new(server, addr);
    if ( peer == NULL )
        return;

    err = rudp_peer_incoming_packet(&peer->base, pc);
    if ( err == 0 )
        server->handler->peer_new(server, &peer->base);
    else
        server_peer_forget(server, peer);
    return;

garbage:
    rudp_log_printf(server->rudp, RUDP_LOG_DEBUG, "Garbage data\n");
}

static const struct rudp_endpoint_handler server_endpoint_handler = {
    .handle_packet = server_handle_endpoint_packet,
};

/***/

rudp_error_t rudp_server_send(
    struct rudp_server *server,
    struct rudp_peer *peer,
    int reliable, int command,
    const void *data,
    const size_t size)
{
    if ( (command + RUDP_CMD_APP) > 255 )
        return EINVAL;

    struct rudp_packet_chain *pc = rudp_packet_chain_alloc(
        server->rudp, sizeof(struct rudp_packet_header) + size);

    if ( pc == NULL )
        return ENOMEM;

    memcpy(&pc->packet->data.data[0], data, size);

    pc->packet->header.command = RUDP_CMD_APP + command;

    if ( reliable )
        return rudp_peer_send_reliable(peer, pc);
    else
        return rudp_peer_send_unreliable(peer, pc);
}

rudp_error_t rudp_server_send_all(
    struct rudp_server *server,
    int reliable, int command,
    const void *data,
    const size_t size)
{
    if ( (command + RUDP_CMD_APP) > 255 )
        return EINVAL;

    struct server_peer *peer, *tmp;
    rudp_list_for_each_safe(peer, tmp, &server->peer_list, server_item)
    {
        rudp_server_send(server, &peer->base, reliable, command, data, size);
    }
    return 0;
}


/*
  For the two following functions, server context pointer is actually
  useless.

  It is only here for safety purposes: Users eager to store private
  data for a peer may be tempted to use a function like
  rudp_server_peer_data_set(peer, data) even if peer were not from a
  server.

  Asking users to give reference to the server make them think
  something is wrong if they call rudp_server_peer_data_set(NULL,
  peer, data)
 */

void *rudp_server_peer_data_get(
    struct rudp_server *server,
    struct rudp_peer *_peer)
{
    (void)server;
    struct server_peer *peer = (struct server_peer *)_peer;
    return peer->user_data;
}

void rudp_server_peer_data_set(
    struct rudp_server *server,
    struct rudp_peer *_peer,
    void *data)
{
    (void)server;
    struct server_peer *peer = (struct server_peer *)_peer;
    peer->user_data = data;
}

rudp_error_t rudp_server_set_hostname(
    struct rudp_server *server,
    const char *hostname,
    const uint16_t port,
    uint32_t ip_flags)
{
    return rudp_endpoint_set_hostname(&server->endpoint,
                                      hostname, port, ip_flags);
}

void rudp_server_set_ipv4(
    struct rudp_server *server,
    const struct in_addr *address,
    const uint16_t port)
{
    return rudp_endpoint_set_ipv4(&server->endpoint, address, port);
}

void rudp_server_set_ipv6(
    struct rudp_server *server,
    const struct in6_addr *address,
    const uint16_t port)
{
    struct sockaddr_in6 addr6;

    memset(&addr6, 0, sizeof (addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_addr = *address;
    addr6.sin6_port = htons(port);

    rudp_endpoint_set_addr(&server->endpoint, (struct sockaddr *)&addr6,
                           sizeof (addr6));
}

rudp_error_t rudp_server_set_addr(
    struct rudp_server *server,
    const struct sockaddr *addr,
    socklen_t addrlen)
{
    return rudp_endpoint_set_addr(&server->endpoint, addr, addrlen);
}
