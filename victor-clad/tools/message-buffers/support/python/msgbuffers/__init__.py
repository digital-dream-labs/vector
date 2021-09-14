# Copyright (c) 2016 Anki, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License in the file LICENSE.txt or at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Message buffer support module for generated code.
"""

from __future__ import absolute_import
from __future__ import print_function

import argparse
import struct
import sys

_py3 = sys.version_info[0] >= 3
_struct_cache = dict()

if _py3:
    xrange = range
    unicode = str

def _get_struct(format, length):
    key = (format, length)
    if key in _struct_cache:
        return _struct_cache[key]
    else:
        reader = struct.Struct('<{0}{1}'.format(length, format))
        _struct_cache[key] = reader
        return reader

class ReadError(Exception):
    "An exception that occurs when the buffer length is wrong."
    pass

class BinaryReader(object):
    "Used to read in a stream of binary data a buffer, keeping track of the current position."

    def __init__(self, buffer):
        self._buffer = buffer
        self._index = 0

    def __len__(self):
        return len(self._buffer)

    def tell(self):
        "Returns the current stream position as an offset within the buffer."
        return self._index

    def read(self, format):
        "Reads in a single value of the given format."
        return self.read_farray(format, 1)[0]

    def read_farray(self, format, length):
        "Reads in a fixed-length array of the given format and length."
        reader = _get_struct(format, length)
        if self._index + reader.size > len(self._buffer):
            raise IndexError('Buffer not large enough to read serialized message. Received {0} bytes.'.format(
                len(self._buffer)))
        result = reader.unpack_from(self._buffer, self._index)
        self._index += reader.size
        return result

    def read_varray(self, data_format, length_format):
        "Reads in a variable-length array with the given length format and data format."
        length = self.read(length_format)
        return self.read_farray(data_format, length)

    def read_string(self, length_format):
        "Reads in a variable-length string with the given length format."
        length = self.read(length_format)
        bytes = self.read_farray('s', length)[0]
        return bytes.decode('utf_8')

    def read_string_farray(self, string_length_format, array_length):
        "Reads in a fixed-length array of variable-length strings with the given length format."
        return [self.read_string(string_length_format) for i in xrange(array_length)]

    def read_string_varray(self, string_length_format, array_length_format):
        "Reads in a variable-length array of variable-length strings with the given length format."
        array_length = self.read(array_length_format)
        return [self.read_string(string_length_format) for i in xrange(array_length)]

    def read_object(self, unpack_from_method):
        "Reads in an object according to the given method."
        return unpack_from_method(self)

    def read_object_farray(self, unpack_from_method, length):
        "Reads in a fixed-length object sequence according to the given method."
        return [unpack_from_method(self) for i in xrange(length)]

    def read_object_varray(self, unpack_from_method, length_format):
        "Reads in a variable-length object sequence according to the given method."
        length = self.read(length_format)
        return [unpack_from_method(self) for i in xrange(length)]

class BinaryWriter(object):
    "Used to write out a stream of binary data."

    def __init__(self):
        self._buffer = []

    def clear(self):
        del self._buffer[:]

    def dumps(self):
        return b''.join(self._buffer)

    def write(self, value, format):
        "Writes out a single value of the given format."
        self.write_farray((value,), format, 1)

    def write_farray(self, value, format, length):
        "Writes out a fixed-length array of the given format and length."
        writer = _get_struct(format, length)
        self._buffer.append(writer.pack(*value))

    def write_varray(self, value, data_format, length_format):
        "Writes out a variable-length array with the given length format and data format."
        self.write(len(value), length_format)
        self.write_farray(value, data_format, len(value))

    def write_string(self, value, length_format):
        "Writes out a variable-length string with the given length format."
        bytes = value.encode('utf_8')
        self.write(len(bytes), length_format)
        self.write_farray((bytes,), 's', len(bytes))

    def write_string_farray(self, value, string_length_format, array_length):
        "Writes out a fixed-length array of variable-length strings with the given length format."
        if len(value) != array_length:
            raise ValueError('The given fixed-length sequence has the wrong length.')
        for element in value:
            self.write_string(element, string_length_format)

    def write_string_varray(self, value, string_length_format, array_length_format):
        "Writes out a variable-length array of variable-length strings with the given length format."
        self.write(len(value), array_length_format)
        for element in value:
            self.write_string(element, string_length_format)

    def write_object(self, value):
        "Writes out an object that supports a pack_to method."
        value.pack_to(self)

    def write_object_farray(self, value, length):
        "Writes out a fixed-length object sequence that supports a pack_to method."
        if len(value) != length:
            raise ValueError('The given fixed-length sequence has the wrong length.')
        for element in value:
            element.pack_to(self)

    def write_object_varray(self, value, length_format):
        "Writes out a variable-length object sequence that supports a pack_to method."
        self.write(len(value), length_format)
        for element in value:
            element.pack_to(self)

def size(value, format):
    "Figures out the size of a value with given format."
    return _get_struct(format, 1).size

def size_farray(value, format, length):
    "Figures out the size of a fixed array with given format."
    return _get_struct(format, length).size

def size_varray(value, length_format, data_format):
    "Figures out the size of a fixed array with given format."
    return _get_struct(length_format, 1).size + _get_struct(data_format, len(value)).size

def size_string(value, length_format):
    "Figures out the size of a string with given length format."
    bytes = value.encode('utf_8')
    return _get_struct(length_format, 1).size + _get_struct('s', len(bytes)).size

def size_string_farray(value, string_length_format, array_length):
    "Figures out the size of a fixed-length array of strings with given length format."
    if len(value) != array_length:
        raise ValueError('The given fixed-length sequence has the wrong length.')
    if not value:
        return 0
    else:
        return sum(size_string(element, string_length_format) for element in value)

def size_string_varray(value, string_length_format, array_length_format):
    "Figures out the size of a variable-length array of strings with given length format."
    if not value:
        element_size = 0
    else:
        element_size = sum(size_string(element, string_length_format) for element in value)
    return _get_struct(array_length_format, 1).size + element_size

def size_object(value):
    "Figures out the size of a given object."
    return len(value)

def size_object_farray(value, length):
    "Figures out the size of a given fixed-length object sequence."
    if len(value) != length:
        raise ValueError('The given fixed-length sequence has the wrong length.')
    if not value:
        return 0
    else:
        return sum(size_object(element) for element in value)

def size_object_varray(value, length_format):
    "Figures out the size of a given fixed-length object sequence."
    if not value:
        element_size = 0
    else:
        element_size = sum(size_object(element) for element in value)
    return _get_struct(length_format, 1).size + element_size

def _evaluate_lazy_name(name):
    """
    Gives better error messages by appending indices but only requires tuple creation instead of string concatenation.
    """
    if isinstance(name, tuple) or isinstance(name, list):
        name, suffix = name
        if isinstance(suffix, int):
            format_string = '{name}[{suffix}]'
        else:
            format_string = '{name}.{suffix}'
        return format_string.format(name=_evaluate_lazy_name(name), suffix=suffix)
    else:
        return name

def validate_bool(name, value):
    "Validates and returns a given boolean."
    #try:
    #    value = bool(value)
    #except:
    #    raise ValueError('{name} must be a boolean value. Got a {type}.'.format(
    #        name=_evaluate_lazy_name(name), type=type(value).__name__))
    if not isinstance(value, bool):
        raise ValueError('{name} must be a boolean value. Got a {type}.'.format(
            name=_evaluate_lazy_name(name), type=type(value).__name__))
    return value

def validate_integer(name, value, minimum, maximum):
    "Validates, coerces and returns a given integer."
    try:
        value = int(value)
    except:
        raise ValueError('{name} must be an integer. Got a {type}.'.format(
            name=_evaluate_lazy_name(name), type=type(value).__name__))
    if value < minimum or value > maximum:
        raise ValueError('{name} must be between {minimum} and {maximum}. Got {value}.'.format(
            name=_evaluate_lazy_name(name), minimum=minimum, maximum=maximum, value=value))
    return value

def validate_float(name, value, format):
    "Validates, coerces and returns a given float."
    try:
        value = float(value)
    except:
        raise ValueError('{name} must be a float. Got a {type}.'.format(
            name=_evaluate_lazy_name(name), type=type(value).__name__))
    # coerce to ieee standard
    converter = _get_struct(format, 1)
    return converter.unpack(converter.pack(value))[0]

def validate_object(name, value, type):
    "Validates, coerces and returns a given struct."
    if not isinstance(value, type):
        raise ValueError('{name} must be a {expected_type}. Got a {value_type}.'.format(
            name=_evaluate_lazy_name(name),
            expected_type=type.__name__,
            value_type=type(value).__name__))
    return value

def validate_farray(name, value, length, element_validation):
    "Validates, coerces and returns a given fixed-length array."
    try:
        value = tuple(value)
    except:
        raise ValueError('{name} must be a sequence. Got a {type}.'.format(
            name=_evaluate_lazy_name(name), type=type(value).__name__))
    if len(value) != length:
        raise ValueError(('{name} must be a sequence of length {expected_length}. ' +
            'Got a sequence of length {value_length}.').format(
            name=_evaluate_lazy_name(name),
            expected_length=length,
            value_length=len(value)))
    return [element_validation((name, i), element) for i, element in enumerate(value)]

def validate_varray(name, value, maximum_length, element_validation):
    "Validates, coerces and returns a given variable-length array."
    try:
        value = tuple(value)
    except:
        raise ValueError('{name} must be a sequence. Got a {type}.'.format(
            name=_evaluate_lazy_name(name), type=type(value).__name__))
    if len(value) > maximum_length:
        raise ValueError(('{name} must be a sequence with length less than or equal to {maximum_length}. ' +
            'Got a sequence of length {value_length}.').format(
            name=_evaluate_lazy_name(name),
            maximum_length=maximum_length,
            value_length=len(value)))
    return [element_validation((name, i), element) for i, element in enumerate(value)]

def validate_string(name, value, maximum_length):
    "Validates, coerces and returns a given variable-length string."
    # For Python 3 we aliased unicode to be the same as str
    if isinstance(value, unicode):
        pass
    elif isinstance(value, str):
        try:
            value = value.decode('utf_8')
        except:
            raise ValueError('{name} could not be encoded into UTF-8.'.format(
                name=_evaluate_lazy_name(name), type=type(value).__name__))
    else:
        raise ValueError('{name} must be a string. Got a {type}.'.format(
            name=_evaluate_lazy_name(name), type=type(value).__name__))
    if len(value) > maximum_length:
        raise ValueError(('{name} must be a string with less than or equal to {maximum_length}. ' +
            'Got a string of length {value_length}.').format(
            name=_evaluate_lazy_name(name), maximum_length=maximum_length, value_length=len(value)))
    return value

def safety_check_tag(name, requested_tag, expected_tag, tag_count):
    "Ensures that you have the expected tag and throws an error if you do not."
    if requested_tag != expected_tag:
        if 0 <= requested_tag < tag_count:
            raise ValueError(('{name} is not currently holding a value for {requested_tag}. ' +
                'It contains a value for {expected_tag}.').format(
                name=_evaluate_lazy_name(name),
                requested_tag=tag_names[requested_tag],
                expected_tag=tag_names[expected_tag]))
        else:
            raise ValueError('{name} is not currently holding any value.')

def shorten_sequence(value, element_str=str):
    "Calls str() on a sequence, shortening long sequences."
    if len(value) > 4:
        return '[{0}, ...]'.format(', '.join(element_str(element) for element in value[:4]))
    else:
        return str(value)

def shorten_string(value):
    "Calls str() on a string, shortening long strings."
    if len(value) > 32:
        return repr(unicode('{0}...'.format(value[:29])))
    else:
        return repr(value)

class Namespace(argparse.Namespace):
    "A simple object that pretty prints when converted to string."

    def deep_clone(self):
        namespace = Namespace()
        for key, value in self._get_kwargs():
            if isinstance(value, Namespace):
                value = value.deep_clone()
            setattr(namespace, key, value)
        return namespace

    def update(self, other_namespace):
        for key, value in other_namespace._get_kwargs():
            if isinstance(value, Namespace):
                getattr(self, key).update(value)
            else:
                setattr(self, key, value)
