struct data_struct {
    int a;
};

void bar(int *);

void foo(struct data_struct *a) {
    if (a) {
        bar(&a->a);
        return;
    }
    return;
}
