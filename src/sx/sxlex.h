/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __SXLEX_H__
#define __SXLEX_H__

#include "sxtypes.h"
#include "sxset.h"

/* Forward declarations */
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
	const unsigned char *zText;  /* Current input we are processing */
	const unsigned char *zEnd;   /* End of input marker */
	sxu32  nLine;                /* Total number of processed lines */
	sxu32  nIgn;                 /* Total number of ignored tokens */
	SySet *pSet;                 /* Token containers */
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

/* Lexer access macros */
#define SyLexTotalToken(LEX)    SySetTotalEntry(&(LEX)->aTokenSet)
#define SyLexTotalLines(LEX)    ((LEX)->sStream.nLine)
#define SyLexTotalIgnored(LEX)  ((LEX)->sStream.nIgn)
#define XLEX_IN_LEN(STREAM)     (sxu32)(STREAM->zEnd - STREAM->zText)

/* Lexer function prototypes */
PH7_PRIVATE sxi32 SyLexInit(SyLex *pLex,SySet *pSet,ProcTokenizer xTokenizer,void *pUserData);
PH7_PRIVATE sxi32 SyLexTokenizeInput(SyLex *pLex,const char *zInput,sxu32 nLen,void *pCtxData,ProcSort xSort,ProcCmp xCmp);
PH7_PRIVATE sxi32 SyLexRelease(SyLex *pLex);

#endif /* __SXLEX_H__ */
