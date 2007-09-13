/*
 * presence-mixin.h - Header for TpPresenceMixin
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
#include <telepathy-glib/handle.h>
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
 * Structure specifying a supported optional argument for a presence status.
 *
 * In addition to the fields documented here, there are two gpointer fields
 * which must currently be %NULL. A meaning may be defined for these in a
 * future version of telepathy-glib.
 */
struct _TpPresenceStatusOptionalArgumentSpec {
    const gchar *name;
    const gchar *dtype;

    /*<private>*/
    gpointer _future1;
    gpointer _future2;
};

/**
 * TpPresenceStatusSpec
 * @name: String identifier of the presence status
 * @presence_type: A type value, as specified by #TpConnectionPresenceType
 * @self: Indicates if this status may be set on yourself
 * @optional_arguments: An array of #TpPresenceStatusOptionalArgumentSpec
 *  structures representing the optional arguments for this status, terminated
 *  by a NULL name. If there are no optional arguments for a status, this can
 *  be NULL.
 *
 * Structure specifying a supported presence status.
 *
 * In addition to the fields documented here, there are two gpointer fields
 * which must currently be %NULL. A meaning may be defined for these in a
 * future version of telepathy-glib.
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

typedef struct _TpPresenceStatus TpPresenceStatus;

/**
 * TpPresenceStatus:
 * @index: Index of the presence status in the provided supported presence
 *  statuses array
 * @optional_arguments: A GHashTable mapping of string identifiers to GValues
 *  of the optional status arguments, if any. If there are no optional
 *  arguments, this pointer may be NULL.
 *
 * Structure representing a presence status.
 *
 * In addition to the fields documented here, there are two gpointer fields
 * which must currently be %NULL. A meaning may be defined for these in a
 * future version of telepathy-glib.
 */
struct _TpPresenceStatus {
    guint index;
    GHashTable *optional_arguments;

    /*<private>*/
    gpointer _future1;
    gpointer _future2;
};

TpPresenceStatus *tp_presence_status_new (guint index,
    GHashTable *optional_arguments);
void tp_presence_status_free (TpPresenceStatus *status);

/**
 * TpPresenceMixinStatusAvailableFunc:
 * @obj: An object implementing the presence interface with this mixin
 * @index: The index of the presence status in the provided supported presence
 *  statuses array
 *
 * Signature of the callback used to determine if a given status is currently
 * available to be set on the connection.
 *
 * Returns: %TRUE if the status is available, %FALSE if not.
 */
typedef gboolean (*TpPresenceMixinStatusAvailableFunc) (GObject *obj,
    guint index);

/**
 * TpPresenceMixinGetContactStatusesFunc:
 * @obj: An object with this mixin.
 * @contacts: An array of #TpHandle for the contacts to get presence status for
 * @error: Used to return a Telepathy D-Bus error if %NULL is returned
 *
 * Signature of the callback used to get the stored presence status of
 * contacts. The returned hash table should have contact handles mapped to
 * their respective presence statuses in #TpPresenceStatus structs.
 *
 * The returned hash table will be freed with g_hash_table_destroy. The
 * callback is responsible for ensuring that this does any cleanup that
 * may be necessary.
 *
 * Returns: The contact presence on success, %NULL with error set on error
 */
typedef GHashTable *(*TpPresenceMixinGetContactStatusesFunc) (GObject *obj,
    const GArray *contacts, GError **error);

/**
 * TpPresenceMixinSetOwnStatusFunc:
 * @obj: An object with this mixin.
 * @status: The status to set, or NULL for whatever the protocol defines as a
 *  "default" status
 * @error: Used to return a Telepathy D-Bus error if %FALSE is returned
 *
 * Signature of the callback used to commit changes to the user's own presence
 * status in SetStatuses. It is also used in ClearStatus and RemoveStatus to
 * reset the user's own status back to the "default" one with a %NULL status
 * argument.
 *
 * The optional_arguments hash table in @status, if not NULL, will have been
 * filtered so it only contains recognised parameters, so the callback
 * need not (and cannot) check for unrecognised parameters. However, the
 * types of the parameters are not currently checked, so the callback is
 * responsible for doing so.
 *
 * The callback is responsible for emitting PresenceUpdate, if appropriate,
 * by calling tp_presence_mixin_emit_presence_update().
 *
 * Returns: %TRUE if the operation was successful, %FALSE if not.
 */
typedef gboolean (*TpPresenceMixinSetOwnStatusFunc) (GObject *obj,
    const TpPresenceStatus *status, GError **error);

typedef struct _TpPresenceMixinClass TpPresenceMixinClass;
typedef struct _TpPresenceMixinClassPrivate TpPresenceMixinClassPrivate;
typedef struct _TpPresenceMixin TpPresenceMixin;
typedef struct _TpPresenceMixinPrivate TpPresenceMixinPrivate;

/**
 * TpPresenceMixinClass:
 * @status_available: The status-available function that was passed to
 *  tp_presence_mixin_class_init()
 * @get_contact_statuses: The get-contact-statuses function that was passed to
 *  tp_presence_mixin_class_init()
 * @set_own_status: The set-own-status function that was passed to
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
    TpPresenceMixinGetContactStatusesFunc get_contact_statuses;
    TpPresenceMixinSetOwnStatusFunc set_own_status;

    const TpPresenceStatusSpec *statuses;

    /*<private>*/
    TpPresenceMixinClassPrivate *priv;
    gpointer _future1;
    gpointer _future2;
    gpointer _future3;
    gpointer _future4;
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
    TpPresenceMixinGetContactStatusesFunc get_contact_statuses,
    TpPresenceMixinSetOwnStatusFunc set_own_status,
    const TpPresenceStatusSpec *statuses);

void tp_presence_mixin_init (GObject *obj, glong offset);
void tp_presence_mixin_finalize (GObject *obj);

void tp_presence_mixin_emit_presence_update (GObject *obj,
    GHashTable *contact_presences);
void tp_presence_mixin_emit_one_presence_update (GObject *obj,
    TpHandle handle, const TpPresenceStatus *status);

void tp_presence_mixin_iface_init (gpointer g_iface, gpointer iface_data);

G_END_DECLS

#endif /* #ifndef __TP_PRESENCE_MIXIN_H__ */
