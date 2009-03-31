/*
 * dbus.h - Header for D-Bus utilities
 *
 * Copyright (C) 2005-2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2005-2007 Nokia Corporation
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

#ifndef __TELEPATHY_DBUS_H__
#define __TELEPATHY_DBUS_H__

#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

void tp_dbus_g_method_return_not_implemented (DBusGMethodInvocation *context);
DBusGConnection * tp_get_bus (void);

/* TpDBusDaemon is typedef'd in proxy.h */
typedef struct _TpDBusDaemonPrivate TpDBusDaemonPrivate;
typedef struct _TpDBusDaemonClass TpDBusDaemonClass;
GType tp_dbus_daemon_get_type (void);

#define TP_TYPE_DBUS_DAEMON \
  (tp_dbus_daemon_get_type ())
#define TP_DBUS_DAEMON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_DBUS_DAEMON, \
                              TpDBusDaemon))
#define TP_DBUS_DAEMON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_DBUS_DAEMON, \
                           TpDBusDaemonClass))
#define TP_IS_DBUS_DAEMON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_DBUS_DAEMON))
#define TP_IS_DBUS_DAEMON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_DBUS_DAEMON))
#define TP_DBUS_DAEMON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_DBUS_DAEMON, \
                              TpDBusDaemonClass))

TpDBusDaemon *tp_dbus_daemon_dup (GError **error);

TpDBusDaemon *tp_dbus_daemon_new (DBusGConnection *connection);

typedef void (*TpDBusDaemonNameOwnerChangedCb) (TpDBusDaemon *bus_daemon,
    const gchar *name, const gchar *new_owner, gpointer user_data);

void tp_dbus_daemon_watch_name_owner (TpDBusDaemon *self,
    const gchar *name, TpDBusDaemonNameOwnerChangedCb callback,
    gpointer user_data, GDestroyNotify destroy);

gboolean tp_dbus_daemon_cancel_name_owner_watch (TpDBusDaemon *self,
    const gchar *name, TpDBusDaemonNameOwnerChangedCb callback,
    gconstpointer user_data);

typedef enum
{
  TP_DBUS_NAME_TYPE_UNIQUE = 1,
  TP_DBUS_NAME_TYPE_WELL_KNOWN = 2,
  TP_DBUS_NAME_TYPE_BUS_DAEMON = 4,
  TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON =
    TP_DBUS_NAME_TYPE_UNIQUE | TP_DBUS_NAME_TYPE_WELL_KNOWN,
  TP_DBUS_NAME_TYPE_ANY =
    TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON | TP_DBUS_NAME_TYPE_BUS_DAEMON
} TpDBusNameType;

gboolean tp_dbus_check_valid_bus_name (const gchar *name,
    TpDBusNameType allow_types, GError **error);

gboolean tp_dbus_check_valid_interface_name (const gchar *name,
    GError **error);

gboolean tp_dbus_check_valid_member_name (const gchar *name,
    GError **error);

gboolean tp_dbus_check_valid_object_path (const gchar *path,
    GError **error);

GValue *tp_g_value_slice_new_bytes (guint length, gconstpointer bytes);
GValue *tp_g_value_slice_new_take_bytes (GArray *bytes);
GValue *tp_g_value_slice_new_object_path (const gchar *path);
GValue *tp_g_value_slice_new_static_object_path (const gchar *path);
GValue *tp_g_value_slice_new_take_object_path (gchar *path);

#define tp_asv_size(asv) _tp_asv_size_inline (asv)

static inline guint
_tp_asv_size_inline (const GHashTable *asv)
{
  /* The empty comment here is to stop gtkdoc thinking g_hash_table_size is
   * a declaration. */
  return g_hash_table_size /* */ ((GHashTable *) asv);
}

GHashTable *tp_asv_new (const char *first_key, ...);
gboolean tp_asv_get_boolean (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_boolean (GHashTable *asv, const gchar *key, gboolean value);
gpointer tp_asv_get_boxed (const GHashTable *asv, const gchar *key,
    GType type);
void tp_asv_set_boxed (GHashTable *asv, const gchar *key, GType type,
    gpointer value);
const GArray *tp_asv_get_bytes (const GHashTable *asv, const gchar *key);
void tp_asv_set_bytes (GHashTable *asv, const gchar *key, GArray *value);
gdouble tp_asv_get_double (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_double (GHashTable *asv, const gchar *key, gdouble value);
gint32 tp_asv_get_int32 (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_int32 (GHashTable *asv, const gchar *key, gint32 value);
gint64 tp_asv_get_int64 (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_int64 (GHashTable *asv, const gchar *key, gint64 value);
const gchar *tp_asv_get_object_path (const GHashTable *asv, const gchar *key);
void tp_asv_set_object_path (GHashTable *asv, const gchar *key, gchar *value);
const gchar *tp_asv_get_string (const GHashTable *asv, const gchar *key);
void tp_asv_set_string (GHashTable *asv, const gchar *key, gchar *value);
void tp_asv_set_static_string (GHashTable *asv, const gchar *key, gchar *value);
guint32 tp_asv_get_uint32 (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_uint32 (GHashTable *asv, const gchar *key, guint32 value);
guint64 tp_asv_get_uint64 (const GHashTable *asv, const gchar *key,
    gboolean *valid);
void tp_asv_set_uint64 (GHashTable *asv, const gchar *key, guint64 value);
const GValue *tp_asv_lookup (const GHashTable *asv, const gchar *key);

const gchar * const *
/* this comment stops gtkdoc denying that this function exists */
tp_asv_get_strv (const GHashTable *asv, const gchar *key);
void tp_asv_set_strv (GHashTable *asv, const gchar *key, char **value);
void tp_asv_dump (GHashTable *asv);

G_END_DECLS

#include <telepathy-glib/_gen/tp-cli-dbus-daemon.h>

#ifndef TP_DISABLE_DEPRECATED
DBusGProxy * tp_get_bus_proxy (void) G_GNUC_DEPRECATED;
#endif

#endif /* __TELEPATHY_DBUS_H__ */
