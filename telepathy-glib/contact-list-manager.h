/* ContactList channel manager
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

#ifndef __TP_CONTACT_LIST_MANAGER_H__
#define __TP_CONTACT_LIST_MANAGER_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <telepathy-glib/handle-repo.h>

G_BEGIN_DECLS

typedef struct _TpContactListManager TpContactListManager;
typedef struct _TpContactListManagerClass TpContactListManagerClass;
typedef struct _TpContactListManagerPrivate TpContactListManagerPrivate;
typedef struct _TpContactListManagerClassPrivate TpContactListManagerClassPrivate;

struct _TpContactListManagerClass {
    /*<private>*/
    GObjectClass parent_class;
    GCallback _padding[7];
    TpContactListManagerClassPrivate *priv;
};

struct _TpContactListManager {
    /*<private>*/
    GObject parent;
    TpContactListManagerPrivate *priv;
};

GType tp_contact_list_manager_get_type (void);

#define TP_TYPE_CONTACT_LIST_MANAGER \
  (tp_contact_list_manager_get_type ())
#define TP_CONTACT_LIST_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CONTACT_LIST_MANAGER, \
                               TpContactListManager))
#define TP_CONTACT_LIST_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_CONTACT_LIST_MANAGER, \
                            TpContactListManagerClass))
#define TP_IS_CONTACT_LIST_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_CONTACT_LIST_MANAGER))
#define TP_IS_CONTACT_LIST_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_CONTACT_LIST_MANAGER))
#define TP_CONTACT_LIST_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_CONTACT_LIST_MANAGER, \
                              TpContactListManagerClass))

/* ---- Will be in telepathy-spec later (so, no GEnum) ---- */

typedef enum { /*< skip >*/
    TP_PRESENCE_STATE_NO,
    TP_PRESENCE_STATE_ASK,
    TP_PRESENCE_STATE_YES
} TpPresenceState;

/* ---- Called by subclasses ---- */

void tp_contact_list_manager_set_list_received (TpContactListManager *self);

void tp_contact_list_manager_contacts_changed (TpContactListManager *self,
    TpHandleSet *changed,
    TpHandleSet *removed);

typedef gboolean (*TpContactListManagerBooleanFunc) (
    TpContactListManager *self);

gboolean tp_contact_list_manager_true_func (TpContactListManager *self);
gboolean tp_contact_list_manager_false_func (TpContactListManager *self);

void tp_contact_list_manager_class_implement_can_change_subscriptions (
    TpContactListManagerClass *cls,
    TpContactListManagerBooleanFunc check);

void tp_contact_list_manager_class_implement_subscriptions_persist (
    TpContactListManagerClass *cls,
    TpContactListManagerBooleanFunc check);

void tp_contact_list_manager_class_implement_request_uses_message (
    TpContactListManagerClass *cls,
    TpContactListManagerBooleanFunc check);

typedef TpHandleSet *(*TpContactListManagerGetContactsFunc) (
    TpContactListManager *self);

void tp_contact_list_manager_class_implement_get_contacts (
    TpContactListManagerClass *cls,
    TpContactListManagerGetContactsFunc impl);

typedef void (*TpContactListManagerGetPresenceStatesFunc) (
    TpContactListManager *self,
    TpHandle contact,
    TpPresenceState *subscribe,
    TpPresenceState *publish,
    gchar **publish_request);

void tp_contact_list_manager_class_implement_get_states (
    TpContactListManagerClass *cls,
    TpContactListManagerGetPresenceStatesFunc impl);

typedef gboolean (*TpContactListManagerRequestSubscriptionFunc) (
    TpContactListManager *self,
    TpHandleSet *contacts,
    const gchar *message,
    GError **error);

void tp_contact_list_manager_class_implement_request_subscription (
    TpContactListManagerClass *cls,
    TpContactListManagerRequestSubscriptionFunc impl);

typedef gboolean (*TpContactListManagerActOnContactsFunc) (
    TpContactListManager *self,
    TpHandleSet *contacts,
    GError **error);

void tp_contact_list_manager_class_implement_authorize_publication (
    TpContactListManagerClass *cls,
    TpContactListManagerActOnContactsFunc impl);

void tp_contact_list_manager_class_implement_just_store_contacts (
    TpContactListManagerClass *cls,
    TpContactListManagerActOnContactsFunc impl);

void tp_contact_list_manager_class_implement_remove_contacts (
    TpContactListManagerClass *cls,
    TpContactListManagerActOnContactsFunc impl);

void tp_contact_list_manager_class_implement_unsubscribe (
    TpContactListManagerClass *cls,
    TpContactListManagerActOnContactsFunc impl);

void tp_contact_list_manager_class_implement_unpublish (
    TpContactListManagerClass *cls,
    TpContactListManagerActOnContactsFunc impl);

G_END_DECLS

#endif
