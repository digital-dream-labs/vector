#!/usr/bin/env python2
## C++ Lite send message helper function emitter for Cozmo
## @author "Daniel Casner <daniel@anki.com>"

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

class SendHelperHeaderEmitter(ast.NodeVisitor):
    "An emitter that generates the handler declaration."

    def __init__(self, output=sys.stdout, include_extension=None):
        self.output = output

    def visit_NamespaceDecl(self, node, *args, **kwargs):
        self.output.write('namespace {namespace_name} {{\n\n'.format(namespace_name=node.name))
        self.generic_visit(node, *args, **kwargs)
        self.output.write('}} // namespace {namespace_name}\n\n'.format(namespace_name=node.name))

    def visit_UnionDecl(self, node):
        for member in node.members():
            self.output.write('inline bool SendMessage(const {member_type}& msg) {{ return Anki::Vector::HAL::RadioSendMessage(msg.GetBuffer(), msg.Size(), {member_tag}); }}\n'.format(
                member_tag=member.tag, member_name=member.name, member_type=CPP_emitter.cpp_value_type(member.type)))




if __name__ == '__main__':
    from clad import emitterutil
    from emitters import CPP_emitter

    language = 'C++ Lite (embedded)'

    emitterutil.c_main(language=language, extension='_send_helper.h',
        emitter_types=[SendHelperHeaderEmitter],
        use_inclusion_guards=False)
