/*
 * tp-voip-engine.h - Header for TpVoipEngine
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

#ifndef __TP_VOIP_ENGINE_H__
#define __TP_VOIP_ENGINE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TpVoipEngine TpVoipEngine;
typedef struct _TpVoipEngineClass TpVoipEngineClass;

struct _TpVoipEngineClass {
    GObjectClass parent_class;
};

struct _TpVoipEngine {
    GObject parent;
};

GType tp_voip_engine_get_type(void);

/* TYPE MACROS */
#define TP_TYPE_VOIP_ENGINE \
  (tp_voip_engine_get_type())
#define TP_VOIP_ENGINE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_VOIP_ENGINE, TpVoipEngine))
#define TP_VOIP_ENGINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_VOIP_ENGINE, TpVoipEngineClass))
#define TP_IS_VOIP_ENGINE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_VOIP_ENGINE))
#define TP_IS_VOIP_ENGINE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_VOIP_ENGINE))
#define TP_VOIP_ENGINE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_VOIP_ENGINE, TpVoipEngineClass))


gboolean tp_voip_engine_handle_channel (TpVoipEngine *obj, const gchar * bus_name, const gchar * connection, const gchar * channel_type, const gchar * channel, guint handle_type, guint handle, GError **error);


void _tp_voip_engine_register (TpVoipEngine *self);

G_END_DECLS

#endif /* #ifndef __TP_VOIP_ENGINE_H__*/
