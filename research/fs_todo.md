1. ~~Create a new subdirectory `fs` under `kernel` and figure out how to modify `Makefile` to compile something in that subdirectory too.~~

2. ~~Maybe optimize the directory structure first -- create an `include/` directory and move all kernel header files there. Ignore the user header files for now.~~

3. Start moving the filesystem source files into `kernel/fs`. This directory contains the VFS source code files. Take a look of Linux 1.00 VFS, which files should I move? Which APIs should I build? Write a report, think through before making any modifications.

I decided to make a much more detailed plan for each step, because I want to minimize the "warm-up" time of each session -- ideally, I should be able to quickly review what I need to do in this session and jump in in under 5 minutes. I only have about 60-80 minutes for each session.

  - 3.1: Review how xv6 initiates the filesystem. Specificlly, identify the whole function call chain, and take enough notes for future "warm-up".

    I read that `userinit()` calls `namei("/")`. This is to find the root node and points the process' current working directory pointer (`struct inode *proc::cwd`) to it.

  - 3.2: Review hos Linux 1.0 initiates the filesystem. I know how VFS works, but how Linux initiates the first filesystem using VFS is still a mystery to me.

    I read in `main.c`, `init()` calls `setup()`:
    ```C
    void init(void)
    {
	    int pid,i;

	    setup((void *) &drive_info);
      //...
    }
    ``` 

    `sys_setup()` calls `mount_root()`, which lives in `/fs/super.c`, as part of the VFS interface. It loops through all registered filesystems, find the first one that returns a valid `sb` (superblock pointer `struct super_block * sb`), and returns it.

    ```C
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

    - 3.3: Compare the two

      In xv6, `userinit()` creates a process and sets the root inode as its `cwd`. Then the scheduler runs and picks this process, as it is the ONLY running process after initialization.

      I don't understand where does the kernel loads the shell. It should be a user process, which I assume it is this first one -- I then double checked the code, and `userinit()` calls `allocproc()` which has `p->context.ra = (uint64)forkret;`. For the first user space process, `forkret()` is a bit special -- it executes `/init`.

      ```C
        if (__atomic_load_n(&first, __ATOMIC_ACQUIRE)) {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        fsinit(ROOTDEV);

        // ensure other cores see first=0.
        __atomic_store_n(&first, 0, __ATOMIC_RELEASE);

        // We can invoke kexec() now that file system is initialized.
        // Put the return value (argc) of kexec into a0.
        p->trapframe->a0 = kexec("/init", (char *[]){"/init", 0});
        if (p->trapframe->a0 == -1) {
          panic("exec");
        }
      }
      ```

      `init` is a user space program. We can check its source code in `user/init.c`. It is this program that presents the shell to the user. It makes some filesystem system calls, which I believe `p->cwd` helps to resolve --

      // TODO: 2026-11-23 Please close the following TO-DOs. I put up multiple for better format but they all ask the same questions.

      // TODO (head): Re-read section 3.1, 3.2 and the top part of 3.3 above. Specifically, WRITE DOWN IN A NOTEBOOK, the call chains of Linux 1.00 until the VFS takes over (kernel calls some VFS function), and the call chains of xv6 until the kernel calls some filesystem function. I need to use these two points to help me implement VFS for xv6.

      // TODO (contd.): Make sure that p->cwd, the only place that the process knows about the filesystem, is indeed used in open/dup system calls at the beginning of `init.c`. AT THIS MOMENT, the kernel only has one piece of filesystem information -- the root inode (and also `itable` and `filetable`, see below). Basically I need to fully understand where the VFS should take over. My hunch is that it should take over from the `namei("/")` function call in `userinit()` because this is the first time that the kernel touches the filesystem. 
      
      // TODO (contd.): However, kernel's `main.c` also calls `iinit()` and `fileinit()` before `userinit()`. I believe these two should also be part of VFS -- but the question is, are these two SPECIFIC to the VFS but not to individual filesystems? I'm trying to understand, for the inode array `itable` and file array `ftable`, is it technically correct to have just one copy for ALL individual filesystems? If yes, then I can leave them as is, as moving them into VFS is trivial -- however, if not, then I need to figure out how to add one more abstraction in the VFS so that each individual filesystem has their own `itable` and `filetable`.

      // TODO (contd.): I do believe that each mounted filesystem (could be of same type) should have its own `itable` and `filetable`. But xv6 doesn't even have the mount system call yet.

4. Create a subdirectory `kernel/fs/vsfs` and `kernel/fs/xvfs`. All further development of additional features of the filesystem itself (not VFS), which is a separate project, should stay in `kernel/fs/xvfs`. `kernel/fs/vsfs` is the official filesystem for xv6 so should ONLY be altered according to the needs of VFS. Take the original filesystem files, and figure out how to adapt them to the VFS API.

5. For VFS, we probably need to start from the superblock. Reread the Linux 1.00 VFS implementation and figure out our own solution.