// RUN: env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=$(dirname %s)/$(basename %s .c).yml $TEST_CLANG -fexperimental-new-pass-manager -fpass-plugin=$TEST_PASS -g -S -emit-llvm -O2 -o %t.ll -c %s
// RUN: cat %t.ll | FileCheck %s || exit 1
// test of nested compartment calls

struct data_struct {
    int a;
};

int bar(struct data_struct *b){
    if(b){
        return (b->a)++;
    }
    return (b->a)--; 
}

int foo(struct data_struct *a) {
    if (a) {
        struct data_struct * b;
        b->a = 0;
        return bar(b);
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %2 = call i32 @HAKC_ORIG_foo(%struct.data_struct2* %0)
// CHECK: ret i32 %2

// CHECK-LABEL: HAKC_XFER_bar
// CHECK: %2 = call i32 @HAKC_ORIG_bar(%struct.data_struct* %0)
// CHECK: ret i32 %2

