/* threading.h */

/* Firtree - A generic image processing library
 * Copyright (C) 2009 Rich Wareham <richwareham@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.    See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA    02110-1301, USA
 */

#ifndef COMMON_THREADING_H
#define COMMON_THREADING_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * ThreadingApplyFunc:
 *
 * A worker function suitable for passing to threading_apply().
 */
typedef void (*ThreadingApplyFunc) ( guint i, gpointer data );

 /**
  * threading_apply:
  * @count: Number of times to apply @func.
  * @func: The function to call.
  * @data: Data to pass to the function.
  *
  * Call @func @count times with the initial i parameter of @func taking 
  * unique values 0 to @count-1. There is no guarantee of the order of i and
  * @func may be called simultaneously from multiple threads.
  */
void
threading_apply(guint count, ThreadingApplyFunc func, gpointer data);

G_END_DECLS
 
#endif /* end of include guard: COMMON_THREADING_H */
