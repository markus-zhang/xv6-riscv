#include "types.h"
#include "param.h"
#include "fs.h"
#include "defs.h"

#define NINODES 200

struct fstype filesystems[] = {
  {xvfs_readsb, "xvfs"},
  {vsfs_readsb, "vsfs"},
};

// A very simple implementation of read_super:
// Just return the same superblock for all filesystems.
// For now, I plan to implement 2 filesystems -- both are same except for the name.
// struct superblock read_super(char *fsname)
// {
//   struct superblock s;

//   int nbitmap = FSSIZE / BPB + 1;
//   int ninodeblocks = NINODES / IPB + 1;
//   int nlog = LOGBLOCKS + 1; // Header followed by LOGBLOCKS data blocks.
//   int nmeta;   // Number of meta blocks (boot, sb, nlog, inode, bitmap)
//   int nblocks; // Number of data blocks

//   nmeta = 2 + nlog + ninodeblocks + nbitmap;
//   nblocks = FSSIZE - nmeta;

//   s.magic = XVFSMAGIC;
//   s.size = FSSIZE;
//   s.nblocks = nblocks;
//   s.ninodes = NINODES;
//   s.nlog = nlog;
//   s.logstart = 2;
//   s.inodestart = 2 + nlog;
//   s.bmapstart = 2 + nlog + ninodeblocks;

//   return s;

// }

// Very primitive version of strcmp
// returns 0 if the two strings are equal
static int xvstrcmp(const char *a, const char *b)
{
  while (*a && (*a == *b))
  {
      a++;
      b++;
  }
  
  // So if *a is 0 (termination char), but *b is not, this doesn't return 0
  return (*a) - (*b);
}

struct fstype *getfstype(char *name)
{
  if (!name)
  {
    return &(filesystems[0]);
  }

  // Just hardcode -- vsfs is index 0 and vsfs2 is index 1
  if (xvstrcmp(name, "xvfs") == 0)
  {
    return &(filesystems[0]);
  }
  else
  {
    return &(filesystems[1]);
  }
}