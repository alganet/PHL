# src/ph7/vm_openssl_x509.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1221/1679 lines (72.72%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#ifdef PH7_ENABLE_OPENSSL` |
|    - |    6 | `#include "openssl_int.h"` |
|    - |    7 | `#include <openssl/pkcs12.h>` |
|    - |    8 | `#include <openssl/pkcs7.h>` |
|    - |    9 | `#include <openssl/cms.h>` |
|    - |   10 | `#include <openssl/conf.h>` |
|    - |   11 |  |
|    - |   12 | `/*` |
|    - |   13 | ` * Section:` |
|    - |   14 | ` *    ext/openssl -- the certificate containers: X.509 certificates, signing` |
|    - |   15 | ` *    requests, PKCS#12 bundles and the PKCS#7 / CMS message families. The` |
|    - |   16 | ` *    library-wide surface, the keys and the ciphers are in vm_openssl.c.` |
|    - |   17 | ` * Status:` |
|    - |   18 | ` *    Stable.` |
|    - |   19 | ` *` |
|    - |   20 | ` * THE CONFIGURATION IS PART OF THE CONTRACT, and it is the thing a reading of` |
|    - |   21 | `` * php's manual would miss entirely. `openssl_csr_new(['commonName' => 'x'],`` |
|    - |   22 | `` * $key)` does not produce a one-field subject: it produces the caller's fields`` |
|    - |   23 | `` * followed by every `<name>_default` the system openssl.cnf declares for the`` |
|    - |   24 | `` * `req` section, so on a stock Debian box the subject also carries `C=AU`,`` |
|    - |   25 | ``  * `ST=Some-State` and `O=Internet Widgits Pty Ltd`. And `openssl_csr_sign()` `` |
|    - |   26 | `` * adds whatever the config's `x509_extensions` section says, which is why a`` |
|    - |   27 | ` * certificate signed with no options at all comes back with a` |
|    - |   28 | `` * subjectKeyIdentifier, an authorityKeyIdentifier and `CA:TRUE`. Both walks`` |
|    - |   29 | `` * are reproduced below, over the same file php reads (the `config` option, the`` |
|    - |   30 | ` * OPENSSL_CONF environment variable, or the library's compiled-in default).` |
|    - |   31 | ` *` |
|    - |   32 | ` * That also means the tests around this family pin what does NOT come from a` |
|    - |   33 | ` * config file: the shape of a parse, the order of a subject, the refusals.` |
|    - |   34 | ` */` |
|    - |   35 |  |
|    - |   36 | `/* ------------------------------------------------------------------------` |
|    - |   37 | ` * Reading a certificate or a request out of an argument` |
|    - |   38 | ` * ------------------------------------------------------------------------ */` |
|   38 |   39 | `static X509 * SslCertFromBytes(const char *zData,int nData)` |
|    1 |   40 | `{` |
|    - |   41 | `	BIO *pBio;` |
|    - |   42 | `	X509 *pCert;` |
|   39 |   43 | `	if( nData < 1 ){` |
|  ! 0 |   44 | `		return 0;` |
|    - |   45 | `	}` |
|   39 |   46 | `	pBio = BIO_new_mem_buf(zData,nData);` |
|   39 |   47 | `	if( pBio == 0 ){` |
|  ! 0 |   48 | `		return 0;` |
|    - |   49 | `	}` |
|    - |   50 | `	/* PEM ONLY, and that is php's contract rather than a shortcut: a string` |
|    - |   51 | ``	 * holding a DER certificate is `error:0480006C:PEM routines::no start`` |
|    - |   52 | ``	 * line` and false under php too. DER reaches these doors through a`` |
|    - |   53 | ``	 * `file://` path, which OpenSSL's own reader sniffs. */`` |
|   39 |   54 | `	pCert = PEM_read_bio_X509(pBio,0,0,0);` |
|   39 |   55 | `	BIO_free(pBio);` |
|   39 |   56 | `	return pCert;` |
|   20 |   57 | `}` |
|   10 |   58 | `static X509_REQ * SslCsrFromBytes(const char *zData,int nData)` |
|    1 |   59 | `{` |
|    - |   60 | `	BIO *pBio;` |
|    - |   61 | `	X509_REQ *pReq;` |
|   11 |   62 | `	if( nData < 1 ){` |
|  ! 0 |   63 | `		return 0;` |
|    - |   64 | `	}` |
|   11 |   65 | `	pBio = BIO_new_mem_buf(zData,nData);` |
|   11 |   66 | `	if( pBio == 0 ){` |
|  ! 0 |   67 | `		return 0;` |
|    - |   68 | `	}` |
|   11 |   69 | `	pReq = PEM_read_bio_X509_REQ(pBio,0,0,0);   /* PEM only -- see SslCertFromBytes */` |
|   11 |   70 | `	BIO_free(pBio);` |
|   11 |   71 | `	return pReq;` |
|    6 |   72 | `}` |
|    - |   73 | `/*` |
|    - |   74 | `` * php's `OpenSSLCertificate\|string $certificate`: the object, or PEM/DER`` |
|    - |   75 | `` * bytes, or a `file://` path. *pbOwn says whether the caller frees it.`` |
|    - |   76 | ` *` |
|    - |   77 | ` * bWarn is PER FUNCTION and not a property of the reader, which a sweep had` |
|    - |   78 | `` * to establish one door at a time: `openssl_x509_read`, both exports,`` |
|    - |   79 | `` * `openssl_x509_fingerprint`, both PKCS#12 exports and the two SIGN doors all`` |
|    - |   80 | `` * say `X.509 Certificate cannot be retrieved` for a string that will not`` |
|    - |   81 | `` * parse, while `openssl_x509_parse`, `check_private_key`, `verify`,`` |
|    - |   82 | `` * `checkpurpose` and both ENCRYPT doors answer false or -1 in silence.`` |
|    - |   83 | ` */` |
|  106 |   84 | `static X509 * SslCertOfValue(ph7_context *pCtx,ph7_value *pVal,int *pbOwn,int bWarn)` |
|    1 |   85 | `{` |
|    - |   86 | `	SyBlob sFile;` |
|  107 |   87 | `	const char *zData = 0;` |
|  107 |   88 | `	X509 *pCert = 0;` |
|  107 |   89 | `	int nData = 0;` |
|  107 |   90 | `	*pbOwn = 0;` |
|  107 |   91 | `	if( pVal == 0 ){` |
|  ! 0 |   92 | `		return 0;` |
|    - |   93 | `	}` |
|  107 |   94 | `	if( ph7_value_is_object(pVal) ){` |
|   69 |   95 | `		return (X509 *)PH7_SslHandleOf(pVal,PHL_SSL_KIND_CERT);` |
|    - |   96 | `	}` |
|   39 |   97 | `	if( !ph7_value_is_string(pVal) ){` |
|  ! 0 |   98 | `		return 0;` |
|    - |   99 | `	}` |
|   39 |  100 | `	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);` |
|   39 |  101 | `	if( PH7_SslBytesOfValue(pCtx,pVal,&sFile,&zData,&nData) == 0 ){` |
|   39 |  102 | `		pCert = SslCertFromBytes(zData,nData);` |
|   19 |  103 | `	}` |
|   39 |  104 | `	SyBlobRelease(&sFile);` |
|   39 |  105 | `	if( pCert ){` |
|   15 |  106 | `		*pbOwn = 1;` |
|    8 |  107 | `	}else{` |
|   25 |  108 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|   25 |  109 | `		if( bWarn ){` |
|   17 |  110 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  111 | `				"X.509 Certificate cannot be retrieved");` |
|    8 |  112 | `		}` |
|    - |  113 | `	}` |
|   39 |  114 | `	return pCert;` |
|   54 |  115 | `}` |
|   38 |  116 | `static X509_REQ * SslCsrOfValue(ph7_context *pCtx,ph7_value *pVal,int *pbOwn,int bWarn)` |
|    1 |  117 | `{` |
|    - |  118 | `	SyBlob sFile;` |
|   39 |  119 | `	const char *zData = 0;` |
|   39 |  120 | `	X509_REQ *pReq = 0;` |
|   39 |  121 | `	int nData = 0;` |
|   39 |  122 | `	*pbOwn = 0;` |
|   39 |  123 | `	if( pVal == 0 ){` |
|  ! 0 |  124 | `		return 0;` |
|    - |  125 | `	}` |
|   39 |  126 | `	if( ph7_value_is_object(pVal) ){` |
|   29 |  127 | `		return (X509_REQ *)PH7_SslHandleOf(pVal,PHL_SSL_KIND_CSR);` |
|    - |  128 | `	}` |
|   11 |  129 | `	if( !ph7_value_is_string(pVal) ){` |
|  ! 0 |  130 | `		return 0;` |
|    - |  131 | `	}` |
|   11 |  132 | `	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);` |
|   11 |  133 | `	if( PH7_SslBytesOfValue(pCtx,pVal,&sFile,&zData,&nData) == 0 ){` |
|   11 |  134 | `		pReq = SslCsrFromBytes(zData,nData);` |
|    5 |  135 | `	}` |
|   11 |  136 | `	SyBlobRelease(&sFile);` |
|   11 |  137 | `	if( pReq ){` |
|    3 |  138 | `		*pbOwn = 1;` |
|    2 |  139 | `	}else{` |
|    9 |  140 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    9 |  141 | `		if( bWarn ){` |
|    5 |  142 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  143 | `				"X.509 Certificate Signing Request cannot be retrieved");` |
|    2 |  144 | `		}` |
|    - |  145 | `	}` |
|   11 |  146 | `	return pReq;` |
|   20 |  147 | `}` |
|   94 |  148 | `static void SslFreeCert(X509 *pCert,int bOwn)` |
|    1 |  149 | `{` |
|   95 |  150 | `	if( pCert && bOwn ){` |
|   13 |  151 | `		X509_free(pCert);` |
|    6 |  152 | `	}` |
|   95 |  153 | `}` |
|   32 |  154 | `static void SslFreeCsr(X509_REQ *pReq,int bOwn)` |
|    1 |  155 | `{` |
|   33 |  156 | `	if( pReq && bOwn ){` |
|    3 |  157 | `		X509_REQ_free(pReq);` |
|    1 |  158 | `	}` |
|   33 |  159 | `}` |
|    - |  160 |  |
|    - |  161 | `/* ------------------------------------------------------------------------` |
|    - |  162 | ` * A distinguished name as php's array` |
|    - |  163 | ` * ------------------------------------------------------------------------ */` |
|    - |  164 | `/*` |
|    - |  165 | ` * php's rule for a REPEATED attribute is what makes this more than a loop: the` |
|    - |  166 | ` * first occurrence is a string, and a second one turns the slot into an ARRAY` |
|    - |  167 | `` * of every value in order. `$parsed['subject']['OU']` is therefore a string`` |
|    - |  168 | ` * for one organizational unit and a list for two.` |
|    - |  169 | ` */` |
|   36 |  170 | `static void SslNameToArray(ph7_context *pCtx,X509_NAME *pName,int bShort,ph7_value *pArray)` |
|    1 |  171 | `{` |
|   37 |  172 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   37 |  173 | `	ph7_value *pKeep = ph7_context_new_scalar(pCtx);` |
|   37 |  174 | `	ph7_value *pList = 0;` |
|    - |  175 | `	int i,n;` |
|   37 |  176 | `	if( pName == 0 \|\| pVal == 0 \|\| pKeep == 0 ){` |
|  ! 0 |  177 | `		return;` |
|    - |  178 | `	}` |
|   37 |  179 | `	n = X509_NAME_entry_count(pName);` |
|  211 |  180 | `	for( i = 0 ; i < n ; ++i ){` |
|  175 |  181 | `		X509_NAME_ENTRY *pEntry = X509_NAME_get_entry(pName,i);` |
|  175 |  182 | `		ASN1_OBJECT *pObj = X509_NAME_ENTRY_get_object(pEntry);` |
|  175 |  183 | `		ASN1_STRING *pStr = X509_NAME_ENTRY_get_data(pEntry);` |
|  175 |  184 | `		int nid = OBJ_obj2nid(pObj);` |
|    - |  185 | `		const char *zKey;` |
|    - |  186 | `		char zBuf[256];` |
|    - |  187 | `		ph7_value *pOld;` |
|  175 |  188 | `		if( nid != NID_undef ){` |
|  175 |  189 | `			zKey = bShort ? OBJ_nid2sn(nid) : OBJ_nid2ln(nid);` |
|   88 |  190 | `		}else{` |
|  ! 0 |  191 | `			OBJ_obj2txt(zBuf,(int)sizeof(zBuf),pObj,1);` |
|  ! 0 |  192 | `			zKey = zBuf;` |
|    - |  193 | `		}` |
|  175 |  194 | `		if( zKey == 0 ){` |
|  ! 0 |  195 | `			continue;` |
|    - |  196 | `		}` |
|  175 |  197 | `		pOld = ph7_array_fetch(pArray,zKey,-1);` |
|  175 |  198 | `		if( pOld == 0 ){` |
|  262 |  199 | `			ph7_value_string(pVal,(const char *)ASN1_STRING_get0_data(pStr),` |
|   87 |  200 | `				ASN1_STRING_length(pStr));` |
|  175 |  201 | `			ph7_array_add_strkey_elem(pArray,zKey,pVal);` |
|  175 |  202 | `			ph7_value_reset_string_cursor(pVal);` |
|  175 |  203 | `			continue;` |
|    - |  204 | `		}` |
|    - |  205 | `		/* A second value under the same name: php replaces the string with a` |
|    - |  206 | `		 * list holding both, and appends to that list from then on. */` |
|  ! 0 |  207 | `		if( ph7_value_is_array(pOld) ){` |
|  ! 0 |  208 | `			ph7_value_string(pVal,(const char *)ASN1_STRING_get0_data(pStr),` |
|  ! 0 |  209 | `				ASN1_STRING_length(pStr));` |
|  ! 0 |  210 | `			ph7_array_add_elem(pOld,0,pVal);` |
|  ! 0 |  211 | `			ph7_value_reset_string_cursor(pVal);` |
|  ! 0 |  212 | `			continue;` |
|    - |  213 | `		}` |
|    - |  214 | `		/* The first value has to be COPIED out before the list is created:` |
|    - |  215 | ``		 * `ph7_array_fetch` answers a pointer into the VM's memobj pool, and`` |
|    - |  216 | ``		 * creating an array grows that pool -- so `pOld` is a dangling pointer`` |
|    - |  217 | `		 * the moment ph7_context_new_array() returns. See the` |
|    - |  218 | ``		 * `pointers-die-across-a-user-callback` note. */`` |
|    - |  219 | `		{` |
|  ! 0 |  220 | `			int nOld = 0;` |
|  ! 0 |  221 | `			const char *zOld = ph7_value_to_string(pOld,&nOld);` |
|  ! 0 |  222 | `			ph7_value_string(pKeep,zOld ? zOld : "",nOld);` |
|    - |  223 | `		}` |
|  ! 0 |  224 | `		pList = ph7_context_new_array(pCtx);` |
|  ! 0 |  225 | `		if( pList == 0 ){` |
|  ! 0 |  226 | `			ph7_value_reset_string_cursor(pKeep);` |
|  ! 0 |  227 | `			continue;` |
|    - |  228 | `		}` |
|  ! 0 |  229 | `		ph7_array_add_elem(pList,0,pKeep);` |
|  ! 0 |  230 | `		ph7_value_reset_string_cursor(pKeep);` |
|  ! 0 |  231 | `		ph7_value_string(pVal,(const char *)ASN1_STRING_get0_data(pStr),` |
|  ! 0 |  232 | `			ASN1_STRING_length(pStr));` |
|  ! 0 |  233 | `		ph7_array_add_elem(pList,0,pVal);` |
|  ! 0 |  234 | `		ph7_value_reset_string_cursor(pVal);` |
|  ! 0 |  235 | `		ph7_array_add_strkey_elem(pArray,zKey,pList);` |
|  ! 0 |  236 | `	}` |
|   19 |  237 | `}` |
|    - |  238 | `/*` |
|    - |  239 | ` * An ASN.1 time, both ways php reports it: the RAW string the certificate` |
|    - |  240 | ` * carries ("260824120000Z"), and the epoch second it means. The conversion is` |
|    - |  241 | ` * the engine's own civil-days arithmetic rather than the C library's timegm(),` |
|    - |  242 | ` * which Windows does not have -- and which would answer the LOCAL zone through` |
|    - |  243 | ` * mktime() if it were substituted.` |
|    - |  244 | ` */` |
|   28 |  245 | `static sxi64 SslAsn1TimeToEpoch(const ASN1_TIME *pTime)` |
|    1 |  246 | `{` |
|    - |  247 | `	struct tm sTm;` |
|   29 |  248 | `	if( pTime == 0 ){` |
|  ! 0 |  249 | `		return 0;` |
|    - |  250 | `	}` |
|   29 |  251 | `	SyZero(&sTm,sizeof(sTm));` |
|   29 |  252 | `	if( ASN1_TIME_to_tm(pTime,&sTm) != 1 ){` |
|  ! 0 |  253 | `		return 0;` |
|    - |  254 | `	}` |
|   43 |  255 | `	return DtDaysFromCivil((sxi64)sTm.tm_year + 1900,sTm.tm_mon + 1,sTm.tm_mday) * 86400` |
|   28 |  256 | `		+ (sxi64)sTm.tm_hour * 3600 + (sxi64)sTm.tm_min * 60 + sTm.tm_sec;` |
|   15 |  257 | `}` |
|    - |  258 |  |
|    - |  259 | `/* ------------------------------------------------------------------------` |
|    - |  260 | ` * openssl_x509_parse()` |
|    - |  261 | ` * ------------------------------------------------------------------------ */` |
|  154 |  262 | `static void SslPutStr(ph7_context *pCtx,ph7_value *pArray,ph7_value *pVal,` |
|    - |  263 | `	const char *zKey,const char *zStr,int nStr)` |
|    1 |  264 | `{` |
|   77 |  265 | `	SXUNUSED(pCtx);` |
|  155 |  266 | `	ph7_value_string(pVal,zStr ? zStr : "",nStr);` |
|  155 |  267 | `	ph7_array_add_strkey_elem(pArray,zKey,pVal);` |
|  155 |  268 | `	ph7_value_reset_string_cursor(pVal);` |
|  155 |  269 | `}` |
|   16 |  270 | `static int vm_builtin_openssl_x509_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  271 | `{` |
|    - |  272 | `	X509 *pCert;` |
|    - |  273 | `	ph7_value *pArray,*pVal,*pSub;` |
|    - |  274 | `	BIO *pBio;` |
|   17 |  275 | `	int bOwn = 0,bShort = 1,i,nPurpose;` |
|    - |  276 | `	char zBuf[512];` |
|   17 |  277 | `	if( nArg < 1 ){` |
|  ! 0 |  278 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  279 | `		return PH7_OK;` |
|    - |  280 | `	}` |
|   17 |  281 | `	if( nArg > 1 ){` |
|    3 |  282 | `		bShort = ph7_value_to_bool(apArg[1]);` |
|    1 |  283 | `	}` |
|   17 |  284 | `	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,0);` |
|   17 |  285 | `	if( pCert == 0 ){` |
|    3 |  286 | `		ph7_result_bool(pCtx,0);` |
|    3 |  287 | `		return PH7_OK;` |
|    - |  288 | `	}` |
|   15 |  289 | `	pArray = ph7_context_new_array(pCtx);` |
|   15 |  290 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   15 |  291 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 |  292 | `		SslFreeCert(pCert,bOwn);` |
|  ! 0 |  293 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  294 | `		return PH7_OK;` |
|    - |  295 | `	}` |
|    - |  296 | `	/* name: the one-line rendering, which is the SUBJECT's and always uses the` |
|    - |  297 | `	 * short spelling whatever $short_names says. */` |
|   15 |  298 | `	X509_NAME_oneline(X509_get_subject_name(pCert),zBuf,(int)sizeof(zBuf));` |
|   15 |  299 | `	SslPutStr(pCtx,pArray,pVal,"name",zBuf,-1);` |
|   15 |  300 | `	pSub = ph7_context_new_array(pCtx);` |
|   15 |  301 | `	if( pSub ){` |
|   15 |  302 | `		SslNameToArray(pCtx,X509_get_subject_name(pCert),bShort,pSub);` |
|   15 |  303 | `		ph7_array_add_strkey_elem(pArray,"subject",pSub);` |
|    7 |  304 | `	}` |
|    - |  305 | `	/* hash: OpenSSL's subject-name hash -- the same EIGHT hex digits the` |
|    - |  306 | `	 * c_rehash symlinks in a CA directory carry. The value is narrowed to 32` |
|    - |  307 | `	 * bits before it is formatted, which is not cosmetic: the engine's own` |
|    - |  308 | `` 	 * formatter reads a `%lx` argument as 64 bits, so a 32-bit `unsigned long` `` |
|    - |  309 | `	 * (every Windows build) printed sixteen digits of half-garbage. */` |
|   15 |  310 | `	SyBufferFormat(zBuf,sizeof(zBuf),"%08x",(unsigned int)X509_subject_name_hash(pCert));` |
|   15 |  311 | `	SslPutStr(pCtx,pArray,pVal,"hash",zBuf,-1);` |
|   15 |  312 | `	pSub = ph7_context_new_array(pCtx);` |
|   15 |  313 | `	if( pSub ){` |
|   15 |  314 | `		SslNameToArray(pCtx,X509_get_issuer_name(pCert),bShort,pSub);` |
|   15 |  315 | `		ph7_array_add_strkey_elem(pArray,"issuer",pSub);` |
|    7 |  316 | `	}` |
|   15 |  317 | `	ph7_value_int64(pVal,(sxi64)X509_get_version(pCert));` |
|   15 |  318 | `	ph7_array_add_strkey_elem(pArray,"version",pVal);` |
|    - |  319 | `	{` |
|    - |  320 | `		/* The serial, twice: php gives the DECIMAL string and the HEX one, and` |
|    - |  321 | `		 * the hex has no leading zero and no 0x. Both come out of the BIGNUM` |
|    - |  322 | `		 * rather than the raw ASN.1 bytes, so a negative serial reads with a` |
|    - |  323 | `		 * minus sign exactly as php's does. */` |
|   15 |  324 | `		ASN1_INTEGER *pSerial = X509_get_serialNumber(pCert);` |
|   15 |  325 | `		BIGNUM *pBn = pSerial ? ASN1_INTEGER_to_BN(pSerial,0) : 0;` |
|   15 |  326 | `		char *zDec = pBn ? BN_bn2dec(pBn) : 0;` |
|   15 |  327 | `		char *zHex = pBn ? BN_bn2hex(pBn) : 0;` |
|   15 |  328 | `		SslPutStr(pCtx,pArray,pVal,"serialNumber",zDec ? zDec : "",-1);` |
|   15 |  329 | `		SslPutStr(pCtx,pArray,pVal,"serialNumberHex",zHex ? zHex : "",-1);` |
|   15 |  330 | `		if( zDec ){ OPENSSL_free(zDec); }` |
|   15 |  331 | `		if( zHex ){ OPENSSL_free(zHex); }` |
|   15 |  332 | `		if( pBn ){ BN_free(pBn); }` |
|    - |  333 | `	}` |
|    - |  334 | `	{` |
|   15 |  335 | `		const ASN1_TIME *pFrom = X509_get0_notBefore(pCert);` |
|   15 |  336 | `		const ASN1_TIME *pTo = X509_get0_notAfter(pCert);` |
|   29 |  337 | `		SslPutStr(pCtx,pArray,pVal,"validFrom",` |
|   14 |  338 | `			(const char *)ASN1_STRING_get0_data((const ASN1_STRING *)pFrom),` |
|    7 |  339 | `			ASN1_STRING_length((const ASN1_STRING *)pFrom));` |
|   29 |  340 | `		SslPutStr(pCtx,pArray,pVal,"validTo",` |
|   14 |  341 | `			(const char *)ASN1_STRING_get0_data((const ASN1_STRING *)pTo),` |
|    7 |  342 | `			ASN1_STRING_length((const ASN1_STRING *)pTo));` |
|   15 |  343 | `		ph7_value_int64(pVal,SslAsn1TimeToEpoch(pFrom));` |
|   15 |  344 | `		ph7_array_add_strkey_elem(pArray,"validFrom_time_t",pVal);` |
|   15 |  345 | `		ph7_value_int64(pVal,SslAsn1TimeToEpoch(pTo));` |
|   15 |  346 | `		ph7_array_add_strkey_elem(pArray,"validTo_time_t",pVal);` |
|    - |  347 | `	}` |
|    - |  348 | `	{` |
|   15 |  349 | `		int nid = X509_get_signature_nid(pCert);` |
|   15 |  350 | `		SslPutStr(pCtx,pArray,pVal,"signatureTypeSN",OBJ_nid2sn(nid),-1);` |
|   15 |  351 | `		SslPutStr(pCtx,pArray,pVal,"signatureTypeLN",OBJ_nid2ln(nid),-1);` |
|   15 |  352 | `		ph7_value_int(pVal,nid);` |
|   15 |  353 | `		ph7_array_add_strkey_elem(pArray,"signatureTypeNID",pVal);` |
|    - |  354 | `	}` |
|    - |  355 | `	/* purposes: keyed by php's own X509_PURPOSE_* number, each a triple of` |
|    - |  356 | `	 * "does it serve this purpose", "does it serve it as a CA" and the` |
|    - |  357 | `	 * purpose's short name. */` |
|   15 |  358 | `	pSub = ph7_context_new_array(pCtx);` |
|   15 |  359 | `	nPurpose = X509_PURPOSE_get_count();` |
|   15 |  360 | `	if( pSub ){` |
|    - |  361 | `		ph7_value *pTriple;` |
|  148 |  362 | `		for( i = 0 ; i < nPurpose ; ++i ){` |
|  134 |  363 | `			X509_PURPOSE *pPurpose = X509_PURPOSE_get0(i);` |
|  134 |  364 | `			int id = X509_PURPOSE_get_id(pPurpose);` |
|  134 |  365 | `			pTriple = ph7_context_new_array(pCtx);` |
|  134 |  366 | `			if( pTriple == 0 ){` |
|  ! 0 |  367 | `				continue;` |
|    - |  368 | `			}` |
|  134 |  369 | `			ph7_value_bool(pVal,X509_check_purpose(pCert,id,0) == 1);` |
|  134 |  370 | `			ph7_array_add_elem(pTriple,0,pVal);` |
|  134 |  371 | `			ph7_value_bool(pVal,X509_check_purpose(pCert,id,1) == 1);` |
|  134 |  372 | `			ph7_array_add_elem(pTriple,0,pVal);` |
|  134 |  373 | `			ph7_value_string(pVal,X509_PURPOSE_get0_sname(pPurpose),-1);` |
|  134 |  374 | `			ph7_array_add_elem(pTriple,0,pVal);` |
|  134 |  375 | `			ph7_value_reset_string_cursor(pVal);` |
|  134 |  376 | `			ph7_value_int(pVal,id);` |
|  134 |  377 | `			ph7_array_add_elem(pSub,pVal,pTriple);` |
|   71 |  378 | `		}` |
|   15 |  379 | `		ph7_array_add_strkey_elem(pArray,"purposes",pSub);` |
|    7 |  380 | `	}` |
|    - |  381 | `	/* extensions: the extension's name against the TEXT rendering OpenSSL` |
|    - |  382 | `	 * gives it -- not its DER. An extension that cannot be printed comes back` |
|    - |  383 | `	 * as its raw bytes, which is php's fallback too. */` |
|   15 |  384 | `	pSub = ph7_context_new_array(pCtx);` |
|   15 |  385 | `	if( pSub ){` |
|   15 |  386 | `		int nExt = X509_get_ext_count(pCert);` |
|   57 |  387 | `		for( i = 0 ; i < nExt ; ++i ){` |
|   43 |  388 | `			X509_EXTENSION *pExt = X509_get_ext(pCert,i);` |
|   43 |  389 | `			ASN1_OBJECT *pObj = X509_EXTENSION_get_object(pExt);` |
|   43 |  390 | `			int nid = OBJ_obj2nid(pObj);` |
|    - |  391 | `			const char *zKey;` |
|    - |  392 | `			char zOid[128];` |
|   43 |  393 | `			char *zMem = 0;` |
|   43 |  394 | `			long nMem = 0;` |
|   43 |  395 | `			if( nid != NID_undef ){` |
|   43 |  396 | `				zKey = OBJ_nid2sn(nid);` |
|   22 |  397 | `			}else{` |
|  ! 0 |  398 | `				OBJ_obj2txt(zOid,(int)sizeof(zOid),pObj,1);` |
|  ! 0 |  399 | `				zKey = zOid;` |
|    - |  400 | `			}` |
|   43 |  401 | `			pBio = BIO_new(BIO_s_mem());` |
|   43 |  402 | `			if( pBio == 0 ){` |
|  ! 0 |  403 | `				continue;` |
|    - |  404 | `			}` |
|   43 |  405 | `			if( X509V3_EXT_print(pBio,pExt,0,0) ){` |
|   43 |  406 | `				nMem = BIO_get_mem_data(pBio,&zMem);` |
|   22 |  407 | `			}else{` |
|  ! 0 |  408 | `				ASN1_STRING *pData = X509_EXTENSION_get_data(pExt);` |
|  ! 0 |  409 | `				BIO_write(pBio,ASN1_STRING_get0_data(pData),ASN1_STRING_length(pData));` |
|  ! 0 |  410 | `				nMem = BIO_get_mem_data(pBio,&zMem);` |
|    - |  411 | `			}` |
|   43 |  412 | `			SslPutStr(pCtx,pSub,pVal,zKey,zMem,(int)nMem);` |
|   43 |  413 | `			BIO_free(pBio);` |
|   22 |  414 | `		}` |
|   15 |  415 | `		ph7_array_add_strkey_elem(pArray,"extensions",pSub);` |
|    7 |  416 | `	}` |
|   15 |  417 | `	SslFreeCert(pCert,bOwn);` |
|   15 |  418 | `	ph7_result_value(pCtx,pArray);` |
|   15 |  419 | `	return PH7_OK;` |
|    9 |  420 | `}` |
|    - |  421 | `/* ------------------------------------------------------------------------` |
|    - |  422 | ` * The rest of the certificate surface` |
|    - |  423 | ` * ------------------------------------------------------------------------ */` |
|    6 |  424 | `static int vm_builtin_openssl_x509_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  425 | `{` |
|    - |  426 | `	X509 *pCert;` |
|    7 |  427 | `	int bOwn = 0;` |
|    7 |  428 | `	if( nArg < 1 ){` |
|  ! 0 |  429 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  430 | `		return PH7_OK;` |
|    - |  431 | `	}` |
|    7 |  432 | `	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,1);` |
|    7 |  433 | `	if( pCert == 0 ){` |
|    5 |  434 | `		ph7_result_bool(pCtx,0);` |
|    5 |  435 | `		return PH7_OK;` |
|    - |  436 | `	}` |
|    3 |  437 | `	if( !bOwn ){` |
|    - |  438 | `		/* Already an OpenSSLCertificate: php hands back the same object. */` |
|  ! 0 |  439 | `		ph7_result_value(pCtx,apArg[0]);` |
|  ! 0 |  440 | `		return PH7_OK;` |
|    - |  441 | `	}` |
|    3 |  442 | `	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_CERT,(void *)pCert);` |
|    4 |  443 | `}` |
|   16 |  444 | `static int SslX509Export(ph7_context *pCtx,int nArg,ph7_value **apArg,int bToFile)` |
|    1 |  445 | `{` |
|    - |  446 | `	X509 *pCert;` |
|    - |  447 | `	BIO *pBio;` |
|   17 |  448 | `	char *zMem = 0;` |
|    - |  449 | `	long nMem;` |
|   17 |  450 | `	int bOwn = 0,bNoText = 1,rc = 0;` |
|   17 |  451 | `	if( nArg < 2 ){` |
|  ! 0 |  452 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  453 | `		return PH7_OK;` |
|    - |  454 | `	}` |
|   17 |  455 | `	if( nArg > 2 ){` |
|  ! 0 |  456 | `		bNoText = ph7_value_to_bool(apArg[2]);` |
|  ! 0 |  457 | `	}` |
|   17 |  458 | `	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,1);` |
|   17 |  459 | `	if( pCert == 0 ){` |
|    5 |  460 | `		ph7_result_bool(pCtx,0);` |
|    5 |  461 | `		return PH7_OK;` |
|    - |  462 | `	}` |
|   13 |  463 | `	pBio = BIO_new(BIO_s_mem());` |
|   13 |  464 | `	if( pBio == 0 ){` |
|  ! 0 |  465 | `		SslFreeCert(pCert,bOwn);` |
|  ! 0 |  466 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  467 | `		return PH7_OK;` |
|    - |  468 | `	}` |
|    - |  469 | ``	/* `$no_text = false` asks for the human-readable dump BEFORE the PEM, which`` |
|    - |  470 | ``	 * is what `openssl x509 -text` prints. */`` |
|   13 |  471 | `	if( !bNoText ){` |
|  ! 0 |  472 | `		X509_print(pBio,pCert);` |
|  ! 0 |  473 | `	}` |
|   13 |  474 | `	if( PEM_write_bio_X509(pBio,pCert) != 1 ){` |
|  ! 0 |  475 | `		rc = -1;` |
|  ! 0 |  476 | `	}` |
|   13 |  477 | `	if( rc == 0 ){` |
|   13 |  478 | `		nMem = BIO_get_mem_data(pBio,&zMem);` |
|   13 |  479 | `		if( bToFile ){` |
|  ! 0 |  480 | `			int nPath = 0;` |
|  ! 0 |  481 | `			const char *zPath = ph7_value_to_string(apArg[1],&nPath);` |
|  ! 0 |  482 | `			rc = PH7_SslWriteFileArg(pCtx,zPath,nPath,zMem,(sxu32)nMem);` |
|  ! 0 |  483 | `			if( rc != 0 ){` |
|  ! 0 |  484 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 |  485 | `					"Error opening file %.*s",nPath,zPath ? zPath : "");` |
|  ! 0 |  486 | `			}` |
|  ! 0 |  487 | `		}else{` |
|   13 |  488 | `			ph7_value *pRes = ph7_context_new_scalar(pCtx);` |
|   13 |  489 | `			if( pRes ){` |
|   13 |  490 | `				ph7_value_string(pRes,zMem,(int)nMem);` |
|   13 |  491 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);` |
|   13 |  492 | `				ph7_context_release_value(pCtx,pRes);` |
|    6 |  493 | `			}` |
|    - |  494 | `		}` |
|    6 |  495 | `	}` |
|   13 |  496 | `	BIO_free(pBio);` |
|   13 |  497 | `	SslFreeCert(pCert,bOwn);` |
|   13 |  498 | `	if( rc != 0 ){` |
|  ! 0 |  499 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|  ! 0 |  500 | `	}` |
|   13 |  501 | `	ph7_result_bool(pCtx,rc == 0);` |
|   13 |  502 | `	return PH7_OK;` |
|    9 |  503 | `}` |
|   14 |  504 | `static int vm_builtin_openssl_x509_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  505 | `{` |
|   15 |  506 | `	return SslX509Export(pCtx,nArg,apArg,0);` |
|    1 |  507 | `}` |
|    2 |  508 | `static int vm_builtin_openssl_x509_export_to_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  509 | `{` |
|    3 |  510 | `	return SslX509Export(pCtx,nArg,apArg,1);` |
|    1 |  511 | `}` |
|   28 |  512 | `static int vm_builtin_openssl_x509_fingerprint(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  513 | `{` |
|    - |  514 | `	X509 *pCert;` |
|    - |  515 | `	const EVP_MD *pMd;` |
|   29 |  516 | `	EVP_MD *pFetched = 0;` |
|    - |  517 | `	unsigned char aOut[EVP_MAX_MD_SIZE];` |
|   29 |  518 | `	unsigned int nOut = 0;` |
|   29 |  519 | `	int bOwn = 0,bRaw = 0;` |
|   29 |  520 | `	if( nArg < 1 ){` |
|  ! 0 |  521 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  522 | `		return PH7_OK;` |
|    - |  523 | `	}` |
|   29 |  524 | `	if( nArg > 2 ){` |
|    3 |  525 | `		bRaw = ph7_value_to_bool(apArg[2]);` |
|    1 |  526 | `	}` |
|    - |  527 | ``	/* The certificate is read FIRST: `openssl_x509_fingerprint('junk',`` |
|    - |  528 | ``	 * 'nosuchdigest')` is `X.509 Certificate cannot be retrieved` under php,`` |
|    - |  529 | ``	 * not `Unknown digest algorithm`. */`` |
|   29 |  530 | `	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,1);` |
|   29 |  531 | `	if( pCert == 0 ){` |
|    5 |  532 | `		ph7_result_bool(pCtx,0);` |
|    5 |  533 | `		return PH7_OK;` |
|    - |  534 | `	}` |
|   25 |  535 | `	if( nArg > 1 ){` |
|   19 |  536 | `		pMd = PH7_SslDigestOfValue(apArg[1],&pFetched);` |
|   10 |  537 | `	}else{` |
|    7 |  538 | `		pMd = EVP_sha1();` |
|    - |  539 | `	}` |
|   25 |  540 | `	if( pMd == 0 ){` |
|    3 |  541 | `		SslFreeCert(pCert,bOwn);` |
|    3 |  542 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Unknown digest algorithm");` |
|    3 |  543 | `		ph7_result_bool(pCtx,0);` |
|    3 |  544 | `		return PH7_OK;` |
|    - |  545 | `	}` |
|   23 |  546 | `	if( X509_digest(pCert,pMd,aOut,&nOut) != 1 ){` |
|  ! 0 |  547 | `		SslFreeCert(pCert,bOwn);` |
|  ! 0 |  548 | `		if( pFetched ){ EVP_MD_free(pFetched); }` |
|  ! 0 |  549 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|  ! 0 |  550 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  551 | `		return PH7_OK;` |
|    - |  552 | `	}` |
|   23 |  553 | `	SslFreeCert(pCert,bOwn);` |
|   23 |  554 | `	if( pFetched ){ EVP_MD_free(pFetched); }` |
|   23 |  555 | `	if( bRaw ){` |
|    3 |  556 | `		ph7_result_string(pCtx,(const char *)aOut,(int)nOut);` |
|    2 |  557 | `	}else{` |
|    - |  558 | `		static const char zDigit[] = "0123456789abcdef";` |
|    - |  559 | `		char zHex[EVP_MAX_MD_SIZE * 2];` |
|    - |  560 | `		unsigned int i;` |
|  461 |  561 | `		for( i = 0 ; i < nOut ; ++i ){` |
|  441 |  562 | `			zHex[i * 2]     = zDigit[(aOut[i] >> 4) & 0x0F];` |
|  441 |  563 | `			zHex[i * 2 + 1] = zDigit[aOut[i] & 0x0F];` |
|  221 |  564 | `		}` |
|   21 |  565 | `		ph7_result_string(pCtx,zHex,(int)(nOut * 2));` |
|    - |  566 | `	}` |
|   23 |  567 | `	return PH7_OK;` |
|   15 |  568 | `}` |
|    6 |  569 | `static int vm_builtin_openssl_x509_check_private_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  570 | `{` |
|    - |  571 | `	X509 *pCert;` |
|    - |  572 | `	EVP_PKEY *pKey;` |
|    7 |  573 | `	int bOwnCert = 0,bOwnKey = 0,iRc;` |
|    7 |  574 | `	if( nArg < 2 ){` |
|  ! 0 |  575 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  576 | `		return PH7_OK;` |
|    - |  577 | `	}` |
|    7 |  578 | `	pCert = SslCertOfValue(pCtx,apArg[0],&bOwnCert,0);` |
|    7 |  579 | `	if( pCert == 0 ){` |
|    3 |  580 | `		ph7_result_bool(pCtx,0);` |
|    3 |  581 | `		return PH7_OK;` |
|    - |  582 | `	}` |
|    5 |  583 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[1],0,&bOwnKey);` |
|    5 |  584 | `	if( pKey == 0 ){` |
|  ! 0 |  585 | `		SslFreeCert(pCert,bOwnCert);` |
|  ! 0 |  586 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  587 | `		return PH7_OK;` |
|    - |  588 | `	}` |
|    5 |  589 | `	iRc = X509_check_private_key(pCert,pKey);` |
|    5 |  590 | `	SslFreeCert(pCert,bOwnCert);` |
|    5 |  591 | `	if( bOwnKey > 0 ){` |
|  ! 0 |  592 | `		EVP_PKEY_free(pKey);` |
|  ! 0 |  593 | `	}` |
|    5 |  594 | `	if( iRc != 1 ){` |
|    3 |  595 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    1 |  596 | `	}` |
|    5 |  597 | `	ph7_result_bool(pCtx,iRc == 1);` |
|    5 |  598 | `	return PH7_OK;` |
|    4 |  599 | `}` |
|    6 |  600 | `static int vm_builtin_openssl_x509_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  601 | `{` |
|    - |  602 | `	X509 *pCert;` |
|    - |  603 | `	EVP_PKEY *pKey;` |
|    7 |  604 | `	int bOwnCert = 0,bOwnKey = 0,iRc;` |
|    7 |  605 | `	if( nArg < 2 ){` |
|  ! 0 |  606 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 |  607 | `		return PH7_OK;` |
|    - |  608 | `	}` |
|    7 |  609 | `	pCert = SslCertOfValue(pCtx,apArg[0],&bOwnCert,0);` |
|    7 |  610 | `	if( pCert == 0 ){` |
|    3 |  611 | `		ph7_result_int(pCtx,-1);` |
|    3 |  612 | `		return PH7_OK;` |
|    - |  613 | `	}` |
|    5 |  614 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[1],1,&bOwnKey);` |
|    5 |  615 | `	if( pKey == 0 ){` |
|  ! 0 |  616 | `		SslFreeCert(pCert,bOwnCert);` |
|    - |  617 | `		/* php's three answers here are 1, 0 and -1, and a key it cannot read` |
|    - |  618 | `		 * is the SAME -1 a failed verification gives. */` |
|  ! 0 |  619 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 |  620 | `		return PH7_OK;` |
|    - |  621 | `	}` |
|    5 |  622 | `	iRc = X509_verify(pCert,pKey);` |
|    5 |  623 | `	SslFreeCert(pCert,bOwnCert);` |
|    5 |  624 | `	if( bOwnKey > 0 ){` |
|    5 |  625 | `		EVP_PKEY_free(pKey);` |
|    2 |  626 | `	}` |
|    5 |  627 | `	if( iRc != 1 ){` |
|  ! 0 |  628 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|  ! 0 |  629 | `	}` |
|    5 |  630 | `	ph7_result_int(pCtx,iRc == 1 ? 1 : (iRc == 0 ? 0 : -1));` |
|    5 |  631 | `	return PH7_OK;` |
|    4 |  632 | `}` |
|    - |  633 | `/*` |
|    - |  634 | ` * openssl_x509_checkpurpose(): a chain verification with a purpose attached.` |
|    - |  635 | `` * `$ca_info` is a list of files and DIRECTORIES, which is the same pair`` |
|    - |  636 | ` * X509_STORE takes, and an empty list means "trust nothing", which is why the` |
|    - |  637 | ` * self-signed certificate a program just made answers false.` |
|    - |  638 | ` */` |
|   10 |  639 | `static X509_STORE * SslStoreFromCaInfo(ph7_context *pCtx,ph7_value *pList)` |
|    1 |  640 | `{` |
|   11 |  641 | `	X509_STORE *pStore = X509_STORE_new();` |
|    - |  642 | `	ph7_hashmap *pMap;` |
|    - |  643 | `	ph7_hashmap_node *pEntry;` |
|    - |  644 | `	sxu32 i;` |
|   11 |  645 | `	if( pStore == 0 ){` |
|  ! 0 |  646 | `		return 0;` |
|    - |  647 | `	}` |
|   11 |  648 | `	if( pList == 0 \|\| !ph7_value_is_array(pList) ){` |
|    9 |  649 | `		return pStore;` |
|    - |  650 | `	}` |
|    3 |  651 | `	pMap = (ph7_hashmap *)pList->x.pOther;` |
|    3 |  652 | `	pEntry = pMap->pFirst;` |
|    3 |  653 | `	for( i = 0 ; i < pMap->nEntry ; ++i ){` |
|  ! 0 |  654 | `		ph7_value *pData = HashmapExtractNodeValue(pEntry);` |
|  ! 0 |  655 | `		pEntry = pEntry->pPrev;` |
|  ! 0 |  656 | `		if( pData == 0 \|\| !ph7_value_is_string(pData) ){` |
|  ! 0 |  657 | `			continue;` |
|    - |  658 | `		}` |
|    - |  659 | `		{` |
|  ! 0 |  660 | `			int nPath = 0;` |
|  ! 0 |  661 | `			const char *zPath = ph7_value_to_string(pData,&nPath);` |
|    - |  662 | `			char *zZ;` |
|  ! 0 |  663 | `			if( zPath == 0 \|\| nPath < 1 ){` |
|  ! 0 |  664 | `				continue;` |
|    - |  665 | `			}` |
|  ! 0 |  666 | `			zZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPath + 1));` |
|  ! 0 |  667 | `			if( zZ == 0 ){` |
|  ! 0 |  668 | `				continue;` |
|    - |  669 | `			}` |
|  ! 0 |  670 | `			SyMemcpy(zPath,zZ,(sxu32)nPath);` |
|  ! 0 |  671 | `			zZ[nPath] = 0;` |
|    - |  672 | `			/* php tries the entry as a FILE and then as a DIRECTORY, and says` |
|    - |  673 | `			 * nothing when neither works. */` |
|  ! 0 |  674 | `			if( X509_STORE_load_locations(pStore,zZ,0) != 1 ){` |
|  ! 0 |  675 | `				ERR_clear_error();` |
|  ! 0 |  676 | `				if( X509_STORE_load_locations(pStore,0,zZ) != 1 ){` |
|  ! 0 |  677 | `					ERR_clear_error();` |
|  ! 0 |  678 | `				}` |
|  ! 0 |  679 | `			}` |
|  ! 0 |  680 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);` |
|    - |  681 | `		}` |
|  ! 0 |  682 | `	}` |
|    3 |  683 | `	return pStore;` |
|    6 |  684 | `}` |
|    4 |  685 | `static int vm_builtin_openssl_x509_checkpurpose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  686 | `{` |
|    - |  687 | `	X509 *pCert;` |
|    - |  688 | `	X509_STORE *pStore;` |
|    - |  689 | `	X509_STORE_CTX *pStoreCtx;` |
|    5 |  690 | `	STACK_OF(X509) *pUntrusted = 0;` |
|    5 |  691 | `	int bOwn = 0,iPurpose,iRc = -1;` |
|    5 |  692 | `	if( nArg < 2 ){` |
|  ! 0 |  693 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  694 | `		return PH7_OK;` |
|    - |  695 | `	}` |
|    5 |  696 | `	iPurpose = (int)ph7_value_to_int64(apArg[1]);` |
|    5 |  697 | `	pCert = SslCertOfValue(pCtx,apArg[0],&bOwn,0);` |
|    5 |  698 | `	if( pCert == 0 ){` |
|    - |  699 | `		/* Not false: a certificate this door cannot READ is the same -1 a` |
|    - |  700 | `		 * chain it cannot BUILD gives. */` |
|    3 |  701 | `		ph7_result_int(pCtx,-1);` |
|    3 |  702 | `		return PH7_OK;` |
|    - |  703 | `	}` |
|    3 |  704 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|    - |  705 | `		SyBlob sFile;` |
|  ! 0 |  706 | `		const char *zData = 0;` |
|  ! 0 |  707 | `		int nData = 0;` |
|  ! 0 |  708 | `		SyBlobInit(&sFile,&pCtx->pVm->sAllocator);` |
|  ! 0 |  709 | `		if( PH7_SslBytesOfValue(pCtx,apArg[3],&sFile,&zData,&nData) == 0 && nData > 0 ){` |
|  ! 0 |  710 | `			BIO *pBio = BIO_new_mem_buf(zData,nData);` |
|  ! 0 |  711 | `			if( pBio ){` |
|    - |  712 | `				X509 *pOne;` |
|  ! 0 |  713 | `				pUntrusted = sk_X509_new_null();` |
|  ! 0 |  714 | `				while( pUntrusted && (pOne = PEM_read_bio_X509(pBio,0,0,0)) != 0 ){` |
|  ! 0 |  715 | `					sk_X509_push(pUntrusted,pOne);` |
|  ! 0 |  716 | `				}` |
|  ! 0 |  717 | `				ERR_clear_error();` |
|  ! 0 |  718 | `				BIO_free(pBio);` |
|  ! 0 |  719 | `			}` |
|  ! 0 |  720 | `		}` |
|  ! 0 |  721 | `		SyBlobRelease(&sFile);` |
|  ! 0 |  722 | `	}` |
|    3 |  723 | `	pStore = SslStoreFromCaInfo(pCtx,nArg > 2 ? apArg[2] : 0);` |
|    3 |  724 | `	pStoreCtx = pStore ? X509_STORE_CTX_new() : 0;` |
|    3 |  725 | `	if( pStoreCtx && X509_STORE_CTX_init(pStoreCtx,pStore,pCert,pUntrusted) == 1 ){` |
|    3 |  726 | `		X509_STORE_CTX_set_purpose(pStoreCtx,iPurpose);` |
|    3 |  727 | `		iRc = X509_verify_cert(pStoreCtx);` |
|    1 |  728 | `	}` |
|    3 |  729 | `	if( pStoreCtx ){ X509_STORE_CTX_free(pStoreCtx); }` |
|    3 |  730 | `	if( pStore ){ X509_STORE_free(pStore); }` |
|    3 |  731 | `	if( pUntrusted ){ sk_X509_pop_free(pUntrusted,X509_free); }` |
|    3 |  732 | `	SslFreeCert(pCert,bOwn);` |
|    3 |  733 | `	if( iRc < 0 ){` |
|  ! 0 |  734 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|  ! 0 |  735 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 |  736 | `		return PH7_OK;` |
|    - |  737 | `	}` |
|    3 |  738 | `	if( iRc != 1 ){` |
|    3 |  739 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    1 |  740 | `	}` |
|    3 |  741 | `	ph7_result_bool(pCtx,iRc == 1);` |
|    3 |  742 | `	return PH7_OK;` |
|    3 |  743 | `}` |
|    - |  744 |  |
|    - |  745 | `/* ------------------------------------------------------------------------` |
|    - |  746 | ` * The configuration php reads` |
|    - |  747 | ` * ------------------------------------------------------------------------ */` |
|    - |  748 | `/*` |
|    - |  749 | ``  * Which file, and the section names inside it. php looks at the `config` `` |
|    - |  750 | ` * option first, then OPENSSL_CONF / SSLEAY_CONF, then the library's own` |
|    - |  751 | ` * default -- and a file it cannot load is not an error: the request is simply` |
|    - |  752 | ` * built without defaults and without extensions.` |
|    - |  753 | ` */` |
|    - |  754 | `typedef struct SslReqConf SslReqConf;` |
|    - |  755 | `struct SslReqConf {` |
|    - |  756 | `	CONF *pConf;` |
|    - |  757 | ``	const char *zDnSection;    /* `req`'s distinguished_name */`` |
|    - |  758 | ``	const char *zReqExts;      /* `req`'s req_extensions, or the option's */`` |
|    - |  759 | ``	const char *zX509Exts;     /* `req`'s x509_extensions, or the option's */`` |
|    - |  760 | `	const EVP_MD *pDigest;` |
|    - |  761 | `	EVP_MD *pFetched;` |
|    - |  762 | `	sxi64 iBits;` |
|    - |  763 | `	sxi64 iType;` |
|    - |  764 | `	const char *zCurve;` |
|    - |  765 | `	int nCurve;` |
|    - |  766 | `	int bEncryptKey;` |
|    - |  767 | `};` |
|  120 |  768 | `static const char * SslConfValue(SslReqConf *pReq,const char *zSection,const char *zName)` |
|    1 |  769 | `{` |
|  121 |  770 | `	return pReq->pConf ? NCONF_get_string(pReq->pConf,zSection,zName) : 0;` |
|    1 |  771 | `}` |
|   30 |  772 | `static void SslReqConfLoad(ph7_context *pCtx,ph7_value *pOpts,SslReqConf *pReq)` |
|    1 |  773 | `{` |
|    - |  774 | `	ph7_value *pVal;` |
|   31 |  775 | `	char *zFile = 0;` |
|   31 |  776 | `	const char *zPath = 0;` |
|   31 |  777 | `	int nPath = 0;` |
|   31 |  778 | `	SyZero(pReq,sizeof(*pReq));` |
|   31 |  779 | `	pReq->zDnSection = "req_distinguished_name";` |
|   31 |  780 | `	pReq->iBits = 2048;` |
|   31 |  781 | `	pReq->iType = 0;` |
|   31 |  782 | `	pReq->bEncryptKey = -1;` |
|   31 |  783 | `	if( pOpts && ph7_value_is_array(pOpts) ){` |
|   29 |  784 | `		pVal = ph7_array_fetch(pOpts,"config",-1);` |
|   29 |  785 | `		if( pVal && ph7_value_is_string(pVal) ){` |
|  ! 0 |  786 | `			zPath = ph7_value_to_string(pVal,&nPath);` |
|  ! 0 |  787 | `		}` |
|   14 |  788 | `	}` |
|   31 |  789 | `	if( zPath && nPath > 0 ){` |
|  ! 0 |  790 | `		zFile = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPath + 1));` |
|  ! 0 |  791 | `		if( zFile ){` |
|  ! 0 |  792 | `			SyMemcpy(zPath,zFile,(sxu32)nPath);` |
|  ! 0 |  793 | `			zFile[nPath] = 0;` |
|  ! 0 |  794 | `		}` |
|  ! 0 |  795 | `	}else{` |
|   31 |  796 | `		zFile = CONF_get1_default_config_file();` |
|    - |  797 | `	}` |
|   31 |  798 | `	if( zFile ){` |
|   31 |  799 | `		pReq->pConf = NCONF_new(0);` |
|   31 |  800 | `		if( pReq->pConf && NCONF_load(pReq->pConf,zFile,0) <= 0 ){` |
|  ! 0 |  801 | `			NCONF_free(pReq->pConf);` |
|  ! 0 |  802 | `			pReq->pConf = 0;` |
|  ! 0 |  803 | `			ERR_clear_error();` |
|  ! 0 |  804 | `		}` |
|   15 |  805 | `	}` |
|   31 |  806 | `	if( zPath && nPath > 0 ){` |
|  ! 0 |  807 | `		if( zFile ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zFile); }` |
|   31 |  808 | `	}else if( zFile ){` |
|   31 |  809 | `		OPENSSL_free(zFile);` |
|   15 |  810 | `	}` |
|   31 |  811 | `	if( pReq->pConf ){` |
|   31 |  812 | `		const char *z = SslConfValue(pReq,"req","distinguished_name");` |
|   31 |  813 | `		if( z ){` |
|   31 |  814 | `			pReq->zDnSection = z;` |
|   15 |  815 | `		}` |
|   31 |  816 | `		pReq->zReqExts = SslConfValue(pReq,"req","req_extensions");` |
|   31 |  817 | `		pReq->zX509Exts = SslConfValue(pReq,"req","x509_extensions");` |
|   31 |  818 | `		z = SslConfValue(pReq,"req","default_bits");` |
|   31 |  819 | `		if( z ){` |
|   31 |  820 | `			pReq->iBits = (sxi64)SyStrToInt32(z,(sxu32)SyStrlen(z),0,0);` |
|   31 |  821 | `			if( pReq->iBits < 1 ){` |
|   31 |  822 | `				pReq->iBits = 2048;` |
|   15 |  823 | `			}` |
|   15 |  824 | `		}` |
|   31 |  825 | `		ERR_clear_error();` |
|   15 |  826 | `	}` |
|   31 |  827 | `	if( pOpts == 0 \|\| !ph7_value_is_array(pOpts) ){` |
|    3 |  828 | `		return;` |
|    - |  829 | `	}` |
|   29 |  830 | `	pVal = ph7_array_fetch(pOpts,"digest_alg",-1);` |
|   29 |  831 | `	if( pVal ){` |
|   29 |  832 | `		pReq->pDigest = PH7_SslDigestOfValue(pVal,&pReq->pFetched);` |
|   14 |  833 | `	}` |
|   29 |  834 | `	pVal = ph7_array_fetch(pOpts,"private_key_bits",-1);` |
|   29 |  835 | `	if( pVal ){` |
|  ! 0 |  836 | `		pReq->iBits = ph7_value_to_int64(pVal);` |
|  ! 0 |  837 | `	}` |
|   29 |  838 | `	pVal = ph7_array_fetch(pOpts,"private_key_type",-1);` |
|   29 |  839 | `	if( pVal ){` |
|  ! 0 |  840 | `		pReq->iType = ph7_value_to_int64(pVal);` |
|  ! 0 |  841 | `	}` |
|   29 |  842 | `	pVal = ph7_array_fetch(pOpts,"curve_name",-1);` |
|   29 |  843 | `	if( pVal ){` |
|  ! 0 |  844 | `		pReq->zCurve = ph7_value_to_string(pVal,&pReq->nCurve);` |
|  ! 0 |  845 | `	}` |
|   29 |  846 | `	pVal = ph7_array_fetch(pOpts,"encrypt_key",-1);` |
|   29 |  847 | `	if( pVal ){` |
|  ! 0 |  848 | `		pReq->bEncryptKey = ph7_value_to_bool(pVal);` |
|  ! 0 |  849 | `	}` |
|   29 |  850 | `	pVal = ph7_array_fetch(pOpts,"req_extensions",-1);` |
|   29 |  851 | `	if( pVal && ph7_value_is_string(pVal) ){` |
|  ! 0 |  852 | `		int n = 0;` |
|  ! 0 |  853 | `		pReq->zReqExts = ph7_value_to_string(pVal,&n);` |
|  ! 0 |  854 | `	}` |
|   29 |  855 | `	pVal = ph7_array_fetch(pOpts,"x509_extensions",-1);` |
|   29 |  856 | `	if( pVal && ph7_value_is_string(pVal) ){` |
|  ! 0 |  857 | `		int n = 0;` |
|  ! 0 |  858 | `		pReq->zX509Exts = ph7_value_to_string(pVal,&n);` |
|  ! 0 |  859 | `	}` |
|   16 |  860 | `}` |
|   30 |  861 | `static void SslReqConfRelease(SslReqConf *pReq)` |
|    1 |  862 | `{` |
|   31 |  863 | `	if( pReq->pFetched ){` |
|  ! 0 |  864 | `		EVP_MD_free(pReq->pFetched);` |
|  ! 0 |  865 | `		pReq->pFetched = 0;` |
|  ! 0 |  866 | `	}` |
|   31 |  867 | `	if( pReq->pConf ){` |
|   31 |  868 | `		NCONF_free(pReq->pConf);` |
|   31 |  869 | `		pReq->pConf = 0;` |
|   15 |  870 | `	}` |
|   31 |  871 | `}` |
|    - |  872 | `/*` |
|    - |  873 | ` * The SUBJECT php builds: the caller's fields first, in their own order, then` |
|    - |  874 | `` * every `<name>_default` the config declares for a field the caller did NOT`` |
|    - |  875 | `` * give. A name the object table has never heard of is `dn: %s is not a`` |
|    - |  876 | `` * recognized name` and is skipped rather than fatal.`` |
|    - |  877 | ` */` |
|   14 |  878 | `static int SslBuildSubject(ph7_context *pCtx,X509_NAME *pName,ph7_value *pDn,SslReqConf *pReq)` |
|    1 |  879 | `{` |
|    - |  880 | `	ph7_hashmap *pMap;` |
|    - |  881 | `	ph7_hashmap_node *pEntry;` |
|    - |  882 | `	sxu32 i;` |
|   15 |  883 | `	if( pDn == 0 \|\| !ph7_value_is_array(pDn) ){` |
|  ! 0 |  884 | `		return 0;` |
|    - |  885 | `	}` |
|   15 |  886 | `	pMap = (ph7_hashmap *)pDn->x.pOther;` |
|   15 |  887 | `	pEntry = pMap->pFirst;` |
|   37 |  888 | `	for( i = 0 ; i < pMap->nEntry ; ++i ){` |
|    - |  889 | `		ph7_value sKey;` |
|   23 |  890 | `		ph7_value *pData = HashmapExtractNodeValue(pEntry);` |
|    - |  891 | `		const char *zKey,*zVal;` |
|   23 |  892 | `		int nKey = 0,nVal = 0,nid;` |
|    - |  893 | `		char zName[128];` |
|   23 |  894 | `		PH7_MemObjInit(pCtx->pVm,&sKey);` |
|   23 |  895 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|   23 |  896 | `		pEntry = pEntry->pPrev;` |
|   23 |  897 | `		zKey = ph7_value_to_string(&sKey,&nKey);` |
|   23 |  898 | `		if( zKey == 0 \|\| nKey < 1 \|\| nKey >= (int)sizeof(zName) \|\| pData == 0 ){` |
|  ! 0 |  899 | `			PH7_MemObjRelease(&sKey);` |
|  ! 0 |  900 | `			continue;` |
|    - |  901 | `		}` |
|   23 |  902 | `		SyMemcpy(zKey,zName,(sxu32)nKey);` |
|   23 |  903 | `		zName[nKey] = 0;` |
|   23 |  904 | `		PH7_MemObjRelease(&sKey);` |
|   23 |  905 | `		nid = OBJ_txt2nid(zName);` |
|   23 |  906 | `		if( nid == NID_undef ){` |
|    4 |  907 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    1 |  908 | `				"dn: %s is not a recognized name",zName);` |
|    3 |  909 | `			continue;` |
|    - |  910 | `		}` |
|   21 |  911 | `		zVal = ph7_value_to_string(pData,&nVal);` |
|   21 |  912 | `		if( zVal == 0 ){` |
|  ! 0 |  913 | `			zVal = "";` |
|  ! 0 |  914 | `			nVal = 0;` |
|  ! 0 |  915 | `		}` |
|   30 |  916 | `		if( X509_NAME_add_entry_by_NID(pName,nid,MBSTRING_UTF8,` |
|   21 |  917 | `				(const unsigned char *)zVal,nVal,-1,0) != 1 ){` |
|  ! 0 |  918 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - |  919 | `				"dn: add_entry_by_NID %d -> %s (failed; check error queue and value of string_mask OpenSSL option if illegal characters are reported)",` |
|  ! 0 |  920 | `				nid,zVal);` |
|  ! 0 |  921 | `			return -1;` |
|    - |  922 | `		}` |
|   11 |  923 | `	}` |
|    - |  924 | `	/* Then the config's own defaults, for every field the caller left out. */` |
|   15 |  925 | `	if( pReq->pConf ){` |
|    - |  926 | `		STACK_OF(CONF_VALUE) *pSec;` |
|    - |  927 | `		/* A missing section or key pushes an error of its own. Mark and pop` |
|    - |  928 | `		 * around the whole walk rather than clearing the queue: the caller's` |
|    - |  929 | ``		 * `dn: %s is not a recognized name` left an `unknown object name` in`` |
|    - |  930 | `		 * it a moment ago, and php hands that one to openssl_error_string(). */` |
|   15 |  931 | `		ERR_set_mark();` |
|   15 |  932 | `		pSec = NCONF_get_section(pReq->pConf,pReq->zDnSection);` |
|   15 |  933 | `		int n = pSec ? sk_CONF_VALUE_num(pSec) : 0;` |
|    - |  934 | `		int j;` |
|  211 |  935 | `		for( j = 0 ; j < n ; ++j ){` |
|  197 |  936 | `			CONF_VALUE *pV = sk_CONF_VALUE_value(pSec,j);` |
|  197 |  937 | `			const char *zField = pV->name;` |
|    - |  938 | `			const char *zDot;` |
|    - |  939 | `			char zBuf[192];` |
|    - |  940 | `			const char *zDefault;` |
|    - |  941 | `			int nid;` |
|    - |  942 | `			/* The config numbers repeatable fields ("0.organizationName"); the` |
|    - |  943 | `			 * NAME is what follows the dot. */` |
|    - |  944 | `			{` |
|  197 |  945 | `				sxu32 nPos = 0;` |
|  197 |  946 | `				zDot = SyByteFind(zField,(sxu32)SyStrlen(zField),'.',&nPos) == SXRET_OK` |
|  112 |  947 | `					? zField + nPos : 0;` |
|    - |  948 | `			}` |
|  197 |  949 | `			if( zDot ){` |
|   29 |  950 | `				zField = zDot + 1;` |
|   14 |  951 | `			}` |
|    - |  952 | `			{` |
|  197 |  953 | `				sxu32 nField = (sxu32)SyStrlen(pV->name);` |
|  197 |  954 | `				if( nField + 9 >= sizeof(zBuf) ){` |
|  ! 0 |  955 | `					continue;` |
|    - |  956 | `				}` |
|  197 |  957 | `				if( nField > 8 && SyStrncmp(pV->name + nField - 8,"_default",8) == 0 ){` |
|   43 |  958 | `					continue;   /* the default row itself, not a field */` |
|    - |  959 | `				}` |
|    - |  960 | `			}` |
|  155 |  961 | `			nid = OBJ_txt2nid(zField);` |
|  155 |  962 | `			if( nid == NID_undef ){` |
|   57 |  963 | `				continue;` |
|    - |  964 | `			}` |
|   99 |  965 | `			if( X509_NAME_get_index_by_NID(pName,nid,-1) >= 0 ){` |
|   21 |  966 | `				continue;   /* the caller gave it */` |
|    - |  967 | `			}` |
|   79 |  968 | `			SyBufferFormat(zBuf,sizeof(zBuf),"%s_default",pV->name);` |
|   79 |  969 | `			zDefault = NCONF_get_string(pReq->pConf,pReq->zDnSection,zBuf);` |
|   79 |  970 | `			if( zDefault == 0 \|\| zDefault[0] == 0 ){` |
|   41 |  971 | `				continue;` |
|    - |  972 | `			}` |
|   58 |  973 | `			X509_NAME_add_entry_by_NID(pName,nid,MBSTRING_UTF8,` |
|   19 |  974 | `				(const unsigned char *)zDefault,-1,-1,0);` |
|   20 |  975 | `		}` |
|   15 |  976 | `		ERR_pop_to_mark();` |
|    7 |  977 | `	}` |
|   15 |  978 | `	return 0;` |
|    8 |  979 | `}` |
|    - |  980 |  |
|    - |  981 | `/* ------------------------------------------------------------------------` |
|    - |  982 | ` * Signing requests` |
|    - |  983 | ` * ------------------------------------------------------------------------ */` |
|   14 |  984 | `static int vm_builtin_openssl_csr_new(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 |  985 | `{` |
|    - |  986 | `	SslReqConf sReq;` |
|   15 |  987 | `	X509_REQ *pCsr = 0;` |
|    - |  988 | `	X509_NAME *pName;` |
|   15 |  989 | `	EVP_PKEY *pKey = 0;` |
|   15 |  990 | `	int bOwnKey = 0,rc = -1;` |
|   15 |  991 | `	if( nArg < 2 ){` |
|  ! 0 |  992 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  993 | `		return PH7_OK;` |
|    - |  994 | `	}` |
|   15 |  995 | `	SslReqConfLoad(pCtx,nArg > 2 ? apArg[2] : 0,&sReq);` |
|   15 |  996 | `	if( ph7_value_is_null(apArg[1]) ){` |
|    - |  997 | `		/* php GENERATES the key when the by-reference argument is null, and` |
|    - |  998 | `` 		 * writes it back -- which is the only way `openssl_csr_new($dn, $k)` `` |
|    - |  999 | `		 * on an unset $k can work at all. */` |
|  ! 0 | 1000 | `		pKey = PH7_SslGenerateKey(pCtx,sReq.iBits,sReq.iType,sReq.zCurve,sReq.nCurve);` |
|  ! 0 | 1001 | `		if( pKey == 0 ){` |
|  ! 0 | 1002 | `			goto done;` |
|    - | 1003 | `		}` |
|    - | 1004 | `		/* The object OWNS the key from here: the by-ref write-back copies the` |
|    - | 1005 | `		 * value and takes a reference of its own, so the caller's $key stays` |
|    - | 1006 | `		 * alive after this call returns and nothing frees the EVP_PKEY twice. */` |
|  ! 0 | 1007 | `		PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);` |
|  ! 0 | 1008 | `		if( !ph7_value_is_object(pCtx->pRet) ){` |
|  ! 0 | 1009 | `			pKey = 0;` |
|  ! 0 | 1010 | `			goto done;` |
|    - | 1011 | `		}` |
|  ! 0 | 1012 | `		pKey = (EVP_PKEY *)PH7_SslHandleOf(pCtx->pRet,PHL_SSL_KIND_KEY);` |
|  ! 0 | 1013 | `		if( pKey == 0 ){` |
|  ! 0 | 1014 | `			goto done;` |
|    - | 1015 | `		}` |
|  ! 0 | 1016 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pCtx->pRet);` |
|  ! 0 | 1017 | `	}else{` |
|   15 | 1018 | `		pKey = PH7_SslKeyOfValue(pCtx,apArg[1],0,&bOwnKey);` |
|   15 | 1019 | `		if( bOwnKey < 0 ){` |
|  ! 0 | 1020 | `			SslReqConfRelease(&sReq);` |
|  ! 0 | 1021 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1022 | `			return PH7_SslArrayShapeError(pCtx);` |
|    - | 1023 | `		}` |
|   15 | 1024 | `		if( pKey == 0 ){` |
|  ! 0 | 1025 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1026 | `				"Unable to coerce parameter 2 into a private key");` |
|  ! 0 | 1027 | `			goto done;` |
|    - | 1028 | `		}` |
|    - | 1029 | `	}` |
|   15 | 1030 | `	pCsr = X509_REQ_new();` |
|   15 | 1031 | `	if( pCsr == 0 ){` |
|  ! 0 | 1032 | `		goto done;` |
|    - | 1033 | `	}` |
|   15 | 1034 | `	X509_REQ_set_version(pCsr,0);` |
|   15 | 1035 | `	pName = X509_REQ_get_subject_name(pCsr);` |
|   15 | 1036 | `	if( SslBuildSubject(pCtx,pName,apArg[0],&sReq) != 0 ){` |
|  ! 0 | 1037 | `		goto done;` |
|    - | 1038 | `	}` |
|   15 | 1039 | `	if( X509_REQ_set_pubkey(pCsr,pKey) != 1 ){` |
|  ! 0 | 1040 | `		goto done;` |
|    - | 1041 | `	}` |
|   15 | 1042 | `	if( sReq.zReqExts && sReq.pConf ){` |
|    - | 1043 | `		X509V3_CTX sExtCtx;` |
|  ! 0 | 1044 | `		X509V3_set_ctx(&sExtCtx,0,0,pCsr,0,0);` |
|  ! 0 | 1045 | `		X509V3_set_nconf(&sExtCtx,sReq.pConf);` |
|  ! 0 | 1046 | `		if( !X509V3_EXT_REQ_add_nconf(sReq.pConf,&sExtCtx,(char *)sReq.zReqExts,pCsr) ){` |
|  ! 0 | 1047 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 | 1048 | `				"Error loading extension section %s",sReq.zReqExts);` |
|  ! 0 | 1049 | `			goto done;` |
|    - | 1050 | `		}` |
|  ! 0 | 1051 | `	}` |
|   15 | 1052 | `	if( X509_REQ_sign(pCsr,pKey,sReq.pDigest ? sReq.pDigest : EVP_sha256()) <= 0 ){` |
|  ! 0 | 1053 | `		goto done;` |
|    - | 1054 | `	}` |
|   15 | 1055 | `	rc = 0;` |
|    7 | 1056 | `done:` |
|   15 | 1057 | `	if( bOwnKey > 0 && pKey ){` |
|  ! 0 | 1058 | `		EVP_PKEY_free(pKey);` |
|  ! 0 | 1059 | `	}` |
|   15 | 1060 | `	SslReqConfRelease(&sReq);` |
|   15 | 1061 | `	if( rc != 0 ){` |
|  ! 0 | 1062 | `		if( pCsr ){ X509_REQ_free(pCsr); }` |
|  ! 0 | 1063 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|  ! 0 | 1064 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1065 | `		return PH7_OK;` |
|    - | 1066 | `	}` |
|   15 | 1067 | `	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_CSR,(void *)pCsr);` |
|    8 | 1068 | `}` |
|    8 | 1069 | `static int SslCsrExport(ph7_context *pCtx,int nArg,ph7_value **apArg,int bToFile)` |
|    1 | 1070 | `{` |
|    - | 1071 | `	X509_REQ *pCsr;` |
|    - | 1072 | `	BIO *pBio;` |
|    9 | 1073 | `	char *zMem = 0;` |
|    - | 1074 | `	long nMem;` |
|    9 | 1075 | `	int bOwn = 0,bNoText = 1,rc = 0;` |
|    9 | 1076 | `	if( nArg < 2 ){` |
|  ! 0 | 1077 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1078 | `		return PH7_OK;` |
|    - | 1079 | `	}` |
|    9 | 1080 | `	if( nArg > 2 ){` |
|    3 | 1081 | `		bNoText = ph7_value_to_bool(apArg[2]);` |
|    1 | 1082 | `	}` |
|    9 | 1083 | `	pCsr = SslCsrOfValue(pCtx,apArg[0],&bOwn,1);` |
|    9 | 1084 | `	if( pCsr == 0 ){` |
|    3 | 1085 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1086 | `		return PH7_OK;` |
|    - | 1087 | `	}` |
|    7 | 1088 | `	pBio = BIO_new(BIO_s_mem());` |
|    7 | 1089 | `	if( pBio == 0 ){` |
|  ! 0 | 1090 | `		SslFreeCsr(pCsr,bOwn);` |
|  ! 0 | 1091 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1092 | `		return PH7_OK;` |
|    - | 1093 | `	}` |
|    7 | 1094 | `	if( !bNoText ){` |
|    3 | 1095 | `		X509_REQ_print(pBio,pCsr);` |
|    1 | 1096 | `	}` |
|    7 | 1097 | `	if( PEM_write_bio_X509_REQ(pBio,pCsr) != 1 ){` |
|  ! 0 | 1098 | `		rc = -1;` |
|  ! 0 | 1099 | `	}` |
|    7 | 1100 | `	if( rc == 0 ){` |
|    7 | 1101 | `		nMem = BIO_get_mem_data(pBio,&zMem);` |
|    7 | 1102 | `		if( bToFile ){` |
|    3 | 1103 | `			int nPath = 0;` |
|    3 | 1104 | `			const char *zPath = ph7_value_to_string(apArg[1],&nPath);` |
|    3 | 1105 | `			rc = PH7_SslWriteFileArg(pCtx,zPath,nPath,zMem,(sxu32)nMem);` |
|    3 | 1106 | `			if( rc != 0 ){` |
|  ! 0 | 1107 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 | 1108 | `					"Error opening file %.*s",nPath,zPath ? zPath : "");` |
|  ! 0 | 1109 | `			}` |
|    2 | 1110 | `		}else{` |
|    5 | 1111 | `			ph7_value *pRes = ph7_context_new_scalar(pCtx);` |
|    5 | 1112 | `			if( pRes ){` |
|    5 | 1113 | `				ph7_value_string(pRes,zMem,(int)nMem);` |
|    5 | 1114 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);` |
|    5 | 1115 | `				ph7_context_release_value(pCtx,pRes);` |
|    2 | 1116 | `			}` |
|    - | 1117 | `		}` |
|    3 | 1118 | `	}` |
|    7 | 1119 | `	BIO_free(pBio);` |
|    7 | 1120 | `	SslFreeCsr(pCsr,bOwn);` |
|    7 | 1121 | `	if( rc != 0 ){` |
|  ! 0 | 1122 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|  ! 0 | 1123 | `	}` |
|    7 | 1124 | `	ph7_result_bool(pCtx,rc == 0);` |
|    7 | 1125 | `	return PH7_OK;` |
|    5 | 1126 | `}` |
|    6 | 1127 | `static int vm_builtin_openssl_csr_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1128 | `{` |
|    7 | 1129 | `	return SslCsrExport(pCtx,nArg,apArg,0);` |
|    1 | 1130 | `}` |
|    2 | 1131 | `static int vm_builtin_openssl_csr_export_to_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1132 | `{` |
|    3 | 1133 | `	return SslCsrExport(pCtx,nArg,apArg,1);` |
|    1 | 1134 | `}` |
|   10 | 1135 | `static int vm_builtin_openssl_csr_get_subject(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1136 | `{` |
|    - | 1137 | `	X509_REQ *pCsr;` |
|    - | 1138 | `	ph7_value *pArray;` |
|   11 | 1139 | `	int bOwn = 0,bShort = 1;` |
|   11 | 1140 | `	if( nArg < 1 ){` |
|  ! 0 | 1141 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1142 | `		return PH7_OK;` |
|    - | 1143 | `	}` |
|   11 | 1144 | `	if( nArg > 1 ){` |
|    3 | 1145 | `		bShort = ph7_value_to_bool(apArg[1]);` |
|    1 | 1146 | `	}` |
|   11 | 1147 | `	pCsr = SslCsrOfValue(pCtx,apArg[0],&bOwn,0);` |
|   11 | 1148 | `	if( pCsr == 0 ){` |
|    3 | 1149 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1150 | `		return PH7_OK;` |
|    - | 1151 | `	}` |
|    9 | 1152 | `	pArray = ph7_context_new_array(pCtx);` |
|    9 | 1153 | `	if( pArray ){` |
|    9 | 1154 | `		SslNameToArray(pCtx,X509_REQ_get_subject_name(pCsr),bShort,pArray);` |
|    9 | 1155 | `		ph7_result_value(pCtx,pArray);` |
|    5 | 1156 | `	}else{` |
|  ! 0 | 1157 | `		ph7_result_bool(pCtx,0);` |
|    - | 1158 | `	}` |
|    9 | 1159 | `	SslFreeCsr(pCsr,bOwn);` |
|    9 | 1160 | `	return PH7_OK;` |
|    6 | 1161 | `}` |
|    4 | 1162 | `static int vm_builtin_openssl_csr_get_public_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1163 | `{` |
|    - | 1164 | `	X509_REQ *pCsr;` |
|    - | 1165 | `	EVP_PKEY *pKey;` |
|    5 | 1166 | `	int bOwn = 0;` |
|    5 | 1167 | `	if( nArg < 1 ){` |
|  ! 0 | 1168 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1169 | `		return PH7_OK;` |
|    - | 1170 | `	}` |
|    5 | 1171 | `	pCsr = SslCsrOfValue(pCtx,apArg[0],&bOwn,0);` |
|    5 | 1172 | `	if( pCsr == 0 ){` |
|    3 | 1173 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1174 | `		return PH7_OK;` |
|    - | 1175 | `	}` |
|    3 | 1176 | `	pKey = X509_REQ_get_pubkey(pCsr);` |
|    3 | 1177 | `	SslFreeCsr(pCsr,bOwn);` |
|    3 | 1178 | `	if( pKey == 0 ){` |
|  ! 0 | 1179 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|  ! 0 | 1180 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1181 | `		return PH7_OK;` |
|    - | 1182 | `	}` |
|    3 | 1183 | `	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_KEY,(void *)pKey);` |
|    3 | 1184 | `}` |
|    - | 1185 | `/*` |
|    - | 1186 | ` * openssl_csr_sign(): the request, a CA certificate (or null for a` |
|    - | 1187 | ` * self-signed one), the CA's private key and a validity in DAYS. The serial` |
|    - | 1188 | ` * may be given as an integer or -- since php 8.3 -- as a HEX string, which is` |
|    - | 1189 | ` * the only way to express one that does not fit in an int.` |
|    - | 1190 | ` */` |
|   16 | 1191 | `static int vm_builtin_openssl_csr_sign(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1192 | `{` |
|    - | 1193 | `	SslReqConf sReq;` |
|   17 | 1194 | `	X509_REQ *pCsr = 0;` |
|   17 | 1195 | `	X509 *pCa = 0,*pOut = 0;` |
|   17 | 1196 | `	EVP_PKEY *pKey = 0,*pPub = 0;` |
|   17 | 1197 | `	ASN1_INTEGER *pSerial = 0;` |
|   17 | 1198 | `	int bOwnCsr = 0,bOwnCa = 0,bOwnKey = 0,rc = -1;` |
|    - | 1199 | `	sxi64 iDays;` |
|   17 | 1200 | `	if( nArg < 4 ){` |
|  ! 0 | 1201 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1202 | `		return PH7_OK;` |
|    - | 1203 | `	}` |
|   17 | 1204 | `	SslReqConfLoad(pCtx,nArg > 4 ? apArg[4] : 0,&sReq);` |
|   17 | 1205 | `	iDays = ph7_value_to_int64(apArg[3]);` |
|   17 | 1206 | `	pCsr = SslCsrOfValue(pCtx,apArg[0],&bOwnCsr,1);` |
|   17 | 1207 | `	if( pCsr == 0 ){` |
|    3 | 1208 | `		goto done;` |
|    - | 1209 | `	}` |
|   15 | 1210 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|  ! 0 | 1211 | `		pCa = SslCertOfValue(pCtx,apArg[1],&bOwnCa,1);` |
|  ! 0 | 1212 | `		if( pCa == 0 ){` |
|  ! 0 | 1213 | `			goto done;` |
|    - | 1214 | `		}` |
|  ! 0 | 1215 | `	}` |
|   15 | 1216 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],0,&bOwnKey);` |
|   15 | 1217 | `	if( bOwnKey < 0 ){` |
|  ! 0 | 1218 | `		SslReqConfRelease(&sReq);` |
|  ! 0 | 1219 | `		SslFreeCsr(pCsr,bOwnCsr);` |
|  ! 0 | 1220 | `		SslFreeCert(pCa,bOwnCa);` |
|  ! 0 | 1221 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1222 | `		return PH7_SslArrayShapeError(pCtx);` |
|    - | 1223 | `	}` |
|   15 | 1224 | `	if( pKey == 0 ){` |
|  ! 0 | 1225 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1226 | `			"cannot get private key from parameter 3");` |
|  ! 0 | 1227 | `		goto done;` |
|    - | 1228 | `	}` |
|   15 | 1229 | `	if( pCa && X509_check_private_key(pCa,pKey) != 1 ){` |
|  ! 0 | 1230 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1231 | `			"private key does not correspond to signing cert");` |
|  ! 0 | 1232 | `		goto done;` |
|    - | 1233 | `	}` |
|   15 | 1234 | `	pPub = X509_REQ_get_pubkey(pCsr);` |
|   15 | 1235 | `	if( pPub == 0 \|\| X509_REQ_verify(pCsr,pPub) != 1 ){` |
|  ! 0 | 1236 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1237 | `			"Signature verification problem");` |
|  ! 0 | 1238 | `		goto done;` |
|    - | 1239 | `	}` |
|   15 | 1240 | `	pOut = X509_new();` |
|   15 | 1241 | `	if( pOut == 0 ){` |
|  ! 0 | 1242 | `		goto done;` |
|    - | 1243 | `	}` |
|   15 | 1244 | `	pSerial = ASN1_INTEGER_new();` |
|   15 | 1245 | `	if( pSerial == 0 ){` |
|  ! 0 | 1246 | `		goto done;` |
|    - | 1247 | `	}` |
|   16 | 1248 | `	if( nArg > 6 && !ph7_value_is_null(apArg[6]) ){` |
|    3 | 1249 | `		int nHex = 0;` |
|    3 | 1250 | `		const char *zHex = ph7_value_to_string(apArg[6],&nHex);` |
|    3 | 1251 | `		BIGNUM *pBn = 0;` |
|    3 | 1252 | `		char *zZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nHex + 1));` |
|    3 | 1253 | `		if( zZ == 0 ){` |
|  ! 0 | 1254 | `			goto done;` |
|    - | 1255 | `		}` |
|    3 | 1256 | `		SyMemcpy(zHex,zZ,(sxu32)nHex);` |
|    3 | 1257 | `		zZ[nHex] = 0;` |
|    3 | 1258 | `		if( BN_hex2bn(&pBn,zZ) == 0 \|\| pBn == 0 ){` |
|  ! 0 | 1259 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);` |
|  ! 0 | 1260 | `			if( pBn ){ BN_free(pBn); }` |
|  ! 0 | 1261 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1262 | `			SslReqConfRelease(&sReq);` |
|  ! 0 | 1263 | `			SslFreeCsr(pCsr,bOwnCsr);` |
|  ! 0 | 1264 | `			SslFreeCert(pCa,bOwnCa);` |
|  ! 0 | 1265 | `			if( bOwnKey > 0 ){ EVP_PKEY_free(pKey); }` |
|  ! 0 | 1266 | `			if( pPub ){ EVP_PKEY_free(pPub); }` |
|  ! 0 | 1267 | `			if( pOut ){ X509_free(pOut); }` |
|  ! 0 | 1268 | `			ASN1_INTEGER_free(pSerial);` |
|  ! 0 | 1269 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1270 | `				"%s(): Argument #7 ($serial_hex) must be a valid hexadecimal string",` |
|  ! 0 | 1271 | `				ph7_function_name(pCtx));` |
|    - | 1272 | `		}` |
|    3 | 1273 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);` |
|    3 | 1274 | `		BN_to_ASN1_INTEGER(pBn,pSerial);` |
|    3 | 1275 | `		BN_free(pBn);` |
|    2 | 1276 | `	}else{` |
|   13 | 1277 | `		ASN1_INTEGER_set_int64(pSerial,nArg > 5 ? ph7_value_to_int64(apArg[5]) : 0);` |
|    - | 1278 | `	}` |
|   15 | 1279 | `	X509_set_version(pOut,2);` |
|   15 | 1280 | `	X509_set_serialNumber(pOut,pSerial);` |
|   15 | 1281 | `	X509_set_subject_name(pOut,X509_REQ_get_subject_name(pCsr));` |
|   15 | 1282 | `	X509_set_issuer_name(pOut,pCa ? X509_get_subject_name(pCa) : X509_REQ_get_subject_name(pCsr));` |
|   15 | 1283 | `	X509_gmtime_adj(X509_getm_notBefore(pOut),0);` |
|   15 | 1284 | `	X509_gmtime_adj(X509_getm_notAfter(pOut),(long)(iDays * 24 * 60 * 60));` |
|   15 | 1285 | `	X509_set_pubkey(pOut,pPub);` |
|   15 | 1286 | `	if( sReq.zX509Exts && sReq.pConf ){` |
|    - | 1287 | `		X509V3_CTX sExtCtx;` |
|   15 | 1288 | `		X509V3_set_ctx(&sExtCtx,pCa ? pCa : pOut,pOut,pCsr,0,0);` |
|   15 | 1289 | `		X509V3_set_nconf(&sExtCtx,sReq.pConf);` |
|   15 | 1290 | `		if( !X509V3_EXT_add_nconf(sReq.pConf,&sExtCtx,(char *)sReq.zX509Exts,pOut) ){` |
|  ! 0 | 1291 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 | 1292 | `				"Error loading extension section %s",sReq.zX509Exts);` |
|  ! 0 | 1293 | `			goto done;` |
|    - | 1294 | `		}` |
|    7 | 1295 | `	}` |
|   15 | 1296 | `	if( X509_sign(pOut,pKey,sReq.pDigest ? sReq.pDigest : EVP_sha256()) <= 0 ){` |
|  ! 0 | 1297 | `		goto done;` |
|    - | 1298 | `	}` |
|   15 | 1299 | `	rc = 0;` |
|    8 | 1300 | `done:` |
|   17 | 1301 | `	if( pSerial ){ ASN1_INTEGER_free(pSerial); }` |
|   17 | 1302 | `	if( pPub ){ EVP_PKEY_free(pPub); }` |
|   17 | 1303 | `	if( bOwnKey > 0 && pKey ){ EVP_PKEY_free(pKey); }` |
|   17 | 1304 | `	SslFreeCert(pCa,bOwnCa);` |
|   17 | 1305 | `	SslFreeCsr(pCsr,bOwnCsr);` |
|   17 | 1306 | `	SslReqConfRelease(&sReq);` |
|   17 | 1307 | `	if( rc != 0 ){` |
|    3 | 1308 | `		if( pOut ){ X509_free(pOut); }` |
|    3 | 1309 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    3 | 1310 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1311 | `		return PH7_OK;` |
|    - | 1312 | `	}` |
|   15 | 1313 | `	return PH7_SslResultObject(pCtx,PHL_SSL_KIND_CERT,(void *)pOut);` |
|    9 | 1314 | `}` |
|    - | 1315 |  |
|    - | 1316 | `/* ------------------------------------------------------------------------` |
|    - | 1317 | ` * PKCS#12` |
|    - | 1318 | ` * ------------------------------------------------------------------------ */` |
|    - | 1319 | `/*` |
|    - | 1320 | ` * A bundle is the certificate, its key and any chain that came with it. php's` |
|    - | 1321 | `` * read gives back `cert` and `pkey` as PEM and `extracerts` as a LIST of PEM`` |
|    - | 1322 | ` * -- and the last key is ABSENT rather than empty when the bundle carries no` |
|    - | 1323 | `` * chain, which is why a caller has to `??` it.`` |
|    - | 1324 | ` */` |
|    8 | 1325 | `static int SslPkcs12Export(ph7_context *pCtx,int nArg,ph7_value **apArg,int bToFile)` |
|    1 | 1326 | `{` |
|    - | 1327 | `	X509 *pCert;` |
|    - | 1328 | `	EVP_PKEY *pKey;` |
|    9 | 1329 | `	PKCS12 *pP12 = 0;` |
|    9 | 1330 | `	BIO *pBio = 0;` |
|    9 | 1331 | `	STACK_OF(X509) *pChain = 0;` |
|    9 | 1332 | `	const char *zPass,*zFriendly = 0;` |
|    9 | 1333 | `	char *zPassZ = 0,*zFriendlyZ = 0;` |
|    9 | 1334 | `	int nPass = 0,nFriendly = 0,bOwnCert = 0,bOwnKey = 0,rc = -1;` |
|    9 | 1335 | `	ph7_value *pOpts = 0,*pVal;` |
|    9 | 1336 | `	if( nArg < 4 ){` |
|  ! 0 | 1337 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1338 | `		return PH7_OK;` |
|    - | 1339 | `	}` |
|    9 | 1340 | `	zPass = ph7_value_to_string(apArg[3],&nPass);` |
|    9 | 1341 | `	pCert = SslCertOfValue(pCtx,apArg[0],&bOwnCert,1);` |
|    9 | 1342 | `	if( pCert == 0 ){` |
|    3 | 1343 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1344 | `		return PH7_OK;` |
|    - | 1345 | `	}` |
|    7 | 1346 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[2],0,&bOwnKey);` |
|    7 | 1347 | `	if( bOwnKey < 0 ){` |
|  ! 0 | 1348 | `		SslFreeCert(pCert,bOwnCert);` |
|  ! 0 | 1349 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1350 | `		return PH7_SslArrayShapeError(pCtx);` |
|    - | 1351 | `	}` |
|    7 | 1352 | `	if( pKey == 0 ){` |
|  ! 0 | 1353 | `		SslFreeCert(pCert,bOwnCert);` |
|  ! 0 | 1354 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"cannot get private key from parameter 3");` |
|  ! 0 | 1355 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1356 | `		return PH7_OK;` |
|    - | 1357 | `	}` |
|    7 | 1358 | `	if( X509_check_private_key(pCert,pKey) != 1 ){` |
|  ! 0 | 1359 | `		ERR_clear_error();` |
|  ! 0 | 1360 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 1361 | `			"private key does not correspond to cert");` |
|  ! 0 | 1362 | `		goto done;` |
|    - | 1363 | `	}` |
|    7 | 1364 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|    3 | 1365 | `		pOpts = apArg[4];` |
|    1 | 1366 | `	}` |
|    7 | 1367 | `	if( pOpts ){` |
|    3 | 1368 | `		pVal = ph7_array_fetch(pOpts,"friendly_name",-1);` |
|    3 | 1369 | `		if( pVal && ph7_value_is_string(pVal) ){` |
|    3 | 1370 | `			zFriendly = ph7_value_to_string(pVal,&nFriendly);` |
|    3 | 1371 | `			if( zFriendly && nFriendly > 0 ){` |
|    - | 1372 | `				/* Copied HERE rather than after the extracerts walk: that walk` |
|    - | 1373 | `				 * can reach a userland stream wrapper, and running php code` |
|    - | 1374 | ``				 * moves the memobj pool `pVal` lives in. */`` |
|    4 | 1375 | `				zFriendlyZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|    2 | 1376 | `					(sxu32)(nFriendly + 1));` |
|    3 | 1377 | `				if( zFriendlyZ ){` |
|    3 | 1378 | `					SyMemcpy(zFriendly,zFriendlyZ,(sxu32)nFriendly);` |
|    3 | 1379 | `					zFriendlyZ[nFriendly] = 0;` |
|    1 | 1380 | `				}` |
|    1 | 1381 | `			}` |
|    1 | 1382 | `		}` |
|    3 | 1383 | `		pVal = ph7_array_fetch(pOpts,"extracerts",-1);` |
|    3 | 1384 | `		if( pVal && ph7_value_is_array(pVal) ){` |
|    3 | 1385 | `			ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|    3 | 1386 | `			ph7_hashmap_node *pEntry = pMap->pFirst;` |
|    - | 1387 | `			sxu32 i;` |
|    3 | 1388 | `			pChain = sk_X509_new_null();` |
|    5 | 1389 | `			for( i = 0 ; pChain && i < pMap->nEntry ; ++i ){` |
|    3 | 1390 | `				ph7_value *pData = HashmapExtractNodeValue(pEntry);` |
|    3 | 1391 | `				int bOwnOne = 0;` |
|    3 | 1392 | `				X509 *pOne = pData ? SslCertOfValue(pCtx,pData,&bOwnOne,1) : 0;` |
|    3 | 1393 | `				pEntry = pEntry->pPrev;` |
|    3 | 1394 | `				if( pOne == 0 ){` |
|  ! 0 | 1395 | `					continue;` |
|    - | 1396 | `				}` |
|    3 | 1397 | `				if( !bOwnOne ){` |
|    3 | 1398 | `					X509_up_ref(pOne);` |
|    1 | 1399 | `				}` |
|    3 | 1400 | `				sk_X509_push(pChain,pOne);` |
|    2 | 1401 | `			}` |
|    1 | 1402 | `		}` |
|    1 | 1403 | `	}` |
|    7 | 1404 | `	zPassZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPass + 1));` |
|    7 | 1405 | `	if( zPassZ == 0 ){` |
|  ! 0 | 1406 | `		goto done;` |
|    - | 1407 | `	}` |
|    7 | 1408 | `	if( nPass > 0 ){ SyMemcpy(zPass,zPassZ,(sxu32)nPass); }` |
|    7 | 1409 | `	zPassZ[nPass] = 0;` |
|    7 | 1410 | `	pP12 = PKCS12_create(zPassZ,zFriendlyZ,pKey,pCert,pChain,0,0,0,0,0);` |
|    7 | 1411 | `	if( pP12 == 0 ){` |
|  ! 0 | 1412 | `		goto done;` |
|    - | 1413 | `	}` |
|    7 | 1414 | `	pBio = BIO_new(BIO_s_mem());` |
|    7 | 1415 | `	if( pBio == 0 \|\| i2d_PKCS12_bio(pBio,pP12) != 1 ){` |
|  ! 0 | 1416 | `		goto done;` |
|    - | 1417 | `	}` |
|    - | 1418 | `	{` |
|    7 | 1419 | `		char *zMem = 0;` |
|    7 | 1420 | `		long nMem = BIO_get_mem_data(pBio,&zMem);` |
|    7 | 1421 | `		if( bToFile ){` |
|    3 | 1422 | `			int nPath = 0;` |
|    3 | 1423 | `			const char *zPath = ph7_value_to_string(apArg[1],&nPath);` |
|    3 | 1424 | `			rc = PH7_SslWriteFileArg(pCtx,zPath,nPath,zMem,(sxu32)nMem);` |
|    2 | 1425 | `		}else{` |
|    5 | 1426 | `			ph7_value *pRes = ph7_context_new_scalar(pCtx);` |
|    5 | 1427 | `			if( pRes ){` |
|    5 | 1428 | `				ph7_value_string(pRes,zMem,(int)nMem);` |
|    5 | 1429 | `				PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pRes);` |
|    5 | 1430 | `				ph7_context_release_value(pCtx,pRes);` |
|    2 | 1431 | `			}` |
|    5 | 1432 | `			rc = 0;` |
|    - | 1433 | `		}` |
|    3 | 1434 | `	}` |
|    3 | 1435 | `done:` |
|    7 | 1436 | `	if( zPassZ ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zPassZ); }` |
|    7 | 1437 | `	if( zFriendlyZ ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zFriendlyZ); }` |
|    7 | 1438 | `	if( pBio ){ BIO_free(pBio); }` |
|    7 | 1439 | `	if( pP12 ){ PKCS12_free(pP12); }` |
|    7 | 1440 | `	if( pChain ){ sk_X509_pop_free(pChain,X509_free); }` |
|    7 | 1441 | `	if( bOwnKey > 0 ){ EVP_PKEY_free(pKey); }` |
|    7 | 1442 | `	SslFreeCert(pCert,bOwnCert);` |
|    7 | 1443 | `	if( rc != 0 ){` |
|  ! 0 | 1444 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|  ! 0 | 1445 | `	}` |
|    7 | 1446 | `	ph7_result_bool(pCtx,rc == 0);` |
|    7 | 1447 | `	return PH7_OK;` |
|    5 | 1448 | `}` |
|    6 | 1449 | `static int vm_builtin_openssl_pkcs12_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1450 | `{` |
|    7 | 1451 | `	return SslPkcs12Export(pCtx,nArg,apArg,0);` |
|    1 | 1452 | `}` |
|    2 | 1453 | `static int vm_builtin_openssl_pkcs12_export_to_file(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1454 | `{` |
|    3 | 1455 | `	return SslPkcs12Export(pCtx,nArg,apArg,1);` |
|    1 | 1456 | `}` |
|   10 | 1457 | `static void SslPemPut(ph7_context *pCtx,ph7_value *pArray,ph7_value *pVal,const char *zKey,BIO *pBio)` |
|    1 | 1458 | `{` |
|   11 | 1459 | `	char *zMem = 0;` |
|   11 | 1460 | `	long nMem = BIO_get_mem_data(pBio,&zMem);` |
|    5 | 1461 | `	SXUNUSED(pCtx);` |
|   11 | 1462 | `	ph7_value_string(pVal,zMem,(int)nMem);` |
|   11 | 1463 | `	if( zKey ){` |
|    9 | 1464 | `		ph7_array_add_strkey_elem(pArray,zKey,pVal);` |
|    5 | 1465 | `	}else{` |
|    3 | 1466 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    - | 1467 | `	}` |
|   11 | 1468 | `	ph7_value_reset_string_cursor(pVal);` |
|   11 | 1469 | `}` |
|    6 | 1470 | `static int vm_builtin_openssl_pkcs12_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1471 | `{` |
|    - | 1472 | `	SyBlob sFile;` |
|    7 | 1473 | `	PKCS12 *pP12 = 0;` |
|    7 | 1474 | `	BIO *pIn = 0,*pOut = 0;` |
|    7 | 1475 | `	EVP_PKEY *pKey = 0;` |
|    7 | 1476 | `	X509 *pCert = 0;` |
|    7 | 1477 | `	STACK_OF(X509) *pChain = 0;` |
|    7 | 1478 | `	const char *zData = 0,*zPass;` |
|    7 | 1479 | `	char *zPassZ = 0;` |
|    7 | 1480 | `	int nData = 0,nPass = 0,rc = -1;` |
|    - | 1481 | `	ph7_value *pArray,*pVal;` |
|    7 | 1482 | `	if( nArg < 3 ){` |
|  ! 0 | 1483 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1484 | `		return PH7_OK;` |
|    - | 1485 | `	}` |
|    7 | 1486 | `	zPass = ph7_value_to_string(apArg[2],&nPass);` |
|    7 | 1487 | `	SyBlobInit(&sFile,&pCtx->pVm->sAllocator);` |
|    7 | 1488 | `	if( PH7_SslBytesOfValue(pCtx,apArg[0],&sFile,&zData,&nData) != 0 \|\| nData < 1 ){` |
|  ! 0 | 1489 | `		SyBlobRelease(&sFile);` |
|  ! 0 | 1490 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1491 | `		return PH7_OK;` |
|    - | 1492 | `	}` |
|    7 | 1493 | `	pIn = BIO_new_mem_buf(zData,nData);` |
|    7 | 1494 | `	pP12 = pIn ? d2i_PKCS12_bio(pIn,0) : 0;` |
|    7 | 1495 | `	if( pP12 == 0 ){` |
|  ! 0 | 1496 | `		goto done;` |
|    - | 1497 | `	}` |
|    7 | 1498 | `	zPassZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPass + 1));` |
|    7 | 1499 | `	if( zPassZ == 0 ){` |
|  ! 0 | 1500 | `		goto done;` |
|    - | 1501 | `	}` |
|    7 | 1502 | `	if( nPass > 0 ){ SyMemcpy(zPass,zPassZ,(sxu32)nPass); }` |
|    7 | 1503 | `	zPassZ[nPass] = 0;` |
|    7 | 1504 | `	if( PKCS12_parse(pP12,zPassZ,&pKey,&pCert,&pChain) != 1 ){` |
|    3 | 1505 | `		goto done;` |
|    - | 1506 | `	}` |
|    5 | 1507 | `	pArray = ph7_context_new_array(pCtx);` |
|    5 | 1508 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    5 | 1509 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 | 1510 | `		goto done;` |
|    - | 1511 | `	}` |
|    5 | 1512 | `	pOut = BIO_new(BIO_s_mem());` |
|    5 | 1513 | `	if( pOut == 0 ){` |
|  ! 0 | 1514 | `		goto done;` |
|    - | 1515 | `	}` |
|    5 | 1516 | `	if( pCert && PEM_write_bio_X509(pOut,pCert) == 1 ){` |
|    5 | 1517 | `		SslPemPut(pCtx,pArray,pVal,"cert",pOut);` |
|    2 | 1518 | `	}` |
|    5 | 1519 | `	BIO_free(pOut);` |
|    5 | 1520 | `	pOut = BIO_new(BIO_s_mem());` |
|    5 | 1521 | `	if( pOut && pKey && PEM_write_bio_PrivateKey(pOut,pKey,0,0,0,0,0) == 1 ){` |
|    5 | 1522 | `		SslPemPut(pCtx,pArray,pVal,"pkey",pOut);` |
|    2 | 1523 | `	}` |
|    5 | 1524 | `	if( pOut ){ BIO_free(pOut); }` |
|    5 | 1525 | `	pOut = 0;` |
|    5 | 1526 | `	if( pChain && sk_X509_num(pChain) > 0 ){` |
|    3 | 1527 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|    - | 1528 | `		int i;` |
|    5 | 1529 | `		for( i = 0 ; pList && i < sk_X509_num(pChain) ; ++i ){` |
|    3 | 1530 | `			BIO *pOne = BIO_new(BIO_s_mem());` |
|    3 | 1531 | `			if( pOne == 0 ){` |
|  ! 0 | 1532 | `				continue;` |
|    - | 1533 | `			}` |
|    3 | 1534 | `			if( PEM_write_bio_X509(pOne,sk_X509_value(pChain,i)) == 1 ){` |
|    3 | 1535 | `				SslPemPut(pCtx,pList,pVal,0,pOne);` |
|    1 | 1536 | `			}` |
|    3 | 1537 | `			BIO_free(pOne);` |
|    2 | 1538 | `		}` |
|    3 | 1539 | `		if( pList ){` |
|    3 | 1540 | `			ph7_array_add_strkey_elem(pArray,"extracerts",pList);` |
|    1 | 1541 | `		}` |
|    1 | 1542 | `	}` |
|    5 | 1543 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);` |
|    5 | 1544 | `	rc = 0;` |
|    3 | 1545 | `done:` |
|    7 | 1546 | `	if( pOut ){ BIO_free(pOut); }` |
|    7 | 1547 | `	if( pIn ){ BIO_free(pIn); }` |
|    7 | 1548 | `	if( pP12 ){ PKCS12_free(pP12); }` |
|    7 | 1549 | `	if( pKey ){ EVP_PKEY_free(pKey); }` |
|    7 | 1550 | `	if( pCert ){ X509_free(pCert); }` |
|    7 | 1551 | `	if( pChain ){ sk_X509_pop_free(pChain,X509_free); }` |
|    7 | 1552 | `	if( zPassZ ){ SyMemBackendFree(&pCtx->pVm->sAllocator,zPassZ); }` |
|    7 | 1553 | `	SyBlobRelease(&sFile);` |
|    7 | 1554 | `	if( rc != 0 ){` |
|    3 | 1555 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    1 | 1556 | `	}` |
|    7 | 1557 | `	ph7_result_bool(pCtx,rc == 0);` |
|    7 | 1558 | `	return PH7_OK;` |
|    4 | 1559 | `}` |
|    - | 1560 |  |
|    - | 1561 | `/* ------------------------------------------------------------------------` |
|    - | 1562 | ` * PKCS#7 and CMS` |
|    - | 1563 | ` * ------------------------------------------------------------------------ */` |
|    - | 1564 | `/*` |
|    - | 1565 | ` * The two message families are the SAME shape twice over, which is why they` |
|    - | 1566 | ` * share every helper below: PKCS#7 is the older spelling and CMS the newer` |
|    - | 1567 | ``  * one, and php's only real difference is that CMS takes an `$encoding` `` |
|    - | 1568 | ` * (S/MIME, DER or PEM) where PKCS#7 is always S/MIME.` |
|    - | 1569 | ` *` |
|    - | 1570 | ` * They are also the only functions in this extension that work in FILES rather` |
|    - | 1571 | ` * than strings, which is php's own choice: every input and output is a path,` |
|    - | 1572 | `` * and a path that cannot be opened is `Error opening input file %s!` -- with`` |
|    - | 1573 | ` * php's exclamation mark.` |
|    - | 1574 | ` */` |
|    - | 1575 | `/*` |
|    - | 1576 | ` * The message families work in PATHS, and they open them with OpenSSL's own` |
|    - | 1577 | ` * file BIO rather than through the engine's stream layer -- which is php's` |
|    - | 1578 | ` * choice and is observable twice over: a path that does not exist leaves` |
|    - | 1579 | `` * `system library::No such file or directory` and `BIO routines::no such`` |
|    - | 1580 | `` * file` in the error ring, and a `phar://` or userland-wrapper path is not`` |
|    - | 1581 | ` * a thing these six doors can read under php either.` |
|    - | 1582 | ` */` |
|   40 | 1583 | `static char * SslPathZ(ph7_context *pCtx,ph7_value *pVal)` |
|    1 | 1584 | `{` |
|   41 | 1585 | `	int nPath = 0;` |
|   41 | 1586 | `	const char *zPath = ph7_value_to_string(pVal,&nPath);` |
|    - | 1587 | `	char *zZ;` |
|   41 | 1588 | `	if( zPath == 0 \|\| nPath < 1 ){` |
|  ! 0 | 1589 | `		return 0;` |
|    - | 1590 | `	}` |
|   41 | 1591 | `	zZ = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)(nPath + 1));` |
|   41 | 1592 | `	if( zZ == 0 ){` |
|  ! 0 | 1593 | `		return 0;` |
|    - | 1594 | `	}` |
|   41 | 1595 | `	SyMemcpy(zPath,zZ,(sxu32)nPath);` |
|   41 | 1596 | `	zZ[nPath] = 0;` |
|   41 | 1597 | `	return zZ;` |
|   21 | 1598 | `}` |
|   28 | 1599 | `static BIO * SslBioOfFileArg(ph7_context *pCtx,ph7_value *pVal)` |
|    1 | 1600 | `{` |
|   29 | 1601 | `	char *zZ = SslPathZ(pCtx,pVal);` |
|    - | 1602 | `	BIO *pBio;` |
|   29 | 1603 | `	if( zZ == 0 ){` |
|  ! 0 | 1604 | `		return 0;` |
|    - | 1605 | `	}` |
|   29 | 1606 | `	pBio = BIO_new_file(zZ,"rb");` |
|   29 | 1607 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);` |
|   29 | 1608 | `	return pBio;` |
|   15 | 1609 | `}` |
|   12 | 1610 | `static int SslWriteBioToFileArg(ph7_context *pCtx,ph7_value *pVal,BIO *pBio)` |
|    1 | 1611 | `{` |
|   13 | 1612 | `	char *zMem = 0;` |
|   13 | 1613 | `	long nMem = BIO_get_mem_data(pBio,&zMem);` |
|   13 | 1614 | `	char *zZ = SslPathZ(pCtx,pVal);` |
|    - | 1615 | `	BIO *pOut;` |
|   13 | 1616 | `	int rc = -1;` |
|   13 | 1617 | `	if( zZ == 0 ){` |
|  ! 0 | 1618 | `		return -1;` |
|    - | 1619 | `	}` |
|   13 | 1620 | `	pOut = BIO_new_file(zZ,"wb");` |
|   13 | 1621 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zZ);` |
|   13 | 1622 | `	if( pOut == 0 ){` |
|  ! 0 | 1623 | `		return -1;` |
|    - | 1624 | `	}` |
|   13 | 1625 | `	if( nMem <= 0 \|\| BIO_write(pOut,zMem,(int)nMem) == (int)nMem ){` |
|   13 | 1626 | `		rc = 0;` |
|    6 | 1627 | `	}` |
|   13 | 1628 | `	BIO_free(pOut);` |
|   13 | 1629 | `	return rc;` |
|    7 | 1630 | `}` |
|    - | 1631 | ``/* The `$headers` array both sign doors prepend to an S/MIME body: a string`` |
|    - | 1632 | `` * entry is a whole header line, a keyed one is `Key: value`. */`` |
|    8 | 1633 | `static void SslWriteHeaders(ph7_context *pCtx,ph7_value *pHeaders,BIO *pOut)` |
|    1 | 1634 | `{` |
|    - | 1635 | `	ph7_hashmap *pMap;` |
|    - | 1636 | `	ph7_hashmap_node *pEntry;` |
|    - | 1637 | `	sxu32 i;` |
|    4 | 1638 | `	SXUNUSED(pCtx);` |
|    9 | 1639 | `	if( pHeaders == 0 \|\| !ph7_value_is_array(pHeaders) ){` |
|  ! 0 | 1640 | `		return;` |
|    - | 1641 | `	}` |
|    9 | 1642 | `	pMap = (ph7_hashmap *)pHeaders->x.pOther;` |
|    9 | 1643 | `	pEntry = pMap->pFirst;` |
|    9 | 1644 | `	for( i = 0 ; i < pMap->nEntry ; ++i ){` |
|    - | 1645 | `		ph7_value sKey;` |
|  ! 0 | 1646 | `		ph7_value *pData = HashmapExtractNodeValue(pEntry);` |
|    - | 1647 | `		const char *zVal;` |
|  ! 0 | 1648 | `		int nVal = 0;` |
|  ! 0 | 1649 | `		PH7_MemObjInit(pCtx->pVm,&sKey);` |
|  ! 0 | 1650 | `		PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|  ! 0 | 1651 | `		pEntry = pEntry->pPrev;` |
|  ! 0 | 1652 | `		zVal = pData ? ph7_value_to_string(pData,&nVal) : 0;` |
|  ! 0 | 1653 | `		if( zVal ){` |
|  ! 0 | 1654 | `			if( ph7_value_is_string(&sKey) ){` |
|  ! 0 | 1655 | `				int nKey = 0;` |
|  ! 0 | 1656 | `				const char *zKey = ph7_value_to_string(&sKey,&nKey);` |
|  ! 0 | 1657 | `				BIO_write(pOut,zKey,nKey);` |
|  ! 0 | 1658 | `				BIO_write(pOut,": ",2);` |
|  ! 0 | 1659 | `			}` |
|  ! 0 | 1660 | `			BIO_write(pOut,zVal,nVal);` |
|  ! 0 | 1661 | `			BIO_write(pOut,"\n",1);` |
|  ! 0 | 1662 | `		}` |
|  ! 0 | 1663 | `		PH7_MemObjRelease(&sKey);` |
|  ! 0 | 1664 | `	}` |
|    5 | 1665 | `}` |
|    - | 1666 | `/* php's OPENSSL_CIPHER_* enum, which the two encrypt doors take as a NUMBER` |
|    - | 1667 | ` * rather than a name. */` |
|    6 | 1668 | `static const EVP_CIPHER * SslCipherOfPhpEnum(sxi64 iCipher)` |
|    1 | 1669 | `{` |
|    7 | 1670 | `	switch( iCipher ){` |
|  ! 0 | 1671 | `		case 0: case 1: case 2: return EVP_rc2_cbc();` |
|  ! 0 | 1672 | `		case 3: return EVP_des_cbc();` |
|  ! 0 | 1673 | `		case 4: return EVP_des_ede3_cbc();` |
|    7 | 1674 | `		case 5: return EVP_aes_128_cbc();` |
|  ! 0 | 1675 | `		case 6: return EVP_aes_192_cbc();` |
|  ! 0 | 1676 | `		case 7: return EVP_aes_256_cbc();` |
|  ! 0 | 1677 | `		default: return 0;` |
|    - | 1678 | `	}` |
|    4 | 1679 | `}` |
|    - | 1680 | `/* A recipient list: one certificate, or an array of them. */` |
|    4 | 1681 | `static STACK_OF(X509) * SslRecipients(ph7_context *pCtx,ph7_value *pVal)` |
|    1 | 1682 | `{` |
|    5 | 1683 | `	STACK_OF(X509) *pStack = sk_X509_new_null();` |
|    5 | 1684 | `	if( pStack == 0 ){` |
|  ! 0 | 1685 | `		return 0;` |
|    - | 1686 | `	}` |
|    5 | 1687 | `	if( ph7_value_is_array(pVal) ){` |
|  ! 0 | 1688 | `		ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|  ! 0 | 1689 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|    - | 1690 | `		sxu32 i;` |
|  ! 0 | 1691 | `		for( i = 0 ; i < pMap->nEntry ; ++i ){` |
|  ! 0 | 1692 | `			ph7_value *pData = HashmapExtractNodeValue(pEntry);` |
|  ! 0 | 1693 | `			int bOwn = 0;` |
|  ! 0 | 1694 | `			X509 *pOne = pData ? SslCertOfValue(pCtx,pData,&bOwn,0) : 0;` |
|  ! 0 | 1695 | `			pEntry = pEntry->pPrev;` |
|  ! 0 | 1696 | `			if( pOne == 0 ){` |
|  ! 0 | 1697 | `				continue;` |
|    - | 1698 | `			}` |
|  ! 0 | 1699 | `			if( !bOwn ){` |
|  ! 0 | 1700 | `				X509_up_ref(pOne);` |
|  ! 0 | 1701 | `			}` |
|  ! 0 | 1702 | `			sk_X509_push(pStack,pOne);` |
|  ! 0 | 1703 | `		}` |
|  ! 0 | 1704 | `	}else{` |
|    5 | 1705 | `		int bOwn = 0;` |
|    5 | 1706 | `		X509 *pOne = SslCertOfValue(pCtx,pVal,&bOwn,0);` |
|    5 | 1707 | `		if( pOne ){` |
|    5 | 1708 | `			if( !bOwn ){` |
|    5 | 1709 | `				X509_up_ref(pOne);` |
|    2 | 1710 | `			}` |
|    5 | 1711 | `			sk_X509_push(pStack,pOne);` |
|    2 | 1712 | `		}` |
|    - | 1713 | `	}` |
|    5 | 1714 | `	if( sk_X509_num(pStack) < 1 ){` |
|  ! 0 | 1715 | `		sk_X509_free(pStack);` |
|  ! 0 | 1716 | `		return 0;` |
|    - | 1717 | `	}` |
|    5 | 1718 | `	return pStack;` |
|    3 | 1719 | `}` |
|    - | 1720 | `/* The untrusted-certificate file both verify doors take. */` |
|  ! 0 | 1721 | `static STACK_OF(X509) * SslCertsFromFileArg(ph7_context *pCtx,ph7_value *pVal)` |
|  ! 0 | 1722 | `{` |
|    - | 1723 | `	STACK_OF(X509) *pStack;` |
|  ! 0 | 1724 | `	BIO *pBio = SslBioOfFileArg(pCtx,pVal);` |
|    - | 1725 | `	X509 *pOne;` |
|  ! 0 | 1726 | `	if( pBio == 0 ){` |
|  ! 0 | 1727 | `		return 0;` |
|    - | 1728 | `	}` |
|  ! 0 | 1729 | `	pStack = sk_X509_new_null();` |
|  ! 0 | 1730 | `	while( pStack && (pOne = PEM_read_bio_X509(pBio,0,0,0)) != 0 ){` |
|  ! 0 | 1731 | `		sk_X509_push(pStack,pOne);` |
|  ! 0 | 1732 | `	}` |
|  ! 0 | 1733 | `	ERR_clear_error();   /* running out of certificates is how the walk ENDS */` |
|  ! 0 | 1734 | `	BIO_free(pBio);` |
|  ! 0 | 1735 | `	return pStack;` |
|  ! 0 | 1736 | `}` |
|    6 | 1737 | `static int SslMsgSign(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)` |
|    1 | 1738 | `{` |
|    7 | 1739 | `	BIO *pIn = 0,*pOut = 0;` |
|    7 | 1740 | `	X509 *pCert = 0;` |
|    7 | 1741 | `	EVP_PKEY *pKey = 0;` |
|    7 | 1742 | `	STACK_OF(X509) *pExtra = 0;` |
|    7 | 1743 | `	PKCS7 *pP7 = 0;` |
|    7 | 1744 | `	CMS_ContentInfo *pCms = 0;` |
|    7 | 1745 | `	int bOwnCert = 0,bOwnKey = 0,rc = -1;` |
|    7 | 1746 | `	sxi64 iFlags = bCms ? 0 : PKCS7_DETACHED;` |
|    7 | 1747 | `	sxi64 iEncoding = 1;   /* OPENSSL_ENCODING_SMIME */` |
|    7 | 1748 | `	if( nArg < 5 ){` |
|  ! 0 | 1749 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1750 | `		return PH7_OK;` |
|    - | 1751 | `	}` |
|    7 | 1752 | `	if( nArg > 5 ){` |
|  ! 0 | 1753 | `		iFlags = ph7_value_to_int64(apArg[5]);` |
|  ! 0 | 1754 | `	}` |
|    7 | 1755 | `	if( bCms && nArg > 6 ){` |
|  ! 0 | 1756 | `		iEncoding = ph7_value_to_int64(apArg[6]);` |
|  ! 0 | 1757 | `	}` |
|    - | 1758 | `	/* The CERTIFICATE is read before the input file is opened, which is php's` |
|    - | 1759 | `	 * order and is observable: a bad cert AND a missing input answer the` |
|    - | 1760 | `	 * certificate's complaint. */` |
|    7 | 1761 | `	pCert = SslCertOfValue(pCtx,apArg[2],&bOwnCert,1);` |
|    7 | 1762 | `	if( pCert == 0 ){` |
|    3 | 1763 | `		goto done;` |
|    - | 1764 | `	}` |
|    5 | 1765 | `	pKey = PH7_SslKeyOfValue(pCtx,apArg[3],0,&bOwnKey);` |
|    5 | 1766 | `	if( pKey == 0 ){` |
|  ! 0 | 1767 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Error getting private key");` |
|  ! 0 | 1768 | `		goto done;` |
|    - | 1769 | `	}` |
|    5 | 1770 | `	pIn = SslBioOfFileArg(pCtx,apArg[0]);` |
|    5 | 1771 | `	if( pIn == 0 ){` |
|  ! 0 | 1772 | `		int nPath = 0;` |
|  ! 0 | 1773 | `		const char *zPath = ph7_value_to_string(apArg[0],&nPath);` |
|  ! 0 | 1774 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|  ! 0 | 1775 | `			"Error opening input file %.*s!",nPath,zPath ? zPath : "");` |
|  ! 0 | 1776 | `		goto done;` |
|    - | 1777 | `	}` |
|    5 | 1778 | `	if( nArg > (bCms ? 7 : 6) && !ph7_value_is_null(apArg[bCms ? 7 : 6]) ){` |
|  ! 0 | 1779 | `		pExtra = SslCertsFromFileArg(pCtx,apArg[bCms ? 7 : 6]);` |
|  ! 0 | 1780 | `	}` |
|    5 | 1781 | `	pOut = BIO_new(BIO_s_mem());` |
|    5 | 1782 | `	if( pOut == 0 ){` |
|  ! 0 | 1783 | `		goto done;` |
|    - | 1784 | `	}` |
|    5 | 1785 | `	if( bCms ){` |
|    3 | 1786 | `		pCms = CMS_sign(pCert,pKey,pExtra,pIn,(unsigned int)iFlags);` |
|    3 | 1787 | `		if( pCms == 0 ){` |
|  ! 0 | 1788 | `			goto done;` |
|    - | 1789 | `		}` |
|    3 | 1790 | `		SslWriteHeaders(pCtx,apArg[4],pOut);` |
|    3 | 1791 | `		if( iEncoding == 0 ){` |
|  ! 0 | 1792 | `			rc = i2d_CMS_bio(pOut,pCms) == 1 ? 0 : -1;` |
|    3 | 1793 | `		}else if( iEncoding == 2 ){` |
|  ! 0 | 1794 | `			rc = PEM_write_bio_CMS(pOut,pCms) == 1 ? 0 : -1;` |
|  ! 0 | 1795 | `		}else{` |
|    3 | 1796 | `			BIO_reset(pIn);` |
|    3 | 1797 | `			rc = SMIME_write_CMS(pOut,pCms,pIn,(int)iFlags) == 1 ? 0 : -1;` |
|    - | 1798 | `		}` |
|    2 | 1799 | `	}else{` |
|    3 | 1800 | `		pP7 = PKCS7_sign(pCert,pKey,pExtra,pIn,(int)iFlags);` |
|    3 | 1801 | `		if( pP7 == 0 ){` |
|  ! 0 | 1802 | `			goto done;` |
|    - | 1803 | `		}` |
|    3 | 1804 | `		SslWriteHeaders(pCtx,apArg[4],pOut);` |
|    3 | 1805 | `		BIO_reset(pIn);` |
|    3 | 1806 | `		rc = SMIME_write_PKCS7(pOut,pP7,pIn,(int)iFlags) == 1 ? 0 : -1;` |
|    - | 1807 | `	}` |
|    7 | 1808 | `	if( rc == 0 ){` |
|    5 | 1809 | `		rc = SslWriteBioToFileArg(pCtx,apArg[1],pOut);` |
|    2 | 1810 | `	}` |
|  ! 0 | 1811 | `done:` |
|    7 | 1812 | `	if( pP7 ){ PKCS7_free(pP7); }` |
|    7 | 1813 | `	if( pCms ){ CMS_ContentInfo_free(pCms); }` |
|    7 | 1814 | `	if( pExtra ){ sk_X509_pop_free(pExtra,X509_free); }` |
|    7 | 1815 | `	if( bOwnKey > 0 ){ EVP_PKEY_free(pKey); }` |
|    7 | 1816 | `	SslFreeCert(pCert,bOwnCert);` |
|    7 | 1817 | `	if( pOut ){ BIO_free(pOut); }` |
|    7 | 1818 | `	if( pIn ){ BIO_free(pIn); }` |
|    - | 1819 |  |
|    7 | 1820 | `	if( rc != 0 ){` |
|    3 | 1821 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    1 | 1822 | `	}` |
|    7 | 1823 | `	ph7_result_bool(pCtx,rc == 0);` |
|    7 | 1824 | `	return PH7_OK;` |
|    4 | 1825 | `}` |
|    4 | 1826 | `static int vm_builtin_openssl_pkcs7_sign(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1827 | `{` |
|    5 | 1828 | `	return SslMsgSign(pCtx,nArg,apArg,0);` |
|    1 | 1829 | `}` |
|    2 | 1830 | `static int vm_builtin_openssl_cms_sign(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1831 | `{` |
|    3 | 1832 | `	return SslMsgSign(pCtx,nArg,apArg,1);` |
|    1 | 1833 | `}` |
|    6 | 1834 | `static int SslMsgEncrypt(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)` |
|    1 | 1835 | `{` |
|    7 | 1836 | `	BIO *pIn = 0,*pOut = 0;` |
|    7 | 1837 | `	STACK_OF(X509) *pTo = 0;` |
|    7 | 1838 | `	PKCS7 *pP7 = 0;` |
|    7 | 1839 | `	CMS_ContentInfo *pCms = 0;` |
|    - | 1840 | `	const EVP_CIPHER *pCipher;` |
|    7 | 1841 | `	sxi64 iFlags = 0,iEncoding = 1,iCipher = 5;` |
|    7 | 1842 | `	int rc = -1;` |
|    7 | 1843 | `	if( nArg < 4 ){` |
|  ! 0 | 1844 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1845 | `		return PH7_OK;` |
|    - | 1846 | `	}` |
|    7 | 1847 | `	if( nArg > 4 ){` |
|  ! 0 | 1848 | `		iFlags = ph7_value_to_int64(apArg[4]);` |
|  ! 0 | 1849 | `	}` |
|    7 | 1850 | `	if( bCms ){` |
|    3 | 1851 | `		if( nArg > 5 ){ iEncoding = ph7_value_to_int64(apArg[5]); }` |
|    3 | 1852 | `		if( nArg > 6 ){ iCipher = ph7_value_to_int64(apArg[6]); }` |
|    2 | 1853 | `	}else{` |
|    5 | 1854 | `		if( nArg > 5 ){ iCipher = ph7_value_to_int64(apArg[5]); }` |
|    - | 1855 | `	}` |
|    7 | 1856 | `	pCipher = SslCipherOfPhpEnum(iCipher);` |
|    7 | 1857 | `	if( pCipher == 0 ){` |
|  ! 0 | 1858 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"Failed to get cipher");` |
|  ! 0 | 1859 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1860 | `		return PH7_OK;` |
|    - | 1861 | `	}` |
|    7 | 1862 | `	pIn = SslBioOfFileArg(pCtx,apArg[0]);` |
|    7 | 1863 | `	if( pIn == 0 ){` |
|    3 | 1864 | `		goto done;` |
|    - | 1865 | `	}` |
|    5 | 1866 | `	pTo = SslRecipients(pCtx,apArg[2]);` |
|    5 | 1867 | `	if( pTo == 0 ){` |
|    - | 1868 | `		/* Silent: the two ENCRYPT doors answer a flat false for a recipient` |
|    - | 1869 | `		 * they cannot read, where the two SIGN doors say so. */` |
|  ! 0 | 1870 | `		goto done;` |
|    - | 1871 | `	}` |
|    5 | 1872 | `	pOut = BIO_new(BIO_s_mem());` |
|    5 | 1873 | `	if( pOut == 0 ){` |
|  ! 0 | 1874 | `		goto done;` |
|    - | 1875 | `	}` |
|    5 | 1876 | `	SslWriteHeaders(pCtx,apArg[3],pOut);` |
|    5 | 1877 | `	if( bCms ){` |
|    3 | 1878 | `		pCms = CMS_encrypt(pTo,pIn,pCipher,(unsigned int)iFlags);` |
|    3 | 1879 | `		if( pCms == 0 ){` |
|  ! 0 | 1880 | `			goto done;` |
|    - | 1881 | `		}` |
|    3 | 1882 | `		if( iEncoding == 0 ){` |
|  ! 0 | 1883 | `			rc = i2d_CMS_bio(pOut,pCms) == 1 ? 0 : -1;` |
|    3 | 1884 | `		}else if( iEncoding == 2 ){` |
|  ! 0 | 1885 | `			rc = PEM_write_bio_CMS(pOut,pCms) == 1 ? 0 : -1;` |
|  ! 0 | 1886 | `		}else{` |
|    3 | 1887 | `			rc = SMIME_write_CMS(pOut,pCms,0,(int)iFlags) == 1 ? 0 : -1;` |
|    - | 1888 | `		}` |
|    2 | 1889 | `	}else{` |
|    3 | 1890 | `		pP7 = PKCS7_encrypt(pTo,pIn,pCipher,(int)iFlags);` |
|    3 | 1891 | `		if( pP7 == 0 ){` |
|  ! 0 | 1892 | `			goto done;` |
|    - | 1893 | `		}` |
|    3 | 1894 | `		rc = SMIME_write_PKCS7(pOut,pP7,0,(int)iFlags) == 1 ? 0 : -1;` |
|    - | 1895 | `	}` |
|    7 | 1896 | `	if( rc == 0 ){` |
|    5 | 1897 | `		rc = SslWriteBioToFileArg(pCtx,apArg[1],pOut);` |
|    2 | 1898 | `	}` |
|  ! 0 | 1899 | `done:` |
|    7 | 1900 | `	if( pP7 ){ PKCS7_free(pP7); }` |
|    7 | 1901 | `	if( pCms ){ CMS_ContentInfo_free(pCms); }` |
|    7 | 1902 | `	if( pTo ){ sk_X509_pop_free(pTo,X509_free); }` |
|    7 | 1903 | `	if( pOut ){ BIO_free(pOut); }` |
|    7 | 1904 | `	if( pIn ){ BIO_free(pIn); }` |
|    7 | 1905 | `	if( rc != 0 ){` |
|    3 | 1906 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    1 | 1907 | `	}` |
|    7 | 1908 | `	ph7_result_bool(pCtx,rc == 0);` |
|    7 | 1909 | `	return PH7_OK;` |
|    4 | 1910 | `}` |
|    4 | 1911 | `static int vm_builtin_openssl_pkcs7_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1912 | `{` |
|    5 | 1913 | `	return SslMsgEncrypt(pCtx,nArg,apArg,0);` |
|    1 | 1914 | `}` |
|    2 | 1915 | `static int vm_builtin_openssl_cms_encrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1916 | `{` |
|    3 | 1917 | `	return SslMsgEncrypt(pCtx,nArg,apArg,1);` |
|    1 | 1918 | `}` |
|    6 | 1919 | `static int SslMsgDecrypt(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)` |
|    1 | 1920 | `{` |
|    7 | 1921 | `	BIO *pIn = 0,*pOut = 0;` |
|    7 | 1922 | `	X509 *pCert = 0;` |
|    7 | 1923 | `	EVP_PKEY *pKey = 0;` |
|    7 | 1924 | `	PKCS7 *pP7 = 0;` |
|    7 | 1925 | `	CMS_ContentInfo *pCms = 0;` |
|    7 | 1926 | `	sxi64 iEncoding = 1;` |
|    7 | 1927 | `	int bOwnCert = 0,bOwnKey = 0,rc = -1;` |
|    7 | 1928 | `	if( nArg < 3 ){` |
|  ! 0 | 1929 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1930 | `		return PH7_OK;` |
|    - | 1931 | `	}` |
|    7 | 1932 | `	if( bCms && nArg > 4 ){` |
|  ! 0 | 1933 | `		iEncoding = ph7_value_to_int64(apArg[4]);` |
|  ! 0 | 1934 | `	}` |
|    - | 1935 | `	/* Both DECRYPT doors are silent about everything: a missing input file, an` |
|    - | 1936 | `	 * unreadable certificate and a key that will not open are all a flat` |
|    - | 1937 | `	 * false with the library's own reason left in the error ring. */` |
|    7 | 1938 | `	pIn = SslBioOfFileArg(pCtx,apArg[0]);` |
|    7 | 1939 | `	if( pIn == 0 ){` |
|    3 | 1940 | `		goto done;` |
|    - | 1941 | `	}` |
|    5 | 1942 | `	pCert = SslCertOfValue(pCtx,apArg[2],&bOwnCert,0);` |
|    5 | 1943 | `	if( pCert == 0 ){` |
|  ! 0 | 1944 | `		goto done;` |
|    - | 1945 | `	}` |
|    5 | 1946 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|    5 | 1947 | `		pKey = PH7_SslKeyOfValue(pCtx,apArg[3],0,&bOwnKey);` |
|    3 | 1948 | `	}else{` |
|  ! 0 | 1949 | `		pKey = PH7_SslKeyOfValue(pCtx,apArg[2],0,&bOwnKey);` |
|    - | 1950 | `	}` |
|    5 | 1951 | `	if( pKey == 0 ){` |
|  ! 0 | 1952 | `		goto done;` |
|    - | 1953 | `	}` |
|    5 | 1954 | `	pOut = BIO_new(BIO_s_mem());` |
|    5 | 1955 | `	if( pOut == 0 ){` |
|  ! 0 | 1956 | `		goto done;` |
|    - | 1957 | `	}` |
|    5 | 1958 | `	if( bCms ){` |
|    3 | 1959 | `		if( iEncoding == 0 ){` |
|  ! 0 | 1960 | `			pCms = d2i_CMS_bio(pIn,0);` |
|    3 | 1961 | `		}else if( iEncoding == 2 ){` |
|  ! 0 | 1962 | `			pCms = PEM_read_bio_CMS(pIn,0,0,0);` |
|  ! 0 | 1963 | `		}else{` |
|    3 | 1964 | `			pCms = SMIME_read_CMS(pIn,0);` |
|    - | 1965 | `		}` |
|    3 | 1966 | `		if( pCms == 0 ){` |
|  ! 0 | 1967 | `			goto done;` |
|    - | 1968 | `		}` |
|    3 | 1969 | `		rc = CMS_decrypt(pCms,pKey,pCert,0,pOut,0) == 1 ? 0 : -1;` |
|    2 | 1970 | `	}else{` |
|    3 | 1971 | `		pP7 = SMIME_read_PKCS7(pIn,0);` |
|    3 | 1972 | `		if( pP7 == 0 ){` |
|  ! 0 | 1973 | `			goto done;` |
|    - | 1974 | `		}` |
|    3 | 1975 | `		rc = PKCS7_decrypt(pP7,pKey,pCert,pOut,0) == 1 ? 0 : -1;` |
|    - | 1976 | `	}` |
|    7 | 1977 | `	if( rc == 0 ){` |
|    5 | 1978 | `		rc = SslWriteBioToFileArg(pCtx,apArg[1],pOut);` |
|    2 | 1979 | `	}` |
|  ! 0 | 1980 | `done:` |
|    7 | 1981 | `	if( pP7 ){ PKCS7_free(pP7); }` |
|    7 | 1982 | `	if( pCms ){ CMS_ContentInfo_free(pCms); }` |
|    7 | 1983 | `	if( bOwnKey > 0 ){ EVP_PKEY_free(pKey); }` |
|    7 | 1984 | `	SslFreeCert(pCert,bOwnCert);` |
|    7 | 1985 | `	if( pOut ){ BIO_free(pOut); }` |
|    7 | 1986 | `	if( pIn ){ BIO_free(pIn); }` |
|    7 | 1987 | `	if( rc != 0 ){` |
|    3 | 1988 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    1 | 1989 | `	}` |
|    7 | 1990 | `	ph7_result_bool(pCtx,rc == 0);` |
|    7 | 1991 | `	return PH7_OK;` |
|    4 | 1992 | `}` |
|    4 | 1993 | `static int vm_builtin_openssl_pkcs7_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1994 | `{` |
|    5 | 1995 | `	return SslMsgDecrypt(pCtx,nArg,apArg,0);` |
|    1 | 1996 | `}` |
|    2 | 1997 | `static int vm_builtin_openssl_cms_decrypt(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1998 | `{` |
|    3 | 1999 | `	return SslMsgDecrypt(pCtx,nArg,apArg,1);` |
|    1 | 2000 | `}` |
|   12 | 2001 | `static int SslMsgVerify(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)` |
|    1 | 2002 | `{` |
|   13 | 2003 | `	BIO *pIn = 0,*pContent = 0,*pOut = 0;` |
|   13 | 2004 | `	PKCS7 *pP7 = 0;` |
|   13 | 2005 | `	CMS_ContentInfo *pCms = 0;` |
|   13 | 2006 | `	X509_STORE *pStore = 0;` |
|   13 | 2007 | `	STACK_OF(X509) *pUntrusted = 0,*pSigners = 0;` |
|   13 | 2008 | `	sxi64 iFlags = 0,iEncoding = 1;` |
|   13 | 2009 | `	int iCa = 3,iUntrusted = 4,iContent = 5,iSigners = 2,rc = -1,bRan = 0;` |
|   13 | 2010 | `	if( nArg < 2 ){` |
|  ! 0 | 2011 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2012 | `		return PH7_OK;` |
|    - | 2013 | `	}` |
|   13 | 2014 | `	iFlags = ph7_value_to_int64(apArg[1]);` |
|   13 | 2015 | `	if( bCms && nArg > 8 ){` |
|  ! 0 | 2016 | `		iEncoding = ph7_value_to_int64(apArg[8]);` |
|  ! 0 | 2017 | `	}` |
|   13 | 2018 | `	pIn = SslBioOfFileArg(pCtx,apArg[0]);` |
|   13 | 2019 | `	if( pIn == 0 ){` |
|    5 | 2020 | `		goto done;` |
|    - | 2021 | `	}` |
|    9 | 2022 | `	if( nArg > iUntrusted && !ph7_value_is_null(apArg[iUntrusted]) ){` |
|  ! 0 | 2023 | `		pUntrusted = SslCertsFromFileArg(pCtx,apArg[iUntrusted]);` |
|  ! 0 | 2024 | `	}` |
|    9 | 2025 | `	pStore = SslStoreFromCaInfo(pCtx,nArg > iCa ? apArg[iCa] : 0);` |
|    9 | 2026 | `	pOut = BIO_new(BIO_s_mem());` |
|    9 | 2027 | `	if( pStore == 0 \|\| pOut == 0 ){` |
|  ! 0 | 2028 | `		goto done;` |
|    - | 2029 | `	}` |
|    9 | 2030 | `	if( bCms ){` |
|    5 | 2031 | `		if( iEncoding == 0 ){` |
|  ! 0 | 2032 | `			pCms = d2i_CMS_bio(pIn,0);` |
|    5 | 2033 | `		}else if( iEncoding == 2 ){` |
|  ! 0 | 2034 | `			pCms = PEM_read_bio_CMS(pIn,0,0,0);` |
|  ! 0 | 2035 | `		}else{` |
|    5 | 2036 | `			pCms = SMIME_read_CMS_ex(pIn,0,&pContent,0);` |
|    - | 2037 | `		}` |
|    5 | 2038 | `		if( pCms == 0 ){` |
|  ! 0 | 2039 | `			goto done;` |
|    - | 2040 | `		}` |
|    5 | 2041 | `		bRan = 1;` |
|    5 | 2042 | `		rc = CMS_verify(pCms,pUntrusted,pStore,pContent,pOut,(unsigned int)iFlags) == 1 ? 0 : -1;` |
|    5 | 2043 | `		if( rc == 0 ){` |
|    3 | 2044 | `			pSigners = CMS_get0_signers(pCms);` |
|    1 | 2045 | `		}` |
|    3 | 2046 | `	}else{` |
|    5 | 2047 | `		pP7 = SMIME_read_PKCS7(pIn,&pContent);` |
|    5 | 2048 | `		if( pP7 == 0 ){` |
|  ! 0 | 2049 | `			goto done;` |
|    - | 2050 | `		}` |
|    5 | 2051 | `		bRan = 1;` |
|    5 | 2052 | `		rc = PKCS7_verify(pP7,pUntrusted,pStore,pContent,pOut,(int)iFlags) == 1 ? 0 : -1;` |
|    5 | 2053 | `		if( rc == 0 ){` |
|    3 | 2054 | `			pSigners = PKCS7_get0_signers(pP7,pUntrusted,(int)iFlags);` |
|    1 | 2055 | `		}` |
|    - | 2056 | `	}` |
|    9 | 2057 | `	if( rc == 0 && nArg > iSigners && !ph7_value_is_null(apArg[iSigners]) && pSigners ){` |
|  ! 0 | 2058 | `		BIO *pCerts = BIO_new(BIO_s_mem());` |
|    - | 2059 | `		int i;` |
|  ! 0 | 2060 | `		if( pCerts ){` |
|  ! 0 | 2061 | `			for( i = 0 ; i < sk_X509_num(pSigners) ; ++i ){` |
|  ! 0 | 2062 | `				PEM_write_bio_X509(pCerts,sk_X509_value(pSigners,i));` |
|  ! 0 | 2063 | `			}` |
|  ! 0 | 2064 | `			SslWriteBioToFileArg(pCtx,apArg[iSigners],pCerts);` |
|  ! 0 | 2065 | `			BIO_free(pCerts);` |
|  ! 0 | 2066 | `		}` |
|  ! 0 | 2067 | `	}` |
|    9 | 2068 | `	if( rc == 0 && nArg > iContent && !ph7_value_is_null(apArg[iContent]) ){` |
|  ! 0 | 2069 | `		SslWriteBioToFileArg(pCtx,apArg[iContent],pOut);` |
|  ! 0 | 2070 | `	}` |
|  ! 0 | 2071 | `done:` |
|   13 | 2072 | `	if( pSigners ){ sk_X509_free(pSigners); }` |
|   13 | 2073 | `	if( pP7 ){ PKCS7_free(pP7); }` |
|   13 | 2074 | `	if( pCms ){ CMS_ContentInfo_free(pCms); }` |
|   13 | 2075 | `	if( pUntrusted ){ sk_X509_pop_free(pUntrusted,X509_free); }` |
|   13 | 2076 | `	if( pStore ){ X509_STORE_free(pStore); }` |
|   13 | 2077 | `	if( pContent ){ BIO_free(pContent); }` |
|   13 | 2078 | `	if( pOut ){ BIO_free(pOut); }` |
|   13 | 2079 | `	if( pIn ){ BIO_free(pIn); }` |
|   13 | 2080 | `	if( rc != 0 ){` |
|    9 | 2081 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    4 | 2082 | `	}` |
|   13 | 2083 | `	if( rc != 0 && !bRan ){` |
|    - | 2084 | `		/* Never reached the verification at all. php's two doors part company` |
|    - | 2085 | `		 * exactly here: PKCS#7 answers -1 for "could not even read this" and` |
|    - | 2086 | `		 * keeps false for a signature that did not check out, while CMS --` |
|    - | 2087 | `		 * whose declared return is bool -- answers false for both. */` |
|    5 | 2088 | `		if( bCms ){` |
|    3 | 2089 | `			ph7_result_bool(pCtx,0);` |
|    2 | 2090 | `		}else{` |
|    3 | 2091 | `			ph7_result_int(pCtx,-1);` |
|    - | 2092 | `		}` |
|    5 | 2093 | `		return PH7_OK;` |
|    - | 2094 | `	}` |
|    9 | 2095 | `	ph7_result_bool(pCtx,rc == 0);` |
|    9 | 2096 | `	return PH7_OK;` |
|    7 | 2097 | `}` |
|    6 | 2098 | `static int vm_builtin_openssl_pkcs7_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2099 | `{` |
|    7 | 2100 | `	return SslMsgVerify(pCtx,nArg,apArg,0);` |
|    1 | 2101 | `}` |
|    6 | 2102 | `static int vm_builtin_openssl_cms_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2103 | `{` |
|    7 | 2104 | `	return SslMsgVerify(pCtx,nArg,apArg,1);` |
|    1 | 2105 | `}` |
|    - | 2106 | `/*` |
|    - | 2107 | ` * The two READ doors take BYTES rather than a path, and hand back the` |
|    - | 2108 | ` * certificates a message carries -- as PEM strings, in an array. A signed` |
|    - | 2109 | ` * S/MIME body is not one of the forms they accept: it has to be the DER or PEM` |
|    - | 2110 | ` * structure itself, which is why reading back what sign() just wrote is false` |
|    - | 2111 | ` * under php too.` |
|    - | 2112 | ` */` |
|    4 | 2113 | `static int SslMsgRead(ph7_context *pCtx,int nArg,ph7_value **apArg,int bCms)` |
|    1 | 2114 | `{` |
|    - | 2115 | `	SyBlob sIn;` |
|    5 | 2116 | `	BIO *pIn = 0;` |
|    5 | 2117 | `	PKCS7 *pP7 = 0;` |
|    5 | 2118 | `	CMS_ContentInfo *pCms = 0;` |
|    5 | 2119 | `	STACK_OF(X509) *pCerts = 0;` |
|    5 | 2120 | `	const char *zData = 0;` |
|    5 | 2121 | `	int nData = 0,rc = -1,i;` |
|    - | 2122 | `	ph7_value *pArray,*pVal;` |
|    5 | 2123 | `	if( nArg < 2 ){` |
|  ! 0 | 2124 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2125 | `		return PH7_OK;` |
|    - | 2126 | `	}` |
|    - | 2127 | `	/* These two take BYTES, not a path -- so they read through the engine's` |
|    - | 2128 | `	 * stream layer like every other string argument in this extension. */` |
|    5 | 2129 | `	SyBlobInit(&sIn,&pCtx->pVm->sAllocator);` |
|    5 | 2130 | `	if( PH7_SslBytesOfValue(pCtx,apArg[0],&sIn,&zData,&nData) != 0 \|\| nData < 1 ){` |
|  ! 0 | 2131 | `		goto done;` |
|    - | 2132 | `	}` |
|    5 | 2133 | `	pIn = BIO_new_mem_buf(zData,nData);` |
|    5 | 2134 | `	if( pIn == 0 ){` |
|  ! 0 | 2135 | `		goto done;` |
|    - | 2136 | `	}` |
|    5 | 2137 | `	if( bCms ){` |
|    3 | 2138 | `		pCms = PEM_read_bio_CMS(pIn,0,0,0);` |
|    3 | 2139 | `		if( pCms == 0 ){` |
|    3 | 2140 | `			BIO_reset(pIn);` |
|    3 | 2141 | `			pCms = d2i_CMS_bio(pIn,0);` |
|    1 | 2142 | `		}` |
|    3 | 2143 | `		if( pCms == 0 ){` |
|    3 | 2144 | `			goto done;` |
|    - | 2145 | `		}` |
|  ! 0 | 2146 | `		pCerts = CMS_get1_certs(pCms);` |
|  ! 0 | 2147 | `	}else{` |
|    3 | 2148 | `		pP7 = PEM_read_bio_PKCS7(pIn,0,0,0);` |
|    3 | 2149 | `		if( pP7 == 0 ){` |
|    3 | 2150 | `			BIO_reset(pIn);` |
|    3 | 2151 | `			pP7 = d2i_PKCS7_bio(pIn,0);` |
|    1 | 2152 | `		}` |
|    3 | 2153 | `		if( pP7 == 0 ){` |
|    3 | 2154 | `			goto done;` |
|    - | 2155 | `		}` |
|  ! 0 | 2156 | `		if( PKCS7_type_is_signed(pP7) && pP7->d.sign ){` |
|  ! 0 | 2157 | `			pCerts = pP7->d.sign->cert;` |
|  ! 0 | 2158 | `		}else if( PKCS7_type_is_signedAndEnveloped(pP7) && pP7->d.signed_and_enveloped ){` |
|  ! 0 | 2159 | `			pCerts = pP7->d.signed_and_enveloped->cert;` |
|  ! 0 | 2160 | `		}` |
|    - | 2161 | `	}` |
|  ! 0 | 2162 | `	pArray = ph7_context_new_array(pCtx);` |
|  ! 0 | 2163 | `	pVal = ph7_context_new_scalar(pCtx);` |
|  ! 0 | 2164 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 | 2165 | `		goto done;` |
|    - | 2166 | `	}` |
|  ! 0 | 2167 | `	for( i = 0 ; pCerts && i < sk_X509_num(pCerts) ; ++i ){` |
|  ! 0 | 2168 | `		BIO *pOne = BIO_new(BIO_s_mem());` |
|  ! 0 | 2169 | `		if( pOne == 0 ){` |
|  ! 0 | 2170 | `			continue;` |
|    - | 2171 | `		}` |
|  ! 0 | 2172 | `		if( PEM_write_bio_X509(pOne,sk_X509_value(pCerts,i)) == 1 ){` |
|  ! 0 | 2173 | `			SslPemPut(pCtx,pArray,pVal,0,pOne);` |
|  ! 0 | 2174 | `		}` |
|  ! 0 | 2175 | `		BIO_free(pOne);` |
|  ! 0 | 2176 | `	}` |
|  ! 0 | 2177 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);` |
|  ! 0 | 2178 | `	rc = 0;` |
|    2 | 2179 | `done:` |
|    5 | 2180 | `	if( bCms && pCerts ){ sk_X509_pop_free(pCerts,X509_free); }` |
|    5 | 2181 | `	if( pP7 ){ PKCS7_free(pP7); }` |
|    5 | 2182 | `	if( pCms ){ CMS_ContentInfo_free(pCms); }` |
|    5 | 2183 | `	if( pIn ){ BIO_free(pIn); }` |
|    5 | 2184 | `	SyBlobRelease(&sIn);` |
|    5 | 2185 | `	if( rc != 0 ){` |
|    5 | 2186 | `		PH7_SslStoreErrors(pCtx->pVm);` |
|    2 | 2187 | `	}` |
|    5 | 2188 | `	ph7_result_bool(pCtx,rc == 0);` |
|    5 | 2189 | `	return PH7_OK;` |
|    3 | 2190 | `}` |
|    2 | 2191 | `static int vm_builtin_openssl_pkcs7_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2192 | `{` |
|    3 | 2193 | `	return SslMsgRead(pCtx,nArg,apArg,0);` |
|    1 | 2194 | `}` |
|    2 | 2195 | `static int vm_builtin_openssl_cms_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2196 | `{` |
|    3 | 2197 | `	return SslMsgRead(pCtx,nArg,apArg,1);` |
|    1 | 2198 | `}` |
|    - | 2199 |  |
|    - | 2200 | `/* ------------------------------------------------------------------------` |
|    - | 2201 | ` * Installation` |
|    - | 2202 | ` * ------------------------------------------------------------------------ */` |
| 7925 | 2203 | `PH7_PRIVATE sxi32 PH7_VmInstallOpenSslX509(ph7_vm *pVm)` |
|    5 | 2204 | `{` |
|    - | 2205 | `	/* The three handle classes are installed by the other unit, which mounts` |
|    - | 2206 | `	 * this one; there is nothing of its own to declare here. */` |
| 3957 | 2207 | `	SXUNUSED(pVm);` |
| 7930 | 2208 | `	return SXRET_OK;` |
|    5 | 2209 | `}` |
|    - | 2210 | `/* The functions this unit owns, in php's own registration order. */` |
| 7925 | 2211 | `PH7_PRIVATE const ph7_builtin_func * PH7_OpenSslX509FuncTable(sxu32 *pnEntry)` |
|    5 | 2212 | `{` |
|    - | 2213 | `	static const ph7_builtin_func aFunc[] = {` |
|    - | 2214 | `		{ "openssl_x509_export_to_file",    vm_builtin_openssl_x509_export_to_file    },` |
|    - | 2215 | `		{ "openssl_x509_export",            vm_builtin_openssl_x509_export            },` |
|    - | 2216 | `		{ "openssl_x509_fingerprint",       vm_builtin_openssl_x509_fingerprint       },` |
|    - | 2217 | `		{ "openssl_x509_check_private_key", vm_builtin_openssl_x509_check_private_key },` |
|    - | 2218 | `		{ "openssl_x509_verify",            vm_builtin_openssl_x509_verify            },` |
|    - | 2219 | `		{ "openssl_x509_parse",             vm_builtin_openssl_x509_parse             },` |
|    - | 2220 | `		{ "openssl_x509_checkpurpose",      vm_builtin_openssl_x509_checkpurpose      },` |
|    - | 2221 | `		{ "openssl_x509_read",              vm_builtin_openssl_x509_read              },` |
|    - | 2222 | `		{ "openssl_pkcs12_export_to_file",  vm_builtin_openssl_pkcs12_export_to_file  },` |
|    - | 2223 | `		{ "openssl_pkcs12_export",          vm_builtin_openssl_pkcs12_export          },` |
|    - | 2224 | `		{ "openssl_pkcs12_read",            vm_builtin_openssl_pkcs12_read            },` |
|    - | 2225 | `		{ "openssl_csr_export_to_file",     vm_builtin_openssl_csr_export_to_file     },` |
|    - | 2226 | `		{ "openssl_csr_export",             vm_builtin_openssl_csr_export             },` |
|    - | 2227 | `		{ "openssl_csr_sign",               vm_builtin_openssl_csr_sign               },` |
|    - | 2228 | `		{ "openssl_csr_new",                vm_builtin_openssl_csr_new                },` |
|    - | 2229 | `		{ "openssl_csr_get_subject",        vm_builtin_openssl_csr_get_subject        },` |
|    - | 2230 | `		{ "openssl_csr_get_public_key",     vm_builtin_openssl_csr_get_public_key     },` |
|    - | 2231 | `		{ "openssl_pkcs7_verify",           vm_builtin_openssl_pkcs7_verify           },` |
|    - | 2232 | `		{ "openssl_pkcs7_encrypt",          vm_builtin_openssl_pkcs7_encrypt          },` |
|    - | 2233 | `		{ "openssl_pkcs7_sign",             vm_builtin_openssl_pkcs7_sign             },` |
|    - | 2234 | `		{ "openssl_pkcs7_decrypt",          vm_builtin_openssl_pkcs7_decrypt          },` |
|    - | 2235 | `		{ "openssl_pkcs7_read",             vm_builtin_openssl_pkcs7_read             },` |
|    - | 2236 | `		{ "openssl_cms_verify",             vm_builtin_openssl_cms_verify             },` |
|    - | 2237 | `		{ "openssl_cms_encrypt",            vm_builtin_openssl_cms_encrypt            },` |
|    - | 2238 | `		{ "openssl_cms_sign",               vm_builtin_openssl_cms_sign               },` |
|    - | 2239 | `		{ "openssl_cms_decrypt",            vm_builtin_openssl_cms_decrypt            },` |
|    - | 2240 | `		{ "openssl_cms_read",               vm_builtin_openssl_cms_read               }` |
|    - | 2241 | `	};` |
| 7930 | 2242 | `	*pnEntry = (sxu32)SX_ARRAYSIZE(aFunc);` |
| 7930 | 2243 | `	return aFunc;` |
|    5 | 2244 | `}` |
|    - | 2245 |  |
|    - | 2246 | `#else` |
|    - | 2247 | `/* Ensure non-empty translation unit when openssl is disabled (MSVC C4206) */` |
|    - | 2248 | `typedef int vm_openssl_x509_unused;` |
|    - | 2249 | `#endif /* PH7_ENABLE_OPENSSL */` |
|    - | 2250 |  |
