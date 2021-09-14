#!/usr/bin/env python2

import argparse
import json

class Module(object):
    def __init__(self, name, config):
        self.name = name
        self.config = config

    def name(self):
        return self.name

    def path(self):
        return self.config['path']

class Settings(object):
    def __init__(self):
        self.platform = 'unity'
        self.config = 'debug'
        self.build_dir = 'build'
        self.output_dir = None
        self.log_file = None
        self.basestation_path = None
        self.asset_path = None
        self.verbose = False
        self.debug = False

    def load_options(self, options):
        self.platform = options.platform
        self.config = options.config
        self.build_dir = options.build_dir
        self.output_dir = options.output_dir
        self.log_file = options.log_file
        self.basestation_path = options.with_basestation
        self.asset_path = options.with_assets
        self.verbose = options.verbose
        self.debug = options.debug

class DefaultHelpParser(argparse.ArgumentParser):
    def error(self, message):
        sys.stderr.write('error: %s\n' % message)
        self.print_help()
        sys.exit(2)

class ArgParseUniqueStore(argparse.Action):
    def __call__(self, parser, namespace, values, option_string):
        if getattr(namespace, self.dest, self.default) is not None:
            parser.error(option_string + " appears multiple times.\n")
        setattr(namespace, self.dest, values)

class Builder(object):
    def __init__(self, config=None, module_config=None):
        self.config = config
        self.modules = {}

    def parse_modules(self, module_config):
        data = json.load(module_config)

        for (key, value) in data.iteritems():
            m = Module(key, value)
            self.modules[key] = m

    def argument_parser(self):
        parser = DefaultHelpParser(add_help=False)

        parser.add_argument('-v', '--verbose', action="store_true", default=False)
        parser.add_argument('-d', '--debug', action="store_true", default=False)
        parser.add_argument('-b', '--build-dir', action="store")
        parser.add_argument('-o', '--output-dir', action="store")
        parser.add_argument('--log-file', action="store")
        parser.add_argument('-p', '--platform', action="store", choices=('ios', 'android', 'mac','linux'))
        parser.add_argument('-c', '--config', action="store", choices=('debug', 'release', 'shipping'))
        parser.add_argument('--with-basestation', action="store",
                            help="path to basestation source repo")
        parser.add_argument('--with-assets', action="store",
                            help="path to assets repo")

        return parser

    def parse_arguments(self, parser=None, argv=[]):
        if parser == None:
            parser = self.argument_parser()
        return parser.parse_args(argv)
