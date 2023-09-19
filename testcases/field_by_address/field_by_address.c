// force code to actually be inlined
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

// different results depending on how this is called 
// if the call originates from HAKC_XFER_foo, the struct data_struct * gets transferred
//   and so it is signed coming in, and everything in HAKC_ORIG_foo should work alright
// if a direct call to HAKC_ORIG_foo is made, the struct * is not guaranteed to be signed 
//   and if it is not signed, a memory error will occur later on when trying to access 'a'
//   by address because of an auth check from the inlined code
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
