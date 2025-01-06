#!/bin/bash
# to generate patches and ignore certain directories
# current hash is for llvm-project 19. TODO: make this dynamically grab the submodule hash 
# git diff 7ba7d8e2f7b6445b60679da826210cdde29eaf8b --stat -- ':!compiler-rt/*'
source vars.sh 

version_commit="7ba7d8e2f7b6445b60679da826210cdde29eaf8b"
short_hash=${version_commit:0:8}
git_gen_diff_files='git diff --name-only 7ba7d8e2f7b6445b60679da826210cdde29eaf8b -- :^compiler-rt/lib/hakc/* :^compiler-rt/test/hakc/* :^llvm/lib/hakc/* :^llvm/include/llvm/hakc/*'
git_gen_diff_patch='git diff 7ba7d8e2f7b6445b60679da826210cdde29eaf8b -- :^compiler-rt/lib/hakc/* :^compiler-rt/test/hakc/* :^llvm/lib/hakc/* :^llvm/include/llvm/hakc/*'

# TODO: merge with Derrick's patch generation 
# using .hakc.patch so Derrick's existing patches aren't removed 
rm -rf $HAKC_ROOT/llvm-patches/*.hakc.patch

cd $HAKC_ROOT/llvm-project

git add -A

# Note: the for loop is a bit picky with how it executes commands, so use a while loop instead with 'read line'
$git_gen_diff_files | while read fname; do
   :
   # need special git diff command to exclude the source code that we copied over 
   # goal is to generate patches for code that is already in llvm-project, not new source files (though this may change)
   patch_name=${fname//"./"/""} # strip ./
   patch_name=${patch_name//"/"/"_"} # replace / with _
   patch_name=${patch_name//"."/"_"} # replace . with _ 
   echo $patch_name
   git diff $version_commit $fname > $HAKC_ROOT/llvm-patches/"$patch_name""_""$short_hash".hakc.patch
done

exit 0
