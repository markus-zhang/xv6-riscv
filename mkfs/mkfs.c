#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat // avoid clash with host struct stat
#include "include/types.h"
#include "include/fs.h"
#include "include/stat.h"
#include "include/param.h"

#ifndef static_assert
#define static_assert(a, b)                                                    \
  do {                                                                         \
    switch (0)                                                                 \
    case 0:                                                                    \
    case (a):;                                                                 \
  } while (0)
#endif

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap = FSSIZE / BPB + 1;
int ninodeblocks = NINODES / IPB + 1;
int nlog = LOGBLOCKS + 1; // Header followed by LOGBLOCKS data blocks.
int nmeta;   // Number of meta blocks (boot, sb, nlog, inode, bitmap)
int nblocks; // Number of data blocks

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void balloc(int);
void wsect(uint, void *);
void winode(uint, struct dinode *);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
void die(const char *);

// convert to riscv byte order
ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar *)&y;
  a[0] = x;
  a[1] = x >> 8;

  /**NOTE - See the comment for xint(). xshort() converts big endian numbers to little endian ones with out changing its value. */
  return y;
}

uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar *)&y;
  // a[0] is the lowest byte of x
  a[0] = x;
  // a[1] is the second lowest byte of x
  a[1] = x >> 8;
  // and so on...
  a[2] = x >> 16;
  // a[3] is the highest byte of x
  a[3] = x >> 24;

  /**NOTE - This routine converts big/little endian to little endian 
   * If x is little endian, nothing changes;
   * If x is big endian, e.g. 0x1234, under big endian this is effectively 1+2*16+3*256+4*4096;
   * However, under a little endian system, it will be interpreted as 4+3*16+2*256+1*4096, which is wrong;
   * After the conversion, the lowest byte becomes 1, and the second lowest becomes 2, and so on. So under a little endian system, this is still 1+2*16+3*256+4*4096;
  */

  return y;
}

int
main(int argc, char *argv[])
{
  /**NOTE - It is useful to review the structure of xv6 FS, which is very close to vsfs in the book;
   * [boot sector | superblock | log header block | log blocks | inode blocks | free bit map | data blocks]
   * Usage in the Makefile: mkfs/mkfs fs.img README $(UPROGS)
   */
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct dinode din;

  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if (argc < 2) {
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

  /**NOTE - sizes of struct dinode and dirent must divide BSIZE (1024) */
  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

    /**NOTE - open() fs.img for R/W */
  fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fsfd < 0)
    die(argv[1]);

  // 1 fs block = 1 disk sector
  /**NOTE - 1 boot sector + 1 superblock = 2 blocks, and then the log blocks, etc. 
   * All these are blocks that contain the metadata blocks of the FS, except the boot sector.
  */
  nmeta = 2 + nlog + ninodeblocks + nbitmap;
  nblocks = FSSIZE - nmeta;

  /**NOTE - Building up the superblock, which contains the metadata of the FS */
  sb.magic = FSMAGIC;
  sb.size = xint(FSSIZE);
  sb.nblocks = xint(nblocks);
  sb.ninodes = xint(NINODES);
  sb.nlog = xint(nlog);
  sb.logstart = xint(2);
  sb.inodestart = xint(2 + nlog);
  sb.bmapstart = xint(2 + nlog + ninodeblocks);

  printf(
    "nmeta %d (boot, super, log blocks %u, inode blocks %u, bitmap blocks %u) blocks %d total %d\n",
    nmeta, nlog, ninodeblocks, nbitmap, nblocks, FSSIZE);

  /**NOTE - The block no. of the first freeblock */
  freeblock = nmeta; // the first free block that we can allocate

  /**NOTE - Each loop lseek() and write() 1024 bytes of zeros into a block in fs.img.
   * wesct() takes fsfd, the file descriptor of fs.img.
   * This should write into the on-disk file eventually.
  */
  for (i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  memset(buf, 0, sizeof(buf));
  /**NOTE - Copy the data in the superblock (sb) into the buffer (buf) */
  memmove(buf, &sb, sizeof(sb));
  /**NOTE - lseek to the 2nd block (block no. 1) and write the buffer (containing the superblock) into it */
  wsect(1, buf);

  /**NOTE - Write a struct dinode onto fs.img at the right place.
   * It is pretty complicated -- we need to write the root dinode at the first inode block.
   * ialloc() first creates an in-memory struct dinode and fills it with the root inode data;
   * Then it calls winode() to write it into fs.img (I still need to figure out this one).
  */
  rootino = ialloc(T_DIR);
  assert(rootino == ROOTINO);

  /**NOTE - Make the dirent of . and ..
   * TODO: I need to figure out how iappend() increments the offset -- you see, it is called twice, one for '.' and the other for "..", but in both calls the first argument is rootino. So I figured there must be a way to increment the offset within iappend().
   * 
   * After inspecting iappend(), I think the global variable freeblock is the one I'm looking for. I left some comments in iappend().
  */

  /**NOTE - Understand why both . and .. occupy 1024 bytes, not smaller; 
   * Because they take at least one block, which is 1024 bytes;
   * Even for files smaller than 1 block, they still occupy 1 block;
  */
  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  bzero(&de, sizeof(de));
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  for (i = 2; i < argc; i++) {
    // get rid of "user/"
    /**NOTE - This means to get rid of "user" directory. 
     * Everything is flattened out in the root directory.
    */
    char *shortname;
    if (strncmp(argv[i], "user/", 5) == 0)
      shortname = argv[i] + 5;
    else
      shortname = argv[i];

    assert(index(shortname, '/') == 0);

    if ((fd = open(argv[i], 0)) < 0)
      die(argv[i]);

    // Skip leading _ in name when writing to file system.
    // The binaries are named _rm, _cat, etc. to keep the
    // build operating system from trying to execute them
    // in place of system binaries like rm and cat.
    if (shortname[0] == '_')
      shortname += 1;

    assert(strlen(shortname) <= DIRSIZ);

    inum = ialloc(T_FILE);

    bzero(&de, sizeof(de));
    de.inum = xshort(inum);
    strncpy(de.name, shortname, DIRSIZ);
    iappend(rootino, &de, sizeof(de));

    while ((cc = read(fd, buf, sizeof(buf))) > 0)
      iappend(inum, buf, cc);

    close(fd);
  }

  // fix size of root inode dir
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off / BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);

  balloc(freeblock);

  exit(0);
}

void
wsect(uint sec, void *buf)
{
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
    die("lseek");
  if (write(fsfd, buf, BSIZE) != BSIZE)
    die("write");
}

void
winode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode *)buf) + (inum % IPB);
  *dip = *ip;
  wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = IBLOCK(inum, sb);
  rsect(bn, buf);
  dip = ((struct dinode *)buf) + (inum % IPB);
  *ip = *dip;
}

void
rsect(uint sec, void *buf)
{
  if (lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE)
    die("lseek");
  if (read(fsfd, buf, BSIZE) != BSIZE)
    die("read");
}

uint
ialloc(ushort type)
{
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void
balloc(int used)
{
  uchar buf[BSIZE];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < BPB);
  bzero(buf, BSIZE);
  for (i = 0; i < used; i++) {
    buf[i / 8] = buf[i / 8] | (0x1 << (i % 8));
  }
  printf("balloc: write bitmap block at sector %d\n", sb.bmapstart);
  wsect(sb.bmapstart, buf);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

void
iappend(uint inum, void *xp, int n)
{
  char *p = (char *)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  // printf("append inum %d at off %d sz %d\n", inum, off, n);

  /**TODO: Write the explanation of the whole while loop; 
   * I understand bits here and there but don't understand the big picture;
  */
  while (n > 0) {
    /**NOTE - In each while loop, off is incremented by n1 (check code at end of loop) */
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);

    /**NOTE - If it's small enough, we can use the direct inodes */
    if (fbn < NDIRECT) {
      if (xint(din.addrs[fbn]) == 0) {
        /**NOTE - fbn starts with 0 as the initial off / BSIZE is 0;
         * Once off gets incremented by n1, fbn is also incremented;
         * freeblock is the block no. of the next free block;
         * mkfs.c basically uses freeblock as an incrementing block pointer;
         * This is why users of iappend() don't need to worry about managing the block no. by themselves;
        */
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if (xint(din.addrs[NDIRECT]) == 0) {
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
      if (indirect[fbn - NDIRECT] == 0) {
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT]), (char *)indirect);
      }
      x = xint(indirect[fbn - NDIRECT]);
    }
    /**NOTE - for iappend(rootino, &de, sizeof(de)); 
     * n is 16 bytes, so n1 = 16 bytes;
     * 
     * For bigger files (more than 1 block),
    */
    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * BSIZE), n1);
    wsect(x, buf);

    /**NOTE - for iappend(rootino, &de, sizeof(de)); 
     * n is 0 after this subtraction, so the while loop ends.
     * 
     * For bigger files (more than 1 block),
    */
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}

void
die(const char *s)
{
  perror(s);
  exit(1);
}
