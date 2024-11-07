struct data_struct {
    struct proto *prot;
};

int bar(struct data_struct *);

int foo(struct data_struct *a) {
    if (a) {
        (a->prot) = 0;
        return bar(a);
    }
    return 0;
}