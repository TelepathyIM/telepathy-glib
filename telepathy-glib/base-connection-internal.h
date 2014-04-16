/*<private_header>*/
/* Base class for Connection implementations
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

#ifndef __TP_BASE_CONNECTION_INTERNAL_H__
#define __TP_BASE_CONNECTION_INTERNAL_H__

#include <telepathy-glib/_gdbus/Connection.h>
#include <telepathy-glib/_gdbus/Connection_Interface_Presence1.h>
#include <telepathy-glib/_gdbus/Connection_Interface_Requests.h>

#include <telepathy-glib/base-connection.h>

G_BEGIN_DECLS

struct _TpBaseConnectionPrivate
{
  gchar *bus_name;
  gchar *object_path;

  TpConnectionStatus status;

  TpHandle self_handle;
  const gchar *self_id;

  /* Telepathy properties */
  gchar *protocol;

  /* if TRUE, the object has gone away */
  gboolean dispose_has_run;
  /* array of (TpChannelManager *) */
  GPtrArray *channel_managers;
  /* array of reffed (TpChannelManagerRequest *) */
  GPtrArray *channel_requests;

  TpHandleRepoIface *handles[TP_NUM_ENTITY_TYPES];

  /* Created in constructed, this is an array of static strings which
   * represent the interfaces on this connection.
   *
   * Note that this is a GArray of gchar*, not a GPtrArray,
   * so that we can use GArray's convenient auto-null-termination. */
  GArray *interfaces;

  /* Array of GDBusMethodInvocation * representing Disconnect calls.
   * If NULL and we are in a state != DISCONNECTED, then we have not started
   * shutting down yet.
   * If NULL and we are in state DISCONNECTED, then we have finished shutting
   * down.
   * If not NULL, we are trying to shut down (and must be in state
   * DISCONNECTED). */
  GPtrArray *disconnect_requests;

  GDBusConnection *dbus_connection;
  /* TRUE after constructor() returns */
  gboolean been_constructed;
  /* TRUE if on D-Bus */
  gboolean been_registered;

  /* g_strdup (unique name) => owned ClientData struct */
  GHashTable *clients;
  /* GQuark iface => number of clients interested */
  GHashTable *interests;

  gchar *account_path_suffix;

  _TpGDBusConnection *connection_skeleton;
  _TpGDBusConnectionInterfaceRequests *requests_skeleton;
  _TpGDBusConnectionInterfacePresence1 *presence_skeleton;
};

void _tp_base_connection_set_handle_repo (TpBaseConnection *self,
    TpEntityType entity_type,
    TpHandleRepoIface *handle_repo);

gpointer _tp_base_connection_find_channel_manager (TpBaseConnection *self,
    GType type);

GVariant *_tp_base_connection_dup_contact_attributes (
    TpBaseConnection *self,
    const GArray *handles,
    const gchar * const *interfaces,
    const gchar * const *assumed_interfaces);

/* TpPresenceMixin */
void _tp_presence_mixin_init (TpBaseConnection *self);
gboolean _tp_presence_mixin_fill_contact_attributes (TpBaseConnection *self,
    const gchar *dbus_interface,
    TpHandle contact,
    GVariantDict *attributes);

G_END_DECLS

#endif
