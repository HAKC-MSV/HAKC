Chromium patches
----------------

0. `export CHROME_VERS=105.0.5195.125`
1. `export CHERI_PATCHES_PATH=$(realpath /path/to/ARM-MTE/cheri-patches)`
2. `export LLVM_13_INSTALL=$(realpath /path/to/llvm-system/builid-13.x/install)`
3. `git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git`
4. `export PATH="$PATH:$(realpath depot_tools)"`
5. `mkdir chromium && cd chromium`
6. `fetch --nohooks chromium`
7. `cd src`
8. `git checkout -b cheri $CHROME_VERS`
9. `gclient sync --with_branch_heads --with_tags`
10. `cd v8`
11. `git apply $CHERI_PATCHES_PATH/chromium/0001-getting-chrome-to-compile-with-clang-13.patch`
12. `cd ..`
13. `git apply $CHERI_PATCHES_PATH/chromium/0002-adding-hakc-build-options-and-getting-chrome-to-buil.patch`
15. `./build/install-build-deps.sh`
16. `gclient runhooks`
17. `gn gen out/HAKC` 
18. `echo "clang_base_patch = $LLVM_13_INSTALL" >> out/HAKC/args.gn`
19. `echo "hakc_pass_path = $(realpath CHERI_PATCHES_PATH/../build/HAKC_Compartmentalizer/lib/libHAKC-Compartmentalizer.so)` >> out/HAKC/args.gn`
20. `echo "hakc_dag_analysis = true" >> out/HAKC/args.gn`
21. `echo "clang_use_chrome_plugins = false" >> out/HAKC/args.gn`
22. `echo enable_nacl = false` >> out/HAKC/args.gn`
23. `autoninja -C out/HAKC chrome`
