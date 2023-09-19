Here is an example of a test case that can show erroneous pass behavior when passing pointers to static inline functions.

mkdir -p build
env HAKC_ANALYSIS=dag HAKC_DAG_ANALYSIS_ROOT=hakc-dag-analysis \
clang -fexperimental-new-pass-manager -fpass-plugin=path/to/libHAKC-Compartmentalizer.so \
-g -S -emit-llvm -o build/field_by_address.bc -c field_by_address.c

cd build && python3 path/to/ARM-MTE/scripts/data-access-analysis.py -c ../dag.bin -r ../hakc-dag-analysis/ -o ../calls-and-types.bin --dag --filter_types --filter_mod_files
python3 path/to/ARM-MTE/scripts/data-access-analysis.py -c ../dag.bin --output_compart ../hakc-compartments.yml
cd ..
env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=hakc-compartments.yml \
clang -fexperimental-new-pass-manager -fpass-plugin=path/to/libHAKC-Compartmentalizer.so \
-g -S -emit-llvm -o build/field_by_address.bc -c field_by_address.c
