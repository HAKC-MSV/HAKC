struct file {
    int a;
    char b;
    long long c;
};

int __attribute__ ((noinline)) do_something_with_my_struct(struct file *ms) {
    return (ms->a + ((int) ms->b) * (int) ms->c);
}

int __attribute__ ((noinline)) do_something_with_void_my_struct(void *vms) {
    struct file *ms = (struct file *) vms;
    ms->a = ms->a + 1;
    return do_something_with_my_struct(ms);
}
