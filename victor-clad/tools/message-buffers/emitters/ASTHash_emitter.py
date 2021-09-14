#! /usr/bin/python

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

if __name__ == '__main__':
    "Emits md5 hash of a clad file with optional formatting"
    from clad import emitterutil
    option_parser = emitterutil.SimpleArgumentParser('Emit hash for clad file')
    option_parser.add_argument('-y', '--debuglevel', default=0, dest='debuglevel', type=int,
                               help='PLY debug level (0=none, 1=yacc, 2=lex)')
    option_parser.add_argument('-o', '--output', default="-",
                               help="Specify output, file type determines output format")
    options = option_parser.parse_args()

    tree = emitterutil.parse(options, yacc_optimize=False, debuglevel=options.debuglevel)
    visitor = ast.ASTHash()
    visitor.visit(tree)

    if options.output == "-":
        print(visitor.hash_str)
    else:
        outputBase, outputExt = os.path.splitext(options.output)

        const_name = os.path.splitext(os.path.split(options.input_file)[-1])[0] + "Hash"
        hash = int(visitor.hash_str, 16)
        
        def calc_hash_bytes():
          hash_bytes = ["0x{:02x}".format(((hash >> (8*i))) & 0xff) for i in range(16)]
          return hash_bytes

        if outputExt == '.h':

            def main_output_source_callback(output):
                byte_array = ", ".join(calc_hash_bytes())
                output.write("const unsigned char {}[] = {{ {} }};\n".format(const_name, byte_array))

            comment_lines = emitterutil.get_comment_lines(options, 'C++')
            output_dir = os.path.dirname(options.output)
            main_output_source = os.path.basename(options.output)
            emitterutil.write_c_file(output_dir, main_output_source,
                    main_output_source_callback,
                    comment_lines,
                    use_inclusion_guards=True)

        elif outputExt == ".py":
            
            def main_output_source_callback(output):
                output.write("{} = 0x{:x}\n".format(const_name, hash))

            comment_lines = emitterutil.get_comment_lines(options, 'python')
            output_dir = os.path.dirname(options.output)
            main_output_source = os.path.basename(options.output)
            emitterutil.write_python_file(output_dir, main_output_source,
                    main_output_source_callback,
                    comment_lines)

        elif outputExt == ".cs":

            def main_output_source_callback(output):
                # For C# we need a unique class name as a "namespace", so set the class name
                # to the const_name, but with the first letter capitalized
                class_name = "".join(c.upper() if (i==0) else c for i, c in enumerate(const_name))
                byte_array = ", ".join(calc_hash_bytes())
                output.write("public static class {} {{\n".format(class_name))
                output.write("    public static readonly byte[] _Data = new byte[] {{ {} }};\n".format(byte_array))
                output.write("}\n")

            comment_lines = emitterutil.get_comment_lines(options, 'C#')
            output_dir = os.path.dirname(options.output)
            main_output_source = os.path.basename(options.output)
            emitterutil.write_cs_file(output_dir, main_output_source,
                    main_output_source_callback,
                    comment_lines)
            
        else:
            sys.exit("Unsupported output type: {}".format(outputExt))
