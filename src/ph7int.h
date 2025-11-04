/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __PH7INT_H__
#define __PH7INT_H__
/* Internal interface definitions for PH7. */
#ifdef PH7_AMALGAMATION
/* Marker for routines not intended for external use */
#define PH7_PRIVATE static
#else
#define PH7_PRIVATE
#include "ph7.h"
#endif 
#ifndef PH7_PI
/* Value of PI */
#define PH7_PI 3.1415926535898
#endif
/*
 * Constants for the largest and smallest possible 64-bit signed integers.
 * These macros are designed to work correctly on both 32-bit and 64-bit
 * compilers.
 */
#ifndef LARGEST_INT64
#define LARGEST_INT64  (0xffffffff|(((sxi64)0x7fffffff)<<32))
#endif
#ifndef SMALLEST_INT64
#define SMALLEST_INT64 (((sxi64)-1) - LARGEST_INT64)
#endif
/* Forward declaration of private structures */
typedef struct ph7_class_instance ph7_class_instance;
typedef struct ph7_foreach_info   ph7_foreach_info;
typedef struct ph7_foreach_step   ph7_foreach_step;
typedef struct ph7_hashmap_node   ph7_hashmap_node;
typedef struct ph7_hashmap        ph7_hashmap;
typedef struct ph7_class          ph7_class;
/* Symisc Standard types */
#if !defined(SYMISC_STD_TYPES)
#define SYMISC_STD_TYPES
#ifdef __WINNT__
/* Disable nuisance warnings on Borland compilers */
#if defined(__BORLANDC__)
#pragma warn -rch /* unreachable code */
#pragma warn -ccc /* Condition is always true or false */
#pragma warn -aus /* Assigned value is never used */
#pragma warn -csu /* Comparing signed and unsigned */
#pragma warn -spa /* Suspicious pointer arithmetic */
#endif
#endif
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
/* PH7 private declaration */
/*
 * Memory Objects.
 * Internally, the PH7 virtual machine manipulates nearly all PHP values
 * [i.e: string, int, float, resource, object, bool, null] as ph7_values structures.
 * Each ph7_values struct may cache multiple representations (string, integer etc.)
 * of the same value.
 */
struct ph7_value
{
	ph7_real rVal;      /* Real value */
	union{              
		sxi64 iVal;     /* Integer value */
		void *pOther;   /* Other values (Object, Array, Resource, Namespace, etc.) */
	}x;
	sxi32 iFlags;       /* Control flags (see below) */
	ph7_vm *pVm;        /* Virtual machine that own this instance */
	SyBlob sBlob;       /* String values */
	sxu32 nIdx;         /* Index number of this entry in the global object allocator */
};
/* Allowed value types.
 */
#define MEMOBJ_STRING    0x001  /* Memory value is a UTF-8 string */
#define MEMOBJ_INT       0x002  /* Memory value is an integer */
#define MEMOBJ_REAL      0x004  /* Memory value is a real number */
#define MEMOBJ_BOOL      0x008  /* Memory value is a boolean */
#define MEMOBJ_NULL      0x020  /* Memory value is NULL */
#define MEMOBJ_HASHMAP   0x040  /* Memory value is a hashmap aka 'array' in the PHP jargon */
#define MEMOBJ_OBJ       0x080  /* Memory value is an object [i.e: class instance] */
#define MEMOBJ_RES       0x100  /* Memory value is a resource [User private data] */ 
#define MEMOBJ_REFERENCE 0x400  /* Memory value hold a reference (64-bit index) of another ph7_value */
/* Mask of all known types */
#define MEMOBJ_ALL (MEMOBJ_STRING|MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL|MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES) 
/* Scalar variables
 * According to the PHP language reference manual
 *  Scalar variables are those containing an integer, float, string or boolean.
 *  Types array, object and resource are not scalar. 
 */
#define MEMOBJ_SCALAR (MEMOBJ_STRING|MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL|MEMOBJ_NULL)
#define MEMOBJ_AUX (MEMOBJ_REFERENCE)
/*
 * The following macro clear the current ph7_value type and replace
 * it with the given one.
 */
#define MemObjSetType(OBJ,TYPE) ((OBJ)->iFlags = ((OBJ)->iFlags&~MEMOBJ_ALL)|TYPE)
/* ph7_value cast method signature */
typedef sxi32 (*ProcMemObjCast)(ph7_value *);
/* Forward reference */
typedef struct ph7_output_consumer ph7_output_consumer;
typedef struct ph7_user_func ph7_user_func;
typedef struct ph7_conf ph7_conf;
/*
 * An instance of the following structure store the default VM output 
 * consumer and it's private data.
 * Client-programs can register their own output consumer callback
 * via the [PH7_VM_CONFIG_OUTPUT] configuration directive.
 * Please refer to the official documentation for more information
 * on how to register an output consumer callback.
 */
struct ph7_output_consumer
{
	ProcConsumer xConsumer; /* VM output consumer routine */
	void *pUserData;        /* Third argument to xConsumer() */
	ProcConsumer xDef;      /* Default output consumer routine */
	void *pDefData;         /* Third argument to xDef() */
};
/*
 * PH7 engine [i.e: ph7 instance] configuration is stored in
 * an instance of the following structure.
 * Please refer to the official documentation for more information
 * on how to configure your ph7 engine instance.
 */
struct ph7_conf
{
	ProcConsumer xErr;   /* Compile-time error consumer callback */
	void *pErrData;      /* Third argument to xErr() */
	SyBlob sErrConsumer; /* Default error consumer */
};
/*
 * Signature of the C function responsible of expanding constant values.
 */
typedef void (*ProcConstant)(ph7_value *,void *);
/*
 * Each registered constant [i.e: __TIME__, __DATE__, PHP_OS, INT_MAX, etc.] is stored
 * in an instance of the following structure.
 * Please refer to the official documentation for more information
 * on how to create/install foreign constants.
 */
typedef struct ph7_constant ph7_constant;
struct ph7_constant
{
	SyString sName;        /* Constant name */
	ProcConstant xExpand;  /* Function responsible of expanding constant value */
	void *pUserData;       /* Last argument to xExpand() */
};
typedef struct ph7_aux_data ph7_aux_data;
/*
 * Auxiliary data associated with each foreign function is stored
 * in a stack of the following structure.
 * Note that automatic tracked chunks are also stored in an instance
 * of this structure.
 */
struct ph7_aux_data
{
	void *pAuxData; /* Aux data */
};
/* Foreign functions signature */
typedef int (*ProchHostFunction)(ph7_context *,int,ph7_value **);
/*
 * Each installed foreign function is recored in an instance of the following
 * structure.
 * Please refer to the official documentation for more information on how 
 * to create/install foreign functions.
 */
struct ph7_user_func
{
	ph7_vm *pVm;              /* VM that own this instance */
	SyString sName;           /* Foreign function name */
	ProchHostFunction xFunc;  /* Implementation of the foreign function */
	void *pUserData;          /* User private data [Refer to the official documentation for more information]*/
	SySet aAux;               /* Stack of auxiliary data [Refer to the official documentation for more information]*/
};
/*
 * The 'context' argument for an installable function. A pointer to an
 * instance of this structure is the first argument to the routines used
 * implement the foreign functions.
 */
struct ph7_context
{
	ph7_user_func *pFunc;   /* Function information. */
	ph7_value *pRet;        /* Return value is stored here. */
	SySet sVar;             /* Container of dynamically allocated ph7_values
							 * [i.e: Garbage collection purposes.]
							 */
	SySet sChunk;           /* Track dynamically allocated chunks [ph7_aux_data instance]. 
							 * [i.e: Garbage collection purposes.]
							 */
	ph7_vm *pVm;            /* Virtual machine that own this context */
	sxi32 iFlags;           /* Call flags */
};
/*
 * Each hashmap entry [i.e: array(4,5,6)] is recorded in an instance
 * of the following structure.
 */
struct ph7_hashmap_node
{
	ph7_hashmap *pMap;     /* Hashmap that own this instance */
	sxi32 iType;           /* Node type */
	union{
		sxi64 iKey;        /* Int key */
		SyBlob sKey;       /* Blob key */
	}xKey;
	sxi32 iFlags;          /* Control flags */
	sxu32 nHash;           /* Key hash value */
	sxu32 nValIdx;         /* Value stored in this node */
	ph7_hashmap_node *pNext,*pPrev;               /* Link to other entries [i.e: linear traversal] */
	ph7_hashmap_node *pNextCollide,*pPrevCollide; /* Collision chain */
};
/* 
 * Each active hashmap aka array in the PHP jargon is represented
 * by an instance of the following structure.
 */
struct ph7_hashmap
{
	ph7_vm *pVm;                  /* VM that own this instance */
	ph7_hashmap_node **apBucket;  /* Hash bucket */
	ph7_hashmap_node *pFirst;     /* First inserted entry */
	ph7_hashmap_node *pLast;      /* Last inserted entry */
	ph7_hashmap_node *pCur;       /* Current entry */
	sxu32 nSize;                  /* Bucket size */
	sxu32 nEntry;                 /* Total number of inserted entries */
	sxu32 (*xIntHash)(sxi64);     /* Hash function for int_keys */
	sxu32 (*xBlobHash)(const void *,sxu32); /* Hash function for blob_keys */
	sxi64 iNextIdx;               /* Next available automatically assigned index */
	sxi32 iRef;                   /* Reference count */
};
/* An instance of the following structure is the context
 * for the FOREACH_STEP/FOREACH_INIT VM instructions.
 * Those instructions are used to implement the 'foreach'
 * statement.
 * This structure is made available to these instructions
 * as the P3 operand. 
 */
struct ph7_foreach_info
{
	SyString sKey;      /* Key name. Empty otherwise*/
	SyString sValue;    /* Value name */
	sxi32 iFlags;       /* Control flags */
	SySet aStep;        /* Stack of steps [i.e: ph7_foreach_step instance] */
};
struct ph7_foreach_step
{
	sxi32 iFlags;                   /* Control flags (see below) */
	/* Iterate on those values */
	union {
		ph7_hashmap *pMap;          /* Hashmap [i.e: array in the PHP jargon] iteration
									 * Ex: foreach(array(1,2,3) as $key=>$value){} 
									 */
		ph7_class_instance *pThis;  /* Class instance [i.e: object] iteration */
	}xIter;
};
/* Foreach step control flags */
#define PH7_4EACH_STEP_HASHMAP 0x001 /* Hashmap iteration */
#define PH7_4EACH_STEP_OBJECT  0x002 /* Object  iteration */
#define PH7_4EACH_STEP_KEY     0x004 /* Make Key available */
#define PH7_4EACH_STEP_REF     0x008 /* Pass value by reference not copy */
/*
 * Each PH7 engine is identified by an instance of the following structure.
 * Please refer to the official documentation for more information
 * on how to configure your PH7 engine instance.
 */
struct ph7
{
	SyMemBackend sAllocator;     /* Low level memory allocation subsystem */
	const ph7_vfs *pVfs;         /* Underlying Virtual File System */
	ph7_conf xConf;              /* Configuration */
#if defined(PH7_ENABLE_THREADS)
	const SyMutexMethods *pMethods;  /* Mutex methods */
	SyMutex *pMutex;                 /* Per-engine mutex */
#endif
	ph7_vm *pVms;      /* List of active VM */
	sxi32 iVm;         /* Total number of active VM */
	ph7 *pNext,*pPrev; /* List of active engines */
	sxu32 nMagic;      /* Sanity check against misuse */
};
/* Code generation data structures */
typedef sxi32 (*ProcErrorGen)(void *,sxi32,sxu32,const char *,...);
typedef struct ph7_expr_node   ph7_expr_node;
typedef struct ph7_expr_op     ph7_expr_op;
typedef struct ph7_gen_state   ph7_gen_state;
typedef struct GenBlock        GenBlock;
typedef sxi32 (*ProcLangConstruct)(ph7_gen_state *);
typedef sxi32 (*ProcNodeConstruct)(ph7_gen_state *,sxi32);
/*
 * Each supported operator [i.e: +, -, ==, *, %, >>, >=, new, etc.] is represented
 * by an instance of the following structure.
 * The PH7 parser does not use any external tools and is 100% handcoded.
 * That is, the PH7 parser is thread-safe ,full reentrant, produce consistant 
 * compile-time errrors and at least 7 times faster than the standard PHP parser.
 */
struct ph7_expr_op
{
	SyString sOp;   /* String representation of the operator [i.e: "+","*","=="...] */
	sxi32 iOp;      /* Operator ID */
	sxi32 iPrec;    /* Operator precedence: 1 == Highest */ 
	sxi32 iAssoc;   /* Operator associativity (either left,right or non-associative) */ 
	sxi32 iVmOp;    /* VM OP code for this operator [i.e: PH7_OP_EQ,PH7_OP_LT,PH7_OP_MUL...]*/
};
/*
 * Each expression node is parsed out and recorded
 * in an instance of the following structure.
 */
struct ph7_expr_node
{
	const ph7_expr_op *pOp;  /* Operator ID or NULL if literal, constant, variable, function or class method call */
	ph7_expr_node *pLeft;    /* Left expression tree */
	ph7_expr_node *pRight;   /* Right expression tree */
	SyToken *pStart;         /* Stream of tokens that belong to this node */
	SyToken *pEnd;           /* End of token stream */
	sxi32 iFlags;            /* Node construct flags */
	ProcNodeConstruct xCode; /* C routine responsible of compiling this node */
	SySet aNodeArgs;         /* Node arguments. Only used by postfix operators [i.e: function call]*/
	ph7_expr_node *pCond;    /* Condition: Only used by the ternary operator '?:' */
};
/* Node Construct flags */
#define EXPR_NODE_PRE_INCR 0x01 /* Pre-icrement/decrement [i.e: ++$i,--$j] node */
/*
 * A block of instructions is recorded in an instance of the following structure.
 * This structure is used only during compile-time and have no meaning
 * during bytecode execution.
 */
struct GenBlock
{
	ph7_gen_state *pGen;  /* State of the code generator */
	GenBlock *pParent;    /* Upper block or NULL if global */
	sxu32 nFirstInstr;    /* First instruction to execute  */
	sxi32 iFlags;         /* Block control flags (see below) */
	SySet aJumpFix;       /* Jump fixup (JumpFixup instance) */
	void *pUserData;      /* Upper layer private data */
	/* The following two fields are used only when compiling 
	 * the 'do..while()' language construct.
	 */
	sxu8 bPostContinue;    /* TRUE when compiling the do..while() statement */
	SySet aPostContFix;    /* Post-continue jump fix */
};
/*
 * Code generator state is remembered in an instance of the following
 * structure. We put the information in this structure and pass around
 * a pointer to this structure, rather than pass around  all of the 
 * information separately. This helps reduce the number of  arguments
 * to generator functions.
 * This structure is used only during compile-time and have no meaning
 * during bytecode execution.
 */
struct ph7_gen_state
{
	ph7_vm *pVm;         /* VM that own this instance */
	SyHash hLiteral;     /* Constant string Literals table */
	SyHash hNumLiteral;  /* Numeric literals table */
	SyHash hVar;         /* Collected variable hashtable */
	GenBlock *pCurrent;  /* Current processed block */
	GenBlock sGlobal;    /* Global block */
	ProcConsumer xErr;   /* Error consumer callback */
	void *pErrData;      /* Third argument to xErr() */
	SySet aLabel;        /* Label table */
	SySet aGoto;         /* Gotos table */
	SyBlob sWorker;      /* General purpose working buffer */
	SyBlob sErrBuf;      /* Error buffer */
	SyToken *pIn;        /* Current processed token */
	SyToken *pEnd;       /* Last token in the stream */
	sxu32 nErr;          /* Total number of compilation error */
	SyToken *pRawIn;     /* Current processed raw token */
	SyToken *pRawEnd;    /* Last raw token in the stream */
	SySet   *pTokenSet;  /* Token containers */
};
/* Forward references */
typedef struct ph7_vm_func_closure_env ph7_vm_func_closure_env;
typedef struct ph7_vm_func_static_var  ph7_vm_func_static_var;
typedef struct ph7_vm_func_arg ph7_vm_func_arg;
typedef struct ph7_vm_func ph7_vm_func;
typedef struct VmFrame VmFrame;
/*
 * Each collected function argument is recorded in an instance
 * of the following structure.
 * Note that as an extension, PH7 implements full type hinting
 * which mean that any function can have it's own signature.
 * Example:
 *      function foo(int $a,string $b,float $c,ClassInstance $d){}
 * This is how the powerful function overloading mechanism is
 * implemented.
 * Note that as an extension, PH7 allow function arguments to have
 * any complex default value associated with them unlike the standard
 * PHP engine.
 * Example:
 *    function foo(int $a = rand() & 1023){}
 *    now, when foo is called without arguments [i.e: foo()] the
 *    $a variable (first parameter) will be set to a random number
 *    between 0 and 1023 inclusive.
 * Refer to the official documentation for more information on this
 * mechanism and other extension introduced by the PH7 engine.
 */
struct ph7_vm_func_arg
{
	SyString sName;      /* Argument name */
	SySet aByteCode;     /* Compiled default value associated with this argument */
	sxu32 nType;         /* Type of this argument [i.e: array, int, string, float, object, etc.] */
	SyString sClass;     /* Class name if the argument expect a class instance [i.e: function foo(BaseClass $bar){} ] */
	sxi32 iFlags;        /* Configuration flags */
};
/*
 * Each static variable is parsed out and remembered in an instance
 * of the following structure.
 * Note that as an extension, PH7 allow static variable have
 * any complex default value associated with them unlike the standard
 * PHP engine.
 * Example:
 *   static $rand_str = 'PH7'.rand_str(3); // Concatenate 'PH7' with 
 *                                         // a random three characters(English alphabet)
 *   var_dump($rand_str);
 *   //You should see something like this
 *   string(6 'PH7awt');   
 */
struct ph7_vm_func_static_var
{
	SyString sName;   /* Static variable name */
	SySet aByteCode;  /* Compiled initialization expression  */
	sxu32 nIdx;       /* Object index in the global memory object container */
};
/*
 * Each imported variable from the outside closure environnment is recoded 
 * in an instance of the following structure.
 */
struct ph7_vm_func_closure_env
{
	SyString sName;   /* Imported variable name */
	int iFlags;       /* Control flags */
	ph7_value sValue; /* Imported variable value */
	sxu32 nIdx;       /* Reference to the bounded variable if passed by reference
					   *[Example:
					   *  $x = 1;
					   *  $closure = function() use (&$x) { ++$x; }
					   *  $closure();
					   *]
					   */
};
/* Function configuration flags */
#define VM_FUNC_ARG_BY_REF   0x001 /* Argument passed by reference */
#define VM_FUNC_ARG_HAS_DEF  0x002 /* Argument has default value associated with it */
#define VM_FUNC_REF_RETURN   0x004 /* Return by reference */
#define VM_FUNC_CLASS_METHOD 0x008 /* VM function is in fact a class method */
#define VM_FUNC_CLOSURE      0x010 /* VM function is a closure */
#define VM_FUNC_ARG_IGNORE   0x020 /* Do not install argument in the current frame */
/*
 * Each user defined function is parsed out and stored in an instance
 * of the following structure.
 * PH7 introduced some powerfull extensions to the PHP 5 programming
 * language like function overloading, type hinting, complex default
 * arguments values and many more.
 * Please refer to the official documentation for more information.
 */
struct ph7_vm_func
{
	SySet aArgs;         /* Expected arguments (ph7_vm_func_arg instance) */
	SySet aStatic;       /* Static variable (ph7_vm_func_static_var instance) */
	SyString sName;      /* Function name */
	SySet aByteCode;     /* Compiled function body */
	SySet aClosureEnv;   /* Closure environment (ph7_vm_func_closure_env instace) */
	sxi32 iFlags;        /* VM function configuration */
	SyString sSignature; /* Function signature used to implement function overloading
						  * (Refer to the official docuemntation for more information
						  *  on this powerfull feature)
						  */
	void *pUserData;     /* Upper layer private data associated with this instance */
	ph7_vm_func *pNextName; /* Next VM function with the same name as this one */
};
/* Forward reference */
typedef struct ph7_builtin_constant ph7_builtin_constant;
typedef struct ph7_builtin_func ph7_builtin_func;
/*
 * Each built-in foreign function (C function) is stored in an
 * instance of the following structure.
 * Please refer to the official documentation for more information
 * on how to create/install foreign functions.
 */
struct ph7_builtin_func
{
	const char *zName;        /* Function name [i.e: strlen(), rand(), array_merge(), etc.]*/
	ProchHostFunction xFunc;  /* C routine performing the computation */
};
/*
 * Each built-in foreign constant is stored in an instance
 * of the following structure.
 * Please refer to the official documentation for more information
 * on how to create/install foreign constants.
 */
struct ph7_builtin_constant
{
	const char *zName;     /* Constant name */
	ProcConstant xExpand;  /* C routine responsible of expanding constant value*/
};
/* Forward reference */
typedef struct ph7_class_method ph7_class_method;
typedef struct ph7_class_attr   ph7_class_attr;
/*
 * Each class is parsed out and stored in an instance of the following structure.
 * PH7 introduced powerfull extensions to the PHP 5 OO subsystems.
 * Please refer to the official documentation for more information.
 */
struct ph7_class
{
	ph7_class *pBase;     /* Base class if any */
	SyHash hDerived;      /* Derived [child] classes */ 
	SyString sName;       /* Class full qualified name */
	sxi32 iFlags;         /* Class configuration flags [i.e: final, interface, abstract, etc.]  */
	SyHash hAttr;         /* Class attributes [i.e: variables and constants] */
	SyHash hMethod;       /* Class methods */
	sxu32 nLine;          /* Line number on which this class was declared */
	SySet aInterface;     /* Implemented interface container */
	ph7_class *pNextName; /* Next class [interface, abstract, etc.] with the same name */
};
/* Class configuration flags */
#define PH7_CLASS_FINAL       0x001 /* Class is final [cannot be extended] */
#define PH7_CLASS_INTERFACE   0x002 /* Class is interface */
#define PH7_CLASS_ABSTRACT    0x004 /* Class is abstract */
/* Class attribute/methods/constants protection levels */
#define PH7_CLASS_PROT_PUBLIC     1 /* public */
#define PH7_CLASS_PROT_PROTECTED  2 /* protected */
#define PH7_CLASS_PROT_PRIVATE    3 /* private */    
/*
 * each class attribute (variable, constants) is parsed out and stored
 * in an instance of the following structure.
 */
struct ph7_class_attr
{
	SyString sName;      /* Atrribute name */
	sxi32 iFlags;        /* Attribute configuration [i.e: static, variable, constant, etc.] */
	sxi32 iProtection;   /* Protection level [i.e: public, private, protected] */
	SySet aByteCode;     /* Compiled attribute body */
	sxu32 nIdx;          /* Attribute index */
	sxu32 nLine;         /* Line number on which this attribute was defined */
};
/* Attribute configuration */
#define PH7_CLASS_ATTR_STATIC       0x001  /* Static attribute */
#define PH7_CLASS_ATTR_CONSTANT     0x002  /* Constant attribute */
#define PH7_CLASS_ATTR_ABSTRACT     0x004  /* Abstract method */
#define PH7_CLASS_ATTR_FINAL        0x008  /* Final method */
/* 
 * Each class method is parsed out and stored in an instance of the following
 * structure.
 * PH7 introduced some powerfull extensions to the PHP 5 programming
 * language like function overloading,type hinting,complex default
 * arguments and many more.
 * Please refer to the official documentation for more information.
 */
struct ph7_class_method
{
	ph7_vm_func sFunc;   /* Compiled method body */
	SyString sVmName;    /* Automatically generated name assigned to this method.
						  * Typically this is "[class_name__method_name@random_string]"
						  */
	sxi32 iProtection;   /* Protection level [i.e: public,private,protected] */
	sxi32 iFlags;        /* Methods configuration */
	sxi32 iCloneDepth;   /* Clone depth [Only used by the magic method __clone ] */
    sxu32 nLine;         /* Line on which this method was defined */
};
/*
 * Each active object (class instance) is represented by an instance of 
 * the following structure.
 */
struct ph7_class_instance
{
	ph7_vm *pVm;        /* VM that own this instance */
	ph7_class *pClass;  /* Object is an instance of this class */
	SyHash hAttr;       /* Hashtable of active class members */
	sxi32 iRef;         /* Reference count */
	sxi32 iFlags;       /* Control flags */
};
/*
 * A single instruction of the virtual machine has an opcode
 * and as many as three operands.
 * Each VM instruction resulting from compiling a PHP script
 * is stored in an instance of the following structure.
 */
typedef struct VmInstr VmInstr;
struct VmInstr
{
	sxu8  iOp; /* Operation to preform */
	sxi32 iP1; /* First operand */
	sxu32 iP2; /* Second operand (Often the jump destination) */
	void *p3;  /* Third operand (Often Upper layer private data) */
};
/* Each active class instance attribute is represented by an instance 
 * of the following structure.
 */
typedef struct VmClassAttr VmClassAttr;
struct VmClassAttr
{
	ph7_class_attr *pAttr; /* Class attribute */
	sxu32 nIdx;            /* Memory object index */
};
 /* Forward reference */
typedef struct VmRefObj VmRefObj;
/*
 * Each catch [i.e catch(Exception $e){ } ] block is parsed out and stored
 * in an instance of the following structure.
 */
typedef struct ph7_exception_block ph7_exception_block;
typedef struct ph7_exception ph7_exception;
struct ph7_exception_block
{
	SyString sClass; /* Exception class name [i.e: Exception,MyException...] */
	SyString sThis;  /* Instance name [i.e: $e..] */
	SySet sByteCode; /* Block compiled instructions */
};
/* 
 * Context for the exception mechanism.
 */
struct ph7_exception
{
	ph7_vm *pVm;    /* VM that own this exception */
	SySet sEntry;   /* Compiled 'catch' blocks (ph7_exception_block instance)
				     * container.
					 */
	VmFrame *pFrame; /* Frame that trigger the exception */
};
/* Forward reference */
typedef struct ph7_case_expr ph7_case_expr;
typedef struct ph7_switch ph7_switch;
/*
 * Each compiled case block in a swicth statement is compiled
 * and stored in an instance of the following structure.
 */
struct ph7_case_expr
{
	SySet aByteCode;   /* Compiled body of the case block */
	sxu32 nStart;      /* First instruction to execute */
};
/*
 * Each compiled switch statement is parsed out and stored
 * in an instance of the following structure.
 */
struct ph7_switch
{
	SySet aCaseExpr;  /* Compile case block */
	sxu32 nOut;       /* First instruction to execute after this statement */
	sxu32 nDefault;   /* First instruction to execute in the default block */
};
/* Assertion flags */
#define PH7_ASSERT_DISABLE    0x01  /* Disable assertion */
#define PH7_ASSERT_WARNING    0x02  /* Issue a warning for each failed assertion */
#define PH7_ASSERT_BAIL       0x04  /* Terminate execution on failed assertions */
#define PH7_ASSERT_QUIET_EVAL 0x08  /* Not used */
#define PH7_ASSERT_CALLBACK   0x10  /* Callback to call on failed assertions */
/* 
 * error_log() consumer function signature.
 * Refer to the [PH7_VM_CONFIG_ERR_LOG_HANDLER] configuration directive
 * for more information on how to register an error_log consumer().
 */
typedef void (*ProcErrLog)(const char *,int,const char *,const char *);
/* 
 * An instance of the following structure hold the bytecode instructions
 * resulting from compiling a PHP script.
 * This structure contains the complete state of the virtual machine.
 */
struct ph7_vm
{
	SyMemBackend sAllocator;	/* Memory backend */
#if defined(PH7_ENABLE_THREADS)
	SyMutex *pMutex;           /* Recursive mutex associated with VM. */
#endif
	ph7 *pEngine;               /* Interpreter that own this VM */
	SySet aByteCode;            /* Default bytecode container */
	SySet *pByteContainer;      /* Current bytecode container */
	VmFrame *pFrame;            /* Stack of active frames */
	SyPRNGCtx sPrng;            /* PRNG context */
	SySet aMemObj;              /* Object allocation table */
	SySet aLitObj;              /* Literals allocation table */
	ph7_value *aOps;            /* Operand stack */
	SySet aFreeObj;             /* Stack of free memory objects */
	SyHash hClass;              /* Compiled classes container */
	SyHash hConstant;           /* Host-application and user defined constants container */
	SyHash hHostFunction;       /* Host-application installable functions */
	SyHash hFunction;           /* Compiled functions */
	SyHash hSuper;              /* Superglobals hashtable */
	SyHash hPDO;                /* PDO installed drivers */
	SyBlob sConsumer;           /* Default VM consumer [i.e Redirect all VM output to this blob] */
	SyBlob sWorker;             /* General purpose working buffer */
	SyBlob sArgv;               /* $argv[] collector [refer to the [getopt()] implementation for more information] */
	SySet aFiles;               /* Stack of processed files */
	SySet aPaths;               /* Set of import paths */
	SySet aIncluded;            /* Set of included files */
	SySet aOB;                  /* Stackable output buffers */
	SySet aShutdown;            /* Stack of shutdown user callbacks */
	SySet aException;           /* Stack of loaded exception */
	SySet aIOstream;            /* Installed IO stream container */
	const ph7_io_stream *pDefStream; /* Default IO stream [i.e: typically this is the 'file://' stream] */
	ph7_value sExec;           /* Compiled script return value [Can be extracted via the PH7_VM_CONFIG_EXEC_VALUE directive]*/
	ph7_value aExceptionCB[2]; /* Installed exception handler callbacks via [set_exception_handler()] */
	ph7_value aErrCB[2];       /* Installed error handler callback via [set_error_handler()] */
	void *pStdin;              /* STDIN IO stream */
	void *pStdout;             /* STDOUT IO stream */
	void *pStderr;             /* STDERR IO stream */
	int bErrReport;            /* TRUE to report all runtime Error/Warning/Notice */
	int nRecursionDepth;       /* Current recursion depth */
	int nMaxDepth;             /* Maximum allowed recusion depth */
	int nObDepth;              /* OB depth */
	int nExceptDepth;          /* Exception depth */
	int closure_cnt;           /* Loaded closures counter */
	int json_rc;               /* JSON return status [refer to json_encode()/json_decode()]*/
	sxu32 unique_id;           /* Random number used to generate unique ID [refer to uniqid() for more info]*/
	ProcErrLog xErrLog;        /* error_log() consumer [refer to PH7_VM_CONFIG_ERR_LOG_HANDLER] */
	sxu32 nOutputLen;          /* Total number of generated output */
	ph7_output_consumer sVmConsumer; /* Registered output consumer callback */
	int iAssertFlags;          /* Assertion flags */
	ph7_value sAssertCallback; /* Callback to call on failed assertions */
	VmRefObj **apRefObj;       /* Hashtable of referenced object */
	VmRefObj *pRefList;        /* List of referenced memory objects */
	sxu32 nRefSize;            /* apRefObj[] size */
	sxu32 nRefUsed;            /* Total entries in apRefObj[] */
	SySet aSelf;               /* 'self' stack used for static member access [i.e: self::MyConstant] */
	ph7_hashmap *pGlobal;      /* $GLOBALS hashmap */ 
	sxu32 nGlobalIdx;          /* $GLOBALS index */
	sxi32 iExitStatus;         /* Script exit status */
	ph7_gen_state sCodeGen;    /* Code generator module */
	ph7_vm *pNext,*pPrev;      /* List of active VM's */
	sxu32 nMagic;              /* Sanity check against misuse */
};
/*
 * Allowed value for ph7_vm.nMagic
 */
#define PH7_VM_INIT   0xFADE9512  /* VM correctly initialized */
#define PH7_VM_RUN    0xEA271285  /* VM ready to execute PH7 bytecode */
#define PH7_VM_EXEC   0xCAFE2DAD  /* VM executing PH7 bytecode */
#define PH7_VM_STALE  0xBAD1DEAD  /* Stale VM */
/*
 * Error codes according to the PHP language reference manual.
 */
enum iErrCode
{
	E_ERROR             = 1,   /* Fatal run-time errors. These indicate errors that can not be recovered 
							    * from, such as a memory allocation problem. Execution of the script is
							    * halted.
								* The only fatal error under PH7 is an out-of-memory. All others erros
								* even a call to undefined function will not halt script execution.
							    */
	E_WARNING           = 2,   /* Run-time warnings (non-fatal errors). Execution of the script is not halted.  */
	E_PARSE             = 4,   /* Compile-time parse errors. Parse errors should only be generated by the parser.*/
	E_NOTICE            = 8,   /* Run-time notices. Indicate that the script encountered something that could 
							    * indicate an error, but could also happen in the normal course of running a script. 
							    */
	E_CORE_WARNING      = 16,  /* Fatal errors that occur during PHP's initial startup. This is like an E_ERROR
							    * except it is generated by the core of PHP. 
							    */
	E_USER_ERROR        = 256,  /* User-generated error message.*/
	E_USER_WARNING      = 512,  /* User-generated warning message.*/
	E_USER_NOTICE       = 1024, /* User-generated notice message.*/ 
	E_STRICT            = 2048, /* Enable to have PHP suggest changes to your code which will ensure the best interoperability
								 * and forward compatibility of your code.
								 */
	E_RECOVERABLE_ERROR = 4096, /* Catchable fatal error. It indicates that a probably dangerous error occured, but did not 
								 * leave the Engine in an unstable state. If the error is not caught by a user defined handle
								 * the application aborts as it was an E_ERROR.
								 */
	E_DEPRECATED        = 8192, /* Run-time notices. Enable this to receive warnings about code that will not
								 * work in future versions.  
								 */
	E_USER_DEPRECATED   = 16384, /* User-generated warning message. */
	E_ALL               = 32767  /* All errors and warnings */
};
/*
 * Each VM instruction resulting from compiling a PHP script is represented
 * by one of the following OP codes.
 * The program consists of a linear sequence of operations. Each operation
 * has an opcode and 3 operands.Operands P1 is an integer.
 * Operand P2 is an unsigned integer and operand P3 is a memory address.
 * Few opcodes use all 3 operands.
 */
enum ph7_vm_op {
  PH7_OP_DONE =   1,   /* Done */
  PH7_OP_HALT,         /* Halt */
  PH7_OP_LOAD,         /* Load memory object */
  PH7_OP_LOADC,        /* Load constant */
  PH7_OP_LOAD_IDX,     /* Load array entry */   
  PH7_OP_LOAD_MAP,     /* Load hashmap('array') */
  PH7_OP_LOAD_LIST,    /* Load list */
  PH7_OP_LOAD_CLOSURE, /* Load closure */
  PH7_OP_NOOP,         /* NOOP */
  PH7_OP_JMP,          /* Unconditional jump */
  PH7_OP_JZ,           /* Jump on zero (FALSE jump) */
  PH7_OP_JNZ,          /* Jump on non-zero (TRUE jump) */
  PH7_OP_POP,          /* Stack POP */ 
  PH7_OP_CAT,          /* Concatenation */
  PH7_OP_CVT_INT,      /* Integer cast */
  PH7_OP_CVT_STR,      /* String cast */
  PH7_OP_CVT_REAL,     /* Float cast */
  PH7_OP_CALL,         /* Function call */
  PH7_OP_UMINUS,       /* Unary minus '-'*/
  PH7_OP_UPLUS,        /* Unary plus '+'*/
  PH7_OP_BITNOT,       /* Bitwise not '~' */
  PH7_OP_LNOT,         /* Logical not '!' */
  PH7_OP_MUL,          /* Multiplication '*' */
  PH7_OP_DIV,          /* Division '/' */
  PH7_OP_MOD,          /* Modulus '%' */
  PH7_OP_ADD,          /* Add '+' */
  PH7_OP_SUB,          /* Sub '-' */
  PH7_OP_SHL,          /* Left shift '<<' */
  PH7_OP_SHR,          /* Right shift '>>' */
  PH7_OP_LT,           /* Less than '<' */
  PH7_OP_LE,           /* Less or equal '<=' */
  PH7_OP_GT,           /* Greater than '>' */
  PH7_OP_GE,           /* Greater or equal '>=' */
  PH7_OP_EQ,           /* Equal '==' */
  PH7_OP_NEQ,          /* Not equal '!=' */
  PH7_OP_TEQ,          /* Type equal '===' */
  PH7_OP_TNE,          /* Type not equal '!==' */
  PH7_OP_BAND,         /* Bitwise and '&' */
  PH7_OP_BXOR,         /* Bitwise xor '^' */
  PH7_OP_BOR,          /* Bitwise or '|' */
  PH7_OP_LAND,         /* Logical and '&&','and' */
  PH7_OP_LOR,          /* Logical or  '||','or' */
  PH7_OP_LXOR,         /* Logical xor 'xor' */
  PH7_OP_STORE,        /* Store Object */
  PH7_OP_STORE_IDX,    /* Store indexed object */
  PH7_OP_STORE_IDX_REF,/* Store indexed object by reference */
  PH7_OP_PULL,         /* Stack pull */
  PH7_OP_SWAP,         /* Stack swap */
  PH7_OP_YIELD,        /* Stack yield */
  PH7_OP_CVT_BOOL,     /* Boolean cast */
  PH7_OP_CVT_NUMC,     /* Numeric (integer,real or both) type cast */
  PH7_OP_INCR,         /* Increment ++ */
  PH7_OP_DECR,         /* Decrement -- */
  PH7_OP_SEQ,          /* 'eq' String equal: Strict string comparison */
  PH7_OP_SNE,          /* 'ne' String not equal: Strict string comparison */
  PH7_OP_NEW,          /* new */
  PH7_OP_CLONE,        /* clone */
  PH7_OP_ADD_STORE,    /* Add and store '+=' */
  PH7_OP_SUB_STORE,    /* Sub and store '-=' */
  PH7_OP_MUL_STORE,    /* Mul and store '*=' */
  PH7_OP_DIV_STORE,    /* Div and store '/=' */
  PH7_OP_MOD_STORE,    /* Mod and store '%=' */
  PH7_OP_CAT_STORE,    /* Cat and store '.=' */
  PH7_OP_SHL_STORE,    /* Shift left and store '>>=' */
  PH7_OP_SHR_STORE,    /* Shift right and store '<<=' */
  PH7_OP_BAND_STORE,   /* Bitand and store '&=' */
  PH7_OP_BOR_STORE,    /* Bitor and store '|=' */
  PH7_OP_BXOR_STORE,   /* Bitxor and store '^=' */
  PH7_OP_CONSUME,      /* Consume VM output */
  PH7_OP_LOAD_REF,     /* Load reference */
  PH7_OP_STORE_REF,    /* Store a reference to a variable*/
  PH7_OP_MEMBER,       /* Class member run-time access */
  PH7_OP_UPLINK,       /* Run-Time frame link */
  PH7_OP_CVT_NULL,     /* NULL cast */
  PH7_OP_CVT_ARRAY,    /* Array cast */
  PH7_OP_CVT_OBJ,      /* Object cast */
  PH7_OP_FOREACH_INIT, /* For each init */
  PH7_OP_FOREACH_STEP, /* For each step */
  PH7_OP_IS_A,         /* Instanceof */
  PH7_OP_LOAD_EXCEPTION,/* Load an exception */
  PH7_OP_POP_EXCEPTION, /* POP an exception */
  PH7_OP_THROW,         /* Throw exception */
  PH7_OP_SWITCH,        /* Switch operation */
  PH7_OP_ERR_CTRL      /* Error control */
};
/* -- END-OF INSTRUCTIONS -- */
/*
 * Expression Operators ID.
 */
enum ph7_expr_id {
	EXPR_OP_NEW = 1,   /* new */
	EXPR_OP_CLONE,     /* clone */
	EXPR_OP_ARROW,     /* -> */
	EXPR_OP_DC,        /* :: */
	EXPR_OP_SUBSCRIPT, /* []: Subscripting */
	EXPR_OP_FUNC_CALL, /* func_call() */
	EXPR_OP_INCR,      /* ++ */
	EXPR_OP_DECR,      /* -- */ 
	EXPR_OP_BITNOT,    /* ~ */
	EXPR_OP_UMINUS,    /* Unary minus  */
	EXPR_OP_UPLUS,     /* Unary plus */
	EXPR_OP_TYPECAST,  /* Type cast [i.e: (int),(float),(string)...] */
	EXPR_OP_ALT,       /* @ */
	EXPR_OP_INSTOF,    /* instanceof */
	EXPR_OP_LOGNOT,    /* logical not ! */
	EXPR_OP_MUL,       /* Multiplication */
	EXPR_OP_DIV,       /* division */
	EXPR_OP_MOD,       /* Modulus */
	EXPR_OP_ADD,       /* Addition */
	EXPR_OP_SUB,       /* Substraction */
	EXPR_OP_DOT,       /* Concatenation */
	EXPR_OP_SHL,       /* Left shift */
	EXPR_OP_SHR,       /* Right shift */
	EXPR_OP_LT,        /* Less than */
	EXPR_OP_LE,        /* Less equal */
	EXPR_OP_GT,        /* Greater than */
	EXPR_OP_GE,        /* Greater equal */
	EXPR_OP_EQ,        /* Equal == */
	EXPR_OP_NE,        /* Not equal != <> */
	EXPR_OP_TEQ,       /* Type equal === */
	EXPR_OP_TNE,       /* Type not equal !== */
	EXPR_OP_SEQ,       /* String equal 'eq' */
	EXPR_OP_SNE,       /* String not equal 'ne' */
	EXPR_OP_BAND,      /* Biwise and '&' */
	EXPR_OP_REF,       /* Reference operator '&' */
	EXPR_OP_XOR,       /* bitwise xor '^' */
	EXPR_OP_BOR,       /* bitwise or '|' */
	EXPR_OP_LAND,      /* Logical and '&&','and' */
	EXPR_OP_LOR,       /* Logical or  '||','or'*/
	EXPR_OP_LXOR,      /* Logical xor 'xor' */
	EXPR_OP_QUESTY,    /* Ternary operator '?' */
	EXPR_OP_ASSIGN,    /* Assignment '=' */
	EXPR_OP_ADD_ASSIGN, /* Combined operator: += */
	EXPR_OP_SUB_ASSIGN, /* Combined operator: -= */
	EXPR_OP_MUL_ASSIGN, /* Combined operator: *= */
	EXPR_OP_DIV_ASSIGN, /* Combined operator: /= */
	EXPR_OP_MOD_ASSIGN, /* Combined operator: %= */
	EXPR_OP_DOT_ASSIGN, /* Combined operator: .= */
	EXPR_OP_AND_ASSIGN, /* Combined operator: &= */
	EXPR_OP_OR_ASSIGN,  /* Combined operator: |= */
	EXPR_OP_XOR_ASSIGN, /* Combined operator: ^= */
	EXPR_OP_SHL_ASSIGN, /* Combined operator: <<= */
	EXPR_OP_SHR_ASSIGN, /* Combined operator: >>= */
	EXPR_OP_COMMA       /* Comma expression */
};
/*
 * Very high level tokens.
 */
#define PH7_TOKEN_RAW 0x001 /* Raw text [i.e: HTML,XML...] */
#define PH7_TOKEN_PHP 0x002 /* PHP chunk */
/*
 * Lexer token codes
 * The following set of constants are the tokens recognized
 * by the lexer when processing PHP input.
 * Important: Token values MUST BE A POWER OF TWO.
 */
#define PH7_TK_INTEGER   0x0000001  /* Integer */
#define PH7_TK_REAL      0x0000002  /* Real number */
#define PH7_TK_NUM       (PH7_TK_INTEGER|PH7_TK_REAL) /* Numeric token,either integer or real */
#define PH7_TK_KEYWORD   0x0000004 /* Keyword [i.e: while,for,if,foreach...] */
#define PH7_TK_ID        0x0000008 /* Alphanumeric or UTF-8 stream */
#define PH7_TK_DOLLAR    0x0000010 /* '$' Dollar sign */
#define PH7_TK_OP        0x0000020 /* Operator [i.e: +,*,/...] */
#define PH7_TK_OCB       0x0000040 /* Open curly brace'{' */
#define PH7_TK_CCB       0x0000080 /* Closing curly brace'}' */
#define PH7_TK_NSSEP     0x0000100 /* Namespace separator '\' */
#define PH7_TK_LPAREN    0x0000200 /* Left parenthesis '(' */
#define PH7_TK_RPAREN    0x0000400 /* Right parenthesis ')' */
#define PH7_TK_OSB       0x0000800 /* Open square bracket '[' */
#define PH7_TK_CSB       0x0001000 /* Closing square bracket ']' */
#define PH7_TK_DSTR      0x0002000 /* Double quoted string "$str" */
#define PH7_TK_SSTR      0x0004000 /* Single quoted string 'str' */
#define PH7_TK_HEREDOC   0x0008000 /* Heredoc <<< */
#define PH7_TK_NOWDOC    0x0010000 /* Nowdoc <<< */
#define PH7_TK_COMMA     0x0020000 /* Comma ',' */
#define PH7_TK_SEMI      0x0040000 /* Semi-colon ";" */
#define PH7_TK_BSTR      0x0080000 /* Backtick quoted string [i.e: Shell command `date`] */
#define PH7_TK_COLON     0x0100000 /* single Colon ':' */
#define PH7_TK_AMPER     0x0200000 /* Ampersand '&' */
#define PH7_TK_EQUAL     0x0400000 /* Equal '=' */
#define PH7_TK_ARRAY_OP  0x0800000 /* Array operator '=>' */
#define PH7_TK_OTHER     0x1000000 /* Other symbols */
/*
 * PHP keyword.
 * These words have special meaning in PHP. Some of them represent things which look like
 * functions, some look like constants, and so on, but they're not, really: they are language constructs.
 * You cannot use any of the following words as constants, class names, function or method names.
 * Using them as variable names is generally OK, but could lead to confusion. 
 */
#define PH7_TKWRD_EXTENDS      1 /* extends */
#define PH7_TKWRD_ENDSWITCH    2 /* endswitch */
#define PH7_TKWRD_SWITCH       3 /* switch */
#define PH7_TKWRD_PRINT        4 /* print */
#define PH7_TKWRD_INTERFACE    5 /* interface */
#define PH7_TKWRD_ENDDEC       6 /* enddeclare */
#define PH7_TKWRD_DECLARE      7 /* declare */
/* The number '8' is reserved for PH7_TK_ID */
#define PH7_TKWRD_REQONCE      9 /* require_once */
#define PH7_TKWRD_REQUIRE      10 /* require */
#define PH7_TKWRD_ELIF         0x4000000 /* elseif: MUST BE A POWER OF TWO */
#define PH7_TKWRD_ELSE         0x8000000 /* else:  MUST BE A POWER OF TWO */
#define PH7_TKWRD_IF           13 /* if */
#define PH7_TKWRD_FINAL        14 /* final */
#define PH7_TKWRD_LIST         15 /* list */
#define PH7_TKWRD_STATIC       16 /* static */
#define PH7_TKWRD_CASE         17 /* case */
#define PH7_TKWRD_SELF         18 /* self */
#define PH7_TKWRD_FUNCTION     19 /* function */
#define PH7_TKWRD_NAMESPACE    20 /* namespace */
#define PH7_TKWRD_ENDIF        0x400000 /* endif: MUST BE A POWER OF TWO */
#define PH7_TKWRD_CLONE        0x80 /* clone: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_NEW          0x100 /* new: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_CONST        22 /* const */
#define PH7_TKWRD_THROW        23 /* throw */
#define PH7_TKWRD_USE          24 /* use */
#define PH7_TKWRD_ENDWHILE     0x800000 /* endwhile: MUST BE A POWER OF TWO */
#define PH7_TKWRD_WHILE        26 /* while */
#define PH7_TKWRD_EVAL         27 /* eval */
#define PH7_TKWRD_VAR          28 /* var */
#define PH7_TKWRD_ARRAY        0x200 /* array: MUST BE A POWER OF TWO */
#define PH7_TKWRD_ABSTRACT     29 /* abstract */
#define PH7_TKWRD_TRY          30 /* try */
#define PH7_TKWRD_AND          0x400 /* and: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_DEFAULT      31 /* default */
#define PH7_TKWRD_CLASS        32 /* class */
#define PH7_TKWRD_AS           33 /* as */
#define PH7_TKWRD_CONTINUE     34 /* continue */
#define PH7_TKWRD_EXIT         35 /* exit */
#define PH7_TKWRD_DIE          36 /* die */
#define PH7_TKWRD_ECHO         37 /* echo */
#define PH7_TKWRD_GLOBAL       38 /* global */
#define PH7_TKWRD_IMPLEMENTS   39 /* implements */
#define PH7_TKWRD_INCONCE      40 /* include_once */
#define PH7_TKWRD_INCLUDE      41 /* include */
#define PH7_TKWRD_EMPTY        42 /* empty */
#define PH7_TKWRD_INSTANCEOF   0x800 /* instanceof: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_ISSET        43 /* isset */
#define PH7_TKWRD_PARENT       44 /* parent */
#define PH7_TKWRD_PRIVATE      45 /* private */
#define PH7_TKWRD_ENDFOR       0x1000000 /* endfor: MUST BE A POWER OF TWO */
#define PH7_TKWRD_END4EACH     0x2000000 /* endforeach: MUST BE A POWER OF TWO */
#define PH7_TKWRD_FOR          48 /* for */
#define PH7_TKWRD_FOREACH      49 /* foreach */
#define PH7_TKWRD_OR           0x1000 /* or: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_PROTECTED    50 /* protected */
#define PH7_TKWRD_DO           51 /* do */
#define PH7_TKWRD_PUBLIC       52 /* public */
#define PH7_TKWRD_CATCH        53 /* catch */
#define PH7_TKWRD_RETURN       54 /* return */
#define PH7_TKWRD_UNSET        0x2000 /* unset: MUST BE A POWER OF TWO  */
#define PH7_TKWRD_XOR          0x4000 /* xor: MUST BE A POWER OF TWO  */ 
#define PH7_TKWRD_BREAK        55 /* break */
#define PH7_TKWRD_GOTO         56 /* goto */
#define PH7_TKWRD_BOOL         0x8000  /* bool:  MUST BE A POWER OF TWO */
#define PH7_TKWRD_INT          0x10000  /* int:   MUST BE A POWER OF TWO */
#define PH7_TKWRD_FLOAT        0x20000  /* float:  MUST BE A POWER OF TWO */
#define PH7_TKWRD_STRING       0x40000  /* string: MUST BE A POWER OF TWO */
#define PH7_TKWRD_OBJECT       0x80000 /* object: MUST BE A POWER OF TWO */
#define PH7_TKWRD_SEQ          0x100000 /* String string comparison operator: 'eq' equal MUST BE A POWER OF TWO */
#define PH7_TKWRD_SNE          0x200000 /* String string comparison operator: 'ne' not equal MUST BE A POWER OF TWO */
/* JSON encoding/decoding related definition */
enum json_err_code{
	JSON_ERROR_NONE = 0,  /* No error has occurred. */
	JSON_ERROR_DEPTH,     /* The maximum stack depth has been exceeded.  */
	JSON_ERROR_STATE_MISMATCH, /* Occurs with underflow or with the modes mismatch.  */
	JSON_ERROR_CTRL_CHAR, /* Control character error, possibly incorrectly encoded.  */
	JSON_ERROR_SYNTAX,    /* Syntax error. */
	JSON_ERROR_UTF8       /* Malformed UTF-8 characters */
};
/* The following constants can be combined to form options for json_encode(). */
#define	JSON_HEX_TAG           0x01  /* All < and > are converted to \u003C and \u003E. */
#define JSON_HEX_AMP           0x02  /* All &s are converted to \u0026. */
#define JSON_HEX_APOS          0x04  /* All ' are converted to \u0027. */
#define JSON_HEX_QUOT          0x08  /* All " are converted to \u0022. */
#define JSON_FORCE_OBJECT      0x10  /* Outputs an object rather than an array */
#define JSON_NUMERIC_CHECK     0x20  /* Encodes numeric strings as numbers. */ 
#define JSON_BIGINT_AS_STRING  0x40  /* Not used */
#define JSON_PRETTY_PRINT      0x80  /* Use whitespace in returned data to format it.*/ 
#define JSON_UNESCAPED_SLASHES 0x100 /* Don't escape '/' */
#define JSON_UNESCAPED_UNICODE 0x200 /* Not used */
/* memobj.c function prototypes */
PH7_PRIVATE sxi32 PH7_MemObjDump(SyBlob *pOut,ph7_value *pObj,int ShowType,int nTab,int nDepth,int isRef);
PH7_PRIVATE const char * PH7_MemObjTypeDump(ph7_value *pVal);
PH7_PRIVATE sxi32 PH7_MemObjAdd(ph7_value *pObj1,ph7_value *pObj2,int bAddStore);
PH7_PRIVATE sxi32 PH7_MemObjCmp(ph7_value *pObj1,ph7_value *pObj2,int bStrict,int iNest);
PH7_PRIVATE sxi32 PH7_MemObjInitFromString(ph7_vm *pVm,ph7_value *pObj,const SyString *pVal);
PH7_PRIVATE sxi32 PH7_MemObjInitFromArray(ph7_vm *pVm,ph7_value *pObj,ph7_hashmap *pArray);
#if 0
/* Not used in the current release of the PH7 engine */
PH7_PRIVATE sxi32 PH7_MemObjInitFromReal(ph7_vm *pVm,ph7_value *pObj,ph7_real rVal);
#endif
PH7_PRIVATE sxi32 PH7_MemObjInitFromInt(ph7_vm *pVm,ph7_value *pObj,sxi64 iVal);
PH7_PRIVATE sxi32 PH7_MemObjInitFromBool(ph7_vm *pVm,ph7_value *pObj,sxi32 iVal);
PH7_PRIVATE sxi32 PH7_MemObjInit(ph7_vm *pVm,ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjStringAppend(ph7_value *pObj,const char *zData,sxu32 nLen);
#if 0
/* Not used in the current release of the PH7 engine */
PH7_PRIVATE sxi32 PH7_MemObjStringFormat(ph7_value *pObj,const char *zFormat,va_list ap);
#endif
PH7_PRIVATE sxi32 PH7_MemObjStore(ph7_value *pSrc,ph7_value *pDest);
PH7_PRIVATE sxi32 PH7_MemObjLoad(ph7_value *pSrc,ph7_value *pDest);
PH7_PRIVATE sxi32 PH7_MemObjRelease(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToNumeric(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjTryInteger(ph7_value *pObj);
PH7_PRIVATE ProcMemObjCast PH7_MemObjCastMethod(sxi32 iFlags);
PH7_PRIVATE sxi32 PH7_MemObjIsNumeric(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjIsEmpty(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToHashmap(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToObject(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToString(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToNull(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToReal(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToInteger(ph7_value *pObj);
PH7_PRIVATE sxi32 PH7_MemObjToBool(ph7_value *pObj);
PH7_PRIVATE sxi64 PH7_TokenValueToInt64(SyString *pData);
/* lex.c function prototypes */
PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut);
PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut);
/* vm.c function prototypes */
PH7_PRIVATE void PH7_VmReleaseContextValue(ph7_context *pCtx,ph7_value *pValue);
PH7_PRIVATE sxi32 PH7_VmInitFuncState(ph7_vm *pVm,ph7_vm_func *pFunc,const char *zName,sxu32 nByte,
	sxi32 iFlags,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(ph7_vm *pVm,ph7_vm_func *pFunc,SyString *pName);
PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(ph7_vm *pVm,ph7_class_instance *pObj);
PH7_PRIVATE sxi32 PH7_VmRefObjRemove(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry);
PH7_PRIVATE sxi32 PH7_VmRefObjInstall(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry,ph7_hashmap_node *pMapEntry,sxi32 iFlags);
PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew);
PH7_PRIVATE ph7_class * PH7_VmExtractClass(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable,sxi32 iNest);
PH7_PRIVATE sxi32 PH7_VmRegisterConstant(ph7_vm *pVm,const SyString *pName,ProcConstant xExpand,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(ph7_vm *pVm,const SyString *pName,ProchHostFunction xFunc,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmInstallClass(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE sxi32 PH7_VmBlobConsumer(const void *pSrc,unsigned int nLen,void *pUserData);
PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm);
PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex);
PH7_PRIVATE sxi32 PH7_VmOutputConsume(ph7_vm *pVm,SyString *pString);
PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(ph7_vm *pVm,const char *zFormat,va_list ap);
PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap);
PH7_PRIVATE sxi32 PH7_VmThrowError(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zMessage);
PH7_PRIVATE void  PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmDump(ph7_vm *pVm,ProcConsumer xConsumer,void *pUserData);
PH7_PRIVATE sxi32 PH7_VmInit(ph7_vm *pVm,ph7 *pEngine);
PH7_PRIVATE sxi32 PH7_VmConfigure(ph7_vm *pVm,sxi32 nOp,va_list ap);
PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmMakeReady(ph7_vm *pVm);
PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm);
PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm);
PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm);
PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm);
PH7_PRIVATE VmInstr *PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex);
PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer);
PH7_PRIVATE sxi32 PH7_VmEmitInstr(ph7_vm *pVm,sxi32 iOp,sxi32 iP1,sxu32 iP2,void *p3,sxu32 *pIndex);
PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_VmCallClassMethod(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_method *pMethod,
	ph7_value *pResult,int nArg,ph7_value **apArg);
PH7_PRIVATE sxi32 PH7_VmCallUserFunction(ph7_vm *pVm,ph7_value *pFunc,int nArg,ph7_value **apArg,ph7_value *pResult);
PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(ph7_vm *pVm,ph7_value *pFunc,ph7_value *pResult,...);
PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce);
PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen);
PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm);
PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(ph7_vm *pVm,const char **pzDevice,int nByte);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE int PH7_Utf8Read(
  const unsigned char *z,         /* First byte of UTF-8 character */
  const unsigned char *zTerm,     /* Pretend this byte is 0x00 */
  const unsigned char **pzNext    /* Write first byte past UTF-8 char here */
);
/* parse.c function prototypes */
PH7_PRIVATE int PH7_IsLangConstruct(sxu32 nKeyID,sxu8 bCheckFunc);
PH7_PRIVATE sxi32 PH7_ExprMakeTree(ph7_gen_state *pGen,SySet *pExprNode,ph7_expr_node **ppRoot);
PH7_PRIVATE sxi32 PH7_GetNextExpr(SyToken *pStart,SyToken *pEnd,SyToken **ppNext);
PH7_PRIVATE void PH7_DelimitNestedTokens(SyToken *pIn,SyToken *pEnd,sxu32 nTokStart,sxu32 nTokEnd,SyToken **ppEnd);
PH7_PRIVATE const ph7_expr_op * PH7_ExprExtractOperator(SyString *pStr,SyToken *pLast);
PH7_PRIVATE sxi32 PH7_ExprFreeTree(ph7_gen_state *pGen,SySet *pNodeSet);
/* compile.c function prototypes */
PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType);
PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag);
PH7_PRIVATE sxi32 PH7_InitCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);
PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(ph7_vm *pVm,ProcConsumer xErr,void *pErrData);
PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...);
PH7_PRIVATE sxi32 PH7_CompileScript(ph7_vm *pVm,SyString *pScript,sxi32 iFlags);
/* constant.c function prototypes */
PH7_PRIVATE void PH7_RegisterBuiltInConstant(ph7_vm *pVm);
/* builtin.c function prototypes */
PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm);
/* hashmap.c function prototypes */
PH7_PRIVATE ph7_hashmap * PH7_NewHashmap(ph7_vm *pVm,sxu32 (*xIntHash)(sxi64),sxu32 (*xBlobHash)(const void *,sxu32));
PH7_PRIVATE sxi32 PH7_HashmapCreateSuper(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_HashmapRelease(ph7_hashmap *pMap,int FreeDS);
PH7_PRIVATE void  PH7_HashmapUnref(ph7_hashmap *pMap);
PH7_PRIVATE sxi32 PH7_HashmapLookup(ph7_hashmap *pMap,ph7_value *pKey,ph7_hashmap_node **ppNode);
PH7_PRIVATE sxi32 PH7_HashmapInsert(ph7_hashmap *pMap,ph7_value *pKey,ph7_value *pVal);
PH7_PRIVATE sxi32 PH7_HashmapInsertByRef(ph7_hashmap *pMap,ph7_value *pKey,sxu32 nRefIdx);
PH7_PRIVATE sxi32 PH7_HashmapUnion(ph7_hashmap *pLeft,ph7_hashmap *pRight);
PH7_PRIVATE void PH7_HashmapUnlinkNode(ph7_hashmap_node *pNode,int bRestore);
PH7_PRIVATE sxi32 PH7_HashmapDup(ph7_hashmap *pSrc,ph7_hashmap *pDest);
PH7_PRIVATE sxi32 PH7_HashmapCmp(ph7_hashmap *pLeft,ph7_hashmap *pRight,int bStrict);
PH7_PRIVATE void PH7_HashmapResetLoopCursor(ph7_hashmap *pMap);
PH7_PRIVATE ph7_hashmap_node * PH7_HashmapGetNextEntry(ph7_hashmap *pMap);
PH7_PRIVATE void PH7_HashmapExtractNodeValue(ph7_hashmap_node *pNode,ph7_value *pValue,int bStore);
PH7_PRIVATE void PH7_HashmapExtractNodeKey(ph7_hashmap_node *pNode,ph7_value *pKey);
PH7_PRIVATE void PH7_RegisterHashmapFunctions(ph7_vm *pVm);
PH7_PRIVATE sxi32 PH7_HashmapDump(SyBlob *pOut,ph7_hashmap *pMap,int ShowType,int nTab,int nDepth);
PH7_PRIVATE sxi32 PH7_HashmapWalk(ph7_hashmap *pMap,int (*xWalk)(ph7_value *,ph7_value *,void *),void *pUserData);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE int PH7_HashmapValuesToSet(ph7_hashmap *pMap,SySet *pOut);
/* builtin.c function prototypes */ 
PH7_PRIVATE sxi32 PH7_InputFormat(int (*xConsumer)(ph7_context *,const char *,int,void *),
	ph7_context *pCtx,const char *zIn,int nByte,int nArg,ph7_value **apArg,void *pUserData,int vf);
PH7_PRIVATE sxi32 PH7_ProcessCsv(const char *zInput,int nByte,int delim,int encl,
	int escape,sxi32 (*xConsumer)(const char *,int,void *),void *pUserData);
PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData);
PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen);
PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection);
#endif
/* oo.c function prototypes */
PH7_PRIVATE ph7_class * PH7_NewRawClass(ph7_vm *pVm,const SyString *pName,sxu32 nLine);
PH7_PRIVATE ph7_class_attr * PH7_NewClassAttr(ph7_vm *pVm,const SyString *pName,sxu32 nLine,sxi32 iProtection,sxi32 iFlags);
PH7_PRIVATE ph7_class_method * PH7_NewClassMethod(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,sxu32 nLine,
	sxi32 iProtection,sxi32 iFlags,sxi32 iFuncFlags);
PH7_PRIVATE ph7_class_method * PH7_ClassExtractMethod(ph7_class *pClass,const char *zName,sxu32 nByte);
PH7_PRIVATE ph7_class_attr   * PH7_ClassExtractAttribute(ph7_class *pClass,const char *zName,sxu32 nByte);
PH7_PRIVATE sxi32 PH7_ClassInstallAttr(ph7_class *pClass,ph7_class_attr *pAttr);
PH7_PRIVATE sxi32 PH7_ClassInstallMethod(ph7_class *pClass,ph7_class_method *pMeth);
PH7_PRIVATE sxi32 PH7_ClassInherit(ph7_gen_state *pGen,ph7_class *pSub,ph7_class *pBase);
PH7_PRIVATE sxi32 PH7_ClassInterfaceInherit(ph7_class *pSub,ph7_class *pBase);
PH7_PRIVATE sxi32 PH7_ClassImplement(ph7_class *pMain,ph7_class *pInterface);
PH7_PRIVATE ph7_class_instance * PH7_NewClassInstance(ph7_vm *pVm,ph7_class *pClass);
PH7_PRIVATE ph7_class_instance * PH7_CloneClassInstance(ph7_class_instance *pSrc);
PH7_PRIVATE sxi32 PH7_ClassInstanceCmp(ph7_class_instance *pLeft,ph7_class_instance *pRight,int bStrict,int iNest);
PH7_PRIVATE void  PH7_ClassInstanceUnref(ph7_class_instance *pThis);
PH7_PRIVATE sxi32 PH7_ClassInstanceDump(SyBlob *pOut,ph7_class_instance *pThis,int ShowType,int nTab,int nDepth);
PH7_PRIVATE sxi32 PH7_ClassInstanceCallMagicMethod(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const char *zMethod,
	sxu32 nByte,const SyString *pAttrName);
PH7_PRIVATE ph7_value * PH7_ClassInstanceExtractAttrValue(ph7_class_instance *pThis,VmClassAttr *pAttr);
PH7_PRIVATE sxi32 PH7_ClassInstanceToHashmap(ph7_class_instance *pThis,ph7_hashmap *pMap);
PH7_PRIVATE sxi32 PH7_ClassInstanceWalk(ph7_class_instance *pThis,
	int (*xWalk)(const char *,ph7_value *,void *),void *pUserData);
PH7_PRIVATE ph7_value * PH7_ClassInstanceFetchAttr(ph7_class_instance *pThis,const SyString *pName);
/* vfs.c */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE void * PH7_StreamOpenHandle(ph7_vm *pVm,const ph7_io_stream *pStream,const char *zFile,
	int iFlags,int use_include,ph7_value *pResource,int bPushInclude,int *pNew);
PH7_PRIVATE sxi32 PH7_StreamReadWholeFile(void *pHandle,const ph7_io_stream *pStream,SyBlob *pOut);
PH7_PRIVATE void PH7_StreamCloseHandle(const ph7_io_stream *pStream,void *pHandle);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE const char * PH7_ExtractDirName(const char *zPath,int nByte,int *pLen);
PH7_PRIVATE sxi32 PH7_RegisterIORoutine(ph7_vm *pVm);
PH7_PRIVATE const ph7_vfs * PH7_ExportBuiltinVfs(void);
PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm);
PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm);
PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm);
/* lib.c function prototypes */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyXMLParserInit(SyXMLParser *pParser,SyMemBackend *pAllocator,sxi32 iFlags);
PH7_PRIVATE sxi32 SyXMLParserSetEventHandler(SyXMLParser *pParser,
	void *pUserData,
	ProcXMLStartTagHandler xStartTag,
	ProcXMLTextHandler xRaw,
	ProcXMLSyntaxErrorHandler xErr,
	ProcXMLStartDocument xStartDoc,
	ProcXMLEndTagHandler xEndTag,
	ProcXMLPIHandler xPi,
	ProcXMLEndDocument xEndDoc,
	ProcXMLDoctypeHandler xDoctype,
	ProcXMLNameSpaceStart xNameSpace,
	ProcXMLNameSpaceEnd xNameSpaceEnd
	);
PH7_PRIVATE sxi32 SyXMLProcess(SyXMLParser *pParser,const char *zInput,sxu32 nByte);
PH7_PRIVATE sxi32 SyXMLParserRelease(SyXMLParser *pParser);
PH7_PRIVATE sxi32 SyArchiveInit(SyArchive *pArch,SyMemBackend *pAllocator,ProcHash xHash,ProcRawStrCmp xCmp);
PH7_PRIVATE sxi32 SyArchiveRelease(SyArchive *pArch);
PH7_PRIVATE sxi32 SyArchiveResetLoopCursor(SyArchive *pArch);
PH7_PRIVATE sxi32 SyArchiveGetNextEntry(SyArchive *pArch,SyArchiveEntry **ppEntry);
PH7_PRIVATE sxi32 SyZipExtractFromBuf(SyArchive *pArch,const char *zBuf,sxu32 nLen);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyBinToHexConsumer(const void *pIn,sxu32 nLen,ProcConsumer xConsumer,void *pConsumerData);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#ifndef PH7_DISABLE_HASH_FUNC
PH7_PRIVATE sxu32 SyCrc32(const void *pSrc,sxu32 nLen);
PH7_PRIVATE void MD5Update(MD5Context *ctx, const unsigned char *buf, unsigned int len);
PH7_PRIVATE void MD5Final(unsigned char digest[16], MD5Context *ctx);
PH7_PRIVATE sxi32 MD5Init(MD5Context *pCtx);
PH7_PRIVATE sxi32 SyMD5Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[16]);
PH7_PRIVATE void SHA1Init(SHA1Context *context);
PH7_PRIVATE void SHA1Update(SHA1Context *context,const unsigned char *data,unsigned int len);
PH7_PRIVATE void SHA1Final(SHA1Context *context, unsigned char digest[20]);
PH7_PRIVATE sxi32 SySha1Compute(const void *pIn,sxu32 nLen,unsigned char zDigest[20]);
#endif
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SyRandomness(SyPRNGCtx *pCtx,void *pBuf,sxu32 nLen);
PH7_PRIVATE sxi32 SyRandomnessInit(SyPRNGCtx *pCtx,ProcRandomSeed xSeed,void *pUserData);
PH7_PRIVATE sxu32 SyBufferFormat(char *zBuf,sxu32 nLen,const char *zFormat,...);
PH7_PRIVATE sxu32 SyBlobFormatAp(SyBlob *pBlob,const char *zFormat,va_list ap);
PH7_PRIVATE sxu32 SyBlobFormat(SyBlob *pBlob,const char *zFormat,...);
PH7_PRIVATE sxi32 SyProcFormat(ProcConsumer xConsumer,void *pData,const char *zFormat,...);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE const char *SyTimeGetMonth(sxi32 iMonth);
PH7_PRIVATE const char *SyTimeGetDay(sxi32 iDay);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SyUriDecode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData,int bUTF8);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyUriEncode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
#endif
PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex);
PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp);
PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyBase64Decode(const char *zB64,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
PH7_PRIVATE sxi32 SyBase64Encode(const char *zSrc,sxu32 nLen,ProcConsumer xConsumer,void *pUserData);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SyStrToReal(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyBinaryStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyOctalStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyHexStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyHexToint(sxi32 c);
PH7_PRIVATE sxi32 SyStrToInt64(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyStrToInt32(const char *zSrc,sxu32 nLen,void *pOutVal,const char **zRest);
PH7_PRIVATE sxi32 SyStrIsNumeric(const char *zSrc,sxu32 nLen,sxu8 *pReal,const char **pzTail);
PH7_PRIVATE SyHashEntry *SyHashLastEntry(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashInsert(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void *pUserData);
PH7_PRIVATE sxi32 SyHashForEach(SyHash *pHash,sxi32(*xStep)(SyHashEntry *,void *),void *pUserData);
PH7_PRIVATE SyHashEntry *SyHashGetNextEntry(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashResetLoopCursor(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashDeleteEntry2(SyHashEntry *pEntry);
PH7_PRIVATE sxi32 SyHashDeleteEntry(SyHash *pHash,const void *pKey,sxu32 nKeyLen,void **ppUserData);
PH7_PRIVATE SyHashEntry *SyHashGet(SyHash *pHash,const void *pKey,sxu32 nKeyLen);
PH7_PRIVATE sxi32 SyHashRelease(SyHash *pHash);
PH7_PRIVATE sxi32 SyHashInit(SyHash *pHash,SyMemBackend *pAllocator,ProcHash xHash,ProcCmp xCmp);
PH7_PRIVATE sxu32 SyStrHash(const void *pSrc,sxu32 nLen);
PH7_PRIVATE void *SySetAt(SySet *pSet,sxu32 nIdx);
PH7_PRIVATE void *SySetPop(SySet *pSet);
PH7_PRIVATE void *SySetPeek(SySet *pSet);
PH7_PRIVATE sxi32 SySetRelease(SySet *pSet);
PH7_PRIVATE sxi32 SySetReset(SySet *pSet);
PH7_PRIVATE sxi32 SySetResetCursor(SySet *pSet);
PH7_PRIVATE sxi32 SySetGetNextEntry(SySet *pSet,void **ppEntry);
#ifndef PH7_DISABLE_BUILTIN_FUNC 
PH7_PRIVATE void * SySetPeekCurrentEntry(SySet *pSet);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
PH7_PRIVATE sxi32 SySetTruncate(SySet *pSet,sxu32 nNewSize);
PH7_PRIVATE sxi32 SySetAlloc(SySet *pSet,sxi32 nItem);
PH7_PRIVATE sxi32 SySetPut(SySet *pSet,const void *pItem);
PH7_PRIVATE sxi32 SySetInit(SySet *pSet,SyMemBackend *pAllocator,sxu32 ElemSize);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyBlobSearch(const void *pBlob,sxu32 nLen,const void *pPattern,sxu32 pLen,sxu32 *pOfft);
#endif
PH7_PRIVATE sxi32 SyBlobRelease(SyBlob *pBlob);
PH7_PRIVATE sxi32 SyBlobReset(SyBlob *pBlob);
PH7_PRIVATE sxi32 SyBlobCmp(SyBlob *pLeft,SyBlob *pRight);
PH7_PRIVATE sxi32 SyBlobDup(SyBlob *pSrc,SyBlob *pDest);
PH7_PRIVATE sxi32 SyBlobNullAppend(SyBlob *pBlob);
PH7_PRIVATE sxi32 SyBlobAppend(SyBlob *pBlob,const void *pData,sxu32 nSize);
PH7_PRIVATE sxi32 SyBlobReadOnly(SyBlob *pBlob,const void *pData,sxu32 nByte);
PH7_PRIVATE sxi32 SyBlobInit(SyBlob *pBlob,SyMemBackend *pAllocator);
PH7_PRIVATE sxi32 SyBlobInitFromBuf(SyBlob *pBlob,void *pBuffer,sxu32 nSize);
PH7_PRIVATE char *SyMemBackendStrDup(SyMemBackend *pBackend,const char *zSrc,sxu32 nSize);
PH7_PRIVATE void *SyMemBackendDup(SyMemBackend *pBackend,const void *pSrc,sxu32 nSize);
PH7_PRIVATE sxi32 SyMemBackendRelease(SyMemBackend *pBackend);
PH7_PRIVATE sxi32 SyMemBackendInitFromOthers(SyMemBackend *pBackend,const SyMemMethods *pMethods,ProcMemError xMemErr,void *pUserData);
PH7_PRIVATE sxi32 SyMemBackendInit(SyMemBackend *pBackend,ProcMemError xMemErr,void *pUserData);
PH7_PRIVATE sxi32 SyMemBackendInitFromParent(SyMemBackend *pBackend,SyMemBackend *pParent);
#if 0
/* Not used in the current release of the PH7 engine */
PH7_PRIVATE void *SyMemBackendPoolRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);
#endif
PH7_PRIVATE sxi32 SyMemBackendPoolFree(SyMemBackend *pBackend,void *pChunk);
PH7_PRIVATE void *SyMemBackendPoolAlloc(SyMemBackend *pBackend,sxu32 nByte);
PH7_PRIVATE sxi32 SyMemBackendFree(SyMemBackend *pBackend,void *pChunk);
PH7_PRIVATE void *SyMemBackendRealloc(SyMemBackend *pBackend,void *pOld,sxu32 nByte);
PH7_PRIVATE void *SyMemBackendAlloc(SyMemBackend *pBackend,sxu32 nByte);
#if defined(PH7_ENABLE_THREADS)
PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);
PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);
#endif
PH7_PRIVATE sxu32 SyMemcpy(const void *pSrc,void *pDest,sxu32 nLen);
PH7_PRIVATE sxi32 SyMemcmp(const void *pB1,const void *pB2,sxu32 nSize);
PH7_PRIVATE void SyZero(void *pSrc,sxu32 nSize);
PH7_PRIVATE sxi32 SyStrnicmp(const char *zLeft,const char *zRight,sxu32 SLen);
PH7_PRIVATE sxi32 SyStrnmicmp(const void *pLeft, const void *pRight,sxu32 SLen);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyStrncmp(const char *zLeft,const char *zRight,sxu32 nLen);
#endif
PH7_PRIVATE sxi32 SyByteListFind(const char *zSrc,sxu32 nLen,const char *zList,sxu32 *pFirstPos);
#ifndef PH7_DISABLE_BUILTIN_FUNC
PH7_PRIVATE sxi32 SyByteFind2(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);
#endif
PH7_PRIVATE sxi32 SyByteFind(const char *zStr,sxu32 nLen,sxi32 c,sxu32 *pPos);
PH7_PRIVATE sxu32 SyStrlen(const char *zSrc);
#if defined(PH7_ENABLE_THREADS)
PH7_PRIVATE const SyMutexMethods *SyMutexExportMethods(void);
PH7_PRIVATE sxi32 SyMemBackendMakeThreadSafe(SyMemBackend *pBackend,const SyMutexMethods *pMethods);
PH7_PRIVATE sxi32 SyMemBackendDisbaleMutexing(SyMemBackend *pBackend);
#endif
#endif /* __PH7INT_H__ */
