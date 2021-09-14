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

byte = 'uint8_t'
size_t = 'uint32_t'

_type_translations = {
    'bool': 'bool',
    'int_8': 'int8_t',
    'int_16': 'int16_t',
    'int_32': 'int32_t',
    'int_64': 'int64_t',
    'uint_8': 'uint8_t',
    'uint_16': 'uint16_t',
    'uint_32': 'uint32_t',
    'uint_64': 'uint64_t',
    'float_32': 'float',
    'float_64': 'double'
}

def _generic_type(type, member_name, value_format, reference_format, variable_array_format, fixed_array_format):
    if isinstance(type, ast.BuiltinType):
        if not type.name in _type_translations:
            raise ValueError('Error: {0} was expected to be a primitive type, but is not.'.format(type.name))
        return value_format.format(value_type=_type_translations[type.name], member_name=member_name)
    elif isinstance(type, ast.DefinedType):
        return value_format.format(value_type=type.fully_qualified_name(), member_name=member_name)
    elif isinstance(type, ast.PascalStringType):
        return variable_array_format.format(member_type="char", member_name=member_name);
    elif isinstance(type, ast.FixedArrayType):
        member_type = _generic_type(type.member_type, member_name, value_format, reference_format,
            variable_array_format, fixed_array_format)
        return fixed_array_format.format(value_type=member_type, length=type.length, member_name=member_name)
    elif isinstance(type, ast.VariableArrayType):
        member_type = _generic_type(type.member_type, member_name, value_format, reference_format,
            variable_array_format, fixed_array_format)
        return variable_array_format.format(value_type=member_type, member_name=member_name)
    else:
        return reference_format.format(value_type=type.fully_qualified_name(), member_name=member_name)

def cpplite_mutable_type(type):
    """A return value with any pointers mutable."""
    return _generic_type(type,
                         None,
                         '{value_type}',
                         '{value_type}',
                         '{value_type}*',
                         '{value_type}*')

def cpplite_readonly_type(type):
    """A return value with any pointers const."""
    return _generic_type(type,
                         None,
                         '{value_type}',
                         '{value_type}',
                         'const {value_type}*',
                         'const {value_type}*')

def cpplite_parameter_type(type):
    """A value you can pass as parameter."""
    return _generic_type(type,
                         None,
                         '{value_type}',
                         'const {value_type}&',
                         '{value_type}*',
                         'const {value_type}[{length}]&')

def length_member_name(member_name):
    """The corresponding length member for a variable-length array."""
    return '{member_name}_length'.format(member_name=member_name)

class ConstraintVisitor(ast.NodeVisitor):
    "Base class for emitters."

    def __init__(self, options=None):
        self.options = options
    
    # allow includes
    def visit_IncludeDecl(self, node, *args, **kwargs):
        self.generic_visit(node)
    
    def visit_EnumDecl(self, node):
        pass

    def visit_MessageDecl(self, node, *args, **kwargs):
        globals = dict(
            object_name=node.name,
            qualified_object_name=node.fully_qualified_name(),
            max_size=node.max_message_size(),
            min_size=node.min_message_size(),
            byte=byte,
            size_t=size_t,
        )
        
        self.checkMaximumSize(node, self.options, globals)
        self.checkSubUnions(node, globals)
        self.checkArrays(node, globals)
        self.checkFixedLength(node, globals)
        self.checkEmptyMessagesInMessages(node, globals)
        self.checkAlignment(node, globals)
    
    def visit_UnionDecl(self, node, *args, **kwargs):
        globals = dict(
            object_name=node.name,
            qualified_object_name=node.fully_qualified_name(),
            max_size=node.max_message_size(),
            min_size=node.min_message_size(),
            byte=byte,
            size_t=size_t,
        )
        
        self.checkMaximumSize(node, self.options, globals)
        self.checkSubUnions(node, globals)
        self.checkArrays(node, globals)
    
    @classmethod
    def fail_constraint(cls, node, globals, message):
        if isinstance(node, ast.MessageMemberDecl):
            object_name = '{qualified_object_name}::{member_name}'.format(member_name=node.name, **globals)
        else:
            object_name = '{qualified_object_name}'.format(**globals)
        emitterutil.exit_at_coord(node.coord, 'C++ Lite emitter constraint violated by {object_name}: {message}'.format(
            object_name=object_name, message=message))
    
    @classmethod
    def checkMaximumSize(cls, node, options, globals):
        """Ensures there are no messages or unions of unions."""
        if options.max_message_size is not None and node.max_message_size() > options.max_message_size:
            cls.fail_constraint(node, globals,
                '{qualified_object_name} is maximum {max_size} bytes, larger than the maximum message size of {enforced_max_size}.'.format(
                    enforced_max_size=options.max_message_size, **globals))
        
    @classmethod
    def checkSubUnions(cls, node, globals):
        """Ensures there are no messages or unions of unions."""
        for member in node.members():
            if isinstance(member.type, ast.CompoundType):
                if isinstance(member.type.type_decl, ast.UnionDecl):
                    cls.fail_constraint(member, globals, 'Unable to nest union types in C++ Lite emitter due to padding issues.')
    
    @classmethod
    def checkArrays(cls, node, globals):
        """Ensures there are no arrays of arrays or strings and that variable-length arrays have sane limits."""
        for member in node.members():
            if isinstance(member.type, ast.VariableArrayType) or isinstance(member.type, ast.FixedArrayType):
            
                member_type = member.type.member_type
                if isinstance(member_type, ast.VariableArrayType) or isinstance(member_type, ast.FixedArrayType):
                    cls.fail_constraint(member, globals, 'Unable to have arrays of arrays or strings due to padding issues.')
                
                if not member_type.is_message_size_fixed():
                    cls.fail_constraint(member, globals, 'Cannot have variable-length data inside an array.')
                
                if member_type.max_message_size() % member_type.alignment() != 0:
                    cls.fail_constraint(member, globals, ("Cannot have an array of a type that is not a multiple of its own alignment. " +
                        "A {member_type}'s size is {size}, which is not divisible by its alignment, {alignment}.").format(
                            member_type=member_type.fully_qualified_name(),
                            alignment=member_type.alignment(),
                            size=member_type.max_message_size(),
                            **globals))
                
            if isinstance(member.type, ast.VariableArrayType):
                if member.type.max_length > 255 and not member.type.max_length_is_specified:
                    cls.fail_constraint(member, globals, 'You must specify a maximum length for variable-length arrays that have a larger length type than 1 byte.')
                
                if isinstance(node, ast.UnionDecl) and member.type.length_type.max_message_size() % member.type.member_type.alignment() != 0:
                    cls.fail_constraint(member, globals, ('You cannot have variable-length arrays in unions that cannot be aligned properly. ' +
                        '{qualified_object_name}::{member_name} would be represented as {length_size}-byte length followed by a {alignment}-byte aligned {member_type} array.').format(
                            member_name=member.name,
                            length_size=member.type.length_type.max_message_size(),
                            alignment=member.type.member_type.alignment(),
                            member_type=member.type.member_type.name,
                            **globals))

    @classmethod
    def checkFixedLength(cls, node, globals):
        """Ensures that all members except for the last in a message have a specific, fixed length."""
        for member in node.members()[:-1]:
            if not member.type.is_message_size_fixed():
                cls.fail_constraint(member, globals, 'All message members, other than the last, must be fixed length.')
            
            if member.type.max_message_size() % member.type.alignment() != 0:
                cls.fail_constraint(member, globals, 'You may only put a message with trailing padding as the last member.')
    
    @classmethod
    def checkEmptyMessagesInMessages(cls, node, globals):
        """Ensures that there are no empty messages within messages, because it is disallowed in standard C and different compilers treat it different ways."""
        for member in node.members():
            if isinstance(member.type, ast.CompoundType):
                if isinstance(member.type.type_decl, ast.MessageDecl) and member.type.max_message_size() == 0:
                    cls.fail_constraint(member, globals, 'Unable to have 0-length structs as members of structs due to padding and portability issues.')
    
    @classmethod
    def checkAlignment(cls, node, globals):
        """Ensures that all members are properly aligned so that doubles only appear on 8-byte boundaries, etc."""
        current_offset = 0
        for member in node.members():
            if isinstance(member.type, ast.VariableArrayType):
                cls.testAlignment(member, 'Its length type', member.type.length_type, current_offset, globals)
                cls.testAlignment(member, 'Its member type', member.type.member_type, current_offset + member.type.length_type.max_message_size(), globals)
            else:
                cls.testAlignment(member, 'It', member.type, current_offset, globals)
            current_offset += member.type.max_message_size()
    
    @classmethod
    def testAlignment(cls, member, role, type, current_offset, globals):
        if current_offset % type.alignment() != 0:
            cls.fail_constraint(member, globals,
                'Cannot put a {type_name} at byte offset {current_offset}. ({role} has alignment {type_alignment} and may get padded.)'.format(
                    type_name=type.fully_qualified_name(),
                    current_offset=current_offset,
                    role=role,
                    type_alignment=type.alignment(),
                    **globals))

class BaseEmitter(ast.NodeVisitor):
    "Base class for emitters."
    
    def __init__(self, output=sys.stdout, options=None):
        self.output = output
        self.options = options
    
    # ignore includes unless explicitly allowed
    def visit_IncludeDecl(self, node, *args, **kwargs):
        pass
    
class HNamespaceEmitter(BaseEmitter):
    
    def visit_DeclList(self, node, *args, **kwargs):
        last_was_include = False
        for c_name, c in node.children():
            if last_was_include and not isinstance(c, ast.IncludeDecl):
                self.output.write('\n')
            self.visit(c, *args, **kwargs)
            last_was_include = isinstance(c, ast.IncludeDecl)
        if last_was_include:
            self.output.write('\n')
    
    def visit_IncludeDecl(self, node, *args, **kwargs):
        new_header_file_name = emitterutil.get_included_file(node.name, '.h')
        self.output.write('#include "{0}"\n'.format(new_header_file_name))

    def visit_NamespaceDecl(self, node, *args, **kwargs):
        self.output.write('namespace {namespace_name} {{\n\n'.format(namespace_name=node.name))
        self.generic_visit(node, *args, **kwargs)
        self.output.write('}} // namespace {namespace_name}\n\n'.format(namespace_name=node.name))
    
    def visit_EnumDecl(self, node, *args, **kwargs):
        HEnumEmitter(self.output, self.options).visit(node, *args, **kwargs)
    
    def visit_MessageDecl(self, node, *args, **kwargs):
        HStructEmitter(self.output, self.options).visit(node, *args, **kwargs)
    
    def visit_UnionDecl(self, node, *args, **kwargs):
        HUnionEmitter(self.output, self.options).visit(node, *args, **kwargs)

class CPPNamespaceEmitter(HNamespaceEmitter):
    
    def visit_IncludeDecl(self, node, *args, **kwargs):
        pass
    
    def visit_EnumDecl(self, node, *args, **kwargs):
        CPPEnumEmitter(self.output, self.options).visit(node, *args, **kwargs)
    
    def visit_MessageDecl(self, node, *args, **kwargs):
        CPPStructEmitter(self.output, self.options).visit(node, *args, **kwargs)
    
    def visit_UnionDecl(self, node, *args, **kwargs):
        CPPUnionEmitter(self.output, self.options).visit(node, *args, **kwargs)

class HEnumEmitter(BaseEmitter):
    
    def visit_EnumDecl(self, node):
        globals = dict(
            enum_name=node.name,
            enum_storage_type=cpplite_mutable_type(node.storage_type.builtin_type()),
            byte=byte,
            size_t=size_t,
        )
        
        self.emitHeader(node, globals)
        self.emitMembers(node, globals)
        self.emitFooter(node, globals)
        self.emitSuffix(node, globals)
        
    def emitHeader(self, node, globals):
        if (node.cpp_class):
            self.output.write(textwrap.dedent('''\
                // ENUM {enum_name}
                enum class {enum_name} : {enum_storage_type} {{
                ''').format(**globals));
        else:
            self.output.write(textwrap.dedent('''\
                // ENUM {enum_name}
                enum {enum_name} : {enum_storage_type} {{
                ''').format(**globals));


    def emitFooter(self, node, globals):
        self.output.write('};\n\n')
    
    def emitMembers(self, node, globals):
        with self.output.indent(1):
            if node.members():
                pieces = []
                for member in node.members():
                    start = '{member_name}'.format(member_name=member.name)
                    middle = ' = {initializer},'.format(initializer=str(member.value))
                    end = ''
            
                    pieces.append((start, middle, end))
                
                self.output.write_with_aligned_whitespace(pieces)

    def emitSuffix(self, node, globals):
        with self.output.reset_indent():
            self.output.write(textwrap.dedent('''\
              constexpr {enum_storage_type} EnumToUnderlyingType({enum_name} e)
              {{
                return static_cast<{enum_storage_type}>(e);
              }}
              
              ''').format(**globals));
        
        self.output.write('const char* EnumToString({enum_name} m);\n'.format(**globals))
        self.output.write('{enum_name} {enum_name}FromString(const std::string&);\n\n'.format(**globals))

class CPPEnumEmitter(HEnumEmitter):
  
    def visit_EnumDecl(self, node):
        globals = dict(
            enum_name=node.name,
            enum_storage_type=cpplite_mutable_type(node.storage_type.builtin_type()),
            byte=byte,
            size_t=size_t,
        )
      
        self.emitHeader(node, globals)
        self.emitMembers(node, globals)
        self.emitFooter(node, globals)
        self.emitSuffix(node, globals)
        self.emitStringToEnum(node, globals)

    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            const char* EnumToString({enum_name} m)
            {{
            \tswitch(m) {{
            ''').format(**globals))
    
    def emitFooter(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \t\tdefault:
            \t\t\treturn 0;
            \t}
            }
            
            '''))
    
    def emitMembers(self, node, globals):
        with self.output.indent(2):
            for member in node.members():
                if not member.is_duplicate:
                    self.output.write(textwrap.dedent('''\
                        case {enum_name}::{member_name}:
                        \treturn "{member_name}";
                        ''').format(member_name=member.name, **globals))
    
    def emitSuffix(self, node, globals):
        pass

    def emitStringToEnum(self, node, globals):
        self.output.write(textwrap.dedent('''\
          {enum_name} {enum_name}FromString(const std::string& str)
          {{
          ''').format(num_values=len(node.members()), **globals))
          
        with self.output.indent(1):
          self.output.write('static const std::unordered_map<std::string, {enum_name}> stringToEnumMap = {{\n'.format(**globals))
          for member in node.members():
              self.output.write('\t{{"{member_name}", {enum_name}::{member_name}}},\n'.format(member_name=member.name, **globals))
          self.output.write('};\n\n')
                
          self.output.write(textwrap.dedent('''\
              auto it = stringToEnumMap.find(str);
              if(it == stringToEnumMap.end()) {{
              #ifndef NDEBUG
              std::cerr << "error: string '" << str << "' is not a valid {enum_name} value" << std::endl;
              #endif // NDEBUG
              assert(false && "string must be a valid {enum_name} value");
              return {enum_name}::{first_val};
              }}
              
          ''').format(first_val=node.members()[0].name, **globals))
            
          self.output.write('return it->second;\n')
        
        self.output.write('}\n\n')


class HStructEmitter(BaseEmitter):
    
    body_indent = 1
    
    def visit_MessageDecl(self, node):
        globals = dict(
            object_name=node.name,
            object_type=node.object_type().upper(),
            qualified_object_name=node.fully_qualified_name(),
            max_size=node.max_message_size(),
            min_size=node.min_message_size(),
            byte=byte,
            size_t=size_t,
        )
        
        self.emitHeader(node, globals)
        with self.output.indent(self.body_indent):
            self.emitMembers(node, globals)
            self.emitCast(node, globals)
            self.emitIsValid(node, globals)
            self.emitSize(node, globals)
        self.emitFooter(node, globals)
    
    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            // {object_type} {object_name}
            struct {object_name}
            {{
            ''').format(**globals))
    
    def emitFooter(self, node, globals):
        self.output.write('};\n\n')

    def emitMembers(self, node, globals):
        if node.members():
            emitter = CPPLiteMemberDeclarationEmitter(self.output, self.options, group_compounds=False, do_initialize_values=True)
            for member in node.members():
                emitter.visit(member)
        else:
            self.output.write('// To conform to C99 standard (6.7.2.1)\n')
            self.output.write('char _empty;\n')
        
        self.output.write('\n')
    
    def emitCast(self, node, globals):
        # NOTE: There are no leading padding bytes for messages (at least for now). These are just for compatibility.
        self.output.write(textwrap.dedent('''\
            /**** Cast to/from buffer, adjusting any padding. ****/
            inline {byte}* GetBuffer() {{ return reinterpret_cast<{byte}*>(this); }}
            inline const {byte}* GetBuffer() const {{ return reinterpret_cast<const {byte}*>(this); }}
            
            ''').format(**globals))
    
    def emitIsValid(self, node, globals):
        self.output.write('/**** Check if current message is parsable. ****/\n')
        if node.are_all_representations_valid():
            self.output.write('bool IsValid() const {{ return true; }}\n'.format(**globals))
        else:
            self.output.write('bool IsValid() const;\n'.format(**globals))
        self.output.write('\n')
    
    def emitSize(self, node, globals):
        self.output.write('/**** Serialized size, starting from GetBuffer(). ****/\n')
        self.output.write('static const {size_t} MAX_SIZE = {max_size};\n'.format(**globals))
        self.output.write('static const {size_t} MIN_SIZE = {min_size};\n'.format(**globals))
        if node.is_message_size_fixed():
            self.output.write('inline {size_t} Size() const {{ return {max_size}; }}\n'.format(**globals))
        else:
            self.output.write('{size_t} Size() const;\n'.format(**globals))
        self.output.write('\n')

class CPPStructEmitter(HStructEmitter):
    
    body_indent = 0
    
    def emitHeader(self, node, globals):
        pass
        
    def emitFooter(self, node, globals):
        pass
    
    def emitMembers(self, node, globals):
        pass

    def emitCast(self, node, globals):
        pass
    
    def emitIsValid(self, node, globals):
        if not node.are_all_representations_valid():
            self.output.write(textwrap.dedent('''\
                bool {object_name}::IsValid() const
                {{
                    return (''').format(**globals))
            with self.output.indent(2):
                visitor = CPPLiteIsValidExpressionEmitter(self.output, self.options)
                glue = ''
                members = [member for member in node.members() if not member.type.are_all_representations_valid()]
                for i, member in enumerate(members):
                    visitor.visit(member)
                    if i != len(members) - 1:
                        self.output.write(' &&\n')
                    else:
                        self.output.write(');\n')
            self.output.write('}\n\n')
    
    def emitSize(self, node, globals):
        if not node.is_message_size_fixed():
            self.output.write(textwrap.dedent('''\
                {size_t} {object_name}::Size() const
                {{
                \t{size_t} result = 0;
                ''').format(**globals))
            with self.output.indent(1):
                visitor = CPPLiteSizeStatementEmitter(self.output, self.options)
                for member in node.members():
                    self.output.write('// {member_name}\n'.format(member_name=member.name))
                    visitor.visit(member)
            self.output.write(textwrap.dedent('''\
                \treturn result;
                }

                '''))
    
class HUnionEmitter(BaseEmitter):
    
    body_indent = 1
    
    def visit_UnionDecl(self, node):
        
        globals = dict(
            object_name=node.name,
            qualified_object_name=node.fully_qualified_name(),
            tag_name='Tag',
            qualified_tag_name='{object_name}::Tag'.format(object_name=node.fully_qualified_name()),
            tag_member_name='tag',
            object_type='UNION',
            tag_storage_type=cpplite_mutable_type(node.tag_storage_type),
            max_size=node.max_message_size(),
            min_size=node.min_message_size(),
            byte=byte,
            size_t=size_t,
        )
        
        self.emitHeader(node, globals)
        with self.output.indent(self.body_indent):
            self.emitTagType(node, globals)
            self.emitTagMember(node, globals)
            self.emitUnionMembers(node, globals)
            self.emitConstructors(node, globals)
            self.emitCast(node, globals)
            self.emitIsValid(node, globals)
            self.emitSize(node, globals)
            self.emitTagToString(node, globals)
        self.emitFooter(node, globals)
        
    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            // {object_type} {object_name}
            struct {object_name}
            {{
            ''').format(**globals))
        
    def emitFooter(self, node, globals):
        self.output.write('};\n\n')

    def emitTagType(self, node, globals):
        self.output.write(textwrap.dedent('''\
            enum {{
            ''').format(**globals));
        
        with self.output.indent(1):
            if node.members():
                pieces = []
                for member in node.members():
                    if member.init:
                        initializer = hex(member.tag) if member.init.type == "hex" else str(member.tag)
                        start = '{tag_name}_{member_name}'.format(member_name=member.name, **globals)
                        middle = ' = {initializer},'.format(initializer=initializer)
                    else:
                        start = '{tag_name}_{member_name},'.format(member_name=member.name, **globals)
                        middle = ''
                    end = ' // {value}'.format(value=member.tag)
            
                    pieces.append((start, middle, end))
                
                start = 'INVALID'
                middle = ' = {invalid_tag}'.format(invalid_tag=node.invalid_tag)
                pieces.append((start, middle))
                
                self.output.write_with_aligned_whitespace(pieces)        
        
        self.output.write('};\n')
        self.output.write('typedef {tag_storage_type} {tag_name};\n\n'.format(**globals))
    
    def emitTagMember(self, node, globals):
        padding = node.alignment() - node.tag_storage_type.alignment()
        if padding > 0:
            self.output.write('/**** Padding added to preserve alignment ****/\n')
            self.output.write('char _padding[{padding}];\n\n'.format(padding=padding))
        self.output.write('Tag tag;\n\n')

    def emitUnionMembers(self, node, globals):
        emitter = CPPLiteMemberDeclarationEmitter(self.output, self.options, group_compounds=True)
        self.output.write('union {\n')
        with self.output.indent(1):
            for member in node.members():
                emitter.visit(member)
        self.output.write('};\n\n')
    
    def emitConstructors(self, node, globals):
        self.output.write('{object_name}(): tag(INVALID) {{ }}\n\n'.format(**globals))
        
        if node.members():
          for member in node.members():
            self.output.write('{object_name}( {member_type} msg ): tag({tag_name}_{member_name}), {member_name}(msg) {{ }}\n'.format(member_type=cpplite_mutable_type(member.type), member_name=member.name, **globals))


    def emitCast(self, node, globals):
        self.output.write(textwrap.dedent('''\
            /**** Cast to byte buffer, adjusting any padding. ****/
            inline {byte}* GetBuffer() {{ return reinterpret_cast<{byte}*>(&this->{tag_member_name}); }}
            inline const {byte}* GetBuffer() const {{ return reinterpret_cast<const {byte}*>(&this->{tag_member_name}); }}
            
            ''').format(**globals))
    
    def emitIsValid(self, node, globals):
        self.output.write('/**** Check if current message is parsable. ****/\n')
        self.output.write('bool IsValid() const;\n\n'.format(**globals))
    
    def emitSize(self, node, globals):
        self.output.write('/**** Serialized size, starting from GetBuffer(). ****/\n')
        self.output.write('static const {size_t} MAX_SIZE = {max_size};\n'.format(**globals))
        self.output.write('static const {size_t} MIN_SIZE = {min_size};\n'.format(**globals))
        if node.is_message_size_fixed():
            self.output.write('inline {size_t} Size() const {{ return {max_size}; }}\n'.format(**globals))
        else:
            self.output.write('{size_t} Size() const;\n'.format(**globals))
        self.output.write('\n')
    
    def emitTagToString(self, node, globals):
        self.output.write('static const char* {tag_name}ToString({tag_name} t);\n'.format(**globals))

class CPPUnionEmitter(HUnionEmitter):
    
    body_indent = 0
    
    def emitHeader(self, node, globals):
        pass
        
    def emitFooter(self, node, globals):
        pass

    def emitTagType(self, node, globals):
        pass
    
    def emitTagMember(self, node, globals):
        pass

    def emitUnionMembers(self, node, globals):
        pass
    
    def emitConstructors(self, node, globals):
        pass
    
    def emitCast(self, node, globals):
        pass
    
    def emitIsValid(self, node, globals):
        emitter = CPPLiteIsValidExpressionEmitter(self.output, self.options)
        def body(member):
            self.output.write('return ')
            emitter.visit(member)
            self.output.write(';\n')
        
        self.output.write(textwrap.dedent('''\
            bool {qualified_object_name}::IsValid() const
            {{
            ''').format(**globals))
        with self.output.indent(2):
            self.emitSwitch(node, globals, body, default_case='return false;\n')
        self.output.write('}\n\n')
    
    def emitSize(self, node, globals):
        if not node.is_message_size_fixed():
            emitter = CPPLiteSizeStatementEmitter(self.output, self.options)
            def body(member):
                emitter.visit(member)
                self.output.write('break;\n')
        
            self.output.write(textwrap.dedent('''\
                {size_t} {qualified_object_name}::Size() const
                {{
                \t{size_t} result = 1;
                ''').format(**globals))
            with self.output.indent(2):
                self.emitSwitch(node, globals, body)
            self.output.write(textwrap.dedent('''\
                \treturn result;
                }
            
                '''))
    
    def emitTagToString(self, node, globals):
        def body(member):
            self.output.write('return "{member_name}";\n'.format(member_name=member.name))
        
        self.output.write(textwrap.dedent('''\
            const char* {qualified_object_name}::{tag_name}ToString({tag_name} t)
            {{
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, body, argument='t', default_case='return "INVALID";\n')
        self.output.write('}\n')
    
    def emitSwitch(self, node, globals, callback, tag_type='Tag', argument='tag', default_case='break;\n'):
        self.output.write('switch({argument}) {{\n'.format(argument=argument, **globals))
        
        for member in node.members():
            self.output.write('case {tag_type}_{member_name}:\n'.format(member_name=member.name, tag_type=tag_type))
            with self.output.indent(1):
                callback(member)
        
        self.output.write('default:\n')
        with self.output.indent(1):
            self.output.write(default_case)
        
        self.output.write('}\n')

class CPPLiteMemberDeclarationEmitter(BaseEmitter):
    
    def __init__(self, output, options, group_compounds, do_initialize_values = False):
        super(CPPLiteMemberDeclarationEmitter, self).__init__(output, options)
        self.group_compounds = group_compounds
        self.do_initialize_values = do_initialize_values
    
    def visit_MessageMemberDecl(self, node):
        self.visit(node.type, member_name=node.name)
        # check for initialization for member
        if (self.do_initialize_values and node.init):
            initial_value = node.init
            member_val = initial_value.value
            member_str = hex(member_val) if initial_value.type == "hex" else str(member_val)
            self.output.write(" = %s" % member_str)
        self.output.write(';\n')
    
    def emitSimple(self, node, member_name):
        self.output.write('{member_type} {member_name}'.format(
            member_type=cpplite_mutable_type(node),
            member_name=member_name))
    
    def visit_BuiltinType(self, *args, **kwargs):
        self.emitSimple(*args, **kwargs)
    
    def visit_DefinedType(self, *args, **kwargs):
        self.emitSimple(*args, **kwargs)
    
    def visit_CompoundType(self, *args, **kwargs):
        self.emitSimple(*args, **kwargs)
    
    def visit_FixedArrayType(self, node, member_name):
        self.visit(node.member_type, member_name=member_name)
        self.output.write('[{length}]'.format(length=node.length))
    
    def visit_VariableArrayType(self, node, member_name):
        if self.group_compounds:
            self.output.write('struct {\n')
        with self.output.indent(1 if self.group_compounds else 0):
            self.visit(node.length_type, member_name=length_member_name(member_name))
            self.output.write(';\n')
            self.visit(node.member_type, member_name=member_name)
            self.output.write('[{length}]'.format(length=node.max_length))
        if self.group_compounds:
            self.output.write(';\n')
            self.output.write('}')

    def visit_PascalStringType(self, node, member_name):
        self.visit(node.length_type, member_name=length_member_name(member_name))
        self.output.write(';\n')
        self.output.write('char {member_name}[{length}]'.format(member_name=member_name, length=node.max_length))

class CPPLiteIsValidExpressionEmitter(BaseEmitter):
    
    def visit_MessageMemberDecl(self, node):
        self.visit(node.type, member_name=node.name)
    
    def visit_BuiltinType(self, node, member_name):
        assert(node.are_all_representations_valid())
        self.output.write('true')
    
    def visit_DefinedType(self, *args, **kwargs):
        self.visit_BuiltinType(*args, **kwargs)
    
    def visit_CompoundType(self, node, member_name):
        self.output.write('this->{member_name}.IsValid()'.format(member_name=member_name))
    
    def visit_FixedArrayType(self, node, member_name):
        assert node.are_all_representations_valid()
        self.output.write('true')
    
    def visit_VariableArrayType(self, node, member_name):
        assert node.member_type.are_all_representations_valid()
        has_previous = False
        if node.length_type.min < 0:
            self.output.write('{length_member_name} >= 0'.format(length_member_name=length_member_name(member_name)))
            has_previous = True
        if node.length_type.max > node.max_length:
            if has_previous:
                self.output.write(' && ')
            self.output.write('{length_member_name} <= {max_length}'.format(
                length_member_name=length_member_name(member_name),
                max_length=node.max_length))
            has_previous = True
        return has_previous

    def visit_PascalStringType(self, node, member_name):
        has_previous = self.visit_VariableArrayType(node, member_name=member_name)
        if not has_previous:
            self.output.write('true')
        # could alternately provide better utf8 enforcement; right now it allows null bytes and invalid utf8
        #if has_previous:
        #    self.output.write(' && ')
        #self.output.write('is_utf8_valid(this->{member_name})'.format(member_name=member_name))

class CPPLiteSizeStatementEmitter(BaseEmitter):
    
    def visit_MessageMemberDecl(self, node):
        self.visit(node.type, member_name=node.name)
    
    def visit_BuiltinType(self, node, member_name):
        assert(node.is_message_size_fixed())
        self.output.write('result += {size}; // {type}\n'.format(
            size=node.max_message_size(), type=node.name))
    
    def visit_DefinedType(self, *args, **kwargs):
        self.visit_BuiltinType(*args, **kwargs)
    
    def visit_CompoundType(self, node, member_name):
        self.output.write('result += this->{member_name}.Size(); // {type}\n'.format(
            member_name=member_name, type=node.name))
    
    def visit_FixedArrayType(self, node, member_name):
        assert(node.member_type.is_message_size_fixed())
        self.output.write('result += {member_size} * {length}; // {member_type} * {length}\n'.format(
            member_size=node.member_type.max_message_size(),
            member_type=node.member_type.name,
            length=node.length))
    
    def visit_VariableArrayType(self, node, member_name):
        assert(node.member_type.is_message_size_fixed())
        self.output.write(textwrap.dedent('''\
            result += {length_size}; // {length_type} ({length_member_name})
            result += {member_size} * this->{length_member_name}; // {member_type}
            ''').format(
                length_size=node.length_type.max_message_size(),
                length_type=node.length_type.name,
                length_member_name=length_member_name(member_name),
                member_size=node.member_type.max_message_size(),
                member_type=node.member_type.name,
                member_name=member_name))

    def visit_PascalStringType(self, node, member_name):
        self.visit_VariableArrayType(node, member_name=member_name)

if __name__ == '__main__':
    from clad import emitterutil
    
    language = 'C++ Lite (embedded)'
    header_extension = '.h'
    source_extension = '.cpp'
    
    option_parser = emitterutil.StandardArgumentParser(language)
    option_parser.add_argument('-r', '--header-output-directory', metavar='dir',
        help='The directory to output the {language} header file(s) to.'.format(language=language))
    option_parser.add_argument('--max-message-size', metavar='bytes', type=int,
        help='Maximum serialized size that any single union or message can be.')
    
    options = option_parser.parse_args()
    if not options.header_output_directory:
        options.header_output_directory = options.output_directory
    
    tree = emitterutil.parse(options)

    comment_lines = emitterutil.get_comment_lines(options, language)
    
    ConstraintVisitor(options=options).visit(tree)
    
    namespace_emitter = HNamespaceEmitter(options=options)
    def main_output_header_callback(output):
        namespace_emitter.output = output
        namespace_emitter.visit(tree)
    
    main_output_header = emitterutil.get_output_file(options, header_extension)
    emitterutil.write_c_file(options.header_output_directory, main_output_header,
        main_output_header_callback,
        comment_lines=comment_lines,
        use_inclusion_guards=True,
        system_headers=['stdbool.h', 'stdint.h', 'string'])
    
    
    def main_output_source_callback(output):
        CPPNamespaceEmitter(output, options=options).visit(tree)
    
    main_output_source = emitterutil.get_output_file(options, source_extension)
    emitterutil.write_c_file(options.output_directory, main_output_source,
        main_output_source_callback,
        comment_lines=comment_lines,
        use_inclusion_guards=False,
        local_headers=[main_output_header],
        system_headers=['unordered_map', 'iostream', 'cassert'])
