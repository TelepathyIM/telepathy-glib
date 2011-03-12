
#include "content.h"
#include "content-priv.h"

#include <gst/farsight/fs-conference-iface.h>

#include "channel.h"
#include "tf-signals-marshal.h"


/**
 * SECTION:content
 * @short_description: Represent the Content of a channel handled by #TfChannel
 *
 * Objects of this class allow the user to handle the media side of a Telepathy
 * channel handled by #TfChannel.
 *
 * This object is created by the #TfChannel and the user is notified of its
 * creation by the #TfChannel::content-added signal. In the callback for this
 * signal, the user should call tf_content_set_codec_preferences() and connect
 * to the #TfContent::src-pad-added signal.
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
          "The Telepathy-Farsight Channel for this object",
          TF_TYPE_CHANNEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FS_CONFERENCE,
      g_param_spec_object ("fs-conference",
          "Farsight2 FsConference used by the Content ",
          "The Farsight2 conference for this content "
          "(could be the same as other contents)",
          FS_TYPE_CONFERENCE,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FS_SESSION,
      g_param_spec_object ("fs-session",
          "Farsight2 FsSession ",
          "The Farsight2 session for this content",
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
   * TfContent::start-sending
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
          g_signal_accumulator_true_handled, NULL,
          _tf_marshal_BOOLEAN__VOID,
          G_TYPE_BOOLEAN, 0);

  /**
   * TfContent::stop-sending
   * @content: the #TfContent
   *
   * This signal is emitted when the connection manager ask to stop
   * sending media
   */

  signals[SIGNAL_STOP_SENDING] =
      g_signal_new ("stop-sending",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0, NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);

  /**
   * TfContent::src-pad-added
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
          0, NULL, NULL,
          _tf_marshal_VOID__UINT_OBJECT_OBJECT_BOXED,
          G_TYPE_NONE, 4,
          G_TYPE_UINT, FS_TYPE_STREAM, GST_TYPE_PAD, FS_TYPE_CODEC);
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
    g_signal_emit (self, signals[SIGNAL_STOP_SENDING], 0);
}

void
_tf_content_emit_src_pad_added (TfContent *self, guint handle,
    FsStream *stream, GstPad *pad, FsCodec *codec)
{
  g_signal_emit (self, signals[SIGNAL_SRC_PAD_ADDED], 0, handle,
      stream, pad, codec);
}

/**
 * tf_content_set_codec_preferences:
 * @content: the #TfContent
 * @codec_preferences: The #GList of #FsCodec
 * @error: a #GError or %NULL
 *
 * Set the list of desired codec preferences. It is a #GList
 * of #FsCodec. The function does not take ownership of the list.
 *
 * The payload type may be a valid dynamic PT (96-127), %FS_CODEC_ID_DISABLE
 * or %FS_CODEC_ID_ANY. If the encoding name is "reserve-pt", then the
 * payload type of the codec will be "reserved" and not be used by any
 * dynamically assigned payload type.
 *
 * If the list of specifications would invalidate all codecs, an error will
 * be returned.
 *
 * This function should be called only during the callback for the
 * #TfChannel::content-added signal. Afterwards, the codecs may have been
 * set to the connection manager.
 *
 * Returns: %TRUE if the preferences could be set of %FALSE if there was an
 * error, in that case @error will have been set.
 */

gboolean
tf_content_set_codec_preferences (TfContent *content,
    GList *codec_preferences,
    GError **error)
{
 TfContentClass *klass = TF_CONTENT_GET_CLASS (content);

  if (klass->set_codec_preferences) {
    return klass->set_codec_preferences (content, codec_preferences, error);
  } else {
    GST_WARNING ("set_codec_preferences not defined in class");
    g_set_error (error, FS_ERROR, FS_ERROR_NOT_IMPLEMENTED,
        "set_codec_preferences not defined in class");
  }
  return FALSE;
}

/**
 * tf_content_error:
 * @content: a #TfContent
 * @reason: the reason (a #TfContentRemovalReason)
 * @detailed_reason: The detailled error (as a DBus name)
 * @message: error Message
 *
 * Send an error to the Content to the CM, the effect is most likely that the
 * content will be removed.
 */

void
tf_content_error (TfContent *content,
    guint reason, /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message)
{
   TfContentClass *klass = TF_CONTENT_GET_CLASS (content);

   if (klass->content_error)
     klass->content_error (content, reason, detailed_reason, message);
   else
     GST_WARNING ("content_error not defined in class: %s", message);
}

/**
 * tf_content_error:
 * @content: a #TfContent
 * @reason: the reason (a #TfContentRemovalReason)
 * @detailed_reason: The detailled error (as a DBus name)
 * @message: error Message
 *
 * Send an error to the Content to the CM, the effect is most likely that the
 * content will be removed.
 */

void
tf_content_error_printf (TfContent *content,
    guint reason, /* TfFutureContentRemovalReason */
    const gchar *detailed_reason,
    const gchar *message_format, ...)
{
  gchar *message;
  va_list valist;

  va_start (valist, message_format);
  message = g_strdup_vprintf (message_format, valist);
  va_end (valist);

  tf_content_error (content, reason, detailed_reason, message);
  g_free (message);
}
