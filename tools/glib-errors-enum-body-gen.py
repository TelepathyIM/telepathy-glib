#!/usr/bin/python

import sys
import xml.dom.minidom

from libglibcodegen import NS_TP, camelcase_to_upper, get_docstring, \
        get_descendant_text

class Generator(object):
    def __init__(self, dom):
        self.dom = dom
        self.errors = self.dom.getElementsByTagNameNS(NS_TP, 'errors')[0]

    def do_header(self):
        print '/* Generated from the Telepathy spec\n'
        copyrights = self.errors.getElementsByTagNameNS(NS_TP, 'copyright')
        for copyright in copyrights:
            print get_descendant_text(copyright)
        license = self.errors.getElementsByTagNameNS(NS_TP, 'license')[0]
        print '\n' + get_descendant_text(license) + '\n*/'

    def do_enum_values(self):
        for error in self.errors.getElementsByTagNameNS(NS_TP, 'error'):
            print ''
            nick = error.getAttribute('name').replace(' ', '')
            name = camelcase_to_upper(nick.replace('.', ''))
            ns = error.parentNode.getAttribute('namespace')
            enum = 'TP_ERROR_' + name
            print '        /* ' + ns + '.' + name
            print '    ' + get_docstring(error)
            print '     */'
            print '        { %s, "%s", "%s" },' % (enum, enum, nick)


    def do_get_type(self):
        print """
#include <_gen/telepathy-errors.h>

GType
tp_error_get_type (void)
{
  static GType etype = 0;
  if (G_UNLIKELY (etype == 0))
    {
      static const GEnumValue values[] = {"""
        self.do_enum_values()
        print """\
      };

      etype = g_enum_register_static ("TpError", values);
    }
  return etype;
}
"""

    def __call__(self):
        self.do_header()
        self.do_get_type()

if __name__ == '__main__':
    argv = sys.argv[1:]
    Generator(xml.dom.minidom.parse(argv[0]))()
