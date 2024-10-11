// RUN: $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -ArchYamlPath=/home/al32163/hakc/HAKC/HAKC-pass/tests/llvm_tests/x86config.yaml -mllvm -CompartmentYamlPath=$(dirname %s)/$(basename %s .c).yml -mllvm -HAKCPassMode=compartmentalize  -g -S -emit-llvm -O2 -o %t.ll -c %s
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
// CHECK: br i1 %2, label %12, label %3

// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %2 = bitcast %struct.data_struct* %0 to i8*
