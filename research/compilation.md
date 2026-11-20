## All knowledge learned from modifying Makefile

1. Forward declaration

In `defs.h`, all structs such as `struct pagetable_t`, `struct buf` are supposed to be known to the compiler -- the same for custom types such as `uint64`. That means, if a `.c` file, such as my `fshello.c` in `kernel/fs`, wants to include `defs.h`, it needs to include all header files, which is what all `.c` files do.

However, `defs.h` forward declare a lot of `struct...`, so actually we don't need to include all those as long as we don't need to know the internals of those structs -- for example using a pointer is fine.

`fshello.c` shows what are the minimal `.h` header files a `.c` file needs to include -- just two of them. We need the custom types, and a bunch of `struct` from `riscv.h`.

2. How to make gcc search header files in multiple directories

In `Makefile` (or using plain gcc), I added an `I` flag for each directory I want the compiler to search for header files.

```Makefile
CFLAGS += -I.
CFLAGS += -I$K
CFLAGS += -I$K/fs
```

3. What does $@ and $< mean

```Makefile
$K/%.o: $K/%.S
	$(CC) -march=rv64gc -g $(DETFLAGS) -c -o $@ $<
```

They are called automatic variables: https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html

Basically, the above command expand for each .S file its .o file. It's a bit like saying, for each .S file, do this `$(CC) -march...` thing and generate the .o file.

4. Why there is no .o: .c rules in the Makefile

Because in `Makefile`, there is an implicit rule that says, for each `.c` file, build a `.o` file. This is so common that `Makefile` makes it as a default rule.