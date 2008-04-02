/*
 * /dev/null as a text channel
 *
 * Copyright (C) 2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TEST_TEXT_CHANNEL_NULL_H__
#define __TEST_TEXT_CHANNEL_NULL_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection.h>
#include <telepathy-glib/text-mixin.h>

G_BEGIN_DECLS

typedef struct _TestTextChannelNull TestTextChannelNull;
typedef struct _TestTextChannelNullClass TestTextChannelNullClass;
typedef struct _TestTextChannelNullPrivate TestTextChannelNullPrivate;

GType test_text_channel_null_get_type (void);

#define TEST_TYPE_TEXT_CHANNEL_NULL \
  (test_text_channel_null_get_type ())
#define TEST_TEXT_CHANNEL_NULL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_TEXT_CHANNEL_NULL, \
                               TestTextChannelNull))
#define TEST_TEXT_CHANNEL_NULL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_TEXT_CHANNEL_NULL, \
                            TestTextChannelNullClass))
#define TEST_IS_TEXT_CHANNEL_NULL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_TEXT_CHANNEL_NULL))
#define TEST_IS_TEXT_CHANNEL_NULL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_TEXT_CHANNEL_NULL))
#define TEST_TEXT_CHANNEL_NULL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_TEXT_CHANNEL_NULL, \
                              TestTextChannelNullClass))

struct _TestTextChannelNullClass {
    GObjectClass parent_class;
    TpTextMixinClass text_class;
};

struct _TestTextChannelNull {
    GObject parent;
    TpTextMixin text;

    TestTextChannelNullPrivate *priv;
};

G_END_DECLS

#endif /* #ifndef __TEST_TEXT_CHANNEL_NULL_H__ */
