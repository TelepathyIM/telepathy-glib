#!/usr/bin/python

import sys
import xml.dom.minidom

from libglibcodegen import NS_TP, camelcase_to_upper

class Generator(object):
    def __init__(self, dom):
        self.dom = dom
        self.errors = self.dom.getElementsByTagNameNS(NS_TP, 'errors')[0]

    def __call__(self):

        for error in self.errors.getElementsByTagNameNS(NS_TP, 'error'):
            ns = error.parentNode.getAttribute('namespace')
            nick = error.getAttribute('name').replace(' ', '')
            name = 'TP_ERROR_STR_' + camelcase_to_upper(nick.replace('.', ''))

            print '#define %s "%s.%s"' % (name, ns, nick)

if __name__ == '__main__':
    argv = sys.argv[1:]
    Generator(xml.dom.minidom.parse(argv[0]))()
