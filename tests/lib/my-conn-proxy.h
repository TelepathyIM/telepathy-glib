/*
 * my-conn-proxy.h - header for a simple subclass of TpConnection
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#ifndef __TP_TESTS_MY_CONN_PROXY_H__
#define __TP_TESTS_MY_CONN_PROXY_H__

#include <glib-object.h>
#include <telepathy-glib/telepathy-glib.h>


G_BEGIN_DECLS

typedef struct _TpTestsMyConnProxy TpTestsMyConnProxy;
typedef struct _TpTestsMyConnProxyClass TpTestsMyConnProxyClass;
typedef struct _TpTestsMyConnProxyPrivate TpTestsMyConnProxyPrivate;

struct _TpTestsMyConnProxyClass {
    TpConnectionClass parent_class;
};

struct _TpTestsMyConnProxy {
    TpConnection parent;
};

GType tp_tests_my_conn_proxy_get_type (void);

/* TYPE MACROS */
#define TP_TESTS_TYPE_MY_CONN_PROXY \
  (tp_tests_my_conn_proxy_get_type ())
#define TP_TESTS_MY_CONN_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TESTS_TYPE_MY_CONN_PROXY, \
                              TpTestsMyConnProxy))
#define TP_TESTS_MY_CONN_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TESTS_TYPE_MY_CONN_PROXY, \
                           TpTestsMyConnProxyClass))
#define TP_TESTS_SIMPLE_IS_MY_CONN_PROXY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TESTS_TYPE_MY_CONN_PROXY))
#define TP_TESTS_SIMPLE_IS_MY_CONN_PROXY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TESTS_TYPE_MY_CONN_PROXY))
#define TP_TESTS_MY_CONN_PROXY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TESTS_TYPE_MY_CONN_PROXY, \
                              TpTestsMyConnProxyClass))

/* Core feature */
#define TP_TESTS_MY_CONN_PROXY_FEATURE_CORE \
  (tp_tests_my_conn_proxy_get_feature_quark_core ())
GQuark tp_tests_my_conn_proxy_get_feature_quark_core (void) G_GNUC_CONST;

/* No depends */
#define TP_TESTS_MY_CONN_PROXY_FEATURE_A \
  (tp_tests_my_conn_proxy_get_feature_quark_a ())
GQuark tp_tests_my_conn_proxy_get_feature_quark_a (void) G_GNUC_CONST;

/* Depends on A */
#define TP_TESTS_MY_CONN_PROXY_FEATURE_B \
  (tp_tests_my_conn_proxy_get_feature_quark_b ())
GQuark tp_tests_my_conn_proxy_get_feature_quark_b (void) G_GNUC_CONST;

G_END_DECLS

#endif /* #ifndef __TP_TESTS_MY_CONN_PROXY_H__ */
