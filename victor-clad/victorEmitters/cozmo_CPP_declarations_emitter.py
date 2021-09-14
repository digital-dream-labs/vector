#!/usr/bin/env python2

from __future__ import absolute_import
from __future__ import print_function

import inspect
import os
import sys
import textwrap

def _modify_path():
    message_buffers_path = os.path.join(os.path.dirname(__file__), '..', '..', 'victor-clad', 'tools', 'message-buffers')
    if message_buffers_path not in sys.path:
        sys.path.insert(0, message_buffers_path)
_modify_path()

from clad import ast
from clad import clad
from emitters import CPP_emitter

class UnionDeclarationEmitter(ast.NodeVisitor):
    "An emitter that generates the handler declaration."
    
    def __init__(self, output=sys.stdout, include_extension=None):
        self.output = output
    
    def visit_UnionDecl(self, node):
        for member in node.members():
            self.output.write('void Process_{member_name}(const {member_type}& msg);\n'.format(
                member_name=member.name, member_type=CPP_emitter.cpp_value_type(member.type)))

if __name__ == '__main__':
    from clad import emitterutil
    from emitters import CPP_emitter
    
    emitterutil.c_main(language='C++', extension='_declarations.def',
        emitter_types=[UnionDeclarationEmitter],
        use_inclusion_guards=False)
