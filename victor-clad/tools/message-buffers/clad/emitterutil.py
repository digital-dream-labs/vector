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
Shared __main__ method functionality for emitters.
"""

from __future__ import absolute_import
from __future__ import print_function

import argparse
import contextlib
import errno
import inspect
import io
import os
import pipes
import re
import sys
import textwrap

from . import ast
from . import clad

_py3 = sys.version_info[0] >= 3

def get_included_file(file, suffix):
    return file[:file.find('.', max(file.find('/'), 0))] + suffix

@contextlib.contextmanager
def _disable_with(context):
    yield context

class IndentedTextIOWrapper(io.StringIO):

    class DepthWithHelper(object):
        def __init__(self, wrapper, depth_delta):
            self.wrapper = wrapper
            self.depth_delta = depth_delta

        def __enter__(self):
            # don't hook here so it makes sense to use, even without 'with'
            pass

        def __exit__(self, type, value, traceback):
            self.wrapper.depth -= self.depth_delta

    def __init__(self, path, depth=0, tab_character='\t'):
        super(IndentedTextIOWrapper, self).__init__(newline='\n')
        self.path = path
        self.depth = depth
        self.ended_on_newline = False
        self.tab_character = tab_character

    def __exit__(self, type, value, traceback):
        if type is None:
            self._write_to_file()
        super(IndentedTextIOWrapper, self).__exit__(type, value, traceback)

    def _open_file(self, filename, mode):
        if os.path.exists(filename):
            os.chmod(filename, 0o0755)
        if _py3:
            return open(filename, mode, newline='\n')
        else:
            return open(filename, '{mode}b'.format(mode=mode))

    def _write_to_file(self):
        if self.path is not None:
            with self._open_file(self.path, 'w') as output:
                output.write(self.getvalue())
            if os.path.exists(self.path):
                os.chmod(self.path, 0o0555)
        else:
            sys.stdout.write(self.getvalue())

    def write(self, value):
        if not value:
            return

        if self.ended_on_newline and self.depth > 0:
            value = '\t' * self.depth + value

        self.ended_on_newline = (value[-1] == '\n')

        # Don't indent until there is actual content on the line
        # Don't indent end of file
        if self.depth > 0 and '\n' in value[:-1]:
            value = value[:-1].replace('\n', '\n' + '\t' * self.depth) + value[-1]

        value = value.replace('\t', self.tab_character)

        if not _py3:
            value = unicode(value, 'utf8')

        super(IndentedTextIOWrapper, self).write(value)

    def indent(self, depth_delta):
        "Starts indenting text and returns an object that unindents if this method is called in a 'with' block."
        self.depth += depth_delta
        return self.DepthWithHelper(self, depth_delta)

    def reset_indent(self):
        "Reduces indent to 0 temporarily and returns an object that reindents if this method is called in a 'with' block."
        old_depth = self.depth
        self.depth = 0
        return self.DepthWithHelper(self, -old_depth)

    def write_with_aligned_whitespace(self, lines, extra_whitespace=0):
        """
        Takes in a list of sequences of strings, then writes them all to the buffer as lines,
        adding whitespace to keep each column aligned.

        extra_whitespace -- extra space to join the strings with (1 would mean a space inbetween)

        [('int', ' foo', ' = 5;'), ('float', ' bar;')] ->
        int   foo  = 5;
        float bar;
        """
        if not lines:
            return

        # zip requires rows are equal length, but we don't, so convert just for zip
        max_sequence_length = max(len(sequence) for sequence in lines)
        rows = [tuple(sequence) + ('',) * (max_sequence_length - len(sequence)) for sequence in lines]

        # [(a0, b0, c0, d0), (a1, b1, c1, d1)] -> [(a0, a1), (b0, b1), (c0, c1), (d0, d1)]
        columns = zip(*rows)
        space_per_column = [max(len(piece) for piece in column) for column in columns]

        for sequence in lines:
            if sequence:
                for i, piece in enumerate(sequence[:-1]):
                    self.write(piece)
                    self.write(' ' * (space_per_column[i] - len(piece)))
                self.write(sequence[-1])
                self.write('\n')

def _get_input(options):
    if options.input_file != '-':
        real_path = os.path.join(options.input_directory, options.input_file)
        return open(real_path, 'r')
    else:
        return _disable_with(sys.stdin)

def get_output_file(options, suffix):
    if options.output_file:
        if options.output_file == '-':
            return '-'
        else:
            return options.output_file
    elif options.input_file != '-':
        return get_included_file(options.input_file, suffix)
    else:
        return '-'

def get_output(output_directory, output_file, binary=False):
    if output_directory != '-' and output_file != '-':
        real_path = os.path.join(output_directory, output_file)

        mkdir_p(os.path.dirname(real_path))

        if binary:
            return open(real_path, 'wb')
        else:
            return IndentedTextIOWrapper(real_path, tab_character='  ')
    else:
        return IndentedTextIOWrapper(None, tab_character='  ')

def create_file(path):
    "Creates an empty file unless it already exists, in which case it does nothing."
    if not os.path.exists(path):
        with open(path, 'a'):
            pass

# See: http://stackoverflow.com/a/600612/4093018
def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

def make_path_relative(path, start=None):
    "Does os.path.relpath, first resolving to an absolute path."
    if start:
        start = os.path.normpath(os.path.abspath(start))
    path = os.path.normpath(os.path.abspath(path))
    return os.path.relpath(path, start)

def make_path_portable(path):
    "Makes a path as portable if possible. Can't fix absolute paths."
    path = os.path.normpath(path)
    if os.path.isabs(path):
        return path
    return path.replace('\\', '/')

def get_comment_lines(options, language):
    if options.input_file != '-':
        source = options.input_file
    else:
        source = '<stdin>'
    rel_args = _convert_abspaths_to_relpaths(sys.argv)
    command_line = ' '.join(pipes.quote(arg) for arg in rel_args)
    return [
        'Autogenerated {language} message buffer code.'.format(language=language),
        'Source: {source}'.format(source=source),
        'Full command line: {command_line}'.format(command_line=command_line)
    ]

#http://stackoverflow.com/a/1176023/4093018
def convert_camelcase_to_underscores(identifier):
    identifier = re.sub('([A-Za-z0-9])([A-Z][a-z]+)', r'\1_\2', identifier)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', identifier).lower()

def c_inclusion_guard(output_file):
    "Creates a C-style macro inclusion guard."
    guard = output_file
    guard = re.sub('[^A-Za-z0-9_./-]', '', guard)
    guard = guard.strip('./-')
    guard = re.sub('[./-]', '_', guard)
    guard = convert_camelcase_to_underscores(guard).upper()
    guard = guard or 'clad'
    guard = '__{guard}__'.format(guard=guard)
    return guard

class SimpleArgumentParser(argparse.ArgumentParser):
    """
    The bare-minimum arguments for parsing. No output arguments.

    Calculates the following arguments and metavalues:
    input_file -- the filename of the file to read from (made portable)
    input_directory -- the directory to resolve includes from
    debug_yacc -- turn on yacc debug

    You may add your own arguments like a normal argparse.ArgumentParser.
    """

    def __init__(self, *args, **kwargs):
        super(SimpleArgumentParser, self).__init__(*args, **kwargs)
        self.add_argument('input_file', metavar='input-file',
            help='The .clad file to parse.')
        self.add_argument('-C', '--input-directory', default='./', metavar='dir',
            help='The input prefix directory.')
        self.add_argument('-I', '--include-directory', default=(), metavar='dir',
            nargs='*', dest='include_directories',
            help='Additional directories in which to search for included files.')
        self.add_argument('--namespace', metavar='emitter_namespace', type=str,
            help="Global namespace prefixed in front of all specified namespaces.")

    def parse_known_args(self, *args, **kwargs):
        # add this argument last
        self.add_argument('-d', '--debug-yacc', action='store_true',
            help='Turn on yacc debug.')

        args, argv = super(SimpleArgumentParser, self).parse_known_args(*args, **kwargs)
        self.postprocess(args)
        return args, argv

    def postprocess(self, args):
        args.input_file = make_path_portable(args.input_file)

class StandardArgumentParser(SimpleArgumentParser):
    """
    An argparse.ArgumentParser used to parse arguments supported in every language.

    Calculates the additional arguments and metavalues:
    output_directory -- the directory to output to
    output_file (if permitted) -- the file to output to
    """

    def __init__(self, language, allow_override_output=False, *args, **kwargs):
        description = 'Generate {language} code from a .clad file.'.format(language=language)
        super(StandardArgumentParser, self).__init__(description=description, *args, **kwargs)

        self._allow_override_output = allow_override_output
        if allow_override_output:
            group = self.add_mutually_exclusive_group()
            group.add_argument('-o', '--output-directory', default='-', metavar='dir',
                help='The directory to output the {language} file to.'.format(language=language))
            group.add_argument('--output-file', metavar='file',
                help='The file path to write the {language} file to.'.format(language=language))
        else:
            self.add_argument('-o', '--output-directory', default='-', metavar='dir',
                help='The directory to output the {language} file(s) to.'.format(language=language))

    def postprocess(self, args):
        super(StandardArgumentParser, self).postprocess(args)
        if self._allow_override_output and args.output_file:
            args.output_directory = os.getcwd()
        else:
            args.output_file = None

def parse(options, yacc_optimize=False, debuglevel=0):
    "Parses a clad input file."
    with _get_input(options) as input:
        input_directories = [options.input_directory]
        input_directories.extend(options.include_directories)
        clad_parser = clad.CLADParser(yacc_optimize=yacc_optimize,
            yacc_debug=options.debug_yacc, input_directories=input_directories)
        text = input.read()
        try:
            tree = clad_parser.parse(text,
                filename=options.input_file,
                directory=options.input_directory,
                debuglevel=debuglevel)
        except clad.ParseError as e:
            msg = e.args[1]
            coord = e.args[0]
            exit_at_coord(coord, 'Syntax Error: {msg}'.format(msg=msg))

        # if specified, inject wrapper namespace
        if options.namespace:
            nsw = ast.ASTNamespaceWrapper(wrapper_ns=options.namespace)
            nsw.visit(tree)

        return tree

def exit_at_coord(coord, error_text=None):
    "Exits, specifying a coord as the cause."
    if error_text:
        sys.stderr.write(error_text)
        sys.stderr.write('\n')

    if coord == ast.builtin_coord:
        sys.stderr.write("<located at builtin value>\n")

    if not coord or not coord.file and line is None:
        sys.stderr.write('<unknown location>\n')

    elif not coord.file:
        sys.stderr.write('<unknown file>')
        if coord.lineno is not None:
            sys.stderr.write(' line {line}'.format(line=coord.lineno))
            if coord.column is not None:
                sys.stderr.write(' col {column}'.format(column=coord.column))
        sys.stderr.write('\n')

    else:
        sys.stderr.write(coord.file)
        if coord.lineno is not None:
            sys.stderr.write(' line {line}'.format(line=coord.lineno))
            if coord.column is not None:
                sys.stderr.write(' col {column}'.format(column=coord.column))
        sys.stderr.write(':\n')

        real_file_path = os.path.join(coord.directory, coord.file)
        if coord.lineno is not None and  os.path.exists(real_file_path):
            with open(real_file_path, "r") as debug_input:
                for i, line in enumerate(debug_input):
                    line_number = i + 1
                    if line_number > coord.lineno + 3:
                        break
                    elif line_number >= coord.lineno - 3:
                        sys.stderr.write('{spacer}{line_number} {line}\n'.format(
                            line_number=str(line_number).rjust(3, ' '),
                            line=line.rstrip('\n'),
                            spacer=('>>' if line_number == coord.lineno else '  ')))
    sys.exit(1)


def c_main(language, extension, emitter_types,
        allow_custom_extension=False, allow_override_output=False,
        use_inclusion_guards=False,
        system_headers=None, local_headers=None, usings=None):
    """
    Entry point for a simple C-like language.

    language -- C, C++, C#
    extension -- .h, .cs
    emitter_types -- a list of emitters to have visit the ast
    use_inclusion_guards -- put inclusion guards around the header file
    system_headers -- a list of headers: #include <foo>
    local_headers -- a list of headers: #include "foo"
    usings -- a list of namespaces: using foo;
    """

    option_parser = StandardArgumentParser(language, allow_override_output)
    if allow_custom_extension:
        option_parser.add_argument('--header-output-extension', default=extension, metavar='ext',
            help='The extension to use for header files. (Helps work around a CMake Xcode issue.)')
    options = option_parser.parse_args()
    if allow_custom_extension:
        extension = options.header_output_extension

    tree = parse(options)
    main_output_file = get_output_file(options, extension)
    comment_lines = get_comment_lines(options, language)

    def output_callback(output):
        for emitter_type in emitter_types:
            emitter_type(output, include_extension=extension).visit(tree)

    write_c_file(options.output_directory, main_output_file, output_callback,
        comment_lines, use_inclusion_guards,
        system_headers, local_headers, usings)

def write_c_file(output_directory, output_file, output_callback,
        comment_lines=None, use_inclusion_guards=False,
        system_headers=None, local_headers=None, usings=None):

    with get_output(output_directory, output_file) as output:
        if comment_lines:
            output.write('\n'.join('// {0}'.format(line) for line in comment_lines))
            output.write('\n\n')

        if use_inclusion_guards:
            inclusion_guard = c_inclusion_guard(output_file)
            output.write('#ifndef {inclusion_guard}\n'.format(inclusion_guard=inclusion_guard))
            output.write('#define {inclusion_guard}\n\n'.format(inclusion_guard=inclusion_guard))

        if system_headers:
            for header in sorted(system_headers):
                output.write('#include <{header}>\n'.format(header=header))
            output.write('\n')
        if local_headers:
            for header in sorted(local_headers):
                output.write('#include "{header}"\n'.format(header=header))
            output.write('\n')
        if usings:
            for using in sorted(usings):
                output.write('using {using};\n'.format(using=using))
            output.write('\n')

        output_callback(output)

        if use_inclusion_guards:
            output.write('#endif // {inclusion_guard}\n'.format(inclusion_guard=inclusion_guard))

def write_cs_file(output_directory, output_file, output_callback, comment_lines=None, usings=None):

    write_c_file(output_directory, output_file, output_callback,
        comment_lines, use_inclusion_guards=False,
        system_headers=None, local_headers=None, usings=usings)

def go_main(emitter, options, scanner=None):
    tree = parse(options)
    main_output_file = get_output_file(options, '.go')
    comment_lines = get_comment_lines(options, 'Go')

    def output_callback(output):
        emitter(output).visit(tree)

    properties = dict()
    if scanner is not None:
        scanner(properties).visit(tree)

    write_go_file(options.output_directory, main_output_file, output_callback,
        comment_lines=comment_lines, package=options.package, properties=properties)

def write_go_file(output_directory, output_file, output_callback, comment_lines=None, package=None,
    imports=None, properties=dict()):

    includes = []
    if properties.get('use_bytes', True):
        includes.append('bytes')
    if properties.get('use_binary', True):
        includes.append('encoding/binary')
    if properties.get('use_errors', True):
        includes.append('errors')
    if properties.get('use_clad', True):
        includes.append('anki/clad')
    if properties.get('use_fmt', True):
        includes.append('fmt')

    if package == None:
        full_path = os.path.join(output_directory, output_file)
        package = os.path.basename(os.path.normpath(os.path.dirname(full_path)))
    with get_output(output_directory, output_file) as output:
        if comment_lines:
            output.write('\n'.join('// {0}'.format(line) for line in comment_lines))
            output.write('\n\n')
        output.write('package {}\n\n'.format(package))
        output.write('import (\n')
        includes = ['\t"' + x + '"\n' for x in includes]
        output.write(''.join(includes))
        output.write(')\n\n')

        output_callback(output)

def js_main(emitter, options, scanner=None):
    tree = parse(options)
    main_output_file = get_output_file(options, '.js')
    comment_lines = get_comment_lines(options, 'JS')

    def output_callback(output):
        emitter(output).visit(tree)

    properties = dict()
    if scanner is not None:
        scanner(properties).visit(tree)

    write_js_file(options.output_directory, main_output_file, output_callback,
        comment_lines=comment_lines, package=options.package, properties=properties)

def write_js_file(output_directory, output_file, output_callback, comment_lines=None, package=None,
    imports=None, properties=dict()):

    includes = []
    if properties.get('use_bytes', True):
        includes.append('bytes')
    if properties.get('use_binary', True):
        includes.append('encoding/binary')
    if properties.get('use_errors', True):
        includes.append('errors')
    if properties.get('use_clad', True):
        includes.append('anki/clad')
    if properties.get('use_fmt', True):
        includes.append('fmt')

    if package == None:
        full_path = os.path.join(output_directory, output_file)
        package = os.path.basename(os.path.normpath(os.path.dirname(full_path)))
    with get_output(output_directory, output_file) as output:
        if comment_lines:
            output.write('\n'.join('// {0}'.format(line) for line in comment_lines))
            output.write('\n\n')
        output.write('const {{ Clad, CladBuffer }} = require(\'./cladConfig.js\');\n\n'.format(package))

        output_callback(output)

def write_python_file(output_directory, output_file, output_callback,
        comment_lines=None, future_features=('absolute_import', 'print_function'),
        additional_paths=None, import_modules=None):

    with get_output(output_directory, output_file) as output:
        if comment_lines:
            output.write('"""\n')
            output.write('\n'.join('{0}'.format(line) for line in comment_lines))
            output.write('\n"""\n\n')

        if future_features:
            for feature in future_features:
                output.write('from __future__ import {feature}\n'.format(feature=feature))
            output.write('\n')

        if output != sys.stdout:
            additional_paths = [output_directory] + (additional_paths or [])
        if additional_paths:
            _write_python_modify_path(
                output, os.path.join(output_directory, output_file), additional_paths)

        if import_modules:
            for module in import_modules:
                if isinstance(module, tuple):
                    source, target = module
                    output.write('from {source} import {target}\n'.format(source=source, target=target))
                else:
                    output.write('import {module}\n'.format(module=module))
            output.write('\n')

        output_callback(output)

def decode_hex_string(string):
    return [int(string[i:i+2], 16) for i in range(0, len(string), 2)]

def _pretty_print_list(output, paths, depth=1):
    if not paths:
        output.write('[]')
    else:
        inner_depth = depth + 1
        output.write('[\n')
        for path in paths:
            output.write('\t' * inner_depth)
            output.write(repr(path))
            output.write(',\n')
        output.write('\t' * depth)
        output.write(']')

def _write_python_modify_path(output, output_file, additional_paths):

    if output != sys.stdout:
        currentpath = os.path.dirname(output_file)
        additional_paths = [make_path_relative(path, currentpath) for path in additional_paths]
    else:
        additional_paths = [os.path.normpath(os.path.abspath(path)) for path in additional_paths]

    output.write(textwrap.dedent('''\
        def _modify_path():
        \timport inspect, os, sys
        \tsearch_paths = '''))
    _pretty_print_list(output, additional_paths, depth=1)
    output.write('\n')

    if output != sys.stdout:
        output.write('\tcurrentpath = os.path.abspath(os.path.dirname(inspect.getfile(inspect.currentframe())))\n')

    output.write('\tfor search_path in search_paths:\n')

    if output != sys.stdout:
        output.write('\t\tsearch_path = os.path.normpath(os.path.abspath(os.path.realpath(os.path.join(currentpath, search_path))))\n')

    output.write(textwrap.dedent('''\
        \t\tif search_path not in sys.path:
        \t\t\tsys.path.insert(0, search_path)
        _modify_path()

        '''))

def _do_all_members_have_default_constructor(node):
    '''Recursively traverses all members of an ast.Node and returns true if all of them have a default
       constructor. Returns false if any members don't have a default constructor. NOTE: does not check 'node'
       itself
    '''

    for member in node.members():
        if hasattr(member, 'type') and hasattr(member.type, 'type_decl'):
            child = member.type.type_decl
            if ( hasattr(child, 'default_constructor') and
                 not child.default_constructor ):
                return False
            if not _do_all_members_have_default_constructor(child):
                return False

    return True

def _split_string_on_operators(string):
    '''Splits the string on the characters '*', '/', '+', '-', ' '
    '''
    split = set(re.split("[*+/ -]", string))
    if '' in split:
        split.remove('')
    return split

def _lower_first_char_of_string(string):
    return string[:1].lower() + string[1:] if string else ''

def _convert_abspaths_to_relpaths(args):
    """Returns a list of args with absolute paths replaced by relative paths
       using the longest common path. Absolute paths are detected using os.path.isabs(path).
       Non-path and relative-path args are not modified.
    """
    paths = [ a for a in args if os.path.isabs(a) ]
    # find longest_common_path (lcp)
    # commonprefix might return a partial path, so use dirname on the results
    lcp = os.path.dirname(os.path.commonprefix(paths))
    rel_args = [os.path.relpath(a, lcp) if os.path.isabs(a) else a for a in args]
    return rel_args

