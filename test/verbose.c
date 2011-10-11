/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#include <stdio.h>
#include <stdlib.h>

#include <rudp/rudp.h>

static
void *_rudp_default_alloc(struct rudp *rudp, size_t len)
{
    void *b = malloc(len);
    printf("%s: %d bytes allocated at %p\n",
           __FUNCTION__, (int)len, b);
    return b;
}

static
void _rudp_default_free(struct rudp *rudp, void *buffer)
{
    printf("%s: freed buffer at %p\n",
           __FUNCTION__, buffer);
    return free(buffer);
}

static
void _rudp_log(struct rudp *rudp,
               enum rudp_log_level level,
               const char *fmt,
               va_list arg)
{
    printf("%d ", level);
    vprintf(fmt, arg);
}

const struct rudp_handler verbose_handler =
{
    .log = _rudp_log,
    .mem_alloc = _rudp_default_alloc,
    .mem_free = _rudp_default_free,
};
