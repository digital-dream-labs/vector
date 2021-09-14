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

def goify(arg):
    if not isinstance(arg, str):
        return arg
    arg = arg.replace('::', '_')
    return ''.join([s[:1].upper() + s[1:] for s in arg.split('_')])

type_translations = {
    'bool': 'bool',
    'int_8': 'int8',
    'int_16': 'int16',
    'int_32': 'int32',
    'int_64': 'int64',
    'uint_8': 'uint8',
    'uint_16': 'uint16',
    'uint_32': 'uint32',
    'uint_64': 'uint64',
    'float_32': 'float32',
    'float_64': 'float64'
}

input_file = None
defaults_warned = False

def get_visitor_output(visitorType, node):
    _py3 = sys.version_info[0] >= 3
    output = io.StringIO() if _py3 else io.BytesIO()
    visitorType(output).visit(node)
    return output.getvalue()

class GlobalEmitter(ast.NodeVisitor):

    def __init__(self, output):
        self.output = output
        self.imported = set()

    def visit_IncludeDecl(self, node, *args, **kwargs):
        # only need to import if it's not part of the same package
        #self.output.write('import "{0}"\n'.format(node.name))
        direct = os.path.split(node.name)[0]
        import_stmt = 'import . "{0}"\n'.format(direct)
        inp_f = "clad" + os.path.split(input_file)[0].split('clad', 1)[-1]
        if import_stmt not in self.imported and direct != inp_f:
            self.output.write(import_stmt)
            self.imported.add(import_stmt)
        pass

    def visit_EnumDecl(self, node, *args, **kwargs):
        EnumEmitter(self.output).visit(node, *args, **kwargs)

    def visit_MessageDecl(self, node, *args, **kwargs):
        StructEmitter(self.output).visit(node, *args, **kwargs)

    def visit_UnionDecl(self, node, *args, **kwargs):
        UnionEmitter(self.output).visit(node, *args, **kwargs)
        pass

class EnumEmitter(ast.NodeVisitor):
    def __init__(self, output):
        self.output = output

    def visit_EnumDecl(self, node):
        # print the header
        self.output.write('// ENUM {enum_name}\n'.format(enum_name=node.name))
        self.output.write('type {type} {storage}\n\n'.format(type=node.name,
            storage=type_translations[node.storage_type.builtin_type().name]))
        self.output.write('const (\n')

        starts = []
        ends = []
        need_explicit = False
        last_member = None
        for i, member in enumerate(node.members()):
            start = '\t{enum_name}_{member_name}'.format(enum_name=node.name, member_name=member.name)
            if member.initializer:
                enum_val = member.initializer.value
                initializer = '0x{:x}'.format(enum_val) if member.initializer.type == "hex" else str(enum_val)
                start += ' {type} = {type}({initializer})'.format(type=node.name, initializer=goify(initializer))
                need_explicit = True # initializers for members mess up Go's ability to auto-increment
                last_member = member
            elif i == 0:
                start += ' {type} = iota'.format(type=node.name)
            elif need_explicit:
                start += ' {type} = {enum_name}_{last} + 1'.format(
                    type=node.name, enum_name=node.name, last=last_member.name)
                last_member = member
            end = '\n'

            starts.append(start)
            ends.append(end)

        full_length = max(len(start) for start in starts)
        for start, end in zip(starts, ends):
            self.output.write(start)
            self.output.write(' ' * (full_length - len(start)))
            self.output.write(end)

        self.output.write(')\n\n')



class StructEmitter(ast.NodeVisitor):
    def __init__(self, output):
        self.output = output

    def visit_MessageDecl(self, node):
        self.emitStruct(node)
        self.emitMethods(node)

        self.output.write('\n')

    def emitStruct(self, node):
        self.output.write('// {obj_type} {struct_name}\n'.format(
            obj_type=node.object_type().upper(), struct_name=node.name))
        if node.name != goify(node.name):
            # have to export lowercase classes
            self.output.write('type {alias} = {struct_name}\n\n'.format(alias=goify(node.name), struct_name=node.name))
        self.output.write('type {struct_name} struct {{\n'.format(struct_name=node.name))

        if node.members():
            visitor = MemberVisitor(output=self.output)
            for member in node.members():
                if member.init:
                    # TODO: handle defaults gracefully
                    global defaults_warned
                    if not defaults_warned:
                        defaults_warned = True
                        print("Warning: The Go emitter does not support default values for members. They have been ignored.")
                    pass
                self.output.write('\t')
                visitor.visit(member.type, member_name=goify(member.name))
                self.output.write('\n')
        self.output.write('}\n\n')

    def emitMethods(self, node):
        letter = node.name[0].lower()
        globals = dict(
            struct_name=node.name,
            letter=letter)

        # Size function
        self.output.write('func ({letter} *{struct_name}) Size() uint32 {{\n'.format(**globals))
        if node.members():
            self.output.write('\tvar result uint32\n'.format(**globals))
            with self.output.indent(1):
                visitor = SizeVisitor(letter, self.output)
                for member in node.members():
                    visitor.visit(member.type, member_name=goify(member.name))
            self.output.write('\treturn result\n')
        else:
            self.output.write('\treturn 0\n')
        self.output.write('}\n\n')

        # Unpack function
        self.output.write('func ({letter} *{struct_name}) Unpack(buf *bytes.Buffer) error {{\n'.format(**globals))
        with self.output.indent(1):
            if node.members():
                visitor = UnpackVisitor(letter, self.output)
                for member in node.members():
                    visitor.visit(member.type, member_name=goify(member.name))
        self.output.write('\treturn nil\n}\n\n')

        # Pack function
        self.output.write('func ({letter} *{struct_name}) Pack(buf *bytes.Buffer) error {{\n'.format(**globals))
        with self.output.indent(1):
            if node.members():
                visitor = PackVisitor(letter, self.output)
                for member in node.members():
                    visitor.visit(member.type, member_name=goify(member.name))
        self.output.write('\treturn nil\n}\n\n')

        # String function
        self.output.write('func ({letter} *{struct_name}) String() string {{\n'.format(**globals))
        if node.members():
            with self.output.indent(1):
                visitor = StringVisitor(letter)
                for i, member in enumerate(node.members()):
                    visitor.visit(member.type, member_name=goify(member.name), is_last=(i == len(node.members()) - 1))
                # visitor.args is an array of sub-arrays
                # first join individual sub-arrays into individual strings...
                args_str = [', '.join(x) for x in visitor.args]
                # then join the array of strings into one newline-separated string
                args_str = ',\n\t'.join(args_str)
                self.output.write('return fmt.Sprint({})\n'.format(args_str))
        else:
            self.output.write('\treturn ""\n')
        self.output.write('}\n\n')


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
        self.output.write("[{fixed_length}]".format(fixed_length=goify(node.length)))
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
        self.output.write('result += {fixed_size} // {member_name} {type}\n'.format(
            fixed_size=node.max_message_size(), member_name=member_name, type=node.name))

    def visit_DefinedType(self, node, member_name):
        self.output.write('result += {fixed_size} // {member_name} {type}\n'.format(
            fixed_size=node.max_message_size(), member_name=member_name, type=node.name))

    def visit_CompoundType(self, node, member_name):
        self.output.write('result += {var}.{member_name}.Size()\n'.format(
            var=self.var, member_name=member_name))

    def print_array_size(self, node, member_name, length_if_fixed):
        if node.member_type.is_message_size_fixed():
            item_size = node.member_type.max_message_size()
            multiply_str = ' * {fixed_size}'.format(fixed_size=item_size) if item_size != 1 else ''
            comment_str = ' // {type} array'.format(type=node.member_type.name)

            self.output.write('result += {length_if_fixed}{multiply_str}{comment}\n'.format(
                member_name=member_name, length_if_fixed=length_if_fixed,
                multiply_str=multiply_str, comment=comment_str))
        else:
            self.output.write('for idx := range {var}.{member_name} {{\n'.format(
                var=self.var, member_name=member_name))
            with self.output.indent(1):
                self.visit(node.member_type, member_name='{}[idx]'.format(member_name))
            self.output.write('}\n')

    def visit_VariableArrayType(self, node, member_name):
        self.output.write('result += {fixed_size} // {member_name} length ({type})\n'.format(
            fixed_size=node.length_type.max_message_size(), member_name=member_name, type=node.length_type.name))
        self.print_array_size(node, member_name, 'uint32(len({var}.{member_name}))'.format(
            var=self.var, member_name=member_name))

    def visit_FixedArrayType(self, node, member_name):
        length=goify(node.length)
        if isinstance(length, str):
            # a string could represent a non-uint32 type
            length = 'uint32({})'.format(length)
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
        if_error_return(self.output, 'binary.Write(buf, binary.LittleEndian, {var}.{member_name})'.format(
            var=self.var, member_name=member_name))

    def length_check(self, node, member_name):
        # avoid int overflow error - don't check length if max size overflows int
        if node.max_length > 2147483647:
            return
        self.output.write('if len({var}.{member_name}) > {max_size} {{\n'.format(
            var=self.var, member_name=member_name, max_size=node.max_length))
        self.output.write('\treturn errors.New("max_length overflow in field {member_name}")\n'.format(
            member_name=member_name))
        self.output.write('}\n')

    def visit_PascalStringType(self, node, member_name):
        self.length_check(node, member_name)
        self.write_array_length(node, member_name)
        if_blank_error_return(self.output, 'buf.WriteString({var}.{member_name})'.format(
            var=self.var, member_name=member_name))

    def visit_BuiltinType(self, node, member_name):
        self.simple_write(node, member_name)

    def visit_DefinedType(self, node, member_name):
        self.simple_write(node, member_name)

    def visit_CompoundType(self, node, member_name):
        if_error_return(self.output, '{var}.{member_name}.Pack(buf)'.format(
            var=self.var, member_name=member_name))

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
        self.length_check(node, member_name)
        self.write_array_length(node, member_name)
        self.pack_array(node, member_name)

    def visit_FixedArrayType(self, node, member_name):
        self.pack_array(node, member_name)


class UnpackVisitor(ast.NodeVisitor):
    def __init__(self, var, output):
        self.var = var
        self.output = output

    def write_array_length(self, node, member_name):
        self.output.write('var {member_name}Len {len_type}\n'.format(
            member_name=member_name, len_type=type_translations[node.length_type.name]))
        if_error_return(self.output, 'binary.Read(buf, binary.LittleEndian, &{member_name}Len)'.format(
            member_name=member_name))

    def simple_read(self, node, member_name):
        if_error_return(self.output, 'binary.Read(buf, binary.LittleEndian, &{var}.{member_name})'.format(
            var=self.var, member_name=member_name))

    def visit_PascalStringType(self, node, member_name):
        clean_name = member_name.replace('[', '').replace(']', '')
        self.write_array_length(node, clean_name)
        self.output.write('{var}.{member_name} = string(buf.Next(int({clean_name}Len)))\n'.format(
            var=self.var, member_name=member_name, clean_name=clean_name))
        self.output.write('if len({var}.{member_name}) != int({clean_name}Len) {{\n'.format(
            var=self.var, member_name=member_name, clean_name=clean_name))
        self.output.write('\treturn errors.New("string byte mismatch")\n}\n')

    def visit_BuiltinType(self, node, member_name):
        self.simple_read(node, member_name)

    def visit_DefinedType(self, node, member_name):
        self.simple_read(node, member_name)

    def visit_CompoundType(self, node, member_name):
        if_error_return(self.output, '{var}.{member_name}.Unpack(buf)'.format(
            var=self.var, member_name=member_name))

    def unpack_array(self, node, member_name):
        if node.member_type.is_message_size_fixed():
            if_error_return(self.output, 'binary.Read(buf, binary.LittleEndian, &{var}.{member_name})'.format(
                var=self.var, member_name=member_name))
        else:
            self.output.write('for idx := range {var}.{member_name} {{\n'.format(
                var=self.var, member_name=member_name))
            with self.output.indent(1):
                self.visit(node.member_type, member_name='{}[idx]'.format(member_name))
            self.output.write('}\n')

    def visit_VariableArrayType(self, node, member_name):
        self.write_array_length(node, member_name)
        self.output.write('{var}.{member_name} = make([]{type}, {member_name}Len)\n'.format(
            var=self.var, member_name=member_name, type=get_visitor_output(TypeVisitor, node.member_type)))
        self.unpack_array(node, member_name)

    def visit_FixedArrayType(self, node, member_name):
        self.unpack_array(node, member_name)


class UnionEmitter(ast.NodeVisitor):
    def __init__(self, output):
        self.output = output

    def visit_UnionDecl(self, node):

        globals = dict(
            union_name=node.name,
            max_size=node.max_message_size(),
            tag_name='{0}Tag'.format(node.name),
            tag_member='tag')

        self.output.write('// UNION {union_name}\n'.format(**globals))
        self.emitEnum(node, globals)
        self.emitStruct(node, globals)
        self.emitMethods(node, globals)
        self.output.write('\n')

    def emitEnum(self, node, globals):
        self.output.write('type {tag_name} uint8\n\n'.format(**globals))
        self.output.write('const (\n')

        with self.output.indent(1):
            lines = []
            need_init = False
            last = None
            for i, member in enumerate(node.members()):
                start = '{tag_name}_{member_name}'.format(member_name=goify(member.name), **globals)
                if member.init:
                    initializer = '0x{:x}'.format(member.tag) if member.init.type == "hex" else str(member.tag)
                    middle = ' {tag_name} = {initializer}'.format(initializer=initializer, **globals)
                    need_init = True
                    last = start
                elif i == 0:
                    middle = ' {tag_name} = iota'.format(**globals)
                elif need_init:
                    middle = ' {tag_name} = {last} + 1'.format(last=last, **globals)
                    last = start
                else:
                    middle = ''
                end = ' // {value}'.format(value=member.tag)
                lines.append((start, middle, end))

            start = '{tag_name}_INVALID'.format(**globals)
            middle = ' {tag_name} = {invalid_tag}'.format(invalid_tag=node.invalid_tag, **globals)
            lines.append((start, middle))
            self.output.write_with_aligned_whitespace(lines)

        self.output.write(')\n\n')

    def emitStruct(self, node, globals):
        self.output.write('type {union_name} struct {{\n'.format(**globals))
        self.output.write('\t{tag_member} *{tag_name}\n'.format(**globals))
        self.output.write('\tvalue clad.Struct\n')
        self.output.write('}\n\n')

    def emitMethods(self, node, globals):

        self.output.write('func (m *{union_name}) Tag() {union_name}Tag {{\n'.format(**globals))
        with self.output.indent(1):
            self.output.write('if m.tag == nil {\n')
            self.output.write('\treturn {union_name}Tag_INVALID\n'.format(**globals))
            self.output.write('}\n')
            self.output.write('return *m.tag\n')
        self.output.write('}\n\n')

        self.output.write('func (m *{union_name}) Size() uint32 {{\n'.format(**globals))
        with self.output.indent(1):
            self.output.write('if m.tag == nil || *m.tag == {tag_name}_INVALID {{\n'.format(**globals))
            self.output.write('\treturn 1\n')
            self.output.write('}\n')
            self.output.write('return 1 + m.value.Size()\n')
        self.output.write('}\n\n')

        self.output.write('func (m *{union_name}) Pack(buf *bytes.Buffer) error {{\n'.format(**globals))
        with self.output.indent(1):
            self.output.write('tag := {tag_name}_INVALID\n'.format(**globals))
            self.output.write('if m.tag != nil {\n')
            self.output.write('\ttag = *m.tag\n')
            self.output.write('}\n')
            if_error_return(self.output, 'binary.Write(buf, binary.LittleEndian, tag)')
            self.output.write('if tag == {tag_name}_INVALID {{\n'.format(**globals))
            self.output.write('\treturn nil\n')
            self.output.write('}\n')
            self.output.write('return m.value.Pack(buf)\n')
        self.output.write('}\n\n')

        self.output.write('func (m *{union_name}) unpackStruct(tag {tag_name}'.format(**globals))
        self.output.write(', buf *bytes.Buffer) (clad.Struct, error) {\n')
        with self.output.indent(1):
            self.output.write('switch tag {\n')
            for member in node.members():
                self.output.write('case {tag_name}_{member_name}:\n'.format(
                    member_name=goify(member.name), **globals))
                self.output.write('\tvar ret {type}\n'.format(type=member.type))
                self.output.write('\tif err := ret.Unpack(buf); err != nil {\n')
                self.output.write('\t\treturn nil, err\n')
                self.output.write('\t}\n')
                self.output.write('\treturn &ret, nil\n')
            self.output.write('default:\n')
            self.output.write('\treturn nil, errors.New("invalid tag to unpackStruct")\n')
            self.output.write('}\n')
        self.output.write('}\n\n')

        self.output.write('func (m *{union_name}) Unpack(buf *bytes.Buffer) error {{\n'.format(**globals))
        with self.output.indent(1):
            self.output.write('tag := {tag_name}_INVALID\n'.format(**globals))
            if_error_return(self.output, 'binary.Read(buf, binary.LittleEndian, &tag)')
            self.output.write('m.tag = &tag\n')
            self.output.write('if tag == {tag_name}_INVALID {{\n'.format(**globals))
            self.output.write('\tm.value = nil\n')
            self.output.write('\treturn nil\n')
            self.output.write('}\n')
            self.output.write('val, err := m.unpackStruct(tag, buf)\n')
            self.output.write('if err != nil {\n')
            self.output.write('\t*m.tag = {tag_name}_INVALID\n'.format(**globals))
            self.output.write('\treturn err\n}\n')
            self.output.write('m.value = val\nreturn nil\n')
        self.output.write('}\n\n')

        self.output.write('func (t {union_name}Tag) String() string {{\n'.format(**globals))
        with self.output.indent(1):
            self.output.write('switch t {\n')
            for member in node.members():
                self.output.write('case {tag_name}_{member_name}:\n'.format(
                    member_name=goify(member.name), **globals))
                self.output.write('\treturn "{member_name}"\n'.format(member_name=goify(member.name)))
            self.output.write('default:\n')
            self.output.write('\treturn "INVALID"\n')
            self.output.write('}\n')
        self.output.write('}\n\n')

        self.output.write('func (m *{union_name}) String() string {{\n'.format(**globals))
        with self.output.indent(1):
            self.output.write('if m.tag == nil {\n')
            self.output.write('\treturn "nil"\n}\n')
            self.output.write('if *m.tag == {union_name}Tag_INVALID {{\n'.format(**globals))
            self.output.write('\treturn "INVALID"\n}\n')
            self.output.write('return fmt.Sprintf("%s: {%s}", *m.tag, m.value)\n')
        self.output.write('}\n\n')

        for member in node.members():
            member_globals = dict(
                member_name = goify(member.name),
                member_type = member.type,
                **globals
            )

            self.output.write('func (m *{union_name}) Get{member_name}() *{member_type} {{\n'.format(
                **member_globals))
            with self.output.indent(1):
                self.output.write('if m.tag == nil || *m.tag != {tag_name}_{member_name} {{\n'.format(
                    **member_globals))
                self.output.write('\treturn nil\n}\n')
                self.output.write('return m.value.(*{member_type})\n'.format(**member_globals))
            self.output.write('}\n\n')

            self.output.write('func (m *{union_name}) Set{member_name}(value *{member_type}) {{\n'.format(
                **member_globals))
            with self.output.indent(1):
                self.output.write('newTag := {tag_name}_{member_name}\n'.format(**member_globals))
                self.output.write('m.tag = &newTag\n')
                self.output.write('m.value = value\n'.format(**member_globals))
            self.output.write('}\n\n')

            self.output.write('func New{union_name}With{member_name}(value *{member_type}) *{union_name} {{\n'.format(
                **member_globals))
            with self.output.indent(1):
                self.output.write('var ret {union_name}\n'.format(**member_globals))
                self.output.write('ret.Set{member_name}(value)\n'.format(**member_globals))
                self.output.write('return &ret\n')
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
    option_parser = emitterutil.StandardArgumentParser('Go')
    option_parser.add_argument('--package', default=None,
        help='The name of the package to be generated from this file')
    options = option_parser.parse_args()
    input_file = options.input_file

    emitterutil.go_main(GlobalEmitter, options, scanner=OptionDetector)
