/*
 * call-content-media-description.h - Header for TpyCallContentMediaDescription
 * Copyright (C) 2009-2011 Collabora Ltd.
 * @author Sjoerd Simons <sjoerd.simons@collabora.com>
 * @author Olivier Crete <olivier.crete@collabora.com>
 * @author Xavier Claessens <xavier.claessens@collabora.co.uk>
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

#ifndef __TP_CALL_CONTENT_MEDIA_DESCRIPTION_H__
#define __TP_CALL_CONTENT_MEDIA_DESCRIPTION_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/dbus-properties-mixin.h>
#include <telepathy-glib/handle.h>

G_BEGIN_DECLS

typedef struct _TpCallContentMediaDescription TpCallContentMediaDescription;
typedef struct _TpCallContentMediaDescriptionPrivate
    TpCallContentMediaDescriptionPrivate;
typedef struct _TpCallContentMediaDescriptionClass
    TpCallContentMediaDescriptionClass;

struct _TpCallContentMediaDescriptionClass {
  /*<private>*/
  GObjectClass parent_class;

  TpDBusPropertiesMixinClass dbus_props_class;
  gpointer future[4];
};

struct _TpCallContentMediaDescription {
  /*<private>*/
  GObject parent;

  TpCallContentMediaDescriptionPrivate *priv;
};

GType tp_call_content_media_description_get_type (void);

/* TYPE MACROS */
#define TP_TYPE_CALL_CONTENT_MEDIA_DESCRIPTION \
  (tp_call_content_media_description_get_type ())
#define TP_CALL_CONTENT_MEDIA_DESCRIPTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  TP_TYPE_CALL_CONTENT_MEDIA_DESCRIPTION, TpCallContentMediaDescription))
#define TP_CALL_CONTENT_MEDIA_DESCRIPTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  TP_TYPE_CALL_CONTENT_MEDIA_DESCRIPTION, TpCallContentMediaDescriptionClass))
#define TP_IS_CALL_CONTENT_MEDIA_DESCRIPTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_CALL_CONTENT_MEDIA_DESCRIPTION))
#define TP_IS_CALL_CONTENT_MEDIA_DESCRIPTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_CALL_CONTENT_MEDIA_DESCRIPTION))
#define TP_CALL_CONTENT_MEDIA_DESCRIPTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CALL_CONTENT_MEDIA_DESCRIPTION, \
  TpCallContentMediaDescriptionClass))

TpCallContentMediaDescription *tp_call_content_media_description_new (
    TpDBusDaemon *dbus_daemon,
    const gchar *object_path,
    TpHandle remote_contact,
    gboolean has_remote_information,
    gboolean further_negotiation_required);

const gchar *tp_call_content_media_description_get_object_path (
    TpCallContentMediaDescription *self);
TpHandle tp_call_content_media_description_get_remote_contact (
    TpCallContentMediaDescription *self);

void tp_call_content_media_description_add_ssrc (
    TpCallContentMediaDescription *self,
    TpHandle contact,
    guint ssrc);

void tp_call_content_media_description_append_codec (
    TpCallContentMediaDescription *self,
    guint identifier,
    const gchar *name,
    guint clock_rate,
    guint channels,
    gboolean updated,
    GHashTable *parameters);

G_END_DECLS

#endif /* #ifndef __TP_CALL_CONTENT_MEDIA_DESCRIPTION_H__*/
