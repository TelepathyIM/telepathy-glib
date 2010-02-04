/*
 * main.c - entry point for telepathy-gabble-debug used by tests
 * Copyright (C) 2008 Collabora Ltd.
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

#include <stdlib.h>

#include <dbus/dbus.h>

#include <telepathy-logger/observer.h>
#include <telepathy-logger/channel-factory.h>
#include <telepathy-logger/channel-text.h>


static TplObserver *
tpl_init(void)
{
  TplObserver *observer;

	g_type_init ();
  tpl_channel_factory_init ();

  tpl_channel_factory_add ("org.freedesktop.Telepathy.Channel.Type.Text",
      (TplChannelConstructor) tpl_channel_text_new);

	observer = tpl_observer_new ();
#if 0
  if (tpl_observer_register_dbus (observer, &error) == FALSE)
    {
      g_debug ("Error during D-Bus registration: %s", error->message);
      return 1;
    }
#endif

  return observer;
}


int
main (int argc,
      char **argv)
{
  int ret = 1;
  TplObserver *observer;

  observer = tpl_init ();

  dbus_shutdown ();

  return ret;
}
