#! /usr/bin/python
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

#########################
# TEST MAIN             #
# -Dumps the parse tree #
#########################
if __name__ == '__main__':
    from clad import emitterutil
    option_parser = emitterutil.SimpleArgumentParser('Test the clad parser.')
    option_parser.add_argument('-y', '--debuglevel', default=0, dest='debuglevel',
                               help='PLY debug level (0=none, 1=yacc, 2=lex)')
    options = option_parser.parse_args()

    tree = emitterutil.parse(options, yacc_optimize=False, debuglevel=int(options.debuglevel))
    ast.ASTDebug().visit(tree)
