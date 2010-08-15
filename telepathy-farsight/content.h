/*
 * content.c - Source for TfContent
 * Copyright (C) 2010 Collabora Ltd.
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

#ifndef __TF_CONTENT_H__
#define __TF_CONTENT_H__

#include <glib-object.h>

#include <gst/gst.h>
#include <telepathy-glib/channel.h>

#include "call-channel.h"

G_BEGIN_DECLS

#define TF_TYPE_CONTENT tf_content_get_type()

#define TF_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_CONTENT, TfContent))

#define TF_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_CONTENT, TfContentClass))

#define TF_IS_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TF_TYPE_CONTENT))

#define TF_IS_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TF_TYPE_CONTENT))

#define TF_CONTENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_CONTENT, TfContentClass))

typedef struct _TfContentPrivate TfContentPrivate;

/**
 * TfContent:
 *
 * All members of the object are private
 */

typedef struct _TfContent TfContent;

/**
 * TfContentClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */

typedef struct _TfContentClass TfContentClass;

GType tf_content_get_type (void);

TfContent *tf_content_new (
    TfCallChannel *callchannel,
    const gchar *object_path,
    GError **error);

gboolean tf_content_bus_message (TfContent *content,
    GstMessage *message);

G_END_DECLS

#endif /* __TF_CONTENT_H__ */

