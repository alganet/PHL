/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXINT_H__
#define __SXINT_H__

/* Include public API */
#include "sx.h"

/* Symisc Standard types */
#if !defined(SYMISC_STD_TYPES)
#define SYMISC_STD_TYPES
typedef signed char        sxi8; /* signed char */
typedef unsigned char      sxu8; /* unsigned char */
typedef signed short int   sxi16; /* 16 bits(2 bytes) signed integer */
typedef unsigned short int sxu16; /* 16 bits(2 bytes) unsigned integer */
typedef int                sxi32; /* 32 bits(4 bytes) integer */
typedef unsigned int       sxu32; /* 32 bits(4 bytes) unsigned integer */
typedef long               sxptr;
typedef unsigned long      sxuptr;
typedef long               sxlong;
typedef unsigned long      sxulong;
typedef sxi32              sxofft;
typedef sxi64              sxofft64;
typedef long double	       sxlongreal;
typedef double             sxreal;
#define SXI8_HIGH       0x7F
#define SXU8_HIGH       0xFF
#define SXI16_HIGH      0x7FFF
#define SXU16_HIGH      0xFFFF
#define SXI32_HIGH      0x7FFFFFFF
#define SXU32_HIGH      0xFFFFFFFF
#define SXI64_HIGH      0x7FFFFFFFFFFFFFFF
#define SXU64_HIGH      0xFFFFFFFFFFFFFFFF
#if !defined(TRUE)
#define TRUE 1
#endif
#if !defined(FALSE)
#define FALSE 0
#endif
/*
 * The following macros are used to cast pointers to integers and
 * integers to pointers.
 */
#if defined(__PTRDIFF_TYPE__)
# define SX_INT_TO_PTR(X)  ((void*)(__PTRDIFF_TYPE__)(X))
# define SX_PTR_TO_INT(X)  ((int)(__PTRDIFF_TYPE__)(X))
#elif !defined(__GNUC__)
# define SX_INT_TO_PTR(X)  ((void*)&((char*)0)[X])
# define SX_PTR_TO_INT(X)  ((int)(((char*)X)-(char*)0))
#else
# define SX_INT_TO_PTR(X)  ((void*)(X))
# define SX_PTR_TO_INT(X)  ((int)(X))
#endif
#define SXMIN(a,b)  ((a < b) ? (a) : (b))
#define SXMAX(a,b)  ((a < b) ? (b) : (a))
#endif /* SYMISC_STD_TYPES */
/* Symisc Run-time API private definitions */
#if !defined(SYMISC_PRIVATE_DEFS)
#define SYMISC_PRIVATE_DEFS

typedef sxi32 (*ProcRawStrCmp)(const SyString *,const SyString *);
#define SyStringData(RAW)	((RAW)->zString)
#define SyStringLength(RAW)	((RAW)->nByte)
#define SyStringInitFromBuf(RAW,ZBUF,NLEN){\
	(RAW)->zString 	= (const char *)ZBUF;\
	(RAW)->nByte	= (sxu32)(NLEN);\
}
#define SyStringUpdatePtr(RAW,NBYTES){\
	if( NBYTES > (RAW)->nByte ){\
		(RAW)->nByte = 0;\
	}else{\
		(RAW)->zString += NBYTES;\
		(RAW)->nByte -= NBYTES;\
	}\
}
#define SyStringDupPtr(RAW1,RAW2)\
	(RAW1)->zString = (RAW2)->zString;\
	(RAW1)->nByte = (RAW2)->nByte;

#define SyStringTrimLeadingChar(RAW,CHAR)\
	while((RAW)->nByte > 0 && (RAW)->zString[0] == CHAR ){\
			(RAW)->zString++;\
			(RAW)->nByte--;\
	}
#define SyStringTrimTrailingChar(RAW,CHAR)\
	while((RAW)->nByte > 0 && (RAW)->zString[(RAW)->nByte - 1] == CHAR){\
		(RAW)->nByte--;\
	}
#define SyStringCmp(RAW1,RAW2,xCMP)\
	(((RAW1)->nByte == (RAW2)->nByte) ? xCMP((RAW1)->zString,(RAW2)->zString,(RAW2)->nByte) : (sxi32)((RAW1)->nByte - (RAW2)->nByte))

#define SyStringCmp2(RAW1,RAW2,xCMP)\
	(((RAW1)->nByte >= (RAW2)->nByte) ? xCMP((RAW1)->zString,(RAW2)->zString,(RAW2)->nByte) : (sxi32)((RAW2)->nByte - (RAW1)->nByte))

#define SyStringCharCmp(RAW,CHAR) \
	(((RAW)->nByte == sizeof(char)) ? ((RAW)->zString[0] == CHAR ? 0 : CHAR - (RAW)->zString[0]) : ((RAW)->zString[0] == CHAR ? 0 : (RAW)->nByte - sizeof(char)))

#define SX_ADDR(PTR)    ((sxptr)PTR)
#define SX_ARRAYSIZE(X) (sizeof(X)/sizeof(X[0]))
#define SXUNUSED(P)	(P = 0)
#define	SX_EMPTY(PTR)   (PTR == 0)
#define SX_EMPTY_STR(STR) (STR == 0 || STR[0] == 0 )
typedef struct SyMemBackend SyMemBackend;
typedef struct SyBlob SyBlob;
typedef struct SySet SySet;
/* Standard function signatures */
typedef sxi32 (*ProcCmp)(const void *,const void *,sxu32);
typedef sxi32 (*ProcPatternMatch)(const char *,sxu32,const char *,sxu32,sxu32 *);
typedef sxi32 (*ProcSearch)(const void *,sxu32,const void *,sxu32,ProcCmp,sxu32 *);
typedef sxu32 (*ProcHash)(const void *,sxu32);
typedef sxi32 (*ProcHashSum)(const void *,sxu32,unsigned char *,sxu32);
typedef sxi32 (*ProcSort)(void *,sxu32,sxu32,ProcCmp);
#define MACRO_LIST_PUSH(Head,Item)\
	Item->pNext = Head;\
	Head = Item;
#define MACRO_LD_PUSH(Head,Item)\
	if( Head == 0 ){\
		Head = Item;\
	}else{\
		Item->pNext = Head;\
		Head->pPrev = Item;\
		Head = Item;\
	}
#define MACRO_LD_REMOVE(Head,Item)\
	if( Head == Item ){\
		Head = Head->pNext;\
	}\
	if( Item->pPrev ){ Item->pPrev->pNext = Item->pNext;}\
	if( Item->pNext ){ Item->pNext->pPrev = Item->pPrev;}
/*
 * A generic dynamic set.
 */
struct SySet
{
	SyMemBackend *pAllocator; /* Memory backend */
	void *pBase;              /* Base pointer */
	sxu32 nUsed;              /* Total number of used slots  */
	sxu32 nSize;              /* Total number of available slots */
	sxu32 eSize;              /* Size of a single slot */
	sxu32 nCursor;	          /* Loop cursor */
	void *pUserData;          /* User private data associated with this container */
};
#define SySetBasePtr(S)           ((S)->pBase)
#define SySetBasePtrJump(S,OFFT)  (&((char *)(S)->pBase)[OFFT*(S)->eSize])
#define SySetUsed(S)              ((S)->nUsed)
#define SySetSize(S)              ((S)->nSize)
#define SySetElemSize(S)          ((S)->eSize)
#define SySetCursor(S)            ((S)->nCursor)
#define SySetGetAllocator(S)      ((S)->pAllocator)
#define SySetSetUserData(S,DATA)  ((S)->pUserData = DATA)
#define SySetGetUserData(S)       ((S)->pUserData)
/*
 * A variable length containers for generic data.
 */
struct SyBlob
{
	SyMemBackend *pAllocator; /* Memory backend */
	void   *pBlob;	          /* Base pointer */
	sxu32  nByte;	          /* Total number of used bytes */
	sxu32  mByte;	          /* Total number of available bytes */
	sxu32  nFlags;	          /* Blob internal flags,see below */
};
#define SXBLOB_LOCKED	0x01	/* Blob is locked [i.e: Cannot auto grow] */
#define SXBLOB_STATIC	0x02	/* Not allocated from heap   */
#define SXBLOB_RDONLY   0x04    /* Read-Only data */

#define SyBlobFreeSpace(BLOB)	 ((BLOB)->mByte - (BLOB)->nByte)
#define SyBlobLength(BLOB)	     ((BLOB)->nByte)
#define SyBlobData(BLOB)	     ((BLOB)->pBlob)
#define SyBlobCurData(BLOB)	     ((void*)(&((char*)(BLOB)->pBlob)[(BLOB)->nByte]))
#define SyBlobDataAt(BLOB,OFFT)	 ((void *)(&((char *)(BLOB)->pBlob)[OFFT]))
#define SyBlobGetAllocator(BLOB) ((BLOB)->pAllocator)

#define SXMEM_POOL_INCR			3
#define SXMEM_POOL_NBUCKETS		12
#define SXMEM_BACKEND_MAGIC	0xBAC3E67D
#define SXMEM_BACKEND_CORRUPT(BACKEND)	(BACKEND == 0 || BACKEND->nMagic != SXMEM_BACKEND_MAGIC)

#define SXMEM_BACKEND_RETRY	3
/* A memory backend subsystem is defined by an instance of the following structures */
typedef union SyMemHeader SyMemHeader;
typedef struct SyMemBlock SyMemBlock;
struct SyMemBlock
{
	SyMemBlock *pNext,*pPrev; /* Chain of allocated memory blocks */
#ifdef UNTRUST
	sxu32 nGuard;             /* magic number associated with each valid block,so we
						   * can detect misuse.
						   */
#endif
};
/*
 * Header associated with each valid memory pool block.
 */
union SyMemHeader
{
	SyMemHeader *pNext; /* Next chunk of size 1 << (nBucket + SXMEM_POOL_INCR) in the list */
	sxu32 nBucket;      /* Bucket index in aPool[] */
};
struct SyMemBackend
{
	const SyMutexMethods *pMutexMethods; /* Mutex methods */
	const SyMemMethods *pMethods;  /* Memory allocation methods */
	SyMemBlock *pBlocks;           /* List of valid memory blocks */
	sxu32 nBlock;                  /* Total number of memory blocks allocated so far */
	ProcMemError xMemError;        /* Out-of memory callback */
	void *pUserData;               /* First arg to xMemError() */
	SyMutex *pMutex;               /* Per instance mutex */
	sxu32 nMagic;                  /* Sanity check against misuse */
	SyMemHeader *apPool[SXMEM_POOL_NBUCKETS+SXMEM_POOL_INCR]; /* Pool of memory chunks */
};
/* Mutex types */
#define SXMUTEX_TYPE_FAST	1
#define SXMUTEX_TYPE_RECURSIVE	2
#define SXMUTEX_TYPE_STATIC_1	3
#define SXMUTEX_TYPE_STATIC_2	4
#define SXMUTEX_TYPE_STATIC_3	5
#define SXMUTEX_TYPE_STATIC_4	6
#define SXMUTEX_TYPE_STATIC_5	7
#define SXMUTEX_TYPE_STATIC_6	8

#define SyMutexGlobalInit(METHOD){\
	if( (METHOD)->xGlobalInit ){\
	(METHOD)->xGlobalInit();\
	}\
}
#define SyMutexGlobalRelease(METHOD){\
	if( (METHOD)->xGlobalRelease ){\
	(METHOD)->xGlobalRelease();\
	}\
}
#define SyMutexNew(METHOD,TYPE)			(METHOD)->xNew(TYPE)
#define SyMutexRelease(METHOD,MUTEX){\
	if( MUTEX && (METHOD)->xRelease ){\
		(METHOD)->xRelease(MUTEX);\
	}\
}
#define SyMutexEnter(METHOD,MUTEX){\
	if( MUTEX ){\
	(METHOD)->xEnter(MUTEX);\
	}\
}
#define SyMutexTryEnter(METHOD,MUTEX){\
	if( MUTEX && (METHOD)->xTryEnter ){\
	(METHOD)->xTryEnter(MUTEX);\
	}\
}
#define SyMutexLeave(METHOD,MUTEX){\
	if( MUTEX ){\
	(METHOD)->xLeave(MUTEX);\
	}\
}
/* Comparison,byte swap,byte copy macros */
#define SX_MACRO_FAST_CMP(X1,X2,SIZE,RC){\
	register unsigned char *r1 = (unsigned char *)X1;\
	register unsigned char *r2 = (unsigned char *)X2;\
	register sxu32 LEN = SIZE;\
	for(;;){\
	  if( !LEN ){ break; }if( r1[0] != r2[0] ){ break; } r1++; r2++; LEN--;\
	  if( !LEN ){ break; }if( r1[0] != r2[0] ){ break; } r1++; r2++; LEN--;\
	  if( !LEN ){ break; }if( r1[0] != r2[0] ){ break; } r1++; r2++; LEN--;\
	  if( !LEN ){ break; }if( r1[0] != r2[0] ){ break; } r1++; r2++; LEN--;\
	}\
	RC = !LEN ? 0 : r1[0] - r2[0];\
}
#define	SX_MACRO_FAST_MEMCPY(SRC,DST,SIZ){\
	register unsigned char *xSrc = (unsigned char *)SRC;\
	register unsigned char *xDst = (unsigned char *)DST;\
	register sxu32 xLen = SIZ;\
	for(;;){\
	    if( !xLen ){ break; }xDst[0] = xSrc[0]; xDst++; xSrc++; --xLen;\
		if( !xLen ){ break; }xDst[0] = xSrc[0]; xDst++; xSrc++; --xLen;\
		if( !xLen ){ break; }xDst[0] = xSrc[0]; xDst++; xSrc++; --xLen;\
		if( !xLen ){ break; }xDst[0] = xSrc[0]; xDst++; xSrc++; --xLen;\
	}\
}
#define SX_MACRO_BYTE_SWAP(X,Y,Z){\
	register unsigned char *s = (unsigned char *)X;\
	register unsigned char *d = (unsigned char *)Y;\
	sxu32	ZLong = Z;  \
	sxi32 c; \
	for(;;){\
	  if(!ZLong){ break; } c = s[0] ; s[0] = d[0]; d[0] = (unsigned char)c; s++; d++; --ZLong;\
	  if(!ZLong){ break; } c = s[0] ; s[0] = d[0]; d[0] = (unsigned char)c; s++; d++; --ZLong;\
	  if(!ZLong){ break; } c = s[0] ; s[0] = d[0]; d[0] = (unsigned char)c; s++; d++; --ZLong;\
	  if(!ZLong){ break; } c = s[0] ; s[0] = d[0]; d[0] = (unsigned char)c; s++; d++; --ZLong;\
	}\
}
#define SX_MSEC_PER_SEC	(1000)			/* Millisec per seconds */
#define SX_USEC_PER_SEC	(1000000)		/* Microsec per seconds */
#define SX_NSEC_PER_SEC	(1000000000)	/* Nanosec per seconds */
#endif /* SYMISC_PRIVATE_DEFS */
/* Symisc Run-time API auxiliary definitions */
#if !defined(SYMISC_PRIVATE_AUX_DEFS)
#define SYMISC_PRIVATE_AUX_DEFS

typedef struct SyHashEntry_Pr SyHashEntry_Pr;
typedef struct SyHashEntry SyHashEntry;
typedef struct SyHash SyHash;
/*
 * Each public hashtable entry is represented by an instance
 * of the following structure.
 */
struct SyHashEntry
{
	const void *pKey; /* Hash key */
	sxu32 nKeyLen;    /* Key length */
	void *pUserData;  /* User private data */
};
#define SyHashEntryGetUserData(ENTRY) ((ENTRY)->pUserData)
#define SyHashEntryGetKey(ENTRY)      ((ENTRY)->pKey)
/* Each active hashtable is identified by an instance of the following structure */
struct SyHash
{
	SyMemBackend *pAllocator;         /* Memory backend */
	ProcHash xHash;                   /* Hash function */
	ProcCmp xCmp;                     /* Comparison function */
	SyHashEntry_Pr *pList,*pCurrent;  /* Linked list of hash entries user for linear traversal */
	sxu32 nEntry;                     /* Total number of entries */
	SyHashEntry_Pr **apBucket;        /* Hash buckets */
	sxu32 nBucketSize;                /* Current bucket size */
};
#define SXHASH_BUCKET_SIZE 16 /* Initial bucket size: must be a power of two */
#define SXHASH_FILL_FACTOR 3
/* Hash access macro */
#define SyHashFunc(HASH)		((HASH)->xHash)
#define SyHashCmpFunc(HASH)		((HASH)->xCmp)
#define SyHashTotalEntry(HASH)	((HASH)->nEntry)
#define SyHashGetPool(HASH)		((HASH)->pAllocator)
/*
 * An instance of the following structure define a single context
 * for an Pseudo Random Number Generator.
 *
 * Nothing in this file or anywhere else in the library does any kind of
 * encryption.  The RC4 algorithm is being used as a PRNG (pseudo-random
 * number generator) not as an encryption device.
 * This implementation is taken from the SQLite3 source tree.
 */
typedef struct SyPRNGCtx SyPRNGCtx;
struct SyPRNGCtx
{
    sxu8 i,j;				/* State variables */
    unsigned char s[256];   /* State variables */
	sxu16 nMagic;			/* Sanity check */
 };
typedef sxi32 (*ProcRandomSeed)(void *,unsigned int,void *);
/* High resolution timer.*/
typedef struct sytime sytime;
struct sytime
{
	long tm_sec;	/* seconds */
	long tm_usec;	/* microseconds */
};
/* Forward declaration */
typedef struct SyStream SyStream;
typedef struct SyToken  SyToken;
typedef struct SyLex    SyLex;
/*
 * Tokenizer callback signature.
 */
typedef sxi32 (*ProcTokenizer)(SyStream *,SyToken *,void *,void *);
/*
 * Each token in the input is represented by an instance
 * of the following structure.
 */
struct SyToken
{
	SyString sData;  /* Token text and length */
	sxu32 nType;     /* Token type */
	sxu32 nLine;     /* Token line number */
	void *pUserData; /* User private data associated with this token */
};
/*
 * During tokenization, information about the state of the input
 * stream is held in an instance of the following structure.
 */
struct SyStream
{
	const unsigned char *zInput; /* Complete text of the input */
	const unsigned char *zText; /* Current input we are processing */
	const unsigned char *zEnd; /* End of input marker */
	sxu32  nLine; /* Total number of processed lines */
	sxu32  nIgn; /* Total number of ignored tokens */
	SySet *pSet; /* Token containers */
};
/*
 * Each lexer is represented by an instance of the following structure.
 */
struct SyLex
{
	SyStream sStream;         /* Input stream */
	ProcTokenizer xTokenizer; /* Tokenizer callback */
	void * pUserData;         /* Third argument to xTokenizer() */
	SySet *pTokenSet;         /* Token set */
};
#define SyLexTotalToken(LEX)    SySetTotalEntry(&(LEX)->aTokenSet)
#define SyLexTotalLines(LEX)    ((LEX)->sStream.nLine)
#define SyLexTotalIgnored(LEX)  ((LEX)->sStream.nIgn)
#define XLEX_IN_LEN(STREAM)     (sxu32)(STREAM->zEnd - STREAM->zText)
#endif /* SYMISC_PRIVATE_AUX_DEFS */
/*
** Notes on UTF-8 (According to SQLite3 authors):
**
**   Byte-0    Byte-1    Byte-2    Byte-3    Value
**  0xxxxxxx                                 00000000 00000000 0xxxxxxx
**  110yyyyy  10xxxxxx                       00000000 00000yyy yyxxxxxx
**  1110zzzz  10yyyyyy  10xxxxxx             00000000 zzzzyyyy yyxxxxxx
**  11110uuu  10uuzzzz  10yyyyyy  10xxxxxx   000uuuuu zzzzyyyy yyxxxxxx
**
*/
/*
** Assuming zIn points to the first byte of a UTF-8 character,
** advance zIn to point to the first byte of the next UTF-8 character.
*/
#define SX_JMP_UTF8(zIn,zEnd)\
	while(zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){ zIn++; }
#define SX_WRITE_UTF8(zOut, c) {                       \
  if( c<0x00080 ){                                     \
    *zOut++ = (sxu8)(c&0xFF);                          \
  }else if( c<0x00800 ){                               \
    *zOut++ = 0xC0 + (sxu8)((c>>6)&0x1F);              \
    *zOut++ = 0x80 + (sxu8)(c & 0x3F);                 \
  }else if( c<0x10000 ){                               \
    *zOut++ = 0xE0 + (sxu8)((c>>12)&0x0F);             \
    *zOut++ = 0x80 + (sxu8)((c>>6) & 0x3F);            \
    *zOut++ = 0x80 + (sxu8)(c & 0x3F);                 \
  }else{                                               \
    *zOut++ = 0xF0 + (sxu8)((c>>18) & 0x07);           \
    *zOut++ = 0x80 + (sxu8)((c>>12) & 0x3F);           \
    *zOut++ = 0x80 + (sxu8)((c>>6) & 0x3F);            \
    *zOut++ = 0x80 + (sxu8)(c & 0x3F);                 \
  }                                                    \
}
/* Rely on the standard ctype */
#include <ctype.h>
#define SyToUpper(c) toupper(c)
#define SyToLower(c) tolower(c)
#define SyisUpper(c) isupper(c)
#define SyisLower(c) islower(c)
#define SyisSpace(c) isspace(c)
#define SyisBlank(c) isspace(c)
#define SyisAlpha(c) isalpha(c)
#define SyisDigit(c) isdigit(c)
#define SyisHex(c)	 isxdigit(c)
#define SyisPrint(c) isprint(c)
#define SyisPunct(c) ispunct(c)
#define SyisSpec(c)	 iscntrl(c)
#define SyisCtrl(c)	 iscntrl(c)
#define SyisAscii(c) isascii(c)
#define SyisAlphaNum(c) isalnum(c)
#define SyisGraph(c)     isgraph(c)
#define SyDigToHex(c)    "0123456789ABCDEF"[c & 0x0F]
#define SyDigToInt(c)     ((c < 0xc0 && SyisDigit(c))? (c - '0') : 0 )
#define SyCharToUpper(c)  ((c < 0xc0 && SyisLower(c))? SyToUpper(c) : c)
#define SyCharToLower(c)  ((c < 0xc0 && SyisUpper(c))? SyToLower(c) : c)
/* Remove white space/NUL byte from a raw string */
#define SyStringLeftTrim(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[0] < 0xc0 && SyisSpace((RAW)->zString[0])){\
		(RAW)->nByte--;\
		(RAW)->zString++;\
	}
#define SyStringLeftTrimSafe(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[0] < 0xc0 && ((RAW)->zString[0] == 0 || SyisSpace((RAW)->zString[0]))){\
		(RAW)->nByte--;\
		(RAW)->zString++;\
	}
#define SyStringRightTrim(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[(RAW)->nByte - 1] < 0xc0  && SyisSpace((RAW)->zString[(RAW)->nByte - 1])){\
		(RAW)->nByte--;\
	}
#define SyStringRightTrimSafe(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[(RAW)->nByte - 1] < 0xc0  && \
	(( RAW)->zString[(RAW)->nByte - 1] == 0 || SyisSpace((RAW)->zString[(RAW)->nByte - 1]))){\
		(RAW)->nByte--;\
	}

#define SyStringFullTrim(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[0] < 0xc0  && SyisSpace((RAW)->zString[0])){\
		(RAW)->nByte--;\
		(RAW)->zString++;\
	}\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[(RAW)->nByte - 1] < 0xc0  && SyisSpace((RAW)->zString[(RAW)->nByte - 1])){\
		(RAW)->nByte--;\
	}
#define SyStringFullTrimSafe(RAW)\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[0] < 0xc0  && \
          ( (RAW)->zString[0] == 0 || SyisSpace((RAW)->zString[0]))){\
		(RAW)->nByte--;\
		(RAW)->zString++;\
	}\
	while((RAW)->nByte > 0 && (unsigned char)(RAW)->zString[(RAW)->nByte - 1] < 0xc0  && \
                   ( (RAW)->zString[(RAW)->nByte - 1] == 0 || SyisSpace((RAW)->zString[(RAW)->nByte - 1]))){\
		(RAW)->nByte--;\
	}
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * An XML raw text,CDATA,tag name and son is parsed out and stored
 * in an instance of the following structure.
 */
typedef struct SyXMLRawStr SyXMLRawStr;
struct SyXMLRawStr
{
	const char *zString; /* Raw text [UTF-8 ENCODED EXCEPT CDATA] [NOT NULL TERMINATED] */
	sxu32 nByte; /* Text length */
	sxu32 nLine; /* Line number this text occurs */
};
/*
 * Event callback signatures.
 */
typedef sxi32 (*ProcXMLStartTagHandler)(SyXMLRawStr * ,SyXMLRawStr *,sxu32,SyXMLRawStr *,void *);
typedef sxi32 (*ProcXMLTextHandler)(SyXMLRawStr *,void *);
typedef sxi32 (*ProcXMLEndTagHandler)(SyXMLRawStr * ,SyXMLRawStr *,void *);
typedef sxi32 (*ProcXMLPIHandler)(SyXMLRawStr *,SyXMLRawStr *,void *);
typedef sxi32 (*ProcXMLDoctypeHandler)(SyXMLRawStr *,void *);
typedef sxi32 (*ProcXMLSyntaxErrorHandler)(const char *,int,SyToken *,void *);
typedef sxi32 (*ProcXMLStartDocument)(void *);
typedef sxi32 (*ProcXMLNameSpaceStart)(SyXMLRawStr *,SyXMLRawStr *,void *);
typedef sxi32 (*ProcXMLNameSpaceEnd)(SyXMLRawStr *,void *);
typedef sxi32 (*ProcXMLEndDocument)(void *);
/* XML processing control flags */
#define SXML_ENABLE_NAMESPACE	    0x01 /* Parse XML with namespace support enbaled */
#define SXML_ENABLE_QUERY		    0x02 /* Not used */
#define SXML_OPTION_CASE_FOLDING    0x04 /* Controls whether case-folding is enabled for this XML parser */
#define SXML_OPTION_SKIP_TAGSTART   0x08 /* Specify how many characters should be skipped in the beginning of a tag name.*/
#define SXML_OPTION_SKIP_WHITE      0x10 /* Whether to skip values consisting of whitespace characters. */
#define SXML_OPTION_TARGET_ENCODING 0x20 /* Default encoding: UTF-8 */
/* XML error codes */
enum xml_err_code{
    SXML_ERROR_NONE = 1,
    SXML_ERROR_NO_MEMORY,
    SXML_ERROR_SYNTAX,
    SXML_ERROR_NO_ELEMENTS,
    SXML_ERROR_INVALID_TOKEN,
    SXML_ERROR_UNCLOSED_TOKEN,
    SXML_ERROR_PARTIAL_CHAR,
    SXML_ERROR_TAG_MISMATCH,
    SXML_ERROR_DUPLICATE_ATTRIBUTE,
    SXML_ERROR_JUNK_AFTER_DOC_ELEMENT,
    SXML_ERROR_PARAM_ENTITY_REF,
    SXML_ERROR_UNDEFINED_ENTITY,
    SXML_ERROR_RECURSIVE_ENTITY_REF,
    SXML_ERROR_ASYNC_ENTITY,
    SXML_ERROR_BAD_CHAR_REF,
    SXML_ERROR_BINARY_ENTITY_REF,
    SXML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF,
    SXML_ERROR_MISPLACED_XML_PI,
    SXML_ERROR_UNKNOWN_ENCODING,
    SXML_ERROR_INCORRECT_ENCODING,
    SXML_ERROR_UNCLOSED_CDATA_SECTION,
    SXML_ERROR_EXTERNAL_ENTITY_HANDLING
};
/* Each active XML SAX parser is represented by an instance
 * of the following structure.
 */
typedef struct SyXMLParser SyXMLParser;
struct SyXMLParser
{
	SyMemBackend *pAllocator; /* Memory backend */
	void *pUserData;          /* User private data forwarded varbatim by the XML parser
						   * as the last argument to the users callbacks.
						       */
	SyHash hns;               /* Namespace hashtable */
	SySet sToken;             /* XML tokens */
	SyLex sLex;               /* Lexical analyzer */
	sxi32 nFlags;             /* Control flags */
	/* User callbacks */
	ProcXMLStartTagHandler    xStartTag;     /* Start element handler */
	ProcXMLEndTagHandler      xEndTag;       /* End element handler */
	ProcXMLTextHandler        xRaw;          /* Raw text/CDATA handler   */
	ProcXMLDoctypeHandler     xDoctype;      /* DOCTYPE handler */
	ProcXMLPIHandler          xPi;           /* Processing instruction (PI) handler*/
	ProcXMLSyntaxErrorHandler xError;        /* Error handler */
	ProcXMLStartDocument      xStartDoc;     /* StartDoc handler */
	ProcXMLEndDocument        xEndDoc;       /* EndDoc handler */
	ProcXMLNameSpaceStart   xNameSpace;    /* Namespace declaration handler  */
	ProcXMLNameSpaceEnd       xNameSpaceEnd; /* End namespace declaration handler */
};
/*
 * --------------
 * Archive extractor:
 * --------------
 * Each open ZIP/TAR archive is identified by an instance of the following structure.
 * That is, a process can open one or more archives and manipulates them in thread safe
 * way by simply working with pointers to the following structure.
 * Each entry in the archive is remembered in a hashtable.
 * Lookup is very fast and entry with the same name are chained together.
 */
 typedef struct SyArchiveEntry SyArchiveEntry;
 typedef struct SyArchive SyArchive;
 struct SyArchive
 {
 	SyMemBackend	*pAllocator; /* Memory backend */
	SyArchiveEntry *pCursor;     /* Cursor for linear traversal of archive entries */
	SyArchiveEntry *pList;       /* Pointer to the List of the loaded archive */
	SyArchiveEntry **apHash;     /* Hashtable for archive entry */
	ProcRawStrCmp xCmp;          /* Hash comparison function */
	ProcHash xHash;              /* Hash Function */
	sxu32 nSize;        /* Hashtable size */
 	sxu32 nEntry;       /* Total number of entries in the zip/tar archive */
 	sxu32 nLoaded;      /* Total number of entries loaded in memory */
 	sxu32 nCentralOfft;	/* Central directory offset(ZIP only. Otherwise Zero) */
 	sxu32 nCentralSize;	/* Central directory size(ZIP only. Otherwise Zero) */
	void *pUserData;    /* Upper layer private data */
	sxu32 nMagic;       /* Sanity check */

 };
#define SXARCH_MAGIC	0xDEAD635A
#define SXARCH_INVALID(ARCH)            (ARCH == 0  || ARCH->nMagic != SXARCH_MAGIC)
#define SXARCH_ENTRY_INVALID(ENTRY)	    (ENTRY == 0 || ENTRY->nMagic != SXARCH_MAGIC)
#define SyArchiveHashFunc(ARCH)	        (ARCH)->xHash
#define SyArchiveCmpFunc(ARCH)	        (ARCH)->xCmp
#define SyArchiveUserData(ARCH)         (ARCH)->pUserData
#define SyArchiveSetUserData(ARCH,DATA) (ARCH)->pUserData = DATA
/*
 * Each loaded archive record is identified by an instance
 * of the following structure.
 */
 struct SyArchiveEntry
 {
 	sxu32 nByte;         /* Contents size before compression */
 	sxu32 nByteCompr;    /* Contents size after compression */
	sxu32 nReadCount;    /* Read counter */
 	sxu32 nCrc;          /* Contents CRC32  */
 	Sytm  sFmt;	         /* Last-modification time */
 	sxu32 nOfft;         /* Data offset. */
 	sxu16 nComprMeth;	 /* Compression method 0 == stored/8 == deflated and so on (see appnote.txt)*/
 	sxu16 nExtra;        /* Extra size if any */
 	SyString sFileName;  /* entry name & length */
 	sxu32 nDup;	/* Total number of entries with the same name */
	SyArchiveEntry *pNextHash,*pPrevHash; /* Hash collision chains */
 	SyArchiveEntry *pNextName;    /* Next entry with the same name */
	SyArchiveEntry *pNext,*pPrev; /* Next and previous entry in the list */
	sxu32 nHash;     /* Hash of the entry name */
 	void *pUserData; /* User data */
	sxu32 nMagic;    /* Sanity check */
 };
 /*
 * Extra flags for extending the file local header
 */
#define SXZIP_EXTRA_TIMESTAMP	0x001	/* Extended UNIX timestamp */
#endif /* PH7_DISABLE_BUILTIN_FUNC */
#ifndef PH7_DISABLE_HASH_FUNC
/* MD5 context */
typedef struct MD5Context MD5Context;
struct MD5Context {
 sxu32 buf[4];
 sxu32 bits[2];
 unsigned char in[64];
};
/* SHA1 context */
typedef struct SHA1Context SHA1Context;
struct SHA1Context {
  unsigned int state[5];
  unsigned int count[2];
  unsigned char buffer[64];
};
#endif /* PH7_DISABLE_HASH_FUNC */
#endif /* __SXINT_H__ */
