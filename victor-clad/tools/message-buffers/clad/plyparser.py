#-----------------------------------------------------------------
# plyparser.py
#
# PLYParser class and other utilites for simplifying programming
# parsers with PLY
#
# Copyright (C) 2008-2012, Eli Bendersky
# License: BSD
#-----------------------------------------------------------------

from __future__ import absolute_import
from __future__ import print_function

class Coord(object):
    """ Coordinates of a syntactic element. Consists of:
            - File name
            - Line number
            - (optional) column number, for the Lexer
    """
    def __init__(self, file, directory=None, lineno=None):
        self.file = file
        self.directory = directory
        self.lineno = lineno
        self.column = None

    def __str__(self):
        str = "%s:%s" % (self.file, self.lineno)
        if self.column: str += ":%s" % self.column
        return str


class ParseError(Exception): pass


class PLYParser(object):
    def _create_opt_rule(self, rulename):
        """ Given a rule name, creates an optional ply.yacc rule
            for it. The name of the optional rule is
            <rulename>_opt
        """
        optname = rulename + '_opt'

        def optrule(self, p):
            p[0] = p[1]

        optrule.__doc__ = '%s : empty\n| %s' % (optname, rulename)
        optrule.__name__ = 'p_%s' % optname
        setattr(self.__class__, optrule.__name__, optrule)

    def _parse_error(self, msg, coord):
        raise ParseError(coord, msg)
