#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

  typedef struct __NexusFile {
    struct iStack {
      int32 *iRefDir;
      int32 *iTagDir;
      int32 iVref;
      int32 __iStack_pad;               
      int iNDir;
      int iCurDir;
    } iStack[NXMAXSTACK];
    struct iStack iAtt;
    int32 iVID;
    int32 iSID;
    int32 iCurrentVG;
    int32 iCurrentSDS;
    int iNXID;
    int iStackPtr;
    char iAccess[2];
  } NexusFile, *pNexusFile;
   
    
   /*--------------------------------------------------------------------*/

  static pNexusFile NXIassert(NXhandle fid)
  {
    pNexusFile pRes;
  
    assert(fid != NULL);
    pRes = (pNexusFile)fid;
    assert(pRes->iNXID == NXSIGNATURE);
    return pRes;
  }
  
  /* --------------------------------------------------------------------- */

  static int32 NXIFindVgroup (pNexusFile pFile, char *name, char *nxclass)
  {
    int32 iNew, iRef, iTag;
    int iN, i;
    int32 *pArray = NULL;
    NXname pText;
  
    assert (pFile != NULL);
  
    if (pFile->iCurrentVG == 0) { /* root level */
      /* get the number and ID's of all lone Vgroups in the file */
      iN = Vlone (pFile->iVID, NULL, 0);
      if(iN == 0) {
         return NX_EOD;
      }
      pArray = (int32 *) malloc (iN * sizeof (int32));
      if (!pArray) {
        NXIReportError (NXpData, "ERROR: out of memory in NXIFindVgroup");
        return NX_EOD;
      }
      Vlone (pFile->iVID, pArray, iN);
  
      /* loop and check */
      for (i = 0; i < iN; i++) {
        iNew = Vattach (pFile->iVID, pArray[i], "r");
        Vgetname (iNew, pText);
        if (strcmp (pText, name) == 0) {
          Vgetclass (iNew, pText);
          if (strcmp (pText, nxclass) == 0) {
            /* found ! */
            Vdetach (iNew);
            iNew = pArray[i];
            free (pArray);
            return iNew;
          }
        }
        Vdetach (iNew);
      }
      /* nothing found */
      free (pArray);
      return NX_EOD;
    } else {                      /* case in Vgroup */
      iN = Vntagrefs (pFile->iCurrentVG);
      for (i = 0; i < iN; i++) {
        Vgettagref (pFile->iCurrentVG, i, &iTag, &iRef);
        if (iTag == DFTAG_VG) {
          iNew = Vattach (pFile->iVID, iRef, "r");
          Vgetname (iNew, pText);
          if (strcmp (pText, name) == 0) {
            Vgetclass (iNew, pText);
            if (strcmp (pText, nxclass) == 0) {
              /* found ! */
              Vdetach (iNew);
              return iRef;
            }
          }
          Vdetach (iNew);
        }
      }                           /* end for */
    }                             /* end else */
    /* not found */
    return NX_EOD;
  }
  
  /*----------------------------------------------------------------------*/  

  static int32 NXIFindSDS (NXhandle fid, CONSTCHAR *name)
  {
    pNexusFile self;
    int32 iNew, iRet, iTag, iRef;
    int32 i, iN, iA, iD1, iD2;
    NXname pNam;
    int32 iDim[MAX_VAR_DIMS];
  
    self = NXIassert (fid);
  
    /* root level search */
    if (self->iCurrentVG == 0) {
      i = SDfileinfo (self->iSID, &iN, &iA);
      if (i < 0) {
        NXIReportError (NXpData, "ERROR: failure to read file information");
        return NX_EOD;
      }
      for (i = 0; i < iN; i++) {
        iNew = SDselect (self->iSID, i);
        SDgetinfo (iNew, pNam, &iA, iDim, &iD1, &iD2);
        if (strcmp (pNam, name) == 0) {
          iRet = SDidtoref (iNew);
          SDendaccess (iNew);
          return iRet;
        } else {
          SDendaccess (iNew);
        }
      }
      /* not found */
      return NX_EOD;
    }
    /* end root level */
    else {                        /* search in a Vgroup */
      iN = Vntagrefs (self->iCurrentVG);
      for (i = 0; i < iN; i++) {
        Vgettagref (self->iCurrentVG, i, &iTag, &iRef);
        /* we are now writing using DFTAG_NDG, but need others for backward compatability */
        if ((iTag == DFTAG_SDG) || (iTag == DFTAG_NDG) || (iTag == DFTAG_SDS)) {
          iNew = SDreftoindex (self->iSID, iRef);
          iNew = SDselect (self->iSID, iNew);
          SDgetinfo (iNew, pNam, &iA, iDim, &iD1, &iD2);
          if (strcmp (pNam, name) == 0) {
            SDendaccess (iNew);
            return iRef;
          }
          SDendaccess (iNew);
        }
      }                           /* end for */
    }                             /* end Vgroup */
    /* we get here, only if nothing found */
    return NX_EOD;
  }
  
  /*----------------------------------------------------------------------*/

  static int NXIInitDir (pNexusFile self)
  {
    int i;
    int32 iTag, iRef;
    int iStackPtr;
  
  /* 
   * Note: the +1 to various malloc() operations is to avoid a
   *  malloc(0), which is an error on some operating systems 
   */
    iStackPtr = self->iStackPtr;
    if (self->iCurrentVG == 0 &&
        self->iStack[iStackPtr].iRefDir == NULL) {        /* root level */
      /* get the number and ID's of all lone Vgroups in the file */
      self->iStack[iStackPtr].iNDir = Vlone (self->iVID, NULL, 0);
      self->iStack[iStackPtr].iRefDir = 
          (int32 *) malloc (self->iStack[iStackPtr].iNDir * sizeof (int32) + 1);
      if (!self->iStack[iStackPtr].iRefDir) {
        NXIReportError (NXpData, "ERROR: out of memory in NXIInitDir");
        return NX_EOD;
      }
      Vlone (self->iVID,
             self->iStack[self->iStackPtr].iRefDir,
             self->iStack[self->iStackPtr].iNDir);
    } else {
      /* Vgroup level */
      self->iStack[iStackPtr].iNDir = Vntagrefs (self->iCurrentVG);
      self->iStack[iStackPtr].iRefDir =
        (int32 *) malloc (self->iStack[iStackPtr].iNDir * sizeof (int32) + 1);
      self->iStack[iStackPtr].iTagDir =
        (int32 *) malloc (self->iStack[iStackPtr].iNDir * sizeof (int32) + 1);
      if ((!self->iStack[iStackPtr].iRefDir) ||
          (!self->iStack[iStackPtr].iTagDir)) {
        NXIReportError (NXpData, "ERROR: out of memory in NXIInitDir");
        return NX_EOD;
      }
      for (i = 0; i < self->iStack[self->iStackPtr].iNDir; i++) {
        Vgettagref (self->iCurrentVG, i, &iTag, &iRef);
        self->iStack[iStackPtr].iRefDir[i] = iRef;
        self->iStack[iStackPtr].iTagDir[i] = iTag;
      }
    }
    self->iStack[iStackPtr].iCurDir = 0;
    return 1;
  }

  /*----------------------------------------------------------------------*/
    
  static void NXIKillDir (pNexusFile self)
  {
    if (self->iStack[self->iStackPtr].iRefDir) {
      free (self->iStack[self->iStackPtr].iRefDir);
      self->iStack[self->iStackPtr].iRefDir = NULL;
    }
    if (self->iStack[self->iStackPtr].iTagDir) {
      free (self->iStack[self->iStackPtr].iTagDir);
      self->iStack[self->iStackPtr].iTagDir = NULL;
    }
    self->iStack[self->iStackPtr].iCurDir = 0;
    self->iStack[self->iStackPtr].iNDir = 0;
  }

 
  /*-------------------------------------------------------------------------*/

  static int NXIInitAttDir (pNexusFile pFile)
  {
    int iRet;
    int32 iData, iAtt, iRank, iType;
    int32 iDim[MAX_VAR_DIMS];
    NXname pNam;
  
    pFile->iAtt.iCurDir = 0;
    if (pFile->iCurrentSDS != 0) {        /* SDS level */
      iRet = SDgetinfo (pFile->iCurrentSDS, pNam, &iRank, iDim, &iType,
                        &iAtt);
    } else {                      /* global level */
      iRet = SDfileinfo (pFile->iSID, &iData, &iAtt);
    }
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDF cannot read attribute numbers");
      pFile->iAtt.iNDir = 0;
      return NX_ERROR;
    }
    pFile->iAtt.iNDir = iAtt;
    return NX_OK;
  }

  /* --------------------------------------------------------------------- */

  static void NXIKillAttDir (pNexusFile self)
  {
    if (self->iAtt.iRefDir) {
      free (self->iAtt.iRefDir);
      self->iAtt.iRefDir = NULL;
    }
    if (self->iAtt.iTagDir) {
      free (self->iAtt.iTagDir);
      self->iAtt.iTagDir = NULL;
    }
    self->iAtt.iCurDir = 0;
    self->iAtt.iNDir = 0;
  }

 
  /* ---------------------------------------------------------------------- 
  
                          Definition of NeXus API

   ---------------------------------------------------------------------*/


   NXstatus CALLING_STYLE NX4open(CONSTCHAR *filename, NXaccess am, NXhandle* pHandle)
  {
    pNexusFile pNew = NULL;
    char pBuffer[512], time_buffer[64];
    char HDF_VERSION[64];
    uint32 lmajor, lminor, lrelease;
    int32 am1=0;
    int32 file_id=0, an_id=0, ann_id=0;
    time_t timer;
    struct tm *time_info;
    const char* time_format;
    long gmt_offset;
#ifdef USE_FTIME
    struct timeb timeb_struct;
#endif /* USE_FTIME */
  
    *pHandle = NULL;
    /* map Nexus NXaccess types to HDF4 types */
    if (am == NXACC_CREATE) {
      am1 = DFACC_CREATE;
    } else if (am == NXACC_READ) { 
      am1 = DFACC_READ;
    } else if (am == NXACC_RDWR) {
      am1 = DFACC_RDWR;
    }   
    /* get memory */
    pNew = (pNexusFile) malloc (sizeof (NexusFile));
    if (!pNew) {
      NXIReportError (NXpData, "ERROR: no memory to create File datastructure");
      return NX_ERROR;
    }
    memset (pNew, 0, sizeof (NexusFile));
/* 
 * get time in ISO 8601 format 
 */
#ifdef NEED_TZSET
    tzset();
#endif /* NEED_TZSET */
    time(&timer);
#ifdef USE_FTIME
    ftime(&timeb_struct);
    gmt_offset = -timeb_struct.timezone * 60;
    if (timeb_struct.dstflag != 0) {
        gmt_offset += 3600;
    }
#else
    time_info = gmtime(&timer);
    if (time_info != NULL) {
      gmt_offset = difftime(timer, mktime(time_info));
    } else {
      NXIReportError(NXpData, "Your gmtime() function does not work ... timezone information will be incorrect\n");
      gmt_offset = 0;
    }
#endif /* USE_FTIME */
    time_info = localtime(&timer);
    if (time_info != NULL) {
      if (gmt_offset < 0) {
        time_format = "%04d-%02d-%02d %02d:%02d:%02d-%02d%02d";
      } else {
        time_format = "%04d-%02d-%02d %02d:%02d:%02d+%02d%02d";
      }
      sprintf(time_buffer, time_format,
            1900 + time_info->tm_year,
            1 + time_info->tm_mon,
            time_info->tm_mday,
            time_info->tm_hour,
            time_info->tm_min,
            time_info->tm_sec,
            abs(gmt_offset / 3600),
            abs((gmt_offset % 3600) / 60)
      );
    } else {
      strcpy(time_buffer, "1970-01-01 00:00:00+0000");
    }
  
#if WRITE_OLD_IDENT     /* not used at moment */
/*
 * write something that can be used by OLE
 */
    
    if (am == NXACC_CREATE) {
      if ( (file_id = Hopen(filename, am1, 0)) == -1 ) {
        sprintf (pBuffer, "ERROR: cannot open file_a: %s", filename);
        NXIReportError (NXpData, pBuffer);
        free (pNew);
        return NX_ERROR;
      }
      an_id = ANstart(file_id);
      ann_id = ANcreatef(an_id, AN_FILE_LABEL); /* AN_FILE_DESC */
      ANwriteann(ann_id, "NeXus", 5);
      ANendaccess(ann_id);
      ANend(an_id);
      if (Hclose(file_id) == -1) {
        sprintf (pBuffer, "ERROR: cannot close file: %s", filename);
        NXIReportError (NXpData, pBuffer);
        free (pNew);
        return NX_ERROR;
      }
      am = NXACC_RDWR;
    }
#endif /* WRITE_OLD_IDENT */

    /* start SDS interface */
    pNew->iSID = SDstart (filename, am1);
    if (pNew->iSID <= 0) {
      sprintf (pBuffer, "ERROR: cannot open file_b: %s", filename);
      NXIReportError (NXpData, pBuffer);
      free (pNew);
      return NX_ERROR;
    }
/*
 * need to create global attributes         file_name file_time NeXus_version 
 * at some point for new files
 */
    if (am != NXACC_READ) {
      if (SDsetattr(pNew->iSID, "NeXus_version", DFNT_CHAR8, strlen(NEXUS_VERSION), NEXUS_VERSION) < 0) {
          NXIReportError (NXpData, "ERROR: HDF failed to store NeXus_version attribute ");
          return NX_ERROR;
      }
      Hgetlibversion(&lmajor, &lminor, &lrelease, HDF_VERSION); 
      if (SDsetattr(pNew->iSID, "HDF_version", DFNT_CHAR8, strlen(HDF_VERSION), HDF_VERSION) < 0) {
          NXIReportError (NXpData, "ERROR: HDF failed to store HDF_version attribute ");
          return NX_ERROR;
      }
    }
    if (am == NXACC_CREATE) {
      if (SDsetattr(pNew->iSID, "file_name", DFNT_CHAR8, strlen(filename), (char*)filename) < 0) {
        NXIReportError (NXpData, "ERROR: HDF failed to store file_name attribute ");
        return NX_ERROR;
      }
      if (SDsetattr(pNew->iSID, "file_time", DFNT_CHAR8, strlen(time_buffer), time_buffer) < 0) {
        NXIReportError (NXpData, "ERROR: HDF failed to store file_time attribute ");
        return NX_ERROR;
      }
    }

    /* 
     * Otherwise we try to create the file two times which makes HDF
     * Throw up on us.
     */
    if (am == NXACC_CREATE) {
      am = NXACC_RDWR;
      am1 = DFACC_RDWR;
    }

    /* Set Vgroup access mode */
    if (am == NXACC_READ) {
      strcpy(pNew->iAccess,"r");
    } else {
      strcpy(pNew->iAccess,"w");
    }
  
    /* start Vgroup API */
    
    pNew->iVID = Hopen (filename, am1, 100);
    if (pNew->iVID <= 0) {
      sprintf (pBuffer, "ERROR: cannot open file_c: %s", filename);
      NXIReportError (NXpData, pBuffer);
      free (pNew);
      return NX_ERROR;
    }
    Vstart (pNew->iVID);
    pNew->iNXID = NXSIGNATURE;
    pNew->iStack[0].iVref = 0;    /* root! */
  
    *pHandle = (NXhandle)pNew;
    return NX_OK;
  }

/*-----------------------------------------------------------------------*/
 
  NXstatus CALLING_STYLE NX4close (NXhandle* fid)
  {
    pNexusFile pFile = NULL;
    int iRet;
  
    pFile = NXIassert(*fid);
    iRet = 0;
    /* close links into vGroups or SDS */
    if (pFile->iCurrentVG != 0) {
      Vdetach (pFile->iCurrentVG);
    }
    if (pFile->iCurrentSDS != 0) {
      iRet = SDendaccess (pFile->iCurrentSDS);
    }
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: ending access to SDS");
    }
    /* close the SDS and Vgroup API's */
    Vend (pFile->iVID);
    iRet = SDend (pFile->iSID);
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDF cannot close SDS interface");
    }
    iRet = Hclose (pFile->iVID);
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDF cannot close HDF file");
    }
    /* release memory */
    NXIKillDir (pFile);
    free (pFile);
    *fid = NULL;
    return NX_OK;
  } 


/*-----------------------------------------------------------------------*/   

  
  NXstatus CALLING_STYLE NX4makegroup (NXhandle fid, CONSTCHAR *name, char *nxclass) 
  {
    pNexusFile pFile;
    int32 iNew, iRet;
    char pBuffer[256];
  
    pFile = NXIassert (fid);
    /* 
     * Make sure that a group with the same name and nxclass does not
     * already exist.
     */
    if ((iRet = NXIFindVgroup (pFile, (char*)name, nxclass)) >= 0) {
      sprintf (pBuffer, "ERROR: Vgroup %s, class %s already exists", 
                        name, nxclass);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
  
    /* create and configure the group */
    iNew = Vattach (pFile->iVID, -1, "w");
    if (iNew < 0) {
      NXIReportError (NXpData, "ERROR: HDF could not create Vgroup");
      return NX_ERROR;
    }
    Vsetname (iNew, name);
    Vsetclass (iNew, nxclass);
  
    /* Insert it into the hierarchy, when appropriate */
    iRet = 0;     
    if (pFile->iCurrentVG != 0) {
      iRet = Vinsert (pFile->iCurrentVG, iNew);
    }
    Vdetach (iNew);
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDF failed to insert Vgroup");
      return NX_ERROR;
    }
    return NX_OK;
  }


  /*------------------------------------------------------------------------*/

  
  NXstatus CALLING_STYLE NX4opengroup (NXhandle fid, CONSTCHAR *name, char *nxclass)
  {
    pNexusFile pFile;
    int32 iRef;
    char pBuffer[256];
  
    pFile = NXIassert (fid);
  
    iRef = NXIFindVgroup (pFile, (char*)name, nxclass);
    if (iRef < 0) {
      sprintf (pBuffer, "ERROR: Vgroup %s, class %s NOT found", name, nxclass);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
    /* are we at root level ? */
    if (pFile->iCurrentVG == 0) {
      pFile->iCurrentVG = Vattach (pFile->iVID, iRef,pFile->iAccess);
      pFile->iStackPtr++;
      pFile->iStack[pFile->iStackPtr].iVref = iRef;
    } else {
      Vdetach (pFile->iCurrentVG);
      pFile->iStackPtr++;
      pFile->iStack[pFile->iStackPtr].iVref = iRef;
      pFile->iCurrentVG = Vattach (pFile->iVID,
                                   pFile->iStack[pFile->iStackPtr].iVref,
                                   pFile->iAccess);
    }
    NXIKillDir (pFile);
    return NX_OK;
  }
  
  /* ------------------------------------------------------------------- */

  
   NXstatus CALLING_STYLE NX4closegroup (NXhandle fid)
  {
    pNexusFile pFile;
  
    pFile = NXIassert (fid);
  
    /* first catch the trivial case: we are at root and cannot get 
       deeper into a negative directory hierarchy (anti-directory)
     */
    if (pFile->iCurrentVG == 0) {
      NXIKillDir (pFile);
      return NX_OK;
    } else {                      /* Sighhh. Some work to do */
      /* close the current VG and decrement stack */
      Vdetach (pFile->iCurrentVG);
      NXIKillDir (pFile);
      pFile->iStackPtr--;
      if (pFile->iStackPtr <= 0) {        /* we hit root */
        pFile->iStackPtr = 0;
        pFile->iCurrentVG = 0;
      } else {
        /* attach to the lower Vgroup */
        pFile->iCurrentVG = Vattach (pFile->iVID,
                                     pFile->iStack[pFile->iStackPtr].iVref,
                                     pFile->iAccess);
      }
    }
    return NX_OK;
  }
  
  
  /* --------------------------------------------------------------------- */
  
  NXstatus CALLING_STYLE NX4makedata (NXhandle fid, CONSTCHAR *name, int datatype, int rank,
              int dimensions[])
  {
    pNexusFile pFile;
    int32 iNew;
    char pBuffer[256];
    int i, iRet, type;
    int32 myDim[MAX_VAR_DIMS];  

    pFile = NXIassert (fid);
      
    if (dimensions[0] == NX_UNLIMITED)
      {
        dimensions[0] = SD_UNLIMITED;
      }
     
    if ((iNew = NXIFindSDS (fid, name))>=0) {
      sprintf (pBuffer, "ERROR: SDS %s already exists at this level", name);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
  
    if (datatype == NX_CHAR)
    {
        type=DFNT_CHAR8;
    }
    else if (datatype == NX_INT8)
    {
        type=DFNT_INT8;
    }
    else if (datatype == NX_UINT8)
    {
        type=DFNT_UINT8;
    }
    else if (datatype == NX_INT16)
    {
        type=DFNT_INT16;
    }
    else if (datatype == NX_UINT16)
    {
        type=DFNT_UINT16;
    }
    else if (datatype == NX_INT32)
    {
        type=DFNT_INT32;
    }
    else if (datatype == NX_UINT32)
    {
        type=DFNT_UINT32;
    }
    else if (datatype == NX_FLOAT32)
    {
        type=DFNT_FLOAT32;
    }
    else if (datatype == NX_FLOAT64)
    {
        type=DFNT_FLOAT64;
    }
      
    if (rank <= 0) {
      sprintf (pBuffer, "ERROR: invalid rank specified for SDS %s",
               name);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }

    /*
      Check dimensions for consistency. The first dimension may be 0
      thus denoting an unlimited dimension.
    */
    for (i = 1; i < rank; i++) {
      if (dimensions[i] <= 0) {
        sprintf (pBuffer,
                 "ERROR: invalid dimension %d, value %d given for SDS %s",
                 i, dimensions[i], name);
        NXIReportError (NXpData, pBuffer);
        return NX_ERROR;
      }
    }

    /* cast the dimensions array properly for non 32-bit ints */
    for(i = 0; i < rank; i++)
    {
      myDim[i] = (int32)dimensions[i];
    }


    /* behave nicely, if there is still an SDS open */
    if (pFile->iCurrentSDS != 0) {
      SDendaccess (pFile->iCurrentSDS);
      pFile->iCurrentSDS = 0;
    }
  
    /* Do not allow creation of SDS's at the root level */
    if (pFile->iCurrentVG == 0) {
      sprintf(pBuffer, "ERROR: SDS creation at root level is not permitted");
      NXIReportError(NXpData, pBuffer);
      return NX_ERROR;
    }
          
    /* dataset creation */
    iNew = SDcreate (pFile->iSID, (char*)name, (int32)type, 
                     (int32)rank, myDim);
    if (iNew < 0) {
      sprintf (pBuffer, "ERROR: cannot create SDS %s, check arguments",
               name);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
    /* link into Vgroup, if in one */
    if (pFile->iCurrentVG != 0) {
      iRet = Vaddtagref (pFile->iCurrentVG, DFTAG_NDG, SDidtoref (iNew));
    }
    iRet = SDendaccess (iNew);
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDF cannot end access to SDS");
      return NX_ERROR;
    }
    return NX_OK;
  }
  
  
 /* --------------------------------------------------------------------- */
  
   
  NXstatus CALLING_STYLE NX4compmakedata (NXhandle fid, CONSTCHAR *name, int datatype, int rank,
              int dimensions[],int compress_type, int chunk_size[])
  {
    pNexusFile pFile;
    int32 iNew, iRet, type;
    char pBuffer[256];
    int i;
    int32 myDim[MAX_VAR_DIMS];  
    comp_info compstruct;

    pFile = NXIassert (fid);
      
    if (dimensions[0] == NX_UNLIMITED)
      {
        dimensions[0] = SD_UNLIMITED;
      }
     
    if ((iNew = NXIFindSDS (fid, name))>=0) {
      sprintf (pBuffer, "ERROR: SDS %s already exists at this level", name);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
  
    if (datatype == NX_CHAR)
    {
        type=DFNT_CHAR8;
    }
    else if (datatype == NX_INT8)
    {
        type=DFNT_INT8;
    }
    else if (datatype == NX_UINT8)
    {
        type=DFNT_UINT8;
    }
    else if (datatype == NX_INT16)
    {
        type=DFNT_INT16;
    }
    else if (datatype == NX_UINT16)
    {
        type=DFNT_UINT16;
    }
    else if (datatype == NX_INT32)
    {
        type=DFNT_INT32;
    }
    else if (datatype == NX_UINT32)
    {
        type=DFNT_UINT32;
    }
    else if (datatype == NX_FLOAT32)
    {
        type=DFNT_FLOAT32;
    }
    else if (datatype == NX_FLOAT64)
    {
        type=DFNT_FLOAT64;
    }
      
    if (rank <= 0) {
      sprintf (pBuffer, "ERROR: invalid rank specified for SDS %s",
               name);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }

    /*
      Check dimensions for consistency. The first dimension may be 0
      thus denoting an unlimited dimension.
    */
    for (i = 1; i < rank; i++) {
      if (dimensions[i] <= 0) {
        sprintf (pBuffer,
                 "ERROR: invalid dimension %d, value %d given for SDS %s",
                 i, dimensions[i], name);
        NXIReportError (NXpData, pBuffer);
        return NX_ERROR;
      }
    }

    /* cast the dimensions array properly for non 32-bit ints */
    for(i = 0; i < rank; i++)
    {
      myDim[i] = (int32)dimensions[i];
    }


    /* behave nicely, if there is still an SDS open */
    if (pFile->iCurrentSDS != 0) {
      SDendaccess (pFile->iCurrentSDS);
      pFile->iCurrentSDS = 0;
    }
  
    /* Do not allow creation of SDS's at the root level */
    if (pFile->iCurrentVG == 0) {
      sprintf(pBuffer, "ERROR: SDS creation at root level is not permitted");
      NXIReportError(NXpData, pBuffer);
      return NX_ERROR;
    }
          
    /* dataset creation */
    iNew = SDcreate (pFile->iSID, (char*)name, (int32)type, 
                     (int32)rank, myDim);
    if (iNew < 0) {
      sprintf (pBuffer, "ERROR: cannot create SDS %s, check arguments",
               name);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
     
    /* compress SD data set */

    if(compress_type == NX_COMP_LZW)
    {
      compstruct.deflate.level = 6; 
      iRet = SDsetcompress(iNew, COMP_CODE_DEFLATE, &compstruct);
      if (iRet < 0) 
      {
        NXIReportError (NXpData, "LZW-Compression failure!");
        return NX_ERROR;
      } 
    }
    else if (compress_type == NX_COMP_RLE)
    {
      iRet = SDsetcompress(iNew, COMP_CODE_RLE, &compstruct);
      if (iRet < 0) 
        {
          NXIReportError (NXpData, "RLE-Compression failure!");
          return NX_ERROR;
        }   
    }
    else if (compress_type == NX_COMP_HUF)
    {
      compstruct.skphuff.skp_size = DFKNTsize(type); 
      iRet = SDsetcompress(iNew, COMP_CODE_SKPHUFF, &compstruct);
      if (iRet < 0) 
        {
          NXIReportError (NXpData, "HUF-Compression failure!");
          return NX_ERROR;
        }  
    }
    else if (compress_type == NX_COMP_NONE)
    {
      /*      */
    }
    else 
    {
      NXIReportError (NXpData, "Unknown compression method!");
      NX_ERROR; 
    }
    /* link into Vgroup, if in one */
    if (pFile->iCurrentVG != 0) {
      iRet = Vaddtagref (pFile->iCurrentVG, DFTAG_NDG, SDidtoref (iNew));
    }
    iRet = SDendaccess (iNew);
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDF cannot end access to SDS");
      return NX_ERROR;
    }
    
    return NX_OK;
  }
 
  /* --------------------------------------------------------------------- */

   
  NXstatus CALLING_STYLE NX4compress (NXhandle fid, int compress_type)
  {
    pNexusFile pFile;
    int32 iRank, iAtt, iType, iRet;
    int32 iSize[MAX_VAR_DIMS];
    int compress_typei;
    NXname pBuffer;
    char pError[512];
    comp_info compstruct;  
   
    pFile = NXIassert (fid);
  
    /* check if there is an SDS open */
    if (pFile->iCurrentSDS == 0) {
      NXIReportError (NXpData, "ERROR: no SDS open");
      return NX_ERROR;
    }
    
    if (compress_type == NX_COMP_NONE) 
      {
        compress_typei = COMP_CODE_NONE;
      }
      else if (compress_type == NX_COMP_LZW) 
      {
        compress_typei = COMP_CODE_DEFLATE;
      }
      else if (compress_type == NX_COMP_RLE)
      {
        compress_typei = COMP_CODE_RLE;
      }
      else if  
      (compress_type == NX_COMP_HUF)
      {
        compress_typei = COMP_CODE_SKPHUFF;
      }
    
    /* first read dimension information */
    SDgetinfo (pFile->iCurrentSDS, pBuffer, &iRank, iSize, &iType, &iAtt);

    /* 
       according to compression type initialize compression
       information 
    */
    if(compress_type == NX_COMP_LZW)
    {
         compstruct.deflate.level = 6;
    }
    else if(compress_type == NX_COMP_HUF)
    {
        compstruct.skphuff.skp_size = DFKNTsize(iType);
    }

    iRet = SDsetcompress(pFile->iCurrentSDS, compress_typei, &compstruct);
    if (iRet < 0) {
      sprintf (pError, "ERROR: failure to compress data to %s", pBuffer);
      NXIReportError (NXpData, pError);
      return NX_ERROR;
    }
    return NX_OK;
  }  

  /* --------------------------------------------------------------------- */
  
 
  NXstatus CALLING_STYLE NX4opendata (NXhandle fid, CONSTCHAR *name)
  {
    pNexusFile pFile;
    int32 iNew;
    char pBuffer[256];
    int iRet;
  
    pFile = NXIassert (fid);
  
    /* First find the reference number of the SDS */
    iNew = NXIFindSDS (fid, name);
    if (iNew < 0) {
      sprintf (pBuffer, "ERROR: SDS %s not found at this level", name);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
    /* Be nice: properly close the old open SDS silently if there is
     * still an SDS open.
     */
    if (pFile->iCurrentSDS) {
      iRet = SDendaccess (pFile->iCurrentSDS);
      if (iRet < 0) {
        NXIReportError (NXpData, "ERROR: HDF cannot end access to SDS");
      }
    }
    /* clear pending attribute directories first */
    NXIKillAttDir (pFile);
  
    /* open the SDS */
    iNew = SDreftoindex (pFile->iSID, iNew);
    pFile->iCurrentSDS = SDselect (pFile->iSID, iNew);
    if (pFile->iCurrentSDS < 0) {
      NXIReportError (NXpData, "ERROR: HDF error opening SDS");
      pFile->iCurrentSDS = 0;
      return NX_ERROR;
    }
    return NX_OK;
  }
    
  /* ----------------------------------------------------------------- */
    
  
  NXstatus CALLING_STYLE NX4closedata (NXhandle fid)
  {
    pNexusFile pFile;
    int iRet;
  
    pFile = NXIassert (fid);
  
    if (pFile->iCurrentSDS != 0) {
      iRet = SDendaccess (pFile->iCurrentSDS);
      pFile->iCurrentSDS = 0;
      if (iRet < 0) {
        NXIReportError (NXpData, "ERROR: HDF cannot end access to SDS");
        return NX_ERROR;
      }
    } else {
      NXIReportError (NXpData, "ERROR: no SDS open --> nothing to do");
      return NX_ERROR;
    }
    NXIKillAttDir (pFile);                /* for attribute data */
    return NX_OK;
  }
   
  
  /* ------------------------------------------------------------------- */

  NXstatus CALLING_STYLE NX4putdata (NXhandle fid, void *data)
  {
    pNexusFile pFile;
    int32 iStart[MAX_VAR_DIMS], iSize[MAX_VAR_DIMS], iStride[MAX_VAR_DIMS];
    NXname pBuffer;
    int32 iRank, iAtt, iType, iRet, i;
    char pError[512];
      
    pFile = NXIassert (fid);
  
    /* check if there is an SDS open */
    if (pFile->iCurrentSDS == 0) {
      NXIReportError (NXpData, "ERROR: no SDS open");
      return NX_ERROR;
    }
    /* first read dimension information */
    memset (iStart, 0, MAX_VAR_DIMS * sizeof (int32));
    SDgetinfo (pFile->iCurrentSDS, pBuffer, &iRank, iSize, &iType, &iAtt);
  
    /* initialise stride to 1 */
    for (i = 0; i < iRank; i++) {
      iStride[i] = 1;
    }
  
    /* actually write */
    iRet = SDwritedata (pFile->iCurrentSDS, iStart, iStride, iSize, data);
    if (iRet < 0) {
      sprintf (pError, "ERROR: failure to write data to %s", pBuffer);
      NXIReportError (NXpData, pError);
      return NX_ERROR;
    }
    return NX_OK;
  }
    
  /* ------------------------------------------------------------------- */

  NXstatus
  CALLING_STYLE NX4putattr (NXhandle fid, CONSTCHAR *name, void *data, int datalen, int iType)
  {
    pNexusFile pFile;
    int iRet, type;
  
    pFile = NXIassert (fid);
    if (iType == NX_CHAR)
    {
        type=DFNT_CHAR8;
    }
    else if (iType == NX_INT8)
    {
        type=DFNT_INT8;
    }
    else if (iType == NX_UINT8)
    {
        type=DFNT_UINT8;
    }
    else if (iType == NX_INT16)
    {
        type=DFNT_INT16;
    }
    else if (iType == NX_UINT16)
    {
        type=DFNT_UINT16;
    }
    else if (iType == NX_INT32)
    {
        type=DFNT_INT32;
    }
    else if (iType == NX_UINT32)
    {
        type=DFNT_UINT32;
    }
    else if (iType == NX_FLOAT32)
    {
        type=DFNT_FLOAT32;
    }
    else if (iType == NX_FLOAT64)
    {
        type=DFNT_FLOAT64;
    }
    if (pFile->iCurrentSDS != 0) {
      /* SDS attribute */
      iRet = SDsetattr (pFile->iCurrentSDS, (char*)name, (int32)type,
                        (int32)datalen, data);
    } else {
      /* global attribute */
      iRet = SDsetattr (pFile->iSID, (char*)name, (int32)type,
                        (int32)datalen, data);
  
    }
    iType = type;
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDf failed to store attribute ");
      return NX_ERROR;
    }
    return NX_OK;
  }
 
   /* ------------------------------------------------------------------- */

   
  NXstatus CALLING_STYLE NX4putslab (NXhandle fid, void *data, int iStart[], int iSize[])
  {
    pNexusFile pFile;
    int iRet;
    int32 iStride[MAX_VAR_DIMS];
    int32 myStart[MAX_VAR_DIMS], mySize[MAX_VAR_DIMS];
    int32 i, iRank, iType, iAtt;
    NXname pBuffer;  
  
  
    pFile = NXIassert (fid);
  
    /* check if there is an SDS open */
    if (pFile->iCurrentSDS == 0) {
      NXIReportError (NXpData, "ERROR: no SDS open");
      return NX_ERROR;
    }
    /* initialise stride to 1 */
    for (i = 0; i < MAX_VAR_DIMS; i++) {
      iStride[i] = 1;
    }

    /* if an int is not 32-bit we have to cast them properly in order
       to kill a bug.
    */
    if(sizeof(int) != 4)
    {
         SDgetinfo (pFile->iCurrentSDS, pBuffer, 
            &iRank, myStart, &iType, &iAtt);
         for(i = 0; i < iRank; i++)
         {
           myStart[i] = (int32)iStart[i];
           mySize[i]  = (int32)iSize[i];
         }
         /* finally write */
         iRet = SDwritedata (pFile->iCurrentSDS, myStart, 
                        iStride, mySize, data);

    }
    else
    {
       /* write directly */ 
       
       iRet = SDwritedata (pFile->iCurrentSDS,(int32*)iStart, iStride, (int32*)iSize, data);
    }

    /* deal with HDF errors */
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: writing slab failed");
      return NX_ERROR;
    }
    return NX_OK;
  }

  
  /* ------------------------------------------------------------------- */

  NXstatus CALLING_STYLE NX4getdataID (NXhandle fid, NXlink* sRes)
  {
    pNexusFile pFile;
  
    pFile = NXIassert (fid);
  
    if (pFile->iCurrentSDS == 0) {
      sRes->iTag = NX_ERROR;
      return NX_ERROR;
    } else {
      sRes->iTag = DFTAG_NDG;
      sRes->iRef = SDidtoref (pFile->iCurrentSDS);
      return NX_OK;
    }
    sRes->iTag = NX_ERROR;
    return NX_ERROR;                  /* not reached */
  }


  /* ------------------------------------------------------------------- */

  
  NXstatus CALLING_STYLE NX4makelink (NXhandle fid, NXlink* sLink)
  {
    pNexusFile pFile;
    int32 iVG, iRet;
  
    pFile = NXIassert (fid);
  
    if (pFile->iCurrentVG == 0) { /* root level, can not link here */
      return NX_ERROR;
    }
    Vaddtagref (pFile->iCurrentVG, sLink->iTag, sLink->iRef);
    return NX_OK;
  }
  
  
  /*----------------------------------------------------------------------*/

  NXstatus CALLING_STYLE NX4flush(NXhandle *pHandle)
  {
    char *pFileName, *pCopy = NULL;
    int access, dummy, iRet, i, iStack;
    pNexusFile pFile = NULL;
    NXaccess ac;
    int *iRefs = NULL;

    pFile = NXIassert(*pHandle);
    
    /*
      The HDF4-API does not support a flush. We help ourselves with
      inquiring the name and access type of the file, closing it and
      opening it again. This is also the reason why this needs a pointer
      to the handle structure as the handle changes. The other thing we
      do is to store the refs of all open vGroups in a temporary array
      in order to recover the position in the vGroup hierarchy before the
      flush.
    */
    iRet = Hfidinquire(pFile->iVID,&pFileName,&access,&dummy);
    if (iRet < 0) {
      NXIReportError (NXpData, 
        "ERROR: Failed to inquire file name for HDF file");
      return NX_ERROR;
    }
    if(pFile->iAccess[0] == 'r') {
      ac = NXACC_READ;
    }else if(pFile->iAccess[0] == 'w') {
      ac = NXACC_RDWR;
    }
    pCopy = (char *)malloc((strlen(pFileName)+10)*sizeof(char));
    if(!pCopy) {
      NXIReportError (NXpData, 
        "ERROR: Failed to allocate data for filename copy");
      return NX_ERROR;
    }
    memset(pCopy,0,strlen(pFileName)+10);
    strcpy(pCopy,pFileName);      
    
    /* get refs for recovering vGroup position */
    iStack = 0;
    if(pFile->iStackPtr > 0) {
      iStack = pFile->iStackPtr + 1;
      iRefs = (int *)malloc(iStack*sizeof(int));
      if(!iRefs){
        NXIReportError (NXpData, 
        "ERROR: Failed to allocate data for hierarchy copy");
        return NX_ERROR;
      }
      for(i = 0; i < iStack; i++){
         iRefs[i] = pFile->iStack[i].iVref;
      }
    }

    iRet = NX4close(pHandle);
    if(iRet != NX_OK) {
      return iRet;
    }

    iRet = NX4open(pCopy, ac, pHandle);
    free(pCopy);
   
    /* return to position in vGroup hierarchy */
    pFile = NXIassert(*pHandle);
    if(iStack > 0){
      pFile->iStackPtr = iStack - 1;
      for(i = 0; i < iStack; i++){
        pFile->iStack[i].iVref = iRefs[i];
      }
      free(iRefs);
      pFile->iCurrentVG = Vattach(pFile->iVID,
                                  pFile->iStack[pFile->iStackPtr].iVref,
                                  pFile->iAccess);
    }
    
    return iRet;
  }  

  /*-------------------------------------------------------------------------*/
  

  NXstatus CALLING_STYLE NX4getnextentry (NXhandle fid, NXname name, NXname nxclass, int *datatype)
  {
    pNexusFile pFile;
    int iRet, iStackPtr, iCurDir;
    int32 iTemp, iD1, iD2, iA;
    int32 iDim[MAX_VAR_DIMS];
  
    pFile = NXIassert (fid);
  
    iStackPtr = pFile->iStackPtr;
    iCurDir   = pFile->iStack[pFile->iStackPtr].iCurDir;
  
    /* first case to check for: no directory entry */
    if (pFile->iStack[pFile->iStackPtr].iRefDir == NULL) {
      iRet = NXIInitDir (pFile);
      if (iRet < 0) {
        NXIReportError (NXpData,
                        "ERROR: no memory to store directory info");
        return NX_EOD;
      }
    }
    /* Next case: end of directory */
    if (iCurDir >= pFile->iStack[pFile->iStackPtr].iNDir) {
      NXIKillDir (pFile);
      return NX_EOD;
    }
    /* Next case: we have data! supply it and increment counter */
    if (pFile->iCurrentVG == 0) { /* root level */
      iTemp = Vattach (pFile->iVID,
                       pFile->iStack[iStackPtr].iRefDir[iCurDir], "r");
      if (iTemp < 0) {
        NXIReportError (NXpData, "ERROR: HDF cannot attach to Vgroup");
        return NX_ERROR;
      }
      Vgetname (iTemp, name);
      Vgetclass (iTemp, nxclass);
      *datatype = DFTAG_VG;
      pFile->iStack[pFile->iStackPtr].iCurDir++;
      Vdetach (iTemp);
      return NX_OK;
    } else {                      /* in Vgroup */
      if (pFile->iStack[iStackPtr].iTagDir[iCurDir] == DFTAG_VG) {/* Vgroup */
        iTemp = Vattach (pFile->iVID,
                         pFile->iStack[iStackPtr].iRefDir[iCurDir], "r");
        if (iTemp < 0) {
          NXIReportError (NXpData, "ERROR: HDF cannot attach to Vgroup");
          return NX_ERROR;
        }
        Vgetname (iTemp, name);
        Vgetclass (iTemp, nxclass);
        *datatype = DFTAG_VG;
        pFile->iStack[pFile->iStackPtr].iCurDir++;
        Vdetach (iTemp);
        return NX_OK;
        /* we are now writing using DFTAG_NDG, but need others for backward compatability */
      } else if ((pFile->iStack[iStackPtr].iTagDir[iCurDir] == DFTAG_SDG) ||
                 (pFile->iStack[iStackPtr].iTagDir[iCurDir] == DFTAG_NDG) ||
                 (pFile->iStack[iStackPtr].iTagDir[iCurDir] == DFTAG_SDS)) {
        iTemp = SDreftoindex (pFile->iSID,
                              pFile->iStack[iStackPtr].iRefDir[iCurDir]);
        iTemp = SDselect (pFile->iSID, iTemp);
        SDgetinfo (iTemp, name, &iA, iDim, &iD1, &iD2);
        strcpy (nxclass, "SDS");
        *datatype = iD1;
        SDendaccess (iTemp);
        pFile->iStack[pFile->iStackPtr].iCurDir++;
        return NX_OK;
      } else {                    /* unidentified */
        strcpy (name, "UNKNOWN");
        strcpy (nxclass, "UNKNOWN");
        *datatype = pFile->iStack[iStackPtr].iTagDir[iCurDir];
        pFile->iStack[pFile->iStackPtr].iCurDir++;
        return NX_OK;
      }
    }
    return NX_ERROR;              /* not reached */
  }


  /*-------------------------------------------------------------------------*/

   
  NXstatus CALLING_STYLE NX4getdata (NXhandle fid, void *data)
  {
    pNexusFile pFile;
    int32 iStart[MAX_VAR_DIMS], iSize[MAX_VAR_DIMS];
    NXname pBuffer;
    int32 iRank, iAtt, iType;
  
    pFile = NXIassert (fid);
  
    /* check if there is an SDS open */
    if (pFile->iCurrentSDS == 0) {
      NXIReportError (NXpData, "ERROR: no SDS open");
      return NX_ERROR;
    }
    /* first read dimension information */
    memset (iStart, 0, MAX_VAR_DIMS * sizeof (int32));
    SDgetinfo (pFile->iCurrentSDS, pBuffer, &iRank, iSize, &iType, &iAtt);
    /* actually read */
    SDreaddata (pFile->iCurrentSDS, iStart, NULL, iSize, data);
    return NX_OK;
  } 
  
  /*-------------------------------------------------------------------------*/

  NXstatus
  CALLING_STYLE NX4getinfo (NXhandle fid, int *rank, int dimension[], int *iType)
  {
    pNexusFile pFile;
    NXname pBuffer;
    int32 iAtt, myDim[MAX_VAR_DIMS], i, iRank, mType;
  
    pFile = NXIassert (fid);
  
    /* check if there is an SDS open */
    if (pFile->iCurrentSDS == 0) {
      NXIReportError (NXpData, "ERROR: no SDS open");
      return NX_ERROR;
    }
    /* read information */
    SDgetinfo (pFile->iCurrentSDS, pBuffer, &iRank, myDim, 
               &mType, &iAtt);

    /* conversion to proper ints for the platform */ 
    *iType = (int)mType;
    *rank = (int)iRank;
    for(i = 0; i < iRank; i++)
    {
       dimension[i] = (int)myDim[i];
    }
    return NX_OK;
  }
  
   
  /*-------------------------------------------------------------------------*/

  
  NXstatus CALLING_STYLE NX4getslab (NXhandle fid, void *data, int iStart[], int iSize[])
  {
    pNexusFile pFile;
    int32 myStart[MAX_VAR_DIMS], mySize[MAX_VAR_DIMS];
    int32 i, iRank, iType, iAtt;
    NXname pBuffer;  

    pFile = NXIassert (fid);
  
    /* check if there is an SDS open */
    if (pFile->iCurrentSDS == 0) {
      NXIReportError (NXpData, "ERROR: no SDS open");
      return NX_ERROR;
    }

    /* if an int is not 32-bit we have to cast them properly in order
       to kill a bug.
    */
    if(sizeof(int) != 4)
    {
         SDgetinfo (pFile->iCurrentSDS, pBuffer, 
            &iRank, myStart, &iType, &iAtt);
         for(i = 0; i < iRank; i++)
         {
           myStart[i] = (int32)iStart[i];
           mySize[i]  = (int32)iSize[i];
         }
        /* finally read  */
        SDreaddata (pFile->iCurrentSDS, myStart, NULL, 
                   mySize, data);
        return NX_OK;
    }
    else
    {
        /* read directly  */
        SDreaddata (pFile->iCurrentSDS, (int32*)iStart, NULL, 
                   (int32*)iSize, data);
        return NX_OK;
    }
  }
  
  /*-------------------------------------------------------------------------*/

  NXstatus CALLING_STYLE NX4getnextattr (NXhandle fileid, NXname pName,
           int *iLength, int *iType)
  {
    pNexusFile pFile;
    int iRet;
    int32 iPType, iCount;
  
    pFile = NXIassert (fileid);
  
    /* first check if we have to start a new attribute search */
    if (pFile->iAtt.iNDir == 0) {
      iRet = NXIInitAttDir (pFile);
      if (iRet == NX_ERROR) {
        return NX_ERROR;
      }
    }
    /* are we done ? */
    if (pFile->iAtt.iCurDir >= pFile->iAtt.iNDir) {
      NXIKillAttDir (pFile);
      return NX_EOD;
    }
    /* well, there must be data to copy */
    if (pFile->iCurrentSDS == 0) {        /* global attribute */
      iRet = SDattrinfo (pFile->iSID, pFile->iAtt.iCurDir,
                         pName, &iPType, &iCount);
    } else {
      iRet = SDattrinfo (pFile->iCurrentSDS, pFile->iAtt.iCurDir,
                         pName, &iPType, &iCount);
    }
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDF cannot read attribute info");
      return NX_ERROR;
    }
    *iLength = iCount;
    *iType = iPType;
    pFile->iAtt.iCurDir++;
    return NX_OK;
  }
   
  
  /*-------------------------------------------------------------------------*/


  NXstatus CALLING_STYLE NX4getattr (NXhandle fid, char *name, void *data, int* datalen, int* iType)
  {
    pNexusFile pFile;
    int32 iNew, iType32;
    void *pData = NULL;
    int32 iLen, iRet;
    int type;
    char pBuffer[256];
    NXname pNam;
  
    type = *iType;
    if (type == NX_CHAR)
    {
        type=DFNT_CHAR8;
    }
    else if (type == NX_INT8)
    {
        type=DFNT_INT8;
    }
    else if (type == NX_UINT8)
    {
        type=DFNT_UINT8;
    }
    else if (type == NX_INT16)
    {
        type=DFNT_INT16;
    }
    else if (type == NX_UINT16)
    {
        type=DFNT_UINT16;
    }
    else if (type == NX_INT32)
    {
        type=DFNT_INT32;
    }
    else if (type == NX_UINT32)
    {
        type=DFNT_UINT32;
    }
    else if (type == NX_FLOAT32)
    {
        type=DFNT_FLOAT32;
    }
    else if (type == NX_FLOAT64)
    {
        type=DFNT_FLOAT64;
    }
    *datalen = (*datalen) * DFKNTsize(type);
    pFile = NXIassert (fid);
  
    /* find attribute */
    if (pFile->iCurrentSDS != 0) {
      /* SDS attribute */
      iNew = SDfindattr (pFile->iCurrentSDS, name);
    } else {
      /* global attribute */
      iNew = SDfindattr (pFile->iSID, name);
    }
    if (iNew < 0) {
      sprintf (pBuffer, "ERROR: attribute %s not found", name);
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
    /* get more info, allocate temporary data space */
    iType32 = (int32)type;
    if (pFile->iCurrentSDS != 0) {
      iRet = SDattrinfo (pFile->iCurrentSDS, iNew, pNam, &iType32, &iLen);
    } else {
      iRet = SDattrinfo (pFile->iSID, iNew, pNam, &iType32, &iLen);
    }
    if (iRet < 0) {
      sprintf (pBuffer, "ERROR: HDF could not read attribute info");
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
    *iType = (int)iType32;
    iLen = iLen * DFKNTsize (*iType);
    pData = (void *) malloc (iLen);
    if (!pData) {
      NXIReportError (NXpData, "ERROR: allocating memory in NXgetattr");
      return NX_ERROR;
    }
    memset (pData, 0, iLen);
  
    /* finally read the data */
    if (pFile->iCurrentSDS != 0) {
      iRet = SDreadattr (pFile->iCurrentSDS, iNew, pData);
    } else {
      iRet = SDreadattr (pFile->iSID, iNew, pData);
    }
    if (iRet < 0) {
      sprintf (pBuffer, "ERROR: HDF could not read attribute data");
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
    /* copy data to caller */
    memset (data, 0, *datalen);
    if ((*datalen <= iLen) && (*iType == DFNT_UINT8 || *iType == DFNT_CHAR8 || *iType == DFNT_UCHAR8)) {
      iLen = *datalen - 1;
    }
    memcpy (data, pData, iLen);
    *datalen = iLen / DFKNTsize(*iType);
    free (pData);
    return NX_OK;
  }
 
  /*-------------------------------------------------------------------------*/

    
  NXstatus CALLING_STYLE NX4getattrinfo (NXhandle fid, int *iN)
  {
    pNexusFile pFile;
    int iRet;
    int32 iData, iAtt, iRank, iType;
    int32 iDim[MAX_VAR_DIMS];
    NXname pNam;
  
    pFile = NXIassert (fid);
    if (pFile->iCurrentSDS != 0) {        /* SDS level */
      iRet = SDgetinfo (pFile->iCurrentSDS, pNam, &iRank, iDim, &iType,
                        &iAtt);
    } else {                      /* global level */
      iRet = SDfileinfo (pFile->iSID, &iData, &iAtt);
    }
    if (iRet < 0) {
      NXIReportError (NXpData, "NX_ERROR: HDF cannot read attribute numbers");
      *iN = 0;
      return NX_ERROR;
    }
    *iN = iAtt;
    return iRet;
  }
  
  /*-------------------------------------------------------------------------*/

  NXstatus CALLING_STYLE NX4getgroupID (NXhandle fileid, NXlink* sRes)
  {
    pNexusFile pFile;
  
    pFile = NXIassert (fileid);
  
    if (pFile->iCurrentVG == 0) {
      sRes->iTag = NX_ERROR;
      return NX_ERROR;
    } else {
      sRes->iTag = DFTAG_VG;
      sRes->iRef = VQueryref(pFile->iCurrentVG);
      return NX_OK;
    }
    /* not reached */
    sRes->iTag = NX_ERROR;
    return NX_ERROR;
  }

 /*-------------------------------------------------------------------------*/

  NXstatus
  CALLING_STYLE NX4getgroupinfo (NXhandle fid, int *iN, NXname pName, NXname pClass)
  {
    pNexusFile pFile;
  
    pFile = NXIassert (fid);
    /* check if there is a group open */
    if (pFile->iCurrentVG == 0) {
      *iN = Vlone (pFile->iVID, NULL, 0);
      strcpy (pName, "root");
      strcpy (pClass, "NXroot");
    }
    else {
      *iN = Vntagrefs (pFile->iCurrentVG);
      Vgetname (pFile->iCurrentVG, pName);
      Vgetclass (pFile->iCurrentVG, pClass);
    }
    return NX_OK;
  }
  
  /*-------------------------------------------------------------------------*/

  
  NXstatus CALLING_STYLE NX4initattrdir (NXhandle fid)
  {
    pNexusFile pFile;
    int iRet;
    
    pFile = NXIassert (fid);
    NXIKillAttDir (fid);
    iRet = NXIInitAttDir (pFile);
    if (iRet == NX_ERROR)
      return NX_ERROR;
    return NX_OK;
  }
  
 
  /*-------------------------------------------------------------------------*/
 
  
  NXstatus CALLING_STYLE NX4initgroupdir (NXhandle fid)
  {
    pNexusFile pFile;
    int iRet;
    
    pFile = NXIassert (fid);
    NXIKillDir (fid);
    iRet = NXIInitDir (pFile);
    if (iRet < 0) {
      NXIReportError (NXpData,"NX_ERROR: no memory to store directory info");
      return NX_EOD;
    }
    return NX_OK;
  }
 
