#ifndef __XRDPOSIXXROOTD_H__
#define __XRDPOSIXXROOTD_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d P o s i x X r o o t d                         */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/* Modified by Frank Winklmeier to add the full Posix file system definition. */
/******************************************************************************/
  
//           $Id$

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucPthread.hh"

const int XrdPosixFD = 16384;

class XrdPosixFile;
class XrdPosixDir;

class XrdPosixXrootd
{
public:

// POSIX methods
//
static int     Close(int fildes);

static int     Closedir(DIR *dirp);

static off_t   Lseek(int fildes, off_t offset, int whence);

static int     Fstat(int fildes, struct stat *buf);

static int     Fsync(int fildes);

static int     Mkdir(const char *path, mode_t mode);

static int     Open(const char *path, int oflag, int mode=0);

static DIR*    Opendir(const char *path);
  
static ssize_t Pread(int fildes, void *buf, size_t nbyte, off_t offset);
  
static ssize_t Read(int fildes, void *buf, size_t nbyte);

static ssize_t Readv(int fildes, const struct iovec *iov, int iovcnt);

static struct dirent* Readdir(DIR *dirp);

static int     Readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);

static void    Rewinddir(DIR *dirp);

static int     Rmdir(const char *path);

static void    Seekdir(DIR *dirp, long loc);

static int     Stat(const char *path, struct stat *buf);

static ssize_t Pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);

static long    Telldir(DIR *dirp);

static int     Unlink(const char *path);

static ssize_t Write(int fildes, const void *buf, size_t nbyte);

static ssize_t Write(int fildes, void *buf, size_t nbyte, off_t offset);

static ssize_t Writev(int fildes, const struct iovec *iov, int iovcnt);

// Some non POSIX methods
//
static int     Access(const char *path, int amode);

static bool    isXrootdDir(DIR *dirp);

static int     mapError(int rc);

static void    setDebug(int val);

static void    setEnv(const char *var, const char *val);

static void    setEnv(const char *var, int val);

static int     Debug;

               XrdPosixXrootd(int maxfd=64, int maxdir=64);
              ~XrdPosixXrootd();

private:

static int                   Fault(XrdPosixFile *fp, int complete=1);
static XrdPosixFile         *findFP(int fildes, int glk=0);
static XrdPosixDir          *findDIR(DIR *dirp, int glk=0);
static void                  initStat(struct stat *buf);
static void                  initXdev(dev_t &st_dev, dev_t &st_rdev);
static int                   mapFlags(int flags);

static XrdOucMutex    myMutex;
static const  int     FDMask;
static const  int     FDOffs;
static const  int     FDLeft;
static XrdPosixFile **myFiles;
static XrdPosixDir  **myDirs;
static int            lastFD;
static int            highFD;
static int            lastDir;
static int            highDir;
};
#endif