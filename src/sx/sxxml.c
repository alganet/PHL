/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "sxtypes.h"
#include "sxmacros.h"
#include "sxset.h"
#include "sxmem.h"
#include "sxhashtable.h"
#include "sxlex.h"
#include "sxxml.h"
#include "sxstr.h"
#include "sxutils.h"

#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
* Symisc XML Parser Engine (UTF-8) SAX(Event Driven) API
* @author Mrad Chems Eddine <chm@symisc.net>
* @started 08/03/2010 21:32 FreeBSD
* @finished	07/04/2010 23:24 Win32[VS8]
*/
/*
 * An XML raw text,CDATA,tag name is parsed out and stored
 * in an instance of the following structure.
 */
typedef struct SyXMLRawStrNS SyXMLRawStrNS;
struct SyXMLRawStrNS
{
	/* Public field [Must match the SyXMLRawStr fields ] */
	const char *zString; /* Raw text [UTF-8 ENCODED EXCEPT CDATA] [NOT NULL TERMINATED] */
	sxu32 nByte; /* Text length */
	sxu32 nLine; /* Line number this text occurs */
	/* Private fields */
	SySet sNSset; /* Namespace entries */
};
/*
 * Lexer token codes
 * The following set of constants are the token value recognized
 * by the lexer when processing XML input.
 */
#define SXML_TOK_INVALID	0xFFFF /* Invalid Token */
#define SXML_TOK_COMMENT	0x01   /* Comment */
#define SXML_TOK_PI	        0x02   /* Processing instruction */
#define SXML_TOK_DOCTYPE	0x04   /* Doctype directive */
#define SXML_TOK_RAW		0x08   /* Raw text */
#define SXML_TOK_START_TAG	0x10   /* Starting tag */
#define SXML_TOK_CDATA		0x20   /* CDATA */
#define SXML_TOK_END_TAG	0x40   /* Ending tag */
#define SXML_TOK_START_END	0x80   /* Tag */
#define SXML_TOK_SPACE		0x100  /* Spaces (including new lines) */
#define IS_XML_DIRTY(c) \
	( c == '<' || c == '$'|| c == '"' || c == '\''|| c == '&'|| c == '(' || c == ')' || c == '*' ||\
	c == '%'  || c == '#' || c == '|' || c == '/'|| c == '~' || c == '{' || c == '}' ||\
	c == '['  || c == ']' || c == '\\'|| c == ';'||c == '^'  || c == '`' )
/* Tokenize an entire XML input */
static sxi32 XML_Tokenize(SyStream *pStream,SyToken *pToken,void *pUserData,void *pUnused2)
{
	SyXMLParser *pParse = (SyXMLParser *)pUserData;
	SyString *pStr;
	sxi32 rc;
	int c;
	/* Jump leading white spaces */
	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){
		/* Advance the stream cursor */
		if( pStream->zText[0] == '\n' ){
			/* Increment line counter */
			pStream->nLine++;
		}
		pStream->zText++;
	}
	if( pStream->zText >= pStream->zEnd ){
		SXUNUSED(pUnused2);
		/* End of input reached */
		return SXERR_EOF;
	}
	/* Record token starting position and line */
	pToken->nLine = pStream->nLine;
	pToken->pUserData = 0;
	pStr = &pToken->sData;
	SyStringInitFromBuf(pStr,pStream->zText,0);
	/* Extract the current token */
	c = pStream->zText[0];
	if( c == '<' ){
		pStream->zText++;
		pStr->zString++;
		if( pStream->zText >= pStream->zEnd ){
			if( pParse->xError ){
				rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			/* End of input reached */
			return SXERR_EOF;
		}
		c = pStream->zText[0];
		if( c == '?' ){
			/* Processing instruction */
			pStream->zText++;
			pStr->zString++;
			pToken->nType = SXML_TOK_PI;
			while( XLEX_IN_LEN(pStream) >= sizeof("?>")-1 &&
				SyMemcmp((const void *)pStream->zText,"?>",sizeof("?>")-1) != 0 ){
					if( pStream->zText[0] == '\n' ){
						/* Increment line counter */
						pStream->nLine++;
					}
					pStream->zText++;
			}
			/* Record token length */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
			if( XLEX_IN_LEN(pStream) < sizeof("?>")-1 ){
				if( pParse->xError ){
					rc = pParse->xError("End of input found,but processing instruction was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				return SXERR_EOF;
			}
			pStream->zText += sizeof("?>")-1;
		}else if( c == '!' ){
			pStream->zText++;
			if( XLEX_IN_LEN(pStream) >= sizeof("--")-1 && pStream->zText[0] == '-' && pStream->zText[1] == '-' ){
				/* Comment */
				pStream->zText += sizeof("--") - 1;
				while( XLEX_IN_LEN(pStream) >= sizeof("-->")-1 &&
					SyMemcmp((const void *)pStream->zText,"-->",sizeof("-->")-1) != 0 ){
						if( pStream->zText[0] == '\n' ){
							/* Increment line counter */
							pStream->nLine++;
						}
						pStream->zText++;
				}
				pStream->zText += sizeof("-->")-1;
				/* Tell the lexer to ignore this token */
				return SXERR_CONTINUE;
			}
			if( XLEX_IN_LEN(pStream) >= sizeof("[CDATA[") - 1 && SyMemcmp((const void *)pStream->zText,"[CDATA[",sizeof("[CDATA[")-1) == 0 ){
				/* CDATA */
				pStream->zText += sizeof("[CDATA[") - 1;
				pStr->zString = (const char *)pStream->zText;
				while( XLEX_IN_LEN(pStream) >= sizeof("]]>")-1 &&
					SyMemcmp((const void *)pStream->zText,"]]>",sizeof("]]>")-1) != 0 ){
						if( pStream->zText[0] == '\n' ){
							/* Increment line counter */
							pStream->nLine++;
						}
						pStream->zText++;
				}
				/* Record token type and length */
				pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
				pToken->nType = SXML_TOK_CDATA;
				if( XLEX_IN_LEN(pStream) < sizeof("]]>")-1 ){
					if( pParse->xError ){
						rc = pParse->xError("End of input found,but ]]> was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}
					return SXERR_EOF;
				}
				pStream->zText += sizeof("]]>")-1;
				return SXRET_OK;
			}
			if( XLEX_IN_LEN(pStream) >= sizeof("DOCTYPE") - 1 && SyMemcmp((const void *)pStream->zText,"DOCTYPE",sizeof("DOCTYPE")-1) == 0 ){
				SyString sDelim = { ">" , sizeof(char) }; /* Default delimiter */
				int c0 = 0;
				/* DOCTYPE */
				pStream->zText += sizeof("DOCTYPE") - 1;
				pStr->zString = (const char *)pStream->zText;
				/* Check for element declaration */
				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){
					if( pStream->zText[0] >= 0xc0 || !SyisSpace(pStream->zText[0]) ){
						c0 = pStream->zText[0];
						if( c0 == '>' ){
							break;
						}
					}
					pStream->zText++;
				}
				if( c0 == '[' ){
					/* Change the delimiter */
					SyStringInitFromBuf(&sDelim,"]>",sizeof("]>")-1);
				}
				if( c0 != '>' ){
					while( XLEX_IN_LEN(pStream) >= sDelim.nByte &&
						SyMemcmp((const void *)pStream->zText,sDelim.zString,sDelim.nByte) != 0 ){
							if( pStream->zText[0] == '\n' ){
								/* Increment line counter */
								pStream->nLine++;
							}
							pStream->zText++;
					}
				}
				/* Record token type and length */
				pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
				pToken->nType = SXML_TOK_DOCTYPE;
				if( XLEX_IN_LEN(pStream) < sDelim.nByte ){
					if( pParse->xError ){
						rc = pParse->xError("End of input found,but ]> or > was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}
					return SXERR_EOF;
				}
				pStream->zText += sDelim.nByte;
				return SXRET_OK;
			}
			}else{
			/* reuse function-scope variable 'c' declared at top of function */
			c = pStream->zText[0];
			rc = SXRET_OK;
			pToken->nType = SXML_TOK_START_TAG;
			if( c == '/' ){
				/* End tag */
				pToken->nType = SXML_TOK_END_TAG;
				pStream->zText++;
				pStr->zString++;
				if( pStream->zText >= pStream->zEnd ){
					if( pParse->xError ){
						rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}
					return SXERR_EOF;
				}
				c = pStream->zText[0];
			}
			if( c == '>' ){
				/*<>*/
				if( pParse->xError ){
					rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				/* Ignore the token */
				return SXERR_CONTINUE;
			}
			if( c < 0xc0 && (SyisSpace(c) || SyisDigit(c) || c == '.' || c == '-' ||IS_XML_DIRTY(c) ) ){
				if( pParse->xError ){
					rc = pParse->xError("Illegal syntax,expecting valid start name character",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				rc = SXERR_INVALID;
			}
			pStream->zText++;
			/* Delimit the tag */
			while( pStream->zText < pStream->zEnd && pStream->zText[0] != '>' ){
				c = pStream->zText[0];
				if( c >= 0xc0 ){
					/* UTF-8 stream */
					pStream->zText++;
					SX_JMP_UTF8(pStream->zText,pStream->zEnd);
				}else{
					if( c == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '>' ){
						pStream->zText++;
						if( pToken->nType != SXML_TOK_START_TAG ){
							if( pParse->xError ){
								rc = pParse->xError("Unexpected closing tag,expecting '>'",
									SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
								if( rc == SXERR_ABORT ){
									return SXERR_ABORT;
								}
							}
							/* Ignore the token */
							rc = SXERR_INVALID;
						}else{
							pToken->nType = SXML_TOK_START_END;
						}
						break;
					}
					if( pStream->zText[0] == '\n' ){
						/* Increment line counter */
						pStream->nLine++;
					}
					/* Advance the stream cursor */
					pStream->zText++;
				}
			}
			if( rc != SXRET_OK ){
				/* Tell the lexer to ignore this token */
				return SXERR_CONTINUE;
			}
			/* Record token length */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
			if( pToken->nType == SXML_TOK_START_END && pStr->nByte > 0){
				pStr->nByte -= sizeof(char);
			}
			if ( pStream->zText < pStream->zEnd ){
				pStream->zText++;
			}else{
				if( pParse->xError ){
					rc = pParse->xError("End of input found,but closing tag '>' was not found",SXML_ERROR_UNCLOSED_TOKEN,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}
		}
	}else{
		/* Raw input */
		while( pStream->zText < pStream->zEnd ){
			c = pStream->zText[0];
			if( c < 0xc0 ){
				if( c == '<' ){
					break;
				}else if( c == '\n' ){
					/* Increment line counter */
					pStream->nLine++;
				}
				/* Advance the stream cursor */
				pStream->zText++;
			}else{
				/* UTF-8 stream */
				pStream->zText++;
				SX_JMP_UTF8(pStream->zText,pStream->zEnd);
			}
		}
		/* Record token type,length */
		pToken->nType = SXML_TOK_RAW;
		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
	}
	/* Return to the lexer */
	return SXRET_OK;
}
static int XMLCheckDuplicateAttr(SyXMLRawStr *aSet,sxu32 nEntry,SyXMLRawStr *pEntry)
{
	sxu32 n;
	for( n = 0 ; n < nEntry ; n += 2 ){
		SyXMLRawStr *pAttr = &aSet[n];
		if( pAttr->nByte == pEntry->nByte && SyMemcmp(pAttr->zString,pEntry->zString,pEntry->nByte) == 0 ){
			/* Attribute found */
			return 1;
		}
	}
	/* No duplicates */
	return 0;
}
static sxi32 XMLProcessNamesSpace(SyXMLParser *pParse,SyXMLRawStrNS *pTag,SyToken *pToken,SySet *pAttr)
{
	SyXMLRawStr *pPrefix,*pUri; /* Namespace prefix/URI */
	SyHashEntry *pEntry;
	SyXMLRawStr *pDup;
	sxi32 rc;
	/* Extract the URI first */
	pUri = (SyXMLRawStr *)SySetPeek(pAttr);
	/* Extract the prefix */
	pPrefix =  (SyXMLRawStr *)SySetAt(pAttr,SySetUsed(pAttr) - 2);
	/* Prefix name */
	if( pPrefix->nByte == sizeof("xmlns")-1 ){
		/* Default namespace */
		pPrefix->nByte = 0;
		pPrefix->zString = ""; /* Empty string */
	}else{
		pPrefix->nByte   -= sizeof("xmlns")-1;
		pPrefix->zString += sizeof("xmlns")-1;
		if( pPrefix->zString[0] != ':' ){
			return SXRET_OK;
		}
		pPrefix->nByte--;
		pPrefix->zString++;
		if( pPrefix->nByte < 1 ){
			if( pParse->xError ){
				rc = pParse->xError("Invalid namespace name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			/* POP the last inserted two entries */
			(void)SySetPop(pAttr);
			(void)SySetPop(pAttr);
			return SXERR_SYNTAX;
		}
	}
	/* Invoke the namespace callback if available */
	if( pParse->xNameSpace ){
		rc = pParse->xNameSpace(pPrefix,pUri,pParse->pUserData);
		if( rc == SXERR_ABORT ){
			/* User callback request an operation abort */
			return SXERR_ABORT;
		}
	}
	/* Duplicate structure */
	pDup = (SyXMLRawStr *)SyMemBackendAlloc(pParse->pAllocator,sizeof(SyXMLRawStr));
	if( pDup == 0 ){
		if( pParse->xError ){
			pParse->xError("Out of memory",SXML_ERROR_NO_MEMORY,pToken,pParse->pUserData);
		}
		/* Abort processing immediately */
		return SXERR_ABORT;
	}
	*pDup = *pUri; /* Structure assignement */
	/* Save the namespace */
	if( pPrefix->nByte == 0 ){
		pPrefix->zString = "Default";
		pPrefix->nByte = sizeof("Default")-1;
	}
	SyHashInsert(&pParse->hns,(const void *)pPrefix->zString,pPrefix->nByte,pDup);
	/* Peek the last inserted entry */
	pEntry = SyHashLastEntry(&pParse->hns);
	/* Store in the corresponding tag container*/
	SySetPut(&pTag->sNSset,(const void *)&pEntry);
	/* POP the last inserted two entries */
	(void)SySetPop(pAttr);
	(void)SySetPop(pAttr);
	return SXRET_OK;
}
static sxi32 XMLProcessStartTag(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pTag,SySet  *pAttrSet,SySet *pTagStack)
{
	SyString *pIn = &pToken->sData;
	const char *zIn,*zCur,*zEnd;
	SyXMLRawStr sEntry;
	sxi32 rc;
	int c;
	/* Reset the working set */
	SySetReset(pAttrSet);
	/* Delimit the raw tag */
	zIn = pIn->zString;
	zEnd = &zIn[pIn->nByte];
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		zIn++;
	}
	/* Isolate tag name */
	sEntry.nLine = pTag->nLine = pToken->nLine;
	zCur = zIn;
	while( zIn < zEnd ){
		if( (unsigned char)zIn[0] >= 0xc0 ){
			/* UTF-8 stream */
			zIn++;
			SX_JMP_UTF8(zIn,zEnd);
		}else if( SyisSpace(zIn[0])){
			break;
		}else{
			if( IS_XML_DIRTY(zIn[0]) ){
				if( pParse->xError ){
					rc = pParse->xError("Illegal character in XML name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}
			zIn++;
		}
	}
	if( zCur >= zIn ){
		if( pParse->xError ){
			rc = pParse->xError("Invalid XML name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	pTag->zString = zCur;
	pTag->nByte = (sxu32)(zIn-zCur);
	/* Process tag attribute */
	for(;;){
		int is_ns = 0;
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
			zIn++;
		}
		if( zIn >= zEnd ){
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '=' ){
			if( (unsigned char)zIn[0] >= 0xc0 ){
				/* UTF-8 stream */
				zIn++;
				SX_JMP_UTF8(zIn,zEnd);
			}else if( SyisSpace(zIn[0]) ){
				break;
			}else{
				zIn++;
			}
		}
		if( zCur >= zIn ){
			if( pParse->xError ){
				rc = pParse->xError("Missing attribute name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			return SXERR_SYNTAX;
		}
		/* Store attribute name */
		sEntry.zString = zCur;
		sEntry.nByte = (sxu32)(zIn-zCur);
		if( (pParse->nFlags & SXML_ENABLE_NAMESPACE) && sEntry.nByte >= sizeof("xmlns") - 1 &&
			SyMemcmp(sEntry.zString,"xmlns",sizeof("xmlns") - 1) == 0 ){
				is_ns = 1;
		}
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
			zIn++;
		}
		if( zIn >= zEnd || zIn[0] != '=' ){
			if( pParse->xError ){
				rc = pParse->xError("Missing attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			return SXERR_SYNTAX;
		}
		while( sEntry.nByte > 0 && (unsigned char)zCur[sEntry.nByte - 1] < 0xc0
			&& SyisSpace(zCur[sEntry.nByte - 1])){
				sEntry.nByte--;
		}
		/* Check for duplicates first */
		if( XMLCheckDuplicateAttr((SyXMLRawStr *)SySetBasePtr(pAttrSet),SySetUsed(pAttrSet),&sEntry) ){
			if( pParse->xError ){
				rc = pParse->xError("Duplicate attribute",SXML_ERROR_DUPLICATE_ATTRIBUTE,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			return SXERR_SYNTAX;
		}
		if( SXRET_OK != SySetPut(pAttrSet,(const void *)&sEntry) ){
			return SXERR_ABORT;
		}
		/* Extract attribute value */
		zIn++; /* Jump the trailing '=' */
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
			zIn++;
		}
		if( zIn >= zEnd ){
			if( pParse->xError ){
				rc = pParse->xError("Missing attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			(void)SySetPop(pAttrSet);
			return SXERR_SYNTAX;
		}
		if( zIn[0] != '\'' && zIn[0] != '"' ){
			if( pParse->xError ){
				rc = pParse->xError("Missing quotes on attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			(void)SySetPop(pAttrSet);
			return SXERR_SYNTAX;
		}
		c = zIn[0];
		zIn++;
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != c ){
			zIn++;
		}
		if( zIn >= zEnd ){
			if( pParse->xError ){
				rc = pParse->xError("Missing quotes on attribute value",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			(void)SySetPop(pAttrSet);
			return SXERR_SYNTAX;
		}
		/* Store attribute value */
		sEntry.zString = zCur;
		sEntry.nByte = (sxu32)(zIn-zCur);
		if( SXRET_OK != SySetPut(pAttrSet,(const void *)&sEntry) ){
			return SXERR_ABORT;
		}
		zIn++;
		if( is_ns ){
			/* Process namespace declaration */
			XMLProcessNamesSpace(pParse,pTag,pToken,pAttrSet);
		}
	}
	/* Store in the tag stack */
	if( pToken->nType == SXML_TOK_START_TAG ){
		rc = SySetPut(pTagStack,(const void *)pTag);
	}
	return SXRET_OK;
}
static void XMLExtactPI(SyToken *pToken,SyXMLRawStr *pTarget,SyXMLRawStr *pData,int *pXML)
{
	SyString *pIn = &pToken->sData;
	const char *zIn,*zCur,*zEnd;

	pTarget->nLine = pData->nLine = pToken->nLine;
	/* Nullify the entries first */
	pTarget->zString = pData->zString = 0;
	/* Ignore leading and trailing white spaces */
	SyStringFullTrim(pIn);
	/* Delimit the raw PI */
	zIn  = pIn->zString;
	zEnd = &zIn[pIn->nByte];
	if( pXML ){
		*pXML = 0;
	}
	/* Extract the target */
	zCur = zIn;
	while( zIn < zEnd ){
		if( (unsigned char)zIn[0] >= 0xc0 ){
			/* UTF-8 stream */
			zIn++;
			SX_JMP_UTF8(zIn,zEnd);
		}else if( SyisSpace(zIn[0])){
			break;
		}else{
			zIn++;
		}
	}
	if( zIn > zCur ){
		pTarget->zString = zCur;
		pTarget->nByte = (sxu32)(zIn-zCur);
		if( pXML && pTarget->nByte == sizeof("xml")-1 && SyStrnicmp(pTarget->zString,"xml",sizeof("xml")-1) == 0 ){
			*pXML = 1;
		}
	}
	/* Extract the PI data  */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		zIn++;
	}
	if( zIn < zEnd ){
		pData->zString = zIn;
		pData->nByte = (sxu32)(zEnd-zIn);
	}
}
static sxi32 XMLExtractEndTag(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pOut)
{
	SyString *pIn = &pToken->sData;
	const char *zEnd = &pIn->zString[pIn->nByte];
	const char *zIn = pIn->zString;
	/* Ignore leading white spaces */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		zIn++;
	}
	pOut->nLine = pToken->nLine;
	pOut->zString = zIn;
	pOut->nByte = (sxu32)(zEnd-zIn);
	/* Ignore trailing white spaces */
	while( pOut->nByte > 0 && (unsigned char)pOut->zString[pOut->nByte - 1] < 0xc0
		&& SyisSpace(pOut->zString[pOut->nByte - 1]) ){
			pOut->nByte--;
	}
	if( pOut->nByte < 1 ){
		if( pParse->xError ){
			sxi32 rc;
			rc  = pParse->xError("Invalid end tag name",SXML_ERROR_INVALID_TOKEN,pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	return SXRET_OK;
}
static void TokenToXMLString(SyToken *pTok,SyXMLRawStrNS *pOut)
{
	/* Remove leading and trailing white spaces first */
	SyStringFullTrim(&pTok->sData);
	pOut->zString = SyStringData(&pTok->sData);
	pOut->nByte = SyStringLength(&pTok->sData);
}
static sxi32 XMLExtractNS(SyXMLParser *pParse,SyToken *pToken,SyXMLRawStrNS *pTag,SyXMLRawStr *pnsUri)
{
	SyXMLRawStr *pUri,sPrefix;
	SyHashEntry *pEntry;
	sxu32 nOfft;
	sxi32 rc;
	/* Extract a prefix if available */
	rc = SyByteFind(pTag->zString,pTag->nByte,':',&nOfft);
	if( rc != SXRET_OK ){
		/* Check if there is a default namespace */
		pEntry = SyHashGet(&pParse->hns,"Default",sizeof("Default")-1);
		if( pEntry  ){
			/* Extract the ns URI */
			pUri = (SyXMLRawStr *)pEntry->pUserData;
			/* Save the ns URI */
			pnsUri->zString = pUri->zString;
			pnsUri->nByte = pUri->nByte;
		}
		return SXRET_OK;
	}
	if( nOfft < 1 ){
		if( pParse->xError ){
			rc = pParse->xError("Empty prefix is not allowed according to XML namespace specification",
				SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	sPrefix.zString = pTag->zString;
	sPrefix.nByte = nOfft;
	sPrefix.nLine = pTag->nLine;
	pTag->zString += nOfft + 1;
	pTag->nByte -= nOfft;
	if( pTag->nByte < 1 ){
		if( pParse->xError ){
			rc = pParse->xError("Missing tag name",SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	/* Check if the prefix is already registered */
	pEntry = SyHashGet(&pParse->hns,sPrefix.zString,sPrefix.nByte);
	if( pEntry == 0 ){
		if( pParse->xError ){
			rc = pParse->xError("Namespace prefix is not defined",SXML_ERROR_SYNTAX,
				pToken,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		return SXERR_SYNTAX;
	}
	/* Extract the ns URI */
	pUri = (SyXMLRawStr *)pEntry->pUserData;
	/* Save the ns URI */
	pnsUri->zString = pUri->zString;
	pnsUri->nByte = pUri->nByte;
	/* All done */
	return SXRET_OK;
}
static sxi32 XMLnsUnlink(SyXMLParser *pParse,SyXMLRawStrNS *pLast,SyToken *pToken)
{
	SyHashEntry **apEntry,*pEntry;
	void *pUserData;
	sxu32 n;
	/* Release namespace entries */
	apEntry = (SyHashEntry **)SySetBasePtr(&pLast->sNSset);
	for( n = 0 ; n < SySetUsed(&pLast->sNSset) ; ++n ){
		pEntry = apEntry[n];
		/* Invoke the end namespace declaration callback */
		if( pParse->xNameSpaceEnd && (pParse->nFlags & SXML_ENABLE_NAMESPACE) && pToken ){
			SyXMLRawStr sPrefix;
			sxi32 rc;
			sPrefix.zString = (const char *)pEntry->pKey;
			sPrefix.nByte = pEntry->nKeyLen;
			sPrefix.nLine = pToken->nLine;
			rc = pParse->xNameSpaceEnd(&sPrefix,pParse->pUserData);
			if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
		}
		pUserData = pEntry->pUserData;
		/* Remove from the namespace hashtable */
		SyHashDeleteEntry2(pEntry);
		SyMemBackendFree(pParse->pAllocator,pUserData);
	}
	SySetRelease(&pLast->sNSset);
	return SXRET_OK;
}
/* Process XML tokens */
static sxi32  ProcessXML(SyXMLParser *pParse,SySet *pTagStack,SySet *pWorker)
{
	SySet *pTokenSet = &pParse->sToken;
	SyXMLRawStrNS sEntry;
	SyXMLRawStr sNs;
	SyToken *pToken;
	int bGotTag;
	sxi32 rc;
	/* Initialize fields */
	bGotTag = 0;
	/* Start processing */
	if( pParse->xStartDoc && (SXERR_ABORT == pParse->xStartDoc(pParse->pUserData)) ){
		/* User callback request an operation abort */
		return SXERR_ABORT;
	}
	/* Reset the loop cursor */
	SySetResetCursor(pTokenSet);
	/* Extract the current token */
	while( SXRET_OK == (SySetGetNextEntry(&(*pTokenSet),(void **)&pToken)) ){
		SyZero(&sEntry,sizeof(SyXMLRawStrNS));
		SyZero(&sNs,sizeof(SyXMLRawStr));
		SySetInit(&sEntry.sNSset,pParse->pAllocator,sizeof(SyHashEntry *));
		sEntry.nLine = sNs.nLine = pToken->nLine;
		switch(pToken->nType){
		case SXML_TOK_DOCTYPE:
			if( SySetUsed(pTagStack) > 1 || bGotTag ){
				if( pParse->xError ){
					rc = pParse->xError("DOCTYPE must be declared first",SXML_ERROR_MISPLACED_XML_PI,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				break;
			}
			/* Invoke the supplied callback if any */
			if( pParse->xDoctype ){
				TokenToXMLString(pToken,&sEntry);
				rc = pParse->xDoctype((SyXMLRawStr *)&sEntry,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
		case SXML_TOK_CDATA:
			if( SySetUsed(pTagStack) < 1 ){
				if( pParse->xError ){
					rc = pParse->xError("CDATA without matching tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}
			/* Invoke the supplied callback if any */
			if( pParse->xRaw ){
				TokenToXMLString(pToken,&sEntry);
				rc = pParse->xRaw((SyXMLRawStr *)&sEntry,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
		case SXML_TOK_PI:{
			SyXMLRawStr sTarget,sData;
			int isXML = 0;
			/* Extract the target and data */
			XMLExtactPI(pToken,&sTarget,&sData,&isXML);
			if( isXML && SySetCursor(pTokenSet) > 1 ){
				if( pParse->xError ){
					rc = pParse->xError("Unexpected XML declaration. The XML declaration must be the first node in the document",
						SXML_ERROR_MISPLACED_XML_PI,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}else if( pParse->xPi ){
				/* Invoke the supplied callback*/
				rc = pParse->xPi(&sTarget,&sData,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
						 }
		case SXML_TOK_RAW:
			if( SySetUsed(pTagStack) < 1 ){
				if( pParse->xError ){
					rc = pParse->xError("Text (Raw data) without matching tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				break;
			}
			/* Invoke the supplied callback if any */
			if( pParse->xRaw ){
				TokenToXMLString(pToken,&sEntry);
				rc = pParse->xRaw((SyXMLRawStr *)&sEntry,pParse->pUserData);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
		case SXML_TOK_END_TAG:{
			SyXMLRawStrNS *pLast = 0; /* cc warning */
			if( SySetUsed(pTagStack) < 1 ){
				if( pParse->xError ){
					rc = pParse->xError("Unexpected closing tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				break;
			}
			rc = XMLExtractEndTag(pParse,pToken,&sEntry);
			if( rc == SXRET_OK ){
				/* Extract the last inserted entry */
				pLast = (SyXMLRawStrNS *)SySetPeek(pTagStack);
				if( pLast == 0 || pLast->nByte != sEntry.nByte ||
					SyMemcmp(pLast->zString,sEntry.zString,sEntry.nByte) != 0 ){
						if( pParse->xError ){
							rc = pParse->xError("Unexpected closing tag",SXML_ERROR_TAG_MISMATCH,pToken,pParse->pUserData);
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
						}
				}else{
					/* Invoke the supplied callback if any */
					if( pParse->xEndTag ){
						rc = SXRET_OK;
						if( pParse->nFlags & SXML_ENABLE_NAMESPACE ){
							/* Extract namespace URI */
							rc = XMLExtractNS(pParse,pToken,&sEntry,&sNs);
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
						}
						if( rc == SXRET_OK ){
							rc = pParse->xEndTag((SyXMLRawStr *)&sEntry,&sNs,pParse->pUserData);
							if( rc == SXERR_ABORT ){
								return SXERR_ABORT;
							}
						}
					}
				}
			}else if( rc == SXERR_ABORT ){
				return SXERR_ABORT;
			}
			if( pLast ){
				rc = XMLnsUnlink(pParse,pLast,pToken);
				(void)SySetPop(pTagStack);
				if( rc == SXERR_ABORT ){
					return SXERR_ABORT;
				}
			}
			break;
							  }
		case SXML_TOK_START_TAG:
		case SXML_TOK_START_END:
			if( SySetUsed(pTagStack) < 1 && bGotTag ){
				if( pParse->xError ){
					rc = pParse->xError("XML document cannot contain multiple root level elements documents",
						SXML_ERROR_SYNTAX,pToken,pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				break;
			}
			bGotTag = 1;
			/* Extract the tag and it's supplied attribute */
			rc = XMLProcessStartTag(pParse,pToken,&sEntry,pWorker,pTagStack);
			if( rc == SXRET_OK ){
				if( pParse->nFlags & SXML_ENABLE_NAMESPACE ){
					/* Extract namespace URI */
					rc = XMLExtractNS(pParse,pToken,&sEntry,&sNs);
				}
			}
			if( rc == SXRET_OK ){
				/* Invoke the supplied callback */
				if( pParse->xStartTag ){
					rc = pParse->xStartTag((SyXMLRawStr *)&sEntry,&sNs,SySetUsed(pWorker),
						(SyXMLRawStr *)SySetBasePtr(pWorker),pParse->pUserData);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
				if( pToken->nType == SXML_TOK_START_END ){
					if ( pParse->xEndTag ){
						rc = pParse->xEndTag((SyXMLRawStr *)&sEntry,&sNs,pParse->pUserData);
						if( rc == SXERR_ABORT ){
							return SXERR_ABORT;
						}
					}
					rc = XMLnsUnlink(pParse,&sEntry,pToken);
					if( rc == SXERR_ABORT ){
						return SXERR_ABORT;
					}
				}
			}else if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				return SXERR_ABORT;
			}
			break;
		default:
			/* Can't happen */
			break;
		}
	}
	if( SySetUsed(pTagStack) > 0 && pParse->xError){
		pParse->xError("Missing closing tag",SXML_ERROR_SYNTAX,
			(SyToken *)SySetPeek(&pParse->sToken),pParse->pUserData);
	}
	if( pParse->xEndDoc ){
		pParse->xEndDoc(pParse->pUserData);
	}
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyXMLParserInit(SyXMLParser *pParser,SyMemBackend *pAllocator,sxi32 iFlags)
{
	/* Zero the structure first */
	SyZero(pParser,sizeof(SyXMLParser));
	/* Initialize fields */
	SySetInit(&pParser->sToken,pAllocator,sizeof(SyToken));
	SyLexInit(&pParser->sLex,&pParser->sToken,XML_Tokenize,pParser);
	SyHashInit(&pParser->hns,pAllocator,0,0);
	pParser->pAllocator = pAllocator;
	pParser->nFlags = iFlags;
	return SXRET_OK;
}
PH7_PRIVATE sxi32 SyXMLParserSetEventHandler(SyXMLParser *pParser,
	void *pUserData,
	ProcXMLStartTagHandler xStartTag,
	ProcXMLTextHandler xRaw,
	ProcXMLSyntaxErrorHandler xErr,
	ProcXMLStartDocument xStartDoc,
	ProcXMLEndTagHandler xEndTag,
	ProcXMLPIHandler   xPi,
	ProcXMLEndDocument xEndDoc,
	ProcXMLDoctypeHandler xDoctype,
	ProcXMLNameSpaceStart xNameSpace,
	ProcXMLNameSpaceEnd   xNameSpaceEnd
	){
	/* Install user callbacks */
	if( xErr ){
		pParser->xError = xErr;
	}
	if( xStartDoc ){
		pParser->xStartDoc = xStartDoc;
	}
	if( xStartTag ){
		pParser->xStartTag = xStartTag;
	}
	if( xRaw ){
		pParser->xRaw = xRaw;
	}
	if( xEndTag ){
		pParser->xEndTag = xEndTag;
	}
	if( xPi ){
		pParser->xPi = xPi;
	}
	if( xEndDoc ){
		pParser->xEndDoc = xEndDoc;
	}
	if( xDoctype ){
		pParser->xDoctype = xDoctype;
	}
	if( xNameSpace ){
		pParser->xNameSpace	= xNameSpace;
	}
	if( xNameSpaceEnd ){
		pParser->xNameSpaceEnd = xNameSpaceEnd;
	}
	pParser->pUserData = pUserData;
	return SXRET_OK;
}
/* Process an XML chunk */
PH7_PRIVATE sxi32 SyXMLProcess(SyXMLParser *pParser,const char *zInput,sxu32 nByte)
{
	SySet sTagStack;
	SySet sWorker;
	sxi32 rc;
	/* Initialize working sets */
	SySetInit(&sWorker,pParser->pAllocator,sizeof(SyXMLRawStr)); /* Tag container */
	SySetInit(&sTagStack,pParser->pAllocator,sizeof(SyXMLRawStrNS)); /* Tag stack */
	/* Tokenize the entire input */
	rc = SyLexTokenizeInput(&pParser->sLex,zInput,nByte,0,0,0);
	if( rc == SXERR_ABORT ){
		/* Tokenize callback request an operation abort */
		return SXERR_ABORT;
	}
	if( SySetUsed(&pParser->sToken) < 1 ){
		/* Nothing to process [i.e: white spaces] */
		rc = SXRET_OK;
	}else{
		/* Process XML Tokens */
		rc = ProcessXML(&(*pParser),&sTagStack,&sWorker);
		if( pParser->nFlags & SXML_ENABLE_NAMESPACE ){
			if( SySetUsed(&sTagStack) > 0  ){
				SyXMLRawStrNS *pEntry;
				SyHashEntry **apEntry;
				sxu32 n;
				SySetResetCursor(&sTagStack);
				while( SySetGetNextEntry(&sTagStack,(void **)&pEntry) == SXRET_OK ){
					/* Release namespace entries */
					apEntry = (SyHashEntry **)SySetBasePtr(&pEntry->sNSset);
					for( n = 0 ; n < SySetUsed(&pEntry->sNSset) ; ++n ){
						SyMemBackendFree(pParser->pAllocator,apEntry[n]->pUserData);
					}
					SySetRelease(&pEntry->sNSset);
				}
			}
		}
	}
	/* Clean-up the mess left behind */
	SySetRelease(&sWorker);
	SySetRelease(&sTagStack);
	/* Processing result */
	return rc;
}
PH7_PRIVATE sxi32 SyXMLParserRelease(SyXMLParser *pParser)
{
	SyLexRelease(&pParser->sLex);
	SySetRelease(&pParser->sToken);
	SyHashRelease(&pParser->hns);
	return SXRET_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
