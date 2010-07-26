/*
 * manager.h - header for an example connection manager
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TP_TESTS_ECHO_CONNECTION_MANAGER_H__
#define __TP_TESTS_ECHO_CONNECTION_MANAGER_H__

#include <glib-object.h>
#include <telepathy-glib/base-connection-manager.h>

G_BEGIN_DECLS

typedef struct _TpTestsEchoConnectionManager TpTestsEchoConnectionManager;
typedef struct _TpTestsEchoConnectionManagerPrivate
    TpTestsEchoConnectionManagerPrivate;
typedef struct _TpTestsEchoConnectionManagerClass
    TpTestsEchoConnectionManagerClass;
typedef struct _TpTestsEchoConnectionManagerClassPrivate
    TpTestsEchoConnectionManagerClassPrivate;

struct _TpTestsEchoConnectionManagerClass {
    TpBaseConnectionManagerClass parent_class;

    TpTestsEchoConnectionManagerClassPrivate *priv;
};

struct _TpTestsEchoConnectionManager {
    TpBaseConnectionManager parent;

    TpTestsEchoConnectionManagerPrivate *priv;
};

GType tp_tests_echo_connection_manager_get_type (void);

/* TYPE MACROS */
#define TP_TESTS_TYPE_ECHO_CONNECTION_MANAGER \
  (tp_tests_echo_connection_manager_get_type ())
#define TP_TESTS_ECHO_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TESTS_TYPE_ECHO_CONNECTION_MANAGER, \
                              TpTestsEchoConnectionManager))
#define TP_TESTS_ECHO_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TESTS_TYPE_ECHO_CONNECTION_MANAGER, \
                           TpTestsEchoConnectionManagerClass))
#define TP_TESTS_IS_ECHO_CONNECTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TESTS_TYPE_ECHO_CONNECTION_MANAGER))
#define TP_TESTS_IS_ECHO_CONNECTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TESTS_TYPE_ECHO_CONNECTION_MANAGER))
#define TP_TESTS_ECHO_CONNECTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TESTS_TYPE_ECHO_CONNECTION_MANAGER, \
                              TpTestsEchoConnectionManagerClass))

G_END_DECLS

#endif
