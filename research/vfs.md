## Journal of VFS implementation for xv6

### Objective 1: Understand how Linux 1.00 implemented VFS

I have looked at how Linux 1.00 implemented VFS. Basically there is an array of `struct file_system_type`, with each of the following layout:

```C
struct file_system_type {
	struct super_block *(*read_super) (struct super_block *, void *, int);
	char *name;
	int requires_dev;
};
```

So let's say we have two filesystems, Minix and EXT, then the array looks like this, with `minix_read_super` and `ext_read_super` function pointers pointing to functions to read the super block of each filesystem.

```C
struct file_system_type file_systems[] = {

	{minix_read_super,	"minix",	1},

	{ext_read_super,	"ext",		1},

}
```

Now that each of the two `struct file_system_type` has its own `read_super` function pointer, pointing to `minix_read_super()` or `ext_read_super()`. And the kernel can call each with the same code.

```C
// type is a pointer to a struct file_system_type

if (!(type = get_fs_type(name))) {
  printk("VFS: on device %d/%d: get_fs_type(%s) failed\n",
          MAJOR(dev), MINOR(dev), name);
  return NULL;
}

// Now we can call read_super(), which calls the correct *_read_super() function

if (!type->read_super(s,data, silent)) {
  s->s_dev = 0;
  return NULL;
}
```

Note that all filesystem-specific `*_read_super()` function returns the same type of object -- a `struct super_block *` object. Now that apparently different filesystems have different implementations, and some like FAT don't even have superblocks, so it is quite interesting to see how did Linux 1.00 pool everything together.

E.g. the proc filesystem is an in-memory filesystem for users to inspect the processes. It doesn't even need a superblock. But for the sake of compatibility, the design MUST uses one. Although a `struct super_block` has 14 fields, `proc_read_super()` only assigns a handful of them.

```C
struct super_block *proc_read_super(struct super_block *s,void *data, 
				    int silent)
{
	lock_super(s);
	s->s_blocksize = 1024;
	s->s_blocksize_bits = 10;
	s->s_magic = PROC_SUPER_MAGIC;
	s->s_op = &proc_sops;
	unlock_super(s);
	if (!(s->s_mounted = iget(s,PROC_ROOT_INO))) {
		s->s_dev = 0;
		printk("get root inode failed\n");
		return NULL;
	}
	return s;
}
```

But how does the kernel know which file system it needs to pick? After all, the array could contain multiple `struct file_system_type`, but only one of them is needed during bootup. It is pretty complicated, but `mount_root()` is the main entry of that purpose. It is used in `sys_setup`(), which gets called from the user side as `setup()` in `init()`.

Essentially, `mount_root()` tries every registered filesystem against `ROOT_DEV`. I'm still not 100% sure how `ROOT_DEV` is setup, but there are multiple places in the code that touches `ROOT_DEV`. For example, `starts_kernel()` sets it, then calls `parse_options()` which modifies `ROOT_DEV` further. 

```C
// mount_root() throws each filesystem against ROOT_DEV and hopes one sticks

	for (fs_type = file_systems; fs_type->read_super; fs_type++) {
		if (!fs_type->requires_dev)
			continue;
		sb = read_super(ROOT_DEV,fs_type->name,root_mountflags,NULL,1);
		if (sb) {
			inode = sb->s_mounted;
			inode->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */
			sb->s_covered = inode;
			sb->s_flags = root_mountflags;
			current->pwd = inode;
			current->root = inode;
			printk ("VFS: Mounted root (%s filesystem)%s.\n",
				fs_type->name,
				(sb->s_flags & MS_RDONLY) ? " readonly" : "");
			return;
		}
	}
```