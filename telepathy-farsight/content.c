
#include "content.h"
#include "content-priv.h"

#include <gst/farsight/fs-conference-iface.h>

#include "channel.h"
#include "tf-signals-marshal.h"


G_DEFINE_ABSTRACT_TYPE (TfContent, tf_content, G_TYPE_OBJECT);


enum
{
  PROP_TF_CHANNEL = 1,
  PROP_FS_CONFERENCE,
  PROP_FS_SESSION,
  PROP_SINK_PAD
};

enum
{
  SIGNAL_START_SENDING,
  SIGNAL_STOP_SENDING,
  SIGNAL_COUNT
};

static guint signals[SIGNAL_COUNT] = {0};

static void
tf_content_class_init (TfContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_object_class_install_property (object_class, PROP_TF_CHANNEL,
      g_param_spec_object ("tf-channel",
          "Parent TfChannel object ",
          "The Telepathy-Farsight Channel for this object",
          TF_TYPE_CHANNEL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_FS_SESSION,
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

  /**
   * TfContent::start-sending
   * @content: the #TfContent
   * @direction: The direction for which this resource is requested
   *  (as a #TpMediaDirection
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


  signals[SIGNAL_STOP_SENDING] =
      g_signal_new ("stop-sending",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          0, NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
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

  if (self->sending)
    return TRUE;

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

  self->sending = sending_success;

  return sending_success;
}

void
_tf_content_stop_sending (TfContent *self)
{
  g_signal_emit (self, signals[SIGNAL_STOP_SENDING], 0);

  self->sending = FALSE;
}
