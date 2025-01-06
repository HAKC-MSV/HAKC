#!/bin/bash
# to generate patches and ignore certain directories
# current hash is for llvm-project 19. TODO: make this dynamically grab the submodule hash 
# git diff 7ba7d8e2f7b6445b60679da826210cdde29eaf8b --stat -- ':!compiler-rt/*'
source vars.sh 

git_patch='git apply '

cd $HAKC_ROOT/llvm-project

# if first command argument is -R, then revert the patches
if [ "$1" == "-R" ]; then
    # Note: the for loop is a bit picky with how it executes commands, so use a while loop instead with 'read line'
    ls $HAKC_ROOT/llvm-patches/*.hakc.patch | while read patch; do
    :
    echo $patch
    git apply -R $patch 
    done
else
    # Note: the for loop is a bit picky with how it executes commands, so use a while loop instead with 'read line'
    ls $HAKC_ROOT/llvm-patches/*.hakc.patch | while read patch; do
    :
    echo $patch
    git apply $patch 
    done
fi

exit 0
