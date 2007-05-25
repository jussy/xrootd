/******************************************************************************/
/*                                                                            */
/*                          X r d O s s A p i . c c                           */
/*                                                                            */
/* (c) 2006 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//         $Id$

const char *XrdOssApiCVSID = "$Id$";
 
/* These routines are thread-safe if compiled with:
   AIX: -D_THREAD_SAFE
   SUN: -D_REENTRANT
*/

/******************************************************************************/
/*                             i n c l u d e s                                */
/******************************************************************************/
  
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream.h>
#include <signal.h>
#include <strings.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __solaris__
#include <sys/vnode.h>
#endif

#include "XrdVersion.hh"

#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssLock.hh"
#include "XrdOss/XrdOssMio.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucName2Name.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucPlugin.hh"

#ifdef XRDOSSCX
#include "oocx_CXFile.h"
#endif
// IOS_USING_DECLARATION_MARKER - BaBar iostreams migration, do not touch this line!

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/

XrdOssSys  *XrdOssSS = 0;
  
XrdOucError OssEroute(0, "oss_");

XrdOucTrace OssTrace(&OssEroute);

/******************************************************************************/
/*           S t o r a g e   S y s t e m   I n s t a n t i a t o r            */
/******************************************************************************/

char      XrdOssSys::tryMmap = 0;
char      XrdOssSys::chkMmap = 0;

/******************************************************************************/
/*                XrdOssGetSS (a.k.a. XrdOssGetStorageSystem)                 */
/******************************************************************************/
  
// This function is called by the OFS layer to retrieve the Storage System
// object. If a plugin library has been specified, then this function will
// return the object provided by XrdOssGetStorageSystem() within the library.
//
XrdOss *XrdOssGetSS(XrdOucLogger *Logger, const char   *config_fn,
                    const char   *OssLib)
{
   static XrdOssSys   myOssSys;
   extern XrdOucError OssEroute;
   XrdOucPlugin    *myLib;
   XrdOss          *(*ep)(XrdOss *, XrdOucLogger *, const char *, const char *);
   char *parms;

// If no library has been specified, return the default object
//
   if (!OssLib)
      if (myOssSys.Init(Logger, config_fn)) return 0;
         else return (XrdOss *)&myOssSys;

// Find the parms (ignore the constness of the variable)
//
   parms = (char *)OssLib;
   while(*parms && *parms != ' ') parms++;
   if (*parms) *parms++ = '\0';
   while(*parms && *parms == ' ') parms++;
   if (!*parms) parms = 0;

// Create a pluin object (we will throw this away without deletion because
// the library must stay open but we never want to reference it again).
//
   OssEroute.logger(Logger);
   if (!(myLib = new XrdOucPlugin(&OssEroute, OssLib))) return 0;

// Now get the entry point of the object creator
//
   ep = (XrdOss *(*)(XrdOss *, XrdOucLogger *, const char *, const char *))
                    (myLib->getPlugin("XrdOssGetStorageSystem"));
   if (!ep) return 0;

// Get the Object now
//
   return ep((XrdOss *)&myOssSys, Logger, config_fn, parms);
}
 
/******************************************************************************/
/*                      o o s s _ S y s   M e t h o d s                       */
/******************************************************************************/
/******************************************************************************/
/*                                  i n i t                                   */
/******************************************************************************/
  
/*
  Function: Initialize staging subsystem

  Input:    None

  Output:   Returns zero upon success otherwise (-errno).
*/
int XrdOssSys::Init(XrdOucLogger *lp, const char *configfn)
{
     int retc;

// Do the herald thing
//
   OssEroute.logger(lp);
   OssEroute.Say("Copr.  2007, Stanford University, oss Version " XrdVSTRING);

// Initialize the subsystems
//
   XrdOssSS = this;
   if ( (retc = Configure(configfn, OssEroute)) ) return retc;

// All done.
//
   return XrdOssOK;
}

/******************************************************************************/
/*                          G e n L o c a l P a t h                           */
/******************************************************************************/
  
/* GenLocalPath() generates the path that a file will have in the local file
   system. The decision is made based on the user-given path (typically what 
   the user thinks is the local file system path). The output buffer where the 
   new path is placed must be at least XrdOssMAX_PATH_LEN bytes long.
*/
int XrdOssSys::GenLocalPath(const char *oldp, char *newp)
{
    if (lcl_N2N) return -(lcl_N2N->lfn2pfn(oldp, newp, XrdOssMAX_PATH_LEN));
    if (strlen(oldp) >= XrdOssMAX_PATH_LEN) return -ENAMETOOLONG;
    strcpy(newp, oldp);
    return 0;
}

/******************************************************************************/
/*                         G e n R e m o t e P a t h                          */
/******************************************************************************/
  
/* GenRemotePath() generates the path that a file will have in the remote file
   system. The decision is made based on the user-given path (typically what 
   the user thinks is the local file system path). The output buffer where the 
   new path is placed must be at least XrdOssMAX_PATH_LEN bytes long.
*/
int XrdOssSys::GenRemotePath(const char *oldp, char *newp)
{
    if (rmt_N2N) return -(rmt_N2N->lfn2rfn(oldp, newp, XrdOssMAX_PATH_LEN));
    if (strlen(oldp) >= XrdOssMAX_PATH_LEN) return -ENAMETOOLONG;
    strcpy(newp, oldp);
    return 0;
}

/******************************************************************************/
/*                                 C h m o d                                  */
/******************************************************************************/
/*
  Function: Change file mode.

  Input:    path        - Is the fully qualified name of the target file.
            mode        - The new mode that the file is to have.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    Files are only changed in the local disk cache.
*/

int XrdOssSys::Chmod(const char *path, mode_t mode)
{
    char actual_path[XrdOssMAX_PATH_LEN+1], *local_path;
    int retc;

// Generate local path
//
   if (lcl_N2N)
      if ((retc = lcl_N2N->lfn2pfn(path, actual_path, sizeof(actual_path)))) 
         return retc;
         else local_path = actual_path;
      else local_path = (char *)path;

// Change the file only in the local filesystem.
//
   if (!chmod(local_path, mode)) return XrdOssOK;
   return -errno;
}

/******************************************************************************/
/*                                 M k d i r                                  */
/******************************************************************************/
/*
  Function: Create a directory

  Input:    path        - Is the fully qualified name of the new directory.
            mode        - The new mode that the directory is to have.
            mkpath      - If true, makes the full path.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    Directories are only created in the local disk cache.
*/

int XrdOssSys::Mkdir(const char *path, mode_t mode, int mkpath)
{
    char actual_path[XrdOssMAX_PATH_LEN+1], *local_path;
    int retc;

// Generate local path
//
   if (lcl_N2N)
      if ((retc = lcl_N2N->lfn2pfn(path, actual_path, sizeof(actual_path)))) 
         return retc;
         else local_path = actual_path;
      else local_path = (char *)path;

// Create the directory only in the loal file system
//
   if (!mkdir(local_path, mode)) return XrdOssOK;

// Check if the full path is to be created
//
   if (!mkpath || errno != ENOENT) return -errno;
   if ((retc = Mkpath(local_path, mode))) return retc;
   if (!mkdir(local_path, mode)) return XrdOssOK;
   return -errno;
}

/******************************************************************************/
/*                                M k p a t h                                 */
/******************************************************************************/
/*
  Function: Create a directory path

  Input:    path        - Is the fully qualified name of the new path.
            mode        - The new mode that each new directory is to have.

  Output:   Returns XrdOssOK upon success and -errno upon failure.

  Notes:    Directories are only created in the local disk cache.
*/

int XrdOssSys::Mkpath(const char *path, mode_t mode)
{
    char local_path[XrdOssMAX_PATH_LEN+1], *next_path;
    struct stat buf;
    int retc;

// Generate local path (we need to do this to get a r/w copy of the path)
//
   if ((retc = GenLocalPath(path, local_path))) return retc;

// Trim off the trailing slash so that we make everything but the last component
//
   if (!(retc = strlen(local_path))) return -ENOENT;
   while(retc && local_path[retc-1] == '/') 
        {retc--; local_path[retc] = '\0';}

// Typically, the path exists. So, do a quick check before launching into it
//
   if (!(next_path = rindex(local_path, (int)'/'))
   ||  next_path == local_path) return XrdOssOK;
   *next_path = '\0';
   if (!stat(local_path, &buf)) return XrdOssOK;
   *next_path = '/';

// Start creating directories starting with the root
//
   next_path = local_path;
   while((next_path = index(next_path+1, int('/'))))
        {*next_path = '\0';
         if (mkdir(local_path, mode) && errno != EEXIST) return -errno;
         *next_path = '/';
        }

// All done
//
   return XrdOssOK;
}

/******************************************************************************/
/*                                 s t a t                                    */
/******************************************************************************/

/*
  Function: Determine if file 'path' actually exists.

  Input:    path        - Is the fully qualified name of the file to be tested.
            buff        - pointer to a 'stat' structure to hold the attributes
                          of the file.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/

int XrdOssSys::Stat(const char *path, struct stat *buff, int resonly)
{
     const int ro_Mode = ~(S_IWUSR | S_IWGRP | S_IWOTH);
    char actual_path[XrdOssMAX_PATH_LEN+1], *local_path, *remote_path;
    unsigned long long popts;
    int retc;

// Construct the processing options for this path
//
   popts = PathOpts(path);

// Generate local path
//
   if (lcl_N2N)
      if ((retc = lcl_N2N->lfn2pfn(path, actual_path, sizeof(actual_path)))) 
         return retc;
         else local_path = actual_path;
      else local_path = (char *)path;

// Stat the file in the local filesystem first.
//
   if (!stat(local_path, buff)) 
      {if (popts & XRDEXP_NOTRW) buff->st_mode &= ro_Mode;
       return XrdOssOK;
      }
   if (!IsRemote(path)) return -errno;
   if (resonly) return -ENOMSG;

// Generate remote path
//
   if (rmt_N2N)
      if ((retc = rmt_N2N->lfn2pfn(path, actual_path, sizeof(actual_path))))
         return retc;
         else remote_path = actual_path;
      else remote_path = (char *)path;

// Now stat the file in the remote system (it doesn't exist locally)
//
   if ((retc = MSS_Stat(remote_path, buff))) return retc;
   if (popts & XRDEXP_NOTRW) buff->st_mode &= ro_Mode;
   return XrdOssOK;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                      o o s s _ D i r   M e t h o d s                       */
/******************************************************************************/
/******************************************************************************/
/*                               o p e n d i r                                */
/******************************************************************************/
  
/*
  Function: Open the directory `path' and prepare for reading.

  Input:    path      - The fully qualified name of the directory to open.

  Output:   Returns XrdOssOK upon success; (-errno) otherwise.
*/
int XrdOssDir::Opendir(const char *dir_path) 
{
#ifndef NODEBUG
   const char *epname = "Opendir";
#endif
   char actual_path[XrdOssMAX_PATH_LEN+1], *local_path, *remote_path;
   unsigned long long isremote;
   int retc;

// Return an error if this object is already open
//
   if (isopen) return -XRDOSS_E8001;

// Get the processing flags for this directory
//
   isremote = XRDEXP_REMOTE & (pflags = XrdOssSS->PathOpts(dir_path));
   ateof = 0;

// Generate local path
//
   if (XrdOssSS->lcl_N2N)
      if ((retc = XrdOssSS->lcl_N2N->lfn2pfn(dir_path, actual_path, sizeof(actual_path))))
         return retc;
         else local_path = actual_path;
      else local_path = (char *)dir_path;

// If this is a local filesystem request, open locally.
//
   if (!isremote)
      {TRACE(Opendir, "lcl path " <<local_path <<" (" <<dir_path <<")");
       if (!(lclfd = opendir((char *)local_path))) return -errno;
       isopen = 1;
       return XrdOssOK;
      }

// Generate remote path
//
   if (XrdOssSS->rmt_N2N)
      if ((retc = XrdOssSS->rmt_N2N->lfn2pfn(dir_path, actual_path, sizeof(actual_path))))
         return retc;
         else remote_path = actual_path;
      else remote_path = (char *)dir_path;

// Trace this remote request
//
   TRACE(Opendir, "rmt path " <<remote_path <<" (" <<dir_path <<")");

// If we need not read the actual directory, just check if it exists
//
   if (pflags & XRDEXP_NODREAD)
      {struct stat fstat;
       if (stat(local_path, &fstat)
       && (retc = XrdOssSS->MSS_Stat(remote_path, &fstat))) return retc;
       if (!(S_ISDIR(fstat.st_mode))) return -ENOTDIR;
       isopen = -1;
       return XrdOssOK;
      }

// This is a remote directory and we must read it. Perform remote open
//
   if (!(mssfd = XrdOssSS->MSS_Opendir(remote_path, retc))) return retc;
   isopen = 1;
   return XrdOssOK;
}

/******************************************************************************/
/*                               r e a d d i r                                */
/******************************************************************************/

/*
  Function: Read the next entry if directory associated with this object.

  Input:    buff       - Is the address of the buffer that is to hold the next
                         directory name.
            blen       - Size of the buffer.

  Output:   Upon success, places the contents of the next directory entry
            in buff. When the end of the directory is encountered buff
            will be set to the null string.

            Upon failure, returns a (-errno).

  Warning: The caller must provide proper serialization.
*/
int XrdOssDir::Readdir(char *buff, int blen)
{
   struct dirent *rp;

// Check if this object is actually open
//
   if (!isopen) return -XRDOSS_E8002;

// Perform local reads if this is a local directory
//
   if (lclfd)
      {errno = 0;
       if ((rp = readdir(lclfd)))
          {strlcpy(buff, rp->d_name, blen);
           return XrdOssOK;
          }
       *buff = '\0'; ateof = 1;
       return -errno;
      }

// Simulate the read operation, if need be.
//
   if (pflags & XRDEXP_NODREAD)
      {if (ateof) *buff = '\0';
          else   {*buff = '.'; ateof = 1;}
       return XrdOssOK;
      }

// Perform a remote read
//
   return XrdOssSS->MSS_Readdir(mssfd, buff, blen);
}

/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/
  
/*
  Function: Close the directory associated with this object.

  Input:    None.

  Output:   Returns XrdOssOK upon success and (errno) upon failure.
*/
int XrdOssDir::Close(void)
{
    int retc;

// Make sure this object is open
//
    if (!isopen) return -XRDOSS_E8002;

// Close whichever handle is open
//
    if (lclfd) {if (!(retc = closedir(lclfd))) lclfd = 0;}
       else if (mssfd) { if (!(retc = XrdOssSS->MSS_Closedir(mssfd))) mssfd = 0;}
               else retc = 0;

// Indicate whether or not we really closed this object
//
   return retc;
}

/******************************************************************************/
/*                     o o s s _ F i l e   M e t h o d s                      */
/******************************************************************************/
  
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

/*
  Function: Open the file `path' in the mode indicated by `Mode'.

  Input:    path      - The fully qualified name of the file to open.
            Oflag     - Standard open flags.
            Mode      - Create mode (i.e., rwx).
            env       - Environmental information.

  Output:   XrdOssOK upon success; -errno otherwise.
*/
int XrdOssFile::Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &Env)
{
   unsigned long long popts;
   int retc, mopts;
   char actual_path[XrdOssMAX_PATH_LEN+1], *local_path;
   struct stat buf;

// Return an error if this object is already open
//
   if (fd >= 0) return -XRDOSS_E8003;
      else cxobj = 0;

// Construct the processing options for this path
//
   popts = XrdOssSS->PathOpts(path);

// Generate local path
//
   if (XrdOssSS->lcl_N2N)
      if ((retc = XrdOssSS->lcl_N2N->lfn2pfn(path, actual_path, sizeof(actual_path))))
         return retc;
         else local_path = actual_path;
      else local_path = (char *)path;

// Check if this is a read/only filesystem
//
   if ((Oflag & (O_WRONLY | O_RDWR)) && (popts & XRDEXP_NOTRW))
      if (popts & XRDEXP_FORCERO) Oflag = O_RDONLY;
         else return OssEroute.Emsg("XrdOssOpen",-XRDOSS_E8005,"open r/w",path);

// If we can open the local copy. If not found, try to stage it in if possible.
// Note that stage will regenerate the right local and remote paths.
//
   if ( (fd = (int)Open_ufs(local_path, Oflag, Mode, popts))
         == -ENOENT && (popts & XRDEXP_REMOTE))
      {if (popts & XRDEXP_NOSTAGE)
          return OssEroute.Emsg("XrdOssOpen",-XRDOSS_E8006,"open",path);
       if ((retc = XrdOssSS->Stage(tident, path, Env, Oflag, Mode))) return retc;
       fd = (int)Open_ufs(local_path, Oflag, Mode, popts & ~XRDEXP_REMOTE);
      }

// This interface supports only regular files. Complain if this is not one.
//
   if (fd >= 0)
      {do {retc = fstat(fd, &buf);} while(retc && errno == EINTR);
       if (!retc && !(buf.st_mode & S_IFREG))
          {close(fd); fd = (buf.st_mode & S_IFDIR ? -EISDIR : -ENOTBLK);}
      } else if (fd == -EEXIST)
                {do {retc = stat(local_path,&buf);} while(retc && errno==EINTR);
                 if (!retc && (buf.st_mode & S_IFDIR)) fd = -EISDIR;
                }

// See if should memory map this file
//
   if (fd >= 0 && XrdOssSS->tryMmap)
      {mopts = 0;
       if (popts & XRDEXP_MKEEP) mopts |= OSSMIO_MPRM;
       if (popts & XRDEXP_MLOK)  mopts |= OSSMIO_MLOK;
       if (popts & XRDEXP_MMAP)  mopts |= OSSMIO_MMAP;
       if (XrdOssSS->chkMmap) mopts = XrdOssMio::getOpts(local_path, mopts);
       if (mopts) mmFile = XrdOssMio::Map(local_path, fd, mopts);
      } else mmFile = 0;

// Return the result of this open
//
   return (fd < 0 ? fd : XrdOssOK);
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

/*
  Function: Close the file associated with this object.

  Input:    None.

  Output:   Returns XrdOssOK upon success and -1 upon failure.
*/
int XrdOssFile::Close(void) 
{
    if (fd < 0) return -XRDOSS_E8004;
    if (close(fd)) return -errno;
    if (mmFile) {XrdOssMio::Recycle(mmFile); mmFile = 0;}
#ifdef XRDOSSCX
    if (cxobj) {delete cxobj; cxobj = 0;}
#endif
    fd = -1;
    return XrdOssOK;
}

/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

/*
  Function: Preread `blen' bytes from the associated file.

  Input:    offset    - The absolute 64-bit byte offset at which to read.
            blen      - The size to preread.

  Output:   Returns zero read upon success and -errno upon failure.
*/

ssize_t XrdOssFile::Read(off_t offset, size_t blen)
{

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

     return 0;  // We haven't implemented this yet!
}


/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

/*
  Function: Read `blen' bytes from the associated file, placing in 'buff'
            the data and returning the actual number of bytes read.

  Input:    buff      - Address of the buffer in which to place the data.
            offset    - The absolute 64-bit byte offset at which to read.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be read.

  Output:   Returns the number bytes read upon success and -errno upon failure.
*/

ssize_t XrdOssFile::Read(void *buff, off_t offset, size_t blen)
{
     ssize_t retval;

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

#ifdef XRDOSSCX
     if (cxobj)  
        if (XrdOssSS->DirFlags & XrdOssNOSSDEC) return (ssize_t)-XRDOSS_E8021;
           else   retval = cxobj->Read((char *)buff, blen, offset);
        else 
#endif
             do { retval = pread(fd, buff, blen, offset); }
                while(retval < 0 && errno == EINTR);

     return (retval >= 0 ? retval : (ssize_t)-errno);
}

/******************************************************************************/
/*                               R e a d R a w                                */
/******************************************************************************/

/*
  Function: Read `blen' bytes from the associated file, placing in 'buff'
            the data and returning the actual number of bytes read.

  Input:    buff      - Address of the buffer in which to place the data.
            offset    - The absolute 64-bit byte offset at which to read.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be read.

  Output:   Returns the number bytes read upon success and -errno upon failure.
*/

ssize_t XrdOssFile::ReadRaw(void *buff, off_t offset, size_t blen)
{
     ssize_t retval;

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

#ifdef XRDOSSCX
     if (cxobj)   retval = cxobj->ReadRaw((char *)buff, blen, offset);
        else 
#endif
             do { retval = pread(fd, buff, blen, offset); }
                while(retval < 0 && errno == EINTR);

     return (retval >= 0 ? retval : (ssize_t)-errno);
}

/******************************************************************************/
/*                                 w r i t e                                  */
/******************************************************************************/

/*
  Function: Write `blen' bytes to the associated file, from 'buff'
            and return the actual number of bytes written.

  Input:    buff      - Address of the buffer from which to get the data.
            offset    - The absolute 64-bit byte offset at which to write.
            blen      - The number of bytes to write from the buffer.

  Output:   Returns the number of bytes written upon success and -errno o/w.
*/

ssize_t XrdOssFile::Write(const void *buff, off_t offset, size_t blen)
{
     ssize_t retval;

     if (fd < 0) return (ssize_t)-XRDOSS_E8004;

     if (XrdOssSS->MaxDBsize && (long long)(offset+blen) > XrdOssSS->MaxDBsize)
        return (ssize_t)-XRDOSS_E8007;

     do { retval = pwrite(fd, buff, blen, offset); }
          while(retval < 0 && errno == EINTR);

     if (retval < 0) retval = (retval == EBADF && cxobj ? -XRDOSS_E8022 : -errno);
     return retval;
}

/******************************************************************************/
/*                                 f s t a t                                  */
/******************************************************************************/

/*
  Function: Return file status for the associated file.

  Input:    buff      - Pointer to buffer to hold file status.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/

int XrdOssFile::Fstat(struct stat *buff)
{
    return (fstat(fd, buff) ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                               f s y n c                                    */
/******************************************************************************/

/*
  Function: Synchronize associated file.

  Input:    None.

  Output:   Returns XrdOssOK upon success and -errno upon failure.
*/
int XrdOssFile::Fsync(void)
{
    return (fsync(fd) ? -errno : XrdOssOK);
}

/******************************************************************************/
/*                               g e t M m a p                                */
/******************************************************************************/
  
/*
  Function: Indicate whether or not file is memory mapped.

  Input:    addr      - Points to an address which will receive the location
                        memory where the file is mapped. If the address is
                        null, true is returned if a mapping exist.

  Output:   Returns the size of the file if it is memory mapped (see above).
            Otherwise, zero is returned and addr is set to zero.
*/
off_t XrdOssFile::getMmap(void **addr)
{
   if (mmFile) return (addr ? mmFile->Export(addr) : 1);
   if (addr) *addr = 0;
   return 0;
}
  
/******************************************************************************/
/*                          i s C o m p r e s s e d                           */
/******************************************************************************/
  
/*
  Function: Indicate whether or not file is compressed.

  Input:    cxidp     - Points to a four byte buffer to hold the compression
                        algorithm used if the file is compressed or null.

  Output:   Returns the regios size which is 0 if the file is not compressed.
            If cxidp is not null, the algorithm is returned only if the file
            is compressed.
*/
int XrdOssFile::isCompressed(char *cxidp)
{
    if (cxpgsz)
       {cxidp[0] = cxid[0]; cxidp[1] = cxid[1];
        cxidp[2] = cxid[2]; cxidp[3] = cxid[3];
       }
    return cxpgsz;
}

/******************************************************************************/
/*                              t r u n c a t e                               */
/******************************************************************************/

/*
  Function: Set the length of associated file to 'flen'.

  Input:    flen      - The new size of the file. Only 32-bit lengths
                        are supported.

  Output:   Returns XrdOssOK upon success and -1 upon failure.

  Notes:    If 'flen' is smaller than the current size of the file, the file
            is made smaller and the data past 'flen' is discarded. If 'flen'
            is larger than the current size of the file, a hole is created
            (i.e., the file is logically extended by filling the extra bytes 
            with zeroes).

            If compiled w/o large file support, only lower 32 bits are used.
            used.
            in supporting it for any other system.
*/
int XrdOssFile::Ftruncate(unsigned long long flen) {
    off_t newlen = flen;

#if _FILE_OFFSET_BITS!=64
    if (flen>>31) return -XRDOSS_E8008;
#endif

    return (ftruncate(fd, newlen) ?  -errno : XrdOssOK);
    }

/******************************************************************************/
/*                     P R I V A T E    S E C T I O N                         */
/******************************************************************************/
/******************************************************************************/
/*                      o o s s _ O p e n _ u f s                             */
/******************************************************************************/

int XrdOssFile::Open_ufs(const char *path, int Oflag, int Mode, 
                         unsigned long long popts)
{
    EPNAME("Open_ufs")
    int myfd, newfd, retc;
#ifndef NODEBUG
    char *ftype = (char *)" path=";
#endif
    XrdOssLock ufs_file;
#ifdef XRDOSSCX
    int attcx = 0;
#endif

// Obtain exclusive control over the directory.
//
    if ((popts & XRDEXP_REMOTE) 
    && (retc = ufs_file.Serialize(path, XrdOssDIR|XrdOssEXC)) < 0) return retc;

// Now open the actual data file in the appropriate mode.
//
    do { myfd = open(path, Oflag|O_LARGEFILE, Mode);}
       while( myfd < 0 && errno == EINTR);

// Chck if file is compressed
//
    if (myfd < 0) myfd = -errno;
#ifdef XRDOSSCX
       else if ((popts & XRDEXP_COMPCHK)
            && oocx_CXFile::isCompressed(myfd, cxid, &cxpgsz)) 
               if (Oflag != O_RDONLY) {close(myfd); return -XRDOSS_E8022;}
                  else attcx = 1;
#endif

// Relocate the file descriptor if need be and make sure file is closed on exec
//
    if (myfd >= 0)
       {if (myfd < XrdOssSS->FDFence)
           if ((newfd = fcntl(myfd, F_DUPFD, XrdOssSS->FDFence)) < 0)
              OssEroute.Emsg("XrdOssOpen_ufs",errno,"reloc FD",path);
              else {close(myfd); myfd = newfd;}
        fcntl(myfd, F_SETFD, FD_CLOEXEC);
#ifdef XRDOSSCX
        // If the file is compressed get a CXFile object and attach the FD to it
        //
        if (attcx) {cxobj = new oocx_CXFile;
                    ftype = (char *)" CXpath=";
                    if ((retc = cxobj->Attach(myfd, path)) < 0)
                       {close(myfd); myfd = retc; delete cxobj; cxobj = 0;}
                   }
#endif
       }

// Trace the action.
//
    TRACE(Open, "fd=" <<myfd <<" flags=" <<std::hex <<Oflag <<" mode="
                <<std::oct <<Mode <<std::dec <<ftype <<path);

// Deserialize the directory and return the result.
//
    if (popts & XRDEXP_REMOTE) ufs_file.UnSerialize(0);
    return myfd;
}