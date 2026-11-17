### Comparison of 3 designs:

Design A: No VFS, but the kernel detects what filesystem it is handling and use a big `switch` to route code.

Design B: VFS, but each specific filesystem (e.g. Minix-fs, vsfs, etc.) detects what type of inodes it is handling, and route code accordingly.

Design C: Linux 1.00 VFS, each inode embeds its own inode operations, and points them to the right ones during inode creation.

--------------------------------------

Design A is definitely bad, because the kernel needs to know all details of EACH filesystem, which doesn't make sense. There is zero encapsulation. We need to modify the kernel for each modification/addition of a filesystem.

I think Design C (Linux 1.00) does have some advantage over Design B (which was my intention, where specific filesystem handle/manipulate inode operations for ALL inodes).

1. Encapsulation: On the filesystem level, it doesn't necessarily need to know every details of the inode operations. Minix has 5 types of inode operations based on which type the inode is, and 2 of them, `chrdev_inode_operations` and `blkdev_inode_operations` are NOT Minix-specific. Instead, every Minix inode holds a pointer `i_op` pointing to 1 of the 5 `struct inode_operation`, and every inode operation gets routed through `i_op->some_inode_op()`.

2. Flexibility: As mentioned in the "Encapsulation" section, some inode operations are NOT filesystem specific. One of them, `pipe_inode_operations`, doesn't even go through any specific filesystem. I listed all references of `get_pipe_inode(void)` and it is only used in `fs/pipe.c`. 

```C
// fs/inode.c
struct inode * get_pipe_inode(void)
{
	struct inode * inode;
	extern struct inode_operations pipe_inode_operations;

	if (!(inode = get_empty_inode()))
		return NULL;
	if (!(PIPE_BASE(*inode) = (char*) __get_free_page(GFP_USER))) {
		iput(inode);
		return NULL;
	}
	inode->i_op = &pipe_inode_operations;
	inode->i_count = 2;	/* sum of readers/writers */

	//...
	return inode;
}
```

### First step: Move fs code into a subdirectory

My pre-vfs plan:

Before doing anything, commit everything in this branch, and create a new branch based on this branch. I don't want to mess around the valuable docs.

**First step**, move the fs code into a subdirectory, modify `Makefile` and make sure the whole kernel compiles and runs fine. The final directory structure should be:

xv6-riscv----
            |
            /kernel--
                    |
                    /fs--
                        |
                        (All fs code)

**Second step**, figure out how to bootstrap this thing...in general, `/fs` should contain the VFS version of filesystem code, and `/fs/vsfs` should contain the VSFS code. I should also implement a file_system_type or something similar so that specific filesystems can register with the kernel.