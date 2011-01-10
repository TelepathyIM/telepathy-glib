/*
 * echo-channel-manager-conn.h - header for an example conn
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION_H__
#define __TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION_H__

#include "echo-conn.h"

G_BEGIN_DECLS

typedef struct _TpTestsEchoChannelManagerConnection TpTestsEchoChannelManagerConnection;
typedef struct _TpTestsEchoChannelManagerConnectionClass TpTestsEchoChannelManagerConnectionClass;
typedef struct _TpTestsEchoChannelManagerConnectionPrivate TpTestsEchoChannelManagerConnectionPrivate;

struct _TpTestsEchoChannelManagerConnectionClass {
    TpTestsEchoConnectionClass parent_class;
};

struct _TpTestsEchoChannelManagerConnection {
    TpTestsEchoConnection parent;

    TpTestsEchoChannelManagerConnectionPrivate *priv;
};

GType tp_tests_echo_channel_manager_connection_get_type (void);

/* TYPE MACROS */
#define TP_TESTS_TYPE_ECHO_CHANNEL_MANAGER_CONNECTION \
  (tp_tests_echo_channel_manager_connection_get_type ())
#define TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TESTS_TYPE_ECHO_CHANNEL_MANAGER_CONNECTION, \
                              TpTestsEchoChannelManagerConnection))
#define TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TESTS_TYPE_ECHO_CHANNEL_MANAGER_CONNECTION, \
                           TpTestsEchoChannelManagerConnectionClass))
#define TP_TESTS_IS_ECHO_CHANNEL_MANAGER_CONNECTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TESTS_TYPE_ECHO_CHANNEL_MANAGER_CONNECTION))
#define TP_TESTS_IS_ECHO_CHANNEL_MANAGER_CONNECTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TESTS_TYPE_ECHO_CHANNEL_MANAGER_CONNECTION))
#define TP_TESTS_ECHO_CHANNEL_MANAGER_CONNECTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TESTS_TYPE_ECHO_CHANNEL_MANAGER_CONNECTION, \
                              TpTestsEchoChannelManagerConnectionClass))

G_END_DECLS

#endif
