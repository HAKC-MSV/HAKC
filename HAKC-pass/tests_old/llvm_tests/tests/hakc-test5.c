// RUN: $TEST_CLANG -I $ROOT/linux/tools/include -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKC_ARCH_CONFIG=$ROOT/HAKC-pass/config/linux_x86_config.yaml -mllvm -HAKC_COMPARTMENT_PATH=$(dirname %s)/$(basename %s .c).yml -mllvm -HAKC_ANALYSIS=compartmentalize  -g -S -emit-llvm -O2 -o %t.ll -c %s
// RUN: cat %t.ll | FileCheck %s || exit 1

// testing function that allocates variable sized memory is tagged correctly 
#include <linux/slab.h>         // kmalloc()

inline void *kmalloc(size_t size, gfp_t gfp);

int foo() {
    int * mem; 
    mem = (int *) kmalloc(2<<8, GFP_KERNEL);
    for(int i = 0; i < 2<<8; ++i){
        *(mem + i) = (int) i;
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: foo
// CHECK: %1 = tail call i8* @kmalloc(i64 512, i32 3264) #3
// CHECK: %2 = icmp ugt i8* %1, inttoptr (i64 281474976710655 to i8*)
