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

class UnionSwitchEmitter(ast.NodeVisitor):
    "An emitter that generates the handler switch statement."

    groupSwitchPrefix = None
    groupedSwitchMembers = []
    startID = None
    endID = None

    def __init__(self, output=sys.stdout, include_extension=None):
        self.output = output

    def visit_UnionDecl(self, node):
        globals = dict(
            union_name=node.name,
            qualified_union_name=node.fully_qualified_name(),
        )

        self.writeHeader(node, globals)
        self.writeMemberCases(node, globals)
        self.writeFooter(node, globals)

    def writeHeader(self, node, globals):
        pass

    def writeFooter(self, node, globals):
        if self.groupedSwitchMembers:
            for member in self.groupedSwitchMembers:
                self.output.write('case 0x{member_tag:x}:\n'.format(member_tag=member.tag))
            self.output.write('\tProcess_{group_prefix}(msg);\n\tbreak;\n'.format(group_prefix=self.groupSwitchPrefix))

    def writeMemberCases(self, node, globals):
        for member in node.members():
            if self.groupSwitchPrefix is not None and member.name.startswith(self.groupSwitchPrefix):
                self.groupedSwitchMembers.append(member)
            elif self.startID is not None and member.tag < self.startID:
                continue
            elif self.endID is not None and member.tag > self.endID:
                continue
            else:
                self.output.write(textwrap.dedent('''\
                    case 0x{member_tag:x}:
                    \tProcess_{member_name}(msg.{member_name});
                    \tbreak;
                    ''').format(member_tag=member.tag, member_name=member.name, **globals))

if __name__ == '__main__':
    from clad import emitterutil
    from emitters import CPP_emitter

    suffix = '_switch'

    language = 'C++ Lite (embedded)'

    option_parser = emitterutil.StandardArgumentParser(language)
    option_parser.add_argument('--group', metavar='group', help='Group name')
    option_parser.add_argument('--start', metavar='start_idx', help='Start ID tag')
    option_parser.add_argument('--end', metavar='end_idx', help='End ID tag')
    
    options = option_parser.parse_args()

    if options.group:
        UnionSwitchEmitter.groupSwitchPrefix = options.group
        suffix += '_group_' + options.group
    if options.start:
        UnionSwitchEmitter.startID = int(options.start, 16)
        suffix += '_from_' + options.start
    if options.end:
        UnionSwitchEmitter.endID = int(options.end, 16)
        suffix += '_to_' + options.end

    tree = emitterutil.parse(options)

    def main_output_source_callback(output):
        UnionSwitchEmitter(output).visit(tree)

    main_output_source = emitterutil.get_output_file(options, suffix+'.def')
    emitterutil.write_c_file(options.output_directory, main_output_source,
        main_output_source_callback,
        use_inclusion_guards=False)