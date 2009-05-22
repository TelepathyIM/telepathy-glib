#!/usr/bin/python

import sys
import xml.dom.minidom

from libglibcodegen import NS_TP, camelcase_to_upper, get_docstring, xml_escape

class Generator(object):
    def __init__(self, dom):
        self.dom = dom
        self.errors = self.dom.getElementsByTagNameNS(NS_TP, 'errors')[0]

    def __call__(self):

        for error in self.errors.getElementsByTagNameNS(NS_TP, 'error'):
            ns = error.parentNode.getAttribute('namespace')
            nick = error.getAttribute('name').replace(' ', '')
            name = 'TP_ERROR_STR_' + camelcase_to_upper(nick.replace('.', ''))
            error_name = '%s.%s' % (ns, nick)

            print ''
            print '/**'
            print ' * %s:' % name
            print ' *'
            print ' * The D-Bus error name %s' % error_name
            print ' *'
            print ' * %s' % xml_escape(get_docstring(error))
            print ' */'
            print '#define %s "%s"' % (name, error_name)

if __name__ == '__main__':
    argv = sys.argv[1:]
    Generator(xml.dom.minidom.parse(argv[0]))()
