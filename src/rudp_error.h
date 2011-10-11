/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_ERROR_IMPL_H
#define RUDP_ERROR_IMPL_H

static inline
rudp_error_t rudp_error_from_ela(ela_error_t e)
{
    return e;
}

#endif
