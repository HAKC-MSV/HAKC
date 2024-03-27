#!/bin/sh
cd ../../linux
git checkout -- .
git clean -f -d

files=`ls ../linux-patches/v5.15/*.patch`

for i in $files
do
   :
   git apply $i
done

patch drivers/Makefile ../demo-components/v5.15/drivers/Makefile.patch
patch drivers/Kconfig ../demo-components/v5.15/drivers/Kconfig.patch
patch kernel/Kconfig.hakc ../demo-components/v5.15/kernel/Kconfig.hakc.patch
cp -r ../demo-components/v5.15/drivers/rosdemo/ drivers/
cp -r ../demo-components/v5.15/drivers/tb-exploiter/ drivers/
cp -r ../demo-components/v5.15/include/linux/hakc/pointer-leak-demo include/linux/hakc

exit 0

