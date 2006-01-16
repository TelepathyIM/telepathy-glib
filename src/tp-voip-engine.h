/*
 * voip-engine.h - Header for VoipEngine
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
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

#ifndef __VOIP_ENGINE_H__
#define __VOIP_ENGINE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _VoipEngine VoipEngine;
typedef struct _VoipEngineClass VoipEngineClass;

struct _VoipEngineClass {
    GObjectClass parent_class;
};

struct _VoipEngine {
    GObject parent;
};

GType voip_engine_get_type(void);

/* TYPE MACROS */
#define VOIP_TYPE_ENGINE \
  (voip_engine_get_type())
#define VOIP_ENGINE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), VOIP_TYPE_ENGINE, VoipEngine))
#define VOIP_ENGINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), VOIP_TYPE_ENGINE, VoipEngineClass))
#define VOIP_IS_ENGINE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), VOIP_TYPE_ENGINE))
#define VOIP_IS_ENGINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), VOIP_TYPE_ENGINE))
#define VOIP_ENGINE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), VOIP_TYPE_ENGINE, VoipEngineClass))


gboolean voip_engine_handle_channel (VoipEngine *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error);


G_END_DECLS

#endif /* #ifndef __VOIP_ENGINE_H__*/
