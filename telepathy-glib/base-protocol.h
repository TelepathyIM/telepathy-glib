/* TpBaseProtocol
 *
 * Copyright © 2007-2010 Collabora Ltd.
 * Copyright © 2007-2009 Nokia Corporation
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

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef TP_BASE_PROTOCOL_H
#define TP_BASE_PROTOCOL_H

#include <glib-object.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/presence-mixin.h>

G_BEGIN_DECLS

typedef struct _TpCMParamSpec TpCMParamSpec;

typedef GVariant *(*TpCMParamFilter) (const TpCMParamSpec *paramspec,
    GVariant *value, gpointer user_data, GError **error);

GVariant *tp_cm_param_filter_string_nonempty (const TpCMParamSpec *paramspec,
    GVariant *value, gpointer user_data, GError **error);

GVariant *tp_cm_param_filter_uint_nonzero (const TpCMParamSpec *paramspec,
    GVariant *value, gpointer user_data, GError **error);

struct _TpCMParamSpec {
    gchar *name;
    const gchar *dtype;
    TpConnMgrParamFlags flags;
    GVariant *def;

    /*<private>*/
    gpointer _future[5];

    TpCMParamFilter filter;
    gpointer user_data;
    GDestroyNotify destroy;

    guint ref_count;
};

TpCMParamSpec *tp_cm_param_spec_new (const gchar *name,
    TpConnMgrParamFlags flags,
    GVariant *def,
    TpCMParamFilter filter,
    gpointer user_data,
    GDestroyNotify destroy);
TpCMParamSpec *tp_cm_param_spec_ref (TpCMParamSpec *self);
void tp_cm_param_spec_unref (TpCMParamSpec *self);

GType tp_cm_param_spec_get_type (void) G_GNUC_CONST;
#define TP_TYPE_CM_PARAM_SPEC (tp_cm_param_spec_get_type ())

typedef struct _TpBaseProtocol TpBaseProtocol;
typedef struct _TpBaseProtocolClass TpBaseProtocolClass;
typedef struct _TpBaseProtocolPrivate TpBaseProtocolPrivate;
typedef struct _TpBaseProtocolClassPrivate TpBaseProtocolClassPrivate;

GType tp_base_protocol_get_type (void) G_GNUC_CONST;

#define TP_TYPE_BASE_PROTOCOL \
  (tp_base_protocol_get_type ())
#define TP_BASE_PROTOCOL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_BASE_PROTOCOL, \
                               TpBaseProtocol))
#define TP_BASE_PROTOCOL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_BASE_PROTOCOL, \
                            TpBaseProtocolClass))
#define TP_IS_BASE_PROTOCOL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_BASE_PROTOCOL))
#define TP_IS_BASE_PROTOCOL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_BASE_PROTOCOL))
#define TP_BASE_PROTOCOL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_BASE_PROTOCOL, \
                              TpBaseProtocolClass))

struct _TpBaseProtocol
{
  /*<private>*/
  GDBusObjectSkeleton parent;
  TpBaseProtocolPrivate *priv;
};

typedef GPtrArray *(*TpBaseProtocolDupParametersFunc) (TpBaseProtocol *self);

typedef TpBaseConnection *(*TpBaseProtocolNewConnectionFunc) (
    TpBaseProtocol *self,
    GHashTable *asv,
    GError **error);

typedef gchar *(*TpBaseProtocolNormalizeContactFunc) (TpBaseProtocol *self,
    const gchar *contact,
    GError **error);

typedef gchar *(*TpBaseProtocolIdentifyAccountFunc) (TpBaseProtocol *self,
    GHashTable *asv,
    GError **error);

typedef void (*TpBaseProtocolGetConnectionDetailsFunc) (TpBaseProtocol *self,
    GStrv *connection_interfaces,
    GType **channel_manager_types,
    gchar **icon_name,
    gchar **english_name,
    gchar **vcard_field);

typedef gboolean (*TpBaseProtocolGetAvatarDetailsFunc) (TpBaseProtocol *self,
    GStrv *supported_mime_types,
    guint *min_height,
    guint *min_width,
    guint *rec_height,
    guint *rec_width,
    guint *max_height,
    guint *max_width,
    guint *max_bytes);

struct _TpBaseProtocolClass
{
  GDBusObjectSkeletonClass parent_class;

  gboolean is_stub;
  TpBaseProtocolDupParametersFunc dup_parameters;
  TpBaseProtocolNewConnectionFunc new_connection;

  TpBaseProtocolNormalizeContactFunc normalize_contact;
  TpBaseProtocolIdentifyAccountFunc identify_account;

  TpBaseProtocolGetConnectionDetailsFunc get_connection_details;

  const TpPresenceStatusSpec * (*get_statuses) (TpBaseProtocol *self);

  TpBaseProtocolGetAvatarDetailsFunc get_avatar_details;

  GStrv (*dup_authentication_types) (TpBaseProtocol *self);

  /*<private>*/
  GCallback padding[4];
  TpBaseProtocolClassPrivate *priv;
};

const gchar *tp_base_protocol_get_name (TpBaseProtocol *self);
GHashTable *tp_base_protocol_get_immutable_properties (TpBaseProtocol *self);

GPtrArray *tp_base_protocol_dup_parameters (TpBaseProtocol *self);
const TpPresenceStatusSpec *tp_base_protocol_get_statuses (TpBaseProtocol *self);

TpBaseConnection *tp_base_protocol_new_connection (TpBaseProtocol *self,
    GHashTable *asv, GError **error);


/* ---- Implemented by subclasses for Addressing support ---- */

#define TP_TYPE_PROTOCOL_ADDRESSING \
  (tp_protocol_addressing_get_type ())

#define TP_IS_PROTOCOL_ADDRESSING(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
      TP_TYPE_PROTOCOL_ADDRESSING))

#define TP_PROTOCOL_ADDRESSING_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
      TP_TYPE_PROTOCOL_ADDRESSING, TpProtocolAddressingInterface))

typedef struct _TpProtocolAddressingInterface TpProtocolAddressingInterface;
typedef struct _TpProtocolAddressing TpProtocolAddressing;

typedef GStrv (*TpBaseProtocolDupSupportedVCardFieldsFunc) (TpBaseProtocol *self);

typedef GStrv (*TpBaseProtocolDupSupportedURISchemesFunc) (TpBaseProtocol *self);

typedef gchar *(*TpBaseProtocolNormalizeVCardAddressFunc) (
    TpBaseProtocol *self,
    const gchar *vcard_field,
    const gchar *vcard_address,
    GError **error);

typedef gchar *(*TpBaseProtocolNormalizeURIFunc) (
    TpBaseProtocol *self,
    const gchar *uri,
    GError **error);

struct _TpProtocolAddressingInterface {
  GTypeInterface parent;

  TpBaseProtocolDupSupportedVCardFieldsFunc dup_supported_vcard_fields;

  TpBaseProtocolDupSupportedURISchemesFunc dup_supported_uri_schemes;

  TpBaseProtocolNormalizeVCardAddressFunc normalize_vcard_address;

  TpBaseProtocolNormalizeURIFunc normalize_contact_uri;
};

_TP_AVAILABLE_IN_0_18
GType tp_protocol_addressing_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
