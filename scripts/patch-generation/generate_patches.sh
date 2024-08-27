#!/bin/bash

git_root=$(git rev-parse --show-toplevel)
curr_dir=$PWD
source_dir=$git_root/"linux"
version_commit="$VERSION_COMMIT"
short_hash=${version_commit:0:8}

cd $source_dir

git add -A

for i in `git diff --name-only $version_commit`
do
   :
   patch_name=${i//"./"/""}
   patch_name=${patch_name//"/"/"_"}
   patch_name=${patch_name//"."/"_"}
   git diff $version_commit $i > $curr_dir/"$patch_name""_""$short_hash".patch
done
exit 0
