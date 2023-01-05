# Get the tracking branch (if we're on a branch)
TRACKING_BRANCH=`git svn info | grep URL | sed -e 's/.*\/branches\///'`

# If the tracking branch has 'URL' at the beginning, then the sed wasn't successful
# and we'll fall back to the svn-remote config option
if [[ "$TRACKING_BRANCH" =~ URL.* ]]
then
        TRACKING_BRANCH=`git config --get svn-remote.svn.fetch | sed -e 's/.*:refs\/remotes\///'`
fi

REV=`git svn find-rev $(git rev-list --date-order --max-count=1 $TRACKING_BRANCH)`
#REV=`git svn info | grep 'Last Changed Rev:' | sed -E 's/^.*: ([[:digit:]]*)/\1/'`
git diff --no-prefix --no-indent-heuristic $(git rev-list --date-order --max-count=1 $TRACKING_BRANCH) $* |
    -e "/^deleted file mode [0-9]\+$/d"