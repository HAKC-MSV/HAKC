// Allan Nesathurai 09/18/2024
struct data_struct {
    int a;
};

int bar(struct data_struct *);

// dummy function named after function that is in GetNoTransferFunctions
// should work on all platforms and operating systems 
int ftrace_stub(struct data_struct *a) {
    if (a) {
        (a->a)++;
        return bar(a);
    }
    return 0;
}