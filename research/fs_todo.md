1. ~~Create a new subdirectory `fs` under `kernel` and figure out how to modify `Makefile` to compile something in that subdirectory too.~~

2. ~~Maybe optimize the directory structure first -- create an `include/` directory and move all kernel header files there. Ignore the user header files for now.~~

3. Start moving the filesystem header files into a separate directory, either `kernel/fs` or `include/fs`. I prefer the latter, but make sure to do some research beforehand.

4. Start moving the filesystem source files into `kernel/fs`. Take a look of Linux 1.00 VFS, which files should I move?