#!/bin/sh
cd ../../linux
git checkout -- .
git clean -f -d

files=`ls ../linux-patches/v6.6/*.patch`

for i in $files
do
   :
   git apply $i
done

patch drivers/Makefile ../demo-components/v6.6/drivers/Makefile.patch
patch drivers/Kconfig ../demo-components/v6.6/drivers/Kconfig.patch
patch kernel/Kconfig.hakc ../demo-components/v6.6/kernel/Kconfig.hakc.patch
cp -r ../demo-components/v6.6/drivers/rosdemo/ drivers/
cp -r ../demo-components/v6.6/drivers/tb-exploiter/ drivers/
cp -r ../demo-components/v6.6/include/linux/hakc/pointer-leak-demo include/linux/hakc

exit 0

