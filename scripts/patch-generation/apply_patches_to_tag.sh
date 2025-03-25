#!/bin/sh

git_root=$(git rev-parse --show-toplevel)
tag_name="$GIT_TAG"
cd $git_root/linux
git checkout -- .
git clean -f -d
git checkout $tag_name

files=`ls $git_root/linux-patches/$tag_name/*.patch`

for i in $files
do
   :
   git apply $i
done

patch drivers/Makefile $git_root/demo-components/$tag_name/drivers/Makefile.patch
patch drivers/Kconfig $git_root/demo-components/$tag_name/drivers/Kconfig.patch
patch kernel/Kconfig.hakc $git_root/demo-components/$tag_name/kernel/Kconfig.hakc.patch

cp -r $git_root/demo-components/$tag_name/drivers/rosdemo/ drivers/
cp -r $git_root/demo-components/$tag_name/drivers/tb-exploiter/ drivers/
cp -r $git_root/demo-components/$tag_name/include/linux/hakc/pointer-leak-demo include/linux/hakc

exit 0
