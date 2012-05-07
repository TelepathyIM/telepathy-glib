/*
 * room.c - a chatroom channel
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "room.h"

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

G_DEFINE_TYPE_WITH_CODE (ExampleCSHRoomChannel,
    example_csh_room_channel,
    TP_TYPE_BASE_CHANNEL,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_TYPE_TEXT,
      tp_message_mixin_text_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_MESSAGES,
      tp_message_mixin_messages_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_INTERFACE_GROUP,
      tp_group_mixin_iface_init);)

/* type definition stuff */

enum
{
  PROP_SIMULATION_DELAY = 1,
  N_PROPS
};

struct _ExampleCSHRoomChannelPrivate
{
  guint simulation_delay;
};

static GPtrArray *
example_csh_room_channel_get_interfaces (TpBaseChannel *self)
{
  GPtrArray *interfaces;

  interfaces = TP_BASE_CHANNEL_CLASS (example_csh_room_channel_parent_class)->
    get_interfaces (self);

  g_ptr_array_add (interfaces, TP_IFACE_CHANNEL_INTERFACE_GROUP);
  g_ptr_array_add (interfaces, TP_IFACE_CHANNEL_INTERFACE_MESSAGES);
  return interfaces;
};

static void
example_csh_room_channel_init (ExampleCSHRoomChannel *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EXAMPLE_TYPE_CSH_ROOM_CHANNEL,
      ExampleCSHRoomChannelPrivate);
}

static TpHandle
suggest_room_identity (ExampleCSHRoomChannel *self)
{
  TpBaseConnection *connection =
    tp_base_channel_get_connection (TP_BASE_CHANNEL (self));
  TpHandleRepoIface *contact_repo =
    tp_base_connection_get_handles (connection, TP_HANDLE_TYPE_CONTACT);
  TpHandleRepoIface *room_repo =
    tp_base_connection_get_handles (connection, TP_HANDLE_TYPE_ROOM);
  gchar *nick, *id;
  TpHandle ret;

  nick = g_strdup (tp_handle_inspect (contact_repo,
          tp_base_connection_get_self_handle (connection)));
  g_strdelimit (nick, "@", '\0');
  id = g_strdup_printf ("%s@%s", nick, tp_handle_inspect (room_repo,
        tp_base_channel_get_target_handle (TP_BASE_CHANNEL (self))));
  g_free (nick);

  ret = tp_handle_ensure (contact_repo, id, NULL, NULL);
  g_free (id);

  g_assert (ret != 0);
  return ret;
}


/* This timeout callback represents a successful join. In a real CM it'd
 * happen in response to network events, rather than just a timer */
static void
complete_join (ExampleCSHRoomChannel *self)
{
  TpBaseConnection *conn = tp_base_channel_get_connection (TP_BASE_CHANNEL (self));
  TpHandleRepoIface *contact_repo =
    tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);
  TpHandleRepoIface *room_repo =
    tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_ROOM);
  const gchar *room_name = tp_handle_inspect (room_repo,
      tp_base_channel_get_target_handle (TP_BASE_CHANNEL (self)));
  gchar *str;
  TpHandle alice_local, bob_local, chris_local, anon_local;
  TpHandle alice_global, bob_global, chris_global;
  TpGroupMixin *mixin = TP_GROUP_MIXIN (self);
  TpIntset *added;

  /* For this example, we assume that all chatrooms initially contain
   * Alice, Bob and Chris (and that their global IDs are also known),
   * and they also contain one anonymous user. */

  str = g_strdup_printf ("alice@%s", room_name);
  alice_local = tp_handle_ensure (contact_repo, str, NULL, NULL);
  g_free (str);
  alice_global = tp_handle_ensure (contact_repo, "alice@alpha", NULL, NULL);

  str = g_strdup_printf ("bob@%s", room_name);
  bob_local = tp_handle_ensure (contact_repo, str, NULL, NULL);
  g_free (str);
  bob_global = tp_handle_ensure (contact_repo, "bob@beta", NULL, NULL);

  str = g_strdup_printf ("chris@%s", room_name);
  chris_local = tp_handle_ensure (contact_repo, str, NULL, NULL);
  g_free (str);
  chris_global = tp_handle_ensure (contact_repo, "chris@chi", NULL, NULL);

  str = g_strdup_printf ("anonymous coward@%s", room_name);
  anon_local = tp_handle_ensure (contact_repo, str, NULL, NULL);
  g_free (str);

  /* If our chosen nick is not available, pretend the server would
   * automatically rename us on entry. */
  if (mixin->self_handle == alice_local ||
      mixin->self_handle == bob_local ||
      mixin->self_handle == chris_local ||
      mixin->self_handle == anon_local)
    {
      TpHandle new_self;
      TpIntset *rp = tp_intset_new ();
      TpIntset *removed = tp_intset_new ();

      str = g_strdup_printf ("renamed by server@%s", room_name);
      new_self = tp_handle_ensure (contact_repo, str, NULL, NULL);
      g_free (str);

      tp_intset_add (rp, new_self);
      tp_intset_add (removed, mixin->self_handle);

      tp_group_mixin_add_handle_owner ((GObject *) self, new_self,
          tp_base_connection_get_self_handle (conn));
      tp_group_mixin_change_self_handle ((GObject *) self, new_self);

      tp_group_mixin_change_members ((GObject *) self, "", NULL, removed, NULL,
          rp, 0, TP_CHANNEL_GROUP_CHANGE_REASON_RENAMED);

      tp_intset_destroy (removed);
      tp_intset_destroy (rp);
    }

  tp_group_mixin_add_handle_owner ((GObject *) self, alice_local,
      alice_global);
  tp_group_mixin_add_handle_owner ((GObject *) self, bob_local,
      bob_global);
  tp_group_mixin_add_handle_owner ((GObject *) self, chris_local,
      chris_global);
  /* we know that anon_local is channel-specific, but not whose it is,
   * hence 0 */
  tp_group_mixin_add_handle_owner ((GObject *) self, anon_local, 0);

  /* everyone in! */
  added = tp_intset_new();
  tp_intset_add (added, alice_local);
  tp_intset_add (added, bob_local);
  tp_intset_add (added, chris_local);
  tp_intset_add (added, anon_local);
  tp_intset_add (added, mixin->self_handle);

  tp_group_mixin_change_members ((GObject *) self, "", added, NULL, NULL,
      NULL, 0, TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  /* now that the dust has settled, we can also invite people */
  tp_group_mixin_change_flags ((GObject *) self,
      TP_CHANNEL_GROUP_FLAG_CAN_ADD | TP_CHANNEL_GROUP_FLAG_MESSAGE_ADD,
      0);
}


static void
join_room (ExampleCSHRoomChannel *self)
{
  TpBaseConnection *conn = tp_base_channel_get_connection (TP_BASE_CHANNEL (self));
  TpGroupMixin *mixin = TP_GROUP_MIXIN (self);
  GObject *object = (GObject *) self;
  TpIntset *add_remote_pending;

  g_assert (!tp_handle_set_is_member (mixin->members, mixin->self_handle));
  g_assert (!tp_handle_set_is_member (mixin->remote_pending,
        mixin->self_handle));

  /* Indicate in the Group interface that a join is in progress */

  add_remote_pending = tp_intset_new ();
  tp_intset_add (add_remote_pending, mixin->self_handle);

  tp_group_mixin_add_handle_owner (object, mixin->self_handle,
      tp_base_connection_get_self_handle (conn));
  tp_group_mixin_change_members (object, "", NULL, NULL, NULL,
      add_remote_pending, tp_base_connection_get_self_handle (conn),
      TP_CHANNEL_GROUP_CHANGE_REASON_NONE);

  tp_intset_destroy (add_remote_pending);

  /* Actually join the room. In a real implementation this would be a network
   * round-trip - we don't have a network, so pretend that joining takes
   * a short time */
  g_timeout_add (self->priv->simulation_delay, (GSourceFunc) complete_join,
      self);
}

static void
send_message (GObject *object,
    TpMessage *message,
    TpMessageSendingFlags flags)
{
  /* The /dev/null of text channels - we claim to have sent the message,
   * but nothing more happens */
  tp_message_mixin_sent (object, message, flags, "", NULL);
}

static GObject *
constructor (GType type,
             guint n_props,
             GObjectConstructParam *props)
{
  static TpChannelTextMessageType const types[] = {
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION,
      TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE
  };
  static const gchar * const content_types[] = { "*/*", NULL };
  GObject *object =
      G_OBJECT_CLASS (example_csh_room_channel_parent_class)->constructor (type,
          n_props, props);
  ExampleCSHRoomChannel *self = EXAMPLE_CSH_ROOM_CHANNEL (object);
  TpBaseConnection *conn = tp_base_channel_get_connection (TP_BASE_CHANNEL (self));
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles
      (conn, TP_HANDLE_TYPE_CONTACT);
  TpHandle self_handle;

  tp_base_channel_register (TP_BASE_CHANNEL (self));

  tp_message_mixin_init (object, G_STRUCT_OFFSET (ExampleCSHRoomChannel,
        message_mixin), conn);

  tp_message_mixin_implement_sending (object, send_message,
      G_N_ELEMENTS (types), types,
      TP_MESSAGE_PART_SUPPORT_FLAG_ONE_ATTACHMENT |
        TP_MESSAGE_PART_SUPPORT_FLAG_MULTIPLE_ATTACHMENTS,
      0, /* no TpDeliveryReportingSupportFlags */
      content_types);

  /* We start off remote-pending (if this CM supported other people inviting
   * us, we'd start off local-pending in that case instead - but it doesn't),
   * with this self-handle. */
  self_handle = suggest_room_identity (self);

  tp_group_mixin_init (object,
      G_STRUCT_OFFSET (ExampleCSHRoomChannel, group),
      contact_repo, self_handle);

  /* Initially, we can't do anything. */
  tp_group_mixin_change_flags (object,
      TP_CHANNEL_GROUP_FLAG_CHANNEL_SPECIFIC_HANDLES |
      TP_CHANNEL_GROUP_FLAG_PROPERTIES,
      0);

  /* Immediately attempt to join the group */
  join_room (self);

  return object;
}


static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *pspec)
{
  ExampleCSHRoomChannel *self = EXAMPLE_CSH_ROOM_CHANNEL (object);

  switch (property_id)
    {
    case PROP_SIMULATION_DELAY:
      g_value_set_uint (value, self->priv->simulation_delay);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
set_property (GObject *object,
              guint property_id,
              const GValue *value,
              GParamSpec *pspec)
{
  ExampleCSHRoomChannel *self = EXAMPLE_CSH_ROOM_CHANNEL (object);

  switch (property_id)
    {
    case PROP_SIMULATION_DELAY:
      self->priv->simulation_delay = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
example_csh_room_channel_close (TpBaseChannel *self)
{
  tp_base_channel_destroyed (self);
}

static void
finalize (GObject *object)
{
  tp_message_mixin_finalize (object);

  ((GObjectClass *) example_csh_room_channel_parent_class)->finalize (object);
}


static gboolean
add_member (GObject *object,
            TpHandle handle,
            const gchar *message,
            GError **error)
{
  /* In a real implementation, if handle was mixin->self_handle we'd accept
   * an invitation here; otherwise we'd invite the given contact.
   * Here, we do nothing for now. */
  return TRUE;
}

static gboolean
remove_member_with_reason (GObject *object,
                           TpHandle handle,
                           const gchar *message,
                           guint reason,
                           GError **error)
{
  ExampleCSHRoomChannel *self = EXAMPLE_CSH_ROOM_CHANNEL (object);

  if (handle == self->group.self_handle)
    {
      /* TODO: if simulating a channel where the user is an operator, let them
       * kick themselves (like in IRC), resulting in different "network"
       * messages */

      example_csh_room_channel_close (TP_BASE_CHANNEL (self));
      return TRUE;
    }
  else
    {
      /* TODO: also simulate some channels where the user is an operator and
       * can kick people */
      g_set_error (error, TP_ERROR, TP_ERROR_PERMISSION_DENIED,
          "You can't eject other users from this channel");
      return FALSE;
    }
}

static void
example_csh_room_channel_class_init (ExampleCSHRoomChannelClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;
  TpBaseChannelClass *base_class = TP_BASE_CHANNEL_CLASS (klass);
  GParamSpec *param_spec;

  g_type_class_add_private (klass, sizeof (ExampleCSHRoomChannelPrivate));

  object_class->constructor = constructor;
  object_class->set_property = set_property;
  object_class->get_property = get_property;
  object_class->finalize = finalize;

  base_class->channel_type = TP_IFACE_CHANNEL_TYPE_TEXT;
  base_class->target_handle_type = TP_HANDLE_TYPE_ROOM;
  base_class->get_interfaces = example_csh_room_channel_get_interfaces;

  base_class->close = example_csh_room_channel_close;

  param_spec = g_param_spec_uint ("simulation-delay", "Simulation delay",
      "Delay between simulated network events",
      0, G_MAXUINT32, 500,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SIMULATION_DELAY,
      param_spec);

  tp_group_mixin_class_init (object_class,
      G_STRUCT_OFFSET (ExampleCSHRoomChannelClass, group_class),
      add_member,
      NULL);
  tp_group_mixin_class_allow_self_removal (object_class);
  tp_group_mixin_class_set_remove_with_reason_func (object_class,
      remove_member_with_reason);
  tp_group_mixin_init_dbus_properties (object_class);

  tp_message_mixin_init_dbus_properties (object_class);
}
