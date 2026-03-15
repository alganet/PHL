/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    HTTP/URI related routines.
 * Status:
 *    Stable.
 */
 /*
  * URI Parser: Split an URI into components [i.e: Host,Path,Query,...].
  * URI syntax: [method:/][/[user[:pwd]@]host[:port]/][document]
  * This almost, but not quite, RFC1738 URI syntax.
  * This routine is not a validator,it does not check for validity
  * nor decode URI parts,the only thing this routine does is splitting
  * the input to its fields.
  * Upper layer are responsible of decoding and validating URI parts.
  * On success,this function populate the "SyhttpUri" structure passed
  * as the first argument. Otherwise SXERR_* is returned when a malformed
  * input is encountered.
  */
 PH7_PRIVATE sxi32 PH7_VmHttpSplitURI(SyhttpUri *pOut,const char *zUri,sxu32 nLen)
 {
	 const char *zEnd = &zUri[nLen];
	 sxu8 bHostOnly = FALSE;
	 sxu8 bIPv6 = FALSE	;
	 const char *zCur;
	 SyString *pComp;
	 sxu32 nPos = 0;
	 sxi32 rc;
	 /* Zero the structure first */
	 SyZero(pOut,sizeof(SyhttpUri));
	 /* Remove leading and trailing white spaces  */
	 SyStringInitFromBuf(&pOut->sRaw,zUri,nLen);
	 SyStringFullTrim(&pOut->sRaw);
	 /* Find the first '/' separator */
	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);
	 if( rc != SXRET_OK ){
		 /* Assume a host name only */
		 zCur = zEnd;
		 bHostOnly = TRUE;
		 goto ProcessHost;
	 }
	 zCur = &zUri[nPos];
	 if( zUri != zCur && zCur[-1] == ':' ){
		 /* Extract a scheme:
		  * Not that we can get an invalid scheme here.
		  * Fortunately the caller can discard any URI by comparing this scheme with its
		  * registered schemes and will report the error as soon as his comparison function
		  * fail.
		  */
	 	pComp = &pOut->sScheme;
		SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri - 1));
		SyStringLeftTrim(pComp);
	 }
	 if( zCur[1] != '/' ){
		 if( zCur == zUri || zCur[-1] == ':' ){
		  /* No authority */
		  goto PathSplit;
		}
		 /* There is something here , we will assume its an authority
		  * and someone has forgot the two prefix slashes "//",
		  * sooner or later we will detect if we are dealing with a malicious
		  * user or not,but now assume we are dealing with an authority
		  * and let the caller handle all the validation process.
		  */
		 goto ProcessHost;
	 }
	 zUri = &zCur[2];
	 zCur = zEnd;
	 rc = SyByteFind(zUri,(sxu32)(zEnd - zUri),'/',&nPos);
	 if( rc == SXRET_OK ){
		 zCur = &zUri[nPos];
	 }
 ProcessHost:
	 /* Extract user information if present */
	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),'@',&nPos);
	 if( rc == SXRET_OK ){
		 if( nPos > 0 ){
			 sxu32 nPassOfft; /* Password offset */
			 pComp = &pOut->sUser;
			 SyStringInitFromBuf(pComp,zUri,nPos);
			 /* Extract the password if available */
			 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPassOfft);
			 if( rc == SXRET_OK && nPassOfft < nPos){
				 pComp->nByte = nPassOfft;
				 pComp = &pOut->sPass;
				 pComp->zString = &zUri[nPassOfft+sizeof(char)];
				 pComp->nByte = nPos - nPassOfft - 1;
			 }
			 /* Update the cursor */
			 zUri = &zUri[nPos+1];
		 }else{
			 zUri++;
		 }
	 }
	 pComp = &pOut->sHost;
	 while( zUri < zCur && SyisSpace(zUri[0])){
		 zUri++;
	 }
	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zCur - zUri));
	 if( pComp->zString[0] == '[' ){
		 /* An IPv6 Address: Make a simple naive test
		  */
		 zUri++; pComp->zString++; pComp->nByte = 0;
		 while( ((unsigned char)zUri[0] < 0xc0 && SyisHex(zUri[0])) || zUri[0] == ':' ){
			 zUri++; pComp->nByte++;
		 }
		 if( zUri[0] != ']' ){
			 return SXERR_CORRUPT; /* Malformed IPv6 address */
		 }
		 zUri++;
		 bIPv6 = TRUE;
	 }
	 /* Extract a port number if available */
	 rc = SyByteFind(zUri,(sxu32)(zCur - zUri),':',&nPos);
	 if( rc == SXRET_OK ){
		 if( bIPv6 == FALSE ){
			 pComp->nByte = (sxu32)(&zUri[nPos] - zUri);
		 }
		 pComp = &pOut->sPort;
		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zCur - &zUri[nPos+1]));
	 }
	 if( bHostOnly == TRUE ){
		 return SXRET_OK;
	 }
PathSplit:
	 zUri = zCur;
	 pComp = &pOut->sPath;
	 SyStringInitFromBuf(pComp,zUri,(sxu32)(zEnd-zUri));
	 if( pComp->nByte == 0 ){
		 return SXRET_OK; /* Empty path */
	 }
	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'?',&nPos) ){
		 pComp->nByte = nPos; /* Update path length */
		 pComp = &pOut->sQuery;
		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]));
	 }
	 if( SXRET_OK == SyByteFind(zUri,(sxu32)(zEnd-zUri),'#',&nPos) ){
		 /* Update path or query length */
		 if( pComp == &pOut->sPath ){
			 pComp->nByte = nPos;
		 }else{
			 if( &zUri[nPos] < (char *)SyStringData(pComp) ){
				 /* Malformed syntax : Query must be present before fragment */
				 return SXERR_SYNTAX;
			 }
			 pComp->nByte -= (sxu32)(zEnd - &zUri[nPos]);
		 }
		 pComp = &pOut->sFragment;
		 SyStringInitFromBuf(pComp,&zUri[nPos+1],(sxu32)(zEnd-&zUri[nPos+1]))
	 }
	 return SXRET_OK;
 }
 /*
 * Extract a single line from a raw HTTP request.
 * Return SXRET_OK on success,SXERR_EOF when end of input
 * and SXERR_MORE when more input is needed.
 */
static sxi32 VmGetNextLine(SyString *pCursor,SyString *pCurrent)
{
  	const char *zIn;
  	sxu32 nPos;
	/* Jump leading white spaces */
	SyStringLeftTrim(pCursor);
	if( pCursor->nByte < 1 ){
		SyStringInitFromBuf(pCurrent,0,0);
		return SXERR_EOF; /* End of input */
	}
	zIn = SyStringData(pCursor);
	if( SXRET_OK != SyByteListFind(pCursor->zString,pCursor->nByte,"\r\n",&nPos) ){
		/* Line not found,tell the caller to read more input from source */
		SyStringDupPtr(pCurrent,pCursor);
		return SXERR_MORE;
	}
  	pCurrent->zString = zIn;
  	pCurrent->nByte	= nPos;
  	/* advance the cursor so we can call this routine again */
  	pCursor->zString = &zIn[nPos];
  	pCursor->nByte -= nPos;
  	return SXRET_OK;
 }
 /*
  * Split a single MIME header into a name value pair.
  * This function return SXRET_OK,SXERR_CONTINUE on success.
  * Otherwise SXERR_NEXT is returned when a malformed header
  * is encountered.
  * Note: This function handle also mult-line headers.
  */
 static sxi32 VmHttpProcessOneHeader(SyhttpHeader *pHdr,SyhttpHeader *pLast,const char *zLine,sxu32 nLen)
 {
	 SyString *pName;
	 sxu32 nPos;
	 sxi32 rc;
	 if( nLen < 1 ){
		 return SXERR_NEXT;
	 }
	 /* Check for multi-line header */
	if( pLast && (zLine[-1] == ' ' || zLine[-1] == '\t') ){
		SyString *pTmp = &pLast->sValue;
		SyStringFullTrim(pTmp);
		if( pTmp->nByte == 0 ){
			SyStringInitFromBuf(pTmp,zLine,nLen);
		}else{
			/* Update header value length */
			pTmp->nByte = (sxu32)(&zLine[nLen] - pTmp->zString);
		}
		 /* Simply tell the caller to reset its states and get another line */
		 return SXERR_CONTINUE;
	 }
	/* Split the header */
	pName = &pHdr->sName;
	rc = SyByteFind(zLine,nLen,':',&nPos);
	if(rc != SXRET_OK ){
		return SXERR_NEXT; /* Malformed header;Check the next entry */
	}
	SyStringInitFromBuf(pName,zLine,nPos);
	SyStringFullTrim(pName);
	/* Extract a header value */
	SyStringInitFromBuf(&pHdr->sValue,&zLine[nPos + 1],nLen - nPos - 1);
	/* Remove leading and trailing whitespaces */
	SyStringFullTrim(&pHdr->sValue);
	return SXRET_OK;
 }
 /*
  * Extract all MIME headers associated with a HTTP request.
  * After processing the first line of a HTTP request,the following
  * routine is called in order to extract MIME headers.
  * This function return SXRET_OK on success,SXERR_MORE when it needs
  * more inputs.
  * Note: Any malformed header is simply discarded.
  */
 static sxi32 VmHttpExtractHeaders(SyString *pRequest,SySet *pOut)
 {
	 SyhttpHeader *pLast = 0;
	 SyString sCurrent;
	 SyhttpHeader sHdr;
	 sxu8 bEol;
	 sxi32 rc;
	 if( SySetUsed(pOut) > 0 ){
		 pLast = (SyhttpHeader *)SySetAt(pOut,SySetUsed(pOut)-1);
	 }
	 bEol = FALSE;
	 for(;;){
		 SyZero(&sHdr,sizeof(SyhttpHeader));
		 /* Extract a single line from the raw HTTP request */
		 rc = VmGetNextLine(pRequest,&sCurrent);
		 if(rc != SXRET_OK ){
			 if( sCurrent.nByte < 1 ){
				 break;
			 }
			 bEol = TRUE;
		 }
		 /* Process the header */
		 if( SXRET_OK == VmHttpProcessOneHeader(&sHdr,pLast,sCurrent.zString,sCurrent.nByte)){
			 if( SXRET_OK != SySetPut(pOut,(const void *)&sHdr) ){
				 break;
			 }
			 /* Retrieve the last parsed header so we can handle multi-line header
			  * in case we face one of them.
			  */
			 pLast = (SyhttpHeader *)SySetPeek(pOut);
		 }
		 if( bEol ){
			 break;
		 }
	 } /* for(;;) */
	 return SXRET_OK;
 }
 /*
  * Process the first line of a HTTP request.
  * This routine perform the following operations
  *  1) Extract the HTTP method.
  *  2) Split the request URI to it's fields [ie: host,path,query,...].
  *  3) Extract the HTTP protocol version.
  */
 static sxi32 VmHttpProcessFirstLine(
	 SyString *pRequest, /* Raw HTTP request */
	 sxi32 *pMethod,     /* OUT: HTTP method */
	 SyhttpUri *pUri,    /* OUT: Parse of the URI */
	 sxi32 *pProto       /* OUT: HTTP protocol */
	 )
 {
	 static const char *azMethods[] = { "get","post","head","put"};
	 static const sxi32 aMethods[]  = { HTTP_METHOD_GET,HTTP_METHOD_POST,HTTP_METHOD_HEAD,HTTP_METHOD_PUT};
	 const char *zIn,*zEnd,*zPtr;
	 SyString sLine;
	 sxu32 nLen;
	 sxi32 rc;
	 /* Extract the first line and update the pointer */
	 rc = VmGetNextLine(pRequest,&sLine);
	 if( rc != SXRET_OK ){
		 return rc;
	 }
	 if ( sLine.nByte < 1 ){
		 /* Empty HTTP request */
		 return SXERR_EMPTY;
	 }
	 /* Delimit the line and ignore trailing and leading white spaces */
	 zIn = sLine.zString;
	 zEnd = &zIn[sLine.nByte];
	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 /* Extract the HTTP method */
	 zPtr = zIn;
	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 *pMethod = HTTP_METHOD_OTHR;
	 if( zIn > zPtr ){
		 sxu32 i;
		 nLen = (sxu32)(zIn-zPtr);
		 for( i = 0 ; i < SX_ARRAYSIZE(azMethods) ; ++i ){
			 if( SyStrnicmp(azMethods[i],zPtr,nLen) == 0 ){
				 *pMethod = aMethods[i];
				 break;
			 }
		 }
	 }
	 /* Jump trailing white spaces */
	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	  /* Extract the request URI */
	 zPtr = zIn;
	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 if( zIn > zPtr ){
		 nLen = (sxu32)(zIn-zPtr);
		 /* Split raw URI to it's fields */
		 PH7_VmHttpSplitURI(pUri,zPtr,nLen);
	 }
	 /* Jump trailing white spaces */
	 while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 /* Extract the HTTP version */
	 zPtr = zIn;
	 while( zIn < zEnd && !SyisSpace(zIn[0]) ){
		 zIn++;
	 }
	 *pProto = HTTP_PROTO_11; /* HTTP/1.1 */
	 rc = 1;
	 if( zIn > zPtr ){
		 rc = SyStrnicmp(zPtr,"http/1.0",(sxu32)(zIn-zPtr));
	 }
	 if( !rc ){
		 *pProto = HTTP_PROTO_10; /* HTTP/1.0 */
	 }
	 return SXRET_OK;
 }
 /*
  * Tokenize,decode and split a raw query encoded as: "x-www-form-urlencoded"
  * into a name value pair.
  * Note that this encoding is implicit in GET based requests.
  * After the tokenization process,register the decoded queries
  * in the $_GET/$_POST/$_REQUEST superglobals arrays.
  */
 static sxi32 VmHttpSplitEncodedQuery(
	 ph7_vm *pVm,       /* Target VM */
	 SyString *pQuery,  /* Raw query to decode */
	 SyBlob *pWorker,   /* Working buffer */
	 int is_post        /* TRUE if we are dealing with a POST request */
	 )
 {
	 const char *zEnd = &pQuery->zString[pQuery->nByte];
	 const char *zIn = pQuery->zString;
	 ph7_value *pGet,*pRequest;
	 SyString sName,sValue;
	 const char *zPtr;
	 sxu32 nBlobOfft;
	 /* Extract superglobals */
	 if( is_post ){
		 /* $_POST superglobal */
		 pGet = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);
	 }else{
		 /* $_GET superglobal */
		 pGet = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);
	 }
	 pRequest = PH7_VmExtractSuper(&(*pVm),"_REQUEST",sizeof("_REQUEST")-1);
	 /* Split up the raw query */
	 for(;;){
		 /* Jump leading white spaces */
		 while(zIn < zEnd  && SyisSpace(zIn[0]) ){
			 zIn++;
		 }
		 if( zIn >= zEnd ){
			 break;
		 }
		 zPtr = zIn;
		 while( zPtr < zEnd && zPtr[0] != '=' && zPtr[0] != '&' && zPtr[0] != ';' ){
			 zPtr++;
		 }
		 /* Reset the working buffer */
		 SyBlobReset(pWorker);
		 /* Decode the entry */
		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);
		 /* Save the entry */
		 sName.nByte = SyBlobLength(pWorker);
		 sValue.zString = 0;
		 sValue.nByte = 0;
		 if( zPtr < zEnd && zPtr[0] == '=' ){
			 zPtr++;
			 zIn = zPtr;
			 /* Store field value */
			 while( zPtr < zEnd && zPtr[0] != '&' && zPtr[0] != ';' ){
				 zPtr++;
			 }
			 if( zPtr > zIn ){
				 /* Decode the value */
				  nBlobOfft = SyBlobLength(pWorker);
				  SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);
				  sValue.zString = (const char *)SyBlobDataAt(pWorker,nBlobOfft);
				  sValue.nByte = SyBlobLength(pWorker) - nBlobOfft;

			 }
			 /* Synchronize pointers */
			 zIn = zPtr;
		 }
		 sName.zString = (const char *)SyBlobData(pWorker);
		 /* Install the decoded query in the $_GET/$_REQUEST array */
		 if( pGet && (pGet->iFlags & MEMOBJ_HASHMAP) ){
			 PH7_VmHashmapInsert((ph7_hashmap *)pGet->x.pOther,
				 sName.zString,(int)sName.nByte,
				 sValue.zString,(int)sValue.nByte
				 );
		 }
		 if( pRequest && (pRequest->iFlags & MEMOBJ_HASHMAP) ){
			 PH7_VmHashmapInsert((ph7_hashmap *)pRequest->x.pOther,
				 sName.zString,(int)sName.nByte,
				 sValue.zString,(int)sValue.nByte
					 );
		 }
		 /* Advance the pointer */
		 zIn = &zPtr[1];
	 }
	/* All done*/
	return SXRET_OK;
 }
 /*
  * Extract MIME header value from the given set.
  * Return header value on success. NULL otherwise.
  */
 static SyString * VmHttpExtractHeaderValue(SySet *pSet,const char *zMime,sxu32 nByte)
 {
	 SyhttpHeader *aMime,*pMime;
	 SyString sMime;
	 sxu32 n;
	 SyStringInitFromBuf(&sMime,zMime,nByte);
	 /* Point to the MIME entries */
	 aMime = (SyhttpHeader *)SySetBasePtr(pSet);
	 /* Perform the lookup */
	 for( n = 0 ; n < SySetUsed(pSet) ; ++n ){
		 pMime = &aMime[n];
		 if( SyStringCmp(&sMime,&pMime->sName,SyStrnicmp) == 0 ){
			 /* Header found,return it's associated value */
			 return &pMime->sValue;
		 }
	 }
	 /* No such MIME header */
	 return 0;
 }
 /*
  * Tokenize and decode a raw "Cookie:" MIME header into a name value pair
  * and insert it's fields [i.e name,value] in the $_COOKIE superglobal.
  */
 static sxi32 VmHttpPorcessCookie(ph7_vm *pVm,SyBlob *pWorker,const char *zIn,sxu32 nByte)
 {
	 const char *zPtr,*zDelimiter,*zEnd = &zIn[nByte];
	 SyString sName,sValue;
	 ph7_value *pCookie;
	 sxu32 nOfft;
	 /* Make sure the $_COOKIE superglobal is available */
	 pCookie = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);
	 if( pCookie == 0 || (pCookie->iFlags & MEMOBJ_HASHMAP) == 0 ){
		 /* $_COOKIE superglobal not available */
		 return SXERR_NOTFOUND;
	 }
	 for(;;){
		  /* Jump leading white spaces */
		 while( zIn < zEnd && SyisSpace(zIn[0]) ){
			 zIn++;
		 }
		 if( zIn >= zEnd ){
			 break;
		 }
		  /* Reset the working buffer */
		 SyBlobReset(pWorker);
		 zDelimiter = zIn;
		 /* Delimit the name[=value]; pair */
		 while( zDelimiter < zEnd && zDelimiter[0] != ';' ){
			 zDelimiter++;
		 }
		 zPtr = zIn;
		 while( zPtr < zDelimiter && zPtr[0] != '=' ){
			 zPtr++;
		 }
		 /* Decode the cookie */
		 SyUriDecode(zIn,(sxu32)(zPtr-zIn),PH7_VmBlobConsumer,pWorker,TRUE);
		 sName.nByte = SyBlobLength(pWorker);
		 zPtr++;
		 sValue.zString = 0;
		 sValue.nByte = 0;
		 if( zPtr < zDelimiter ){
			 /* Got a Cookie value */
			 nOfft = SyBlobLength(pWorker);
			 SyUriDecode(zPtr,(sxu32)(zDelimiter-zPtr),PH7_VmBlobConsumer,pWorker,TRUE);
			 SyStringInitFromBuf(&sValue,SyBlobDataAt(pWorker,nOfft),SyBlobLength(pWorker)-nOfft);
		 }
		 /* Synchronize pointers */
		 zIn = &zDelimiter[1];
		 /* Perform the insertion */
		 sName.zString = (const char *)SyBlobData(pWorker);
		 PH7_VmHashmapInsert((ph7_hashmap *)pCookie->x.pOther,
			 sName.zString,(int)sName.nByte,
			 sValue.zString,(int)sValue.nByte
			 );
	 }
	 return SXRET_OK;
 }
 /*
  * Process a full HTTP request and populate the appropriate arrays
  * such as $_SERVER,$_GET,$_POST,$_COOKIE,$_REQUEST,... with the information
  * extracted from the raw HTTP request. As an extension Symisc introduced
  * the $_HEADER array which hold a copy of the processed HTTP MIME headers
  * and their associated values. [i.e: $_HEADER['Server'],$_HEADER['User-Agent'],...].
  * This function return SXRET_OK on success. Any other return value indicates
  * a malformed HTTP request.
  */
 PH7_PRIVATE sxi32 PH7_VmHttpProcessRequest(ph7_vm *pVm,const char *zRequest,int nByte)
 {
	 SyString *pName,*pValue,sRequest; /* Raw HTTP request */
	 ph7_value *pHeaderArray;          /* $_HEADER superglobal (Symisc eXtension to the PHP specification)*/
	 SyhttpHeader *pHeader;            /* MIME header */
	 SyhttpUri sUri;     /* Parse of the raw URI*/
	 SyBlob sWorker;     /* General purpose working buffer */
	 SySet sHeader;      /* MIME headers set */
	 sxi32 iMethod;      /* HTTP method [i.e: GET,POST,HEAD...]*/
	 sxi32 iVer;         /* HTTP protocol version */
	 sxi32 rc;
	 SyStringInitFromBuf(&sRequest,zRequest,nByte);
	 SySetInit(&sHeader,&pVm->sAllocator,sizeof(SyhttpHeader));
	 SyBlobInit(&sWorker,&pVm->sAllocator);
	 /* Ignore leading and trailing white spaces*/
	 SyStringFullTrim(&sRequest);
	 /* Process the first line */
	 rc = VmHttpProcessFirstLine(&sRequest,&iMethod,&sUri,&iVer);
	 if( rc != SXRET_OK ){
		 return rc;
	 }
	 /* Process MIME headers */
	 VmHttpExtractHeaders(&sRequest,&sHeader);
	 /*
	  * Setup $_SERVER environments
	  */
	 /* 'SERVER_PROTOCOL': Name and revision of the information protocol via which the page was requested */
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "SERVER_PROTOCOL",
		 iVer == HTTP_PROTO_10 ? "HTTP/1.0" : "HTTP/1.1",
		 sizeof("HTTP/1.1")-1
		 );
	 /* 'REQUEST_METHOD':  Which request method was used to access the page */
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "REQUEST_METHOD",
		 iMethod == HTTP_METHOD_GET ?   "GET" :
		 (iMethod == HTTP_METHOD_POST ? "POST":
		 (iMethod == HTTP_METHOD_PUT  ? "PUT" :
		 (iMethod == HTTP_METHOD_HEAD ?  "HEAD" : "OTHER"))),
		 -1 /* Compute attribute length automatically */
		 );
	 if( SyStringLength(&sUri.sQuery) > 0 && iMethod == HTTP_METHOD_GET ){
		 pValue = &sUri.sQuery;
		 /* 'QUERY_STRING': The query string, if any, via which the page was accessed */
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "QUERY_STRING",
			 pValue->zString,
			 pValue->nByte
			 );
		 /* Decoded the raw query */
		 VmHttpSplitEncodedQuery(&(*pVm),pValue,&sWorker,FALSE);
	 }
	 /* REQUEST_URI: The URI which was given in order to access this page; for instance, '/index.html' */
	 pValue = &sUri.sRaw;
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "REQUEST_URI",
		 pValue->zString,
		 pValue->nByte
		 );
	 /*
	  * 'PATH_INFO'
	  * 'ORIG_PATH_INFO'
      * Contains any client-provided pathname information trailing the actual script filename but preceding
	  * the query string, if available. For instance, if the current script was accessed via the URL
	  * http://www.example.com/php/path_info.php/some/stuff?foo=bar, then $_SERVER['PATH_INFO'] would contain
	  * /some/stuff.
	  */
	 pValue = &sUri.sPath;
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "PATH_INFO",
		 pValue->zString,
		 pValue->nByte
		 );
	 ph7_vm_config(pVm,
		 PH7_VM_CONFIG_SERVER_ATTR,
		 "ORIG_PATH_INFO",
		 pValue->zString,
		 pValue->nByte
		 );
	 /* 'HTTP_ACCEPT': Contents of the Accept: header from the current request, if there is one */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept",sizeof("Accept")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_ACCEPT",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_ACCEPT_CHARSET': Contents of the Accept-Charset: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Charset",sizeof("Accept-Charset")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_ACCEPT_CHARSET",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_ACCEPT_ENCODING': Contents of the Accept-Encoding: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Encoding",sizeof("Accept-Encoding")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_ACCEPT_ENCODING",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	  /* 'HTTP_ACCEPT_LANGUAGE': Contents of the Accept-Language: header from the current request, if there is one */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Accept-Language",sizeof("Accept-Language")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_ACCEPT_LANGUAGE",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_CONNECTION': Contents of the Connection: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Connection",sizeof("Connection")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_CONNECTION",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_HOST': Contents of the Host: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Host",sizeof("Host")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_HOST",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_REFERER': Contents of the Referer: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Referer",sizeof("Referer")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_REFERER",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* 'HTTP_USER_AGENT': Contents of the Referer: header from the current request, if there is one. */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"User-Agent",sizeof("User-Agent")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "HTTP_USER_AGENT",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	  /* 'PHP_AUTH_DIGEST': When doing Digest HTTP authentication this variable is set to the 'Authorization'
	   * header sent by the client (which you should then use to make the appropriate validation).
	   */
	 pValue = VmHttpExtractHeaderValue(&sHeader,"Authorization",sizeof("Authorization")-1);
	 if( pValue ){
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "PHP_AUTH_DIGEST",
			 pValue->zString,
			 pValue->nByte
		 );
		 ph7_vm_config(pVm,
			 PH7_VM_CONFIG_SERVER_ATTR,
			 "PHP_AUTH",
			 pValue->zString,
			 pValue->nByte
		 );
	 }
	 /* Install all clients HTTP headers in the $_HEADER superglobal */
	 pHeaderArray = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);
	 /* Iterate throw the available MIME headers*/
	 SySetResetCursor(&sHeader);
	 pHeader = 0; /* stupid cc warning */
	 while( SXRET_OK == SySetGetNextEntry(&sHeader,(void **)&pHeader) ){
		 pName  = &pHeader->sName;
		 pValue = &pHeader->sValue;
		 if( pHeaderArray && (pHeaderArray->iFlags & MEMOBJ_HASHMAP)){
			 /* Insert the MIME header and it's associated value */
			 PH7_VmHashmapInsert((ph7_hashmap *)pHeaderArray->x.pOther,
				 pName->zString,(int)pName->nByte,
				 pValue->zString,(int)pValue->nByte
				 );
		 }
		 if( pName->nByte == sizeof("Cookie")-1 && SyStrnicmp(pName->zString,"Cookie",sizeof("Cookie")-1) == 0
			 && pValue->nByte > 0){
				 /* Process the name=value pair and insert them in the $_COOKIE superglobal array */
				 VmHttpPorcessCookie(&(*pVm),&sWorker,pValue->zString,pValue->nByte);
		 }
	 }
	 if( iMethod == HTTP_METHOD_POST ){
		 /* Extract raw POST data */
		 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Type",sizeof("Content-Type") - 1);
		 if( pValue && pValue->nByte >= sizeof("application/x-www-form-urlencoded") - 1 &&
			 SyMemcmp("application/x-www-form-urlencoded",pValue->zString,pValue->nByte) == 0 ){
				 /* Extract POST data length */
				 pValue = VmHttpExtractHeaderValue(&sHeader,"Content-Length",sizeof("Content-Length") - 1);
				 if( pValue ){
					 sxi32 iLen = 0; /* POST data length */
					 SyStrToInt32(pValue->zString,pValue->nByte,(void *)&iLen,0);
					 if( iLen > 0 ){
						 /* Remove leading and trailing white spaces */
						 SyStringFullTrim(&sRequest);
						 if( (int)sRequest.nByte > iLen ){
							 sRequest.nByte = (sxu32)iLen;
						 }
						 /* Decode POST data now */
						 VmHttpSplitEncodedQuery(&(*pVm),&sRequest,&sWorker,TRUE);
					 }
				 }
		 }
	 }
	 /* All done,clean-up the mess left behind */
	 SySetRelease(&sHeader);
	 SyBlobRelease(&sWorker);
	 return SXRET_OK;
 }
