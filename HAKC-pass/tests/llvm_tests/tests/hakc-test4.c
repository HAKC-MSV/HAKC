// RUN: env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=$(dirname %s)/$(basename %s .c).yml $TEST_CLANG -fexperimental-new-pass-manager -fpass-plugin=$TEST_PASS -g -S -emit-llvm -O2 -o %t.ll -c %s
// RUN: cat %t.ll | FileCheck %s || exit 1

struct data_struct {
    int a;
};

int bar(struct data_struct *);

// dummy function named after function that is in GetNoTransferFunctions
// should work on all platforms and operating systems 
int ftrace_stub(struct data_struct *a) {
    if (a) {
        (a->a)++;
        return bar(a);
    }
    return 0;
}

// CHECK-NOT: HAKC_XFER
