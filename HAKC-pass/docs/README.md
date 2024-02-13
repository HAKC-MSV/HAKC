# HAKC Design Document

This document details the design of the HAKC Compiler Pass and how HAKC works in the Linux kernel.  The compiler 
pass is used for compiling code, and requires LLVM 12 to function correctly.  We try to follow the Long Term Support 
kernel versions for the Linux kernel, and we maintain a patch set between the latest LTS release and our changes to 
add in HAKC support.

Table of Contents:

* [Compiler Pass Documentation](hakc-compiler-pass.md)
* [Linux Kernel Documentation](hakc-in-linux.md)
