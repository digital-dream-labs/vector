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

def cpp_value_type(type):
    if isinstance(type, ast.BuiltinType):
        if not type.name in _type_translations:
            raise ValueError('Error: {0} was expected to be a primitive type, but is not.'.format(type.name))
        return _type_translations[type.name]
    elif isinstance(type, ast.PascalStringType):
        return "std::string";
    elif isinstance(type, ast.VariableArrayType) or isinstance(type, ast.FixedArrayType):
        if (not isinstance(type.member_type, ast.PascalStringType) and
                (isinstance(type.member_type, ast.VariableArrayType) or
                isinstance(type.member_type, ast.FixedArrayType))):
            raise ValueError('Error: Arrays of arrays are not supported.')
        elif isinstance(type, ast.VariableArrayType):
            return 'std::vector<{member_type}>'.format(
                member_type=cpp_value_type(type.member_type))
        else:
            if isinstance(type.length, str):
                 return 'std::array<{member_type}, static_cast<size_t>({length})>'.format(
                    member_type=cpp_value_type(type.member_type),
                    length=type.length)
            else:
                return 'std::array<{member_type}, {length}>'.format(
                    member_type=cpp_value_type(type.member_type),
                    length=type.length)
    else:
        return type.fully_qualified_name()

def cpp_value_type_destructor(type):
    if isinstance(type, ast.BuiltinType):
        if not type.name in _type_translations:
            raise ValueError('Error: {0} was expected to be a primitive type, but is not.'.format(type.name))
        return _type_translations[type.name]
    elif isinstance(type, ast.PascalStringType):
        return "basic_string";
    elif isinstance(type, ast.VariableArrayType) or isinstance(type, ast.FixedArrayType):
        if (not isinstance(type.member_type, ast.PascalStringType) and
                (isinstance(type.member_type, ast.VariableArrayType) or
                isinstance(type.member_type, ast.FixedArrayType))):
            raise ValueError('Error: Arrays of arrays are not supported.')
        elif isinstance(type, ast.VariableArrayType):
            return 'vector<{member_type}>'.format(
                member_type=cpp_value_type(type.member_type))
        else:
            return 'array<{member_type}, {length}>'.format(
                member_type=cpp_value_type(type.member_type),
                length=type.length)
    else:
        return type.name

def is_trivial_type(type):
    return isinstance(type, ast.BuiltinType) or isinstance(type, ast.DefinedType)

def is_pass_by_value(type):
    return is_trivial_type(type)

def cpp_parameter_type(type):
    if is_pass_by_value(type):
        return cpp_value_type(type)
    else:
        return 'const {value_type}&'.format(value_type=cpp_value_type(type))

_jsoncpp_as_method_translations = {
    'bool': 'asBool()',
    'int_8': 'asInt()',
    'int_16': 'asInt()',
    'int_32': 'asInt()',
    'int_64': 'asInt64()',
    'uint_8': 'asUInt()',
    'uint_16': 'asUInt()',
    'uint_32': 'asUInt()',
    'uint_64': 'asUInt64()',
    'float_32': 'asFloat()',
    'float_64': 'asDouble()'
}

def jsoncpp_as_method(type):
    if isinstance(type, ast.BuiltinType):
        if not type.name in _jsoncpp_as_method_translations:
            raise ValueError('Error: {0} was expected to be a primitive type, but is not.'.format(type.name))
        return _jsoncpp_as_method_translations[type.name]
    elif isinstance(type, ast.PascalStringType):
        return 'asString()'
    else:
        return None

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
        new_header_file_name = emitterutil.get_included_file(node.name, self.options.header_output_extension)
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

    def visit_EnumConceptDecl(self, node, *args, **kargs):
        HEnumConceptEmitter(self.output, self.options).visit(node, *args, **kargs)

class CPPNamespaceEmitter(HNamespaceEmitter):

    def visit_IncludeDecl(self, node, *args, **kwargs):
        pass

    def visit_EnumDecl(self, node, *args, **kwargs):
        CPPEnumEmitter(self.output, self.options).visit(node, *args, **kwargs)

    def visit_MessageDecl(self, node, *args, **kwargs):
        CPPStructEmitter(self.output, self.options).visit(node, *args, **kwargs)

    def visit_UnionDecl(self, node, *args, **kwargs):
        CPPUnionEmitter(self.output, self.options).visit(node, *args, **kwargs)

    def visit_EnumConceptDecl(self, node, *args, **kargs):
        CPPEnumConceptEmitter(self.output, self.options).visit(node, *args, **kargs)

class HEnumEmitter(BaseEmitter):

    def visit_EnumDecl(self, node):
        globals = dict(
            enum_name=node.name,
            enum_hash=node.hash_str,
            enum_storage_type=cpp_value_type(node.storage_type.builtin_type()),
            enum_member_count=0
        )

        self.emitPrefix(node, globals)
        self.emitHeader(node, globals)
        self.emitMembers(node, globals)
        self.emitFooter(node, globals)
        self.emitSuffix(node, globals)

    def emitPrefix(self, node, globals):
        if self.options.emitJSON:
            # need optional macro for "warn unused"
            self.output.write(textwrap.dedent('''\
            #ifndef CLAD_CPP_WARN_UNUSED_RESULT
            #define CLAD_CPP_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
            #endif

            '''))
        
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
        globals['enum_member_count'] = 0
        with self.output.indent(1):
            if node.members():
                pieces = []
                for member in node.members():
                    start = '{member_name}'.format(member_name=member.name)
                    middle = ' = {initializer},'.format(initializer=str(member.value))
                    end = ''
                    globals['enum_member_count'] += 1
            
                    pieces.append((start, middle, end))

                self.output.write_with_aligned_whitespace(pieces)

    def emitSuffix(self, node, globals):
        self.output.write('const char* EnumToString(const {enum_name} m);\n'.format(**globals))
        self.output.write('inline const char* {enum_name}ToString(const {enum_name} m) {{ return EnumToString(m); }}\n\n'.format(**globals))
        if self.options.emitJSON:
            self.output.write('template<typename T>\nCLAD_CPP_WARN_UNUSED_RESULT bool EnumFromString(const std::string& str, T& enumOutput);\n')
            self.output.write('CLAD_CPP_WARN_UNUSED_RESULT bool {enum_name}FromString(const std::string& str, {enum_name}& enumOutput);\n\n'.format(**globals))
            self.output.write('{enum_name} {enum_name}FromString(const std::string&);\n\n'.format(**globals))
        else:
            self.output.write('\n')
        self.output.write('extern const char* {enum_name}VersionHashStr;\n'.format(**globals))
        self.output.write('extern const uint8_t {enum_name}VersionHash[16];\n\n'.format(**globals))
        self.output.write('constexpr {enum_storage_type} {enum_name}NumEntries = {enum_member_count};\n\n'.format(**globals))

class CPPEnumEmitter(HEnumEmitter):

    def visit_EnumDecl(self, node):
        globals = dict(
            enum_name=node.name,
            enum_hash=node.hash_str,
            enum_storage_type=cpp_value_type(node.storage_type.builtin_type()),
            enum_member_count=0
        )

        self.emitToStringHeader(node, globals)
        self.emitToStringMembers(node, globals)
        self.emitToStringFooter(node, globals)
        if self.options.emitJSON:
            self.emitStringToEnum(node, globals)
        self.emitSuffix(node, globals)

    def emitToStringHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            const char* EnumToString(const {enum_name} m)
            {{
            \tswitch(m) {{
            ''').format(**globals))

    def emitToStringFooter(self, node, globals):
        self.output.write(textwrap.dedent('''\
            \t\tdefault: return nullptr;
            \t}
            \treturn nullptr;
            }

            '''))

    def emitToStringMembers(self, node, globals):
        globals['enum_member_count'] = 0
        with self.output.indent(2):
            for member in node.members():
                globals['enum_member_count'] += 1
                if not member.is_duplicate:
                    self.output.write(textwrap.dedent('''\
                        case {enum_name}::{member_name}:
                        \treturn "{member_name}";
                        ''').format(member_name=member.name, **globals))

    def emitStringToEnum(self, node, globals):
        self.output.write(textwrap.dedent('''\
            template<>
            bool EnumFromString(const std::string& str, {enum_name}& enumOutput)
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
                  return false;
                }}

                enumOutput = it->second;
                return true;
            ''').format(**globals))

        self.output.write('}\n\n')

        self.output.write(textwrap.dedent('''\
        bool {enum_name}FromString(const std::string& str, {enum_name}& enumOutput)
        {{
          return EnumFromString(str, enumOutput);
        }}

        ''').format(**globals))

        self.output.write(textwrap.dedent('''\
        {enum_name} {enum_name}FromString(const std::string& str)
        {{
        ''').format(**globals))
        
        with self.output.indent(1):
            self.output.write(textwrap.dedent('''\
            {enum_name} returnVal;
            if( !EnumFromString(str, returnVal) ) {{
              #ifndef NDEBUG
              std::cerr << "error: string '" << str << "' is not a valid {enum_name} value" << std::endl;
              #endif // NDEBUG
              assert(false && "string must be a valid {enum_name} value");
              return {enum_name}::{first_val};
            }}
            else {{
              return returnVal;
            }}
            ''').format(first_val=node.members()[0].name, **globals))

        self.output.write('}\n\n')


    def emitSuffix(self, node, globals):
        self.output.write('const char* {enum_name}VersionHashStr = "{enum_hash}";\n\n'.format(**globals))
        self.output.write('const uint8_t {enum_name}VersionHash[16] = {{ \n'.format(**globals))
        hex_data = emitterutil.decode_hex_string(globals['enum_hash'])
        with self.output.indent(2):
            for i, byte in enumerate(hex_data):
                separator = ','
                if i == (len(hex_data) - 1):
                    separator = ''
                self.output.write("{hex_byte}{separator} ".format(hex_byte=hex(byte),
                                                                  separator=separator))
        self.output.write("\n};\n\n")

class HStructEmitter(BaseEmitter):

    body_indent = 1

    def visit_MessageDecl(self, node):

        globals = dict(
            message_name=node.name,
            message_hash=node.hash_str,
            object_type=node.object_type().upper(),
        )

        self.emitHeader(node, globals)
        with self.output.indent(self.body_indent):
            self.emitMembers(node, globals)
            self.emitConstructors(node, globals)
            self.emitPack(node, globals)
            self.emitUnpack(node, globals)
            self.emitSize(node, globals)
            self.emitEquality(node, globals)
            self.emitInvoke(node, globals)
            if node.object_type() == 'structure' and self.options.emitProperties:
                self.emitProperties(node, globals)

            if node.object_type() == 'structure':
                # emit if options are set and we have default constructors
                if ( self.options.emitJSON and
                     node.default_constructor and
                     emitterutil._do_all_members_have_default_constructor(node) ):
                    self.emitJSONSerializer(node, globals)
                
        self.emitFooter(node, globals)

    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            // {object_type} {message_name}
            struct {message_name}
            {{
            ''').format(**globals))

    def emitFooter(self, node, globals):
        self.output.write('};\n\n')
        self.output.write('extern const char* {message_name}VersionHashStr;\n'.format(**globals))
        self.output.write('extern const uint8_t {message_name}VersionHash[16];\n\n'.format(**globals))

    def emitMembers(self, node, globals):
        for member in node.members():
            self.output.write('{member_type} {member_name}'.format(
                member_type=cpp_value_type(member.type),
                member_name=member.name))
            if member.init:
                initial_value = member.init
                member_val = initial_value.value
                member_str = hex(member_val) if initial_value.type == "hex" else str(member_val)
                self.output.write(" = %s" % member_str)
            self.output.write(';\n')

        self.output.write('\n')

    def emitInvoke(self, node, globals):
        paramsStr = ', '.join([m.name for m in node.members()])
        self.output.write(textwrap.dedent('''
            template <typename Callable>
            void Invoke(Callable&& func) const {{
               func({0});
            }}
            '''.format(paramsStr)))

    def emitConstructors(self, node, globals):
        self.output.write(textwrap.dedent('''\
            /**** Constructors ****/
            '''))
        
        all_members_have_default_constructor = emitterutil._do_all_members_have_default_constructor(node)
        
        if node.default_constructor and all_members_have_default_constructor:
            self.output.write(textwrap.dedent('''\
                {message_name}() = default;
                '''.format(**globals)))
                
        self.output.write(textwrap.dedent('''\
            {message_name}(const {message_name}& other) = default;
            {message_name}({message_name}& other) = default;
            {message_name}({message_name}&& other) noexcept = default;
            {message_name}& operator=(const {message_name}& other) = default;
            {message_name}& operator=({message_name}&& other) = default;

            '''.format(**globals)))

        if node.members():
            self.output.write('explicit {message_name}('.format(**globals))

            # parameters
            with self.output.indent(1):
                for i, member in enumerate(node.members()):
                    self.output.write('{member_type} {member_name}'.format(
                        member_type=cpp_parameter_type(member.type),
                        member_name=member.name))
                    if i < len(node.members()) - 1:
                        self.output.write(',\n')
                    else:
                        self.output.write(')\n')

            # initializers
            for i, member in enumerate(node.members()):
                if i == 0:
                    self.output.write(':')
                else:
                    self.output.write(',')
                self.output.write(' {member_name}({member_name})'.format(member_name=member.name))
                self.output.write('\n')
            self.output.write('{}\n\n')

        self.output.write(textwrap.dedent('''\
            explicit {message_name}(const uint8_t* buff, size_t len);
            explicit {message_name}(const CLAD::SafeMessageBuffer& buffer);

            '''.format(**globals)))

    def emitPack(self, node, globals):
        self.output.write(textwrap.dedent('''\
            /**** Pack ****/
            size_t Pack(uint8_t* buff, size_t len) const;
            size_t Pack(CLAD::SafeMessageBuffer& buffer) const;

            '''))

    def emitUnpack(self, node, globals):
        self.output.write(textwrap.dedent('''\
            /**** Unpack ****/
            size_t Unpack(const uint8_t* buff, const size_t len);
            size_t Unpack(const CLAD::SafeMessageBuffer& buffer);

            '''))

    def emitSize(self, node, globals):
        self.output.write('size_t Size() const;\n\n')

    def emitEquality(self, node, globals):
        self.output.write(textwrap.dedent('''\
            bool operator==(const {message_name}& other) const;
            bool operator!=(const {message_name}& other) const;
            ''').format(**globals))

    def emitProperties(self, node, globals):
        def prettifyName(str):
            str = str.lstrip('_')
            for i,c in enumerate(str):
                if c.isalpha():
                    return str[:i] + str[i].capitalize() + str[i+1:]
            return str

        self.output.write(textwrap.dedent('''
        /**** Properties ****/
        '''))
        for member in node.members():
            is_basic_type = isinstance(member.type, ast.BuiltinType)
            member_type = cpp_value_type(member.type) if is_basic_type else 'const ' + cpp_value_type(member.type) + '&'

            self.output.write(textwrap.dedent('''\
                {member_type} Get{pretty_name}() const {{ return {member_name}; }};
                void Set{pretty_name}({member_type} __value) {{ {member_name} = __value; }};

                '''.format(
                    member_type=member_type,
                    member_name=member.name,
                    pretty_name=prettifyName(member.name))))

    def emitJSONSerializer(self, node, globals):
        self.output.write(textwrap.dedent('''\
            /**** JSON ****/
            Json::Value GetJSON() const;
            bool SetFromJSON(const Json::Value& json);

            '''))



class CPPStructEmitter(HStructEmitter):

    body_indent = 0

    def emitHeader(self, node, globals):
        self.output.write("// {object_type} {message_name}\n\n".format(**globals))

    def emitFooter(self, node, globals):
        self.output.write('\n')
        self.output.write('const char* {message_name}VersionHashStr = "{message_hash}";\n\n'.format(**globals))
        self.output.write('const uint8_t {message_name}VersionHash[16] = {{ \n'.format(**globals))
        hex_data = emitterutil.decode_hex_string(globals['message_hash'])
        with self.output.indent(2):
            for i, byte in enumerate(hex_data):
                separator = ','
                if i == (len(hex_data) - 1):
                    separator = ''
                self.output.write("{hex_byte}{separator} ".format(hex_byte=hex(byte),
                                                                  separator=separator))
        self.output.write("\n};\n\n")


    def emitMembers(self, node, globals):
        pass
    
    # recursively goes through all members of the node and generates code to explicitly initialize
    # each member
    def __init_explicit_members(self, node):
        arg_list = '('
        for member in node.members():
            member_dict = member.__dict__['type'].__dict__
            if 'type_decl' in member_dict:
                arg_list += ' ' + member_dict['type_decl'].name + self.__init_explicit_members(member_dict['type_decl']) + ','
            else:
                if isinstance(member.type, ast.PascalStringType):
                    arg_list += ' "",'
                elif isinstance(member.type, ast.VariableArrayType) or isinstance(member.type, ast.FixedArrayType):
                    arg_list += ' {},'
                else:
                    arg_list += ' 0,'
        arg_list = arg_list[:-1]
        arg_list += ')'
        return arg_list
    
    def emitConstructors(self, node, globals):
        all_members_have_default_constructor = emitterutil._do_all_members_have_default_constructor(node)
      
        explicit_members = ''
        
        if not all_members_have_default_constructor:
            # at least one of the members does not have a default constructor so go through all of them
            # and genertate code to explicitly initialize each member
            for member in node.members():
                member_dict = member.__dict__['type'].__dict__
                if 'type_decl' in member_dict:
                    arg_list = self.__init_explicit_members(member_dict['type_decl'])
                    if explicit_members is '':
                        explicit_members += ': {member_name}{arg_list}'.format(member_name = member.name,
                                                                               arg_list = arg_list)
                    else:
                        explicit_members += ', {member_name}{arg_list}'.format(member_name = member.name,
                                                                           arg_list = arg_list)
    
    
        self.output.write(textwrap.dedent('''\
            {message_name}::{message_name}(const CLAD::SafeMessageBuffer& buffer)
            {explicit_constructors}
            {{
            \tUnpack(buffer);
            }}
          
            {message_name}::{message_name}(const uint8_t* buff, size_t len)
            : {message_name}::{message_name}({{const_cast<uint8_t*>(buff), len, false}})
            {{
            }}
            
            '''.format(explicit_constructors = explicit_members, **globals)))

    def emitInvoke(self, node, globals):
        pass

    def emitPack(self, node, globals):
        self.output.write(textwrap.dedent('''\
            size_t {message_name}::Pack(uint8_t* buff, size_t len) const
            {{
            \tCLAD::SafeMessageBuffer buffer(buff, len, false);
            \treturn Pack(buffer);
            }}

            size_t {message_name}::Pack(CLAD::SafeMessageBuffer& buffer) const
            {{
            '''.format(**globals)))
        with self.output.indent(1):
            visitor = CPPPackStatementEmitter(self.output, self.options)
            for member in node.members():
                visitor.visit(member.type, member_name=member.name)
        self.output.write(textwrap.dedent('''\
            \tconst size_t bytesWritten {buffer.GetBytesWritten()};
            \treturn bytesWritten;
            }

            '''))

    def emitUnpack(self, node, globals):
        self.output.write(textwrap.dedent('''\
            size_t {message_name}::Unpack(const uint8_t* buff, const size_t len)
            {{
            \tconst CLAD::SafeMessageBuffer buffer(const_cast<uint8_t*>(buff), len, false);
            \treturn Unpack(buffer);
            }}

            size_t {message_name}::Unpack(const CLAD::SafeMessageBuffer& buffer)
            {{
            ''').format(**globals));
        with self.output.indent(1):
            visitor = CPPUnpackStatementEmitter(self.output, self.options)
            for member in node.members():
                visitor.visit(member.type, member_name=member.name)
        self.output.write(textwrap.dedent('''\
            \treturn buffer.GetBytesRead();
            }

            '''))

    def emitSize(self, node, globals):
        self.output.write(textwrap.dedent('''\
            size_t {message_name}::Size() const
            {{
            \tsize_t result = 0;
            ''').format(**globals))
        with self.output.indent(1):
            visitor = CPPSizeStatementEmitter(self.output, self.options)
            for member in node.members():
                self.output.write('// {member_name}\n'.format(member_name=member.name))
                visitor.visit(member.type, member_name=member.name)
        self.output.write(textwrap.dedent('''\
            \treturn result;
            }

            '''))

    def emitEquality(self, node, globals):
        self.output.write(textwrap.dedent('''\
            bool {message_name}::operator==(const {message_name}& other) const
            {{
            '''.format(**globals)))
        if node.members():
            with self.output.indent(1):
                self.output.write('return (')
                with self.output.indent(1):
                    for i, member in enumerate(node.members()):
                        if i > 0:
                            self.output.write(' &&\n')
                        self.output.write('this->{member_name} == other.{member_name}'.format(member_name=member.name))
                self.output.write(');\n')
        else:
            self.output.write('return true;\n')
        self.output.write(textwrap.dedent('''\
            }}

            bool {message_name}::operator!=(const {message_name}& other) const
            {{
            \treturn !(operator==(other));
            }}

            '''.format(**globals)))

    def emitProperties(self, node, globals):
        pass

    def emitJSONSerializer(self, node, globals):
        self.output.write(textwrap.dedent('''\
            Json::Value {message_name}::GetJSON() const
            ''').format(**globals))

        self.output.write(textwrap.dedent('''\
        {
        \tJson::Value root;

        '''))

        def toJsonValue(member_name, member_type):
            if isinstance(member_type, ast.DefinedType):
                return 'EnumToString({member_name})'.format(member_name=member_name)
            elif isinstance(member_type, ast.CompoundType):
                return '{member_name}.GetJSON()'.format(member_name=member_name)
            else:
                return member_name

        for member in node.members():
            if isinstance(member.type, (ast.VariableArrayType, ast.FixedArrayType)) and not isinstance(member.type, ast.PascalStringType):
                with self.output.indent(1):

                    # vector<bool> is not a container in C++, so we need some special logic because we can't use
                    # ranged-for with references
                    if isinstance(member.type.member_type, ast.BuiltinType) and member.type.member_type.name == 'bool':
                        range_type = 'auto'
                        cast_prefix = '(bool)'
                    else:
                        range_type = 'const auto&'
                        cast_prefix = ''

                    self.output.write(textwrap.dedent('''\
                        for({range_type} value : {member_name}) {{
                          root["{json_name}"].append({cast_prefix}{json_value});
                        }}
                        ''').format(json_name=member.name.lstrip('_'),
                                    member_name=member.name,
                                    json_value=toJsonValue("value", member.type.member_type),
                                    range_type=range_type,
                                    cast_prefix=cast_prefix))
            else:
                self.output.write('\troot["{json_name}"] = {json_value};\n'.format(json_name=member.name.lstrip('_'), member_name=member.name, json_value=toJsonValue(member.name, member.type)))

        self.output.write(textwrap.dedent('''
        \treturn root;
        }
        '''))

        self.output.write(textwrap.dedent('''
            bool {message_name}::SetFromJSON(const Json::Value& root)
            {{
              try {{

            ''').format(**globals))

        with self.output.indent(1):

            for member in node.members():
                json_name = member.name.lstrip('_')

                self.output.write('\tif (root.isMember("{json_name}")) {{\n'.format(json_name=json_name))
                with self.output.indent(1):
                    if jsoncpp_as_method(member.type) is not None:
                        self.output.write('\t{member_name} = root["{json_name}"].{as_method};\n'.format(json_name=json_name, member_name=member.name, as_method=jsoncpp_as_method(member.type)))
                    elif isinstance(member.type, (ast.VariableArrayType, ast.FixedArrayType)):
                        self.output.write('\tauto& json_array = root["{json_name}"];\n'.format(json_name=json_name))

                        if isinstance(member.type, ast.VariableArrayType):
                            self.output.write('\t{member_name}.resize(json_array.size());\n'.format(member_name=member.name))

                        self.output.write('\tfor(Json::ArrayIndex i = 0; i < json_array.size(); i++) {\n')

                        with self.output.indent(1):
                            if jsoncpp_as_method(member.type.member_type) is not None:
                                self.output.write('\t{member_name}[i] = json_array[i].{as_method};\n'.format(member_name=member.name, as_method=jsoncpp_as_method(member.type.member_type)))
                            elif isinstance(member.type.member_type, ast.DefinedType):
                                with self.output.indent(1):
                                    self.output.write(textwrap.dedent('''\
                                    if (!{enum_name}FromString(json_array[i].asString(), {member_name}[i])) {{
                                      return false;
                                    }}
                                    ''').format(enum_name=member.type.member_type.name, member_name=member.name))
                            else:
                                with self.output.indent(1):
                                    self.output.write(textwrap.dedent('''\
                                    if (!{member_name}[i].SetFromJSON(json_array[i])) {{
                                      return false;
                                    }}
                                    ''').format(member_name=member.name))

                        self.output.write('\t}\n')
                    elif isinstance(member.type, ast.DefinedType):
                        with self.output.indent(1):
                            self.output.write(textwrap.dedent('''\
                            if (!{enum_name}FromString(root["{json_name}"].asString(), {member_name})) {{
                              return false;
                            }}
                            ''').format(enum_name=member.type.name, json_name=json_name, member_name=member.name))
                    else:
                        with self.output.indent(1):
                            self.output.write(textwrap.dedent('''\
                            if (!{member_name}.SetFromJSON(root["{json_name}"])) {{
                              return false;
                            }}
                            ''').format(json_name=json_name, member_name=member.name))
                self.output.write('\t}\n')                

        self.output.write(textwrap.dedent('''
              }}
              catch(Json::LogicError) {{
                return false;
              }}

              return true;
            }}
            ''').format(**globals))

class UnionDiscoverer(ast.NodeVisitor):

    found = False

    def visit_IncludeDecl(self, node):
        pass

    def visit_UnionDecl(self, node):
        self.found = True

class HUnionEmitter(BaseEmitter):

    def visit_UnionDecl(self, node):

        globals = dict(
            union_name=node.name,
            union_hash=node.hash_str,
            tag_name='{union_name}Tag'.format(union_name=node.name),
            qualified_union_name=node.fully_qualified_name(),
            qualified_tag_name='{union_name}Tag'.format(union_name=node.fully_qualified_name()),
            object_type='UNION',
            dupes_allowed=node.dupes_allowed)

        # Templated TagToType lookup structs
        self.output.write(textwrap.dedent('''\
            // "Lookup Tables" for getting type by tag using template specializations
            template<{union_name}Tag tag>
            struct {union_name}_TagToType;

            ''').format(**globals))

        for member in node.members():
            locals = dict(
                member_name=member.name,
                value_type=cpp_value_type(member.type),
                **globals)

            self.output.write(textwrap.dedent('''\
                template<>
                struct {union_name}_TagToType<{union_name}Tag::{member_name}> {{
                \tusing type = {value_type};
                }};
                ''').format(**locals))

        self.emitHeader(node, globals)

        # public stuff
        self.output.write('public:\n')
        with self.output.indent(1):
            self.emitUsingTag(node, globals)
            self.emitConstructors(node, globals)
            self.emitDestructor(node, globals)
            self.emitGetTag(node, globals)
            self.emitTemplatedGet(node, globals)
            self.emitTemplatedCreate(node, globals)
            self.emitAccessors(node, globals)
            self.emitUnpack(node, globals)
            self.emitPack(node, globals)
            self.emitSize(node, globals)
            self.emitEquality(node, globals)

            # emit if option is set and default constrcutors exist for all members of the union (the union
            # itself always has a default constructor)
            if self.options.emitJSON and emitterutil._do_all_members_have_default_constructor(node):
                self.emitJSONSerializer(node, globals)

        # private stuff
        self.output.write('private:\n')
        with self.output.indent(1):
            self.emitClearCurrent(node, globals)
            self.emitTagMember(node, globals)
            self.emitUnionMembers(node, globals)

        self.emitFooter(node, globals)

    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\

            // {object_type} {union_name}
            class {union_name}
            {{
            ''').format(**globals))

    def emitFooter(self, node, globals):
        self.output.write('};\n')
        self.output.write('extern const char* {union_name}VersionHashStr;\n'.format(**globals))
        self.output.write('extern const uint8_t {union_name}VersionHash[16];\n\n'.format(**globals))
                
    def emitUsingTag(self, node, globals):
        self.output.write('using Tag = {tag_name};\n'.format(**globals))

    def emitConstructors(self, node, globals):
        self.output.write(textwrap.dedent('''\
            /**** Constructors ****/
            {union_name}() :_tag(Tag::INVALID) {{ }}
            explicit {union_name}(const CLAD::SafeMessageBuffer& buff);
            explicit {union_name}(const uint8_t* buffer, size_t length);
            {union_name}(const {union_name}& other);
            {union_name}({union_name}&& other) noexcept;
            ''').format(**globals))

        self.output.write(textwrap.dedent('''\
            {union_name}& operator=(const {union_name}& other);
            {union_name}& operator=({union_name}&& other) noexcept;

            ''').format(**globals))

    def emitDestructor(self, node, globals):
        self.output.write('~{union_name}() {{ ClearCurrent(); }}\n'.format(**globals))

    def emitGetTag(self, node, globals):
        self.output.write('Tag GetTag() const { return _tag; }\n\n')

    def emitTemplatedGet(self, node, globals):
        self.output.write('// Templated getter for union members by type\n')
        self.output.write('// NOTE: Always returns a reference, even for trivial types, unlike untemplated version\n')
        self.output.write('template<Tag tag>\n')
        self.output.write('const typename {union_name}_TagToType<tag>::type & Get_() const;\n\n'.format(**globals))

    def emitTemplatedCreate(self, node, globals):
        self.output.write('// Templated creator for making a union object out of one if its members\n')
        self.output.write('template <Tag tag>\n')
        self.output.write('static {union_name} Create_(typename {union_name}_TagToType<tag>::type member);\n\n'.format(**globals))

    def emitAccessors(self, node, globals):
        for member in node.members():
            locals = dict(
                member_name=member.name,
                value_type=cpp_value_type(member.type),
                parameter_type=cpp_parameter_type(member.type),
                **globals)

            self.output.write('/** {member_name} **/\n'.format(**locals))
            self.output.write('static {union_name} Create{member_name}({value_type}&& new_{member_name});\n'.format(**locals))

            # helper constructor: add it if (1) options request it, and (2) either dupes are not allowed, or dupes are
            # allowed but the ctor is distinct, i.e., that member has no duplicates
            if self.options and self.options.helperConstructors and (not globals["dupes_allowed"] or not member.has_duplicates):
                self.output.write('explicit {union_name}({value_type}&& new_{member_name});\n'.format(**locals))

            self.output.write('{parameter_type} Get_{member_name}() const;\n'.format(**locals))
            self.output.write('void Set_{member_name}({parameter_type} new_{member_name});\n'.format(**locals))

            # emit r-val setter if this is a complex type
            if not is_pass_by_value(member.type):
                self.output.write('void Set_{member_name}({value_type}&& new_{member_name});\n'.format(**locals))
            self.output.write('\n')

    def emitPack(self, node, globals):
        self.output.write('size_t Pack(uint8_t* buff, size_t len) const;\n')
        self.output.write('size_t Pack(CLAD::SafeMessageBuffer& buffer) const;\n\n')

    def emitUnpack(self, node, globals):
        self.output.write('size_t Unpack(const uint8_t* buff, const size_t len);\n')
        self.output.write('size_t Unpack(const CLAD::SafeMessageBuffer& buffer);\n\n')

    def emitSize(self, node, globals):
        self.output.write('size_t Size() const;\n\n')

    def emitEquality(self, node, globals):
        self.output.write(textwrap.dedent('''\
            bool operator==(const {union_name}& other) const;
            bool operator!=(const {union_name}& other) const;
            ''').format(**globals))

    def emitJSONSerializer(self, node, globals):
        self.output.write(textwrap.dedent('''\
            /**** JSON ****/
            Json::Value GetJSON() const;
            bool SetFromJSON(const Json::Value& json);

            '''))

    def emitClearCurrent(self, node, globals):
        self.output.write('void ClearCurrent();\n')

    def emitTagMember(self, node, globals):
        self.output.write('Tag _tag;\n\n')

    def emitUnionMembers(self, node, globals):
        self.output.write('union {\n')
        with self.output.indent(1):
            for member in node.members():
                self.output.write('{member_type} _{member_name};\n'.format(
                    member_type=cpp_value_type(member.type),
                    member_name=member.name))
        self.output.write('};\n')

class TagHUnionTagEmitter(BaseEmitter):

    def visit_NamespaceDecl(self, node, *args, **kwargs):
        discoverer = UnionDiscoverer()
        discoverer.visit(node)
        if discoverer.found:
            self.output.write('namespace {namespace_name} {{\n\n'.format(namespace_name=node.name))
            self.generic_visit(node, *args, **kwargs)
            self.output.write('}} // namespace {namespace_name}\n\n'.format(namespace_name=node.name))

    def visit_UnionDecl(self, node):
        globals = dict(
            union_name=node.name,
            tag_name='{union_name}Tag'.format(union_name=node.name),
            qualified_union_name=node.fully_qualified_name(),
            qualified_tag_name='{union_name}Tag'.format(union_name=node.fully_qualified_name()),
            object_type='UNION')

        members = node.members()
        self.emitTags(node, globals)
        self.output.write(textwrap.dedent('''\
            const char* {tag_name}ToString(const {tag_name} tag);

            '''.format(**globals)))

    def emitTags(self, node, globals):
        # Tags are 1 byte only for now!
        self.output.write('enum class {tag_name} : uint8_t {{\n'.format(**globals))

        with self.output.indent(1):
            lines = []
            for member in node.members():
                if member.init:
                    initializer = hex(member.tag) if member.init.type == "hex" else str(member.tag)
                    start = '{member_name}'.format(member_name=member.name)
                    middle = ' = {initializer},'.format(initializer=initializer)
                else:
                    start = '{member_name},'.format(member_name=member.name)
                    middle = ''
                end = ' // {value}'.format(value=member.tag)
                lines.append((start, middle, end))

            start = 'INVALID'.format(**globals)
            middle = ' = {invalid_tag}'.format(invalid_tag=node.invalid_tag)
            lines.append((start, middle))

            self.output.write_with_aligned_whitespace(lines)

        self.output.write(textwrap.dedent('''\
            };

            '''))

class TagHHashEmitter(BaseEmitter):

    def visit_UnionDecl(self, node):
        globals = dict(
            union_name=node.name,
            tag_name='{union_name}Tag'.format(union_name=node.name),
            qualified_union_name=node.fully_qualified_name(),
            qualified_tag_name='{union_name}Tag'.format(union_name=node.fully_qualified_name()),
            object_type='UNION')

        self.output.write(textwrap.dedent('''\
            template<>
            struct std::hash<{qualified_tag_name}>
            {{
            \tsize_t operator()({qualified_tag_name} t) const
            \t{{
            \t\treturn static_cast<std::underlying_type<{qualified_tag_name}>::type>(t);
            \t}}
            }};

            ''').format(**globals))

class CPPUnionEmitter(BaseEmitter):
    def __init__(self, output=sys.stdout, options=None, prefix=''):
        self.output = output
        self.options = options
        self.prefix = prefix

    def visit_UnionDecl(self, node):

        globals = dict(
            union_name=node.name,
            union_hash=node.hash_str,
            tag_name='{union_name}Tag'.format(union_name=node.name),
            qualified_union_name=node.fully_qualified_name(),
            qualified_tag_name='{union_name}Tag'.format(union_name=node.fully_qualified_name()),
            object_type='UNION',
            dupes_allowed=node.dupes_allowed)

        self.emitHeader(node, globals)

        self.emitConstructors(node, globals)
        self.emitAccessors(node, globals)
        self.emitUnpack(node, globals)
        self.emitPack(node, globals)
        self.emitSize(node, globals)
        self.emitEquality(node, globals)


        # emit if option is set and default constrcutors exist for all members of the union (the union
        # itself always has a default constructor)
        if self.options.emitJSON and emitterutil._do_all_members_have_default_constructor(node):
            self.emitJSONSerializer(node, globals)

        self.emitClearCurrent(node, globals)
        self.emitTagToString(node, globals)

        self.emitFooter(node, globals)

    def emitHeader(self, node, globals):
        self.output.write(textwrap.dedent('''\
            // {object_type} {union_name}

            ''').format(**globals))

    def emitFooter(self, node, globals):
        self.output.write('\n')
        self.output.write('const char* {union_name}VersionHashStr = "{union_hash}";\n\n'.format(**globals))
        self.output.write('const uint8_t {union_name}VersionHash[16] = {{ \n'.format(**globals))
        hex_data = emitterutil.decode_hex_string(globals['union_hash'])
        with self.output.indent(2):
            for i, byte in enumerate(hex_data):
                separator = ','
                if i == (len(hex_data) - 1):
                    separator = ''
                self.output.write("{hex_byte}{separator} ".format(hex_byte=hex(byte),
                                                                  separator=separator))
        self.output.write("\n};\n\n")


    def emitSwitch(self, node, globals, callback, tag_type='Tag', argument='GetTag()', default_case='break;\n'):
        self.output.write('switch({argument}) {{\n'.format(argument=argument))

        for member in node.members():
            self.output.write('case {tag_type}::{member_name}:\n'.format(member_name=member.name, tag_type=tag_type))
            with self.output.indent(1):
                callback(member)

        self.output.write('default:\n')
        with self.output.indent(1):
            self.output.write(default_case)

        self.output.write('}\n')

    def emitConstructors(self, node, globals):
        self.output.write(textwrap.dedent('''\
            {union_name}::{union_name}(const CLAD::SafeMessageBuffer& buff)
            : _tag(Tag::INVALID)
            {{
            \tUnpack(buff);
            }}

            {union_name}::{union_name}(const uint8_t* buffer, size_t length)
            : _tag(Tag::INVALID)
            {{
            \tCLAD::SafeMessageBuffer buff(const_cast<uint8_t*>(buffer), length);
            \tUnpack(buff);
            }}

            ''').format(**globals))

        def copy_body(member):
            if is_trivial_type(member.type):
                self.output.write('this->{private_name} = other.{private_name};\n'.format(
                    private_name='_{member_name}'.format(member_name=member.name),
                    member_name=member.name,
                    member_type=cpp_value_type(member.type)))
            else:
                self.output.write('new(&(this->{private_name})) {member_type}(other.{private_name});\n'.format(
                    private_name='_{member_name}'.format(member_name=member.name),
                    member_type=cpp_value_type(member.type)))
            self.output.write('break;\n')

        def move_body(member):
            if is_trivial_type(member.type):
                copy_body(member)
            else:
                self.output.write('new(&(this->{private_name})) {member_type}(std::move(other.{private_name}));\n'.format(
                    private_name='_{member_name}'.format(member_name=member.name),
                    member_type=cpp_value_type(member.type)))
            self.output.write('break;\n')

        # copy contructor
        self.output.write(textwrap.dedent('''\
            {union_name}::{union_name}(const {union_name}& other)
            : _tag(other._tag)
            {{
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, copy_body, default_case='_tag = Tag::INVALID;\nbreak;\n')
        self.output.write(textwrap.dedent('''\
            }

            '''))

        # move constructor
        self.output.write(textwrap.dedent('''\
            {union_name}::{union_name}({union_name}&& other) noexcept
            : _tag(other._tag)
            {{
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, move_body, default_case='_tag = Tag::INVALID;\nbreak;\n')
        self.output.write(textwrap.dedent('''\
            \tother.ClearCurrent();
            }

            '''))

        # assignment opperator
        self.output.write(textwrap.dedent('''\
            {union_name}& {union_name}::operator=(const {union_name}& other)
            {{
            \tif(this == &other) {{ return *this; }}
            \tClearCurrent();
            \t_tag = other._tag;
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, copy_body, default_case='_tag = Tag::INVALID;\nbreak;\n')
        self.output.write(textwrap.dedent('''\
            \treturn *this;
            }

            '''))

        # assignment opperator
        self.output.write(textwrap.dedent('''\
            {union_name}& {union_name}::operator=({union_name}&& other) noexcept
            {{
            \tif(this == &other) {{ return *this; }}
            \tClearCurrent();
            \t_tag = other._tag;
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, move_body, default_case='_tag = Tag::INVALID;\nbreak;\n')
        self.output.write(textwrap.dedent('''\
            \tother.ClearCurrent();
            \treturn *this;
            }

            '''))

    def emitAccessors(self, node, globals):
        value_types = dict()
        for member in node.members():
            locals = dict(
                member_name=member.name,
                private_name='_{member_name}'.format(member_name=member.name),
                value_type=cpp_value_type(member.type),
                parameter_type=cpp_parameter_type(member.type),
                **globals)

            # factory
            self.output.write(textwrap.dedent('''\
              {union_name} {union_name}::Create{member_name}({value_type}&& new_{member_name})
              {{
              \t{union_name} m;
              \tm.Set_{member_name}(new_{member_name});
              \treturn m;
              }}

              ''').format(**locals))

            # helper constructor: add it if (1) options request it, and (2) either dupes are not allowed, or dupes are
            # allowed but the ctor is distinct, i.e., that member has no duplicates
            if self.options and self.options.helperConstructors and (not globals["dupes_allowed"] or not member.has_duplicates):
                value_type = locals['value_type']
                if value_type in value_types:
                    emitterutil.exit_at_coord(member.coord, ('Type-based helper constructors are being generated, ' +
                        'but there are two objects of type {0}: {1} and {2}.').format(
                            value_type,
                            value_types[value_type].name,
                            member.name))
                value_types[value_type] = member

                self.output.write(textwrap.dedent('''\
                    {union_name}::{union_name}({value_type}&& new_{member_name})
                    {{
                    \tnew(&this->{private_name}) {value_type}(std::move(new_{member_name}));
                    \t_tag = Tag::{member_name};
                    }}

                    ''').format(**locals))

            self.output.write(textwrap.dedent('''\
                {parameter_type} {union_name}::Get_{member_name}() const
                {{
                \tassert(_tag == Tag::{member_name});
                \treturn this->{private_name};
                }}

                void {union_name}::Set_{member_name}({parameter_type} new_{member_name})
                {{
                \tif(this->_tag == Tag::{member_name}) {{
                \t\tthis->{private_name} = new_{member_name};
                \t}}
                \telse {{
                \t\tClearCurrent();
                ''').format(**locals))

            with self.output.indent(2):
                if not is_trivial_type(member.type):
                    self.output.write('new(&this->{private_name}) {value_type}(new_{member_name});\n'.format(**locals))
                else:
                    self.output.write('this->{private_name} = new_{member_name};\n'.format(**locals))

            self.output.write(textwrap.dedent('''\
                \t\t_tag = Tag::{member_name};
                \t}}
                }}

                ''').format(**locals))

            # Emit templated Get_<Tag>()
            value_type = locals['value_type']
            self.output.write(textwrap.dedent('''\
                template<>
                const {value_type}& {union_name}::Get_<{union_name}::Tag::{member_name}>() const
                {{
                \tassert(_tag == Tag::{member_name});
                \treturn this->{private_name};
                }}

                ''').format(**locals))

            # Emit templated Create_<Tag>()
            self.output.write(textwrap.dedent('''\
                template<>
                {union_name} {union_name}::Create_<{union_name}::Tag::{member_name}>({value_type} member)
                {{
                \treturn Create{member_name}(std::move(member));
                }}

                ''').format(**locals))

            # emit r-val setter if this is a complex type
            if not is_pass_by_value(member.type):
                self.output.write(textwrap.dedent('''\
                    void {union_name}::Set_{member_name}({value_type}&& new_{member_name})
                    {{
                    \tif (this->_tag == Tag::{member_name}) {{
                    \t\tthis->{private_name} = std::move(new_{member_name});
                    \t}}
                    \telse {{
                    \t\tClearCurrent();
                    \t\tnew(&this->{private_name}) {value_type}(std::move(new_{member_name}));
                    \t\t_tag = Tag::{member_name};
                    \t}}
                    }}

                    ''').format(**locals))

    def emitPack(self, node, globals):
        visitor = CPPPackStatementEmitter(self.output, self.options)
        def body(member):
            visitor.visit(member.type,
                member_name='_{member_name}'.format(member_name=member.name))
            self.output.write('break;\n')

        self.output.write(textwrap.dedent('''\
            size_t {union_name}::Pack(uint8_t* buff, size_t len) const
            {{
            \tCLAD::SafeMessageBuffer buffer(buff, len, false);
            \treturn Pack(buffer);
            }}

            size_t {union_name}::Pack(CLAD::SafeMessageBuffer& buffer) const
            {{
            \tbuffer.Write(_tag);
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, body)
        self.output.write(textwrap.dedent('''\
            \treturn buffer.GetBytesWritten();
            }

            '''))

    def emitUnpack(self, node, globals):
        visitor = CPPUnpackStatementEmitter(self.output, self.options)
        def body(member):
            private_name = '_{member_name}'.format(member_name=member.name)
            if is_trivial_type(member.type):
                visitor.visit(member.type, member_name=private_name)
            elif isinstance(member.type, ast.CompoundType):
                self.output.write(textwrap.dedent('''\
                    if (newTag != oldTag) {{
                    \tnew(&(this->{private_name})) {member_type}(buffer);
                    }}
                    else {{
                    ''').format(
                        private_name=private_name,
                        member_type=cpp_value_type(member.type)))
                with self.output.indent(1):
                    visitor.visit(member.type, member_name=private_name)
                self.output.write('}\n')
            else:
                # must clear current if we didn't already do so
                self.output.write(textwrap.dedent('''\
                    if (newTag == oldTag) {{
                    \tClearCurrent();
                    }}
                    new(&(this->{private_name})) {member_type}();
                    ''').format(
                        private_name=private_name,
                        member_type=cpp_value_type(member.type)))
                visitor.visit(member.type, member_name=private_name)
            self.output.write('break;\n')

        self.output.write(textwrap.dedent('''\
            size_t {union_name}::Unpack(const uint8_t* buff, const size_t len)
            {{
            \tconst CLAD::SafeMessageBuffer buffer(const_cast<uint8_t*>(buff), len, false);
            \treturn Unpack(buffer);
            }}

            size_t {union_name}::Unpack(const CLAD::SafeMessageBuffer& buffer)
            {{
            \tTag newTag {{Tag::INVALID}};
            \tconst Tag oldTag {{GetTag()}};
            \tbuffer.Read(newTag);
            \tif (newTag != oldTag) {{
            \t\tClearCurrent();
            \t}}
            ''').format(**globals))

        with self.output.indent(1):
            self.emitSwitch(node, globals, body, argument='newTag')
        self.output.write(textwrap.dedent('''\
            \t_tag = newTag;
            \treturn buffer.GetBytesRead();
            }

            '''))

    def emitSize(self, node, globals):
        visitor = CPPSizeStatementEmitter(self.output, self.options)
        def body(member):
            visitor.visit(member.type,
                member_name='_{member_name}'.format(member_name=member.name))
            self.output.write('break;\n')

        # Tags are one byte only for now!
        self.output.write(textwrap.dedent('''\
            size_t {union_name}::Size() const
            {{
            \tsize_t result {{1}}; // tag = uint_8
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, body)
        self.output.write(textwrap.dedent('''\
            \treturn result;
            }

            '''))

    def emitEquality(self, node, globals):
        def body(member):
            private_name = '_{member_name}'.format(member_name=member.name)
            self.output.write('return this->{private_name} == other.{private_name};\n'.format(
                private_name=private_name))

        self.output.write(textwrap.dedent('''\
            bool {union_name}::operator==(const {union_name}& other) const
            {{
            \tif (this->_tag != other._tag) {{
            \t\treturn false;
            \t}}
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, body, default_case='return true;\n')
        self.output.write(textwrap.dedent('''\
            }}

            bool {union_name}::operator!=(const {union_name}& other) const
            {{
            \treturn !(operator==(other));
            }}

            '''.format(**globals)))

    def emitJSONSerializer(self, node, globals):
        self.output.write(textwrap.dedent('''\
            Json::Value {union_name}::GetJSON() const
            {{
            \tJson::Value root;
            ''').format(**globals))

        def body(member):
            private_name = '_{member_name}'.format(member_name=member.name)
            if jsoncpp_as_method(member.type) is None:
                if isinstance(member.type.type_decl, ast.MessageDecl) and member.type.type_decl.object_type() == "structure":
                    self.output.write('root = this->{private_name}.GetJSON();\n'.format(private_name=private_name))
                else:
                    self.output.write('// {member_name} is not serializable.\n'.format(member_name=member.name))
            else:
                self.output.write('root["value"] = this->{private_name};\n'.format(private_name=private_name))
            self.output.write('root["type"] = "{member_name}";\n'.format(member_name=member.name))
            self.output.write('break;\n')

        with self.output.indent(1):
            self.emitSwitch(node, globals, body)

        self.output.write(textwrap.dedent('''\
        \treturn root;
        }
        '''))

        self.output.write(textwrap.dedent('''
            bool {union_name}::SetFromJSON(const Json::Value& json)
            {{
              ClearCurrent();

              bool result = false;

              if(json.isMember("type")) {{
                std::string tagStr = json["type"].asString();

                try {{

                  if(tagStr == "INVALID") {{
                    // Already cleared, do nothing.
                    result = true;  
                  }}
            ''').format(**globals))

        with self.output.indent(3):
            for member in node.members():
                self.output.write('else if(tagStr == "{member_name}") {{\n'.format(member_name=member.name))
                with self.output.indent(1):
                    private_name = '_{member_name}'.format(member_name=member.name)
                    self.output.write('new(&(this->{private_name})) {member_type};\n'.format(private_name=private_name, member_type=cpp_value_type(member.type)))
                    if jsoncpp_as_method(member.type) is None:
                        if isinstance(member.type.type_decl, ast.MessageDecl) and member.type.type_decl.object_type() == "structure":
                            self.output.write('result = this->{private_name}.SetFromJSON(json);\n'.format(private_name=private_name))
                        else:
                            self.output.write('// {member_name} is not a structure, is not serializable.\nresult = false;\n'.format(member_name=member.name))
                    else:
                        self.output.write('this->{private_name} = json["value"].{as_method};\nresult = true;\n'.format(private_name=private_name, as_method=jsoncpp_as_method(member.type)))
                    self.output.write('_tag = Tag::{member_name};\n'.format(member_name=member.name))

                self.output.write('}\n')

        self.output.write(textwrap.dedent('''\

                }
                catch(Json::LogicError) {
                  result = false;
                }
              }

              return result;
            }

        '''))

    def emitClearCurrent(self, node, globals):
        def body(member):
            if (not is_trivial_type(member.type)):
                # need to emit a destructor
                self.output.write('{private_name}.~{member_type}();\n'.format(
                    private_name='_{member_name}'.format(member_name=member.name),
                    member_type=cpp_value_type_destructor(member.type)))
            self.output.write('break;\n')

        self.output.write(textwrap.dedent('''\
            void {union_name}::ClearCurrent()
            {{
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, body)
        self.output.write(textwrap.dedent('''\
            \t_tag = Tag::INVALID;
            }

            '''))

    def emitTagToString(self, node, globals):
        def body(member):
            self.output.write('return "{member_name}";\n'.format(member_name=member.name, **globals))

        self.output.write(textwrap.dedent('''\
            const char* {tag_name}ToString(const {tag_name} tag) {{
            ''').format(**globals))
        with self.output.indent(1):
            self.emitSwitch(node, globals, body,
                tag_type=globals['tag_name'], argument='tag', default_case='return "INVALID";\n')
        self.output.write('}\n')

class CPPPackStatementEmitter(BaseEmitter):

    def visit_BuiltinType(self, node, member_name):
        self.output.write('buffer.Write(this->{member_name});\n'.format(member_name=member_name))

    def visit_DefinedType(self, *args, **kwargs):
        self.visit_BuiltinType(*args, **kwargs)

    def visit_PascalStringType(self, node, member_name):
        self.output.write('buffer.WritePString<{length_type}>(this->{member_name});\n'.format(
            length_type=cpp_value_type(node.length_type),
            member_name=member_name))

    def visit_CompoundType(self, node, member_name):
        self.output.write('this->{member_name}.Pack(buffer);\n'.format(member_name=member_name))

    def visit_FixedArrayType(self, node, member_name):
        if isinstance(node.member_type, ast.PascalStringType):
            self.output.write('buffer.WritePStringFArray<{length}, {string_length_type}>(this->{member_name});\n'.format(
                length=node.length,
                string_length_type=cpp_value_type(node.member_type.length_type),
                member_name=member_name))
        elif isinstance(node.member_type, ast.CompoundType):
            self.output.write(textwrap.dedent('''\
                for ({parameter_type} m : {member_name}) {{
                \tm.Pack(buffer);
                }}
                ''').format(member_name=member_name,
                    parameter_type=cpp_parameter_type(node.member_type)))
        else:
            if isinstance(node.length, str):
                self.output.write('buffer.WriteFArray<{member_type}, static_cast<size_t>({length})>(this->{member_name});\n'.format(
                    length=node.length,
                    member_type=cpp_value_type(node.member_type),
                    member_name=member_name))
            else:
                self.output.write('buffer.WriteFArray<{member_type}, {length}>(this->{member_name});\n'.format(
                    length=node.length,
                    member_type=cpp_value_type(node.member_type),
                    member_name=member_name))            

    def visit_VariableArrayType(self, node, member_name):
        if isinstance(node.member_type, ast.PascalStringType):
            self.output.write('buffer.WritePStringVArray<{array_length_type}, {string_length_type}>(this->{member_name});\n'.format(
                array_length_type=cpp_value_type(node.length_type),
                string_length_type=cpp_value_type(node.member_type.length_type),
                member_name=member_name))
        elif isinstance(node.member_type, ast.CompoundType):
            self.output.write('buffer.Write(static_cast<{array_length_type}>({member_name}.size()));\n'.format(
                array_length_type=cpp_value_type(node.length_type),
                member_name=member_name))
            self.output.write(textwrap.dedent('''\
                for ({parameter_type} m : {member_name}) {{
                \tm.Pack(buffer);
                }}
                ''').format(member_name=member_name,
                    parameter_type=cpp_parameter_type(node.member_type)))
        else:
            self.output.write('buffer.WriteVArray<{member_type}, {length_type}>(this->{member_name});\n'.format(
                member_type=cpp_value_type(node.member_type),
                length_type=cpp_value_type(node.length_type),
                member_name=member_name))

class CPPUnpackStatementEmitter(BaseEmitter):

    def visit_BuiltinType(self, node, member_name):
        self.output.write('buffer.Read(this->{member_name});\n'.format(member_name=member_name))

    def visit_DefinedType(self, *args, **kwargs):
        self.visit_BuiltinType(*args, **kwargs)

    def visit_PascalStringType(self, node, member_name):
        self.output.write('buffer.ReadPString<{length_type}>(this->{member_name});\n'.format(
            length_type=cpp_value_type(node.length_type),
            member_name=member_name))

    def visit_CompoundType(self, node, member_name):
        self.output.write('this->{member_name}.Unpack(buffer);\n'.format(member_name=member_name))

    def visit_FixedArrayType(self, node, member_name):
        if isinstance(node.member_type, ast.PascalStringType):
            self.output.write('buffer.ReadPStringFArray<{length}, {string_length_type}>(this->{member_name});\n'.format(
                length=node.length,
                string_length_type=cpp_value_type(node.member_type.length_type),
                member_name=member_name))
        elif isinstance(node.member_type, ast.CompoundType):
            self.output.write('buffer.ReadCompoundTypeFArray<{member_type}, {length}>(this->{member_name});\n'.format(
                 member_type=cpp_value_type(node.member_type),
                 length=node.length,
                 member_name=member_name))
        else:
            if isinstance(node.length, str):
                self.output.write('buffer.ReadFArray<{member_type}, static_cast<size_t>({length})>(this->{member_name});\n'.format(
                    length=node.length,
                    member_type=cpp_value_type(node.member_type),
                    member_name=member_name))
            else:
                self.output.write('buffer.ReadFArray<{member_type}, {length}>(this->{member_name});\n'.format(
                    length=node.length,
                    member_type=cpp_value_type(node.member_type),
                    member_name=member_name))            
    
    def visit_VariableArrayType(self, node, member_name):
        if isinstance(node.member_type, ast.PascalStringType):
            self.output.write('buffer.ReadPStringVArray<{array_length_type}, {string_length_type}>(this->{member_name});\n'.format(
                array_length_type=cpp_value_type(node.length_type),
                string_length_type=cpp_value_type(node.member_type.length_type),
                member_name=member_name))
        elif isinstance(node.member_type, ast.CompoundType):
            self.output.write('buffer.ReadCompoundTypeVArray<{member_type}, {length_type}>(this->{member_name});\n'.format(
                member_type=cpp_value_type(node.member_type),
                length_type=cpp_value_type(node.length_type),
                member_name=member_name))
        else:
            self.output.write('buffer.ReadVArray<{member_type}, {length_type}>(this->{member_name});\n'.format(
                member_type=cpp_value_type(node.member_type),
                length_type=cpp_value_type(node.length_type),
                member_name=member_name))

class CPPSizeStatementEmitter(BaseEmitter):

    def visit_BuiltinType(self, node, member_name):
        self.output.write('result += {size}; // {type}\n'.format(
            size=node.size, type=node.name))

    def visit_DefinedType(self, *args, **kwargs):
        self.visit_BuiltinType(*args, **kwargs)

    def visit_PascalStringType(self, node, member_name):
        self.output.write(textwrap.dedent('''\
            result += {length_size}; // {length_type} (string length)
            result += this->{member_name}.length(); // {member_type}
            ''').format(
                length_size=node.length_type.size,
                length_type=node.length_type.name,
                member_type=node.member_type.name,
                member_name=member_name))

    def visit_CompoundType(self, node, member_name):
        self.output.write('result += this->{member_name}.Size(); // {type}\n'.format(
            member_name=member_name, type=node.name))

    def visit_FixedArrayType(self, node, member_name):
        if isinstance(node.member_type, ast.PascalStringType):
            self.output.write('result += {string_length_size} * {length}; // {string_length_type} (string lengths)\n'.format(
                string_length_size=node.member_type.length_type.size,
                string_length_type=node.member_type.length_type.name,
                length=node.length,
                member_name=member_name))
            self.emitRecursiveSize(node, member_name, 'length')
        elif isinstance(node.member_type, ast.CompoundType):
            self.emitRecursiveSize(node, member_name)
        else:
            if isinstance(node.length, str):
                self.output.write('result += {member_size} * static_cast<size_t>({length}); // {member_type} * {length}\n'.format(
                    member_size=node.member_type.size,
                    member_type=node.member_type.name,
                    length=node.length))
            else:
                self.output.write('result += {member_size} * {length}; // {member_type} * {length}\n'.format(
                    member_size=node.member_type.size,
                    member_type=node.member_type.name,
                    length=node.length))

    def visit_VariableArrayType(self, node, member_name):
        self.output.write('result += {length_size}; // {length_type} (array length)\n'.format(
            length_size=node.length_type.size,
            length_type=node.length_type.name))
        if isinstance(node.member_type, ast.PascalStringType):
            self.output.write('result += {string_length_size} * this->{member_name}.size(); // {string_length_type} (string lengths)\n'.format(
                string_length_size=node.member_type.length_type.size,
                string_length_type=node.member_type.length_type.name,
                member_name=member_name))
            self.emitRecursiveSize(node, member_name, 'length')
        elif isinstance(node.member_type, ast.CompoundType):
            self.emitRecursiveSize(node, member_name)
        else:
            self.output.write('result += {member_size} * this->{member_name}.size(); // {member_type}\n'.format(
                member_size=node.member_type.size,
                member_type=node.member_type.name,
                member_name=member_name))

    def emitPascalStringArraySize(self, node, member_name):
        self.output.write('result += {string_length_size} * this->{member_name}.size(); // {string_length_type} (string lengths)\n'.format(
            string_length_size=node.member_type.length_type.size,
            string_length_type=node.member_type.length_type.name,
            member_name=member_name))
        self.emitRecursiveSize(node, member_name, 'length')

    def emitRecursiveSize(self, node, member_name, size_method='Size'):
        self.output.write(textwrap.dedent('''\
            for ({parameter_type} m : this->{member_name}) {{
            \tresult += m.{size_method}();
            }}
            ''').format(
                member_type=node.member_type.name,
                parameter_type=cpp_parameter_type(node.member_type),
                member_name=member_name,
                size_method=size_method))

class HEnumConceptEmitter(BaseEmitter):

    def visit_EnumConceptDecl(self, node):
        globals = dict(
            enum_concept_name=node.name,
            enum_concept_hash=node.hash_str,
            enum_concept_return_type=cpp_value_type(node.return_type.type),
            enum_concept_type=node.enum
        )

        self.emitHeader(node, globals)

    def emitHeader(self, node, globals):
        argument_name = emitterutil._lower_first_char_of_string(globals['enum_concept_type'])
        self.output.write('{enum_concept_return_type} {enum_concept_name}(const {enum_concept_type}& {argument_name}, const {enum_concept_return_type}& defaultValue);\n\n'.format(argument_name=argument_name, **globals))


class CPPEnumConceptEmitter(HEnumEmitter):

    def visit_EnumConceptDecl(self, node):
        globals = dict(
            enum_concept_name=node.name,
            enum_concept_hash=node.hash_str,
            enum_concept_return_type=cpp_value_type(node.return_type.type),
            enum_concept_type=node.enum
        )
        self.emit(node, globals)

    def emit(self, node, globals):
        argument_name = emitterutil._lower_first_char_of_string(globals['enum_concept_type'])
        self.output.write('{enum_concept_return_type} {enum_concept_name}(const {enum_concept_type}& {argument_name}, const {enum_concept_return_type}& defaultValue)\n{{\n'.format(argument_name=argument_name, **globals))
        self.output.write('\tswitch({argument_name}) {{\n'.format(argument_name=argument_name))

        for member in node.members():
            self.output.write('\t\tcase {enum_concept_type}::{member_name}:\n'.format(member_name=member.name, **globals))
            self.output.write('\t\t\treturn {member_value};\n'.format(member_value=member.value.value))

        self.output.write('\t\tdefault:\n')
        self.output.write('\t\t\treturn defaultValue;\n')

        self.output.write('\t}\n')
        self.output.write('}\n\n')

if __name__ == '__main__':
    from clad import emitterutil

    language = 'C++'
    default_header_extension = '.h'
    source_extension = '.cpp'

    option_parser = emitterutil.StandardArgumentParser(language)
    option_parser.add_argument('-r', '--header-output-directory', metavar='dir',
        help='The directory to output the {language} header file(s) to.'.format(language=language))
    option_parser.add_argument('--header-output-extension', default=default_header_extension, metavar='ext',
        help='The extension to use for header files. (Helps work around a CMake Xcode issue.)')
    option_parser.add_argument('--output-union-helper-constructors', dest='helperConstructors', action='store_true',
        help='Emits helper union constructor, one for each member')
    option_parser.add_argument('--output-properties', dest='emitProperties', action='store_true',
        help='Emits accessors and mutators for all members of a structure.')
    option_parser.add_argument('--output-json', dest='emitJSON', action='store_true',
        help='Emits code to serialize all members in a structure to and from jsoncpp values.')

    options = option_parser.parse_args()
    if not options.header_output_directory:
        options.header_output_directory = options.output_directory

    tree = emitterutil.parse(options)
    comment_lines = emitterutil.get_comment_lines(options, language)

    union_discoverer = UnionDiscoverer()
    union_discoverer.visit(tree)

    if union_discoverer.found:
        def tag_output_header_callback(output):
            for emitter_type in [TagHUnionTagEmitter, TagHHashEmitter]:
                emitter_type(output, options=options).visit(tree)

        tag_output_header = emitterutil.get_output_file(options, 'Tag' + options.header_output_extension)
        emitterutil.write_c_file(options.header_output_directory, tag_output_header,
            tag_output_header_callback,
            comment_lines, use_inclusion_guards=True,
            system_headers=['functional'])

        main_output_header_local_headers = [tag_output_header]
    else:
        main_output_header_local_headers = []

    if options.emitJSON:
        main_output_header_local_headers += ['json/json.h']

    def main_output_header_callback(output):
        HNamespaceEmitter(output, options=options).visit(tree)

    main_output_header = emitterutil.get_output_file(options, options.header_output_extension)
    emitterutil.write_c_file(options.header_output_directory, main_output_header,
        main_output_header_callback,
        comment_lines=comment_lines,
        use_inclusion_guards=True,
        system_headers=['array', 'cassert', 'cstdint', 'string', 'vector', 'CLAD/SafeMessageBuffer.h'],
        local_headers=main_output_header_local_headers)


    def main_output_source_callback(output):
        CPPNamespaceEmitter(output, options=options).visit(tree)

    cfile_system_headers = []
    if options.emitJSON:
        cfile_system_headers += ['unordered_map', 'limits', 'iostream']

    main_output_source = emitterutil.get_output_file(options, source_extension)
    emitterutil.write_c_file(options.output_directory, main_output_source,
        main_output_source_callback,
        comment_lines=comment_lines,
        use_inclusion_guards=False,
        system_headers=cfile_system_headers,
        local_headers=[main_output_header])
