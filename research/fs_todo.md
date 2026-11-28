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

      ~~ // TODO (VFS-3-00): 2026-11-23 Please start working the following TO-DOs. Record your progress, close any items that you have completed, and breakdown/modify existing items if needed.~~

      Task: As described by the TODO item. This is a pre-requisite for the next item.

      Hypothesis: N/A

      Deliverables: N/A

      // TODO (VFS-3-01): Re-read section 3.1, 3.2 and the top part of 3.3 above. Specifically, WRITE DOWN IN A NOTEBOOK, the call chains of Linux 1.00 until the VFS takes over (kernel calls some VFS function), and the call chains of xv6 until the kernel calls some filesystem function. I need to use these two points to help me implement VFS for xv6.

      Task: As described by the TODO item. This is a pre-requisite for the next item.

      Hypothesis: N/A

      Deliverables: The call chains of Linux 1.00 until the VFS takes over (kernel calls some VFS function), and the call chains of xv6 until the kernel calls some filesystem function.

      Findings:

      In Linux 1.00 source code `main.c`, `start_kernel()`, it says:

      ```C
    	move_to_user_mode();
      if (!fork())		/* we count on this going ok */
        init();
      ```

      My hunch is that the kernel expects the first `fork()` to fail, then it calls `init()` to initiate the user space (?).

      `mount_root()` sets the cwd of the process, as in xv6. Here is the code snippet:

      ```C
      if (sb) {
        inode = sb->s_mounted;
        // Some code
        current->pwd = inode;
        current->root = inode;
        // Some code
        return;
      }
      ```

      // FUTUR-TODO: Investigate how the Linux kernel manages to call `init()` during startup.

      // TODO (VFS-3-02): Make sure that p->cwd, the only place that the process knows about the filesystem, is indeed used in open/dup system calls at the beginning of `init.c`. AT THIS MOMENT, the kernel only has one piece of filesystem information -- the root inode (and also `itable` and `filetable`, see below). 

      Task: Trace the `open()` and `dup()` system calls made by user/init.c. Identify which filesystem-related structures they access, directly or indirectly, and which of those structures must already be initialized.
      
      Pay particular notice to `itable`, `filetable`, the root inode (pointed to by `p->cwp`).

      Hypothesis: I believe ATM the kernel only need to know `itable` (used in `iinit()`), `filetable` (used in `fileinit()`), the root inode (pointed to by `p->cwp`) (used in `userinit()`). The kernel is able to complete the system calls. I do not miss any other filesystem components BEFORE `init.c` runs.

      Deliverable: Write down evidence supporting or contradicting the hypothesis.

      Findings: 
      
      I have traced `open()` system calls. 

      `open()` -> `sys_open()`, which has three fs function calls: `namei()`, `filealloc()` and `fdalloc()`. 

      `filealloc()` finds an un-referenced `struct file *` in `ftable`, and increment the ref to 1. It needs to know `ftable`. This is why the kernel initializes `ftable` before `user/init.c`.

      `fdalloc()` needs to know `p->ofile[]`, and the file descriptor is simply an index into that array. This array is already known during `allocproc()`, as the kernel knows the size of `ofile[NOFILE]`.

      `namei()` calls `namex()`, which calls `iget()`, `idup()` and `dirlookup()`.

      `iget()` needs to access `itable`. `idup()` needs to access `itable`. `dirlookup()` calls `readi()`, which reaches low level buffer system to call `bmap()`, `bread()`, etc. This is very complicated, but I don't think that individual filesystem should implement their own buffer cache code.

      I have also traced `dup()` system calls.

      Further study needed to find out whether individual filesystems should implement their own low level buffer cache code. In Linux 1.00. minix does implement `minix_bread()`, while ext has `ext_bread()`.

      // FUTUR-TODO: Take minix and ext as two examples, compare `minix_bread()` and `ext_bread()`. Explain why they need different implementations of `bread()`?

      2. Use the superblock to mount an individual filesystem. In xv6 by saying "mount" simply means `p->cwd = namei("/");`, while in Linux 1.00 it means `mount_root()`.

      

      // TODO (VFS-3-02A): Determine the minimum responsibilities of a new VFS initialization function, assuming two registered filesystem implementations.

      // TODO (VFS-3-02B): Investigate how the selected filesystem could provide its root inode to the VFS, and how the kernel could obtain it. 

      Deliverables:

      Coding: Let's do some coding this time. 
      - ~~Move code into kernel/fs, modify `Makefile` and compile once~~
      - ~~Create file_system_type object based on the Linux 1.00 design~~
      - ~~Figure out a way to compile the code -> Add some code in `main.c` to print the two filesystem names ("xvfs" and "xvfs1")~~
      - Add startup code to mimic `mount_root()`, but simpilied, in `userinit()`

      // TODO: I need to figure out what does *_read_super() exactly does, because I want to implement something similar but significantly simpler in xv6 vfsinit(). Read minix_read_super() to figure out, and go back to vfsinit() to implement it.

      
      
      Question: Is namei() is the first filesystem function to convert to VFS?

      Answer: No, It is not. The fsinit() is the first one. Call chain: 
        `userinit()`->`allocproc()`->`forkret()`->`fsinit()`

      Question: How does Linux 1.00 find out which filesystem matches the dev?

      Answer: The whole call chain is:
      `init()`->`setup()`->`sys_setup()`->`mount_root()`->loop through each `*_read_super()`->Once one superblock is read successfully, return.

      Question: How should xv6's VFS recognize a certain type of filesystem on disk?

      Answer: look at `fsinit()`. Basically it calls `readsb()` to read a super block and initializes the log.

        - TODO: In xv6, when was the super block first used? I need to know this, because the VFS starts with read_super() which returns the super block.


      1. A call-chain diagram of the proposed initialization.
      2. A list of the minimum responsibilities of the VFS.
      3. A list of existing xv6 functions that may require changes.
      4. An explanation of how to verify that either registered filesystem can be selected and initialized.
    
      Stop condition:
      I can explain how the selected filesystem's root inode becomes available to userinit(), without depending on filesystem-specific code in userinit().
    
      Scope:
      No sys_mount(), multiple mounts, or device-driver modifications.
      Do not investigate general VFS operations yet.
    
      Findings: Not investigated yet.

      
      // TODO (VFS-3-03): However, kernel's `main.c` also calls `iinit()` and `fileinit()` before `userinit()`. I believe these two should also be part of VFS -- but the question is, are these two SPECIFIC to the VFS but not to individual filesystems? I'm trying to understand, for the inode array `itable` and file array `ftable`, is it technically correct to have just one copy for ALL individual filesystems? If yes, then I can leave them as is, as moving them into VFS is trivial -- however, if not, then I need to figure out how to add one more abstraction in the VFS so that each individual filesystem has their own `itable` and `filetable`.

      Task: (This is a very large TO-DO item that needs more clarification and further breakdown) Investigate whether `itable` and `filetable` must be per-filesystem, if we allow multiple TYPEs of filesystems (e.g. vsfs and minix) in xv6.

      Note that this item mixes 3 subitems:

      - What responsibilities do itable and ftable currently have in xv6?

      - Should those responsibilities remain global, belong to filesystem types, or belong to individual mounted filesystem instances?
      
      - How does Linux 1.0 handle the equivalent responsibilities?

      Hypothesis: Each MOUNTED filesystem needs its own `itable` and `filetable`. So the question is incorrect -- it has nothing to do with TYPEs. We can have two exactly same filesystems mounted and each needs its own copy of these two data structures.

      Deliverable: Write down evidence supporting or contradicting the hypothesis. Since xv6 doesn't support sys_mount, this is essentially a two-part research: Part 1 requires thinking within the xv6 code space (For xv6 VFS, do I need to define `itable` and `filetable` for each filesystem registered, assuming I'm going to borrow the concept of `file_system_type` and filesystem registration from Linux 1.00?), and Part 2 requires a reading of filesystem mounting in Linux 1.00 because xv6 doesn't even have it.

      Findings: Not Investigated yet.

      // TODO (VFS-3-04): Basically I need to know, for my xv6 VFS implementation, where existing xv6 filesystem operations should begin calling the new VFS interface. My hunch is that in the current xv6 code space, should take over from the `namei("/")` function call in `userinit()` because this is the first time that the kernel touches an important part of the filesystem -- the root inode.

      Task: Investigate whether I can keep `userinit()` unchanged, but rewrite `namei()` and its friends in VFS layer.

      Hypothesis: I hypothesize that `userinit()` can remain unchanged if pathname-resolution responsibilities (`namei()` and its friends) are moved behind a VFS interface..

      Deliverables: Write down evidence supporting or contradicting the hypothesis. Please make sure to include flowcharts, call chains to prove/contradict. No coding is needed at this stage.

      Findings: Not Investigated yet.

4. Create a subdirectory `kernel/fs/vsfs` and `kernel/fs/xvfs`. All further development of additional features of the filesystem itself (not VFS), which is a separate project, should stay in `kernel/fs/xvfs`. `kernel/fs/vsfs` is the official filesystem for xv6 so should ONLY be altered according to the needs of VFS. Take the original filesystem files, and figure out how to adapt them to the VFS API.

5. For VFS, we probably need to start from the superblock. Reread the Linux 1.00 VFS implementation and figure out our own solution.