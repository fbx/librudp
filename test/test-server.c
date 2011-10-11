/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include <rudp/rudp.h>
#include <rudp/server.h>

#define display_err(statement) \
    do { \
    rudp_error_t err = statement; \
    printf("%s:%d %s: %s\n", __FILE__, __LINE__, #statement, strerror(err)); \
    } while(0)

static
void handle_packet(struct rudp_server *server,
                   struct rudp_peer *peer,
                   int command, const void *data, size_t len)
{
    printf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__);
    printf(">>> command %d message '''", command);
    fwrite(data, 1, len, stdout);
    printf("'''\n");
    if ( !strncmp((const char *)data, "quit", 4) )
        ela_exit(server->rudp->el);
}

static
void link_info(
    struct rudp_server *server,
    struct rudp_peer *peer,
    struct rudp_link_info *info)
{
    printf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__);
}

static
void peer_dropped(struct rudp_server *server, struct rudp_peer *peer)
{
    printf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__);
}

static
void peer_new(struct rudp_server *server, struct rudp_peer *peer)
{
    printf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__);
}

static const struct rudp_server_handler handler = {
    .handle_packet = handle_packet,
    .link_info = link_info,
    .peer_dropped = peer_dropped,
    .peer_new = peer_new,
};

static
void handle_stdin(
    struct ela_event_source *source,
    int fd, uint32_t mask, void *data)
{
    struct rudp_server *server = data;
    char buffer[512], *tmp;

    tmp = fgets(buffer, 512, stdin);
    ssize_t size = strlen(tmp);
    rudp_server_send_all(server, 1, 0, tmp, size);
}

extern const struct rudp_handler verbose_handler;

int main(int argc, char **argv)
{
    struct rudp_server server;
    struct in_addr address;
    struct ela_el *el = ela_create(NULL);
    struct ela_event_source *source;
    struct rudp rudp;
    const struct rudp_handler *my_handler = RUDP_HANDLER_DEFAULT;

    if ( argc > 1 && !strcmp(argv[1], "-v") )
        my_handler = &verbose_handler;

    rudp_init(&rudp, el, my_handler);

    ela_source_alloc(el, handle_stdin, &server, &source);

    address.s_addr = INADDR_ANY;

    display_err(  rudp_server_init(&server, &rudp, &handler)  );
    rudp_server_set_ipv4(&server, &address, 4242);
    display_err(  rudp_server_bind(&server)  );

    ela_set_fd(el, source, 0, ELA_EVENT_READABLE);
    ela_add(el, source);

    ela_run(el);

    ela_remove(el, source);
    ela_source_free(el, source);

    display_err(  rudp_server_close(&server)  );
    display_err(  rudp_server_deinit(&server)  );

    rudp_deinit(&rudp);
    ela_close(el);

    return 0;
}
