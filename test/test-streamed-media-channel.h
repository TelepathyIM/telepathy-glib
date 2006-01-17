/*
 * test-streamed-media-channel.h - Header for TestStreamedMediaChannel
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

#ifndef __TEST_STREAMED_MEDIA_CHANNEL_H__
#define __TEST_STREAMED_MEDIA_CHANNEL_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TestStreamedMediaChannel TestStreamedMediaChannel;
typedef struct _TestStreamedMediaChannelClass TestStreamedMediaChannelClass;

struct _TestStreamedMediaChannelClass {
    GObjectClass parent_class;
};

struct _TestStreamedMediaChannel {
    GObject parent;
};

GType test_streamed_media_channel_get_type(void);

/* TYPE MACROS */
#define TEST_TYPE_STREAMED_MEDIA_CHANNEL \
  (test_streamed_media_channel_get_type())
#define TEST_STREAMED_MEDIA_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TEST_TYPE_STREAMED_MEDIA_CHANNEL, TestStreamedMediaChannel))
#define TEST_STREAMED_MEDIA_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TEST_TYPE_STREAMED_MEDIA_CHANNEL, TestStreamedMediaChannelClass))
#define TEST_IS_STREAMED_MEDIA_CHANNEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TEST_TYPE_STREAMED_MEDIA_CHANNEL))
#define TEST_IS_STREAMED_MEDIA_CHANNEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TEST_TYPE_STREAMED_MEDIA_CHANNEL))
#define TEST_STREAMED_MEDIA_CHANNEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_STREAMED_MEDIA_CHANNEL, TestStreamedMediaChannelClass))


gboolean test_streamed_media_channel_add_members (TestStreamedMediaChannel *obj, const GArray * contacts, const gchar * message, GError **error);
gboolean test_streamed_media_channel_close (TestStreamedMediaChannel *obj, GError **error);
gboolean test_streamed_media_channel_get_channel_type (TestStreamedMediaChannel *obj, gchar ** ret, GError **error);
gboolean test_streamed_media_channel_get_group_flags (TestStreamedMediaChannel *obj, guint* ret, GError **error);
gboolean test_streamed_media_channel_get_handle (TestStreamedMediaChannel *obj, guint* ret, guint* ret1, GError **error);
gboolean test_streamed_media_channel_get_interfaces (TestStreamedMediaChannel *obj, gchar *** ret, GError **error);
gboolean test_streamed_media_channel_get_local_pending_members (TestStreamedMediaChannel *obj, GArray ** ret, GError **error);
gboolean test_streamed_media_channel_get_members (TestStreamedMediaChannel *obj, GArray ** ret, GError **error);
gboolean test_streamed_media_channel_get_remote_pending_members (TestStreamedMediaChannel *obj, GArray ** ret, GError **error);
gboolean test_streamed_media_channel_get_self_handle (TestStreamedMediaChannel *obj, guint* ret, GError **error);
gboolean test_streamed_media_channel_get_session_handlers (TestStreamedMediaChannel *obj, DBusGMethodInvocation *context);
gboolean test_streamed_media_channel_remove_members (TestStreamedMediaChannel *obj, const GArray * contacts, const gchar * message, GError **error);


G_END_DECLS

#endif /* #ifndef __TEST_STREAMED_MEDIA_CHANNEL_H__*/
