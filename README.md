# AD-alloc 
Alzheimer's memory allocator

## Tests

Run the allocator tests with:

```sh
bash test/run.sh
```

The suite checks aligned, writable allocations; free-space and block-list
bookkeeping; reuse after freeing; oversized requests; and heap cleanup when
heaps are released from different positions in the heap list.
