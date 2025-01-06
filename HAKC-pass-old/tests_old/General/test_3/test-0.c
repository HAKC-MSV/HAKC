// testing static branch
int static_branch_bar(){
    int a = 0; 
    return a;
}

int foo() {
    static_branch_bar();
    return 0;
}