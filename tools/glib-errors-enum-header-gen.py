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

    def do_gtkdoc(self):
        for error in self.errors.getElementsByTagNameNS(NS_TP, 'error'):
            ns = error.parentNode.getAttribute('namespace')
            nick = error.getAttribute('name').replace(' ', '')
            enum = 'TP_ERROR_' + camelcase_to_upper(nick.replace('.', ''))
            print ' * @' + enum + ': ' + ns + '.' + nick + ':'
            print ' *     ' + get_docstring(error) + '    '

    def do_enumnames(self):
        for error in self.errors.getElementsByTagNameNS(NS_TP, 'error'):
            nick = error.getAttribute('name').replace(' ', '')
            enum = 'TP_ERROR_' + camelcase_to_upper(nick.replace('.', ''))
            print '    ' + enum + ','

    def do_get_type(self):
        print """
#include <glib-object.h>

G_BEGIN_DECLS

GType tp_error_get_type (void);

/**
 * TP_TYPE_ERROR:
 *
 * The GType of the Telepathy error enumeration.
 */
#define TP_TYPE_ERROR (tp_error_get_type())
"""

    def do_enum(self):
        print """\
/**
 * TpError:"""
        self.do_gtkdoc()
        print """\
 *
 * Enumerated type representing the Telepathy D-Bus errors.
 */
typedef enum {"""
        self.do_enumnames()
        print """\
} TpError;

G_END_DECLS"""

    def __call__(self):
        self.do_header()
        self.do_get_type()
        self.do_enum()

if __name__ == '__main__':
    argv = sys.argv[1:]
    Generator(xml.dom.minidom.parse(argv[0]))()
