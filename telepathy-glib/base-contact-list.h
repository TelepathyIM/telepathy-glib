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

#ifndef __TP_BASE_CONTACT_LIST_H__
#define __TP_BASE_CONTACT_LIST_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/svc-connection.h>

G_BEGIN_DECLS

typedef struct _TpBaseContactList TpBaseContactList;
typedef struct _TpBaseContactListClass TpBaseContactListClass;
typedef struct _TpBaseContactListPrivate TpBaseContactListPrivate;
typedef struct _TpBaseContactListClassPrivate TpBaseContactListClassPrivate;

struct _TpBaseContactList {
    /*<private>*/
    GObject parent;
    TpBaseContactListPrivate *priv;
};

GType tp_base_contact_list_get_type (void);

#define TP_TYPE_BASE_CONTACT_LIST \
  (tp_base_contact_list_get_type ())
#define TP_BASE_CONTACT_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_BASE_CONTACT_LIST, \
                               TpBaseContactList))
#define TP_BASE_CONTACT_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TP_TYPE_BASE_CONTACT_LIST, \
                            TpBaseContactListClass))
#define TP_IS_BASE_CONTACT_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TP_TYPE_BASE_CONTACT_LIST))
#define TP_IS_BASE_CONTACT_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TP_TYPE_BASE_CONTACT_LIST))
#define TP_BASE_CONTACT_LIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_BASE_CONTACT_LIST, \
                              TpBaseContactListClass))

/* ---- Called by subclasses for ContactList (or both) ---- */

void tp_base_contact_list_set_list_received (TpBaseContactList *self);

void tp_base_contact_list_contacts_changed (TpBaseContactList *self,
    TpHandleSet *changed,
    TpHandleSet *removed);

/* ---- Implemented by subclasses for ContactList (mandatory read-only
 * things) ---- */

typedef gboolean (*TpBaseContactListBooleanFunc) (
    TpBaseContactList *self);

gboolean tp_base_contact_list_true_func (TpBaseContactList *self);
gboolean tp_base_contact_list_false_func (TpBaseContactList *self);

gboolean tp_base_contact_list_get_subscriptions_persist (
    TpBaseContactList *self);

typedef TpHandleSet *(*TpBaseContactListGetContactsFunc) (
    TpBaseContactList *self);

TpHandleSet *tp_base_contact_list_get_contacts (TpBaseContactList *self);

typedef void (*TpBaseContactListGetStatesFunc) (
    TpBaseContactList *self,
    TpHandle contact,
    TpPresenceState *subscribe,
    TpPresenceState *publish,
    gchar **publish_request);

void tp_base_contact_list_get_states (TpBaseContactList *self,
    TpHandle contact,
    TpPresenceState *subscribe,
    TpPresenceState *publish,
    gchar **publish_request);

struct _TpBaseContactListClass {
    GObjectClass parent_class;

    TpBaseContactListGetContactsFunc get_contacts;
    TpBaseContactListGetStatesFunc get_states;
    TpBaseContactListBooleanFunc get_subscriptions_persist;

    /*<private>*/
    GCallback _padding[7];
    TpBaseContactListClassPrivate *priv;
};

/* ---- Implemented by subclasses for ContactList modification ---- */

#define TP_TYPE_MUTABLE_CONTACT_LIST \
  (tp_mutable_contact_list_get_type ())

#define TP_IS_MUTABLE_CONTACT_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_TYPE_MUTABLE_CONTACT_LIST))

#define TP_MUTABLE_CONTACT_LIST_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  TP_TYPE_MUTABLE_CONTACT_LIST, TpMutableContactListInterface))

typedef struct _TpMutableContactListInterface TpMutableContactListInterface;

typedef void (*TpBaseContactListRequestSubscriptionFunc) (
    TpBaseContactList *self,
    TpHandleSet *contacts,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);

typedef void (*TpBaseContactListActOnContactsFunc) (
    TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

typedef gboolean (*TpBaseContactListAsyncFinishFunc) (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

struct _TpMutableContactListInterface {
    GTypeInterface parent;

    /* _async mandatory-to-implement, _finish has a default implementation
     * suitable for a GSimpleAsyncResult */

    TpBaseContactListRequestSubscriptionFunc request_subscription_async;
    TpBaseContactListAsyncFinishFunc request_subscription_finish;

    TpBaseContactListActOnContactsFunc authorize_publication_async;
    TpBaseContactListAsyncFinishFunc authorize_publication_finish;

    TpBaseContactListActOnContactsFunc remove_contacts_async;
    TpBaseContactListAsyncFinishFunc remove_contacts_finish;

    TpBaseContactListActOnContactsFunc unsubscribe_async;
    TpBaseContactListAsyncFinishFunc unsubscribe_finish;

    TpBaseContactListActOnContactsFunc unpublish_async;
    TpBaseContactListAsyncFinishFunc unpublish_finish;

    /* optional-to-implement */

    TpBaseContactListActOnContactsFunc store_contacts_async;
    TpBaseContactListAsyncFinishFunc store_contacts_finish;

    TpBaseContactListBooleanFunc can_change_subscriptions;
    TpBaseContactListBooleanFunc get_request_uses_message;
};

GType tp_mutable_contact_list_get_type (void) G_GNUC_CONST;

gboolean tp_base_contact_list_can_change_subscriptions (
    TpBaseContactList *self);

gboolean tp_base_contact_list_get_request_uses_message (
    TpBaseContactList *self);

void tp_base_contact_list_request_subscription_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_request_subscription_finish (
    TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_authorize_publication_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_authorize_publication_finish (
    TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_store_contacts_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_store_contacts_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_remove_contacts_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_remove_contacts_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_unsubscribe_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_unsubscribe_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_unpublish_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_unpublish_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

/* ---- contact blocking ---- */

#define TP_TYPE_BLOCKABLE_CONTACT_LIST \
  (tp_blockable_contact_list_get_type ())
GType tp_blockable_contact_list_get_type (void) G_GNUC_CONST;

#define TP_IS_BLOCKABLE_CONTACT_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_TYPE_BLOCKABLE_CONTACT_LIST))

#define TP_BLOCKABLE_CONTACT_LIST_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  TP_TYPE_BLOCKABLE_CONTACT_LIST, TpBlockableContactListInterface))

typedef struct _TpBlockableContactListInterface
    TpBlockableContactListInterface;

void tp_base_contact_list_contact_blocking_changed (
    TpBaseContactList *self,
    TpHandleSet *changed);

gboolean tp_base_contact_list_can_block (TpBaseContactList *self);

TpHandleSet *tp_base_contact_list_get_blocked_contacts (
    TpBaseContactList *self);

void tp_base_contact_list_block_contacts_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_block_contacts_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_unblock_contacts_async (TpBaseContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_unblock_contacts_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

struct _TpBlockableContactListInterface {
    GTypeInterface parent;

    /* mandatory to implement */

    TpBaseContactListGetContactsFunc get_blocked_contacts;

    /* _async mandatory-to-implement, _finish has a default implementation
     * suitable for a GSimpleAsyncResult */

    TpBaseContactListActOnContactsFunc block_contacts_async;
    TpBaseContactListAsyncFinishFunc block_contacts_finish;
    TpBaseContactListActOnContactsFunc unblock_contacts_async;
    TpBaseContactListAsyncFinishFunc unblock_contacts_finish;

    /* optional to implement */

    TpBaseContactListBooleanFunc can_block;
};

/* ---- Called by subclasses for ContactGroups ---- */

void tp_base_contact_list_groups_created (TpBaseContactList *self,
    const gchar * const *created, gssize n_created);

void tp_base_contact_list_groups_removed (TpBaseContactList *self,
    const gchar * const *removed, gssize n_removed);

void tp_base_contact_list_group_renamed (TpBaseContactList *self,
    const gchar *old_name,
    const gchar *new_name);

void tp_base_contact_list_groups_changed (TpBaseContactList *self,
    TpHandleSet *contacts,
    const gchar * const *added, gssize n_added,
    const gchar * const *removed, gssize n_removed);

/* ---- Implemented by subclasses for ContactGroups ---- */

gboolean tp_base_contact_list_has_disjoint_groups (TpBaseContactList *self);

typedef GStrv (*TpBaseContactListGetGroupsFunc) (
    TpBaseContactList *self);

GStrv tp_base_contact_list_get_groups (TpBaseContactList *self);

typedef GStrv (*TpBaseContactListGetContactGroupsFunc) (
    TpBaseContactList *self,
    TpHandle contact);

GStrv tp_base_contact_list_get_contact_groups (TpBaseContactList *self,
    TpHandle contact);

typedef gchar *(*TpBaseContactListNormalizeFunc) (
    TpBaseContactList *self,
    const gchar *s);

gchar *tp_base_contact_list_normalize_group (
    TpBaseContactList *self,
    const gchar *s);

#define TP_TYPE_CONTACT_GROUP_LIST \
  (tp_contact_group_list_get_type ())
GType tp_contact_group_list_get_type (void) G_GNUC_CONST;

#define TP_IS_CONTACT_GROUP_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_TYPE_CONTACT_GROUP_LIST))

#define TP_CONTACT_GROUP_LIST_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  TP_TYPE_CONTACT_GROUP_LIST, TpContactGroupListInterface))

typedef struct _TpContactGroupListInterface
    TpContactGroupListInterface;

struct _TpContactGroupListInterface {
    GTypeInterface parent;
    /* mandatory to implement */
    TpBaseContactListGetGroupsFunc get_groups;
    TpBaseContactListGetContactGroupsFunc get_contact_groups;
    /* optional to implement */
    TpBaseContactListBooleanFunc has_disjoint_groups;
    TpBaseContactListNormalizeFunc normalize_group;
};

/* ---- Implemented by subclasses for mutable ContactGroups ---- */

typedef guint (*TpBaseContactListUIntFunc) (
    TpBaseContactList *self);

TpContactMetadataStorageType tp_base_contact_list_get_group_storage (
    TpBaseContactList *self);

typedef void (*TpBaseContactListSetContactGroupsFunc) (TpBaseContactList *self,
    TpHandle contact,
    const gchar * const *normalized_names,
    gsize n_names,
    GAsyncReadyCallback callback,
    gpointer user_data);

void tp_base_contact_list_set_contact_groups_async (TpBaseContactList *self,
    TpHandle contact,
    const gchar * const *normalized_names,
    gsize n_names,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_set_contact_groups_finish (
    TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

typedef void (*TpBaseContactListCreateGroupsFunc) (
    TpBaseContactList *self,
    const gchar * const *normalized_names,
    gsize n_names);

void tp_base_contact_list_create_groups (TpBaseContactList *self,
    const gchar * const *normalized_names,
    gsize n_names);

typedef void (*TpBaseContactListGroupContactsFunc) (TpBaseContactList *self,
    const gchar *group,
    TpHandleSet *contacts);

void tp_base_contact_list_add_to_group (TpBaseContactList *self,
    const gchar *group,
    TpHandleSet *contacts);

void tp_base_contact_list_remove_from_group (TpBaseContactList *self,
    const gchar *group,
    TpHandleSet *contacts);

typedef void (*TpBaseContactListRemoveGroupFunc) (TpBaseContactList *self,
    const gchar *group);

void tp_base_contact_list_remove_group (TpBaseContactList *self,
    const gchar *group);

typedef void (*TpBaseContactListRenameGroupFunc) (TpBaseContactList *self,
    const gchar *old_name,
    const gchar *new_name);

void tp_base_contact_list_rename_group (TpBaseContactList *self,
    const gchar *old_name,
    const gchar *new_name);

#define TP_TYPE_MUTABLE_CONTACT_GROUP_LIST \
  (tp_mutable_contact_group_list_get_type ())
GType tp_mutable_contact_group_list_get_type (void) G_GNUC_CONST;

#define TP_IS_MUTABLE_CONTACT_GROUP_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_TYPE_MUTABLE_CONTACT_GROUP_LIST))

#define TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, TpMutableContactGroupListInterface))

typedef struct _TpMutableContactGroupListInterface
    TpMutableContactGroupListInterface;

struct _TpMutableContactGroupListInterface {
    GTypeInterface parent;

    /* _async mandatory-to-implement, _finish has a default implementation
     * suitable for a GSimpleAsyncResult */

    TpBaseContactListSetContactGroupsFunc set_contact_groups_async;
    TpBaseContactListAsyncFinishFunc set_contact_groups_finish;

    TpBaseContactListCreateGroupsFunc create_groups;
    TpBaseContactListGroupContactsFunc add_to_group;
    TpBaseContactListGroupContactsFunc remove_from_group;
    TpBaseContactListRemoveGroupFunc remove_group;

    /* optional to implement */

    TpBaseContactListRenameGroupFunc rename_group;
    TpBaseContactListUIntFunc get_group_storage;
};

/* ---- Mixin-like functionality for our parent TpBaseConnection ---- */

void tp_base_contact_list_mixin_class_init (TpBaseConnectionClass *cls);
void tp_base_contact_list_mixin_register_with_contacts_mixin (
    TpBaseConnection *conn);
void tp_base_contact_list_mixin_list_iface_init (
    TpSvcConnectionInterfaceContactListClass *klass);

void tp_base_contact_list_mixin_groups_iface_init (
    TpSvcConnectionInterfaceContactGroupsClass *klass);

G_END_DECLS

#endif
