/*
  Librudp, a reliable UDP transport library.

  This file is part of FOILS, the Freebox Open Interface
  Libraries. This file is distributed under a 2-clause BSD license,
  see LICENSE.TXT for details.

  Copyright (c) 2011, Freebox SAS
  See AUTHORS for details
 */

#ifndef RUDP_LIST_H_
/** @hidden */
#define RUDP_LIST_H_

/**
   @file
   @hidden
*/

struct rudp_list;

#if 1 /* mkdoc:skip */

struct rudp_list
{
    struct rudp_list *next;
    struct rudp_list *prev;
};

#endif

#endif
