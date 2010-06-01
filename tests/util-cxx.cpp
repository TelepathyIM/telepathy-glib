/* Test that tp_clear_pointer is syntactically OK in C++
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
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

#include <glib.h>
#include <gio/gio.h>
#include <telepathy-glib/util.h>

int main (int argc, char **argv)
{
  GObject *o;
  GHashTable *h;

  g_type_init ();

  o = (GObject *) g_file_new_for_path ("/");
  tp_clear_object (&o);

  h = g_hash_table_new (NULL, NULL);
  tp_clear_pointer (&h, (GDestroyNotify) g_hash_table_unref);

  h = g_hash_table_new (NULL, NULL);
  tp_clear_boxed (G_TYPE_HASH_TABLE, &h);

  return 0;
}
