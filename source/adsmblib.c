/*
 * File:        adsmblib.c
 * Program:     adsmpipe
 * Author:      Tim Bell
 *
 * Modification: Matthias Eichstaedt
 *               09/30/1995  -p Option added
 *               10/01/1995  function rcApiOut added
 * 
 * Provide basic ADSM API library which hides many of
 *  the functions of the full library but provides
 *  a basic store/retrieve interface
 *
 * (C) IBM 1995
 *
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "dsmrc.h"
#include "dsmapitd.h"
#include "dsmapifp.h"

/*
 * Globals for the BLIB library
 */
static unsigned short adsm_version;
static uint32 handle;
static char configfile[10]="";
static char options[100]="" ;
static char *rcMsg;
static char *adsm_filespace;
static char *logname;
static char splat[]="*";
static char *pw=NULL;
static char *new_pw=NULL;
static int  change_pw=0;   /* 1=change password; 0=do not change */


/*
 * iterate_data_t hides the difference between archive and 
 * backup data for iteration operations
 */
struct iterate_data_t
{
  dsmObjName  objName;  
  uint32      copyGroup;
  uint32      objState;
  char        owner[128];
  dsmDate     insDate;                        /* archive insertion date */
  dsmDate     expDate;                    /* expiration date for object */
  uint64      objId;
};
typedef int (*iterate_fn_t)(struct iterate_data_t *iterate_data);

static void  rcApiOut(int16 rc);
static int16 blib_iterate_files(int fd, char *adsm_filename, int active_only, iterate_fn_t fn);
static void backup_to_iterate_data(qryRespBackupData *qbDataArea,
                                   struct iterate_data_t *data);
static void archive_to_iterate_data(qryRespArchiveData *qaDataArea,
			                       struct iterate_data_t *data);

int16 blib_set_pw(char *pw_string)
{
  int16 rc=0;
  char  *token=NULL;
  char  *newpw1=NULL;
  char  *newpw2=NULL;

  if (pw_string != NULL)
  {
    token=strtok(pw_string, "/");
    pw=token;
    if (token != NULL)
    {
      token=strtok(NULL, "/");
      newpw1=token;
      if (token != NULL)
      {
        token=strtok(NULL, "/");
        newpw2=token;
      }
    }
  }
  if (((newpw1 == NULL) && (newpw2 != NULL))
     || ((newpw1 != NULL) && (newpw2 == NULL)))
  {
    fprintf(stderr,"Error: new passwords do not match\n");
    rc=1;
  }
  if ((newpw1 != NULL) && (newpw2 != NULL))
  {
    if (strcmp(newpw1, newpw2)==0)
    {
      new_pw=newpw1;
      change_pw=1;
    }
    else
    {
      fprintf(stderr,"Error: new passwords do not match\n");
      rc=1;
    }
  }
  return rc;
}

/*
 * blib_init
 *
 * Starts the basic ADSM interface
 */
int16 blib_init()
{
  dsmApiVersion      dsmApiVer ;
  dsmApiVersion      apiLibVer ;
  int16 rc;
  char  putenv_buffer[1024];


  memset(&apiLibVer,0x00,sizeof(dsmApiVersion));
  dsmQueryApiVersion(&apiLibVer);
  adsm_version = apiLibVer.version * 100;
  adsm_version = adsm_version + (apiLibVer.release * 10);
  adsm_version = adsm_version + apiLibVer.level;

  if (apiLibVer.version < DSM_API_VERSION)
  {
    fprintf(stderr, "\n***************************************************************\n");
    fprintf(stderr, "The ADSM API library is at a lower version than the application.\n");
    fprintf(stderr, "Install the current version.\n");
    fprintf(stderr, "***************************************************************\n");
    return -1;
  }
  if (apiLibVer.release < DSM_API_RELEASE)
  {
    fprintf(stderr, "\n***************************************************************\n");
    fprintf(stderr, "The ADSM API library is at a lower release than the application.\n");
    fprintf(stderr, "Proceed with caution.\n");
    fprintf(stderr, "***************************************************************\n");
    /* Make sure you know what changes you have incorporated from the  */
    /* later release to ensure compatibility.                          */
  }

  rcMsg = (char *)malloc(DSM_MAX_RC_MSG_LENGTH) ;

  /*========================================================================*/
  dsmApiVer.version = DSM_API_VERSION ;
  dsmApiVer.release = DSM_API_RELEASE ;
  dsmApiVer.level   = DSM_API_LEVEL   ;
  
  /* Initialize the API session */
  rc = dsmInit(&handle,
                    &dsmApiVer,
                    NULL,
                    NULL,
                    pw,
                    NULL,
                    configfile,
                    options);

  if (((rc != 0) && (rc != DSM_RC_REJECT_VERIFIER_EXPIRED))
     || ((rc == DSM_RC_REJECT_VERIFIER_EXPIRED) && (change_pw ==0)))
  {
    dsmRCMsg(handle,rc,rcMsg) ;
    fprintf(stderr, "error in dsmInit. rcMsg=%s\n",rcMsg);
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }
  logname=getenv("LOGNAME");
  if (change_pw == 1)
  {
    if ((rc = dsmChangePW(handle,
                          pw,
                          new_pw)) != 0)
    {
      dsmRCMsg(handle,rc,rcMsg) ;
      fprintf(stderr, "Password change failed. rcMsg=%s\n",rcMsg);
      free(rcMsg);
      dsmTerminate(handle);
      return rc;
    }
    else
    {
      fprintf(stderr,"Your new password has been accepted and ");
      fprintf(stderr,"updated.\n");
    }
  }
  return 0;
}

/*
 * blib_update_pw
 *
 * Updates the password only
 */
int16 blib_update_pw()
{
  int16 rc;
  
  rc=blib_init();
  dsmTerminate(handle);
  return rc;
} 

/*
 * blib_register_filespace
 *
 * Creates a file space if not already created
 *
 */
int16 blib_register_filespace(char *filespace_name)
{
  regFSData          regFilespace ;
  int16 rc;

  /************ Build File Space ****************************************/
  regFilespace.fsName = (char *)malloc(100) ;
  regFilespace.fsType = (char *)malloc(100) ;
  
  strcpy(regFilespace.fsName,filespace_name) ;
  strcpy(regFilespace.fsType,"ADSMPIPE") ;
  regFilespace.capacity.lo = 0;
  regFilespace.capacity.hi = 0;
  regFilespace.occupancy.lo = 0;
  regFilespace.occupancy.hi = 0;
  regFilespace.stVersion = regFSDataVersion ;
  regFilespace.fsAttr.unixFSAttr.fsInfoLength = 9 ;
  strcpy(regFilespace.fsAttr.unixFSAttr.fsInfo,"adsmpipe") ;

  /* Register the file space */
  rc = dsmRegisterFS(handle, &regFilespace) ;
  if ((rc != 0) && (rc != DSM_RC_FS_ALREADY_REGED))
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error in dsmRegisterFS. rcMsg=%s (%d)\n", rcMsg,rc );
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }
  rc=0;
  adsm_filespace=(void*)strdup(filespace_name);
  return rc;
}

static int adsm_filelength;

/*
 * blib_set_filelength
 *
 * Sets the maximum length for the ADSM put operations
 *
 */
int blib_set_filelength(int length)
{
  adsm_filelength=length;
  return 0;
}

static int adsm_verbose;

/*
 * blib_set_verbose
 *
 * Controls verbose output from routines
 *
 */
int blib_set_verbose(int verbose)
{
  adsm_verbose=verbose;
  return 0;
}

static int adsm_arch_or_back;
#define ADSM_IS_ARCHIVE ((adsm_arch_or_back)=='A')
#define ADSM_IS_BACKUP ((adsm_arch_or_back)=='B')
static char *adsm_mgmtclass;

/*
 * blib_set_store
 *
 * Archive vs backup
 * management class to use
 *
 */
int blib_set_store(int arch_or_back, char *management_class)
{
  switch(arch_or_back)
  {
  case 'A':
  case 'B':
    break;
  default:
    fprintf(stderr,"Error: invalid archive setting\n");
    return EINVAL;
  }
  adsm_arch_or_back=arch_or_back;
  adsm_mgmtclass=management_class;
  return 0;
}

#define BUFFER_SIZE (65536*4)

char *blib_basename(char *name)
{
  char *p = strrchr(name, '/');
  if (p == NULL) return name;
  else return p + 1;
}

static void makeObjName(dsmObjName *objName, char *adsm_filename)
{
  char *b;
  int dirname_len;

  objName->objType = DSM_OBJ_FILE ;
  if (*adsm_filename=='/')
    ++adsm_filename;

  b=blib_basename(adsm_filename);
  dirname_len=b-adsm_filename;
  if (dirname_len>0)
    --dirname_len;
  strcpy(objName->fs, adsm_filespace);
  sprintf(objName->hl,"/%*.*s",dirname_len,dirname_len,adsm_filename);
  sprintf(objName->ll,"/%s",b) ;
}

/*
 * Send the contents of file handle 'fd' to the adsm_filename
 * associating the comment 'adsm_info'
 */
int16 blib_send_file(int fd,char *adsm_filename,char *adsm_info)
{
  char buffer[BUFFER_SIZE];
  dsmObjName         objName ;
  char               *b;
  int                dirname_len;
  mcBindKey          MCBindKey;
  ObjAttr            objAttrArea;
  int                read_rc;
  int16              rc;
  DataBlk            dataBlkArea;
  uint16             reason ;
  int                count;
  int                read_more=1;
  int                read_offset;
  int                state;
  sndArchiveData  archData;
  sndArchiveData *archDataPtr=NULL;

  makeObjName(&objName,adsm_filename);

  switch (adsm_arch_or_back)
  {
  case 'A':
    state              = stArchiveMountWait;
    archData.stVersion = sndArchiveDataVersion;
    archData.descr     = adsm_info;
    archDataPtr        = &archData;
    break;
  case 'B':
  default :
    state=stBackupMountWait;
    break;
  }

  if ((rc = dsmBeginTxn(handle)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error in dsmBeginTxn. rcMsg=%s (%d)\n", rcMsg,rc) ;
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  memset(&MCBindKey,0x00,sizeof(mcBindKey));
  MCBindKey.stVersion = mcBindKeyVersion ;
 
  if ((rc = dsmBindMC(handle,
		      &objName,
		      state,
		      &MCBindKey)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error in dsmBindMC. rcMsg=%s (%d)\n", rcMsg,rc);
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  /*=======================================================================*/
  strcpy(objAttrArea.owner,logname);       
  objAttrArea.sizeEstimate.hi = 0;       /* size estimate of object */
  if (adsm_filelength==0)
    adsm_filelength=1000000;
  objAttrArea.sizeEstimate.lo = adsm_filelength;      /* size estimate of object */
  objAttrArea.objCompressed = bFalse ;  
  objAttrArea.objInfoLength = strlen(adsm_info);      /* length of objInfo */
  objAttrArea.objInfo = (char *)malloc(objAttrArea.objInfoLength + 1) ;

#if DSM_API_VERSION >= 2
  if (adsm_version >= 212)
  {
    /* override mgmt class name */
    objAttrArea.mcNameP = adsm_mgmtclass;
    objAttrArea.stVersion = ObjAttrVersion ;
  }
  else
  {
    objAttrArea.stVersion = 1 ;
    if (adsm_mgmtclass != NULL)
    {
      fprintf(stderr, "The installed library does not support ");
      fprintf(stderr, "the -m option.\n");
      fprintf(stderr, "The management class option will be ignored.\n");
    } /* if */
  } /* if */
#endif

  strcpy(objAttrArea.objInfo,adsm_info);      /* object-dependent info */

  /* Backup the data  */
  if ((rc = dsmSendObj(handle,
		       state,
		       archDataPtr,
		       &objName,
		       &objAttrArea,
		       NULL)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error in dsmSendData. rcMsg=%s (%d)\n",rcMsg,rc);
    if (rc == DSM_RC_WILL_ABORT)
    {
      rc = dsmEndTxn(handle,DSM_VOTE_COMMIT,&reason);
      if (rc == DSM_RC_CHECK_REASON_CODE)
      {
        fprintf(stderr,"reason code: (%d)\n",reason);
      }
      else
      {
        dsmRCMsg(handle,rc,rcMsg);
        fprintf(stderr, "error in dsmEndTxn. rcMsg=%s (%d)\n",rcMsg,rc);
      }
    }
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  /*========================================================================*/
  dataBlkArea.bufferPtr = buffer;
  dataBlkArea.stVersion = DataBlkVersion ;

  if (adsm_verbose)
    fprintf(stderr,"adsm: about to send data to ADSM\n");

  count=0;
  while (read_more)
  {
    read_rc=0;
    for (read_offset=0;read_offset<BUFFER_SIZE;read_offset+=read_rc)
    {
      read_rc=read(fd,buffer+read_offset,BUFFER_SIZE-read_offset);
      if (read_rc==0)
	break;

      if (read_rc<0)
      {
	read_more=0;
	break;
      }
      count+=read_rc;
    }
    if (read_offset>0)
    {
      dataBlkArea.bufferLen=read_offset;
      if ((rc = dsmSendData(handle,&dataBlkArea)) != DSM_RC_OK)
      {
	dsmRCMsg(handle,rc,rcMsg) ;  
	fprintf(stderr, "error in dsmSendData. rcMsg=%s (%d)\n",rcMsg,rc);
        if (rc == DSM_RC_WILL_ABORT)
        {
          rc = dsmEndTxn(handle,DSM_VOTE_COMMIT,&reason);
          if (rc == DSM_RC_CHECK_REASON_CODE)
          {
            fprintf(stderr,"reason code: (%d)\n",reason);
          }
          else
          {
            dsmRCMsg(handle,rc,rcMsg);
	    fprintf(stderr, "error in dsmEndTxn. rcMsg=%s (%d)\n",rcMsg,rc);
          }
        }
	free(rcMsg);
	dsmTerminate(handle);
	return rc;
      }
      if (adsm_verbose)
	fprintf(stderr,"adsm: sent %d bytes to ADSM\n",count);
    }
    else
      break;
  }

  if (read_rc==-1)
  {
    perror("read");
    return -1;
  }


  dsmEndSendObj(handle) ;


  if (adsm_verbose)
    fprintf(stderr,"adsm: committing transaction\n");

  if ((rc = dsmEndTxn(handle,DSM_VOTE_COMMIT, &reason)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error in dsmEndTxn. rcMsg=%s (%d) reason=%d\n", rcMsg,rc,reason) ;
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }
  return 0;
}

/*
 * Print the information associated with iterate_data
 */
static void dumpDataArea(struct iterate_data_t *iterate_data)
{
  dsmObjName *objName;
  char buffer[PATH_MAX];
  char *state="(A)";
  char *fs;

  objName=&iterate_data->objName;
  if (iterate_data->objState==DSM_INACTIVE)
    state="(I)";

  fs=objName->fs;
  if (strcmp(adsm_filespace,fs)==0)
    fs="";
  if (strcmp(objName->hl,"/")==0)
  {
    sprintf(buffer,"%s%s",fs,objName->ll);
  }
  else
  {
    sprintf(buffer,"%s%s%s",fs,objName->hl,objName->ll);
  }
  fprintf(stderr, "%-8.8s %3.3s %4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d %s",
	 iterate_data->owner,
	 state,
	 iterate_data->insDate.year,
	 iterate_data->insDate.month,
	 iterate_data->insDate.day,
	 iterate_data->insDate.hour,
	 iterate_data->insDate.minute,
	 iterate_data->insDate.second,
	 buffer);
  fprintf(stderr, "\n");
}

/*
 * Gets the contents of file and sends it to handle 'fd'.
 * 'version' more recent files are skipped.
 */
int16 blib_get_file(int fd, char *adsm_filename,int version)
{
  char               buffer[BUFFER_SIZE];
  dsmQueryType       queryType;
  DataBlk            qDataBlkArea, getBlk , dummyDataBlkArea;
  DataBlk            *qDataBlkAreaPtr;
  qryBackupData      queryBackupBuffer;
  qryRespBackupData  qbDataArea, qbdummyDataArea;
  qryArchiveData     queryArchiveBuffer;
  qryRespArchiveData qaDataArea, qadummyDataArea;
  void              *queryBufferPtr;
  char              *b;
  int                dirname_len;
  dsmObjName         objName ;
  int16              rc;
  dsmGetList         dsmObjList ;
  int write_rc;
  int count;
  int version_index;
  char *owner;
  dsmGetType         getType;
  
  owner=(char *)malloc(100) ;
  strcpy(owner, logname);

  dsmObjList.stVersion = 1 ;
  dsmObjList.numObjId = 1 ;
  dsmObjList.objId = (ObjID *)malloc(sizeof(ObjID) * dsmObjList.numObjId) ;

  getBlk.bufferPtr = buffer;
  getBlk.bufferLen = sizeof(buffer);
  getBlk.stVersion = DataBlkVersion ;

  makeObjName(&objName,adsm_filename);
    
  switch(adsm_arch_or_back)
  {
  case 'B':
    queryType = qtBackup ;

    qbDataArea.stVersion = qryRespBackupDataVersion ;
    qDataBlkArea.stVersion = DataBlkVersion ;
    qDataBlkArea.bufferPtr = (char *)&qbDataArea;
    qDataBlkArea.bufferLen = sizeof(qryRespBackupData);
    qbdummyDataArea.stVersion = qryRespBackupDataVersion ;
    dummyDataBlkArea.stVersion = DataBlkVersion ;
    dummyDataBlkArea.bufferPtr = (char *)&qbdummyDataArea;
    dummyDataBlkArea.bufferLen = sizeof(qryRespBackupData);
    
    queryBackupBuffer.objName = (dsmObjName *)malloc(sizeof(dsmObjName)) ;
    queryBackupBuffer.owner = owner;

    strcpy(queryBackupBuffer.objName->fs, objName.fs);
    strcpy(queryBackupBuffer.objName->hl, objName.hl);
    strcpy(queryBackupBuffer.objName->ll, objName.ll);
    queryBackupBuffer.objName->objType = objName.objType;
    /*queryBuffer.copyGroup = DSM_COPYGROUP_ANY_MATCH;*/
    /*queryBuffer.mgmtClass = DSM_MGMTCLASS_ANY_MATCH;*/
    queryBackupBuffer.owner=owner;
    queryBackupBuffer.objState = DSM_ANY_MATCH;
    queryBackupBuffer.stVersion = qryBackupDataVersion ;
    queryBufferPtr=&queryBackupBuffer;
    break;

  case 'A':
    queryType=qtArchive;

    queryArchiveBuffer.stVersion = qryArchiveDataVersion;
    queryArchiveBuffer.objName   = &objName;
    queryArchiveBuffer.owner     = owner;

    /* Note this needs to be changed when dialog handles date input */
    queryArchiveBuffer.insDateLowerBound.year   = DATE_MINUS_INFINITE;
    queryArchiveBuffer.insDateLowerBound.month  = 0;
    queryArchiveBuffer.insDateLowerBound.day    = 0;
    queryArchiveBuffer.insDateLowerBound.hour   = 0;
    queryArchiveBuffer.insDateLowerBound.minute = 0;
    queryArchiveBuffer.insDateLowerBound.second = 0;

    queryArchiveBuffer.insDateUpperBound.year   = DATE_PLUS_INFINITE;
    queryArchiveBuffer.insDateUpperBound.month  = 0;
    queryArchiveBuffer.insDateUpperBound.day    = 0;
    queryArchiveBuffer.insDateUpperBound.hour   = 0;
    queryArchiveBuffer.insDateUpperBound.minute = 0;
    queryArchiveBuffer.insDateUpperBound.second = 0;

    queryArchiveBuffer.expDateLowerBound.year   = DATE_MINUS_INFINITE;
    queryArchiveBuffer.expDateLowerBound.month  = 0;
    queryArchiveBuffer.expDateLowerBound.day    = 0;
    queryArchiveBuffer.expDateLowerBound.hour   = 0;
    queryArchiveBuffer.expDateLowerBound.minute = 0;
    queryArchiveBuffer.expDateLowerBound.second = 0;

    queryArchiveBuffer.expDateUpperBound.year   = DATE_PLUS_INFINITE;
    queryArchiveBuffer.expDateUpperBound.month  = 0;
    queryArchiveBuffer.expDateUpperBound.day    = 0;
    queryArchiveBuffer.expDateUpperBound.hour   = 0;
    queryArchiveBuffer.expDateUpperBound.minute = 0;
    queryArchiveBuffer.expDateUpperBound.second = 0;

    queryArchiveBuffer.descr = splat;
    
    qDataBlkArea.stVersion = DataBlkVersion;
    qDataBlkArea.bufferLen = sizeof(qryRespArchiveData);
    qDataBlkArea.bufferPtr = (char *)&qaDataArea;
    qaDataArea.stVersion = qryRespArchiveDataVersion;
    queryBufferPtr = (dsmQueryBuff *)&queryArchiveBuffer;
    dummyDataBlkArea.stVersion = DataBlkVersion;
    dummyDataBlkArea.bufferLen = sizeof(qryRespArchiveData);
    dummyDataBlkArea.bufferPtr = (char *)&qadummyDataArea;
    qadummyDataArea.stVersion = qryRespArchiveDataVersion;
  }

  if ((rc=dsmBeginQuery(handle,queryType, (void *)queryBufferPtr)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error in dsmBeginQuery. rcMsg=%s (%d)\n",rcMsg,rc);
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  } 

  version_index=1;
  qDataBlkAreaPtr = &qDataBlkArea;
  while ((rc = dsmGetNextQObj(handle, qDataBlkAreaPtr)) == DSM_RC_MORE_DATA)
  {
    if (version_index>=version)
    {
      qDataBlkAreaPtr = &dummyDataBlkArea;
    }
    else
    {
      if (adsm_verbose)
      {
        struct iterate_data_t iterate_data;
        fprintf(stderr, "Skipping file version %d\n",version_index);
        switch(adsm_arch_or_back)
        {
        case 'A':
	      archive_to_iterate_data(&qaDataArea,&iterate_data);
          break;
        case 'B':
          backup_to_iterate_data(&qbDataArea,&iterate_data);
          break;
        }
        dumpDataArea(&iterate_data);
      }
      ++version_index;
    }
  }

  if ((rc != DSM_RC_FINISHED) && (rc != DSM_RC_MORE_DATA))
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error from dsmGetNextQObj. rcMsg=%s\n (%d)", rcMsg),rc ;
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  if ((rc = dsmEndQuery(handle)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error from dsmEndQuery. rcMsg=%s (%d)\n", rcMsg,rc) ;
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  switch(adsm_arch_or_back)
  {
  case 'B':
    dsmObjList.objId[0].hi = qbDataArea.objId.hi ;
    dsmObjList.objId[0].lo = qbDataArea.objId.lo ;
    getType=gtBackup;
    break;
  case 'A':
    dsmObjList.objId[0].hi = qaDataArea.objId.hi ;
    dsmObjList.objId[0].lo = qaDataArea.objId.lo ;
    getType=gtArchive;
    break;
  }

  if (adsm_verbose)
  {
    struct iterate_data_t iterate_data;
    fprintf(stderr,"adsm: about to get data\n");
    switch(adsm_arch_or_back)
    {
      case 'A':
	archive_to_iterate_data(&qaDataArea,&iterate_data);
	break;
      case 'B':
	backup_to_iterate_data(&qbDataArea,&iterate_data);
	break;
      }
    dumpDataArea(&iterate_data);
  }

  if ((rc = dsmBeginGetData(handle,bTrue,getType,&dsmObjList)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error from dsmBeginGetData. rcMsg=%s (%d)\n", rcMsg,rc) ;
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  count=0;
  rc = dsmGetObj(handle, &(dsmObjList.objId[0]), &getBlk) ;

  if (rc == DSM_RC_FINISHED)
  {
    count+=getBlk.numBytes;
    if (adsm_verbose)
      fprintf(stderr,"adsm: read %d bytes from ADSM\n",count);
    write_rc=write(fd,getBlk.bufferPtr,getBlk.numBytes);
    if (write_rc!=getBlk.numBytes)
    {
      perror("write");
      return -1;
    }
    if ((rc = dsmEndGetObj(handle)) != DSM_RC_OK)
    {
      dsmRCMsg(handle,rc,rcMsg) ;  
      fprintf(stderr, "error from dsmEndGetObj. rcMsg=%s (%d)\n", rcMsg,rc) ;
      free(rcMsg);
      dsmTerminate(handle);
      return rc;
    }
  }
  else if (rc == DSM_RC_MORE_DATA)
  {
    count+=getBlk.numBytes;
    if (adsm_verbose)
      fprintf(stderr,"adsm: read %d bytes from ADSM\n",count);
    write_rc=write(fd,getBlk.bufferPtr,getBlk.numBytes);
    if (write_rc!=getBlk.numBytes)
    {
      perror("write");
      return -1;
    }
    while ((rc = dsmGetData(handle, &getBlk)) == DSM_RC_MORE_DATA) 
    {
      count+=getBlk.numBytes;
      if (adsm_verbose)
	fprintf(stderr,"adsm: read %d bytes from ADSM\n",count);
      write_rc=write(fd,getBlk.bufferPtr,getBlk.numBytes);
      if (write_rc!=getBlk.numBytes)
      {
	perror("write");
	return -1;
      }
    }

    if (rc == DSM_RC_FINISHED)
    {
      count+=getBlk.numBytes;
      if (adsm_verbose)
	fprintf(stderr,"adsm: read %d bytes from ADSM\n",count);
      write_rc=write(fd,getBlk.bufferPtr,getBlk.numBytes);
      if (write_rc!=getBlk.numBytes)
      {
	perror("write");
	return -1;
      }
      if ((rc = dsmEndGetObj(handle)) != DSM_RC_OK)
      {
	dsmRCMsg(handle,rc,rcMsg) ;  
	fprintf(stderr, "error from dsmEndGetObj. rcMsg=%s (%d)\n", rcMsg,rc);
	free(rcMsg);
	dsmTerminate(handle);
	return rc;
      }
    }
    else    
    {
      dsmRCMsg(handle,rc,rcMsg) ;  
      fprintf(stderr, "error from dsmGetData. rcMsg=%s (%d)\n",rcMsg,rc);
      free(rcMsg);
      dsmTerminate(handle);
      return rc;
    }
  }
  else
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error from dsmGetObj. rcMsg=%s (%d)\n",rcMsg,rc);
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  if ((rc = dsmEndGetData(handle)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error from dsmEndGetData. rcMsg=%s (%d)\n",rcMsg,rc);
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  if (adsm_verbose)
    fprintf(stderr,"adsm: read completed\n");

  return 0;
}

/*
 * list the files that match adsm_filename
 */
int16 blib_list_files(int fd, char *adsm_filename)
{
  int16 rc;

  rc=blib_iterate_files(fd, adsm_filename, 0, (iterate_fn_t)dumpDataArea);
  return rc;
}

static void backup_to_iterate_data(qryRespBackupData *qbDataArea,
			      struct iterate_data_t *data)
{
  data->objName=qbDataArea->objName;
  data->copyGroup=qbDataArea->copyGroup;
  data->objState=qbDataArea->objState;
  strcpy(data->owner,qbDataArea->owner);
  data->insDate=qbDataArea->insDate;
  data->expDate=qbDataArea->expDate;
  data->objId.lo=0;
  data->objId.hi=0;
}

static void archive_to_iterate_data(qryRespArchiveData *qaDataArea,
			      struct iterate_data_t *data)
{
  data->objName=qaDataArea->objName;
  data->copyGroup=qaDataArea->copyGroup;
  data->objState=DSM_ACTIVE;	/* Archive always active */
  strcpy(data->owner,qaDataArea->owner);
  data->insDate=qaDataArea->insDate;
  data->expDate=qaDataArea->expDate;
  data->objId=qaDataArea->objId;
}

/*
 * iterate over the files and call 'fn' for each one.  This
 * is used by delete and list to find the right object
 */
static int16 blib_iterate_files(int fd, char *adsm_filename, int active_only, iterate_fn_t fn)
{
  char               buffer[BUFFER_SIZE];
  dsmQueryType       queryType;
  DataBlk            qDataBlkArea, getBlk ;
  qryBackupData      queryBuffer;
  qryRespBackupData  qbDataArea;
  char              *b;
  int                dirname_len;
  dsmObjName         objName ;
  int16              rc;
  dsmGetList         dsmObjList ;
  int write_rc;
  qryArchiveData     queryArchiveBuffer;
  qryRespArchiveData qaDataArea;
  void              *queryBufferPtr;
  struct iterate_data_t iterate_data;

  dsmObjList.stVersion = 1 ;
  dsmObjList.numObjId = 1 ;
  dsmObjList.objId = (ObjID *)malloc(sizeof(ObjID) * dsmObjList.numObjId) ;
  
  makeObjName(&objName,adsm_filename);

  getBlk.bufferPtr = buffer;
  getBlk.bufferLen = sizeof(buffer);
  getBlk.stVersion = DataBlkVersion ;


  switch(adsm_arch_or_back)
  {
  case 'B':
    queryType = qtBackup;

    qbDataArea.stVersion = qryRespBackupDataVersion ;
    qDataBlkArea.stVersion = DataBlkVersion ;
    qDataBlkArea.bufferPtr = (char *)&qbDataArea;
    qDataBlkArea.bufferLen = sizeof(qryRespBackupData);

    queryBuffer.objName = (dsmObjName *)malloc(sizeof(dsmObjName)) ;
    queryBuffer.owner = (char *)malloc(100) ;

    strcpy(queryBuffer.objName->fs, objName.fs);
    strcpy(queryBuffer.objName->hl, objName.hl);
    strcpy(queryBuffer.objName->ll, objName.ll);
    queryBuffer.objName->objType = objName.objType;
    /*queryBuffer.copyGroup = DSM_COPYGROUP_ANY_MATCH;*/
    /*queryBuffer.mgmtClass = DSM_MGMTCLASS_ANY_MATCH;*/
    strcpy(queryBuffer.owner, logname);
    if (active_only)
      queryBuffer.objState = DSM_ACTIVE;
    else
      queryBuffer.objState = DSM_ANY_MATCH;
    queryBuffer.stVersion = qryBackupDataVersion ;
    queryBufferPtr=(void *)&queryBuffer;
    break;

  case 'A':
    queryType=qtArchive;

    queryArchiveBuffer.stVersion = qryArchiveDataVersion;
    queryArchiveBuffer.objName   = &objName;
    queryArchiveBuffer.owner     = (char *)malloc(100);

    /* Note this needs to be changed when dialog handles date input */
    queryArchiveBuffer.insDateLowerBound.year   = DATE_MINUS_INFINITE;
    queryArchiveBuffer.insDateLowerBound.month  = 0;
    queryArchiveBuffer.insDateLowerBound.day    = 0;
    queryArchiveBuffer.insDateLowerBound.hour   = 0;
    queryArchiveBuffer.insDateLowerBound.minute = 0;
    queryArchiveBuffer.insDateLowerBound.second = 0;

    queryArchiveBuffer.insDateUpperBound.year   = DATE_PLUS_INFINITE;
    queryArchiveBuffer.insDateUpperBound.month  = 0;
    queryArchiveBuffer.insDateUpperBound.day    = 0;
    queryArchiveBuffer.insDateUpperBound.hour   = 0;
    queryArchiveBuffer.insDateUpperBound.minute = 0;
    queryArchiveBuffer.insDateUpperBound.second = 0;

    queryArchiveBuffer.expDateLowerBound.year   = DATE_MINUS_INFINITE;
    queryArchiveBuffer.expDateLowerBound.month  = 0;
    queryArchiveBuffer.expDateLowerBound.day    = 0;
    queryArchiveBuffer.expDateLowerBound.hour   = 0;
    queryArchiveBuffer.expDateLowerBound.minute = 0;
    queryArchiveBuffer.expDateLowerBound.second = 0;

    queryArchiveBuffer.expDateUpperBound.year   = DATE_PLUS_INFINITE;
    queryArchiveBuffer.expDateUpperBound.month  = 0;
    queryArchiveBuffer.expDateUpperBound.day    = 0;
    queryArchiveBuffer.expDateUpperBound.hour   = 0;
    queryArchiveBuffer.expDateUpperBound.minute = 0;
    queryArchiveBuffer.expDateUpperBound.second = 0;

    queryArchiveBuffer.descr = splat;
    
    qDataBlkArea.stVersion = DataBlkVersion;
    qDataBlkArea.bufferLen = sizeof(qryRespArchiveData);
    qDataBlkArea.bufferPtr = (char *)&qaDataArea;
    qaDataArea.stVersion = qryRespArchiveDataVersion;
    queryBufferPtr = (dsmQueryBuff *)&queryArchiveBuffer;
  }

  if ((rc=dsmBeginQuery(handle,queryType, (void *)queryBufferPtr)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error in dsmBeginQuery. rcMsg=%s (%d)\n",rcMsg,rc);
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  } 

  while ((rc = dsmGetNextQObj(handle, &qDataBlkArea)) == DSM_RC_MORE_DATA)
  {
    switch(adsm_arch_or_back)
    {
    case 'B':
      backup_to_iterate_data(&qbDataArea,&iterate_data);
      break;
    case 'A':
      archive_to_iterate_data(&qaDataArea,&iterate_data);
      break;
    }
    (*fn)(&iterate_data);
  }
  if ((rc != DSM_RC_FINISHED) && (rc != DSM_RC_MORE_DATA))
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error from dsmGetNextQObj. rcMsg=%s\n", rcMsg) ;
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }

  if ((rc = dsmEndQuery(handle)) != DSM_RC_OK)
  {
    dsmRCMsg(handle,rc,rcMsg) ;  
    fprintf(stderr, "error from dsmEndQuery. rcMsg=%s (%d)\n",rcMsg,rc);
    free(rcMsg);
    dsmTerminate(handle);
    return rc;
  }
  return 0;
}

dsmObjName delObjName;
uint32     delObjCopyGroup;
int        delObjFound;
uint64     delObjId;

void deleteDataArea(struct iterate_data_t *iterate_data)
{
  dsmObjName *objName;

  objName=&iterate_data->objName;
  delObjName=*objName;
  delObjCopyGroup=iterate_data->copyGroup;
  delObjId=iterate_data->objId;
  delObjFound=1;
}

void deleteCurrentData()
{
  dsmDelInfo delInfo;
  dsmDelType delType;
  int16      rc;
  uint16     txn_reason=0;
  dsmObjName *objName;

  rc = dsmBeginTxn(handle);
  if (rc!=0)
  {
    fprintf(stderr, "*** dsmBeginTxn(1) failed: ");
    rcApiOut(rc);
  }
  else
  {
    memset(&delInfo,0x00,sizeof(delInfo));
    objName=&delObjName;
    switch(adsm_arch_or_back)
    {
    case 'B':
      delInfo.backInfo.stVersion = delBackVersion;
      delInfo.backInfo.objNameP  = objName;
      delInfo.backInfo.copyGroup = delObjCopyGroup;
      delType = dtBackup;
      break;
    case 'A':
      delInfo.archInfo.stVersion = delArchVersion;
      delInfo.archInfo.objId = delObjId;
      delType = dtArchive;
    }
    rc = dsmDeleteObj(handle,delType,delInfo);
    if (rc)
    {
      fprintf(stderr, "*** dsmDeleteObj failed: ");
      rcApiOut(rc);
    }
  }

  rc = dsmEndTxn(handle,          /* DSM API sess handle         */
		 DSM_VOTE_COMMIT,     /* Commit transaction.         */
		 &txn_reason);        /* Reason if txn aborted.      */
  if (rc || txn_reason)
  {
    fprintf(stderr, "*** dsmEndTxn(2) failed: ");
    rcApiOut(rc);
    fprintf(stderr, "   Reason = ");
    rcApiOut(txn_reason);
  }
  return;
}

/*
 * delete the first file which matches the filename
 */
int16 blib_delete_files(int fd, char *adsm_filename)
{
  int16 rc;

  delObjFound=0;
  rc=blib_iterate_files(fd,adsm_filename,1,(iterate_fn_t)deleteDataArea);
  if (rc==0)
    deleteCurrentData();
  return 0;
}

/*
 * Close session
 */
int16 blib_term()
{
  int16 rc;
  dsmTerminate(handle);
  rc=0;
  return rc;
}

/*
 * Print the message text
 */
static void rcApiOut(int16 rc)
{

     char *msgBuf ;
     uint32   fakeHandle=1 ;

     if ((msgBuf = (char *)malloc(DSM_MAX_RC_MSG_LENGTH+1)) == NULL)
     {
          fprintf(stderr, "Abort: Not enough memory.\n") ;
          exit(1) ;
     }

     dsmRCMsg(fakeHandle,rc, msgBuf);

     fprintf(stderr, "%s\n",msgBuf);

     free(msgBuf) ;

     return;
}

