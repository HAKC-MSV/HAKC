struct data_struct {
    int* a;
};

int glob;

struct data_struct baz = {
        .a = &glob
};

void bar(struct data_struct *);

int foo(int i) {
    bar(&baz);
    return *baz.a;
}
