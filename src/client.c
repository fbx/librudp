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
#include <rudp/client.h>
#include <rudp/peer.h>
#include "rudp_list.h"
#include "rudp_packet.h"

static const struct rudp_endpoint_handler client_endpoint_handler;
static const struct rudp_peer_handler client_peer_handler;

rudp_error_t rudp_client_init(
    struct rudp_client *client,
    struct rudp *rudp,
    const struct rudp_client_handler *handler)
{
    rudp_endpoint_init(&client->endpoint, rudp, &client_endpoint_handler);
    rudp_address_init(&client->address, rudp);
    client->rudp = rudp;
    client->handler = handler;
    client->connected = 0;
    return 0;
}

rudp_error_t rudp_client_connect(struct rudp_client *client)
{
    const struct sockaddr_storage *addr;
    struct sockaddr bind_addr;
    socklen_t size;

    rudp_error_t err = rudp_address_get(&client->address, &addr, &size);
    if ( err )
        return err;

    rudp_peer_from_sockaddr(&client->peer, client->rudp,
                            addr,
                            &client_peer_handler, &client->endpoint);

    rudp_peer_send_connect(&client->peer);

    memset(&bind_addr, 0, sizeof (bind_addr));
    bind_addr.sa_family = addr->ss_family;
    rudp_endpoint_set_addr(&client->endpoint, &bind_addr, sizeof (bind_addr));

    return rudp_endpoint_bind(&client->endpoint);
}

rudp_error_t rudp_client_close(struct rudp_client *client)
{
    rudp_peer_send_close_noqueue(&client->peer);

    rudp_peer_deinit(&client->peer);

    rudp_endpoint_close(&client->endpoint);
    return 0;
}

rudp_error_t rudp_client_deinit(struct rudp_client *client)
{
    rudp_address_deinit(&client->address);
    rudp_endpoint_deinit(&client->endpoint);
    return 0;
}

static
void client_handle_data_packet(
    struct rudp_peer *peer,
    struct rudp_packet_chain *pc)
{
    struct rudp_client *client = __container_of(peer, client, peer);
    struct rudp_packet_data *header = &pc->packet->data;

    client->handler->handle_packet(
        client, header->header.command - RUDP_CMD_APP,
        header->data, pc->len - sizeof(header));
}

static
void client_link_info(struct rudp_peer *peer, struct rudp_link_info *info)
{
    struct rudp_client *client = __container_of(peer, client, peer);

    client->handler->link_info(client, info);
}

static
void client_peer_dropped(struct rudp_peer *peer)
{
    struct rudp_client *client = __container_of(peer, client, peer);

    client->connected = 0;

    rudp_peer_deinit(&client->peer);

    rudp_endpoint_close(&client->endpoint);

    client->handler->server_lost(client);
}

static const struct rudp_peer_handler client_peer_handler = {
    .handle_packet = client_handle_data_packet,
    .link_info = client_link_info,
    .dropped = client_peer_dropped,
};

/*
  - socket watcher
     - endpoint packet reader
        - client packet handler <===
           - peer packet handler
 */
static
void client_handle_endpoint_packet(struct rudp_endpoint *endpoint,
                                   const struct sockaddr_storage *addr,
                                   struct rudp_packet_chain *pc)
{
    struct rudp_client *client = __container_of(endpoint, client, endpoint);

    rudp_log_printf(client->rudp, RUDP_LOG_INFO,
                    "Endpoint handling packet\n");

    rudp_error_t err = rudp_peer_incoming_packet(&client->peer, pc);
    if ( err == 0 && client->connected == 0 )
    {
        client->connected = 1;
        client->handler->connected(client);
    }
}

static const struct rudp_endpoint_handler client_endpoint_handler = {
    .handle_packet = client_handle_endpoint_packet,
};

rudp_error_t rudp_client_send(
    struct rudp_client *client,
    int reliable, int command,
    const void *data,
    const size_t size)
{
    if ( (command + RUDP_CMD_APP) > 255 )
        return EINVAL;

    if ( !client->connected )
        return EINVAL;

    struct rudp_packet_chain *pc = rudp_packet_chain_alloc(
        client->rudp, sizeof(struct rudp_packet_header) + size);
    if ( pc == NULL )
        return ENOMEM;

    memcpy(&pc->packet->data.data[0], data, size);

    pc->packet->header.command = RUDP_CMD_APP + command;

    if ( reliable )
        return rudp_peer_send_reliable(&client->peer, pc);
    else
        return rudp_peer_send_unreliable(&client->peer, pc);
}

rudp_error_t rudp_client_set_hostname(
    struct rudp_client *client,
    const char *hostname,
    const uint16_t port,
    uint32_t ip_flags)
{
    return rudp_address_set_hostname(&client->address,
                                      hostname, port, ip_flags);
}

void rudp_client_set_ipv4(
    struct rudp_client *client,
    const struct in_addr *address,
    const uint16_t port)
{
    return rudp_address_set_ipv4(&client->address, address, port);
}

void rudp_client_set_ipv6(
    struct rudp_client *client,
    const struct in6_addr *address,
    const uint16_t port)
{
    struct sockaddr_in6 addr6;

    memset(&addr6, 0, sizeof (addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_addr = *address;
    addr6.sin6_port = htons(port);

    rudp_address_set(&client->address, (struct sockaddr *)&addr6,
                     sizeof (addr6));
}

rudp_error_t rudp_client_set_addr(
    struct rudp_client *client,
    const struct sockaddr *addr,
    socklen_t addrlen)
{
    return rudp_address_set(&client->address, addr, addrlen);
}
