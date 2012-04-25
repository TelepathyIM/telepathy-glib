/*
 * future-account.c - object for a currently non-existent account to create
 *
 * Copyright © 2012 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "telepathy-glib/future-account.h"

#define DEBUG_FLAG TP_DEBUG_ACCOUNTS
#include "telepathy-glib/debug-internal.h"

/**
 * SECTION:future-account
 * @title: TpFutureAccount
 * @short_description: object for a currently non-existent account in
 *   order to create easily without knowing speaking fluent D-Bus
 * @see_also: #TpAccountManager
 *
 * TODO
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpFutureAccount:
 *
 * TODO
 *
 * Since: 0.UNRELEASED
 */

/**
 * TpFutureAccountClass:
 *
 * The class of a #TpFutureAccount.
 */

struct _TpFutureAccountPrivate {
  gboolean dispose_has_run;
};

G_DEFINE_TYPE (TpFutureAccount, tp_future_account, G_TYPE_OBJECT)

/* signals */
enum {
  LAST_SIGNAL
};

/*static guint signals[LAST_SIGNAL];*/

/* properties */
enum {
  N_PROPS
};

static void
tp_future_account_init (TpFutureAccount *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_FUTURE_ACCOUNT,
      TpFutureAccountPrivate);
}

static void
tp_future_account_dispose (GObject *object)
{
  TpFutureAccount *self = TP_FUTURE_ACCOUNT (object);
  TpFutureAccountPrivate *priv = self->priv;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  /* release any references held by the object here */

  if (G_OBJECT_CLASS (tp_future_account_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (tp_future_account_parent_class)->dispose (object);
}

static void
tp_future_account_finalize (GObject *object)
{
  /* free any data held directly by the object here */

  if (G_OBJECT_CLASS (tp_future_account_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (tp_future_account_parent_class)->finalize (object);
}

static void
tp_future_account_class_init (TpFutureAccountClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpFutureAccountPrivate));

  object_class->dispose = tp_future_account_dispose;
  object_class->finalize = tp_future_account_finalize;
}

/**
 * tp_future_account_new:
 *
 * Convenience function to create a new future account object.
 *
 * Returns: a new reference to a future account object, or %NULL
 */
TpFutureAccount *
tp_future_account_new (void)
{
  return g_object_new (TP_TYPE_FUTURE_ACCOUNT, NULL);
}

