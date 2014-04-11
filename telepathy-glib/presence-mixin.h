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

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_PRESENCE_MIXIN_H__
#define __TP_PRESENCE_MIXIN_H__

#include <telepathy-glib/enums.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/base-connection.h>
#include "util.h"

G_BEGIN_DECLS

/* -- TpPresenceStatusSpec -- */

typedef struct _TpPresenceStatusSpec TpPresenceStatusSpec;
typedef struct _TpPresenceStatusSpecPrivate TpPresenceStatusSpecPrivate;

struct _TpPresenceStatusSpec {
    const gchar *name;
    TpConnectionPresenceType presence_type;
    gboolean self;
    gboolean has_message;

    /*<private>*/
    GCallback _future[10];
    TpPresenceStatusSpecPrivate *priv;
};

_TP_AVAILABLE_IN_1_0
TpConnectionPresenceType tp_presence_status_spec_get_presence_type (
    const TpPresenceStatusSpec *self);

_TP_AVAILABLE_IN_1_0
const gchar *tp_presence_status_spec_get_name (
    const TpPresenceStatusSpec *self);

_TP_AVAILABLE_IN_1_0
gboolean tp_presence_status_spec_can_set_on_self (
    const TpPresenceStatusSpec *self);

_TP_AVAILABLE_IN_1_0
gboolean tp_presence_status_spec_has_message (
    const TpPresenceStatusSpec *self);

_TP_AVAILABLE_IN_1_0
GType tp_presence_status_spec_get_type (void);

_TP_AVAILABLE_IN_1_0
TpPresenceStatusSpec *tp_presence_status_spec_new (const gchar *name,
    TpConnectionPresenceType type,
    gboolean can_set_on_self,
    gboolean has_message);

_TP_AVAILABLE_IN_1_0
TpPresenceStatusSpec *tp_presence_status_spec_copy (
    const TpPresenceStatusSpec *self);

_TP_AVAILABLE_IN_1_0
void tp_presence_status_spec_free (TpPresenceStatusSpec *self);

/* -- TpPresenceStatus -- */


typedef struct _TpPresenceStatus TpPresenceStatus;

struct _TpPresenceStatus {
    guint index;
    gchar *message;

    /*<private>*/
    gpointer _future[6];
};

TpPresenceStatus *tp_presence_status_new (guint which,
    const gchar *message) G_GNUC_WARN_UNUSED_RESULT;
void tp_presence_status_free (TpPresenceStatus *status);

/* -- TpPresenceMixinInterface -- */

#define TP_TYPE_PRESENCE_MIXIN \
  (tp_presence_mixin_get_type ())

#define TP_IS_PRESENCE_MIXIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_TYPE_PRESENCE_MIXIN))

#define TP_PRESENCE_MIXIN_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  TP_TYPE_PRESENCE_MIXIN, TpPresenceMixinInterface))

typedef struct _TpPresenceMixinInterface TpPresenceMixinInterface;
/* For some reason g-i wants that name */
typedef struct _TpPresenceMixinInterface TpPresenceMixin;

typedef gboolean (*TpPresenceMixinStatusAvailableFunc) (TpBaseConnection *self,
    guint which);

typedef TpPresenceStatus *(*TpPresenceMixinGetContactStatusFunc) (
    TpBaseConnection *self,
    TpHandle contact);

typedef gboolean (*TpPresenceMixinSetOwnStatusFunc) (TpBaseConnection *self,
    const TpPresenceStatus *status,
    GError **error);

typedef guint (*TpPresenceMixinGetMaximumStatusMessageLengthFunc) (
    TpBaseConnection *self);

struct _TpPresenceMixinInterface {
  GTypeInterface parent;

  TpPresenceMixinStatusAvailableFunc status_available;
  TpPresenceMixinGetContactStatusFunc get_contact_status;
  TpPresenceMixinSetOwnStatusFunc set_own_status;
  TpPresenceMixinGetMaximumStatusMessageLengthFunc
      get_maximum_status_message_length;

  const TpPresenceStatusSpec *statuses;
};

GType tp_presence_mixin_get_type (void) G_GNUC_CONST;

void tp_presence_mixin_emit_presence_update (TpBaseConnection *self,
    GHashTable *contact_presences);
void tp_presence_mixin_emit_one_presence_update (TpBaseConnection *self,
    TpHandle handle,
    const TpPresenceStatus *status);

G_END_DECLS

#endif /* #ifndef __TP_PRESENCE_MIXIN_H__ */
