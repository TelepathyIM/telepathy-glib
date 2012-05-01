/*
 * defs.h - miscellaneous definitions
 *
 * Copyright (C) 2007-2009 Collabora Ltd.
 * Copyright (C) 2007-2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __TP_DEFS_H__
#define __TP_DEFS_H__

#include <glib.h>

G_BEGIN_DECLS

/* telepathy-glib-specific version of G_GNUC_DEPRECATED so our regression
 * tests can continue to test deprecated functionality, while avoiding
 * deprecated bits of other libraries */
#ifdef _TP_IGNORE_DEPRECATIONS
#define _TP_GNUC_DEPRECATED /* nothing */
#define _TP_GNUC_DEPRECATED_FOR(f) /* nothing */
#else
#define _TP_GNUC_DEPRECATED G_GNUC_DEPRECATED
#define _TP_GNUC_DEPRECATED_FOR(f) G_GNUC_DEPRECATED_FOR(f)
#endif

G_END_DECLS
#endif
