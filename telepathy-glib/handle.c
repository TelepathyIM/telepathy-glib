/*
 * handle.c - basic Telepathy-GLib handle functionality
 *
 * Copyright (C) 2007 Collabora Ltd.
 * Copyright (C) 2007 Nokia Corporation
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

#include "config.h"

#include <telepathy-glib/handle.h>

/**
 * tp_entity_type_to_string:
 * @type: A entity type, which need not be valid
 *
 * <!---->
 *
 * Returns: a human-readable string describing the entity type, e.g. "contact".
 *  For invalid entity types, returns "(no handle)" for 0 or
 *  "(invalid entity type)" for others.
 */
const gchar *
tp_entity_type_to_string (TpEntityType type)
{
  switch (type)
    {
    case TP_ENTITY_TYPE_NONE:
      return "(no handle)";
    case TP_ENTITY_TYPE_CONTACT:
      return "contact";
    case TP_ENTITY_TYPE_ROOM:
      return "room";
    case TP_ENTITY_TYPE_SELF:
      return "self";
    }

  return "(invalid entity type)";
}
