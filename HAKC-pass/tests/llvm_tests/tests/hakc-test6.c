// RUN: $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKC_ARCH_CONFIG=$ROOT/HAKC-pass/config/linux_x86_config.yaml -mllvm -HAKC_COMPARTMENT_PATH=$(dirname %s)/$(basename %s .c).yml -mllvm -HAKC_ANALYSIS=compartmentalize  -g -S -emit-llvm -O2 -o %t.ll -c %s
// RUN: cat %t.ll | FileCheck %s || exit 1

// testing static branch
struct data_struct {
    int a;
};

int static_branch_foo(struct data_struct* ds) {
    if(ds){
        return 1; 
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_static_branch_foo
// CHECK: %2 = call i32 @HAKC_ORIG_static_branch_foo(%struct.data_struct* %0)
// CHECK: ret i32 %2