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