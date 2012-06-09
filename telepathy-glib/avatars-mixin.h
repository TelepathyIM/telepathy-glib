/*
 * avatars-mixin.h - Header for TpAvatarsMixin
 * Copyright (C) 2012 Collabora Ltd.
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

#ifndef __TP_AVATARS_MIXIN_H__
#define __TP_AVATARS_MIXIN_H__

#include <telepathy-glib/enums.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/connection.h>

G_BEGIN_DECLS

typedef struct _TpAvatarsMixin TpAvatarsMixin;
typedef struct _TpAvatarsMixinPrivate TpAvatarsMixinPrivate;

typedef gboolean (*TpAvatarsMixinSetAvatarFunc) (GObject *object,
    const GArray *avatar,
    const gchar *mime_type,
    GError **error);
typedef gboolean (*TpAvatarsMixinClearAvatarFunc) (GObject *object,
    GError **error);
typedef gboolean (*TpAvatarsMixinRequestAvatarsFunc) (GObject *object,
    const GArray *contacts,
    GError **error);

struct _TpAvatarsMixin {
  /*<private>*/
  TpAvatarsMixinPrivate *priv;
};

/* Update avatar */
void tp_avatars_mixin_avatar_retrieved (GObject *object,
    TpHandle contact,
    const gchar *token,
    GArray *data,
    const gchar *mime_type);

void tp_avatars_mixin_avatar_changed (GObject *object,
    TpHandle contact,
    const gchar *token);

void tp_avatars_mixin_drop_avatar (GObject *object,
    TpHandle contact);

/* Initialisation */
void tp_avatars_mixin_init (GObject *object,
    glong offset,
    TpAvatarsMixinSetAvatarFunc set_avatar,
    TpAvatarsMixinClearAvatarFunc clear_avatar,
    TpAvatarsMixinRequestAvatarsFunc request_avatars,
    gboolean avatar_persists,
    TpAvatarRequirements *requirements);

void tp_avatars_mixin_finalize (GObject *object);

void tp_avatars_mixin_iface_init (gpointer g_iface,
    gpointer iface_data);

void tp_avatars_mixin_init_dbus_properties (GObjectClass *klass);

void tp_avatars_mixin_register_with_contacts_mixin (GObject *object);

G_END_DECLS

#endif /* #ifndef __TP_AVATARS_MIXIN_H__ */
