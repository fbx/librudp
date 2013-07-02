/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_ENDPOINT_H_
/** @hidden */
#define RUDP_ENDPOINT_H_

/**
   @file
   @module {Endpoint}
   @short Network endpoint (socket handling)

   Endpoint is an abstration over a system socket. It handles sending
   and receiving of packets.

   Endpoint contexts use the same @xref {olc} {life cycle} as other
   complex objects of the library.  Before use, endpoint contexts must
   be initialized with @ref rudp_endpoint_init, and after use, they must
   be cleaned with @ref rudp_endpoint_deinit.
*/

#include <rudp/address.h>
#include <ela/ela.h>
#include <rudp/compiler.h>

struct rudp_peer;
struct rudp_endpoint;
struct rudp_packet_chain;

/**
   Endpoint handler code callbacks
 */
struct rudp_endpoint_handler
{
    /**
       @this is called on packet reception. Packet contains raw data.

       Packet chain ownership is not given to handler, handler must
       copy data and forget the chain afterwards.

       @param endpoint Endpoint context
       @param addr Remote address the packet was received from
       @param packet Packet descriptor structure
     */
    void (*handle_packet)(
        struct rudp_endpoint *endpoint,
        const struct sockaddr_storage *addr,
        struct rudp_packet_chain *packet);
};

/**
   @this is a endpoint context structure.  User must not use its
   fields directly.

   Endpoints are objects with the same @xref {olc} {life cycle} as
   most other complex objects of the library.  Before use, endpoint
   contexts must be initialized with @ref rudp_endpoint_init, and
   after use, they must be cleaned with @ref rudp_endpoint_deinit.

   An initialized endpoint context can be bound with @ref
   rudp_endpoint_bind to its address, and a bound endpoint context can
   be closed with @ref rudp_endpoint_close.  A close endpoint context
   stays initialized.

   @hidecontent
 */
struct rudp_endpoint
{
    const struct rudp_endpoint_handler *handler;
    struct rudp_address addr;
    struct rudp *rudp;
    struct ela_event_source *ela_source;
    int socket_fd;
};

/**
   @this initializes a endpoint structure.

   @param endpoint Endpoint structure to initialize
   @param rudp A valid rudp context
   @param handler A endpoint handler descriptor
 */
RUDP_EXPORT
void rudp_endpoint_init(
    struct rudp_endpoint *endpoint,
    struct rudp *rudp,
    const struct rudp_endpoint_handler *handler);

/**
   @this frees all data associated to a endpoint structure

   The endpoint should have no references left.

   @param endpoint Endpoint structure to deinit
 */
RUDP_EXPORT
void rudp_endpoint_deinit(struct rudp_endpoint *endpoint);

/**
   @this specifies a hostname to bind to.  Actual underlying address
   will be resolved.  @see rudp_address_set_hostname for details.

   @param endpoint An initialized endpoint context structure
   @param hostname Hostname to resolve
   @param port Numeric target port (machine order)
   @param ip_flags RUDP_IPV4_ONLY, RUDP_IPV6_ONLY or RUDP_IP_ANY
   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_endpoint_set_hostname(
    struct rudp_endpoint *endpoint,
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
rudp_error_t rudp_endpoint_set_addr(
    struct rudp_endpoint *endpoint,
    const struct sockaddr *addr,
    socklen_t addrlen);

/**
   @this specifies an IPv4 address to bind to.  @see
   rudp_address_set_ipv4 for details.

   @param endpoint An initialized endpoint context structure
   @param address IPv4 to use (usual @tt {struct in_addr} order)
   @param port Numeric target port (machine order)
 */
RUDP_EXPORT
void rudp_endpoint_set_ipv4(
    struct rudp_endpoint *endpoint,
    const struct in_addr *address,
    const uint16_t port);

/**
   @this specifies an IPv6 address to bind to.  @see
   rudp_address_set_ipv6 for details.

   @deprecated
   This function should not be used anymore, since it does not allow
   to set the IPv6 scope. Use @ref rudp_endpoint_set_addr instead.

   @param endpoint An initialized endpoint context structure
   @param address IPv6 to use (usual @tt {struct in6_addr} order)
   @param port Numeric target port (machine order)
 */
RUDP_EXPORT
void rudp_endpoint_set_ipv6(
    struct rudp_endpoint *endpoint,
    const struct in6_addr *address,
    const uint16_t port) RUDP_DEPRECATED;

/**
   @this open and binds an endpoint to its address

   @param endpoint Endpoint to bind
 */
RUDP_EXPORT
rudp_error_t rudp_endpoint_bind(struct rudp_endpoint *endpoint);

/**
   @this closes the socket associated to this endpoint

   @param endpoint Endpoint to close
 */
RUDP_EXPORT
void rudp_endpoint_close(struct rudp_endpoint *endpoint);

/**
   @this sends data from the endpoint to the designated remote address.

   @param endpoint Enpoint to use as source
   @param addr Destination address
   @param data Data pointer
   @param len Length of data
   @returns a possible error
 */
RUDP_EXPORT
rudp_error_t rudp_endpoint_send(struct rudp_endpoint *endpoint,
                                const struct rudp_address *addr,
                                const void *data, size_t len);

/**
   @this receives data from the associated socket.

   @param endpoint Endpoint
   @param data Data buffer
   @param len (inout) On call: size avaiable in @tt data. On return:
          size actually read
   @param addr (out) Peer address
   @returns a possible error, 0 if received
 */
RUDP_EXPORT
rudp_error_t rudp_endpoint_recv(struct rudp_endpoint *endpoint,
                                void *data, size_t *len,
                                struct sockaddr_storage *addr);

/**
   @this compares the endpoint address with another address

   @param endpoint Endpoint to compare address from
   @param addr Address to compare to
   @returns 0 if they match, another value otherwise
 */
RUDP_EXPORT
int rudp_endpoint_address_compare(const struct rudp_endpoint *endpoint,
                                  const struct sockaddr_storage *addr);

#endif
