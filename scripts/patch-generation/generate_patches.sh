#!/bin/bash

git_root=$(git rev-parse --show-toplevel)
curr_dir=$PWD
source_dir=$git_root/"linux"
version_commit="$VERSION_COMMIT"
short_hash=${version_commit:0:8}
new_files=("./arch/arm64/configs/hakc_armv8_defconfig" "./arch/arm64/configs/hakc_armv9_defconfig"
"./arch/x86/configs/hakc_x86_defconfig" "./arch/arm64/Kconfig.hakc" "./arch/arm64/kernel/hakc/armv8/Makefile"
"./arch/arm64/kernel/hakc/armv8/hakc_neon.c" "./arch/arm64/kernel/hakc/armv9/Makefile"
"./arch/arm64/kernel/hakc/armv9/hakc_pac_mte.c" "./arch/x86/Kconfig.hakc"
"./arch/x86/kernel/hakc/Makefile" "./arch/x86/kernel/hakc/hakc_ni.c"
"./include/linux/hakc/hakc.h" "./include/linux/hakc/hakc-defs.h" "./kernel/Kconfig.hakc"
"./kernel/hakc/Makefile" "./kernel/hakc/hakc_common.c" "./kernel/hakc/hakc_noarch_tag_btree.c"
"./kernel/hakc/hakc_noarch_tag_memory.c" "./kernel/hakc/hakc_global_init.c" "./include/linux/hakc/hakc-globals.h")

cd $source_dir

git diff $version_commit ${new_files[@]} > $curr_dir/new_hakc_files_$short_hash.patch

for i in `grep -R -e "HAKC" -e "hakc" -l . | grep -v ".git" | grep -v ".sh"`
do
   :
   inarray=$(echo ${new_files[@]} | grep -ow "$i" | wc -w)
   if [ $inarray -eq 0 ];
   then
     patch_name=${i//"./"/""}
     patch_name=${patch_name//"/"/"_"}
     patch_name=${patch_name//"."/"_"}
     git diff $version_commit $i > $curr_dir/"$patch_name""_""$short_hash".patch
   fi
done
exit 0
