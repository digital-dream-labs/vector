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
import os
import sys
import textwrap

def _modify_path():
    currentpath = os.path.dirname(inspect.getfile(inspect.currentframe()))
    searchpath = os.path.join(currentpath, '..')
    searchpath = os.path.normpath(os.path.abspath(os.path.realpath(searchpath)))
    if searchpath not in sys.path:
        sys.path.insert(0, searchpath)
_modify_path()

from clad import clad
from clad import ast
from clad import emitterutil

class PythonQualifiedNamer(object):

    @classmethod
    def join(cls, *args):
        return '.'.join(args)

    @classmethod
    def disambiguate(cls, qualified_name):
        return "globals()['{0}']{1}{2}".format(*qualified_name.partition('.'))

type_translations = {
    'bool': 'b',
    'int_8': 'b',
    'int_16': 'h',
    'int_32': 'i',
    'int_64': 'q',
    'uint_8': 'B',
    'uint_16': 'H',
    'uint_32': 'I',
    'uint_64': 'Q',
    'float_32': 'f',
    'float_64': 'd'
}

support_module = 'msgbuffers'

def sort_key_lowercase(key):
    return (key.lower(), key)

class BaseEmitter(ast.NodeVisitor):
    "Base class for emitters."

    def __init__(self, output=sys.stdout):
        self.output = output

    # ignore includes unless explicitly allowed
    def visit_IncludeDecl(self, node, *args, **kwargs):
        pass

class Module(object):
    "A record of all the symbols within a module."

    def __init__(self, include_name):
        self.name = include_name[:include_name.find('.')].replace('/', '.')
        self.global_namespaces = set()
        self.global_decls = set()
        self.included_modules = []

    def include(self, other):
        self.global_namespaces.update(other.global_namespaces)
        self.global_decls.update(other.global_decls)
        self.included_modules.append(other)

    def sort_key(self):
        return sort_key_lowercase(self.name)

    def get_unique_name(self, name):
        while name in self:
            name = '_' + name
        return name

    def is_symbol_in_includes(self, symbol):
        for included_module in self.included_modules:
            if symbol in included_module:
                return True
        return False

    def __contains__(self, symbol):
        return (symbol in self.global_namespaces or symbol in self.global_decls)

class IncludeSearcher(ast.NodeVisitor):
    "A visitor that searches through all the symbols, looking for names."

    def __init__(self):
        self.all_namespaces = set()
        self.module = Module('__main__')
        self.included_modules = []

    def visit_IncludeDecl(self, node):
        saved_module = self.module
        self.module = Module(node.name)
        self.generic_visit(node)
        saved_module.include(self.module)
        self.module = saved_module

    def visit_NamespaceDecl(self, node):
        self.all_namespaces.add(node.fully_qualified_name(PythonQualifiedNamer))
        if node.namespace is None:
            self.module.global_namespaces.add(node.name)
        self.generic_visit(node)

    def visit_Decl_subclass(self, node):
        if node.namespace is None:
            self.module.global_decls.add(node.name)

class DeclEmitter(BaseEmitter):
    "An emitter that redirects to DeclBodyEmitter after cleaning up stray symbols."

    class DeclBodyEmitter(BaseEmitter):

        def visit_EnumDecl(self, node, *args, **kwargs):
            EnumEmitter(self.output, *args, **kwargs).visit(node, *args, **kwargs)

        def visit_MessageDecl(self, node, *args, **kwargs):
            MessageEmitter(self.output, *args, **kwargs).visit(node, *args, **kwargs)

        def visit_UnionDecl(self, node, *args, **kwargs):
            UnionEmitter(self.output, *args, **kwargs).visit(node, *args, **kwargs)

        def visit_EnumConceptDecl(self, node, *args, **kargs):
            EnumConceptEmitter(self.output, *args, **kargs).visit(node, *args, **kargs)

    def __init__(self, output, module):
        super(DeclEmitter, self).__init__(output)
        self.module = module
        self.body_emitter = self.DeclBodyEmitter(self.output)

    def visit_NamespaceDecl(self, node, *args, **kwargs):
        self.generic_visit(node, *args, **kwargs)

    def visit_Decl_subclass(self, node, *args, **kwargs):

        globals = dict(
            decl_name=node.name,
            qualified_decl_name=node.fully_qualified_name(PythonQualifiedNamer),
            saved_name=self.module.get_unique_name('_' + node.name))

        if node.namespace:
            need_to_save_name = self.module.is_symbol_in_includes(node.name)
            if need_to_save_name:
                self.output.write('{saved_name} = {decl_name}\n'.format(**globals))

        self.body_emitter.visit(node, *args, **kwargs)

        if node.namespace:
            # The idea is that if an enum is referencing another enum using the verbatim keyword then the enum that is being
            # referenced has to specify no_cpp_class in order for the c++ code to compile. Generated python code needs to not
            # delete the decl_name in this case
            try:
                # Only EnumDecls have cpp_class so this needs to be in a try except
                # cpp_class is false if no_cpp_class was specified in the clad file so we need to save the decl name
                need_to_save_name = not node.cpp_class
            except AttributeError:
                pass
            finally:
              self.output.write('{qualified_decl_name} = {decl_name}\n'.format(**globals))
              # if we don't need to save the decl_name then delete it
              if not need_to_save_name:
                  self.output.write('del {decl_name}\n\n'.format(**globals))

        self.output.write('\n')

class EnumEmitter(BaseEmitter):

    def visit_EnumDecl(self, node):

        globals = dict(
            support_module=support_module,
            enum_name=node.name,
            enum_type=node.storage_type.name,
        )

        self.emitHeader(node, globals)
        self.emitMembers(node, globals)

    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            class {enum_name}(object):
            \t"Automatically-generated {enum_type} enumeration."
            #''')[:-1].format(**globals))

    def emitMembers(self, node, globals):
        if node.members():
            starts = []
            ends = []
            enum_val = 0
            enum_str_val = None
            for member in node.members():
                value = member.value
                
                if type(value) is str and "::" in member.value:
                    value = value.replace("::", ".")
                
                start = '\t{member_name}'.format(member_name=member.name)
                end = ' = {initializer}\n'.format(initializer=str(value))

                starts.append(start)
                ends.append(end)

            full_length = max(len(start) for start in starts)
            for start, end in zip(starts, ends):
                self.output.write(start)
                self.output.write(' ' * (full_length - len(start)))
                self.output.write(end)

        self.output.write('\n')

class MessageEmitter(BaseEmitter):

    def visit_MessageDecl(self, node):

        globals = dict(
            support_module=support_module,
            struct_name=node.name,
        )

        self.emitHeader(node, globals)
        self.emitSlots(node, globals)
        self.emitProperties(node, globals)
        self.emitConstructor(node, globals)
        self.emitPackers(node, globals)
        self.emitComparisons(node, globals)
        self.emitLength(node, globals)
        self.emitStr(node, globals)
        self.emitRepr(node, globals)

    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            class {struct_name}(object):
            \t"Generated message-passing {obj_type}."

            #''')[:-1].format(obj_type=node.object_type(), **globals))

    def emitSlots(self, node, globals):
        if node.members():
            self.output.write('\t__slots__ = (\n')

            starts = []
            ends = []
            for member in node.members():
                starts.append("\t\t'_{member_name}',".format(member_name=member.name))
                ends.append(' # {member_type}\n'.format(member_type=member.type.fully_qualified_name(PythonQualifiedNamer)))

            full_length = max(len(start) for start in starts)
            for start, end in zip(starts, ends):
                self.output.write(start)
                self.output.write(' ' * (full_length - len(start)))
                self.output.write(end)

            self.output.write('\t)\n\n')
        else:
            self.output.write('\t__slots__ = ()\n\n')

    def emitProperties(self, node, globals):
        setter_visitor = SetterVisitor(output=self.output, depth=2)
        for member in node.members():
            self.output.write(textwrap.dedent('''\
                \t@property
                \tdef {member_name}(self):
                \t\t"{member_type} {member_name} struct property."
                \t\treturn self._{member_name}

                \t@{member_name}.setter
                \tdef {member_name}(self, value):
                #''').format(member_name=member.name, member_type=member.type.fully_qualified_name(PythonQualifiedNamer), **globals)[:-1])
            self.output.write('\t\tself._{member_name} = '.format(member_name=member.name))
            setter_visitor.visit(
                member.type,
                name="'{struct_name}.{member_name}'".format(member_name=member.name, **globals),
                value='value')
            self.output.write('\n\n')

    def emitConstructor(self, node, globals):
        default_value_visitor = DefaultValueVisitor(output=self.output)
        self.output.write('\tdef __init__(self')
        
        all_members_have_default_constructor = emitterutil._do_all_members_have_default_constructor(node) and node.default_constructor
        
        for member in node.members():
            self.output.write(', {member_name}'.format(member_name=member.name))
            
            # If any members do not have a default constructor then don't generate the assignment
            if not all_members_have_default_constructor:
                continue
            
            self.output.write('=')
            if member.init is not None and type(member.init.value) is str:
                member_init=member.init.value
                if "::" in member_init:
                    # Replace '::' with '.'
                    member_init = member_init.replace("::", ".")
                self.output.write('{member_init}'.format(member_init=member_init))
            else:
                default_value_visitor.visit(member)
                
        self.output.write('):\n')

        if node.members():
            for member in node.members():
                self.output.write('\t\tself.{member_name} = {member_name}\n'.format(member_name=member.name))
        else:
            self.output.write('\t\tpass\n')
        self.output.write('\n')

    def emitPackers(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \t@classmethod
            \tdef unpack(cls, buffer):
            \t\t"Reads a new {struct_name} from the given buffer."
            \t\treader = {support_module}.BinaryReader(buffer)
            \t\tvalue = cls.unpack_from(reader)
            \t\tif reader.tell() != len(reader):
            \t\t\traise {support_module}.ReadError(
            \t\t\t\t('{struct_name}.unpack received a buffer of length {{length}}, ' +
            \t\t\t\t'but only {{position}} bytes were read.').format(
            \t\t\t\tlength=len(reader), position=reader.tell()))
            \t\treturn value

            \t@classmethod
            \tdef unpack_from(cls, reader):
            \t\t"Reads a new {struct_name} from the given BinaryReader."
            #''').format(**globals)[:-1])

        read_visitor = ReadVisitor(output=self.output)
        for member in node.members():
            self.output.write('\t\t_{member_name} = '.format(member_name=member.name))
            read_visitor.visit(member.type, reader='reader')
            self.output.write('\n')

        self.output.write('\t\treturn cls(')
        for i, member in enumerate(node.members()):
            if i > 0:
                self.output.write(', ')
            self.output.write('_{member_name}'.format(member_name=member.name))
        self.output.write(')\n\n')

        self.output.write(textwrap.dedent('''\
            \tdef pack(self):
            \t\t"Writes the current {struct_name}, returning bytes."
            \t\twriter = {support_module}.BinaryWriter()
            \t\tself.pack_to(writer)
            \t\treturn writer.dumps()

            \tdef pack_to(self, writer):
            \t\t"Writes the current {struct_name} to the given BinaryWriter."
            #''').format(**globals)[:-1])

        write_visitor = WriteVisitor(output=self.output)
        for member in node.members():
            self.output.write('\t\t')
            write_visitor.visit(member.type, writer='writer',
                value='self._{member_name}'.format(member_name=member.name))
            self.output.write('\n')
        self.output.write('\n')

    def emitComparisons(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tdef __eq__(self, other):
            \t\tif type(self) is type(other):
            #''')[:-1])

        if node.members():
            self.output.write('\t\t\treturn ')
            if len(node.members()) > 1:
                self.output.write('(')
            for i, member in enumerate(node.members()):
                if i > 0:
                    self.output.write('\t\t\t\t')
                self.output.write('self._{member_name} == other._{member_name}'.format(member_name=member.name))
                if i < len(node.members()) - 1:
                    self.output.write(' and\n')
            if len(node.members()) > 1:
                self.output.write(')')
            self.output.write('\n')
        else:
            self.output.write('\t\t\treturn True\n')

        self.output.write(textwrap.dedent('''\
            \t\telse:
            \t\t\treturn NotImplemented

            \tdef __ne__(self, other):
            \t\tif type(self) is type(other):
            \t\t\treturn not self.__eq__(other)
            \t\telse:
            \t\t\treturn NotImplemented

            #''')[:-1])

    def emitLength(self, node, globals):
        self.output.write('\tdef __len__(self):\n')

        if node.members():
            size_visitor = SizeVisitor(output=self.output)
            self.output.write('\t\treturn (')
            for i, member in enumerate(node.members()):
                if i > 0:
                    self.output.write(' +\n\t\t\t')
                size_visitor.visit(member.type, value='self._{member_name}'.format(member_name=member.name))
            self.output.write(')\n\n')
        else:
            self.output.write('\t\treturn 0\n\n')

    def emitStr(self, node, globals):

        self.output.write('\tdef __str__(self):\n')

        if node.members():
            self.output.write("\t\treturn '{type}(")
            self.output.write(', '.join(('%s={%s}' % (member.name, member.name) for member in node.members())))
            self.output.write(")'.format(\n")

            self.output.write('\t\t\ttype=type(self).__name__,\n')

            str_visitor = StrVisitor(output=self.output)
            for i, member in enumerate(node.members()):
                self.output.write('\t\t\t{member_name}='.format(member_name=member.name))
                str_visitor.visit(member.type,
                    value='self._{member_name}'.format(member_name=member.name))
                if i < len(node.members()) - 1:
                    self.output.write(',\n')
                else:
                    self.output.write(')\n')
        else:
            self.output.write("\t\treturn '{type}()'.format(type=type(self).__name__)\n")
        self.output.write('\n')

    def emitRepr(self, node, globals):
        self.output.write('\tdef __repr__(self):\n')
        if node.members():
            self.output.write("\t\treturn '{type}(")
            self.output.write(', '.join(('%s={%s}' % (member.name, member.name) for member in node.members())))
            self.output.write(")'.format(\n")

            self.output.write('\t\t\ttype=type(self).__name__,\n')

            for i, member in enumerate(node.members()):
                self.output.write('\t\t\t{member_name}=repr(self._{member_name})'.format(member_name=member.name))
                if i < len(node.members()) - 1:
                    self.output.write(',\n')
                else:
                    self.output.write(')\n')
        else:
            self.output.write("\t\treturn '{type}()'.format(type=type(self).__name__)\n")
        self.output.write('\n')

class UnionEmitter(BaseEmitter):

    def visit_UnionDecl(self, node):

        globals = dict(
            support_module=support_module,
            union_name=node.name,
            tag_count=len(node.members()),
        )

        self.emitHeader(node, globals)
        self.emitSlots(node, globals)
        self.emitTags(node, globals)
        self.emitProperties(node, globals)
        self.emitConstructor(node, globals)
        self.emitPackers(node, globals)
        self.emitClear(node, globals)
        self.emitTypeGetter(node, globals)
        self.emitComparisons(node, globals)
        self.emitLength(node, globals)
        self.emitStr(node, globals)
        self.emitRepr(node, globals)
        self.emitDicts(node, globals)

    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            class {union_name}(object):
            \t"Generated message-passing union."

            #''')[:-1].format(**globals))

    def emitSlots(self, node, globals):
        self.output.write("\t__slots__ = ('_tag', '_data')\n\n")

    def emitTags(self, node, globals):
        self.output.write('\tclass Tag(object):\n')
        self.output.write('\t\t"The type indicator for this union."\n')

        if node.members():
            starts = []
            mids = []
            ends = []
            isHex = False
            for member in node.members():
                if member.init:
                    isHex = member.init.type == "hex"
                start = '\t\t{member_name}'.format(member_name=member.name)
                init_str = '0x{0:x}'.format(member.tag) if isHex else "{0:d}".format(member.tag)
                mid = ' = {initializer}'.format(initializer=init_str)
                end = ' # {member_type}\n'.format(
                    member_type=member.type.fully_qualified_name(PythonQualifiedNamer))
                starts.append(start)
                mids.append(mid)
                ends.append(end)

            full_start_length = max(len(start) for start in starts)
            full_mid_length = max(len(mid) for mid in mids)
            for start, mid, end in zip(starts, mids, ends):
                self.output.write(start)
                self.output.write(' ' * (full_start_length - len(start)))
                self.output.write(mid)
                self.output.write(' ' * (full_mid_length - len(mid)))
                self.output.write(end)
        else:
            self.output.write('\t\tpass\n')

        self.output.write('\n')

        self.output.write(textwrap.dedent('''\
            \t@property
            \tdef tag(self):
            \t\t"The current tag for this union."
            \t\treturn self._tag

            \t@property
            \tdef tag_name(self):
            \t\t"The name of the current tag for this union."
            \t\tif self._tag in self._tags_by_value:
            \t\t\treturn self._tags_by_value[self._tag]
            \t\telse:
            \t\t\treturn None

            \t@property
            \tdef data(self):
            \t\t"The data held by this union. None if no data is set."
            \t\treturn self._data

            #''')[:-1].format(**globals))

    def emitProperties(self, node, globals):
        setter_visitor = SetterVisitor(output=self.output, depth=2)
        for member in node.members():
            self.output.write(textwrap.dedent('''\
                \t@property
                \tdef {member_name}(self):
                \t\t"{member_type} {member_name} union property."
                \t\t{support_module}.safety_check_tag('{member_name}', self._tag, self.Tag.{member_name}, self._tags_by_value)
                \t\treturn self._data

                \t@{member_name}.setter
                \tdef {member_name}(self, value):
                #''')[:-1].format(member_name=member.name, member_type=member.type.fully_qualified_name(PythonQualifiedNamer), **globals))
            self.output.write('\t\tself._data = '.format(member_name=member.name, value='value'))
            setter_visitor.visit(
                member.type,
                name="'{union_name}.{member_name}'".format(member_name=member.name, **globals),
                value='value')
            self.output.write('\n')
            self.output.write('\t\tself._tag = self.Tag.{member_name}\n\n'.format(member_name=member.name))

    def emitConstructor(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tdef __init__(self, **kwargs):
            \t\tif not kwargs:
            \t\t\tself._tag = None
            \t\t\tself._data = None

            \t\telif len(kwargs) == 1:
            \t\t\tkey, value = next(iter(kwargs.items()))
            \t\t\tif key not in self._tags_by_name:
            \t\t\t\traise TypeError("'{{argument}}' is an invalid keyword argument for this method.".format(argument=key))
            \t\t\t# calls the correct property
            \t\t\tsetattr(self, key, value)

            \t\telse:
            \t\t\traise TypeError('This method only accepts up to one keyword argument.')

            #''')[:-1].format(**globals))

    def emitPackers(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \t@classmethod
            \tdef unpack(cls, buffer):
            \t\t"Reads a new {union_name} from the given buffer."
            \t\treader = {support_module}.BinaryReader(buffer)
            \t\tvalue = cls.unpack_from(reader)
            \t\tif reader.tell() != len(reader):
            \t\t\traise {support_module}.ReadError(
            \t\t\t\t('{union_name}.unpack received a buffer of length {{length}}, ' +
            \t\t\t\t'but only {{position}} bytes were read.').format(
            \t\t\t\tlength=len(reader), position=reader.tell()))
            \t\treturn value

            \t@classmethod
            \tdef unpack_from(cls, reader):
            \t\t"Reads a new {union_name} from the given BinaryReader."
            \t\ttag = reader.read('B')
            \t\tif tag in cls._tags_by_value:
            \t\t\tvalue = cls()
            \t\t\tsetattr(value, cls._tags_by_value[tag], cls._tag_unpack_methods[tag](reader))
            \t\t\treturn value
            \t\telse:
            \t\t\traise ValueError('{union_name} attempted to unpack unknown tag {{tag}}.'.format(tag=tag))

            \tdef pack(self):
            \t\t"Writes the current {union_name}, returning bytes."
            \t\twriter = {support_module}.BinaryWriter()
            \t\tself.pack_to(writer)
            \t\treturn writer.dumps()

            \tdef pack_to(self, writer):
            \t\t"Writes the current SampleUnion to the given BinaryWriter."
            \t\tif self._tag in self._tags_by_value:
            \t\t\twriter.write(self._tag, 'B')
            \t\t\tself._tag_pack_methods[self._tag](writer, self._data)
            \t\telse:
            \t\t\traise ValueError('Cannot pack an empty {union_name}.')

            #''')[:-1].format(**globals))

    def emitClear(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tdef clear(self):
            \t\tself._tag = None
            \t\tself._data = None

            #''')[:-1])

    def emitComparisons(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tdef __eq__(self, other):
            \t\tif type(self) is type(other):
            \t\t\treturn self._tag == other._tag and self._data == other._data
            \t\telse:
            \t\t\treturn NotImplemented

            \tdef __ne__(self, other):
            \t\tif type(self) is type(other):
            \t\t\treturn not self.__eq__(other)
            \t\telse:
            \t\t\treturn NotImplemented

            #''')[:-1])

    def emitLength(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tdef __len__(self):
            \t\tif 0 <= self._tag < {tag_count}:
            \t\t\treturn self._tag_size_methods[self._tag](self._data)
            \t\telse:
            \t\t\treturn 1

            #''')[:-1].format(**globals))

    def emitStr(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tdef __str__(self):
            \t\tif 0 <= self._tag < {tag_count}:
            \t\t\treturn '{{type}}({{name}}={{value}})'.format(
            \t\t\t\ttype=type(self).__name__,
            \t\t\t\tname=self.tag_name,
            \t\t\t\tvalue=self._data)
            \t\telse:
            \t\t\treturn '{{type}}()'.format(
            \t\t\t\ttype=type(self).__name__)

            #''')[:-1].format(**globals))

    def emitRepr(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tdef __repr__(self):
            \t\tif 0 <= self._tag < {tag_count}:
            \t\t\treturn '{{type}}({{name}}={{value}})'.format(
            \t\t\t\ttype=type(self).__name__,
            \t\t\t\tname=self.tag_name,
            \t\t\t\tvalue=repr(self._data))
            \t\telse:
            \t\t\treturn '{{type}}()'.format(
            \t\t\t\ttype=type(self).__name__)

            #''')[:-1].format(**globals))

    def emitTypeGetter(self, node, globals):
        self.output.write('\t@classmethod\n\tdef typeByTag(cls, tag):\n\t\treturn cls._type_by_tag_value[tag]()\n\n')

    def emitDicts(self, node, globals):
        self.output.write('\t_tags_by_name = dict(\n')
        for member in node.members():
            self.output.write("\t\t{member_name}={tag},\n".format(member_name=member.name,
                                                                  tag=member.tag))

        self.output.write('\t)\n\n')

        if node.members():
            self.output.write('\t_tags_by_value = dict()\n')
            for member in node.members():
                self.output.write("\t_tags_by_value[{tag_value}] = '{member_name}'\n".format(member_name=member.name, tag_value=member.tag))
            self.output.write('\t\n\n')
        else:
            self.output.write('\t_tags_by_value = ()\n\n')

        self.emitVisitorList(node, globals, '_tag_unpack_methods', ReadVisitor(output=self.output), 'reader')
        self.emitVisitorList(node, globals, '_tag_pack_methods', WriteVisitor(output=self.output), 'writer', 'value')
        self.emitVisitorList(node, globals, '_tag_size_methods', SizeVisitor(output=self.output), 'value')
        self.emitVisitorList(node, globals, '_type_by_tag_value', TypeVisitor(output=self.output))

    def emitVisitorList(self, node, globals, dict_name, visitor, *args):
        if node.members():
            self.output.write('\t{dict_name} = dict()\n'.format(dict_name=dict_name))
            for member in node.members():
                self.output.write('\t{dict_name}[{tag}] = lambda {args}: '.format(args=', '.join(args),
                                                                                tag=member.tag,
                                                                                dict_name=dict_name))
                visitor.visit(member.type, *args)
                self.output.write('\n')
            self.output.write('\t\n\n')
        else:
            self.output.write('\t{dict_name} = ()\n\n'.format(dict_name=dict_name))

class EnumConceptEmitter(BaseEmitter):

    def visit_EnumConceptDecl(self, node):

        globals = dict(
            support_module=support_module,
            enum_concept_name=node.name,
            enum_concept_hash=node.hash_str,
            enum_concept_type=node.enum
        )

        self.emitConcept(node, globals)

    def emitConcept(self, node, globals):
        argument_name = emitterutil._lower_first_char_of_string(node.enum)
        self.output.write('def {enum_concept_name}({argument_name}, defaultValue):\n'.format(argument_name=argument_name, **globals))
        self.output.write('\treturn {\n')

        for member in node.members():

            member_value = member.value.value

            # If this is a string and it contains "::" meaning it is likely a verbatim value
            if type(member_value) is str and "::" in member_value:
                # Replace '::' with '.'
                member_value = member_value.replace("::", ".")

            self.output.write('\t\t{enum_concept_type}.{member_name}: {member_value},\n'.format(
                argument_name=argument_name,
                member_name=member.name,
                member_value=member_value,
                **globals))

        self.output.write('\t\t}}.get({argument_name}, defaultValue)\n'.format(argument_name=argument_name))

class PythonMemberVisitor(BaseEmitter):

    def __init__(self, output=sys.stdout, depth=1):
        super(PythonMemberVisitor, self).__init__(output)
        self.depth = depth

    def visit_DefinedType(self, node, *args, **kwargs):
        self.visit(node.underlying_type, *args, **kwargs)

    def visit_EnumDecl(self, node, *args, **kwargs):
        self.visit(node.storage_type, *args, **kwargs)

class SetterVisitor(PythonMemberVisitor):

    def visit_BuiltinType(self, node, name, value):
        if node.name == 'bool':
            self.output.write("{support_module}.validate_bool(\n".format(support_module=support_module))
            self.output.write('\t' * (self.depth + 1))
            self.output.write("{name}, {value})".format(
                name=name, value=value))

        elif node.type == 'int':
            self.output.write("{support_module}.validate_integer(\n".format(support_module=support_module))
            self.output.write('\t' * (self.depth + 1))
            self.output.write("{name}, {value}, {minimum}, {maximum})".format(
                name=name, value=value, minimum=node.min, maximum=node.max))

        elif node.type == 'float':
            self.output.write("{support_module}.validate_float(\n".format(support_module=support_module))
            self.output.write('\t' * (self.depth + 1))
            self.output.write("{name}, {value}, '{format}')".format(
                name=name, value=value, format=type_translations[node.name]))

        else:
            raise ValueError('Unknown primitive type {type}.'.format(type=node.type))

    def visit_CompoundType(self, node, name, value):
        self.output.write("{support_module}.validate_object(\n".format(support_module=support_module))
        self.output.write('\t' * (self.depth + 1))
        self.output.write("{name}, {value}, {type})".format(
            name=name, value=value, type=node.fully_qualified_name(PythonQualifiedNamer)))

    def visit_PascalStringType(self, node, name, value):
        self.output.write("{support_module}.validate_string(\n".format(support_module=support_module))
        self.output.write('\t' * (self.depth + 1))
        self.output.write("{name}, {value}, {maximum_length})".format(
            name=name, value=value, maximum_length=node.length_type.max))

    def visit_FixedArrayType(self, node, name, value):
        self.output.write("{support_module}.validate_farray(\n".format(support_module=support_module))
        self.output.write('\t' * (self.depth + 1))
        length = node.length
        if isinstance(length, str) and "::" in length:
            length = length.replace("::", ".")
        self.output.write("{name}, {value}, {length},\n".format(
            name=name, value=value, length=length))
        self.output.write('\t' * (self.depth + 1))
        inner = '{value}_inner'.format(value=value)
        self.output.write('lambda name, {inner}: '.format(inner=inner))
        self.depth += 1
        self.visit(node.member_type, name='name', value=inner)
        self.depth -= 1
        self.output.write(')')

    def visit_VariableArrayType(self, node, name, value):
        self.output.write("{support_module}.validate_varray(\n".format(support_module=support_module))
        self.output.write('\t' * (self.depth + 1))
        self.output.write("{name}, {value}, {maximum_length},\n".format(
            name=name, value=value, maximum_length=node.length_type.max))
        self.output.write('\t' * (self.depth + 1))
        inner = '{value}_inner'.format(value=value)
        self.output.write('lambda name, {inner}: '.format(inner=inner))
        self.depth += 1
        self.visit(node.member_type, name='name', value=inner)
        self.depth -= 1
        self.output.write(')')

class DefaultValueVisitor(PythonMemberVisitor):

    def __init__(self, *args, **kwargs):
        PythonMemberVisitor.__init__(self, *args, **kwargs)
        self.init = None

    def visit_MessageMemberDecl(self, node):
        self.init = node.init # Cache this for when we hit it later
        self.visit(node.type) # Recurse

    def visit_BuiltinType(self, node):
        if node.name == 'bool':
            if self.init:
                self.output.write('True' if self.init.value else 'False')
            else:
                self.output.write('False')
        elif node.type == 'int':
            if self.init:
                if self.init.type == 'hex':
                    self.output.write(hex(self.init.value))
                else:
                    self.output.write(repr(self.init.value))
            else:
                self.output.write('0')
        elif node.type == 'float':
            if self.init:
                self.output.write(repr(self.init.value))
            else:
                self.output.write('0.0')
        else:
            raise ValueError('Unknown primitive type {type}.'.format(node.type))

    def visit_EnumDecl(self, node):
        if not node.members():
            self.output.write('0')
        else:
            member = node.members()[0]
            self.output.write('{enum_name}.{enum_member}'.format(
                enum_name=node.fully_qualified_name(PythonQualifiedNamer), enum_member=member.name))

    def visit_CompoundType(self, node):
        self.output.write('{type}()'.format(type=node.fully_qualified_name(PythonQualifiedNamer)))

    def visit_PascalStringType(self, node):
        self.output.write("''")

    def visit_FixedArrayType(self, node):
        self.output.write('(')
        self.visit(node.member_type)
        length = node.length
        if isinstance(length, str) and "::" in length:
            length = length.replace("::", ".")
        self.output.write(',) * {length}'.format(length=length))

    def visit_VariableArrayType(self, node):
        self.output.write('()')

class ReadVisitor(PythonMemberVisitor):

    class ChildFixedArrayVisitor(PythonMemberVisitor):
        def visit_BuiltinType(self, node, reader, length):
            if node.name == 'bool':
                self.output.write('list(map(bool, ')
            self.output.write("{reader}.read_farray('{format}', {length})".format(
                reader=reader, format=type_translations[node.name], length=length))
            if node.name == 'bool':
                self.output.write('))')

        def visit_CompoundType(self, node, reader, length):
            self.output.write("{reader}.read_object_farray({type}.unpack_from, {length})".format(
                reader=reader, type=node.fully_qualified_name(PythonQualifiedNamer), length=length))

        def visit_PascalStringType(self, node, reader, length):
            self.output.write("{reader}.read_string_farray('{string_length_format}', {array_length})".format(
                reader=reader, string_length_format=type_translations[node.length_type.name], array_length=length))

        def visit_FixedArrayType(self, node, reader, length):
            sys.exit('Python emitter does not support arrays of arrays.')

        def visit_VariableArrayType(self, node, reader, length):
            sys.exit('Python emitter does not support arrays of arrays.')

    class ChildVariableArrayVisitor(PythonMemberVisitor):
        def visit_BuiltinType(self, node, reader, length_type):
            if node.name == 'bool':
                self.output.write('list(map(bool, ')
            self.output.write("{reader}.read_varray('{data_format}', '{length_format}')".format(
                reader=reader, data_format=type_translations[node.name],
                length_format=type_translations[length_type.name]))
            if node.name == 'bool':
                self.output.write('))')

        def visit_CompoundType(self, node, reader, length_type):
            self.output.write("{reader}.read_object_varray({type}.unpack_from, '{length_format}')".format(
                reader=reader, type=node.fully_qualified_name(PythonQualifiedNamer), length_format=type_translations[length_type.name]))

        def visit_PascalStringType(self, node, reader, length_type):
            self.output.write("{reader}.read_string_varray('{string_length_format}', '{array_length_format}')".format(
                reader=reader, string_length_format=type_translations[node.length_type.name],
                array_length_format=type_translations[length_type.name]))

        def visit_FixedArrayType(self, node, reader, length_type):
            sys.exit('Python emitter does not support arrays of arrays.')

        def visit_VariableArrayType(self, node, reader, length_type):
            sys.exit('Python emitter does not support arrays of arrays.')

    def __init__(self, *args, **kwargs):
        super(ReadVisitor, self).__init__(*args, **kwargs)
        self.fixed_visitor = self.ChildFixedArrayVisitor(*args, **kwargs)
        self.variable_visitor = self.ChildVariableArrayVisitor(*args, **kwargs)

    def visit_BuiltinType(self, node, reader):
        if node.name == 'bool':
            self.output.write('bool(')
        self.output.write("{reader}.read('{format}')".format(reader=reader, format=type_translations[node.name]))
        if node.name == 'bool':
            self.output.write(')')

    def visit_CompoundType(self, node, reader):
        self.output.write("{reader}.read_object({type}.unpack_from)".format(
            reader=reader, type=node.fully_qualified_name(PythonQualifiedNamer)))

    def visit_PascalStringType(self, node, reader):
        self.output.write("{reader}.read_string('{format}')".format(
            reader=reader, format=type_translations[node.length_type.name]))

    def visit_FixedArrayType(self, node, reader):
        length = node.length
        if isinstance(length, str) and "::" in length:
            length = length.replace("::", ".")
        self.fixed_visitor.visit(node.member_type, reader=reader, length=length)

    def visit_VariableArrayType(self, node, reader):
        self.variable_visitor.visit(node.member_type, reader=reader, length_type=node.length_type)

class WriteVisitor(PythonMemberVisitor):

    class ChildFixedArrayVisitor(PythonMemberVisitor):
        def visit_BuiltinType(self, node, writer, value, length):
            if node.name == 'bool':
                value = 'list(map(int, {value}))'.format(value=value)
            self.output.write("{writer}.write_farray({value}, '{format}', {length})".format(
                writer=writer, value=value, format=type_translations[node.name], length=length))

        def visit_CompoundType(self, node, writer, value, length):
            self.output.write("{writer}.write_object_farray({value}, {length})".format(
                writer=writer, value=value, length=length))

        def visit_PascalStringType(self, node, writer, value, length):
            self.output.write("{writer}.write_string_farray({value}, '{string_length_format}', {array_length})".format(
                writer=writer, value=value, string_length_format=type_translations[node.length_type.name],
                array_length=length))

        def visit_FixedArrayType(self, node, writer, value, length):
            sys.exit('Python emitter does not support arrays of arrays.')

        def visit_VariableArrayType(self, node, writer, value, length):
            sys.exit('Python emitter does not support arrays of arrays.')

    class ChildVariableArrayVisitor(PythonMemberVisitor):
        def visit_BuiltinType(self, node, writer, value, length_type):
            if node.name == 'bool':
                value = 'list(map(int, {value}))'.format(value=value)
            self.output.write("{writer}.write_varray({value}, '{data_format}', '{length_format}')".format(
                writer=writer, value=value, data_format=type_translations[node.name],
                length_format=type_translations[length_type.name]))

        def visit_CompoundType(self, node, writer, value, length_type):
            self.output.write("{writer}.write_object_varray({value}, '{length_format}')".format(
                writer=writer, value=value, length_format=type_translations[length_type.name]))

        def visit_PascalStringType(self, node, writer, value, length_type):
            self.output.write("{writer}.write_string_varray({value}, '{string_length_format}', '{array_length_format}')".format(
                writer=writer, value=value, string_length_format=type_translations[node.length_type.name],
                array_length_format=type_translations[length_type.name]))

        def visit_FixedArrayType(self, node, writer, value, length_type):
            sys.exit('Python emitter does not support arrays of arrays.')

        def visit_VariableArrayType(self, node, writer, value, length_type):
            sys.exit('Python emitter does not support arrays of arrays.')

    def __init__(self, *args, **kwargs):
        super(WriteVisitor, self).__init__(*args, **kwargs)
        self.fixed_visitor = self.ChildFixedArrayVisitor(*args, **kwargs)
        self.variable_visitor = self.ChildVariableArrayVisitor(*args, **kwargs)

    def visit_BuiltinType(self, node, writer, value):
        if node.name == 'bool':
            value = 'int({value})'.format(value=value)
        self.output.write("{writer}.write({value}, '{format}')".format(
            writer=writer, value=value, format=type_translations[node.name], support_module=support_module))

    def visit_CompoundType(self, node, writer, value):
        self.output.write("{writer}.write_object({value})".format(
            writer=writer, value=value, support_module=support_module))

    def visit_PascalStringType(self, node, writer, value):
        self.output.write("{writer}.write_string({value}, '{format}')".format(
            writer=writer, value=value, format=type_translations[node.length_type.name], support_module=support_module))

    def visit_FixedArrayType(self, node, writer, value):
        length = node.length
        if isinstance(length, str) and "::" in length:
            length = length.replace("::", ".")
        self.fixed_visitor.visit(node.member_type, writer=writer, value=value, length=length)

    def visit_VariableArrayType(self, node, writer, value):
        self.variable_visitor.visit(node.member_type, writer=writer, value=value, length_type=node.length_type)

class SizeVisitor(PythonMemberVisitor):

    class ChildFixedArrayVisitor(PythonMemberVisitor):
        def visit_BuiltinType(self, node, value, length):
            self.output.write("{support_module}.size_farray({value}, '{format}', {length})".format(
                value=value, format=type_translations[node.name], length=length, support_module=support_module))

        def visit_CompoundType(self, node, value, length):
            self.output.write("{support_module}.size_object_farray({value}, {length})".format(
                value=value, length=length, support_module=support_module))

        def visit_PascalStringType(self, node, value, length):
            self.output.write("{support_module}.size_string_farray({value}, '{string_length_format}', {array_length})".format(
                value=value, string_length_format=type_translations[node.length_type.name],
                array_length=length, support_module=support_module))

        def visit_FixedArrayType(self, node, value, length):
            sys.exit('Python emitter does not support arrays of arrays.')

        def visit_VariableArrayType(self, node, value, length):
            sys.exit('Python emitter does not support arrays of arrays.')

    class ChildVariableArrayVisitor(PythonMemberVisitor):
        def visit_BuiltinType(self, node, value, length_type):
            self.output.write("{support_module}.size_varray({value}, '{data_format}', '{length_format}')".format(
                value=value, data_format=type_translations[node.name],
                length_format=type_translations[length_type.name], support_module=support_module))

        def visit_CompoundType(self, node, value, length_type):
            self.output.write("{support_module}.size_object_varray({value}, '{length_format}')".format(
                value=value, length_format=type_translations[length_type.name], support_module=support_module))

        def visit_PascalStringType(self, node, value, length_type):
            self.output.write("{support_module}.size_string_varray({value}, '{string_length_format}', '{array_length_format}')".format(
                value=value, string_length_format=type_translations[node.length_type.name],
                array_length_format=type_translations[length_type.name], support_module=support_module))

        def visit_FixedArrayType(self, node, value, length_type):
            sys.exit('Python emitter does not support arrays of arrays.')

        def visit_VariableArrayType(self, node, value, length_type):
            sys.exit('Python emitter does not support arrays of arrays.')

    def __init__(self, *args, **kwargs):
        super(SizeVisitor, self).__init__(*args, **kwargs)
        self.fixed_visitor = self.ChildFixedArrayVisitor(*args, **kwargs)
        self.variable_visitor = self.ChildVariableArrayVisitor(*args, **kwargs)

    def visit_BuiltinType(self, node, value):
        self.output.write("{support_module}.size({value}, '{format}')".format(
            value=value, format=type_translations[node.name], support_module=support_module))

    def visit_CompoundType(self, node, value):
        self.output.write('{support_module}.size_object({value})'.format(
            value=value, support_module=support_module))

    def visit_PascalStringType(self, node, value):
        self.output.write("{support_module}.size_string({value}, '{length_format}')".format(
            value=value, length_format=type_translations[node.length_type.name], support_module=support_module))

    def visit_FixedArrayType(self, node, value):
        length = node.length
        if isinstance(length, str) and "::" in length:
            length = length.replace("::", ".")
        self.fixed_visitor.visit(node.member_type, value=value, length=length)

    def visit_VariableArrayType(self, node, value):
        self.variable_visitor.visit(node.member_type, value=value, length_type=node.length_type)

class StrVisitor(PythonMemberVisitor):

    def visit_BuiltinType(self, node, value):
        self.output.write(value)

    def visit_CompoundType(self, node, value):
        self.output.write(value)

    def visit_PascalStringType(self, node, value):
        self.output.write('{support_module}.shorten_string({value})'.format(
            value=value, support_module=support_module))

    def visit_FixedArrayType(self, node, value):
        if isinstance(node.member_type, ast.PascalStringType):
            self.output.write('{support_module}.shorten_sequence({value}, {support_module}.shorten_string)'.format(
                value=value, support_module=support_module))
        else:
            self.output.write('{support_module}.shorten_sequence({value})'.format(
                value=value, support_module=support_module))

    def visit_VariableArrayType(self, node, value):
        self.visit_FixedArrayType(node, value)

class TypeVisitor(PythonMemberVisitor):

    class ChildArrayVisitor(PythonMemberVisitor):
        def visit_BuiltinType(self, node):
            self.output.write(node.name + ", ")
        def visit_CompoundType(self, node):
            self.output.write(node.name + ", ")
        def visit_PascalStringType(self, node):
            self.output.write("str, ")
        def visit_FixedArrayType(self, node):
            sys.exit("Python emitter does not support arrays of arrays.")
        def visit_VariableArrayType(self, node):
            sys.exit("Python emitter does not support arrays of arrays.")

    def visit_BuiltinType(self, node):
        if node.name == 'bool':
            self.output.write("bool")
        elif node.type == 'int':
            self.output.write("int")
        elif node.type == 'float':
            self.output.write('float')
        else:
            raise ValueError("Unknown primitive type {type}".format(type=node.type))

    def visit_CompoundType(self, node):
        self.output.write(node.fully_qualified_name(PythonQualifiedNamer))

    def visit_PascalStringType(self, node):
        self.output.write("bytes")

    def visit_FixedArrayType(self, node):
        self.output.write("(")
        # TODO something more useful here
        self.output.write(")")

    def visit_VariableArrayType(self, node):
        self.output.write("[")
        # TODO something more useful here
        self.output.write("]")

def emit_body(tree, output):
    searcher = IncludeSearcher()
    searcher.visit(tree)

    # add all namespaces, even those not in use
    if searcher.all_namespaces:
        for namespace in sorted(searcher.all_namespaces, key=sort_key_lowercase):
            output.write('{namespace} = {support_module}.Namespace()\n'.format(
                namespace=namespace, support_module=support_module))
        output.write('\n')

    # add the imports for each module
    for current_module in sorted(searcher.module.included_modules, key=Module.sort_key):
        namespace_imports = []
        namespace_assignments = []
        for name in sorted(current_module.global_namespaces, key=sort_key_lowercase):
            unique_name = current_module.get_unique_name('_' + name)
            namespace_imports.append('{name} as {unique_name}'.format(
                name=name, unique_name=unique_name))
            # need to clone so we don't change the original module
            namespace_assignments.append('{name}.update({unique_name}.deep_clone())\n'.format(
                name=name, unique_name=unique_name))

        decl_imports = sorted(current_module.global_decls, key=sort_key_lowercase)

        if namespace_imports:
            output.write('from {module_name} import {imports}\n'.format(
                module_name=current_module.name,
                imports=', '.join(namespace_imports)))
            for line in namespace_assignments:
                output.write(line)
        if decl_imports:
            output.write('from {module_name} import {imports}\n'.format(
                module_name=current_module.name,
                imports=', '.join(decl_imports)))
        if namespace_imports or decl_imports:
            output.write('\n')

    DeclEmitter(output, module=searcher.module).visit(tree)

if __name__ == '__main__':
    local_current_path = os.path.dirname(inspect.getfile(inspect.currentframe()))
    local_support_path = os.path.normpath(os.path.join(local_current_path, '..', 'support', 'python'))

    from clad import emitterutil
    option_parser = emitterutil.StandardArgumentParser('python')
    options = option_parser.parse_args()

    tree = emitterutil.parse(options)
    main_output_file = emitterutil.get_output_file(options, '.py')
    comment_lines = emitterutil.get_comment_lines(options, 'python')

    emitterutil.write_python_file(options.output_directory, main_output_file,
        lambda output: emit_body(tree, output),
        comment_lines,
        additional_paths=[local_support_path],
        import_modules=[support_module])

    if options.output_directory != '-' and main_output_file != '-':
        current_path = options.output_directory
        for component in os.path.dirname(main_output_file).split('/'):
            if component:
                current_path = os.path.join(current_path, component)
                emitterutil.create_file(os.path.join(current_path, '__init__.py'))
