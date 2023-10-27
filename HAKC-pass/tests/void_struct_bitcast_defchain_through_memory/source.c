//#include <stdlib.h>
extern void *malloc(unsigned long);

struct file {
    int a;
    char b;
    long long c;
};

extern int do_something_with_void_my_struct(void *ms);

int make_my_struct(int x, char y, long long z) {
    struct file *ms = (struct file *)malloc(sizeof(struct file));
    void *vms = (void *)ms;
    ms->a = x;
    ms->b = y;
    ms->c = z;

    return do_something_with_void_my_struct(vms);
}
