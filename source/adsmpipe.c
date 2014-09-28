/********************************************************/
/* File:         adsmpipe.c                             */
/* Program:      adsmpipe                               */
/* Author:       Tim Bell                               */
/*                                                      */
/* Modification: Matthias Eichstaedt                    */
/*               09/30/1995  -p Option added            */
/*               10/01/1995  function rcApiOut added    */
/*                                                      */
/* Pipe data to the ADSM backup or archive pools        */
/*                                                      */
/* (C) IBM 1995                                         */
/*                                                      */
/********************************************************/
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "dsmrc.h"
#include "dsmapitd.h"
#include "dsmapifp.h"

enum adsm_direction
{
  ad_none,
  ad_to,
  ad_from,
  ad_list,
  ad_delete
};

int verbose;

void usage()
{
#if DSM_API_VERSION >= 2
  fprintf(stderr,"adsmpipe [-[tcxd] -f <filename> [-l<size>] [-v]] ");
  fprintf(stderr,"[-p <oldpw/newpw/newpw>]\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"Creates, extracts or lists files in the ADSM pipe backup area\n");
  fprintf(stderr,"\t-t\tLists files in pipe backup area matching the pattern\n");
  fprintf(stderr,"\t-c\tCreates a file in pipe backup area.\n\t\t Data comes from standard input.\n");
  fprintf(stderr,"\t-x\tExtracts a file in pipe backup.\n\t\t Data goes to standard output.\n");
  fprintf(stderr,"\t-d\tDeletes the file from active store.\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"\t-B\tStore data in the backup space of the ADSM server\n");
  fprintf(stderr,"\t-A\tStore data in the archive space of the ADSM server\n");
  fprintf(stderr,"\t-f <file>\n\t\tProvides filename for create, and extract operations.\n\t\t For list operations, the filename can be a pattern\n");
  fprintf(stderr,"\t-l <size>\n\t\tEstimated size of data in bytes.\n\t\t This is only needed for create\n");
  fprintf(stderr,"\t-p <oldpw/newpw/newpw>\n\t\t<oldpw/newpw/newpw> Changes password\n");
  fprintf(stderr,"\t\t <passwd> Uses passwd for signon\n");
  fprintf(stderr,"\t-v\tVerbose\n");
  fprintf(stderr,"\t-m\tOverwrite management class (API Version 2.1.2 and higher)\n");
  fprintf(stderr,"\t-n <count>\n\t\tFile number to retrieve if multiple versions\n");
  fprintf(stderr,"\t-s <filespace>\n\t\tSpecify file space (default \"/adsmpipe\")\n");
#else
  fprintf(stderr,"adsmpipe [-[tcxd] -f <filename> [-l<size>] [-v]] ");
  fprintf(stderr,"[-p <oldpw/newpw/newpw>]\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"Creates, extracts or lists files in the ADSM pipe backup area\n");
  fprintf(stderr,"\t-t\tLists files in pipe backup area matching the pattern\n");
  fprintf(stderr,"\t-c\tCreates a file in pipe backup area.\n\t\t Data comes from standard input.\n");
  fprintf(stderr,"\t-x\tExtracts a file in pipe backup.\n\t\t Data goes to standard output.\n");
  fprintf(stderr,"\t-d\tDeletes the file from active store.\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"\t-B\tStore data in the backup space of the ADSM server\n");
  fprintf(stderr,"\t-A\tStore data in the archive space of the ADSM server\n");
  fprintf(stderr,"\t-f <file>\n\t\tProvides filename for create, and extract operations.\n\t\t For list operations, the filename can be a pattern\n");
  fprintf(stderr,"\t-l <size>\n\t\tEstimated size of data in bytes.\n\t\t This is only needed for create\n");
  fprintf(stderr,"\t-p <oldpw/newpw/newpw>\n\t\t<oldpw/newpw/newpw> Changes password\n");
  fprintf(stderr,"\t\t <passwd> Uses passwd for signon\n");
  fprintf(stderr,"\t-v\tVerbose\n");
  fprintf(stderr,"\t-n <count>\n\t\tFile number to retrieve if multiple versions\n");
  fprintf(stderr,"\t-s <filespace>\n\t\tSpecify file space (default \"/adsmpipe\")\n");
#endif
  exit(22);
}

int size_atoi(char *string)
{
  double dvalue=0;
  int ivalue=0;
  char *de=NULL;
  char *ie=NULL;
  char *e;
  int factor=1;
  dvalue=strtod(string,&de);
  ivalue=strtol(string,&ie,0);
  if (de>ie)
    e=de;
  else
  {
    e=ie;
    dvalue=ivalue;
  }
  if (e==string)
  {
    fprintf(stderr,"Error: invalid numeric value %s\n",string);
    usage();
  }
  switch(tolower(*e))
  {
  case 'g':
    factor=1024*1024*1024;
    ++e;
    break;
  case 'm':
    factor=1024*1024;
    ++e;
    break;
  case 'k':
    factor=1024;
    ++e;
    break;
  case 'b':
    factor=512;
    ++e;
  }
  return (int)(dvalue*factor);
}

int main(int argc, char **argv)
{
  int   arg=0;
  char  *adsm_filename=NULL;
  enum  adsm_direction direction=ad_none;
  int16 rc;
  char  *adsm_filespace="/pipe";
  char  *adsm_passwd=NULL;
  char  adsm_info[10240]="";
  int   adsm_filelength=1000000;
  char  *mgmtclass=NULL;
  int   arch_or_back='B';
  int   version=0;

  while (arg!=EOF)
  {
#if DSM_API_VERSION >= 2
    arg=getopt(argc, argv, "f:ctxs:l:vdD:ABm:n:p:");
#else
    arg=getopt(argc, argv, "f:ctxs:l:vdD:ABn:p:");
#endif
    switch(arg)
    {
    default:
    case '?':
      usage();
      break;
    case 'f':
      adsm_filename=optarg;
      break;
    case 'p':
      adsm_passwd=optarg;
      break;
    case 'c':
      direction=ad_to;
      break;
    case 't':
      direction=ad_list;
      break;
    case 'd':
      direction=ad_delete;
      break;
    case 'x':
      direction=ad_from;
      break;
    case 's':
      adsm_filespace=optarg;
      break;
    case 'D':
      strcpy(adsm_info,optarg);
      break;
    case 'l':
      adsm_filelength=size_atoi(optarg);
      break;
#if DSM_API_VERSION >= 2
    case 'm':
      mgmtclass=optarg;
      break;
#endif
    case 'n':
      version=atoi(optarg);
      break;
    case 'A':
    case 'B':
      arch_or_back=arg;
      break;
    case EOF:
      break;
    case 'v':
      verbose=1;
      break;
    }
  }

  if ((adsm_filename==NULL) && (adsm_passwd==NULL))
  {
    fprintf(stderr,"Error: you must supply a filename or pattern using -f\n");
    usage();
  }
  
  if (direction==ad_none)
  {
    if (adsm_passwd != NULL)
    {
      rc=blib_set_pw(adsm_passwd);
      if (rc!=0)
        exit(rc);
      rc=blib_update_pw(adsm_passwd);
      exit(rc);
    }
    else
    {
      usage();
    }
  }

  rc=blib_set_pw(adsm_passwd);
  if (rc!=0)
    exit(rc);

  rc=blib_init();
  if (rc!=0)
    exit(rc);

  rc=blib_register_filespace(adsm_filespace);
  if (rc!=0)
    exit(rc);

  rc=blib_set_filelength(adsm_filelength);
  if (rc!=0)
    exit(rc);

  rc=blib_set_verbose(verbose);
  if (rc!=0)
    exit(rc);

  rc=blib_set_store(arch_or_back,mgmtclass);
  if (rc!=0)
    exit(rc);

  switch(direction)
  {
  case ad_to:
    rc=blib_send_file(STDIN_FILENO,adsm_filename,adsm_info);
    break;
  case ad_from:
    rc=blib_get_file(STDOUT_FILENO,adsm_filename,version);
    break;
  case ad_list:
    rc=blib_list_files(STDOUT_FILENO,adsm_filename);
    break;
  case ad_delete:
    rc=blib_delete_files(STDOUT_FILENO,adsm_filename);
    break;
  }
  if (rc)
    exit(rc);
  
  rc=blib_term();
  return rc;
}

