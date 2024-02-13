# Linux Kernel HAKC Documentation

## Introduction

The HAKC API is defined in [include/linux/hakc/hakc.h](../../linux/include/linux/hakc/hakc.h).  The most important 
functions are the transfer and validation functions:

- Transfer Functions
  - `hakc_transfer_to_clique`
- Validation Functions
  - `check_hakc_data_access`
  - `check_hakc_code_access`

The transfer functions encrypt pointers with the Compartment context, which makes the pointer invalid when used 
without decryption.  The validation functions compute a candidate Compartment context that should have been used 
when encrypting the pointer, and attempts to decrypt the pointer with the candidate Compartment context.  If the 
pointer was actually signed with the candidate Compartment context, the decryption succeeds, and the resulting 
pointer will be valid for dereference.  Otherwise, another invalid pointer will be the result, and no memory access 
can occur.  HAKC relies on tagging memory for the correct computation of the Compartment context, so before HAKC 
functions execute, the relevant memory must be tagged correctly.

## Initializing HAKC tags for code and data in the core kernel

Unlike kernel modules, which get loaded after the kernel is running, the code and data in the core kernel image is 
loaded by the boot loader.  We can't control the boot loader, so we have to tag compartmentalized code and data 
after the kernel is loaded and executing.

In order to accomplish tagging 
