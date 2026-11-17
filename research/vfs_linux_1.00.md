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

**Summary**:

Without dipping into the details of implementation, the idea of Linux 1.00 VFS is summarized as the following points:

TODO: I only read the code for loading the superblock. Please add the summary for other filesystem system calls such as open(), read(), etc.

1. Loading the superblock

  - Pre-compile config registers filesystems required by the programmer.

  - The registration takes the format of adding an item into an array of `struct file_system_type`. 

  - In `mount_root()`, enumerate all registered filesystems against `ROOT_DEV` and finds out the one that returns a valid pointer to a superblock. This process calls all of the registered `*_read_super()` functions.

  - Once found that means the superblock has already been loaded.

2. The inode

Many filesystem operations require a `struct inode *`. It embeds a `struct inode_operations * i_op` which is filesystem specific.

```C
struct inode {
	dev_t		i_dev;
	unsigned long	i_ino;
	//...
	struct inode_operations * i_op;
	//...
	union {
		struct pipe_inode_info pipe_i;
		struct minix_inode_info minix_i;
		struct ext_inode_info ext_i;
		struct ext2_inode_info ext2_i;
		struct hpfs_inode_info hpfs_i;
		struct msdos_inode_info msdos_i;
		struct iso_inode_info isofs_i;
		struct nfs_inode_info nfs_i;
		struct xiafs_inode_info xiafs_i;
		struct sysv_inode_info sysv_i;
	} u;
};
```

First, I'd like to explain how each specific filesystem "installs" its own `struct inode_operations * i_op`. Taking Minix as an example:

Minix filesystem has its own `struct super_operations minix_sops`:

```C
static struct super_operations minix_sops = { 
	minix_read_inode,
	NULL,
	minix_write_inode,
	minix_put_inode,
	minix_put_super,
	minix_write_super,
	minix_statfs,
	minix_remount
};
```

Recall, that `mount_root()` installs all pre-configured filesystem (including Minix, if it has been configured), and calls `*_read_super()` for each of them. `*_read_super()`, in this case `minix_read_super()`, is the one that installs `minix_sops`:

```C
  s->s_op = &minix_sops;
```

Each `struct super_operations` embeds a function pointer `void (*read_inode) (struct inode *)`. For some filesystems that do not need this function, such as NFS, it is NULL. But since Minix is a traditional Unix-style filesystem, it does have one -- the `minix_read_inode()` listed above. Note that the second function pointer, which points to `notify_change()`, is NULL for Minix. The VFS version of `read_inode()` is very simple. It checks whether the filesystem has its own `read_inode()` function, and call it if yes. For Minix it calls `minix_read_inode()`.

```C
// fs/inode.c
static void read_inode(struct inode * inode)
{
	lock_inode(inode);
	if (inode->i_sb && inode->i_sb->s_op && inode->i_sb->s_op->read_inode)
		inode->i_sb->s_op->read_inode(inode);
	unlock_inode(inode);
}
```

Now, as a static function, `read_inode()` is only called by `__iget()`, which is in turn called by `iget()`, a wrapper function. `__iget()` is called by some (few) Minix functions -- but the majority of Minix filesystem functions use `iget()` instead. 

Coming back to Minix, `minix_read_inode()` is the function that "installs" (I really think I overused this word) the Minix inode operations. Curiously, it has 5 inode operations. The reason for that is different type of file requires different inode operations.

```C
// fs/minix/inode.c

	if (S_ISREG(inode->i_mode))
		inode->i_op = &minix_file_inode_operations;
	else if (S_ISDIR(inode->i_mode))
		inode->i_op = &minix_dir_inode_operations;
	else if (S_ISLNK(inode->i_mode))
		inode->i_op = &minix_symlink_inode_operations;
	else if (S_ISCHR(inode->i_mode))
		inode->i_op = &chrdev_inode_operations;
	else if (S_ISBLK(inode->i_mode))
		inode->i_op = &blkdev_inode_operations;
```

Since VFS sits as the middleman between the kernel and various filesystems, all system calls get routed to the VFS version of the system call functions, and then are passed to the filesystem specific ones. Take `lookup()` in `fs/namei.c` as an example -- it checks whether the filesystem has its own version of `lookup()`, and call it if yes. For Minix it calls `minix_lookup()`.

```C
//fs/namei.c
int lookup(struct inode * dir,const char * name, int len,
	struct inode ** result)
{
	//...
	if (!dir->i_op || !dir->i_op->lookup) {
		iput(dir);
		return -ENOTDIR;
	}
	//...
	return dir->i_op->lookup(dir,name,len,result);
}

// fs/minix/dir.c
struct inode_operations minix_dir_inode_operations = {
	&minix_dir_operations,	/* default directory file-ops */
	minix_create,		/* create */
	minix_lookup,		/* lookup */
	minix_link,		/* link */
	minix_unlink,		/* unlink */
	minix_symlink,		/* symlink */
	minix_mkdir,		/* mkdir */
	minix_rmdir,		/* rmdir */
	minix_mknod,		/* mknod */
	minix_rename,		/* rename */
	NULL,			/* readlink */
	NULL,			/* follow_link */
	NULL,			/* bmap */
	minix_truncate,		/* truncate */
	NULL			/* permission */
};
```
3. The whole call chain to reach filesystem specific inode operations functions such as minix_rename(). It is a better example than minix_lookup() as lookup() doesn't seem to be a system call while sys_rename() is:

This is pretty convoluted, so I'll try to be as concise as possible. The first part is always to config the kernel to include Minix as one of its filesystems. In this case we assume it is the root filesystem and everything goes well ->

During `mount_root()`, it calls `minix_read_super()`, and "installs" the Minix superblock operations `minix_sops`. The kernel now has access to `minix_read_inode()` which is part of `minix_sops` whenever a `read_inode()` is called ->

A user program makes the system call `rename()`. It gets routed to the VFS version of `sys_rename()`, which is a wrapper and calls `do_rename()`. Note that up until now, none of the functions we mentioned acknowledge the inode -- all `do_rename()` knows are the string of `oldname` and `newname` ->

`do_rename()` calls `dir_namei()` to get the inodes. `dir_namei()` starts from two possible inodes, one is the `current->cwd`, and the other is the `current->root`. **This is the most tricky part**, as there is no indication **where and how is the i_ops gets installed** -- somehow `dir_namei()` calls `lookup()`, which immediately routes to the Minix version of `minix_lookup()`, which is part of the inode operation functions. How is the connection made?

We need to check how the kernel sets up `current->cwd` and `current->root`. Both are set during `mount_root()`, and `cwd` is probably changed afterwards based on the user's workflow.

```C
		sb = read_super(ROOT_DEV,fs_type->name,root_mountflags,NULL,1);
		//...
			inode = sb->s_mounted;
			//...
			current->pwd = inode;
			current->root = inode;
```

As we can see from the above code snippet, `sb->s_mounted` is obtained **AFTER the call to `read_super()`, which calls `minix_read_super()`**. If we inspect `minix_read_super()`, it uses `iget()` to fetch `s_mounted`. And we already know that `iget()` calls `__iget()`, which calls `s_op->read_inode()` -- that is exactly `minix_read_inode()`, and **it is `minix_read_inode()` that installs the 5 inode oeprations** for any inode passed as its argument -- in this case it is `s_mounted`.

```C
	s->s_mounted = iget(s,MINIX_ROOT_INO);
	if (!s->s_mounted) {
		s->s_dev = 0;
		brelse(bh);
		printk("MINIX-fs: get root inode failed\n");
		return NULL;
	}
```

Once we understand the above, let's go back to `dir_namei()`. As we said, it calls `lookup()` to bear the burden of the work. Now that the much needed Minix version of inode operations functions have been installed by `minix_read_inode()`, `lookup()` is able to check whether Minix has its own version of `lookup()`, which it does, and call `i_op->lookup()`. This is actually `minix_lookup()` ->

Once the inodes have been obtained, we now need to take a step back and read `do_rename()`. At the end, it checks whether Minix has its own version of `rename()`, which it does, and call `i_op->rename()`. This is actually `minix_rename()`.

**The key to understand the whole schema, is to understand that Linux 1.00 lets each inode embeds its own inode operations, instead of a filesystem specific "global" one. This is by my understanding more convoluted, but much more flexible and preferred.**