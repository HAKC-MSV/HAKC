Here is an example of a test case that can show erroneous pass behavior when passing pointers to static inline functions.

This is to replicate a situation seen in the FUSE module as follows:
```
struct fuse_conn {
    ...
    atomic64_t khctr;
    ...
};

static inline atomic64_t * some_thing_that_has_inline_asm(atomic64_t * v) {
    asm(...);
    return v;
}

static inline atomic64_t * atomic64_inc_return(atomic64_t * v) {
    return some_thing_that_has_inline_asm(v);
}

void fuse_foo(...) {
    struct fuse_conn * fc = ...;
    ...
    atomic64_inc_return(&fc->khctr);
    ...
}
```
The `atomic64_inc_return` call caused a HAKC-related address failure (manifesting as an `address between user and kernel` error from the kernel).

The cause was the address of the struct member being unsigned, going through a `check_hakc_data_access` call in the code inlined from `atomic64_inc_return` in the body
of `fuse_foo` , resulting in an invalid address that is then dereferenced in a manner similar to
`LDR dest, [src, offset of field]` .


The simplified standalone test case in `field_by_address.c` is as follows:

`foo()` is a function that calls `bar()`

`bar()` is a static inline function (so it follows that it is in the same compilation unit)

`bar()` takes a pointer as an argument

Expected behavior is that when `bar()` is inlined, it should not require a `check_hakc_data_access` call and one should not be generated.

Actual behavior as of 2023/09/19 is that a `check_hakc_data_access` call is emitted before the field gets dereferenced. This is ok if and only if `foo()` is called against
a signed pointer to begin with.

A situation observed while trying to compartmentalize the FUSE module was that functions were being called without data being `hakc_transfer`-ed first, resulting in that field
access failing.




how to reproduce the IR:
```mkdir -p build
env HAKC_ANALYSIS=dag HAKC_DAG_ANALYSIS_ROOT=hakc-dag-analysis \
clang -fexperimental-new-pass-manager -fpass-plugin=path/to/libHAKC-Compartmentalizer.so \
-g -S -emit-llvm -o build/field_by_address.bc -c field_by_address.c

cd build && python3 path/to/ARM-MTE/scripts/data-access-analysis.py -c ../dag.bin -r ../hakc-dag-analysis/ -o ../calls-and-types.bin --dag --filter_types --filter_mod_files
python3 path/to/ARM-MTE/scripts/data-access-analysis.py -c ../dag.bin --output_compart ../hakc-compartments.yml
cd ..
env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=hakc-compartments.yml \
clang -fexperimental-new-pass-manager -fpass-plugin=path/to/libHAKC-Compartmentalizer.so \
-g -S -emit-llvm -o build/field_by_address.bc -c field_by_address.c```
