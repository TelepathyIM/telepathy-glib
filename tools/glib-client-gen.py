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
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import sys
import os.path
import xml.dom.minidom
from getopt import gnu_getopt

from libglibcodegen import Signature, type_to_gtype, cmp_by_name, \
        camelcase_to_lower, get_docstring


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
        self.proxy_cls = opts.get('--subclass', 'TpProxy') + ' *'
        self.proxy_arg = opts.get('--subclass', 'void') + ' *'
        self.proxy_assert = opts.get('--subclass-assert', 'TP_IS_PROXY')
        self.proxy_doc = ('A #%s or subclass'
            % opts.get('--subclass', 'TpProxy'))
        if self.proxy_arg == 'void *':
            self.proxy_arg = 'gpointer '

    def h(self, s):
        self.__header.append(s)

    def b(self, s):
        self.__body.append(s)

    def do_signal(self, iface, signal):
        iface_lc = iface.lower()

        member = signal.getAttribute('name')
        member_lc = camelcase_to_lower(member)
        member_uc = member_lc.upper()

        arg_count = 0
        args = []
        out_args = []

        for arg in signal.getElementsByTagName('arg'):
            name = arg.getAttribute('name')
            type = arg.getAttribute('type')
            tp_type = arg.getAttribute('tp:type')

            if not name:
                name = 'arg%u' % arg_count
                arg_count += 1
            else:
                name = 'arg_%s' % name

            info = type_to_gtype(type)
            args.append((name, info, tp_type, arg))

        callback_name = ('%s_%s_signal_callback_%s'
                         % (self.prefix_lc, iface_lc, member_lc))
        collect_name = ('_%s_%s_collect_args_of_%s'
                        % (self.prefix_lc, iface_lc, member_lc))
        invoke_name = ('_%s_%s_invoke_callback_for_%s'
                       % (self.prefix_lc, iface_lc, member_lc))

        # Example:
        #
        # typedef void (*tp_cli_connection_signal_callback_new_channel)
        #   (TpConnection *proxy, const gchar *arg_object_path,
        #   const gchar *arg_channel_type, guint arg_handle_type,
        #   guint arg_handle, gboolean arg_suppress_handler,
        #   gpointer user_data, GObject *weak_object);

        self.b('/**')
        self.b(' * %s:' % callback_name)
        self.b(' * @proxy: The proxy on which %s_%s_connect_to_%s ()'
               % (self.prefix_lc, iface_lc, member_lc))
        self.b(' *  was called')

        for arg in args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: <![CDATA[%s]]>' % (name,
                get_docstring(elt) or '(Undocumented)'))

        self.b(' * @user_data: User-supplied data')
        self.b(' * @weak_object: User-supplied weakly referenced object')
        self.b(' *')
        self.b(' * Represents the signature of a callback for the signal %s.'
               % member)
        self.b(' */')
        self.h('typedef void (*%s) (%sproxy,'
               % (callback_name, self.proxy_cls))

        for arg in args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.h('    %s%s%s,' % (const, ctype, name))

        self.h('    gpointer user_data, GObject *weak_object);')

        if args:
            self.b('static void')
            self.b('%s (DBusGProxy *proxy,' % collect_name)

            for arg in args:
                name, info, tp_type, elt = arg
                ctype, gtype, marshaller, pointer = info

                const = pointer and 'const ' or ''

                self.b('    %s%s%s,' % (const, ctype, name))

            self.b('    TpProxySignalConnection *sc)')
            self.b('{')
            self.b('  GValueArray *args = g_value_array_new (%d);' % len(args))
            self.b('  GValue blank = { 0 };')
            self.b('  guint i;')
            self.b('')
            self.b('  g_value_init (&blank, G_TYPE_INT);')
            self.b('')
            self.b('  for (i = 0; i < %d; i++)' % len(args))
            self.b('    g_value_array_append (args, &blank);')
            self.b('')

            for i, arg in enumerate(args):
                name, info, tp_type, elt = arg
                ctype, gtype, marshaller, pointer = info

                self.b('  g_value_unset (args->values + %d);' % i)
                self.b('  g_value_init (args->values + %d, %s);' % (i, gtype))

                if gtype == 'G_TYPE_STRING':
                    self.b('  g_value_set_string (args->values + %d, %s);'
                           % (i, name))
                elif marshaller == 'BOXED':
                    self.b('  g_value_set_boxed (args->values + %d, %s);'
                           % (i, name))
                elif gtype == 'G_TYPE_UCHAR':
                    self.b('  g_value_set_uchar (args->values + %d, %s);'
                           % (i, name))
                elif gtype == 'G_TYPE_BOOLEAN':
                    self.b('  g_value_set_boolean (args->values + %d, %s);'
                           % (i, name))
                elif gtype == 'G_TYPE_INT':
                    self.b('  g_value_set_int (args->values + %d, %s);'
                           % (i, name))
                elif gtype == 'G_TYPE_UINT':
                    self.b('  g_value_set_uint (args->values + %d, %s);'
                           % (i, name))
                elif gtype == 'G_TYPE_INT64':
                    self.b('  g_value_set_int (args->values + %d, %s);'
                           % (i, name))
                elif gtype == 'G_TYPE_UINT64':
                    self.b('  g_value_set_uint (args->values + %d, %s);'
                           % (i, name))
                elif gtype == 'G_TYPE_DOUBLE':
                    self.b('  g_value_set_double (args->values + %d, %s);'
                           % (i, name))
                else:
                    assert False, ("Don't know how to put %s in a GValue"
                                   % gtype)
                self.b('')

            self.b('  tp_proxy_signal_connection_v0_take_results (sc, args);')
            self.b('}')

        self.b('static void')
        self.b('%s (TpProxy *tpproxy,' % invoke_name)
        self.b('    GError *error,')
        self.b('    GValueArray *args,')
        self.b('    GCallback generic_callback,')
        self.b('    gpointer user_data,')
        self.b('    GObject *weak_object)')
        self.b('{')
        self.b('  %s callback =' % callback_name)
        self.b('      (%s) generic_callback;' % callback_name)
        self.b('')
        self.b('  if (callback != NULL)')
        self.b('    callback (g_object_ref (tpproxy),')

        # FIXME: factor out into a function
        for i, arg in enumerate(args):
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            if marshaller == 'BOXED':
                self.b('      g_value_get_boxed (args->values + %d),' % i)
            elif gtype == 'G_TYPE_STRING':
                self.b('      g_value_get_string (args->values + %d),' % i)
            elif gtype == 'G_TYPE_UCHAR':
                self.b('      g_value_get_uchar (args->values + %d),' % i)
            elif gtype == 'G_TYPE_BOOLEAN':
                self.b('      g_value_get_boolean (args->values + %d),' % i)
            elif gtype == 'G_TYPE_UINT':
                self.b('      g_value_get_uint (args->values + %d),' % i)
            elif gtype == 'G_TYPE_INT':
                self.b('      g_value_get_int (args->values + %d),' % i)
            elif gtype == 'G_TYPE_UINT64':
                self.b('      g_value_get_uint64 (args->values + %d),' % i)
            elif gtype == 'G_TYPE_INT64':
                self.b('      g_value_get_int64 (args->values + %d),' % i)
            elif gtype == 'G_TYPE_DOUBLE':
                self.b('      g_value_get_double (args->values + %d),' % i)
            else:
                assert False, "Don't know how to get %s from a GValue" % gtype

        self.b('      user_data,')
        self.b('      weak_object);')
        self.b('')

        if len(args) > 0:
            self.b('  g_value_array_free (args);')
        else:
            self.b('  if (args != NULL)')
            self.b('    g_value_array_free (args);')
            self.b('')

        self.b('  g_object_unref (tpproxy);')
        self.b('}')

        # Example:
        #
        # TpProxySignalConnection *
        #   tp_cli_connection_connect_to_new_channel
        #   (TpConnection *proxy,
        #   tp_cli_connection_signal_callback_new_channel callback,
        #   gpointer user_data,
        #   GDestroyNotify destroy);
        #
        # destroy is invoked when the signal becomes disconnected. This
        # is either because the signal has been disconnected explicitly
        # by the user, because the TpProxy has become invalid and
        # emitted the 'destroyed' signal, or because the weakly referenced
        # object has gone away.

        self.b('/**')
        self.b(' * %s_%s_connect_to_%s:'
               % (self.prefix_lc, iface_lc, member_lc))
        self.b(' * @proxy: %s' % self.proxy_doc)
        self.b(' * @callback: Callback to be called when the signal is')
        self.b(' *   received')
        self.b(' * @user_data: User-supplied data for the callback')
        self.b(' * @destroy: Destructor for the user-supplied data')
        self.b(' * @weak_object: A #GObject which will be weakly referenced; ')
        self.b(' *   if it is destroyed, this callback will automatically be')
        self.b(' *   disconnected')
        self.b(' *')
        self.b(' * Connect a handler to the signal %s.' % member)
        self.b(' *')
        self.b(' * <![CDATA[%s]]>'
                % (get_docstring(signal) or '(Undocumented)'))
        self.b(' *')
        self.b(' * Returns: a #TpProxySignalConnection containing all of the')
        self.b(' * above, which can be used to disconnect the signal')
        self.b(' */')
        self.h('TpProxySignalConnection *%s_%s_connect_to_%s (%sproxy,'
               % (self.prefix_lc, iface_lc, member_lc, self.proxy_arg))
        self.h('    %s callback,' % callback_name)
        self.h('    gpointer user_data,')
        self.h('    GDestroyNotify destroy,')
        self.h('    GObject *weak_object);')

        self.b('TpProxySignalConnection *')
        self.b('%s_%s_connect_to_%s (%sproxy,'
               % (self.prefix_lc, iface_lc, member_lc, self.proxy_arg))
        self.b('    %s callback,' % callback_name)
        self.b('    gpointer user_data,')
        self.b('    GDestroyNotify destroy,')
        self.b('    GObject *weak_object)')
        self.b('{')
        self.b('  GType expected_types[%d] = {' % (len(args) + 1))

        for arg in args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b('      %s,' % gtype)

        self.b('      G_TYPE_INVALID };')
        self.b('')
        self.b('  g_return_val_if_fail (%s (proxy), NULL);'
               % self.proxy_assert)
        self.b('  g_return_val_if_fail (callback != NULL, NULL);')
        self.b('')
        self.b('  return tp_proxy_signal_connection_v0_new ((TpProxy *) proxy,')
        self.b('      TP_IFACE_QUARK_%s, \"%s\",' % (iface_lc.upper(), member))
        self.b('      expected_types,')

        if args:
            self.b('      G_CALLBACK (%s),' % collect_name)
        else:
            self.b('      NULL, /* no args => no collector function */')

        self.b('      %s,' % invoke_name)
        self.b('      G_CALLBACK (callback), user_data, destroy,')
        self.b('      weak_object);')
        self.b('}')
        self.b('')

        self.h('')

    def do_method(self, iface, method):
        iface_lc = iface.lower()

        member = method.getAttribute('name')
        member_lc = camelcase_to_lower(member)
        member_uc = member_lc.upper()

        in_count = 0
        ret_count = 0
        in_args = []
        out_args = []

        for arg in method.getElementsByTagName('arg'):
            name = arg.getAttribute('name')
            direction = arg.getAttribute('direction')
            type = arg.getAttribute('type')
            tp_type = arg.getAttribute('tp:type')

            if direction != 'out':
                if not name:
                    name = 'in%u' % in_count
                    in_count += 1
                else:
                    name = 'in_%s' % name
            else:
                if not name:
                    name = 'out%u' % ret_count
                    ret_count += 1
                else:
                    name = 'out_%s' % name

            info = type_to_gtype(type)
            if direction != 'out':
                in_args.append((name, info, tp_type, arg))
            else:
                out_args.append((name, info, tp_type, arg))

        # Synchronous stub

        # Example:
        # gboolean tp_cli_properties_interface_block_on_get_properties
        #   (gpointer proxy,
        #       gint timeout_ms,
        #       const GArray *in_properties,
        #       GPtrArray **out0,
        #       GError **error);

        self.h('gboolean %s_%s_block_on_%s (%sproxy,'
               % (self.prefix_lc, iface_lc, member_lc, self.proxy_arg))
        self.h('    gint timeout_ms,')

        self.b('/**')
        self.b(' * %s_%s_block_on_%s:' % (self.prefix_lc, iface_lc, member_lc))
        self.b(' * @proxy: %s' % self.proxy_doc)
        self.b(' * @timeout_ms: Timeout in milliseconds, or -1 for default')

        for arg in in_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: Used to pass an \'in\' argument: <![CDATA[%s]]>'
                   % (name, get_docstring(elt) or '(Undocumented)'))

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: Used to return an \'out\' argument if %%TRUE is '
                   'returned: <![CDATA[%s]]>'
                   % (name, get_docstring(elt) or '(Undocumented)'))

        self.b(' * @error: Used to return errors if %FALSE is returned')
        self.b(' *')
        self.b(' * Call the method %s and block, without' % member)
        self.b(' * re-entering the main loop, until it returns.')
        self.b(' *')
        self.b(' * <![CDATA[%s]]>'
                % (get_docstring(method) or '(Undocumented)'))
        self.b(' *')
        self.b(' * Returns: TRUE on success, FALSE and sets @error on error')
        self.b(' */')
        self.b('gboolean\n%s_%s_block_on_%s (%sproxy,'
               % (self.prefix_lc, iface_lc, member_lc, self.proxy_arg))
        self.b('    gint timeout_ms,')

        for arg in in_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.h('    %s%s%s,' % (const, ctype, name))
            self.b('    %s%s%s,' % (const, ctype, name))

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.h('    %s*%s,' % (ctype, name))
            self.b('    %s*%s,' % (ctype, name))

        self.h('    GError **error);')
        self.h('')

        self.b('    GError **error)')
        self.b('{')
        self.b('  DBusGProxy *iface;')
        self.b('')
        self.b('  g_return_val_if_fail (%s (proxy), FALSE);'
               % self.proxy_assert)
        self.b('')
        self.b('  iface = tp_proxy_borrow_interface_by_id (')
        self.b('      (TpProxy *) proxy,')
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
        #       gpointer user_data,
        #       GObject *weak_object);

        self.b('/**')
        self.b(' * %s_%s_callback_for_%s:'
               % (self.prefix_lc, iface_lc, member_lc))
        self.b(' * @proxy: the proxy on which the call was made')

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: Used to return an \'out\' argument if @error is '
                   '%%NULL: <![CDATA[%s]]>'
                   % (name, get_docstring(elt) or '(Undocumented)'))

        self.b(' * @error: %NULL on success, or an error on failure')
        self.b(' * @user_data: user-supplied data')
        self.b(' * @weak_object: user-supplied object')
        self.b(' *')
        self.b(' * Signature of the callback called when a %s method call'
               % member)
        self.b(' * succeeds or fails.')
        self.b(' */')

        callback_name = '%s_%s_callback_for_%s' % (self.prefix_lc, iface_lc,
                                                   member_lc)

        self.h('typedef void (*%s) (%sproxy,'
               % (callback_name, self.proxy_cls))

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info
            const = pointer and 'const ' or ''

            self.h('    %s%s%s,' % (const, ctype, name))

        self.h('    const GError *error, gpointer user_data,')
        self.h('    GObject *weak_object);')
        self.h('')

        # Async callback implementation

        invoke_callback = '_%s_%s_invoke_callback_%s' % (self.prefix_lc,
                                                         iface_lc,
                                                         member_lc)

        collect_callback = '_%s_%s_collect_callback_%s' % (self.prefix_lc,
                                                           iface_lc,
                                                           member_lc)

        # The callback called by dbus-glib; this ends the call and collects
        # the results into a GValueArray.
        self.b('static void')
        self.b('%s (DBusGProxy *proxy,' % collect_callback)
        self.b('    DBusGProxyCall *call,')
        self.b('    gpointer user_data)')
        self.b('{')
        self.b('  GError *error = NULL;')

        if len(out_args) > 0:
            self.b('  GValueArray *args;')
            self.b('  GValue blank = { 0 };')
            self.b('  guint i;')

            for arg in out_args:
                name, info, tp_type, elt = arg
                ctype, gtype, marshaller, pointer = info

                self.b('  %s%s;' % (ctype, name))

        self.b('')
        self.b('  dbus_g_proxy_end_call (proxy, call, &error,')

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b('      %s, &%s,' % (gtype, name))

        self.b('      G_TYPE_INVALID);')

        if len(out_args) == 0:
            self.b('  tp_proxy_pending_call_v0_take_results (user_data, error,'
                   'NULL);')
        else:
            self.b('')
            self.b('  if (error != NULL)')
            self.b('    {')
            self.b('      tp_proxy_pending_call_v0_take_results (user_data, error,')
            self.b('          NULL);')
            self.b('      return;')
            self.b('    }')
            self.b('')
            self.b('  args = g_value_array_new (%d);' % len(out_args))
            self.b('  g_value_init (&blank, G_TYPE_INT);')
            self.b('')
            self.b('  for (i = 0; i < %d; i++)' % len(out_args))
            self.b('    g_value_array_append (args, &blank);')

            for i, arg in enumerate(out_args):
                name, info, tp_type, elt = arg
                ctype, gtype, marshaller, pointer = info

                self.b('')
                self.b('  g_value_unset (args->values + %d);' % i)
                self.b('  g_value_init (args->values + %d, %s);' % (i, gtype))

                if gtype == 'G_TYPE_STRING':
                    self.b('  g_value_take_string (args->values + %d, %s);'
                           % (i, name))
                elif marshaller == 'BOXED':
                    self.b('  g_value_take_boxed (args->values + %d, %s);'
                            % (i, name))
                elif gtype == 'G_TYPE_UCHAR':
                    self.b('  g_value_set_uchar (args->values + %d, %s);'
                            % (i, name))
                elif gtype == 'G_TYPE_BOOLEAN':
                    self.b('  g_value_set_boolean (args->values + %d, %s);'
                            % (i, name))
                elif gtype == 'G_TYPE_INT':
                    self.b('  g_value_set_int (args->values + %d, %s);'
                            % (i, name))
                elif gtype == 'G_TYPE_UINT':
                    self.b('  g_value_set_uint (args->values + %d, %s);'
                            % (i, name))
                elif gtype == 'G_TYPE_INT64':
                    self.b('  g_value_set_int (args->values + %d, %s);'
                            % (i, name))
                elif gtype == 'G_TYPE_UINT64':
                    self.b('  g_value_set_uint (args->values + %d, %s);'
                            % (i, name))
                elif gtype == 'G_TYPE_DOUBLE':
                    self.b('  g_value_set_double (args->values + %d, %s);'
                            % (i, name))
                else:
                    assert False, ("Don't know how to put %s in a GValue"
                                   % gtype)

            self.b('  tp_proxy_pending_call_v0_take_results (user_data, '
                   'NULL, args);')

        self.b('}')

        self.b('static void')
        self.b('%s (TpProxy *self,' % invoke_callback)
        self.b('    GError *error,')
        self.b('    GValueArray *args,')
        self.b('    GCallback generic_callback,')
        self.b('    gpointer user_data,')
        self.b('    GObject *weak_object)')
        self.b('{')
        self.b('  %s callback = (%s) generic_callback;'
               % (callback_name, callback_name))
        self.b('')
        self.b('  if (error != NULL)')
        self.b('    {')
        self.b('      callback ((%s) self,' % self.proxy_cls)

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            if marshaller == 'BOXED' or pointer:
                self.b('          NULL,')
            elif gtype == 'G_TYPE_DOUBLE':
                self.b('          0.0,')
            else:
                self.b('          0,')

        self.b('          error, user_data, weak_object);')
        self.b('      g_error_free (error);')
        self.b('      return;')
        self.b('    }')

        self.b('  callback ((%s) self,' % self.proxy_cls)

        # FIXME: factor out into a function
        for i, arg in enumerate(out_args):
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            if marshaller == 'BOXED':
                self.b('      g_value_get_boxed (args->values + %d),' % i)
            elif gtype == 'G_TYPE_STRING':
                self.b('      g_value_get_string (args->values + %d),' % i)
            elif gtype == 'G_TYPE_UCHAR':
                self.b('      g_value_get_uchar (args->values + %d),' % i)
            elif gtype == 'G_TYPE_BOOLEAN':
                self.b('      g_value_get_boolean (args->values + %d),' % i)
            elif gtype == 'G_TYPE_UINT':
                self.b('      g_value_get_uint (args->values + %d),' % i)
            elif gtype == 'G_TYPE_INT':
                self.b('      g_value_get_int (args->values + %d),' % i)
            elif gtype == 'G_TYPE_UINT64':
                self.b('      g_value_get_uint64 (args->values + %d),' % i)
            elif gtype == 'G_TYPE_INT64':
                self.b('      g_value_get_int64 (args->values + %d),' % i)
            elif gtype == 'G_TYPE_DOUBLE':
                self.b('      g_value_get_double (args->values + %d),' % i)
            else:
                assert False, "Don't know how to get %s from a GValue" % gtype

        self.b('      error, user_data, weak_object);')
        self.b('')

        if len(out_args) > 0:
            self.b('  g_value_array_free (args);')
        else:
            self.b('  if (args != NULL)')
            self.b('    g_value_array_free (args);')

        self.b('}')
        self.b('')

        # Async stub

        # Example:
        # TpProxyPendingCall *
        #   tp_cli_properties_interface_call_get_properties
        #   (gpointer proxy,
        #   gint timeout_ms,
        #   const GArray *in_properties,
        #   tp_cli_properties_interface_callback_for_get_properties callback,
        #   gpointer user_data,
        #   GDestroyNotify *destructor);

        self.h('TpProxyPendingCall *%s_%s_call_%s (%sproxy,'
               % (self.prefix_lc, iface_lc, member_lc, self.proxy_arg))
        self.h('    gint timeout_ms,')

        self.b('/**')
        self.b(' * %s_%s_call_%s:'
               % (self.prefix_lc, iface_lc, member_lc))
        self.b(' * @proxy: the #TpProxy')
        self.b(' * @timeout_ms: the timeout in milliseconds, or -1 to use the')
        self.b(' *   default')

        for arg in in_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: Used to pass an \'in\' argument: <![CDATA[%s]]>'
                   % (name, get_docstring(elt) or '(Undocumented)'))

        self.b(' * @callback: called when the method call succeeds or fails')
        self.b(' * @user_data: user-supplied data passed to the callback')
        self.b(' * @destroy: called with the user_data as argument, after the')
        self.b(' *   call has succeeded, failed or been cancelled')
        self.b(' * @weak_object: A #GObject which will be weakly referenced; ')
        self.b(' *   if it is destroyed, this callback will automatically be')
        self.b(' *   disconnected')
        self.b(' *')
        self.b(' * Start a %s method call.' % member)
        self.b(' *')
        self.b(' * <![CDATA[%s]]>'
                % (get_docstring(method) or '(Undocumented)'))
        self.b(' *')
        self.b(' * Returns: a #TpProxyPendingCall representing the call in')
        self.b(' *  progress. It is borrowed from the object, and will become')
        self.b(' *  invalid when the callback is called, the call is')
        self.b(' *  cancelled or the #TpProxy becomes invalid.')
        self.b(' */')
        self.b('TpProxyPendingCall *\n%s_%s_call_%s (%sproxy,'
               % (self.prefix_lc, iface_lc, member_lc, self.proxy_arg))
        self.b('    gint timeout_ms,')

        for arg in in_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.h('    %s%s%s,' % (const, ctype, name))
            self.b('    %s%s%s,' % (const, ctype, name))

        self.h('    %s callback,' % callback_name)
        self.h('    gpointer user_data,')
        self.h('    GDestroyNotify destroy,')
        self.h('    GObject *weak_object);')
        self.h('')

        self.b('    %s callback,' % callback_name)
        self.b('    gpointer user_data,')
        self.b('    GDestroyNotify destroy,')
        self.b('    GObject *weak_object)')
        self.b('{')
        self.b('  GError *error = NULL;')
        self.b('  GQuark interface = TP_IFACE_QUARK_%s;' % iface_lc.upper())
        self.b('  DBusGProxy *iface;')
        self.b('')
        self.b('  g_return_val_if_fail (%s (proxy), NULL);'
               % self.proxy_assert)
        self.b('')
        self.b('  iface = tp_proxy_borrow_interface_by_id (')
        self.b('      (TpProxy *) proxy,')
        self.b('      interface, &error);')
        self.b('')
        self.b('  if (iface == NULL)')
        self.b('    {')
        self.b('      if (callback != NULL)')
        self.b('        callback (proxy,')

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            if pointer:
                self.b('            NULL,')
            else:
                self.b('            0,')

        self.b('            error, user_data, weak_object);')
        self.b('      return NULL;')
        self.b('    }')
        self.b('')
        self.b('  if (callback == NULL)')
        self.b('    {')
        self.b('      dbus_g_proxy_call_no_reply (iface, "%s",' % member)

        for arg in in_args:
            name, info, tp_type, elt = arg
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
        self.b('      data = tp_proxy_pending_call_v0_new ((TpProxy *) proxy,')
        self.b('          interface, "%s",' % member)
        self.b('          %s,' % invoke_callback)
        self.b('          G_CALLBACK (callback), user_data, destroy,')
        self.b('          weak_object);')
        self.b('      tp_proxy_pending_call_v0_take_pending_call (data,')
        self.b('          dbus_g_proxy_begin_call_with_timeout (iface,')
        self.b('              "%s",' % member)
        self.b('              %s,' % collect_callback)
        self.b('              data,')
        self.b('              tp_proxy_pending_call_v0_completed,')
        self.b('              timeout_ms,')

        for arg in in_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.b('              %s, %s,' % (gtype, name))

        self.b('              G_TYPE_INVALID));')
        self.b('')
        self.b('      return data;')
        self.b('    }')
        self.b('}')
        self.b('')

        # Reentrant blocking calls
        # Example:
        # gboolean tp_cli_properties_interface_run_get_properties
        #   (gpointer proxy,
        #       gint timeout_ms,
        #       const GArray *in_properties,
        #       GPtrArray **out0,
        #       GError **error);

        self.b('typedef struct {')
        self.b('    GMainLoop *loop;')
        self.b('    GError **error;')

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b('    %s*%s;' % (ctype, name))

        self.b('    gboolean success:1;')
        self.b('    gboolean completed:1;')
        self.b('} _%s_%s_run_state_%s;'
               % (self.prefix_lc, iface_lc, member_lc))

        reentrant_invoke = '_%s_%s_finish_running_%s' % (self.prefix_lc,
                                                         iface_lc,
                                                         member_lc)

        self.b('static void')
        self.b('%s (TpProxy *self,' % reentrant_invoke)
        self.b('    GError *error,')
        self.b('    GValueArray *args,')
        self.b('    GCallback unused,')
        self.b('    gpointer user_data,')
        self.b('    GObject *unused2)')
        self.b('{')
        self.b('  _%s_%s_run_state_%s *state = user_data;'
               % (self.prefix_lc, iface_lc, member_lc))
        self.b('')
        self.b('  state->success = (error == NULL);')
        self.b('  state->completed = TRUE;')
        self.b('  g_main_loop_quit (state->loop);')
        self.b('')
        self.b('  if (error != NULL)')
        self.b('    {')
        self.b('      if (state->error != NULL)')
        self.b('        *state->error = error;')
        self.b('      else')
        self.b('        g_error_free (error);')
        self.b('')
        self.b('      return;')
        self.b('    }')
        self.b('')

        for i, arg in enumerate(out_args):
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b('  if (state->%s != NULL)' % name)
            if marshaller == 'BOXED':
                self.b('    *state->%s = g_value_dup_boxed ('
                       'args->values + %d);' % (name, i))
            elif marshaller == 'STRING':
                self.b('    *state->%s = g_value_dup_string '
                       '(args->values + %d);' % (name, i))
            elif marshaller in ('UCHAR', 'BOOLEAN', 'INT', 'UINT',
                    'INT64', 'UINT64', 'DOUBLE'):
                self.b('    *state->%s = g_value_get_%s (args->values + %d);'
                       % (name, marshaller.lower(), i))
            else:
                assert False, "Don't know how to copy %s" % gtype

            self.b('')

        self.b('  g_value_array_free (args);')
        self.b('}')
        self.b('')

        self.h('gboolean %s_%s_run_%s (%sproxy,'
               % (self.prefix_lc, iface_lc, member_lc, self.proxy_arg))
        self.h('    gint timeout_ms,')

        self.b('/**')
        self.b(' * %s_%s_run_%s:' % (self.prefix_lc, iface_lc, member_lc))
        self.b(' * @proxy: %s' % self.proxy_doc)
        self.b(' * @timeout_ms: Timeout in milliseconds, or -1 for default')

        for arg in in_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: Used to pass an \'in\' argument: <![CDATA[%s]]>'
                   % (name, get_docstring(elt) or '(Undocumented)'))

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.b(' * @%s: Used to return an \'out\' argument if %%TRUE is '
                   'returned: <![CDATA[%s]]>'
                   % (name, get_docstring(elt) or '(Undocumented)'))

        self.b(' * @error: Used to return errors if %FALSE is returned')
        self.b(' *')
        self.b(' * Call the method %s and block, without' % member)
        self.b(' * re-entering the main loop, until it returns.')
        self.b(' *')
        self.b(' * <![CDATA[%s]]>'
                % (get_docstring(method) or '(Undocumented)'))
        self.b(' *')
        self.b(' * Returns: TRUE on success, FALSE and sets @error on error')
        self.b(' */')
        self.b('gboolean\n%s_%s_run_%s (%sproxy,'
               % (self.prefix_lc, iface_lc, member_lc, self.proxy_arg))
        self.b('    gint timeout_ms,')

        for arg in in_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.h('    %s%s%s,' % (const, ctype, name))
            self.b('    %s%s%s,' % (const, ctype, name))

        for arg in out_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            self.h('    %s*%s,' % (ctype, name))
            self.b('    %s*%s,' % (ctype, name))

        self.h('    GError **error);')
        self.h('')

        self.b('    GError **error)')
        self.b('{')
        self.b('  DBusGProxy *iface;')
        self.b('  GQuark interface = TP_IFACE_QUARK_%s;' % iface_lc.upper())
        self.b('  TpProxyPendingCall *pc;')
        self.b('  _%s_%s_run_state_%s state = {'
               % (self.prefix_lc, iface_lc, member_lc))
        self.b('      NULL /* loop */, error,')

        for arg in out_args:
            name, info, tp_type, elt = arg

            self.b('    %s,' % name)

        self.b('      FALSE /* completed */, FALSE /* success */ };')
        self.b('')
        self.b('  g_return_val_if_fail (%s (proxy), FALSE);'
               % self.proxy_assert)
        self.b('')
        self.b('  iface = tp_proxy_borrow_interface_by_id')
        self.b('       ((TpProxy *) proxy, interface, error);')
        self.b('')
        self.b('  if (iface == NULL)')
        self.b('    return FALSE;')
        self.b('')
        self.b('  state.loop = g_main_loop_new (NULL, FALSE);')
        self.b('')
        self.b('  pc = tp_proxy_pending_call_v0_new ((TpProxy *) proxy,')
        self.b('      interface, "%s", ' % member)
        self.b('      %s,' % reentrant_invoke)
        self.b('      NULL, &state, NULL, NULL);')
        self.b('  tp_proxy_pending_call_v0_take_pending_call (pc,')
        self.b('      dbus_g_proxy_begin_call_with_timeout (iface,')
        self.b('          "%s",' % member)
        self.b('          %s,' % collect_callback)
        self.b('          pc,')
        self.b('          tp_proxy_pending_call_v0_completed,')
        self.b('          timeout_ms,')

        for arg in in_args:
            name, info, tp_type, elt = arg
            ctype, gtype, marshaller, pointer = info

            const = pointer and 'const ' or ''

            self.b('              %s, %s,' % (gtype, name))

        self.b('          G_TYPE_INVALID));')
        self.b('')
        self.b('  if (!state.completed)')
        self.b('    g_main_loop_run (state.loop);')
        self.b('  g_main_loop_unref (state.loop);')
        self.b('')
        self.b('  return state.success;')
        self.b('}')
        self.b('')

        # leave a gap for the end of the method
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
            self.do_signal(name, signal)

        for method in methods:
            self.do_method(name, method)

    def __call__(self):

        self.h('G_BEGIN_DECLS')
        self.h('')

        self.b('/* We don\'t want gtkdoc scanning this file, it\'ll get')
        self.b(' * confused by seeing function definitions, so mark it as: */')
        self.b('/*<private_header>*/')
        self.b('')

        ifaces = self.dom.getElementsByTagName('node')
        ifaces.sort(cmp_by_name)

        for iface in ifaces:
            self.do_interface(iface)

        if self.group is not None:

            self.b('/*')
            self.b(' * %s_%s_add_signals:' % (self.prefix_lc, self.group))
            self.b(' * @self: the #TpProxy')
            self.b(' * @quark: a quark whose string value is the interface')
            self.b(' *   name whose signals should be added')
            self.b(' * @proxy: the D-Bus proxy to which to add the signals')
            self.b(' * @unused: not used for anything')
            self.b(' *')
            self.b(' * Tell dbus-glib that @proxy has the signatures of all')
            self.b(' * signals on the given interface, if it\'s one we')
            self.b(' * support.')
            self.b(' *')
            self.b(' * This function should be used as a signal handler for')
            self.b(' * #TpProxy::interface-added.')
            self.b(' */')
            self.b('static void')
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
        open(self.basename + '-body.h', 'w').write('\n'.join(self.__body))


def types_to_gtypes(types):
    return [type_to_gtype(t)[1] for t in types]


if __name__ == '__main__':
    options, argv = gnu_getopt(sys.argv[1:], '',
                               ['group=', 'subclass=', 'subclass-assert='])

    opts = {}

    for option, value in options:
        opts[option] = value

    dom = xml.dom.minidom.parse(argv[0])

    Generator(dom, argv[1], argv[2], opts)()
