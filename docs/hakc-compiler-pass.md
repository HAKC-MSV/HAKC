# HAKC Compiler Pass Documentation

The source for the HAKC pass is in
[
`llvm-project/llvm/lib/Transforms/Compartmentalization/hakc`](../llvm-project/llvm/lib/Transforms/Compartmentalization/hakc).

The HAKC pass communicates with the policy server over a named socket, provided in the
policy server config as `socket_path` and in the HAKC config as `Database.server-url`. The
policy server can consume a compartmentalization policy in a yaml format, and example of
which can be found in
`build/llvm-project/llvm/test/Transforms/Compartmentalization/hakc/tests/hakc_test0/db.yml`.
However, for large projects a graph database will likely be faster. We are using
the [Kuzu graph database](https://docs.kuzudb.com/) for our database, since it has wide
support for many different languages.

