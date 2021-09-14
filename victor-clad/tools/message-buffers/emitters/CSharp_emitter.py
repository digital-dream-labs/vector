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

from clad import ast
from clad import clad
from clad import emitterutil

class CSharpQualifiedNamer(object):

    @classmethod
    def join(cls, *args):
        return '.'.join(args)

    @classmethod
    def disambiguate(cls, qualified_name):
        return "global::" + qualified_name

# primitive type translation map
type_translations = {
    'bool': 'bool',
    'int_8': 'sbyte',
    'int_16': 'short',
    'int_32': 'int',
    'int_64': 'long',
    'uint_8': 'byte',
    'uint_16': 'ushort',
    'uint_32': 'uint',
    'uint_64': 'ulong',
    'float_32': 'float',
    'float_64': 'double'
}

# primitive type reading translation map (for BinaryReader)
read_translations = {
    'bool': 'ReadBoolean',
    'int_8': 'ReadSByte',
    'int_16': 'ReadInt16',
    'int_32': 'ReadInt32',
    'int_64': 'ReadInt64',
    'uint_8': 'ReadByte',
    'uint_16': 'ReadUInt16',
    'uint_32': 'ReadUInt32',
    'uint_64': 'ReadUInt64',
    'float_32': 'ReadSingle',
    'float_64': 'ReadDouble'
}

def primitive_type_name(type):
    type_name = type.name
    return type_translations[type_name] if type_name in type_translations else None

def cast_type(type):
    if primitive_type_name(type) is not None:
        return type_translations[type.name]
    elif isinstance(type, ast.PascalStringType):
        return "string"
    else:
        return type.fully_qualified_name(CSharpQualifiedNamer)

class NamespaceEmitter(ast.NodeVisitor):

    def __init__(self, output, include_extension=None):
        self.output = output

    def visit_IncludeDecl(self, node, *args, **kwargs):
        pass

    def visit_NamespaceDecl(self, node, *args, **kwargs):
        self.output.write('namespace {namespace_name} {{\n\n'.format(namespace_name=node.name))
        self.generic_visit(node, *args, **kwargs)
        self.output.write('}} // namespace {namespace_name}\n\n'.format(namespace_name=node.name))

    def visit_EnumDecl(self, node, *args, **kwargs):
        EnumEmitter(self.output).visit(node, *args, **kwargs)

    def visit_MessageDecl(self, node, *args, **kwargs):
        StructEmitter(self.output).visit(node, *args, **kwargs)

    def visit_UnionDecl(self, node, *args, **kwargs):
        UnionEmitter(self.output).visit(node, *args, **kwargs)

    def visit_EnumConceptDecl(self, node, *args, **kwargs):
        EnumConceptEmitter(self.output).visit(node, *args, **kwargs)

class EnumEmitter(ast.NodeVisitor):
    def __init__(self, output=sys.stdout):
        self.output = output

    def visit_EnumDecl(self, node):
        if self.usesBitflags(node):
            self.output.write('[System.Flags]\n')

        header = textwrap.dedent("""\
        public enum {0} : {1}
        {{
        """.format(node.name, type_translations[node.storage_type.name]))
        self.output.write(header)
        members = node.members()
        #print each member of the enum
        for i, member in enumerate(members):
            self.emitEnum(member, i < len(members) - 1, prefix='')
        #print the footer
        footer = '};\n\n'
        self.output.write(footer)

    def usesBitflags(self, node):
        """
        Returns True if the field is considered a list of bitflags.
        Right now that's True if it is not contiguous and no value is negative.
        System.FlagsAttribute is for enums that are bitflags.
        All it really does is change the ToString method a little.
        Currently, I don't think Unity uses it in the inspector, but that may change.
        """
        if not node.members():
            return False

        current = -1
        values = set()
        for member in node.members():
            if member.initializer and type(member.initializer.value) is str:
              return False
            current = member.initializer.value if member.initializer else current + 1
            values.add(current)

        minimum, maximum, length = min(values), max(values), len(values)
        return minimum >= 0 and maximum - minimum + 1 != length

    def emitEnum(self, member, trailing_comma=True, prefix=''):
        separator = ',' if trailing_comma else ''
        value = member.value
        
        if type(value) is str and "::" in member.value:
            value = value.replace("::", ".")
    
        self.output.write('\t' + prefix + member.name + ' = {0}'.format(value) + separator + '\n')

class StructEmitter(ast.NodeVisitor):
    def __init__(self, output=sys.stdout):
        self.output = output

    def visit_MessageDecl(self, node):
        globals = dict(
            message_name=node.name)
        
        header = 'public partial class {message_name}\n{{\n'.format(**globals)
        footer = '}\n\n'
        self.output.write(header)
        self.emitFields(node, globals)
        self.emitConstructors(node, globals)
        self.emitInitializers(node, globals)
        self.emitUnpack(node, globals)
        self.emitPack(node, globals)
        self.emitSize(node, globals)
        self.emitEquals(node, globals)
        self.emitGetHashCode(node, globals)
        self.output.write(footer)

    def emitFields(self, node, globals):
        if node.members():
            visitor = TypeVisitor(self.output)
            for member in node.members():
                self.output.write('\tprivate ')
                visitor.visit(member.type)
                self.output.write(' _{member_name}'.format(member_name=member.name))
                if member.init:
                    initial_value = member.init
                    member_val = initial_value.value
                    member_str = ""
                    if member.type.name == "bool":
                        member_str = "false" if member_val == 0 else "true"
                    elif initial_value.type == "hex":
                        member_str = hex(member_val)
                    elif member.type.name == "float_32":
                        member_str = str(member_val) + "f"
                    elif member.type.name == "float_64":
                        member_str = str(member_val) + "d"
                    elif member.init is not None and type(member.init.value) is str:
                        member_str=member.init.value
                        if "::" in member_str:
                            # Replace '::' with '.'
                            member_str = member_str.replace("::", ".")
                    else:
                        member_str = str(member_val)
                    self.output.write(" = %s" % member_str)
                self.output.write('; // {member_type}\n'.format(member_type=member.type.name))

            self.output.write('\n')

            visitor = PropertyVisitor(self.output)
            for member in node.members():
                visitor.visit(member.type, member_name=member.name)
            self.output.write('\n')

    def emitEquals(self, node, globals):
        if node.members():
            self.output.write(textwrap.dedent('''\
            \tpublic static bool ArrayEquals<T>(System.Collections.Generic.IList<T> a1, System.Collections.Generic.IList<T> a2) {{
            \t\tif (System.Object.ReferenceEquals(a1, a2))
            \t\t\treturn true;

            \t\tif (System.Object.ReferenceEquals(a1, null) || System.Object.ReferenceEquals(a2, null))
            \t\t\treturn false;

            \t\tif (a1.Count != a2.Count)
            \t\t\treturn false;

            \t\tfor (int i = 0; i < a1.Count; i++)
            \t\t{{
            \t\t\tif (!a1[i].Equals(a2[i])) {{
            \t\t\t\treturn false;
            \t\t\t}}
            \t\t}}
            \t\treturn true;
            \t}}

            \tpublic static bool operator ==({message_name} a, {message_name} b)
            \t{{
            \t\tif (System.Object.ReferenceEquals(a, null))
            \t\t{{
            \t\t\treturn System.Object.ReferenceEquals(b, null);
            \t\t}}

            \t\treturn a.Equals(b);
            \t}}

            \tpublic static bool operator !=({message_name} a, {message_name} b)
            \t{{
            \t\treturn !(a == b);
            \t}}

            \tpublic override bool Equals(System.Object obj)
            \t{{
            \t\treturn this.Equals(obj as {message_name});
            \t}}

            \tpublic bool Equals({message_name} p)
            \t{{
            \t\tif (System.Object.ReferenceEquals(p, null))
            \t\t{{
            \t\t\treturn false;
            \t\t}}

            #''')[:-1].format(**globals))
            self.output.write('\t\treturn ');
            for i,member in enumerate(node.members()):
                if i > 0:
                    self.output.write('\n\t\t\t&& ')
                if (isinstance(member.type, ast.FixedArrayType) or
                    isinstance(member.type, ast.VariableArrayType) and not
                    isinstance(member.type, ast.PascalStringType)):
                    self.output.write('ArrayEquals<{0}>(this._{1},p._{1})'.
                                   format(cast_type(member.type.member_type),member.name))
                else:
                    self.output.write('this._{0}.Equals(p._{0})'.format(member.name))
            self.output.write(';\n\t}\n\n')

    def emitGetHashCode(self, node, globals):
        if node.members():
            self.output.write(textwrap.dedent('''\
            \tpublic override int GetHashCode()
            \t{
            \t\tunchecked
            \t\t{
            \t\t\tint hash = 17;
            #''')[:-1])
            for member in node.members():
                if (isinstance(member.type, ast.FixedArrayType) or
                    isinstance(member.type, ast.VariableArrayType)):
                    #todo: string/array hash codes (also handle nulls)
                    pass
                else:
                    self.output.write('\t\t\thash = hash * 23 + this._{member_name}.GetHashCode();\n'.format(
                        member_name=member.name))
            self.output.write(textwrap.dedent('''\
            \t\t\treturn hash;
            \t\t}
            \t}
            #''')[:-1])

    def emitConstructors(self, node, globals):
        self.output.write('\t/**** Constructors ****/\n\n')

        if(node.default_constructor and emitterutil._do_all_members_have_default_constructor(node)):
            self.emitDefaultConstructor(node, globals)
        if node.members():
            self.emitValueConstructor(node, globals)
        self.emitUnpackConstructors(node, globals)

    def emitDefaultConstructor(self, node, globals):
        self.output.write('\tpublic {message_name}()\n\t{{\n'.format(**globals))
        for member in node.members():
            if isinstance(member.type, ast.FixedArrayType):
                length = member.type.length
                if isinstance(length, str) and "::" in length:
                    length = length.replace("::", ".")
                    length = "(int)" + length
                self.output.write('\t\tthis.{member_name} = new {element_type}[{length}];\n'.format(
                    member_name=member.name,
                    element_type=cast_type(member.type.member_type),
                    length=length))
            elif isinstance(member.type, ast.CompoundType):
                self.output.write('\t\tthis.{member_name} = new {member_type}();\n'.format(
                    member_name=member.name,
                    member_type=member.type.fully_qualified_name(CSharpQualifiedNamer)))
        self.output.write('\t}\n\n')

    def emitValueConstructor(self, node, globals):
        self.output.write('\tpublic {message_name}('.format(**globals))

        visitor = TypeVisitor(self.output)
        for i, member in enumerate(node.members()):
            visitor.visit(member.type)
            self.output.write(' {member_name}'.format(member_name=member.name))
            if (i < len(node.members()) - 1):
                self.output.write(',\n\t\t')

        self.output.write(')\n')

        self.output.write('\t{\n')
        for member in node.members():
            self.output.write('\t\tthis.{member_name} = {member_name};\n'.format(member_name=member.name))
        self.output.write('\t}\n\n')

    def emitUnpackConstructors(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tpublic {message_name}(System.IO.Stream stream)
            \t{{
            \t\tUnpack(stream);
            \t}}

            \tpublic {message_name}(System.IO.BinaryReader reader)
            \t{{
            \t\tUnpack(reader);
            \t}}

            #''')[:-1].format(**globals))

    def emitUnpack(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tpublic void Unpack(System.IO.Stream stream)
            \t{{
            #''')[:-1].format(**globals))

        if node.members():
            self.output.write(textwrap.dedent('''\
                \t\tSystem.IO.BinaryReader reader = new System.IO.BinaryReader(stream);
                \t\tUnpack(reader);
                #''')[:-1].format(**globals))

        self.output.write(textwrap.dedent('''\
            \t}}

            \tpublic void Unpack(System.IO.BinaryReader reader)
            \t{{
            #''')[:-1].format(**globals))

        visitor = UnpackVisitor(self.output)
        for member in node.members():
            visitor.visit(member.type, destination='_{member_name}'.format(member_name=member.name), depth=2)

        self.output.write(textwrap.dedent('''\
            \t}}

            #''')[:-1].format(**globals))

    def emitPack(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tpublic void Pack(System.IO.Stream stream)
            \t{{
            #''')[:-1].format(**globals))

        if node.members():
            self.output.write(textwrap.dedent('''\
                \t\tSystem.IO.BinaryWriter writer = new System.IO.BinaryWriter(stream);
                \t\tPack(writer);
                #''')[:-1].format(**globals))

        self.output.write(textwrap.dedent('''\
            \t}}

            \tpublic void Pack(System.IO.BinaryWriter writer)
            \t{{
            #''')[:-1].format(**globals))

        visitor = PackVisitor(self.output)
        for member in node.members():
            visitor.visit(member.type, value='_{member_name}'.format(member_name=member.name), depth=2)

        self.output.write(textwrap.dedent('''\
            \t}}

            #''')[:-1].format(**globals))

    def isUnion(self, t):
        # I assume there is a better way to determine this. Someone who knows
        # the system better should fix this.
        return ("Union" in t.name and type(t) is ast.CompoundType);

    def emitInitializers(self, node, globals):
        if node.members():
            self.emitInitializer(node, globals)

            union_count = sum(self.isUnion(m[1].type) for m in enumerate(node.members()));
            if union_count > 0:
                self.emitGenericInitializer(node, union_count, globals)


    def emitInitializer(self, node, globals):
      self.output.write('\tpublic {message_name} Initialize('.format(**globals))
      
      visitor = TypeVisitor(self.output)
      for i, member in enumerate(node.members()):
          visitor.visit(member.type)
          self.output.write(' {member_name}'.format(member_name=member.name))
          if (i < len(node.members()) - 1):
              self.output.write(',\n\t\t')
    
      self.output.write(')\n')
      
      self.output.write('\t{\n')
      for member in node.members():
          self.output.write('\t\tthis.{member_name} = {member_name};\n'.format(member_name=member.name))
      self.output.write('\t\treturn this;\n')
      self.output.write('\t}\n\n')

    def emitGenericInitializer(self, node, union_count, globals):
        self.output.write('\tpublic {message_name} Initialize<'.format(**globals))
        
        for i in range(union_count):
            if(i > 0):
                self.output.write(', ')
            self.output.write('T%d' % i)
        
        self.output.write('>(')

        visitor = TypeVisitor(self.output)
        union_index = 0
        for i, member in enumerate(node.members()):
            if self.isUnion(member.type):
                self.output.write('T%d' % union_index)
                self.output.write(' {member_name}State'.format(member_name=member.name))
                union_index += 1
            else:
                visitor.visit(member.type)
                self.output.write(' {member_name}'.format(member_name=member.name))
            if (i < len(node.members()) - 1):
                self.output.write(',\n\t\t')

        self.output.write(')\n')

        self.output.write('\t{\n')
        union_index = 0
        for member in node.members():
          if self.isUnion(member.type):
              self.output.write('\t\tthis.{member_name}.Initialize<T{union_index}>({member_name}State);\n'.format(member_name=member.name, union_index=union_index))
              union_index += 1
          else:
              self.output.write('\t\tthis.{member_name} = {member_name};\n'.format(member_name=member.name))
        self.output.write('\t\treturn this;\n')
        self.output.write('\t}\n\n')


    def emitSize(self, node, globals):
        
        self.output.write('>(')

        visitor = TypeVisitor(self.output)
        union_index = 0
        for i, member in enumerate(node.members()):
            if self.isUnion(member.type):
                self.output.write('T%d' % union_index)
                self.output.write(' {member_name}State'.format(member_name=member.name))
                union_index += 1
            else:
                visitor.visit(member.type)
                self.output.write(' {member_name}'.format(member_name=member.name))
            if (i < len(node.members()) - 1):
                self.output.write(',\n\t\t')

        self.output.write(')\n')

        self.output.write('\t{\n')
        union_index = 0
        for member in node.members():
          if self.isUnion(member.type):
              self.output.write('\t\tthis.{member_name}.Initialize<T{union_index}>({member_name}State);\n'.format(member_name=member.name, union_index=union_index))
              union_index += 1
          else:
              self.output.write('\t\tthis.{member_name} = {member_name};\n'.format(member_name=member.name))
        self.output.write('\t\treturn this;\n')
        self.output.write('\t}\n\n')

    def emitSize(self, node, globals):

        self.output.write(textwrap.dedent('''\
            \tpublic int Size
            \t{{
            \t\tget {{
            #''')[:-1].format(**globals))

        if node.is_message_size_fixed():
            max_size = node.max_message_size()
            size = 0
            if isinstance(max_size, list):
                size = ""
                # For every unique element in max_size
                for s in set(max_size):
                    # If it is a string then generate code that casts it to an int and multiplies it by the number
                    # of occurences there are in max_size
                    if isinstance(s, str):
                        s_count = max_size.count(s)
                        s = "((int)" + s + ")"
                        if s_count > 1:
                            s += "*" + str(s_count)
                        size += (s.replace("::", "."))
                    else:
                        size += str(s)
                    size += "+"
                # remove last '+' that was added
                size = size[:-1]
            else:
                size = max_size
            self.output.write('\t\t\treturn {size};\n'.format(size=size))
        else:
            self.output.write('\t\t\tint result = 0;\n')
            visitor = SizeVisitor(self.output)
            for member in node.members():
                visitor.visit(member.type, value=member.name, depth=3)
            self.output.write('\t\t\treturn result;\n')

        self.output.write(textwrap.dedent('''\
            \t\t}}
            \t}}

            #''')[:-1].format(**globals))

    def visit_UnionDecl(self, node):
        pass

class TypeVisitor(ast.NodeVisitor):

    def __init__(self, output=sys.stdout):
        self.output = output

    def visit_BuiltinType(self, node):
        self.output.write(type_translations[node.name])

    def visit_DefinedType(self, node):
        self.output.write(node.fully_qualified_name(CSharpQualifiedNamer))

    def visit_CompoundType(self, node):
        self.output.write(node.fully_qualified_name(CSharpQualifiedNamer))

    def visit_PascalStringType(self, node):
        self.output.write('string')

    def visit_VariableArrayType(self, node):
        self.visit(node.member_type)
        self.output.write('[]')

    def visit_FixedArrayType(self, node):
        self.visit(node.member_type)
        self.output.write('[]')

class PropertyVisitor(ast.NodeVisitor):

    def __init__(self, output=sys.stdout):
        self.output = output
        self.type_visitor = TypeVisitor(self.output)

    def visit_Terse(self, node, member_name):
        self.output.write('\tpublic ')
        self.type_visitor.visit(node)
        self.output.write(' {member_name} {{ get {{ return _{member_name}; }} set {{ _{member_name} = value; }} }}\n\n'.format(
            member_name=member_name))

    def visit_Verbose(self, node, member_name, check_body):
        self.output.write('\tpublic ')
        self.type_visitor.visit(node)
        self.output.write(' {member_name}\n'.format(
            member_name=member_name))
        self.output.write(textwrap.dedent('''\
            \t{{
            \t\tget {{
            \t\t\treturn _{member_name};
            \t\t}}
            \t\tset {{
            #''')[:-1].format(member_name=member_name))

        self.output.write(check_body);

        self.output.write(textwrap.dedent('''\
            \t\t\t_{member_name} = value;
            \t\t}}
            \t}}

            #''')[:-1].format(member_name=member_name))

    def visit_BuiltinType(self, node, member_name):
        self.visit_Terse(node, member_name)

    def visit_DefinedType(self, node, member_name):
        self.visit_Terse(node, member_name)

    def visit_CompoundType(self, node, member_name):
        self.visit_Verbose(node, member_name,
            check_body=textwrap.dedent('''\
            \t\t\tif (value == null) {{
            \t\t\t\tthrow new System.ArgumentNullException("{member_name} cannot be null.", "value");
            \t\t\t}}
            #''')[:-1].format(member_name=member_name))

    def visit_PascalStringType(self, node, member_name):
        self.visit_Verbose(node, member_name,
            check_body=textwrap.dedent('''\
            \t\t\tif (!string.IsNullOrEmpty(value) && System.Text.Encoding.UTF8.GetByteCount(value) > {max_length}) {{
            \t\t\t\tthrow new System.ArgumentException("{member_name} string is too long. Must decode to less than or equal to {max_length} bytes.", "value");
            \t\t\t}}
            #''')[:-1].format(member_name=member_name, max_length=node.length_type.max))

    def visit_VariableArrayType(self, node, member_name):
        if node.length_type.max == (2 << 63) - 1:
            self.visit_Terse(node, member_name)
        else:
            max_length = node.max_length if node.max_length_is_specified else node.length_type.max
            self.visit_Verbose(node, member_name,
                check_body=textwrap.dedent('''\
                \t\t\tif (value != null && value.Length > {max_length}) {{
                \t\t\t\tthrow new System.ArgumentException("{member_name} variable-length array is too long. Must be less than or equal to {max_length}.", "value");
                \t\t\t}}
                #''')[:-1].format(member_name=member_name, max_length=max_length))
        
    def visit_FixedArrayType(self, node, member_name):
        length = node.length
        if isinstance(length, str) and "::" in length:
            length = length.replace("::", ".")
            length = "(int)" + length
        self.visit_Verbose(node, member_name,
            check_body=textwrap.dedent('''\
            \t\t\tif (value == null) {{
            \t\t\t\tthrow new System.ArgumentException("{member_name} fixed-length array is null. Must have a length of {length}.", "value");
            \t\t\t}}
            \t\t\tif (value.Length != {length}) {{
            \t\t\t\tthrow new System.ArgumentException("{member_name} fixed-length array is the wrong size. Must have a length of {length}.", "value");
            \t\t\t}}
            #''')[:-1].format(member_name=member_name, length=length))

class UnpackVisitor(ast.NodeVisitor):

    def __init__(self, output=sys.stdout):
        self.output = output

    def get_index_variable(self, depth):
        return chr(ord('i') + (depth - 2))

    def visit_BuiltinType(self, node, destination, depth):
        self.output.write('{indent}{destination} = reader.{read_method}();\n'.format(
            indent='\t' * depth,
            destination=destination,
            read_method=read_translations[node.builtin_type().name]))

    def visit_DefinedType(self, node, destination, depth):
        self.output.write('{indent}{destination} = ({defined_type})reader.{read_method}();\n'.format(
            indent='\t' * depth,
            destination=destination,
            defined_type=node.fully_qualified_name(CSharpQualifiedNamer),
            read_method=read_translations[node.builtin_type().name]))

    def visit_CompoundType(self, node, destination, depth):
        self.output.write('{indent}{destination} = new {member_type}(reader);\n'.format(
            indent='\t' * depth,
            destination=destination,
            member_type=node.fully_qualified_name(CSharpQualifiedNamer)))

    def visit_PascalStringType(self, node, destination, depth):
        prefix = destination.replace('[', '_').replace(']', '')
        length_variable = '{prefix}_length'.format(prefix=prefix)
        bytes_variable = '{prefix}_bytes'.format(prefix=prefix)
        self.output.write(textwrap.dedent('''\
            {indent}int {length_variable} = (int)reader.{read_length_method}();
            {indent}byte[] {bytes_variable} = reader.ReadBytes({length_variable});
            {indent}{destination} = System.Text.Encoding.UTF8.GetString({bytes_variable});
            ''').format(
            indent='\t' * depth,
            destination=destination,
            length_variable=length_variable,
            bytes_variable=bytes_variable,
            read_length_method=read_translations[node.length_type.name]))

    def visit_VariableArrayType(self, node, destination, depth):
        prefix = destination.replace('[', '_').replace(']', '')
        length_variable = '{prefix}_length'.format(prefix=prefix)
        index_variable = self.get_index_variable(depth)
        self.output.write(textwrap.dedent('''\
            {indent}int {length_variable} = (int)reader.{read_length_method}();
            {indent}{destination} = new {element_type}[{length_variable}];
            {indent}for (int {index} = 0; {index} < {length_variable}; ++{index}) {{
            ''').format(
            indent='\t' * depth,
            destination=destination,
            element_type=cast_type(node.member_type),
            length_variable=length_variable,
            index=index_variable,
            read_length_method=read_translations[node.length_type.name]))
        self.visit(node.member_type, '{old}[{index}]'.format(old=destination, index=index_variable), depth + 1)
        self.output.write('{indent}}}\n'.format(indent='\t' * depth))

    def visit_FixedArrayType(self, node, destination, depth):
        index_variable = self.get_index_variable(depth)
        length = node.length
        if isinstance(length, str) and "::" in length:
            length = length.replace("::", ".")
            length = "(int)" + length
        self.output.write(textwrap.dedent('''\
            {indent}{destination} = new {element_type}[{length}];
            {indent}for (int {index} = 0; {index} < {length}; ++{index}) {{
            ''').format(
            indent='\t' * depth,
            destination=destination,
            element_type=cast_type(node.member_type),
            length=length,
            index=index_variable))
        self.visit(node.member_type, '{old}[{index}]'.format(old=destination, index=index_variable), depth + 1)
        self.output.write('{indent}}}\n'.format(indent='\t' * depth))

class PackVisitor(ast.NodeVisitor):

    def __init__(self, output=sys.stdout):
        self.output = output

    def get_index_variable(self, depth):
        return chr(ord('i') + (depth - 2) // 2)

    def visit_BuiltinType(self, node, value, depth):
        self.output.write('{indent}writer.Write(({builtin_type}){value});\n'.format(
            indent='\t' * depth,
            value=value,
            builtin_type=cast_type(node.builtin_type())))

    def visit_DefinedType(self, node, value, depth):
        self.visit_BuiltinType(node, value, depth)

    def visit_CompoundType(self, node, value, depth):
        if depth > 2:
            self.output.write(textwrap.dedent('''\
                {indent}if ({value} == null) {{
                {indent}\tthrow new System.InvalidOperationException("Arrays in messages may not have null entries.");
                {indent}}}
                ''').format(
                    indent='\t' * depth,
                    value=value))
        self.output.write('{indent}{value}.Pack(writer);\n'.format(
            indent='\t' * depth,
            value=value))

    def visit_PascalStringType(self, node, value, depth):
        prefix = value.replace('[', '_').replace(']', '')
        bytes_variable = '{prefix}_bytes'.format(prefix=prefix)
        self.output.write(textwrap.dedent('''\
            {indent}if ({value} != null) {{
            {indent}\tbyte[] {bytes_variable} = System.Text.Encoding.UTF8.GetBytes({value});
            {indent}\twriter.Write(({length_type}){bytes_variable}.Length);
            {indent}\twriter.Write({bytes_variable});
            {indent}}}
            {indent}else {{
            {indent}\twriter.Write(({length_type})0);
            {indent}}}
            ''').format(
            indent='\t' * depth,
            value=value,
            bytes_variable=bytes_variable,
            length_type=cast_type(node.length_type.builtin_type())))

    def visit_VariableArrayType(self, node, value, depth):
        index_variable = self.get_index_variable(depth)
        self.output.write(textwrap.dedent('''\
            {indent}if ({value} != null) {{
            {indent}\twriter.Write(({length_type}){value}.Length);
            {indent}\tfor (int {index} = 0; {index} < {value}.Length; ++{index}) {{
            ''').format(
            indent='\t' * depth,
            value=value,
            index=index_variable,
            length_type=cast_type(node.length_type.builtin_type())))
        self.visit(node.member_type, '{old}[{index}]'.format(old=value, index=index_variable), depth + 2)
        self.output.write(textwrap.dedent('''\
            {indent}\t}}
            {indent}}}
            {indent}else {{
            {indent}\twriter.Write(({length_type})0);
            {indent}}}
            ''').format(
            indent='\t' * depth,
            length_type=cast_type(node.length_type.builtin_type())))

    def visit_FixedArrayType(self, node, value, depth):
        index_variable = self.get_index_variable(depth)
        length=node.length
        if isinstance(length, str) and "::" in length:
            length = length.replace("::", ".")
            length = "(int)" + length
        self.output.write(textwrap.dedent('''\
            {indent}for (int {index} = 0; {index} < {length}; ++{index}) {{
            ''').format(
            indent='\t' * depth,
            length=length,
            index=index_variable))
        self.visit(node.member_type, '{old}[{index}]'.format(old=value, index=index_variable), depth + 1)
        self.output.write('{indent}}}\n'.format(indent='\t' * depth))

class SizeVisitor(ast.NodeVisitor):

    def __init__(self, output=sys.stdout):
        self.output = output

    def get_index_variable(self, depth):
        return chr(ord('i') + (depth - 2) // 2)

    def visit_BuiltinType(self, node, value, depth):
        self.output.write('{indent}result += {size}; // {type}\n'.format(
            indent='\t' * depth,
            size=node.max_message_size(),
            type=node.name))

    def visit_DefinedType(self, node, value, depth):
        self.visit_BuiltinType(node, value, depth)

    def visit_CompoundType(self, node, value, depth):
        if depth > 3:
            self.output.write(textwrap.dedent('''\
                {indent}if ({value} == null) {{
                {indent}\tthrow new System.InvalidOperationException("Messages may not have null members.");
                {indent}}}
                ''').format(
                    indent='\t' * depth,
                    value=value))
        self.output.write('{indent}result += {value}.Size;\n'.format(
            indent='\t' * depth,
            value=value))

    def visit_PascalStringType(self, node, value, depth):
        self.output.write(textwrap.dedent('''\
            {indent}result += {length_size}; // {length_type}
            {indent}if ({value} != null) {{
            {indent}\tresult += System.Text.Encoding.UTF8.GetByteCount({value});
            {indent}}}
            ''').format(
            indent='\t' * depth,
            length_type=node.length_type.name,
            length_size=node.length_type.max_message_size(),
            value=value))

    def visit_VariableArrayType(self, node, value, depth):
        self.output.write(textwrap.dedent('''\
            {indent}result += {length_size}; // {length_type}
            {indent}if ({value} != null) {{
            ''').format(
            indent='\t' * depth,
            value=value,
            length_size=node.length_type.max_message_size(),
            length_type=node.length_type.name))
        if node.member_type.is_message_size_fixed():
            self.output.write('{indent}\tresult += {value}.Length * {element_size}; // {type}\n'.format(
                indent='\t' * depth,
                value=value,
                element_size=node.member_type.max_message_size(),
                type=node.member_type.name))
        else:
            index_variable = self.get_index_variable(depth)
            self.output.write('{indent}\tfor (int {index} = 0; {index} < {value}.Length; ++{index}) {{\n'.format(
                indent='\t' * depth,
                index=index_variable,
                value=value))
            self.visit(node.member_type, '{old}[{index}]'.format(old=value, index=index_variable), depth + 2)
            self.output.write('{indent}\t}}\n'.format(indent='\t' * depth))
        self.output.write('{indent}}}\n'.format(indent='\t' * depth))

    def visit_FixedArrayType(self, node, value, depth):
        if node.is_message_size_fixed():
            length = node.length
            if isinstance(length, str) and "::" in length:
                length = length.replace("::", ".")
                length = "(int)" + length
            self.output.write('{indent}result += {length} * {element_size};\n'.format(
                indent='\t' * depth,
                length=length,
                element_size=node.member_type.max_message_size()))
        else:
            index_variable = self.get_index_variable(depth)
            self.output.write('{indent}for (int {index} = 0; {index} < {length}; ++{index}) {{\n'.format(
                indent='\t' * depth,
                length = node.length,
                index=index_variable))
            self.visit(node.member_type, '{old}[{index}]'.format(old=value, index=index_variable), depth + 1)
            self.output.write('{indent}}}\n'.format(indent='\t' * depth))


class UnionEmitter(ast.NodeVisitor):
    def __init__(self, output=sys.stdout, prefix=''):
        self.output = output
        self.prefix = prefix

    def visit_UnionDecl(self, node):
        globals = dict(union_name=node.name)

        header = "public class {union_name} {{\n".format(**globals)
        footer = '}\n\n'
        members = node.members()
        self.output.write(header)
        #union stuff
        with self.output.indent(1):
            self.emitTags(node, globals)
        self.emitTagMember(node, globals)
        self.emitGetTag(node, globals)
        self.emitUnion(node, globals)
        self.emitAccessors(node, globals)
        self.emitInitializer(node, globals)
        self.emitConstructors(node, globals)
        self.emitUnpack(node, globals)
        self.emitPack(node, globals)
        self.emitSize(node, globals)
        self.emitEquals(node, globals)
        self.emitGetHashCode(node, globals)
        #end union stuff
        self.output.write(footer)

    def emitTags(self, node, globals):
        self.output.write('public enum Tag {\n')

        with self.output.indent(1):
            lines = []
            for i, member in enumerate(node.members()):
                if member.init:
                    initializer = hex(member.tag) if member.init.type == "hex" else str(member.tag)
                    start = '{member_name}'.format(member_name=member.name, **globals)
                    middle = ' = {initializer},'.format(initializer=initializer)
                else:
                    start = '{member_name},'.format(member_name=member.name, **globals)
                    middle = ''
                end = ' // {value}'.format(value=member.tag)
                lines.append((start, middle, end))

            start = 'INVALID'.format(**globals)
            middle = ' = {invalid_tag}'.format(invalid_tag=node.invalid_tag)
            lines.append((start, middle))
            self.output.write_with_aligned_whitespace(lines)

        self.output.write('};\n\n')

    def emitTagMember(self, node, globals):
        self.output.write('\tprivate Tag _tag = Tag.INVALID;\n\n')

    def emitUnion(self, node, globals):
        self.output.write('\tprivate object _state = null;\n\n')
        self.output.write('\tpublic object GetState() { return _state; }\n\n')
    def emitGetTag(self, node, globals):
        self.output.write('\tpublic Tag GetTag() { return _tag; }\n\n')

    def emitAccessors(self, node, globals):
        for member in node.members():
            Union_AccessorEmitter(output=self.output,
                                     prefix='_').visit(member)

    def emitInitializer(self, node, globals):
        self.output.write('\tpublic {union_name} Initialize<T>('.format(**globals))
        self.output.write('T state')
        self.output.write(')\n')
        self.output.write('\t{\n')
        self.output.write('\t\t_state = state;\n')
        visitor = TypeVisitor(self.output)
        for i, member in enumerate(node.members()):
            self.output.write('\t\t')
            if i > 0:
                self.output.write('else ')
            self.output.write('if(typeof(T) == typeof(')
            visitor.visit(member.type)
            self.output.write('))\n\t\t{\n');
            self.output.write('\t\t\t_tag = Tag.{tag};\n'.format(tag=member.name));
            self.output.write('\t\t}\n')
        self.output.write('\t\telse\n\t\t{\n')
        self.output.write('\t\t\t_state = null;\n')
        self.output.write('\t\t\t_tag = Tag.INVALID;\n')
        self.output.write('\t\t}\n\n')
        self.output.write('\t\treturn this;\n')
        self.output.write('\t}\n\n')

    def emitConstructors(self, node, globals):
        self.output.write('\t/**** Constructors ****/\n\n')

        self.emitDefaultConstructor(node, globals)
        self.emitUnpackConstructors(node, globals)

    def emitDefaultConstructor(self, node, globals):
        self.output.write('\tpublic {union_name}()\n\t{{\n'.format(**globals))
        self.output.write('\t}\n\n')

    def emitUnpackConstructors(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tpublic {union_name}(System.IO.Stream stream)
            \t{{
            \t\tUnpack(stream);
            \t}}

            \tpublic {union_name}(System.IO.BinaryReader reader)
            \t{{
            \t\tUnpack(reader);
            \t}}

            #''')[:-1].format(**globals))

    def emitUnpack(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tpublic void Unpack(System.IO.Stream stream)
            \t{{
            #''')[:-1].format(**globals))

        if node.members():
            self.output.write(textwrap.dedent('''\
                \t\tSystem.IO.BinaryReader reader = new System.IO.BinaryReader(stream);
                \t\tUnpack(reader);
                #''')[:-1].format(**globals))

        self.output.write(textwrap.dedent('''\
            \t}}

            \tpublic void Unpack(System.IO.BinaryReader reader)
            \t{{
            \t\tTag newTag = (Tag)reader.{tag_type}();
            \t\tswitch(newTag) {{
            #''')[:-1].format(
                tag_type=read_translations[node.tag_storage_type.name],
                **globals))

        visitor = UnpackVisitor(self.output)
        for member in node.members():
            self.output.write('\t\tcase Tag.%s:\n' % member.name)
            visitor.visit(member.type, destination=member.name, depth=3)
            self.output.write('\t\t\tbreak;\n')

        self.output.write(textwrap.dedent('''\
            \t\tdefault:
            \t\t\tbreak;\n
            \t\t}}
            \t\t_tag = newTag;
            \t}}

            #''')[:-1].format(**globals))

    def emitPack(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \tpublic void Pack(System.IO.Stream stream)
            \t{{
            #''')[:-1].format(**globals))

        if node.members():
            self.output.write(textwrap.dedent('''\
                \t\tSystem.IO.BinaryWriter writer = new System.IO.BinaryWriter(stream);
                \t\tPack(writer);
                #''')[:-1].format(**globals))

        self.output.write(textwrap.dedent('''\
            \t}}

            \tpublic void Pack(System.IO.BinaryWriter writer)
            \t{{
            \t\twriter.Write(({tag_type})GetTag());
            \t\tswitch(GetTag()) {{
            #''')[:-1].format(
                tag_type=type_translations[node.tag_storage_type.name],
                **globals))

        visitor = PackVisitor(self.output)
        for member in node.members():
            self.output.write('\t\tcase Tag.%s:\n' % member.name)
            visitor.visit(member.type, value=member.name, depth=3)
            self.output.write('\t\t\tbreak;\n')

        self.output.write(textwrap.dedent('''\
            \t\tdefault:
            \t\t\tbreak;
            \t\t}}
            \t}}

            #''')[:-1].format(**globals))

    def emitEquals(self, node, globals):
        self.output.write(textwrap.dedent('''\
        \tpublic static bool operator ==({union_name} a, {union_name} b)
        \t{{
        \t\tif (System.Object.ReferenceEquals(a, null))
        \t\t{{
        \t\t\treturn System.Object.ReferenceEquals(b, null);
        \t\t}}

        \t\treturn a.Equals(b);
        \t}}

        \tpublic static bool operator !=({union_name} a, {union_name} b)
        \t{{
        \t\treturn !(a == b);
        \t}}

        \tpublic override bool Equals(System.Object obj)
        \t{{
        \t\treturn this.Equals(obj as {union_name});
        \t}}

        \tpublic bool Equals({union_name} p)
        \t{{
        \t\tif (System.Object.ReferenceEquals(p, null))
        \t\t{{
        \t\t\treturn false;
        \t\t}}

        \t\treturn (_tag == p._tag && _state.Equals(p._state));
        \t}}

        #''')[:-1].format(**globals))

    def emitGetHashCode(self, node, globals):
        self.output.write(textwrap.dedent('''\
        \tpublic override int GetHashCode()
        \t{
        \t\tunchecked
        \t\t{
        \t\t\tint hash = 17;
        \t\t\thash = hash * 23 + this._tag.GetHashCode();
        \t\t\thash = hash * 23 + this._state.GetHashCode();
        \t\t\treturn hash;
        \t\t}
        \t}
        #''')[:-1])

    def emitSize(self, node, globals):

        self.output.write(textwrap.dedent('''\
            \tpublic int Size
            \t{{
            \t\tget {{
            #''')[:-1].format(**globals))

        if node.is_message_size_fixed():
            self.output.write('\t\t\treturn {size};\n'.format(size=node.max_message_size()))
        else:
            self.output.write(textwrap.dedent('''\
                \t\t\tint result = {tag_size}; // tag = {tag_type}
                \t\t\tswitch(GetTag()) {{
                #''')[:-1].format(
                    tag_size=node.tag_storage_type.max_message_size(),
                    tag_type=type_translations[node.tag_storage_type.name],
                    **globals))
            visitor = SizeVisitor(self.output)
            for member in node.members():
                self.output.write('\t\t\tcase Tag.%s:\n' % member.name)
                visitor.visit(member.type, value=member.name, depth=4)
                self.output.write('\t\t\t\tbreak;\n')
            self.output.write(textwrap.dedent('''\
                \t\t\tdefault:
                \t\t\t\t// Just tag size
                \t\t\t\tbreak;
                \t\t\t}}
                \t\t\treturn result;
                #''')[:-1].format(**globals))
        self.output.write(textwrap.dedent('''\
                \t\t}}
                \t}}\n
                #''')[:-1].format(**globals))

class Union_AccessorEmitter(ast.NodeVisitor):
    def __init__(self, output=sys.stdout, prefix=''):
        self.output = output
        self.prefix = prefix

    def visit_MessageMemberDecl(self, node):
        self.emitProperty(node)
        self.output.write('\n')

    def emitProperty(self, node):
        self.output.write('\tpublic ')
        self.visit(node.type)
        self.output.write(' %s\n' % node.name)
        self.output.write('\t{\n')
        self.output.write('\t\tget {\n')
        self.output.write('\t\t\tif (_tag != Tag.%s) {\n' % node.name)
        self.output.write('\t\t\t\tthrow new System.InvalidOperationException(string.Format(\n')
        self.output.write('\t\t\t\t\t"Cannot access union member \\"%s\\" when a value of type {0} is stored.",\n' % node.name)
        self.output.write('\t\t\t\t\t_tag.ToString()));\n')
        self.output.write('\t\t\t}\n')
        self.output.write('\t\t\treturn (')
        self.visit(node.type)
        self.output.write(')this._state;\n')
        self.output.write('\t\t}\n')
        self.output.write('\t\t\n')
        self.output.write('\t\tset {\n')
        if primitive_type_name(node.type) is not None:
          self.output.write('\t\t\t_tag = Tag.%s;\n'% node.name)
        else:
          self.output.write('\t\t\t_tag = (value != null) ? Tag.%s : Tag.INVALID;\n' % node.name)
        self.output.write('\t\t\t_state = value;\n')
        self.output.write('\t\t}\n')
        self.output.write('\t}\n')

    def visit_BuiltinType(self, node):
        self.output.write(primitive_type_name(node))

    def visit_DefinedType(self, node):
        self.output.write(node.type.name)

    def visit_CompoundType(self, node):
        self.output.write(node.fully_qualified_name(CSharpQualifiedNamer))

    def visit_PascalStringType(self, node):
        self.output.write('string')

class EnumConceptEmitter(ast.NodeVisitor):
    def __init__(self, output=sys.stdout):
        self.output = output

    def visit_EnumConceptDecl(self, node):
        argument_name = emitterutil._lower_first_char_of_string(node.enum)

        return_type = cast_type(node.return_type.type)

        self.output.write('public partial class EnumConcept\n{\n')

        self.output.write('\tpublic static {enum_concept_return_type} {enum_concept_name}({enum_concept_type} {argument_name}, {enum_concept_return_type} defaultValue)\n\t{{\n'.format(
            enum_concept_return_type=return_type,
            enum_concept_name=node.name,
            enum_concept_type=node.enum,
            argument_name=argument_name))

        self.output.write('\t\tswitch({argument_name}) {{\n'.format(argument_name=argument_name))

        for member in node.members():
            member_value = member.value.value

            # If this is a string and it contains "::" meaning it is likely a verbatim value
            if type(member_value) is str and "::" in member_value:
                # Split the value on operators in order to cast all parts of the verbatim statement
                # C# does not like to implicitly convert between types so we have to cast to the enum concepts return type
                split = emitterutil._split_string_on_operators(member_value)
                for token in split:
                    cast_token = "({enum_concept_return_type})".format(enum_concept_return_type=return_type) + token;
                    member_value = member_value.replace(token, cast_token)

                # Finally replace all occurences of '::' with '.'
                member_value = member_value.replace("::", ".")

            # C# won't implicitly cast ints to bools so we need to replace the int with a bool should
            # the EnumConcept's return type be a bool
            if node.return_type.type.name == "bool":
                member_value = "true" if member_value == 1 else "false" 

            self.output.write('\t\t\tcase {enum_concept_type}.{member_name}: return {member_value};\n'.format(
                enum_concept_type=node.enum,
                member_name=member.name,
                member_value=member_value))

        # Default case of the swich throws an ArgumentException because we shouldn't be able to get here unless someone is
        # doing some odd casting
        self.output.write('\t\t\tdefault:\n')
        self.output.write('\t\t\t\treturn defaultValue;\n')

        self.output.write('\t\t}\n')

        self.output.write('\t}\n}\n\n')

if __name__ == '__main__':
    from clad import emitterutil
    emitterutil.c_main(language='C#', extension='.cs',
        emitter_types=[NamespaceEmitter],
        allow_custom_extension=False, allow_override_output=True,
        use_inclusion_guards=False)
