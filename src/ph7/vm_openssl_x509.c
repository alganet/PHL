/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_OPENSSL
#include "openssl_int.h"
#include <openssl/pkcs12.h>
#include <openssl/pkcs7.h>
#include <openssl/cms.h>
#include <openssl/conf.h>

/*
 * Section:
 *    ext/openssl -- the certificate containers: X.509 certificates, signing
 *    requests, PKCS#12 bundles and the PKCS#7 / CMS message families. The
 *    library-wide surface, the keys and the ciphers are in vm_openssl.c.
 * Status:
 *    Stable.
 *
 * THE CONFIGURATION IS PART OF THE CONTRACT, and it is the thing a reading of
 * php's manual would miss entirely. `openssl_csr_new(['commonName' => 'x'],
 * $key)` does not produce a one-field subject: it produces the caller's fields
 * followed by every `<name>_default` the system openssl.cnf declares for the
 * `req` section, so on a stock Debian box the subject also carries `C=AU`,
 * `ST=Some-State` and `O=Internet Widgits Pty Ltd`. And `openssl_csr_sign()`
 * adds whatever the config's `x509_extensions` section says, which is why a
 * certificate signed with no options at all comes back with a
 * subjectKeyIdentifier, an authorityKeyIdentifier and `CA:TRUE`. Both walks
 * are reproduced below, over the same file php reads (the `config` option, the
 * OPENSSL_CONF environment variable, or the library's compiled-in default).
 *
 * That also means the tests around this family pin what does NOT come from a
 * config file: the shape of a parse, the order of a subject, the refusals.
 */

/* ------------------------------------------------------------------------
 * Reading a certificate or a request out of an argument
 * ------------------------------------------------------------------------ */
static X509 * SslCertFromBytes(const char *zData,int nData)
{
	BIO *pBio;
	X509 *pCert;
	if( nData < 1 ){
		return 0;
	}
	pBio = BIO_new_mem_buf(zData,nData);
	if( pBio == 0 ){
		return 0;
	}
	/* PEM ONLY, and that is php's contract rather than a shortcut: a string
	 * holding a DER certificate is `error:0480006C:PEM routines::no start
	 * line` and false under php too. DER reaches these doors through a
	 * `file://` path, which OpenSSL's own reader sniffs. */
	pCert = PEM_read_bio_X509(pBio,0,0,0);
	BIO_free(pBio);
	return pCert;
}
static X509_REQ * SslCsrFromBytes(const char *zData,int nData)
{
	BIO *pBio;
	X509_REQ *pReq;
	if( nData < 1 ){
		return 0;
	}
	pBio = BIO_new_mem_buf(zData,nData);
	if( pBio == 0 ){
		return 0;
	}
	pReq = PEM_read_bio_X509_REQ(pBio,0,0,0);   /* PEM only -- see SslCertFromBytes */
	BIO_free(pBio);
	return pReq;
}
/*
 * php's `OpenSSLCertificate|string $certificate`: the object, or PEM/DER
 * bytes, or a `file://` path. *pbOwn says whether the caller frees it.
 *
 * bWarn is PER FUNCTION and not a property of the reader, which a sweep had
 * to establish one door at a time: `openssl_x509_read`, both exports,
 * `openssl_x509_fingerprint`, both PKCS#12 exports and the two SIGN doors all
 * say `X.509 Certificate cannot be retrieved` for a string that will not
 * parse, while `openssl_x509_parse`, `check_private_key`, `verify`,
 * `checkpurpose` and both ENCRYPT doors answer false or -1 in silence.
 */
static X509 * SslCertOfValue(ph7_context *pCtx,ph7_value *pVal,int *pbOwn,int bWarn)
{
	SyBlob sFile;
	const char *zData = 0;
	X509 *pCert = 0;
	int nData = 0;
	*pbOwn = 0;
	if( pVal == 0 ){
		return 0;
	}
	if( ph7_value_is_object(pVal) ){
		return (X509 *)PH7_SslHandleOf(pVal,PHL_SSL_KIND_CERT);
	}
	if( !ph7_value_is_string(pVal) ){
		return 0;
	}
	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);
	if( PH7_SslBytesOfValue(pCtx,pVal,&sFile,&zData,&nData) == 0 ){
		pCert = SslCertFromBytes(zData,nData);
	}
	SyBlobRelease(&sFile);
	if( pCert ){
		*pbOwn = 1;
	}else{
		PH7_SslStoreErrors(pCtx->pVm);
		if( bWarn ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"X.509 Certificate cannot be retrieved");
		}
	}
	return pCert;
}
static X509_REQ * SslCsrOfValue(ph7_context *pCtx,ph7_value *pVal,int *pbOwn,int bWarn)
{
	SyBlob sFile;
	const char *zData = 0;
	X509_REQ *pReq = 0;
	int nData = 0;
	*pbOwn = 0;
	if( pVal == 0 ){
		return 0;
	}
	if( ph7_value_is_object(pVal) ){
		return (X509_REQ *)PH7_SslHandleOf(pVal,PHL_SSL_KIND_CSR);
	}
	if( !ph7_value_is_string(pVal) ){
		return 0;
	}
	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);
	if( PH7_SslBytesOfValue(pCtx,pVal,&sFile,&zData,&nData) == 0 ){
		pReq = SslCsrFromBytes(zData,nData);
	}
	SyBlobRelease(&sFile);
	if( pReq ){
		*pbOwn = 1;
	}else{
		PH7_SslStoreErrors(pCtx->pVm);
		if( bWarn ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"X.509 Certificate Signing Request cannot be retrieved");
		}
	}
	return pReq;
}
static void SslFreeCert(X509 *pCert,int bOwn)
{
	if( pCert && bOwn ){
		X509_free(pCert);
	}
}
static void SslFreeCsr(X509_REQ *pReq,int bOwn)
{
	if( pReq && bOwn ){
		X509_REQ_free(pReq);
	}
}

/* ------------------------------------------------------------------------
 * A distinguished name as php's array
 * ------------------------------------------------------------------------ */
/*
 * php's rule for a REPEATED attribute is what makes this more than a loop: the
 * first occurrence is a string, and a second one turns the slot into an ARRAY
 * of every value in order. `$parsed['subject']['OU']` is therefore a string
 * for one organizational unit and a list for two.
 */
static void SslNameToArray(ph7_context *pCtx,X509_NAME *pName,int bShort,ph7_value *pArray)
{
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	ph7_value *pKeep = ph7_context_new_scalar(pCtx);
	ph7_value *pList = 0;
	int i,n;
	if( pName == 0 || pVal == 0 || pKeep == 0 ){
		return;
	}
	n = X509_NAME_entry_count(pName);
	for( i = 0 ; i < n ; ++i ){
		X509_NAME_ENTRY *pEntry = X509_NAME_get_entry(pName,i);
		ASN1_OBJECT *pObj = X509_NAME_ENTRY_get_object(pEntry);
		ASN1_STRING *pStr = X509_NAME_ENTRY_get_data(pEntry);
		int nid = OBJ_obj2nid(pObj);
		const char *zKey;
		char zBuf[256];
		ph7_value *pOld;
		if( nid != NID_undef ){
			zKey = bShort ? OBJ_nid2sn(nid) : OBJ_nid2ln(nid);
		}else{
			OBJ_obj2txt(zBuf,(int)sizeof(zBuf),pObj,1);
			zKey = zBuf;
		}
		if( zKey == 0 ){
			continue;
		}
		pOld = ph7_array_fetch(pArray,zKey,-1);
		if( pOld == 0 ){
			ph7_value_string(pVal,(const char *)ASN1_STRING_get0_data(pStr),
				ASN1_STRING_length(pStr));
			ph7_array_add_strkey_elem(pArray,zKey,pVal);
			ph7_value_reset_string_cursor(pVal);
			continue;
		}
		/* A second value under the same name: php replaces the string with a
		 * list holding both, and appends to that list from then on. */
		if( ph7_value_is_array(pOld) ){
			ph7_value_string(pVal,(const char *)ASN1_STRING_get0_data(pStr),
				ASN1_STRING_length(pStr));
			ph7_array_add_elem(pOld,0,pVal);
			ph7_value_reset_string_cursor(pVal);
			continue;
		}
		/* The first value has to be COPIED out before the list is created:
		 * `ph7_array_fetch` answers a pointer into the VM's memobj pool, and
		 * creating an array grows that pool -- so `pOld` is a dangling pointer
		 * the moment ph7_context_new_array() returns. See the
		 * `pointers-die-across-a-user-callback` note. */
		{
			int nOld = 0;
			const char *zOld = ph7_value_to_string(pOld,&nOld);
			ph7_value_string(pKeep,zOld ? zOld : "",nOld);
		}
		pList = ph7_context_new_array(pCtx);
		if( pList == 0 ){
			ph7_value_reset_string_cursor(pKeep);
			continue;
		}
		ph7_array_add_elem(pList,0,pKeep);
		ph7_value_reset_string_cursor(pKeep);
		ph7_value_string(pVal,(const char *)ASN1_STRING_get0_data(pStr),
			ASN1_STRING_length(pStr));
		ph7_array_add_elem(pList,0,pVal);
		ph7_value_reset_string_cursor(pVal);
		ph7_array_add_strkey_elem(pArray,zKey,pList);
	}
}
/*
 * An ASN.1 time, both ways php reports it: the RAW string the certificate
 * carries ("260824120000Z"), and the epoch second it means. The conversion is
 * the engine's own civil-days arithmetic rather than the C library's timegm(),
 * which Windows does not have -- and which would answer the LOCAL zone through
 * mktime() if it were substituted.
 */
static sxi64 SslAsn1TimeToEpoch(const ASN1_TIME *pTime)
{
	struct tm sTm;
	if( pTime == 0 ){
		return 0;
	}
	SyZero(&sTm,sizeof(sTm));
	if( ASN1_TIME_to_tm(pTime,&sTm) != 1 ){
		return 0;
	}
	return DtDaysFromCivil((sxi64)sTm.tm_year + 1900,sTm.tm_mon + 1,sTm.tm_mday) * 86400
		+ (sxi64)sTm.tm_hour * 3600 + (sxi64)sTm.tm_min * 60 + sTm.tm_sec;
}

/* ------------------------------------------------------------------------
 * openssl_x509_parse()
 * ------------------------------------------------------------------------ */
static void SslPutStr(ph7_context *pCtx,ph7_value *pArray,ph7_value *pVal,
	const char *zKey,const char *zStr,int nStr)
{
	SXUNUSED(pCtx);
	ph7_value_string(pVal,zStr ? zStr : "",nStr);
	ph7_array_add_strkey_elem(pArray,zKey,pVal);
	ph7_value_reset_string_cursor(pVal);
}
static int vm_builtin_openssl_x509_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	X509 *pCert;
	ph7_value *pArray,*pVal,*pSub;
	BIO *pBio;
	int bOwn = 0,bShort = 1,i,nPurpose;
	char zBuf[512];
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		bShort = ph7_value_to_bool(apArg[1]);
	}
	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,0);
	if( pCert == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		SslFreeCert(pCert,bOwn);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* name: the one-line rendering, which is the SUBJECT's and always uses the
	 * short spelling whatever $short_names says. */
	X509_NAME_oneline(X509_get_subject_name(pCert),zBuf,(int)sizeof(zBuf));
	SslPutStr(pCtx,pArray,pVal,"name",zBuf,-1);
	pSub = ph7_context_new_array(pCtx);
	if( pSub ){
		SslNameToArray(pCtx,X509_get_subject_name(pCert),bShort,pSub);
		ph7_array_add_strkey_elem(pArray,"subject",pSub);
	}
	/* hash: OpenSSL's subject-name hash -- the same EIGHT hex digits the
	 * c_rehash symlinks in a CA directory carry. The value is narrowed to 32
	 * bits before it is formatted, which is not cosmetic: the engine's own
	 * formatter reads a `%lx` argument as 64 bits, so a 32-bit `unsigned long`
	 * (every Windows build) printed sixteen digits of half-garbage. */
	SyBufferFormat(zBuf,sizeof(zBuf),"%08x",(unsigned int)X509_subject_name_hash(pCert));
	SslPutStr(pCtx,pArray,pVal,"hash",zBuf,-1);
	pSub = ph7_context_new_array(pCtx);
	if( pSub ){
		SslNameToArray(pCtx,X509_get_issuer_name(pCert),bShort,pSub);
		ph7_array_add_strkey_elem(pArray,"issuer",pSub);
	}
	ph7_value_int64(pVal,(sxi64)X509_get_version(pCert));
	ph7_array_add_strkey_elem(pArray,"version",pVal);
	{
		/* The serial, twice: php gives the DECIMAL string and the HEX one, and
		 * the hex has no leading zero and no 0x. Both come out of the BIGNUM
		 * rather than the raw ASN.1 bytes, so a negative serial reads with a
		 * minus sign exactly as php's does. */
		ASN1_INTEGER *pSerial = X509_get_serialNumber(pCert);
		BIGNUM *pBn = pSerial ? ASN1_INTEGER_to_BN(pSerial,0) : 0;
		char *zDec = pBn ? BN_bn2dec(pBn) : 0;
		char *zHex = pBn ? BN_bn2hex(pBn) : 0;
		SslPutStr(pCtx,pArray,pVal,"serialNumber",zDec ? zDec : "",-1);
		SslPutStr(pCtx,pArray,pVal,"serialNumberHex",zHex ? zHex : "",-1);
		if( zDec ){ OPENSSL_free(zDec); }
		if( zHex ){ OPENSSL_free(zHex); }
		if( pBn ){ BN_free(pBn); }
	}
	{
		const ASN1_TIME *pFrom = X509_get0_notBefore(pCert);
		const ASN1_TIME *pTo = X509_get0_notAfter(pCert);
		SslPutStr(pCtx,pArray,pVal,"validFrom",
			(const char *)ASN1_STRING_get0_data((const ASN1_STRING *)pFrom),
			ASN1_STRING_length((const ASN1_STRING *)pFrom));
		SslPutStr(pCtx,pArray,pVal,"validTo",
			(const char *)ASN1_STRING_get0_data((const ASN1_STRING *)pTo),
			ASN1_STRING_length((const ASN1_STRING *)pTo));
		ph7_value_int64(pVal,SslAsn1TimeToEpoch(pFrom));
		ph7_array_add_strkey_elem(pArray,"validFrom_time_t",pVal);
		ph7_value_int64(pVal,SslAsn1TimeToEpoch(pTo));
		ph7_array_add_strkey_elem(pArray,"validTo_time_t",pVal);
	}
	{
		int nid = X509_get_signature_nid(pCert);
		SslPutStr(pCtx,pArray,pVal,"signatureTypeSN",OBJ_nid2sn(nid),-1);
		SslPutStr(pCtx,pArray,pVal,"signatureTypeLN",OBJ_nid2ln(nid),-1);
		ph7_value_int(pVal,nid);
		ph7_array_add_strkey_elem(pArray,"signatureTypeNID",pVal);
	}
	/* purposes: keyed by php's own X509_PURPOSE_* number, each a triple of
	 * "does it serve this purpose", "does it serve it as a CA" and the
	 * purpose's short name. */
	pSub = ph7_context_new_array(pCtx);
	nPurpose = X509_PURPOSE_get_count();
	if( pSub ){
		ph7_value *pTriple;
		for( i = 0 ; i < nPurpose ; ++i ){
			X509_PURPOSE *pPurpose = X509_PURPOSE_get0(i);
			int id = X509_PURPOSE_get_id(pPurpose);
			pTriple = ph7_context_new_array(pCtx);
			if( pTriple == 0 ){
				continue;
			}
			ph7_value_bool(pVal,X509_check_purpose(pCert,id,0) == 1);
			ph7_array_add_elem(pTriple,0,pVal);
			ph7_value_bool(pVal,X509_check_purpose(pCert,id,1) == 1);
			ph7_array_add_elem(pTriple,0,pVal);
			ph7_value_string(pVal,X509_PURPOSE_get0_sname(pPurpose),-1);
			ph7_array_add_elem(pTriple,0,pVal);
			ph7_value_reset_string_cursor(pVal);
			ph7_value_int(pVal,id);
			ph7_array_add_elem(pSub,pVal,pTriple);
		}
		ph7_array_add_strkey_elem(pArray,"purposes",pSub);
	}
	/* extensions: the extension's name against the TEXT rendering OpenSSL
	 * gives it -- not its DER. An extension that cannot be printed comes back
	 * as its raw bytes, which is php's fallback too. */
	pSub = ph7_context_new_array(pCtx);
	if( pSub ){
		int nExt = X509_get_ext_count(pCert);
		for( i = 0 ; i < nExt ; ++i ){
			X509_EXTENSION *pExt = X509_get_ext(pCert,i);
			ASN1_OBJECT *pObj = X509_EXTENSION_get_object(pExt);
			int nid = OBJ_obj2nid(pObj);
			const char *zKey;
			char zOid[128];
			char *zMem = 0;
			long nMem = 0;
			if( nid != NID_undef ){
				zKey = OBJ_nid2sn(nid);
			}else{
				OBJ_obj2txt(zOid,(int)sizeof(zOid),pObj,1);
				zKey = zOid;
			}
			pBio = BIO_new(BIO_s_mem());
			if( pBio == 0 ){
				continue;
			}
			if( X509V3_EXT_print(pBio,pExt,0,0) ){
				nMem = BIO_get_mem_data(pBio,&zMem);
			}else{
				ASN1_STRING *pData = X509_EXTENSION_get_data(pExt);
				BIO_write(pBio,ASN1_STRING_get0_data(pData),ASN1_STRING_length(pData));
				nMem = BIO_get_mem_data(pBio,&zMem);
			}
			SslPutStr(pCtx,pSub,pVal,zKey,zMem,(int)nMem);
			BIO_free(pBio);
		}
		ph7_array_add_strkey_elem(pArray,"extensions",pSub);
	}
	SslFreeCert(pCert,bOwn);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* ------------------------------------------------------------------------
 * The rest of the certificate surface
 * ------------------------------------------------------------------------ */
static int vm_builtin_openssl_x509_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	X509 *pCert;
	int bOwn = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,1);
	if( pCert == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !bOwn ){
		/* Already an OpenSSLCertificate: php hands back the same object. */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_CERT,(void *)pCert);
}
static int SslX509Export(ph7_context *pCtx,int nArg,ph7_value **apArg,int bToFile)
{
	X509 *pCert;
	BIO *pBio;
	char *zMem = 0;
	long nMem;
	int bOwn = 0,bNoText = 1,rc = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		bNoText = ph7_value_to_bool(apArg[2]);
	}
	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,1);
	if( pCert == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pBio = BIO_new(BIO_s_mem());
	if( pBio == 0 ){
		SslFreeCert(pCert,bOwn);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* `$no_text = false` asks for the human-readable dump BEFORE the PEM, which
	 * is what `openssl x509 -text` prints. */
	if( !bNoText ){
		X509_print(pBio,pCert);
	}
	if( PEM_write_bio_X509(pBio,pCert) != 1 ){
		rc = -1;
	}
	if( rc == 0 ){
		nMem = BIO_get_mem_data(pBio,&zMem);
		if( bToFile ){
			int nPath = 0;
			const char *zPath = ph7_value_to_string(apArg[1],&nPath);
			rc = PH7_SslWriteFileArg(pCtx,zPath,nPath,zMem,(sxu32)nMem);
			if( rc != 0 ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Error opening file %.*s",nPath,zPath ? zPath : "");
			}
		}else{
			ph7_value *pRes = ph7_context_new_scalar(pCtx);
			if( pRes ){
				ph7_value_string(pRes,zMem,(int)nMem);
				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);
				ph7_context_release_value(pCtx,pRes);
			}
		}
	}
	BIO_free(pBio);
	SslFreeCert(pCert,bOwn);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_x509_export(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslX509Export(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_x509_export_to_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslX509Export(pCtx,nArg,apArg,1);
}
static int vm_builtin_openssl_x509_fingerprint(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	X509 *pCert;
	const EVP_MD *pMd;
	EVP_MD *pFetched = 0;
	unsigned char aOut[EVP_MAX_MD_SIZE];
	unsigned int nOut = 0;
	int bOwn = 0,bRaw = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		bRaw = ph7_value_to_bool(apArg[2]);
	}
	/* The certificate is read FIRST: `openssl_x509_fingerprint('junk',
	 * 'nosuchdigest')` is `X.509 Certificate cannot be retrieved` under php,
	 * not `Unknown digest algorithm`. */
	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,1);
	if( pCert == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		pMd = PH7_SslDigestOfValue(apArg[1],&pFetched);
	}else{
		pMd = EVP_sha1();
	}
	if( pMd == 0 ){
		SslFreeCert(pCert,bOwn);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( X509_digest(pCert,pMd,aOut,&nOut) != 1 ){
		SslFreeCert(pCert,bOwn);
		if( pFetched ){ EVP_MD_free(pFetched); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SslFreeCert(pCert,bOwn);
	if( pFetched ){ EVP_MD_free(pFetched); }
	if( bRaw ){
		ph7_result_string(pCtx,(const char *)aOut,(int)nOut);
	}else{
		static const char zDigit[] = "0123456789abcdef";
		char zHex[EVP_MAX_MD_SIZE * 2];
		unsigned int i;
		for( i = 0 ; i < nOut ; ++i ){
			zHex[i * 2]     = zDigit[(aOut[i] >> 4) & 0x0F];
			zHex[i * 2 + 1] = zDigit[aOut[i] & 0x0F];
		}
		ph7_result_string(pCtx,zHex,(int)(nOut * 2));
	}
	return PH7_OK;
}
static int vm_builtin_openssl_x509_check_private_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	X509 *pCert;
	EVP_PKEY *pKey;
	int bOwnCert = 0,bOwnKey = 0,iRc;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCert = SslCertOfValue(pCtx,apArg[0],&bOwnCert,0);
	if( pCert == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[1],0,&bOwnKey);
	if( pKey == 0 ){
		SslFreeCert(pCert,bOwnCert);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iRc = X509_check_private_key(pCert,pKey);
	SslFreeCert(pCert,bOwnCert);
	if( bOwnKey > 0 ){
		EVP_PKEY_free(pKey);
	}
	if( iRc != 1 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,iRc == 1);
	return PH7_OK;
}
static int vm_builtin_openssl_x509_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	X509 *pCert;
	EVP_PKEY *pKey;
	int bOwnCert = 0,bOwnKey = 0,iRc;
	if( nArg < 2 ){
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	pCert = SslCertOfValue(pCtx,apArg[0],&bOwnCert,0);
	if( pCert == 0 ){
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[1],1,&bOwnKey);
	if( pKey == 0 ){
		SslFreeCert(pCert,bOwnCert);
		/* php's three answers here are 1, 0 and -1, and a key it cannot read
		 * is the SAME -1 a failed verification gives. */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	iRc = X509_verify(pCert,pKey);
	SslFreeCert(pCert,bOwnCert);
	if( bOwnKey > 0 ){
		EVP_PKEY_free(pKey);
	}
	if( iRc != 1 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_int(pCtx,iRc == 1 ? 1 : (iRc == 0 ? 0 : -1));
	return PH7_OK;
}
/*
 * openssl_x509_checkpurpose(): a chain verification with a purpose attached.
 * `$ca_info` is a list of files and DIRECTORIES, which is the same pair
 * X509_STORE takes, and an empty list means "trust nothing", which is why the
 * self-signed certificate a program just made answers false.
 */
static X509_STORE * SslStoreFromCaInfo(ph7_context *pCtx,ph7_value *pList)
{
	X509_STORE *pStore = X509_STORE_new();
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 i;
	if( pStore == 0 ){
		return 0;
	}
	if( pList == 0 || !ph7_value_is_array(pList) ){
		return pStore;
	}
	pMap = (ph7_hashmap *)pList->x.pOther;
	pEntry = pMap->pFirst;
	for( i = 0 ; i < pMap->nEntry ; ++i ){
		ph7_value *pData = HashmapExtractNodeValue(pEntry);
		pEntry = pEntry->pPrev;
		if( pData == 0 || !ph7_value_is_string(pData) ){
			continue;
		}
		{
			int nPath = 0;
			const char *zPath = ph7_value_to_string(pData,&nPath);
			char *zZ;
			if( zPath == 0 || nPath < 1 ){
				continue;
			}
			zZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPath + 1));
			if( zZ == 0 ){
				continue;
			}
			SyMemcpy(zPath,zZ,(sxu32)nPath);
			zZ[nPath] = 0;
			/* php tries the entry as a FILE and then as a DIRECTORY, and says
			 * nothing when neither works. */
			if( X509_STORE_load_locations(pStore,zZ,0) != 1 ){
				ERR_clear_error();
				if( X509_STORE_load_locations(pStore,0,zZ) != 1 ){
					ERR_clear_error();
				}
			}
			SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);
		}
	}
	return pStore;
}
static int vm_builtin_openssl_x509_checkpurpose(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	X509 *pCert;
	X509_STORE *pStore;
	X509_STORE_CTX *pStoreCtx;
	STACK_OF(X509) *pUntrusted = 0;
	int bOwn = 0,iPurpose,iRc = -1;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iPurpose = (int)ph7_value_to_int64(apArg[1]);
	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,0);
	if( pCert == 0 ){
		/* Not false: a certificate this door cannot READ is the same -1 a
		 * chain it cannot BUILD gives. */
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
		SyBlob sFile;
		const char *zData = 0;
		int nData = 0;
		SyBlobInit(&sFile,&pCtx->pVm->sAllocator);
		if( PH7_SslBytesOfValue(pCtx,apArg[3],&sFile,&zData,&nData) == 0 && nData > 0 ){
			BIO *pBio = BIO_new_mem_buf(zData,nData);
			if( pBio ){
				X509 *pOne;
				pUntrusted = sk_X509_new_null();
				while( pUntrusted && (pOne = PEM_read_bio_X509(pBio,0,0,0)) != 0 ){
					sk_X509_push(pUntrusted,pOne);
				}
				ERR_clear_error();
				BIO_free(pBio);
			}
		}
		SyBlobRelease(&sFile);
	}
	pStore = SslStoreFromCaInfo(pCtx,nArg > 2 ? apArg[2] : 0);
	pStoreCtx = pStore ? X509_STORE_CTX_new() : 0;
	if( pStoreCtx && X509_STORE_CTX_init(pStoreCtx,pStore,pCert,pUntrusted) == 1 ){
		X509_STORE_CTX_set_purpose(pStoreCtx,iPurpose);
		iRc = X509_verify_cert(pStoreCtx);
	}
	if( pStoreCtx ){ X509_STORE_CTX_free(pStoreCtx); }
	if( pStore ){ X509_STORE_free(pStore); }
	if( pUntrusted ){ sk_X509_pop_free(pUntrusted,X509_free); }
	SslFreeCert(pCert,bOwn);
	if( iRc < 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	if( iRc != 1 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,iRc == 1);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * The configuration php reads
 * ------------------------------------------------------------------------ */
/*
 * Which file, and the section names inside it. php looks at the `config`
 * option first, then OPENSSL_CONF / SSLEAY_CONF, then the library's own
 * default -- and a file it cannot load is not an error: the request is simply
 * built without defaults and without extensions.
 */
typedef struct SslReqConf SslReqConf;
struct SslReqConf {
	CONF *pConf;
	const char *zDnSection;    /* `req`'s distinguished_name */
	const char *zReqExts;      /* `req`'s req_extensions, or the option's */
	const char *zX509Exts;     /* `req`'s x509_extensions, or the option's */
	const EVP_MD *pDigest;
	EVP_MD *pFetched;
	sxi64 iBits;
	sxi64 iType;
	const char *zCurve;
	int nCurve;
	int bEncryptKey;
};
static const char * SslConfValue(SslReqConf *pReq,const char *zSection,const char *zName)
{
	return pReq->pConf ? NCONF_get_string(pReq->pConf,zSection,zName) : 0;
}
static void SslReqConfLoad(ph7_context *pCtx,ph7_value *pOpts,SslReqConf *pReq)
{
	ph7_value *pVal;
	char *zFile = 0;
	const char *zPath = 0;
	int nPath = 0;
	SyZero(pReq,sizeof(*pReq));
	pReq->zDnSection = "req_distinguished_name";
	pReq->iBits = 2048;
	pReq->iType = 0;
	pReq->bEncryptKey = -1;
	if( pOpts && ph7_value_is_array(pOpts) ){
		pVal = ph7_array_fetch(pOpts,"config",-1);
		if( pVal && ph7_value_is_string(pVal) ){
			zPath = ph7_value_to_string(pVal,&nPath);
		}
	}
	if( zPath && nPath > 0 ){
		zFile = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPath + 1));
		if( zFile ){
			SyMemcpy(zPath,zFile,(sxu32)nPath);
			zFile[nPath] = 0;
		}
	}else{
		zFile = CONF_get1_default_config_file();
	}
	if( zFile ){
		pReq->pConf = NCONF_new(0);
		if( pReq->pConf && NCONF_load(pReq->pConf,zFile,0) <= 0 ){
			NCONF_free(pReq->pConf);
			pReq->pConf = 0;
			ERR_clear_error();
		}
	}
	if( zPath && nPath > 0 ){
		if( zFile ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zFile); }
	}else if( zFile ){
		OPENSSL_free(zFile);
	}
	if( pReq->pConf ){
		const char *z = SslConfValue(pReq,"req","distinguished_name");
		if( z ){
			pReq->zDnSection = z;
		}
		pReq->zReqExts = SslConfValue(pReq,"req","req_extensions");
		pReq->zX509Exts = SslConfValue(pReq,"req","x509_extensions");
		z = SslConfValue(pReq,"req","default_bits");
		if( z ){
			pReq->iBits = (sxi64)SyStrToInt32(z,(sxu32)SyStrlen(z),0,0);
			if( pReq->iBits < 1 ){
				pReq->iBits = 2048;
			}
		}
		ERR_clear_error();
	}
	if( pOpts == 0 || !ph7_value_is_array(pOpts) ){
		return;
	}
	pVal = ph7_array_fetch(pOpts,"digest_alg",-1);
	if( pVal ){
		pReq->pDigest = PH7_SslDigestOfValue(pVal,&pReq->pFetched);
	}
	pVal = ph7_array_fetch(pOpts,"private_key_bits",-1);
	if( pVal ){
		pReq->iBits = ph7_value_to_int64(pVal);
	}
	pVal = ph7_array_fetch(pOpts,"private_key_type",-1);
	if( pVal ){
		pReq->iType = ph7_value_to_int64(pVal);
	}
	pVal = ph7_array_fetch(pOpts,"curve_name",-1);
	if( pVal ){
		pReq->zCurve = ph7_value_to_string(pVal,&pReq->nCurve);
	}
	pVal = ph7_array_fetch(pOpts,"encrypt_key",-1);
	if( pVal ){
		pReq->bEncryptKey = ph7_value_to_bool(pVal);
	}
	pVal = ph7_array_fetch(pOpts,"req_extensions",-1);
	if( pVal && ph7_value_is_string(pVal) ){
		int n = 0;
		pReq->zReqExts = ph7_value_to_string(pVal,&n);
	}
	pVal = ph7_array_fetch(pOpts,"x509_extensions",-1);
	if( pVal && ph7_value_is_string(pVal) ){
		int n = 0;
		pReq->zX509Exts = ph7_value_to_string(pVal,&n);
	}
}
static void SslReqConfRelease(SslReqConf *pReq)
{
	if( pReq->pFetched ){
		EVP_MD_free(pReq->pFetched);
		pReq->pFetched = 0;
	}
	if( pReq->pConf ){
		NCONF_free(pReq->pConf);
		pReq->pConf = 0;
	}
}
/*
 * The SUBJECT php builds: the caller's fields first, in their own order, then
 * every `<name>_default` the config declares for a field the caller did NOT
 * give. A name the object table has never heard of is `dn: %s is not a
 * recognized name` and is skipped rather than fatal.
 */
static int SslBuildSubject(ph7_context *pCtx,X509_NAME *pName,ph7_value *pDn,SslReqConf *pReq)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 i;
	if( pDn == 0 || !ph7_value_is_array(pDn) ){
		return 0;
	}
	pMap = (ph7_hashmap *)pDn->x.pOther;
	pEntry = pMap->pFirst;
	for( i = 0 ; i < pMap->nEntry ; ++i ){
		ph7_value sKey;
		ph7_value *pData = HashmapExtractNodeValue(pEntry);
		const char *zKey,*zVal;
		int nKey = 0,nVal = 0,nid;
		char zName[128];
		PH7_MemObjInit(pCtx->pVm,&sKey);
		PH7_HashmapExtractNodeKey(pEntry,&sKey);
		pEntry = pEntry->pPrev;
		zKey = ph7_value_to_string(&sKey,&nKey);
		if( zKey == 0 || nKey < 1 || nKey >= (int)sizeof(zName) || pData == 0 ){
			PH7_MemObjRelease(&sKey);
			continue;
		}
		SyMemcpy(zKey,zName,(sxu32)nKey);
		zName[nKey] = 0;
		PH7_MemObjRelease(&sKey);
		nid = OBJ_txt2nid(zName);
		if( nid == NID_undef ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"dn: %s is not a recognized name",zName);
			continue;
		}
		zVal = ph7_value_to_string(pData,&nVal);
		if( zVal == 0 ){
			zVal = "";
			nVal = 0;
		}
		if( X509_NAME_add_entry_by_NID(pName,nid,MBSTRING_UTF8,
				(const unsigned char *)zVal,nVal,-1,0) != 1 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"dn: add_entry_by_NID %d -> %s (failed; check error queue and value of string_mask OpenSSL option if illegal characters are reported)",
				nid,zVal);
			return -1;
		}
	}
	/* Then the config's own defaults, for every field the caller left out. */
	if( pReq->pConf ){
		STACK_OF(CONF_VALUE) *pSec;
		/* A missing section or key pushes an error of its own. Mark and pop
		 * around the whole walk rather than clearing the queue: the caller's
		 * `dn: %s is not a recognized name` left an `unknown object name` in
		 * it a moment ago, and php hands that one to openssl_error_string(). */
		ERR_set_mark();
		pSec = NCONF_get_section(pReq->pConf,pReq->zDnSection);
		int n = pSec ? sk_CONF_VALUE_num(pSec) : 0;
		int j;
		for( j = 0 ; j < n ; ++j ){
			CONF_VALUE *pV = sk_CONF_VALUE_value(pSec,j);
			const char *zField = pV->name;
			const char *zDot;
			char zBuf[192];
			const char *zDefault;
			int nid;
			/* The config numbers repeatable fields ("0.organizationName"); the
			 * NAME is what follows the dot. */
			{
				sxu32 nPos = 0;
				zDot = SyByteFind(zField,(sxu32)SyStrlen(zField),'.',&nPos) == SXRET_OK
					? zField + nPos : 0;
			}
			if( zDot ){
				zField = zDot + 1;
			}
			{
				sxu32 nField = (sxu32)SyStrlen(pV->name);
				if( nField + 9 >= sizeof(zBuf) ){
					continue;
				}
				if( nField > 8 && SyStrncmp(pV->name + nField - 8,"_default",8) == 0 ){
					continue;   /* the default row itself, not a field */
				}
			}
			nid = OBJ_txt2nid(zField);
			if( nid == NID_undef ){
				continue;
			}
			if( X509_NAME_get_index_by_NID(pName,nid,-1) >= 0 ){
				continue;   /* the caller gave it */
			}
			SyBufferFormat(zBuf,sizeof(zBuf),"%s_default",pV->name);
			zDefault = NCONF_get_string(pReq->pConf,pReq->zDnSection,zBuf);
			if( zDefault == 0 || zDefault[0] == 0 ){
				continue;
			}
			X509_NAME_add_entry_by_NID(pName,nid,MBSTRING_UTF8,
				(const unsigned char *)zDefault,-1,-1,0);
		}
		ERR_pop_to_mark();
	}
	return 0;
}

/* ------------------------------------------------------------------------
 * Signing requests
 * ------------------------------------------------------------------------ */
static int vm_builtin_openssl_csr_new(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SslReqConf sReq;
	X509_REQ *pCsr = 0;
	X509_NAME *pName;
	EVP_PKEY *pKey = 0;
	int bOwnKey = 0,rc = -1;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SslReqConfLoad(pCtx,nArg > 2 ? apArg[2] : 0,&sReq);
	if( ph7_value_is_null(apArg[1]) ){
		/* php GENERATES the key when the by-reference argument is null, and
		 * writes it back -- which is the only way `openssl_csr_new($dn, $k)`
		 * on an unset $k can work at all. */
		pKey = PH7_SslGenerateKey(pCtx,sReq.iBits,sReq.iType,sReq.zCurve,sReq.nCurve);
		if( pKey == 0 ){
			goto done;
		}
		/* The object OWNS the key from here: the by-ref write-back copies the
		 * value and takes a reference of its own, so the caller's $key stays
		 * alive after this call returns and nothing frees the EVP_PKEY twice. */
		PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);
		if( !ph7_value_is_object(pCtx->pRet) ){
			pKey = 0;
			goto done;
		}
		pKey = (EVP_PKEY *)PH7_SslHandleOf(pCtx->pRet,PHL_SSL_KIND_KEY);
		if( pKey == 0 ){
			goto done;
		}
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pCtx->pRet);
	}else{
		pKey = PH7_SslKeyOfValue(pCtx,apArg[1],0,&bOwnKey);
		if( bOwnKey < 0 ){
			SslReqConfRelease(&sReq);
			ph7_result_bool(pCtx,0);
			return PH7_SslArrayShapeError(pCtx);
		}
		if( pKey == 0 ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Unable to coerce parameter 2 into a private key");
			goto done;
		}
	}
	pCsr = X509_REQ_new();
	if( pCsr == 0 ){
		goto done;
	}
	X509_REQ_set_version(pCsr,0);
	pName = X509_REQ_get_subject_name(pCsr);
	if( SslBuildSubject(pCtx,pName,apArg[0],&sReq) != 0 ){
		goto done;
	}
	if( X509_REQ_set_pubkey(pCsr,pKey) != 1 ){
		goto done;
	}
	if( sReq.zReqExts && sReq.pConf ){
		X509V3_CTX sExtCtx;
		X509V3_set_ctx(&sExtCtx,0,0,pCsr,0,0);
		X509V3_set_nconf(&sExtCtx,sReq.pConf);
		if( !X509V3_EXT_REQ_add_nconf(sReq.pConf,&sExtCtx,(char *)sReq.zReqExts,pCsr) ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Error loading extension section %s",sReq.zReqExts);
			goto done;
		}
	}
	if( X509_REQ_sign(pCsr,pKey,sReq.pDigest ? sReq.pDigest : EVP_sha256()) <= 0 ){
		goto done;
	}
	rc = 0;
done:
	if( bOwnKey > 0 && pKey ){
		EVP_PKEY_free(pKey);
	}
	SslReqConfRelease(&sReq);
	if( rc != 0 ){
		if( pCsr ){ X509_REQ_free(pCsr); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_CSR,(void *)pCsr);
}
static int SslCsrExport(ph7_context *pCtx,int nArg,ph7_value **apArg,int bToFile)
{
	X509_REQ *pCsr;
	BIO *pBio;
	char *zMem = 0;
	long nMem;
	int bOwn = 0,bNoText = 1,rc = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		bNoText = ph7_value_to_bool(apArg[2]);
	}
	pCsr = SslCsrOfValue(pCtx,apArg[0],&bOwn,1);
	if( pCsr == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pBio = BIO_new(BIO_s_mem());
	if( pBio == 0 ){
		SslFreeCsr(pCsr,bOwn);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !bNoText ){
		X509_REQ_print(pBio,pCsr);
	}
	if( PEM_write_bio_X509_REQ(pBio,pCsr) != 1 ){
		rc = -1;
	}
	if( rc == 0 ){
		nMem = BIO_get_mem_data(pBio,&zMem);
		if( bToFile ){
			int nPath = 0;
			const char *zPath = ph7_value_to_string(apArg[1],&nPath);
			rc = PH7_SslWriteFileArg(pCtx,zPath,nPath,zMem,(sxu32)nMem);
			if( rc != 0 ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Error opening file %.*s",nPath,zPath ? zPath : "");
			}
		}else{
			ph7_value *pRes = ph7_context_new_scalar(pCtx);
			if( pRes ){
				ph7_value_string(pRes,zMem,(int)nMem);
				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);
				ph7_context_release_value(pCtx,pRes);
			}
		}
	}
	BIO_free(pBio);
	SslFreeCsr(pCsr,bOwn);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_csr_export(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslCsrExport(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_csr_export_to_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslCsrExport(pCtx,nArg,apArg,1);
}
static int vm_builtin_openssl_csr_get_subject(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	X509_REQ *pCsr;
	ph7_value *pArray;
	int bOwn = 0,bShort = 1;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		bShort = ph7_value_to_bool(apArg[1]);
	}
	pCsr = SslCsrOfValue(pCtx,apArg[0],&bOwn,0);
	if( pCsr == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	if( pArray ){
		SslNameToArray(pCtx,X509_REQ_get_subject_name(pCsr),bShort,pArray);
		ph7_result_value(pCtx,pArray);
	}else{
		ph7_result_bool(pCtx,0);
	}
	SslFreeCsr(pCsr,bOwn);
	return PH7_OK;
}
static int vm_builtin_openssl_csr_get_public_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	X509_REQ *pCsr;
	EVP_PKEY *pKey;
	int bOwn = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCsr = SslCsrOfValue(pCtx,apArg[0],&bOwn,0);
	if( pCsr == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = X509_REQ_get_pubkey(pCsr);
	SslFreeCsr(pCsr,bOwn);
	if( pKey == 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);
}
/*
 * openssl_csr_sign(): the request, a CA certificate (or null for a
 * self-signed one), the CA's private key and a validity in DAYS. The serial
 * may be given as an integer or -- since php 8.3 -- as a HEX string, which is
 * the only way to express one that does not fit in an int.
 */
static int vm_builtin_openssl_csr_sign(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SslReqConf sReq;
	X509_REQ *pCsr = 0;
	X509 *pCa = 0,*pOut = 0;
	EVP_PKEY *pKey = 0,*pPub = 0;
	ASN1_INTEGER *pSerial = 0;
	int bOwnCsr = 0,bOwnCa = 0,bOwnKey = 0,rc = -1;
	sxi64 iDays;
	if( nArg < 4 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SslReqConfLoad(pCtx,nArg > 4 ? apArg[4] : 0,&sReq);
	iDays = ph7_value_to_int64(apArg[3]);
	pCsr = SslCsrOfValue(pCtx,apArg[0],&bOwnCsr,1);
	if( pCsr == 0 ){
		goto done;
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		pCa = SslCertOfValue(pCtx,apArg[1],&bOwnCa,1);
		if( pCa == 0 ){
			goto done;
		}
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],0,&bOwnKey);
	if( bOwnKey < 0 ){
		SslReqConfRelease(&sReq);
		SslFreeCsr(pCsr,bOwnCsr);
		SslFreeCert(pCa,bOwnCa);
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pKey == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"cannot get private key from parameter 3");
		goto done;
	}
	if( pCa && X509_check_private_key(pCa,pKey) != 1 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"private key does not correspond to signing cert");
		goto done;
	}
	pPub = X509_REQ_get_pubkey(pCsr);
	if( pPub == 0 || X509_REQ_verify(pCsr,pPub) != 1 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Signature verification problem");
		goto done;
	}
	pOut = X509_new();
	if( pOut == 0 ){
		goto done;
	}
	pSerial = ASN1_INTEGER_new();
	if( pSerial == 0 ){
		goto done;
	}
	if( nArg > 6 && !ph7_value_is_null(apArg[6]) ){
		int nHex = 0;
		const char *zHex = ph7_value_to_string(apArg[6],&nHex);
		BIGNUM *pBn = 0;
		char *zZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nHex + 1));
		if( zZ == 0 ){
			goto done;
		}
		SyMemcpy(zHex,zZ,(sxu32)nHex);
		zZ[nHex] = 0;
		if( BN_hex2bn(&pBn,zZ) == 0 || pBn == 0 ){
			SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);
			if( pBn ){ BN_free(pBn); }
			ph7_result_bool(pCtx,0);
			SslReqConfRelease(&sReq);
			SslFreeCsr(pCsr,bOwnCsr);
			SslFreeCert(pCa,bOwnCa);
			if( bOwnKey > 0 ){ EVP_PKEY_free(pKey); }
			if( pPub ){ EVP_PKEY_free(pPub); }
			if( pOut ){ X509_free(pOut); }
			ASN1_INTEGER_free(pSerial);
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #7 ($serial_hex) must be a valid hexadecimal string",
				ph7_function_name(pCtx));
		}
		SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);
		BN_to_ASN1_INTEGER(pBn,pSerial);
		BN_free(pBn);
	}else{
		ASN1_INTEGER_set_int64(pSerial,nArg > 5 ? ph7_value_to_int64(apArg[5]) : 0);
	}
	X509_set_version(pOut,2);
	X509_set_serialNumber(pOut,pSerial);
	X509_set_subject_name(pOut,X509_REQ_get_subject_name(pCsr));
	X509_set_issuer_name(pOut,pCa ? X509_get_subject_name(pCa) : X509_REQ_get_subject_name(pCsr));
	X509_gmtime_adj(X509_getm_notBefore(pOut),0);
	X509_gmtime_adj(X509_getm_notAfter(pOut),(long)(iDays * 24 * 60 * 60));
	X509_set_pubkey(pOut,pPub);
	if( sReq.zX509Exts && sReq.pConf ){
		X509V3_CTX sExtCtx;
		X509V3_set_ctx(&sExtCtx,pCa ? pCa : pOut,pOut,pCsr,0,0);
		X509V3_set_nconf(&sExtCtx,sReq.pConf);
		if( !X509V3_EXT_add_nconf(sReq.pConf,&sExtCtx,(char *)sReq.zX509Exts,pOut) ){
			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
				"Error loading extension section %s",sReq.zX509Exts);
			goto done;
		}
	}
	if( X509_sign(pOut,pKey,sReq.pDigest ? sReq.pDigest : EVP_sha256()) <= 0 ){
		goto done;
	}
	rc = 0;
done:
	if( pSerial ){ ASN1_INTEGER_free(pSerial); }
	if( pPub ){ EVP_PKEY_free(pPub); }
	if( bOwnKey > 0 && pKey ){ EVP_PKEY_free(pKey); }
	SslFreeCert(pCa,bOwnCa);
	SslFreeCsr(pCsr,bOwnCsr);
	SslReqConfRelease(&sReq);
	if( rc != 0 ){
		if( pOut ){ X509_free(pOut); }
		PH7_SslStoreErrors(pCtx->pVm);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_CERT,(void *)pOut);
}

/* ------------------------------------------------------------------------
 * PKCS#12
 * ------------------------------------------------------------------------ */
/*
 * A bundle is the certificate, its key and any chain that came with it. php's
 * read gives back `cert` and `pkey` as PEM and `extracerts` as a LIST of PEM
 * -- and the last key is ABSENT rather than empty when the bundle carries no
 * chain, which is why a caller has to `??` it.
 */
static int SslPkcs12Export(ph7_context *pCtx,int nArg,ph7_value **apArg,int bToFile)
{
	X509 *pCert;
	EVP_PKEY *pKey;
	PKCS12 *pP12 = 0;
	BIO *pBio = 0;
	STACK_OF(X509) *pChain = 0;
	const char *zPass,*zFriendly = 0;
	char *zPassZ = 0,*zFriendlyZ = 0;
	int nPass = 0,nFriendly = 0,bOwnCert = 0,bOwnKey = 0,rc = -1;
	ph7_value *pOpts = 0,*pVal;
	if( nArg < 4 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPass = ph7_value_to_string(apArg[3],&nPass);
	pCert = SslCertOfValue(pCtx,apArg[0],&bOwnCert,1);
	if( pCert == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],0,&bOwnKey);
	if( bOwnKey < 0 ){
		SslFreeCert(pCert,bOwnCert);
		ph7_result_bool(pCtx,0);
		return PH7_SslArrayShapeError(pCtx);
	}
	if( pKey == 0 ){
		SslFreeCert(pCert,bOwnCert);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"cannot get private key from parameter 3");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( X509_check_private_key(pCert,pKey) != 1 ){
		ERR_clear_error();
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"private key does not correspond to cert");
		goto done;
	}
	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){
		pOpts = apArg[4];
	}
	if( pOpts ){
		pVal = ph7_array_fetch(pOpts,"friendly_name",-1);
		if( pVal && ph7_value_is_string(pVal) ){
			zFriendly = ph7_value_to_string(pVal,&nFriendly);
			if( zFriendly && nFriendly > 0 ){
				/* Copied HERE rather than after the extracerts walk: that walk
				 * can reach a userland stream wrapper, and running php code
				 * moves the memobj pool `pVal` lives in. */
				zFriendlyZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
					(sxu32)(nFriendly + 1));
				if( zFriendlyZ ){
					SyMemcpy(zFriendly,zFriendlyZ,(sxu32)nFriendly);
					zFriendlyZ[nFriendly] = 0;
				}
			}
		}
		pVal = ph7_array_fetch(pOpts,"extracerts",-1);
		if( pVal && ph7_value_is_array(pVal) ){
			ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;
			ph7_hashmap_node *pEntry = pMap->pFirst;
			sxu32 i;
			pChain = sk_X509_new_null();
			for( i = 0 ; pChain && i < pMap->nEntry ; ++i ){
				ph7_value *pData = HashmapExtractNodeValue(pEntry);
				int bOwnOne = 0;
				X509 *pOne = pData ? SslCertOfValue(pCtx,pData,&bOwnOne,1) : 0;
				pEntry = pEntry->pPrev;
				if( pOne == 0 ){
					continue;
				}
				if( !bOwnOne ){
					X509_up_ref(pOne);
				}
				sk_X509_push(pChain,pOne);
			}
		}
	}
	zPassZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPass + 1));
	if( zPassZ == 0 ){
		goto done;
	}
	if( nPass > 0 ){ SyMemcpy(zPass,zPassZ,(sxu32)nPass); }
	zPassZ[nPass] = 0;
	pP12 = PKCS12_create(zPassZ,zFriendlyZ,pKey,pCert,pChain,0,0,0,0,0);
	if( pP12 == 0 ){
		goto done;
	}
	pBio = BIO_new(BIO_s_mem());
	if( pBio == 0 || i2d_PKCS12_bio(pBio,pP12) != 1 ){
		goto done;
	}
	{
		char *zMem = 0;
		long nMem = BIO_get_mem_data(pBio,&zMem);
		if( bToFile ){
			int nPath = 0;
			const char *zPath = ph7_value_to_string(apArg[1],&nPath);
			rc = PH7_SslWriteFileArg(pCtx,zPath,nPath,zMem,(sxu32)nMem);
		}else{
			ph7_value *pRes = ph7_context_new_scalar(pCtx);
			if( pRes ){
				ph7_value_string(pRes,zMem,(int)nMem);
				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);
				ph7_context_release_value(pCtx,pRes);
			}
			rc = 0;
		}
	}
done:
	if( zPassZ ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zPassZ); }
	if( zFriendlyZ ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zFriendlyZ); }
	if( pBio ){ BIO_free(pBio); }
	if( pP12 ){ PKCS12_free(pP12); }
	if( pChain ){ sk_X509_pop_free(pChain,X509_free); }
	if( bOwnKey > 0 ){ EVP_PKEY_free(pKey); }
	SslFreeCert(pCert,bOwnCert);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_pkcs12_export(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslPkcs12Export(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_pkcs12_export_to_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslPkcs12Export(pCtx,nArg,apArg,1);
}
static void SslPemPut(ph7_context *pCtx,ph7_value *pArray,ph7_value *pVal,const char *zKey,BIO *pBio)
{
	char *zMem = 0;
	long nMem = BIO_get_mem_data(pBio,&zMem);
	SXUNUSED(pCtx);
	ph7_value_string(pVal,zMem,(int)nMem);
	if( zKey ){
		ph7_array_add_strkey_elem(pArray,zKey,pVal);
	}else{
		ph7_array_add_elem(pArray,0,pVal);
	}
	ph7_value_reset_string_cursor(pVal);
}
static int vm_builtin_openssl_pkcs12_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyBlob sFile;
	PKCS12 *pP12 = 0;
	BIO *pIn = 0,*pOut = 0;
	EVP_PKEY *pKey = 0;
	X509 *pCert = 0;
	STACK_OF(X509) *pChain = 0;
	const char *zData = 0,*zPass;
	char *zPassZ = 0;
	int nData = 0,nPass = 0,rc = -1;
	ph7_value *pArray,*pVal;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPass = ph7_value_to_string(apArg[2],&nPass);
	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);
	if( PH7_SslBytesOfValue(pCtx,apArg[0],&sFile,&zData,&nData) != 0 || nData < 1 ){
		SyBlobRelease(&sFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pIn = BIO_new_mem_buf(zData,nData);
	pP12 = pIn ? d2i_PKCS12_bio(pIn,0) : 0;
	if( pP12 == 0 ){
		goto done;
	}
	zPassZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPass + 1));
	if( zPassZ == 0 ){
		goto done;
	}
	if( nPass > 0 ){ SyMemcpy(zPass,zPassZ,(sxu32)nPass); }
	zPassZ[nPass] = 0;
	if( PKCS12_parse(pP12,zPassZ,&pKey,&pCert,&pChain) != 1 ){
		goto done;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		goto done;
	}
	pOut = BIO_new(BIO_s_mem());
	if( pOut == 0 ){
		goto done;
	}
	if( pCert && PEM_write_bio_X509(pOut,pCert) == 1 ){
		SslPemPut(pCtx,pArray,pVal,"cert",pOut);
	}
	BIO_free(pOut);
	pOut = BIO_new(BIO_s_mem());
	if( pOut && pKey && PEM_write_bio_PrivateKey(pOut,pKey,0,0,0,0,0) == 1 ){
		SslPemPut(pCtx,pArray,pVal,"pkey",pOut);
	}
	if( pOut ){ BIO_free(pOut); }
	pOut = 0;
	if( pChain && sk_X509_num(pChain) > 0 ){
		ph7_value *pList = ph7_context_new_array(pCtx);
		int i;
		for( i = 0 ; pList && i < sk_X509_num(pChain) ; ++i ){
			BIO *pOne = BIO_new(BIO_s_mem());
			if( pOne == 0 ){
				continue;
			}
			if( PEM_write_bio_X509(pOne,sk_X509_value(pChain,i)) == 1 ){
				SslPemPut(pCtx,pList,pVal,0,pOne);
			}
			BIO_free(pOne);
		}
		if( pList ){
			ph7_array_add_strkey_elem(pArray,"extracerts",pList);
		}
	}
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);
	rc = 0;
done:
	if( pOut ){ BIO_free(pOut); }
	if( pIn ){ BIO_free(pIn); }
	if( pP12 ){ PKCS12_free(pP12); }
	if( pKey ){ EVP_PKEY_free(pKey); }
	if( pCert ){ X509_free(pCert); }
	if( pChain ){ sk_X509_pop_free(pChain,X509_free); }
	if( zPassZ ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zPassZ); }
	SyBlobRelease(&sFile);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * PKCS#7 and CMS
 * ------------------------------------------------------------------------ */
/*
 * The two message families are the SAME shape twice over, which is why they
 * share every helper below: PKCS#7 is the older spelling and CMS the newer
 * one, and php's only real difference is that CMS takes an `$encoding`
 * (S/MIME, DER or PEM) where PKCS#7 is always S/MIME.
 *
 * They are also the only functions in this extension that work in FILES rather
 * than strings, which is php's own choice: every input and output is a path,
 * and a path that cannot be opened is `Error opening input file %s!` -- with
 * php's exclamation mark.
 */
/*
 * The message families work in PATHS, and they open them with OpenSSL's own
 * file BIO rather than through the engine's stream layer -- which is php's
 * choice and is observable twice over: a path that does not exist leaves
 * `system library::No such file or directory` and `BIO routines::no such
 * file` in the error ring, and a `phar://` or userland-wrapper path is not
 * a thing these six doors can read under php either.
 */
static char * SslPathZ(ph7_context *pCtx,ph7_value *pVal)
{
	int nPath = 0;
	const char *zPath = ph7_value_to_string(pVal,&nPath);
	char *zZ;
	if( zPath == 0 || nPath < 1 ){
		return 0;
	}
	zZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPath + 1));
	if( zZ == 0 ){
		return 0;
	}
	SyMemcpy(zPath,zZ,(sxu32)nPath);
	zZ[nPath] = 0;
	return zZ;
}
static BIO * SslBioOfFileArg(ph7_context *pCtx,ph7_value *pVal)
{
	char *zZ = SslPathZ(pCtx,pVal);
	BIO *pBio;
	if( zZ == 0 ){
		return 0;
	}
	pBio = BIO_new_file(zZ,"rb");
	SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);
	return pBio;
}
static int SslWriteBioToFileArg(ph7_context *pCtx,ph7_value *pVal,BIO *pBio)
{
	char *zMem = 0;
	long nMem = BIO_get_mem_data(pBio,&zMem);
	char *zZ = SslPathZ(pCtx,pVal);
	BIO *pOut;
	int rc = -1;
	if( zZ == 0 ){
		return -1;
	}
	pOut = BIO_new_file(zZ,"wb");
	SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);
	if( pOut == 0 ){
		return -1;
	}
	if( nMem <= 0 || BIO_write(pOut,zMem,(int)nMem) == (int)nMem ){
		rc = 0;
	}
	BIO_free(pOut);
	return rc;
}
/* The `$headers` array both sign doors prepend to an S/MIME body: a string
 * entry is a whole header line, a keyed one is `Key: value`. */
static void SslWriteHeaders(ph7_context *pCtx,ph7_value *pHeaders,BIO *pOut)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 i;
	SXUNUSED(pCtx);
	if( pHeaders == 0 || !ph7_value_is_array(pHeaders) ){
		return;
	}
	pMap = (ph7_hashmap *)pHeaders->x.pOther;
	pEntry = pMap->pFirst;
	for( i = 0 ; i < pMap->nEntry ; ++i ){
		ph7_value sKey;
		ph7_value *pData = HashmapExtractNodeValue(pEntry);
		const char *zVal;
		int nVal = 0;
		PH7_MemObjInit(pCtx->pVm,&sKey);
		PH7_HashmapExtractNodeKey(pEntry,&sKey);
		pEntry = pEntry->pPrev;
		zVal = pData ? ph7_value_to_string(pData,&nVal) : 0;
		if( zVal ){
			if( ph7_value_is_string(&sKey) ){
				int nKey = 0;
				const char *zKey = ph7_value_to_string(&sKey,&nKey);
				BIO_write(pOut,zKey,nKey);
				BIO_write(pOut,": ",2);
			}
			BIO_write(pOut,zVal,nVal);
			BIO_write(pOut,"\n",1);
		}
		PH7_MemObjRelease(&sKey);
	}
}
/* php's OPENSSL_CIPHER_* enum, which the two encrypt doors take as a NUMBER
 * rather than a name. */
static const EVP_CIPHER * SslCipherOfPhpEnum(sxi64 iCipher)
{
	switch( iCipher ){
		case 0: case 1: case 2: return EVP_rc2_cbc();
		case 3: return EVP_des_cbc();
		case 4: return EVP_des_ede3_cbc();
		case 5: return EVP_aes_128_cbc();
		case 6: return EVP_aes_192_cbc();
		case 7: return EVP_aes_256_cbc();
		default: return 0;
	}
}
/* A recipient list: one certificate, or an array of them. */
static STACK_OF(X509) * SslRecipients(ph7_context *pCtx,ph7_value *pVal)
{
	STACK_OF(X509) *pStack = sk_X509_new_null();
	if( pStack == 0 ){
		return 0;
	}
	if( ph7_value_is_array(pVal) ){
		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;
		ph7_hashmap_node *pEntry = pMap->pFirst;
		sxu32 i;
		for( i = 0 ; i < pMap->nEntry ; ++i ){
			ph7_value *pData = HashmapExtractNodeValue(pEntry);
			int bOwn = 0;
			X509 *pOne = pData ? SslCertOfValue(pCtx,pData,&bOwn,0) : 0;
			pEntry = pEntry->pPrev;
			if( pOne == 0 ){
				continue;
			}
			if( !bOwn ){
				X509_up_ref(pOne);
			}
			sk_X509_push(pStack,pOne);
		}
	}else{
		int bOwn = 0;
		X509 *pOne = SslCertOfValue(pCtx,pVal,&bOwn,0);
		if( pOne ){
			if( !bOwn ){
				X509_up_ref(pOne);
			}
			sk_X509_push(pStack,pOne);
		}
	}
	if( sk_X509_num(pStack) < 1 ){
		sk_X509_free(pStack);
		return 0;
	}
	return pStack;
}
/* The untrusted-certificate file both verify doors take. */
static STACK_OF(X509) * SslCertsFromFileArg(ph7_context *pCtx,ph7_value *pVal)
{
	STACK_OF(X509) *pStack;
	BIO *pBio = SslBioOfFileArg(pCtx,pVal);
	X509 *pOne;
	if( pBio == 0 ){
		return 0;
	}
	pStack = sk_X509_new_null();
	while( pStack && (pOne = PEM_read_bio_X509(pBio,0,0,0)) != 0 ){
		sk_X509_push(pStack,pOne);
	}
	ERR_clear_error();   /* running out of certificates is how the walk ENDS */
	BIO_free(pBio);
	return pStack;
}
static int SslMsgSign(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)
{
	BIO *pIn = 0,*pOut = 0;
	X509 *pCert = 0;
	EVP_PKEY *pKey = 0;
	STACK_OF(X509) *pExtra = 0;
	PKCS7 *pP7 = 0;
	CMS_ContentInfo *pCms = 0;
	int bOwnCert = 0,bOwnKey = 0,rc = -1;
	sxi64 iFlags = bCms ? 0 : PKCS7_DETACHED;
	sxi64 iEncoding = 1;   /* OPENSSL_ENCODING_SMIME */
	if( nArg < 5 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 5 ){
		iFlags = ph7_value_to_int64(apArg[5]);
	}
	if( bCms && nArg > 6 ){
		iEncoding = ph7_value_to_int64(apArg[6]);
	}
	/* The CERTIFICATE is read before the input file is opened, which is php's
	 * order and is observable: a bad cert AND a missing input answer the
	 * certificate's complaint. */
	pCert = SslCertOfValue(pCtx,apArg[2],&bOwnCert,1);
	if( pCert == 0 ){
		goto done;
	}
	pKey = PH7_SslKeyOfValue(pCtx,apArg[3],0,&bOwnKey);
	if( pKey == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error getting private key");
		goto done;
	}
	pIn = SslBioOfFileArg(pCtx,apArg[0]);
	if( pIn == 0 ){
		int nPath = 0;
		const char *zPath = ph7_value_to_string(apArg[0],&nPath);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Error opening input file %.*s!",nPath,zPath ? zPath : "");
		goto done;
	}
	if( nArg > (bCms ? 7 : 6) && !ph7_value_is_null(apArg[bCms ? 7 : 6]) ){
		pExtra = SslCertsFromFileArg(pCtx,apArg[bCms ? 7 : 6]);
	}
	pOut = BIO_new(BIO_s_mem());
	if( pOut == 0 ){
		goto done;
	}
	if( bCms ){
		pCms = CMS_sign(pCert,pKey,pExtra,pIn,(unsigned int)iFlags);
		if( pCms == 0 ){
			goto done;
		}
		SslWriteHeaders(pCtx,apArg[4],pOut);
		if( iEncoding == 0 ){
			rc = i2d_CMS_bio(pOut,pCms) == 1 ? 0 : -1;
		}else if( iEncoding == 2 ){
			rc = PEM_write_bio_CMS(pOut,pCms) == 1 ? 0 : -1;
		}else{
			BIO_reset(pIn);
			rc = SMIME_write_CMS(pOut,pCms,pIn,(int)iFlags) == 1 ? 0 : -1;
		}
	}else{
		pP7 = PKCS7_sign(pCert,pKey,pExtra,pIn,(int)iFlags);
		if( pP7 == 0 ){
			goto done;
		}
		SslWriteHeaders(pCtx,apArg[4],pOut);
		BIO_reset(pIn);
		rc = SMIME_write_PKCS7(pOut,pP7,pIn,(int)iFlags) == 1 ? 0 : -1;
	}
	if( rc == 0 ){
		rc = SslWriteBioToFileArg(pCtx,apArg[1],pOut);
	}
done:
	if( pP7 ){ PKCS7_free(pP7); }
	if( pCms ){ CMS_ContentInfo_free(pCms); }
	if( pExtra ){ sk_X509_pop_free(pExtra,X509_free); }
	if( bOwnKey > 0 ){ EVP_PKEY_free(pKey); }
	SslFreeCert(pCert,bOwnCert);
	if( pOut ){ BIO_free(pOut); }
	if( pIn ){ BIO_free(pIn); }

	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_pkcs7_sign(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgSign(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_cms_sign(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgSign(pCtx,nArg,apArg,1);
}
static int SslMsgEncrypt(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)
{
	BIO *pIn = 0,*pOut = 0;
	STACK_OF(X509) *pTo = 0;
	PKCS7 *pP7 = 0;
	CMS_ContentInfo *pCms = 0;
	const EVP_CIPHER *pCipher;
	sxi64 iFlags = 0,iEncoding = 1,iCipher = 5;
	int rc = -1;
	if( nArg < 4 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 4 ){
		iFlags = ph7_value_to_int64(apArg[4]);
	}
	if( bCms ){
		if( nArg > 5 ){ iEncoding = ph7_value_to_int64(apArg[5]); }
		if( nArg > 6 ){ iCipher = ph7_value_to_int64(apArg[6]); }
	}else{
		if( nArg > 5 ){ iCipher = ph7_value_to_int64(apArg[5]); }
	}
	pCipher = SslCipherOfPhpEnum(iCipher);
	if( pCipher == 0 ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to get cipher");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pIn = SslBioOfFileArg(pCtx,apArg[0]);
	if( pIn == 0 ){
		goto done;
	}
	pTo = SslRecipients(pCtx,apArg[2]);
	if( pTo == 0 ){
		/* Silent: the two ENCRYPT doors answer a flat false for a recipient
		 * they cannot read, where the two SIGN doors say so. */
		goto done;
	}
	pOut = BIO_new(BIO_s_mem());
	if( pOut == 0 ){
		goto done;
	}
	SslWriteHeaders(pCtx,apArg[3],pOut);
	if( bCms ){
		pCms = CMS_encrypt(pTo,pIn,pCipher,(unsigned int)iFlags);
		if( pCms == 0 ){
			goto done;
		}
		if( iEncoding == 0 ){
			rc = i2d_CMS_bio(pOut,pCms) == 1 ? 0 : -1;
		}else if( iEncoding == 2 ){
			rc = PEM_write_bio_CMS(pOut,pCms) == 1 ? 0 : -1;
		}else{
			rc = SMIME_write_CMS(pOut,pCms,0,(int)iFlags) == 1 ? 0 : -1;
		}
	}else{
		pP7 = PKCS7_encrypt(pTo,pIn,pCipher,(int)iFlags);
		if( pP7 == 0 ){
			goto done;
		}
		rc = SMIME_write_PKCS7(pOut,pP7,0,(int)iFlags) == 1 ? 0 : -1;
	}
	if( rc == 0 ){
		rc = SslWriteBioToFileArg(pCtx,apArg[1],pOut);
	}
done:
	if( pP7 ){ PKCS7_free(pP7); }
	if( pCms ){ CMS_ContentInfo_free(pCms); }
	if( pTo ){ sk_X509_pop_free(pTo,X509_free); }
	if( pOut ){ BIO_free(pOut); }
	if( pIn ){ BIO_free(pIn); }
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_pkcs7_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgEncrypt(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_cms_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgEncrypt(pCtx,nArg,apArg,1);
}
static int SslMsgDecrypt(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)
{
	BIO *pIn = 0,*pOut = 0;
	X509 *pCert = 0;
	EVP_PKEY *pKey = 0;
	PKCS7 *pP7 = 0;
	CMS_ContentInfo *pCms = 0;
	sxi64 iEncoding = 1;
	int bOwnCert = 0,bOwnKey = 0,rc = -1;
	if( nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( bCms && nArg > 4 ){
		iEncoding = ph7_value_to_int64(apArg[4]);
	}
	/* Both DECRYPT doors are silent about everything: a missing input file, an
	 * unreadable certificate and a key that will not open are all a flat
	 * false with the library's own reason left in the error ring. */
	pIn = SslBioOfFileArg(pCtx,apArg[0]);
	if( pIn == 0 ){
		goto done;
	}
	pCert = SslCertOfValue(pCtx,apArg[2],&bOwnCert,0);
	if( pCert == 0 ){
		goto done;
	}
	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
		pKey = PH7_SslKeyOfValue(pCtx,apArg[3],0,&bOwnKey);
	}else{
		pKey = PH7_SslKeyOfValue(pCtx,apArg[2],0,&bOwnKey);
	}
	if( pKey == 0 ){
		goto done;
	}
	pOut = BIO_new(BIO_s_mem());
	if( pOut == 0 ){
		goto done;
	}
	if( bCms ){
		if( iEncoding == 0 ){
			pCms = d2i_CMS_bio(pIn,0);
		}else if( iEncoding == 2 ){
			pCms = PEM_read_bio_CMS(pIn,0,0,0);
		}else{
			pCms = SMIME_read_CMS(pIn,0);
		}
		if( pCms == 0 ){
			goto done;
		}
		rc = CMS_decrypt(pCms,pKey,pCert,0,pOut,0) == 1 ? 0 : -1;
	}else{
		pP7 = SMIME_read_PKCS7(pIn,0);
		if( pP7 == 0 ){
			goto done;
		}
		rc = PKCS7_decrypt(pP7,pKey,pCert,pOut,0) == 1 ? 0 : -1;
	}
	if( rc == 0 ){
		rc = SslWriteBioToFileArg(pCtx,apArg[1],pOut);
	}
done:
	if( pP7 ){ PKCS7_free(pP7); }
	if( pCms ){ CMS_ContentInfo_free(pCms); }
	if( bOwnKey > 0 ){ EVP_PKEY_free(pKey); }
	SslFreeCert(pCert,bOwnCert);
	if( pOut ){ BIO_free(pOut); }
	if( pIn ){ BIO_free(pIn); }
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_pkcs7_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgDecrypt(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_cms_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgDecrypt(pCtx,nArg,apArg,1);
}
static int SslMsgVerify(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)
{
	BIO *pIn = 0,*pContent = 0,*pOut = 0;
	PKCS7 *pP7 = 0;
	CMS_ContentInfo *pCms = 0;
	X509_STORE *pStore = 0;
	STACK_OF(X509) *pUntrusted = 0,*pSigners = 0;
	sxi64 iFlags = 0,iEncoding = 1;
	int iCa = 3,iUntrusted = 4,iContent = 5,iSigners = 2,rc = -1,bRan = 0;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iFlags = ph7_value_to_int64(apArg[1]);
	if( bCms && nArg > 8 ){
		iEncoding = ph7_value_to_int64(apArg[8]);
	}
	pIn = SslBioOfFileArg(pCtx,apArg[0]);
	if( pIn == 0 ){
		goto done;
	}
	if( nArg > iUntrusted && !ph7_value_is_null(apArg[iUntrusted]) ){
		pUntrusted = SslCertsFromFileArg(pCtx,apArg[iUntrusted]);
	}
	pStore = SslStoreFromCaInfo(pCtx,nArg > iCa ? apArg[iCa] : 0);
	pOut = BIO_new(BIO_s_mem());
	if( pStore == 0 || pOut == 0 ){
		goto done;
	}
	if( bCms ){
		if( iEncoding == 0 ){
			pCms = d2i_CMS_bio(pIn,0);
		}else if( iEncoding == 2 ){
			pCms = PEM_read_bio_CMS(pIn,0,0,0);
		}else{
			pCms = SMIME_read_CMS_ex(pIn,0,&pContent,0);
		}
		if( pCms == 0 ){
			goto done;
		}
		bRan = 1;
		rc = CMS_verify(pCms,pUntrusted,pStore,pContent,pOut,(unsigned int)iFlags) == 1 ? 0 : -1;
		if( rc == 0 ){
			pSigners = CMS_get0_signers(pCms);
		}
	}else{
		pP7 = SMIME_read_PKCS7(pIn,&pContent);
		if( pP7 == 0 ){
			goto done;
		}
		bRan = 1;
		rc = PKCS7_verify(pP7,pUntrusted,pStore,pContent,pOut,(int)iFlags) == 1 ? 0 : -1;
		if( rc == 0 ){
			pSigners = PKCS7_get0_signers(pP7,pUntrusted,(int)iFlags);
		}
	}
	if( rc == 0 && nArg > iSigners && !ph7_value_is_null(apArg[iSigners]) && pSigners ){
		BIO *pCerts = BIO_new(BIO_s_mem());
		int i;
		if( pCerts ){
			for( i = 0 ; i < sk_X509_num(pSigners) ; ++i ){
				PEM_write_bio_X509(pCerts,sk_X509_value(pSigners,i));
			}
			SslWriteBioToFileArg(pCtx,apArg[iSigners],pCerts);
			BIO_free(pCerts);
		}
	}
	if( rc == 0 && nArg > iContent && !ph7_value_is_null(apArg[iContent]) ){
		SslWriteBioToFileArg(pCtx,apArg[iContent],pOut);
	}
done:
	if( pSigners ){ sk_X509_free(pSigners); }
	if( pP7 ){ PKCS7_free(pP7); }
	if( pCms ){ CMS_ContentInfo_free(pCms); }
	if( pUntrusted ){ sk_X509_pop_free(pUntrusted,X509_free); }
	if( pStore ){ X509_STORE_free(pStore); }
	if( pContent ){ BIO_free(pContent); }
	if( pOut ){ BIO_free(pOut); }
	if( pIn ){ BIO_free(pIn); }
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	if( rc != 0 && !bRan ){
		/* Never reached the verification at all. php's two doors part company
		 * exactly here: PKCS#7 answers -1 for "could not even read this" and
		 * keeps false for a signature that did not check out, while CMS --
		 * whose declared return is bool -- answers false for both. */
		if( bCms ){
			ph7_result_bool(pCtx,0);
		}else{
			ph7_result_int(pCtx,-1);
		}
		return PH7_OK;
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_pkcs7_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgVerify(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_cms_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgVerify(pCtx,nArg,apArg,1);
}
/*
 * The two READ doors take BYTES rather than a path, and hand back the
 * certificates a message carries -- as PEM strings, in an array. A signed
 * S/MIME body is not one of the forms they accept: it has to be the DER or PEM
 * structure itself, which is why reading back what sign() just wrote is false
 * under php too.
 */
static int SslMsgRead(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)
{
	SyBlob sIn;
	BIO *pIn = 0;
	PKCS7 *pP7 = 0;
	CMS_ContentInfo *pCms = 0;
	STACK_OF(X509) *pCerts = 0;
	const char *zData = 0;
	int nData = 0,rc = -1,i;
	ph7_value *pArray,*pVal;
	if( nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* These two take BYTES, not a path -- so they read through the engine's
	 * stream layer like every other string argument in this extension. */
	SyBlobInit(&sIn,&pCtx->pVm->sAllocator);
	if( PH7_SslBytesOfValue(pCtx,apArg[0],&sIn,&zData,&nData) != 0 || nData < 1 ){
		goto done;
	}
	pIn = BIO_new_mem_buf(zData,nData);
	if( pIn == 0 ){
		goto done;
	}
	if( bCms ){
		pCms = PEM_read_bio_CMS(pIn,0,0,0);
		if( pCms == 0 ){
			BIO_reset(pIn);
			pCms = d2i_CMS_bio(pIn,0);
		}
		if( pCms == 0 ){
			goto done;
		}
		pCerts = CMS_get1_certs(pCms);
	}else{
		pP7 = PEM_read_bio_PKCS7(pIn,0,0,0);
		if( pP7 == 0 ){
			BIO_reset(pIn);
			pP7 = d2i_PKCS7_bio(pIn,0);
		}
		if( pP7 == 0 ){
			goto done;
		}
		if( PKCS7_type_is_signed(pP7) && pP7->d.sign ){
			pCerts = pP7->d.sign->cert;
		}else if( PKCS7_type_is_signedAndEnveloped(pP7) && pP7->d.signed_and_enveloped ){
			pCerts = pP7->d.signed_and_enveloped->cert;
		}
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		goto done;
	}
	for( i = 0 ; pCerts && i < sk_X509_num(pCerts) ; ++i ){
		BIO *pOne = BIO_new(BIO_s_mem());
		if( pOne == 0 ){
			continue;
		}
		if( PEM_write_bio_X509(pOne,sk_X509_value(pCerts,i)) == 1 ){
			SslPemPut(pCtx,pArray,pVal,0,pOne);
		}
		BIO_free(pOne);
	}
	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);
	rc = 0;
done:
	if( bCms && pCerts ){ sk_X509_pop_free(pCerts,X509_free); }
	if( pP7 ){ PKCS7_free(pP7); }
	if( pCms ){ CMS_ContentInfo_free(pCms); }
	if( pIn ){ BIO_free(pIn); }
	SyBlobRelease(&sIn);
	if( rc != 0 ){
		PH7_SslStoreErrors(pCtx->pVm);
	}
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}
static int vm_builtin_openssl_pkcs7_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgRead(pCtx,nArg,apArg,0);
}
static int vm_builtin_openssl_cms_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SslMsgRead(pCtx,nArg,apArg,1);
}

/* ------------------------------------------------------------------------
 * Installation
 * ------------------------------------------------------------------------ */
PH7_PRIVATE sxi32 PH7_VmInstallOpenSslX509(ph7_vm *pVm)
{
	/* The three handle classes are installed by the other unit, which mounts
	 * this one; there is nothing of its own to declare here. */
	SXUNUSED(pVm);
	return SXRET_OK;
}
/* The functions this unit owns, in php's own registration order. */
PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslX509FuncTable(sxu32 *pnEntry)
{
	static const ph7_builtin_func aFunc[] = {
		{ "openssl_x509_export_to_file",    vm_builtin_openssl_x509_export_to_file    },
		{ "openssl_x509_export",            vm_builtin_openssl_x509_export            },
		{ "openssl_x509_fingerprint",       vm_builtin_openssl_x509_fingerprint       },
		{ "openssl_x509_check_private_key", vm_builtin_openssl_x509_check_private_key },
		{ "openssl_x509_verify",            vm_builtin_openssl_x509_verify            },
		{ "openssl_x509_parse",             vm_builtin_openssl_x509_parse             },
		{ "openssl_x509_checkpurpose",      vm_builtin_openssl_x509_checkpurpose      },
		{ "openssl_x509_read",              vm_builtin_openssl_x509_read              },
		{ "openssl_pkcs12_export_to_file",  vm_builtin_openssl_pkcs12_export_to_file  },
		{ "openssl_pkcs12_export",          vm_builtin_openssl_pkcs12_export          },
		{ "openssl_pkcs12_read",            vm_builtin_openssl_pkcs12_read            },
		{ "openssl_csr_export_to_file",     vm_builtin_openssl_csr_export_to_file     },
		{ "openssl_csr_export",             vm_builtin_openssl_csr_export             },
		{ "openssl_csr_sign",               vm_builtin_openssl_csr_sign               },
		{ "openssl_csr_new",                vm_builtin_openssl_csr_new                },
		{ "openssl_csr_get_subject",        vm_builtin_openssl_csr_get_subject        },
		{ "openssl_csr_get_public_key",     vm_builtin_openssl_csr_get_public_key     },
		{ "openssl_pkcs7_verify",           vm_builtin_openssl_pkcs7_verify           },
		{ "openssl_pkcs7_encrypt",          vm_builtin_openssl_pkcs7_encrypt          },
		{ "openssl_pkcs7_sign",             vm_builtin_openssl_pkcs7_sign             },
		{ "openssl_pkcs7_decrypt",          vm_builtin_openssl_pkcs7_decrypt          },
		{ "openssl_pkcs7_read",             vm_builtin_openssl_pkcs7_read             },
		{ "openssl_cms_verify",             vm_builtin_openssl_cms_verify             },
		{ "openssl_cms_encrypt",            vm_builtin_openssl_cms_encrypt            },
		{ "openssl_cms_sign",               vm_builtin_openssl_cms_sign               },
		{ "openssl_cms_decrypt",            vm_builtin_openssl_cms_decrypt            },
		{ "openssl_cms_read",               vm_builtin_openssl_cms_read               }
	};
	*pnEntry = (sxu32)SX_ARRAYSIZE(aFunc);
	return aFunc;
}

#else
/* Ensure non-empty translation unit when openssl is disabled (MSVC C4206) */
typedef int vm_openssl_x509_unused;
#endif /* PH7_ENABLE_OPENSSL */
