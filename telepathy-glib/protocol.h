/* TpProtocol
 *
 * Copyright © 2010 Collabora Ltd.
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

#ifndef TP_PROTOCOL_H
#define TP_PROTOCOL_H

#include <glib-object.h>

#include <telepathy-glib/capabilities.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

typedef struct _TpConnectionManagerParam TpConnectionManagerParam;

struct _TpConnectionManagerParam
{
  /*<public>*/
  gchar *name;
  gchar *dbus_signature;
  GValue default_value;
  guint flags;

  gpointer priv;
};

typedef struct _TpProtocol TpProtocol;
typedef struct _TpProtocolClass TpProtocolClass;
typedef struct _TpProtocolPrivate TpProtocolPrivate;
typedef struct _TpProtocolClassPrivate TpProtocolClassPrivate;

GType tp_protocol_get_type (void) G_GNUC_CONST;

#define TP_TYPE_PROTOCOL \
  (tp_protocol_get_type ())
#define TP_PROTOCOL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_PROTOCOL, \
                               TpProtocol))
#define TP_PROTOCOL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_PROTOCOL, \
                            TpProtocolClass))
#define TP_IS_PROTOCOL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_PROTOCOL))
#define TP_IS_PROTOCOL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_PROTOCOL))
#define TP_PROTOCOL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_PROTOCOL, \
                              TpProtocolClass))

struct _TpProtocol
{
  /*<private>*/
  TpProxy parent;
  TpProtocolPrivate *priv;
};

void tp_protocol_init_known_interfaces (void);

TpProtocol *tp_protocol_new (TpDBusDaemon *dbus, const gchar *cm_name,
    const gchar *protocol_name, const GHashTable *immutable_properties,
    GError **error);

const gchar *tp_protocol_get_name (TpProtocol *self);

#define TP_PROTOCOL_FEATURE_PARAMETERS \
  (tp_protocol_get_feature_quark_parameters ())
GQuark tp_protocol_get_feature_quark_parameters (void) G_GNUC_CONST;

const TpConnectionManagerParam *tp_protocol_get_param (TpProtocol *self,
    const gchar *param);
gboolean tp_protocol_has_param (TpProtocol *self,
    const gchar *param);
gboolean tp_protocol_can_register (TpProtocol *self);
GStrv tp_protocol_dup_param_names (TpProtocol *self) G_GNUC_WARN_UNUSED_RESULT;

const gchar * const *
/* gtk-doc sucks */
tp_protocol_get_authentication_types (TpProtocol *self);

#define TP_PROTOCOL_FEATURE_CORE \
  (tp_protocol_get_feature_quark_core ())
GQuark tp_protocol_get_feature_quark_core (void) G_GNUC_CONST;

const gchar *tp_protocol_get_vcard_field (TpProtocol *self);
const gchar *tp_protocol_get_english_name (TpProtocol *self);
const gchar *tp_protocol_get_icon_name (TpProtocol *self);
TpCapabilities *tp_protocol_get_capabilities (TpProtocol *self);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-protocol.h>

#endif
