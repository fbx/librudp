/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_SERVER_H_
/** @hidden */
#define RUDP_SERVER_H_

/**
   @file
   @module {Server}
   @short Server implementation

   Server binds to a given socket address and waits for clients to
   connect.  For binding, It either uses an explicit constant IPv4 or
   IPv6 address, or an hostname to resolve.

   When a client connects, as long as the connection is established,
   data can be exchanged both ways, either reliably or not.  User code
   gets notified of clients (un)connecting.

   Server contexts use the same @xref {olc} {life cycle} as other
   complex objects of the library.  Before use, server contexts must
   be initialized with @ref rudp_server_init, and after use, they must
   be cleaned with @ref rudp_server_deinit.

   An initialized but not yet bound Server context must have its
   server address setup with any of the following functions: @ref
   rudp_server_set_hostname, @ref rudp_server_set_ipv4 or @ref
   rudp_server_set_addr.

   Once the server address is setup properly, Server context can be
   bound with @ref rudp_server_bind.  After successful binding, the
   server waits for clients and data packets.

   @label {peer_user_data}

   User code may store an user-handled user data pointer inside the
   peer structures associated to a server context. This can be done
   through the @ref rudp_server_peer_data_set and @ref
   rudp_server_peer_data_get functions.

   Sample usage:
   @code
    // assuming you have a valid rudp context here:
    struct rudp *rudp;

    struct rudp_server server;

    rudp_server_init(&server, rudp, &my_server_handlers);

    // set the address and bind
    rudp_server_set_ipv4(&server, ...);
    rudp_server_bind(&server);

    // run your event loop

    rudp_server_close(&server);
    rudp_server_deinit(&server);
   @end code
*/

#include <rudp/list.h>
#include <rudp/endpoint.h>
#include <rudp/packet.h>
#include <rudp/compiler.h>
#include <ela/ela.h>

struct rudp_server;
struct rudp_link_info;
struct rudp_peer;

/**
   Server handler code callbacks
 */
struct rudp_server_handler
{
    /**
       @this is called on packet reception. Packet is already decoded
       and is ensured to be an application packet (@ref RUDP_CMD_APP
       command or equivalent).

       Packet chain ownership is not given to handler, handler must
       copy data and forget the chain afterwards.

       @param server Server context
       @param peer Relevant peer
       @param command User command used
       @param data Data buffer
       @param len Useful data length
     */
    void (*handle_packet)(struct rudp_server *server, struct rudp_peer *peer,
                          int command, const void *data, size_t len);

    /**
       @this is called anytime when link statistics are updated.

       @param server Server context
       @param peer Relevant peer
       @param info Link quality updated information
     */
    void (*link_info)(
        struct rudp_server *server,
        struct rudp_peer *peer,
        struct rudp_link_info *info);

    /**
       @this is called when a peer drops, either explicitely or
       because of a timeout.

       After this handler is called, handler code must forget its
       references to the peer. The peer structure is not valid
       afterwards.

       @param server Server context
       @param peer Relevant peer
     */
    void (*peer_dropped)(struct rudp_server *server, struct rudp_peer *peer);

    /**
       @this is called when a new peer gets connected.  When this
       handler is called, peer is guaranteed to have established a
       valid connection.

       Handler code may retain pointer references to @tt peer, and may
       even store @xref {peer_user_data} {user data} pointer inside
       it.

       Peer will stay valid until @ref
       rudp_server_handler::peer_dropped is called.

       @param server Server context
       @param peer New peer
     */
    void (*peer_new)(struct rudp_server *server, struct rudp_peer *peer);
};

/**
   @this is a server context structure.  User should not use its
   fields directly.

   Servers are objects with the same @xref {olc} {life cycle} as
   most other complex objects of the library.  Before use, server
   contexts must be initialized with @ref rudp_server_init, and after
   use, they must be cleaned with @ref rudp_server_deinit.

   An initialized server context can be bound with @ref
   rudp_server_bind to a port, and a bound server context can be
   closed with @ref rudp_server_close.  A closed server context stays
   initialized.

   @hidecontent
 */
struct rudp_server
{
    const struct rudp_server_handler *handler;
    struct rudp_list peer_list;
    struct rudp_endpoint endpoint;
    struct rudp *rudp;
};

struct rudp_peer;

/**
   @this initializes the server context.

   @param server Server unitialized context structure
   @param rudp A valid rudp context
   @param handler A server handler descriptor

   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_server_init(
    struct rudp_server *server,
    struct rudp *rudp,
    const struct rudp_server_handler *handler);

/**
   @this binds the server context to an address.  This both creates a
   socket and binds it to the relevant address/port set with any of
   the @ref rudp_server_set_hostname, @ref rudp_server_set_ipv4 or
   @ref rudp_server_set_addr.

   @param server An initialized server context structure

   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_server_bind(struct rudp_server *server);

/**
   @this unbinds the server context from an address, closes all
   associated sockets and drops all peers.  A @ref
   rudp_server_handler::peer_dropped handler will be called for each
   present peer.

   @param server A bound server context structure

   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_server_close(struct rudp_server *server);

/**
   @this frees all internally allocated server data.

   @param server An initialized and unbound server context

   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_server_deinit(struct rudp_server *server);

/**
   @this specifies a hostname to bind to.  Actual underlying address
   will be resolved.  @see rudp_address_set_hostname for details.

   @param server An initialized server context structure
   @param hostname Hostname to resolve
   @param port Numeric target port (machine order)
   @param ip_flags RUDP_IPV4_ONLY, RUDP_IPV6_ONLY or RUDP_IP_ANY
   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_server_set_hostname(
    struct rudp_server *server,
    const char *hostname,
    const uint16_t port,
    uint32_t ip_flags);

/**
   @this specifies an address to bind to. Supported address families
   are AF_INET and AF_INET6.

   @param server An initialized server context structure
   @param addr IPv4 or IPv6 address to use
   @param addrlen Size of the address structure
   @returns 0 on success, EAFNOSUPPORT if address family is not supported
 */
RUDP_EXPORT
rudp_error_t rudp_server_set_addr(
    struct rudp_server *server,
    const struct sockaddr *addr,
    socklen_t addrlen);

/**
   @this specifies an IPv4 address to bind to.  @see
   rudp_address_set_ipv4 for details.

   @param server An initialized server context structure
   @param address IPv4 to use (usual @tt {struct in_addr} order)
   @param port Numeric target port (machine order)
 */
RUDP_EXPORT
void rudp_server_set_ipv4(
    struct rudp_server *server,
    const struct in_addr *address,
    const uint16_t port);

/**
   @this specifies an IPv6 address to bind to.  @see
   rudp_address_set_ipv6 for details.

   @deprecated
   This function should not be used anymore, since it does not allow
   to set the IPv6 scope. Use @ref rudp_server_set_addr instead.

   @param server An initialized server context structure
   @param address IPv6 to use (usual @tt {struct in6_addr} order)
   @param port Numeric target port (machine order)
 */
RUDP_EXPORT
void rudp_server_set_ipv6(
    struct rudp_server *server,
    const struct in6_addr *address,
    const uint16_t port) RUDP_DEPRECATED;

/**
   @this sends data from this server to a peer.

   @param server Source server
   @param peer Destination peer
   @param reliable Whether to send the payload reliably
   @param command User command code. It may be between 0 and RUDP_CMD_APP_MAX.
   @param data Payload
   @param size Total packet size

   @returns An error level
 */
RUDP_EXPORT
rudp_error_t rudp_server_send(
    struct rudp_server *server,
    struct rudp_peer *peer,
    int reliable, int command,
    const void *data, const size_t size);

/**
   @this sends data from this server to all peers.

   @param server Source server
   @param reliable Whether to send the payload reliably
   @param command User command code. It may be between 0 and RUDP_CMD_APP_MAX.
   @param data Payload
   @param size Total packet size

   @returns An error level
 */
RUDP_EXPORT
rudp_error_t rudp_server_send_all(
    struct rudp_server *server,
    int reliable, int command,
    const void *data, const size_t size);

/**
   @this retrieves user code private data pointer associated to a
   peer.

   @param server Server context this peer belongs to
   @param peer Peer context pointer is associated to
   @returns the latest user pointer set for this peer (NULL if never
   set before)
 */
RUDP_EXPORT
void *rudp_server_peer_data_get(
    struct rudp_server *server,
    struct rudp_peer *peer);

/**
   @this sets user code private data pointer associated to a peer.

   @param server Server context this peer belongs to
   @param peer Peer context to associate the pointer to
   @param data User pointer
 */
RUDP_EXPORT
void rudp_server_peer_data_set(
    struct rudp_server *server,
    struct rudp_peer *peer,
    void *data);

/**
   @this cleanly drops connection to one client.

   @param server Source server
   @param peer Destination peer corresponding to client

 */
RUDP_EXPORT
void rudp_server_client_close(
    struct rudp_server *server,
    struct rudp_peer *peer);

#endif
