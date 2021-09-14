#!/usr/bin/env node
'use strict';

const child_process = require('child_process');
const fs = require('fs');
const path = require('path');
const process = require('process');

const execSync = child_process.execSync;
const execSyncTrim = (...args) => execSync(...args).toString().trim();
const configFile = '.subtrees';
const userFile = '.subtreeconfig';

const hashRegex = /^[a-fA-F0-9]{40}$/;

// make sure we're in the top level of a git repo
const toplevel = (() => {
  try {
    return execSyncTrim('git rev-parse --show-toplevel');
  }
  catch (error) {
    return error.toString();
  }
})();
if (toplevel !== process.cwd()) {
  console.log('This must be run from the top level of a git repository');
  process.exit(1);
}

// Base definition for serializable config files
class Serializable {
  constructor(propname, filename) {
    this.propname = propname;
    this.filename = filename;
    this[propname] = {};
  }
  load() {
    if (!fs.existsSync(this.filename)) {
      // not an error for file to not exist, return
      return;
    }
    try {
      const contents = fs.readFileSync(this.filename, 'utf8');
      this[this.propname] = JSON.parse(contents);
    }
    catch (error) {
      console.log('Error, could not read or parse configuration file: ' + error.toString());
    }
  }
  save() {
    fs.writeFileSync(this.filename, JSON.stringify(this[this.propname], null, 2));
  }
}

// Representation of subtree configuration file
class Config extends Serializable {
  constructor() {
    super('subtrees', configFile);
    this.userConfig = new UserConfig(this);
    this.load();
  }

  addSubtree(name, subtreeData) {
    this.subtrees[name] = subtreeData;
    this.save();
  }

  removeSubtree(name) {
    delete this.subtrees[name];
    this.save();
  }
}

// Representation of user configuration file (defines remotes for subtrees)
class UserConfig extends Serializable {
  constructor(subtreeConfig) {
    super('remotes', userFile);
    this.config = subtreeConfig;
    this.load();
  }

  addRemote(subtree, remote) {
    // verify the given subtree is something our config knows about
    if (!this.config.subtrees[subtree]) {
      console.log('Unknown subtree: ' + subtree);
      return;
    }
    this.remotes[subtree] = remote;
    this.save();
  }

  removeRemote(subtree) {
    delete this.remotes[subtree];
    this.save();
  }
}

const config = new Config();

// build command list
const subtreeCommands = {
  list: listSubtrees,
  add: addSubtree,
  remove: removeSubtree,
  push: pushSubtree,
  pull: pullSubtree,
  commits: showCommits
}

const remoteCommands = {
  list: listRemotes,
  add: addRemote,
  remove: removeRemote
}

const commands = {
  subtree: subtreeCommands,
  remote: remoteCommands,
}

// run whatever command the user passed in
const args = process.argv.slice(2);
runCommand(args);

// function to traverse command tree and send user input to command-specific function
function runCommand(args) {
  if (!this) {
    // this is the initial call into our recursive function - re-invoke with the top-level
    // 'commands' object as 'this'
    runCommand.call(commands, args);
    return;
  }
  if (args.length === 0) {
    // out of args and we haven't found a command endpoint yet - list out the commands we know about
    console.log('valid commands: ' + Object.keys(this).join(' '));
    return;
  }
  else {
    // do we know about the next tree branch?
    if (this.hasOwnProperty(args[0])) {
      // is it a function we can execute, or the next branch?
      if (typeof this[args[0]] === 'function') {
        // invoke the function with our remaining arguments
        this[args[0]](args.slice(1));
      }
      else {
        // recursively call runCommand, now with the inner command branch as 'this'
        runCommand.call(this[args[0]], args.slice(1));
      }
    }
    else {
      console.log('unknown command: ' + args[0]);
      console.log('valid commands: ' + Object.keys(this).join(' '));
    }
  }
}

// define command functions
function listSubtrees() {
  if (Object.keys(config.subtrees).length === 0) {
    // no subtrees, nothing to print
    return;
  }
  // call listData() with key-value pairs of (subtree name, subtree path)
  listData(Object.keys(config.subtrees).sort().map(key => {
    return { key, val: config.subtrees[key].path };
  }), 'name', 'path');
}

function addSubtree(args) {
  if (args.length < 2) {
    console.log('Usage: subtree add <name> <path>');
    return;
  }
  // remove trailing slash if there is one
  const dir = args[1].replace(/\/+$/, "");
  if (!fs.existsSync(path.join(process.cwd(), dir))) {
    console.log('Could not find path: ' + dir);
    return;
  }
  const subtreeData = getSubtreeData(dir);
  if (!subtreeData) {
    // error already printed
    return;
  }
  console.log('Found info for subtree "' + args[0] + '" in path ' + dir + ':');
  console.log('Added in commit: ' + subtreeData.initial);
  console.log('Remote commit that was merged in: ' + subtreeData.onto);
  config.addSubtree(args[0], subtreeData);
}

function removeSubtree(args) {
  if (args.length < 1) {
    console.log('Usage: subtree remove <name>');
    return;
  }
  config.removeSubtree(args[0]);
}

function pushSubtree(args) {
  if (args.length < 2) {
    console.log('Usage: subtree push <name> <remote-branch>');
    return;
  }
  const subtree = args[0];
  const branch = args[1];
  if (!config.subtrees[subtree]) {
    console.log('No subtree named: ' + subtree);
    return;
  }
  const remote = config.userConfig.remotes[subtree];
  if (!remote) {
    console.log('No remote for subtree: ' + subtree);
    console.log('Run "remote add <subtree> <remote>" to add one');
    return;
  }
  fetchRemote(remote);
  const subtreeData = config.subtrees[subtree];
  const splitCmd = 'git subtree split --prefix=' + subtreeData.path + ' ' + subtreeData.initial + '^.. '
    + '--onto=' + subtreeData.onto;
  console.log('Running command: ' + splitCmd);
  liveExec(splitCmd, (err, splitHash) => {
    // callback when split finishes - splitHash is the object we push to the remote
    if (err) {
      return;
    }
    const pushCmd = 'git push ' + remote + ' ' + splitHash + ':refs/heads/' + branch;
    console.log('Running: ' + pushCmd);
    execSync(pushCmd, { stdio: 'inherit'});
  });
}

function pullSubtree(args) {
  if (args.length < 2) {
    console.log('Usage: subtree pull <name> <remote-branch>');
    return;
  }
  const subtree = args[0];
  const branch = args[1];
  if (!config.subtrees[subtree]) {
    console.log('No subtree named: ' + subtree);
    return;
  }
  const remote = config.userConfig.remotes[subtree];
  if (!remote) {
    console.log('No remote for subtree: ' + subtree);
    console.log('Run "remote add <subtree> <remote>" to add one');
    return;
  }
  fetchRemote(remote);
  const cmd = 'git subtree pull --prefix=' + config.subtrees[subtree].path + ' ' + remote + ' ' + branch + ' --squash';
  console.log('Running command: ' + cmd);
  // 'inherit' option will display this command's output in the console
  try {
    execSync(cmd, { stdio: 'inherit' });
  } catch (err) {}
}

function showCommits(args) {
  if (args.length < 1) {
    console.log('Usage: subtree commits <subtree>');
    return;
  }
  const subtree = args[0];
  const treePath = config.subtrees[subtree] && config.subtrees[subtree].path;
  if (!treePath) {
    console.log('No subtree named: ' + subtree);
    return;
  }
  const remote = config.userConfig.remotes[subtree];
  if (!remote) {
    console.log('No remote for subtree: ' + subtree);
    console.log('Run "remote add <subtree> <remote>" to add one');
    return;
  }
  fetchRemote(remote);

  // step 1: get latest commit with subtree pull info
  const latestLocalSquash = execSyncTrim('git log -1 --grep="^git-subtree-dir: '
    + treePath + '/*\\s*" --pretty=format:"%H"');
  // the main line commit we're interested in first is the one that merged in latestLocalSquash
  const latestLocalMerge = execSyncTrim('git log -1 --grep="' + latestLocalSquash + '" --merges --pretty=format:"%H"');
  // next, the latest remote commit we have is in the commit message for latestLocalSquash
  const latestRemoteHash = execSyncTrim('git log -1 ' + latestLocalSquash + ' | grep "git-subtree-split: "')
    .split(' ').pop();

  const isValidHash = hash => hash && hashRegex.test(hash);
  // make sure all hashes are valid
  if (![latestLocalSquash, latestLocalMerge, latestRemoteHash].every(isValidHash)) {
    console.log('Invalid hash detected, got values...');
    console.log('Most recent subtree update: ' + latestLocalSquash);
    console.log('Merge of that commit: ' + latestLocalMerge);
    console.log('Remote commit last updated to: ' + latestRemoteHash);
    return;
  }
  
  console.log('Commits in remote ' + remote + ' but not in HEAD:');
  console.log('(if commits here look redundant, the subtree was likely not merged back here after a push)\n');
  console.log(execSyncTrim('git log ' + latestRemoteHash + '..' + remote + '/master --oneline'));
  console.log('\nCommits in HEAD ' + treePath + ' but not in remote ' + remote + ':');
  console.log('(note: this lists commits in HEAD after the latest pull from the remote,');
  console.log(' but doesn\'t necessarily mean commits before these were pushed to the remote)\n');
  console.log(execSyncTrim('git log --oneline ' + latestLocalMerge + '.. ' + treePath));
}

function listRemotes(args) {
  if (Object.keys(config.userConfig.remotes).length === 0) {
    // no remotes, nothing to print
    return;
  }
  // call listData() with key-value pairs of (subtree name, remote name)
  listData(Object.keys(config.userConfig.remotes).map(key => {
    return { key, val: config.userConfig.remotes[key] };
  }), 'subtree', 'remote');
}

function addRemote(args) {
  if (args.length < 2) {
    console.log('Usage: remote add <subtree> <remote>');
    return;
  }
  if (!config.subtrees[args[0]]) {
    console.log('No subtree named: ' + args[0]);
    return;
  }
  // todo: check git remote command to verify remote existence?
  config.userConfig.addRemote(args[0], args[1]);
}

function removeRemote(args) {
  if (args.length < 1) {
    console.log('Usage: remote remove <subtree>');
    return;
  }
  config.userConfig.removeRemote(args[0]);
}

function listData(keyValPairs, keyTitle, valTitle) {
  // for formatting, get longest key name
  const colWidth = Math.max(...keyValPairs.map(pair => pair.key.length), 4) + 2;
  // print header
  console.log(keyTitle + ' '.repeat(colWidth - keyTitle.length) + valTitle);
  console.log('-'.repeat(colWidth + valTitle.length));
  // print each line
  keyValPairs.forEach(pair => {
    console.log(pair.key + ' '.repeat(colWidth - pair.key.length) + pair.val);
  });
}

function getSubtreeData(dir) {
  // find the commit where this subtree was first introduced into the repo
  const initialSquashCommand = 'git log --grep="Squashed \'' + dir + '/*\' content from commit" --pretty=format:"%H"';
  const initialSubtreeSquash = execSyncTrim(initialSquashCommand);

  if (!initialSubtreeSquash || !hashRegex.test(initialSubtreeSquash)) {
    console.log('Error: could not find squash commit for path ' + dir);
    return;
  }

  // the commit where the subtree was added is the one that merged initialSubtreeSquash into the tree,
  // and should have that commit hash in its message
  let initialSubtreeAdd = execSyncTrim('git log --grep=' + initialSubtreeSquash + ' --pretty=format:"%H"');
  if (!initialSubtreeAdd || !hashRegex.test(initialSubtreeAdd)) {

    // it's possible to rewrite the merge commit, so try an alternate method to find it before giving up
    // this should give us the earliest commit that merged initialSubtreeSquash in...
    initialSubtreeAdd = execSyncTrim('git log --merges --reverse --ancestry-path ' + initialSubtreeSquash +
      '..HEAD --pretty=format:"%H" | head -1');

    if (!initialSubtreeAdd || !hashRegex.test(initialSubtreeAdd)) {
      console.log('Error: could not find commit that merged initial squash ' + initialSubtreeSquash);
      return;
    }
    // because we had to deploy the emergency method, let's make sure this looks sensible
    console.log('Didn\'t find expected commit that merged in subtree squash, but did find this:\n');
    execSync('git log -1 ' + initialSubtreeAdd, { stdio: 'inherit' });
    console.log('\nPlease make sure the above looks like the right merge commit!\n');
  }

  // last piece of info we need is the remote commit that was brought into our repo
  // this is the "onto" commit that we'll pass into future subtree split commands
  
  // add escape characters for slashes in directory, since it will go in regex
  // from: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Regular_Expressions
  const escapedDir = dir.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const commitRegex = new RegExp("Squashed '" + escapedDir + "/*' content from commit ([a-fA-F0-9]+)");
  const initialSquashLog = execSyncTrim('git log -1 ' + initialSubtreeSquash);
  const matchArray = commitRegex.exec(initialSquashLog);

  if (!matchArray || matchArray.length < 2) {
    console.log('Error: could not parse remote commit info from commit message of: ' + initialSubtreeSquash);
    return;
  }
  const ontoCommit = matchArray[1];

  return {
    path: dir,
    initial: initialSubtreeAdd,
    onto: ontoCommit
  };
}

// spawn a command, print its stdout while it runs, pass its final stdout to callback
// (intended for git subtree split, which takes awhile)
function liveExec(command, callback) {
  const [cmd, ...args] = command.split(' ');
  const childproc = child_process.spawn(cmd, args);
  let lastOutput = '';
  childproc.stdout.on('data', data => {
    process.stdout.write(data);
    lastOutput = data.toString().trim();
  });
  childproc.stderr.pipe(process.stderr);
  childproc.on('exit', () => callback(null, lastOutput));
  childproc.on('error', err => {
    console.log('Subprocess error: ' + err);
    callback(err);
  });
}

// run 'git fetch' for a remote, with some disclaimers
function fetchRemote(remote) {
  console.log('Fetching from ' + remote + '...');
  console.log('(if this remote is on the local disk, it may still be out of date!)');
  execSync('git fetch ' + remote, {stdio: 'inherit'});
  console.log('');
}
