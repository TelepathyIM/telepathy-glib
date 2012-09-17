/*
 * media-signalling-content.h - Source for TfMediaSignallingContent
 * Copyright (C) 2011 Collabora Ltd.
 * Copyright (C) 2011 Nokia Corporation
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

#ifndef __TF_CALL_CONTENT_H__
#define __TF_CALL_CONTENT_H__

#include <glib-object.h>

#include <gst/gst.h>
#include <telepathy-glib/telepathy-glib.h>

#include "media-signalling-channel.h"
#include "content.h"
#include "content-priv.h"

G_BEGIN_DECLS

#define TF_TYPE_MEDIA_SIGNALLING_CONTENT tf_media_signalling_content_get_type()

#define TF_MEDIA_SIGNALLING_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TF_TYPE_MEDIA_SIGNALLING_CONTENT, TfMediaSignallingContent))

#define TF_MEDIA_SIGNALLING_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TF_TYPE_MEDIA_SIGNALLING_CONTENT, TfMediaSignallingContentClass))

#define TF_IS_MEDIA_SIGNALLING_CONTENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TF_TYPE_MEDIA_SIGNALLING_CONTENT))

#define TF_IS_MEDIA_SIGNALLING_CONTENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TF_TYPE_MEDIA_SIGNALLING_CONTENT))

#define TF_MEDIA_SIGNALLING_CONTENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TF_TYPE_MEDIA_SIGNALLING_CONTENT, TfMediaSignallingContentClass))

typedef struct _TfMediaSignallingContentPrivate TfMediaSignallingContentPrivate;

/**
 * TfMediaSignallingContent:
 *
 * All members of the object are private
 */

typedef struct _TfMediaSignallingContent TfMediaSignallingContent;

/**
 * TfMediaSignallingContentClass:
 * @parent_class: the parent #GObjecClass
 *
 * There are no overridable functions
 */

typedef struct _TfMediaSignallingContentClass TfMediaSignallingContentClass;

GType tf_media_signalling_content_get_type (void);

TfMediaSignallingContent *
tf_media_signalling_content_new (
    TfMediaSignallingChannel *media_signalling_channel,
    TfStream *stream,
    guint handle);

G_END_DECLS

#endif /* __TF_MEDIA_SIGNALLING_CONTENT_H__ */

