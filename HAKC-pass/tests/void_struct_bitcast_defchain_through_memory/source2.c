extern void *malloc(unsigned long);

struct my_struct {
    int a;
    char b;
    long long c;
};

extern int do_something_with_void_my_struct(void *ms);

int make_my_struct(int x, char y, long long z) {
    struct my_struct *ms = (struct my_struct *)malloc(sizeof(struct my_struct));
    void *vms = (void *)ms;
    ms->a = x;
    ms->b = y;
    ms->c = z;

    return do_something_with_void_my_struct(vms);
}
