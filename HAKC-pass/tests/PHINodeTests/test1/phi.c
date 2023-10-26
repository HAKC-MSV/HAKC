void *allocate(int);

int test(int *I) {
    int *Test;
    if (I) {
        Test = I;
    } else {
        Test = (int *) allocate(sizeof(int));
        *Test = 0;
    }

    (*Test)++;
    return *Test;
}