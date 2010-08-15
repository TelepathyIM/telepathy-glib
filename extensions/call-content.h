/*
 * call-content.h - proxy for a Content in a Call channel
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
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

#ifndef TF_FUTURE_CALL_CONTENT_H
#define TF_FUTURE_CALL_CONTENT_H

#include <telepathy-glib/channel.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _TfFutureCallContent TfFutureCallContent;
typedef struct _TfFutureCallContentPrivate TfFutureCallContentPrivate;
typedef struct _TfFutureCallContentClass TfFutureCallContentClass;

GType tf_future_call_content_get_type (void);

/* TYPE MACROS */
#define TF_FUTURE_TYPE_CALL_CONTENT \
  (tf_future_call_content_get_type ())
#define TF_FUTURE_CALL_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TF_FUTURE_TYPE_CALL_CONTENT, \
                              TfFutureCallContent))
#define TF_FUTURE_CALL_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TF_FUTURE_TYPE_CALL_CONTENT, \
                           TfFutureCallContentClass))
#define TF_FUTURE_IS_CALL_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TF_FUTURE_TYPE_CALL_CONTENT))
#define TF_FUTURE_IS_CALL_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TF_FUTURE_TYPE_CALL_CONTENT))
#define TF_FUTURE_CALL_CONTENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TF_FUTURE_TYPE_CALL_CONTENT, \
                              TfFutureCallContentClass))

TfFutureCallContent *tf_future_call_content_new (TpChannel *channel,
    const gchar *object_path, GError **error);

void tf_future_call_content_init_known_interfaces (void);

G_END_DECLS

#include "extensions/_gen/cli-call-content.h"

#endif
