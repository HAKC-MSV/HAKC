Symbols belong to compartments
Pointers passed from one function to another must be resigned before use


<!-- @formatter:off -->

```c++
void *hakc_transfer_to_compartment(void *data_to_transfer, int64_t size, int64_t compartment, int32_t division);
```
The function 'hakc_tranfer_to_compartment' transfers data to a specific compartment and division. 

Example 1.
.c code
```c++
void bar(void* a); 

void foo() {
    void* a = kmalloc(...); 
    bar(a); 
}
```

```
  ┌─────┐         ┌─────┐
  │ foo ├────────►│ bar │
  └──┬──┘         └──┬──┘
     │               │
     │   ┌───────┐   │
     └──►│DivID 0│◄──┘
         └───┬───┘
             │
             │
             ▼
         ┌────────┐
         │CompID 0│
         └────────┘
```

Since kmalloc is called in foo, which is in the NEC, no transfer will be called.
Since foo and bar are both in the No Enforcement Compartment (NEC) no transfer will occur. 


```
  ┌─────┐         ┌─────┐
  │ foo ├────────►│ bar │
  └──┬──┘         └──┬──┘
     │               │
     │   ┌───────┐   │
     └──►│DivID 1│◄──┘
         └───┬───┘
             │
             │
             ▼
         ┌────────┐
         │CompID 1│
         └────────┘
```

Since kmalloc is called in foo, which is not in the NEC, a transfer must be called.
Foo and bar are not in the NEC, so they are eligible for a transfer.
Since foo and bar are both in the same compartment and division, a transfer is not necessary.
The HAKC implementation may still insert a transfer that is redundant.

note: transfer after kmalloc, but not for bar since same compartment

```
  ┌─────┐         ┌─────┐
  │ foo ├────────►│ bar │
  └──┬──┘         └──┬──┘
     │               │
     ▼               ▼
 ┌───────┐       ┌───────┐   
 │DivID 1│       │DivID 2│  
 └───┬───┘       └───┬───┘
     │               │
     │               │
     ▼               ▼
 ┌────────┐      ┌────────┐
 │CompID 1│      │CompID 2│
 └────────┘      └────────┘
```

Since kmalloc is called in foo, which is not in the NEC, a transfer must be called.
Foo and bar are not in the NEC, so they are eligible for a transfer.
Since foo and bar are in different compartments and divisions, a transfer must be inserted.

### Temporal 

```c++
struct struct_type {
	int (*fptr)(void);
	int string_len;
	char *string;
};

void bar(void* a); 

void do_call(struct struct_type *a) {
    a->fptr();
}

void init() {
    void* a = kmalloc(sizeof(struct_type), GFP_KERNEL); 
    a->fptr=&bar; 
    a->string_len = 8;
    a->string = kmalloc(sizeof(char)*a->string_len, GFP_KERNEL);
    do_call(a);
}


```


```
Pseudo IR:

void init() {
    void* a = kmalloc(sizeof(struct_type), GFP_KERNEL);
    void *hakc_a = hakc_transfer_to_clique(a, sizeof(struct_type), COMP_1, DIV_1);
    a->fptr=&bar;
    a->string_len = 8;
    a->string = kmalloc(sizeof(char)*a->string_len, GFP_KERNEL);
    // transferring string to same compartment as parent symbol
    a->string = hakc_transfer_string(a->string, COMP_1, DIV_1);
    HAKC_XFER_do_call(hakc_a); 
}

void HAKC_ORIG_do_call(struct struct_type *a) {
    a = check_hakc_data_access(a, COMP_2, DIV_2);
    a->fptr();
}

void HAKC_XFER_do_call(struct struct_type *a) {
    orig_color = get_hakc_address_color(a);
    a = hakc_transfer_to_clique(a, sizeof(*a), COMP_2, DIV_2);
    HAKC_ORIG_do_call(a);
    hakc_color_address(a, orig_color, sizeof(*a));
}


```

<!-- @formatter:on -->
