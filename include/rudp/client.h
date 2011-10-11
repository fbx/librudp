/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_CLIENT_H_
/** @hidden */
#define RUDP_CLIENT_H_

/**
   @file
   @module {Client}
   @short Client implementation

   Client connects to a server.  As long as the connection is
   established, data can be exchanged both ways, either reliably or
   not.

   For connecting, It either uses an explicit constant IPv4 or IPv6
   address, or an hostname to resolve.

   Client contexts use the same @xref {olc} {life cycle} as other
   complex objects of the library.  Before use, client contexts must
   be initialized with @ref rudp_client_init, and after use, they must
   be cleaned with @ref rudp_client_deinit.

   An initialized but not yet connected Client context must have its
   server address setup with any of the following functions: @ref
   rudp_client_set_hostname, @ref rudp_client_set_ipv4 or @ref
   rudp_client_set_ipv6.

   Once the server address is setup properly, Client context can be
   connected with @ref rudp_client_connect.  After successful
   handshake with the server, the @ref rudp_client_handler::connected
   handler is called back.

   Either because of a connection timeout or because of server
   disconnection the @ref rudp_client_handler::server_lost handler
   is called back.

   Anytime afer call to @ref rudp_client_connect and until the @ref
   rudp_client_handler::server_lost handler is called, user may ask
   for disconnection with @ref rudp_client_close.

   Sample usage:
   @code
    // assuming you have a valid rudp context here:
    struct rudp *rudp;

    struct rudp_client client;

    rudp_client_init(&client, rudp, &my_client_handlers);

    // set the server address and connect
    rudp_client_set_ipv4(&client, ...);
    rudp_client_connect(&client);

    // run your event loop

    rudp_client_close(&client);
    rudp_client_deinit(&client);
   @end code
 */

#include <rudp/list.h>
#include <rudp/compiler.h>
#include <rudp/endpoint.h>
#include <rudp/address.h>
#include <rudp/packet.h>
#include <rudp/peer.h>
#include <ela/ela.h>

struct rudp_client;
struct rudp_link_info;
struct rudp_peer;

/**
   Client handler code callbacks
 */
struct rudp_client_handler
{
    /**
       @this is called on packet reception. Packet is already decoded
       and is ensured to be an application packet (@ref RUDP_CMD_APP
       command or equivalent).

       Packet chain ownership is not given to handler, handler must
       copy data and forget the chain afterwards.

       @param client Client context
       @param command User command used
       @param data Data buffer
       @param len Useful data length
     */
    void (*handle_packet)(
        struct rudp_client *client,
        int command, const void *data, size_t len);

    /**
       @this is called anytime when link statistics are updated.

       @param client Client context
       @param info Link quality updated information
     */
    void (*link_info)(struct rudp_client *client, struct rudp_link_info *info);
    /**
       @this is called when client gets connected.  When this handler
       is called, client is guaranteed to have established a valid
       connection to the server.

       Client will stay valid until @ref
       rudp_client_handler::server_lost is called.

       @param client Client context
     */
    void (*connected)(struct rudp_client *client);

    /**
       @this is called when connection to the server drops, either
       explicitely or because of a timeout.

       After this handler is called, client is left in the initialized
       state, like after @ref rudp_client_close is called.  Handler
       code can either use @ref rudp_client_connect or @ref
       rudp_client_deinit afterwards.

       @param client Client context
     */
    void (*server_lost)(struct rudp_client *client);
};

/**
   @this is a client context structure.  User should not use its
   fields directly.

   Clients are objects with the same @xref {olc} {life cycle} as
   most other complex objects of the library.  Before use, client
   contexts must be initialized with @ref rudp_client_init, and after
   use, they must be cleaned with @ref rudp_client_deinit.

   An initialized client context can try connection to a server with
   @ref rudp_client_connect, and a connected client context can be
   closed with @ref rudp_client_close.  A closed client context stays
   initialized.

   @hidecontent
 */
struct rudp_client
{
    const struct rudp_client_handler *handler;
    struct rudp_peer peer;
    struct rudp_endpoint endpoint;
    struct rudp_address address;
    struct rudp *rudp;
    char connected;
};

struct rudp_peer;

/**
   @this initializes the client context.

   @param client Client unitialized context structure
   @param rudp A valid rudp context
   @param handler A client handler descriptor

   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_client_init(
    struct rudp_client *client,
    struct rudp *rudp,
    const struct rudp_client_handler *handler);

/**
   @this tries to establish a connection with the server.  Server
   address can be set with any of the @ref rudp_client_set_hostname,
   @ref rudp_client_set_ipv4 or @ref rudp_client_set_ipv6.

   @param client An initialized client context structure

   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_client_connect(struct rudp_client *client);

/**
   @this cleanly drops connection to the server and closes all
   associated sockets.

   @param client A connected (or currently trying to connect) client
   context structure

   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_client_close(struct rudp_client *client);

/**
   @this frees all internally allocated client data.

   @param client An initialized and unconnected client context

   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_client_deinit(struct rudp_client *client);

/**
   @this specifies a hostname to connect to.  Actual underlying address
   will be resolved.  @see rudp_address_set_hostname for details.

   @param client An initialized client context structure
   @param hostname Hostname to resolve
   @param port Numeric target port (machine order)
   @param ip_flags RUDP_IPV4_ONLY, RUDP_IPV6_ONLY or RUDP_IP_ANY
   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_client_set_hostname(
    struct rudp_client *client,
    const char *hostname,
    const uint16_t port,
    uint32_t ip_flags);

/**
   @this specifies an IPv4 address to connect to.  @see
   rudp_address_set_ipv4 for details.

   @param client An initialized client context structure
   @param address IPv4 to use (usual @tt {struct in_addr} order)
   @param port Numeric target port (machine order)
   @returns a possible error
 */
RUDP_EXPORT
void rudp_client_set_ipv4(
    struct rudp_client *client,
    const struct in_addr *address,
    const uint16_t port);

/**
   @this specifies an IPv6 address to connect to.  @see
   rudp_address_set_ipv6 for details.

   @param client An initialized client context structure
   @param address IPv6 to use (usual @tt {struct in6_addr} order)
   @param port Numeric target port (machine order)
   @returns a possible error
 */
RUDP_EXPORT
void rudp_client_set_ipv6(
    struct rudp_client *client,
    const struct in6_addr *address,
    const uint16_t port);

/**
   @this sends data to remote server

   @param client Source client
   @param reliable Whether to send the payload reliably
   @param command User command code. It may be between 0 and RUDP_CMD_APP_MAX.
   @param data Payload
   @param size Total payload size

   @returns An error level
 */
RUDP_EXPORT
rudp_error_t rudp_client_send(struct rudp_client *client,
                              int reliable, int command,
                              const void *data, const size_t size);

#endif
