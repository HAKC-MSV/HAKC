#/bin/sh
source vars.sh

./init.sh # initialize git submodules 
./copy_files.sh # copy over HAKC pass files to llvm-project 
./apply_patches.sh # apply the required for HAKC pass patches to llvm-project 
./build_llvm.sh # build HAKC pass, tests, and llvm-project together 
./run_tests.sh # execute HAKC tests to ensure that build succeeded
