#!/usr/bin/python

# Generate GLib GInterfaces from the Telepathy specification.
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

from libglibcodegen import camelcase_to_lower, \
                           camelcase_to_upper, \
                           cmp_by_name, \
                           dbus_gutils_wincaps_to_uscore, \
                           signal_to_marshal_type, \
                           signal_to_marshal_name, \
                           Signature, \
                           type_to_gtype


def cmdline_error():
    print """\
usage:
    gen-ginterface [OPTIONS] xmlfile classname
options:
    --include='<header.h>' (may be repeated)
    --include='"header.h"' (ditto)
        Include extra headers in the generated .c file
    --signal-marshal-prefix='prefix'
        Use the given prefix on generated signal marshallers (default is
        derived from class name).
    --filename='BASENAME'
        Set the basename for the output files (default is derived from class
        name)
    --not-implemented-func='symbol'
        Set action when methods not implemented in the interface vtable are
        called. symbol must have signature
            void symbol (DBusGMethodInvocation *context)
        and return some sort of "not implemented" error via
            dbus_g_method_return_error (context, ...)
"""
    sys.exit(1)


def signal_to_gtype_list(signal):
    gtype=[]
    for i in signal.getElementsByTagName("arg"):
        name =i.getAttribute("name")
        type = i.getAttribute("type")
        gtype.append(type_to_gtype(type)[1])

    return gtype


def print_header_begin(stream, prefix):
    guardname = '__'+prefix.upper()+'_H__'
    stream.write ("#ifndef "+guardname+"\n")
    stream.write ("#define "+guardname+"\n\n")

    stream.write ("#include <glib-object.h>\n#include <dbus/dbus-glib.h>\n\n")
    stream.write ("G_BEGIN_DECLS\n\n")

def print_header_end(stream, prefix):
    guardname = '__'+prefix.upper()+'_H__'
    stream.write ("\nG_END_DECLS\n\n")
    stream.write ("#endif /* #ifndef "+guardname+"*/\n")

def print_class_declaration(stream, prefix, classname, methods):
    stream.write ("""\
/**
 * %(classname)s:
 *
 * Dummy typedef representing any implementation of this interface.
 */
typedef struct _%(classname)s %(classname)s;

/**
 * %(classname)sClass:
 *
 * The class of %(classname)s.
 */
typedef struct _%(classname)sClass %(classname)sClass;

""" % locals())

    stream.write(
"""
GType %(prefix)s_get_type (void);

""" % {'prefix':prefix,'uprefix':prefix.upper()})

    macro_prefix = prefix.upper().split('_',1)
    gtype = '_TYPE_'.join(macro_prefix)

    stream.write(
"""/* TYPE MACROS */
#define %(type)s \\
  (%(prefix)s_get_type ())
#define %(main)s_%(sub)s(obj) \\
  (G_TYPE_CHECK_INSTANCE_CAST((obj), %(type)s, %(name)s))
#define %(main)s_IS_%(sub)s(obj) \\
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), %(type)s))
#define %(main)s_%(sub)s_GET_CLASS(obj) \\
  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), %(type)s, %(name)sClass))

""" % {"main":macro_prefix[0], "sub":macro_prefix[1], "type":gtype, "name":classname, "prefix":prefix})
 

def signal_emit_stub(signal):
    # for signal: org.freedesktop.Telepathy.Thing::StuffHappened (s, u)
    # emit: void tp_svc_thing_emit_stuff_happened (gpointer instance,
    #           const char *arg, guint arg2)
    dbus_name = signal.getAttributeNode("name").nodeValue
    c_emitter_name = prefix + '_emit_' + camelcase_to_lower(dbus_name)
    c_signal_const_name = 'SIGNAL_' + dbus_name

    macro_prefix = prefix.upper().split('_',1)

    decl = 'void ' + c_emitter_name + ' (gpointer instance'
    args = ''
    argdoc = ''

    for i in signal.getElementsByTagName("arg"):
        name = i.getAttribute("name")
        type = i.getAttribute("type")
        info = type_to_gtype(type)
        gtype = info[0]
        if gtype[3]:
            gtype = 'const ' + gtype
        decl += ',\n    ' + gtype + ' ' + name
        args += ', ' + name
        argdoc += ' * @' + name + ': FIXME: document args in genginterface\n'
    decl += ')'

    doc = ("""\
/**
 * %s:
 * @instance: An object implementing this interface
%s *
 * Emit the %s D-Bus signal from @instance with the given arguments.
 */
""" % (c_emitter_name, argdoc, dbus_name))

    header = decl + ';\n\n'
    body = doc + decl + ('\n{\n'
                   '  g_assert (%s_IS_%s (instance));\n'
                   '  g_signal_emit (instance, signals[%s], 0%s);\n'
                   '}\n\n'
                   % (macro_prefix[0], macro_prefix[1], c_signal_const_name,
                      args))

    return header, body


def print_class_definition(stream, prefix, classname, methods):
    stream.write ("struct _%sClass {\n" % classname)
    stream.write ("    GTypeInterface parent_class;\n")

    for method in methods:
        dbus_method_name = method.getAttributeNode("name").nodeValue
        lc_method_name = camelcase_to_lower(dbus_method_name)
        c_impl_name = prefix + '_' + lc_method_name + '_impl'
        stream.write('    %s %s;\n' % (c_impl_name, lc_method_name))

    stream.write ("};\n\n")


def do_method(method):
    # DoStuff (s -> u)
    dbus_method_name = method.getAttributeNode("name").nodeValue
    lc_method_name = camelcase_to_lower(dbus_method_name)
    # void tp_svc_thing_do_stuff (TpSvcThing *, const char *,
    #                             DBusGMethodInvocation *);
    c_method_name = prefix + '_' + lc_method_name
    # typedef void (*tp_svc_thing_do_stuff_impl) (TpSvcThing *, const char *,
    #                                             DBusGMethodInvocation *);
    c_impl_name = prefix + '_' + lc_method_name + '_impl'
    # void tp_svc_thing_return_from_do_stuff (DBusGMethodInvocation *, guint);
    ret_method_name = prefix + '_return_from_' + lc_method_name

    ret_count=0

    header = ''
    body = ''

    c_decl = "static void\n"
    method_decl = "typedef void (*" + c_impl_name + ') ('
    ret_decl = 'void\n'
    ret_body = '{\n  dbus_g_method_return (dbus_context'
    arg_doc = ''
    ret_arg_doc = ''

    tmp = c_method_name+' ('
    pad = ' ' * len(tmp)
    c_decl += tmp+classname+' *self'

    method_pad = ' ' * len(method_decl)
    method_decl += classname + ' *self'
    args = 'self'

    tmp = ret_method_name+' ('
    ret_pad = ' ' * len(tmp)
    ret_decl += tmp+'DBusGMethodInvocation *dbus_context'

    for i in method.getElementsByTagName("arg"):
        name =i.getAttribute("name")
        direction = i.getAttribute("direction")
        type = i.getAttribute("type")

        if not name and direction == "out":
            if ret_count==0:
                name = "ret"
            else:
                name = "ret"+str(ret_count)
            ret_count += 1

        gtype = type_to_gtype(type)[0]
        if type_to_gtype(type)[3]:
            gtype="const "+gtype
        if direction != "out":
            c_decl +=",\n"+pad+gtype+name
            method_decl +=",\n"+method_pad+gtype+name
            args += ', '+name
            arg_doc += (' * @' + name
                    + ': FIXME: document args in genginterface\n')
        else:
            ret_decl += ",\n"+ret_pad+gtype+name
            ret_body += ', '+name
            ret_arg_doc += (' * @' + name
                    + ': FIXME: document args in genginterface\n')

    c_decl += ",\n"+pad+"DBusGMethodInvocation *context)"
    method_decl += ",\n"+method_pad+"DBusGMethodInvocation *context);\n"
    args += ', context'

    ret_doc = ("""\
/**
 * %s:
 * @dbus_context: The D-Bus method invocation context
%s *
 * Return successfully by calling dbus_g_method_return (@dbus_context,
 * ...). This inline function is just a type-safe wrapper for
 * dbus_g_method_return.
 */
""" % (ret_method_name, ret_arg_doc))

    interface = method.parentNode.getAttribute("name");
    ret_decl += ')\n'
    ret_body += ');\n}\n'
    header += (ret_doc + 'static inline\n/**/\n' + ret_decl + ';\n'
            + 'static inline ' + ret_decl + ret_body)
    body += (
"""
/**
 * %(c_impl_name)s
 * @self: The object implementing this interface
%(arg_doc)s * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 *
 * Signature of an implementation of D-Bus method %(dbus_method_name)s
 * on interface %(interface)s
 */
""" % locals())

    body += c_decl+"\n{\n"
    body += "  %s impl = (%s_GET_CLASS (self)->%s);\n" % (
            c_impl_name, prefix.upper(), lc_method_name)
    body += "  if (impl)\n"
    body += "    (impl) (%s);\n" % args
    body += "  else\n"
    if not_implemented_func:
        body += "    %s (context);\n" % not_implemented_func
    else:
        # this seems as appropriate an error as any
        body += """\
    {
      GError e = { DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD,
          "Method not implemented" };

      dbus_g_method_return_error (context, &e);
    }
"""
    body += "}\n\n"

    dg_method_name = prefix + '_' + dbus_gutils_wincaps_to_uscore(dbus_method_name)
    if dg_method_name != c_method_name:
        body += ("""\
#define %(dg_method_name)s %(c_method_name)s

""" % {'dg_method_name': dg_method_name, 'c_method_name': c_method_name })

    method_decl += 'void %s_implement_%s (%sClass *klass, %s impl);\n\n' \
                   % (prefix, lc_method_name, classname, c_impl_name)

    body += ("""\
/**
 * %s_implement_%s:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the %s method
 *
 * Register an implementation for the %s method in the vtable of an
 * implementation of this interface. To be called from the interface
 * init function.
 */
""" % (prefix, lc_method_name, dbus_method_name, dbus_method_name))
    body += 'void\n%s_implement_%s (%sClass *klass, %s impl)\n{\n'\
            % (prefix, lc_method_name, classname, c_impl_name)
    body += '  klass->%s = impl;\n' % lc_method_name
    body += '}\n\n'

    return (method_decl, header, body)

if __name__ == '__main__':
    from getopt import gnu_getopt

    options, argv = gnu_getopt(sys.argv[1:], '',
                               ['filename=', 'signal-marshal-prefix=',
                                'include=',
                                'not-implemented-func='])

    try:
        classname = argv[1]
    except IndexError:
        cmdline_error()

    prefix = camelcase_to_lower(classname)

    basename = prefix.replace('_', '-')
    signal_marshal_prefix = prefix
    headers = []
    not_implemented_func = ''

    for option, value in options:
        if option == '--filename':
            basename = value
        elif option == '--signal-marshal-prefix':
            signal_marshal_prefix = value
        elif option == '--include':
            if value[0] not in '<"':
                value = '"%s"' % value
            headers.append(value)
        elif option == '--not-implemented-func':
            not_implemented_func = value

    outname_header = basename + ".h"
    outname_body = basename + ".c"

    header=open(outname_header,'w')
    body=open(outname_body, 'w')

    try:
        dom = xml.dom.minidom.parse(argv[0])
    except IndexError:
        cmdline_error()

    signals = dom.getElementsByTagName("signal")
    signals.sort(cmp_by_name)
    methods = dom.getElementsByTagName("method")
    methods.sort(cmp_by_name)

    print_header_begin(header,prefix)

    print_class_declaration(header, prefix, classname, methods)

    # include my own header first, to ensure self-contained
    body.write(
"""#include "%s"

""" % outname_header)

    # required headers
    body.write(
"""#include <stdio.h>
#include <stdlib.h>

""")

    for h in headers:
        body.write('#include %s\n' % h)
    body.write('\n')

    if signal_marshal_prefix == prefix:
        body.write('#include "%s-signals-marshal.h"\n' % basename)
        # else assume the signal marshallers are declared in one of the headers

    body.write('const DBusGObjectInfo dbus_glib_%s_object_info;\n'
            % prefix)

    print_class_definition(body, prefix, classname, methods)

    if signals:
        body.write('enum {\n')
        for signal in signals:
            dbus_name = signal.getAttributeNode("name").nodeValue
            body.write('    SIGNAL_%s,\n' % (dbus_name))
        body.write('    N_SIGNALS\n};\nstatic guint signals[N_SIGNALS] = {0};\n\n')

    gtypename = '_TYPE_'.join(prefix.upper().split('_',1))

    body.write(
"""
static void
%(prefix)s_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;
""" % {'classname':classname, 'gtypename':gtypename, 'prefix':prefix, 'uprefix':prefix.upper()})

    header.write("\n")

    marshallers = {}
    for signal in signals:
        dbus_name = signal.getAttributeNode("name").nodeValue
        gtypelist = signal_to_gtype_list(signal)
        marshal_name = signal_to_marshal_name(signal, signal_marshal_prefix)

        body.write(
"""
      signals[SIGNAL_%s] =
      g_signal_new ("%s",
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                    0,
                    NULL, NULL,
                    %s,
                    G_TYPE_NONE, %s);
""" % (dbus_name,
       (dbus_gutils_wincaps_to_uscore(dbus_name)).replace('_','-'),
       marshal_name,
       ', '.join([str(len(gtypelist))] + gtypelist)))

        if not marshal_name.startswith('g_cclosure_marshal_VOID__'):
            mtype = signal_to_marshal_type(signal)
            assert(len(mtype))
            marshallers[','.join(mtype)] = True

    body.write(
"""
      dbus_g_object_type_install_info (%(prefix)s_get_type (), &dbus_glib_%(prefix)s_object_info);
    }
}

GType
%(prefix)s_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0)) {
    static const GTypeInfo info = {
      sizeof (%(classname)sClass),
      %(prefix)s_base_init, /* base_init */
      NULL, /* base_finalize */
      NULL, /* class_init */
      NULL, /* class_finalize */
      NULL, /* class_data */
      0,
      0, /* n_preallocs */
      NULL /* instance_init */
    };

    type = g_type_register_static (G_TYPE_INTERFACE, "%(classname)s", &info, 0);
  }

  return type;
}

""" % {'classname':classname,'prefix':prefix, 'uprefix':prefix.upper()})

    for method in methods:
        m, h, b = do_method(method)
        header.write(m + '\n')
        header.write(h)
        body.write(b)

    for signal in signals:
        h, b = signal_emit_stub(signal)
        header.write(h)
        body.write(b)

    header.write('\n')

    body.write("""\
#include "%s-glue.h"

""" % (basename))

    print_header_end(header,prefix)
    header.close()
    body.close()
