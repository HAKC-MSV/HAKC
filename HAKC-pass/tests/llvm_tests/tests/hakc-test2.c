// RUN: env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=$(dirname %s)/$(basename %s .c).yml $TEST_CLANG -fexperimental-new-pass-manager -fpass-plugin=$TEST_PASS -g -S -emit-llvm -O2 -o %t.ll -c %s
// RUN: cat %t.ll | FileCheck %s || exit 1

struct linked_list {
    struct linked_list *next;
};

struct data {
    struct linked_list list;
    void *data;
};

void init_data(struct data *data) {
    data->data = 0;
    data->list.next = &data->list;
}


// CHECK-LABEL: HAKC_ORIG_init_data
// CHECK: %3 = call i8* @check_hakc_data_access(i8* %2, i32 6, i64 393218)
// CHECK-LABEL: HAKC_XFER_init_data
// CHECK: %5 = call i8* @hakc_transfer_to_clique(i8* %4, i64 16, i32 6, i32 241, i1 false)
