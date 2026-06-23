# src/ph7/vm_http.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 302/444 lines (68.02%)

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
|   64 |   25 | ` PH7_PRIVATE sxi32 PH7_VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)` |
|    1 |   26 | ` {` |
|   65 |   27 | `	 const char *zEnd = &zUri[nLen];` |
|   65 |   28 | `	 sxu8 bHostOnly = FALSE;` |
|   65 |   29 | `	 sxu8 bIPv6 = FALSE	;` |
|    - |   30 | `	 const char *zCur;` |
|    - |   31 | `	 SyString *pComp;` |
|   65 |   32 | `	 sxu32 nPos = 0;` |
|    - |   33 | `	 sxi32 rc;` |
|    - |   34 | `	 /* Zero the structure first */` |
|   65 |   35 | `	 SyZero(pOut,sizeof(SyhttpUri));` |
|    - |   36 | `	 /* Remove leading and trailing white spaces  */` |
|   65 |   37 | `	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);` |
|   65 |   38 | `	 SyStringFullTrim(&pOut->sRaw);` |
|    - |   39 | `	 /* Find the first '/' separator */` |
|   65 |   40 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|   65 |   41 | `	 if( rc != SXRET_OK ){` |
|    - |   42 | `		 /* Assume a host name only */` |
|    9 |   43 | `		 zCur = zEnd;` |
|    9 |   44 | `		 bHostOnly = TRUE;` |
|    9 |   45 | `		 goto ProcessHost;` |
|    - |   46 | `	 }` |
|   57 |   47 | `	 zCur = &zUri[nPos];` |
|   57 |   48 | `	 if( zUri != zCur && zCur[-1] == ':' ){` |
|    - |   49 | `		 /* Extract a scheme:` |
|    - |   50 | `		  * Not that we can get an invalid scheme here.` |
|    - |   51 | `		  * Fortunately the caller can discard any URI by comparing this scheme with its` |
|    - |   52 | `		  * registered schemes and will report the error as soon as his comparison function` |
|    - |   53 | `		  * fail.` |
|    - |   54 | `		  */` |
|   29 |   55 | `	 	pComp = &pOut->sScheme;` |
|   29 |   56 | `		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));` |
|   29 |   57 | `		SyStringLeftTrim(pComp);` |
|   14 |   58 | `	 }` |
|   57 |   59 | `	 if( zCur[1] != '/' ){` |
|   24 |   60 | `		 if( zCur == zUri \|\| zCur[-1] == ':' ){` |
|    - |   61 | `		  /* No authority */` |
|   24 |   62 | `		  goto PathSplit;` |
|    - |   63 | `		}` |
|    - |   64 | `		 /* There is something here , we will assume its an authority` |
|    - |   65 | `		  * and someone has forgot the two prefix slashes "//",` |
|    - |   66 | `		  * sooner or later we will detect if we are dealing with a malicious` |
|    - |   67 | `		  * user or not,but now assume we are dealing with an authority` |
|    - |   68 | `		  * and let the caller handle all the validation process.` |
|    - |   69 | `		  */` |
|  ! 0 |   70 | `		 goto ProcessHost;` |
|    - |   71 | `	 }` |
|   33 |   72 | `	 zUri = &zCur[2];` |
|   33 |   73 | `	 zCur = zEnd;` |
|   33 |   74 | `	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);` |
|   42 |   75 | `	 if( rc == SXRET_OK ){` |
|   19 |   76 | `		 zCur = &zUri[nPos];` |
|    9 |   77 | `	 }` |
|    7 |   78 | ` ProcessHost:` |
|    - |   79 | `	 /* Extract user information if present */` |
|   41 |   80 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);` |
|   41 |   81 | `	 if( rc == SXRET_OK ){` |
|    9 |   82 | `		 if( nPos > 0 ){` |
|    - |   83 | `			 sxu32 nPassOfft; /* Password offset */` |
|    9 |   84 | `			 pComp = &pOut->sUser;` |
|    9 |   85 | `			 SyStringInitFromBuf(pComp,zUri,nPos);` |
|    - |   86 | `			 /* Extract the password if available */` |
|    9 |   87 | `			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);` |
|    9 |   88 | `			 if( rc == SXRET_OK && nPassOfft < nPos){` |
|    9 |   89 | `				 pComp->nByte = nPassOfft;` |
|    9 |   90 | `				 pComp = &pOut->sPass;` |
|    9 |   91 | `				 pComp->zString = &zUri[nPassOfft+sizeof(char)];` |
|    9 |   92 | `				 pComp->nByte = nPos - nPassOfft - 1;` |
|    4 |   93 | `			 }` |
|    - |   94 | `			 /* Update the cursor */` |
|    9 |   95 | `			 zUri = &zUri[nPos+1];` |
|    5 |   96 | `		 }else{` |
|  ! 0 |   97 | `			 zUri++;` |
|    - |   98 | `		 }` |
|    4 |   99 | `	 }` |
|   41 |  100 | `	 pComp = &pOut->sHost;` |
|   41 |  101 | `	 while( zUri < zCur && SyisSpace(zUri[0])){` |
|  ! 0 |  102 | `		 zUri++;` |
|  ! 0 |  103 | `	 }` |
|   41 |  104 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));` |
|   41 |  105 | `	 if( pComp->zString[0] == '[' ){` |
|    - |  106 | `		 /* An IPv6 Address: Make a simple naive test` |
|    - |  107 | `		  */` |
|    3 |  108 | `		 zUri++; pComp->zString++; pComp->nByte = 0;` |
|    9 |  109 | `		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) \|\| zUri[0] == ':' ){` |
|    7 |  110 | `			 zUri++; pComp->nByte++;` |
|    1 |  111 | `		 }` |
|    3 |  112 | `		 if( zUri[0] != ']' ){` |
|  ! 0 |  113 | `			 return SXERR_CORRUPT; /* Malformed IPv6 address */` |
|    - |  114 | `		 }` |
|    3 |  115 | `		 zUri++;` |
|    3 |  116 | `		 bIPv6 = TRUE;` |
|    1 |  117 | `	 }` |
|    - |  118 | `	 /* Extract a port number if available */` |
|   41 |  119 | `	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);` |
|   41 |  120 | `	 if( rc == SXRET_OK ){` |
|   13 |  121 | `		 if( bIPv6 == FALSE ){` |
|   13 |  122 | `			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);` |
|    6 |  123 | `		 }` |
|   13 |  124 | `		 pComp = &pOut->sPort;` |
|   13 |  125 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));` |
|    6 |  126 | `	 }` |
|   41 |  127 | `	 if( bHostOnly == TRUE ){` |
|    9 |  128 | `		 return SXRET_OK;` |
|    - |  129 | `	 }` |
|   16 |  130 | `PathSplit:` |
|   57 |  131 | `	 zUri = zCur;` |
|   57 |  132 | `	 pComp = &pOut->sPath;` |
|   57 |  133 | `	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));` |
|   57 |  134 | `	 if( pComp->nByte == 0 ){` |
|   15 |  135 | `		 return SXRET_OK; /* Empty path */` |
|    - |  136 | `	 }` |
|   43 |  137 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){` |
|   15 |  138 | `		 pComp->nByte = nPos; /* Update path length */` |
|   15 |  139 | `		 pComp = &pOut->sQuery;` |
|   15 |  140 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));` |
|    7 |  141 | `	 }` |
|   43 |  142 | `	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){` |
|    - |  143 | `		 /* Update path or query length */` |
|    7 |  144 | `		 if( pComp == &pOut->sPath ){` |
|  ! 0 |  145 | `			 pComp->nByte = nPos;` |
|  ! 0 |  146 | `		 }else{` |
|    7 |  147 | `			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){` |
|    - |  148 | `				 /* Malformed syntax : Query must be present before fragment */` |
|  ! 0 |  149 | `				 return SXERR_SYNTAX;` |
|    - |  150 | `			 }` |
|    7 |  151 | `			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);` |
|    - |  152 | `		 }` |
|    7 |  153 | `		 pComp = &pOut->sFragment;` |
|    7 |  154 | `		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))` |
|    3 |  155 | `	 }` |
|   43 |  156 | `	 return SXRET_OK;` |
|   33 |  157 | ` }` |
|    - |  158 | ` /*` |
|    - |  159 | ` * Extract a single line from a raw HTTP request.` |
|    - |  160 | ` * Return SXRET_OK on success,SXERR_EOF when end of input` |
|    - |  161 | ` * and SXERR_MORE when more input is needed.` |
|    - |  162 | ` */` |
|   96 |  163 | `static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)` |
|  ! 0 |  164 |  |
|    - |  165 | `  	const char *zIn;` |
|    - |  166 | `  	sxu32 nPos;` |
|    - |  167 | `	/* Jump leading white spaces */` |
|  240 |  168 | `	SyStringLeftTrim(pCursor);` |
|   96 |  169 | `	if( pCursor->nByte < 1 ){` |
|  ! 0 |  170 | `		SyStringInitFromBuf(pCurrent,0,0);` |
|  ! 0 |  171 | `		return SXERR_EOF; /* End of input */` |
|    - |  172 | `	}` |
|   96 |  173 | `	zIn = SyStringData(pCursor);` |
|   96 |  174 | `	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){` |
|    - |  175 | `		/* Line not found,tell the caller to read more input from source */` |
|   24 |  176 | `		SyStringDupPtr(pCurrent,pCursor);` |
|   24 |  177 | `		return SXERR_MORE;` |
|    - |  178 | `	}` |
|   72 |  179 | `  	pCurrent->zString = zIn;` |
|   72 |  180 | `  	pCurrent->nByte	= nPos;` |
|    - |  181 | `  	/* advance the cursor so we can call this routine again */` |
|   72 |  182 | `  	pCursor->zString = &zIn[nPos];` |
|   72 |  183 | `  	pCursor->nByte -= nPos;` |
|   72 |  184 | `  	return SXRET_OK;` |
|   48 |  185 | ` }` |
|    - |  186 | ` /*` |
|    - |  187 | `  * Split a single MIME header into a name value pair.` |
|    - |  188 | `  * This function return SXRET_OK,SXERR_CONTINUE on success.` |
|    - |  189 | `  * Otherwise SXERR_NEXT is returned when a malformed header` |
|    - |  190 | `  * is encountered.` |
|    - |  191 | `  * Note: This function handle also mult-line headers.` |
|    - |  192 | `  */` |
|   72 |  193 | ` static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)` |
|  ! 0 |  194 | ` {` |
|    - |  195 | `	 SyString *pName;` |
|    - |  196 | `	 sxu32 nPos;` |
|    - |  197 | `	 sxi32 rc;` |
|   72 |  198 | `	 if( nLen < 1 ){` |
|  ! 0 |  199 | `		 return SXERR_NEXT;` |
|    - |  200 | `	 }` |
|    - |  201 | `	 /* Check for multi-line header */` |
|   72 |  202 | `	if( pLast && (zLine[-1] == ' ' \|\| zLine[-1] == '\t') ){` |
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
|   72 |  215 | `	pName = &pHdr->sName;` |
|   72 |  216 | `	rc = SyByteFind(zLine,nLen,':',&nPos);` |
|   72 |  217 | `	if(rc != SXRET_OK ){` |
|  ! 0 |  218 | `		return SXERR_NEXT; /* Malformed header;Check the next entry */` |
|    - |  219 | `	}` |
|   72 |  220 | `	SyStringInitFromBuf(pName,zLine,nPos);` |
|   72 |  221 | `	SyStringFullTrim(pName);` |
|    - |  222 | `	/* Extract a header value */` |
|   72 |  223 | `	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);` |
|    - |  224 | `	/* Remove leading and trailing whitespaces */` |
|  144 |  225 | `	SyStringFullTrim(&pHdr->sValue);` |
|   72 |  226 | `	return SXRET_OK;` |
|   36 |  227 | ` }` |
|    - |  228 | ` /*` |
|    - |  229 | `  * Extract all MIME headers associated with a HTTP request.` |
|    - |  230 | `  * After processing the first line of a HTTP request,the following` |
|    - |  231 | `  * routine is called in order to extract MIME headers.` |
|    - |  232 | `  * This function return SXRET_OK on success,SXERR_MORE when it needs` |
|    - |  233 | `  * more inputs.` |
|    - |  234 | `  * Note: Any malformed header is simply discarded.` |
|    - |  235 | `  */` |
|   24 |  236 | ` static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)` |
|  ! 0 |  237 | ` {` |
|   24 |  238 | `	 SyhttpHeader *pLast = 0;` |
|    - |  239 | `	 SyString sCurrent;` |
|    - |  240 | `	 SyhttpHeader sHdr;` |
|    - |  241 | `	 sxu8 bEol;` |
|    - |  242 | `	 sxi32 rc;` |
|   24 |  243 | `	 if( SySetUsed(pOut) > 0 ){` |
|  ! 0 |  244 | `		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);` |
|  ! 0 |  245 | `	 }` |
|   24 |  246 | `	 bEol = FALSE;` |
|   36 |  247 | `	 for(;;){` |
|   72 |  248 | `		 SyZero(&sHdr,sizeof(SyhttpHeader));` |
|    - |  249 | `		 /* Extract a single line from the raw HTTP request */` |
|   72 |  250 | `		 rc = VmGetNextLine(pRequest,&sCurrent);` |
|   72 |  251 | `		 if(rc != SXRET_OK ){` |
|   24 |  252 | `			 if( sCurrent.nByte < 1 ){` |
|  ! 0 |  253 | `				 break;` |
|    - |  254 | `			 }` |
|   24 |  255 | `			 bEol = TRUE;` |
|   12 |  256 | `		 }` |
|    - |  257 | `		 /* Process the header */` |
|   72 |  258 | `		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){` |
|   72 |  259 | `			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){` |
|  ! 0 |  260 | `				 break;` |
|    - |  261 | `			 }` |
|    - |  262 | `			 /* Retrieve the last parsed header so we can handle multi-line header` |
|    - |  263 | `			  * in case we face one of them.` |
|    - |  264 | `			  */` |
|   72 |  265 | `			 pLast = (SyhttpHeader *)SySetPeek(pOut);` |
|   36 |  266 | `		 }` |
|   72 |  267 | `		 if( bEol ){` |
|   24 |  268 | `			 break;` |
|    - |  269 | `		 }` |
|  ! 0 |  270 | `	 } /* for(;;) */` |
|   24 |  271 | `	 return SXRET_OK;` |
|  ! 0 |  272 | ` }` |
|    - |  273 | ` /*` |
|    - |  274 | `  * Process the first line of a HTTP request.` |
|    - |  275 | `  * This routine perform the following operations` |
|    - |  276 | `  *  1) Extract the HTTP method.` |
|    - |  277 | `  *  2) Split the request URI to it's fields [ie: host,path,query,...].` |
|    - |  278 | `  *  3) Extract the HTTP protocol version.` |
|    - |  279 | `  */` |
|   24 |  280 | ` static sxi32 VmHttpProcessFirstLine(` |
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
|   24 |  294 | `	 rc = VmGetNextLine(pRequest,&sLine);` |
|   24 |  295 | `	 if( rc != SXRET_OK ){` |
|  ! 0 |  296 | `		 return rc;` |
|    - |  297 | `	 }` |
|   24 |  298 | `	 if ( sLine.nByte < 1 ){` |
|    - |  299 | `		 /* Empty HTTP request */` |
|  ! 0 |  300 | `		 return SXERR_EMPTY;` |
|    - |  301 | `	 }` |
|    - |  302 | `	 /* Delimit the line and ignore trailing and leading white spaces */` |
|   24 |  303 | `	 zIn = sLine.zString;` |
|   24 |  304 | `	 zEnd = &zIn[sLine.nByte];` |
|   24 |  305 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|  ! 0 |  306 | `		 zIn++;` |
|  ! 0 |  307 | `	 }` |
|    - |  308 | `	 /* Extract the HTTP method */` |
|   24 |  309 | `	 zPtr = zIn;` |
|   96 |  310 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|   72 |  311 | `		 zIn++;` |
|  ! 0 |  312 | `	 }` |
|   24 |  313 | `	 *pMethod = HTTP_METHOD_OTHR;` |
|   24 |  314 | `	 if( zIn > zPtr ){` |
|    - |  315 | `		 sxu32 i;` |
|   24 |  316 | `		 nLen = (sxu32)(zIn-zPtr);` |
|   24 |  317 | `		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){` |
|   24 |  318 | `			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){` |
|   24 |  319 | `				 *pMethod = aMethods[i];` |
|   24 |  320 | `				 break;` |
|    - |  321 | `			 }` |
|  ! 0 |  322 | `		 }` |
|   12 |  323 | `	 }` |
|    - |  324 | `	 /* Jump trailing white spaces */` |
|   48 |  325 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|   24 |  326 | `		 zIn++;` |
|  ! 0 |  327 | `	 }` |
|    - |  328 | `	  /* Extract the request URI */` |
|   24 |  329 | `	 zPtr = zIn;` |
|  256 |  330 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|  232 |  331 | `		 zIn++;` |
|  ! 0 |  332 | `	 }` |
|   24 |  333 | `	 if( zIn > zPtr ){` |
|   24 |  334 | `		 nLen = (sxu32)(zIn-zPtr);` |
|    - |  335 | `		 /* Split raw URI to it's fields */` |
|   24 |  336 | `		 PH7_VmHttpSplitURI(pUri,zPtr,nLen);` |
|   12 |  337 | `	 }` |
|    - |  338 | `	 /* Jump trailing white spaces */` |
|   48 |  339 | `	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|   24 |  340 | `		 zIn++;` |
|  ! 0 |  341 | `	 }` |
|    - |  342 | `	 /* Extract the HTTP version */` |
|   24 |  343 | `	 zPtr = zIn;` |
|  216 |  344 | `	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){` |
|  192 |  345 | `		 zIn++;` |
|  ! 0 |  346 | `	 }` |
|   24 |  347 | `	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */` |
|   24 |  348 | `	 rc = 1;` |
|   24 |  349 | `	 if( zIn > zPtr ){` |
|   24 |  350 | `		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));` |
|   12 |  351 | `	 }` |
|   24 |  352 | `	 if( !rc ){` |
|  ! 0 |  353 | `		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */` |
|  ! 0 |  354 | `	 }` |
|   24 |  355 | `	 return SXRET_OK;` |
|   12 |  356 | ` }` |
|    - |  357 | ` /*` |
|    - |  358 | `  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"` |
|    - |  359 | `  * into a name value pair.` |
|    - |  360 | `  * Note that this encoding is implicit in GET based requests.` |
|    - |  361 | `  * After the tokenization process,register the decoded queries` |
|    - |  362 | `  * in the $_GET/$_POST/$_REQUEST superglobals arrays.` |
|    - |  363 | `  */` |
|    8 |  364 | ` static sxi32 VmHttpSplitEncodedQuery(` |
|    - |  365 | `	 ph7_vm *pVm,       /* Target VM */` |
|    - |  366 | `	 SyString *pQuery,  /* Raw query to decode */` |
|    - |  367 | `	 SyBlob *pWorker,   /* Working buffer */` |
|    - |  368 | `	 int is_post        /* TRUE if we are dealing with a POST request */` |
|    - |  369 | `	 )` |
|  ! 0 |  370 | ` {` |
|    8 |  371 | `	 const char *zEnd = &pQuery->zString[pQuery->nByte];` |
|    8 |  372 | `	 const char *zIn = pQuery->zString;` |
|    - |  373 | `	 ph7_value *pGet,*pRequest;` |
|    - |  374 | `	 SyString sName,sValue;` |
|    - |  375 | `	 const char *zPtr;` |
|    - |  376 | `	 sxu32 nBlobOfft;` |
|    - |  377 | `	 /* Extract superglobals */` |
|    8 |  378 | `	 if( is_post ){` |
|    - |  379 | `		 /* $_POST superglobal */` |
|  ! 0 |  380 | `		 pGet = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|  ! 0 |  381 | `	 }else{` |
|    - |  382 | `		 /* $_GET superglobal */` |
|    8 |  383 | `		 pGet = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|    - |  384 | `	 }` |
|    8 |  385 | `	 pRequest = PH7_VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);` |
|    - |  386 | `	 /* Split up the raw query */` |
|    8 |  387 | `	 for(;;){` |
|    - |  388 | `		 /* Jump leading white spaces */` |
|   16 |  389 | `		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){` |
|  ! 0 |  390 | `			 zIn++;` |
|  ! 0 |  391 | `		 }` |
|   16 |  392 | `		 if( zIn >= zEnd ){` |
|    8 |  393 | `			 break;` |
|    - |  394 | `		 }` |
|    8 |  395 | `		 zPtr = zIn;` |
|   20 |  396 | `		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|   12 |  397 | `			 zPtr++;` |
|  ! 0 |  398 | `		 }` |
|    - |  399 | `		 /* Reset the working buffer */` |
|    8 |  400 | `		 SyBlobReset(pWorker);` |
|    - |  401 | `		 /* Decode the entry */` |
|    8 |  402 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|    - |  403 | `		 /* Save the entry */` |
|    8 |  404 | `		 sName.nByte = SyBlobLength(pWorker);` |
|    8 |  405 | `		 sValue.zString = 0;` |
|    8 |  406 | `		 sValue.nByte = 0;` |
|    8 |  407 | `		 if( zPtr < zEnd && zPtr[0] == '=' ){` |
|    8 |  408 | `			 zPtr++;` |
|    8 |  409 | `			 zIn = zPtr;` |
|    - |  410 | `			 /* Store field value */` |
|   20 |  411 | `			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){` |
|   12 |  412 | `				 zPtr++;` |
|  ! 0 |  413 | `			 }` |
|    8 |  414 | `			 if( zPtr > zIn ){` |
|    - |  415 | `				 /* Decode the value */` |
|    8 |  416 | `				  nBlobOfft = SyBlobLength(pWorker);` |
|    8 |  417 | `				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|    8 |  418 | `				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);` |
|    8 |  419 | `				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;` |
|    - |  420 |  |
|    4 |  421 | `			 }` |
|    - |  422 | `			 /* Synchronize pointers */` |
|    8 |  423 | `			 zIn = zPtr;` |
|    4 |  424 | `		 }` |
|    8 |  425 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|    - |  426 | `		 /* Install the decoded query in the $_GET/$_REQUEST array */` |
|    8 |  427 | `		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){` |
|   12 |  428 | `			 PH7_VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,` |
|    8 |  429 | `				 sName.zString,(int)sName.nByte,` |
|    8 |  430 | `				 sValue.zString,(int)sValue.nByte` |
|    - |  431 | `				 );` |
|    4 |  432 | `		 }` |
|    8 |  433 | `		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){` |
|   12 |  434 | `			 PH7_VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,` |
|    8 |  435 | `				 sName.zString,(int)sName.nByte,` |
|    8 |  436 | `				 sValue.zString,(int)sValue.nByte` |
|    - |  437 | `					 );` |
|    4 |  438 | `		 }` |
|    - |  439 | `		 /* Advance the pointer */` |
|    8 |  440 | `		 zIn = &zPtr[1];` |
|  ! 0 |  441 | `	 }` |
|    - |  442 | `	/* All done*/` |
|    8 |  443 | `	return SXRET_OK;` |
|  ! 0 |  444 | ` }` |
|    - |  445 | ` /*` |
|    - |  446 | `  * Extract MIME header value from the given set.` |
|    - |  447 | `  * Return header value on success. NULL otherwise.` |
|    - |  448 | `  */` |
|  216 |  449 | ` static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)` |
|  ! 0 |  450 | ` {` |
|    - |  451 | `	 SyhttpHeader *aMime,*pMime;` |
|    - |  452 | `	 SyString sMime;` |
|    - |  453 | `	 sxu32 n;` |
|  216 |  454 | `	 SyStringInitFromBuf(&sMime,zMime,nByte);` |
|    - |  455 | `	 /* Point to the MIME entries */` |
|  216 |  456 | `	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);` |
|    - |  457 | `	 /* Perform the lookup */` |
|  720 |  458 | `	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|  576 |  459 | `		 pMime = &aMime[n];` |
|  576 |  460 | `		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){` |
|    - |  461 | `			 /* Header found,return it's associated value */` |
|   72 |  462 | `			 return &pMime->sValue;` |
|    - |  463 | `		 }` |
|  252 |  464 | `	 }` |
|    - |  465 | `	 /* No such MIME header */` |
|  144 |  466 | `	 return 0;` |
|  108 |  467 | ` }` |
|    - |  468 | ` /*` |
|    - |  469 | `  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair` |
|    - |  470 | `  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.` |
|    - |  471 | `  */` |
|  ! 0 |  472 | ` static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)` |
|  ! 0 |  473 | ` {` |
|  ! 0 |  474 | `	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];` |
|    - |  475 | `	 SyString sName,sValue;` |
|    - |  476 | `	 ph7_value *pCookie;` |
|    - |  477 | `	 sxu32 nOfft;` |
|    - |  478 | `	 /* Make sure the $_COOKIE superglobal is available */` |
|  ! 0 |  479 | `	 pCookie = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|  ! 0 |  480 | `	 if( pCookie == 0 \|\| (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    - |  481 | `		 /* $_COOKIE superglobal not available */` |
|  ! 0 |  482 | `		 return SXERR_NOTFOUND;` |
|    - |  483 | `	 }` |
|  ! 0 |  484 | `	 for(;;){` |
|    - |  485 | `		  /* Jump leading white spaces */` |
|  ! 0 |  486 | `		 while( zIn < zEnd && SyisSpace(zIn[0]) ){` |
|  ! 0 |  487 | `			 zIn++;` |
|  ! 0 |  488 | `		 }` |
|  ! 0 |  489 | `		 if( zIn >= zEnd ){` |
|  ! 0 |  490 | `			 break;` |
|    - |  491 | `		 }` |
|    - |  492 | `		  /* Reset the working buffer */` |
|  ! 0 |  493 | `		 SyBlobReset(pWorker);` |
|  ! 0 |  494 | `		 zDelimiter = zIn;` |
|    - |  495 | `		 /* Delimit the name[=value]; pair */` |
|  ! 0 |  496 | `		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){` |
|  ! 0 |  497 | `			 zDelimiter++;` |
|  ! 0 |  498 | `		 }` |
|  ! 0 |  499 | `		 zPtr = zIn;` |
|  ! 0 |  500 | `		 while( zPtr < zDelimiter && zPtr[0] != '=' ){` |
|  ! 0 |  501 | `			 zPtr++;` |
|  ! 0 |  502 | `		 }` |
|    - |  503 | `		 /* Decode the cookie */` |
|  ! 0 |  504 | `		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);` |
|  ! 0 |  505 | `		 sName.nByte = SyBlobLength(pWorker);` |
|  ! 0 |  506 | `		 zPtr++;` |
|  ! 0 |  507 | `		 sValue.zString = 0;` |
|  ! 0 |  508 | `		 sValue.nByte = 0;` |
|  ! 0 |  509 | `		 if( zPtr < zDelimiter ){` |
|    - |  510 | `			 /* Got a Cookie value */` |
|  ! 0 |  511 | `			 nOfft = SyBlobLength(pWorker);` |
|  ! 0 |  512 | `			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);` |
|  ! 0 |  513 | `			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);` |
|  ! 0 |  514 | `		 }` |
|    - |  515 | `		 /* Synchronize pointers */` |
|  ! 0 |  516 | `		 zIn = &zDelimiter[1];` |
|    - |  517 | `		 /* Perform the insertion */` |
|  ! 0 |  518 | `		 sName.zString = (const char *)SyBlobData(pWorker);` |
|  ! 0 |  519 | `		 PH7_VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,` |
|  ! 0 |  520 | `			 sName.zString,(int)sName.nByte,` |
|  ! 0 |  521 | `			 sValue.zString,(int)sValue.nByte` |
|    - |  522 | `			 );` |
|  ! 0 |  523 | `	 }` |
|  ! 0 |  524 | `	 return SXRET_OK;` |
|  ! 0 |  525 | ` }` |
|    - |  526 | ` /*` |
|    - |  527 | `  * Process a full HTTP request and populate the appropriate arrays` |
|    - |  528 | `  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information` |
|    - |  529 | `  * extracted from the raw HTTP request. As an extension Symisc introduced` |
|    - |  530 | `  * the $_HEADER array which hold a copy of the processed HTTP MIME headers` |
|    - |  531 | `  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].` |
|    - |  532 | `  * This function return SXRET_OK on success. Any other return value indicates` |
|    - |  533 | `  * a malformed HTTP request.` |
|    - |  534 | `  */` |
|   24 |  535 | ` PH7_PRIVATE sxi32 PH7_VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)` |
|  ! 0 |  536 | ` {` |
|    - |  537 | `	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */` |
|    - |  538 | `	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/` |
|    - |  539 | `	 SyhttpHeader *pHeader;            /* MIME header */` |
|    - |  540 | `	 SyhttpUri sUri;     /* Parse of the raw URI*/` |
|    - |  541 | `	 SyBlob sWorker;     /* General purpose working buffer */` |
|    - |  542 | `	 SySet sHeader;      /* MIME headers set */` |
|    - |  543 | `	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/` |
|    - |  544 | `	 sxi32 iVer;         /* HTTP protocol version */` |
|    - |  545 | `	 sxi32 rc;` |
|   24 |  546 | `	 SyStringInitFromBuf(&sRequest,zRequest,nByte);` |
|   24 |  547 | `	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));` |
|   24 |  548 | `	 SyBlobInit(&sWorker,&pVm->sAllocator);` |
|    - |  549 | `	 /* Ignore leading and trailing white spaces*/` |
|  120 |  550 | `	 SyStringFullTrim(&sRequest);` |
|    - |  551 | `	 /* Process the first line */` |
|   24 |  552 | `	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);` |
|   24 |  553 | `	 if( rc != SXRET_OK ){` |
|  ! 0 |  554 | `		 return rc;` |
|    - |  555 | `	 }` |
|    - |  556 | `	 /* Process MIME headers */` |
|   24 |  557 | `	 VmHttpExtractHeaders(&sRequest,&sHeader);` |
|    - |  558 | `	 /*` |
|    - |  559 | `	  * Setup $_SERVER environments` |
|    - |  560 | `	  */` |
|    - |  561 | `	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */` |
|   24 |  562 | `	 ph7_vm_config(pVm,` |
|    - |  563 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  564 | `		 "SERVER_PROTOCOL",` |
|   24 |  565 | `		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",` |
|    - |  566 | `		 sizeof("HTTP/1.1")-1` |
|    - |  567 | `		 );` |
|    - |  568 | `	 /* 'REQUEST_METHOD':  Which request method was used to access the page */` |
|   24 |  569 | `	 ph7_vm_config(pVm,` |
|    - |  570 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  571 | `		 "REQUEST_METHOD",` |
|   24 |  572 | `		 iMethod == HTTP_METHOD_GET ?   "GET" :` |
|  ! 0 |  573 | `		 (iMethod == HTTP_METHOD_POST ? "POST":` |
|  ! 0 |  574 | `		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :` |
|  ! 0 |  575 | `		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),` |
|    - |  576 | `		 -1 /* Compute attribute length automatically */` |
|    - |  577 | `		 );` |
|   24 |  578 | `	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){` |
|    8 |  579 | `		 pValue = &sUri.sQuery;` |
|    - |  580 | `		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */` |
|   12 |  581 | `		 ph7_vm_config(pVm,` |
|    - |  582 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  583 | `			 "QUERY_STRING",` |
|    4 |  584 | `			 pValue->zString,` |
|    4 |  585 | `			 pValue->nByte` |
|    - |  586 | `			 );` |
|    - |  587 | `		 /* Decoded the raw query */` |
|    8 |  588 | `		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);` |
|    4 |  589 | `	 }` |
|    - |  590 | `	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */` |
|   24 |  591 | `	 pValue = &sUri.sRaw;` |
|   36 |  592 | `	 ph7_vm_config(pVm,` |
|    - |  593 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  594 | `		 "REQUEST_URI",` |
|   12 |  595 | `		 pValue->zString,` |
|   12 |  596 | `		 pValue->nByte` |
|    - |  597 | `		 );` |
|    - |  598 | `	 /*` |
|    - |  599 | `	  * 'PATH_INFO'` |
|    - |  600 | `	  * 'ORIG_PATH_INFO'` |
|    - |  601 | `      * Contains any client-provided pathname information trailing the actual script filename but preceding` |
|    - |  602 | `	  * the query string, if available. For instance, if the current script was accessed via the URL` |
|    - |  603 | `	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain` |
|    - |  604 | `	  * /some/stuff.` |
|    - |  605 | `	  */` |
|   24 |  606 | `	 pValue = &sUri.sPath;` |
|   36 |  607 | `	 ph7_vm_config(pVm,` |
|    - |  608 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  609 | `		 "PATH_INFO",` |
|   12 |  610 | `		 pValue->zString,` |
|   12 |  611 | `		 pValue->nByte` |
|    - |  612 | `		 );` |
|   36 |  613 | `	 ph7_vm_config(pVm,` |
|    - |  614 | `		 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  615 | `		 "ORIG_PATH_INFO",` |
|   12 |  616 | `		 pValue->zString,` |
|   12 |  617 | `		 pValue->nByte` |
|    - |  618 | `		 );` |
|    - |  619 | `	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */` |
|   24 |  620 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);` |
|   24 |  621 | `	 if( pValue ){` |
|   36 |  622 | `		 ph7_vm_config(pVm,` |
|    - |  623 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  624 | `			 "HTTP_ACCEPT",` |
|   12 |  625 | `			 pValue->zString,` |
|   12 |  626 | `			 pValue->nByte` |
|    - |  627 | `		 );` |
|   12 |  628 | `	 }` |
|    - |  629 | `	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */` |
|   24 |  630 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);` |
|   24 |  631 | `	 if( pValue ){` |
|  ! 0 |  632 | `		 ph7_vm_config(pVm,` |
|    - |  633 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  634 | `			 "HTTP_ACCEPT_CHARSET",` |
|  ! 0 |  635 | `			 pValue->zString,` |
|  ! 0 |  636 | `			 pValue->nByte` |
|    - |  637 | `		 );` |
|  ! 0 |  638 | `	 }` |
|    - |  639 | `	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */` |
|   24 |  640 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);` |
|   24 |  641 | `	 if( pValue ){` |
|  ! 0 |  642 | `		 ph7_vm_config(pVm,` |
|    - |  643 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  644 | `			 "HTTP_ACCEPT_ENCODING",` |
|  ! 0 |  645 | `			 pValue->zString,` |
|  ! 0 |  646 | `			 pValue->nByte` |
|    - |  647 | `		 );` |
|  ! 0 |  648 | `	 }` |
|    - |  649 | `	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */` |
|   24 |  650 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);` |
|   24 |  651 | `	 if( pValue ){` |
|  ! 0 |  652 | `		 ph7_vm_config(pVm,` |
|    - |  653 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  654 | `			 "HTTP_ACCEPT_LANGUAGE",` |
|  ! 0 |  655 | `			 pValue->zString,` |
|  ! 0 |  656 | `			 pValue->nByte` |
|    - |  657 | `		 );` |
|  ! 0 |  658 | `	 }` |
|    - |  659 | `	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */` |
|   24 |  660 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);` |
|   24 |  661 | `	 if( pValue ){` |
|  ! 0 |  662 | `		 ph7_vm_config(pVm,` |
|    - |  663 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  664 | `			 "HTTP_CONNECTION",` |
|  ! 0 |  665 | `			 pValue->zString,` |
|  ! 0 |  666 | `			 pValue->nByte` |
|    - |  667 | `		 );` |
|  ! 0 |  668 | `	 }` |
|    - |  669 | `	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */` |
|   24 |  670 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);` |
|   24 |  671 | `	 if( pValue ){` |
|   36 |  672 | `		 ph7_vm_config(pVm,` |
|    - |  673 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  674 | `			 "HTTP_HOST",` |
|   12 |  675 | `			 pValue->zString,` |
|   12 |  676 | `			 pValue->nByte` |
|    - |  677 | `		 );` |
|   12 |  678 | `	 }` |
|    - |  679 | `	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */` |
|   24 |  680 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);` |
|   24 |  681 | `	 if( pValue ){` |
|  ! 0 |  682 | `		 ph7_vm_config(pVm,` |
|    - |  683 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  684 | `			 "HTTP_REFERER",` |
|  ! 0 |  685 | `			 pValue->zString,` |
|  ! 0 |  686 | `			 pValue->nByte` |
|    - |  687 | `		 );` |
|  ! 0 |  688 | `	 }` |
|    - |  689 | `	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */` |
|   24 |  690 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);` |
|   24 |  691 | `	 if( pValue ){` |
|   36 |  692 | `		 ph7_vm_config(pVm,` |
|    - |  693 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  694 | `			 "HTTP_USER_AGENT",` |
|   12 |  695 | `			 pValue->zString,` |
|   12 |  696 | `			 pValue->nByte` |
|    - |  697 | `		 );` |
|   12 |  698 | `	 }` |
|    - |  699 | `	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'` |
|    - |  700 | `	   * header sent by the client (which you should then use to make the appropriate validation).` |
|    - |  701 | `	   */` |
|   24 |  702 | `	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);` |
|   24 |  703 | `	 if( pValue ){` |
|  ! 0 |  704 | `		 ph7_vm_config(pVm,` |
|    - |  705 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  706 | `			 "PHP_AUTH_DIGEST",` |
|  ! 0 |  707 | `			 pValue->zString,` |
|  ! 0 |  708 | `			 pValue->nByte` |
|    - |  709 | `		 );` |
|  ! 0 |  710 | `		 ph7_vm_config(pVm,` |
|    - |  711 | `			 PH7_VM_CONFIG_SERVER_ATTR,` |
|    - |  712 | `			 "PHP_AUTH",` |
|  ! 0 |  713 | `			 pValue->zString,` |
|  ! 0 |  714 | `			 pValue->nByte` |
|    - |  715 | `		 );` |
|  ! 0 |  716 | `	 }` |
|    - |  717 | `	 /* Install all clients HTTP headers in the $_HEADER superglobal */` |
|   24 |  718 | `	 pHeaderArray = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|    - |  719 | `	 /* Iterate throw the available MIME headers*/` |
|   24 |  720 | `	 SySetResetCursor(&sHeader);` |
|   24 |  721 | `	 pHeader = 0; /* stupid cc warning */` |
|  108 |  722 | `	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){` |
|   72 |  723 | `		 pName  = &pHeader->sName;` |
|   72 |  724 | `		 pValue = &pHeader->sValue;` |
|   72 |  725 | `		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){` |
|    - |  726 | `			 /* Insert the MIME header and it's associated value */` |
|  108 |  727 | `			 PH7_VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,` |
|   72 |  728 | `				 pName->zString,(int)pName->nByte,` |
|   72 |  729 | `				 pValue->zString,(int)pValue->nByte` |
|    - |  730 | `				 );` |
|   36 |  731 | `		 }` |
|   72 |  732 | `		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0` |
|   12 |  733 | `			 && pValue->nByte > 0){` |
|    - |  734 | `				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */` |
|  ! 0 |  735 | `				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);` |
|  ! 0 |  736 | `		 }` |
|  ! 0 |  737 | `	 }` |
|   24 |  738 | `	 if( iMethod == HTTP_METHOD_POST ){` |
|    - |  739 | `		 /* Extract raw POST data */` |
|  ! 0 |  740 | `		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);` |
|  ! 0 |  741 | `		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&` |
|  ! 0 |  742 | `			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){` |
|    - |  743 | `				 /* Extract POST data length */` |
|  ! 0 |  744 | `				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);` |
|  ! 0 |  745 | `				 if( pValue ){` |
|  ! 0 |  746 | `					 sxi32 iLen = 0; /* POST data length */` |
|  ! 0 |  747 | `					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);` |
|  ! 0 |  748 | `					 if( iLen > 0 ){` |
|    - |  749 | `						 /* Remove leading and trailing white spaces */` |
|  ! 0 |  750 | `						 SyStringFullTrim(&sRequest);` |
|  ! 0 |  751 | `						 if( (int)sRequest.nByte > iLen ){` |
|  ! 0 |  752 | `							 sRequest.nByte = (sxu32)iLen;` |
|  ! 0 |  753 | `						 }` |
|    - |  754 | `						 /* Decode POST data now */` |
|  ! 0 |  755 | `						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);` |
|  ! 0 |  756 | `					 }` |
|  ! 0 |  757 | `				 }` |
|  ! 0 |  758 | `		 }` |
|  ! 0 |  759 | `	 }` |
|    - |  760 | `	 /* All done,clean-up the mess left behind */` |
|   24 |  761 | `	 SySetRelease(&sHeader);` |
|   24 |  762 | `	 SyBlobRelease(&sWorker);` |
|   24 |  763 | `	 return SXRET_OK;` |
|   12 |  764 | ` }` |
|    - |  765 |  |
