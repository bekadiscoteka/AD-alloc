# AD-alloc 
Alzheimer's memory allocator

## Editor settings

C and header files use tabs displayed as 8 columns, as specified in
`.editorconfig`. Vim does not read EditorConfig files by itself; install an
EditorConfig Vim plugin to apply these settings automatically. For the current
buffer, apply them directly with:

```vim
:setlocal tabstop=8 shiftwidth=8 softtabstop=8 noexpandtab
```

## Tests

Run the allocator tests with:

```sh
bash test/run.sh
```

The suite checks aligned, writable allocations; free-space and block-list
bookkeeping; reuse after freeing; oversized requests; and heap cleanup when
heaps are released from different positions in the heap list.
