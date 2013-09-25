#include "config.h"

#include "content.h"
#include "content-priv.h"

#include <farstream/fs-conference.h>

#include "channel.h"


/**
 * SECTION:content
 * @short_description: Represent the Content of a channel handled by #TfChannel
 *
 * Objects of this class allow the user to handle the media side of a Telepathy
 * channel handled by #TfChannel.
 *
 * This object is created by the #TfChannel and the user is notified
 * of its creation by the #TfChannel::content-added signal. In the
 * callback for this signal, the user should connect to the
 * #TfContent::src-pad-added signal.
 *
 */


G_DEFINE_ABSTRACT_TYPE (TfContent, tf_content, G_TYPE_OBJECT);


enum
{
  PROP_TF_CHANNEL = 1,
  PROP_FS_CONFERENCE,
  PROP_FS_SESSION,
  PROP_MEDIA_TYPE,
  PROP_SINK_PAD,
  PROP_OBJECT_PATH
};

enum
{
  SIGNAL_START_SENDING,
  SIGNAL_STOP_SENDING,
  SIGNAL_SRC_PAD_ADDED,
  SIGNAL_START_RECEIVING,
  SIGNAL_STOP_RECEIVING,
  SIGNAL_RESTART_SOURCE,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

static void
tf_content_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  switch (property_id)
    {
      /* Other properties need to be overwritten */
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tf_content_class_init (TfContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = tf_content_get_property;

  g_object_class_install_property (object_class, PROP_TF_CHANNEL,
      g_param_spec_object ("tf-channel",
          "Parent TfChannel object ",
          "The Telepathy-Farstream Channel for this object",
          TF_TYPE_CHANNEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FS_CONFERENCE,
      g_param_spec_object ("fs-conference",
          "Farstream FsConference used by the Content ",
          "The Farstream conference for this content "
          "(could be the same as other contents)",
          FS_TYPE_CONFERENCE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FS_SESSION,
      g_param_spec_object ("fs-session",
          "Farstream FsSession ",
          "The Farstream session for this content",
          FS_TYPE_SESSION,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_SINK_PAD,
      g_param_spec_object ("sink-pad",
          "Sink Pad",
          "Sink GstPad for this content",
          GST_TYPE_PAD,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MEDIA_TYPE,
      g_param_spec_enum ("media-type",
          "MediaType",
          "The FsMediaType for this content",
          FS_TYPE_MEDIA_TYPE,
          0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_OBJECT_PATH,
      g_param_spec_string ("object-path",
          "content object path",
          "D-Bus object path of the Telepathy content which this content"
          " operates on",
          NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));


  /**
   * TfContent::start-sending:
   * @content: the #TfContent
   *
   * This signal is emitted when the connection manager ask to send media.
   * For example, this can be used to open a camera, start recording from a
   * microphone or play back a file. The application should start
   * sending data on the #TfContent:sink-pad
   *
   * Returns: %TRUE if the application can start providing data or %FALSE
   * otherwise
   */

  signals[SIGNAL_START_SENDING] =
      g_signal_new ("start-sending",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          g_signal_accumulator_true_handled, NULL, NULL,
          G_TYPE_BOOLEAN, 0);

  /**
   * TfContent::stop-sending:
   * @content: the #TfContent
   *
   * This signal is emitted when the connection manager ask to stop
   * sending media
   */

  signals[SIGNAL_STOP_SENDING] =
      g_signal_new ("stop-sending",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0, NULL, NULL, NULL,
          G_TYPE_NONE, 0);

  /**
   * TfContent::src-pad-added:
   * @content: the #TfContent
   * @handle: the handle of the remote party producing the content on this pad
   *    or 0 if unknown
   * @stream: the #FsStream for this pad
   * @pad: a #GstPad
   * @codec: the #FsCodec for this pad
   *
   * This signal is emitted when a data is coming on a new pad. This signal
   * is not emitted on the main thread, so special care must be made to lock
   * the relevant data. When the callback returns from this signal, data will
   * start flowing through the pad, so the application MUST connect a sink.
   */

  signals[SIGNAL_SRC_PAD_ADDED] =
      g_signal_new ("src-pad-added",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0, NULL, NULL, NULL,
          G_TYPE_NONE, 4,
          G_TYPE_UINT, FS_TYPE_STREAM, GST_TYPE_PAD, FS_TYPE_CODEC);

  /**
   * TfContent::start-receiving:
   * @content: the #TfContent
   * @handles: a 0-terminated array of #guint containing the handles
   * @handle_count: The number of handles in the @handles array
   *
   * This signal is emitted when the connection managers requests that the
   * application prepares itself to start receiving data again from certain
   * handles.
   *
   * This signal will only be emitted after the #TfContent::stop-receiving
   * signal has succeeded. It will not be emitted right after
   *  #TfContent::src-pad-added.
   *
   * Returns: %TRUE if the application can start receiving data or %FALSE
   * otherwise
   */

  signals[SIGNAL_START_RECEIVING] =
      g_signal_new ("start-receiving",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0,
          g_signal_accumulator_true_handled, NULL, NULL,
          G_TYPE_BOOLEAN, 2, G_TYPE_POINTER, G_TYPE_UINT);

  /**
   * TfContent::stop-receiving:
   * @content: the #TfContent
   * @handles: a 0-terminated array of #guint containing the handles
   * @handle_count: The number of handles in the @handles array
   *
   * This signal is emitted when the connection manager wants to tell the
   * application that it is now allowed to stop receiving.
   */

  signals[SIGNAL_STOP_RECEIVING] =
      g_signal_new ("stop-receiving",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0, NULL, NULL, NULL,
          G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_UINT);

  /**
   * TfContent::restart-source:
   * @content: the #TfContent
   *
   * This signal requests that the source be restarted so that the caps can
   * be renegotiated with a new resolutions and framerate.
   */

  signals[SIGNAL_RESTART_SOURCE] =
      g_signal_new ("restart-source",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0, NULL, NULL, NULL,
          G_TYPE_NONE, 0);
}


static void
tf_content_init (TfContent *self)
{
}


gboolean
_tf_content_start_sending (TfContent *self)
{
  GValue instance = {0};
  GValue sending_success_val = {0,};
  gboolean sending_success;


  if (self->sending_count)
    {
      self->sending_count ++;
      return TRUE;
    }

  g_value_init (&sending_success_val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&sending_success_val, TRUE);

  g_value_init (&instance, TF_TYPE_CONTENT);
  g_value_set_object (&instance, self);

  g_debug ("Requesting that the application start sending");

  g_signal_emitv (&instance, signals[SIGNAL_START_SENDING], 0,
      &sending_success_val);
  sending_success = g_value_get_boolean (&sending_success_val);

  g_value_unset (&instance);

  g_debug ("Request to start sending %s",
      sending_success ? "succeeded" : "failed");

  self->sending_count = 1;

  return sending_success;
}

void
_tf_content_stop_sending (TfContent *self)
{
  self->sending_count --;

  if (self->sending_count == 0)
    {
      g_signal_emit (self, signals[SIGNAL_STOP_SENDING], 0);
    }
}


void
_tf_content_emit_src_pad_added (TfContent *self, guint handle,
    FsStream *stream, GstPad *pad, FsCodec *codec)
{
  g_signal_emit (self, signals[SIGNAL_SRC_PAD_ADDED], 0, handle,
      stream, pad, codec);
}

/**
 * tf_content_error_literal:
 * @content: a #TfContent
 * @message: error Message
 *
 * Send a fatal streaming error to the Content to the CM, the effect is most
 * likely that the content will be removed.
 *
 * Rename to: tf_content_error
 */

void
tf_content_error_literal (TfContent *content,
    const gchar *message)
{
   TfContentClass *klass = TF_CONTENT_GET_CLASS (content);

   g_return_if_fail (content != NULL);
   g_return_if_fail (message != NULL);

   if (klass->content_error)
     klass->content_error (content, message);
   else
     GST_WARNING ("content_error not defined in class: %s", message);
}

/**
 * tf_content_error:
 * @content: a #TfContent
 * @message_format: error Message with printf style formatting
 * @...:  Parameters to insert into the @message_format string
 *
 * Send a fatal streaming error to the Content to the CM, the effect is most
 * likely that the content will be removed.
 */

void
tf_content_error (TfContent *content,
    const gchar *message_format,
    ...)
{
  gchar *message;
  va_list valist;

  g_return_if_fail (content != NULL);
  g_return_if_fail (message_format != NULL);

  va_start (valist, message_format);
  message = g_strdup_vprintf (message_format, valist);
  va_end (valist);

  tf_content_error_literal (content, message);
  g_free (message);
}

/**
 * tf_content_iterate_src_pads:
 * @content: a #TfContent
 * @handles: a 0 terminated array of #guint representing Telepathy handles
 * @handle_count: the numner of handles in @handles
 *
 * Provides a iterator that can be used to iterate through all of the src
 * pads that are are used to receive from a group of Telepathy handles.
 *
 * Returns: a #GstIterator
 */

GstIterator *
tf_content_iterate_src_pads (TfContent *content, guint *handles,
    guint handle_count)
{
  TfContentClass *klass = TF_CONTENT_GET_CLASS (content);

  g_return_val_if_fail (content != NULL, NULL);

  if (klass->iterate_src_pads)
    return klass->iterate_src_pads (content, handles, handle_count);
  else
    GST_WARNING ("iterate_src_pads not defined in class");

  return NULL;
}

gboolean
_tf_content_start_receiving (TfContent *self, guint *handles,
    guint handle_count)
{
  GValue instance_and_params[3] = {{0} , {0}, {0}};
  GValue receiving_success_val = {0,};
  gboolean receiving_success;

  g_value_init (&receiving_success_val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&receiving_success_val, TRUE);

  g_value_init (&instance_and_params[0], TF_TYPE_CONTENT);
  g_value_set_object (&instance_and_params[0], self);

  g_value_init (&instance_and_params[1], G_TYPE_POINTER);
  g_value_set_pointer (&instance_and_params[1], handles);

  g_value_init (&instance_and_params[2], G_TYPE_UINT);
  g_value_set_uint (&instance_and_params[2], handle_count);

  g_debug ("Requesting that the application start receiving");

  g_signal_emitv (instance_and_params, signals[SIGNAL_START_RECEIVING], 0,
      &receiving_success_val);
  receiving_success = g_value_get_boolean (&receiving_success_val);

  g_value_unset (&instance_and_params[0]);

  g_debug ("Request to start receiving %s",
      receiving_success ? "succeeded" : "failed");

  return receiving_success;
}

void
_tf_content_stop_receiving (TfContent *self, guint *handles,
    guint handle_count)
{
  g_debug ("Requesting that the application stop receiving");
  g_signal_emit (self, signals[SIGNAL_STOP_RECEIVING], 0, handles,
      handle_count);
}


/**
 * tf_content_sending_failed_literal:
 * @content: a #TfContent
 * @message: The error message
 *
 * Informs the Connection Manager that sending has failed for this
 * content. This is a transient error and it may or not not end the Content
 * and the call.
 *
 * Rename to: tf_content_sending_failed
 */

void
tf_content_sending_failed_literal (TfContent *content,
    const gchar *message)
{
  TfContentClass *klass = TF_CONTENT_GET_CLASS (content);

  g_return_if_fail (content != NULL);
  g_return_if_fail (message != NULL);

   if (klass->content_error)
     klass->sending_failed (content, message);
   else
     GST_WARNING ("sending_failed not defined in class, ignoring error: %s",
         message);
}

/**
 * tf_content_sending_failed:
 * @content: a #TfContent
 * @message_format: Message with printf style formatting
 * @...:  Parameters to insert into the @message_format string
 *
 * Informs the Connection Manager that sending has failed for this
 * content. This is a transient error and it may or not not end the Content
 * and the call.
 */

void
tf_content_sending_failed (TfContent *content,
    const gchar *message_format, ...)
{
  gchar *message;
  va_list valist;

  g_return_if_fail (content != NULL);
  g_return_if_fail (message_format != NULL);

  va_start (valist, message_format);
  message = g_strdup_vprintf (message_format, valist);
  va_end (valist);

  tf_content_sending_failed_literal (content, message);
  g_free (message);
}

/**
 * tf_content_receiving_failed_literal:
 * @content: a #TfContent
 * @handles: an array of #guint representing Telepathy handles, may be %NULL
 * @handle_count: the numner of handles in @handles
 * @message: The error message
 *
 * Informs the Connection Manager that receiving has failed for this
 * content. This is a transient error and it may or not not end the Content
 * and the call.
 *
 * If handles are not specific, it assumes that it is valid for all handles.
 *
 * Rename to: tf_content_receiving_failed
 */

void
tf_content_receiving_failed_literal (TfContent *content,
    guint *handles, guint handle_count,
    const gchar *message)
{
  TfContentClass *klass = TF_CONTENT_GET_CLASS (content);

  g_return_if_fail (content != NULL);
  g_return_if_fail (message != NULL);

  if (klass->content_error)
    klass->receiving_failed (content, handles, handle_count, message);
  else
    GST_WARNING ("receiving_failed not defined in class, ignoring error: %s",
        message);
}


/**
 * tf_content_receiving_failed:
 * @content: a #TfContent
 * @handles: an array of #guint representing Telepathy handles, may be %NULL
 * @handle_count: the numner of handles in @handles
 * @message_format: Message with printf style formatting
 * @...:  Parameters to insert into the @message_format string
 *
 * Informs the Connection Manager that receiving has failed for this
 * content. This is a transient error and it may or not not end the Content
 * and the call.
 *
 * If handles are not specific, it assumes that it is valid for all handles.
 */

void
tf_content_receiving_failed (TfContent *content,
    guint *handles, guint handle_count,
    const gchar *message_format, ...)
{
  gchar *message;
  va_list valist;

  g_return_if_fail (content != NULL);
  g_return_if_fail (message_format != NULL);

  va_start (valist, message_format);
  message = g_strdup_vprintf (message_format, valist);
  va_end (valist);

  tf_content_receiving_failed_literal (content, handles, handle_count, message);
  g_free (message);
}
