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

"""
The parser for CLAD
Author: Mark Pauley
Date: 12/12/2014
"""

from __future__ import absolute_import
from __future__ import print_function

import copy
import os.path
import re
import itertools

from .ply import yacc

from . import ast
from .lexer import AnkiBufferLexer
from .plyparser import PLYParser, Coord, ParseError

THIS_DIR = os.path.dirname(os.path.realpath(__file__))

class CLADParser(PLYParser):
    """ A parser for the Anki Buffer definition language.
    """

    def __init__(self,
                 input_directories=(".",),
                 lex_optimize=False,
                 lextab='clad.lextab',
                 yacc_optimize=False,
                 yacctab='clad.yacctab',
                 yacc_debug=False,
                 lex_debug=False):
        """
        Create a new CLAD parser
        Options:
        - input_directories: a tuple including the directories to search for
          input files and #include files.

        - lex_optimize / lextab: tell PLY to generate a tokenizer table to
          increase performance of the tokenizer.

        - yacc_optimize / yacctab: tell PLY to generate a parser table to
          increase performance of the LALR parser.

        - yacc_debug: tell PLY to spew LALR parser traces to the stderr

        - lex_debug: tell PLY to spew tokenizer traces to the stderr
        """
        self.lexer = AnkiBufferLexer(
            lex_debug=lex_debug,
            error_func=self._lex_error_func,
            type_lookup_func=self._lex_type_lookup_func)

        self.lexer.build(optimize=lex_optimize, lextab=lextab, outputdir=THIS_DIR)
        self.tokens = self.lexer.tokens
        self.parser = yacc.yacc(
            module=self,
            start='start',
            debug=yacc_debug,
            optimize=yacc_optimize,
            tabmodule=yacctab,
            outputdir=THIS_DIR
        )
        self._input_directories = input_directories

    _include_directive_re = re.compile('^#include\s+\"(.*)\"(?:\s*/.*)*$')

    def token_to_coord(self, token):
        return self.lineno_to_coord(token.lineno, self.lexer.find_tok_column(token.lexpos))

    def production_to_coord(self, production, index):
        return self.lineno_to_coord(production.lineno(index), self.lexer.find_tok_column(production.lexpos(index)))

    def lineno_to_coord(self, lineno, column):
        coord = copy.copy(self._lines_to_coords[lineno - 1])
        coord.column = column
        return coord

    def preprocess(self, text, filename, directory):
        lines = text.splitlines(True)
        if lines and lines[-1] and lines[-1][-1] != '\n':
            lines[-1] = lines[-1] + '\n'
        processed_text = []
        lineno = 1
        for line in lines:
            self._lines_to_coords.append(Coord(filename, directory, lineno))
            m = re.match(self._include_directive_re, line)
            if m:
                include_file_path = m.group(1)
                for input_directory in self._input_directories:
                    real_include_file_path = os.path.join(input_directory, include_file_path)
                    if os.path.exists(real_include_file_path):
                        chosen_input_directory = input_directory
                        break
                else:
                    raise ParseError(self._lines_to_coords[-1],
                                     'Could not find file "{0}"\nInclude directories searched:\n\t{1}\n'.format(
                                        include_file_path, '\n\t'.join(self._input_directories)))
                processed_text.append("// " + line)
                try:
                    with open(real_include_file_path, "r") as f:
                        data = f.read()
                except IOError as e:
                    raise ParseError(self._lines_to_coords[-1],
                                     'Error reading file "{0}" in directory "{1}"'.format(
                                        include_file_path, chosen_input_directory))
                processed_text.append('include "{0}" {{\n'.format(include_file_path))
                self._lines_to_coords.append(Coord(filename, directory, lineno))
                if m.group(1) not in self._included_files:
                    self._included_files.append(include_file_path)
                    processed_text.extend(self.preprocess(data, include_file_path, chosen_input_directory))
                processed_text.append('}\n')
                self._lines_to_coords.append(Coord(filename, directory, lineno))
            else:
                processed_text.append(line)
            lineno += 1
        return processed_text

    def parse(self, text, filename='', directory='', debuglevel=0):
        """ Parses buffer definitions and returns an AST.

          text:
            A String containing the source code

          filename:
            Name of the file being parsed (for meaningful
            error messages)

          debuglevel:
            Debug level to yacc
        """
        self._last_yielded_token = None
        self.declared_types = dict()
        self._namespace_stack = [ None ]

        self._enum_decls = []
        self._message_types = []
        self._union_decls = []
        self._all_members = []
        self._enum_concepts = []

        self._included_files = []
        self._lines_to_coords = []
        processed_text = ''.join(self.preprocess(text, filename, directory))
        self._last_yielded_token = None

        self._syntax_tree = self.parser.parse(
            input=processed_text,
            lexer=self.lexer,
            tracking=True,
            debug=debuglevel)

        self._postprocess_enums()
        self._postprocess_unions()
        self._postprocess_enum_concepts()
        self._postprocess_versioning_hashes(self._syntax_tree)
        self._note_ambiguity()

        return self._syntax_tree

    def _postprocess_enums(self):
        for enum in self._enum_decls:
            value = 0
            str_value = None
            all_values = set()
            for member in enum.members():
                isHex = False
                if member.initializer:
                    str_value = None
                    value = member.initializer.value
                  
                    if member.initializer.type == "hex":
                        value = hex(value)
                        isHex = True

                    # if the enum value is initialized with a string then set str_value and reset value
                    if type(value) is str:
                        str_value = value
                        value = 0

                if type(value) is not str and not (enum.storage_type.min <= value <= enum.storage_type.max):
                    raise ParseError(
                        member.coord,
                        "Enum '{0}' has a value '{1}' outside the range of '{2}'".format(
                        enum.fully_qualified_name(),
                        member.name,
                        enum.storage_type.name))

                # if the enum value is a string add " + <incrementing value>" to the end of the string so the actual generated value
                # is incrementing
                if str_value is not None:
                    addition = " + {0}".format(value) if value != 0 else ""
                    member.value = str_value + addition
                else:
                    member.value = value

                comp_value = member.initializer.value if isHex else member.value
                if comp_value in all_values:
                    member.is_duplicate = True

                all_values.add(comp_value)

                value += 1

    def _postprocess_unions(self):
        # Figure out autounions and dupes
        for union_entry in self._union_decls:
            # Autounions are specified explicitly with the autounion keyword
            if union_entry.is_explicit_auto_union():
                coord = union_entry.coord

                if union_entry.member_list is None:
                    union_entry.member_list = ast.MessageMemberDeclList([], coord)

                names = dict()
                for message_type in self._message_types:
                    if message_type.name in names:
                        raise ParseError(
                            union_entry.coord,
                            "Autounion '{0}' would contain two members with the name '{1}': '{2}' and '{3}'".format(
                            union_entry.fully_qualified_name(),
                            message_type.name,
                            names[message_type.name].fully_qualified_name(),
                            message_type.fully_qualified_name()))
                    names[message_type.name] = message_type

                # generate the list of already initialized names, don't add auto entries for these
                initialized_names = dict()
                for member in union_entry.members():
                    initialized_names[member.name] = member
            
                for init_value, message_type in enumerate(self._message_types):
                    if message_type.name not in initialized_names:
                        decl = ast.MessageMemberDecl(message_type.name, message_type, None, coord)
                        union_entry.member_list.append(decl)
                        self._all_members.append(decl)

            if union_entry.dupes_allowed:
                for member in union_entry.members():
                    member.has_duplicates = False
                for pair in itertools.combinations(union_entry.members(), r=2):
                    if pair[0].type == pair[1].type:
                        pair[0].has_duplicates = True
                        pair[1].has_duplicates = True
        
        # calculate all union initializers
        for union in self._union_decls:
            name_set = dict()
            value_set = dict()
            value = 0
            for member in union.members():

                if member.name in name_set:
                    raise ParseError(
                        member.coord,
                        "Union '{0}' has two members with the name '{1}': '{2}' and '{3}'".format(
                        union.fully_qualified_name(),
                        member.name,
                        name_set[member.name].type.fully_qualified_name(),
                        member.type.fully_qualified_name()))
                name_set[member.name] = member

                if member.init:
                    value = member.init.value

                if not (union.tag_storage_type.min <= value <= union.tag_storage_type.max):
                    raise ParseError(
                        member.coord,
                        "Union '{0}' has a tag value that is out of range: '{1}' = '{2}'".format(
                        union.fully_qualified_name(),
                        member.name,
                        member.init.value))

                if value in value_set:
                    raise ParseError(
                        member.coord,
                        "Union '{0}' would contain two members with the same value '{3}': '{1}' and '{2}'".format(
                        union.fully_qualified_name(),
                        member.name,
                        value_set[value].name,
                        value))
                value_set[value] = member

                if value == union.invalid_tag:
                    raise ParseError(
                        member.coord,
                        "Union '{0}' has a tag that would be equal to invalid tag value '{1}': '{2}' = '{3}'".format(
                        union.fully_qualified_name(),
                        union.invalid_tag,
                        member.name,
                        value))

                member.tag = value
                value += 1

    def _postprocess_enum_concepts(self):
        # For every enum concept check that
        # 1) It is using a valid Enum
        # 2) There is a 1 - 1 mapping for all entries in both the EnumConcept and the Enum it is working on
        for enum_concept in self._enum_concepts:

            # Find the matching enum decl
            matching_enum_decl = None
            for enum_decl in self._enum_decls:
                if enum_decl.name == enum_concept.enum:
                    matching_enum_decl = enum_decl
                    break
            if matching_enum_decl is None:
                raise ParseError(
                    enum_concept.coord,
                    "Enum '{0}' in EnumConcept '{1}' does not exist".format(
                        enum_concept.enum,
                        enum_concept.name))

            # Figure out if there are either extra or missing entries in the enum concept
            concept_member_names = set(x.name for x in enum_concept.members())
            enum_member_names = set(x.name for x in matching_enum_decl.members())

            extra_concept_names = list(concept_member_names - enum_member_names)
            extra_enum_names = list(enum_member_names - concept_member_names)

            # There are entries in the EnumConcept that do not exist in the Enum it is working on
            if len(extra_concept_names) > 0:
                raise ParseError(
                    enum_concept.coord,
                    "Entries '{0}' in EnumConcept '{1}' do not exist in Enum '{2}'".format(
                        extra_concept_names,
                        enum_concept.name,
                        matching_enum_decl.name))

            # The EnumConcept is missing entries for some Enum values
            if len(extra_enum_names) > 0:
                raise ParseError(
                    enum_concept.coord,
                    "EnumConcept '{0}' missing entries for EnumValues '{1}' in Enum '{2}'".format(
                        enum_concept.name,
                        extra_enum_names,
                        matching_enum_decl.name))

            # If the return type of the EnumConcept is a builtin type check return values are valid
            if isinstance(enum_concept.return_type.type, ast.BuiltinType):
                for member in enum_concept.members():
                    if isinstance(member.value, ast.StringConst):
                        continue
                    if (member.value.value < enum_concept.return_type.type.min) or (member.value.value > enum_concept.return_type.type.max):
                        raise ParseError(
                            enum_concept.coord,
                            "Entry '{0}' in EnumConcept '{1}' contains invalid value '{2}'".format(
                                member.name,
                                enum_concept.name,
                                member.value.value))




    def _note_ambiguity(self):
        for member in self._all_members:
            type = self._get_type_or_namespace(member.name)
            if type:
                type.possibly_ambiguous = True

    class HashVisitor(ast.NodeVisitor):
        def __init__(self):
            self.hash_str = "Invalid"

        def get_hash_from_node(self, node):
            hasher = ast.ASTHash()
            hasher.visit(node)
            return hasher.hash_str

        def visit_IncludeDecl(self, node, *args, **kwargs):
            # don't go hashing #included entities
            pass

        def visit_Decl_subclass(self, node, *args, **kwargs):
            self.generic_visit(node, *args, **kwargs)
            node.hash_str = self.get_hash_from_node(node)

    def _postprocess_versioning_hashes(self, node):
        self.HashVisitor().visit(node)
        pass

    ###### implementation ######

    def _get_current_namespace(self):
        return self._namespace_stack[-1]

    def _get_type_or_namespace_from_fully_qualified_name(self, typename):
        if typename is None:
            return self.declared_types
        namespace_list = typename.split('::')
        t = self.declared_types
        for namespace in namespace_list:
            #add some error handling stuff here
            if not namespace in t:
                return None
            t = t[namespace]
        return t

    def _get_type_or_namespace(self, m_typename):
        namespace = self._get_current_namespace()
        while True:
            typename = m_typename
            if namespace is not None:
                typename = namespace.fully_qualified_name() + '::' + typename
            t = self._get_type_or_namespace_from_fully_qualified_name(typename)
            if t is not None:
                return t
            if namespace is None:
                break
            namespace = namespace.namespace
        return None

    def _add_namespace(self, new_namespace):
        namespace_name = new_namespace.fully_qualified_name()
        namespace_list = namespace_name.split('::')
        current_types = self.declared_types
        for namespace in namespace_list[:-1]:
            if not namespace in current_types:
                #namespace not found, error
                raise ParseError(new_namespace.coord,
                                 'Unknown namespace {0}'.format(namespace_name))
            current_types = current_types[namespace]
        final_namespace = namespace_list[-1]
        if not final_namespace in current_types:
            current_types[final_namespace] = dict()
        return current_types[final_namespace]

    def _add_type(self, production, index, m_type):
        """ Adds a new message type
        """
        fully_qualified_name = None
        current_namespace = self._get_current_namespace()
        if current_namespace is not None:
            fully_qualified_name = current_namespace.fully_qualified_name()
        namespace_types = self._get_type_or_namespace_from_fully_qualified_name(fully_qualified_name)
        if m_type.name in namespace_types:
            raise ParseError(self.production_to_coord(production, index),
                             "Name '{0}' already exists".format(m_type.fully_qualified_name()))
        namespace_types[m_type.name] = m_type

    def _is_type_in_scope(self, name):
        """ Is <name> a type in the current-scope?
        """

    def _lex_error_func(self, msg, token):
        self._parse_error(msg, self.token_to_coord(token))

    def _lex_type_lookup_func(self, name):
        """ Looks up types that were previously defined.
        """
        is_type = self._is_type_in_scope(name)

    def _get_yacc_lookahead_token(self):
        """ The last token yacc requested from the lexer.
        Saved in the lexer.
        """
        return self.lexer.last_token

    def _check_duplicates(self, decl, decl_type):
        all_members = dict()
        for member in decl.members():
            if member.name in all_members:
                raise ParseError(member.coord,
                    "{0} '{1}' has duplicate member {2}.".format(
                    decl_type,
                    decl.fully_qualified_name(),
                    member.name))
            all_members[member.name] = member

    def _check_variable_length_array(self, member, data_type, length_type):
        if length_type.name == 'bool':
            self._parse_error("{0} is a variable-length array with a bool length, which isn't supported.".format(member.name),
                              member.coord)
        if length_type.min < 0:
            # emitters (especially C++) don't necessarily deal well with signed length fields
            # I think the C++ issue is in SafeMessageBuffer if you need to fix it.
            # also, unsigned is more future proof
            self._parse_error("{0} uses a signed datatype for the length field ({1}). You must use an unsigned type.".format(member.name, length_type.name),
                              member.coord)
        if not (0 <= member.type.max_length <= length_type.max):
            self._parse_error("{0} specifies a max length that is not within the range of the length field ({1}).".format(member.name, length_type.name),
                              member.coord)

    def _check_message_member_initializer(self, member_type_ref, initializer, member_coord):
        # Is this is a string type initializer we can't check it (could also be a use of the verbatim keyword)
        if initializer.type is "str":
            return
        if not member_type_ref.type.name in ast.builtin_types:
            self._parse_error("{0} is not a built in data type".format(member_type_ref.type.name),
                              member_coord)
        if member_type_ref.type.name in ast.builtin_int_types:
            # int types require an int constant
            if not initializer.isInt:
                self._parse_error("{0} is an int type, but {1} is not".format(member_type_ref.type.name, initializer.value),
                                  member_coord)
        if member_type_ref.type.name in ast.builtin_float_types:
            if not initializer.isFloat:
                self._parse_error("{0} is a float type, but {1} is not".format(member_type_ref.type.name, initializer.value),
                                  member_coord)
        if (member_type_ref.type.min is not None) and (member_type_ref.type.min > initializer.value):
            self._parse_error("{0} is smaller than the minimum value ({1}) for type {2}".format(initializer.value, member_type_ref.type.min, member_type_ref.type.name),
                              member_coord)
        if (member_type_ref.type.max is not None) and (member_type_ref.type.max < initializer.value):
            self._parse_error("{0} is larger than the maximum value ({1}) for type {2}".format(initializer.value, member_type_ref.type.max, member_type_ref.type.name),
                              member_coord)
        if (member_type_ref.type.name is 'bool'):
            if (((member_type_ref.type.min is not None) and (initializer.value != member_type_ref.type.min)) and 
                ((member_type_ref.type.max is not None) and (initializer.value != member_type_ref.type.max))):
                self._parse_error("bool initializer value must be 0 or 1".format(member_type_ref.type.name),
                                  member_coord)

    #################################
    ###### Parser Productions #######
    #################################

    # Any new additions to CLAD syntax will require changes to this
    # list of productions.
    # Note: the comments are used by PLY to match rules against incoming tokens

    def p_start(self, p):
        """ start : decl_list
        """
        p[0] = p[1]

    def p_decl_list(self, p):
        """ decl_list : decl
                      | decl_list decl
        """
        if len(p) == 2:
            p[0] = ast.DeclList([p[1]], p[1].coord)
        else:
            p[0] = p[1].append(p[2])

    def p_decl(self, p):
        """ decl : namespace_decl
                 | message_decl
                 | enum_decl
                 | union_decl
                 | include_decl
                 | enum_concept_decl
        """
        p[0] = p[1]

    def p_include_decl(self, p):
        """ include_decl : include_begin decl_list RBRACE
                         | include_begin RBRACE
        """
        include = p[1]
        if len(p) >= 4:
            include.decl_list = p[2]
        p[0] = include

    def p_include_begin(self, p):
        """ include_begin : INCLUDE QUOTED_PATH LBRACE
        """
        new_include = ast.IncludeDecl(p[2][1:-1], None, self.production_to_coord(p, 2))
        p[0] = new_include

    def p_namespace_decl(self, p):
        """ namespace_decl : namespace_begin decl_list RBRACE
        """
        namespace = p[1]
        namespace.decl_list = p[2]
        self._namespace_stack.pop()
        p[0] = namespace

    def p_namespace_begin(self, p):
        """ namespace_begin : NAMESPACE ID LBRACE
        """
        # push namespace onto stack
        new_namespace = ast.NamespaceDecl(p[2], None,
                                          self.production_to_coord(p, 2),
                                          self._get_current_namespace())
        namespace_types = self._add_namespace(new_namespace)
        self._namespace_stack.append(new_namespace)
        p[0] = new_namespace

    def p_message_decl(self, p):
        """ message_decl : message_decl_begin ID LBRACE message_member_decl_list RBRACE
                         | message_decl_begin ID LBRACE message_member_decl_list COMMA RBRACE
                         | message_decl_begin ID LBRACE RBRACE
                         | NO_DEFAULT_CONSTRUCTOR message_decl_begin ID LBRACE message_member_decl_list RBRACE
                         | NO_DEFAULT_CONSTRUCTOR message_decl_begin ID LBRACE message_member_decl_list COMMA RBRACE
                         | NO_DEFAULT_CONSTRUCTOR message_decl_begin ID LBRACE RBRACE
        """
        default_constructor = p[1] != "no_default_constructor"
        
        # if no default constructor remove the word from the list(YaccProduction object)
        # so the later code is not affected by it
        if not default_constructor:
            p.pop(1)
        
        if len(p) >= 6:
            members = p[4]
        else:
            members = ast.MessageMemberDeclList([], self.production_to_coord(p, 3))

        is_structure = p[1] == "structure"
        p[0] = ast.MessageDecl(p[2], members,
                               self.production_to_coord(p, 2),
                               self._get_current_namespace(),
                               is_structure,
                               default_constructor)

        self._check_duplicates(p[0], p[1])

        type = ast.CompoundType(p[0].name, p[0], p[0].coord)
        if not is_structure:
            self._message_types.append(type)
        self._add_type(p, 2, type)

    def p_message_decl_begin(self, p):
        """ message_decl_begin : MESSAGE
                               | STRUCTURE
        """
        p[0] = p[1]

    def p_message_member_decl_list(self, p):
        """ message_member_decl_list : message_member_decl
                                     | message_member_decl_list COMMA message_member_decl
        """
        if len(p) == 2:
            p[0] = ast.MessageMemberDeclList([p[1]], p[1].coord)
        else:
            p[0] = p[1].append(p[3])

    def p_message_member_decl(self, p):
        """ message_member_decl : message_member
                                | message_variable_array_member
                                | message_fixed_array_member
        """
        self._all_members.append(p[1])
        p[0] = p[1]

    def p_message_member(self, p):
        """ message_member : type ID
                           | type ID EQ constant
        """
        coord = self.production_to_coord(p, 2)
        if len(p) > 4:
            self._check_message_member_initializer(p[1], p[4], coord)
            p[0] = ast.MessageMemberDecl(
                p[2], #name
                p[1].type, #type
                p[4], #initializer
                coord, #file / line number
            )
        else:
            p[0] = ast.MessageMemberDecl(
                p[2], #name
                p[1].type, #type
                None, #initializer
                coord #file / line number
            )

    def p_message_variable_array_member(self, p):
        """ message_variable_array_member : type ID LSQ builtin_int_type RSQ
                                          | type ID LSQ builtin_int_type COLON int_constant RSQ
        """
        if p[5] == ':':
            max_length = p[6].value
        else:
            max_length = None

        type = ast.VariableArrayType(p[1].type, p[4].type, max_length, self.production_to_coord(p, 1))
        p[0] = ast.MessageMemberDecl(p[2], type, None, self.production_to_coord(p, 1))
        self._check_variable_length_array(p[0], p[1].type, p[4].type)

    def p_message_fixed_array_member(self, p):
        """ message_fixed_array_member : type ID LSQ int_constant RSQ
                                       | type ID LSQ string_constant RSQ
        """
        if not isinstance(p[4].value, str) and p[4].value < 0:
            self._parse_error("Array index must be positive, got {0}".format(p[4].value),
                              self.production_to_coord(p, 1))
        type = ast.FixedArrayType(p[1].type, p[4].value, self.production_to_coord(p, 1))
        p[0] = ast.MessageMemberDecl(p[2], type, None, self.production_to_coord(p, 1))

    def p_enum_decl(self, p):
        """ enum_decl : ENUM builtin_type ID LBRACE enum_member_list RBRACE
                      | ENUM builtin_type ID LBRACE enum_member_list COMMA RBRACE
                      | ENUM NO_CPP_CLASS builtin_type ID LBRACE enum_member_list RBRACE
                      | ENUM NO_CPP_CLASS builtin_type ID LBRACE enum_member_list COMMA RBRACE
        """
        # if no_cpp_class was defined, all indices after 1 (not included) have an offset
        cpp_class = (p[2] != "no_cpp_class")
        index_offset = 0 if cpp_class else 1

        decl = ast.EnumDecl(p[3+index_offset], p[2+index_offset].type, p[5+index_offset],
                            cpp_class,
                            self.production_to_coord(p, 1),
                            self._get_current_namespace())
        decl_type = ast.DefinedType(decl.name, decl, self.production_to_coord(p, 1))
        self._add_type(p, 1, decl_type)
        p[0] = decl

        self._enum_decls.append(decl)

    def p_enum_member_list(self, p):
        """ enum_member_list : enum_member
                             | enum_member_list COMMA enum_member
        """
        if len(p) == 2:
            p[0] = ast.EnumMemberList([p[1]], p[1].coord)
        else:
            p[0] = p[1].append(p[3])

    def p_enum_member(self, p):
        """ enum_member : ID
                        | ID EQ int_constant
                        | ID EQ string_constant
        """
        value = p[3] if len(p) == 4 else None
            
        p[0] = ast.EnumMember(p[1], value, self.production_to_coord(p, 1))

    def p_union_decl_begin(self, p):
        """ union_decl_begin : UNION
                             | AUTOUNION
        """
        p[0] = p[1]

    def p_union_decl(self, p):
        """ union_decl : union_decl_begin ID LBRACE union_member_decl_list RBRACE
                       | union_decl_begin ID LBRACE union_member_decl_list COMMA RBRACE
                       | union_decl_begin ID LBRACE RBRACE
                       | union_decl_begin DUPES_ALLOWED ID LBRACE union_member_decl_list RBRACE
                       | union_decl_begin DUPES_ALLOWED ID LBRACE union_member_decl_list COMMA RBRACE
                       | union_decl_begin DUPES_ALLOWED ID LBRACE RBRACE
        """

        dupes_allowed = (p[2] == "dupes_allowed")
        is_explicit_auto_union = p[1] == "autounion"
        indexOffset = 1 if dupes_allowed else 0

        member_list = p[4+indexOffset] if len(p) > (5+indexOffset) else None
        decl = ast.UnionDecl(p[2+indexOffset], member_list,
                             self.production_to_coord(p, 2),
                             self._get_current_namespace(),
                             is_explicit_auto_union,
                             dupes_allowed)

        p[0] = decl

        if len(p) > 5:
            self._check_duplicates(p[0], 'union')

        self._union_decls.append(decl)
        type = ast.CompoundType(p[0].name, p[0], p[0].coord)
        self._add_type(p, 1, type)

    def p_union_member_decl_list(self, p):
        """ union_member_decl_list : union_member_decl
                                   | union_member_decl_list COMMA union_member_decl
        """
        if len(p) == 2:
            p[0] = ast.MessageMemberDeclList([p[1]], p[1].coord)
        else:
            p[0] = p[1].append(p[3])

    def p_union_member_decl(self, p):
        """ union_member_decl : union_member
                              | union_variable_array_member
                              | union_fixed_array_member
        """
        p[0] = p[1]

    def get_union_initializer(self, p, normal_length):
        if len(p) > normal_length:
            return p[normal_length + 1]
        else:
            return None

    def p_union_member(self, p):
        """ union_member : type ID
                         | type ID EQ int_constant
        """
        p[0] = ast.MessageMemberDecl(p[2],
                                     p[1].type,
                                     self.get_union_initializer(p, 3),
                                     self.production_to_coord(p, 1))
        self._all_members.append(p[0])

    def p_union_variable_array_member(self, p):
        """ union_variable_array_member : type ID LSQ builtin_int_type RSQ
                                        | type ID LSQ builtin_int_type COLON int_constant RSQ
                                        | type ID LSQ builtin_int_type RSQ EQ int_constant
                                        | type ID LSQ builtin_int_type COLON int_constant RSQ EQ int_constant
        """
        if p[5] == ':':
            max_length = p[6].value
            initializer = self.get_union_initializer(p, 8)
        else:
            max_length = None
            initializer = self.get_union_initializer(p, 6)

        type = ast.VariableArrayType(p[1].type, p[4].type, max_length, self.production_to_coord(p, 1))
        p[0] = ast.MessageMemberDecl(p[2], type, initializer, self.production_to_coord(p, 1))
        self._check_variable_length_array(p[0], p[1].type, p[4].type)
        self._all_members.append(p[0])

    def p_union_fixed_array_member(self, p):
        """ union_fixed_array_member : type ID LSQ int_constant RSQ
                                     | type ID LSQ int_constant RSQ EQ int_constant
        """
        if p[4].value < 0:
            self._parse_error("Array index must be positive, got {0}".format(p[4].value),
                              self.production_to_coord(p, 1))
        type = ast.FixedArrayType(p[1].type, p[4].value, self.production_to_coord(p, 1))
        p[0] = ast.MessageMemberDecl(p[2], type, self.get_union_initializer(p, 6), self.production_to_coord(p, 1))
        self._all_members.append(p[0])

    def p_enum_concept_decl(self, p):
        """ enum_concept_decl : ENUM_CONCEPT type ID LSQ ID RSQ LBRACE enum_concept_decl_list RBRACE
        """
        
        decl = ast.EnumConceptDecl(p[3], p[2], p[5], p[8],
                                    self.production_to_coord(p, 1),
                                    self._get_current_namespace())
        p[0] = decl

        self._check_duplicates(p[0], 'enum_concept')

        self._enum_concepts.append(decl)

    def p_enum_concept_decl_list(self, p):
        """ enum_concept_decl_list : enum_concept_member
                                   | enum_concept_decl_list COMMA enum_concept_member
        """
        if len(p) == 2:
            p[0] = ast.EnumConceptMemberList([p[1]], p[1].coord)
        else:
            p[0] = p[1].append(p[3])

    def p_enum_concept_member(self, p):
        """ enum_concept_member : ID EQ constant
        """
        p[0] = ast.EnumConceptMember(p[1], p[3], self.production_to_coord(p, 1))

    def p_type(self, p):
        """ type : string_type
                 | non_array_type
        """
        p[0] = p[1]

    def p_string_type(self, p):
        """ string_type : STRING
                        | STRING LSQ type RSQ
                        | STRING LSQ type COLON int_constant RSQ
        """
        if len(p) > 5:
            max_length = p[5].value
        else:
            max_length = None

        if len(p) == 2:
            length_type = 'uint_8'
        else:
            length_type = p[3].type.name
        if length_type not in ast.string_types:
            self._parse_error("{0} is not a valid string length type".format(length_type),
                              self.production_to_coord(p, 1))
        string_type = ast.PascalStringType(
            ast.string_types[length_type].length_type,
            max_length,
            self.production_to_coord(p, 1))
        p[0] = ast.TypeReference(string_type, string_type.coord)

    def p_non_array_type(self, p):
        """ non_array_type : builtin_type
                           | declared_type
        """
        p[0] = p[1]

    def p_builtin_type(self, p):
        """ builtin_type : builtin_float_type
                         | builtin_int_type
        """
        p[0] = p[1]

    def p_builtin_float_type(self,p):
        """ builtin_float_type : FLOAT_32
                               | FLOAT_64
        """
        p[0] = ast.TypeReference(ast.builtin_types[p[1]], self.production_to_coord(p, 1))

    def p_builtin_int_type(self, p):
        """ builtin_int_type : INT_8
                             | INT_16
                             | INT_32
                             | INT_64
                             | UINT_8
                             | UINT_16
                             | UINT_32
                             | UINT_64
                             | BOOL
        """
        p[0] = ast.TypeReference(ast.builtin_types[p[1]], self.production_to_coord(p, 1))

    def p_declared_type(self, p):
        """ declared_type : ID
        """
        t = self._get_type_or_namespace(p[1])
        if t is not None:
            if not isinstance(t, ast.Type):
                self._parse_error("{1} {0} is not a type.".format(typeof(t).__name__, t.fully_qualified_name()), self.production_to_coord(p, 1))
            p[0] = ast.TypeReference(t, self.production_to_coord(p, 1))
        else:
            self._parse_error("Unknown type: %s" % p[1], self.production_to_coord(p, 1))

    def p_constant(self, p):
        """ constant : int_constant
                     | float_constant
                     | string_constant
        """
        p[0] = p[1]

    def p_int_constant(self, p):
        """ int_constant : INT_CONST_DEC
                         | INT_CONST_HEX
        """
        p[0] = ast.IntConst(p[1].value, p[1].type, self.production_to_coord(p, 1))

    def p_float_constant(self, p):
        """ float_constant : FLOAT_CONST_DEC
        """
        p[0] = ast.FloatConst(p[1].value, p[1].type, self.production_to_coord(p, 1))
    
    def p_string_constant(self, p):
        """ string_constant : STRING_LITERAL
        """
        p[0] = ast.StringConst(p[1].value, self.production_to_coord(p, 1))
        
    def p_error(self, p):
        if p:
            self._parse_error("before '{0}'".format(p.value),
                              self.lineno_to_coord(p.lineno, self.lexer.find_tok_column(p.lexpos)))
        else:
            self._parse_error('At end of input.', self._lines_to_coords[-1])
