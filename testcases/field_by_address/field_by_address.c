// this is the same way the Linux kernel forces code to actually be inlined
#define inline   inline __attribute__((always_inline))

// simple struct, a is the only field and we pass it around by address
// representative of how atomic64_t passed around from struct fuse_conn
struct data_struct {
        int a;
};

// a static inline function that takes an int*, does some arithmetic on the value pointed to,
// stores updated value back through the int*
static inline void do_something_nested_with(int *b) {
        *b = (*b * 8) + 4;
}

// a static inline function that takes an int* and passes it along to another static inline function
// this nesting of static inline functions is representative of the structure of the atomic64_* functions 
// in atomic.h , atomic_lse.h
static inline void do_something_with(int *b) {
        do_something_nested_with(b);
}

void foo(struct data_struct *a) {
        // if a is non-null
        if(a) {
                // pass &a->a to our static inline function
                // given that this is in the same compilation unit and should even be in the same
                // compartment as the caller (it gets inlined), we would expect that...
                // auth checks should not be necessary and should not be generated
                // the IR shows otherwise
                do_something_with(&a->a);
                return;
        }
        return;
}