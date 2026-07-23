# Repository Scope

This repository contains two very large upstream submodules:

- `llvm-project`
- `linux`

The project-specific code is primarily located in:

- `llvm-project/llvm/lib/Transforms/Compartmentalization`
- `llvm-project/llvm/include/llvm/Transforms/Compartmentalization`
- `llvm-project/llvm/utils/hakc`
- `linux/kernel/hakc`
- `linux/include/linux/hakc`
- `python`
- `scripts`
- `config`

Focus on those directories by default.

Do not broadly scan the Linux or LLVM source trees. Access unrelated files only
when required to trace a symbol, understand an interface, or diagnose a build
failure.
