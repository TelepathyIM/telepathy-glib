
#include "content.h"

#include <gst/farsight/fs-conference-iface.h>


struct _TfContent {
  GObject parent;

};

struct _TfContentClass{
  GObjectClass parent_class;
};


G_DEFINE_TYPE (TfContent, tf_content, G_TYPE_OBJECT);


enum
{
  PROP_FS_CONFERENCE = 1,
  PROP_FS_SESSION
};

enum
{
  SIGNAL_COUNT
};

static void
tf_content_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec);



static void
tf_content_class_init (TfContentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = tf_content_get_property;


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
}


static void
tf_content_init (TfContent *self)
{
}


static void
tf_content_get_property (GObject    *object,
    guint       property_id,
    GValue     *value,
    GParamSpec *pspec)
{
  //TfContent *self = TF_CONTENT (object);

  switch (property_id)
    {
    case PROP_FS_CONFERENCE:
      break;
    case PROP_FS_SESSION:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}
