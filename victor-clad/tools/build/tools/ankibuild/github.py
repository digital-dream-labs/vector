import json
import os
import re
import util


def default_access_token():
  return os.getenv('ANKI_BUILD_GITHUB_TOKEN')


# this API is used for both issues and PRs
# for PRs, it will return comments posted on the PR and not specific commits/diffs
# returns comments as json array
def get_issue_comments(access_token, repo, issue_num):
  url = 'https://' + access_token + ':x-oauth-basic@api.github.com/repos/anki/' + repo + '/issues/' + str(issue_num) \
    + '/comments'
  try:
    comments = json.loads(util.File.evaluate(['curl', url]))
    if not isinstance(comments, list):
      print 'Server return value is not a comment list: ' + str(comments)
      return []
    else:
      return comments
  except Exception as err:
    print 'Error trying to get comments: ' + str(err)
    return []


# this API is used for both issues and PRs
# for PRs, it will post a comment on the PR rather than a specific commit/diff
def post_issue_comment(access_token, repo, issue_num, comment):
  url = 'https://' + access_token + ':x-oauth-basic@api.github.com/repos/anki/' + repo + '/issues/' + str(issue_num) \
    + '/comments'
  curl_args = ['curl', '-H', 'Content-Type: application/json', '-X', 'POST', '-d',
               '{"body":' + json.dumps(comment) + '}', url]
  try:
    util.File.execute(curl_args)
  except Exception as err:
    print 'Error trying to post comment: ' + str(err)
    return


# decodes from "1234/head" or "refs/heads/my_branch" to a branch name
# returns None if string doesn't match
def get_branch_from_ref(ref_string):
  # try a couple formats
  # asdf/head
  match = re.match('([a-zA-Z_0-9/]+)/head$', ref_string)
  if match:
    return match.group(1)
  # refs/heads/asdf
  prefix = 'refs/heads/'
  if ref_string.startswith(prefix):
    return ref_string[len(prefix):]
  return None
