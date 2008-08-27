/*---------------------------------------------------------------------------
  NeXus - Neutron & X-ray Common Data Format
  
  Application Program Interface (HDF4) Routines
  
  Copyright (C) 1997-2006 Mark Koennecke, Przemek Klosowski
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
 
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
             
  For further information, see <http://www.neutron.anl.gov/NeXus/>
  
  $Id$

----------------------------------------------------------------------------*/
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "napi.h"
#include "napi4.h"

extern	void *NXpData;

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
   /*-------------------------------------------------------------------*/
   static void ignoreError(void *data, char *text){
   }
   /*--------------------------------------------------------------------*/

  static pNexusFile NXIassert(NXhandle fid)
  {
    pNexusFile pRes;
  
    assert(fid != NULL);
    pRes = (pNexusFile)fid;
    assert(pRes->iNXID == NXSIGNATURE);
    return pRes;
  }
  /*----------------------------------------------------------------------*/
static int findNapiClass(pNexusFile pFile, int groupRef, NXname nxclass)
{
  NXname classText, linkClass;
  int32 tags[2], attID, linkID, groupID;

  groupID = Vattach(pFile->iVID,groupRef,"r");
  Vgetclass(groupID, classText);
  if(strcmp(classText,"NAPIlink") != 0)
  {
    /* normal group */
    strcpy(nxclass,classText);
    Vdetach(groupID);
    return groupRef;
  }
  else 
  {
    /* code for linked renamed groups */
    attID = Vfindattr(groupID,"NAPIlink");
    if(attID >= 0)
    {
      Vgetattr(groupID,attID, tags);
      linkID = Vattach(pFile->iVID,tags[1],"r");
      Vgetclass(linkID, linkClass);
      Vdetach(groupID);
      Vdetach(linkID);
      strcpy(nxclass,linkClass);
      return tags[1];
    } 
    else
    {
      /* this allows for finding the NAPIlink group in NXmakenamedlink */
      strcpy(nxclass,classText);
      Vdetach(groupID);
      return groupRef;
    }
  } 
}
  /* --------------------------------------------------------------------- */

  static int32 NXIFindVgroup (pNexusFile pFile, CONSTCHAR *name, CONSTCHAR *nxclass)
  {
    int32 iNew, iRef, iTag;
    int iN, i, status;
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
        Vdetach(iNew);
        if (strcmp (pText, name) == 0) {
          pArray[i] = findNapiClass(pFile,pArray[i],pText);
          if (strcmp (pText, nxclass) == 0) {
            /* found ! */
            iNew = pArray[i];
            free (pArray);
            return iNew;
          }
        }
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
          Vdetach(iNew);
          if (strcmp (pText, name) == 0) {
	    iRef = findNapiClass(pFile,iRef, pText);
            if (strcmp (pText, nxclass) == 0) {
              return iRef;
            }
          }
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
    int32 iDim[H4_MAX_VAR_DIMS];
  
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
    int32 iDim[H4_MAX_VAR_DIMS];
    NXname pNam;
  
    pFile->iAtt.iCurDir = 0;
    if (pFile->iCurrentSDS != 0) {        /* SDS level */
      iRet = SDgetinfo (pFile->iCurrentSDS, pNam, &iRank, iDim, &iType,
                        &iAtt);
    } else {
      if(pFile->iCurrentVG == 0){
	/* global level */
	iRet = SDfileinfo (pFile->iSID, &iData, &iAtt);
      } else {
	/* group attribute */
	iRet = Vnattrs(pFile->iCurrentVG);
	iAtt = iRet;
      }
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
/*------------------------------------------------------------------*/
  static void NXIbuildPath(pNexusFile pFile, char *buffer, int bufLen)
  {
    int i;
    int32 groupID, iA, iD1, iD2, iDim[H4_MAX_VAR_DIMS];
    NXname pText;

    buffer[0] = '\0';
    for(i = 1; i <= pFile->iStackPtr; i++){
      strncat(buffer,"/",bufLen-strlen(buffer));
      groupID = Vattach(pFile->iVID,pFile->iStack[i].iVref, "r");
      if (groupID != -1)
      {
          if (Vgetname(groupID, pText) != -1) {
              strncat(buffer,pText,bufLen-strlen(buffer));
          } else {
              NXIReportError (NXpData, "ERROR: NXIbuildPath cannot get vgroup name");
          }
          Vdetach(groupID);
      }
      else
      {
          NXIReportError (NXpData, "ERROR: NXIbuildPath cannot attach to vgroup");
      }
    }
    if(pFile->iCurrentSDS != 0){
      if (SDgetinfo(pFile->iCurrentSDS,pText,&iA,iDim,&iD1,&iD2) != -1) {
          strncat(buffer,"/",bufLen-strlen(buffer));
          strncat(buffer,pText,bufLen-strlen(buffer));
      }
      else
      {
          NXIReportError (NXpData, "ERROR: NXIbuildPath cannot read SDS");
      }
    }
  } 
  /* ---------------------------------------------------------------------- 
  
                          Definition of NeXus API

   ---------------------------------------------------------------------*/


   NXstatus  NX4open(CONSTCHAR *filename, NXaccess am, 
				  NXhandle* pHandle)
  {
    pNexusFile pNew = NULL;
    char pBuffer[512];
    char *time_puffer = NULL;
    char HDF_VERSION[64];
    uint32 lmajor, lminor, lrelease;
    int32 am1=0;
    int32 file_id=0, an_id=0, ann_id=0;
  
    *pHandle = NULL;

    /* mask off any options for now */
    am = (NXaccess)(am & NXACCMASK_REMOVEFLAGS);
    /* map Nexus NXaccess types to HDF4 types */
    if (am == NXACC_CREATE) {
      am1 = DFACC_CREATE;
    } else if (am == NXACC_CREATE4) {
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

#if WRITE_OLD_IDENT     /* not used at moment */
/*
 * write something that can be used by OLE
 */
    
    if (am == NXACC_CREATE || am == NXACC_CREATE4) {
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

    time_puffer = NXIformatNeXusTime();
    if (am == NXACC_CREATE || am == NXACC_CREATE4) {
      if (SDsetattr(pNew->iSID, "file_name", DFNT_CHAR8, strlen(filename), (char*)filename) < 0) {
        NXIReportError (NXpData, "ERROR: HDF failed to store file_name attribute ");
        return NX_ERROR;
      }
      if(time_puffer != NULL){
	if (SDsetattr(pNew->iSID, "file_time", DFNT_CHAR8, 
		      strlen(time_puffer), time_puffer) < 0) {
	  NXIReportError (NXpData, 
			  "ERROR: HDF failed to store file_time attribute ");
	  free(time_puffer);
	  return NX_ERROR;
	}
      }
    }
    if (time_puffer != NULL) {
	free(time_puffer);
    }

    /* 
     * Otherwise we try to create the file two times which makes HDF
     * Throw up on us.
     */
    if (am == NXACC_CREATE || am == NXACC_CREATE4) {
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
    
    pNew->iVID = Hopen(filename, am1, 100);
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
 
  NXstatus  NX4close (NXhandle* fid)
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

  
  NXstatus  NX4makegroup (NXhandle fid, CONSTCHAR *name, CONSTCHAR *nxclass) 
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
  NXstatus  NX4opengroup (NXhandle fid, CONSTCHAR *name, CONSTCHAR *nxclass)
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

  
   NXstatus  NX4closegroup (NXhandle fid)
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
  
  NXstatus  NX4makedata (NXhandle fid, CONSTCHAR *name, int datatype, int rank,
              int dimensions[])
  {
    pNexusFile pFile;
    int32 iNew;
    char pBuffer[256];
    int i, iRet, type;
    int32 myDim[H4_MAX_VAR_DIMS];  

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
    else
    {
      NXIReportError (NXpData, "ERROR: invalid type in NX4makedata");
      return NX_ERROR;
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
  
   
  NXstatus  NX4compmakedata (NXhandle fid, CONSTCHAR *name, int datatype, int rank,
              int dimensions[],int compress_type, int chunk_size[])
  {
    pNexusFile pFile;
    int32 iNew, iRet, type;
    char pBuffer[256];
    int i, compress_level;
    int32 myDim[H4_MAX_VAR_DIMS];  
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
    else
    {
      NXIReportError (NXpData, "ERROR: invalid datatype in NX4compmakedata");
      return NX_ERROR;
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
    compress_level = 6;
    if( (compress_type / 100) == NX_COMP_LZW )
    {
	compress_level = compress_type % 100;
	compress_type = NX_COMP_LZW;
    }

    if(compress_type == NX_COMP_LZW)
    {
      compstruct.deflate.level = compress_level; 
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

   
  NXstatus  NX4compress (NXhandle fid, int compress_type)
  {
    pNexusFile pFile;
    int32 iRank, iAtt, iType, iRet;
    int32 iSize[H4_MAX_VAR_DIMS];
    comp_coder_t compress_typei = COMP_CODE_NONE;
    NXname pBuffer;
    char pError[512];
    comp_info compstruct;  
    int compress_level = 6;
   
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
      else if ( (compress_type / 100) == NX_COMP_LZW )
      {
        compress_typei = COMP_CODE_DEFLATE;
        compress_level = compress_type % 100;
	compress_type = NX_COMP_LZW;
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
         compstruct.deflate.level = compress_level;
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
  
 
  NXstatus  NX4opendata (NXhandle fid, CONSTCHAR *name)
  {
    pNexusFile pFile;
    int32 iNew, attID, tags[2];
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

    /* open the SDS, thereby watching for linked SDS under a different name */
    iNew = SDreftoindex (pFile->iSID, iNew);
    pFile->iCurrentSDS = SDselect (pFile->iSID, iNew);
    attID = SDfindattr(pFile->iCurrentSDS,"NAPIlink");
    if(attID >= 0)
    {
      SDreadattr(pFile->iCurrentSDS,attID, tags);
      SDendaccess(pFile->iCurrentSDS);
      iNew = SDreftoindex (pFile->iSID, tags[1]);
      pFile->iCurrentSDS = SDselect (pFile->iSID, iNew);
    }

    if (pFile->iCurrentSDS < 0) {
      NXIReportError (NXpData, "ERROR: HDF error opening SDS");
      pFile->iCurrentSDS = 0;
      return NX_ERROR;
    }
    return NX_OK;
  }
    
  /* ----------------------------------------------------------------- */
    
  
  NXstatus  NX4closedata (NXhandle fid)
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

  NXstatus  NX4putdata (NXhandle fid, void *data)
  {
    pNexusFile pFile;
    int32 iStart[H4_MAX_VAR_DIMS], iSize[H4_MAX_VAR_DIMS], iStride[H4_MAX_VAR_DIMS];
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
    memset (iStart, 0, H4_MAX_VAR_DIMS * sizeof (int32));
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
   NX4putattr (NXhandle fid, CONSTCHAR *name, void *data, int datalen, int iType)
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
    else
    {
      NXIReportError (NXpData, "ERROR: Invalid data type for HDF attribute");
      return NX_ERROR;
    }
    if (pFile->iCurrentSDS != 0) {
      /* SDS attribute */
      iRet = SDsetattr (pFile->iCurrentSDS, (char*)name, (int32)type,
                        (int32)datalen, data);
    } else {
      if(pFile->iCurrentVG == 0){
	/* global attribute */
	iRet = SDsetattr (pFile->iSID, (char*)name, (int32)type,
			  (int32)datalen, data);
      } else {
	/* group attribute */
	iRet = Vsetattr(pFile->iCurrentVG, (char *)name, (int32) type,
			(int32)datalen,data);
      }
    }
    iType = type;
    if (iRet < 0) {
      NXIReportError (NXpData, "ERROR: HDF failed to store attribute ");
      return NX_ERROR;
    }
    return NX_OK;
  }
 
   /* ------------------------------------------------------------------- */

   
  NXstatus  NX4putslab (NXhandle fid, void *data, int iStart[], int iSize[])
  {
    pNexusFile pFile;
    int iRet;
    int32 iStride[H4_MAX_VAR_DIMS];
    int32 myStart[H4_MAX_VAR_DIMS], mySize[H4_MAX_VAR_DIMS];
    int32 i, iRank, iType, iAtt;
    NXname pBuffer;  
  
  
    pFile = NXIassert (fid);
  
    /* check if there is an SDS open */
    if (pFile->iCurrentSDS == 0) {
      NXIReportError (NXpData, "ERROR: no SDS open");
      return NX_ERROR;
    }
    /* initialise stride to 1 */
    for (i = 0; i < H4_MAX_VAR_DIMS; i++) {
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

  NXstatus  NX4getdataID (NXhandle fid, NXlink* sRes)
  {
    pNexusFile pFile;
    ErrFunc oldErr;
    int datalen, type = NX_CHAR;

    pFile = NXIassert (fid);
  
    if (pFile->iCurrentSDS == 0) {
      sRes->iTag = NX_ERROR;
      return NX_ERROR;
    } else {
      sRes->iTag = DFTAG_NDG;
      sRes->iRef = SDidtoref (pFile->iCurrentSDS);
      oldErr = NXMGetError();
      NXMSetError(NXpData, ignoreError);
      datalen = 1024;
      memset(&sRes->targetPath,0,1024);
      if(NX4getattr(fid,"target",&sRes->targetPath,&datalen,&type) != NX_OK)
      {
	NXIbuildPath(pFile,sRes->targetPath,1024);
      }
      NXMSetError(NXpData,oldErr);
      return NX_OK;
    }
    sRes->iTag = NX_ERROR;
    return NX_ERROR;                  /* not reached */
  }


  /* ------------------------------------------------------------------- */

  
  NXstatus  NX4makelink (NXhandle fid, NXlink* sLink)
  {
    pNexusFile pFile;
    int32 iVG, iRet, dataID, type = DFNT_CHAR8, length;
    char name[] = "target";
  
    pFile = NXIassert (fid);
  
    if (pFile->iCurrentVG == 0) { /* root level, can not link here */
      return NX_ERROR;
    }
    Vaddtagref(pFile->iCurrentVG, sLink->iTag, sLink->iRef);
    length = strlen(sLink->targetPath);
    if(sLink->iTag == DFTAG_SDG || sLink->iTag == DFTAG_NDG ||
       sLink->iTag == DFTAG_SDS)
    {
      dataID = SDreftoindex(pFile->iSID,sLink->iRef);
      dataID = SDselect(pFile->iSID,dataID);
      SDsetattr(dataID,name,type,length,sLink->targetPath);
      SDendaccess(dataID);
    }
    else 
    {
      dataID = Vattach(pFile->iVID,sLink->iRef,"w");
      Vsetattr(dataID, (char *)name, type, (int32) length, sLink->targetPath);
      Vdetach(dataID);
    }
    return NX_OK;
  }
  /* ------------------------------------------------------------------- */

  
  NXstatus  NX4makenamedlink (NXhandle fid, CONSTCHAR* newname, NXlink* sLink)
  {
    pNexusFile pFile;
    int32 iVG, iRet, dataID, type = DFNT_CHAR8, length, dataType = NX_CHAR, 
      rank = 1, attType = NX_INT32;
    int iDim[1];
    char name[] = "target";
    int tags[2];  

    pFile = NXIassert (fid);
  
    if (pFile->iCurrentVG == 0) { /* root level, can not link here */
      return NX_ERROR;
    }

    tags[0] = sLink->iTag;
    tags[1] = sLink->iRef;

    length = strlen(sLink->targetPath); 
    if(sLink->iTag == DFTAG_SDG || sLink->iTag == DFTAG_NDG ||
       sLink->iTag == DFTAG_SDS)
    {
      iDim[0] = 1;
      NX4makedata(fid,newname, dataType,rank,iDim);
      NX4opendata(fid,newname);
      NX4putattr(fid,"NAPIlink",tags, 2, attType);
      NX4closedata(fid); 
      dataID = SDreftoindex(pFile->iSID,sLink->iRef);
      dataID = SDselect(pFile->iSID,dataID);
      SDsetattr(dataID,name,type,length,sLink->targetPath);
      SDendaccess(dataID);
    } else {
      NX4makegroup(fid,newname,"NAPIlink");
      NX4opengroup(fid,newname,"NAPIlink");
      NX4putattr(fid,"NAPIlink",tags, 2, attType);
      NX4closegroup(fid);
      dataID = Vattach(pFile->iVID,sLink->iRef,"w");
      Vsetattr(dataID, (char *)name, type, (int32) length, sLink->targetPath);
      Vdetach(dataID);
    }
    return NX_OK;
  }

  /*----------------------------------------------------------------------*/
  
  NXstatus  NX4printlink (NXhandle fid, NXlink* sLink)
  {
    pNexusFile pFile;
    pFile = NXIassert (fid);
     printf("HDF4 link: iTag = %ld, iRef = %ld, target=\"%s\"\n", sLink->iTag, sLink->iRef, sLink->targetPath);
    return NX_OK;
  }
  
  /*----------------------------------------------------------------------*/

  NXstatus  NX4flush(NXhandle *pHandle)
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
    } else {
      NXIReportError (NXpData, 
        "ERROR: NX4flush failed to determine file access mode");
      return NX_ERROR;
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
  

  NXstatus  NX4getnextentry (NXhandle fid, NXname name, NXname nxclass, int *datatype)
  {
    pNexusFile pFile;
    int iRet, iStackPtr, iCurDir;
    int32 iTemp, iD1, iD2, iA;
    int32 iDim[H4_MAX_VAR_DIMS];
  
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
      Vdetach (iTemp);
      findNapiClass(pFile, pFile->iStack[pFile->iStackPtr].iRefDir[iCurDir], nxclass);
      *datatype = DFTAG_VG;
      pFile->iStack[pFile->iStackPtr].iCurDir++;
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
        Vdetach(iTemp);
	findNapiClass(pFile, pFile->iStack[pFile->iStackPtr].iRefDir[iCurDir], nxclass);
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

   
  NXstatus  NX4getdata (NXhandle fid, void *data)
  {
    pNexusFile pFile;
    int32 iStart[H4_MAX_VAR_DIMS], iSize[H4_MAX_VAR_DIMS];
    NXname pBuffer;
    int32 iRank, iAtt, iType;
  
    pFile = NXIassert (fid);
  
    /* check if there is an SDS open */
    if (pFile->iCurrentSDS == 0) {
      NXIReportError (NXpData, "ERROR: no SDS open");
      return NX_ERROR;
    }
    /* first read dimension information */
    memset (iStart, 0, H4_MAX_VAR_DIMS * sizeof (int32));
    SDgetinfo (pFile->iCurrentSDS, pBuffer, &iRank, iSize, &iType, &iAtt);
    /* actually read */
    SDreaddata (pFile->iCurrentSDS, iStart, NULL, iSize, data);
    return NX_OK;
  } 
  
  /*-------------------------------------------------------------------------*/

  NXstatus
   NX4getinfo (NXhandle fid, int *rank, int dimension[], 
			    int *iType)
  {
    pNexusFile pFile;
    NXname pBuffer;
    int32 iAtt, myDim[H4_MAX_VAR_DIMS], i, iRank, mType;
  
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

  
  NXstatus  NX4getslab (NXhandle fid, void *data, int iStart[], int iSize[])
  {
    pNexusFile pFile;
    int32 myStart[H4_MAX_VAR_DIMS], mySize[H4_MAX_VAR_DIMS];
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

  NXstatus  NX4getnextattr (NXhandle fileid, NXname pName,
           int *iLength, int *iType)
  {
    pNexusFile pFile;
    int iRet;
    int32 iPType, iCount, count;
  
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
    if (pFile->iCurrentSDS == 0) {        
      if(pFile->iCurrentVG == 0) {
	/* global attribute */
	iRet = SDattrinfo (pFile->iSID, pFile->iAtt.iCurDir,
			   pName, &iPType, &iCount);
      }else {
	/* group attribute */
	iRet = Vattrinfo(pFile->iCurrentVG, pFile->iAtt.iCurDir,
			 pName, &iPType, &iCount, &count);
      }
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


  NXstatus  NX4getattr (NXhandle fid, char *name, void *data, int* datalen, int* iType)
  {
    pNexusFile pFile;
    int32 iNew, iType32, count;
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
      if(pFile->iCurrentVG == 0){
       /* global attribute */
       iNew = SDfindattr (pFile->iSID, name);
      } else {
        /* group attribute */
	iNew = Vfindattr(pFile->iCurrentVG, name); 
      }
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
      if(pFile->iCurrentVG == 0){
	iRet = SDattrinfo (pFile->iSID, iNew, pNam, &iType32, &iLen);
      } else {
	iRet = Vattrinfo(pFile->iCurrentVG,iNew,pNam,&iType32,&count,
			 &iLen);
      }
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
      if(pFile->iCurrentVG == 0){
	iRet = SDreadattr (pFile->iSID, iNew, pData);
      } else {
	iRet = Vgetattr(pFile->iCurrentVG, iNew, pData);
      }
    }
    if (iRet < 0) {
      sprintf (pBuffer, "ERROR: HDF could not read attribute data");
      NXIReportError (NXpData, pBuffer);
      return NX_ERROR;
    }
    /* copy data to caller */
    memset (data, 0, *datalen);
    if ((*datalen <= iLen) && 
     (*iType == DFNT_UINT8 || *iType == DFNT_CHAR8 || *iType == DFNT_UCHAR8)) {
      iLen = *datalen - 1; /* this enforces NULL termination regardless of size of datalen */
    }
    memcpy (data, pData, iLen);
    *datalen = iLen / DFKNTsize(*iType);
    free (pData);
    return NX_OK;
  }
 
  /*-------------------------------------------------------------------------*/

    
  NXstatus  NX4getattrinfo (NXhandle fid, int *iN)
  {
    pNexusFile pFile;
    int iRet;
    int32 iData, iAtt, iRank, iType;
    int32 iDim[H4_MAX_VAR_DIMS];
    NXname pNam;
  
    pFile = NXIassert (fid);
    if (pFile->iCurrentSDS != 0) {        /* SDS level */
      iRet = SDgetinfo (pFile->iCurrentSDS, pNam, &iRank, iDim, &iType,
                        &iAtt);
    } else {
      if(pFile->iCurrentVG == 0){  
	/* global level */
	iRet = SDfileinfo (pFile->iSID, &iData, &iAtt);
      } else {
	iRet = Vnattrs(pFile->iCurrentVG);
        iAtt = iRet;
      }
    }
    if (iRet < 0) {
      NXIReportError (NXpData, "NX_ERROR: HDF cannot read attribute numbers");
      *iN = 0;
      return NX_ERROR;
    }
    *iN = iAtt;
    return NX_OK;
  }
  
  /*-------------------------------------------------------------------------*/

  NXstatus  NX4getgroupID (NXhandle fileid, NXlink* sRes)
  {
    pNexusFile pFile;
  
    pFile = NXIassert (fileid);
  
    if (pFile->iCurrentVG == 0) {
      sRes->iTag = NX_ERROR;
      return NX_ERROR;
    } else {
      sRes->iTag = DFTAG_VG;
      sRes->iRef = VQueryref(pFile->iCurrentVG);
      NXIbuildPath(pFile,sRes->targetPath,1024);
      return NX_OK;
    }
    /* not reached */
    sRes->iTag = NX_ERROR;
    return NX_ERROR;
  }

 /*-------------------------------------------------------------------------*/

  NXstatus
   NX4getgroupinfo (NXhandle fid, int *iN, NXname pName, NXname pClass)
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
  
  /* ------------------------------------------------------------------- */

  NXstatus  NX4sameID (NXhandle fileid, NXlink* pFirstID, NXlink* pSecondID)
  {
    pNexusFile pFile;

    pFile = NXIassert (fileid);
    if ((pFirstID->iTag == pSecondID->iTag) & (pFirstID->iRef == pSecondID->iRef)) {
       return NX_OK;
    } else {
       return NX_ERROR;
    }
  }

  /*-------------------------------------------------------------------------*/

  
  NXstatus  NX4initattrdir (NXhandle fid)
  {
    pNexusFile pFile;
    int iRet;
    
    pFile = NXIassert (fid);
    NXIKillAttDir (pFile);
    iRet = NXIInitAttDir (pFile);
    if (iRet == NX_ERROR)
      return NX_ERROR;
    return NX_OK;
  }
  
 
  /*-------------------------------------------------------------------------*/
 
  
  NXstatus  NX4initgroupdir (NXhandle fid)
  {
    pNexusFile pFile;
    int iRet;
    
    pFile = NXIassert (fid);
    NXIKillDir (pFile);
    iRet = NXIInitDir (pFile);
    if (iRet < 0) {
      NXIReportError (NXpData,"NX_ERROR: no memory to store directory info");
      return NX_EOD;
    }
    return NX_OK;
  }
 
/*--------------------------------------------------------------------*/
void NX4assignFunctions(pNexusFunction fHandle)
{
      fHandle->nxclose=NX4close;
      fHandle->nxflush=NX4flush;
      fHandle->nxmakegroup=NX4makegroup;
      fHandle->nxopengroup=NX4opengroup;
      fHandle->nxclosegroup=NX4closegroup;
      fHandle->nxmakedata=NX4makedata;
      fHandle->nxcompmakedata=NX4compmakedata;
      fHandle->nxcompress=NX4compress;
      fHandle->nxopendata=NX4opendata;
      fHandle->nxclosedata=NX4closedata;
      fHandle->nxputdata=NX4putdata;
      fHandle->nxputattr=NX4putattr;
      fHandle->nxputslab=NX4putslab;    
      fHandle->nxgetdataID=NX4getdataID;
      fHandle->nxmakelink=NX4makelink;
      fHandle->nxmakenamedlink=NX4makenamedlink;
      fHandle->nxgetdata=NX4getdata;
      fHandle->nxgetinfo=NX4getinfo;
      fHandle->nxgetnextentry=NX4getnextentry;
      fHandle->nxgetslab=NX4getslab;
      fHandle->nxgetnextattr=NX4getnextattr;
      fHandle->nxgetattr=NX4getattr;
      fHandle->nxgetattrinfo=NX4getattrinfo;
      fHandle->nxgetgroupID=NX4getgroupID;
      fHandle->nxgetgroupinfo=NX4getgroupinfo;
      fHandle->nxsameID=NX4sameID;
      fHandle->nxinitgroupdir=NX4initgroupdir;
      fHandle->nxinitattrdir=NX4initattrdir;
      fHandle->nxprintlink=NX4printlink;
}
