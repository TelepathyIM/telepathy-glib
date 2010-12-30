
#include "content.h"
#include "content-priv.h"

#include <gst/farsight/fs-conference-iface.h>

#include "channel.h"


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
  SIGNAL_COUNT
};

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
}


static void
tf_content_init (TfContent *self)
{
}
