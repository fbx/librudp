/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_LOG_IMPL_H
#define RUDP_LOG_IMPL_H

#include <rudp/rudp.h>

static inline
void rudp_log_printf(
    struct rudp *rudp,
    const enum rudp_log_level level,
    const char *fmt, ...)
{
    if ( rudp->handler->log == NULL )
        return;

    va_list arg;
    va_start(arg, fmt);

    rudp->handler->log(rudp, level, fmt, arg);

    va_end(arg);
}

static inline
void *rudp_alloc(struct rudp *rudp, size_t len)
{
    return rudp->handler->mem_alloc(rudp, len);
}

static inline
void rudp_free(struct rudp *rudp, void *buffer)
{
    return rudp->handler->mem_free(rudp, buffer);
}

#endif
