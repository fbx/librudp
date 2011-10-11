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
#include <rudp/client.h>

#define display_err(statement,r)                 \
    do { \
    rudp_error_t err = statement; \
    printf("%s:%d %s: %s\n", __FILE__, __LINE__, #statement, strerror(err)); \
    if (err) return r;                                                   \
    } while(0)

static
void handle_packet(
    struct rudp_client *client,
    int command, const void *data, size_t len)
{
    printf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__);
    printf(">>> command %d, message '''", command);
    fwrite(data, 1, len, stdout);
    printf("'''\n");
    if ( !strncmp((const char *)data, "quit", 4) )
        ela_exit(client->rudp->el);
}

static
void link_info(struct rudp_client *client, struct rudp_link_info *info)
{
    printf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__);
}

static
void server_lost(struct rudp_client *client)
{
    printf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__);

    display_err(  rudp_client_connect(client),  );
}

static
void connected(struct rudp_client *client)
{
    printf("%s:%d %s\n", __FILE__, __LINE__, __FUNCTION__);
}

static const struct rudp_client_handler handler = {
    .handle_packet = handle_packet,
    .link_info = link_info,
    .server_lost = server_lost,
    .connected = connected,
};

static
void handle_stdin(
    struct ela_event_source *source,
    int fd, uint32_t mask, void *data)
{
    struct rudp_client *client = data;
    char buffer[512], *tmp;

    tmp = fgets(buffer, 512, stdin);
    ssize_t size = strlen(tmp);
    rudp_client_send(client, 1, 0, tmp, size);
}

extern const struct rudp_handler verbose_handler;

int main(int argc, char **argv)
{
    struct rudp_client client;
    struct ela_el *el = ela_create(NULL);
    struct ela_event_source *source;
    struct rudp rudp;
    const struct rudp_handler *my_handler = RUDP_HANDLER_DEFAULT;
    const char *peer = "127.0.0.1";
    int peer_no = 1;

    if ( argc > 1 && !strcmp(argv[1], "-v") ) {
        my_handler = &verbose_handler;
        peer_no = 2;
    }

    if ( argc > peer_no )
        peer = argv[peer_no];

    rudp_init(&rudp, el, my_handler);

    ela_source_alloc(el, handle_stdin, &client, &source);

    display_err(  rudp_client_init(&client, &rudp, &handler) , 1);
    rudp_client_set_hostname(&client, peer, 4242, 0);
    display_err(  rudp_client_connect(&client) , 1);

    ela_set_fd(el, source, 0, ELA_EVENT_READABLE);
    ela_add(el, source);

    ela_run(el);

    ela_remove(el, source);
    ela_source_free(el, source);

    display_err(  rudp_client_close(&client) , 1);
    display_err(  rudp_client_deinit(&client) , 1);

    rudp_deinit(&rudp);
    ela_close(el);

    return 0;
}
