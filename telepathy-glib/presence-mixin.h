/*
 * presence-mixin.h - Header for GabblePresenceMixin
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

#ifndef __TP_PRESENCE_MIXIN_H__
#define __TP_PRESENCE_MIXIN_H__

#include <telepathy-glib/enums.h>
#include <telepathy-glib/svc-connection.h>
#include "util.h"

G_BEGIN_DECLS

typedef struct _TpPresenceStatusOptionalArgumentSpec
    TpPresenceStatusOptionalArgumentSpec;
typedef struct _TpPresenceStatusSpec TpPresenceStatusSpec;

/**
 * TpPresenceStatusOptionalArgumentSpec
 * @name: Name of the argument as passed over D-Bus
 * @dtype: D-Bus type signature of the argument
 *
 * Structure representing an optional argument for a presence status.
 */
struct _TpPresenceStatusOptionalArgumentSpec {
    const gchar *name;
    const gchar *dtype;
};

/**
 * TpPresenceStatusSpec
 * @name: String identifier of the presence status
 * @presence_type: A type value, as specified by #TpConnectionPresenceType
 * @self: Indicates if this status may be set on yourself
 * @optional_arguments: An array of #TpPresenceStatusOptionalArgumentSpec
 *  structures representing the optional arguments for this status, terminated
 *  by a NULL name. If there are no optional arguments for a status, this can be
 *  NULL.
 *
 * Structure representing a presence status.
 *
 * In addition to the fields documented here, there are two gpointer fields
 * which must currently be %NULL. A meaning may be defined for these in a future
 * version of telepathy-glib.
 */
struct _TpPresenceStatusSpec {
    const gchar *name;
    TpConnectionPresenceType presence_type;
    gboolean self;
    const TpPresenceStatusOptionalArgumentSpec *optional_arguments;

    /*<private>*/
    gpointer _future1;
    gpointer _future2;
};

/**
 * TpPresenceMixinStatusAvailableFunc:
 * @obj: An object implementing the presence interface with this mixin
 * @nth_status: The index of the status in the provided statuses array
 *
 * Signature of the callback used to determine if a given status is currently
 * available.
 *
 * Returns: %TRUE if the status is available, %FALSE if not.
 */
typedef gboolean (*TpPresenceMixinStatusAvailableFunc) (GObject *obj,
    guint nth_status);

typedef struct _TpPresenceMixinClass TpPresenceMixinClass;
typedef struct _TpPresenceMixinClassPrivate TpPresenceMixinClassPrivate;
typedef struct _TpPresenceMixin TpPresenceMixin;
typedef struct _TpPresenceMixinPrivate TpPresenceMixinPrivate;

/**
 * TpPresenceMixinClass:
 * @status_available: The status-available function that was passed to
 *  tp_presence_mixin_class_init()
 * @statuses: The presence statuses array that was passed to
 *  tp_presence_mixin_class_init()
 *
 * Structure to be included in the class structure of objects that
 * use this mixin. Initialize it with tp_presence_mixin_class_init().
 *
 * All fields should be considered read-only.
 */
struct _TpPresenceMixinClass {
    TpPresenceMixinStatusAvailableFunc status_available;

    const TpPresenceStatusSpec *statuses;

    /*<private>*/
    TpPresenceMixinClassPrivate *priv;
};

/**
 * TpPresenceMixin:
 *
 * Structure to be included in the instance structure of objects that
 * use this mixin. Initialize it with tp_presence_mixin_init().
 *
 * There are no public fields.
 */
struct _TpPresenceMixin {
  /*<private>*/
  TpPresenceMixinPrivate *priv;
};

/* TYPE MACROS */
#define TP_PRESENCE_MIXIN_CLASS_OFFSET_QUARK \
  (tp_presence_mixin_class_get_offset_quark ())
#define TP_PRESENCE_MIXIN_CLASS_OFFSET(o) \
  (GPOINTER_TO_UINT (g_type_get_qdata (G_OBJECT_CLASS_TYPE (o), \
                                       TP_PRESENCE_MIXIN_CLASS_OFFSET_QUARK)))
#define TP_PRESENCE_MIXIN_CLASS(o) \
  ((TpPresenceMixinClass *) tp_mixin_offset_cast (o, \
    TP_PRESENCE_MIXIN_CLASS_OFFSET (o)))

#define TP_PRESENCE_MIXIN_OFFSET_QUARK (tp_presence_mixin_get_offset_quark ())
#define TP_PRESENCE_MIXIN_OFFSET(o) \
  (GPOINTER_TO_UINT (g_type_get_qdata (G_OBJECT_TYPE (o), \
                                       TP_PRESENCE_MIXIN_OFFSET_QUARK)))
#define TP_PRESENCE_MIXIN(o) \
  ((TpPresenceMixin *) tp_mixin_offset_cast (o, TP_PRESENCE_MIXIN_OFFSET (o)))

GQuark tp_presence_mixin_class_get_offset_quark (void);
GQuark tp_presence_mixin_get_offset_quark (void);

void tp_presence_mixin_class_init (GObjectClass *obj_cls, glong offset,
    TpPresenceMixinStatusAvailableFunc status_available,
    const TpPresenceStatusSpec *statuses);

void tp_presence_mixin_init (GObject *obj, glong offset);
void tp_presence_mixin_finalize (GObject *obj);

void tp_presence_mixin_iface_init (gpointer g_iface, gpointer iface_data);

G_END_DECLS

#endif /* #ifndef __TP_PRESENCE_MIXIN_H__ */
