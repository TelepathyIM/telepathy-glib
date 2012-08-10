/*
 * Copyright (C) 2003-2007 Imendio AB
 * Copyright (C) 2007-2011 Collabora Ltd.
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
 *
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Jonny Lamb <jonny.lamb@collabora.co.uk>
 *          Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "log-store-xml-internal.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <telepathy-glib/telepathy-glib.h>

#include "telepathy-logger/call-event.h"
#include "telepathy-logger/call-event-internal.h"
#include "telepathy-logger/entity-internal.h"
#include "telepathy-logger/event-internal.h"
#include "telepathy-logger/text-event.h"
#include "telepathy-logger/text-event-internal.h"
#include "telepathy-logger/log-manager.h"
#include "telepathy-logger/log-store-internal.h"
#include "telepathy-logger/log-manager-internal.h"
#include "telepathy-logger/util-internal.h"

#define DEBUG_FLAG TPL_DEBUG_LOG_STORE
#include "telepathy-logger/debug-internal.h"

#define LOG_DIR_CREATE_MODE       (S_IRUSR | S_IWUSR | S_IXUSR)
#define LOG_FILE_CREATE_MODE      (S_IRUSR | S_IWUSR)
#define LOG_DIR_CHATROOMS         "chatrooms"
#define LOG_FILENAME_SUFFIX       ".log"
#define LOG_FILENAME_CALL_TAG     ".call"
#define LOG_FILENAME_CALL_SUFFIX LOG_FILENAME_CALL_TAG LOG_FILENAME_SUFFIX
#define LOG_DATE_PATTERN          "[0-9]{8,}"
#define LOG_FILENAME_PATTERN      "^" LOG_DATE_PATTERN "\\" LOG_FILENAME_SUFFIX "$"
#define LOG_FILENAME_CALL_PATTERN "^" LOG_DATE_PATTERN "\\" LOG_FILENAME_CALL_TAG "\\" LOG_FILENAME_SUFFIX "$"

#define LOG_TIME_FORMAT_FULL      "%Y%m%dT%H:%M:%S"
#define LOG_TIME_FORMAT           "%Y%m%d"
#define LOG_HEADER \
    "<?xml version='1.0' encoding='utf-8'?>\n" \
    "<?xml-stylesheet type=\"text/xsl\" href=\"log-store-xml.xsl\"?>\n" \
    "<log>\n"

#define LOG_FOOTER \
    "</log>\n"

#define ALL_SUPPORTED_TYPES (TPL_EVENT_MASK_TEXT | TPL_EVENT_MASK_CALL)
#define CONTAINS_ALL_SUPPORTED_TYPES(type_mask) \
  (((type_mask) & ALL_SUPPORTED_TYPES) == ALL_SUPPORTED_TYPES)


struct _TplLogStoreXmlPriv
{
  gchar *basedir;
  gchar *name;
  gboolean readable;
  gboolean writable;
  gboolean empathy_legacy;
  gboolean test_mode;
  TpAccountManager *account_manager;
};

enum {
    PROP_0,
    PROP_NAME,
    PROP_READABLE,
    PROP_WRITABLE,
    PROP_BASEDIR,
    PROP_EMPATHY_LEGACY,
    PROP_TESTMODE
};

static void log_store_iface_init (gpointer g_iface, gpointer iface_data);
static void tpl_log_store_xml_get_property (GObject *object, guint param_id, GValue *value,
    GParamSpec *pspec);
static void tpl_log_store_xml_set_property (GObject *object, guint param_id, const GValue *value,
    GParamSpec *pspec);
static const gchar *log_store_xml_get_name (TplLogStore *store);
static void log_store_xml_set_name (TplLogStoreXml *self, const gchar *data);
static const gchar *log_store_xml_get_basedir (TplLogStoreXml *self);
static void log_store_xml_set_basedir (TplLogStoreXml *self,
    const gchar *data);
static void log_store_xml_set_writable (TplLogStoreXml *self, gboolean data);
static void log_store_xml_set_readable (TplLogStoreXml *self, gboolean data);


G_DEFINE_TYPE_WITH_CODE (TplLogStoreXml, _tpl_log_store_xml,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TPL_TYPE_LOG_STORE, log_store_iface_init))


static void
log_store_xml_dispose (GObject *object)
{
  TplLogStoreXml *self = TPL_LOG_STORE_XML (object);
  TplLogStoreXmlPriv *priv = self->priv;

  /* FIXME See TP-bug #25569, when dispose a non prepared TP_AM, it
     might segfault.
     To avoid it, a *klduge*, a reference in the TplObserver to
     the TplLogManager is kept, so that until TplObserver is instanced,
     there will always be a TpLogManager reference and it won't be
     diposed */
  if (priv->account_manager != NULL)
    {
      g_object_unref (priv->account_manager);
      priv->account_manager = NULL;
    }

  G_OBJECT_CLASS (_tpl_log_store_xml_parent_class)->dispose (object);
}


static void
log_store_xml_finalize (GObject *object)
{
  TplLogStoreXml *self = TPL_LOG_STORE_XML (object);
  TplLogStoreXmlPriv *priv = self->priv;

  if (priv->basedir != NULL)
    {
      g_free (priv->basedir);
      priv->basedir = NULL;
    }
  if (priv->name != NULL)
    {
      g_free (priv->name);
      priv->name = NULL;
    }
}


static void
tpl_log_store_xml_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplLogStoreXmlPriv *priv = TPL_LOG_STORE_XML (object)->priv;

  switch (param_id)
    {
      case PROP_NAME:
        g_value_set_string (value, priv->name);
        break;
      case PROP_WRITABLE:
        g_value_set_boolean (value, priv->writable);
        break;
      case PROP_READABLE:
        g_value_set_boolean (value, priv->readable);
        break;
      case PROP_BASEDIR:
        g_value_set_string (value, priv->basedir);
        break;
      case PROP_EMPATHY_LEGACY:
        g_value_set_boolean (value, priv->empathy_legacy);
        break;
      case PROP_TESTMODE:
        g_value_set_boolean (value, priv->test_mode);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
tpl_log_store_xml_set_property (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplLogStoreXml *self = TPL_LOG_STORE_XML (object);

  switch (param_id)
    {
      case PROP_NAME:
        log_store_xml_set_name (self, g_value_get_string (value));
        break;
      case PROP_READABLE:
        log_store_xml_set_readable (self, g_value_get_boolean (value));
        break;
      case PROP_WRITABLE:
        log_store_xml_set_writable (self, g_value_get_boolean (value));
        break;
      case PROP_EMPATHY_LEGACY:
        self->priv->empathy_legacy = g_value_get_boolean (value);
        break;
      case PROP_BASEDIR:
        log_store_xml_set_basedir (self, g_value_get_string (value));
        break;
      case PROP_TESTMODE:
        self->priv->test_mode = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}


static void
_tpl_log_store_xml_class_init (TplLogStoreXmlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->finalize = log_store_xml_finalize;
  object_class->dispose = log_store_xml_dispose;
  object_class->get_property = tpl_log_store_xml_get_property;
  object_class->set_property = tpl_log_store_xml_set_property;

  g_object_class_override_property (object_class, PROP_NAME, "name");
  g_object_class_override_property (object_class, PROP_READABLE, "readable");
  g_object_class_override_property (object_class, PROP_WRITABLE, "writable");

  /**
   * TplLogStoreXml:basedir:
   *
   * The log store's basedir.
   */
  param_spec = g_param_spec_string ("basedir",
      "Basedir",
      "The TplLogStore implementation's name",
      NULL, G_PARAM_READABLE | G_PARAM_WRITABLE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_BASEDIR, param_spec);

  /**
   * TplLogStoreXml:empathy-legacy:
   *
   * If %TRUE, the logstore pointed by TplLogStoreXml::base-dir will be
   * considered formatted as an Empathy's LogStore (pre telepathy-logger).
   * Xml: %FALSE.
   */
  param_spec = g_param_spec_boolean ("empathy-legacy",
      "EmpathyLegacy",
      "Enables compatibility with old Empathy's logs",
      FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_EMPATHY_LEGACY,
      param_spec);

  param_spec = g_param_spec_boolean ("testmode",
      "TestMode",
      "Whether the logstore is in testmode, for testsuite use only",
      FALSE, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TESTMODE, param_spec);

  g_type_class_add_private (object_class, sizeof (TplLogStoreXmlPriv));
}


static void
_tpl_log_store_xml_init (TplLogStoreXml *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_LOG_STORE_XML, TplLogStoreXmlPriv);
  self->priv->account_manager = tp_account_manager_dup ();
}


static gchar *
log_store_account_to_dirname (TpAccount *account)
{
  const gchar *name;

  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);

  name = tp_proxy_get_object_path (account);
  if (g_str_has_prefix (name, TP_ACCOUNT_OBJECT_PATH_BASE))
    name += strlen (TP_ACCOUNT_OBJECT_PATH_BASE);

  return g_strdelimit (g_strdup (name), "/", '_');
}


/* id can be NULL, but if present have to be a non zero-lenght string.
 * If NULL, the returned dir will be composed until the account part.
 * If non-NULL, the returned dir will be composed until the id part */
static gchar *
log_store_xml_get_dir (TplLogStoreXml *self,
    TpAccount *account,
    TplEntity *target)
{
  gchar *basedir;
  gchar *escaped_account;
  gchar *escaped_id = NULL;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);

  escaped_account = log_store_account_to_dirname (account);

  if (target != NULL)
    {
      /* FIXME This may be source of bug (does that case still exist ?)
       * avoid that 1-1 conversation generated from a chatroom, having id similar
       * to room@conference.domain/My_Alias (in XMPP) are treated as a directory
       * path, creating My_Alias as a subdirectory of room@conference.domain */
      escaped_id = g_strdelimit (
          g_strdup (tpl_entity_get_identifier (target)),
          "/", '_');
    }

  if (target != NULL
      && tpl_entity_get_entity_type (target) == TPL_ENTITY_ROOM)
    basedir = g_build_path (G_DIR_SEPARATOR_S,
        log_store_xml_get_basedir (self), escaped_account, LOG_DIR_CHATROOMS,
        escaped_id, NULL);
  else
    basedir = g_build_path (G_DIR_SEPARATOR_S,
        log_store_xml_get_basedir (self), escaped_account, escaped_id, NULL);

  g_free (escaped_account);
  g_free (escaped_id);

  return basedir;
}


static const gchar *
log_store_xml_get_file_suffix (GType type)
{
  if (type == TPL_TYPE_TEXT_EVENT)
    return LOG_FILENAME_SUFFIX;
  else if (type == TPL_TYPE_CALL_EVENT)
    return LOG_FILENAME_CALL_SUFFIX;
  else
    g_return_val_if_reached (NULL);
}


static gchar *
log_store_xml_get_timestamp_filename (GType type,
    gint64 timestamp)
{
  gchar *date_str;
  gchar *filename;
  GDateTime *date;

  date = g_date_time_new_from_unix_utc (timestamp);
  date_str = g_date_time_format (date, LOG_TIME_FORMAT);
  filename = g_strconcat (date_str, log_store_xml_get_file_suffix (type),
      NULL);

  g_date_time_unref (date);
  g_free (date_str);

  return filename;
}


static gchar *
log_store_xml_format_timestamp (gint64 timestamp)
{
  GDateTime *ts;
  gchar *ts_str;

  ts = g_date_time_new_from_unix_utc (timestamp);
  ts_str = g_date_time_format (ts, LOG_TIME_FORMAT_FULL);

  g_date_time_unref (ts);

  return ts_str;
}


static gchar *
log_store_xml_get_timestamp_from_event (TplEvent *event)
{
  return log_store_xml_format_timestamp (tpl_event_get_timestamp (event));
}


static gchar *
log_store_xml_get_filename (TplLogStoreXml *self,
    TpAccount *account,
    TplEntity *target,
    GType type,
    gint64 timestamp)
{
  gchar *id_dir;
  gchar *timestamp_str;
  gchar *filename;

  id_dir = log_store_xml_get_dir (self, account, target);
  timestamp_str = log_store_xml_get_timestamp_filename (type, timestamp);
  filename = g_build_filename (id_dir, timestamp_str, NULL);

  g_free (id_dir);
  g_free (timestamp_str);

  return filename;
}


/* this is a method used at the end of the add_event process, used by any
 * Event<Type> instance. it should the only method allowed to write to the
 * store */
static gboolean
_log_store_xml_write_to_store (TplLogStoreXml *self,
    TpAccount *account,
    TplEntity *target,
    const gchar *event,
    GType type,
    gint64 timestamp,
    GError **error)
{
  FILE *file;
  gchar *filename;
  gchar *basedir;
  gboolean ret = TRUE;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), FALSE);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), FALSE);
  g_return_val_if_fail (TPL_IS_ENTITY (target), FALSE);


  filename = log_store_xml_get_filename (self, account, target, type, timestamp);
  basedir = g_path_get_dirname (filename);

  if (!g_file_test (basedir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
    {
      DEBUG ("Creating directory: '%s'", basedir);
      g_mkdir_with_parents (basedir, LOG_DIR_CREATE_MODE);
    }

  g_free (basedir);

  if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      file = g_fopen (filename, "w+");
      if (file != NULL)
        g_fprintf (file, LOG_HEADER);

      g_chmod (filename, LOG_FILE_CREATE_MODE);
    }
  else
    {
      file = g_fopen (filename, "r+");
      if (file != NULL)
        fseek (file, -strlen (LOG_FOOTER), SEEK_END);
    }

  if (file == NULL)
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_FAILED,
          "Couldn't open log file: %s", filename);
      ret = FALSE;
      goto out;
    }

  g_fprintf (file, "%s", event);
  DEBUG ("%s: written: %s", filename, event);

  fclose (file);
 out:
  g_free (filename);
  return ret;
}


static gboolean
add_text_event (TplLogStoreXml *self,
    TplTextEvent *message,
    GError **error)
{
  gboolean ret = FALSE;
  gint64 timestamp;
  TpDBusDaemon *bus_daemon;
  TpAccount *account;
  TplEntity *sender;
  const gchar *body_str;
  const gchar *token_str;
  gchar *avatar_token = NULL;
  gchar *body = NULL;
  gchar *time_str = NULL;
  gchar *contact_name = NULL;
  gchar *contact_id = NULL;
  GString *event = NULL;
  TpChannelTextMessageType msg_type;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), FALSE);
  g_return_val_if_fail (TPL_IS_TEXT_EVENT (message), FALSE);

  bus_daemon = tp_dbus_daemon_dup (error);
  if (bus_daemon == NULL)
    {
      DEBUG ("Error acquiring bus daemon: %s", (*error)->message);
      goto out;
    }

  account =  tpl_event_get_account (TPL_EVENT (message));

  body_str = tpl_text_event_get_message (message);
  if (TPL_STR_EMPTY (body_str))
    {
      g_set_error (error, TPL_LOG_STORE_ERROR,
          TPL_LOG_STORE_ERROR_FAILED,
          "The message body is empty or NULL");
      goto out;
    }

  body = g_markup_escape_text (body_str, -1);
  msg_type = tpl_text_event_get_message_type (message);
  time_str = log_store_xml_get_timestamp_from_event (
      TPL_EVENT (message));

  sender = tpl_event_get_sender (TPL_EVENT (message));

  if (sender != NULL)
    {
      contact_id = g_markup_escape_text (tpl_entity_get_identifier (sender), -1);
      contact_name = g_markup_escape_text (tpl_entity_get_alias (sender), -1);
      avatar_token = g_markup_escape_text (tpl_entity_get_avatar_token (sender),
          -1);
    }

  event = g_string_new (NULL);
  g_string_printf (event, "<message time='%s' id='%s' name='%s' "
      "token='%s' isuser='%s' type='%s'",
      time_str,
      contact_id ? contact_id : "",
      contact_name ? contact_name : "",
      avatar_token ? avatar_token : "",
      (sender && tpl_entity_get_entity_type (sender)
          == TPL_ENTITY_SELF) ? "true" : "false",
      _tpl_text_event_message_type_to_str (msg_type));

  token_str = tpl_text_event_get_message_token (message);
  if (!TPL_STR_EMPTY (token_str))
    {
      gchar *message_token = g_markup_escape_text (token_str, -1);
      g_string_append_printf (event, " message-token='%s'", message_token);
      g_free (message_token);

      token_str = tpl_text_event_get_supersedes_token (message);
      if (!TPL_STR_EMPTY (token_str))
        {
          gchar *supersedes_token = g_markup_escape_text (token_str, -1);
          guint edit_timestamp;
          g_string_append_printf (event, " supersedes-token='%s'",
              supersedes_token);

          edit_timestamp = tpl_text_event_get_edit_timestamp (message);
          if (edit_timestamp != 0)
            {
              gchar *edit_timestamp_str =
                  log_store_xml_format_timestamp (edit_timestamp);
              g_string_append_printf (event, " edit-timestamp='%s'",
                  edit_timestamp_str);
              g_free (edit_timestamp_str);
            }
        }

    }

  timestamp = tpl_event_get_timestamp (TPL_EVENT (message));

  g_string_append_printf (event, ">%s</message>\n" LOG_FOOTER, body);

  DEBUG ("writing text event from %s (ts %s)",
      contact_id, time_str);

  ret = _log_store_xml_write_to_store (self, account,
      _tpl_event_get_target (TPL_EVENT (message)), event->str,
      TPL_TYPE_TEXT_EVENT, timestamp, error);

out:
  g_free (contact_id);
  g_free (contact_name);
  g_free (time_str);
  g_free (body);
  g_string_free (event, TRUE);
  g_free (avatar_token);

  if (bus_daemon != NULL)
    g_object_unref (bus_daemon);

  return ret;
}


static gboolean
add_call_event (TplLogStoreXml *self,
    TplCallEvent *event,
    GError **error)
{
  gboolean ret = FALSE;
  TpDBusDaemon *bus_daemon;
  TpAccount *account;
  TplEntity *sender;
  TplEntity *actor;
  TplEntity *target;
  gchar *time_str = NULL;
  gchar *sender_avatar = NULL;
  gchar *sender_name = NULL;
  gchar *sender_id = NULL;
  gchar *actor_name = NULL;
  gchar *actor_avatar = NULL;
  gchar *actor_id = NULL;
  gchar *log_str = NULL;
  TpCallStateChangeReason reason;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), FALSE);
  g_return_val_if_fail (TPL_IS_CALL_EVENT (event), FALSE);

  bus_daemon = tp_dbus_daemon_dup (error);
  if (bus_daemon == NULL)
    {
      DEBUG ("Error acquiring bus daemon: %s", (*error)->message);
      goto out;
    }

  account = tpl_event_get_account (TPL_EVENT (event));

  time_str = log_store_xml_get_timestamp_from_event (
      TPL_EVENT (event));
  reason = tpl_call_event_get_end_reason (event);

  sender = tpl_event_get_sender (TPL_EVENT (event));
  actor = tpl_call_event_get_end_actor (event);
  target = _tpl_event_get_target (TPL_EVENT (event));

  if (sender != NULL)
    {
      sender_id = g_markup_escape_text (tpl_entity_get_identifier (sender), -1);
      sender_name = g_markup_escape_text (tpl_entity_get_alias (sender), -1);
      sender_avatar = g_markup_escape_text (tpl_entity_get_avatar_token (sender),
          -1);
    }

  if (actor != NULL)
    {
      actor_id = g_markup_escape_text (tpl_entity_get_identifier (actor), -1);
      actor_name = g_markup_escape_text (tpl_entity_get_alias (actor), -1);
      actor_avatar = g_markup_escape_text (tpl_entity_get_avatar_token (actor),
          -1);
    }


  log_str = g_strdup_printf ("<call time='%s' "
      "id='%s' name='%s' isuser='%s' token='%s' "
      "duration='%" G_GINT64_FORMAT "' "
      "actor='%s' actortype='%s' "
      "actorname='%s' actortoken='%s' "
      "reason='%s' detail='%s'/>\n"
      LOG_FOOTER,
        time_str,
        sender_id ? sender_id : "",
        sender_name ? sender_name : "",
        (sender && tpl_entity_get_entity_type (sender) ==
            TPL_ENTITY_SELF) ? "true" : "false",
        sender_avatar ? sender_avatar : "",
        tpl_call_event_get_duration (event),
        actor_id ? actor_id : "",
        actor ? _tpl_entity_type_to_str (tpl_entity_get_entity_type (actor)) : "",
        actor_name ? actor_name : "",
        actor_avatar ? actor_avatar : "",
        _tpl_call_event_end_reason_to_str (reason),
        tpl_call_event_get_detailed_end_reason (event));

  DEBUG ("writing call event from %s (ts %s)",
      tpl_entity_get_identifier (target),
      time_str);

  ret = _log_store_xml_write_to_store (self, account, target, log_str,
      TPL_TYPE_CALL_EVENT, tpl_event_get_timestamp (TPL_EVENT (event)),
      error);

out:
  g_free (sender_id);
  g_free (sender_name);
  g_free (sender_avatar);
  g_free (actor_id);
  g_free (actor_name);
  g_free (actor_avatar);
  g_free (time_str);
  g_free (log_str);

  if (bus_daemon != NULL)
    g_object_unref (bus_daemon);

  return ret;
}


/* First of two phases selection: understand the type Event */
static gboolean
log_store_xml_add_event (TplLogStore *store,
    TplEvent *event,
    GError **error)
{
  TplLogStoreXml *self = TPL_LOG_STORE_XML (store);

  g_return_val_if_fail (TPL_IS_EVENT (event), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (TPL_IS_TEXT_EVENT (event))
    return add_text_event (self, TPL_TEXT_EVENT (event), error);
  else if (TPL_IS_CALL_EVENT (event))
    return add_call_event (self, TPL_CALL_EVENT (event), error);

  DEBUG ("TplEntry not handled by this LogStore (%s). "
      "Ignoring Event", log_store_xml_get_name (store));
  /* do not consider it an error, this LogStore simply do not want/need
   * this Event */
  return TRUE;
}


static gboolean
log_store_xml_exists_in_directory (const gchar *dirname,
    GRegex *regex,
    gint type_mask,
    gboolean recursive)
{
  gboolean exists;
  const gchar *basename;
  GDir *dir;

  DEBUG ("Looking in directory '%s' %s",
      dirname, recursive ? "resursively" : "");

  dir = g_dir_open (dirname, 0, NULL);
  exists = (dir != NULL);

  if (CONTAINS_ALL_SUPPORTED_TYPES (type_mask) || !exists)
    goto out;

  exists = FALSE;
  while ((basename = g_dir_read_name (dir)) != NULL)
    {
      gchar *filename;

      filename = g_build_filename (dirname, basename, NULL);

      DEBUG ("Matching with filename '%s'", basename);

      if (recursive && g_file_test (filename, G_FILE_TEST_IS_DIR))
        exists = log_store_xml_exists_in_directory (filename, regex, type_mask,
            !tp_strdiff (basename, LOG_DIR_CHATROOMS));
      else if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        exists = g_regex_match (regex, basename, 0, 0);

      g_free (filename);

      if (exists)
        break;
    }

out:
  if (dir != NULL)
    g_dir_close (dir);

  return exists;
}


static GRegex *
log_store_xml_create_filename_regex (gint type_mask)
{
  GString *pattern;
  GRegex *regex = NULL;
  GError *error = NULL;

  pattern = g_string_new ("");

  if (type_mask & TPL_EVENT_MASK_TEXT)
    g_string_append (pattern, LOG_FILENAME_PATTERN);

  if (type_mask & TPL_EVENT_MASK_CALL)
    g_string_append_printf (pattern,
        "%s" LOG_FILENAME_CALL_PATTERN,
        pattern->len == 0 ? "" : "|");

  if (pattern->len == 0)
    goto out;

  DEBUG ("Pattern is '%s'", pattern->str);

  regex = g_regex_new (pattern->str, G_REGEX_OPTIMIZE, 0, &error);

  if (regex == NULL)
    {
      DEBUG ("Failed to create regex: %s", error->message);
      g_error_free (error);
    }

out:
  g_string_free (pattern, TRUE);

  return regex;
}


static gboolean
log_store_xml_exists (TplLogStore *store,
    TpAccount *account,
    TplEntity *target,
    gint type_mask)
{
  TplLogStoreXml *self = (TplLogStoreXml *) store;
  gchar *dirname;
  GRegex *regex;
  gboolean exists = FALSE;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), FALSE);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), FALSE);
  g_return_val_if_fail (target == NULL || TPL_IS_ENTITY (target), FALSE);

  dirname = log_store_xml_get_dir (self, account, target);
  regex = log_store_xml_create_filename_regex (type_mask);

  if (regex != NULL)
    exists = log_store_xml_exists_in_directory (dirname, regex, type_mask,
        target == NULL);

  g_free (dirname);

  if (regex != NULL)
    g_regex_unref (regex);

  return exists;
}

static GDate *
create_date_from_string (const gchar *str)
{
  GDate *date;
  guint u;
  guint day, month, year;

  if (sscanf (str, "%u", &u) != 1)
    return NULL;

  day = (u % 100);
  month = ((u / 100) % 100);
  year = (u / 10000);

  if (!g_date_valid_dmy (day, month, year))
    return NULL;

  date = g_date_new_dmy (day, month, year);

  return date;
}


static gboolean
log_store_xml_match_in_file (const gchar *filename,
    GRegex *regex)
{
  gboolean retval = FALSE;
  GMappedFile *file;
  gsize length;
  gchar *contents = NULL;

  file = g_mapped_file_new (filename, FALSE, NULL);
  if (file == NULL)
    goto out;

  length = g_mapped_file_get_length (file);
  contents = g_mapped_file_get_contents (file);

  if (length == 0 || contents == NULL)
    goto out;

  retval = g_regex_match_full (regex, contents, length, 0, 0, NULL, NULL);

  DEBUG ("%s pattern '%s' in file '%s'",
      retval ? "Matched" : "Not matched",
      g_regex_get_pattern (regex),
      filename);

out:
  if (file != NULL)
    g_mapped_file_unref (file);

  return retval;
}


static GList *
log_store_xml_get_dates (TplLogStore *store,
    TpAccount *account,
    TplEntity *target,
    gint type_mask)
{
  TplLogStoreXml *self = (TplLogStoreXml *) store;
  GList *dates = NULL;
  GList *l;
  gchar *directory = NULL;
  GDir *dir = NULL;
  GString *pattern = NULL;
  GRegex *regex = NULL;
  const gchar *basename;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (TPL_IS_ENTITY (target), NULL);

  directory = log_store_xml_get_dir (self, account, target);
  dir = g_dir_open (directory, 0, NULL);
  if (!dir)
    {
      DEBUG ("Could not open directory:'%s'", directory);
      goto out;
    }

  DEBUG ("Collating a list of dates in:'%s'", directory);
  regex = log_store_xml_create_filename_regex (type_mask);

  if (regex == NULL)
    goto out;

  while ((basename = g_dir_read_name (dir)) != NULL)
    {
      const gchar *p;
      gchar *str;
      GDate *date;

      if (!g_regex_match (regex, basename, 0, NULL))
        continue;

      p = strstr (basename, LOG_FILENAME_CALL_SUFFIX);

      if (p == NULL)
        p = strstr (basename, LOG_FILENAME_SUFFIX);

      str = g_strndup (basename, p - basename);

      if (str == NULL)
        continue;

      date = create_date_from_string (str);

      if (date != NULL)
        dates = g_list_insert_sorted (dates, date,
            (GCompareFunc) g_date_compare);

      g_free (str);
    }

  /* Filter out duplicate dates in-place */
  for (l = dates; g_list_next (l) != NULL; l = g_list_next (l))
    {
      GList *next = g_list_next (l);

      if (g_date_compare ((GDate *) next->data, (GDate *) l->data) == 0)
        {
          g_date_free ((GDate *) next->data);
          l = g_list_delete_link (l, next);
        }
    }

out:
  g_free (directory);

  if (dir != NULL)
    g_dir_close (dir);

  if (pattern != NULL)
    g_string_free (pattern, TRUE);

  if (regex != NULL)
    g_regex_unref (regex);

  DEBUG ("Parsed %d dates", g_list_length (dates));

  return dates;
}


static gchar *
log_store_xml_get_filename_for_date (TplLogStoreXml *self,
    TpAccount *account,
    TplEntity *target,
    const GDate *date,
    GType type)
{
  gchar *basedir;
  gchar *timestamp;
  gchar *filename;
  gchar str[9];

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (TPL_IS_ENTITY (target), NULL);
  g_return_val_if_fail (date != NULL, NULL);

  g_date_strftime (str, 9, "%Y%m%d", date);

  basedir = log_store_xml_get_dir (self, account, target);
  timestamp = g_strconcat (str, log_store_xml_get_file_suffix (type), NULL);
  filename = g_build_filename (basedir, timestamp, NULL);

  g_free (basedir);
  g_free (timestamp);

  return filename;
}


static TplLogSearchHit *
log_store_xml_search_hit_new (TplLogStoreXml *self,
    const gchar *filename)
{
  TplLogSearchHit *hit;
  gchar *account_name;
  const gchar *end;
  gchar **strv;
  guint len;
  GList *accounts, *l;
  gchar *tmp;
  TpAccount *account = NULL;
  GDate *date;
  const gchar *chat_id;
  gboolean is_chatroom;
  TplEntity *target;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (filename), NULL);
  g_return_val_if_fail (g_str_has_suffix (filename, LOG_FILENAME_SUFFIX),
      NULL);

  strv = g_strsplit (filename, G_DIR_SEPARATOR_S, -1);
  len = g_strv_length (strv);

  end = strstr (strv[len - 1], LOG_FILENAME_SUFFIX);
  tmp = g_strndup (strv[len - 1], end - strv[len - 1]);
  date = create_date_from_string (tmp);
  g_free (tmp);
  chat_id = strv[len - 2];
  is_chatroom = (strcmp (strv[len - 3], LOG_DIR_CHATROOMS) == 0);

  if (is_chatroom)
    account_name = strv[len - 4];
  else
    account_name = strv[len - 3];

  /* FIXME: This assumes the account manager is prepared, but the
   * synchronous API forces this. See bug #599189. */
  accounts = tp_account_manager_get_valid_accounts (
      self->priv->account_manager);

  for (l = accounts; l != NULL && account == NULL; l = g_list_next (l))
    {
      TpAccount *acc = TP_ACCOUNT (l->data);
      gchar *name;

      name = log_store_account_to_dirname (acc);
      if (!tp_strdiff (name, account_name))
        account = acc;
      g_free (name);
    }
  g_list_free (accounts);

  if (is_chatroom)
    target = tpl_entity_new_from_room_id (chat_id);
  else
    target = tpl_entity_new (chat_id, TPL_ENTITY_CONTACT, NULL, NULL);

  hit = _tpl_log_manager_search_hit_new (account, target, date);

  g_strfreev (strv);
  g_date_free (date);
  g_object_unref (target);

  return hit;
}


static TplEvent *
parse_text_node (TplLogStoreXml *self,
    xmlNodePtr node,
    gboolean is_room,
    const gchar *target_id,
    const gchar *self_id,
    TpAccount *account)
{
  TplEvent *event;
  TplEntity *sender;
  TplEntity *receiver;
  gchar *time_str;
  gint64 timestamp;
  gchar *edit_time_str;
  gint64 edit_timestamp = 0;
  gchar *sender_id;
  gchar *sender_name;
  gchar *sender_avatar_token;
  gchar *body;
  gchar *message_token;
  gchar *supersedes_token;
  gchar *is_user_str;
  gboolean is_user = FALSE;
  gchar *msg_type_str;
  TpChannelTextMessageType msg_type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;

  body = (gchar *) xmlNodeGetContent (node);
  time_str = (gchar *) xmlGetProp (node, (const xmlChar *) "time");
  edit_time_str = (gchar *) xmlGetProp (node,
      (const xmlChar *) "edit-timestamp");
  sender_id = (gchar *) xmlGetProp (node, (const xmlChar *) "id");
  sender_name = (gchar *) xmlGetProp (node, (const xmlChar *) "name");
  sender_avatar_token = (gchar *) xmlGetProp (node,
      (const xmlChar *) "token");
  message_token = (gchar *) xmlGetProp (node,
      (const xmlChar *) "message-token");
  supersedes_token = (gchar *) xmlGetProp (node,
      (const xmlChar *) "supersedes-token");
  is_user_str = (gchar *) xmlGetProp (node, (const xmlChar *) "isuser");
  msg_type_str = (gchar *) xmlGetProp (node, (const xmlChar *) "type");

  if (is_user_str != NULL)
    is_user = (!tp_strdiff (is_user_str, "true"));

  if (msg_type_str != NULL)
    msg_type = _tpl_text_event_message_type_from_str (msg_type_str);

  timestamp = _tpl_time_parse (time_str);

  if (supersedes_token != NULL && edit_time_str != NULL)
    {
      edit_timestamp = _tpl_time_parse (edit_time_str);
    }

  if (is_room)
    receiver = tpl_entity_new_from_room_id (target_id);
  else if (is_user)
    receiver = tpl_entity_new (target_id, TPL_ENTITY_CONTACT, NULL, NULL);
  else
    receiver = tpl_entity_new (self_id, TPL_ENTITY_SELF,
        tp_account_get_nickname (account), NULL);

  sender = tpl_entity_new (sender_id,
      is_user ? TPL_ENTITY_SELF : TPL_ENTITY_CONTACT,
      sender_name, sender_avatar_token);

  event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", account,
      /* MISSING: "channel-path", channel_path, */
      "receiver", receiver,
      "sender", sender,
      "timestamp", timestamp,
      /* TplTextEvent */
      "message-type", msg_type,
      "message", body,
      "message-token", message_token,
      "supersedes-token", supersedes_token,
      "edit-timestamp", edit_timestamp,
      NULL);

  g_object_unref (sender);
  g_object_unref (receiver);
  xmlFree (time_str);
  xmlFree (edit_time_str);
  xmlFree (sender_id);
  xmlFree (sender_name);
  xmlFree (body);
  xmlFree (message_token);
  xmlFree (supersedes_token);
  xmlFree (is_user_str);
  xmlFree (msg_type_str);
  xmlFree (sender_avatar_token);

  return event;
}


static TplEvent *
parse_call_node (TplLogStoreXml *self,
    xmlNodePtr node,
    gboolean is_room,
    const gchar *target_id,
    const gchar *self_id,
    TpAccount *account)
{
  TplEvent *event;
  TplEntity *sender;
  TplEntity *receiver;
  TplEntity *actor;
  gchar *time_str;
  gint64 timestamp;
  gchar *sender_id;
  gchar *sender_name;
  gchar *sender_avatar_token;
  gchar *is_user_str;
  gboolean is_user = FALSE;
  gchar *actor_id;
  gchar *actor_name;
  gchar *actor_type;
  gchar *actor_avatar_token;
  gchar *duration_str;
  gint64 duration = -1;
  gchar *reason_str;
  TpCallStateChangeReason reason = TP_CALL_STATE_CHANGE_REASON_UNKNOWN;
  gchar *detailed_reason;

  time_str = (gchar *) xmlGetProp (node, (const xmlChar *) "time");
  sender_id = (gchar *) xmlGetProp (node, (const xmlChar *) "id");
  sender_name = (gchar *) xmlGetProp (node, (const xmlChar *) "name");
  sender_avatar_token = (gchar *) xmlGetProp (node,
      (const xmlChar *) "token");
  is_user_str = (gchar *) xmlGetProp (node, (const xmlChar *) "isuser");
  duration_str = (char *) xmlGetProp (node, (const xmlChar*) "duration");
  actor_id = (char *) xmlGetProp (node, (const xmlChar *) "actor");
  actor_name = (char *) xmlGetProp (node, (const xmlChar *) "actorname");
  actor_type = (char *) xmlGetProp (node, (const xmlChar *) "actortype");
  actor_avatar_token = (char *) xmlGetProp (node,
      (const xmlChar *) "actortoken");
  reason_str = (char *) xmlGetProp (node, (const xmlChar *) "reason");
  detailed_reason = (char *) xmlGetProp (node, (const xmlChar *) "detail");

  if (is_user_str != NULL)
    is_user = (!tp_strdiff (is_user_str, "true"));

  if (reason_str != NULL)
    reason = _tpl_call_event_str_to_end_reason (reason_str);

  timestamp = _tpl_time_parse (time_str);

  if (is_room)
    receiver = tpl_entity_new_from_room_id (target_id);
  else if (is_user)
    receiver = tpl_entity_new (target_id, TPL_ENTITY_CONTACT, NULL, NULL);
  else
    receiver = tpl_entity_new (self_id, TPL_ENTITY_SELF,
        tp_account_get_nickname (account), NULL);

  sender = tpl_entity_new (sender_id,
      is_user ? TPL_ENTITY_SELF : TPL_ENTITY_CONTACT,
      sender_name, sender_avatar_token);

  actor = tpl_entity_new (actor_id,
      _tpl_entity_type_from_str (actor_type),
      actor_name, actor_avatar_token);

  if (duration_str != NULL)
    duration = atoll (duration_str);

  event = g_object_new (TPL_TYPE_CALL_EVENT,
      /* TplEvent */
      "account", account,
      /* MISSING: "channel-path", channel_path, */
      "receiver", receiver,
      "sender", sender,
      "timestamp", timestamp,
      /* TplCallEvent */
      "duration", duration,
      "end-actor", actor,
      "end-reason", reason,
      "detailed-end-reason", detailed_reason,
      NULL);

  g_object_unref (sender);
  g_object_unref (receiver);
  g_object_unref (actor);
  xmlFree (time_str);
  xmlFree (sender_id);
  xmlFree (sender_name);
  xmlFree (sender_avatar_token);
  xmlFree (is_user_str);
  xmlFree (actor_id);
  xmlFree (actor_name);
  xmlFree (actor_type);
  xmlFree (actor_avatar_token);
  xmlFree (duration_str);

  return event;
}


static void
event_queue_replace_and_supersede (GQueue *events,
    GList *index,
    GHashTable *superseded_links,
    TplTextEvent *event)
{
  _tpl_text_event_add_supersedes (event, index->data);
  g_hash_table_insert (superseded_links,
      (gpointer) tpl_text_event_get_message_token (index->data), index);
  g_object_unref (index->data);
  index->data = event;
}


static GList *
event_queue_add_text_event (GQueue *events,
    GList *index,
    GHashTable *superseded_links,
    TplTextEvent *event)
{
  GList *l = NULL;
  const gchar *supersedes_token = tpl_text_event_get_supersedes_token (event);
  TplTextEvent *dummy_event;

  if (supersedes_token == NULL)
    return _tpl_event_queue_insert_sorted_after (events, index,
        TPL_EVENT (event));

  l = g_hash_table_lookup (superseded_links, supersedes_token);
  if (l != NULL)
    {
      event_queue_replace_and_supersede (events, l, superseded_links, event);
      return index;
    }

  /* Search backwards from "now" and insert (but don't update "now") */
  for (l = index; l != NULL; l = g_list_previous (l))
    {
      if (!tp_strdiff (tpl_text_event_get_message_token (l->data),
          supersedes_token))
        {
          event_queue_replace_and_supersede (events, l, superseded_links,
              event);
          return index;
        }
    }

  DEBUG ("Can't find event %s (superseded by %s). "
      "Adding Dummy event.",
      supersedes_token, tpl_text_event_get_message_token (event));

  dummy_event = g_object_new (TPL_TYPE_TEXT_EVENT,
      /* TplEvent */
      "account", tpl_event_get_account (TPL_EVENT (event)),
      /* MISSING: "channel-path", channel_path, */
      "receiver", tpl_event_get_receiver (TPL_EVENT (event)),
      "sender", tpl_event_get_sender (TPL_EVENT (event)),
      "timestamp", tpl_event_get_timestamp (TPL_EVENT (event)),
      /* TplTextEvent */
      "message-type", tpl_text_event_get_message_type (event),
      "message", "",
      "message-token", supersedes_token,
      NULL);

  index = _tpl_event_queue_insert_sorted_after (events, index,
      TPL_EVENT (dummy_event));
  event_queue_replace_and_supersede (events, index, superseded_links, event);
  return index;
}


/* returns a Glist of TplEvent instances */
static void
log_store_xml_get_events_for_file (TplLogStoreXml *self,
    TpAccount *account,
    const gchar *filename,
    GType type,
    GQueue *events)
{
  xmlParserCtxtPtr ctxt;
  xmlDocPtr doc;
  xmlNodePtr log_node;
  xmlNodePtr node;
  gboolean is_room;
  gchar *dirname;
  gchar *tmp;
  gchar *target_id;
  gchar *self_id;
  GHashTable *supersedes_links;
  GError *error = NULL;
  guint num_events = 0;
  GList *index;

  g_return_if_fail (TPL_IS_LOG_STORE_XML (self));
  g_return_if_fail (TP_IS_ACCOUNT (account));
  g_return_if_fail (!TPL_STR_EMPTY (filename));

  DEBUG ("Attempting to parse filename:'%s'...", filename);

  if (!g_file_test (filename, G_FILE_TEST_EXISTS))
    {
      DEBUG ("Filename:'%s' does not exist", filename);
      return;
    }

  if (!tp_account_parse_object_path (
        tp_proxy_get_object_path (TP_PROXY (account)),
        NULL, NULL, &self_id, &error))
    {
      DEBUG ("Cannot get self identifier from account: %s",
          error->message);
      g_error_free (error);
      return;
    }

  /* Create parser. */
  ctxt = xmlNewParserCtxt ();

  /* Parse and validate the file. */
  doc = xmlCtxtReadFile (ctxt, filename, NULL, 0);
  if (!doc)
    {
      g_warning ("Failed to parse file:'%s'", filename);
      xmlFreeParserCtxt (ctxt);
      g_free (self_id);
      return;
    }

  /* The root node, presets. */
  log_node = xmlDocGetRootElement (doc);
  if (!log_node)
    {
      xmlFreeDoc (doc);
      xmlFreeParserCtxt (ctxt);
      g_free (self_id);
      return;
    }

  /* Guess the target based on directory name */
  dirname = g_path_get_dirname (filename);
  target_id = g_path_get_basename (dirname);

  /* Determine if it's a chatroom */
  tmp = dirname;
  dirname = g_path_get_dirname (tmp);
  g_free (tmp);
  tmp = g_path_get_basename (dirname);
  is_room = (g_strcmp0 (LOG_DIR_CHATROOMS, tmp) == 0);
  g_free (dirname);
  g_free (tmp);

  /* Temporary hash from (borrowed) supersedes-token to (borrowed) link in
   * events, for any event that was once in events, but has since been
   * superseded (and therefore won't be found by a linear search). */
  supersedes_links = g_hash_table_new (g_str_hash, g_str_equal);

  /* Now get the events. */
  index = NULL;
  for (node = log_node->children; node; node = node->next)
    {
      TplEvent *event = NULL;

      if (type == TPL_TYPE_TEXT_EVENT
          && strcmp ((const gchar *) node->name, "message") == 0)
        {
          event = parse_text_node (self, node, is_room, target_id, self_id,
              account);

          if (event == NULL)
            continue;

          index = event_queue_add_text_event (events, index,
              supersedes_links, TPL_TEXT_EVENT (event));
          num_events++;
        }
      else if (type == TPL_TYPE_CALL_EVENT
          && strcmp ((const char*) node->name, "call") == 0)
        {
          event = parse_call_node (self, node, is_room, target_id, self_id,
              account);

          if (event == NULL)
            continue;

          index = _tpl_event_queue_insert_sorted_after (events, index, event);
          num_events++;
        }
    }

  DEBUG ("Parsed %u events", num_events);

  g_free (target_id);
  xmlFreeDoc (doc);
  xmlFreeParserCtxt (ctxt);
  g_hash_table_unref (supersedes_links);
}


/* If dir is NULL, basedir will be used instead.
 * Used to make possible the full search vs. specific subtrees search */
static GList *
log_store_xml_get_all_files (TplLogStoreXml *self,
    const gchar *dir,
    gint type_mask)
{
  GDir *gdir;
  GList *files = NULL;
  const gchar *name;
  const gchar *basedir;
  GRegex *regex;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  /* dir can be NULL, do not check */

  basedir = (dir != NULL) ? dir : log_store_xml_get_basedir (self);

  gdir = g_dir_open (basedir, 0, NULL);
  if (!gdir)
    return NULL;

  regex = log_store_xml_create_filename_regex (type_mask);

  if (regex == NULL)
    goto out;

  while ((name = g_dir_read_name (gdir)) != NULL)
    {
      gchar *filename;

      filename = g_build_filename (basedir, name, NULL);

      if (g_regex_match (regex, name, 0, NULL))
        files = g_list_prepend (files, filename);
      else if (g_file_test (filename, G_FILE_TEST_IS_DIR))
        {
          /* Recursively get all log files */
          files = g_list_concat (files,
              log_store_xml_get_all_files (self, filename, type_mask));
          g_free (filename);
        }
    }

out:
  g_dir_close (gdir);

  if (regex != NULL)
    g_regex_unref (regex);

  return files;
}


static GList *
_log_store_xml_search_in_files (TplLogStoreXml *self,
    const gchar *text,
    GList *files,
    gint type_mask)
{
  GList *l;
  GList *hits = NULL;
  gchar *markup_text;
  gchar *escaped_text;
  GString *pattern = NULL;
  GRegex *regex = NULL;
  GError *error = NULL;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (text), NULL);

  markup_text = g_markup_escape_text (text, -1);
  escaped_text = g_regex_escape_string (markup_text, -1);
  g_free (markup_text);

  pattern = g_string_new ("");

  if (type_mask & TPL_EVENT_MASK_TEXT)
    g_string_append_printf (pattern,
        "<message [^>]*>[^<]*%s[^<]*</message>"
        "|<message( [^>]* | )id='[^>]*%s[^>]*'"
        "|<message( [^>]* | )name='[^>]*%s[^>]*'",
        escaped_text, escaped_text, escaped_text);

  if (type_mask & TPL_EVENT_MASK_CALL)
    g_string_append_printf (pattern,
        "%s<call( [^>]* | )id='[^>]*%s[^>]*'"
        "|<call( [^>]* | )name='[^>]*%s[^>]*'"
        "|<call( [^>]* | )actor='[^>]*%s[^>]*'"
        "|<call( [^>]* | )actorname='[^>]*%s[^>]*'",
        pattern->len == 0 ? "" : "|",
        escaped_text, escaped_text, escaped_text, escaped_text);

  if (TPL_STR_EMPTY (pattern->str))
    goto out;

  regex = g_regex_new (pattern->str,
      G_REGEX_CASELESS | G_REGEX_OPTIMIZE,
      0,
      &error);

  if (!regex)
    {
      DEBUG ("Failed to compile regex: %s", error->message);
      g_error_free (error);
      goto out;
    }

  for (l = files; l; l = g_list_next (l))
    {
      gchar *filename = l->data;

      if (log_store_xml_match_in_file (filename, regex))
        {
          TplLogSearchHit *hit;

          hit = log_store_xml_search_hit_new (self, filename);
          if (hit != NULL)
            {
              hits = g_list_prepend (hits, hit);
              DEBUG ("Found text:'%s' in file:'%s' on date: %04u-%02u-%02u",
                  text, filename, g_date_get_year (hit->date),
                  g_date_get_month (hit->date), g_date_get_day (hit->date));
            }
        }
    }

out:
  g_free (escaped_text);

  if (pattern != NULL)
    g_string_free (pattern, TRUE);

  if (regex != NULL)
    g_regex_unref (regex);

  g_list_free (files);
  return hits;
}


static GList *
log_store_xml_search_new (TplLogStore *store,
    const gchar *text,
    gint type_mask)
{
  TplLogStoreXml *self = (TplLogStoreXml *) store;
  GList *files;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (text), NULL);

  files = log_store_xml_get_all_files (self, NULL, type_mask);
  DEBUG ("Found %d log files in total", g_list_length (files));

  return _log_store_xml_search_in_files (self, text, files, type_mask);
}


/* Returns: (GList *) of (TplLogSearchHit *) */
static GList *
log_store_xml_get_entities_for_dir (TplLogStoreXml *self,
    const gchar *dir,
    gboolean is_chatroom,
    TpAccount *account)
{
  GDir *gdir;
  GList *entities = NULL;
  const gchar *name;
  GError *error = NULL;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (!TPL_STR_EMPTY (dir), NULL);

  gdir = g_dir_open (dir, 0, &error);
  if (!gdir)
    {
      DEBUG ("Failed to open directory: %s, error: %s", dir, error->message);
      g_error_free (error);
      return NULL;
    }

  while ((name = g_dir_read_name (gdir)) != NULL)
    {
      TplEntity *entity;

      if (!is_chatroom && strcmp (name, LOG_DIR_CHATROOMS) == 0)
        {
          gchar *filename = g_build_filename (dir, name, NULL);
          entities = g_list_concat (entities,
              log_store_xml_get_entities_for_dir (self, filename, TRUE, account));
          g_free (filename);
          continue;
        }

      if (is_chatroom)
        entity = tpl_entity_new_from_room_id (name);
      else
        entity = tpl_entity_new (name, TPL_ENTITY_CONTACT, NULL, NULL);

      entities = g_list_prepend (entities, entity);
    }

  g_dir_close (gdir);

  return entities;
}


/* returns a Glist of TplEvent instances */
static GList *
log_store_xml_get_events_for_date (TplLogStore *store,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    const GDate *date)
{
  TplLogStoreXml *self = (TplLogStoreXml *) store;
  gchar *filename;
  GQueue events = G_QUEUE_INIT;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (TPL_IS_ENTITY (target), NULL);
  g_return_val_if_fail (date != NULL, NULL);

  if (type_mask & TPL_EVENT_MASK_TEXT)
    {
      filename = log_store_xml_get_filename_for_date (self, account, target,
          date, TPL_TYPE_TEXT_EVENT);
      log_store_xml_get_events_for_file (self, account, filename,
          TPL_TYPE_TEXT_EVENT, &events);
      g_free (filename);
    }

  if (type_mask & TPL_EVENT_MASK_CALL)
    {
      filename = log_store_xml_get_filename_for_date (self, account, target,
          date, TPL_TYPE_CALL_EVENT);
      log_store_xml_get_events_for_file (self, account, filename,
          TPL_TYPE_CALL_EVENT, &events);
      g_free (filename);
    }

  return events.head;
}


static GList *
log_store_xml_get_entities (TplLogStore *store,
    TpAccount *account)
{
  TplLogStoreXml *self = (TplLogStoreXml *) store;
  gchar *dir;
  GList *entities;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);

  dir = log_store_xml_get_dir (self, account, NULL);
  entities = log_store_xml_get_entities_for_dir (self, dir, FALSE, account);
  g_free (dir);

  return entities;
}


static const gchar *
log_store_xml_get_name (TplLogStore *store)
{
  TplLogStoreXml *self = (TplLogStoreXml *) store;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);

  return self->priv->name;
}


/* returns am absolute path for the base directory of LogStore */
static const gchar *
log_store_xml_get_basedir (TplLogStoreXml *self)
{
  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);

  /* set default based on name if NULL, see prop's comment about it in
   * class_init method */
  if (self->priv->basedir == NULL)
    {
      gchar *dir;
      const char *user_data_dir;
      const char *name;

      if (self->priv->test_mode && g_getenv ("TPL_TEST_LOG_DIR") != NULL)
        {
          user_data_dir = g_getenv ("TPL_TEST_LOG_DIR");
          name = self->priv->empathy_legacy ? "Empathy" : "TpLogger";
        }
      else
        {
          user_data_dir = g_get_user_data_dir ();
          name =  log_store_xml_get_name ((TplLogStore *) self);
        }

      dir = g_build_path (G_DIR_SEPARATOR_S, user_data_dir, name, "logs",
          NULL);
      log_store_xml_set_basedir (self, dir);
      g_free (dir);
    }

  return self->priv->basedir;
}


static void
log_store_xml_set_name (TplLogStoreXml *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_LOG_STORE_XML (self));
  g_return_if_fail (!TPL_STR_EMPTY (data));
  g_return_if_fail (self->priv->name == NULL);

  self->priv->name = g_strdup (data);
}

static void
log_store_xml_set_basedir (TplLogStoreXml *self,
    const gchar *data)
{
  g_return_if_fail (TPL_IS_LOG_STORE_XML (self));
  g_return_if_fail (self->priv->basedir == NULL);
  /* data may be NULL when the class is initialized and the default value is
   * set */

  self->priv->basedir = g_strdup (data);

  /* at install_spec time, default value is set to NULL, ignore it */
  if (self->priv->basedir != NULL)
    DEBUG ("logstore set to dir: %s", data);
}


static void
log_store_xml_set_readable (TplLogStoreXml *self,
    gboolean data)
{
  g_return_if_fail (TPL_IS_LOG_STORE_XML (self));

  self->priv->readable = data;
}


static void
log_store_xml_set_writable (TplLogStoreXml *self,
    gboolean data)
{
  g_return_if_fail (TPL_IS_LOG_STORE_XML (self));

  self->priv->writable = data;
}


static GList *
log_store_xml_get_filtered_events (TplLogStore *store,
    TpAccount *account,
    TplEntity *target,
    gint type_mask,
    guint num_events,
    TplLogEventFilter filter,
    gpointer user_data)
{
  TplLogStoreXml *self = (TplLogStoreXml *) store;
  GList *dates, *l, *events = NULL;
  guint i = 0;

  g_return_val_if_fail (TPL_IS_LOG_STORE_XML (self), NULL);
  g_return_val_if_fail (TP_IS_ACCOUNT (account), NULL);
  g_return_val_if_fail (TPL_IS_ENTITY (target), NULL);

  dates = log_store_xml_get_dates (store, account, target, type_mask);

  for (l = g_list_last (dates); l != NULL && i < num_events;
       l = g_list_previous (l))
    {
      GList *new_events, *n;

      /* FIXME: We should really restrict the event parsing to get only
       * the newest num_events. */
      new_events = log_store_xml_get_events_for_date (store, account,
          target, type_mask, l->data);

      n = g_list_last (new_events);
      while (n != NULL && i < num_events)
        {
          if (filter == NULL || filter (n->data, user_data))
            {
              events = g_list_prepend (events, g_object_ref (n->data));
              i++;
            }
          n = g_list_previous (n);
        }
      g_list_foreach (new_events, (GFunc) g_object_unref, NULL);
      g_list_free (new_events);
    }

  g_list_foreach (dates, (GFunc) g_date_free, NULL);
  g_list_free (dates);

  return events;
}


static void
log_store_xml_clear (TplLogStore *store)
{
  TplLogStoreXml *self = TPL_LOG_STORE_XML (store);
  const gchar *basedir;

  /* We need to use the getter otherwise the basedir might not be set yet */
  basedir = log_store_xml_get_basedir (self);

  DEBUG ("Clear all logs from XML store in: %s", basedir);

  _tpl_rmdir_recursively (basedir);
}


static void
log_store_xml_clear_account (TplLogStore *store,
    TpAccount *account)
{
  TplLogStoreXml *self = TPL_LOG_STORE_XML (store);
  gchar *account_dir;

  account_dir = log_store_xml_get_dir (self, account, NULL);

  if (account_dir)
    {
      DEBUG ("Clear account logs from XML store in: %s",
          account_dir);
      _tpl_rmdir_recursively (account_dir);
      g_free (account_dir);
    }
  else
    DEBUG ("Nothing to clear in account: %s",
        tp_proxy_get_object_path (TP_PROXY (account)));
}


static void
log_store_xml_clear_entity (TplLogStore *store,
    TpAccount *account,
    TplEntity *entity)
{
  TplLogStoreXml *self = TPL_LOG_STORE_XML (store);
  gchar *entity_dir;

  entity_dir = log_store_xml_get_dir (self, account, entity);

  if (entity_dir)
    {
      DEBUG ("Clear entity logs from XML store in: %s",
          entity_dir);

      _tpl_rmdir_recursively (entity_dir);
      g_free (entity_dir);
    }
  else
    DEBUG ("Nothing to clear for account/entity: %s/%s",
        tp_proxy_get_object_path (TP_PROXY (account)),
        tpl_entity_get_identifier (entity));
}


static void
log_store_iface_init (gpointer g_iface,
    gpointer iface_data)
{
  TplLogStoreInterface *iface = (TplLogStoreInterface *) g_iface;

  iface->get_name = log_store_xml_get_name;
  iface->exists = log_store_xml_exists;
  iface->add_event = log_store_xml_add_event;
  iface->get_dates = log_store_xml_get_dates;
  iface->get_events_for_date = log_store_xml_get_events_for_date;
  iface->get_entities = log_store_xml_get_entities;
  iface->search_new = log_store_xml_search_new;
  iface->get_filtered_events = log_store_xml_get_filtered_events;
  iface->clear = log_store_xml_clear;
  iface->clear_account = log_store_xml_clear_account;
  iface->clear_entity = log_store_xml_clear_entity;
}
