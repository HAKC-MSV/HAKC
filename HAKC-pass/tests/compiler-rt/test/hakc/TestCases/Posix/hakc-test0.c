// RUN: source ../vars.sh
// RUN: source ../set_env.sh
// run build dag
// RUN: $TEST_CLANG -fpass-plugin=$TEST_PASS -Xclang -load -Xclang $TEST_PASS -mllvm -HAKC_CONFIG=$(dirname %s)/$(basename %s .c)-dag.yml -g -S -emit-llvm -O2 -o %t.ll -c %s
// run build hakc-db
// python3 $ROOT/python/analysis/hakc-dag.py --db-dir hakc-dag-analysis/hakc-db --dag-files-root $HAKC_DAG_ROOT --core-count 1
// env PYTHONPATH=$(realpath $ROOT/kuzu/tools/python_api/build) python $ROOT/python/analysis/hakc-dag.py --log-level INFO --dag-files-root $PWD --db-dir $PWD/hakc-db --create-dag --single-thread
// run adjust hakc-db
// env PYTHONPATH=$(realpath $ROOT/kuzu/tools/python_api/build) python $ROOT/python/analysis/hakc-dag.py --log-level INFO --db-dir $PWD/hakc-db --adjust --adjust-path adjustments.yml
// run compartmentalization
// RUN: $TEST_CLANG -fpass-plugin=$TEST_PASS -Xclang -load -Xclang $TEST_PASS -mllvm -HAKC_CONFIG=$(dirname %s)/$(basename %s .c)-comp.yml -g -S -emit-llvm -O2 -o %t.ll -c %s
// run checks
// RUN: cat %t.ll | FileCheck %s || exit 1

struct data_struct {
    int a;
};

int bar(struct data_struct *);

int foo(struct data_struct *a) {
    if (a) {
        (a->a)++;
        return bar(a);
    }
    return 0;
}
// todo: add better checking

// CHECK-LABEL: HAKC_ORIG_foo
// CHECK: %2 = icmp eq %struct.data_struct* %0, null

// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %5 = call i8* @hakc_transfer_to_clique(i8* %4, i64 4, i32 6, i32 241, i1 false)
