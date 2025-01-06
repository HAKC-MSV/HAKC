struct data_struct {
    int* a;
};

int bar(struct data_struct *);

int foo(int i) {
    struct data_struct a;
    bar(&a);
    return *a.a;
}
