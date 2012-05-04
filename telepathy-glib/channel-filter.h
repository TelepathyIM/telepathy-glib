/*
 * A filter matching certain channels.
 *
 * Copyright © 2010-2014 Collabora Ltd.
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

#ifndef TP_CHANNEL_FILTER_H
#define TP_CHANNEL_FILTER_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>

G_BEGIN_DECLS

typedef struct _TpChannelFilter TpChannelFilter;
typedef struct _TpChannelFilterClass TpChannelFilterClass;
typedef struct _TpChannelFilterPrivate TpChannelFilterPrivate;

GType tp_channel_filter_get_type (void);

#define TP_TYPE_CHANNEL_FILTER \
  (tp_channel_filter_get_type ())
#define TP_CHANNEL_FILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CHANNEL_FILTER, \
                               TpChannelFilter))
#define TP_CHANNEL_FILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_CHANNEL_FILTER, \
                            TpChannelFilterClass))
#define TP_IS_CHANNEL_FILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CHANNEL_FILTER))
#define TP_IS_CHANNEL_FILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_CHANNEL_FILTER))
#define TP_CHANNEL_FILTER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CHANNEL_FILTER, \
                              TpChannelFilterClass))

_TP_AVAILABLE_IN_UNRELEASED G_GNUC_WARN_UNUSED_RESULT
TpChannelFilter *tp_channel_filter_new (void);

_TP_AVAILABLE_IN_UNRELEASED G_GNUC_WARN_UNUSED_RESULT
TpChannelFilter *tp_channel_filter_new_for_text_chats (void);

_TP_AVAILABLE_IN_UNRELEASED G_GNUC_WARN_UNUSED_RESULT
TpChannelFilter *tp_channel_filter_new_for_text_chatrooms (void);

_TP_AVAILABLE_IN_UNRELEASED G_GNUC_WARN_UNUSED_RESULT
TpChannelFilter *tp_channel_filter_new_for_calls (TpEntityType entity_type);

_TP_AVAILABLE_IN_UNRELEASED G_GNUC_WARN_UNUSED_RESULT
TpChannelFilter *tp_channel_filter_new_for_stream_tubes (const gchar *service);

_TP_AVAILABLE_IN_UNRELEASED G_GNUC_WARN_UNUSED_RESULT
TpChannelFilter *tp_channel_filter_new_for_dbus_tubes (const gchar *service);

_TP_AVAILABLE_IN_UNRELEASED G_GNUC_WARN_UNUSED_RESULT
TpChannelFilter *tp_channel_filter_new_for_file_transfers (
    const gchar *service);

_TP_AVAILABLE_IN_UNRELEASED
void tp_channel_filter_require_target_is_contact (TpChannelFilter *self);

_TP_AVAILABLE_IN_UNRELEASED
void tp_channel_filter_require_target_is_room (TpChannelFilter *self);

_TP_AVAILABLE_IN_UNRELEASED
void tp_channel_filter_require_no_target (TpChannelFilter *self);

_TP_AVAILABLE_IN_UNRELEASED
void tp_channel_filter_require_target_type (TpChannelFilter *self,
    TpEntityType entity_type);

_TP_AVAILABLE_IN_UNRELEASED
void tp_channel_filter_require_locally_requested (TpChannelFilter *self,
    gboolean requested);

_TP_AVAILABLE_IN_UNRELEASED
void tp_channel_filter_require_channel_type (TpChannelFilter *self,
    const gchar *channel_type);

_TP_AVAILABLE_IN_UNRELEASED
void tp_channel_filter_require_property (TpChannelFilter *self,
    const gchar *name,
    GVariant *value);

G_END_DECLS

#endif
