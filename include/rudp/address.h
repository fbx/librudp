/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_ADDRESS_H_
/** @hidden */
#define RUDP_ADDRESS_H_

/**
   @file
   @module {Address}
   @short Internet address (IPv4/IPv6) abstraction, with resolving

   Addresses are abstracted in the library.  Addresses are
   represented through the @ref rudp_address.

   Addresses are objects with the same @xref {olc} {life cycle} as
   most other complex objects of the library.  Before use, address
   structures must be initialized with @ref rudp_address_init, and
   after use, they must be cleaned with @ref rudp_address_deinit.

   Addresses may be set 3 ways:
   @list
     @item with a constant IPv4 address (@ref rudp_address_set_ipv4)
     @item with a constant IPv6 address (@ref rudp_address_set_ipv6)
     @item with an hostname to lookup (@ref rudp_address_set_hostname)
   @end list

   After the address is correctly setup, user code may use @ref
   rudp_address_get to retrieve a @tt {struct sockaddr} containing
   the address.

   In order to walk through the resolved addresses, user may call
   @ref rudp_address_next. This loops through the available addresses
   corresponding to an hostname.
 */

#include <rudp/error.h>
#include <rudp/compiler.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <inttypes.h>

#define RUDP_NO_IPV4   2
#define RUDP_NO_IPV6   1
#define RUDP_IPV4_ONLY RUDP_NO_IPV6
#define RUDP_IPV6_ONLY RUDP_NO_IPV4
#define RUDP_IP_ANY    0

/**
   @this is an abstract representation of an address.

   Addresses are objects with the same @xref {olc} {life cycle} as
   most other complex objects of the library.  Before use, address
   structures must be initialized with @ref rudp_address_init, and
   after use, they must be cleaned with @ref rudp_address_deinit.

   @hidecontent
 */
struct rudp_address
{
    /** Rudp context */
    struct rudp *rudp;
    /** Low-level address structure */
    struct sockaddr_storage *addr;
    /** High_level address structure */
    struct addrinfo *addrinfo;
    /** Walk pointer */
    struct addrinfo *cur;
    /** Hostname (if symbolic resolving) */
    char *hostname;
    /** Flags about IP versions allowed in lookup */
    uint8_t allowed_ip_version;
    /** Internal resolver state */
    uint8_t resolver_state;
    /** Port associated with the address */
    uint16_t port;
    char text[INET6_ADDRSTRLEN+6];
};

/**
   @this specifies a hostname to be resolved with system resolver, and
   uses the results in turn.  This can implement a round-roubin service
   usage.

   @param addr The address structure
   @param hostname Hostname to resolve
   @param port Numeric target port (machine order)
   @param ip_flags RUDP_IPV4_ONLY, RUDP_IPV6_ONLY or RUDP_IP_ANY
   @returns 0 on success, RUDP_EBACKEND if resolution failed,
   RUDP_ENOMEM if allocation failed
 */
RUDP_EXPORT
rudp_error_t rudp_address_set_hostname(
    struct rudp_address *addr,
    const char *hostname,
    const uint16_t port,
    uint32_t ip_flags);

/**
   @this gets a pointer to a textual representation of the address.

   As this function only takes a reference, this function has a cost
   only on the first call, and then has a o(1) complexity and can be
   called as often as needed.

   @param addr Address structure
   @returns a valid constant string
 */
RUDP_EXPORT
const char *rudp_address_text(struct rudp_address *addr);

/**
   @this takes the next usable address in the resolver state. This
   call only makes sense for addresses using @ref
   rudp_address_set_hostname.

   @param addr Address to walk in
   @returns 0 on success, RUDP_ENOADDR if resolution failed,
   RUDP_EBACKEND on unhandled errors
 */
RUDP_EXPORT
rudp_error_t rudp_address_next(
    struct rudp_address *addr);

/**
   @this gets a pointer to the socket() api-compatible
   structure. Pointer is owner by the address structure and is valid
   until next call to @tt rudp_address_set_* or @ref rudp_address_next.

   As this function only takes a reference, this function has a o(1)
   complexity and can be called as often as needed.

   @param addr Address to get the sockaddr structure from
   @param address (out) System-compatible address structure
   @param size (out) Size of the address structure
   @returns 0 on success, RUDP_ENOADDR if resolution failed,
   RUDP_EBACKEND on unhandled errors
 */
RUDP_EXPORT
rudp_error_t rudp_address_get(
    const struct rudp_address *addr,
    const struct sockaddr_storage **address,
    socklen_t *size);

/**
   @this specifies an IPv4 address and port

   @param addr The address structure
   @param address IPv4 to use (usual @tt {struct in_addr} order)
   @param port Numeric target port (machine order)
 */
RUDP_EXPORT
void rudp_address_set_ipv4(
    struct rudp_address *addr,
    const struct in_addr *address,
    const uint16_t port);

/**
   @this specifies an IPv6 address and port

   @param addr The address structure
   @param address IPv6 to use (usual @tt {struct in6_addr} order)
   @param port Numeric target port (machine order)
 */
RUDP_EXPORT
void rudp_address_set_ipv6(
    struct rudp_address *addr,
    const struct in6_addr *address,
    const uint16_t port);

/**
   @this specifies an address (AF_INET or AF_INET6)

   @param addr The address structure
   @param address Generic address to use
   @param size Address structure size
 */
RUDP_EXPORT
rudp_error_t rudp_address_set(
    struct rudp_address *rua,
    const struct sockaddr *sockaddr,
    socklen_t size);

/**
   @this initializes an address structure for future usage.  This
   function must be called before any other address-related function
   on the structure.

   @param addr The address structure to initialize
   @param rudp A valid rudp context
 */
RUDP_EXPORT
void rudp_address_init(struct rudp_address *addr, struct rudp *rudp);

/**
   @this releases all data internally referenced by an address
   structure.

   @param addr The address structure to deinit
 */
RUDP_EXPORT
void rudp_address_deinit(struct rudp_address *addr);

/**
   @this compares the address against another address

   @param address Address structure to compare
   @param addr System address to compare to
   @returns 0 if they match, another value otherwise
 */
RUDP_EXPORT
int rudp_address_compare(const struct rudp_address *address,
                         const struct sockaddr_storage *addr);

#endif
