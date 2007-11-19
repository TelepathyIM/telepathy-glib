#!/usr/bin/python

# glib-client-gen.py: "I Can't Believe It's Not dbus-binding-tool"
#
# Generate GLib client wrappers from the Telepathy specification.
# The master copy of this program is in the telepathy-glib repository -
# please make any changes there.
#
# Copyright (C) 2006, 2007 Collabora Limited
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import sys
import os.path
import xml.dom.minidom
from getopt import gnu_getopt

from libglibcodegen import Signature, type_to_gtype, cmp_by_name, \
        camelcase_to_lower


NS_TP = "http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0"

class Generator(object):

    def __init__(self, dom, prefix, basename, opts):
        self.dom = dom
        self.__header = []
        self.__body = []

        self.prefix_lc = prefix.lower()
        self.prefix_uc = prefix.upper()
        self.prefix_mc = prefix.replace('_', '')
        self.basename = basename
        self.group = opts.get('--group', None)

    def h(self, s):
        self.__header.append(s)

    def b(self, s):
        self.__body.append(s)

    def do_signal(self, signal):
        # FIXME: implement connect_to_signal
        pass

    def do_method(self, iface, method):
        iface_lc = iface.lower()

        member = method.getAttribute('name')
        member_lc = camelcase_to_lower(member)
        member_uc = member_lc.upper()

        ret_count = 0
        in_args = []
        out_args = []

        for arg in method.getElementsByTagName('arg'):
            name = arg.getAttribute('name')
            direction = arg.getAttribute('direction')
            type = arg.getAttribute('type')
            tp_type = arg.getAttribute('tp:type')

            if direction != 'out':
                name = 'in_%s' % name
            else:
                if not name:
                    name = 'out%u' % ret_count
                    ret_count += 1
                else:
                    name = 'out_%s' % name

            info = type_to_gtype(type)
            if direction != 'out':
                in_args.append((name, info, tp_type))
            else:
                out_args.append((name, info, tp_type))

        # Synchronous stub

        # Example:
        # gboolean tp_cli_properties_interface_block_on_get_properties
        #   (gpointer proxy,
        #       gint timeout_ms,
        #       const GArray *in_properties,
        #       GPtrArray **out0,
        #       GError **error);

        self.h('gboolean %s_%s_block_on_%s (gpointer proxy,'
               % (self.prefix_lc, iface_lc, member_lc))
        self.h('    gint timeout_ms,')

        self.b('/**')
        self.b(' * %s_%s_block_on_%s:' % (self.prefix_lc, iface_lc, member_lc))
        self.b(' * @proxy: A #TpProxy or subclass')
        self.b(' * @timeout_ms: Timeout in milliseconds, or -1 for default')

        for arg in in_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: Used to pass an \'in\' argument (FIXME: docs)'
                   % name)

        for arg in out_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: Used to return an \'out\' argument (FIXME: docs)'
                   % name)

        self.b(' * @error: Used to return errors')
        self.b(' *')
        self.b(' * Auto-generated synchronous call wrapper.')
        self.b(' *')
        self.b(' * Returns: TRUE on success, FALSE and sets @error on error')
        self.b(' */')
        self.b('gboolean\n%s_%s_block_on_%s (gpointer proxy,'
               % (self.prefix_lc, iface_lc, member_lc))
        self.b('    gint timeout_ms,')

        for arg in in_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.h('    %s%s%s,' % (const, ctype, name))
            self.b('    %s%s%s,' % (const, ctype, name))

        for arg in out_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            self.h('    %s*%s,' % (ctype, name))
            self.b('    %s*%s,' % (ctype, name))

        self.h('    GError **error);')
        self.h('')

        self.b('    GError **error)')
        self.b('{')
        self.b('  DBusGProxy *iface = tp_proxy_borrow_interface_by_id (')
        self.b('      TP_PROXY (proxy),')
        self.b('      TP_IFACE_QUARK_%s,' % iface_lc.upper())
        self.b('      error);')
        self.b('')
        self.b('  if (iface == NULL)')
        self.b('    return FALSE;')
        self.b('')
        self.b('  return dbus_g_proxy_call_with_timeout (iface, "%s",'
               % member)
        self.b('      timeout_ms, error,')
        self.b('      /* in arguments */')
        for arg in in_args:
            gtype = arg[1][1]
            name = arg[0]
            self.b('      %s, %s,' % (gtype, name))
        self.b('      G_TYPE_INVALID,')
        self.b('      /* out arguments */')
        for arg in out_args:
            gtype = arg[1][1]
            name = arg[0]
            self.b('      %s, %s,' % (gtype, name))
        self.b('      G_TYPE_INVALID);')
        self.b('}')
        self.b('')

        # Async reply callback type

        # Example:
        # void (*tp_cli_properties_interface_callback_for_get_properties)
        #   (TpProxy *proxy,
        #       const GPtrArray *out0,
        #       const GError *error,
        #       gpointer user_data);

        callback_name = '%s_%s_callback_for_%s' % (self.prefix_lc, iface_lc,
                                                   member_lc)

        self.h('typedef void (*%s) (TpProxy *proxy,' % callback_name)

        for arg in out_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info
            const = pointer and 'const ' or ''

            self.h('    %s%s%s,' % (const, ctype, name))

        self.h('    const GError *error, gpointer user_data);')
        self.h('')

        # Async callback implementation

        callback_impl_name =  '_%s_%s_take_reply_from_%s' % (self.prefix_lc,
                                                             iface_lc,
                                                             member_lc)

        self.b('static void')
        self.b('%s (DBusGProxy *proxy,' % callback_impl_name)
        self.b('    DBusGProxyCall *call,')
        self.b('    gpointer user_data)')
        self.b('{')
        self.b('  TpProxyPendingCall *data = user_data;')
        self.b('  GError *error = NULL;')
        self.b('  %s callback = (%s) (data->callback);' % (callback_name,
                                                           callback_name))

        for arg in out_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            self.b('  %s%s;' % (ctype, name))

        self.b('')
        self.b('  g_assert (data->pending_call == call);')
        self.b('  dbus_g_proxy_end_call (proxy, call, &error,')

        for arg in out_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            self.b('      %s, &%s,' % (gtype, name))

        self.b('      G_TYPE_INVALID);')
        self.b('  callback (data->proxy,')

        for arg in out_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            if gtype == 'G_TYPE_STRV':
                self.b('      (const gchar **) %s,' % name)
            else:
                self.b('      %s,' % name)

        self.b('      error, data->user_data);')
        self.b('')

        for arg in out_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            if not pointer:
                continue
            if marshaller == 'STRING':
                self.b('  g_free (%s);' % name)
            else:
                self.b('  g_boxed_free (%s, %s);' % (gtype, name))

        self.b('}')
        self.b('')

        # Async stub

        # Example:
        # TpProxyPendingCall *tp_cli_properties_interface_call_get_properties
        #   (gpointer proxy,
        #   gint timeout_ms,
        #   const GArray *in_properties,
        #   tp_cli_properties_interface_callback_for_get_properties callback,
        #   gpointer user_data,
        #   GDestroyNotify *destructor);

        # The destructor is called after success, failure or cancellation.
        # The callback is called after success or failure only.

        self.h('TpProxyPendingCall *%s_%s_call_%s (gpointer proxy,'
               % (self.prefix_lc, iface_lc, member_lc))
        self.h('    gint timeout_ms,')

        self.b('TpProxyPendingCall *\n%s_%s_call_%s (gpointer proxy,'
               % (self.prefix_lc, iface_lc, member_lc))
        self.b('    gint timeout_ms,')

        for arg in in_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.h('    %s%s%s,' % (const, ctype, name))
            self.b('    %s%s%s,' % (const, ctype, name))

        self.h('    %s callback,' % callback_name)
        self.h('    gpointer user_data,')
        self.h('    GDestroyNotify destroy);')
        self.h('')

        self.b('    %s callback,' % callback_name)
        self.b('    gpointer user_data,')
        self.b('    GDestroyNotify destroy)')
        self.b('{')
        self.b('  GError *error = NULL;')
        self.b('  DBusGProxy *iface = tp_proxy_borrow_interface_by_id (')
        self.b('      TP_PROXY (proxy),')
        self.b('      TP_IFACE_QUARK_%s,' % iface_lc.upper())
        self.b('      &error);')
        self.b('')
        self.b('  if (iface == NULL)')
        self.b('    {')
        self.b('      if (callback != NULL)')
        self.b('        callback (proxy,')

        for arg in out_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            if pointer:
                self.b('            NULL,')
            else:
                self.b('            0,')

        self.b('            error, user_data);')
        self.b('      return NULL;')
        self.b('    }')
        self.b('')
        self.b('  if (callback == NULL)')
        self.b('    {')
        self.b('      dbus_g_proxy_call_no_reply (iface, "%s",' % member)

        for arg in in_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.b('          %s, %s,' % (gtype, name))

        self.b('          G_TYPE_INVALID);')
        self.b('      return NULL;')
        self.b('    }')
        self.b('  else')
        self.b('    {')
        self.b('      TpProxyPendingCall *data;')
        self.b('')
        self.b('      data = tp_proxy_pending_call_new (proxy,')
        self.b('          G_CALLBACK (callback),')
        self.b('          user_data,')
        self.b('          destroy);')
        self.b('      data->pending_call = '
               'dbus_g_proxy_begin_call_with_timeout (iface,')
        self.b('          "%s",' % member)
        self.b('          %s,' % callback_impl_name)
        self.b('          data,')
        self.b('          tp_proxy_pending_call_free,')
        self.b('          timeout_ms,')

        for arg in in_args:
            name, info, tp_type = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.b('          %s, %s,' % (gtype, name))

        self.b('          G_TYPE_INVALID);')
        self.b('')
        self.b('      return data;')
        self.b('    }')
        self.b('}')
        self.b('')

        self.b('')

        self.h('')

    def do_signal_add(self, signal):
        marshaller_items = []
        gtypes = []

        for i in signal.getElementsByTagName('arg'):
            name = i.getAttribute('name')
            type = i.getAttribute('type')
            info = type_to_gtype(type)
            # type, GType, STRING, is a pointer
            gtypes.append(info[1])

        self.b('  dbus_g_proxy_add_signal (proxy, "%s",'
               % signal.getAttribute('name'))
        for gtype in gtypes:
            self.b('      %s,' % gtype)
        self.b('      G_TYPE_INVALID);')

    def do_interface(self, node):
        ifaces = node.getElementsByTagName('interface')
        assert len(ifaces) == 1
        iface = ifaces[0]
        name = node.getAttribute('name').replace('/', '')

        signals = node.getElementsByTagName('signal')
        methods = node.getElementsByTagName('method')

        self.b('static inline void')
        self.b('%s_add_signals_for_%s (DBusGProxy *proxy)'
                % (self.prefix_lc, name.lower()))
        self.b('{')

        for signal in signals:
            self.do_signal_add(signal)

        self.b('}')
        self.b('')
        self.b('')

        for signal in signals:
            self.do_signal(signal)

        for method in methods:
            self.do_method(name, method)

    def __call__(self):

        self.h('#include <dbus/dbus-glib.h>')
        self.h('#include <telepathy-glib/interfaces.h>')
        self.h('')
        self.h('G_BEGIN_DECLS')

        self.b('#define TP_PROXY_IN_CLI_IMPLEMENTATION')
        self.b('#include "telepathy-glib/proxy-internal.h"')
        self.b('#include "%s.h"' % self.basename)
        self.b('')
        self.b('')

        ifaces = self.dom.getElementsByTagName('node')
        ifaces.sort(cmp_by_name)

        for iface in ifaces:
            self.do_interface(iface)

        if self.group is not None:

            self.h('void %s_%s_add_signals (TpProxy *self, guint quark,'
                    % (self.prefix_lc, self.group))
            self.h('    DBusGProxy *proxy, gpointer unused);')
            self.h('')

            self.b('void')
            self.b('%s_%s_add_signals (TpProxy *self,'
                    % (self.prefix_lc, self.group))
            self.b('    guint quark,')
            self.b('    DBusGProxy *proxy,')
            self.b('    gpointer unused)')

            self.b('{')

            for iface in ifaces:
                name = iface.getAttribute('name').replace('/', '').lower()
                self.b('  if (quark == TP_IFACE_QUARK_%s)' % name.upper())
                self.b('    %s_add_signals_for_%s (proxy);'
                       % (self.prefix_lc, name))

            self.b('}')
            self.b('')

        self.h('G_END_DECLS')
        self.h('')

        open(self.basename + '.h', 'w').write('\n'.join(self.__header))
        open(self.basename + '.c', 'w').write('\n'.join(self.__body))


def types_to_gtypes(types):
    return [type_to_gtype(t)[1] for t in types]


if __name__ == '__main__':
    options, argv = gnu_getopt(sys.argv[1:], '',
                               ['group='])

    opts = {}

    for option, value in options:
        opts[option] = value

    dom = xml.dom.minidom.parse(argv[0])

    Generator(dom, argv[1], argv[2], opts)()
