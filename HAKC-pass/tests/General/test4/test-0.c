struct data_struct {
    int a;
};

void *hakc_transfer_to_clique(void *, unsigned long, int, int, char);

void* foo(struct data_struct *a) {
    return hakc_transfer_to_clique(a, sizeof(struct data_struct), 0, 0, 0);
}