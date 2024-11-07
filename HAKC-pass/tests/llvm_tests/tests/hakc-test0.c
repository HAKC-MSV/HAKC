// RUN: $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKC_ARCH_CONFIG=$ROOT/HAKC-pass/config/linux_x86_config.yaml -mllvm -HAKC_COMPARTMENT_PATH=$(dirname %s)/$(basename %s .c).yml -mllvm -HAKC_ANALYSIS=compartmentalize  -g -S -emit-llvm -O2 -o %t.ll -c %s
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
