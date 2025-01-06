#!/bin/bash
# to generate patches and ignore certain directories
# current hash is for llvm-project 19. TODO: make this dynamically grab the submodule hash 
# git diff 7ba7d8e2f7b6445b60679da826210cdde29eaf8b --stat -- ':!compiler-rt/*'
source vars.sh 

short_hash=${HAKC_LLVM_COMMIT:0:8}

# TODO: merge with Derrick's patch generation 
# using .hakc.patch so Derrick's existing patches aren't removed 
rm -rf $HAKC_LLVM_PATCH_PATH/*.hakc.patch

cd $HAKC_LLVM_SOURCE_PATH
git add -A
for fname in $(git diff --name-only $HAKC_LLVM_COMMIT); do
   patch_name=${fname//"./"/""} # strip ./
   patch_name=${patch_name//"/"/"_"} # replace / with _
   patch_name=${patch_name//"."/"_"} # replace . with _
   exec_cmd_and_check_status git diff $HAKC_LLVM_COMMIT $fname > $HAKC_LLVM_PATCH_PATH/"$patch_name""_""$short_hash".hakc.patch
done

exit 0
