/*
 * my-conn-proxy.c - a simple subclass of TpConnection
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "my-conn-proxy.h"

G_DEFINE_TYPE  (TpTestsMyConnProxy, tp_tests_my_conn_proxy,
    TP_TYPE_CONNECTION)

static void
tp_tests_my_conn_proxy_init (TpTestsMyConnProxy *self)
{
}

static void
tp_tests_my_conn_proxy_class_init (TpTestsMyConnProxyClass *klass)
{
}
