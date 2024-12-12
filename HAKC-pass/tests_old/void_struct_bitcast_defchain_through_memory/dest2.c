struct my_struct {
    int a;
    char b;
    long long c;
};

int __attribute__ ((noinline)) do_something_with_my_struct(struct my_struct *ms) {
    return (ms->a + ((int) ms->b) * (int) ms->c);
}

int __attribute__ ((noinline)) do_something_with_void_my_struct(void *vms) {
    struct my_struct *ms = (struct my_struct *) vms;
    return do_something_with_my_struct(ms);
}
