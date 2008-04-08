/*
 * message-mixin.h - Header for TpMessageMixin
 * Copyright (C) 2006-2008 Collabora Ltd.
 * Copyright (C) 2006-2008 Nokia Corporation
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

#ifndef TP_MESSAGE_MIXIN_H
#define TP_MESSAGE_MIXIN_H

#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/svc-channel.h>
#include "util.h"

G_BEGIN_DECLS

typedef struct _TpMessageMixinClass TpMessageMixinClass;
typedef struct _TpMessageMixinClassPrivate TpMessageMixinClassPrivate;
typedef struct _TpMessageMixin TpMessageMixin;
typedef struct _TpMessageMixinPrivate TpMessageMixinPrivate;

struct _TpMessageMixinClass {
    /*<private>*/
    TpMessageMixinClassPrivate *priv;
};

struct _TpMessageMixin {
  /*<private>*/
  TpMessageMixinPrivate *priv;
};

#define TP_MESSAGE_MIXIN_CLASS_OFFSET_QUARK \
  (tp_message_mixin_class_get_offset_quark ())
#define TP_MESSAGE_MIXIN_CLASS_OFFSET(o) \
  (GPOINTER_TO_UINT (g_type_get_qdata (G_OBJECT_CLASS_TYPE (o), \
                                       TP_MESSAGE_MIXIN_CLASS_OFFSET_QUARK)))
#define TP_MESSAGE_MIXIN_CLASS(o) \
  ((TpMessageMixinClass *) tp_mixin_offset_cast (o, \
    TP_MESSAGE_MIXIN_CLASS_OFFSET (o)))

#define TP_MESSAGE_MIXIN_OFFSET_QUARK (tp_message_mixin_get_offset_quark ())
#define TP_MESSAGE_MIXIN_OFFSET(o) \
  (GPOINTER_TO_UINT (g_type_get_qdata (G_OBJECT_TYPE (o), \
                                       TP_MESSAGE_MIXIN_OFFSET_QUARK)))
#define TP_MESSAGE_MIXIN(o) \
  ((TpMessageMixin *) tp_mixin_offset_cast (o, TP_MESSAGE_MIXIN_OFFSET (o)))

GQuark tp_message_mixin_class_get_offset_quark (void);
GQuark tp_message_mixin_get_offset_quark (void);

void tp_message_mixin_class_init (GObjectClass *obj_cls, gsize offset);

void tp_message_mixin_init (GObject *obj, gsize offset,
    TpHandleRepoIface *contact_repo);
void tp_message_mixin_finalize (GObject *obj);

void tp_message_mixin_text_iface_init (gpointer g_iface, gpointer iface_data);
void tp_message_mixin_message_parts_iface_init (gpointer g_iface,
    gpointer iface_data);

G_END_DECLS

#endif
