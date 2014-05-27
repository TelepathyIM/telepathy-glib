/* ContactList base class
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

#if !defined (_TP_GLIB_H_INSIDE) && !defined (_TP_COMPILATION)
#error "Only <telepathy-glib/telepathy-glib.h> can be included directly."
#endif

#ifndef __TP_BASE_CONTACT_LIST_H__
#define __TP_BASE_CONTACT_LIST_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/handle-repo.h>

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

/* ---- Utility stuff which subclasses can use ---- */

TpContactListState tp_base_contact_list_get_state (TpBaseContactList *self,
    GError **error);
TpBaseConnection *tp_base_contact_list_get_connection (
    TpBaseContactList *self, GError **error);
_TP_AVAILABLE_IN_0_18
gboolean tp_base_contact_list_get_download_at_connection (
    TpBaseContactList *self);

/* ---- Called by subclasses for ContactList (or both) ---- */

void tp_base_contact_list_set_list_pending (TpBaseContactList *self);
void tp_base_contact_list_set_list_failed (TpBaseContactList *self,
    GQuark domain,
    gint code,
    const gchar *message);
void tp_base_contact_list_set_list_received (TpBaseContactList *self);

void tp_base_contact_list_contacts_changed (TpBaseContactList *self,
    TpHandleSet *changed,
    TpHandleSet *removed);
void tp_base_contact_list_one_contact_changed (TpBaseContactList *self,
    TpHandle changed);
void tp_base_contact_list_one_contact_removed (TpBaseContactList *self,
    TpHandle removed);

/* ---- Implemented by subclasses for ContactList (mandatory read-only
 * things) ---- */

typedef gboolean (*TpBaseContactListBooleanFunc) (
    TpBaseContactList *self);

gboolean tp_base_contact_list_true_func (TpBaseContactList *self);
gboolean tp_base_contact_list_false_func (TpBaseContactList *self);

gboolean tp_base_contact_list_get_contact_list_persists (
    TpBaseContactList *self);

typedef TpHandleSet *(*TpBaseContactListDupContactsFunc) (
    TpBaseContactList *self);

TpHandleSet *tp_base_contact_list_dup_contacts (TpBaseContactList *self);

typedef void (*TpBaseContactListDupStatesFunc) (
    TpBaseContactList *self,
    TpHandle contact,
    TpSubscriptionState *subscribe,
    TpSubscriptionState *publish,
    gchar **publish_request);

void tp_base_contact_list_dup_states (TpBaseContactList *self,
    TpHandle contact,
    TpSubscriptionState *subscribe,
    TpSubscriptionState *publish,
    gchar **publish_request);

typedef void (*TpBaseContactListAsyncFunc) (
    TpBaseContactList *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

_TP_AVAILABLE_IN_0_18
void tp_base_contact_list_download_async (TpBaseContactList *self,
    GAsyncReadyCallback callback,
    gpointer user_data);

_TP_AVAILABLE_IN_0_18
gboolean tp_base_contact_list_download_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

typedef gboolean (*TpBaseContactListAsyncFinishFunc) (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

struct _TpBaseContactListClass {
    GObjectClass parent_class;

    TpBaseContactListDupContactsFunc dup_contacts;
    TpBaseContactListDupStatesFunc dup_states;
    TpBaseContactListBooleanFunc get_contact_list_persists;

    TpBaseContactListAsyncFunc download_async;
    TpBaseContactListAsyncFinishFunc download_finish;

    /*<private>*/
    GCallback _padding[5];
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

#define TP_MUTABLE_CONTACT_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_MUTABLE_CONTACT_LIST, \
  TpMutableContactList))

typedef struct _TpMutableContactListInterface TpMutableContactListInterface;
typedef struct _TpMutableContactList TpMutableContactList;

typedef void (*TpMutableContactListRequestSubscriptionFunc) (
    TpMutableContactList *self,
    TpHandleSet *contacts,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);

typedef void (*TpMutableContactListActOnContactsFunc) (
    TpMutableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

typedef gboolean (*TpMutableContactListAsyncFinishFunc) (
    TpMutableContactList *self,
    GAsyncResult *result,
    GError **error);

typedef gboolean (*TpMutableContactListBooleanFunc) (
    TpMutableContactList *self);

gboolean tp_mutable_contact_list_true_func (TpMutableContactList *self);
gboolean tp_mutable_contact_list_false_func (TpMutableContactList *self);

struct _TpMutableContactListInterface {
    GTypeInterface parent;

    /* _async mandatory-to-implement, _finish has a default implementation
     * suitable for a GSimpleAsyncResult */

    TpMutableContactListRequestSubscriptionFunc request_subscription_async;
    TpMutableContactListAsyncFinishFunc request_subscription_finish;

    TpMutableContactListActOnContactsFunc authorize_publication_async;
    TpMutableContactListAsyncFinishFunc authorize_publication_finish;

    TpMutableContactListActOnContactsFunc remove_contacts_async;
    TpMutableContactListAsyncFinishFunc remove_contacts_finish;

    TpMutableContactListActOnContactsFunc unsubscribe_async;
    TpMutableContactListAsyncFinishFunc unsubscribe_finish;

    TpMutableContactListActOnContactsFunc unpublish_async;
    TpMutableContactListAsyncFinishFunc unpublish_finish;

    /* optional-to-implement */

    TpMutableContactListActOnContactsFunc store_contacts_async;
    TpMutableContactListAsyncFinishFunc store_contacts_finish;

    TpMutableContactListBooleanFunc can_change_contact_list;
    TpMutableContactListBooleanFunc get_request_uses_message;
};

GType tp_mutable_contact_list_get_type (void) G_GNUC_CONST;

gboolean tp_base_contact_list_can_change_contact_list (
    TpBaseContactList *self);

gboolean tp_base_contact_list_get_request_uses_message (
    TpBaseContactList *self);

void tp_base_contact_list_request_subscription_async (
    TpMutableContactList *self,
    TpHandleSet *contacts,
    const gchar *message,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_request_subscription_finish (
    TpMutableContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_authorize_publication_async (
    TpMutableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_authorize_publication_finish (
    TpMutableContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_store_contacts_async (TpMutableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_store_contacts_finish (TpMutableContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_remove_contacts_async (TpMutableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_remove_contacts_finish (
    TpMutableContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_unsubscribe_async (TpMutableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_unsubscribe_finish (TpMutableContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_unpublish_async (TpMutableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_unpublish_finish (TpMutableContactList *self,
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

#define TP_BLOCKABLE_CONTACT_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_BLOCKABLE_CONTACT_LIST, \
  TpBlockableContactList))

typedef struct _TpBlockableContactListInterface
    TpBlockableContactListInterface;

typedef struct _TpBlockableContactList TpBlockableContactList;

void tp_blockable_contact_list_contact_blocking_changed (
    TpBlockableContactList *self,
    TpHandleSet *changed);

gboolean tp_base_contact_list_can_block (TpBaseContactList *self);

_TP_AVAILABLE_IN_1_0
gboolean tp_blockable_contact_list_is_blocked (TpBlockableContactList *self,
    TpHandle contact);
TpHandleSet *tp_blockable_contact_list_dup_blocked_contacts (
    TpBlockableContactList *self);

void tp_blockable_contact_list_block_contacts_async (TpBlockableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_blockable_contact_list_block_contacts_finish (
    TpBlockableContactList *self,
    GAsyncResult *result,
    GError **error);

_TP_AVAILABLE_IN_0_16
void tp_blockable_contact_list_block_contacts_with_abuse_async (
    TpBlockableContactList *self,
    TpHandleSet *contacts,
    gboolean report_abusive,
    GAsyncReadyCallback callback,
    gpointer user_data);
_TP_AVAILABLE_IN_0_16
gboolean tp_blockable_contact_list_block_contacts_with_abuse_finish (
    TpBlockableContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_blockable_contact_list_unblock_contacts_async (TpBlockableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_blockable_contact_list_unblock_contacts_finish (
    TpBlockableContactList *self,
    GAsyncResult *result,
    GError **error);

typedef void (*TpBlockableContactListBlockContactsWithAbuseFunc) (
    TpBlockableContactList *self,
    TpHandleSet *contacts,
    gboolean report_abusive,
    GAsyncReadyCallback callback,
    gpointer user_data);

typedef TpHandleSet *(*TpBlockableContactListDupContactsFunc) (
    TpBlockableContactList *self);

typedef void (*TpBlockableContactListActOnContactsFunc) (
    TpBlockableContactList *self,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

typedef gboolean (*TpBlockableContactListAsyncFinishFunc) (
    TpBlockableContactList *self,
    GAsyncResult *result,
    GError **error);

typedef gboolean (*TpBlockableContactListBooleanFunc) (
    TpBlockableContactList *self);

gboolean tp_blockable_contact_list_true_func (TpBlockableContactList *self);
gboolean tp_blockable_contact_list_false_func (TpBlockableContactList *self);

struct _TpBlockableContactListInterface {
    GTypeInterface parent;

    /* mandatory to implement */

    gboolean (*is_blocked) (TpBlockableContactList *self,
        TpHandle contact);
    TpBlockableContactListDupContactsFunc dup_blocked_contacts;

    /* unblock_contacts_async is mandatory to implement; either
     * block_contacts_async or block_contacts_with_abuse_async (but not both!)
     * must also be implemented. _finish have default implementations
     * suitable for a GSimpleAsyncResult */

    TpBlockableContactListActOnContactsFunc block_contacts_async;
    TpBlockableContactListAsyncFinishFunc block_contacts_finish;
    TpBlockableContactListActOnContactsFunc unblock_contacts_async;
    TpBlockableContactListAsyncFinishFunc unblock_contacts_finish;

    /* optional to implement */
    TpBlockableContactListBooleanFunc can_block;

    /* see above. block_contacts_finish is the corresponding _finish function.
     */
    TpBlockableContactListBlockContactsWithAbuseFunc block_contacts_with_abuse_async;
};

/* ---- Called by subclasses for ContactGroups ---- */

typedef struct _TpContactGroupList TpContactGroupList;

void tp_contact_group_list_groups_created (TpContactGroupList *self,
    const gchar * const *created, gssize n_created);

void tp_contact_group_list_groups_removed (TpContactGroupList *self,
    const gchar * const *removed, gssize n_removed);

void tp_contact_group_list_group_renamed (TpContactGroupList *self,
    const gchar *old_name,
    const gchar *new_name);

void tp_contact_group_list_groups_changed (TpContactGroupList *self,
    TpHandleSet *contacts,
    const gchar * const *added, gssize n_added,
    const gchar * const *removed, gssize n_removed);

void tp_contact_group_list_one_contact_groups_changed (TpContactGroupList *self,
    TpHandle contact,
    const gchar * const *added, gssize n_added,
    const gchar * const *removed, gssize n_removed);

/* ---- Implemented by subclasses for ContactGroups ---- */

typedef gboolean (*TpContactGroupListBooleanFunc) (
    TpContactGroupList *self);

gboolean tp_contact_group_list_false_func (
    TpContactGroupList *self G_GNUC_UNUSED);

gboolean tp_contact_group_list_has_disjoint_groups (TpContactGroupList *self);

typedef GStrv (*TpContactGroupListDupGroupsFunc) (
    TpContactGroupList *self);

GStrv tp_contact_group_list_dup_groups (TpContactGroupList *self);

typedef GStrv (*TpContactGroupListDupContactGroupsFunc) (
    TpContactGroupList *self,
    TpHandle contact);

GStrv tp_contact_group_list_dup_contact_groups (TpContactGroupList *self,
    TpHandle contact);

typedef TpHandleSet *(*TpContactGroupListDupGroupMembersFunc) (
    TpContactGroupList *self,
    const gchar *group);

TpHandleSet *tp_contact_group_list_dup_group_members (TpContactGroupList *self,
    const gchar *group);

typedef gchar *(*TpContactGroupListNormalizeFunc) (
    TpContactGroupList *self,
    const gchar *s);

gchar *tp_contact_group_list_normalize_group (
    TpContactGroupList *self,
    const gchar *s);

#define TP_TYPE_CONTACT_GROUP_LIST \
  (tp_contact_group_list_get_type ())
GType tp_contact_group_list_get_type (void) G_GNUC_CONST;

#define TP_CONTACT_GROUP_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_CONTACT_GROUP_LIST, \
  TpContactGroupList))

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
    TpContactGroupListDupGroupsFunc dup_groups;
    TpContactGroupListDupGroupMembersFunc dup_group_members;
    TpContactGroupListDupContactGroupsFunc dup_contact_groups;
    /* optional to implement */
    TpContactGroupListBooleanFunc has_disjoint_groups;
    TpContactGroupListNormalizeFunc normalize_group;
};

/* ---- Implemented by subclasses for mutable ContactGroups ---- */

typedef struct _TpMutableContactGroupList TpMutableContactGroupList;

typedef gboolean (*TpMutableContactGroupListAsyncFinishFunc) (
    TpMutableContactGroupList *self,
    GAsyncResult *result,
    GError **error);

typedef guint (*TpBaseContactListUIntFunc) (
    TpBaseContactList *self);

TpContactMetadataStorageType tp_base_contact_list_get_group_storage (
    TpBaseContactList *self);

typedef void (*TpMutableContactGroupListSetContactGroupsFunc) (
    TpMutableContactGroupList *self,
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

typedef void (*TpMutableContactGroupListGroupContactsFunc) (
    TpMutableContactGroupList *self,
    const gchar *group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

void tp_base_contact_list_add_to_group_async (TpBaseContactList *self,
    const gchar *group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_add_to_group_finish (
    TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_remove_from_group_async (TpBaseContactList *self,
    const gchar *group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_remove_from_group_finish (
    TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

void tp_base_contact_list_set_group_members_async (TpBaseContactList *self,
    const gchar *normalized_group,
    TpHandleSet *contacts,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_set_group_members_finish (
    TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

typedef void (*TpMutableContactGroupListRemoveGroupFunc) (
    TpMutableContactGroupList *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data);

void tp_base_contact_list_remove_group_async (TpBaseContactList *self,
    const gchar *group,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_remove_group_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

typedef void (*TpMutableContactGroupListRenameGroupFunc) (
    TpMutableContactGroupList *self,
    const gchar *old_name,
    const gchar *new_name,
    GAsyncReadyCallback callback,
    gpointer user_data);

void tp_base_contact_list_rename_group_async (TpBaseContactList *self,
    const gchar *old_name,
    const gchar *new_name,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean tp_base_contact_list_rename_group_finish (TpBaseContactList *self,
    GAsyncResult *result,
    GError **error);

#define TP_TYPE_MUTABLE_CONTACT_GROUP_LIST \
  (tp_mutable_contact_group_list_get_type ())
GType tp_mutable_contact_group_list_get_type (void) G_GNUC_CONST;

#define TP_IS_MUTABLE_CONTACT_GROUP_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TP_TYPE_MUTABLE_CONTACT_GROUP_LIST))

#define TP_MUTABLE_CONTACT_GROUP_LIST_GET_INTERFACE(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
  TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, TpMutableContactGroupListInterface))

#define TP_MUTABLE_CONTACT_GROUP_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TP_TYPE_MUTABLE_CONTACT_GROUP_LIST, \
  TpMutableContactGroupList))

typedef struct _TpMutableContactGroupListInterface
    TpMutableContactGroupListInterface;

struct _TpMutableContactGroupListInterface {
    GTypeInterface parent;

    /* _async mandatory-to-implement, _finish has a default implementation
     * suitable for a GSimpleAsyncResult */

    TpMutableContactGroupListSetContactGroupsFunc set_contact_groups_async;
    TpMutableContactGroupListAsyncFinishFunc set_contact_groups_finish;

    TpMutableContactGroupListGroupContactsFunc set_group_members_async;
    TpMutableContactGroupListAsyncFinishFunc set_group_members_finish;

    TpMutableContactGroupListGroupContactsFunc add_to_group_async;
    TpMutableContactGroupListAsyncFinishFunc add_to_group_finish;

    TpMutableContactGroupListGroupContactsFunc remove_from_group_async;
    TpMutableContactGroupListAsyncFinishFunc remove_from_group_finish;

    TpMutableContactGroupListRemoveGroupFunc remove_group_async;
    TpMutableContactGroupListAsyncFinishFunc remove_group_finish;

    /* optional to implement */

    TpMutableContactGroupListRenameGroupFunc rename_group_async;
    TpMutableContactGroupListAsyncFinishFunc rename_group_finish;
    TpBaseContactListUIntFunc get_group_storage;
};

_TP_AVAILABLE_IN_1_0
gboolean tp_base_contact_list_fill_contact_attributes (TpBaseContactList *self,
  const gchar *dbus_interface,
  TpHandle contact,
  GVariantDict *attributes);

G_END_DECLS

#endif
