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
CLAD parser AST types
heavily based on c_ast.py in py_c_parser
"""

from __future__ import absolute_import
from __future__ import print_function

import os.path
import sys
import struct

try:
    from StringIO import StringIO
except:
    import io
    class StringIO(io.StringIO):
        "Override StringIO class with one which wraps output with an encode for python2/3 compatibility"
        def getvalue(self):
            return io.StringIO.getvalue(self).encode("ASCII")

import hashlib

from . import plyparser

class DefaultQualifiedNamer(object):

    @classmethod
    def join(cls, *args):
        return '::'.join(args)

    @classmethod
    def disambiguate(cls, qualified_name):
        return '::' + qualified_name

class Node(object):
    """ Base object for AST Nodes.
    """
    def __init__(self, coord):
        self.coord = coord

    def children(self):
        """ A sequence of all children that are Nodes
        """
        return []

    def show(self, output=sys.stdout, offset=0,
             nodenames=False, attrnames=False, showcoord=False, _node_name=None):
        """ Print the Node and its attributes to a buffer.
        TODO: describe all of the kw_args
        """
        lead = ' ' * offset
        if nodenames and _node_name is not None:
            output.write(lead + self.__class__.__name__ + ' <' + _node_name + '>: ')
        else:
            output.write(lead + self.__class__.__name__ + ': ')

        if self.attr_names:
            if attrnames:
                nvlist = [(n, getattr(self, n) if not callable(getattr(self, n))
                           else getattr(self, n)()) for n in self.attr_names]
                attrstr = ', '.join('%s=%s' % nv for nv in nvlist)
            else:
                vlist = [getattr(self, n) if not callable(getattr(self, n))
                           else getattr(self, n)() for n in self.attr_names]
                attrstr = ', '.join('%s' % v for v in vlist)
            output.write(attrstr)

        if showcoord:
            output.write(' (at %s)' % self.coord)
            output.write('\n')

class NodeVisitor(object):
    """ A base Node Visitor class for visiting ast nodes.
        Subclass this and define your own visit_XXX methods,
        where XXX is the class name you want to visit with these methods.
    """
    def visit(self, node, *args, **kwargs):
        """ Visit a node.
        """
        method = 'visit_' + node.__class__.__name__
        visitor = getattr(self, method, None)
        if visitor is not None:
            return visitor(node, *args, **kwargs)
        else:
            for base in node.__class__.__bases__:
                method = 'visit_' + base.__name__ + '_subclass'
                visitor = getattr(self, method, None)
                if visitor is not None:
                    return visitor(node, *args, **kwargs)
            return self.generic_visit(node, *args, **kwargs)

    def generic_visit(self, node, *args, **kwargs):
        """ Called if no explicit visitor function exists for a node.
            Implements preorder visiting of the node.
        """
        for c_name, c in node.children():
            self.visit(c, *args, **kwargs)

# Concrete node subclasses

class DeclList(Node):
    def __init__(self, decl_list, coord):
        super(DeclList, self).__init__(coord)
        self.decl_list = decl_list

    def children(self):
        nodelist = []
        for i, child in enumerate(self.decl_list or []):
            nodelist.append(("decl[%d]" % i, child))
        return tuple(nodelist)

    def append(self, decl):
        self.decl_list.append(decl)
        return self

    def remove(self, decl):
        self.decl_list.remove(decl)

    def length(self):
        return len(self.decl_list)

    attr_names = ()

class Decl(Node):
    def __init__(self, name, coord, namespace=None):
        super(Decl, self).__init__(coord)
        self.name = name
        self.coord = coord
        self._namespace = namespace
        self.hash_str = "Invalid"
        self.possibly_ambiguous = False


    attr_names = tuple(['hash_str', 'possibly_ambiguous'])

    def relative_qualified_name(self, base_namespace, namer=DefaultQualifiedNamer):
        "The fully-qualified name relative to the given namespace."
        if self.namespace == base_namespace:
            return self.name
        elif self.namespace:
            return self.namespace.relative_qualified_name(base_namespace, namer=namer) + namer.join(separator, self.name)
        else:
            raise ValueError('Node not in namespace {0}.'.format(base_namespace.name))

    def fully_qualified_name(self, namer=DefaultQualifiedNamer):
        """ List of namespaces, with the most specific being first """
        result = self.name
        if self.namespace is not None:
            result = namer.join(self.namespace.fully_qualified_name(namer=namer), result)
        if self.possibly_ambiguous:
            result = namer.disambiguate(result)
        return result

    @property
    def namespace(self):
        return self._namespace

    @namespace.setter
    def namespace(self, value):
        self._namespace = value

    def namespace_list(self):
        if self.namespace:
            return self.namespace.get_hierarchy_list()
        else:
            return []

    def __str__(self):
        return self.fully_qualified_name()

class NamespaceDecl(Decl):
    def __init__(self, name, decl_list, coord, namespace=None):
        super(NamespaceDecl, self).__init__(name, coord=coord, namespace=namespace)
        self.decl_list = decl_list
        self.possibly_ambiguous = False

    def __contains__(self, decl):
        while decl:
            decl = decl.namespace
            if self == decl:
                return True
        return False

    def __eq__(self, other):
        if type(self) is type(other):
            return self.name == other.name and self.namespace == other.namespace
        else:
            return NotImplemented

    def __ne__(self, other):
        if type(self) is type(other):
            return not self.__eq__(other)
        else:
            return NotImplemented

    def children(self):
        nodelist = []
        if self.decl_list is not None: nodelist.append(("members", self.decl_list))
        return tuple(nodelist)

    def members(self):
        return self.decl_list.members

    def get_hierarchy_list(self):
        backwards_list = []
        current = self
        while current:
            backwards_list.append(current)
            current = current.namespace
        return reversed(backwards_list)

    attr_names = tuple(['name', 'fully_qualified_name'])

    def __str__(self):
        return self.fully_qualified_name()

class IncludeDecl(Node):
    def __init__(self, name, decl_list, coord):
        super(IncludeDecl, self).__init__(coord)
        self.name = name
        self.decl_list = decl_list

    def children(self):
        nodelist = []
        if self.decl_list is not None: nodelist.append(("members", self.decl_list))
        return tuple(nodelist)

    def members(self):
        return self.decl_list.members

    attr_names = tuple(['name'])

    def __str__(self):
        return self.name

class MessageDecl(Decl):
    def __init__(self, name, decl_list, coord, namespace=None, is_structure=False, default_constructor=True):
        super(MessageDecl, self).__init__(name, coord=coord, namespace=namespace)
        self.decl_list = decl_list
        self._is_structure = is_structure
        self.default_constructor = default_constructor
        self.hash_str = "None"

    def children(self):
        nodelist = []
        if self.decl_list is not None: nodelist.append(("members", self.decl_list))
        return tuple(nodelist)

    def members(self):
        return self.decl_list.members

    def alignment(self):
        "The alignment requirements on the message when it is a C struct."
        if self.members():
            return max(member.type.alignment() for member in self.members())
        else:
            return 1

    def __message_size_helper(self, accumulator):
        if self.members():
            sumList = []
            sum = 0
            for member in self.members():
                size = getattr(member.type, accumulator)()
                if isinstance(size, list):
                    sumList += size
                else:
                    sum += size
        
            if len(sumList) > 0:
                sumList.append(sum)
                return sumList
            return sum
        else:
            return 0

    def max_message_size(self):
        """The maximum size that the transmitted message can possibly be.
        Returns a number or a list of elements that when summed represent the max message size"""
        return self.__message_size_helper("max_message_size");

    def min_message_size(self):
        """The minimum size that the transmitted message could possibly be.
        I.e. the number of bytes that must be read in order to know how big the structure will be.
        Returns a number or a list of elements that when summed represent the min message size"""
        return self.__message_size_helper("min_message_size")
    
    def is_message_size_fixed(self):
        "Returns true if the transmitted message will always be the same size."
        if self.members():
            return all(member.type.is_message_size_fixed() for member in self.members())
        else:
            return True

    def are_all_representations_valid(self):
        if self.members():
            return all(member.type.are_all_representations_valid() for member in self.members())
        else:
            return True

    def object_type(self):
        return "structure" if self._is_structure else "message"

    attr_names = tuple(['name', 'fully_qualified_name', 'hash_str'])

class MessageMemberDeclList(Node):
    def __init__(self, members, coord):
        super(MessageMemberDeclList, self).__init__(coord)
        self.members = members

    def children(self):
        nodelist = []
        for i, child in enumerate(self.members or []):
            nodelist.append(("member[%d]" % i, child))
        return tuple(nodelist)

    def append(self, member):
        self.members.append(member)
        return self

    def length(self):
        return len(self.members)

    attr_names = tuple(['length'])


class MessageMemberDecl(Node):
    def __init__(self, name, type, init, coord):
        super(MessageMemberDecl, self).__init__(coord)
        self.name = name
        self.type = type
        self.init = init
        self.tag = None

    def children(self):
        nodelist = []
        if self.type is not None: nodelist.append(("type", self.type))
        if self.init is not None: nodelist.append(("init", self.init))
        return tuple(nodelist)

    attr_names = ('name', 'type')

    def __str__(self):
        return self.name

class EnumDecl(Decl):
    def __init__(self, name, storage_type, member_list, cpp_class, coord, namespace=None):
        super(EnumDecl, self).__init__(name, coord=coord, namespace=namespace)
        self.storage_type = storage_type
        self.member_list = member_list or []
        self.cpp_class = cpp_class
        self.hash_str = "None"

    def children(self):
        nodelist = [("member_list", self.member_list)]
        return tuple(nodelist)

    def members(self):
        return self.member_list.members

    def alignment(self):
        return self.storage_type.alignment()

    def max_message_size(self):
        return self.storage_type.max_message_size()

    def min_message_size(self):
        return self.storage_type.min_message_size()

    def is_message_size_fixed(self):
        return self.storage_type.is_message_size_fixed()

    def are_all_representations_valid(self):
        return self.storage_type.are_all_representations_valid()

    attr_names = tuple(['name', 'fully_qualified_name', 'storage_type', 'hash_str'])

class EnumMemberList(Node):
    def __init__(self, members, coord):
        super(EnumMemberList, self).__init__(coord)
        self.members = members

    def children(self):
        nodelist = []
        for i, child in enumerate(self.members):
            nodelist.append(("member[%d]" % i, child))
        return tuple(nodelist)

    def append(self, member):
        self.members.append(member)
        return self

    attr_names = ()

class EnumMember(Node):
    def __init__(self, name, initializer, coord):
        super(EnumMember, self).__init__(coord)
        self.name = name
        self.initializer = initializer
        self.value = None
        self.is_duplicate = False

    def children(self):
        nodelist = []
        if self.initializer:
            nodelist.append(("initializer", self.initializer))
        return tuple(nodelist)

    attr_names = tuple(["name"])

class UnionDecl(Decl):
    def __init__(self, name, member_list, coord, namespace=None, is_explicit_auto_union=False, dupes_allowed=False):
        super(UnionDecl, self).__init__(name, coord=coord, namespace=namespace)
        self.member_list = member_list
        self.tag_storage_type = builtin_types['uint_8']
        self.hash_str = "None"
        self._is_explicit_auto_union = is_explicit_auto_union
        self._dupes_allowed = dupes_allowed
    
    def children(self):
        nodelist = [("member_list", self.member_list)]
        return tuple(nodelist)

    def members(self):
        return self.member_list.members

    invalid_tag = 0xFF

    @property
    def members_by_tag(self):
        return dict((member.tag, member) for member in self.members())

    def alignment(self):
        "The alignment requirements on the message when it is a C struct."
        if self.members():
            alignment = max(member.type.alignment() for member in self.members())
            return max(alignment, self.tag_storage_type.alignment())
        else:
            return self.tag_storage_type.alignment()

    def max_message_size(self):
        "The maximum size that the transmitted message can possibly be."
        if self.members():
            return self.tag_storage_type.max_message_size() + max(member.type.max_message_size() for member in self.members())
        else:
            return self.tag_storage_type.max_message_size()

    def min_message_size(self):
        "The minimum size that the transmitted message can possibly be."
        if self.members():
            return self.tag_storage_type.min_message_size() + min(member.type.min_message_size() for member in self.members())
        else:
            return self.tag_storage_type.min_message_size()

    def is_message_size_fixed(self):
        "Returns true if the transmitted message will always be the same size."
        if self.members():
            size = self.members()[0].type.max_message_size()
            for member in self.members():
                if not member.type.is_message_size_fixed():
                    return False
                if member.type.max_message_size() != size:
                    return False
        return self.tag_storage_type.is_message_size_fixed()

    def are_all_representations_valid(self):
        "Can always be invalid, because the union tag can be the invalid tag."
        return False
    
    def is_explicit_auto_union(self):
        return self._is_explicit_auto_union

    @property
    def dupes_allowed(self):
        return self._dupes_allowed

    attr_names = tuple(["name", "fully_qualified_name", "alignment", "hash_str"])

class EnumConceptDecl(Decl):
    def __init__(self, name, return_type, enum, member_list, coord, namespace=None):
        super(EnumConceptDecl, self).__init__(name, coord=coord, namespace=namespace)
        self.return_type = return_type
        self.enum = enum
        self.member_list = member_list or []
        self.hash_str = "None"

    def children(self):
        nodelist = [("member_list", self.member_list)]
        return tuple(nodelist)

    def members(self):
        return self.member_list.members

    def return_type_as_string(self):
        return self.return_type.type

    attr_names = tuple(['name', 'fully_qualified_name', 'return_type_as_string', 'enum', 'hash_str'])

class EnumConceptMemberList(Node):
    def __init__(self, members, coord):
        super(EnumConceptMemberList, self).__init__(coord)
        self.members = members

    def children(self):
        nodelist = []
        for i, child in enumerate(self.members):
            nodelist.append(("member[%d]" % i, child))
        return tuple(nodelist)

    def append(self, member):
        self.members.append(member)
        return self

    attr_names = ()

class EnumConceptMember(Node):
    def __init__(self, name, value, coord):
        super(EnumConceptMember, self).__init__(coord)
        self.name = name
        self.value = value

    def children(self):
        nodelist = []
        nodelist.append(("value", self.value))
        return tuple(nodelist)

    attr_names = tuple(["name"])

##### Types #####
class Type(Node):
    def __init__(self, name, coord):
        super(Type, self).__init__(coord)
        self.name = name

        self._possibly_ambiguous = False

    def builtin_type(self):
        return None

    def children(self):
        return tuple([])

    def alignment(self):
        "The alignment requirements on the type in C."
        return None

    def max_message_size(self):
        "The maximum size that the transmitted type can possibly be."
        return None

    def min_message_size(self):
        "The minimum size that the transmitted type can possibly be."
        return None

    def is_message_size_fixed(self):
        "Returns true if the transmitted type will always be the same size."
        return False

    def are_all_representations_valid(self):
        "Returns true if all bit representations of this type are considered valid input."
        return False

    def fully_qualified_name(self, namer=DefaultQualifiedNamer):
        result = self.name
        if self.possibly_ambiguous:
            result = namer.disambiguate(result)
        return result

    @property
    def possibly_ambiguous(self):
        return self._possibly_ambiguous

    @possibly_ambiguous.setter
    def possibly_ambiguous(self, value):
        self._possibly_ambiguous = value

    def __str__(self):
        return self.name

class BuiltinType(Type):
    # to be used for builtin stuff like ints and floats
    def __init__(self, name, type, size, coord, unsigned=False, min=None, max=None):
        super(BuiltinType, self).__init__(name, coord)
        self.size = size
        self.type = type
        self.unsigned = unsigned
        self.min = min
        self.max = max

    def builtin_type(self):
        return self

    def alignment(self):
        return self.size

    def max_message_size(self):
        return self.size

    def min_message_size(self):
        return self.size

    def is_message_size_fixed(self):
        return True

    def are_all_representations_valid(self):
        return True

    attr_names = tuple(["name", "size", "type", "unsigned", "min", "max"])

class VariableArrayType(Type):
    def __init__(self, member_type, length_type, max_length, coord):
        super(VariableArrayType, self).__init__('%s[%s]' % (member_type.name, length_type.name), coord)
        self.member_type = member_type
        self.length_type = length_type
        if max_length is not None:
            self.max_length = max_length
            self.max_length_is_specified = True
        else:
            self.max_length = length_type.max
            self.max_length_is_specified = False

    def builtin_type(self):
        return self.member_type.builtin_type()

    def children(self):
        nodelist = [
            ("member_type", self.member_type),
            ("length_type", self.length_type)
        ]
        return tuple(nodelist)

    def alignment(self):
        if self.max_length == 0:
            return self.length_type.alignment()
        else:
            return max(self.length_type.alignment(), self.member_type.alignment())

    def max_message_size(self):
        return self.length_type.max_message_size() + self.max_length * self.member_type.max_message_size()

    def min_message_size(self):
        return self.length_type.min_message_size()

    def is_message_size_fixed(self):
        if self.max_length == 0:
            return True
        else:
            return False

    def are_all_representations_valid(self):
        "Can be invalid if length is above max."
        return (self.length_type.max == self.max_length and
            self.length_type.min == 0 and
            self.member_type.are_all_representations_valid())

    attr_names = tuple(["length_type"])

# String type is just a variable array, member type must be uint_8
class PascalStringType(VariableArrayType):
    def __init__(self, length_type, max_length, coord):
        super(PascalStringType, self).__init__(builtin_types['uint_8'], length_type, max_length, coord)
        if length_type.type == 'uint_8':
            self.name = 'string'
        else:
            self.name = 'string[%s]' % length_type.name

    def are_all_representations_valid(self):
        "Is invalid if there are null bytes or invalid UTF-8, or if length is above max."
        return False

class FixedArrayType(Type):
    def __init__(self, member_type, length, coord):
        super(FixedArrayType, self).__init__('%s[%s]' % (member_type.name, length), coord)
        self.member_type = member_type
        self.length = length

    def builtin_type(self):
        return self.member_type.builtin_type()

    def children(self):
        nodelist = [
            ("member_type", self.member_type),
        ]
        return nodelist

    def alignment(self):
        if self.length == 0:
            return 1
        else:
            return self.member_type.alignment()

    def max_message_size(self):
        """Returns either a number or a list of elements that when summed represent the max message size"""
        max_message_size = self.member_type.max_message_size()
        if isinstance(self.length, str):
            # A list where all the elements are self.length and there will be max_message_size
            # occurences
            return [self.length] * max_message_size
        return max_message_size * self.length

    def min_message_size(self):
        """Returns either a number or a list of elements that when summed represent the min message size"""
        min_message_size = self.member_type.min_message_size()
        if isinstance(self.length, str):
            # A a list where all the elements are self.length and there will be min_message_size
            # occurences
            return [self.length] * min_message_size
        return min_message_size * self.length

    def is_message_size_fixed(self):
        if self.length == 0:
            return True
        else:
            return self.member_type.is_message_size_fixed()

    def are_all_representations_valid(self):
        "Is invalid if there are null bytes or invalid UTF-8, or if length is above max."
        if self.length == 0:
            return True
        else:
            return self.member_type.are_all_representations_valid()

    attr_names = tuple(["length"])

class DefinedType(Type):
    # to be used for aliases and enums
    def __init__(self, name, underlying_type, coord):
        super(DefinedType, self).__init__(name, coord)
        self.underlying_type = underlying_type
        self.size = underlying_type.storage_type.size

    def builtin_type(self):
        if self.underlying_type:
            return self.underlying_type.storage_type.builtin_type()
        return None

    def children(self):
        nodelist = []
        return tuple(nodelist)

    def alignment(self):
        return self.underlying_type.alignment()

    def max_message_size(self):
        return self.underlying_type.max_message_size()

    def min_message_size(self):
        return self.underlying_type.min_message_size()

    def is_message_size_fixed(self):
        return self.underlying_type.is_message_size_fixed()

    def are_all_representations_valid(self):
        return self.underlying_type.are_all_representations_valid()

    def relative_qualified_name(self, base_namespace, namer=DefaultQualifiedNamer):
        "The fully-qualified name relative to the given namespace."
        return self.underlying_type.relative_qualified_name(base_namespace, namer=namer)

    def fully_qualified_name(self, namer=DefaultQualifiedNamer):
        return self.underlying_type.fully_qualified_name(namer=namer)

    @property
    def possibly_ambiguous(self):
        return self.underlying_type.possibly_ambiguous

    @possibly_ambiguous.setter
    def possibly_ambiguous(self, value):
        self.underlying_type.possibly_ambiguous = value

    @property
    def namespace(self):
        return self.underlying_type.namespace

    attr_names = tuple(['name'])

class CompoundType(Type):
    # to be used for structs and unions
    def __init__(self, name, type_decl, coord):
        super(CompoundType, self).__init__(name, coord)
        self.type_decl = type_decl

    def children(self):
        nodelist = []
        return tuple(nodelist)

    def alignment(self):
        return self.type_decl.alignment()

    def max_message_size(self):
        return self.type_decl.max_message_size()

    def min_message_size(self):
        return self.type_decl.min_message_size()

    def is_message_size_fixed(self):
        return self.type_decl.is_message_size_fixed()

    def are_all_representations_valid(self):
        return self.type_decl.are_all_representations_valid()

    def relative_qualified_name(self, base_namespace, namer=DefaultQualifiedNamer):
        return self.type_decl.relative_qualified_name(base_namespace, namer=namer)

    def fully_qualified_name(self, namer=DefaultQualifiedNamer):
        return self.type_decl.fully_qualified_name(namer=namer)

    @property
    def possibly_ambiguous(self):
        return self.type_decl.possibly_ambiguous

    @possibly_ambiguous.setter
    def possibly_ambiguous(self, value):
        self.type_decl.possibly_ambiguous = value

    @property
    def namespace(self):
        return self.type_decl.namespace

    attr_names = tuple(['name'])

class TypeReference(Node):

    def __init__(self, type, coord):
        super(TypeReference, self).__init__(coord)
        self.type = type

    attr_names = tuple(['type'])

builtin_coord = plyparser.Coord("__builtin__")

#Fill out builtin types
#concrete builtin types

builtin_uint_types = {
    "uint_8":   BuiltinType("uint_8",  "int", 1, unsigned=True, min=0, max=2**8-1,  coord=builtin_coord),
    "uint_16":  BuiltinType("uint_16", "int", 2, unsigned=True, min=0, max=2**16-1, coord=builtin_coord),
    "uint_32":  BuiltinType("uint_32", "int", 4, unsigned=True, min=0, max=2**32-1, coord=builtin_coord),
    "uint_64":  BuiltinType("uint_64", "int", 8, unsigned=True, min=0, max=2**64-1, coord=builtin_coord),
}

builtin_int_types = {
    "int_8":   BuiltinType("int_8",  "int", 1, min=-2**7,  max=2**7-1,  coord=builtin_coord),
    "int_16":  BuiltinType("int_16", "int", 2, min=-2**15, max=2**15-1, coord=builtin_coord),
    "int_32":  BuiltinType("int_32", "int", 4, min=-2**31, max=2**31-1, coord=builtin_coord),
    "int_64":  BuiltinType("int_64", "int", 8, min=-2**63, max=2**63-1, coord=builtin_coord),
}
# builtin_int_types also include uint_types
for name, uint_type in builtin_uint_types.items():
    builtin_int_types[name] = uint_type

builtin_float_types = {
    "float_32":     BuiltinType("float_32", "float", 4, coord=builtin_coord),
    "float_64":     BuiltinType("float_64", "float", 8, coord=builtin_coord),
}

builtin_types = {
    "bool":    BuiltinType("bool", "int", 1, min=0, max=1, coord=builtin_coord),
}
#Add Float and Int types to builtin_types
for name, int_type in builtin_int_types.items():
    builtin_types[name] = int_type
for name, float_type in builtin_float_types.items():
    builtin_types[name] = float_type


#builtin 8bit, 16bit and 32bit length string types
string_length_types = [
    'uint_8', 'uint_16', 'uint_32'
]
string_types = dict()
for length_type in string_length_types:
    string_types[length_type] = PascalStringType(builtin_types[length_type], None, coord=builtin_coord)

class IntConst(Node):
    def __init__(self, value, type, coord):
        self.value = value
        self.type = type
        self.coord = coord
        self.isInt = True
        self.isFloat = False

    def children(self):
        return tuple()

    attr_names = tuple(["value", "type"])

class FloatConst(Node):
    def __init__(self, value, type, coord):
        self.value = value
        self.type = type
        self.coord = coord
        self.isInt = False
        self.isFloat = True

    def children(self):
        return tuple()

    attr_names = tuple(["value", "type"])

class StringConst(Node):
    def __init__(self, value, coord):
        self.value = value
        self.type = "str"
        self.coord = coord
        self.isInt = False
        self.isFloat = False
    
    def children(self):
        return tuple()
    
    attr_names = tuple(["value", "type"])

class ASTNamespaceWrapper(NodeVisitor):
    def __init__(self, wrapper_ns):
        self.wrapper_ns = wrapper_ns
        self.wrapped_namespace = False
        self.parent_decl_list = None

    def visit_IncludeDecl(self, node, *args, **kwargs):
        # ignore includes, we only wrap namespaces in the current file
        pass

    def generic_visit(self, node):
        if node:
            if isinstance(node, DeclList):
                # keep track of the current DeclList
                self.parent_decl_list = node
            if isinstance(node, NamespaceDecl):
                # if this is the first namespace then process it
                if not self.wrapped_namespace:
                    # Create a new DeclList wrapper for the current Namespace
                    wrapper_decl_list = DeclList([], None)
                    wrapper_decl_list.append(node)

                    # Create a new namespace containing the current Namespace
                    wrapper_ns = NamespaceDecl(self.wrapper_ns, wrapper_decl_list, None)

                    # Set the nodes (current ns) "parent" namespace to be our wrapper namespace
                    # This will ensure correct "fully qualified namespace" generation
                    node.namespace = wrapper_ns
                    self.wrapped_namespace = True

                    # Replace the current namespace with our wrapper
                    self.parent_decl_list.remove(node)
                    self.parent_decl_list.append(wrapper_ns)

            for (name, child) in node.children():
                self.visit(child)

# DEBUG emitter
class ASTDebug(NodeVisitor):
    def __init__(self):
        self.depth = 0
        self.node_name = None

    def generic_visit(self, node):
        if node:
            node.show(output=sys.stdout,
                      offset=self.depth,
                      _node_name=self.node_name,
                      nodenames=True,
                      attrnames=True,
                      showcoord=True)

            self.depth = self.depth + 1
            for (name, child) in node.children():
                self.node_name = name
                self.visit(child)
            self.node_name = None
            self.depth = self.depth - 1

class ASTHash(NodeVisitor):
    def __init__(self):
        self.depth = 0
        self.node_name = None
        self.output = StringIO()

    def generic_visit(self, node):
        if node:
            node.show(output=self.output,
                      offset=0,
                      _node_name=self.node_name,
                      nodenames=True,
                      attrnames=True,
                      showcoord=False)

            self.depth = self.depth + 1
            for (name, child) in node.children():
                self.node_name = name
                self.visit(child)
            self.node_name = None
            self.depth = self.depth - 1

    @property
    def hash_str(self):
        m = hashlib.md5()
        m.update(self.output.getvalue())
        return m.hexdigest()
