/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_COMPILER_H_
/** @hidden */
#define RUDP_COMPILER_H_

/**
   @file
   @hidden
*/

/* GCC visibility */
#if defined(__GNUC__) && __GNUC__ >= 4 /** mkdoc:skip */
#define RUDP_EXPORT __attribute__ ((visibility("default")))
#else
#define RUDP_EXPORT
#endif

/* GCC deprecated */
#if defined(__GNUC__) && __GNUC__ >= 4 /** mkdoc:skip */
#define RUDP_DEPRECATED __attribute__ ((deprecated))
#else
#define RUDP_DEPRECATED
#endif

#endif
