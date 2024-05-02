struct data_struct {
    int* a;
};

int glob;

/* current_task is the name of an ignored global, so nothing should happen with objects derived from it */
struct data_struct current_task = {
        .a = &glob
};

void bar(struct data_struct *);

int foo(int i) {
    bar(&current_task);
    return *current_task.a;
}
