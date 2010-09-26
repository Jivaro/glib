/* GLIB - Library of useful routines for C programming
 *
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __G_MEM_PRIVATE_H__
#define __G_MEM_PRIVATE_H__

#include <glib/gtypes.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL void _g_convert_deinit (void);
G_GNUC_INTERNAL void _g_dataset_deinit (void);
G_GNUC_INTERNAL void _g_main_deinit (void);
G_GNUC_INTERNAL void _g_quark_deinit (void);
G_GNUC_INTERNAL void _g_rand_deinit (void);
G_GNUC_INTERNAL void _g_slice_deinit (void);
G_GNUC_INTERNAL void _g_utils_deinit (void);

G_END_DECLS

#endif
