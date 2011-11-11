/*
 * call-content.h - high level API for Call contents
 *
 * Copyright (C) 2011 Collabora Ltd. <http://www.collabora.co.uk/>
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

#ifndef __TP_CALL_CONTENT_H__
#define __TP_CALL_CONTENT_H__

#include <telepathy-glib/proxy.h>
#include <telepathy-glib/call-channel.h>

G_BEGIN_DECLS

#define TP_TYPE_CALL_CONTENT (tp_call_content_get_type ())
#define TP_CALL_CONTENT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CALL_CONTENT, TpCallContent))
#define TP_CALL_CONTENT_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST ((obj), TP_TYPE_CALL_CONTENT, TpCallContentClass))
#define TP_IS_CALL_CONTENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CALL_CONTENT))
#define TP_IS_CALL_CONTENT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((obj), TP_TYPE_CALL_CONTENT))
#define TP_CALL_CONTENT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CALL_CONTENT, TpCallContentClass))

typedef struct _TpCallContent TpCallContent;
typedef struct _TpCallContentClass TpCallContentClass;
typedef struct _TpCallContentPrivate TpCallContentPrivate;

struct _TpCallContent
{
  /*<private>*/
  TpProxy parent;
  TpCallContentPrivate *priv;
};

struct _TpCallContentClass
{
  /*<private>*/
  TpProxyClass parent_class;
  GCallback _padding[7];
};

GType tp_call_content_get_type (void);

void tp_call_content_init_known_interfaces (void);

#define TP_CALL_CONTENT_FEATURE_CORE \
  tp_call_content_get_feature_quark_core ()
GQuark tp_call_content_get_feature_quark_core (void) G_GNUC_CONST;

const gchar *tp_call_content_get_name (TpCallContent *self);
TpMediaStreamType tp_call_content_get_media_type (TpCallContent *self);
TpCallContentDisposition tp_call_content_get_disposition (TpCallContent *self);
GPtrArray *tp_call_content_get_streams (TpCallContent *self);

G_END_DECLS

#include "_gen/tp-cli-call-content.h"

#endif
