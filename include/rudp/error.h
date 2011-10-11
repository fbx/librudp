/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_ERROR_H_
/** @hidden */
#define RUDP_ERROR_H_

#include <rudp/compiler.h>

/**
   @file
   @module {Errors}
   @short Common error representation

   Error handling avoids usage of the @tt errno C global. Error codes
   are directly returned from functions where relevant. They use the
   common @ref rudp_error_t type.  This type only means the value is
   an error. Error codes are taken from errno definition.
 */

/**
   @this is an error code type
 */
typedef int rudp_error_t;

#endif
