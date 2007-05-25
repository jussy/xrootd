/******************************************************************************/
/*                                                                            */
/*                       X r d O u c P l u g i n . c c                        */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//       $Id$

const char *XrdOucPluginCVSID = "$Id$";

// Bypass Solaris ELF madness
//
#ifdef __solaris__
#include <sys/isa_defs.h>
#if defined(_ILP32) && (_FILE_OFFSET_BITS != 32)
#undef  _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#undef  _LARGEFILE_SOURCE
#endif
#endif

#ifndef WIN32
#include <dlfcn.h>
#if !defined(__macos__) && !defined(__CYGWIN__)
#include <link.h>
#endif
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <errno.h>
#include <iostream.h>
#else
#include "XrdSys/XrdWin32.hh"
#include <iostream>
using namespace std;
#endif
  
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPlugin.hh"
 
/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdOucPlugin::~XrdOucPlugin()
{
   if (libHandle) dlclose(libHandle);
}

/******************************************************************************/
/*                             g e t P l u g i n                              */
/******************************************************************************/
  
void *XrdOucPlugin::getPlugin(const char *pname, int errok)
{
   void *ep;

// Open the plugin library if not already opened
//
   if (!libHandle && !(libHandle = dlopen(libPath, RTLD_NOW)))
      {eDest->Emsg("getPlugin", "Unable to open", libPath, dlerror());
       return 0;
      }

// Get the plugin object creator
//
   if (!(ep = dlsym(libHandle, pname)) && !errok)
      {char buff[1024];
       printf(buff, "Unable to find %s in", pname);
       eDest->Emsg("getPlugin", buff, libPath, dlerror());
       return 0;
      }

// All done
//
   return ep;
}