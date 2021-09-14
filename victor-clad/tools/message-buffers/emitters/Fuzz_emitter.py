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

import base64
import inspect
import os
import random as _random
import re
import struct
import sys
import textwrap

def _modify_path():
    currentpath = os.path.dirname(inspect.getfile(inspect.currentframe()))
    for path in [('..',), ('..', 'support', 'python')]:
        searchpath = os.path.join(currentpath, *path)
        searchpath = os.path.normpath(os.path.abspath(os.path.realpath(searchpath)))
        if searchpath not in sys.path:
            sys.path.insert(0, searchpath)
_modify_path()

import msgbuffers
from clad import ast
from clad import clad
from clad import emitterutil

_py3 = sys.version_info[0] >= 3

if _py3:
    xrange = range

def float_to_bits(char, bits):
    return struct.unpack('=' + ('L' if char == 'f' else 'Q'), struct.pack('=' + char, bits))[0]

def bits_to_float(char, bits):
    return struct.unpack('=' + char, struct.pack('=' + ('L' if char == 'f' else 'Q'), bits))[0]

max_f64 = bits_to_float('d', 0x7FEFFFFFFFFFFFFF)
max_f32 = bits_to_float('f', 0x7F7FFFFF)

low_sub_f64 = bits_to_float('d', 0x0000000000000001)
high_sub_f64 = bits_to_float('d', 0x000FFFFFFFFFFFFF)
low_f64 = bits_to_float('d', 0x0010000000000000)

low_sub_f32 = bits_to_float('f', 0x00000001)
high_sub_f32 = bits_to_float('f', 0x007FFFFF)
low_f32 = bits_to_float('f', 0x00400000)

class Fuzzer(object):

    def __init__(self, primitive_type):
        assert(primitive_type.type in ('int', 'float'))
        self.mode = primitive_type.type
        if primitive_type.type == 'int':
            self.is_f64 = False
            self.min = primitive_type.min
            self.max = primitive_type.max
        else:
            # high bit is sign, least significant exponent is 8 or 11 bits over
            if primitive_type.name == 'float_64':
                self.is_f64 = True
                self.min = -max_f64
                self.max = max_f64
            else:
                self.is_f64 = False
                self.min = -max_f32
                self.max = max_f32

            # also implicitly tests for NaN (any comparisons with NaN return False)
            infinity = float('inf')
            assert(-infinity < self.min < infinity)
            assert(-infinity < self.max < infinity)

        assert(self.min <= self.max)

        # Figure out fuzz_zero
        if self.min > 0:
            self.zero = self.min
        elif self.max < 0:
            self.zero = self.max
        else:
            self.zero = 0

        # Figure out fuzz_special
        self.specials = [self.zero]
        if self.mode == 'int':
            values = [0, self.min, self.max, 0xFF, -0x100, 0xFFFF, -0x10000, 0xFFFFFFFF, -0x100000000]
            modifiers = [(-1, 1), (0, 1), (1, 1)]
        elif self.mode == 'float':
            values = [0, self.max, max_f32, low_sub_f32, high_sub_f32, low_f32]
            modifiers = [(-1, 1), (0, 1), (1, 1), (0, 2), (0, .5)]
            if self.is_f64:
                values += [low_sub_f64, high_sub_f64, low_f64, max_f64]
                bit_modifiers = [0x0000000000000001, 0x0010000000000000, 0x0008000000000000, 0x4000000000000000]
            else:
                bit_modifiers = [0x00000001, 0x00400000, 0x00800000, 0x40000000]

            # add negatives
            for value in list(values):
                if -value not in values:
                    values.append(-value)

            # add bit modified versions
            for value in values:
                bits = float_to_bits('d' if self.is_f64 else 'f', value)
                for bit_modifier in bit_modifiers:
                    result = bits_to_float('d' if self.is_f64 else 'f', bits ^ bit_modifier)
                    if self.min <= result <= self.max and result not in self.specials:
                        self.specials.append(result)

        # add arithmetically-modified versions
        for value in values:
            if self.min <= value <= self.max and value not in self.specials:
                self.specials.append(value)
            for addend, multiplier in modifiers:
                result = value * multiplier + addend
                if self.min <= result <= self.max and result not in self.specials:
                    self.specials.append(result)

        # Figure out fuzz_small
        if self.mode == 'int':
            small_range = self.max - self.min
            small_range_bits = 0
            while small_range > 0:
                small_range >>= 1
                small_range_bits += 1
            self.small_low = max(self.min, self.zero - small_range_bits * 4)
            self.small_high = min(self.max, self.zero + small_range_bits * 4)

    def fuzz_zero(self):
        "Always return as close to 0 as possible."
        return self.zero

    def fuzz_uniform(self, random):
        "Uniform randomization."
        if self.mode == 'int':
            return random.randint(self.min, self.max)
        elif self.mode == 'float':
            return random.uniform(self.min, self.max)
        else:
            return self.fuzz_zero()

    def fuzz_special(self, random):
        "Fuzz from the set of special values."
        if self.specials:
            return self.specials[random.randrange(0, len(self.specials))]
        else:
            return self.fuzz_zero()

    def fuzz_small(self, random):
        "Fuzz within a small range of values."
        if self.mode == 'int':
            return random.randint(self.small_low, self.small_high)
        else:
            return self.fuzz_uniform(random)

    def fuzz_bits(self, random):
        if self.mode == 'float':
            if self.is_f64:
                bits = random.randint(0, 0xFFFFFFFFFFFFFFFF)
                char = 'd'
                bit = 0x0010000000000000
            else:
                bits = random.randint(0, 0xFFFFFFFF)
                char = 'f'
                bit = 0x00800000

            value = bits_to_float(char, bits)
            if not (self.min <= value <= self.max):
                value = bits_to_float(char, bits ^ bit)
            return value
        else:
            return self.fuzz_uniform(random)

    def fuzz_weighted(self, random, prefer_small=False):
        "Fuzz using a random one of the other fuzzers."
        # lots of zeroes
        if random.randrange(0, 3) == 0:
            return self.fuzz_zero()

        # prefer special tricky edge cases
        if random.randrange(0, 2) != 0 and not prefer_small:
            return self.fuzz_special(random)

        # use the full range
        if self.mode == 'float':
            return self.fuzz_bits(random)
        elif prefer_small:
            return self.fuzz_small(random)
        else:
            return self.fuzz_uniform(random)

_type_formats = {
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
_type_fuzzer = {}

def get_format(builtin_type):
    return _type_formats[builtin_type.name]

def get_fuzzer(builtin_type):
    if builtin_type not in _type_fuzzer:
        _type_fuzzer[builtin_type] = Fuzzer(builtin_type)
    return _type_fuzzer[builtin_type]

class FuzzWriter(ast.NodeVisitor):

    def __init__(self, writer, random, options):
        self.writer = writer
        self.random = random
        self.zero = (options.random_mode == 'zero')
        self.uniform = (options.random_mode == 'uniform')

    def fuzz(self, builtin_type, prefer_small=False, maximum=None):
        fuzzer = get_fuzzer(builtin_type)
        if self.zero:
            value = fuzzer.fuzz_zero()
        elif self.uniform:
            if prefer_small:
                value = fuzz.fuzz_small(self.random)
            else:
                value = fuzzer.fuzz_uniform(self.random)
        else:
            value = fuzzer.fuzz_weighted(self.random, prefer_small)
        if maximum is not None:
            value = min(maximum, value)
        self.writer.write(value, get_format(builtin_type))
        return value

    def visit_BuiltinType(self, node):
        self.fuzz(node, False)

    def visit_DefinedType(self, node):
        self.visit(node.underlying_type)

    def visit_CompoundType(self, node):
        self.visit(node.type_decl)

    def visit_FixedArrayType(self, node):
        for i in xrange(node.length):
            self.visit(node.member_type)

    def visit_VariableArrayType(self, node):
        length = self.fuzz(node.length_type, True, maximum=node.max_length - 1)
        for i in xrange(length):
            self.visit(node.member_type)

    def visit_PascalStringType(self, node):
        # TODO: Unicode fuzzing?
        length = self.fuzz(node.length_type, True, maximum=node.max_length - 1)
        for i in xrange(length):
            if self.zero:
                value = 0
            else:
                value = self.random.randint(1, 127)
            self.writer.write(value, get_format(node.member_type))

    def visit_EnumDecl(self, node):
        self.visit(node.storage_type)

    def visit_MessageDecl(self, node):
        for member in node.members():
            self.visit(member.type)

    def visit_UnionDecl(self, node):
        tags = [member.tag for member in node.members()]
        if tags:
            if self.zero:
                tag = tags[0]
            else:
                tag = tags[self.random.randrange(len(tags))]
        else:
            tag = node.invalid_tag
        self.writer.write(tag, get_format(node.tag_storage_type))
        if tags:
            self.visit(node.members_by_tag[tag])

class FuzzEmitter(ast.NodeVisitor):

    def __init__(self, binary_writer, random, options, search, full_type=True):
        self.fuzz_writer = FuzzWriter(binary_writer, random, options)
        self.search = search
        self.full_type = full_type
        self.found = None

    def emit(self, fuzz_type):
        self.fuzz_writer.visit(fuzz_type)
        self.found = fuzz_type

    def visit_Decl_subclass(self, node):
        if not self.found:
            # hack: can't have prefix in compare
            node.possibly_ambiguous = False

            name = node.fully_qualified_name() if self.full_type else node.name
            if self.search == name:
                if isinstance(node, ast.NamespaceDecl):
                    emitterutil.exit_at_coord(node.coord, 'Cannot fuzz for namespace {0}.'.format(self.full_type))
                else:
                    self.emit(node)
            else:
                self.generic_visit(node)

def permissive_identifier(text):
    text = '::'.join(re.findall(r'[a-zA-Z_][0-9a-zA-Z_]*', text))
    if not text:
        raise ValueError('Need to specify an identifier.')
    return text

if __name__ == '__main__':
    from clad import emitterutil

    description = 'Generate random output for the specified clad type. Outputs the type name plus newline, then the random seed plus newline, then the fuzz.'
    option_parser = emitterutil.SimpleArgumentParser(description=description)
    option_parser.add_argument('fuzz_type', type=permissive_identifier,
        help='The name of the type to create random output for.')
    option_parser.add_argument('-o', '--output-file', default='-', metavar='file',
        help='The file path to write the fuzz output to (defaults to stdout).')
    option_parser.add_argument('--random-mode', default='weighted', choices=['zero', 'uniform', 'weighted'],
        help='The distribution: zero (as close to all 0s as possible), uniformly random, or weighted toward common values. Uniform may lead to especially long outputs.')
    option_parser.add_argument('--random-seed', type=base64.b64decode,
        help='The Base 64 seed to use for a randomness source.')
    options = option_parser.parse_args()

    if not options.random_seed:
        try:
            seed = os.urandom(2500)
        except NotImplementedError:
            import time
            seed = int(time.time() * 256) # use fractional seconds

    random = _random.Random(seed)

    tree = emitterutil.parse(options)

    writer = msgbuffers.BinaryWriter()
    emitter = FuzzEmitter(writer, random, options, search=options.fuzz_type, full_type=True)
    if options.fuzz_type in ast.builtin_types:
        emitter.emit(ast.builtin_types[options.fuzz_type])
    else:
        emitter.visit(tree)
        if not emitter.found:
            emitter = FuzzEmitter(writer, random, options, search=options.fuzz_type, full_type=False)
            emitter.visit(tree)

    if not emitter.found:
        sys.exit("Could not find type '{0}'.".format(options.fuzz_type))

    with emitterutil.get_output(os.getcwd(), options.output_file, binary=True) as output:
        output.write(emitter.found.fully_qualified_name().encode('utf-8'))
        output.write('\n'.encode('utf-8'))
        output.write(base64.b64encode(seed))
        output.write('\n'.encode('utf-8'))
        output.write(writer.dumps())
