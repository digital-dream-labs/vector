#!/usr/bin/env python2
#
# Copyright 2015-2016 Anki Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import absolute_import
from __future__ import print_function

import inspect
import io
import os
import sys

def _modify_path():
    currentpath = os.path.dirname(inspect.getfile(inspect.currentframe()))
    searchpath = os.path.join(currentpath, '..')
    searchpath = os.path.normpath(os.path.abspath(os.path.realpath(searchpath)))
    if searchpath not in sys.path:
        sys.path.insert(0, searchpath)
_modify_path()

from clad import ast
from clad import clad
from clad import emitterutil

type_translations = {
    'bool': 'Bool',
    'int_8': 'Int8',
    'int_16': 'Int16',
    'int_32': 'Int32',
    'int_64': 'BigInt64',
    'uint_8': 'Uint8',
    'uint_16': 'Uint16',
    'uint_32': 'Uint32',
    'uint_64': 'BigUint64',
    'float_32': 'Float32',
    'float_64': 'Float64'
}

input_file = None
defaults_warned = False
type_namespace_map = {}

def get_visitor_output(visitorType, node):
    _py3 = sys.version_info[0] >= 3
    output = io.StringIO() if _py3 else io.BytesIO()
    visitorType(output).visit(node)
    return output.getvalue()

def write_export(node, name, output):
    if (not hasattr(node, 'namespace')) or node.namespace == None:
        # export object itself
        output.write('\nmodule.exports = {{ {} }};\n\n'.format(name))

def get_namespace_variable(node, init):
    if (not hasattr(node, 'namespace')) or node.namespace == None:
        if init:
            return 'const ' + node.name
        else:
            return node.name
    else:
        ns_str = ''
        ns_list = []
        if node.namespace:
          ns_list = str(node.namespace).split('::')
        for ns in ns_list:
          ns_str += (ns + '.')
        return ns_str + str(node.name)

class NamespaceEmitter(ast.NodeVisitor):

    def __init__(self, output):
        self.output = output
        self.imported = set()
        self.namespace_stack = []
        self.namespace_text = ''
        self.namespace_dict = {}
        self.export_dict = {}

    def visit_NamespaceDecl(self, node, *args, **kwargs):
        self.namespace_stack.append(node.name)

        namespace_js = ''
        for n in self.namespace_stack:
            namespace_js += (n + '.')
        if len(namespace_js) > 0:
            namespace_js = namespace_js[:-1]

        self.namespace_text = namespace_js

        if not (namespace_js in self.namespace_dict):
            self.namespace_dict[namespace_js] = {}
            if len(self.namespace_stack) == 1:
                self.output.write('if({ns} === undefined) {{\n\tvar {ns} = {{}};\n}}\n'.format(ns=namespace_js))
            elif len(self.namespace_stack) > 1:
                self.output.write('if({ns} === undefined) {{\n\t{ns} = {{}};\n}}\n'.format(ns=namespace_js))

        self.generic_visit(node, *args, **kwargs)

        if len(self.namespace_stack) == 1:
            if not (namespace_js in self.export_dict):
                self.export_dict[namespace_js] = {}
                self.output.write('module.exports = {{ {} }};\n\n'.format(namespace_js))
        self.namespace_stack.pop()

    def visit_IncludeDecl(self, node, *args, **kwargs):
        pass

    def visit_EnumDecl(self, node, *args, **kwargs):
        EnumEmitter(self.output).visit(node, *args, **kwargs)

    def visit_MessageDecl(self, node, *args, **kwargs):
        StructEmitter(self.output).visit(node, *args, **kwargs)

    def visit_UnionDecl(self, node, *args, **kwargs):
        UnionEmitter(self.output).visit(node, *args, **kwargs)

class EnumEmitter(ast.NodeVisitor):
    def __init__(self, output):
        self.output = output

    def visit_EnumDecl(self, node, *args, **kwargs):      
        self.output.write('// ENUM {enum_name}\n'.format(enum_name=node.name))
        self.output.write('{var} = Object.freeze({{\n'.format(var=get_namespace_variable(node, True)))

        # if a member has an explicit value, use it
        # if a member doesn't have an explicit value 
        #   increment the previous known value (or 0 if None)
        starts = []
        ends = []
        need_explicit = False
        last_member = None
        last_value = -1
        for i, member in enumerate(node.members()):
            start = '\t{member_name}:'.format(member_name=member.name)
            if member.initializer:
                enum_val = member.initializer.value
                initializer = '0x{:x}'.format(enum_val) if member.initializer.type == "hex" else str(enum_val)
                start += initializer
                last_member = member
                last_value = enum_val
            else:
                initializer = '0x{:x}'.format(last_value + 1)
                start += initializer
                last_value = last_value + 1

            if i != len(node.members()) - 1:
              start += ','

            end = '\n'

            starts.append(start)
            ends.append(end)

        full_length = max(len(start) for start in starts)
        for start, end in zip(starts, ends):
            self.output.write(start)
            self.output.write(' ' * (full_length - len(start)))
            self.output.write(end)

        self.output.write('});\n\n')

        write_export(node, node.name, self.output)

class StructEmitter(ast.NodeVisitor):
    def __init__(self, output):
        self.output = output

    def print_one_default_warning(self):
        if hasattr(self.print_one_default_warning, 'printed'):
            return
        print("Warning: The JS emitter does not support default values for members. They have been ignored.")
        setattr(self.print_one_default_warning, 'printed', True)

    def visit_MessageDecl(self, node):
        self.emitStruct(node)
        self.output.write('\n')

    def emitStruct(self, node):
        self.output.write('// {obj_type} {struct_name}\n'.format(
            obj_type=node.object_type().upper(), struct_name=node.name))
        full_name = get_namespace_variable(node, True)
        self.output.write('{struct_name} = class extends Clad {{\n'.format(struct_name=full_name))

        with self.output.indent(1):
            self.emitMethods(node)
        self.output.write('}\n\n')

        write_export(node, node.name, self.output)

    def emitMethods(self, node):
        letter = node.name[0].lower()
        globals = dict(
            struct_name=node.name,
            letter=letter)

        # Constructor function
        constructor_args = ''
        constructor_inits = ''
        if node.members():
            visitor = MemberVisitor(output=self.output)
            for member in node.members():
                if member.init:
                    self.print_one_default_warning()
                constructor_args += (member.name + ', ')
                constructor_inits += '\tthis.{name} = {name};\n'.format(name=member.name)

        constructor_args = constructor_args[:-2]

        self.output.write('constructor({args}) {{\n\tsuper();\n{inits}}}\n\n'.format(args=constructor_args, inits=constructor_inits))

        # Type function
        self.output.write('type() {\n')
        self.output.write('\treturn \"{struct_name}\";\n'.format(struct_name=node.name))
        self.output.write('}\n\n')

        # Size function
        self.output.write('get size() {\n')
        if node.members():
            self.output.write('\tlet result = 0;\n')
            with self.output.indent(1):
                visitor = SizeVisitor(letter, self.output)
                for member in node.members():
                    visitor.visit(member.type, member_name=member.name)
            self.output.write('\treturn result;\n')
        else:
            self.output.write('\treturn 0\n')
        self.output.write('}\n\n')

        # Unpack function
        self.output.write('unpack(buffer) {\n')
        with self.output.indent(1):
            self.output.write('let cladBuffer = new CladBuffer(buffer);\n')
            if node.members():
                visitor = UnpackVisitor(letter, self.output)
                for member in node.members():
                    visitor.visit(member.type, member_name=member.name)
        self.output.write('}\n\n')

        # Pack function
        self.output.write('pack() {{\n'.format(**globals))
        with self.output.indent(1):
            self.output.write('let buffer = new Uint8Array(this.size);\n')
            self.output.write('let cladBuffer = new CladBuffer(buffer);\n\n')
            self.output.write('try {\n')
            with self.output.indent(1):
              if node.members():
                  visitor = PackVisitor(letter, self.output)
                  for member in node.members():
                      visitor.visit(member.type, member_name=member.name)
            self.output.write('}\n')
            self.output.write('catch {\n')
            self.output.write('\treturn null;\n')
            self.output.write('}\n')
            self.output.write('return cladBuffer.buffer;\n')

        self.output.write('}\n\n')

        # String function
        self.output.write('string() {\n')
        if node.members():
            with self.output.indent(1):
                self.output.write('return JSON.stringify(this);\n')
        else:
            self.output.write('\treturn ""\n')
        self.output.write('}\n')


class TypeVisitor(ast.NodeVisitor):
    def __init__(self, output):
        self.output = output

    def visit_PascalStringType(self, member):
        self.output.write("string")

    def visit_BuiltinType(self, node):
        self.output.write("{type}".format(type=type_translations[node.name]))

    def visit_DefinedType(self, node):
        self.output.write("{type}".format(type=node.name))

    def visit_CompoundType(self, node):
        self.output.write("{type}".format(type=node.name))

    def visit_VariableArrayType(self, node):
        self.output.write("[]")
        self.visit(node.member_type)

    def visit_FixedArrayType(self, node):
        self.output.write("[{fixed_length}]".format(fixed_length=node.length))
        self.visit(node.member_type)


class StringVisitor(ast.NodeVisitor):
    def __init__(self, letter):
        self.args = []
        self.letter = letter

    def default_visit(self, node, member_name, is_last):
        thisargs = []
        thisargs.append('"{}: {{"'.format(member_name))
        thisargs.append('{letter}.{member_name}'.format(letter=self.letter, member_name=member_name))
        thisargs.append('"}}{}"'.format(' ' if not is_last else ''))
        self.args.append(thisargs)

    def visit_PascalStringType(self, node, member_name, is_last):
        self.default_visit(node, member_name, is_last)

    def visit_BuiltinType(self, node, member_name, is_last):
        self.default_visit(node, member_name, is_last)

    def visit_DefinedType(self, node, member_name, is_last):
        self.default_visit(node, member_name, is_last)

    def visit_CompoundType(self, node, member_name, is_last):
        self.default_visit(node, member_name, is_last)

    def visit_VariableArrayType(self, node, member_name, is_last):
        self.default_visit(node, member_name, is_last)

    def visit_FixedArrayType(self, node, member_name, is_last):
        self.default_visit(node, member_name, is_last)


class MemberVisitor(ast.NodeVisitor):
    def __init__(self, output):
        self.output = output

    def default_visit(self, node, member_name):
        self.output.write("{member_name} ".format(member_name=member_name))
        TypeVisitor(self.output).visit(node)

    def simple_print(self, member_name, type_name):
        self.output.write("{member_name} {type_name}".format(
            member_name=member_name, type_name=type_name))

    def visit_PascalStringType(self, node, member_name):
        self.default_visit(node, member_name)

    def visit_BuiltinType(self, node, member_name):
        self.simple_print(member_name, type_translations[node.name])

    def visit_DefinedType(self, node, member_name):
        self.simple_print(member_name, node.name)

    def visit_CompoundType(self, node, member_name):
        self.simple_print(member_name, node.name)

    def visit_VariableArrayType(self, node, member_name):
        self.default_visit(node, member_name)

    def visit_FixedArrayType(self, node, member_name):
        self.default_visit(node, member_name)


class SizeVisitor(ast.NodeVisitor):
    def __init__(self, var, output):
        self.var = var
        self.output = output

    def visit_PascalStringType(self, node, member_name):
        self.visit_VariableArrayType(node, member_name)

    def visit_BuiltinType(self, node, member_name):
        self.output.write('result += {fixed_size}; // {member_name} {type}\n'.format(
            fixed_size=node.max_message_size(), member_name=member_name, type=node.name))

    def visit_DefinedType(self, node, member_name):
        self.output.write('result += {fixed_size}; // {member_name} {type}\n'.format(
            fixed_size=node.max_message_size(), member_name=member_name, type=node.name))

    def visit_CompoundType(self, node, member_name):
        self.output.write('result += this.{member_name}.size;\n'.format(member_name=member_name))

    def print_array_size(self, node, member_name, length_if_fixed):
        if node.member_type.is_message_size_fixed():
            item_size = node.member_type.max_message_size()
            multiply_str = ' * {fixed_size}'.format(fixed_size=item_size) if item_size != 1 else ''
            comment_str = ' // {type} array'.format(type=node.member_type.name)

            self.output.write('result += {length_if_fixed}{multiply_str};{comment}\n'.format(
                length_if_fixed=length_if_fixed,
                multiply_str=multiply_str, comment=comment_str))
        else:
            self.output.write('for(let i = 0; i < this.{member_name}.length; i++) {{\n'.format(member_name=member_name))
            with self.output.indent(1):
                self.visit(node.member_type, member_name='{}[i]'.format(member_name))
            self.output.write('}\n')

    def visit_VariableArrayType(self, node, member_name):
        self.output.write('result += {fixed_size}; // {member_name} length ({type})\n'.format(
            fixed_size=node.length_type.max_message_size(), member_name=member_name, type=node.length_type.name))
        self.print_array_size(node, member_name, 'this.{member_name}.length'.format(member_name=member_name))

    def visit_FixedArrayType(self, node, member_name):
        length=node.length
        if isinstance(length, str):
            # a string could represent a non-uint32 type
            length = '{}'.format(length)
        self.print_array_size(node, member_name, '{}'.format(length))

def if_error_return(output, expression):
    output.write('if err := {expr}; err != nil {{\n\treturn err\n}}\n'.format(expr=expression))

def if_blank_error_return(output, expression):
    output.write('if _, err := {expr}; err != nil {{\n\treturn err\n}}\n'.format(expr=expression))


class PackVisitor(ast.NodeVisitor):
    def __init__(self, var, output):
        self.var = var
        self.output = output

    def write_array_length(self, node, member_name):
        if_error_return(self.output, 'binary.Write(buf, binary.LittleEndian, {type}(len({var}.{member_name})))'.format(
            type=type_translations[node.length_type.name], var=self.var, member_name=member_name))

    def simple_write(self, node, member_name):
        base_type = node.name

        if (hasattr(node, 'underlying_type')):
            # is enum, so get underlying
            base_type = node.underlying_type.storage_type.name

        self.output.write('cladBuffer.write{type}(this.{member_name});\n'.format(
          member_name = member_name,
          type = type_translations[base_type]
        ))

    def length_check(self, node, member_name):
        # avoid int overflow error - don't check length if max size overflows int
        if node.max_length > 2147483647:
            return
        self.output.write('if(this.{member_name}.length > {max_size}) {{\n'.format(
            var=self.var, member_name=member_name, max_size=node.max_length))
        self.output.write('\tbuffer = null;\n\treturn;\n'.format(
            member_name=member_name))
        self.output.write('}\n')

    def visit_PascalStringType(self, node, member_name):
        self.length_check(node, member_name)
        self.output.write('cladBuffer.writeString(this.{member_name}, {size_length});\n'.format(
          member_name = member_name,
          size_length = node.length_type.max_message_size()
        ))

    def visit_BuiltinType(self, node, member_name):
        self.simple_write(node, member_name)

    def visit_DefinedType(self, node, member_name):
        self.simple_write(node, member_name)

    def visit_CompoundType(self, node, member_name):
        self.output.write('cladBuffer.write(this.{member_name}.pack());\n'.format(
          member_name = member_name
        ))

    def pack_array(self, node, member_name):
        if node.member_type.is_message_size_fixed():
            if_error_return(self.output, 'binary.Write(buf, binary.LittleEndian, {var}.{member_name})'.format(
                var=self.var, member_name=member_name))
        else:
            self.output.write('for idx := range {var}.{member_name} {{\n'.format(
                var=self.var, member_name=member_name))
            with self.output.indent(1):
                self.visit(node.member_type, member_name='{}[idx]'.format(member_name))
            self.output.write('}\n')

    def visit_VariableArrayType(self, node, member_name):
        is_string = isinstance(node.member_type, ast.PascalStringType)
        is_compound = hasattr(node.member_type, 'type_decl')

        if is_compound:
          self.output.write('cladBuffer.write{type}(this.{member_name}.length);\n'.format(
            type = type_translations[node.length_type.name],
            member_name = member_name
          ))
          self.output.write('for(let i = 0; i < this.{member_name}.length; i++) {{\n'.format(member_name=member_name))
          self.output.write('\tcladBuffer.write(this.{member_name}[i].pack());\n'.format(member_name=member_name))
          self.output.write('}\n')
          return

        if is_string:
            self.output.write('cladBuffer.writeStringVArray(this.{member_name}, {length}, {type_size});\n'.format(
              member_name = member_name,
              length = node.length_type.max_message_size(),
              type_size = 1
            ))
        else:
            self.output.write('cladBuffer.writeVArray(this.{member_name}, {type_size});\n'.format(
              member_name = member_name,
              type_size = node.length_type.max_message_size()
            ))


    def visit_FixedArrayType(self, node, member_name):
        is_string = isinstance(node.member_type, ast.PascalStringType)
        is_compound = hasattr(node.member_type, 'type_decl')

        if is_compound:
            self.output.write('for(let i = 0; i < {length}; i++) {{\n'.format(length=node.length))
            self.output.write('\tcladBuffer.write(this.{member_name}[i].pack());\n'.format(member_name=member_name))
            self.output.write('}\n')
            return

        if is_string:
            self.output.write('cladBuffer.writeStringFArray(this.{member_name}, {length}, {type_size});\n'.format(
              member_name = member_name,
              length = node.length,
              type_size = 1
            ))
        else:
            self.output.write('cladBuffer.writeFArray(this.{member_name});\n'.format(
              member_name=member_name))

class UnpackVisitor(ast.NodeVisitor):
    def __init__(self, var, output):
        self.var = var
        self.output = output

    def simple_read(self, node, member_name):
        base_type = node.name

        if (hasattr(node, 'underlying_type')):
            # is enum, so get underlying
            base_type = node.underlying_type.storage_type.name

        self.output.write('this.{member_name} = cladBuffer.read{type}();\n'.format(
            member_name=member_name, type=type_translations[base_type]))

    def visit_PascalStringType(self, node, member_name):
        self.output.write('this.{member_name} = cladBuffer.readString({type_size});\n'.format(
            member_name=member_name, type_size=node.length_type.max_message_size()))

    def visit_BuiltinType(self, node, member_name):
        self.simple_read(node, member_name)

    def visit_DefinedType(self, node, member_name):
        self.simple_read(node, member_name)

    def visit_CompoundType(self, node, member_name):
        self.output.write('this.{member_name} = new {type}(null);\nthis.{member_name}.unpackFromClad(cladBuffer);\n'.format(
            member_name=member_name,
            type=get_namespace_variable(node.type_decl, False)
          ))

    def visit_CompoundArray(self, node, member_name, length):
      self.output.write('this.{member_name} = [];\nfor(let i = 0; i < {length}; i++) {{\n'.format(
        member_name=member_name,
        length=length
      ))

      with self.output.indent(1):
        self.output.write('let {member_name}New = new {type}(null);\n'.format(member_name=member_name, type=get_namespace_variable(node.member_type.type_decl, False)))
        self.output.write('{member_name}New.unpackFromClad(cladBuffer);\n'.format(member_name=member_name))
        self.output.write('this.{member_name}.push({member_name}New);\n'.format(member_name=member_name))
      self.output.write('}\n')

    def visit_VariableArrayType(self, node, member_name):
        is_string = isinstance(node.member_type, ast.PascalStringType)
        is_compound = hasattr(node.member_type, 'type_decl')
        read_method = 'readVArray'
        element_size=node.member_type.max_message_size()
        is_float_type = 'float' in node.member_type.name
        is_type_signed = node.member_type.name [:1] != 'u'

        if is_compound:
          self.output.write('let {member_name}Length = cladBuffer.read{type}();\n'.format(
            member_name=member_name,
            type=type_translations[node.length_type.name]
          ))

          self.visit_CompoundArray(node, member_name, '{member_name}Length'.format(member_name=member_name))
          return

        if is_string:
            read_method = 'readStringVArray'
            element_size = 1

        self.output.write('this.{member_name} = cladBuffer.{read_method}({is_float}, {element_size}, {array_size}{is_signed});\n'
              .format(
                is_float='true' if is_float_type else 'false',
                read_method=read_method,
                member_name=member_name, 
                element_size=element_size,
                array_size=node.length_type.max_message_size(),
                is_signed='' if is_string else (', true' if is_type_signed else ', false')))

    def visit_FixedArrayType(self, node, member_name):
        is_string = isinstance(node.member_type, ast.PascalStringType)
        is_compound = hasattr(node.member_type, 'type_decl')
        read_method = 'readFArray'
        element_size=node.member_type.max_message_size()
        is_float_type = 'float' in node.member_type.name
        is_type_signed = node.member_type.name[:1] != 'u'

        if is_compound:
          self.visit_CompoundArray(node, member_name, node.length)
          return

        if is_string:
            read_method = 'readStringFArray'
            element_size = 1
        
        self.output.write('this.{member_name} = cladBuffer.{read_method}({is_float}, {element_size}, {array_size}{is_signed});\n'
            .format(
              is_float='true' if is_float_type else 'false',
              read_method=read_method,
              member_name=member_name, 
              element_size=element_size, 
              array_size=node.length,
              is_signed='' if is_string else (', true' if is_type_signed else ', false')))


class UnionEmitter(ast.NodeVisitor):
    def __init__(self, output):
        self.output = output

    def visit_UnionDecl(self, node):

        globals = dict(
            union_name=node.name,
            max_size=node.max_message_size(),
            tag_name='{}Tag'.format(node.name),
            tag_member='tag')

        self.output.write('// UNION {union_name}\n'.format(**globals))
        self.emitEnum(node, globals)
        self.emitStruct(node, globals)
        self.output.write('\n')

    def emitEnum(self, node, _globals):
        self.output.write('{type}Tag = Object.freeze({{\n'.format(type=get_namespace_variable(node, True)))

        # if a member has an explicit value, use it
        # if a member doesn't have an explicit value 
        #   increment the previous known value (or 0 if None)
        starts = []
        ends = []
        need_explicit = False
        last_member = None
        last_value = -1
        for i, member in enumerate(node.members()):
            start = '\t{member_name}:'.format(member_name=member.name)

            if member.init:
                enum_val = member.init.value
                initializer = '0x{:x}'.format(enum_val) if member.init.type == "hex" else str(enum_val)
                start += initializer
                last_member = member
                last_value = enum_val
            else:
                initializer = '0x{:x}'.format(last_value + 1)
                start += initializer
                last_value = last_value + 1

            start += ','

            end = '\n'

            starts.append(start)
            ends.append(end)

        starts.append('\tINVALID:0xFF')
        ends.append('\n')

        full_length = max(len(start) for start in starts)
        for start, end in zip(starts, ends):
            self.output.write(start)
            self.output.write(' ' * (full_length - len(start)))
            self.output.write(end)

        self.output.write('});\n\n')

        write_export(node, node.name + 'Tag', self.output)

    def emitStruct(self, node, globals):
        self.output.write('{n} = class extends Clad {{\n'.format(n=get_namespace_variable(node, True)))
        with self.output.indent(1):
            self.emitMethods(node, globals)

        self.output.write('}\n\n')

        write_export(node, node.name, self.output)

    def emitMethods(self, node, globals):
        self.output.write('constructor() {\n')
        with self.output.indent(1):
            self.output.write('super();\n')
            self.output.write('this._tag = {n}Tag.INVALID;\n'.format(n=get_namespace_variable(node, False)))
        self.output.write('}\n\n')

        self.output.write('get tag() {\n')
        with self.output.indent(1):
            self.output.write('if(this._tag == null) {\n')
            self.output.write('\treturn {n}Tag.INVALID;\n'.format(n=get_namespace_variable(node, False)))
            self.output.write('}\n')
            self.output.write('return this._tag;\n')
        self.output.write('}\n\n')

        self.output.write('get size() {\n')
        with self.output.indent(1):
            self.output.write('if(this._tag == null || this._tag == {n}Tag.INVALID) {{\n'.format(n=get_namespace_variable(node, False)))
            self.output.write('\treturn 1;\n')
            self.output.write('}\n')
            self.output.write('return 1 + this.value.size;\n')
        self.output.write('}\n\n')

        self.output.write('pack() {\n')
        with self.output.indent(1):
            self.output.write('if(this._tag == {n}Tag.INVALID) {{\n'.format(n=get_namespace_variable(node, False)))
            self.output.write('\treturn null;\n')
            self.output.write('}\n')
            self.output.write('let buffer = new Uint8Array(this.size);\n')
            self.output.write('// add tag\nbuffer.set([this._tag], 0);\n')
            self.output.write('// add message\nbuffer.set(this.value.pack(), 1);\n')
            self.output.write('return buffer;\n')
        self.output.write('}\n\n')

        self.output.write('unpackStructure(tag, buffer) {\n')
        with self.output.indent(1):
            self.output.write('let ret = null;\n')
            self.output.write('switch(tag) {\n')
            for member in node.members():
                self.output.write('case {n}Tag.{member_name}:\n'.format(
                    member_name=member.name, n=get_namespace_variable(node, False)))
                self.output.write('\tret = new {type}();\n'.format(type=get_namespace_variable(member.type, False)))
                self.output.write('\tret.unpack(buffer);\n')
                self.output.write('\tthis.value = ret;\n')
                self.output.write('\treturn ret;\n')
            self.output.write('default:\n')
            self.output.write('\treturn ret;\n')
            self.output.write('}\n')
        self.output.write('}\n\n')

        self.output.write('unpack(buffer) {\n')
        with self.output.indent(1):
            self.output.write('this._tag = {n}Tag.INVALID;\n'.format(n=get_namespace_variable(node, False)))
            self.output.write('if(buffer.length == 0) {\n')
            self.output.write('\t// error case\n\treturn null;\n')
            self.output.write('}\n')
            self.output.write('this._tag = buffer[0];\n')
            self.output.write('if(this._tag == {n}Tag.INVALID) {{\n'.format(n=get_namespace_variable(node, False)))
            self.output.write('\treturn null;\n')
            self.output.write('}\n')
            self.output.write('return this.unpackStructure(this._tag, buffer.slice(1));\n')
        self.output.write('}\n\n')

        self.output.write('string() {\n')
        with self.output.indent(1):
            self.output.write('if(this._tag == null) {\n')
            self.output.write('\treturn "null";\n}\n')
            self.output.write('if(this._tag == {n}Tag.INVALID) {{\n'.format(n=get_namespace_variable(node, False)))
            self.output.write('\treturn "INVALID";\n}\n')
            self.output.write('return JSON.stringify(this);\n')
        self.output.write('}\n\n')

        for member in node.members():
            member_globals = dict(
                member_name = member.name,
                member_type = member.type,
                **globals
            )

            self.output.write('get{member_name}() {{\n'.format(
                **member_globals))
            with self.output.indent(1):
                self.output.write('if(this.tag != {n}Tag.{member_name}) {{\n'.format(
                    n=get_namespace_variable(node, False), **member_globals))
                self.output.write('\treturn null;\n}\n')
                self.output.write('return this.value;\n'.format(**member_globals))
            self.output.write('}\n\n')

            self.output.write('static New{union_name}With{member_name}(value) {{\n'.format(
                **member_globals))
            with self.output.indent(1):
                self.output.write('let m = new {n}();\n'.format(n=get_namespace_variable(node, False)))
                self.output.write('m._tag = {n}Tag.{member_name};\n'.format(n=get_namespace_variable(node, False), **member_globals))
                self.output.write('m.value = value;\n')
                self.output.write('return m;\n')
            self.output.write('}\n\n')


class OptionDetector(ast.NodeVisitor):
    def __init__(self, options):
        self.options = options
        self.options['use_binary'] = False
        self.options['use_errors'] = False
        self.options['use_bytes'] = False
        self.options['use_clad'] = False
        self.options['use_fmt'] = False

    def visit_MessageDecl(self, node, *args, **kwargs):
        self.options['use_bytes'] = True
        if len(node.members()) > 0:
            self.options['use_binary'] = True
            self.options['use_fmt'] = True
        for member in node.members():
            self.visit(member.type, member_name=member.name)

    def visit_UnionDecl(self, node, *args, **kwargs):
        self.options['use_clad'] = True
        self.options['use_bytes'] = True
        self.options['use_binary'] = True
        self.options['use_errors'] = True
        self.options['use_fmt'] = True

    def visit_PascalStringType(self, node, *args, **kwargs):
        self.options['use_errors'] = True

    def visit_VariableArrayType(self, node, *args, **kwargs):
        self.options['use_errors'] = True

if __name__ == '__main__':
    option_parser = emitterutil.StandardArgumentParser('JS')
    option_parser.add_argument('--package', default=None,
        help='The name of the package to be generated from this file')
    options = option_parser.parse_args()
    input_file = options.input_file

    emitterutil.js_main(NamespaceEmitter, options, scanner=OptionDetector)
