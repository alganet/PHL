# src/ph7/vm_http.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 285/444 lines (64.19%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `/*` |
|    - |    8 | ` * Section:` |
|    - |    9 | ` *    HTTP/URI related routines.` |
|    - |   10 | ` * Status:` |
|    - |   11 | ` *    Stable.` |
|    - |   12 | ` */` |
|    - |   13 | ` /*` |
|    - |   14 | `  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].` |
|    - |   15 | `  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]` |
|    - |   16 | `  * This almost, but not quite, RFC1738 URI syntax.` |
|    - |   17 | `  * This routine is not a validator,it does not check for validity` |
|    - |   18 | `  * nor decode URI parts,the only thing this routine does is splitting` |
|    - |   19 | `  * the input to its fields.` |
|    - |   20 | `  * Upper layer are responsible of decoding and validating URI parts.` |
|    - |   21 | `  * On success,this function populate the "SyhttpUri" structure passed` |
|    - |   22 | `  * as the first argument. Otherwise SXERR_* is returned when a malformed` |
|    - |   23 | `  * input is encountered.` |
|    - |   24 | `  */` |
|   44 |   25 | ` PH7_PRIVATE sxi32 PH7_VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|  ! 0 |   26 | ` {` |
|   44 |   27 | `	 const char *zEnd = &zUri[nLen];` |
|   44 |   28 | `	 sxu8 bHostOnly = FALSE;` |
|   44 |   29 | `	 sxu8 bIPv6 = FALSE	;` |
|    - |   30 | `	 const char *zCur;` |
|    - |   31 | `	 SyString *pComp;` |
|   44 |   32 | `	 sxu32 nPos = 0;` |
|    - |   33 | `	 sxi32 rc;` |
|    - |   34 | `	 /* Zero the structure first */` |
|   44 |   35 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|    - |   36 | `	 /* Remove leading and trailing white spaces  */` |
|   44 |   37 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|   44 |   38 | `	 SyStringFullTrim(&pOut->sRaw);` |
|    - |   39 | `	 /* Find the first '/' separator */` |
|   44 |   40 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|   44 |   41 | `	 if( rc != SXRET_OK ){` |
|    - |   42 | `		 /* Assume a host name only */` |
|  ! 0 |   43 | `		 zCur = zEnd;` |
|  ! 0 |   44 | `		 bHostOnly = TRUE;` |
|  ! 0 |   45 | `		 goto ProcessHost;` |
|    - |   46 | `	 }` |
|   44 |   47 | `	 zCur = &zUri[nPos];` |
|   44 |   48 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|    - |   49 | `		 /* Extract a scheme:` |
|    - |   50 | `		  * Not that we can get an invalid scheme here.` |
|    - |   51 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|    - |   52 | `		  * registered schemes and will report the error as soon as his comparison function` |
|    - |   53 | `		  * fail.` |
|    - |   54 | `		  */` |
|  ! 0 |   55 | `	 	pComp = &pOut->sScheme;` |
|  ! 0 |   56 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|  ! 0 |   57 | `		SyStringLeftTrim(pComp);` |
|  ! 0 |   58 | `	 }` |
|   44 |   59 | `	 if( zCur[1] != '/' ){` |
|   44 |   60 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|    - |   61 | `		  /* No authority */` |
|   44 |   62 | `		  goto PathSplit;` |
|    - |   63 | `		}` |
|    - |   64 | `		 /* There is something here , we will assume its an authority` |
|    - |   65 | `		  * and someone has forgot the two prefix slashes "//",` |
|    - |   66 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|    - |   67 | `		  * user or not,but now assume we are dealing with an authority` |
|    - |   68 | `		  * and let the caller handle all the validation process.` |
|    - |   69 | `		  */` |
|  ! 0 |   70 | `		 goto ProcessHost;` |
|    - |   71 | `	 }` |
|  ! 0 |   72 | `	 zUri = &zCur[2];` |
|  ! 0 |   73 | `	 zCur = zEnd;` |
|  ! 0 |   74 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|  ! 0 |   75 | `	 if( rc == SXRET_OK ){` |
|  ! 0 |   76 | `		 zCur = &zUri[nPos];` |
|  ! 0 |   77 | `	 }` |
|  ! 0 |   78 | ` ProcessHost:` |
|    - |   79 | `	 /* Extract user information if present */` |
|  ! 0 |   80 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|  ! 0 |   81 | `	 if( rc == SXRET_OK ){` |
|  ! 0 |   82 | `		 if( nPos > 0 ){` |
|    - |   83 | `			 sxu32 nPassOfft; /* Password offset */` |
|  ! 0 |   84 | `			 pComp = &pOut->sUser;` |
|  ! 0 |   85 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|    - |   86 | `			 /* Extract the password if available */` |
|  ! 0 |   87 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|  ! 0 |   88 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|  ! 0 |   89 | `				 pComp->nByte = nPassOfft;` |
|  ! 0 |   90 | `				 pComp = &pOut->sPass;` |
|  ! 0 |   91 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|  ! 0 |   92 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|  ! 0 |   93 | `			 }` |
|    - |   94 | `			 /* Update the cursor */` |
|  ! 0 |   95 | `			 zUri = &zUri[nPos+1];` |
|  ! 0 |   96 | `		 }else{` |
|  ! 0 |   97 | `			 zUri++;` |
|    - |   98 | `		 }` |
|  ! 0 |   99 | `	 }` |
|  ! 0 |  100 | `	 pComp = &pOut->sHost;` |
|  ! 0 |  101 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|  ! 0 |  102 | `		 zUri++;` |
|  ! 0 |  103 | `	 }` |
|  ! 0 |  104 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|  ! 0 |  105 | `	 if( pComp->zString[0] == '[' ){` |
|    - |  106 | `		 /* An IPv6 Address: Make a simple naive test` |
|    - |  107 | `		  */` |
|  ! 0 |  108 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|  ! 0 |  109 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|  ! 0 |  110 | `			 zUri++; pComp->nByte++;` |
|  ! 0 |  111 | `		 }` |
|  ! 0 |  112 | `		 if( zUri[0] != ']' ){` |
|  ! 0 |  113 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|    - |  114 | `		 }` |
|  ! 0 |  115 | `		 zUri++;` |
|  ! 0 |  116 | `		 bIPv6 = TRUE;` |
|  ! 0 |  117 | `	 }` |
|    - |  118 | `	 /* Extract a port number if available */` |
|  ! 0 |  119 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|  ! 0 |  120 | `	 if( rc == SXRET_OK ){` |
|  ! 0 |  121 | `		 if( bIPv6 == FALSE ){` |
|  ! 0 |  122 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|  ! 0 |  123 | `		 }` |
|  ! 0 |  124 | `		 pComp = &pOut->sPort;` |
|  ! 0 |  125 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|  ! 0 |  126 | `	 }` |
|  ! 0 |  127 | `	 if( bHostOnly == TRUE ){` |
|  ! 0 |  128 | `		 return SXRET_OK;` |
|    - |  129 | `	 }` |
|  ! 0 |  130 | `PathSplit:` |
|   44 |  131 | `	 zUri = zCur;` |
|   44 |  132 | `	 pComp = &pOut->sPath;` |
|   44 |  133 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|   44 |  134 | `	 if( pComp->nByte == 0 ){` |
|  ! 0 |  135 | `		 return SXRET_OK; /* Empty path */` |
|    - |  136 | `	 }` |
|   44 |  137 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|   18 |  138 | `		 pComp->nByte = nPos; /* Update path length */` |
|   18 |  139 | `		 pComp = &pOut->sQuery;` |
|   18 |  140 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|    9 |  141 | `	 }` |
|   44 |  142 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|    - |  143 | `		 /* Update path or query length */` |
|  ! 0 |  144 | `		 if( pComp == &pOut->sPath ){` |
|  ! 0 |  145 | `			 pComp->nByte = nPos;` |
|  ! 0 |  146 | `		 }else{` |
|  ! 0 |  147 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|    - |  148 | `				 /* Malformed syntax : Query must be present before fragment */` |
|  ! 0 |  149 | `				 return SXERR_SYNTAX;` |
|    - |  150 | `			 }` |
|  ! 0 |  151 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|    - |  152 | `		 }` |
|  ! 0 |  153 | `		 pComp = &pOut->sFragment;` |
|  ! 0 |  154 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|  ! 0 |  155 | `	 }` |
|   44 |  156 | `	 return SXRET_OK;` |
|   22 |  157 | ` }` |
|    - |  158 | ` /*` |
|    - |  159 | ` * Extract a single line from a raw HTTP request.` |
|    - |  160 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|    - |  161 | ` * and SXERR_MORE when more input is needed.` |
|    - |  162 | ` */` |
|  178 |  163 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|  ! 0 |  164 | `{` |
|    - |  165 | `  	const char *zIn;` |
|    - |  166 | `  	sxu32 nPos;` |
|    - |  167 | `	/* Jump leading white spaces */` |
|  442 |  168 | `	SyStringLeftTrim(pCursor);` |
|  178 |  169 | `	if( pCursor->nByte < 1 ){` |
|  ! 0 |  170 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|  ! 0 |  171 | `		return SXERR_EOF; /* End of input */` |
|    - |  172 | `	}` |
|  178 |  173 | `	zIn = SyStringData(pCursor);` |
|  178 |  174 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|    - |  175 | `		/* Line not found,tell the caller to read more input from source */` |
|   46 |  176 | `		SyStringDupPtr(pCurrent,pCursor);` |
|   46 |  177 | `		return SXERR_MORE;` |
|    - |  178 | `	}` |
|  132 |  179 | `  	pCurrent->zString = zIn;` |
|  132 |  180 | `  	pCurrent->nByte	= nPos;` |
|    - |  181 | `  	/* advance the cursor so we can call this routine again */` |
|  132 |  182 | `  	pCursor->zString = &zIn[nPos];` |
|  132 |  183 | `  	pCursor->nByte -= nPos;` |
|  132 |  184 | `  	return SXRET_OK;` |
|   89 |  185 | ` }` |
|    - |  186 | ` /*` |
|    - |  187 | `  * Split a single MIME header into a name value pair.` |
|    - |  188 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|    - |  189 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|    - |  190 | `  * is encountered.` |
|    - |  191 | `  * Note: This function handle also mult-line headers.` |
|    - |  192 | `  */` |
|  132 |  193 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|  ! 0 |  194 | ` {` |
|    - |  195 | `	 SyString *pName;` |
|    - |  196 | `	 sxu32 nPos;` |
|    - |  197 | `	 sxi32 rc;` |
|  132 |  198 | `	 if( nLen < 1 ){` |
|  ! 0 |  199 | `		 return SXERR_NEXT;` |
|    - |  200 | `	 }` |
|    - |  201 | `	 /* Check for multi-line header */` |
|  132 |  202 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
|  ! 0 |  203 | `		SyString *pTmp = &pLast->sValue;` |
|  ! 0 |  204 | `		SyStringFullTrim(pTmp);` |
|  ! 0 |  205 | `		if( pTmp->nByte == 0 ){` |
|  ! 0 |  206 | `			SyStringInitFromBuf(pTmp,zLine,nLen);` |
|  ! 0 |  207 | `		}else{` |
|    - |  208 | `			/* Update header value length */` |
|  ! 0 |  209 | `			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);` |
|    - |  210 | `		}` |
|    - |  211 | `		 /* Simply tell the caller to reset its states and get another line */` |
|  ! 0 |  212 | `		 return SXERR_CONTINUE;` |
|    - |  213 | `	 }` |
|    - |  214 | `	/* Split the header */` |
|  132 |  215 | `	pName = &pHdr->sName;` |
|  132 |  216 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|  132 |  217 | `	if(rc != SXRET_OK ){` |
|  ! 0 |  218 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|    - |  219 | `	}` |
|  132 |  220 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|  132 |  221 | `	SyStringFullTrim(pName);` |
|    - |  222 | `	/* Extract a header value */` |
|  132 |  223 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|    - |  224 | `	/* Remove leading and trailing whitespaces */` |
|  264 |  225 | `	SyStringFullTrim(&pHdr->sValue);` |
|  132 |  226 | `	return SXRET_OK;` |
|   66 |  227 | ` }` |
|    - |  228 | ` /*` |
|    - |  229 | `  * Extract all MIME headers associated with a HTTP request.` |
|    - |  230 | `  * After processing the first line of a HTTP request,the following` |
|    - |  231 | `  * routine is called in order to extract MIME headers.` |
|    - |  232 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|    - |  233 | `  * more inputs.` |
|    - |  234 | `  * Note: Any malformed header is simply discarded.` |
|    - |  235 | `  */` |
|   44 |  236 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|  ! 0 |  237 | ` {` |
|   44 |  238 | `	 SyhttpHeader *pLast = 0;` |
|    - |  239 | `	 SyString sCurrent;` |
|    - |  240 | `	 SyhttpHeader sHdr;` |
|    - |  241 | `	 sxu8 bEol;` |
|    - |  242 | `	 sxi32 rc;` |
|   44 |  243 | `	 if( SySetUsed(pOut) > 0 ){` |
|  ! 0 |  244 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|  ! 0 |  245 | `	 }` |
|   44 |  246 | `	 bEol = FALSE;` |
|   66 |  247 | `	 for(;;){` |
|  132 |  248 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|    - |  249 | `		 /* Extract a single line from the raw HTTP request */` |
|  132 |  250 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|  132 |  251 | `		 if(rc != SXRET_OK ){` |
|   44 |  252 | `			 if( sCurrent.nByte < 1 ){` |
|  ! 0 |  253 | `				 break;` |
|    - |  254 | `			 }` |
|   44 |  255 | `			 bEol = TRUE;` |
|   22 |  256 | `		 }` |
|    - |  257 | `		 /* Process the header */` |
|  132 |  258 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|  132 |  259 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|  ! 0 |  260 | `				 break;` |
|    - |  261 | `			 }` |
|    - |  262 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|    - |  263 | `			  * in case we face one of them.` |
|    - |  264 | `			  */` |
|  132 |  265 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|   66 |  266 | `		 }` |
|  132 |  267 | `		 if( bEol ){` |
|   44 |  268 | `			 break;` |
|    - |  269 | `		 }` |
|  ! 0 |  270 | `	 } /* for(;;) */` |
|   44 |  271 | `	 return SXRET_OK;` |
|  ! 0 |  272 | ` }` |
|    - |  273 | ` /*` |
|    - |  274 | `  * Process the first line of a HTTP request.` |
|    - |  275 | `  * This routine perform the following operations` |
|    - |  276 | `  *  1) Extract the HTTP method.` |
|    - |  277 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|    - |  278 | `  *  3) Extract the HTTP protocol version.` |
|    - |  279 | `  */` |
|   46 |  280 | ` static sxi32 VmHttpProcessFirstLine(` |
|    - |  281 | `	 SyString *pRequest, /* Raw HTTP request */` |
|    - |  282 | `	 sxi32 *pMethod,     /* OUT: HTTP method */` |
|    - |  283 | `	 SyhttpUri *pUri,    /* OUT: Parse of the URI */` |
|    - |  284 | `	 sxi32 *pProto       /* OUT: HTTP protocol */` |
|    - |  285 | `	 )` |
|  ! 0 |  286 | ` {` |
|    - |  287 | `	 static const char *azMethods[] = { "get","post","head","put"};` |
|    - |  288 | `	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};` |
|    - |  289 | `	 const char *zIn,*zEnd,*zPtr;` |
|    - |  290 | `	 SyString sLine;` |
|    - |  291 | `	 sxu32 nLen;` |
|    - |  292 | `	 sxi32 rc;` |
|    - |  293 | `	 /* Extract the first line and update the pointer */` |
|   46 |  294 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|   46 |  295 | `	 if( rc != SXRET_OK ){` |
|    2 |  296 | `		 return rc;` |
|    - |  297 | `	 }` |
|   44 |  298 | `	 if ( sLine.nByte < 1 ){` |
|    - |  299 | `		 /* Empty HTTP request */` |
|  ! 0 |  300 | `		 return SXERR_EMPTY;` |
|    - |  301 | `	 }` |
|    - |  302 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|   44 |  303 | `	 zIn = sLine.zString;` |
|   44 |  304 | `	 zEnd = &zIn[sLine.nByte];` |
|   44 |  305 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|  ! 0 |  306 | `		 zIn++;` |
|  ! 0 |  307 | `	 }` |
|    - |  308 | `	 /* Extract the HTTP method */` |
|   44 |  309 | `	 zPtr = zIn;` |
|  176 |  310 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|  132 |  311 | `		 zIn++;` |
|  ! 0 |  312 | `	 }` |
|   44 |  313 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|   44 |  314 | `	 if( zIn > zPtr ){` |
|    - |  315 | `		 sxu32 i;` |
|   44 |  316 | `		 nLen = (sxu32)(zIn-zPtr);` |
|   44 |  317 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|   44 |  318 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|   44 |  319 | `				 *pMethod = aMethods[i];` |
|   44 |  320 | `				 break;` |
|    - |  321 | `			 }` |
|  ! 0 |  322 | `		 }` |
|   22 |  323 | `	 }` |
|    - |  324 | `	 /* Jump trailing white spaces */` |
|   88 |  325 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|   44 |  326 | `		 zIn++;` |
|  ! 0 |  327 | `	 }` |
|    - |  328 | `	  /* Extract the request URI */` |
|   44 |  329 | `	 zPtr = zIn;` |
|  522 |  330 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|  478 |  331 | `		 zIn++;` |
|  ! 0 |  332 | `	 }` |
|   44 |  333 | `	 if( zIn > zPtr ){` |
|   44 |  334 | `		 nLen = (sxu32)(zIn-zPtr);` |
|    - |  335 | `		 /* Split raw URI to it's fields */` |
|   44 |  336 | `		 PH7_VmHttpSplitURI(pUri,zPtr,nLen);` |
|   22 |  337 | `	 }` |
|    - |  338 | `	 /* Jump trailing white spaces */` |
|   88 |  339 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|   44 |  340 | `		 zIn++;` |
|  ! 0 |  341 | `	 }` |
|    - |  342 | `	 /* Extract the HTTP version */` |
|   44 |  343 | `	 zPtr = zIn;` |
|  396 |  344 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|  352 |  345 | `		 zIn++;` |
|  ! 0 |  346 | `	 }` |
|   44 |  347 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|   44 |  348 | `	 rc = 1;` |
|   44 |  349 | `	 if( zIn > zPtr ){` |
|   44 |  350 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|   22 |  351 | `	 }` |
|   44 |  352 | `	 if( !rc ){` |
|    2 |  353 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|    1 |  354 | `	 }` |
|   44 |  355 | `	 return SXRET_OK;` |
|   23 |  356 | ` }` |
|    - |  357 | ` /*` |
|    - |  358 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|    - |  359 | `  * into a name value pair.` |
|    - |  360 | `  * Note that this encoding is implicit in GET based requests.` |
|    - |  361 | `  * After the tokenization process,register the decoded queries` |
|    - |  362 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|    - |  363 | `  */` |
|   18 |  364 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|    - |  365 | `	 ph7_vm *pVm,       /* Target VM */` |
|    - |  366 | `	 SyString *pQuery,  /* Raw query to decode */` |
|    - |  367 | `	 SyBlob *pWorker,   /* Working buffer */` |
|    - |  368 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|    - |  369 | `	 )` |
|  ! 0 |  370 | ` {` |
|   18 |  371 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|   18 |  372 | `	 const char *zIn = pQuery->zString;` |
|    - |  373 | `	 ph7_value *pGet,*pRequest;` |
|    - |  374 | `	 SyString sName,sValue;` |
|    - |  375 | `	 const char *zPtr;` |
|    - |  376 | `	 sxu32 nBlobOfft;` |
|    - |  377 | `	 /* Extract superglobals */` |
|   18 |  378 | `	 if( is_post ){` |
|    - |  379 | `		 /* $_POST superglobal */` |
|  ! 0 |  380 | `		 pGet = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|  ! 0 |  381 | `	 }else{` |
|    - |  382 | `		 /* $_GET superglobal */` |
|   18 |  383 | `		 pGet = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|    - |  384 | `	 }` |
|   18 |  385 | `	 pRequest = PH7_VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|    - |  386 | `	 /* Split up the raw query */` |
|   18 |  387 | `	 for(;;){` |
|    - |  388 | `		 /* Jump leading white spaces */` |
|   36 |  389 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|  ! 0 |  390 | `			 zIn++;` |
|  ! 0 |  391 | `		 }` |
|   36 |  392 | `		 if( zIn >= zEnd ){` |
|   18 |  393 | `			 break;` |
|    - |  394 | `		 }` |
|   18 |  395 | `		 zPtr = zIn;` |
|   40 |  396 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|   22 |  397 | `			 zPtr++;` |
|  ! 0 |  398 | `		 }` |
|    - |  399 | `		 /* Reset the working buffer */` |
|   18 |  400 | `		 SyBlobReset(pWorker);` |
|    - |  401 | `		 /* Decode the entry */` |
|   18 |  402 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|    - |  403 | `		 /* Save the entry */` |
|   18 |  404 | `		 sName.nByte = SyBlobLength(pWorker);` |
|   18 |  405 | `		 sValue.zString = 0;` |
|   18 |  406 | `		 sValue.nByte = 0;` |
|   18 |  407 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|   18 |  408 | `			 zPtr++;` |
|   18 |  409 | `			 zIn = zPtr;` |
|    - |  410 | `			 /* Store field value */` |
|  114 |  411 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|   96 |  412 | `				 zPtr++;` |
|  ! 0 |  413 | `			 }` |
|   18 |  414 | `			 if( zPtr > zIn ){` |
|    - |  415 | `				 /* Decode the value */` |
|   18 |  416 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|   18 |  417 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|   18 |  418 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|   18 |  419 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|    - |  420 |  |
|    9 |  421 | `			 }` |
|    - |  422 | `			 /* Synchronize pointers */` |
|   18 |  423 | `			 zIn = zPtr;` |
|    9 |  424 | `		 }` |
|   18 |  425 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|    - |  426 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|   18 |  427 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|   27 |  428 | `			 PH7_VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|   18 |  429 | `				 sName.zString,(int)sName.nByte,` |
|   18 |  430 | `				 sValue.zString,(int)sValue.nByte` |
|    - |  431 | `				 );` |
|    9 |  432 | `		 }` |
|   18 |  433 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|   27 |  434 | `			 PH7_VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|   18 |  435 | `				 sName.zString,(int)sName.nByte,` |
|   18 |  436 | `				 sValue.zString,(int)sValue.nByte` |
|    - |  437 | `					 );` |
|    9 |  438 | `		 }` |
|    - |  439 | `		 /* Advance the pointer */` |
|   18 |  440 | `		 zIn = &zPtr[1];` |
|  ! 0 |  441 | `	 }` |
|    - |  442 | `	/* All done*/` |
|   18 |  443 | `	return SXRET_OK;` |
|  ! 0 |  444 | ` }` |
|    - |  445 | ` /*` |
|    - |  446 | `  * Extract MIME header value from the given set.` |
|    - |  447 | `  * Return header value on success. NULL otherwise.` |
|    - |  448 | `  */` |
|  396 |  449 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|  ! 0 |  450 | ` {` |
|    - |  451 | `	 SyhttpHeader *aMime,*pMime;` |
|    - |  452 | `	 SyString sMime;` |
|    - |  453 | `	 sxu32 n;` |
|  396 |  454 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|    - |  455 | `	 /* Point to the MIME entries */` |
|  396 |  456 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|    - |  457 | `	 /* Perform the lookup */` |
| 1320 |  458 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
| 1054 |  459 | `		 pMime = &aMime[n];` |
| 1054 |  460 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|    - |  461 | `			 /* Header found,return it's associated value */` |
|  130 |  462 | `			 return &pMime->sValue;` |
|    - |  463 | `		 }` |
|  462 |  464 | `	 }` |
|    - |  465 | `	 /* No such MIME header */` |
|  266 |  466 | `	 return 0;` |
|  198 |  467 | ` }` |
|    - |  468 | ` /*` |
|    - |  469 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|    - |  470 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|    - |  471 | `  */` |
|    2 |  472 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|  ! 0 |  473 | ` {` |
|    2 |  474 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|    - |  475 | `	 SyString sName,sValue;` |
|    - |  476 | `	 ph7_value *pCookie;` |
|    - |  477 | `	 sxu32 nOfft;` |
|    - |  478 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|    2 |  479 | `	 pCookie = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|    2 |  480 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    - |  481 | `		 /* $_COOKIE superglobal not available */` |
|  ! 0 |  482 | `		 return SXERR_NOTFOUND;` |
|    - |  483 | `	 }` |
|    4 |  484 | `	 for(;;){` |
|    - |  485 | `		  /* Jump leading white spaces */` |
|   12 |  486 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|    4 |  487 | `			 zIn++;` |
|  ! 0 |  488 | `		 }` |
|    8 |  489 | `		 if( zIn >= zEnd ){` |
|    2 |  490 | `			 break;` |
|    - |  491 | `		 }` |
|    - |  492 | `		  /* Reset the working buffer */` |
|    6 |  493 | `		 SyBlobReset(pWorker);` |
|    6 |  494 | `		 zDelimiter = zIn;` |
|    - |  495 | `		 /* Delimit the name[=value]; pair */` |
|   62 |  496 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|   56 |  497 | `			 zDelimiter++;` |
|  ! 0 |  498 | `		 }` |
|    6 |  499 | `		 zPtr = zIn;` |
|   32 |  500 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|   26 |  501 | `			 zPtr++;` |
|  ! 0 |  502 | `		 }` |
|    - |  503 | ``		 /* Decode the cookie. RAW url-decoding, php's: a `+` in a cookie value is a`` |
|    - |  504 | `		  * PLUS, not a space — the browser hands back what setcookie() wrote with` |
|    - |  505 | `		  * rawurlencode(), and reading it as a query string turns every literal` |
|    - |  506 | ``		  * `+` in a token or a base64 payload into a space. */`` |
|    6 |  507 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,FALSE);` |
|    6 |  508 | `		 sName.nByte = SyBlobLength(pWorker);` |
|    6 |  509 | `		 zPtr++;` |
|    6 |  510 | `		 sValue.zString = 0;` |
|    6 |  511 | `		 sValue.nByte = 0;` |
|    6 |  512 | `		 if( zPtr < zDelimiter ){` |
|    - |  513 | `			 /* Got a Cookie value */` |
|    6 |  514 | `			 nOfft = SyBlobLength(pWorker);` |
|    6 |  515 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,FALSE);` |
|    6 |  516 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|    3 |  517 | `		 }` |
|    - |  518 | `		 /* Synchronize pointers */` |
|    6 |  519 | `		 zIn = &zDelimiter[1];` |
|    - |  520 | `		 /* Perform the insertion */` |
|    6 |  521 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|    9 |  522 | `		 PH7_VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|    6 |  523 | `			 sName.zString,(int)sName.nByte,` |
|    6 |  524 | `			 sValue.zString,(int)sValue.nByte` |
|    - |  525 | `			 );` |
|  ! 0 |  526 | `	 }` |
|    2 |  527 | `	 return SXRET_OK;` |
|    1 |  528 | ` }` |
|    - |  529 | ` /*` |
|    - |  530 | `  * Process a full HTTP request and populate the appropriate arrays` |
|    - |  531 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|    - |  532 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|    - |  533 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|    - |  534 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|    - |  535 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|    - |  536 | `  * a malformed HTTP request.` |
|    - |  537 | `  */` |
|   46 |  538 | ` PH7_PRIVATE sxi32 PH7_VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|  ! 0 |  539 | ` {` |
|    - |  540 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|    - |  541 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|    - |  542 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|    - |  543 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|    - |  544 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|    - |  545 | `	 SySet sHeader;      /* MIME headers set */` |
|    - |  546 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|    - |  547 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|    - |  548 | `	 sxi32 rc;` |
|   46 |  549 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|   46 |  550 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|   46 |  551 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|    - |  552 | `	 /* Ignore leading and trailing white spaces*/` |
|  230 |  553 | `	 SyStringFullTrim(&sRequest);` |
|    - |  554 | `	 /* Process the first line */` |
|   46 |  555 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|   46 |  556 | `	 if( rc != SXRET_OK ){` |
|    2 |  557 | `		 return rc;` |
|    - |  558 | `	 }` |
|    - |  559 | `	 /* Process MIME headers */` |
|   44 |  560 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|    - |  561 | `	 /*` |
|    - |  562 | `	  * Setup $_SERVER environments` |
|    - |  563 | `	  */` |
|    - |  564 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|   66 |  565 | `	 ph7_vm_config(pVm,` |
|    - |  566 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  567 | `		 "SERVER_PROTOCOL",` |
|   44 |  568 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|    - |  569 | `		 sizeof("HTTP/1.1")-1` |
|    - |  570 | `		 );` |
|    - |  571 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|   44 |  572 | `	 ph7_vm_config(pVm,` |
|    - |  573 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  574 | `		 "REQUEST_METHOD",` |
|   44 |  575 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|  ! 0 |  576 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|  ! 0 |  577 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|  ! 0 |  578 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|    - |  579 | `		 -1 /* Compute attribute length automatically */` |
|    - |  580 | `		 );` |
|   44 |  581 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|   18 |  582 | `		 pValue = &sUri.sQuery;` |
|    - |  583 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|   27 |  584 | `		 ph7_vm_config(pVm,` |
|    - |  585 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  586 | `			 "QUERY_STRING",` |
|    9 |  587 | `			 pValue->zString,` |
|    9 |  588 | `			 pValue->nByte` |
|    - |  589 | `			 );` |
|    - |  590 | `		 /* Decoded the raw query */` |
|   18 |  591 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|    9 |  592 | `	 }` |
|    - |  593 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|   44 |  594 | `	 pValue = &sUri.sRaw;` |
|   66 |  595 | `	 ph7_vm_config(pVm,` |
|    - |  596 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  597 | `		 "REQUEST_URI",` |
|   22 |  598 | `		 pValue->zString,` |
|   22 |  599 | `		 pValue->nByte` |
|    - |  600 | `		 );` |
|    - |  601 | `	 /*` |
|    - |  602 | `	  * 'PATH_INFO'` |
|    - |  603 | `	  * 'ORIG_PATH_INFO'` |
|    - |  604 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|    - |  605 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|    - |  606 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|    - |  607 | `	  * /some/stuff.` |
|    - |  608 | `	  */` |
|   44 |  609 | `	 pValue = &sUri.sPath;` |
|   66 |  610 | `	 ph7_vm_config(pVm,` |
|    - |  611 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  612 | `		 "PATH_INFO",` |
|   22 |  613 | `		 pValue->zString,` |
|   22 |  614 | `		 pValue->nByte` |
|    - |  615 | `		 );` |
|   66 |  616 | `	 ph7_vm_config(pVm,` |
|    - |  617 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  618 | `		 "ORIG_PATH_INFO",` |
|   22 |  619 | `		 pValue->zString,` |
|   22 |  620 | `		 pValue->nByte` |
|    - |  621 | `		 );` |
|    - |  622 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|   44 |  623 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|   44 |  624 | `	 if( pValue ){` |
|   63 |  625 | `		 ph7_vm_config(pVm,` |
|    - |  626 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  627 | `			 "HTTP_ACCEPT",` |
|   21 |  628 | `			 pValue->zString,` |
|   21 |  629 | `			 pValue->nByte` |
|    - |  630 | `		 );` |
|   21 |  631 | `	 }` |
|    - |  632 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|   44 |  633 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|   44 |  634 | `	 if( pValue ){` |
|  ! 0 |  635 | `		 ph7_vm_config(pVm,` |
|    - |  636 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  637 | `			 "HTTP_ACCEPT_CHARSET",` |
|  ! 0 |  638 | `			 pValue->zString,` |
|  ! 0 |  639 | `			 pValue->nByte` |
|    - |  640 | `		 );` |
|  ! 0 |  641 | `	 }` |
|    - |  642 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|   44 |  643 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|   44 |  644 | `	 if( pValue ){` |
|  ! 0 |  645 | `		 ph7_vm_config(pVm,` |
|    - |  646 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  647 | `			 "HTTP_ACCEPT_ENCODING",` |
|  ! 0 |  648 | `			 pValue->zString,` |
|  ! 0 |  649 | `			 pValue->nByte` |
|    - |  650 | `		 );` |
|  ! 0 |  651 | `	 }` |
|    - |  652 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|   44 |  653 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|   44 |  654 | `	 if( pValue ){` |
|  ! 0 |  655 | `		 ph7_vm_config(pVm,` |
|    - |  656 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  657 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|  ! 0 |  658 | `			 pValue->zString,` |
|  ! 0 |  659 | `			 pValue->nByte` |
|    - |  660 | `		 );` |
|  ! 0 |  661 | `	 }` |
|    - |  662 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|   44 |  663 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|   44 |  664 | `	 if( pValue ){` |
|    3 |  665 | `		 ph7_vm_config(pVm,` |
|    - |  666 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  667 | `			 "HTTP_CONNECTION",` |
|    1 |  668 | `			 pValue->zString,` |
|    1 |  669 | `			 pValue->nByte` |
|    - |  670 | `		 );` |
|    1 |  671 | `	 }` |
|    - |  672 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|   44 |  673 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|   44 |  674 | `	 if( pValue ){` |
|   66 |  675 | `		 ph7_vm_config(pVm,` |
|    - |  676 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  677 | `			 "HTTP_HOST",` |
|   22 |  678 | `			 pValue->zString,` |
|   22 |  679 | `			 pValue->nByte` |
|    - |  680 | `		 );` |
|   22 |  681 | `	 }` |
|    - |  682 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|   44 |  683 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|   44 |  684 | `	 if( pValue ){` |
|  ! 0 |  685 | `		 ph7_vm_config(pVm,` |
|    - |  686 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  687 | `			 "HTTP_REFERER",` |
|  ! 0 |  688 | `			 pValue->zString,` |
|  ! 0 |  689 | `			 pValue->nByte` |
|    - |  690 | `		 );` |
|  ! 0 |  691 | `	 }` |
|    - |  692 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|   44 |  693 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|   44 |  694 | `	 if( pValue ){` |
|   63 |  695 | `		 ph7_vm_config(pVm,` |
|    - |  696 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  697 | `			 "HTTP_USER_AGENT",` |
|   21 |  698 | `			 pValue->zString,` |
|   21 |  699 | `			 pValue->nByte` |
|    - |  700 | `		 );` |
|   21 |  701 | `	 }` |
|    - |  702 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|    - |  703 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|    - |  704 | `	   */` |
|   44 |  705 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|   44 |  706 | `	 if( pValue ){` |
|  ! 0 |  707 | `		 ph7_vm_config(pVm,` |
|    - |  708 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  709 | `			 "PHP_AUTH_DIGEST",` |
|  ! 0 |  710 | `			 pValue->zString,` |
|  ! 0 |  711 | `			 pValue->nByte` |
|    - |  712 | `		 );` |
|  ! 0 |  713 | `		 ph7_vm_config(pVm,` |
|    - |  714 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  715 | `			 "PHP_AUTH",` |
|  ! 0 |  716 | `			 pValue->zString,` |
|  ! 0 |  717 | `			 pValue->nByte` |
|    - |  718 | `		 );` |
|  ! 0 |  719 | `	 }` |
|    - |  720 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|   44 |  721 | `	 pHeaderArray = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|    - |  722 | `	 /* Iterate throw the available MIME headers*/` |
|   44 |  723 | `	 SySetResetCursor(&sHeader);` |
|   44 |  724 | `	 pHeader = 0; /* stupid cc warning */` |
|  198 |  725 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|  132 |  726 | `		 pName  = &pHeader->sName;` |
|  132 |  727 | `		 pValue = &pHeader->sValue;` |
|  132 |  728 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|    - |  729 | `			 /* Insert the MIME header and it's associated value */` |
|  198 |  730 | `			 PH7_VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|  132 |  731 | `				 pName->zString,(int)pName->nByte,` |
|  132 |  732 | `				 pValue->zString,(int)pValue->nByte` |
|    - |  733 | `				 );` |
|   66 |  734 | `		 }` |
|  132 |  735 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|   23 |  736 | `			 && pValue->nByte > 0){` |
|    - |  737 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|    2 |  738 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|    1 |  739 | `		 }` |
|  ! 0 |  740 | `	 }` |
|   44 |  741 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|    - |  742 | `		 /* Extract raw POST data */` |
|  ! 0 |  743 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|  ! 0 |  744 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|  ! 0 |  745 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|    - |  746 | `				 /* Extract POST data length */` |
|  ! 0 |  747 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|  ! 0 |  748 | `				 if( pValue ){` |
|  ! 0 |  749 | `					 sxi32 iLen = 0; /* POST data length */` |
|  ! 0 |  750 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|  ! 0 |  751 | `					 if( iLen > 0 ){` |
|    - |  752 | `						 /* Remove leading and trailing white spaces */` |
|  ! 0 |  753 | `						 SyStringFullTrim(&sRequest);` |
|  ! 0 |  754 | `						 if( (int)sRequest.nByte > iLen ){` |
|  ! 0 |  755 | `							 sRequest.nByte = (sxu32)iLen;` |
|  ! 0 |  756 | `						 }` |
|    - |  757 | `						 /* Decode POST data now */` |
|  ! 0 |  758 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|  ! 0 |  759 | `					 }` |
|  ! 0 |  760 | `				 }` |
|  ! 0 |  761 | `		 }` |
|  ! 0 |  762 | `	 }` |
|    - |  763 | `	 /* All done,clean-up the mess left behind */` |
|   44 |  764 | `	 SySetRelease(&sHeader);` |
|   44 |  765 | `	 SyBlobRelease(&sWorker);` |
|   44 |  766 | `	 return SXRET_OK;` |
|   23 |  767 | ` }` |
|    - |  768 |  |
