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
Long constants definitions
Author: Greg Nagel
Date: 3/18/2015
"""

from __future__ import absolute_import
from __future__ import print_function

# keywords in C
c_keywords = frozenset([
	'auto',
	'break',
	'case',
	'char',
	'const',
	'continue',
	'default',
	'do',
	'double',
	'else',
	'enum',
	'extern',
	'float',
	'for',
	'goto',
	'if',
	'int',
	'long',
	'register',
	'return',
	'short',
	'signed',
	'sizeof',
	'static',
	'struct',
	'switch',
	'typedef',
	'union',
	'unsigned',
	'void',
	'volatile',
	'while',
])

# keywords in C++98 that are not in C
cpp_98_keywords = frozenset([
	'asm',
	'bool',
	'catch',
	'class',
	'const_cast',
	'delete',
	'dynamic_cast',
	'explicit',
	'false',
	'friend',
	'inline',
	'mutable',
	'namespace',
	'new',
	'operator',
	'private',
	'protected',
	'public',
	'reinterpret_cast',
	'static_cast',
	'template',
	'this',
	'throw',
	'true',
	'try',
	'typeid',
	'typename',
	'using',
	'virtual',
	'wchar_t',
])

# keywords in C++11 that are not in C++98
cpp_11_keywords = frozenset([
	'and_eq',
	'and',
	'bitand',
	'bitor',
	'compl',
	'not_eq',
	'not',
	'or_eq',
	'or',
	'xor_eq',
	'xor',
])

cpp_keywords = c_keywords | cpp_98_keywords | cpp_11_keywords

# Until python 3: None, False and True are technically not keywords, but can't be assigned to in any context
python_2_keywords = frozenset([
	'False',
	'None',
	'True',
	'and',
	'as',
	'assert',
	'break',
	'class',
	'continue',
	'def',
	'del',
	'elif',
	'else',
	'except',
	'exec',
	'finally',
	'for',
	'from',
	'global',
	'if',
	'import',
	'in',
	'is',
	'lambda',
	'not',
	'or',
	'pass',
	'print',
	'raise',
	'return',
	'try',
	'while',
	'with',
	'yield',
	'type',
])

# add nonlocal
# no exec, print
python_3_keywords = frozenset([
	'False',
	'None',
	'True',
	'and',
	'as',
	'assert',
	'break',
	'class',
	'continue',
	'def',
	'del',
	'elif',
	'else',
	'except',
	'finally',
	'for',
	'from',
	'global',
	'if',
	'import',
	'in',
	'is',
	'lambda',
	'nonlocal',
	'not',
	'or',
	'pass',
	'raise',
	'return',
	'try',
	'while',
	'with',
	'yield',
	'type',
])

python_keywords = python_2_keywords | python_3_keywords


# keywords in C# (yup there are that many)
# note that you can always specify @ before an identifier in C#
csharp_keywords = frozenset([
	'abstract',
	'as',
	'base',
	'bool',
	'break',
	'byte',
	'case',
	'catch',
	'char',
	'checked',
	'class',
	'const',
	'continue',
	'decimal',
	'default',
	'delegate',
	'do',
	'double',
	'else',
	'enum',
	'event',
	'explicit',
	'extern',
	'false',
	'finally',
	'fixed',
	'float',
	'for',
	'foreach',
	'goto',
	'if',
	'implicit',
	'in',
	'int',
	'interface',
	'internal',
	'is',
	'lock',
	'long',
	'namespace',
	'new',
	'null',
	'object',
	'operator',
	'out',
	'override',
	'params',
	'private',
	'protected',
	'public',
	'readonly',
	'ref',
	'return',
	'sbyte',
	'sealed',
	'short',
	'sizeof',
	'stackalloc',
	'static',
	'string',
	'struct',
	'switch',
	'this',
	'throw',
	'true',
	'try',
	'typeof',
	'uint',
	'ulong',
	'unchecked',
	'unsafe',
	'ushort',
	'using',
	'virtual',
	'void',
	'volatile',
	'while',
])

# mostly this exists to catch enumeration "None" values
keywords = (c_keywords | cpp_keywords |
    python_keywords | csharp_keywords)

def what_language_uses_keyword(identifier):
    "Returns the name of the language that defines the specified identifier."
    if identifier in c_keywords:
        return 'C'
    elif identifier in cpp_98_keywords:
        return 'C++98'
    elif identifier in cpp_11_keywords:
        return 'C++11'
    elif identifier in python_keywords:
        return 'Python'
    elif identifier in csharp_keywords:
        return 'C#'
    else:
        return None
