# Victor Branch Management

Created by David Mudie Last updated Aug 09, 2018

Everything you wanted to know about Victor Branch Management but were afraid to ask.

## Test Rail

The official release checklist lives on https://ankitest.testrail.com. Talk to IT(?) if you need access.

## Branch Process
Producers determine which master PR or build number will be used as the branch point.

Build manager, or someone from the build team, will branch from master to release branch (usually release/candidate).

After branching, version numbers on master should be bumped to next planned release.

Create a new tab on the spreadsheet by copying the generic 'template' tab. The template tab has formatting macros to simplify approvals.

## Branch Scripts

Victor branch scripts can be found in https://github.com/anki/victor/tree/master/lib/util/tools/branch-scripts.

See https://github.com/anki/victor/tree/master/lib/util/tools/branch-scripts/README.md for more information.

## Tracking Process

Best practices: Note target version on the spreadsheet to identify which tickets need attention.

Best practices: Use a dashboard like COZMO 2.1.0 Release to watch for tickets that have passed testing on master but need to be cherry-picked for release branch.

## Approval Process
Commits should be approved by Producers, Engineering, and QA before they are cherry-picked to the release branch.

When a commit is approved for release, drag the row down to "APPROVED" so it is easy to spot.

If a commit is not approved or is not intended for release, drag the row down to "DENIED" so you don't have to keep looking at it.  If you remove the row completely, it will be added again by the update script.

Best practices: Changes should be committed to master and tested on master before they are considered for cherry-picking.

Best practices: If you don't have confidence about approving a change, talk to the developer.

## Cherry-Pick Process

Most of the time, you can cherry-pick from master directly to the release branch. You can do this using Sourcetree or 'victor-branch-cherrypick.sh'.

If automatic merge fails, you can open a PR branch to make changes by hand, do a test build, or get review from the developer.

If automatic merge fails and you don't know how to resolve the conflict, ask the developer to create a PR branch for the merge.

If you cherry-pick by hand, use 'git cherry-pick -x' to include a reference to the original SHA.

See also https://git-scm.com/docs/git-cherry-pick and https://git-scm.com/docs/git-merge.

Best practices: Try to preserve order of commits. If you cherry-pick out of order, you may overwrite changes that were already committed.

Best practices: Do all cherry-pick operations in a CLEAN WORKSPACE, separate from your normal development. This will reduce the chance of accidentally merging unexpected changes.

## Release Builds
Start builds before 5pm so they will be available for overnight testing.

When builds are ready, update tickets with new build number so QA knows what can be tested.

Best practices: Note build numbers on spreadsheet 

## Version Numbers
Version numbers should be set using the official helper script:

project/build-scripts/update_version.sh

An example of the change set will look like this:

https://github.com/anki/cozmo-one/pull/6832

## Related Pages
Cozmo Branch Management

