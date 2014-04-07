/*
 * handle.h - Header for basic Telepathy-GLib handle functionality
 *
 * Copyright (C) 2005, 2007 Collabora Ltd.
 * Copyright (C) 2005, 2007 Nokia Corporation
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

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_HANDLE_H__
#define __TP_HANDLE_H__

#include <glib.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>

G_BEGIN_DECLS

/**
 * TpHandle:
 *
 * Type representing Telepathy handles within telepathy-glib.
 *
 * This is guint despite the wire protocol having 32-bit integers, because
 * dbus-glib expects GArrays of guint and so on. If the dbus-glib ABI changes
 * in future, telepathy-glib is likely to have a matching ABI change.
 */
typedef guint TpHandle;

/**
 * TP_TYPE_HANDLE:
 *
 * The GType of a TpHandle, currently G_TYPE_UINT.
 *
 * This won't change unless in an ABI-incompatible version of telepathy-glib.
 */
#define TP_TYPE_HANDLE G_TYPE_UINT

/**
 * TP_UNKNOWN_ENTITY_TYPE:
 *
 * An invalid entity type (-1 cast to TpEntityType) used to represent an
 * unknown entity type.
 *
 * Since: 0.7.0
 */
#define TP_UNKNOWN_ENTITY_TYPE ((TpEntityType) -1)

/**
 * tp_entity_type_is_valid:
 * @type: A entity type, valid or not, to be checked
 * @error: Set if the entity type is invalid
 *
 * If the given entity type is valid, return %TRUE. If not, set @error
 * and return %FALSE.
 *
 * Returns: %TRUE if the entity type is valid.
 */
static inline
/* spacer so gtkdoc documents this function as though not static */
gboolean tp_entity_type_is_valid (TpEntityType type, GError **error);

/* Must be static inline because it references TP_NUM_ENTITY_TYPES -
 * if it wasn't inlined, a newer libtelepathy-glib with a larger number
 * of entity types might accept entity types that won't fit in the
 * connection manager's array of length TP_NUM_ENTITY_TYPES
 */

static inline gboolean
tp_entity_type_is_valid (TpEntityType type, GError **error)
{
  if (type > TP_ENTITY_TYPE_NONE && type < TP_NUM_ENTITY_TYPES)
    return TRUE;

  tp_g_set_error_invalid_entity_type (type, error);
  return FALSE;
}

const gchar *tp_entity_type_to_string (TpEntityType type);

G_END_DECLS

#endif
