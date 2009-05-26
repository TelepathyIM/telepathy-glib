#!/usr/bin/python

import sys
import xml.dom.minidom

from libglibcodegen import NS_TP, camelcase_to_upper, get_docstring, xml_escape

class Generator(object):
    def __init__(self, dom, basename):
        self.dom = dom
        self.errors = self.dom.getElementsByTagNameNS(NS_TP, 'errors')[0]
        self.basename = basename

        self.__header = []
        self.__body = []

    def h(self, s):
        self.__header.append(s)

    def b(self, s):
        self.__body.append(s)

    def __call__(self):
        errors = self.errors.getElementsByTagNameNS(NS_TP, 'error')

        for error in errors:
            ns = error.parentNode.getAttribute('namespace')
            nick = error.getAttribute('name').replace(' ', '')
            name = 'TP_ERROR_STR_' + camelcase_to_upper(nick.replace('.', ''))
            error_name = '%s.%s' % (ns, nick)

            self.h('')
            self.h('/**')
            self.h(' * %s:' % name)
            self.h(' *')
            self.h(' * The D-Bus error name %s' % error_name)
            self.h(' *')
            self.h(' * %s' % xml_escape(get_docstring(error)))
            self.h(' */')
            self.h('#define %s "%s"' % (name, error_name))

        open(self.basename + '.h', 'w').write('\n'.join(self.__header))
        open(self.basename + '.c', 'w').write('\n'.join(self.__body))

if __name__ == '__main__':
    argv = sys.argv[1:]
    basename = argv[0]

    Generator(xml.dom.minidom.parse(argv[1]), basename)()
