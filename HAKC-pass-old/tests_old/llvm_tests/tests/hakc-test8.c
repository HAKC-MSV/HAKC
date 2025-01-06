// RUN: $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKC_ARCH_CONFIG=$ROOT/HAKC-pass/config/linux_x86_config.yaml -mllvm -HAKC_COMPARTMENT_PATH=$(dirname %s)/$(basename %s .c).yml -mllvm -HAKC_ANALYSIS=compartmentalize  -g -S -emit-llvm -O2 -o %t.ll -c %s
// RUN: cat %t.ll | FileCheck %s || exit 1
// test of GetIgnoredGlobals

int* kmalloc_caches = 53;
int* somevar = 53;

struct data_struct {
    int a;
};

struct data_struct2 {
    int (*f)(struct data_struct *);
};

int bar(){
    struct data_struct2 ds2; 
    foo(&ds2, kmalloc_caches, somevar);
}

int foo(struct data_struct2 *a, int* v1, int* v2) {
    if (a) {
        *v1++;
        *v2++;
        struct data_struct b;
        b.a = 0;
        return a->f(&b);
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %7 = call i8* @hakc_transfer_to_clique