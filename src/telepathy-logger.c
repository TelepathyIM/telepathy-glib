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

#include <telepathy-logger/channel-factory.h>
#include <telepathy-logger/channel-text.h>
#include <telepathy-logger/observer.h>

static GMainLoop *loop = NULL;

int
main(int argc,
    char *argv[])
{
  TplObserver *observer;
  GError *error = NULL;

	g_type_init ();
  tpl_channel_factory_init ();

  g_debug ("Initialising TPL Channel Factory");
  tpl_channel_factory_add ("org.freedesktop.Telepathy.Channel.Type.Text",
      (TplChannelConstructor) tpl_channel_text_new);
  g_debug ("- TplChannelText registred.");

	observer = tpl_observer_new ();
  g_debug ("Registering channel factory into TplObserver");
  tpl_observer_set_channel_factory (observer, tpl_channel_factory_build);

  if (tpl_observer_register_dbus (observer, &error) == FALSE)
    {
      g_debug ("Error during D-Bus registration: %s", error->message);
      return 1;
    }

	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);

  g_object_unref (observer);
  tpl_channel_factory_deinit ();

	return 0;
}
