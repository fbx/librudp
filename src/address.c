/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */


#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <rudp/error.h>
#include <rudp/address.h>
#include "rudp_rudp.h"

enum resolver_state {
    RUDP_RESOLV_NONE,
    RUDP_RESOLV_ADDR,
    RUDP_RESOLV_ERROR,
    RUDP_RESOLV_DONE,
    RUDP_RESOLV_FAILED,
};

rudp_error_t rudp_address_set_hostname(
    struct rudp_address *rua,
    const char *hostname,
    const uint16_t port,
    uint32_t ip_flags)
{
    rua->text[0] = 0;

    if ( hostname == NULL )
        return EINVAL;

    if ( rua->hostname )
        free(rua->hostname);

    rua->port = port;

    rua->hostname = strdup(hostname);
    if ( rua->hostname == NULL )
        return ENOMEM;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    if ( ip_flags & RUDP_NO_IPV4 )
        hints.ai_family = AF_INET6;
    else if ( ip_flags & RUDP_NO_IPV6 )
        hints.ai_family = AF_INET;
    else
        hints.ai_family = AF_UNSPEC;

    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    rua->cur = NULL;

    int s = getaddrinfo(hostname, NULL, &hints, &rua->addrinfo);
    if (s != 0) {
        rua->resolver_state = RUDP_RESOLV_ERROR;
        return errno;
    }

    rua->resolver_state = RUDP_RESOLV_DONE;

    rudp_address_next(rua);

    return 0;
}

void rudp_address_deinit(struct rudp_address *rua)
{
    switch ( (enum resolver_state)rua->resolver_state )
    {
    case RUDP_RESOLV_NONE:
    case RUDP_RESOLV_ADDR:
    case RUDP_RESOLV_FAILED:
    case RUDP_RESOLV_ERROR:
        break;
    case RUDP_RESOLV_DONE:
        freeaddrinfo(rua->addrinfo);
    }

    if ( rua->hostname )
        free(rua->hostname);
    if ( rua->addr )
        rudp_free(rua->rudp, rua->addr);
    rua->hostname = NULL;
    rua->resolver_state = RUDP_RESOLV_NONE;
    rua->text[0] = 0;
}

void rudp_address_init(struct rudp_address *rua, struct rudp *rudp)
{
    memset(rua, 0, sizeof(*rua));
    rua->text[0] = 0;

    rua->rudp = rudp;
    rua->resolver_state = RUDP_RESOLV_NONE;
    rua->addr = rudp_alloc(rudp, sizeof(struct sockaddr_storage));
    memset(rua->addr, 0, sizeof(struct sockaddr_storage));
}

void rudp_address_set_ipv4(
    struct rudp_address *rua,
    const struct in_addr *in_addr,
    const uint16_t port)
{
    struct sockaddr_in *addr = (struct sockaddr_in *)rua->addr;

    rua->text[0] = 0;

    rua->resolver_state = RUDP_RESOLV_ADDR;

    rua->port = port;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    memcpy(&addr->sin_addr, in_addr, sizeof(struct in_addr));
}


void rudp_address_set_ipv6(
    struct rudp_address *rua,
    const struct in6_addr *in6_addr,
    const uint16_t port)
{
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *)rua->addr;

    rua->text[0] = 0;

    memset(addr, 0, sizeof(*addr));

    rua->resolver_state = RUDP_RESOLV_ADDR;

    rua->port = port;
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    memcpy(&addr->sin6_addr, in6_addr, sizeof(struct in6_addr));
}

rudp_error_t rudp_address_set(
    struct rudp_address *rua,
    const struct sockaddr *sockaddr,
    socklen_t size)
{
    if (size < sizeof (*sockaddr) || size > sizeof (*rua->addr))
        return EINVAL;

    switch (sockaddr->sa_family)
    {
    case AF_INET:
        rua->port = ntohs(((struct sockaddr_in *)sockaddr)->sin_port);
        break;
    case AF_INET6:
        rua->port = ntohs(((struct sockaddr_in6 *)sockaddr)->sin6_port);
        break;
    default:
        return EAFNOSUPPORT;
    }

    rua->text[0] = 0;
    rua->resolver_state = RUDP_RESOLV_ADDR;
    memcpy(rua->addr, sockaddr, size);

    return 0;
}

rudp_error_t rudp_address_next(
    struct rudp_address *rua)
{
    rua->text[0] = 0;

    switch ( (enum resolver_state)rua->resolver_state )
    {
    case RUDP_RESOLV_ADDR:
        return 0;

    case RUDP_RESOLV_DONE:
        if ( rua->cur == NULL || rua->cur->ai_next == NULL )
            rua->cur = rua->addrinfo;
        else
            rua->cur = rua->cur->ai_next;

        if ( rua->cur->ai_family == AF_INET6 )
        {
            struct sockaddr_in6 *addr = (struct sockaddr_in6 *)rua->addr;
            memcpy(addr, rua->cur->ai_addr, sizeof(struct sockaddr_in6));
            addr->sin6_port = htons(rua->port);
        }
        else
        {
            struct sockaddr_in *addr = (struct sockaddr_in *)rua->addr;
            memcpy(addr, rua->cur->ai_addr, sizeof(struct sockaddr_in));
            addr->sin_port = htons(rua->port);
        }

        return 0;

    case RUDP_RESOLV_NONE:
        return EDESTADDRREQ;

    case RUDP_RESOLV_FAILED:
    case RUDP_RESOLV_ERROR:
        return ENXIO;
    }
    return EINVAL;
}

rudp_error_t rudp_address_get(
    const struct rudp_address *rua,
    const struct sockaddr_storage **addr,
    socklen_t *addrsize)
{
    const struct sockaddr_in *addr4 = (const struct sockaddr_in *)rua->addr;
    int family = addr4->sin_family;

    switch ( (enum resolver_state)rua->resolver_state )
    {
    case RUDP_RESOLV_ADDR:
    case RUDP_RESOLV_DONE:
        *addr = rua->addr;
        *addrsize = family == AF_INET ?
            sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
        return 0;

    case RUDP_RESOLV_NONE:
        return EDESTADDRREQ;

    case RUDP_RESOLV_FAILED:
    case RUDP_RESOLV_ERROR:
        return ENXIO;
    }
    return EINVAL;
}

int rudp_address_compare(const struct rudp_address *rua,
                         const struct sockaddr_storage *addr)
{
    struct sockaddr_in6 *left6 = (struct sockaddr_in6 *)rua->addr;
    struct sockaddr_in *left = (struct sockaddr_in *)rua->addr;

    struct sockaddr_in6 *right6 = (struct sockaddr_in6 *)addr;
    struct sockaddr_in *right = (struct sockaddr_in *)addr;

    if ( left->sin_family != right->sin_family )
        return 1;

    if ( left->sin_family == AF_INET )
    {
        if ( left->sin_port != right->sin_port )
            return 1;

        return memcmp(
            &left->sin_addr,
            &right->sin_addr,
            sizeof(struct in_addr));
    }
    else
    {
        if ( left6->sin6_port != right6->sin6_port )
            return 1;

        return memcmp(
            &left6->sin6_addr,
            &right6->sin6_addr,
            sizeof(struct in6_addr));
    }
}

const char *rudp_address_text(struct rudp_address *rua)
{
    if ( rua->text[0] )
        return rua->text;

    const struct sockaddr_storage *addr;
    socklen_t size;
    rudp_error_t err = rudp_address_get(rua, &addr, &size);
    if ( err )
        return "<unresolved>";

    const struct sockaddr_in *addr4 = (const struct sockaddr_in *)addr;
    const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)addr;
    const void *sin = addr4->sin_family == AF_INET
        ? (void*)&addr4->sin_addr
        : (void*)&addr6->sin6_addr;

    if ( inet_ntop(addr4->sin_family, sin, rua->text, size) == NULL )
        return "<unresolved>";

    char *end = rua->text + strlen(rua->text);
    snprintf(end, 7, ":%d", (int)rua->port);

    return rua->text;
}
