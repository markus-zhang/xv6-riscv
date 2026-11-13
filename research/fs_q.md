Q: What happens when we remove some data from a middle block? E.g. lets say:

```C
addrs[] = [5, 20, 9, ...];
```

Then we remove 60 bytes from block 20. What happens to the blocks? What if we remove more than 1KBytes starting from somewhere?

Actually, does xv6 even have any file editing capacity? After all, there is no text editing program.

