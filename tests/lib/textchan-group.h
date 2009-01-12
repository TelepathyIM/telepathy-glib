/*
 * a stub anonymous MUC
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TEST_TEXT_CHANNEL_GROUP_H__
#define __TEST_TEXT_CHANNEL_GROUP_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/group-mixin.h>
#include <telepathy-glib/text-mixin.h>

G_BEGIN_DECLS

typedef struct _TestTextChannelGroup TestTextChannelGroup;
typedef struct _TestTextChannelGroupClass TestTextChannelGroupClass;
typedef struct _TestTextChannelGroupPrivate TestTextChannelGroupPrivate;

GType test_text_channel_group_get_type (void);

#define TEST_TYPE_TEXT_CHANNEL_GROUP \
  (test_text_channel_group_get_type ())
#define TEST_TEXT_CHANNEL_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_TEXT_CHANNEL_GROUP, \
                               TestTextChannelGroup))
#define TEST_TEXT_CHANNEL_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_TEXT_CHANNEL_GROUP, \
                            TestTextChannelGroupClass))
#define TEST_IS_TEXT_CHANNEL_GROUP(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_TEXT_CHANNEL_GROUP))
#define TEST_IS_TEXT_CHANNEL_GROUP_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_TEXT_CHANNEL_GROUP))
#define TEST_TEXT_CHANNEL_GROUP_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_TEXT_CHANNEL_GROUP, \
                              TestTextChannelGroupClass))

struct _TestTextChannelGroupClass {
    GObjectClass parent_class;

    TpTextMixinClass text_class;
    TpGroupMixinClass group_class;
    TpDBusPropertiesMixinClass dbus_properties_class;
};

struct _TestTextChannelGroup {
    GObject parent;

    TpBaseConnection *conn;

    TpTextMixin text;
    TpGroupMixin group;

    TestTextChannelGroupPrivate *priv;
};

G_END_DECLS

#endif /* #ifndef __TEST_TEXT_CHANNEL_GROUP_H__ */
