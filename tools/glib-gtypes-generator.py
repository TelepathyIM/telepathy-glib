#!/usr/bin/python

# Generate GLib GInterfaces from the Telepathy specification.
# The master copy of this stylesheet is in the telepathy-glib repository -
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
import xml.dom.minidom
from string import ascii_letters, digits


from genginterface import Signature, type_to_gtype


NS_TP = "http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0"
_ASCII_ALNUM = ascii_letters + digits


def escape_identifier(identifier):
    """Escape the given string to be a valid D-Bus object path or service
    name component, using a reversible encoding to ensure uniqueness.

    The reversible encoding is as follows:

    * The empty string becomes '_'
    * Otherwise, each non-alphanumeric character is replaced by '_' plus
      two lower-case hex digits; the same replacement is carried out on
      the first character, if it's a digit
    """
    # '' -> '_'
    if not identifier:
        return '_'

    # A bit of a fast path for strings which are already OK.
    # We deliberately omit '_' because, for reversibility, that must also
    # be escaped.
    if (identifier.strip(_ASCII_ALNUM) == '' and
        identifier[0] in ascii_letters):
        return identifier

    # The first character may not be a digit
    if identifier[0] not in ascii_letters:
        ret = ['_%02x' % ord(identifier[0])]
    else:
        ret = [identifier[0]]

    # Subsequent characters may be digits or ASCII letters
    for c in identifier[1:]:
        if c in _ASCII_ALNUM:
            ret.append(c)
        else:
            ret.append('_%02x' % ord(c))

    return ''.join(ret)


def types_to_gtypes(types):
    return [type_to_gtype(t)[1] for t in types]


class GTypesGenerator(object):
    def __init__(self, dom, output, mixed_case_prefix):
        self.dom = dom
        self.Prefix = mixed_case_prefix
        self.PREFIX_ = self.Prefix.upper() + '_'
        self.prefix_ = self.Prefix.lower() + '_'

        self.header = open(output + '.h', 'w')
        self.body = open(output + '-body.h', 'w')

        for f in (self.header, self.body):
            f.write('/* Auto-generated, do not edit.\n * \n'
                    ' * This file may be distributed under the same terms\n'
                    ' * as the specification from which it was generated.\n'
                    ' */\n\n')

        self.need_mappings = {}
        self.need_structs = {}

    def do_mapping_header(self, mapping):
        impl_sig = ''.join([elt.getAttribute('type')
                            for elt in mapping.getElementsByTagNameNS(NS_TP,
                                'member')])
        esc_impl_sig = escape_identifier(impl_sig)

        name = (self.PREFIX_ + 'HASH_TYPE_' +
                mapping.getAttribute('name').upper())
        impl = self.prefix_ + 'type_dbus_hash_' + esc_impl_sig

        docstring = mapping.getElementsByTagNameNS(NS_TP, 'docstring')
        if docstring:
            docstring = docstring[0].toprettyxml()
            if docstring.startswith('<tp:docstring>'):
                docstring = docstring[14:]
            if docstring.endswith('</tp:docstring>\n'):
                docstring = docstring[:-16]
        self.header.write('/**\n * %s:\n\n' % name)
        self.header.write(' * <![CDATA[%s]]>\n' % docstring)
        self.header.write(' * This macro expands to a call to a function\n')
        self.header.write(' * that returns a GType.\n')
        self.header.write(' */\n')

        self.header.write('#define %s (%s ())\n\n' % (name, impl))
        self.need_mappings[impl_sig] = esc_impl_sig

    def do_struct_header(self, struct):
        impl_sig = ''.join([elt.getAttribute('type')
                            for elt in struct.getElementsByTagNameNS(NS_TP,
                                'member')])
        esc_impl_sig = escape_identifier(impl_sig)

        name = (self.PREFIX_ + 'STRUCT_TYPE_' +
                struct.getAttribute('name').upper())
        impl = self.prefix_ + 'type_dbus_struct_' + esc_impl_sig
        docstring = struct.getElementsByTagNameNS(NS_TP, 'docstring')
        if docstring:
            docstring = docstring[0].toprettyxml()
            if docstring.startswith('<tp:docstring>'):
                docstring = docstring[14:]
            if docstring.endswith('</tp:docstring>\n'):
                docstring = docstring[:-16]
        self.header.write('/**\n * %s:\n\n' % name)
        self.header.write(' * <![CDATA[%s]]>\n' % docstring)
        self.header.write(' * This macro expands to a call to a function\n')
        self.header.write(' * that returns a GType.\n')
        self.header.write(' */\n')
        self.header.write('#define %s (%s ())\n\n' % (name, impl))

        if struct.hasAttribute('array-name'):
            array_name = struct.getAttribute('array-name')
        else:
            array_name = struct.getAttribute('name') + '_LIST'
        if array_name != '':
            array_name = (self.PREFIX_ + 'ARRAY_TYPE_' + array_name.upper())
            impl = self.prefix_ + 'type_dbus_array_' + esc_impl_sig
            self.header.write('/**\n * %s:\n\n' % array_name)
            self.header.write(' * An array of #%s.\n' % name)
            self.header.write(' * This macro expands to a call to a function\n')
            self.header.write(' * that returns a GType.\n')
            self.header.write(' */\n')
            self.header.write('#define %s (%s ())\n\n' % (array_name, impl))

        self.need_structs[impl_sig] = esc_impl_sig

    def __call__(self):
        mappings = self.dom.getElementsByTagNameNS(NS_TP, 'mapping')
        structs = self.dom.getElementsByTagNameNS(NS_TP, 'struct')

        for mapping in mappings:
            self.do_mapping_header(mapping)

        for sig in self.need_mappings:
            self.header.write('GType %stype_dbus_hash_%s (void);\n\n' %
                              (self.prefix_, self.need_mappings[sig]))
            self.body.write('GType\n%stype_dbus_hash_%s (void)\n{\n' %
                              (self.prefix_, self.need_mappings[sig]))
            self.body.write('  static GType t = 0;\n\n')
            self.body.write('  if (G_UNLIKELY (t == 0))\n')
            # FIXME: translate sig into two GTypes
            items = tuple(Signature(sig))
            gtypes = types_to_gtypes(items)
            self.body.write('    t = dbus_g_type_get_map ("GHashTable", '
                            '%s, %s);\n' % (gtypes[0], gtypes[1]))
            self.body.write('  return t;\n')
            self.body.write('}\n\n')

        for struct in structs:
            self.do_struct_header(struct)

        for sig in self.need_structs:
            self.header.write('GType %stype_dbus_struct_%s (void);\n\n' %
                              (self.prefix_, self.need_structs[sig]))
            self.body.write('GType\n%stype_dbus_struct_%s (void)\n{\n' %
                              (self.prefix_, self.need_structs[sig]))
            self.body.write('  static GType t = 0;\n\n')
            self.body.write('  if (G_UNLIKELY (t == 0))\n')
            self.body.write('    t = dbus_g_type_get_struct ("GValueArray",\n')
            items = tuple(Signature(sig))
            gtypes = types_to_gtypes(items)
            for gtype in gtypes:
                self.body.write('        %s,\n' % gtype)
            self.body.write('        G_TYPE_INVALID);\n')
            self.body.write('  return t;\n')
            self.body.write('}\n\n')

            self.header.write('GType %stype_dbus_array_%s (void);\n\n' %
                              (self.prefix_, self.need_structs[sig]))
            self.body.write('GType\n%stype_dbus_array_%s (void)\n{\n' %
                              (self.prefix_, self.need_structs[sig]))
            self.body.write('  static GType t = 0;\n\n')
            self.body.write('  if (G_UNLIKELY (t == 0))\n')
            self.body.write('    t = dbus_g_type_get_collection ("GPtrArray", '
                            '%stype_dbus_struct_%s ());\n' %
                            (self.prefix_, self.need_structs[sig]))
            self.body.write('  return t;\n')
            self.body.write('}\n\n')

if __name__ == '__main__':
    argv = sys.argv[1:]

    dom = xml.dom.minidom.parse(argv[0])

    GTypesGenerator(dom, argv[1], argv[2])()
