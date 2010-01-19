/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include <glib.h>

#include <telepathy-logger/conf.h>

#define ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/gabble/jabber/cosimo_2ealfarano_40collabora_2eco_2euk0"
#define ID "echo@test.collabora.co.uk"

static GMainLoop *loop = NULL;

int
main (int argc, char *argv[])
{

  TplConf *conf;
  GSList *list;
  GSList *newlist = NULL;

  g_type_init ();

  conf = tpl_conf_dup();

  g_message ("enabled: %d\n",
		  tpl_conf_is_globally_enabled(conf, NULL));


  list = tpl_conf_get_accounts_ignorelist(conf, NULL);
  while (list)
  {
    g_message("list elemnet: %s\n",(gchar*)list->data);
    list = g_slist_next(list);
  }
  g_message("FINISH\n");

  /* set */
  tpl_conf_togle_globally_enable(conf, TRUE, NULL);
  newlist = g_slist_append(newlist, "foo");
  newlist = g_slist_append(newlist, "bar");
  tpl_conf_set_accounts_ignorelist(conf, newlist, NULL);

  /* re-read */
  g_message ("enabled: %d\n",
		  tpl_conf_is_globally_enabled(conf, NULL));


  list = tpl_conf_get_accounts_ignorelist(conf, NULL);
  while (list)
  {
    g_message("list elemnet: %s\n",(gchar*)list->data);
    list = g_slist_next(list);
  }
  g_message("FINISH\n");

  g_message("FOUND: %d\n",
    tpl_conf_is_account_ignored(conf, "fooa", NULL));


  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);


  return 0;
}
