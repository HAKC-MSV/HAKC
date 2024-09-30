// RUN: env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=$(dirname %s)/$(basename %s .c).yml $TEST_CLANG -fexperimental-new-pass-manager -fpass-plugin=$TEST_PASS -g -S -emit-llvm -O2 -o %t.ll -c %s
// RUN: cat %t.ll | FileCheck %s || exit 1

struct data_struct {
    int a;
};

struct data_struct2 {
    int (*f)(struct data_struct *);
};

int foo(struct data_struct2 *a) {
    if (a) {
        struct data_struct b;
        b.a = 0;
        return a->f(&b);
    }
    return 0;
}

// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %7 = call i32 @HAKC_ORIG_foo(%struct.data_struct2* %6)
