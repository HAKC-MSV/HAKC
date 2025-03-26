# HAKC Compiler Pass Documentation

After building LLVM, the HAKC pass is used by either of the following:

* `clang -g -mllvm --enable-hakc -mllvm 
--hakc-config=<path/to/compartmentalization/config> <path/to/source>`
    * The pass requires debug information to properly find types
* `opt -passes=hakc --hakc-config=<path/to/compartmentalization/config> <path/to/LLVM/IR>`
