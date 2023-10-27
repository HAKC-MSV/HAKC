HAKCPass
=======


HAKCPass is an LLVM pass that attempts to instrument as much of the Linux kernel
as possible with the PAC-MTE checks.

Prerequisites
------------

* Binutils 2.33.1+,
* aarch64-linux-gnu
* Clang 12 built with the patch in HAKC-Annotator

To Build LLVM Pass
------------------

1. `cd ARM-MTE`
1. `mkdir build-linux-armv9`
1. `cd build-linux-armv9`
1. `cmake -DLT_LLVM_INSTALL_DIR=/path/to/built/clang/install
   -DPMC_LLVM_VERSION=12.0.1 .. -DHAKC_LINUX_ARMV9=True`
1. `cmake --build .`

There are other build types for the pass. Replace `HAKC_LINUX_ARMV9` with one of the following:

1. `HAKC_LINUX_ARMV8`
2. `HAKC_LINUX_X86`
3. `HAKC_CHERIBSD`
4. `HAKC_LINUX_ARMV9`

To Use while compiling the Linux kernel
---------------------------------------

1. Make sure the MTE annotated kernel is available. It can be retrieved
   {https://github.mit.edu/inherently-secure/MTE-kernel}[here].
2. cd `MTE-kernel`
3. The pass does one of three functionalities: Produce data for automated
   compartmentalization (`dag`), produce symbol information for user specified
   compartmentalization (`symbols`), and actually produce compartmentalized kernel modules (
   `compartmentalize`). These are determined by setting an environment variable
   `HAKC_ANALYSIS`. E.g., `export HAKC_ANALYSIS=symbols` to generate symbol
   information for user compartmentalization.
4. `export BUILD_TYPE=hakc-build`
5. `env HAKC_ANALYSIS=dag HAKC_DAG_ANALYSIS_ROOT=$(realpath $BUILD_TYPE) make ARCH=arm64
   CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 CC=/path/to/built/clang-12 HOSTCC=/path/to/built/clang-12
   -j$(nproc) O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE menuconfig`
6. Enable `HAKC` and Debug Symbols, and set the `HAKC_PASS_PATH` to the path to your build HAKC Pass from above.
5. `env HAKC_ANALYSIS=dag HAKC_DAG_ANALYSIS_ROOT=$(realpath $BUILD_TYPE) make ARCH=arm64
   CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 CC=/path/to/built/clang-12 HOSTCC=/path/to/built/clang-12
   -j$(nproc) O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE`

To boot PMC Protected kernel
----------------------------

1. Build QEMU from latest main branch to get MTE support
1. Follow the `Emulate with QEMU` directions in rpi-setup/README.md

Running the GUI
--------------

To run the gui, run `python3 ../compartment-display.py` from
MTE-kernel/pmc-build.

<!-- In the bottom of the file, in line
`menu = MenuWidget(yaml_in_sorted, yaml_out)`
you can replace `yaml_in_sorted` by any yaml file you would like to load. This file can contain color and compartment assignments, and they will be loaded accordingly into de gui. (TODO cortegap: NEEDS TO BE UPDATED) -->

You can replace `yaml_out` by wherever you would like to store the yaml output.
If you do so, remember to also modify `yaml_file` in `HAKCPass.cpp`, since that
is where information will be loaded from. After selecting the features in the
gui, build the pass (see `To Build LLVM Pass`) and compile your files.
