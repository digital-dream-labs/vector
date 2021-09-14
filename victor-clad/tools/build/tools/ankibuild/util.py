import collections
import errno
import hashlib
import json
import os
import re
import shutil
import sys
import time
try:
    import subprocess32 as subprocess
except ImportError:
    import subprocess

class Module(object):

    @staticmethod
    def _find_module_config(config_file=None):
        if config_file:
            return config_file
    
        env_key = 'ANKI_BUILD_MODULE_CONFIG'
        if env_key in os.environ:
            return os.environ[env_key] 

        repo_config = os.path.join(Git.repo_root(), '.ankimodules')
        if repo_config:
            return repo_config
    
    @staticmethod
    def _load_module_config(config_file=None):
        '''Parse .ankimodules'''
        if not config_file:
            config_file = Module._find_module_config()

        with open(config_file) as data_file:
            data = json.load(data_file)

        return data

    @staticmethod
    def get_path(name, config_file=None): 
        config = Module._load_module_config(config_file)
        entry = config[name]
        rel_path = entry['path']
        path = os.path.join(Git.repo_root(), rel_path)
        return path


class Git(object):

    @classmethod
    def repo_root(cls):
        p = subprocess.Popen(['git', 'rev-parse', '--show-toplevel'], stdout=subprocess.PIPE, cwd=".")
        out, err = p.communicate()
        retcode = p.poll()
        if retcode != 0:
            env_repo_root = os.environ['ANKI_BUILD_REPO_ROOT']
            if env_repo_root != None:
                out = env_repo_root
            else:
                out = ''
        return out.strip()

    @classmethod
    def find_submodule_path(cls, pattern):
        p = subprocess.Popen(['git', 'submodule', 'status'], stdout=subprocess.PIPE, cwd=".")
        out, err = p.communicate()
        retcode = p.poll()
        if retcode != 0:
            return None

        submodule_path = None
        status_lines = out.strip().rsplit('\n')
        for line in status_lines:
            m = re.search('(.?)([0-9A-Fa-f]{40}) ([^\(]+) (.*)', line)
            path = m.group(3)

            path_match = re.search(pattern, path)
            if path_match is not None:
                submodule_path = path
                break
            
        return submodule_path

ECHO_ALL = False

class File(object):

    @classmethod
    def _is_file_up_to_date(cls, target, dep):
        if not os.path.exists(dep):
            return False
        if not os.path.exists(target):
            return False

        target_time = os.path.getmtime(target)
        dep_time = os.path.getmtime(dep)

        return target_time >= dep_time

    @classmethod
    def is_up_to_date(cls, target, deps):
        if not os.path.exists(target):
            if ECHO_ALL:
                print('Not up to date; "{0}" does not exist.'.format(target))
            return False

        if not isinstance(deps, collections.Iterable):
            deps = [deps]

        up_to_date = True
        for dep in deps:
            up_to_date = cls._is_file_up_to_date(target, dep)
            if not up_to_date:
                print('Not up to date; "{0}" is newer than "{1}".'.format(dep, target))
                break

        return up_to_date

    @classmethod
    def md5sum(cls, filename):
        md5 = hashlib.md5()
        if os.path.isfile(filename):
            with open(filename, 'rb') as f:
                for chunk in iter(lambda: f.read(128*md5.block_size), b''):
                    md5.update(chunk)
        return md5.hexdigest()

    @classmethod
    def update_if_changed(cls, target, tmp):
        if cls.md5sum(target) != cls.md5sum(tmp):
            os.rename(tmp, target)
        else:
            os.remove(tmp)

    @classmethod
    def pwd(cls):
        "Returns the absolute path of the current working directory."
        if ECHO_ALL:
            print('pwd')
        try:
            return os.path.abspath(os.getcwd())
        except OSError as e:
            sys.exit('ERROR: Failed to get working directory: {0}'.format(e.strerror))

    @classmethod
    def cd(cls, path):
        "Changes the current working directory to the given path."
        path = os.path.abspath(path)
        if ECHO_ALL:
            print('cd ' + path)
        try:
            os.chdir(path)
        except OSError as e:
            sys.exit('ERROR: Failed to change to directory {0}: {1}'.format(path, e.strerror))


    @classmethod
    def mkdir_p(cls, path):
        "Recursively creates new directories up to and including the given directory, if needed."
        path = os.path.abspath(path)
        if ECHO_ALL:
            print(cls._escape(['mkdir', '-p', path]))
        try:
            os.makedirs(path)
        except OSError as e:
            if not os.path.isdir(path):
                sys.exit('ERROR: Failed to recursively create directories to {0}: {1}'.format(path, e.strerror))


    @classmethod
    def ln_s(cls, source, link_name):
        "Makes a symbolic link, pointing link_name to source."
        source = os.path.abspath(source)
        link_name = os.path.abspath(link_name)
        if ECHO_ALL:
            print(cls._escape(['ln', '-s', source, link_name]))
        try:
            os.symlink(source, link_name)
        except OSError as e:
            if not os.path.islink(link_name):
                sys.exit('ERROR: Failed to create symlink from {0} pointing to {1}: {2}'.format(link_name, source, e.strerror))


    @classmethod
    def rm_rf(cls, path):
        "Removes all files and directories including the given path."
        path = os.path.abspath(path)
        if ECHO_ALL:
            print(cls._escape(['rm', '-rf', path]))
        try:
            if os.path.isfile(path):
                os.remove(path)
            else:
                shutil.rmtree(path)
        except OSError as e:
            if os.path.exists(path):
                sys.exit('ERROR: Failed to recursively remove directory {0}: {1}'.format(path, e.strerror))


    @classmethod
    def rm(cls, path):
        "Removes a single specific cls."
        path = os.path.abspath(path)
        if ECHO_ALL:
            print(cls._escape(['rm', path]))
        try:
            os.remove(path)
        except OSError as e:
            if os.path.exists(path):
                sys.exit('ERROR: Failed to remove file {0}: {1}'.format(path, e.strerror))


    @classmethod
    def rmdir(cls, path):
        "Removes a directory if empty, or just contains directories."
        path = os.path.abspath(path)
        if ECHO_ALL:
            print(cls._escape(['rmdir', path]))
        try:
            dirs = [path]
            cycle_detector = set()
            while dirs:
                dir = dirs.pop()
                
                # infinite symlink loop protection
                realpath = os.path.realpath(dir)
                if realpath in cycle_detector:
                    continue
                cycle_detector.add(realpath)
                
                for file in os.listdir(dir):
                    if file != '.DS_Store':
                        fullpath = os.path.join(dir, file)
                        if os.path.isdir(fullpath):
                            dirs.append(fullpath)
                        else:
                            # normal file, so don't remove
                            print('"{0}" still exists, so not removing "{1}"'.format(fullpath, path))
                            return
            
            shutil.rmtree(path)
        except OSError as e:
            if os.path.exists(path):
                sys.exit('ERROR: Failed to remove empty directory {0}: {1}'.format(path, e.strerror))


    @classmethod
    def cp(cls, source, destination):
        "Copies a file from source to destination, if different."
        # allow specifying directory destination
        if os.path.isdir(destination):
            destination = os.path.join(destination, os.path.basename(source))
        cls.write(destination, cls.read(source))
        return True

    @classmethod
    def cptree(cls, source, destination):
        if not os.path.isdir(source):
            print (source + " is not a folder")
            return False
        try:
            shutil.copytree(source, destination)
        except OSError as exc:
            print ("error: " + str(exc))
            return False
        return True

    @classmethod
    def read(cls, path):
        "Reads the contents of the file at the given path."
        path = os.path.abspath(path)
        if ECHO_ALL:
            print('reading all from {0}'.format(path))
        try:
            with open(path, 'rb') as file:
                return file.read()
        except IOError as e:
            sys.exit('ERROR: Could not read from file {0}: {1}'.format(path, e.strerror))

    @classmethod
    def write(cls, path, contents):
        "Writes the specified contents to the given path, if different."
        path = os.path.abspath(path)
        contents = str(contents)
        if ECHO_ALL:
            print('writing {0} bytes to {1}'.format(len(contents), path))
        try:
            if os.path.isfile(path):
                if os.path.getsize(path) == len(contents):
                    with open(path, 'rb') as file:
                        old = file.read()
                        if contents == old:
                            return
                    
        except IOError as e:
            sys.exit('ERROR: Could not compare against old file for file {0}: {1}'.format(path, e.strerror))
        try:
            with open(path, 'wb') as file:
                file.write(contents)
        except IOError as e:
            sys.exit('ERROR: Could not write to file {0}: {1}'.format(path, e.strerror))


    @classmethod
    def _escape(cls, args):
        try:
            import pipes
            return ' '.join([pipes.quote(arg) for arg in args])
        except ImportError:
            return ' '.join(args)

    @classmethod
    def _escape_piped(cls, arglists):
        return ' | '.join(cls._escape(arglist) for arglist in arglists)
    
    @classmethod
    def _run_subprocess(cls, func, arglists, ignore_result):
        if not arglists:
            raise ValueError('No arguments to execute.')
        try:
            procs = []
            previous_output = None
            for arglist in arglists[:-1]:
                process = subprocess.Popen(arglist, stdin=previous_output, stdout=subprocess.PIPE)
                previous_output = process.stdout
                procs.append(process)
            output = func(arglists[-1], stdin=previous_output)
            if ECHO_ALL and not isinstance(output, int):
                print(output)

            for process in procs:
                if process.returncode == None:
                    # process hasn't finished, so wait for it
                    process.communicate()
                if process.returncode:
                    raise subprocess.CalledProcessError(process.returncode, "PIPE")

            procs = None
            return output
        except subprocess.CalledProcessError as e:
            if ignore_result:
                #print('WARNING: Subprocess `{0}` exited with code {1}.'.format(cls._escape_piped(arglists), e.returncode))
                return None
            else:
                sys.exit('ERROR: Subprocess `{0}` exited with code {1}.'.format(cls._escape_piped(arglists), e.returncode)) 
        except OSError as e:
            if ignore_result:
                print('WARNING: Subprocess `{0}` failed: {1}'.format(cls._escape_piped(arglists), e.strerror))
                return None
            else:
                sys.exit('ERROR: Error in subprocess `{0}`: {1}'.format(cls._escape_piped(arglists), e.strerror))
    
    @classmethod
    def _raw_execute(cls, func, arglists, ignore_result):
        if ECHO_ALL:
            print(cls._escape_piped(arglists))
        return cls._run_subprocess(func, arglists, ignore_result)

    @classmethod
    def execute(cls, *arglists, **kwargs):
        """
        Executes the given list of lists of arguments as a shell command, returning nothing.
        If more than one arglist, it pipes them.
        """
        ignore_result = kwargs.get('ignore_result')
        cls._raw_execute(subprocess.check_call, arglists, ignore_result)


    @classmethod
    def evaluate(cls, *arglists, **kwargs):
        """
        Executes the given list of lists of arguments as a shell command, returning stdout.
        If more than one arglist, it pipes them.
        """
        ignore_result = kwargs.get('ignore_result')
        return cls._raw_execute(subprocess.check_output, arglists, ignore_result)


class Profile(object):

    @staticmethod
    def begin(file, args):
        ANKI_PROFILE = os.environ.get('ANKI_PROFILE')
        if ANKI_PROFILE:
            basename = os.path.basename(file)
            if not os.path.exists(ANKI_PROFILE):
                os.makedirs(ANKI_PROFILE)
            with open(os.path.join(ANKI_PROFILE, basename+".log"), "w") \
              as profile_file:
                profile_file.write("B:%d P:%d F:%s A:%s\n" % \
                                   (int(time.time()), os.getpid(), basename, str(args)))


    @staticmethod
    def end(file, args):
        ANKI_PROFILE = os.environ.get('ANKI_PROFILE')
        if ANKI_PROFILE:
            basename = os.path.basename(file)
            with open(os.path.join(ANKI_PROFILE, basename+".log"), "a") \
              as profile_file:
                profile_file.write("E:%d P:%d F:%s\n" % \
                                   (int(time.time()), os.getpid(), basename))

class Gyp(object):

    @staticmethod
    def getArgFunction(defaultArgs):
        def argFunc(format, outputFolder, gypFile = None):
            returnArgs = defaultArgs + ['-f', format, '--generator-output', outputFolder]
            if gypFile is not None:
                returnArgs += [gypFile]
            return returnArgs
        return argFunc

    @staticmethod
    def getDefineString(defines, overrideDefines = None):
        if overrideDefines is not None:
            for entry in overrideDefines:
                (k,v) = entry.split('=')
                key = k.strip()
                value = v.strip()
                defines[key] = value

        define_args = ["%s=%s" % (k, v) for k,v in defines.iteritems()]
        return "\n".join(define_args)
