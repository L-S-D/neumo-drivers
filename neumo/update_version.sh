#!/bin/bash
dir=.
rev=`git --git-dir ./.git log --pretty=format:'%h' -n 1`
tag=`git --git-dir $dir/.git describe --exact-match --tags`
branch=`git --git-dir $dir/.git rev-parse --abbrev-ref HEAD`

versionfile=neumo/dvb-core/neumo-version.c

file=`mktemp`
cat >$file << EOF
#include <media/neumo-version.h>
#include <linux/module.h>
void neumo_git_versions(const char ** neumo, const char ** rev, const char ** tag, const char ** branch)
{
	//neumo_version_string; this comment is needed to  make version_patch.pl work
	*rev="GIT-REV = \"$rev\";";
	*tag="GIT-TAG = \"$tag\";";
	*branch="GIT-BRANCH = \"$branch\";";
	*neumo  = "type = \"neumo\";\nversion = \"2.1\";";
}

EXPORT_SYMBOL(neumo_git_versions);

EOF
if [[ -a $versionfile ]]
then
    touch $versionfile
fi

cmp $file $versionfile
if [ $? -ne 0 ]
then
    echo "Updating $versionfile"
    cp $file $versionfile
else
    echo  "Not updating $versionfile"
fi
