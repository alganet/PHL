/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * Section:
 *    Hash (md5/sha1/crc32/hash family) and password_* (bcrypt) functions.
 * Status:
 *    Stable.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#define PH7_NEED_BUILTIN_REG 1
#endif
#ifdef PH7_NEED_BUILTIN_REG
#if !defined(PH7_DISABLE_HASH_FUNC)
/*
 * string md5(string $str[,bool $raw_output = false])
 *   Calculate the md5 hash of a string.
 * Parameter
 *  $str
 *   Input string
 * $raw_output
 *   If the optional raw_output is set to TRUE, then the md5 digest
 *   is instead returned in raw binary format with a length of 16.
 * Return
 *  MD5 Hash as a 32-character hexadecimal string.
 */
PH7_PRIVATE int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zDigest[16];
	int raw_output = FALSE;
	const void *pIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string (the empty string hashes to a well-defined
	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_bool(apArg[1])){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Compute the MD5 digest */
	SyMD5Compute(pIn,(sxu32)nLen,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string sha1(string $str[,bool $raw_output = false])
 *   Calculate the sha1 hash of a string.
 * Parameter
 *  $str
 *   Input string
 * $raw_output
 *   If the optional raw_output is set to TRUE, then the md5 digest
 *   is instead returned in raw binary format with a length of 16.
 * Return
 *  SHA1 Hash as a 40-character hexadecimal string.
 */
PH7_PRIVATE int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zDigest[20];
	int raw_output = FALSE;
	const void *pIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string (the empty string hashes to a well-defined
	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_bool(apArg[1])){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Compute the SHA1 digest */
	SySha1Compute(pIn,(sxu32)nLen,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * int64 crc32(string $str)
 *   Calculates the crc32 polynomial of a strin.
 * Parameter
 *  $str
 *   Input string
 * Return
 *  CRC32 checksum of the given input (64-bit integer).
 */
PH7_PRIVATE int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const void *pIn;
	sxu32 nCRC;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike
		 * md5()/sha1(), whose empty-string digests are non-zero. */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Calculate the sum */
	nCRC = SyCrc32(pIn,(sxu32)nLen);
	/* Return the CRC32 as 64-bit integer */
	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);
	return PH7_OK;
}
/*
 * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is
 * described by a small record so one dispatch (and one generic HMAC) serves them
 * all. Thin adapters normalize the differing context types and the reversed
 * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.
 */
static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }
static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }
static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }
static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }
static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }
static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }
static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }
static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }
static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }
static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }
static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }
static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }
static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }
static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }
typedef struct HashAlgo HashAlgo;
struct HashAlgo {
	const char *zName;   /* lowercase canonical name */
	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */
	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */
	void (*xInit)(HashCtx *);
	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);
	void (*xFinal)(HashCtx *,unsigned char *);
};
static const HashAlgo aHashAlgo[] = {
	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },
	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },
	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },
	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },
	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },
	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },
};
/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */
static const HashAlgo * HashFindAlgo(const char *zName,int nLen){
	sxu32 i;
	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){
		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen
			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){
			return &aHashAlgo[i];
		}
	}
	return 0;
}
/*
 * string hash(string $algo,string $data[,bool $binary = false])
 *   Generate a hash value (message digest).
 */
PH7_PRIVATE int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const char *zAlgo,*zData;
	int nAlgoLen,nDataLen,raw_output = FALSE;
	HashCtx sCtx;
	unsigned char zDigest[64];
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash() expects at least 2 arguments, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	if( pAlgo == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");
	}
	zData = ph7_value_to_string(apArg[1],&nDataLen);
	if( nArg > 2 ){
		raw_output = ph7_value_to_bool(apArg[2]);
	}
	pAlgo->xInit(&sCtx);
	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);
	pAlgo->xFinal(&sCtx,zDigest);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])
 *   Generate a keyed hash value using the HMAC method (RFC 2104).
 */
PH7_PRIVATE int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const char *zAlgo,*zData,*zKey;
	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;
	HashCtx sCtx;
	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];
	int i,nBlock,nDigest;
	if( nArg < 3 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_hmac() expects at least 3 arguments, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	if( pAlgo == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");
	}
	zData = ph7_value_to_string(apArg[1],&nDataLen);
	zKey = ph7_value_to_string(apArg[2],&nKeyLen);
	if( nArg > 3 ){
		raw_output = ph7_value_to_bool(apArg[3]);
	}
	nBlock = pAlgo->nBlockLen;
	nDigest = pAlgo->nDigestLen;
	/* Reduce the key to a single block: hash it if longer than the block, then
	 * zero-pad (a short or empty key is just zero-padded). */
	SyZero(zKeyBlock,sizeof(zKeyBlock));
	if( nKeyLen > nBlock ){
		pAlgo->xInit(&sCtx);
		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);
		pAlgo->xFinal(&sCtx,zKeyBlock);
	}else if( nKeyLen > 0 ){
		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);
	}
	for( i = 0; i < nBlock; i++ ){
		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);
		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);
	}
	/* inner = H((key ^ ipad) || data) */
	pAlgo->xInit(&sCtx);
	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);
	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);
	pAlgo->xFinal(&sCtx,zInner);
	/* out = H((key ^ opad) || inner) */
	pAlgo->xInit(&sCtx);
	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);
	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);
	pAlgo->xFinal(&sCtx,zDigest);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,nDigest);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * bool hash_equals(string $known_string,string $user_string)
 *   Timing-attack-safe string comparison.
 */
PH7_PRIVATE int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zKnown,*zUser;
	int nKnown,nUser,i;
	volatile unsigned char vDiff = 0;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_equals() expects exactly 2 arguments, %d given",nArg);
	}
	if( !ph7_value_is_string(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",
			ph7_type_name(apArg[0]));
	}
	if( !ph7_value_is_string(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",
			ph7_type_name(apArg[1]));
	}
	zKnown = ph7_value_to_string(apArg[0],&nKnown);
	zUser = ph7_value_to_string(apArg[1],&nUser);
	if( nKnown != nUser ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Constant-time: read every byte, never short-circuit. */
	for( i = 0; i < nKnown; i++ ){
		vDiff |= (unsigned char)(zKnown[i] ^ zUser[i]);
	}
	ph7_result_bool(pCtx,vDiff == 0);
	return PH7_OK;
}
/*
 * array hash_algos(void)
 *   Return a list of the registered hashing algorithms.
 */
PH7_PRIVATE int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue;
	sxu32 i;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){
		ph7_value_string(pValue,aHashAlgo[i].zName,-1);
		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);
		ph7_value_reset_string_cursor(pValue);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
/*
 * password_* (bcrypt). These live in ext/standard in real PHP — outside the
 * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.
 */
/*
 * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a
 * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).
 */
static int BcryptParseHash(const char *zHash,int nHash,int *piCost)
{
	int iCost;
	if( nHash != 60 || zHash[0] != '$' || zHash[1] != '2' || zHash[3] != '$'
		|| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){
		return FALSE;
	}
	if( zHash[4] < '0' || zHash[4] > '9' || zHash[5] < '0' || zHash[5] > '9' || zHash[6] != '$' ){
		return FALSE;
	}
	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');
	if( iCost < 4 || iCost > 31 ){
		return FALSE;
	}
	if( piCost ){ *piCost = iCost; }
	return TRUE;
}
/*
 * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the
 * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.
 */
static int BcryptIsBcryptAlgo(ph7_value *pAlgo)
{
	if( ph7_value_is_null(pAlgo) ){
		return TRUE;
	}
	if( ph7_value_is_string(pAlgo) ){
		int nAlgo;
		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);
		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );
	}
	return FALSE;
}
/*
 * bool|string password_hash(string $password,string|int|null $algo[,array $options])
 *  Create a bcrypt hash of the password.
 */
PH7_PRIVATE int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPwd;
	int nPwd,iCost = 12;
	unsigned char aSalt[16];
	char zHash[60];
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_hash() expects at least 2 arguments, %d given",nArg);
	}
	if( !BcryptIsBcryptAlgo(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");
	}
	/* cost from $options['cost'] (default 12). */
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);
		if( pCost ){ iCost = ph7_value_to_int(pCost); }
	}
	if( iCost < 4 || iCost > 31 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Invalid bcrypt cost parameter specified: %d",iCost);
	}
	zPwd = ph7_value_to_string(apArg[0],&nPwd);
	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"Exception",
			"password_hash(): unable to gather sufficient entropy for the salt");
	}
	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));
	return PH7_OK;
}
/*
 * bool password_verify(string $password,string $hash)
 *  Verify a password against a bcrypt hash. Never throws on a malformed hash.
 */
PH7_PRIVATE int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPwd,*zHash;
	int nPwd,nHash,iCost,i;
	unsigned char aSalt[16];
	char zComputed[60];
	volatile unsigned char vDiff = 0;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_verify() expects exactly 2 arguments, %d given",nArg);
	}
	zPwd = ph7_value_to_string(apArg[0],&nPwd);
	zHash = ph7_value_to_string(apArg[1],&nHash);
	if( !BcryptParseHash(zHash,nHash,&iCost) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */
	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps
	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */
	for( i = 29; i < 60; i++ ){
		vDiff |= (unsigned char)(zComputed[i] ^ zHash[i]);
	}
	ph7_result_bool(pCtx,vDiff == 0);
	return PH7_OK;
}
/*
 * array password_get_info(string $hash)
 *  Return ["algo"=>id|null, "algoName"=>name, "options"=>[...]].
 */
PH7_PRIVATE int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHash = "";
	int nHash,iCost = 0,bBcrypt = 0;
	ph7_value *pArray,*pOptions,*pVal;
	if( nArg > 0 ){
		zHash = ph7_value_to_string(apArg[0],&nHash);
		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);
	}
	pArray = ph7_context_new_array(pCtx);
	pOptions = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pOptions == 0 || pVal == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( bBcrypt ){
		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */
		ph7_array_add_strkey_elem(pArray,"algo",pVal);
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);
		ph7_array_add_strkey_elem(pArray,"algoName",pVal);
		ph7_value_int(pVal,iCost);
		ph7_array_add_strkey_elem(pOptions,"cost",pVal);
	}else{
		ph7_value_null(pVal);                          /* algo => null */
		ph7_array_add_strkey_elem(pArray,"algo",pVal);
		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);
		ph7_array_add_strkey_elem(pArray,"algoName",pVal);
	}
	ph7_array_add_strkey_elem(pArray,"options",pOptions);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool password_needs_rehash(string $hash,string|int|null $algo[,array $options])
 *  True if the hash was not made with the given algo/options.
 */
PH7_PRIVATE int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHash;
	int nHash,iCost = 0,iWantCost = 12;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);
	}
	zHash = ph7_value_to_string(apArg[0],&nHash);
	if( !BcryptParseHash(zHash,nHash,&iCost) || !BcryptIsBcryptAlgo(apArg[1]) ){
		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);
		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }
	}
	ph7_result_bool(pCtx,iCost != iWantCost);
	return PH7_OK;
}
#endif /* PH7_NEED_BUILTIN_REG */
