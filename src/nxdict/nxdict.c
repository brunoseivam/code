
#line 2264 "nxdict.w"

/*---------------------------------------------------------------------------
                 Nexus Dictionary API implementation file.

  For documentation see the nxdict.tex file which comes with this 
  distribution.

  copyleft: Mark Koennecke
            Labor fuer Neutronenstreuung
            Paul Scherrer Institut
            CH-5232 Villigen-PSI
            Switzerland
            Mark.Koennecke@psi.ch

  No warranties of any kind, whether explicit or implied, taken.
  Distributed under the GNU copyleft license as documented elsewhere.

  August, 1997

  Version: 1.0
-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <mfhdf.h>
#include "lld.h"
#include "napi.h"
#include "stringdict.h"
#include "dynstring.h"
#include "nxdict.h"
/*------------------ The magic number used for pointer checking */
#define NXDMAGIC 260558
/*--------------------------------------------------------------------------
      Things defined in napi.c for error reporting 
---------------------------------------------------------------------------*/
       extern void *NXpData;
       extern void (*NXIReportError)(void *pData, char *pBuffer);  
/*--------------------------------------------------------------------------*/
#define DEFDEBUG 1
/* define DEFDEBUG when you wish to print your definition strings before
   action. This can help a lot to resolve mysteries when working with
   dictionaries.
*/
/*-------------------------------------------------------------------------*/

#line 362 "nxdict.w"

    typedef struct __NXdict
                           {
                               int iID;
                               pStringDict pDictionary;
                           } sNXdict;
/*------------------ verbosity level -------------------------------------*/
   static int iVerbosity = 0 ;

#line 2311 "nxdict.w"

/*-------------------------------------------------------------------------*/
  static char *NXDIReadFile(FILE *fd)
  {
     char *pNew = NULL;
     long lLength = 0;
 
     assert(fd); 

     /* determine length of file */
     fseek(fd,0L,SEEK_END);
     lLength = ftell(fd);
     if(lLength <= 0)
     {
        return NULL;
     }
     fseek(fd,0L,SEEK_SET);

     /* allocate buffer */
     lLength += 3;
     pNew = (char *)malloc(lLength*sizeof(char));
     if(!pNew)
     {
       return NULL;
     }
     memset(pNew,0,lLength); /* this ensures a 0 at the end */

     /* read file */
     fread(pNew,sizeof(char),lLength-3,fd);
     
     /* check for existence of the NXDICT string in the file */
     if(strncmp(pNew,"##NXDICT-1.0",12) != 0 )
     {
        NXIReportError(NXpData,"ERROR: This is NO NXdict file");
        free(pNew);
        return NULL;
     }
     return pNew;
  }
/*--------------------------------------------------------------------------*/

#line 490 "nxdict.w"

#define FWORD 1
#define FHASH 2
#define FEOL  3
#define FEOB  4
#define FEQUAL 5
#define FSLASH 6

   static char *NXDIfNextToken(char *pPtr, char *pToken, int *iToken)
   {
       pToken[0] = '\0';
       /* skip whitespace */
       while( (*pPtr == ' ') || (*pPtr == '\t') )
       {
         pPtr++;
       } 

       /* check for special characters */
       if(*pPtr == '#')
       {
         *iToken = FHASH;
         pToken[0] = *pPtr;
         pPtr++;
         return pPtr;
       }
       else if(*pPtr == '\n')
       {
         *iToken = FEOL;
         pToken[0] = *pPtr;
         pPtr++;
         return pPtr;
       }
       else if(*pPtr == '\0')
       {
         *iToken = FEOB;
         pToken[0] = *pPtr;
         pPtr++;
         return pPtr;
       }
       else if(*pPtr == '=')
       {
         *iToken = FEQUAL;
         pToken[0] = *pPtr;
         pPtr++;
         return pPtr;
       }
       else if(*pPtr == '\\')
       {
         *iToken = FSLASH;
         pToken[0] = *pPtr;
         pPtr++;
         return pPtr;
       }
       else 
       {
         *iToken = FWORD;
         /* copy word to pToken */
         while( (*pPtr != ' ') && (*pPtr != '\t') && 
                (*pPtr != '\n') && (*pPtr != '\0') && (*pPtr != '=') )
         {
           *pToken = *pPtr;
           pPtr++;
           pToken++;
         } 
         *pToken = '\0';
         return pPtr;
       }
      /* not reached */
      return pPtr;
   }
    

#line 2351 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 566 "nxdict.w"

#define AMODE 0
#define DMODE 1

  static void  NXDIParse(char *pBuffer, pStringDict pDict)
  {
     char *pPtr;
     int iToken;
     int iMode;
     char pAlias[132];
     char pDefinition[1024]; /* this is > 10 lines of definition */
     char pWord[132];

     assert(pBuffer);
     assert(pDict);

     iMode = AMODE;
     pPtr = pBuffer;
     iToken = -1;
     pDefinition[0] = '\0';
     pAlias[0] = '\0';
     pWord[0] = '\0';

     while(iToken != FEOB)
     {
         pPtr = NXDIfNextToken(pPtr,pWord,&iToken);
         switch(iToken)
         {
           case FHASH:
           case FSLASH: /* skip over \n to next non blank */
                       while(*pPtr != '\n')
                       {
                          pPtr++;
                          /* check for end of file */
                          if(*pPtr == '\0')
                          {
                            return;
                          }
                       }
                       pPtr++;
                       break;
           case FEQUAL: /* do a mode change */
                       iMode = DMODE;
                       pDefinition[0] = '\0';
                       break;

           case FWORD:
                       if(iMode == AMODE)
                       {
                         strcpy(pAlias,pWord);
                       }  
                       else
                       {
                         strcat(pDefinition,pWord);
                         strcat(pDefinition," ");
                       }
                       break;
           case FEOL:
                       if(iMode == DMODE)
                       {
                          /* enter in dictionary */
                          StringDictAddPair(pDict,pAlias,pDefinition);
                          iMode = AMODE;
                          pAlias[0] = '\0';
                       }
                       break;
           case FEOB:
                       if(iMode == AMODE) 
                       {
                         /* empty line or a problem */
                       }          
                       else
                       {
                          /* enter in dictionary */
                          StringDictAddPair(pDict,pAlias,pDefinition);
                          iMode = AMODE;
                          pAlias[0] = '\0';
                       }
                       return;
           default:
                   assert(0); /* unrecognized token is a programming
                                 error
                              */
                   break;
         }
     }
  }

#line 2353 "nxdict.w"

/*--------------------------------------------------------------------------*/
  NXstatus NXDinitfromfile(char *filename, NXdict *pData)
  {
     NXdict pNew = NULL;
     FILE *fd = NULL;
     char *pBuffer = NULL;
     char pError[512];


#line 383 "nxdict.w"

     /* allocate a new NXdict structure */
     if(iVerbosity == NXalot)
     {
         NXIReportError(NXpData, "Allocating new NXdict structure ");
     }
     pNew = (NXdict)malloc(sizeof(sNXdict));
     if(!pNew)
     {
         NXIReportError(NXpData, "Insufficient memory for creation of NXdict");
         return NX_ERROR;
     }
     
     /* initialise it */
     pNew->iID = NXDMAGIC;
     pNew->pDictionary = CreateStringDict();
     if(!pNew->pDictionary)
     {
         NXIReportError(NXpData, "Insufficient memory for creation of NXdict");
         free(pNew);
         return NX_ERROR;
     }


#line 2362 "nxdict.w"


#line 410 "nxdict.w"

     /* is there a file name argument */
     if(filename == NULL)
     {
        if(iVerbosity == NXalot)
        {
           NXIReportError(NXpData, "NXDinitfrom file finished without data");
        }
        *pData = pNew;
        return NX_OK;
     }

#line 2363 "nxdict.w"


#line 424 "nxdict.w"

      fd = fopen(filename,"rb");
      if(!fd)
      {
         sprintf(pError,"ERROR: file %s NOT found ",filename);
         NXIReportError(NXpData, pError);
         NXIReportError(NXpData, "NXDinitfrom file finished without data");
         *pData = pNew;
         return NX_ERROR;
      }



#line 2364 "nxdict.w"
 

#line 444 "nxdict.w"

       /* read the file contents */
       if(iVerbosity == NXalot)
       {
          NXIReportError(NXpData, "NXDinitfrom: reading file");
       }
       pBuffer = NXDIReadFile(fd);
       fclose(fd); /* we are done with it then */
       if(!pBuffer)
       {
         sprintf(pError,"ERROR: reading file %s or no memory",filename);
         NXIReportError(NXpData, pError);
         NXIReportError(NXpData, "NXDinitfrom file finished without data");
         *pData = pNew;
         return NX_ERROR;
       }

       /* parse it */
       if(iVerbosity == NXalot)
       {
          NXIReportError(NXpData, "NXDinitfrom: parsing dictionary definitions");
       }
       NXDIParse(pBuffer, pNew->pDictionary);

#line 2365 "nxdict.w"


       if(iVerbosity == NXalot)
       {
          NXIReportError(NXpData, "NXDinitfrom: performed successfully");
       }
       free(pBuffer);
       *pData = pNew;
       return NX_OK;
  }   
/*--------------------------------------------------------------------------*/

#line 660 "nxdict.w"

   NXdict NXDIAssert(NXdict handle)
   {
      NXdict self = NULL;
      assert(handle);
      self = (NXdict)handle;
      assert(self->iID == NXDMAGIC);
      return self;
   }

#line 2376 "nxdict.w"

/*-------------------------------------------------------------------------*/

#line 671 "nxdict.w"

   NXstatus NXDclose(NXdict handle, char *filename)
   {
      NXdict self;
      const char *pKey = NULL;
      char pValue[1024];
      FILE *fd = NULL;

      self = NXDIAssert(handle);

      if(filename) /* we must write a file */
      {
        if(iVerbosity == NXalot)
        {
           sprintf(pValue,"Writing file %s",filename);
           NXIReportError(NXpData, pValue);
        }
        fd = fopen(filename,"w");
        if(!fd)
        {
           sprintf(pValue,"ERROR: opening file %s for write",filename);
           NXIReportError(NXpData, pValue);
           return NX_ERROR;
        }   
        
        /* write our magic recognition header */
        fprintf(fd,"##NXDICT-1.0\n");

        /* write all our keys */
        pKey = StringDictGetNext(self->pDictionary, pValue,1023);
        while(pKey != NULL)
        {
          fprintf(fd,"%s = %s\n",pKey,pValue);
          pKey = StringDictGetNext(self->pDictionary,pValue,1023);
        }
        fclose(fd);
        if(iVerbosity == NXalot)
        {
           sprintf(pValue,"File %s written",filename);
           NXIReportError(NXpData, pValue);
        }
      }

      /* now we send the cleaners in */
      DeleteStringDict(self->pDictionary);
      free(self);
      return NX_OK;
   }

#line 2378 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 724 "nxdict.w"

   NXstatus NXDadd(NXdict handle, char *alias, char *pDef)
   {
     NXdict self;
     int iRet;

     self = NXDIAssert(handle);
     iRet = StringDictAddPair(self->pDictionary,alias,pDef);
     if(!iRet)
     {
       return NX_ERROR;
     }
     return NX_OK;
   }
/*---------------------------------------------------------------------------*/
   NXstatus NXDget(NXdict handle, char *pKey, char *pBuffer, int iBufLen)
   {
     NXdict self;
     int iRet;

     self = NXDIAssert(handle);
     iRet = StringDictGet(self->pDictionary,pKey,pBuffer,iBufLen);
     if(!iRet)
     {
       return NX_ERROR;
     }
#ifdef DEFDEBUG
     printf("Resolved: %s to %s\n",pKey,pBuffer);
#endif
     return NX_OK;
   }
/*-------------------------------------------------------------------------*/
   NXstatus NXDupdate(NXdict handle, char *pKey, char *pNewVal)
   {
     NXdict self;
     int iRet;

     self = NXDIAssert(handle);
     iRet = StringDictUpdate(self->pDictionary,pKey,pNewVal);
     if(!iRet)
     {
       return NX_ERROR;
     }
     return NX_OK;
   }


#line 2380 "nxdict.w"

/*-----------------------------------------------------------------------*/

#line 776 "nxdict.w"

#define NORMAL 1
#define ALIAS  2
   pDynString NXDItextreplace(NXdict handle, char *pDefString)
   {
     NXdict self;
     int iRet, iPos, i;
     pDynString pReplaced = NULL;
     char pBueffel[1024];
     char pBuffer2[1024];
     char *pPtr;
     int iState;

     self = NXDIAssert(handle);
     
     /* create a dynamic string */
     pReplaced = CreateDynString(strlen(pDefString),512);
     if(!pReplaced)
     {
       NXIReportError(NXpData,"ERROR: out of memory in NXDtextreplace");
       return NULL;
     }

     /* the loop */
     iState = NORMAL;
     for(i = 0, pPtr = pDefString; i < strlen(pDefString); i++,pPtr++)
     {
        if(iState == NORMAL)
        {
           if(*pPtr == '$')
           {
             iState = ALIAS;
             memset(pBueffel,0,1024);
             iPos = 0;
           }
           else
           {
               DynStringConcatChar(pReplaced,*pPtr);
           }
        }
        else if(iState == ALIAS)
        {
           switch(*pPtr)
           {
             case '(': /* ignore */
                       break;
             case ')': 
                       /* do the replacement */
                       memset(pBuffer2,0,1023);
                       iRet = NXDget(handle, pBueffel,pBuffer2,1023);
                       if(iRet != NX_OK)
                       {
                         DeleteDynString(pReplaced);
                         return NULL;
                       }
                       DynStringConcat(pReplaced,pBuffer2);
                       iState = NORMAL;
                       break;
             default:
                       pBueffel[iPos] = *pPtr;
                       iPos++;
                       if(iPos >= 1024)
                       {
                          NXIReportError(NXpData,
                                     "ERROR: buffer overrun in NXDItextreplace");
                          DeleteDynString(pReplaced);
                          return NULL;
                       }
                       break;
           }
        }
     }
#ifdef DEFDEBUG
    printf("Replacement result: %s\n",GetCharArray(pReplaced));
#endif
     return pReplaced;
   }
/*--------------------------------------------------------------------------*/
   NXstatus NXDtextreplace(NXdict handle, char *pDefString, 
                           char *pBuffer, int iBufLen)
   {
     pDynString pResult = NULL;
     char *pPtr = NULL;

     pResult = NXDItextreplace(handle,pDefString);
     if(!pResult)
     {
         return NX_ERROR;
     }

     /* copy results home */
     pPtr = GetCharArray(pResult);
     strncpy(pBuffer,pPtr,iBufLen);   
     DeleteDynString(pResult);
     return NX_OK;
   }

#line 2382 "nxdict.w"

/*------------------- The Defintion String Parser -----------------------*/
/*------- Data structures */

#line 886 "nxdict.w"

  typedef struct {
                  char pText[20];
                  int iCode;
                 }  TokDat;

#line 2385 "nxdict.w"


#line 896 "nxdict.w"

#define TERMSDS 100
#define TERMVG  200
#define TERMLINK 300

   typedef struct {
                    char *pPtr;
                    char pToken[256];
                    int iToken;
                    int iDepth;
                    int iMayCreate;
                    int iTerminal;
                  } ParDat;

#line 2386 "nxdict.w"


#line 1101 "nxdict.w"

   static void DummyError(void *pData, char *pError)
   {
     return;
   }

#line 2387 "nxdict.w"


#line 1215 "nxdict.w"

  typedef struct {
                   char name[256];
                   char value[256];
                  }AttItem;

#line 2388 "nxdict.w"
 
/*------------------------------------------------------------------------*/

#line 918 "nxdict.w"

/*---------------- Token name defines ---------------------------*/
#define DSLASH 0
#define DKOMMA 1
#define DSDS   2
#define DLINK  3
#define DGROUP 4
#define DRANK  5
#define DDIM   6
#define DTYPE  7
#define DWORD  9
#define DOPEN  10
#define DCLOSE 11
#define DATTR  12
#define DEND   13

/*----------------- Keywords ----------------------------------------*/

  static TokDat TokenList[8] = { 
                                {"SDS",DSDS},
                                {"NXLINK",DLINK},
                                {"NXVGROUP",DGROUP},
                                {"-dim",DDIM},
                                {"-type",DTYPE},
                                {"-rank",DRANK},
                                {"-attr",DATTR},
                                {NULL,0} };

/*-----------------------------------------------------------------------*/
   static void NXDIDefToken(ParDat *sStat)
   {
       int i;
       

       sStat->pToken[0] = '\0';

       /* skip whitespace */
       while( (*(sStat->pPtr) == ' ') || (*(sStat->pPtr) == '\t') )
       {
         sStat->pPtr++;
       } 

       /* check for special characters */
       if(*(sStat->pPtr) == '/')
       {
         sStat->iToken = DSLASH;
         sStat->pToken[0] = *(sStat->pPtr);
         sStat->pPtr++;
         return;
       }
       else if(*(sStat->pPtr) == ',')
       {
         sStat->iToken = DKOMMA;
         sStat->pToken[0] = *(sStat->pPtr);
         sStat->pPtr++;
         return;
       }
       else if(*(sStat->pPtr) == '\0')
       {
         sStat->iToken = DEND;
         sStat->pToken[0] = *(sStat->pPtr);
         sStat->pPtr++;
         return;
       }
       else if(*(sStat->pPtr) == '{')
       {
         sStat->iToken = DOPEN;
         sStat->pToken[0] = *(sStat->pPtr);
         sStat->pPtr++;
         return;
       }
       else if(*(sStat->pPtr) == '}')
       {
         sStat->iToken = DCLOSE;
         sStat->pToken[0] = *(sStat->pPtr);
         sStat->pPtr++;
         return;
       }
       else 
       {
         sStat->iToken = DWORD;
         /* copy word to pToken */
         i = 0;
         while( (*(sStat->pPtr) != ' ') && (*(sStat->pPtr) != '\t') && 
                (*(sStat->pPtr) != '/') && (*(sStat->pPtr) != '\0') &&
                (*(sStat->pPtr) != ',') && (*(sStat->pPtr) != '}') )
         {
           sStat->pToken[i] = *(sStat->pPtr);
           sStat->pPtr++;
           i++;
         } 
         sStat->pToken[i] = '\0';

         /*--------- try to find word in Tokenlist */
         for(i = 0; i < 7; i++)
         {
           if(strcmp(sStat->pToken,TokenList[i].pText) == 0)
           {
             sStat->iToken = TokenList[i].iCode;
             break;
           }
         }
         return;
       }
      /* not reached */
      return;
   }


#line 2390 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1114 "nxdict.w"

   int NXDIParsePath(NXhandle hfil, ParDat *pParse)
   {
       int iRet, iToken;
       void (*ErrFunc)(void *pData, char *pErr); 
       char pName[132], pClass[132];
       char pError[256];

       /* get the name */
       NXDIDefToken(pParse); /* next token */
       if( (pParse->iToken == DSDS) || (pParse->iToken == DGROUP)
           || (pParse->iToken == DLINK) )
       {
         /* put back & OK */
         pParse->pPtr -= strlen(pParse->pToken);
         return NX_OK;
       }
       if(pParse->iToken != DWORD)
       {
          sprintf(pError,"ERROR: parse error at %s, expected vGroup name",
                  pParse->pToken);
          NXIReportError(NXpData, pError);
          return NX_ERROR;
       }       
       strcpy(pName,pParse->pToken);
     
       /* now we expect a komma */
       NXDIDefToken(pParse); /* next token */
       if(pParse->iToken != DKOMMA)
       {
          sprintf(pError,"ERROR: parse error at %s, expected komma",
                  pParse->pToken);
          NXIReportError(NXpData, pError);
          return NX_ERROR;
       }       

       /* next must be the class */
       NXDIDefToken(pParse); /* next token */
       if(pParse->iToken != DWORD)
       {
          sprintf(pError,"ERROR: parse error at %s, expected vGroup class",
                  pParse->pToken);
          NXIReportError(NXpData, pError);
          return NX_ERROR;
       }       
       strcpy(pClass,pParse->pToken);

       /* done reading, ACTION, first install dummy error handler */
       ErrFunc = NXIReportError;
       NXMSetError(NXpData, DummyError);
 
       /* try opening vGroup */
       iRet = NXopengroup(hfil, pName, pClass);
       NXMSetError(NXpData,ErrFunc);
       if(iRet == NX_OK)
       {
         pParse->iDepth++;
         return NX_OK;
       } 
       else
       {
          /* we need to create it, if we may */
          if(pParse->iMayCreate)
          {
            iRet = NXmakegroup(hfil,pName,pClass);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 return iRet;
            }
            iRet = NXopengroup(hfil,pName,pClass);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 return iRet;
            }
            pParse->iDepth++;
            return NX_OK; 
          }
          else
          {
             /* this is an error */
             sprintf(pError,"ERROR: vGroup %s, %s NOT found",pName, pClass);
             NXIReportError(NXpData,pError);
             return NX_ERROR;
          }
       }
       /* not reached */
       return NX_ERROR;
   }

#line 2392 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1481 "nxdict.w"

   static int NXDIParseAttr(ParDat *pParse, int iList)
   {
     char pError[256];
     int iRet;
     AttItem sAtt;

     /* a { is expected */
     NXDIDefToken(pParse);
     if(pParse->iToken != DOPEN)
     {
        sprintf(pError,"ERROR: expected {, got %s",pParse->pToken);
        NXIReportError(NXpData,pError);
        return NX_ERROR;
     }

     /* a word is expected */
     NXDIDefToken(pParse);
     if(pParse->iToken != DWORD)
     {
        sprintf(pError,"ERROR: expected attribute name, got %s",pParse->pToken);
        NXIReportError(NXpData,pError);
        return NX_ERROR;
     }
     strcpy(sAtt.name,pParse->pToken);

     /* a , is expected */
     NXDIDefToken(pParse);
     if(pParse->iToken != DKOMMA)
     {
        sprintf(pError,"ERROR: expected , , got %s",pParse->pToken);
        NXIReportError(NXpData,pError);
        return NX_ERROR;
     }

     /* a word is expected */
     NXDIDefToken(pParse);
     if(pParse->iToken != DWORD)
     {
        sprintf(pError,"ERROR: expected attribute value, got %s",pParse->pToken);
        NXIReportError(NXpData,pError);
        return NX_ERROR;
     }
     strcpy(sAtt.value,pParse->pToken);

     /* a } is expected */
     NXDIDefToken(pParse);
     if(pParse->iToken != DCLOSE)
     {
        sprintf(pError,"ERROR: expected }, got %s",pParse->pToken);
        NXIReportError(NXpData,pError);
        return NX_ERROR;
     }

     /* enter into list */
     LLDnodeAppendFrom(iList,&sAtt);
     return NX_OK;
   }

#line 2394 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1428 "nxdict.w"

   static int NXDIParseDim(ParDat *pParse, int *iDim)
   {
       char pError[256];
       int iRet, i;

        /* initialise dimensions to 0 */
        for(i = 0; i < MAX_VAR_DIMS; i++)
        {
          iDim[i] = 0;
        }

        NXDIDefToken(pParse);
        if(pParse->iToken != DOPEN)
        {
          sprintf(pError,"ERROR: expected {, got %s",pParse->pToken);
          NXIReportError(NXpData,pError);
          return NX_ERROR;
        }

        i = 0;      
        while(pParse->iToken != DCLOSE)
        {
           /* get a number */
           NXDIDefToken(pParse);
           if(pParse->iToken != DWORD)
           {
              sprintf(pError,"ERROR: expected number, got %s",pParse->pToken);
              NXIReportError(NXpData,pError);
              return NX_ERROR;
           }
           iDim[i] = atoi(pParse->pToken);
           i++;
           /* next must be close of komma */
           NXDIDefToken(pParse);
           if( (pParse->iToken != DKOMMA) && (pParse->iToken != DCLOSE) )
           {
              sprintf(pError,"ERROR: expected , or }, got %s",pParse->pToken);
              NXIReportError(NXpData,pError);
              return NX_ERROR;
           }
           if(pParse->iToken == DCLOSE)
           {
              break;
           }
        }
        return NX_OK;
   }

#line 2396 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1379 "nxdict.w"

   static TokDat tDatType[] = {
                      {"DFNT_FLOAT32",DFNT_FLOAT32},   
                      {"DFNT_FLOAT64",DFNT_FLOAT64},    
                      {"DFNT_INT8",DFNT_INT8},  
                      {"DFNT_UINT8",DFNT_UINT8},
                      {"DFNT_INT16",DFNT_INT16},      
                      {"DFNT_UINT16",DFNT_UINT16},
                      {"DFNT_INT32",DFNT_INT32},
                      {"DFNT_UINT32",DFNT_UINT32},
                      {NULL,-122} };



   static int NXDIParseType(ParDat *pParse, int *iType)
   {
      char pError[256];
      int i = 0;

      NXDIDefToken(pParse);
      if(pParse->iToken != DWORD)
      { 
        sprintf(pError,"ERROR: expected data type, got %s", pParse->pToken);
        NXIReportError(NXpData,pError);
        return NX_ERROR;
      }
       
      /* try to interpret data type */
      while(tDatType[i].iCode > 0) {
        if(strcmp(tDatType[i].pText,pParse->pToken) == 0)
        {
           *iType = tDatType[i].iCode;
            return NX_OK;
        }
        i++;
      }
      /* if we are here, the data type has not been recognized. Reason for
         some boring error reporting code
      */
      sprintf(pError,"ERROR: %s not recognized as valid data type",
                      pParse->pToken);
      NXIReportError(NXpData,pError);
      return NX_ERROR;      
   }

#line 2398 "nxdict.w"

/*-------------------------------------------------------------------------*/

#line 1223 "nxdict.w"

   static int NXDIParseSDS(NXhandle hfil, ParDat *pParse)
   {
      int iType = DFNT_FLOAT32;
      int iRank = 1;
      int32 iDim[MAX_VAR_DIMS];
      int iList;
      int iRet, iStat;
      char pError[256];
      char pName[MAX_NC_NAME];
      void (*ErrFunc)(void *pData, char *pErr);
      AttItem sAtt; 


       iDim[0] = 1;
       /* first find the name */
       NXDIDefToken(pParse);
       if(pParse->iToken != DWORD)
       {
          sprintf(pError,"ERROR: parsing, expected name, got %s",
                  pParse->pToken);
          NXIReportError(NXpData,pError);
          return NX_ERROR;
       }
        strcpy(pName,pParse->pToken);

       /* create the attribute list */
       iList = LLDcreate(sizeof(AttItem));
       if(iList < 0)
       {
          NXIReportError(NXpData, "ERROR: cannot create list in NXDIParseSDS");
          return NX_ERROR;
       }

       NXDIDefToken(pParse);
       while(pParse->iToken != DEND)
       {
          switch(pParse->iToken)
          {
            case DRANK: /* rank */
                       NXDIDefToken(pParse); /* advance */
                       if(pParse->iToken != DWORD)
                       {
                         sprintf(pError,
                         "ERROR: expected int, got %s", pParse->pToken);
                          NXIReportError(NXpData,pError);
                          LLDdelete(iList);
                          return NX_ERROR; 
                       }
                       iRank = atoi(pParse->pToken);
                       break;
            case DDIM:
                     iRet = NXDIParseDim(pParse, iDim);
                     if(iRet == NX_ERROR)
                     {
                        LLDdelete(iList);
                        return iRet;
                     } 
                     break;
            case DTYPE:
                     iRet = NXDIParseType(pParse, &iType);
                     if(iRet == NX_ERROR)
                     {
                        LLDdelete(iList);
                        return iRet;
                     } 
                     break;
            case DATTR:
                     iRet = NXDIParseAttr(pParse, iList);
                     if(iRet == NX_ERROR)
                     {
                        LLDdelete(iList);
                        return iRet;
                     } 
                     break;
            case DEND:
                     break;
            default:
                     sprintf(pError,"ERROR: cannot identify token %s",
                                     pParse->pToken);
                     NXIReportError(NXpData, pError);
                     LLDdelete(iList);
                     return NX_ERROR;
                    
          }
          NXDIDefToken(pParse);
       }
       
       /* whew! got all information for doing the SDS */
       /* first install dummy error handler, try open it, then
          deinstall again and create if allowed 
       */
       ErrFunc = NXIReportError;
       NXMSetError(NXpData, DummyError);
 
       /* try opening SDS */
       iRet = NXopendata(hfil, pName);
       NXMSetError(NXpData,ErrFunc);
       if(iRet == NX_OK)
       {
         LLDdelete(iList);
         return NX_OK;
       } 
       else
       {
          /* we need to create it, if we may */
          if(pParse->iMayCreate)
          {
            iRet = NXmakedata(hfil,pName,iType, iRank,iDim);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 LLDdelete(iList);
                 return iRet;
            }
            iRet = NXopendata(hfil,pName);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 LLDdelete(iList);
                 return iRet;
            }
            /* put attributes in */
            iRet = LLDnodePtr2First(iList);
            while(iRet != 0)
            {
              LLDnodeDataTo(iList,&sAtt);
              iStat = NXputattr(hfil,sAtt.name,
                                sAtt.value,strlen(sAtt.value),NX_CHAR);
              if(iStat != NX_OK)
              {
                 /* NeXus already complained bitterly */
                 LLDdelete(iList);
                 return iStat;
              }
              iRet = LLDnodePtr2Next(iList);
            }
            LLDdelete(iList);
            return NX_OK; 
          }
          else
          {
             /* this is an error */
             sprintf(pError,"ERROR: SDS %s NOT found",pName);
             NXIReportError(NXpData,pError);
             LLDdelete(iList);
             return NX_ERROR;
          }
       }
       return NX_OK;
   }

#line 2400 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1544 "nxdict.w"

   static int NXDIParseLink(NXhandle hfil, NXdict pDict,ParDat *pParse)
   {
     char pError[256];
     int i, iRet;

     /* need one word of alias */ 
     NXDIDefToken(pParse);
     if(pParse->iToken != DCLOSE)
     {
        sprintf(pError,"ERROR: expected alias , got %s",pParse->pToken);
        NXIReportError(NXpData,pError);
        return NX_ERROR;
     }
    
     /* move back in hierarchy */
     for(i = 0; i < pParse->iDepth; i++)
     {
       iRet = NXclosegroup(hfil);
       if(iRet == NX_ERROR)
       {
         return NX_ERROR;
       }
     }
     
     /* open the link instead */ 
     return NXDopenalias(hfil, pDict, pParse->pToken);

   }

#line 2402 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1034 "nxdict.w"

    static int NXDIDefParse(NXhandle hFil, NXdict pDict, ParDat *pParse)
    {
       int iRet;
       char pError[256];
 
       pParse->iToken = -1;
       while(pParse->iToken != DEND)
       {
         NXDIDefToken(pParse); /* next token */
         switch(pParse->iToken)
         {
            case DEND:
                      break;
            case DSLASH:
                      iRet = NXDIParsePath(hFil, pParse);
                      if(iRet == NX_ERROR)
                      {
                         return NX_ERROR;
                      }
                      break;
            case DSDS:
                      iRet = NXDIParseSDS(hFil, pParse);
                      if(iRet == NX_ERROR)
                      {
                         return NX_ERROR;
                      }
                      pParse->iTerminal = TERMSDS;
                      break;
            case DLINK:
                      iRet = NXDIParseLink(hFil,pDict, pParse);                      
                      if(iRet == NX_ERROR)
                      {
                         return NX_ERROR;
                      }
                      pParse->iTerminal = TERMLINK;
                      break;
            case DGROUP:
                      pParse->iTerminal = TERMVG;
                      return NX_OK;
            default:
                     sprintf(pError,
            "ERROR: Definition String parse error: %s not permitted here",
             pParse->pToken);
                     NXIReportError(NXpData,pError);
                     return NX_ERROR;
                     break;
         }
       }
       return NX_OK;
    }

#line 2404 "nxdict.w"

/*----------------------------------------------------------------------*/

#line 1576 "nxdict.w"

  static NXstatus NXDIUnwind(NXhandle hFil, int iDepth)
  {
     int i, iRet;
      
     for(i = 0; i < iDepth; i++)
     {
        iRet = NXclosegroup(hFil);
        if(iRet != NX_OK)
        {
          return NX_ERROR;
        }
     }
     return NX_OK;
  }

#line 2406 "nxdict.w"

/*-------------------- The Data Transfer Functions ----------------------*/

#line 1597 "nxdict.w"

   NXstatus NXDopendef(NXhandle hfil, NXdict dict, char *pDef)
   {
     NXdict pDict;
     ParDat pParse;
     int iRet, i, iStat;

     pDict = NXDIAssert(dict);
     
     /* parse and act on definition string */
     pParse.iMayCreate = 1;
     pParse.pPtr = pDef;
     pParse.iDepth = 0;
     iRet = NXDIDefParse(hfil,pDict,&pParse);
     if(iRet == NX_ERROR)
     {
         /* unwind and throw up */
         iRet = NXDIUnwind(hfil,pParse.iDepth);
         return NX_ERROR;
     }

     
     /* try rewinding the hierarchy */
     if(pParse.iTerminal == TERMSDS)
     {
         iStat = NXDIUnwind(hfil,pParse.iDepth);
         if(iStat != NX_OK)
         {
            return NX_ERROR;
         }
     }
     /* do not rewind on links */
     return iRet;
   }

#line 2408 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1637 "nxdict.w"

   NXstatus NXDopenalias(NXhandle hfil, NXdict dict, char *pAlias)
   {
     NXdict pDict;
     int iRet;
     char pDefinition[1024];
     pDynString pReplaced = NULL;

     pDict = NXDIAssert(dict);
     
     /* get Definition String  */
     iRet = NXDget(pDict,pAlias,pDefinition,1023);
     if(iRet != NX_OK)
     {
        sprintf(pDefinition,"ERROR: alias %s not recognized",pAlias);
        NXIReportError(NXpData,pDefinition);
        return NX_ERROR;
     }
     
     /* do the text replacement */
     pReplaced = NXDItextreplace(dict,pDefinition);
     if(!pReplaced)
     {
         return NX_ERROR;
     }

     /* call NXDopendef */
     iRet = NXDopendef(hfil,dict,GetCharArray(pReplaced));
     DeleteDynString(pReplaced);
     return iRet;
   }

#line 2410 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1674 "nxdict.w"

   NXstatus NXDputdef(NXhandle hFil, NXdict dict, char *pDef, void *pData)
   {
     NXdict pDict;
     ParDat pParse;
     int iRet, i, iStat;

     pDict = NXDIAssert(dict);
     
     /* parse and act on definition string */
     pParse.iMayCreate = 1;
     pParse.pPtr = pDef;
     pParse.iDepth = 0;
#ifdef DEFDEBUG
     printf("Putting: %s\n",pDef);
#endif
     iRet = NXDIDefParse(hFil,pDict,&pParse);
     if(iRet == NX_ERROR)
     {
          NXDIUnwind(hFil,pParse.iDepth);
          return NX_ERROR;
     }

     
     /* only SDS can be written */
     if(pParse.iTerminal != TERMSDS)
     {
        NXIReportError(NXpData,
                     "ERROR: can only write to an SDS!");
        iStat = NX_ERROR;
     } 
     else 
     {
       /* the SDS should be open by now, write it */
       iStat = NXputdata(hFil, pData);
       iRet = NXclosedata(hFil);
     }   


     /* rewind the hierarchy */
     iRet = NXDIUnwind(hFil,pParse.iDepth);
     if(iRet != NX_OK)
     {
       return NX_ERROR;
     }
     return iStat;
   }

#line 2412 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1725 "nxdict.w"

  NXstatus NXDputalias(NXhandle hFil, NXdict dict, char *pAlias, void *pData)
  {
     NXdict pDict;
     int iRet;
     char pDefinition[1024];
     pDynString pReplaced = NULL;

     pDict = NXDIAssert(dict);
     
     /* get Definition String  */
     iRet = NXDget(pDict,pAlias,pDefinition,1023);
     if(iRet != NX_OK)
     {
        sprintf(pDefinition,"ERROR: alias %s not recognized",pAlias);
        NXIReportError(NXpData,pDefinition);
        return NX_ERROR;
     }
     
     /* do text replacement */
     pReplaced = NXDItextreplace(dict,pDefinition);
     if(!pReplaced)
     {
       return NX_ERROR;
     }

     /* call NXDputdef */
     iRet = NXDputdef(hFil,dict,GetCharArray(pReplaced),pData);
     DeleteDynString(pReplaced);
     return iRet;
  }

#line 2414 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1760 "nxdict.w"

   NXstatus NXDgetdef(NXhandle hFil, NXdict dict, char *pDef, void *pData)
   {
     NXdict pDict;
     ParDat pParse;
     int iRet, i, iStat;

     pDict = NXDIAssert(dict);
     
     /* parse and act on definition string */
     pParse.iMayCreate = 0;
     pParse.pPtr = pDef;
     pParse.iDepth = 0;
#ifdef DEFDEBUG
     printf("Getting: %s\n",pDef);
#endif
     iRet = NXDIDefParse(hFil,pDict,&pParse);
     if(iRet == NX_ERROR)
     {
         /* unwind and throw up */
          NXDIUnwind(hFil,pParse.iDepth);
          return NX_ERROR;
     }


     /* only SDS can be written */
     if(pParse.iTerminal != TERMSDS)
     {
        NXIReportError(NXpData,
                     "ERROR: can only write to an SDS!");
        iStat = NX_ERROR;
     } 
     else 
     {
       /* the SDS should be open by now, read it */
       iStat = NXgetdata(hFil, pData);
       iRet = NXclosedata(hFil);
     }   


     /* rewind the hierarchy */
     iRet = NXDIUnwind(hFil,pParse.iDepth);
     if(iRet != NX_OK)
     {
        return NX_ERROR;
     }
     return iStat;
   }

#line 2416 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1812 "nxdict.w"

  NXstatus NXDgetalias(NXhandle hFil, NXdict dict, char *pAlias, void *pData)
  {
     NXdict pDict;
     int iRet;
     char pDefinition[1024];
     pDynString pReplaced = NULL;

     pDict = NXDIAssert(dict);
     
     /* get Definition String  */
     iRet = NXDget(pDict,pAlias,pDefinition,1023);
     if(iRet != NX_OK)
     {
        sprintf(pDefinition,"ERROR: alias %s not recognized",pAlias);
        NXIReportError(NXpData,pDefinition);
        return NX_ERROR;
     }
 
     /* do text replacement */
     pReplaced = NXDItextreplace(dict,pDefinition);
     if(!pReplaced)
     {
       return NX_ERROR;
     }
     
     /* call NXDgetdef */
     iRet = NXDgetdef(hFil,dict,GetCharArray(pReplaced),pData);
     DeleteDynString(pReplaced);
     return iRet;
  }

#line 2418 "nxdict.w"

/*------------------------------------------------------------------------*/

#line 1849 "nxdict.w"

  NXstatus NXDdeflink(NXhandle hFil, NXdict dict, 
                      char *pTarget, char *pVictim)
  {
     NXdict pDict;
     ParDat pParseT, pParseV;
     int iRet, i, iStat;
     NXlink sLink;

     pDict = NXDIAssert(dict);


#ifdef DEFDEBUG
     printf("Linking: %s\n",pVictim);
     printf("To: %s\n", pTarget);
#endif
     

     /* parse Victim  */
     pParseV.iMayCreate = 0;
     pParseV.pPtr = pVictim;
     pParseV.iDepth = 0;
     iRet = NXDIDefParse(hFil,pDict,&pParseV);
     if(iRet == NX_ERROR)
     {
         /* unwind and throw up */
          NXDIUnwind(hFil,pParseV.iDepth);
          return NX_ERROR;
     }
     /* get link data */
     if(pParseV.iTerminal == TERMSDS)
     {
       NXgetdataID(hFil,&sLink);
       iRet =  NXclosedata(hFil);
       if(iRet != NX_OK)
       {
         /* unwind and throw up */
          NXDIUnwind(hFil,pParseV.iDepth);
          return NX_ERROR;
       }
     }
     else if(pParseV.iTerminal == TERMVG)
     {
       NXgetgroupID(hFil,&sLink);
     }
     else
     {
        assert(0); /* serious programming error */
     }
     /* Unwind */
     iRet = NXDIUnwind(hFil,pParseV.iDepth);
     if(iRet != NX_OK)
     {
       return NX_ERROR;
     }

     /* parse Target */
     pParseT.iMayCreate = 1;
     pParseT.pPtr = pTarget;
     pParseT.iDepth = 0;
     iRet = NXDIDefParse(hFil,pDict,&pParseT);
     if(iRet == NX_ERROR)
     {
         /* unwind and throw up */
          NXDIUnwind(hFil,pParseT.iDepth);
          return NX_ERROR;
     }
     /* check it being a vGroup! */
     if(pParseT.iTerminal != TERMVG)
     {
       NXIReportError(NXpData,"ERROR: can link only into a vGroup");
       NXDIUnwind(hFil,pParseT.iDepth);
       return NX_ERROR;
     }
    
     /* link, finally */
     iRet = NXmakelink(hFil,&sLink);
     /* Unwind anyway */
     iStat = NXDIUnwind(hFil,pParseT.iDepth);
     if(iStat != NX_OK)
     {
       return NX_ERROR;
     }
     return iStat;
  }
/*--------------------------------------------------------------------------*/
  NXstatus NXDaliaslink(NXhandle hFil, NXdict dict, 
                      char *pTarget, char *pVictim)
  {
    char pTargetDef[1024], pVictimDef[1024];
    int iRet;
    NXdict pDict;
    pDynString pRep1 = NULL, pRep2 = NULL;

     pDict = NXDIAssert(dict);
     
     /* get Target Definition String  */
     iRet = NXDget(pDict,pTarget,pTargetDef,1023);
     if(iRet != NX_OK)
     {
        sprintf(pTargetDef,"ERROR: alias %s not recognized",pTarget);
        NXIReportError(NXpData,pTargetDef);
        return NX_ERROR;
     }       

     /* get Victim definition string */
     iRet = NXDget(pDict,pVictim,pVictimDef,1023);
     if(iRet != NX_OK)
     {
        sprintf(pTargetDef,"ERROR: alias %s not recognized",pTarget);
        NXIReportError(NXpData,pTargetDef);
        return NX_ERROR;
     }

     /* do replacements */
     pRep1 = NXDItextreplace(dict,pTargetDef);
     pRep2 = NXDItextreplace(dict,pVictimDef);
     if( (!pRep1) || (!pRep2) )
     {
        if(pRep1)
           DeleteDynString(pRep1);
        if(pRep2)
           DeleteDynString(pRep2);
        return NX_ERROR;
     }

     /* call NXdeflin */
     iRet = NXDdeflink(hFil,pDict,GetCharArray(pRep1),GetCharArray(pRep2));
     DeleteDynString(pRep1);
     DeleteDynString(pRep2);
     return iRet;
  }


#line 2420 "nxdict.w"

/*-----------------------------------------------------------------------*/

#line 1986 "nxdict.w"

/*-------------------------------------------------------------------------*/
  static void SNXFormatTime(char *pBuffer, int iBufLen)
  {
     time_t iDate;
     struct tm *psTime;

     /* make time string */
     iDate = time(NULL);
     psTime = localtime(&iDate);
     memset(pBuffer,0,iBufLen);         
     strftime(pBuffer,iBufLen,"%Y-%d-%m %H:%M:%S",psTime);
  }  
/*--------------------------------------------------------------------------*/
   NXstatus NXUwriteglobals(NXhandle pFile, 
                            char *filename,
                            char *owner,
                            char *adress,
                            char *phone,
                            char *email,
                            char *fax,
                            char *instrument)
   {
     char pBueffel[512];
     int iStat;
     
     /* store global attributes */
     iStat = NXputattr(pFile,"file_name",filename, 
                       strlen(filename)+1,DFNT_INT8);
     if(iStat == NX_ERROR)
     {
       return NX_ERROR;
     }
     
     /* write creation time */
     SNXFormatTime(pBueffel,512);
     iStat = NXputattr(pFile,"file_time",pBueffel,
                       strlen(pBueffel)+1,DFNT_INT8);
     if(iStat == NX_ERROR)
     {
       return NX_ERROR;
     }

     /* instrument name */
     iStat = NXputattr(pFile,"instrument",instrument,
                       strlen(instrument)+1,DFNT_INT8);
     if(iStat == NX_ERROR)
     {
       return iStat;
     }

     /* owner */
     iStat = NXputattr(pFile,"owner",owner,
                       strlen(owner)+1,DFNT_INT8);
     if(iStat == NX_ERROR)
     {
       return iStat;
     }

     /* Adress */
     iStat = NXputattr(pFile,"owner_adress",adress,
                       strlen(adress)+1,DFNT_INT8);
     if(iStat == NX_ERROR)
     {
        return iStat;
     }

     /* phone */
     iStat = NXputattr(pFile,"owner_telephone_number",phone,
                       strlen(phone)+1,DFNT_INT8);
     if(iStat == NX_ERROR)
     {
        return iStat;
     }

     /* fax */
     iStat = NXputattr(pFile,"owner_fax_number",fax,
                       strlen(fax)+1,DFNT_INT8);
     if(iStat == NX_ERROR)
     {
       return iStat;
     }

     /* email */
     iStat = NXputattr(pFile,"owner_email",email,
                       strlen(email)+1,DFNT_INT8);
     if(iStat == NX_ERROR)
     {
       return iStat;
     }
     return NX_OK;
   }

#line 2422 "nxdict.w"

/*-----------------------------------------------------------------------*/

#line 2082 "nxdict.w"

   NXstatus NXUentergroup(NXhandle hFil, char *name, char *class)
   {
       void (*ErrFunc)(void *pData, char *pErr); 
       int iRet;

       /* ACTION, first install dummy error handler */
       ErrFunc = NXIReportError;
       NXMSetError(NXpData, DummyError);
 
       /* try opening vGroup */
       iRet = NXopengroup(hFil, name, class);
       NXMSetError(NXpData,ErrFunc);
       if(iRet == NX_OK)
       {
         return NX_OK;
       } 
       else
       {
          /* we need to create it */
            iRet = NXmakegroup(hFil,name,class);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 return iRet;
            }
            iRet = NXopengroup(hFil,name,class);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 return iRet;
            }
       }
       return NX_OK;
   }

#line 2424 "nxdict.w"

/*-----------------------------------------------------------------------*/

#line 2119 "nxdict.w"

   NXstatus NXUenterdata(NXhandle hFil, char *label, int datatype,
                         int rank, int dim[], char *pUnits)
   {
       void (*ErrFunc)(void *pData, char *pErr); 
       int iRet;

       /* ACTION, first install dummy error handler */
       ErrFunc = NXIReportError;
       NXMSetError(NXpData, DummyError);
 
       /* try opening SDS */
       iRet = NXopendata(hFil, label);
       NXMSetError(NXpData,ErrFunc);
       if(iRet == NX_OK)
       {
         return NX_OK;
       } 
       else
       {
          /* we need to create it */
            iRet = NXmakedata(hFil,label, datatype, rank,dim);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 return iRet;
            }
            iRet = NXopendata(hFil,label);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 return iRet;
            }
            iRet = NXputattr(hFil, "Units",pUnits,
                             strlen(pUnits) + 1,DFNT_INT8);
            if(iRet != NX_OK)
            { 
                 /* a comment on this one has already been written! */
                 return iRet;
            }
       }
       return NX_OK;
   }

#line 2426 "nxdict.w"

/*-----------------------------------------------------------------------*/

#line 2165 "nxdict.w"

   NXstatus NXUallocSDS(NXhandle hFil, void **pData)
   {
      int iDIM[MAX_VAR_DIMS];
      int iRank,iType;
      int iRet, i;
      long lLength;

      /* get info */
      iRet = NXgetinfo(hFil,&iRank, iDIM, &iType);
      if(iRet != NX_OK)
      {
        return iRet;
      }      

      /* calculate Size */
      lLength = iDIM[0];
      for(i = 1; i < iRank; i++)
      {
        lLength *= iDIM[i];
      }
      switch(iType)
      {
        case DFNT_FLOAT32:
             lLength *= sizeof(float32);
             break;
        case DFNT_FLOAT64:
             lLength *= sizeof(float64);
             break;
        case DFNT_INT8:
             lLength *= sizeof(int8);
             break;
        case DFNT_UINT8:
             lLength *= sizeof(uint8);
             break;
        case DFNT_INT16:
             lLength *= sizeof(int16);
             break;
        case DFNT_UINT16:
             lLength *= sizeof(uint16);
             break;
        case DFNT_INT32:
             lLength *= sizeof(int32);
             break;
        case DFNT_UINT32:
             lLength *= sizeof(uint32);
             break;
        default:
             NXIReportError(NXpData,"ERROR: Internal: number type not recoginized");
             return NX_ERROR;
      }

      /* time to malloc */
      *pData = NULL;
      *pData = malloc(lLength);
      if(*pData == NULL)
      {
        NXIReportError(NXpData,"ERROR: memory exhausted in NXUallocSDS");
        return NX_ERROR;
      }
      return NX_OK;
   }

#line 2428 "nxdict.w"

/*----------------------------------------------------------------------*/

#line 2229 "nxdict.w"

   NXstatus NXUfreeSDS(void **pData)
   {
      free(*pData);
      *pData = NULL;
      return NX_OK;
   }

#line 2430 "nxdict.w"

