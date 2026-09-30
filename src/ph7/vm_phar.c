/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#ifndef PH7_DISABLE_BUILTIN_FUNC
#include <stdio.h>
#include <time.h>   /* an entry's modification time, stamped as php stamps it */
#ifdef PH7_ENABLE_ZLIB
#include <zlib.h>
#endif
/*
 * Section:
 *    php's phar extension: an archive that is also a php program.
 * Status:
 *    Stable.
 *
 * A .phar is one file with three parts: a php STUB that runs when the file is
 * executed, a MANIFEST describing what is inside, and the entries' bytes. The
 * stub ends in `__halt_compiler();`, which is what stops php reading the binary
 * behind it as code -- so the whole format rests on that one language feature,
 * and it is why the compiler learned it in the same commit.
 *
 * That shape is the reason this extension matters out of proportion to its
 * size: `composer.phar` and `phpunit.phar` are how nearly everyone actually
 * INSTALLS those two tools, and neither could be run at all without it.
 *
 * WHAT AN ARCHIVE IS HERE. One `phl_phar` per open archive, holding the whole
 * FILE in memory and an entry table over it. php keeps the file open and seeks;
 * this reads it once, which costs the archive's size in memory (6MB for
 * phpunit.phar) and buys a reader that works over any stream the engine can
 * open -- an archive inside another archive included.
 *
 * THREE FORMATS, ONE MODEL. php's Phar reads its own format, ustar TAR and ZIP,
 * and a PharData is the same reader with execution turned off. The three
 * readers below fill the same entry table, so everything above them --- the
 * wrapper, the classes, the iteration -- is written once.
 *
 * WHAT IS REPRODUCED, and where it is worth saying why:
 *
 *   - The MANIFEST, exactly: php's api version, the global and per-entry flag
 *     words, the alias, the serialized metadata (both levels), and the
 *     signature block whose trailing `GBMB` is what says an archive is signed.
 *   - The SIGNATURE is verified on open, as php does with `phar.require_hash`
 *     on: a corrupt archive is refused rather than half-read.
 *   - `phar.readonly`, php's own default, which is why every write door here
 *     answers a refusal until the ini is turned off -- and the refusals are
 *     php's own five different sentences, one per door.
 *   - The dual nature of a Phar OBJECT: it is an archive AND a directory
 *     iterator over its own entries (php's `spl_filesystem_object`), so
 *     `getPath()` answers the archive while `getFilename()` answers whichever
 *     entry the cursor is on.
 *
 * WHAT IS NOT HERE, and why each is honest rather than missing:
 *
 *   - OpenSSL signatures. php refuses them too when its own build has no
 *     openssl (`getSupportedSignatures()` lists what a build can do, and this
 *     one lists the four hashes it has).
 *   - BZ2 compression, for the same reason: `Phar::canCompress(Phar::BZ2)` is
 *     false here exactly as it is in a php built without ext/bz2.
 *   - `webPhar()` and `mungServer()`, which route a WEB request into an
 *     archive. They are a SAPI feature rather than an archive one.
 */
#define PHAR_FORMAT_PHAR 1
#define PHAR_FORMAT_TAR  2
#define PHAR_FORMAT_ZIP  3
/* php's Phar:: class constants, which are also its wire values. */
#define PHAR_C_NONE   0x0000
#define PHAR_C_GZ     0x1000
#define PHAR_C_BZ2    0x2000
#define PHAR_C_MASK   0xF000
/* The manifest's global flags. */
#define PHAR_GF_SIGNED 0x00010000
/* Signature kinds. These are php's Phar::MD5/SHA1/SHA256/SHA512 class constants
 * AND the value written in the trailing block -- one number, two faces. */
#define PHAR_SIG_MD5    1
#define PHAR_SIG_SHA1   2
#define PHAR_SIG_SHA256 3
#define PHAR_SIG_SHA512 4
#define PHAR_SIG_OPENSSL 16
/* php's own api version for the archives it writes: 1.1.1 in the class's
 * `apiVersion()`, and 1.1.0 in the manifest word (php stores the api version it
 * WRITES, which is one revision behind the reader's). */
#define PHAR_API_WORD   0x1100
#define PHAR_API_STRING "1.1.1"

typedef struct phl_phar phl_phar;
typedef struct phl_phar_ent phl_phar_ent;
/* One member of an archive. A DIRECTORY entry has no bytes; php writes them for
 * an explicitly-added empty directory and infers every other directory from the
 * names, which is why `is_dir('phar://x.phar/src')` is true in an archive that
 * has no `src` entry at all. */
struct phl_phar_ent {
	SyBlob sName;        /* the name inside the archive, `/`-separated, no leading slash */
	SyBlob sData;        /* the UNCOMPRESSED bytes, filled on first read */
	SyBlob sMeta;        /* php-serialized per-entry metadata, or empty */
	sxu32 nSize;         /* uncompressed length */
	sxu32 nCompSize;     /* stored length */
	sxu32 nCrc;          /* crc32 of the uncompressed bytes */
	sxu32 nFlags;        /* permission bits + the compression nibble */
	sxi64 iTime;         /* modification time */
	sxi64 iOffset;       /* where the stored bytes start in the archive file */
	sxu8 bLoaded;        /* sData holds the bytes */
	sxu8 bDir;           /* an explicit directory entry */
	phl_phar_ent *pNext; /* manifest order */
};
struct phl_phar {
	ph7_vm *pVm;
	SyBlob sPath;        /* the archive file, as the engine resolved it */
	SyBlob sAlias;       /* the alias its manifest carries, or one mapPhar set */
	SyBlob sStub;        /* everything before the manifest */
	SyBlob sMeta;        /* php-serialized archive metadata */
	SyBlob sFile;        /* the whole archive, as read */
	SyBlob sSig;         /* the signature bytes, as stored */
	int iFormat;
	int iSigType;
	int bData;           /* a PharData: never executable */
	int bAliasFromManifest;
	sxu32 nApi;
	phl_phar_ent *pFirst,*pLast;
	sxu32 nEnt;
	phl_phar *pNext;     /* the VM's registry chain */
};
/* ------------------------------------------------------------------ */
/* The registry                                                        */
/* ------------------------------------------------------------------ */
static void PharEntFree(ph7_vm *pVm,phl_phar_ent *pEnt)
{
	SyBlobRelease(&pEnt->sName);
	SyBlobRelease(&pEnt->sData);
	SyBlobRelease(&pEnt->sMeta);
	SyMemBackendFree(&pVm->sAllocator,pEnt);
}
static void PharFree(phl_phar *pPhar)
{
	ph7_vm *pVm = pPhar->pVm;
	phl_phar_ent *pEnt = pPhar->pFirst;
	while( pEnt ){
		phl_phar_ent *pNext = pEnt->pNext;
		PharEntFree(pVm,pEnt);
		pEnt = pNext;
	}
	SyBlobRelease(&pPhar->sPath);
	SyBlobRelease(&pPhar->sAlias);
	SyBlobRelease(&pPhar->sStub);
	SyBlobRelease(&pPhar->sMeta);
	SyBlobRelease(&pPhar->sFile);
	SyBlobRelease(&pPhar->sSig);
	SyMemBackendFree(&pVm->sAllocator,pPhar);
}
/*
 * Every archive this run opened, freed together. php's own cache is per-request
 * and behaves the same way: an archive opened by one request is not the next
 * one's, which for the -S server is the difference between a stale manifest and
 * a fresh read.
 */
static void PharVmSweep(ph7_vm *pVm)
{
	phl_phar *p = (phl_phar *)pVm->pPhars;
	while( p ){
		phl_phar *pNext = p->pNext;
		PharFree(p);
		p = pNext;
	}
	pVm->pPhars = 0;
	SyBlobRelease(&pVm->sPharRunning);
	SyBlobRelease(&pVm->sPharErr);
}
PH7_PRIVATE void PH7_PharVmReset(ph7_vm *pVm)
{
	PharVmSweep(&(*pVm));
}
PH7_PRIVATE void PH7_PharVmRelease(ph7_vm *pVm)
{
	PharVmSweep(&(*pVm));
}
/* The archive already open under this exact path, or 0. */
static phl_phar * PharFindByPath(ph7_vm *pVm,const char *zPath,int nPath)
{
	phl_phar *p;
	for( p = (phl_phar *)pVm->pPhars ; p ; p = p->pNext ){
		if( (int)SyBlobLength(&p->sPath) == nPath
		 && SyMemcmp(SyBlobData(&p->sPath),zPath,(sxu32)nPath) == 0 ){
			return p;
		}
	}
	return 0;
}
/* ...or under this alias, which is what `phar://composer.phar/bin/composer`
 * names once the stub has run `Phar::mapPhar('composer.phar')`. */
static phl_phar * PharFindByAlias(ph7_vm *pVm,const char *zAlias,int nAlias)
{
	phl_phar *p;
	if( nAlias < 1 ){
		return 0;
	}
	for( p = (phl_phar *)pVm->pPhars ; p ; p = p->pNext ){
		if( (int)SyBlobLength(&p->sAlias) == nAlias
		 && SyMemcmp(SyBlobData(&p->sAlias),zAlias,(sxu32)nAlias) == 0 ){
			return p;
		}
	}
	return 0;
}
/*
 * The name an entry is looked up under. php normalizes a phar path before it
 * looks: `bin/../src/bootstrap.php` IS `src/bootstrap.php` inside the archive,
 * and every real stub relies on it -- composer's `require __DIR__.'/../src/…'`
 * is exactly that shape, since __DIR__ inside an archive is a phar:// url.
 */
static void PharNormalize(const char *zIn,int nIn,SyBlob *pOut)
{
	int i = 0;
	SyBlobReset(pOut);
	while( i < nIn ){
		int nSeg;
		while( i < nIn && zIn[i] == '/' ){
			i++;
		}
		nSeg = 0;
		while( i + nSeg < nIn && zIn[i+nSeg] != '/' ){
			nSeg++;
		}
		if( nSeg == 0 ){
			break;
		}
		if( nSeg == 1 && zIn[i] == '.' ){
			i += nSeg;
			continue;
		}
		if( nSeg == 2 && zIn[i] == '.' && zIn[i+1] == '.' ){
			/* Step back over the last segment kept, if any. */
			sxu32 n = SyBlobLength(pOut);
			while( n > 0 && ((const char *)SyBlobData(pOut))[n-1] != '/' ){
				n--;
			}
			pOut->nByte = n > 0 ? n - 1 : 0;
			i += nSeg;
			continue;
		}
		if( SyBlobLength(pOut) > 0 ){
			SyBlobAppend(pOut,"/",1);
		}
		SyBlobAppend(pOut,&zIn[i],(sxu32)nSeg);
		i += nSeg;
	}
}
static phl_phar_ent * PharFindEnt(phl_phar *pPhar,const char *zName,int nName)
{
	phl_phar_ent *pEnt;
	SyBlob sNorm;
	SyBlobInit(&sNorm,&pPhar->pVm->sAllocator);
	PharNormalize(zName,nName,&sNorm);
	zName = (const char *)SyBlobData(&sNorm);
	nName = (int)SyBlobLength(&sNorm);
	pEnt = 0;
	{
		phl_phar_ent *p;
		for( p = pPhar->pFirst ; p ; p = p->pNext ){
			if( (int)SyBlobLength(&p->sName) == nName
			 && SyMemcmp(SyBlobData(&p->sName),zName,(sxu32)nName) == 0 ){
				pEnt = p;
				break;
			}
		}
	}
	SyBlobRelease(&sNorm);
	return pEnt;
}
/* Is this name a DIRECTORY in the archive -- an entry of its own, or the prefix
 * of one? php infers the second kind, which is why every archive has directories
 * its manifest never mentions. */
static int PharIsDir(phl_phar *pPhar,const char *zName,int nName)
{
	phl_phar_ent *pEnt;
	SyBlob sNorm;
	int bAnswer = 0;
	SyBlobInit(&sNorm,&pPhar->pVm->sAllocator);
	PharNormalize(zName,nName,&sNorm);
	zName = (const char *)SyBlobData(&sNorm);
	nName = (int)SyBlobLength(&sNorm);
	if( nName < 1 ){
		SyBlobRelease(&sNorm);
		return 1;   /* the archive root */
	}
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		const char *z = (const char *)SyBlobData(&pEnt->sName);
		sxu32 n = SyBlobLength(&pEnt->sName);
		if( pEnt->bDir && (int)n == nName && SyMemcmp(z,zName,(sxu32)nName) == 0 ){
			bAnswer = 1;
			break;
		}
		if( (int)n > nName && z[nName] == '/' && SyMemcmp(z,zName,(sxu32)nName) == 0 ){
			bAnswer = 1;
			break;
		}
	}
	SyBlobRelease(&sNorm);
	return bAnswer;
}
/* ------------------------------------------------------------------ */
/* Reading the file                                                    */
/* ------------------------------------------------------------------ */
/* Drop the last byte of a blob: the trailing `/` that marks a directory entry
 * in every one of the three formats. */
static void PharChopSlash(SyBlob *pBlob)
{
	if( SyBlobLength(pBlob) > 0 ){
		pBlob->nByte--;
	}
}
static sxu32 PharGet32(const unsigned char *z)
{
	return (sxu32)z[0] | ((sxu32)z[1] << 8) | ((sxu32)z[2] << 16) | ((sxu32)z[3] << 24);
}
static sxu32 PharGet16(const unsigned char *z)
{
	return (sxu32)z[0] | ((sxu32)z[1] << 8);
}
/* php stores the manifest's api version MSB first, alone among its fields. */
static sxu32 PharGet16BE(const unsigned char *z)
{
	return ((sxu32)z[0] << 8) | (sxu32)z[1];
}
/*
 * Where the manifest starts: php looks for the halt token and then steps over
 * an optional ` ?>` and ONE line ending, which is what its own writer emits.
 * A file with no token at all is not a phar-format archive.
 */
static int PharFindManifest(const unsigned char *zFile,sxu32 nFile,sxu32 *pnOff)
{
	static const char zTok[] = "__HALT_COMPILER();";
	sxu32 nTok = (sxu32)sizeof(zTok)-1;
	sxu32 i;
	for( i = 0 ; i + nTok <= nFile ; ++i ){
		if( zFile[i] != '_' ){
			continue;
		}
		if( SyStrnicmp((const char *)&zFile[i],zTok,nTok) != 0 ){
			continue;
		}
		i += nTok;
		/* php's writer puts ` ?>` and a line ending here; a hand-made stub may
		 * put any run of blanks, or nothing at all. */
		while( i < nFile && (zFile[i] == ' ' || zFile[i] == '\t') ){
			i++;
		}
		if( i + 1 < nFile && zFile[i] == '?' && zFile[i+1] == '>' ){
			i += 2;
		}
		if( i < nFile && zFile[i] == '\r' ){
			i++;
		}
		if( i < nFile && zFile[i] == '\n' ){
			i++;
		}
		*pnOff = i;
		return 1;
	}
	return 0;
}
static phl_phar_ent * PharNewEnt(phl_phar *pPhar,const char *zName,int nName)
{
	ph7_vm *pVm = pPhar->pVm;
	phl_phar_ent *pEnt = (phl_phar_ent *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar_ent));
	if( pEnt == 0 ){
		return 0;
	}
	SyZero(pEnt,sizeof(*pEnt));
	SyBlobInit(&pEnt->sName,&pVm->sAllocator);
	SyBlobInit(&pEnt->sData,&pVm->sAllocator);
	SyBlobInit(&pEnt->sMeta,&pVm->sAllocator);
	while( nName > 0 && zName[0] == '/' ){
		zName++; nName--;
	}
	SyBlobAppend(&pEnt->sName,zName,(sxu32)nName);
	if( pPhar->pLast ){
		pPhar->pLast->pNext = pEnt;
	}else{
		pPhar->pFirst = pEnt;
	}
	pPhar->pLast = pEnt;
	pPhar->nEnt++;
	return pEnt;
}
/*
 * php's own format. Everything is little-endian but the api version, lengths
 * are unsigned 32-bit, and the per-entry records come first as a block with the
 * stored bytes behind them in the same order.
 */
static int PharParsePhar(phl_phar *pPhar,const unsigned char *zFile,sxu32 nFile,
	sxu32 nOff,const char **pzErr)
{
	sxu32 nManifest,nFiles,nFlags,nAliasLen,nMetaLen,i,q,nDataAt;
	if( nOff > nFile || nFile - nOff < 18 ){
		*pzErr = "internal corruption of phar (truncated manifest header)";
		return -1;
	}
	nManifest = PharGet32(&zFile[nOff]);
	nFiles = PharGet32(&zFile[nOff+4]);
	pPhar->nApi = PharGet16BE(&zFile[nOff+8]);
	nFlags = PharGet32(&zFile[nOff+10]);
	nAliasLen = PharGet32(&zFile[nOff+14]);
	if( nManifest < 1 || nFile - nOff < 4 || nManifest > nFile - nOff - 4 ){
		*pzErr = "internal corruption of phar (truncated manifest header)";
		return -1;
	}
	q = nOff + 18;
	if( q > nFile || nAliasLen > nFile - q ){
		*pzErr = "internal corruption of phar (truncated manifest header)";
		return -1;
	}
	if( nAliasLen > 0 ){
		SyBlobAppend(&pPhar->sAlias,&zFile[q],nAliasLen);
		pPhar->bAliasFromManifest = 1;
	}
	q += nAliasLen;
	if( q > nFile || nFile - q < 4 ){
		*pzErr = "internal corruption of phar (truncated manifest header)";
		return -1;
	}
	nMetaLen = PharGet32(&zFile[q]);
	q += 4;
	if( nMetaLen > nFile - q ){
		*pzErr = "internal corruption of phar (truncated manifest header)";
		return -1;
	}
	if( nMetaLen > 0 ){
		SyBlobAppend(&pPhar->sMeta,&zFile[q],nMetaLen);
	}
	q += nMetaLen;
	/* The entry records, then their bytes in the same order. Every bound below
	 * is written as a REMAINDER (`n > nFile - q`) rather than a sum: the lengths
	 * come out of the file being parsed, and `q + n > nFile` wraps for one near
	 * 2^32 and lets the read through. */
	nDataAt = nOff + 4 + nManifest;
	for( i = 0 ; i < nFiles ; ++i ){
		phl_phar_ent *pEnt;
		sxu32 nNameLen,nEntMeta;
		if( q > nFile || nFile - q < 4 ){
			*pzErr = "internal corruption of phar (truncated manifest entry)";
			return -1;
		}
		nNameLen = PharGet32(&zFile[q]);
		q += 4;
		if( nNameLen > nFile - q || nFile - q - nNameLen < 24 ){
			*pzErr = "internal corruption of phar (truncated manifest entry)";
			return -1;
		}
		pEnt = PharNewEnt(pPhar,(const char *)&zFile[q],(int)nNameLen);
		if( pEnt == 0 ){
			*pzErr = "internal corruption of phar (out of memory)";
			return -1;
		}
		q += nNameLen;
		pEnt->nSize = PharGet32(&zFile[q]);
		pEnt->iTime = (sxi64)PharGet32(&zFile[q+4]);
		pEnt->nCompSize = PharGet32(&zFile[q+8]);
		pEnt->nCrc = PharGet32(&zFile[q+12]);
		pEnt->nFlags = PharGet32(&zFile[q+16]);
		nEntMeta = PharGet32(&zFile[q+20]);
		q += 24;
		if( nEntMeta > nFile - q ){
			*pzErr = "internal corruption of phar (truncated manifest entry)";
			return -1;
		}
		if( nEntMeta > 0 ){
			SyBlobAppend(&pEnt->sMeta,&zFile[q],nEntMeta);
		}
		q += nEntMeta;
		pEnt->iOffset = (sxi64)nDataAt;
		if( nDataAt > nFile || pEnt->nCompSize > nFile - nDataAt ){
			*pzErr = "internal corruption of phar (truncated entry)";
			return -1;
		}
		nDataAt += pEnt->nCompSize;
		/* php marks a directory entry by a trailing slash in its NAME. */
		if( SyBlobLength(&pEnt->sName) > 0 ){
			char *zN = (char *)SyBlobData(&pEnt->sName);
			sxu32 nN = SyBlobLength(&pEnt->sName);
			if( zN[nN-1] == '/' ){
				pEnt->bDir = 1;
				PharChopSlash(&pEnt->sName);
			}
		}
	}
	if( nFlags & PHAR_GF_SIGNED ){
		/* The signature block sits at the very end: the digest, its kind and
		 * `GBMB`. */
		if( nFile >= 8 && SyMemcmp(&zFile[nFile-4],"GBMB",4) == 0 ){
			sxu32 nSigType = PharGet32(&zFile[nFile-8]);
			sxu32 nSigLen = 0;
			switch( nSigType ){
			case PHAR_SIG_MD5:    nSigLen = 16; break;
			case PHAR_SIG_SHA1:   nSigLen = 20; break;
			case PHAR_SIG_SHA256: nSigLen = 32; break;
			case PHAR_SIG_SHA512: nSigLen = 64; break;
			default: break;
			}
			pPhar->iSigType = (int)nSigType;
			if( nSigLen > 0 && nFile >= nSigLen + 8 ){
				SyBlobAppend(&pPhar->sSig,&zFile[nFile-8-nSigLen],nSigLen);
			}
		}
	}
	SyBlobAppend(&pPhar->sStub,zFile,nOff);
	pPhar->iFormat = PHAR_FORMAT_PHAR;
	return 0;
}
/*
 * The signature php checks on every open with `phar.require_hash` on (its own
 * default): the digest covers the whole file EXCEPT the trailing block, so a
 * changed byte anywhere -- stub, manifest or an entry -- is caught.
 */
static int PharVerifySignature(phl_phar *pPhar,SyBlob *pHashOut)
{
	const unsigned char *zFile = (const unsigned char *)SyBlobData(&pPhar->sFile);
	sxu32 nFile = SyBlobLength(&pPhar->sFile);
	sxu32 nSig = SyBlobLength(&pPhar->sSig);
	unsigned char zDigest[64];
	sxu32 nDigest = 0;
	sxu32 nCovered;
	if( pPhar->iSigType == 0 || nSig == 0 ){
		return 0;
	}
	if( nFile < nSig + 8 ){
		return -1;
	}
	nCovered = nFile - nSig - 8;
	switch( pPhar->iSigType ){
	case PHAR_SIG_MD5: {
		MD5Context sMd5;
		MD5Init(&sMd5);
		MD5Update(&sMd5,zFile,nCovered);
		MD5Final(zDigest,&sMd5);
		nDigest = 16;
		break; }
	case PHAR_SIG_SHA1: {
		SHA1Context sSha;
		SHA1Init(&sSha);
		SHA1Update(&sSha,zFile,nCovered);
		SHA1Final(&sSha,zDigest);
		nDigest = 20;
		break; }
	case PHAR_SIG_SHA256: {
		SHA256Context sSha;
		SHA256Init(&sSha);
		SHA256Update(&sSha,zFile,nCovered);
		SHA256Final(&sSha,zDigest);
		nDigest = 32;
		break; }
	case PHAR_SIG_SHA512: {
		SHA512Context sSha;
		SHA512Init(&sSha);
		SHA512Update(&sSha,zFile,nCovered);
		SHA512Final(&sSha,zDigest);
		nDigest = 64;
		break; }
	default:
		/* An OpenSSL signature: php's own build refuses it without the
		 * extension, and so does this one. */
		return -1;
	}
	if( pHashOut ){
		SyBlobAppend(pHashOut,zDigest,nDigest);
	}
	if( nDigest != nSig || SyMemcmp(zDigest,SyBlobData(&pPhar->sSig),nDigest) != 0 ){
		return -1;
	}
	return 0;
}
/*
 * ustar. php reads a tar with `PharData`, and the format is simple enough that
 * the whole of it is here: the 512-byte header, its octal fields, the GNU long
 * name extension and the two zero blocks that end the archive.
 */
static sxi64 PharTarOctal(const char *z,int n)
{
	sxi64 v = 0;
	int i;
	for( i = 0 ; i < n ; ++i ){
		if( z[i] >= '0' && z[i] <= '7' ){
			v = v * 8 + (z[i] - '0');
		}else if( z[i] == ' ' || z[i] == 0 ){
			if( v != 0 || i > 0 ){
				break;
			}
		}
	}
	return v;
}
static int PharParseTar(phl_phar *pPhar,const unsigned char *zFile,sxu32 nFile,const char **pzErr)
{
	sxu32 nPos = 0;
	SyBlob sLongName;
	int bAny = 0;
	SyBlobInit(&sLongName,&pPhar->pVm->sAllocator);
	/* Every bound is a REMAINDER against nFile: the sizes come out of the file
	 * being parsed, and a sum overflows for one a hostile archive can spell. */
	while( nPos <= nFile && nFile - nPos >= 512 ){
		const char *zHdr = (const char *)&zFile[nPos];
		sxi64 nSize;
		sxu32 nAdv;
		int nName;
		char cType;
		if( zHdr[0] == 0 ){
			break;   /* the end-of-archive blocks */
		}
		if( SyMemcmp(&zHdr[257],"ustar",5) != 0 ){
			SyBlobRelease(&sLongName);
			*pzErr = "truncated entry";
			return -1;
		}
		nSize = PharTarOctal(&zHdr[124],12);
		if( nSize < 0 || nSize > (sxi64)nFile ){
			SyBlobRelease(&sLongName);
			*pzErr = "truncated entry";
			return -1;
		}
		cType = zHdr[156];
		for( nName = 0 ; nName < 100 && zHdr[nName] ; ++nName ){}
		nPos += 512;
		nAdv = (sxu32)((nSize + 511) & ~(sxi64)511);
		if( cType == 'L' ){
			/* GNU long name: the NAME is the next entry's payload. */
			SyBlobReset(&sLongName);
			if( (sxu32)nSize <= nFile - nPos ){
				sxu32 n = (sxu32)nSize;
				while( n > 0 && zFile[nPos+n-1] == 0 ){
					n--;
				}
				SyBlobAppend(&sLongName,&zFile[nPos],n);
			}
			if( nAdv > nFile - nPos ){
				break;
			}
			nPos += nAdv;
			continue;
		}
		if( cType == '0' || cType == 0 || cType == '5' ){
			phl_phar_ent *pEnt;
			if( SyBlobLength(&sLongName) > 0 ){
				pEnt = PharNewEnt(pPhar,(const char *)SyBlobData(&sLongName),
					(int)SyBlobLength(&sLongName));
				SyBlobReset(&sLongName);
			}else{
				pEnt = PharNewEnt(pPhar,zHdr,nName);
			}
			if( pEnt == 0 ){
				SyBlobRelease(&sLongName);
				*pzErr = "internal corruption of tar (out of memory)";
				return -1;
			}
			if( cType == '5' ){
				pEnt->bDir = 1;
				if( SyBlobLength(&pEnt->sName) > 0 ){
					char *zN = (char *)SyBlobData(&pEnt->sName);
					if( zN[SyBlobLength(&pEnt->sName)-1] == '/' ){
						PharChopSlash(&pEnt->sName);
					}
				}
				nSize = 0;
			}
			pEnt->nSize = (sxu32)nSize;
			pEnt->nCompSize = (sxu32)nSize;
			pEnt->iTime = PharTarOctal(&zHdr[136],12);
			pEnt->nFlags = (sxu32)(PharTarOctal(&zHdr[100],8) & 0x1FF);
			pEnt->iOffset = (sxi64)nPos;
			bAny = 1;
		}
		if( nAdv > nFile - nPos ){
			break;
		}
		nPos += nAdv;
	}
	SyBlobRelease(&sLongName);
	if( !bAny ){
		/* php's own sentence for a file its tar reader cannot even start on. */
		*pzErr = "truncated entry";
		return -1;
	}
	SyBlobAppend(&pPhar->sStub,"",0);
	pPhar->iFormat = PHAR_FORMAT_TAR;
	return 0;
}
/*
 * ZIP, read from its CENTRAL DIRECTORY -- the only part of the format that is
 * authoritative, which is why an appended or partly-rewritten zip still reads.
 * Entries are either stored or deflated, and the deflate is ext/zlib's.
 */
static int PharParseZip(phl_phar *pPhar,const unsigned char *zFile,sxu32 nFile,const char **pzErr)
{
	sxu32 nEocd,nEntries,nCdOff,i,q;
	int bFound = 0;
	if( nFile < 22 ){
		*pzErr = "internal corruption of zip (truncated)";
		return -1;
	}
	for( nEocd = nFile - 22 ; ; --nEocd ){
		if( zFile[nEocd] == 'P' && zFile[nEocd+1] == 'K'
		 && zFile[nEocd+2] == 5 && zFile[nEocd+3] == 6 ){
			bFound = 1;
			break;
		}
		if( nEocd == 0 || nFile - nEocd > 66000 ){
			break;
		}
	}
	if( !bFound ){
		*pzErr = "internal corruption of zip (end of central directory not found)";
		return -1;
	}
	nEntries = PharGet16(&zFile[nEocd+10]);
	nCdOff = PharGet32(&zFile[nEocd+16]);
	{
		sxu32 nCommentLen = PharGet16(&zFile[nEocd+20]);
		if( nCommentLen > 0 && nCommentLen <= nFile - nEocd - 22 ){
			/* php keeps a zip's comment as the archive's stub. */
			SyBlobAppend(&pPhar->sStub,&zFile[nEocd+22],nCommentLen);
		}
	}
	q = nCdOff;
	for( i = 0 ; i < nEntries ; ++i ){
		sxu32 nNameLen,nExtraLen,nCommentLen,nLocal,nMethod,nCsz,nUsz,nCrc,nAttr;
		phl_phar_ent *pEnt;
		/* Remainders again: nCdOff and every length below come out of the file. */
		if( q > nFile || nFile - q < 46 || SyMemcmp(&zFile[q],"PK\001\002",4) != 0 ){
			*pzErr = "internal corruption of zip (truncated central directory)";
			return -1;
		}
		nMethod = PharGet16(&zFile[q+10]);
		nCrc = PharGet32(&zFile[q+16]);
		nCsz = PharGet32(&zFile[q+20]);
		nUsz = PharGet32(&zFile[q+24]);
		nNameLen = PharGet16(&zFile[q+28]);
		nExtraLen = PharGet16(&zFile[q+30]);
		nCommentLen = PharGet16(&zFile[q+32]);
		nAttr = PharGet32(&zFile[q+38]);
		nLocal = PharGet32(&zFile[q+42]);
		if( nNameLen > nFile - q - 46 ){
			*pzErr = "internal corruption of zip (truncated central directory)";
			return -1;
		}
		pEnt = PharNewEnt(pPhar,(const char *)&zFile[q+46],(int)nNameLen);
		if( pEnt == 0 ){
			*pzErr = "internal corruption of zip (out of memory)";
			return -1;
		}
		pEnt->nSize = nUsz;
		pEnt->nCompSize = nCsz;
		pEnt->nCrc = nCrc;
		pEnt->nFlags = ((nAttr >> 16) & 0x1FF);
		if( pEnt->nFlags == 0 ){
			pEnt->nFlags = 0666;
		}
		if( nMethod == 8 ){
			pEnt->nFlags |= PHAR_C_GZ;
		}
		if( SyBlobLength(&pEnt->sName) > 0 ){
			char *zN = (char *)SyBlobData(&pEnt->sName);
			if( zN[SyBlobLength(&pEnt->sName)-1] == '/' ){
				pEnt->bDir = 1;
				PharChopSlash(&pEnt->sName);
			}
		}
		/* The local header repeats the name and carries its own extra field,
		 * so the payload's offset is only knowable from it. */
		if( nLocal > nFile || nFile - nLocal < 30
		 || SyMemcmp(&zFile[nLocal],"PK\003\004",4) != 0 ){
			*pzErr = "internal corruption of zip (bad local header)";
			return -1;
		}
		pEnt->iOffset = (sxi64)(nLocal + 30 + PharGet16(&zFile[nLocal+26])
			+ PharGet16(&zFile[nLocal+28]));
		q += 46 + nNameLen + nExtraLen + nCommentLen;
	}
	pPhar->iFormat = PHAR_FORMAT_ZIP;
	return 0;
}
/*
 * The bytes of one entry, decompressed if they are stored compressed. Kept on
 * the entry once read: an archive is normally read many times over.
 */
static int PharEntLoad(phl_phar *pPhar,phl_phar_ent *pEnt)
{
	const unsigned char *zFile;
	sxu32 nFile;
	if( pEnt->bLoaded || pEnt->bDir ){
		return 0;
	}
	zFile = (const unsigned char *)SyBlobData(&pPhar->sFile);
	nFile = SyBlobLength(&pPhar->sFile);
	if( pEnt->iOffset < 0 || (sxu32)pEnt->iOffset + pEnt->nCompSize > nFile ){
		return -1;
	}
	if( (pEnt->nFlags & PHAR_C_MASK) == 0 ){
		SyBlobAppend(&pEnt->sData,&zFile[pEnt->iOffset],pEnt->nCompSize);
		pEnt->bLoaded = 1;
		return 0;
	}
#ifdef PH7_ENABLE_ZLIB
	if( (pEnt->nFlags & PHAR_C_MASK) == PHAR_C_GZ ){
		z_stream z;
		unsigned char *zOut;
		int rc;
		if( pEnt->nSize == 0 ){
			pEnt->bLoaded = 1;
			return 0;
		}
		zOut = (unsigned char *)SyMemBackendAlloc(&pPhar->pVm->sAllocator,pEnt->nSize);
		if( zOut == 0 ){
			return -1;
		}
		SyZero(&z,sizeof(z));
		/* A phar's and a zip's compressed member is a RAW deflate stream. */
		if( inflateInit2(&z,-MAX_WBITS) != Z_OK ){
			SyMemBackendFree(&pPhar->pVm->sAllocator,zOut);
			return -1;
		}
		z.next_in = (Bytef *)&zFile[pEnt->iOffset];
		z.avail_in = (uInt)pEnt->nCompSize;
		z.next_out = (Bytef *)zOut;
		z.avail_out = (uInt)pEnt->nSize;
		rc = inflate(&z,Z_FINISH);
		inflateEnd(&z);
		if( rc != Z_STREAM_END ){
			SyMemBackendFree(&pPhar->pVm->sAllocator,zOut);
			return -1;
		}
		SyBlobAppend(&pEnt->sData,zOut,pEnt->nSize - z.avail_out);
		SyMemBackendFree(&pPhar->pVm->sAllocator,zOut);
		pEnt->bLoaded = 1;
		return 0;
	}
#endif
	/* BZ2, or a compression this build has no library for: php's own answer
	 * when its build lacks the extension. */
	return -1;
}
/*
 * Open an archive from a PATH, through the engine's own stream layer -- so an
 * archive inside another archive, or one over http://, reads like any other.
 * The whole file is taken at once; see the section comment.
 */
/*
 * Would php open this NAME for writing? Its write-mode url check wants a `.phar`
 * component in the path; a `.tar` or a `.zip` is a DATA archive, and the two
 * differ in what the doors answer -- `phar.readonly` does not gate a data
 * archive's entries, and its directory doors are refused outright. An archive
 * the WRAPPER opened has no object to have been told which it is, so the name
 * is what says so, exactly as it does for php.
 */
static int PharNameIsExecutable(const char *zPath,int nPath)
{
	int i;
	for( i = 0 ; i + (int)sizeof(".phar")-1 <= nPath ; ++i ){
		if( zPath[i] != '.' || SyMemcmp(&zPath[i],".phar",sizeof(".phar")-1) != 0 ){
			continue;
		}
		i += (int)sizeof(".phar")-1;
		if( i == nPath || zPath[i] == '/' || zPath[i] == '\\' ){
			return 1;
		}
		return 0;
	}
	return 0;
}
static phl_phar * PharOpenPath(ph7_vm *pVm,const char *zPath,int nPath,int bData,
	const char **pzErr)
{
	const ph7_io_stream *pStream;
	const char *zTail;
	phl_phar *pPhar;
	void *pHandle;
	SyBlob sPath;
	int rc;
	*pzErr = 0;
	pPhar = PharFindByPath(pVm,zPath,nPath);
	if( pPhar ){
		return pPhar;
	}
	SyBlobInit(&sPath,&pVm->sAllocator);
	SyBlobAppend(&sPath,zPath,(sxu32)nPath);
	SyBlobNullAppend(&sPath);
	zTail = (const char *)SyBlobData(&sPath);
	pStream = PH7_VmGetStreamDevice(pVm,&zTail,nPath);
	if( pStream == 0 ){
		SyBlobRelease(&sPath);
		*pzErr = "unable to open phar for reading";
		return 0;
	}
	pHandle = PH7_StreamOpenHandle(pVm,pStream,zTail,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,0);
	if( pHandle == 0 ){
		SyBlobRelease(&sPath);
		*pzErr = "unable to open phar for reading";
		return 0;
	}
	pPhar = (phl_phar *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar));
	if( pPhar == 0 ){
		PH7_StreamCloseHandle(pStream,pHandle);
		SyBlobRelease(&sPath);
		*pzErr = "unable to open phar for reading";
		return 0;
	}
	SyZero(pPhar,sizeof(*pPhar));
	pPhar->pVm = pVm;
	pPhar->bData = bData || !PharNameIsExecutable(zPath,nPath);
	SyBlobInit(&pPhar->sPath,&pVm->sAllocator);
	SyBlobInit(&pPhar->sAlias,&pVm->sAllocator);
	SyBlobInit(&pPhar->sStub,&pVm->sAllocator);
	SyBlobInit(&pPhar->sMeta,&pVm->sAllocator);
	SyBlobInit(&pPhar->sFile,&pVm->sAllocator);
	SyBlobInit(&pPhar->sSig,&pVm->sAllocator);
	SyBlobAppend(&pPhar->sPath,zPath,(sxu32)nPath);
	PH7_StreamReadWholeFile(pHandle,pStream,&pPhar->sFile);
	PH7_StreamCloseHandle(pStream,pHandle);
	SyBlobRelease(&sPath);
	{
		const unsigned char *zFile = (const unsigned char *)SyBlobData(&pPhar->sFile);
		sxu32 nFile = SyBlobLength(&pPhar->sFile);
		sxu32 nOff = 0;
		const char *zErr = "internal corruption of phar (truncated manifest header)";
		if( nFile < 4 ){
			rc = -1;
		}else if( PharFindManifest(zFile,nFile,&nOff) ){
			rc = PharParsePhar(pPhar,zFile,nFile,nOff,&zErr);
		}else if( nFile > 4 && zFile[0] == 'P' && zFile[1] == 'K' ){
			rc = PharParseZip(pPhar,zFile,nFile,&zErr);
		}else{
			/* Anything else is offered to the TAR reader, which is php's last
			 * resort too -- and its refusal is what php reports for a file that
			 * is no archive at all. */
			rc = PharParseTar(pPhar,zFile,nFile,&zErr);
		}
		if( rc != 0 ){
			*pzErr = zErr;
			PharFree(pPhar);
			return 0;
		}
	}
	if( pPhar->iFormat == PHAR_FORMAT_PHAR && pPhar->iSigType != 0 ){
		if( PharVerifySignature(pPhar,0) != 0 ){
			*pzErr = "broken signature";
			PharFree(pPhar);
			return 0;
		}
	}
	pPhar->pNext = (phl_phar *)pVm->pPhars;
	pVm->pPhars = pPhar;
	return pPhar;
}
/* ------------------------------------------------------------------ */
/* Writing an archive back out                                         */
/* ------------------------------------------------------------------ */
static void PharPut32(SyBlob *pOut,sxu32 v)
{
	unsigned char z[4];
	z[0] = (unsigned char)(v & 0xFF);
	z[1] = (unsigned char)((v >> 8) & 0xFF);
	z[2] = (unsigned char)((v >> 16) & 0xFF);
	z[3] = (unsigned char)((v >> 24) & 0xFF);
	SyBlobAppend(pOut,z,4);
}
static void PharPut16(SyBlob *pOut,sxu32 v)
{
	unsigned char z[2];
	z[0] = (unsigned char)(v & 0xFF);
	z[1] = (unsigned char)((v >> 8) & 0xFF);
	SyBlobAppend(pOut,z,2);
}
static void PharPut16BE(SyBlob *pOut,sxu32 v)
{
	unsigned char z[2];
	z[0] = (unsigned char)((v >> 8) & 0xFF);
	z[1] = (unsigned char)(v & 0xFF);
	SyBlobAppend(pOut,z,2);
}
static sxu32 PharCrc32(const unsigned char *z,sxu32 n)
{
	SumContext sCtx;
	unsigned char zOut[4];
	SumInit(&sCtx,SUM_CRC32B);
	SumUpdate(&sCtx,z,n);
	SumFinal(&sCtx,zOut);
	/* SumFinal writes the digest MSB first; the wire wants the NUMBER. */
	return ((sxu32)zOut[0] << 24) | ((sxu32)zOut[1] << 16)
	     | ((sxu32)zOut[2] << 8) | (sxu32)zOut[3];
}
/* The bytes an entry is STORED as, compressing them when its flags say so. */
static int PharEntStored(phl_phar *pPhar,phl_phar_ent *pEnt,SyBlob *pOut)
{
	if( PharEntLoad(pPhar,pEnt) != 0 ){
		return -1;
	}
	if( (pEnt->nFlags & PHAR_C_MASK) == 0 || pEnt->bDir ){
		SyBlobAppend(pOut,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));
		return 0;
	}
#ifdef PH7_ENABLE_ZLIB
	if( (pEnt->nFlags & PHAR_C_MASK) == PHAR_C_GZ ){
		z_stream z;
		unsigned char *zBuf;
		uLong nBound;
		int rc;
		SyZero(&z,sizeof(z));
		if( deflateInit2(&z,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,8,
				Z_DEFAULT_STRATEGY) != Z_OK ){
			return -1;
		}
		nBound = deflateBound(&z,(uLong)SyBlobLength(&pEnt->sData)) + 32;
		zBuf = (unsigned char *)SyMemBackendAlloc(&pPhar->pVm->sAllocator,(sxu32)nBound);
		if( zBuf == 0 ){
			deflateEnd(&z);
			return -1;
		}
		z.next_in = (Bytef *)SyBlobData(&pEnt->sData);
		z.avail_in = (uInt)SyBlobLength(&pEnt->sData);
		z.next_out = (Bytef *)zBuf;
		z.avail_out = (uInt)nBound;
		rc = deflate(&z,Z_FINISH);
		deflateEnd(&z);
		if( rc != Z_STREAM_END ){
			SyMemBackendFree(&pPhar->pVm->sAllocator,zBuf);
			return -1;
		}
		SyBlobAppend(pOut,zBuf,(sxu32)(nBound - z.avail_out));
		SyMemBackendFree(&pPhar->pVm->sAllocator,zBuf);
		return 0;
	}
#endif
	return -1;
}
/* php's default stub, which is what an archive gets when nobody set one. */
static void PharDefaultStub(SyBlob *pOut)
{
	static const char zStub[] =
		"<?php __HALT_COMPILER(); ?>\r\n";
	SyBlobAppend(pOut,zStub,(sxu32)sizeof(zStub)-1);
}
/*
 * php's own format, written the way php writes it: the stub, then the manifest
 * (whose first word is the length of everything after it), then every entry's
 * stored bytes in manifest order, then the signature block.
 */
static int PharBuildPhar(phl_phar *pPhar,SyBlob *pOut)
{
	SyBlob sManifest,sData;
	phl_phar_ent *pEnt;
	ph7_vm *pVm = pPhar->pVm;
	sxu32 nFlags = PHAR_GF_SIGNED;
	int rc = 0;
	SyBlobInit(&sManifest,&pVm->sAllocator);
	SyBlobInit(&sData,&pVm->sAllocator);
	if( SyBlobLength(&pPhar->sStub) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pPhar->sStub),SyBlobLength(&pPhar->sStub));
	}else{
		PharDefaultStub(pOut);
	}
	PharPut32(&sManifest,pPhar->nEnt);
	PharPut16BE(&sManifest,PHAR_API_WORD);
	PharPut32(&sManifest,nFlags);
	PharPut32(&sManifest,SyBlobLength(&pPhar->sAlias));
	if( SyBlobLength(&pPhar->sAlias) > 0 ){
		SyBlobAppend(&sManifest,SyBlobData(&pPhar->sAlias),SyBlobLength(&pPhar->sAlias));
	}
	PharPut32(&sManifest,SyBlobLength(&pPhar->sMeta));
	if( SyBlobLength(&pPhar->sMeta) > 0 ){
		SyBlobAppend(&sManifest,SyBlobData(&pPhar->sMeta),SyBlobLength(&pPhar->sMeta));
	}
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		SyBlob sStored;
		sxu32 nStored;
		SyBlobInit(&sStored,&pVm->sAllocator);
		if( PharEntStored(pPhar,pEnt,&sStored) != 0 ){
			SyBlobRelease(&sStored);
			rc = -1;
			break;
		}
		nStored = SyBlobLength(&sStored);
		pEnt->nSize = SyBlobLength(&pEnt->sData);
		pEnt->nCompSize = pEnt->bDir ? 0 : nStored;
		pEnt->nCrc = PharCrc32((const unsigned char *)SyBlobData(&pEnt->sData),
			SyBlobLength(&pEnt->sData));
		PharPut32(&sManifest,SyBlobLength(&pEnt->sName) + (pEnt->bDir ? 1 : 0));
		SyBlobAppend(&sManifest,SyBlobData(&pEnt->sName),SyBlobLength(&pEnt->sName));
		if( pEnt->bDir ){
			SyBlobAppend(&sManifest,"/",1);
		}
		PharPut32(&sManifest,pEnt->nSize);
		PharPut32(&sManifest,(sxu32)pEnt->iTime);
		PharPut32(&sManifest,pEnt->nCompSize);
		PharPut32(&sManifest,pEnt->nCrc);
		PharPut32(&sManifest,pEnt->nFlags);
		PharPut32(&sManifest,SyBlobLength(&pEnt->sMeta));
		if( SyBlobLength(&pEnt->sMeta) > 0 ){
			SyBlobAppend(&sManifest,SyBlobData(&pEnt->sMeta),SyBlobLength(&pEnt->sMeta));
		}
		if( !pEnt->bDir ){
			SyBlobAppend(&sData,SyBlobData(&sStored),nStored);
		}
		SyBlobRelease(&sStored);
	}
	if( rc == 0 ){
		PharPut32(pOut,SyBlobLength(&sManifest));
		SyBlobAppend(pOut,SyBlobData(&sManifest),SyBlobLength(&sManifest));
		SyBlobAppend(pOut,SyBlobData(&sData),SyBlobLength(&sData));
		/* The signature covers everything written so far. */
		{
			SHA1Context sSha;
			unsigned char zDigest[20];
			sxu32 nSigLen;
			/* php 8 signs a new archive with SHA-256 unless it was told
			 * otherwise. */
			int iSig = pPhar->iSigType ? pPhar->iSigType : PHAR_SIG_SHA256;
			nSigLen = 20;
			if( iSig == PHAR_SIG_SHA512 ){
				SHA512Context sS;
				unsigned char zD[64];
				SHA512Init(&sS);
				SHA512Update(&sS,(const unsigned char *)SyBlobData(pOut),SyBlobLength(pOut));
				SHA512Final(&sS,zD);
				SyBlobAppend(pOut,zD,64);
				nSigLen = 64;
			}else if( iSig == PHAR_SIG_SHA256 ){
				SHA256Context sS;
				unsigned char zD[32];
				SHA256Init(&sS);
				SHA256Update(&sS,(const unsigned char *)SyBlobData(pOut),SyBlobLength(pOut));
				SHA256Final(&sS,zD);
				SyBlobAppend(pOut,zD,32);
				nSigLen = 32;
			}else if( iSig == PHAR_SIG_MD5 ){
				MD5Context sM;
				unsigned char zD[16];
				MD5Init(&sM);
				MD5Update(&sM,(const unsigned char *)SyBlobData(pOut),SyBlobLength(pOut));
				MD5Final(zD,&sM);
				SyBlobAppend(pOut,zD,16);
				nSigLen = 16;
			}else{
				iSig = PHAR_SIG_SHA1;
				SHA1Init(&sSha);
				SHA1Update(&sSha,(const unsigned char *)SyBlobData(pOut),SyBlobLength(pOut));
				SHA1Final(&sSha,zDigest);
				SyBlobAppend(pOut,zDigest,20);
			}
			PharPut32(pOut,(sxu32)iSig);
			SyBlobAppend(pOut,"GBMB",4);
			pPhar->iSigType = iSig;
			/* Keep the digest: `getSignature()` answers it without re-reading
			 * the file. */
			SyBlobReset(&pPhar->sSig);
			SyBlobAppend(&pPhar->sSig,
				(const char *)SyBlobData(pOut) + SyBlobLength(pOut) - 8 - nSigLen,nSigLen);
		}
	}
	SyBlobRelease(&sManifest);
	SyBlobRelease(&sData);
	return rc;
}
/* ustar, with a GNU long-name record for anything past 100 bytes. */
static void PharTarField(SyBlob *pOut,const char *zVal,int nVal,int nWidth)
{
	static const char zZero[512] = { 0 };
	if( nVal > nWidth ){
		nVal = nWidth;
	}
	SyBlobAppend(pOut,zVal,(sxu32)nVal);
	SyBlobAppend(pOut,zZero,(sxu32)(nWidth - nVal));
}
static void PharTarOct(SyBlob *pOut,sxi64 iVal,int nWidth)
{
	char zBuf[24];
	int i;
	for( i = nWidth - 2 ; i >= 0 ; --i ){
		zBuf[i] = (char)('0' + (int)(iVal & 7));
		iVal >>= 3;
	}
	zBuf[nWidth-1] = 0;
	SyBlobAppend(pOut,zBuf,(sxu32)nWidth);
}
static void PharTarHeader(SyBlob *pOut,const char *zName,int nName,sxi64 nSize,
	sxi64 iTime,sxu32 nMode,char cType)
{
	static const char zZero[512] = { 0 };
	sxu32 nStart = SyBlobLength(pOut);
	unsigned char *z;
	sxu32 nSum = 0,i;
	PharTarField(pOut,zName,nName,100);
	PharTarOct(pOut,(sxi64)(nMode & 0777),8);
	PharTarOct(pOut,0,8);
	PharTarOct(pOut,0,8);
	PharTarOct(pOut,nSize,12);
	PharTarOct(pOut,iTime,12);
	SyBlobAppend(pOut,"        ",8);   /* checksum field, blank while computed */
	SyBlobAppend(pOut,&cType,1);
	SyBlobAppend(pOut,zZero,100);      /* link name */
	SyBlobAppend(pOut,"ustar",5);
	SyBlobAppend(pOut,zZero,1);
	SyBlobAppend(pOut,"00",2);
	SyBlobAppend(pOut,zZero,32+32+8+8+155+12);
	z = (unsigned char *)SyBlobData(pOut) + nStart;
	for( i = 0 ; i < 512 ; ++i ){
		nSum += z[i];
	}
	{
		char zSum[8];
		sxi64 v = (sxi64)nSum;
		int k;
		for( k = 5 ; k >= 0 ; --k ){
			zSum[k] = (char)('0' + (int)(v & 7));
			v >>= 3;
		}
		zSum[6] = 0;
		zSum[7] = ' ';
		SyMemcpy(zSum,&z[148],8);
	}
}
static void PharTarPad(SyBlob *pOut)
{
	static const char zZero[512] = { 0 };
	sxu32 n = SyBlobLength(pOut) % 512;
	if( n ){
		SyBlobAppend(pOut,zZero,512 - n);
	}
}
static int PharBuildTar(phl_phar *pPhar,SyBlob *pOut)
{
	static const char zZero[512] = { 0 };
	phl_phar_ent *pEnt;
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		const char *zName = (const char *)SyBlobData(&pEnt->sName);
		int nName = (int)SyBlobLength(&pEnt->sName);
		if( PharEntLoad(pPhar,pEnt) != 0 ){
			return -1;
		}
		if( nName > 100 ){
			/* GNU's long-name record: the name travels as a payload of its own. */
			PharTarHeader(pOut,"././@LongLink",13,(sxi64)nName + 1,0,0644,'L');
			SyBlobAppend(pOut,zName,(sxu32)nName);
			SyBlobAppend(pOut,zZero,1);
			PharTarPad(pOut);
		}
		if( pEnt->bDir ){
			PharTarHeader(pOut,zName,nName,0,pEnt->iTime,pEnt->nFlags & 0777,'5');
			continue;
		}
		PharTarHeader(pOut,zName,nName,(sxi64)SyBlobLength(&pEnt->sData),pEnt->iTime,
			pEnt->nFlags & 0777,'0');
		SyBlobAppend(pOut,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));
		PharTarPad(pOut);
	}
	SyBlobAppend(pOut,zZero,512);
	SyBlobAppend(pOut,zZero,512);
	return 0;
}
/* ZIP: a local header and payload per entry, then the central directory. */
static int PharBuildZip(phl_phar *pPhar,SyBlob *pOut)
{
	phl_phar_ent *pEnt;
	SyBlob sCd;
	ph7_vm *pVm = pPhar->pVm;
	sxu32 nCdOff,nEntries = 0;
	int rc = 0;
	SyBlobInit(&sCd,&pVm->sAllocator);
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		SyBlob sStored;
		sxu32 nLocal = SyBlobLength(pOut);
		sxu32 nName = SyBlobLength(&pEnt->sName);
		sxu32 nCrc;
		int bDeflate = (pEnt->nFlags & PHAR_C_MASK) == PHAR_C_GZ;
		SyBlobInit(&sStored,&pVm->sAllocator);
		if( PharEntStored(pPhar,pEnt,&sStored) != 0 ){
			SyBlobRelease(&sStored);
			rc = -1;
			break;
		}
		nCrc = PharCrc32((const unsigned char *)SyBlobData(&pEnt->sData),
			SyBlobLength(&pEnt->sData));
		SyBlobAppend(pOut,"PK\003\004",4);
		PharPut16(pOut,20);
		PharPut16(pOut,0);
		PharPut16(pOut,bDeflate ? 8 : 0);
		PharPut16(pOut,0);
		PharPut16(pOut,0);
		PharPut32(pOut,nCrc);
		PharPut32(pOut,SyBlobLength(&sStored));
		PharPut32(pOut,SyBlobLength(&pEnt->sData));
		PharPut16(pOut,nName + (pEnt->bDir ? 1 : 0));
		PharPut16(pOut,0);
		SyBlobAppend(pOut,SyBlobData(&pEnt->sName),nName);
		if( pEnt->bDir ){
			SyBlobAppend(pOut,"/",1);
		}
		SyBlobAppend(pOut,SyBlobData(&sStored),SyBlobLength(&sStored));
		SyBlobAppend(&sCd,"PK\001\002",4);
		PharPut16(&sCd,20);
		PharPut16(&sCd,20);
		PharPut16(&sCd,0);
		PharPut16(&sCd,bDeflate ? 8 : 0);
		PharPut16(&sCd,0);
		PharPut16(&sCd,0);
		PharPut32(&sCd,nCrc);
		PharPut32(&sCd,SyBlobLength(&sStored));
		PharPut32(&sCd,SyBlobLength(&pEnt->sData));
		PharPut16(&sCd,nName + (pEnt->bDir ? 1 : 0));
		PharPut16(&sCd,0);
		PharPut16(&sCd,0);
		PharPut16(&sCd,0);
		PharPut16(&sCd,0);
		PharPut32(&sCd,((pEnt->nFlags & 0777) | (pEnt->bDir ? 0040000 : 0100000)) << 16);
		PharPut32(&sCd,nLocal);
		SyBlobAppend(&sCd,SyBlobData(&pEnt->sName),nName);
		if( pEnt->bDir ){
			SyBlobAppend(&sCd,"/",1);
		}
		SyBlobRelease(&sStored);
		nEntries++;
	}
	if( rc == 0 ){
		nCdOff = SyBlobLength(pOut);
		SyBlobAppend(pOut,SyBlobData(&sCd),SyBlobLength(&sCd));
		SyBlobAppend(pOut,"PK\005\006",4);
		PharPut16(pOut,0);
		PharPut16(pOut,0);
		PharPut16(pOut,nEntries);
		PharPut16(pOut,nEntries);
		PharPut32(pOut,SyBlobLength(&sCd));
		PharPut32(pOut,nCdOff);
		PharPut16(pOut,SyBlobLength(&pPhar->sStub));
		if( SyBlobLength(&pPhar->sStub) > 0 ){
			SyBlobAppend(pOut,SyBlobData(&pPhar->sStub),SyBlobLength(&pPhar->sStub));
		}
	}
	SyBlobRelease(&sCd);
	return rc;
}
/*
 * Write the archive back to the file it was opened from. Every write door ends
 * here -- php's `stopBuffering` does the same thing, and its buffering pair is
 * only a way to do it ONCE for a run of changes.
 */
static int PharCommit(phl_phar *pPhar)
{
	ph7_vm *pVm = pPhar->pVm;
	const ph7_io_stream *pStream;
	SyBlob sOut;
	const char *zPath;
	void *pHandle;
	int rc;
	SyBlobInit(&sOut,&pVm->sAllocator);
	rc = pPhar->iFormat == PHAR_FORMAT_TAR ? PharBuildTar(pPhar,&sOut)
	   : (pPhar->iFormat == PHAR_FORMAT_ZIP ? PharBuildZip(pPhar,&sOut)
	                                        : PharBuildPhar(pPhar,&sOut));
	if( rc != 0 ){
		SyBlobRelease(&sOut);
		return -1;
	}
	/* NUL-terminate for the device lookup WITHOUT counting the byte:
	 * SyBlobNullAppend already keeps nByte where it was. */
	SyBlobNullAppend(&pPhar->sPath);
	zPath = (const char *)SyBlobData(&pPhar->sPath);
	pStream = PH7_VmGetStreamDevice(pVm,&zPath,(int)SyBlobLength(&pPhar->sPath));
	if( pStream == 0 ){
		SyBlobRelease(&sOut);
		return -1;
	}
	pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,
		PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,0);
	if( pHandle == 0 ){
		SyBlobRelease(&sOut);
		return -1;
	}
	if( pStream->xWrite ){
		pStream->xWrite(pHandle,SyBlobData(&sOut),(ph7_int64)SyBlobLength(&sOut));
	}
	PH7_StreamCloseHandle(pStream,pHandle);
	/* The in-memory copy is the file again. */
	SyBlobReset(&pPhar->sFile);
	SyBlobAppend(&pPhar->sFile,SyBlobData(&sOut),SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return 0;
}
/* ------------------------------------------------------------------ */
/* Splitting a phar:// url                                             */
/* ------------------------------------------------------------------ */
/* Does this name a regular FILE on the platform's own filesystem? The url
 * splitter asks it of every prefix, and only the OS vfs can answer. */
static int PharPathIsFile(ph7_vm *pVm,const char *zPath)
{
	const ph7_vfs *pVfs = pVm->pEngine ? pVm->pEngine->pVfs : 0;
	if( pVfs == 0 || pVfs->xIsfile == 0 ){
		return 0;
	}
	return pVfs->xIsfile(zPath) == PH7_OK;
}
/*
 * `phar://<archive>/<entry>`. Which part is which is not spelled in the url, so
 * php works it out: the first component may be an ALIAS a running archive
 * registered (`phar://composer.phar/bin/composer` inside composer.phar itself),
 * and otherwise the archive is the longest leading run of components that names
 * a FILE. Walking left to right and stopping at the first file is the same
 * answer for every well-formed url and needs no table of extensions.
 */
static phl_phar * PharResolveUrl(ph7_vm *pVm,const char *zUrl,int nUrl,
	const char **pzEnt,int *pnEnt,const char **pzErr)
{
	phl_phar *pPhar;
	int i,nFirst;
	*pzErr = 0;
	*pzEnt = "";
	*pnEnt = 0;
	while( nUrl > 0 && zUrl[0] == '/' && nUrl > 1 && zUrl[1] == '/' ){
		zUrl++; nUrl--;
	}
	/* The alias form: one component, then `/`. */
	for( nFirst = 0 ; nFirst < nUrl && zUrl[nFirst] != '/' ; ++nFirst ){}
	pPhar = PharFindByAlias(pVm,zUrl,nFirst);
	if( pPhar ){
		*pzEnt = &zUrl[nFirst];
		*pnEnt = nUrl - nFirst;
		return pPhar;
	}
	/* ...otherwise a path. Try every prefix, shortest first. */
	for( i = 1 ; i <= nUrl ; ++i ){
		if( i < nUrl && zUrl[i] != '/' ){
			continue;
		}
		pPhar = PharFindByPath(pVm,zUrl,i);
		if( pPhar == 0 ){
			SyBlob sTry;
			int rc;
			SyBlobInit(&sTry,&pVm->sAllocator);
			SyBlobAppend(&sTry,zUrl,(sxu32)i);
			SyBlobNullAppend(&sTry);
			rc = PharPathIsFile(pVm,(const char *)SyBlobData(&sTry));
			if( rc ){
				const char *zErr = 0;
				pPhar = PharOpenPath(pVm,(const char *)SyBlobData(&sTry),i,0,&zErr);
				if( pPhar == 0 ){
					*pzErr = zErr;
					SyBlobRelease(&sTry);
					return 0;
				}
			}
			SyBlobRelease(&sTry);
		}
		if( pPhar ){
			*pzEnt = &zUrl[i];
			*pnEnt = nUrl - i;
			return pPhar;
		}
	}
	*pzErr = "unable to open phar for reading";
	return 0;
}
/* ------------------------------------------------------------------ */
/* The phar:// device                                                  */
/* ------------------------------------------------------------------ */
typedef struct phl_phar_io phl_phar_io;
struct phl_phar_io {
	phl_phar *pPhar;
	phl_phar_ent *pEnt;
	sxu32 nPos;
	int bWrite;
	SyBlob sWrite;
};
typedef struct phl_phar_dir phl_phar_dir;
struct phl_phar_dir {
	ph7_vm *pVm;
	SySet aName;      /* SyBlob per name, in manifest order */
	sxu32 nCur;
};
/* Arm an open failure whose text is BUILT rather than a literal: the engine
 * keeps the pointer and the caller prints it once the open has returned, so the
 * bytes live on the VM. */
static void PharSetOpenError(ph7_vm *pVm,const char *zFmt,...)
{
	va_list ap;
	SyBlobReset(&pVm->sPharErr);
	va_start(ap,zFmt);
	SyBlobFormatAp(&pVm->sPharErr,zFmt,ap);
	va_end(ap);
	SyBlobNullAppend(&pVm->sPharErr);
	PH7_StreamSetOpenError(pVm,(const char *)SyBlobData(&pVm->sPharErr));
}
static int PharReadonly(ph7_vm *pVm)
{
	return PH7_VmIniGetBool(pVm,"phar.readonly",1);
}
/* php's refusal for every write door the ini closes. */
static const char * const zPharReadonlyStream =
	"phar error: write operations disabled by the php.ini setting phar.readonly";

static int PharStreamOpen(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)
{
	ph7_vm *pVm = pResource ? pResource->pVm : 0;
	phl_phar *pPhar;
	phl_phar_ent *pEnt;
	phl_phar_io *pIo;
	const char *zEnt,*zErr;
	int nEnt = 0,bWrite;
	if( pVm == 0 ){
		return -1;
	}
	bWrite = (iMode & (PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_RDWR|PH7_IO_OPEN_APPEND)) != 0;
	pPhar = PharResolveUrl(pVm,zName,(int)SyStrlen(zName),&zEnt,&nEnt,&zErr);
	if( pPhar == 0 ){
		if( bWrite && PharReadonly(pVm) ){
			/* php asks the DIRECTIVE before it asks whether there is an archive:
			 * a write into one that does not exist yet is refused for the same
			 * reason creating it would be. */
			PH7_StreamSetOpenError(pVm,zPharReadonlyStream);
			return -1;
		}
		PharSetOpenError(pVm,"phar error: %s",zErr ? zErr : "unable to open phar");
		return -1;
	}
	if( bWrite && PharReadonly(pVm) && !pPhar->bData ){
		PH7_StreamSetOpenError(pVm,zPharReadonlyStream);
		return -1;
	}
	pEnt = PharFindEnt(pPhar,zEnt,nEnt);
	if( pEnt == 0 || pEnt->bDir ){
		if( !bWrite ){
			/* php's own sentence, which names the entry and the archive. */
			const char *zE = zEnt;
			int nE = nEnt;
			while( nE > 0 && zE[0] == '/' ){ zE++; nE--; }
			PharSetOpenError(pVm,"phar error: \"%.*s\" is not a file in phar \"%.*s\"",
				nE,zE,(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));
			return -1;
		}
		pEnt = 0;
	}
	if( pEnt && PharEntLoad(pPhar,pEnt) != 0 ){
		PH7_StreamSetOpenError(pVm,"phar error: internal corruption of phar (unable to read entry)");
		return -1;
	}
	pIo = (phl_phar_io *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar_io));
	if( pIo == 0 ){
		return -1;
	}
	SyZero(pIo,sizeof(*pIo));
	pIo->pPhar = pPhar;
	pIo->pEnt = pEnt;
	pIo->bWrite = bWrite;
	SyBlobInit(&pIo->sWrite,&pVm->sAllocator);
	if( bWrite ){
		/* A write door open on a name: the bytes land on the entry at close,
		 * which is where an archive is rewritten. */
		SyBlobAppend(&pIo->sWrite,"",0);
		if( pEnt && (iMode & PH7_IO_OPEN_APPEND) ){
			SyBlobAppend(&pIo->sWrite,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));
		}
		if( pEnt == 0 ){
			SyBlob sName;
			SyBlobInit(&sName,&pVm->sAllocator);
			SyBlobAppend(&sName,zEnt,(sxu32)nEnt);
			pIo->pEnt = PharNewEnt(pPhar,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName));
			SyBlobRelease(&sName);
			if( pIo->pEnt == 0 ){
				SyBlobRelease(&pIo->sWrite);
				SyMemBackendFree(&pVm->sAllocator,pIo);
				return -1;
			}
			pIo->pEnt->nFlags = 0666;
			pIo->pEnt->bLoaded = 1;
		}
	}
	*ppHandle = (void *)pIo;
	return PH7_OK;
}
static ph7_int64 PharStreamRead(void *pHandle,void *pBuffer,ph7_int64 nWant)
{
	phl_phar_io *pIo = (phl_phar_io *)pHandle;
	sxu32 nHave;
	if( pIo == 0 || pIo->pEnt == 0 || pIo->bWrite ){
		return -1;
	}
	nHave = SyBlobLength(&pIo->pEnt->sData);
	if( pIo->nPos >= nHave ){
		return 0;
	}
	if( (ph7_int64)(nHave - pIo->nPos) < nWant ){
		nWant = (ph7_int64)(nHave - pIo->nPos);
	}
	SyMemcpy((const char *)SyBlobData(&pIo->pEnt->sData) + pIo->nPos,pBuffer,(sxu32)nWant);
	pIo->nPos += (sxu32)nWant;
	return nWant;
}
static ph7_int64 PharStreamWrite(void *pHandle,const void *pData,ph7_int64 nLen)
{
	phl_phar_io *pIo = (phl_phar_io *)pHandle;
	if( pIo == 0 || !pIo->bWrite ){
		return -1;
	}
	SyBlobAppend(&pIo->sWrite,pData,(sxu32)nLen);
	return nLen;
}
static int PharStreamSeek(void *pHandle,ph7_int64 iOfft,int whence)
{
	phl_phar_io *pIo = (phl_phar_io *)pHandle;
	ph7_int64 iTarget;
	sxu32 nLen;
	if( pIo == 0 || pIo->pEnt == 0 ){
		return -1;
	}
	nLen = pIo->bWrite ? SyBlobLength(&pIo->sWrite) : SyBlobLength(&pIo->pEnt->sData);
	iTarget = whence == 1 ? (ph7_int64)pIo->nPos + iOfft
	       : (whence == 2 ? (ph7_int64)nLen + iOfft : iOfft);
	if( iTarget < 0 ){
		return -1;
	}
	pIo->nPos = (sxu32)iTarget;
	return PH7_OK;
}
static ph7_int64 PharStreamTell(void *pHandle)
{
	phl_phar_io *pIo = (phl_phar_io *)pHandle;
	return pIo ? (ph7_int64)pIo->nPos : -1;
}
/* Fill php's thirteen stat fields for one entry (or for a directory). */
static void PharFillStat(phl_phar *pPhar,phl_phar_ent *pEnt,int bDir,ph7_int64 *aVal)
{
	int i;
	for( i = 0 ; i < 13 ; ++i ){
		aVal[i] = 0;
	}
	aVal[2] = bDir ? (ph7_int64)(PH7_S_IFDIR | 0777) : (ph7_int64)(PH7_S_IFREG | 0666);
	aVal[3] = 1;
	aVal[7] = (pEnt && !bDir) ? (ph7_int64)pEnt->nSize : 0;
	aVal[8] = aVal[9] = aVal[10] = pEnt ? pEnt->iTime : 0;
	aVal[11] = 512;
	aVal[12] = (aVal[7] + 511) / 512;
	SXUNUSED(pPhar);
}
static int PharStreamStat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)
{
	phl_phar_io *pIo = (phl_phar_io *)pHandle;
	ph7_int64 aVal[13];
	if( pIo == 0 || pIo->pEnt == 0 ){
		return -1;
	}
	PharFillStat(pIo->pPhar,pIo->pEnt,0,aVal);
	return PH7_VfsStatFill(pArray,pWorker,aVal);
}
/*
 * A DIRECTORY inside an archive: php lists the names that sit directly under it,
 * with no `.` or `..` -- an archive has no such entries to report.
 */
static int PharDirOpen(const char *zName,ph7_value *pResource,void **ppHandle)
{
	ph7_vm *pVm = pResource ? pResource->pVm : 0;
	phl_phar *pPhar;
	phl_phar_dir *pDir;
	phl_phar_ent *pEnt;
	const char *zEnt,*zErr;
	int nEnt = 0;
	if( pVm == 0 ){
		return -1;
	}
	pPhar = PharResolveUrl(pVm,zName,(int)SyStrlen(zName),&zEnt,&nEnt,&zErr);
	if( pPhar == 0 ){
		return -1;
	}
	while( nEnt > 0 && zEnt[0] == '/' ){ zEnt++; nEnt--; }
	while( nEnt > 0 && zEnt[nEnt-1] == '/' ){ nEnt--; }
	if( nEnt > 0 && !PharIsDir(pPhar,zEnt,nEnt) ){
		return -1;
	}
	pDir = (phl_phar_dir *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar_dir));
	if( pDir == 0 ){
		return -1;
	}
	SyZero(pDir,sizeof(*pDir));
	pDir->pVm = pVm;
	SySetInit(&pDir->aName,&pVm->sAllocator,sizeof(SyBlob));
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		const char *z = (const char *)SyBlobData(&pEnt->sName);
		int n = (int)SyBlobLength(&pEnt->sName);
		const char *zRest;
		int nRest,i;
		if( nEnt > 0 ){
			if( n <= nEnt || z[nEnt] != '/' || SyMemcmp(z,zEnt,(sxu32)nEnt) != 0 ){
				continue;
			}
			zRest = &z[nEnt+1];
			nRest = n - nEnt - 1;
		}else{
			zRest = z;
			nRest = n;
		}
		for( i = 0 ; i < nRest && zRest[i] != '/' ; ++i ){}
		{
			/* One name per child, however many entries sit under it. */
			SyBlob *aSeen = (SyBlob *)SySetBasePtr(&pDir->aName);
			sxu32 k,nSeen = SySetUsed(&pDir->aName);
			int bDup = 0;
			for( k = 0 ; k < nSeen ; ++k ){
				if( (int)SyBlobLength(&aSeen[k]) == i
				 && SyMemcmp(SyBlobData(&aSeen[k]),zRest,(sxu32)i) == 0 ){
					bDup = 1;
					break;
				}
			}
			if( !bDup && i > 0 ){
				SyBlob sName;
				SyBlobInit(&sName,&pVm->sAllocator);
				SyBlobAppend(&sName,zRest,(sxu32)i);
				SySetPut(&pDir->aName,(const void *)&sName);
			}
		}
	}
	*ppHandle = (void *)pDir;
	return PH7_OK;
}
static int PharDirRead(void *pHandle,ph7_context *pCtx)
{
	phl_phar_dir *pDir = (phl_phar_dir *)pHandle;
	SyBlob *aName;
	if( pDir == 0 || pDir->nCur >= SySetUsed(&pDir->aName) ){
		return -1;
	}
	aName = (SyBlob *)SySetBasePtr(&pDir->aName);
	ph7_result_string(pCtx,(const char *)SyBlobData(&aName[pDir->nCur]),
		(int)SyBlobLength(&aName[pDir->nCur]));
	pDir->nCur++;
	return PH7_OK;
}
static void PharDirRewind(void *pHandle)
{
	phl_phar_dir *pDir = (phl_phar_dir *)pHandle;
	if( pDir ){
		pDir->nCur = 0;
	}
}
static void PharDirClose(void *pHandle)
{
	phl_phar_dir *pDir = (phl_phar_dir *)pHandle;
	SyBlob *aName;
	sxu32 n,i;
	if( pDir == 0 ){
		return;
	}
	aName = (SyBlob *)SySetBasePtr(&pDir->aName);
	n = SySetUsed(&pDir->aName);
	for( i = 0 ; i < n ; ++i ){
		SyBlobRelease(&aName[i]);
	}
	SySetRelease(&pDir->aName);
	SyMemBackendFree(&pDir->pVm->sAllocator,pDir);
}
/*
 * Closing a handle opened for WRITING is what commits the bytes to the entry --
 * and, for a phar-format archive, rewrites the file. php buffers the same way
 * (its `startBuffering`/`stopBuffering` pair is the explicit form of it).
 */
static void PharStreamClose(void *pHandle)
{
	phl_phar_io *pIo = (phl_phar_io *)pHandle;
	ph7_vm *pVm;
	if( pIo == 0 ){
		return;
	}
	pVm = pIo->pPhar->pVm;
	if( pIo->bWrite && pIo->pEnt ){
		SyBlobReset(&pIo->pEnt->sData);
		SyBlobAppend(&pIo->pEnt->sData,SyBlobData(&pIo->sWrite),SyBlobLength(&pIo->sWrite));
		pIo->pEnt->nSize = SyBlobLength(&pIo->pEnt->sData);
		pIo->pEnt->nCompSize = pIo->pEnt->nSize;
		pIo->pEnt->nFlags &= ~(sxu32)PHAR_C_MASK;
		pIo->pEnt->bLoaded = 1;
		pIo->pEnt->iTime = (sxi64)time(0);
		PharCommit(pIo->pPhar);
	}
	SyBlobRelease(&pIo->sWrite);
	SyMemBackendFree(&pVm->sAllocator,pIo);
}
PH7_PRIVATE const ph7_io_stream sPHAR_Stream = {
	"phar",
	PH7_IO_STREAM_VERSION,
	PharStreamOpen,   /* xOpen */
	PharDirOpen,      /* xOpenDir */
	PharStreamClose,  /* xClose */
	PharDirClose,     /* xCloseDir */
	PharStreamRead,   /* xRead */
	PharDirRead,      /* xReadDir */
	PharStreamWrite,  /* xWrite */
	PharStreamSeek,   /* xSeek */
	0,                /* xLock */
	PharDirRewind,    /* xRewindDir */
	PharStreamTell,   /* xTell */
	0,                /* xTrunc */
	0,                /* xSync */
	PharStreamStat    /* xStat */
};
PH7_PRIVATE int PH7_PharStreamIs(const ph7_io_stream *pStream)
{
	return pStream == &sPHAR_Stream;
}
/*
 * The stat family over a `phar://` path. php routes every one of its members
 * through the wrapper's url_stat, which is what makes `file_exists()`,
 * `is_dir()` and `filesize()` answer about the ARCHIVE's contents rather than
 * about a file of that name on disk -- there is none.
 */
PH7_PRIVATE int PH7_PharUrlStat(ph7_vm *pVm,const char *zPath,ph7_int64 *aVal)
{
	phl_phar *pPhar;
	phl_phar_ent *pEnt;
	const char *zEnt,*zErr;
	int nEnt = 0;
	pPhar = PharResolveUrl(pVm,zPath,(int)SyStrlen(zPath),&zEnt,&nEnt,&zErr);
	if( pPhar == 0 ){
		return -1;
	}
	while( nEnt > 0 && zEnt[0] == '/' ){ zEnt++; nEnt--; }
	while( nEnt > 0 && zEnt[nEnt-1] == '/' ){ nEnt--; }
	pEnt = PharFindEnt(pPhar,zEnt,nEnt);
	if( pEnt && !pEnt->bDir ){
		PharFillStat(pPhar,pEnt,0,aVal);
		return 0;
	}
	if( PharIsDir(pPhar,zEnt,nEnt) ){
		PharFillStat(pPhar,pEnt,1,aVal);
		return 0;
	}
	return -1;
}
/* ------------------------------------------------------------------ */
/* The classes                                                         */
/* ------------------------------------------------------------------ */
/*
 * A Phar OBJECT is two things at once, and php's own `spl_filesystem_object`
 * is why: an ARCHIVE (what every verb below asks about) and a DIRECTORY
 * ITERATOR positioned on one of its entries. That is what makes `getPath()`
 * answer the archive while `getFilename()` answers the entry the cursor is on,
 * and it is the whole reason Phar extends RecursiveDirectoryIterator rather
 * than holding one.
 */
#define PHAR_SLOT_RES  "__res"    /* the phl_phar */
#define PHAR_SLOT_CUR  "__cur"    /* the iteration cursor */
#define PHAR_SLOT_DIR  "__dir"    /* the directory this iterator walks ("" = the root) */
#define PHAR_SLOT_ENT  "__ent"    /* PharFileInfo: the entry name */
#define PHAR_SLOT_PHAR "__phar"   /* PharFileInfo: the archive's path */

/*
 * The two slots php's SplFileInfo keeps its pathname in. A Phar and a
 * PharFileInfo are both SplFileInfos, and every inherited accessor reads them,
 * so filling them is what makes `getBasename()`, `getExtension()` and
 * `__toString()` answer without a line of code here.
 */
static void PharSetSplPath(ph7_vm *pVm,ph7_class_instance *pThis,const char *zPath,int nPath)
{
	int nFile = nPath,nDir;
	while( nFile > 1 && (zPath[nFile-1] == '/' || zPath[nFile-1] == '\\') ){
		nFile--;
	}
	nDir = nFile;
	while( nDir > 1 && zPath[nDir-1] != '/' && zPath[nDir-1] != '\\' ){
		nDir--;
	}
	if( nDir > 0 ){
		nDir--;
	}
	PH7_NativeSetAttrStr(pVm,pThis,"__n",zPath,nFile);
	PH7_NativeSetAttrStr(pVm,pThis,"__p",zPath,nDir);
}
static phl_phar * PharOfInstance(ph7_class_instance *pThis)
{
	ph7_value *pRes;
	if( pThis == 0 ){
		return 0;
	}
	pRes = PH7_NativeAttr(pThis,PHAR_SLOT_RES);
	if( pRes == 0 || (pRes->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (phl_phar *)pRes->x.pOther;
}
static void PharAttach(ph7_class_instance *pThis,phl_phar *pPhar)
{
	ph7_value *pRes = PH7_NativeAttr(pThis,PHAR_SLOT_RES);
	if( pRes ){
		PH7_MemObjRelease(pRes);
		pRes->x.pOther = pPhar;
		MemObjSetType(pRes,MEMOBJ_RES);
	}
}
static void PharSetInt(ph7_class_instance *pThis,const char *zSlot,sxi64 iVal)
{
	ph7_value *pVal = PH7_NativeAttr(pThis,zSlot);
	if( pVal ){
		PH7_MemObjRelease(pVal);
		PH7_MemObjInitFromInt(pThis->pVm,pVal,iVal);
	}
}
static void PharSetStr(ph7_class_instance *pThis,const char *zSlot,const char *zVal,int nVal)
{
	ph7_value *pVal = PH7_NativeAttr(pThis,zSlot);
	if( pVal ){
		PH7_MemObjRelease(pVal);
		MemObjSetType(pVal,MEMOBJ_STRING);
		SyBlobReset(&pVal->sBlob);
		if( nVal > 0 ){
			SyBlobAppend(&pVal->sBlob,zVal,(sxu32)nVal);
		}
	}
}
/* The archive a method was called on, with php's refusal for an object whose
 * constructor never ran. */
static phl_phar * PharThis(ph7_context *pCtx)
{
	phl_phar *pPhar = PharOfInstance(PH7_ContextThis(pCtx));
	if( pPhar == 0 ){
		PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Cannot call method on an uninitialized Phar object");
		return 0;
	}
	return pPhar;
}
/* php's five write refusals, one per door. Each names its own door's verb,
 * which is why they cannot share a sentence. */
#define PHAR_RO_WRITE   0   /* "Cannot write out phar archive, phar is read-only" */
#define PHAR_RO_SETTING 1   /* "Write operations disabled by the php.ini setting phar.readonly" */
#define PHAR_RO_STUB    2   /* "Cannot change stub, phar is read-only" */
#define PHAR_RO_COMPRESS 3  /* "Phar is readonly, cannot change compression" */
static int PharRefuseWrite(ph7_context *pCtx,int eKind)
{
	phl_phar *pPhar = PharOfInstance(PH7_ContextThis(pCtx));
	if( !PharReadonly(pCtx->pVm) || (pPhar && pPhar->bData) ){
		/* `phar.readonly` guards the archives that can be EXECUTED. A PharData
		 * is a plain tar or zip and php lets a script write one whatever the
		 * directive says -- which is what makes it the class an installer uses
		 * to build a distribution archive on a stock php. */
		return 0;
	}
	switch( eKind ){
	case PHAR_RO_SETTING:
		PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Write operations disabled by the php.ini setting phar.readonly");
		break;
	case PHAR_RO_STUB:
		PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Cannot change stub, phar is read-only");
		break;
	case PHAR_RO_COMPRESS:
		PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Phar is readonly, cannot change compression");
		break;
	default:
		PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Cannot write out phar archive, phar is read-only");
		break;
	}
	return 1;
}
/*
 * A Phar object iterates a DIRECTORY of the archive, not its manifest: php's
 * `foreach ($phar as $f)` yields the five names at the archive's root, and a
 * RecursiveIteratorIterator over it descends into them to reach all 831. The
 * children of a directory are the distinct first segments of the entry names
 * under it, which is the same rule the wrapper's opendir() follows -- a
 * directory nothing declares still has children.
 *
 * The list is walked rather than built: an archive's manifest is short enough
 * that the Nth child costs one pass, and the alternative is a per-instance
 * cache the engine would have to free.
 */
static int PharNthChild(phl_phar *pPhar,const char *zDir,int nDir,sxi64 iIdx,
	SyBlob *pName,int *pbDir)
{
	phl_phar_ent *pEnt;
	SyBlob sSeen;
	int bFound = 0;
	/*
	 * php lists a phar directory SORTED by name (its own `phar_compare_dir_name`
	 * over the whole listing), not in manifest order -- composer.phar's manifest
	 * starts at `src/...` and php's first child is `LICENSE`. The walk below
	 * therefore counts children in byte order rather than in the order the
	 * entries appear.
	 */
	SyBlobInit(&sSeen,&pPhar->pVm->sAllocator);
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		const char *z = (const char *)SyBlobData(&pEnt->sName);
		int n = (int)SyBlobLength(&pEnt->sName);
		const char *zRest;
		int nRest,nSeg,bDup = 0;
		sxu32 k;
		if( nDir > 0 ){
			if( n <= nDir || z[nDir] != '/' || SyMemcmp(z,zDir,(sxu32)nDir) != 0 ){
				continue;
			}
			zRest = &z[nDir+1];
			nRest = n - nDir - 1;
		}else{
			zRest = z;
			nRest = n;
		}
		for( nSeg = 0 ; nSeg < nRest && zRest[nSeg] != '/' ; ++nSeg ){}
		if( nSeg < 1 ){
			continue;
		}
		/* One row per NAME, whatever sits under it. */
		for( k = 0 ; k + 1 < SyBlobLength(&sSeen) ; ){
			int nThis = (int)((const unsigned char *)SyBlobData(&sSeen))[k];
			if( nThis == nSeg
			 && SyMemcmp((const char *)SyBlobData(&sSeen) + k + 1,zRest,(sxu32)nSeg) == 0 ){
				bDup = 1;
				break;
			}
			k += (sxu32)nThis + 1;
		}
		if( bDup ){
			continue;
		}
		if( nSeg < 256 ){
			unsigned char cLen = (unsigned char)nSeg;
			SyBlobAppend(&sSeen,&cLen,1);
			SyBlobAppend(&sSeen,zRest,(sxu32)nSeg);
		}
		/* Every child is collected; which one the cursor wants is decided
		 * after the walk, in NAME order. */
		(void)0;
	}
	{
		/* Pick the iIdx-th smallest name out of what was collected. */
		const unsigned char *z = (const unsigned char *)SyBlobData(&sSeen);
		sxu32 nAll = SyBlobLength(&sSeen);
		sxu32 k;
		const char *zBest = 0;
		int nBest = 0;
		for(;;){
			const char *zMin = 0;
			int nMin = 0;
			for( k = 0 ; k + 1 <= nAll ; ){
				int nThis = (int)z[k];
				const char *zThis = (const char *)&z[k+1];
				int bAfterPrev = zBest == 0
					|| SyMemcmp(zThis,zBest,(sxu32)(nThis < nBest ? nThis : nBest)) > 0
					|| (SyMemcmp(zThis,zBest,(sxu32)(nThis < nBest ? nThis : nBest)) == 0
						&& nThis > nBest);
				if( bAfterPrev ){
					int bLess = zMin == 0
						|| SyMemcmp(zThis,zMin,(sxu32)(nThis < nMin ? nThis : nMin)) < 0
						|| (SyMemcmp(zThis,zMin,(sxu32)(nThis < nMin ? nThis : nMin)) == 0
							&& nThis < nMin);
					if( bLess ){
						zMin = zThis;
						nMin = nThis;
					}
				}
				k += (sxu32)nThis + 1;
			}
			if( zMin == 0 ){
				break;
			}
			if( iIdx == 0 ){
				SyBlobReset(pName);
				if( nDir > 0 ){
					SyBlobAppend(pName,zDir,(sxu32)nDir);
					SyBlobAppend(pName,"/",1);
				}
				SyBlobAppend(pName,zMin,(sxu32)nMin);
				if( pbDir ){
					SyBlob sFull;
					SyBlobInit(&sFull,&pPhar->pVm->sAllocator);
					SyBlobAppend(&sFull,SyBlobData(pName),SyBlobLength(pName));
					*pbDir = PharFindEnt(pPhar,(const char *)SyBlobData(&sFull),
						(int)SyBlobLength(&sFull)) == 0
						|| PharIsDir(pPhar,(const char *)SyBlobData(&sFull),
							(int)SyBlobLength(&sFull));
					SyBlobRelease(&sFull);
				}
				bFound = 1;
				break;
			}
			iIdx--;
			zBest = zMin;
			nBest = nMin;
		}
	}
	SyBlobRelease(&sSeen);
	return bFound;
}
/* The directory an instance walks, and the child its cursor is on. */
static void PharCursorDir(ph7_class_instance *pThis,SyBlob *pDir)
{
	ph7_value *pVal = PH7_NativeAttr(pThis,PHAR_SLOT_DIR);
	SyBlobReset(pDir);
	if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){
		SyBlobAppend(pDir,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));
	}
}
static int PharCursorChild(ph7_class_instance *pThis,phl_phar *pPhar,SyBlob *pName,int *pbDir)
{
	SyBlob sDir;
	int rc;
	SyBlobInit(&sDir,&pPhar->pVm->sAllocator);
	PharCursorDir(pThis,&sDir);
	rc = PharNthChild(pPhar,(const char *)SyBlobData(&sDir),(int)SyBlobLength(&sDir),
		PH7_NativeAttrInt(pThis,PHAR_SLOT_CUR),pName,pbDir);
	SyBlobRelease(&sDir);
	return rc;
}
/* Build `phar://<archive>/<entry>`, the name every phar path is written as. */
static void PharUrl(phl_phar *pPhar,const char *zEnt,int nEnt,SyBlob *pOut)
{
	SyBlobAppend(pOut,"phar://",sizeof("phar://")-1);
	SyBlobAppend(pOut,SyBlobData(&pPhar->sPath),SyBlobLength(&pPhar->sPath));
	SyBlobAppend(pOut,"/",1);
	if( nEnt > 0 ){
		SyBlobAppend(pOut,zEnt,(sxu32)nEnt);
	}
}
/* A PharFileInfo for one entry: php hands one back from offsetGet() and from
 * every step of the iteration. */
static ph7_class_instance * PharNewFileInfo(ph7_context *pCtx,phl_phar *pPhar,
	phl_phar_ent *pEnt)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm,"PharFileInfo",sizeof("PharFileInfo")-1,FALSE,0);
	ph7_class_instance *pThis;
	SyBlob sUrl;
	if( pClass == 0 ){
		return 0;
	}
	pThis = PH7_NewClassInstance(pVm,pClass);
	if( pThis == 0 ){
		return 0;
	}
	SyBlobInit(&sUrl,&pVm->sAllocator);
	PharUrl(pPhar,(const char *)SyBlobData(&pEnt->sName),(int)SyBlobLength(&pEnt->sName),&sUrl);
	/* The inherited SplFileInfo half answers about the ENTRY's url, which is
	 * what php shows for `getPathname()`. */
	PharSetSplPath(pVm,pThis,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));
	PharSetStr(pThis,PHAR_SLOT_ENT,(const char *)SyBlobData(&pEnt->sName),
		(int)SyBlobLength(&pEnt->sName));
	PharSetStr(pThis,PHAR_SLOT_PHAR,(const char *)SyBlobData(&pPhar->sPath),
		(int)SyBlobLength(&pPhar->sPath));
	PharAttach(pThis,pPhar);
	SyBlobRelease(&sUrl);
	return pThis;
}
/* The entry a PharFileInfo names. */
static phl_phar_ent * PharFileInfoEnt(ph7_context *pCtx,phl_phar **ppPhar)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	ph7_value *pName;
	if( pPhar == 0 ){
		PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Cannot call method on an uninitialized PharFileInfo object");
		return 0;
	}
	pName = PH7_NativeAttr(pThis,PHAR_SLOT_ENT);
	if( ppPhar ){
		*ppPhar = pPhar;
	}
	if( pName == 0 ){
		return 0;
	}
	return PharFindEnt(pPhar,(const char *)SyBlobData(&pName->sBlob),
		(int)SyBlobLength(&pName->sBlob));
}
/*
 * Which format a NAME asks for. php reads the extension: `.phar` is its own
 * format and only an executable archive may carry it, `.tar`/`.tar.gz`/`.tgz`
 * are tar and `.zip` is zip. Anything else is not an archive name at all.
 */
static int PharFormatFromName(const char *zPath,int nPath,int bData)
{
	static const struct { const char *zExt; int iFmt; int bDataOnly; } aExt[] = {
		{ ".phar", PHAR_FORMAT_PHAR, 0 },
		{ ".tar",  PHAR_FORMAT_TAR,  1 },
		{ ".tgz",  PHAR_FORMAT_TAR,  1 },
		{ ".zip",  PHAR_FORMAT_ZIP,  1 }
	};
	sxu32 k;
	for( k = 0 ; k < SX_ARRAYSIZE(aExt) ; ++k ){
		int nExt = (int)SyStrlen(aExt[k].zExt);
		int i;
		for( i = 0 ; i + nExt <= nPath ; ++i ){
			if( SyStrnicmp(&zPath[i],aExt[k].zExt,(sxu32)nExt) != 0 ){
				continue;
			}
			if( aExt[k].bDataOnly && !bData ){
				/* php lets a Phar carry a `.phar.tar` name; a bare `.tar` is a
				 * data archive's. */
				continue;
			}
			if( !aExt[k].bDataOnly && bData ){
				/* ...and a PharData may not be a `.phar`. */
				return 0;
			}
			return aExt[k].iFmt;
		}
	}
	return 0;
}
/* An archive with nothing in it, of the format the name asked for. */
static phl_phar * PharNewEmpty(ph7_vm *pVm,const char *zPath,int nPath,int iFmt,int bData)
{
	phl_phar *pPhar = (phl_phar *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar));
	if( pPhar == 0 ){
		return 0;
	}
	SyZero(pPhar,sizeof(*pPhar));
	pPhar->pVm = pVm;
	pPhar->bData = bData;
	pPhar->iFormat = iFmt;
	pPhar->nApi = PHAR_API_WORD;
	SyBlobInit(&pPhar->sPath,&pVm->sAllocator);
	SyBlobInit(&pPhar->sAlias,&pVm->sAllocator);
	SyBlobInit(&pPhar->sStub,&pVm->sAllocator);
	SyBlobInit(&pPhar->sMeta,&pVm->sAllocator);
	SyBlobInit(&pPhar->sFile,&pVm->sAllocator);
	SyBlobInit(&pPhar->sSig,&pVm->sAllocator);
	SyBlobAppend(&pPhar->sPath,zPath,(sxu32)nPath);
	pPhar->pNext = (phl_phar *)pVm->pPhars;
	pVm->pPhars = pPhar;
	return pPhar;
}
/* ---- Phar::__construct ---- */
static int vm_builtin_Phar_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar;
	const char *zPath,*zErr;
	int nPath = 0,bData;
	if( nArg < 1 || pThis == 0 ){
		return PH7_OK;
	}
	{
		ph7_class *pData = PH7_VmExtractClass(pCtx->pVm,"PharData",sizeof("PharData")-1,FALSE,0);
		bData = pData != 0 && pThis->pClass != 0 && PH7_VmInstanceOf(pThis->pClass,pData);
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	if( !bData && PharFormatFromName(zPath,nPath,0) == 0 ){
		/* php screens the NAME of an executable archive before it looks at the
		 * file: a `.phar` somewhere in the last component is what makes a name
		 * one at all. A PharData is not screened -- it opens whatever it is
		 * given and reports what the format reader found. */
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Cannot create phar '%.*s', file extension (or combination) not recognised or the directory does not exist",
			nPath,zPath);
	}
	if( !PharPathIsFile(pCtx->pVm,zPath) && PharFindByPath(pCtx->pVm,zPath,nPath) == 0 ){
		/*
		 * A name with no file behind it is a NEW archive, and php decides three
		 * things about it before creating one: whether the extension names a
		 * format at all, whether an EXECUTABLE archive may be created (that is
		 * what `phar.readonly` guards -- a PharData is exempt, which is why an
		 * installer can build a .tar on a stock php), and which of the three
		 * formats the name asks for. Nothing is written until something is
		 * added: php's file does not appear at construction either.
		 */
		int iFmt = PharFormatFromName(zPath,nPath,bData);
		if( iFmt == 0 ){
			return PH7_VmThrowException(pCtx,"UnexpectedValueException",
				"Cannot create phar '%.*s', file extension (or combination) not recognised or the directory does not exist",
				nPath,zPath);
		}
		if( !bData && PharReadonly(pCtx->pVm) ){
			return PH7_VmThrowException(pCtx,"UnexpectedValueException",
				"creating archive \"%.*s\" disabled by the php.ini setting phar.readonly",
				nPath,zPath);
		}
		pPhar = PharNewEmpty(pCtx->pVm,zPath,nPath,iFmt,bData);
		if( pPhar == 0 ){
			return PH7_VmThrowException(pCtx,"UnexpectedValueException",
				"Cannot create phar '%.*s', file extension (or combination) not recognised or the directory does not exist",
				nPath,zPath);
		}
		PharAttach(pThis,pPhar);
		PharSetInt(pThis,PHAR_SLOT_CUR,0);
		PharSetSplPath(pCtx->pVm,pThis,zPath,nPath);
		return PH7_OK;
	}
	pPhar = PharOpenPath(pCtx->pVm,zPath,nPath,bData,&zErr);
	if( pPhar == 0 ){
		/* php's own two sentences: one for a file it cannot read at all, one for
		 * a file whose contents are not an archive. */
		if( zErr && SyStrncmp(zErr,"unable to open",sizeof("unable to open")-1) == 0 ){
			return PH7_VmThrowException(pCtx,"UnexpectedValueException",
				"Cannot open phar file \"%.*s\": %s",nPath,zPath,zErr);
		}
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"internal corruption of phar \"%.*s\" (%s)",nPath,zPath,
			zErr ? zErr : "truncated entry");
	}
	if( bData ){
		pPhar->bData = 1;
	}
	PharAttach(pThis,pPhar);
	PharSetInt(pThis,PHAR_SLOT_CUR,0);
	/* The SplFileInfo half is the ARCHIVE itself. */
	PharSetSplPath(pCtx->pVm,pThis,zPath,nPath);
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		int nAlias = 0;
		const char *zAlias = ph7_value_to_string(apArg[2],&nAlias);
		SyBlobReset(&pPhar->sAlias);
		SyBlobAppend(&pPhar->sAlias,zAlias,(sxu32)nAlias);
	}
	return PH7_OK;
}
/* ---- the read verbs ---- */
static int vm_builtin_Phar_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,pPhar ? (sxi64)pPhar->nEnt : 0);
	return PH7_OK;
}
static int vm_builtin_Phar_getAlias(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 || SyBlobLength(&pPhar->sAlias) < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pPhar->sAlias),
		(int)SyBlobLength(&pPhar->sAlias));
	return PH7_OK;
}
static int vm_builtin_Phar_getPath(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pPhar->sPath),
		(int)SyBlobLength(&pPhar->sPath));
	return PH7_OK;
}
static int vm_builtin_Phar_getVersion(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	char zBuf[16];
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	if( pPhar->iFormat != PHAR_FORMAT_PHAR ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	SyBufferFormat(zBuf,sizeof(zBuf),"%d.%d.%d",
		(int)((pPhar->nApi >> 12) & 0xF),(int)((pPhar->nApi >> 8) & 0xF),
		(int)((pPhar->nApi >> 4) & 0xF));
	ph7_result_string(pCtx,zBuf,-1);
	return PH7_OK;
}
static int vm_builtin_Phar_getSignature(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	ph7_value *pArray,*pVal;
	const char *zType;
	SyBlob sHex;
	sxu32 i;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	if( pPhar->iSigType == 0 || SyBlobLength(&pPhar->sSig) < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	switch( pPhar->iSigType ){
	case PHAR_SIG_MD5:    zType = "MD5";     break;
	case PHAR_SIG_SHA1:   zType = "SHA-1";   break;
	case PHAR_SIG_SHA256: zType = "SHA-256"; break;
	case PHAR_SIG_SHA512: zType = "SHA-512"; break;
	default:              zType = "OpenSSL"; break;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php prints the digest in UPPER-case hex. */
	SyBlobInit(&sHex,&pCtx->pVm->sAllocator);
	for( i = 0 ; i < SyBlobLength(&pPhar->sSig) ; ++i ){
		static const char zHex[] = "0123456789ABCDEF";
		unsigned char c = ((const unsigned char *)SyBlobData(&pPhar->sSig))[i];
		char zPair[2];
		zPair[0] = zHex[(c >> 4) & 0xF];
		zPair[1] = zHex[c & 0xF];
		SyBlobAppend(&sHex,zPair,2);
	}
	ph7_value_string(pVal,(const char *)SyBlobData(&sHex),(int)SyBlobLength(&sHex));
	ph7_array_add_strkey_elem(pArray,"hash",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_string(pVal,zType,-1);
	ph7_array_add_strkey_elem(pArray,"hash_type",pVal);
	ph7_result_value(pCtx,pArray);
	ph7_context_release_value(pCtx,pVal);
	SyBlobRelease(&sHex);
	return PH7_OK;
}
static int vm_builtin_Phar_getStub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pPhar->sStub),
		(int)SyBlobLength(&pPhar->sStub));
	return PH7_OK;
}
/* php-serialized metadata, handed back as the VALUE it was. */
static void PharMetaResult(ph7_context *pCtx,SyBlob *pMeta)
{
	ph7_value sVal;
	int nRead = 0;
	if( SyBlobLength(pMeta) < 1 ){
		ph7_result_null(pCtx);
		return;
	}
	PH7_MemObjInit(pCtx->pVm,&sVal);
	if( PH7_VmUnserializeOne(pCtx,(const char *)SyBlobData(pMeta),
			(int)SyBlobLength(pMeta),&nRead,&sVal) != SXRET_OK ){
		PH7_MemObjRelease(&sVal);
		ph7_result_null(pCtx);
		return;
	}
	ph7_result_value(pCtx,&sVal);
	PH7_MemObjRelease(&sVal);
}
static int vm_builtin_Phar_getMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar ){
		PharMetaResult(pCtx,&pPhar->sMeta);
	}
	return PH7_OK;
}
static int vm_builtin_Phar_hasMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,pPhar && SyBlobLength(&pPhar->sMeta) > 0);
	return PH7_OK;
}
static int vm_builtin_Phar_isFileFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	sxi64 iFmt = nArg > 0 ? ph7_value_to_int64(apArg[0]) : -1;
	if( pPhar == 0 ){
		return PH7_OK;
	}
	if( iFmt != 1 && iFmt != 2 && iFmt != 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_VmThrowException(pCtx,"UnexpectedValueException","Unknown file format specified");
	}
	ph7_result_bool(pCtx,(int)iFmt == pPhar->iFormat);
	return PH7_OK;
}
static int vm_builtin_Phar_isCompressed(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	SXUNUSED(pPhar);
	/* A whole-archive compression is a `.phar.gz`, which this build opens by
	 * decompressing it: the OPEN archive is never itself compressed. */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
static int vm_builtin_Phar_isWritable(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,pPhar != 0 && (pPhar->bData || !PharReadonly(pCtx->pVm)));
	return PH7_OK;
}
static int vm_builtin_Phar_false(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
static int vm_builtin_Phar_void(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_true(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* ---- ArrayAccess ---- */
static int vm_builtin_Phar_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	const char *zName;
	int nName = 0;
	if( pPhar == 0 || nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	ph7_result_bool(pCtx,PharFindEnt(pPhar,zName,nName) != 0);
	return PH7_OK;
}
static int vm_builtin_Phar_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	phl_phar_ent *pEnt;
	ph7_class_instance *pInfo;
	const char *zName;
	int nName = 0;
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	pEnt = PharFindEnt(pPhar,zName,nName);
	if( pEnt == 0 ){
		return PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Entry %.*s does not exist",nName,zName);
	}
	pInfo = PharNewFileInfo(pCtx,pPhar,pEnt);
	if( pInfo == 0 ){
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx,pInfo);
	return PH7_OK;
}
static int vm_builtin_Phar_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	phl_phar_ent *pEnt;
	const char *zName,*zData;
	int nName = 0,nData = 0;
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	zData = ph7_value_to_string(apArg[1],&nData);
	pEnt = PharFindEnt(pPhar,zName,nName);
	if( pEnt == 0 ){
		pEnt = PharNewEnt(pPhar,zName,nName);
		if( pEnt == 0 ){
			return PH7_OK;
		}
		pEnt->nFlags = 0666;
	}
	SyBlobReset(&pEnt->sData);
	SyBlobAppend(&pEnt->sData,zData,(sxu32)nData);
	pEnt->bLoaded = 1;
	pEnt->bDir = 0;
	pEnt->nSize = (sxu32)nData;
	pEnt->iTime = (sxi64)time(0);
	PharCommit(pPhar);
	return PH7_OK;
}
static int vm_builtin_Phar_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	phl_phar_ent *pEnt,*pPrev = 0;
	const char *zName;
	int nName = 0;
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	for( pEnt = pPhar->pFirst ; pEnt ; pPrev = pEnt, pEnt = pEnt->pNext ){
		if( (int)SyBlobLength(&pEnt->sName) == nName
		 && SyMemcmp(SyBlobData(&pEnt->sName),zName,(sxu32)nName) == 0 ){
			if( pPrev ){
				pPrev->pNext = pEnt->pNext;
			}else{
				pPhar->pFirst = pEnt->pNext;
			}
			if( pPhar->pLast == pEnt ){
				pPhar->pLast = pPrev;
			}
			pPhar->nEnt--;
			PharEntFree(pPhar->pVm,pEnt);
			PharCommit(pPhar);
			break;
		}
	}
	return PH7_OK;
}
/* ---- the iterator half ---- */
static int vm_builtin_Phar_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pThis ){
		PharSetInt(pThis,PHAR_SLOT_CUR,0);
		/* php rebuilds the SplFileInfo name from the iterator here, with an
		 * empty directory part -- which is why `getPathname()` on a rewound
		 * Phar is the bare entry name. */
		PharSetStr(pThis,"__p","",0);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	SyBlob sName;
	int bOk = 0;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar ){
		SyBlobInit(&sName,&pCtx->pVm->sAllocator);
		bOk = PharCursorChild(pThis,pPhar,&sName,0);
		SyBlobRelease(&sName);
	}
	ph7_result_bool(pCtx,bOk);
	return PH7_OK;
}
static int vm_builtin_Phar_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pThis ){
		PharSetInt(pThis,PHAR_SLOT_CUR,PH7_NativeAttrInt(pThis,PHAR_SLOT_CUR) + 1);
		PharSetStr(pThis,"__p","",0);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	SyBlob sUrl,sName;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	if( !PharCursorChild(pThis,pPhar,&sName,0) ){
		SyBlobRelease(&sName);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* php's key is the whole `phar://` url of the entry. */
	SyBlobInit(&sUrl,&pCtx->pVm->sAllocator);
	PharUrl(pPhar,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName),&sUrl);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));
	SyBlobRelease(&sUrl);
	SyBlobRelease(&sName);
	return PH7_OK;
}
/*
 * A PharFileInfo for the child the cursor is on. A DIRECTORY child gets one
 * too -- php answers `isDir()` from it -- even when the manifest has no entry
 * of that name, which is the ordinary case for `src` in an archive that only
 * lists files under it.
 */
static ph7_class_instance * PharChildInfo(ph7_context *pCtx,phl_phar *pPhar,
	const char *zName,int nName,int bDir)
{
	phl_phar_ent *pEnt = PharFindEnt(pPhar,zName,nName);
	if( pEnt ){
		return PharNewFileInfo(pCtx,pPhar,pEnt);
	}
	if( bDir ){
		/* A directory the names IMPLY. Its info object carries the name and the
		 * archive, and every question about it is answered from those. */
		ph7_vm *pVm = pCtx->pVm;
		ph7_class *pClass = PH7_VmExtractClass(pVm,"PharFileInfo",sizeof("PharFileInfo")-1,FALSE,0);
		ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
		SyBlob sUrl;
		if( pThis == 0 ){
			return 0;
		}
		SyBlobInit(&sUrl,&pVm->sAllocator);
		PharUrl(pPhar,zName,nName,&sUrl);
		PharSetSplPath(pVm,pThis,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));
		PharSetStr(pThis,PHAR_SLOT_ENT,zName,nName);
		PharSetStr(pThis,PHAR_SLOT_PHAR,(const char *)SyBlobData(&pPhar->sPath),
			(int)SyBlobLength(&pPhar->sPath));
		PharAttach(pThis,pPhar);
		SyBlobRelease(&sUrl);
		return pThis;
	}
	return 0;
}
static int vm_builtin_Phar_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	ph7_class_instance *pInfo;
	SyBlob sName;
	int bDir = 0;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	if( !PharCursorChild(pThis,pPhar,&sName,&bDir) ){
		SyBlobRelease(&sName);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pInfo = PharChildInfo(pCtx,pPhar,(const char *)SyBlobData(&sName),
		(int)SyBlobLength(&sName),bDir);
	SyBlobRelease(&sName);
	if( pInfo ){
		PH7_NativeResultObject(pCtx,pInfo);
	}
	return PH7_OK;
}
/* The RECURSIVE half: a directory child has children, and descending into one
 * answers another Phar -- php hands back the same class, positioned on the
 * subdirectory. */
static int vm_builtin_Phar_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	SyBlob sName;
	int bDir = 0;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar ){
		SyBlobInit(&sName,&pCtx->pVm->sAllocator);
		if( !PharCursorChild(pThis,pPhar,&sName,&bDir) ){
			bDir = 0;
		}
		SyBlobRelease(&sName);
	}
	ph7_result_bool(pCtx,bDir);
	return PH7_OK;
}
static int vm_builtin_Phar_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	ph7_class_instance *pKid;
	SyBlob sName;
	int bDir = 0;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	if( !PharCursorChild(pThis,pPhar,&sName,&bDir) || !bDir ){
		SyBlobRelease(&sName);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pKid = pThis->pClass ? PH7_NewClassInstance(pCtx->pVm,pThis->pClass) : 0;
	if( pKid ){
		PharAttach(pKid,pPhar);
		PharSetInt(pKid,PHAR_SLOT_CUR,0);
		PharSetStr(pKid,PHAR_SLOT_DIR,(const char *)SyBlobData(&sName),
			(int)SyBlobLength(&sName));
		PharSetSplPath(pCtx->pVm,pKid,(const char *)SyBlobData(&pPhar->sPath),
			(int)SyBlobLength(&pPhar->sPath));
		PH7_NativeResultObject(pCtx,pKid);
	}
	SyBlobRelease(&sName);
	return PH7_OK;
}
/*
 * `getFilename()` on a Phar is the ITERATOR's name, not the archive's, and
 * `getPathname()` is php's own quirk: the archive's path and the entry while
 * the object is fresh, and the bare entry once the iterator has been rewound
 * (php rebuilds the slot from an empty directory part there). Both are what a
 * program printing `$phar` sees.
 */
static int vm_builtin_Phar_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	SyBlob sName;
	const char *z;
	int n,i;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	if( !PharCursorChild(pThis,pPhar,&sName,0) ){
		SyBlobRelease(&sName);
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	z = (const char *)SyBlobData(&sName);
	n = (int)SyBlobLength(&sName);
	for( i = n ; i > 0 ; --i ){
		if( z[i-1] == '/' ){
			break;
		}
	}
	ph7_result_string(pCtx,&z[i],n - i);
	SyBlobRelease(&sName);
	return PH7_OK;
}
static int vm_builtin_Phar_getPathname(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	ph7_value *pPath;
	SyBlob sName,sOut;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	pPath = PH7_NativeAttr(pThis,"__p");
	if( pPath && (pPath->iFlags & MEMOBJ_STRING) && SyBlobLength(&pPath->sBlob) > 0 ){
		SyBlobAppend(&sOut,SyBlobData(&pPath->sBlob),SyBlobLength(&pPath->sBlob));
		SyBlobAppend(&sOut,"/",1);
	}
	if( PharCursorChild(pThis,pPhar,&sName,0) ){
		const char *z = (const char *)SyBlobData(&sName);
		int n = (int)SyBlobLength(&sName),i;
		for( i = n ; i > 0 ; --i ){
			if( z[i-1] == '/' ){
				break;
			}
		}
		SyBlobAppend(&sOut,&z[i],(sxu32)(n - i));
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sName);
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* ---- extraction ---- */
/*
 * php's `extractTo()`: write the archive's entries out as real files. It is the
 * one READ verb that touches the filesystem, and the one Composer's tar
 * downloader calls.
 */
static int PharExtractOne(ph7_context *pCtx,phl_phar *pPhar,phl_phar_ent *pEnt,
	const char *zDir,int nDir,int bOverwrite)
{
	ph7_vm *pVm = pCtx->pVm;
	const ph7_io_stream *pStream;
	SyBlob sPath;
	const char *zPath;
	void *pHandle;
	int rc = 0;
	if( PharEntLoad(pPhar,pEnt) != 0 ){
		return -1;
	}
	SyBlobInit(&sPath,&pVm->sAllocator);
	SyBlobAppend(&sPath,zDir,(sxu32)nDir);
	SyBlobAppend(&sPath,"/",1);
	SyBlobAppend(&sPath,SyBlobData(&pEnt->sName),SyBlobLength(&pEnt->sName));
	SyBlobNullAppend(&sPath);
	zPath = (const char *)SyBlobData(&sPath);
	/* Every directory on the way, as php makes them. */
	{
		const ph7_vfs *pVfs = pVm->pEngine ? pVm->pEngine->pVfs : 0;
		sxu32 i;
		for( i = (sxu32)nDir + 1 ; i < SyBlobLength(&sPath) ; ++i ){
			if( zPath[i] != '/' ){
				continue;
			}
			((char *)zPath)[i] = 0;
			if( pVfs && pVfs->xMkdir ){
				pVfs->xMkdir(zPath,0777,FALSE);
			}
			((char *)zPath)[i] = '/';
		}
		if( pEnt->bDir ){
			if( pVfs && pVfs->xMkdir ){
				pVfs->xMkdir(zPath,0777,FALSE);
			}
			SyBlobRelease(&sPath);
			return 0;
		}
		if( !bOverwrite && pVfs && pVfs->xFileExists && pVfs->xFileExists(zPath) == PH7_OK ){
			SyBlobRelease(&sPath);
			return 1;   /* php refuses to clobber unless asked */
		}
	}
	pStream = PH7_VmGetStreamDevice(pVm,&zPath,(int)SyBlobLength(&sPath));
	pHandle = pStream ? PH7_StreamOpenHandle(pVm,pStream,zPath,
		PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,0) : 0;
	if( pHandle == 0 ){
		rc = -1;
	}else{
		if( pStream->xWrite && SyBlobLength(&pEnt->sData) > 0 ){
			pStream->xWrite(pHandle,SyBlobData(&pEnt->sData),
				(ph7_int64)SyBlobLength(&pEnt->sData));
		}
		PH7_StreamCloseHandle(pStream,pHandle);
	}
	SyBlobRelease(&sPath);
	return rc;
}
static int vm_builtin_Phar_extractTo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = PharThis(pCtx);
	phl_phar_ent *pEnt;
	const char *zDir;
	int nDir = 0,bOverwrite = 0,bAll = 1;
	if( pPhar == 0 || nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zDir = ph7_value_to_string(apArg[0],&nDir);
	while( nDir > 1 && zDir[nDir-1] == '/' ){
		nDir--;
	}
	if( nArg > 2 ){
		bOverwrite = ph7_value_to_bool(apArg[2]);
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		bAll = 0;
	}
	{
		const ph7_vfs *pVfs = pCtx->pVm->pEngine ? pCtx->pVm->pEngine->pVfs : 0;
		SyBlob sDir;
		SyBlobInit(&sDir,&pCtx->pVm->sAllocator);
		SyBlobAppend(&sDir,zDir,(sxu32)nDir);
		SyBlobNullAppend(&sDir);
		if( pVfs && pVfs->xMkdir ){
			pVfs->xMkdir((const char *)SyBlobData(&sDir),0777,TRUE);
		}
		SyBlobRelease(&sDir);
	}
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		if( !bAll ){
			/* php takes one name or a list of them. */
			int bWanted = 0;
			if( ph7_value_is_array(apArg[1]) ){
				ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;
				ph7_hashmap_node *pNode;
				pMap->pCur = pMap->pFirst;
				while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){
					ph7_value sVal;
					const char *zWant;
					int nWant = 0;
					PH7_MemObjInit(pCtx->pVm,&sVal);
					PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
					zWant = ph7_value_to_string(&sVal,&nWant);
					if( (int)SyBlobLength(&pEnt->sName) == nWant
					 && SyMemcmp(SyBlobData(&pEnt->sName),zWant,(sxu32)nWant) == 0 ){
						bWanted = 1;
					}
					PH7_MemObjRelease(&sVal);
				}
			}else{
				int nWant = 0;
				const char *zWant = ph7_value_to_string(apArg[1],&nWant);
				bWanted = (int)SyBlobLength(&pEnt->sName) == nWant
					&& SyMemcmp(SyBlobData(&pEnt->sName),zWant,(sxu32)nWant) == 0;
			}
			if( !bWanted ){
				continue;
			}
		}
		if( PharExtractOne(pCtx,pPhar,pEnt,zDir,nDir,bOverwrite) < 0 ){
			return PH7_VmThrowException(pCtx,"PharException",
				"Extraction from phar \"%.*s\" failed: Cannot extract \"%.*s\"",
				(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath),
				(int)SyBlobLength(&pEnt->sName),(const char *)SyBlobData(&pEnt->sName));
		}
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* ---- the write verbs ---- */
static int PharAddBytes(ph7_context *pCtx,phl_phar *pPhar,const char *zName,int nName,
	const char *zData,int nData,int bDir)
{
	phl_phar_ent *pEnt = PharFindEnt(pPhar,zName,nName);
	if( pEnt == 0 ){
		pEnt = PharNewEnt(pPhar,zName,nName);
		if( pEnt == 0 ){
			return -1;
		}
		pEnt->nFlags = bDir ? 0777 : 0666;
	}
	SyBlobReset(&pEnt->sData);
	if( nData > 0 ){
		SyBlobAppend(&pEnt->sData,zData,(sxu32)nData);
	}
	pEnt->bLoaded = 1;
	pEnt->bDir = (sxu8)bDir;
	pEnt->nSize = (sxu32)(nData > 0 ? nData : 0);
	pEnt->iTime = (sxi64)time(0);
	SXUNUSED(pCtx);
	return 0;
}
static int vm_builtin_Phar_addFromString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	const char *zName,*zData;
	int nName = 0,nData = 0;
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	if( PharReadonly(pCtx->pVm) && !pPhar->bData ){
		/* php's addFromString names the ENTRY and the archive. */
		return PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Entry %.*s does not exist and cannot be created: phar error: file \"%.*s\" in phar \"%.*s\" cannot be opened for writing, disabled by ini setting",
			nName,zName,nName,zName,
			(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	if( PharAddBytes(pCtx,pPhar,zName,nName,zData,nData,0) == 0 ){
		PharCommit(pPhar);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_addFile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	const ph7_io_stream *pStream;
	const char *zFile,*zLocal,*zPath;
	SyBlob sData;
	void *pHandle;
	int nFile = 0,nLocal = 0;
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zFile = ph7_value_to_string(apArg[0],&nFile);
	if( PharReadonly(pCtx->pVm) && !pPhar->bData ){
		return PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Write operations disabled by the php.ini setting phar.readonly");
	}
	zLocal = zFile;
	nLocal = nFile;
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		zLocal = ph7_value_to_string(apArg[1],&nLocal);
	}
	zPath = zFile;
	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nFile);
	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,
		FALSE,0,FALSE,0,0) : 0;
	if( pHandle == 0 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"phar error: unable to open file \"%.*s\" to add to phar archive",nFile,zFile);
	}
	SyBlobInit(&sData,&pCtx->pVm->sAllocator);
	PH7_StreamReadWholeFile(pHandle,pStream,&sData);
	PH7_StreamCloseHandle(pStream,pHandle);
	if( PharAddBytes(pCtx,pPhar,zLocal,nLocal,(const char *)SyBlobData(&sData),
			(int)SyBlobLength(&sData),0) == 0 ){
		PharCommit(pPhar);
	}
	SyBlobRelease(&sData);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_addEmptyDir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	const char *zName;
	int nName = 0;
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	if( PharAddBytes(pCtx,pPhar,zName,nName,"",0,1) == 0 ){
		PharCommit(pPhar);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_delete(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	phl_phar_ent *pEnt,*pPrev = 0;
	const char *zName;
	int nName = 0;
	if( PharRefuseWrite(pCtx,PHAR_RO_WRITE) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	for( pEnt = pPhar->pFirst ; pEnt ; pPrev = pEnt, pEnt = pEnt->pNext ){
		if( (int)SyBlobLength(&pEnt->sName) == nName
		 && SyMemcmp(SyBlobData(&pEnt->sName),zName,(sxu32)nName) == 0 ){
			if( pPrev ){
				pPrev->pNext = pEnt->pNext;
			}else{
				pPhar->pFirst = pEnt->pNext;
			}
			if( pPhar->pLast == pEnt ){
				pPhar->pLast = pPrev;
			}
			pPhar->nEnt--;
			PharEntFree(pPhar->pVm,pEnt);
			PharCommit(pPhar);
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
	}
	return PH7_VmThrowException(pCtx,"BadMethodCallException",
		"Entry %.*s does not exist and cannot be deleted",nName,zName);
}
static int vm_builtin_Phar_setStub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	const char *zStub;
	int nStub = 0;
	if( PharRefuseWrite(pCtx,PHAR_RO_STUB) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zStub = ph7_value_to_string(apArg[0],&nStub);
	if( pPhar->iFormat != PHAR_FORMAT_PHAR ){
		/* php refuses a stub on a plain archive: there is nowhere to put it. */
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"A Phar stub cannot be set in a plain %s archive",
			pPhar->iFormat == PHAR_FORMAT_ZIP ? "zip" : "tar");
	}
	SyBlobReset(&pPhar->sStub);
	SyBlobAppend(&pPhar->sStub,zStub,(sxu32)nStub);
	/* php closes the stub itself when the script did not: the manifest has to
	 * start after a `?>` and a line ending, and `getStub()` shows the closed
	 * form. */
	if( nStub < 3 || SyMemcmp(&zStub[nStub-2],"?>",2) != 0 ){
		SyBlobAppend(&pPhar->sStub," ?>\r\n",sizeof(" ?>\r\n")-1);
	}
	PharCommit(pPhar);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_Phar_setAlias(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	const char *zAlias;
	int nAlias = 0;
	if( PharRefuseWrite(pCtx,PHAR_RO_WRITE) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zAlias = ph7_value_to_string(apArg[0],&nAlias);
	SyBlobReset(&pPhar->sAlias);
	SyBlobAppend(&pPhar->sAlias,zAlias,(sxu32)nAlias);
	PharCommit(pPhar);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_Phar_setMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	SyBlobReset(&pPhar->sMeta);
	PH7_VmSerializeValue(pCtx,apArg[0],&pPhar->sMeta);
	PharCommit(pPhar);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_delMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar ){
		SyBlobReset(&pPhar->sMeta);
		PharCommit(pPhar);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_Phar_compressFiles(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	phl_phar_ent *pEnt;
	sxi64 iComp;
	if( PharRefuseWrite(pCtx,PHAR_RO_COMPRESS) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	iComp = ph7_value_to_int64(apArg[0]);
	if( iComp != PHAR_C_GZ && iComp != PHAR_C_NONE ){
		return PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Cannot compress with Bzip2 compression, bz2 extension is not enabled");
	}
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		if( PharEntLoad(pPhar,pEnt) != 0 ){
			continue;
		}
		pEnt->nFlags = (pEnt->nFlags & ~(sxu32)PHAR_C_MASK) | (sxu32)iComp;
	}
	PharCommit(pPhar);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_decompressFiles(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	phl_phar_ent *pEnt;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( PharRefuseWrite(pCtx,PHAR_RO_COMPRESS) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 ){
		return PH7_OK;
	}
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		if( PharEntLoad(pPhar,pEnt) != 0 ){
			continue;
		}
		pEnt->nFlags &= ~(sxu32)PHAR_C_MASK;
	}
	PharCommit(pPhar);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_Phar_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	phl_phar_ent *pFrom;
	const char *zFrom,*zTo;
	int nFrom = 0,nTo = 0;
	if( PharRefuseWrite(pCtx,PHAR_RO_WRITE) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 2 ){
		return PH7_OK;
	}
	zFrom = ph7_value_to_string(apArg[0],&nFrom);
	zTo = ph7_value_to_string(apArg[1],&nTo);
	pFrom = PharFindEnt(pPhar,zFrom,nFrom);
	if( pFrom == 0 || PharEntLoad(pPhar,pFrom) != 0 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"file \"%.*s\" does not exist in phar \"%.*s\"",nFrom,zFrom,
			(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));
	}
	if( PharAddBytes(pCtx,pPhar,zTo,nTo,(const char *)SyBlobData(&pFrom->sData),
			(int)SyBlobLength(&pFrom->sData),0) == 0 ){
		PharCommit(pPhar);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* php's buffering pair only decides WHEN the file is rewritten; every door here
 * rewrites at once, so these two are the no-ops php's own are for an archive
 * that is not being written. */
static int vm_builtin_Phar_startBuffering(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_Phar_setSignatureAlgorithm(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	sxi64 iAlgo;
	if( PharRefuseWrite(pCtx,PHAR_RO_WRITE) ){
		return PH7_OK;
	}
	pPhar = PharThis(pCtx);
	if( pPhar == 0 || nArg < 1 ){
		return PH7_OK;
	}
	iAlgo = ph7_value_to_int64(apArg[0]);
	if( iAlgo != PHAR_SIG_MD5 && iAlgo != PHAR_SIG_SHA1
	 && iAlgo != PHAR_SIG_SHA256 && iAlgo != PHAR_SIG_SHA512 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Unknown signature algorithm specified");
	}
	pPhar->iSigType = (int)iAlgo;
	PharCommit(pPhar);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/* ---- the statics ---- */
static int vm_builtin_Phar_apiVersion(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_string(pCtx,PHAR_API_STRING,-1);
	return PH7_OK;
}
static int vm_builtin_Phar_canCompress(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iComp = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
#ifdef PH7_ENABLE_ZLIB
	int bGz = 1;
#else
	int bGz = 0;
#endif
	/* php answers for the build it is: BZ2 is false here, exactly as it is in a
	 * php compiled without ext/bz2. */
	if( iComp == PHAR_C_BZ2 ){
		ph7_result_bool(pCtx,0);
	}else if( iComp == PHAR_C_GZ || iComp == 0 ){
		ph7_result_bool(pCtx,bGz);
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
static int vm_builtin_Phar_canWrite(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,!PharReadonly(pCtx->pVm));
	return PH7_OK;
}
static int vm_builtin_Phar_getSupportedCompression(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray = ph7_context_new_array(pCtx);
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pArray == 0 || pVal == 0 ){
		return PH7_OK;
	}
#ifdef PH7_ENABLE_ZLIB
	ph7_value_string(pVal,"GZ",-1);
	ph7_array_add_elem(pArray,0,pVal);
#endif
	ph7_result_value(pCtx,pArray);
	ph7_context_release_value(pCtx,pVal);
	return PH7_OK;
}
static int vm_builtin_Phar_getSupportedSignatures(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char * const azSig[] = { "MD5", "SHA-1", "SHA-256", "SHA-512" };
	ph7_value *pArray = ph7_context_new_array(pCtx);
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	sxu32 i;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pArray == 0 || pVal == 0 ){
		return PH7_OK;
	}
	for( i = 0 ; i < SX_ARRAYSIZE(azSig) ; ++i ){
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,azSig[i],-1);
		ph7_array_add_elem(pArray,0,pVal);
	}
	ph7_result_value(pCtx,pArray);
	ph7_context_release_value(pCtx,pVal);
	return PH7_OK;
}
static int vm_builtin_Phar_createDefaultStub(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyBlob sStub;
	SXUNUSED(nArg); SXUNUSED(apArg);
	SyBlobInit(&sStub,&pCtx->pVm->sAllocator);
	PharDefaultStub(&sStub);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sStub),(int)SyBlobLength(&sStub));
	SyBlobRelease(&sStub);
	return PH7_OK;
}
/*
 * `Phar::isValidPharFilename()`: whether php would treat this NAME as an
 * archive. Two rules, and they are not each other's mirror:
 *
 *   - an EXECUTABLE archive's name must carry `.phar` -- spelled in lower case,
 *     which is why `a.PHAR` is not one -- and its directory part, if it has
 *     one, must be absolute or dot-relative (`dir/b.phar` is refused where
 *     `./b.phar` and `/tmp/b.phar` are taken);
 *   - a DATA archive's name is anything with an extension that is NOT `.phar`.
 *     php does not screen WHICH extension: `a.txt` is a valid PharData name and
 *     the format is decided by what the file turns out to hold -- but a name
 *     with no extension at all (`phar`) is refused.
 */
static int vm_builtin_Phar_isValidPharFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z;
	int n = 0,i,iSlash = -1,bExec = 1,bPhar = 0,bDot = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	z = ph7_value_to_string(apArg[0],&n);
	if( nArg > 1 ){
		bExec = ph7_value_to_bool(apArg[1]);
	}
	while( n > 0 && (z[n-1] == '/' || z[n-1] == '\\') ){
		n--;   /* php tolerates a trailing separator */
	}
	for( i = 0 ; i < n ; ++i ){
		if( z[i] == '/' || z[i] == '\\' ){
			iSlash = i;
		}
	}
	/* `.phar` has to sit BEHIND something: a name that is only the extension
	 * names no archive. */
	for( i = iSlash + 2 ; i + 5 <= n ; ++i ){
		if( SyMemcmp(&z[i],".phar",5) == 0 ){
			bPhar = 1;
			break;
		}
	}
	for( i = iSlash + 2 ; i < n ; ++i ){
		if( z[i] == '.' ){
			bDot = 1;
			break;
		}
	}
	if( bExec ){
		/* A directory part php will not resolve: only an absolute path or one
		 * that starts at `.` names a file it can create. */
		if( iSlash > 0 && !(z[0] == '/' || z[0] == '\\' || z[0] == '.'
#ifdef __WINNT__
			|| (n > 2 && z[1] == ':')
#endif
			) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_bool(pCtx,bPhar);
		return PH7_OK;
	}
	/* A data archive's name is a bare one or a dot-relative one -- php answers
	 * false for an absolute path here, where it answers true for an executable
	 * archive's. */
	if( iSlash >= 0 && z[0] != '.' ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,bDot && !bPhar);
	return PH7_OK;
}
/*
 * `Phar::mapPhar()` -- what every executable archive's stub calls. It opens the
 * archive the RUNNING script is, registers its alias, and from then on
 * `phar://<alias>/entry` names an entry inside it.
 */
static int vm_builtin_Phar_mapPhar(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_phar *pPhar;
	SyString *pFile;
	const char *zErr;
	pFile = (SyString *)SySetPeek(&pVm->aFiles);
	if( pFile == 0 || pFile->nByte < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_VmThrowException(pCtx,"PharException",
			"Cannot call mapPhar() from outside a phar archive");
	}
	pPhar = PharOpenPath(pVm,pFile->zString,(int)pFile->nByte,0,&zErr);
	if( pPhar == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_VmThrowException(pCtx,"PharException",
			"internal corruption of phar \"%z\" (%s)",pFile,zErr ? zErr : "not a phar archive");
	}
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		int nAlias = 0;
		const char *zAlias = ph7_value_to_string(apArg[0],&nAlias);
		SyBlobReset(&pPhar->sAlias);
		SyBlobAppend(&pPhar->sAlias,zAlias,(sxu32)nAlias);
	}
	/* `Phar::running()` answers this from here on. */
	SyBlobReset(&pVm->sPharRunning);
	SyBlobAppend(&pVm->sPharRunning,SyBlobData(&pPhar->sPath),SyBlobLength(&pPhar->sPath));
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_Phar_loadPhar(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	const char *zPath,*zErr;
	int nPath = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	pPhar = PharOpenPath(pCtx->pVm,zPath,nPath,0,&zErr);
	if( pPhar == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_VmThrowException(pCtx,"PharException",
			"Unknown phar archive \"%.*s\": %s",nPath,zPath,zErr ? zErr : "unable to open phar");
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		int nAlias = 0;
		const char *zAlias = ph7_value_to_string(apArg[1],&nAlias);
		SyBlobReset(&pPhar->sAlias);
		SyBlobAppend(&pPhar->sAlias,zAlias,(sxu32)nAlias);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * `Phar::running()`: the archive the CURRENT script is running from, as a
 * `phar://` url with `$returnPhar` (the default) and as the file's own path
 * without it. Outside an archive both are the empty string -- never false,
 * which is what a `if (Phar::running())` guard reads.
 */
static int vm_builtin_Phar_running(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	int bUrl = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 1;
	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);
	SyBlob sPath;
	int bFound = 0;
	SyBlobInit(&sPath,&pVm->sAllocator);
	/*
	 * php answers from the file that is EXECUTING: any code included through a
	 * `phar://` url is "running from" that archive, whether or not a stub ever
	 * called mapPhar(). The mapped archive is the answer for the STUB itself,
	 * whose own path carries no scheme.
	 */
	if( pFile && pFile->nByte > sizeof("phar://")-1
	 && SyStrnicmp(pFile->zString,"phar://",sizeof("phar://")-1) == 0 ){
		const char *zEnt,*zErr;
		int nEnt = 0;
		phl_phar *pPhar = PharResolveUrl(pVm,
			&pFile->zString[sizeof("phar://")-1],
			(int)pFile->nByte - (int)sizeof("phar://") + 1,&zEnt,&nEnt,&zErr);
		if( pPhar ){
			SyBlobAppend(&sPath,SyBlobData(&pPhar->sPath),SyBlobLength(&pPhar->sPath));
			bFound = 1;
		}
	}
	if( !bFound && SyBlobLength(&pVm->sPharRunning) > 0 ){
		SyBlobAppend(&sPath,SyBlobData(&pVm->sPharRunning),SyBlobLength(&pVm->sPharRunning));
		bFound = 1;
	}
	if( !bFound ){
		SyBlobRelease(&sPath);
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( bUrl ){
		SyBlob sUrl;
		SyBlobInit(&sUrl,&pVm->sAllocator);
		SyBlobAppend(&sUrl,"phar://",sizeof("phar://")-1);
		SyBlobAppend(&sUrl,SyBlobData(&sPath),SyBlobLength(&sPath));
		ph7_result_string(pCtx,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));
		SyBlobRelease(&sUrl);
	}else{
		ph7_result_string(pCtx,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));
	}
	SyBlobRelease(&sPath);
	return PH7_OK;
}
static int vm_builtin_Phar_unlinkArchive(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar;
	const ph7_vfs *pVfs = pCtx->pVm->pEngine ? pCtx->pVm->pEngine->pVfs : 0;
	const char *zPath,*zErr;
	int nPath = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	pPhar = PharOpenPath(pCtx->pVm,zPath,nPath,0,&zErr);
	if( pPhar == 0 ){
		return PH7_VmThrowException(pCtx,"PharException",
			"Unknown phar archive \"%.*s\": %s \"%.*s\"",nPath,zPath,
			zErr ? zErr : "unable to open phar for reading",nPath,zPath);
	}
	if( pVfs && pVfs->xUnlink ){
		SyBlob sPath;
		SyBlobInit(&sPath,&pCtx->pVm->sAllocator);
		SyBlobAppend(&sPath,zPath,(sxu32)nPath);
		SyBlobNullAppend(&sPath);
		pVfs->xUnlink((const char *)SyBlobData(&sPath));
		SyBlobRelease(&sPath);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_Phar_mount(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn = "",*zOut = "";
	int nIn = 0,nOut = 0;
	if( nArg > 1 ){
		zIn = ph7_value_to_string(apArg[0],&nIn);
		zOut = ph7_value_to_string(apArg[1],&nOut);
	}
	/* php's own refusal when the mount cannot be made -- and a phar mounted
	 * over a real directory is a face this build does not carry, so it is
	 * always this one. */
	return PH7_VmThrowException(pCtx,"PharException",
		"Mounting of %.*s to %.*s failed",nIn,zIn,nOut,zOut);
}
/* ---- PharFileInfo ---- */
static int vm_builtin_PharFileInfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar;
	phl_phar_ent *pEnt;
	const char *zUrl,*zEnt,*zErr,*zPath;
	int nUrl = 0,nEnt = 0;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	zUrl = ph7_value_to_string(apArg[0],&nUrl);
	zPath = zUrl;
	if( nUrl > 7 && SyStrnicmp(zUrl,"phar://",7) == 0 ){
		zPath = &zUrl[7];
		nUrl -= 7;
	}else{
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"'%.*s' is not a phar archive URL (phar://...)",nUrl,zUrl);
	}
	pPhar = PharResolveUrl(pVm,zPath,nUrl,&zEnt,&nEnt,&zErr);
	if( pPhar == 0 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Cannot open phar file '%.*s': %s",nUrl,zPath,zErr ? zErr : "unable to open phar");
	}
	pEnt = PharFindEnt(pPhar,zEnt,nEnt);
	if( pEnt == 0 ){
		return PH7_VmThrowException(pCtx,"UnexpectedValueException",
			"Cannot access phar file entry '%.*s' in archive '%.*s'",
			nEnt,zEnt,(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));
	}
	PharSetSplPath(pVm,pThis,ph7_value_to_string(apArg[0],0),
		(int)SyStrlen(ph7_value_to_string(apArg[0],0)));
	PharSetStr(pThis,PHAR_SLOT_ENT,(const char *)SyBlobData(&pEnt->sName),
		(int)SyBlobLength(&pEnt->sName));
	PharAttach(pThis,pPhar);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,pEnt ? (sxi64)pEnt->nSize : 0);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_getCompressedSize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,pEnt ? (sxi64)pEnt->nCompSize : 0);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_getCRC32(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_int64(pCtx,pEnt ? (sxi64)pEnt->nCrc : 0);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_getPharFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	/* php's per-entry phar flags: zero for every archive this reader opens. */
	ph7_result_int64(pCtx,0);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_isCompressed(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);
	sxi64 iWant = nArg > 0 && !ph7_value_is_null(apArg[0]) ? ph7_value_to_int64(apArg[0]) : -1;
	if( pEnt == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iWant < 0 ){
		ph7_result_bool(pCtx,(pEnt->nFlags & PHAR_C_MASK) != 0);
	}else{
		ph7_result_bool(pCtx,(sxi64)(pEnt->nFlags & PHAR_C_MASK) == iWant);
	}
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_getContent(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pEnt == 0 || pPhar == 0 || PharEntLoad(pPhar,pEnt) != 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pEnt->sData),
		(int)SyBlobLength(&pEnt->sData));
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_getMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pEnt ){
		PharMetaResult(pCtx,&pEnt->sMeta);
	}
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_hasMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,pEnt != 0 && SyBlobLength(&pEnt->sMeta) > 0);
	return PH7_OK;
}
/* A directory the entry names IMPLY has no manifest row of its own, so both
 * questions are answered from the archive rather than from an entry. */
static int PharInfoIsDir(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_phar *pPhar = PharOfInstance(pThis);
	ph7_value *pName;
	phl_phar_ent *pEnt;
	if( pPhar == 0 ){
		return 0;
	}
	pName = PH7_NativeAttr(pThis,PHAR_SLOT_ENT);
	if( pName == 0 ){
		return 0;
	}
	pEnt = PharFindEnt(pPhar,(const char *)SyBlobData(&pName->sBlob),
		(int)SyBlobLength(&pName->sBlob));
	if( pEnt ){
		return pEnt->bDir;
	}
	return PharIsDir(pPhar,(const char *)SyBlobData(&pName->sBlob),
		(int)SyBlobLength(&pName->sBlob));
}
static int vm_builtin_PharFileInfo_isDir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,PharInfoIsDir(pCtx));
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_isFile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_result_bool(pCtx,pEnt != 0 && !pEnt->bDir);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_setCompression(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt;
	sxi64 iComp = nArg > 0 ? ph7_value_to_int64(apArg[0]) : PHAR_C_NONE;
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pEnt = PharFileInfoEnt(pCtx,&pPhar);
	if( pEnt == 0 || pPhar == 0 ){
		return PH7_OK;
	}
	if( iComp == PHAR_C_BZ2 ){
		return PH7_VmThrowException(pCtx,"BadMethodCallException",
			"Cannot compress with Bzip2 compression, bz2 extension is not enabled");
	}
	if( PharEntLoad(pPhar,pEnt) == 0 ){
		pEnt->nFlags = (pEnt->nFlags & ~(sxu32)PHAR_C_MASK) | (sxu32)iComp;
		PharCommit(pPhar);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_setMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt;
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pEnt = PharFileInfoEnt(pCtx,&pPhar);
	if( pEnt == 0 || nArg < 1 ){
		return PH7_OK;
	}
	SyBlobReset(&pEnt->sMeta);
	PH7_VmSerializeValue(pCtx,apArg[0],&pEnt->sMeta);
	PharCommit(pPhar);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_delMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pEnt = PharFileInfoEnt(pCtx,&pPhar);
	if( pEnt ){
		SyBlobReset(&pEnt->sMeta);
		PharCommit(pPhar);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_PharFileInfo_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_phar *pPhar = 0;
	phl_phar_ent *pEnt;
	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){
		return PH7_OK;
	}
	pEnt = PharFileInfoEnt(pCtx,&pPhar);
	if( pEnt == 0 || nArg < 1 ){
		return PH7_OK;
	}
	pEnt->nFlags = (pEnt->nFlags & ~(sxu32)0777)
		| ((sxu32)ph7_value_to_int64(apArg[0]) & 0777);
	PharCommit(pPhar);
	ph7_result_null(pCtx);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* Registration                                                        */
/* ------------------------------------------------------------------ */
/*
 * php's Phar and PharData both EXTEND RecursiveDirectoryIterator: the archive
 * object is a directory iterator over its own entries, and everything
 * SplFileInfo answers about a path it answers about the archive FILE. That is
 * why the two classes are declared with a parent rather than standing alone --
 * `$phar->getBasename()` and `(string) $phar` come from it for free.
 *
 * The iteration methods, the ArrayAccess four and the SplFileInfo accessors
 * that must speak about an ENTRY instead are overridden below; everything else
 * the parent answers is already php's answer.
 */
static void PharInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	/* The archive itself belongs to the per-VM registry, which frees it: two
	 * objects may name one archive (php caches them the same way), so an
	 * instance going away must not take it with it. */
	SXUNUSED(pVm);
	SXUNUSED(pThis);
}
PH7_PRIVATE sxi32 PH7_VmInstallPhar(ph7_vm *pVm)
{
	static const PH7_NativePropDef aProp[] = {
		{ PHAR_SLOT_RES, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ PHAR_SLOT_CUR, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },
		{ PHAR_SLOT_DIR, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }
	};
	static const PH7_NativePropDef aInfoProp[] = {
		{ PHAR_SLOT_RES,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ PHAR_SLOT_ENT,  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
		{ PHAR_SLOT_PHAR, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }
	};
	/* php's own order for the class, which is what Reflection lists. */
	static const PH7_NativeMethodDef aMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC,
		  "string $filename, int $flags = FilesystemIterator::SKIP_DOTS|FilesystemIterator::UNIX_PATHS, ?string $alias = null",
		  0, vm_builtin_Phar_construct },
		{ "__destruct", PH7_MOD_PUBLIC, "", 0, vm_builtin_Phar_void },
		{ "addEmptyDir", PH7_MOD_PUBLIC, "string $directory", "@void",
		  vm_builtin_Phar_addEmptyDir },
		{ "addFile", PH7_MOD_PUBLIC, "string $filename, ?string $localName = null", "@void",
		  vm_builtin_Phar_addFile },
		{ "addFromString", PH7_MOD_PUBLIC, "string $localName, string $contents", "@void",
		  vm_builtin_Phar_addFromString },
		{ "compressFiles", PH7_MOD_PUBLIC, "int $compression", "@void",
		  vm_builtin_Phar_compressFiles },
		{ "decompressFiles", PH7_MOD_PUBLIC, "", "@true", vm_builtin_Phar_decompressFiles },
		{ "copy", PH7_MOD_PUBLIC, "string $from, string $to", "@true", vm_builtin_Phar_copy },
		{ "count", PH7_MOD_PUBLIC, "int $mode = COUNT_NORMAL", "@int", vm_builtin_Phar_count },
		{ "delete", PH7_MOD_PUBLIC, "string $localName", "@true", vm_builtin_Phar_delete },
		{ "delMetadata", PH7_MOD_PUBLIC, "", "@true", vm_builtin_Phar_delMetadata },
		{ "extractTo", PH7_MOD_PUBLIC,
		  "string $directory, array|string|null $files = null, bool $overwrite = false",
		  "@bool", vm_builtin_Phar_extractTo },
		{ "getAlias", PH7_MOD_PUBLIC, "", "@?string", vm_builtin_Phar_getAlias },
		{ "getPath", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getPath },
		{ "getMetadata", PH7_MOD_PUBLIC, "array $unserializeOptions = []", "@mixed",
		  vm_builtin_Phar_getMetadata },
		{ "getModified", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_false },
		{ "getSignature", PH7_MOD_PUBLIC, "", "@array|false", vm_builtin_Phar_getSignature },
		{ "getStub", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getStub },
		{ "getVersion", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getVersion },
		{ "hasMetadata", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_hasMetadata },
		{ "isBuffering", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_false },
		{ "isCompressed", PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_Phar_isCompressed },
		{ "isFileFormat", PH7_MOD_PUBLIC, "int $format", "@bool", vm_builtin_Phar_isFileFormat },
		{ "isWritable", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_isWritable },
		{ "offsetExists", PH7_MOD_PUBLIC, "$localName", "@bool", vm_builtin_Phar_offsetExists },
		{ "offsetGet", PH7_MOD_PUBLIC, "$localName", "@SplFileInfo", vm_builtin_Phar_offsetGet },
		{ "offsetSet", PH7_MOD_PUBLIC, "$localName, $value", "@void", vm_builtin_Phar_offsetSet },
		{ "offsetUnset", PH7_MOD_PUBLIC, "$localName", "@void", vm_builtin_Phar_offsetUnset },
		{ "setAlias", PH7_MOD_PUBLIC, "string $alias", "@true", vm_builtin_Phar_setAlias },
		{ "setDefaultStub", PH7_MOD_PUBLIC, "?string $index = null, ?string $webIndex = null",
		  "@true", vm_builtin_Phar_true },
		{ "setMetadata", PH7_MOD_PUBLIC, "mixed $metadata", "@void", vm_builtin_Phar_setMetadata },
		{ "setSignatureAlgorithm", PH7_MOD_PUBLIC, "int $algo, ?string $privateKey = null",
		  "@void", vm_builtin_Phar_setSignatureAlgorithm },
		{ "setStub", PH7_MOD_PUBLIC, "$stub, int $length = -1", "@true", vm_builtin_Phar_setStub },
		{ "startBuffering", PH7_MOD_PUBLIC, "", "@void", vm_builtin_Phar_startBuffering },
		{ "stopBuffering", PH7_MOD_PUBLIC, "", "@void", vm_builtin_Phar_startBuffering },
		/* The iterator half, overriding the parent's directory walk. */
		{ "rewind", PH7_MOD_PUBLIC, "", "@void", vm_builtin_Phar_rewind },
		{ "valid", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_valid },
		{ "key", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Phar_key },
		{ "current", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Phar_current },
		{ "next", PH7_MOD_PUBLIC, "", "@void", vm_builtin_Phar_next },
		{ "getFilename", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getFilename },
		{ "getPathname", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getPathname },
		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_hasChildren },
		{ "getChildren", PH7_MOD_PUBLIC, "", "@RecursiveDirectoryIterator",
		  vm_builtin_Phar_getChildren },
		/* The statics. */
		{ "apiVersion", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL, "", "string",
		  vm_builtin_Phar_apiVersion },
		{ "canCompress", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL, "int $compression = 0",
		  "bool", vm_builtin_Phar_canCompress },
		{ "canWrite", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL, "", "bool",
		  vm_builtin_Phar_canWrite },
		{ "createDefaultStub", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL,
		  "?string $index = null, ?string $webIndex = null", "string",
		  vm_builtin_Phar_createDefaultStub },
		{ "getSupportedCompression", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL, "", "array",
		  vm_builtin_Phar_getSupportedCompression },
		{ "getSupportedSignatures", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL, "", "array",
		  vm_builtin_Phar_getSupportedSignatures },
		{ "interceptFileFuncs", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL, "", "void",
		  vm_builtin_Phar_void },
		{ "isValidPharFilename", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL,
		  "string $filename, bool $executable = true", "bool",
		  vm_builtin_Phar_isValidPharFilename },
		{ "loadPhar", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL,
		  "string $filename, ?string $alias = null", "bool", vm_builtin_Phar_loadPhar },
		{ "mapPhar", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL,
		  "?string $alias = null, int $offset = 0", "bool", vm_builtin_Phar_mapPhar },
		{ "running", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL, "bool $returnPhar = true",
		  "string", vm_builtin_Phar_running },
		{ "mount", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL,
		  "string $pharPath, string $externalPath", "void", vm_builtin_Phar_mount },
		{ "unlinkArchive", PH7_MOD_PUBLIC|PH7_MOD_STATIC|PH7_MOD_FINAL, "string $filename",
		  "true", vm_builtin_Phar_unlinkArchive }
	};
	static const PH7_NativeMethodDef aInfoMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $filename", 0,
		  vm_builtin_PharFileInfo_construct },
		{ "__destruct", PH7_MOD_PUBLIC, "", 0, vm_builtin_Phar_void },
		{ "chmod", PH7_MOD_PUBLIC, "int $perms", "@void", vm_builtin_PharFileInfo_chmod },
		{ "compress", PH7_MOD_PUBLIC, "int $compression", "@bool",
		  vm_builtin_PharFileInfo_setCompression },
		{ "decompress", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_setCompression },
		{ "delMetadata", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_delMetadata },
		{ "getCompressedSize", PH7_MOD_PUBLIC, "", "@int",
		  vm_builtin_PharFileInfo_getCompressedSize },
		{ "getContent", PH7_MOD_PUBLIC, "", "@string", vm_builtin_PharFileInfo_getContent },
		{ "getCRC32", PH7_MOD_PUBLIC, "", "@int", vm_builtin_PharFileInfo_getCRC32 },
		{ "getMetadata", PH7_MOD_PUBLIC, "array $unserializeOptions = []", "@mixed",
		  vm_builtin_PharFileInfo_getMetadata },
		{ "getPharFlags", PH7_MOD_PUBLIC, "", "@int", vm_builtin_PharFileInfo_getPharFlags },
		{ "getSize", PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_PharFileInfo_getSize },
		{ "hasMetadata", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_hasMetadata },
		{ "isCompressed", PH7_MOD_PUBLIC, "?int $compression = null", "@bool",
		  vm_builtin_PharFileInfo_isCompressed },
		{ "isCRCChecked", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_true },
		{ "isDir", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_isDir },
		{ "isFile", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_isFile },
		{ "setMetadata", PH7_MOD_PUBLIC, "mixed $metadata", "@void",
		  vm_builtin_PharFileInfo_setMetadata }
	};
	/* php's Phar:: constants: the three formats, the three compressions, the
	 * two stub kinds and the seven signature algorithms. */
	static const PH7_NativeConstDef aConst[] = {
		{ "BZ2",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_C_BZ2, 0, 0.0 },
		{ "GZ",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_C_GZ, 0, 0.0 },
		{ "NONE",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_C_NONE, 0, 0.0 },
		{ "PHAR",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_FORMAT_PHAR, 0, 0.0 },
		{ "TAR",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_FORMAT_TAR, 0, 0.0 },
		{ "ZIP",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_FORMAT_ZIP, 0, 0.0 },
		{ "COMPRESSED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_C_MASK, 0, 0.0 },
		{ "PHP",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },
		{ "PHPS",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },
		{ "MD5",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_MD5, 0, 0.0 },
		{ "OPENSSL",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_OPENSSL, 0, 0.0 },
		{ "OPENSSL_SHA256", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 17, 0, 0.0 },
		{ "OPENSSL_SHA512", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 18, 0, 0.0 },
		{ "SHA1",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_SHA1, 0, 0.0 },
		{ "SHA256",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_SHA256, 0, 0.0 },
		{ "SHA512",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_SHA512, 0, 0.0 }
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "PharException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "Phar", "RecursiveDirectoryIterator", "Countable,ArrayAccess", 0,
		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),
		  aProp, SX_ARRAYSIZE(aProp), PharInstanceRelease, 0, 0 },
		{ "PharData", "RecursiveDirectoryIterator", "Countable,ArrayAccess", 0,
		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),
		  aProp, SX_ARRAYSIZE(aProp), PharInstanceRelease, 0, 0 },
		{ "PharFileInfo", "SplFileInfo", 0, 0,
		  aInfoMethod, SX_ARRAYSIZE(aInfoMethod), 0, 0,
		  aInfoProp, SX_ARRAYSIZE(aInfoProp), PharInstanceRelease, 0, 0 }
	};
	pVm->pPhars = 0;
	SyBlobInit(&pVm->sPharRunning,&pVm->sAllocator);
	SyBlobInit(&pVm->sPharErr,&pVm->sAllocator);
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}
/* ------------------------------------------------------------------ */
/* The path operations over an archive                                 */
/* ------------------------------------------------------------------ */
/*
 * `unlink('phar://x.phar/f')`, `mkdir`, `rmdir`, `rename` and the chmod family
 * over a phar url. php implements the first four for a WRITABLE archive and
 * refuses all of them with one sentence when `phar.readonly` is on -- which is
 * php's default, and so the sentence a program normally sees.
 *
 * Answers 1 when it handled the call (the result is set), 0 to let the caller
 * carry on -- which never happens for a phar url.
 */
/* The archive path, NUL-terminated, for a message that names the FILE. */
static const char * PharPathZ(phl_phar *pPhar)
{
	SyBlobNullAppend(&pPhar->sPath);
	return (const char *)SyBlobData(&pPhar->sPath);
}
/* php's sentence for a write-mode url whose archive is not a writable phar
 * name: a `.tar` or a `.zip` can have an entry unlinked, but a directory is
 * created through a door that first insists on being able to CREATE the phar. */
static void PharNotAPharName(ph7_context *pCtx,phl_phar *pPhar)
{
	PH7_VmThrowWarningFmt(pCtx->pVm,
		"%s(): Cannot create phar '%s', file extension (or combination) not "
		"recognised or the directory does not exist",
		ph7_function_name(pCtx),PharPathZ(pPhar));
}
/* Does anything live UNDER this directory name? An archive's directories are
 * mostly implied by the entry names, so "empty" means no entry has it as a
 * prefix -- an explicit entry of its own does not count. */
static int PharDirHasChild(phl_phar *pPhar,const char *zDir,int nDir)
{
	phl_phar_ent *pEnt;
	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){
		const char *z = (const char *)SyBlobData(&pEnt->sName);
		int n = (int)SyBlobLength(&pEnt->sName);
		if( n > nDir + 1 && z[nDir] == '/' && SyMemcmp(z,zDir,(sxu32)nDir) == 0 ){
			return 1;
		}
	}
	return 0;
}
/* Unhook one entry from the manifest chain and free it. */
static void PharEntUnlink(phl_phar *pPhar,phl_phar_ent *pEnt)
{
	phl_phar_ent *p,*pPrev = 0;
	for( p = pPhar->pFirst ; p ; pPrev = p, p = p->pNext ){
		if( p != pEnt ){
			continue;
		}
		if( pPrev ){
			pPrev->pNext = p->pNext;
		}else{
			pPhar->pFirst = p->pNext;
		}
		if( pPhar->pLast == p ){
			pPhar->pLast = pPrev;
		}
		pPhar->nEnt--;
		PharEntFree(pPhar->pVm,p);
		return;
	}
}
/*
 * rename() over a phar url. It is the one path operation php tells the DESTINATION
 * about, and the only one that can move a whole directory: every entry under the
 * old name is re-prefixed, since a directory is mostly just the names beneath it.
 * Both ends have to be inside the SAME archive -- php refuses the pair rather
 * than copying between two of them.
 */
static int PharRenameOp(ph7_context *pCtx,phl_phar *pPhar,const char *zPath,
	const char *zDest,const char *zName,int nName,phl_phar_ent *pEnt,int bDir)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_phar *pTo;
	phl_phar_ent *p;
	const char *zToEnt,*zErr,*zTo;
	SyBlob sTo;
	int nToEnt = 0,nTo;
	if( zDest == 0 || SyStrnicmp(zDest,"phar://",sizeof("phar://")-1) != 0 ){
		/* php's own sentence, and its rule: a rename never crosses wrappers. */
		PH7_VmThrowWarningFmt(pVm,"%s(): Cannot rename a file across wrapper types",
			ph7_function_name(pCtx));
		ph7_result_bool(pCtx,0);
		return 1;
	}
	if( pPhar->bData ){
		/* A rename is a WRITE-mode url, and php only ever opens one on a name it
		 * could have created the phar under -- which a `.tar` or a `.zip` is not,
		 * whatever `phar.readonly` says. */
		PH7_VmThrowWarningFmt(pVm,
			"%s(): phar error: cannot rename \"%s\" to \"%s\": invalid or non-writable url \"%s\"",
			ph7_function_name(pCtx),zPath,zDest,zPath);
		ph7_result_bool(pCtx,0);
		return 1;
	}
	pTo = PharResolveUrl(pVm,&zDest[sizeof("phar://")-1],
		(int)SyStrlen(zDest) - (int)sizeof("phar://") + 1,&zToEnt,&nToEnt,&zErr);
	if( pTo && pTo->bData ){
		/* php asks the same question of the DESTINATION, and names that end. */
		PH7_VmThrowWarningFmt(pVm,
			"%s(): phar error: cannot rename \"%s\" to \"%s\": invalid or non-writable url \"%s\"",
			ph7_function_name(pCtx),zPath,zDest,zDest);
		ph7_result_bool(pCtx,0);
		return 1;
	}
	if( pTo != pPhar ){
		PH7_VmThrowWarningFmt(pVm,
			"%s(): phar error: cannot rename \"%s\" to \"%s\", not within the same phar archive",
			ph7_function_name(pCtx),zPath,zDest);
		ph7_result_bool(pCtx,0);
		return 1;
	}
	if( pEnt == 0 && !bDir ){
		PH7_VmThrowWarningFmt(pVm,
			"%s(): phar error: cannot rename \"%s\" to \"%s\" from extracted phar archive, "
			"source does not exist",ph7_function_name(pCtx),zPath,zDest);
		ph7_result_bool(pCtx,0);
		return 1;
	}
	SyBlobInit(&sTo,&pVm->sAllocator);
	PharNormalize(zToEnt,nToEnt,&sTo);
	zTo = (const char *)SyBlobData(&sTo);
	nTo = (int)SyBlobLength(&sTo);
	if( nTo < 1 ){
		SyBlobRelease(&sTo);
		ph7_result_bool(pCtx,0);
		return 1;
	}
	/* A name the destination already holds is replaced, as it is on a filesystem. */
	p = PharFindEnt(pPhar,zTo,nTo);
	if( p && p != pEnt ){
		PharEntUnlink(pPhar,p);
	}
	for( p = pPhar->pFirst ; p ; p = p->pNext ){
		const char *z = (const char *)SyBlobData(&p->sName);
		int n = (int)SyBlobLength(&p->sName);
		SyBlob sNew;
		if( n < nName || SyMemcmp(z,zName,(sxu32)nName) != 0
		 || (n > nName && z[nName] != '/') ){
			continue;
		}
		SyBlobInit(&sNew,&pVm->sAllocator);
		SyBlobAppend(&sNew,zTo,(sxu32)nTo);
		if( n > nName ){
			SyBlobAppend(&sNew,&z[nName],(sxu32)(n - nName));
		}
		/* The bytes have to be in memory before the manifest is rewritten: what
		 * is on disk is about to stop being where this entry's offset points. */
		PharEntLoad(pPhar,p);
		SyBlobReset(&p->sName);
		SyBlobAppend(&p->sName,SyBlobData(&sNew),SyBlobLength(&sNew));
		SyBlobRelease(&sNew);
	}
	SyBlobRelease(&sTo);
	PharCommit(pPhar);
	ph7_result_bool(pCtx,1);
	return 1;
}
/*
 * unlink(), rename(), mkdir() and rmdir() over a `phar://` url. php gives its
 * wrapper all four, and each has its own refusal -- which is what the table
 * below is: the doors do not share one sentence, they do not share the
 * `phar.readonly` rule (a data archive's unlink is allowed and its mkdir is
 * not), and only rename is told about a second path.
 *
 * chmod(), chown() and chgrp() are NOT here: php's phar wrapper implements no
 * stream_metadata, so they take the engine's own "Cannot call chmod() for a
 * non-standard stream" -- the sentence vfs.c already writes for every wrapper
 * that has none.
 */
PH7_PRIVATE int PH7_PharPathOp(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_phar *pPhar;
	phl_phar_ent *pEnt;
	const char *zEnt,*zErr,*zName;
	SyBlob sEnt;
	int nEnt = 0,nName,bDir,bFile,rc = 1;
	pPhar = PharResolveUrl(pVm,&zPath[sizeof("phar://")-1],
		(int)SyStrlen(zPath) - (int)sizeof("phar://") + 1,&zEnt,&nEnt,&zErr);
	if( pPhar == 0 ){
		ph7_result_bool(pCtx,0);
		return 1;
	}
	/* The directive closes an executable archive's write doors; a PharData's
	 * are open, and it is the NAME rather than the directive that stops the two
	 * directory ones below. */
	if( PharReadonly(pVm) && !pPhar->bData ){
		switch( eOp ){
		case PHAR_PATHOP_MKDIR:
			PH7_VmThrowWarningFmt(pVm,
				"%s(): phar error: cannot create directory \"%s\", write operations disabled",
				ph7_function_name(pCtx),zPath);
			break;
		case PHAR_PATHOP_RMDIR:
			PH7_VmThrowWarningFmt(pVm,
				"%s(): phar error: cannot rmdir directory \"%s\", write operations disabled",
				ph7_function_name(pCtx),zPath);
			break;
		case PHAR_PATHOP_RENAME:
			PH7_VmThrowWarningFmt(pVm,
				"%s(): phar error: cannot rename \"%s\" to \"%s\": invalid or non-writable url \"%s\"",
				ph7_function_name(pCtx),zPath,zDest ? zDest : "",zPath);
			break;
		default:
			PH7_VmThrowWarningFmt(pVm,"%s(): %s",ph7_function_name(pCtx),zPharReadonlyStream);
			break;
		}
		ph7_result_bool(pCtx,0);
		return 1;
	}
	SyBlobInit(&sEnt,&pVm->sAllocator);
	PharNormalize(zEnt,nEnt,&sEnt);
	SyBlobNullAppend(&sEnt);
	zName = (const char *)SyBlobData(&sEnt);
	nName = (int)SyBlobLength(&sEnt);
	pEnt = nName > 0 ? PharFindEnt(pPhar,zName,nName) : 0;
	bFile = pEnt != 0 && !pEnt->bDir;
	bDir = (pEnt != 0 && pEnt->bDir) || PharDirHasChild(pPhar,zName,nName);
	switch( eOp ){
	case PHAR_PATHOP_UNLINK:
		/* A directory is not a file, and php words its absence the same way. */
		if( bFile ){
			PharEntUnlink(pPhar,pEnt);
			PharCommit(pPhar);
			ph7_result_bool(pCtx,1);
		}else{
			PH7_VmThrowWarningFmt(pVm,"%s(): unlink of \"%s\" failed, file does not exist",
				ph7_function_name(pCtx),zPath);
			ph7_result_bool(pCtx,0);
		}
		break;
	case PHAR_PATHOP_MKDIR:
		if( pPhar->bData ){
			PharNotAPharName(pCtx,pPhar);
			ph7_result_bool(pCtx,0);
		}else if( bFile ){
			PH7_VmThrowWarningFmt(pVm,
				"%s(): phar error: cannot create directory \"%s\" in phar \"%s\", "
				"phar error: path \"%s\" exists and is a not a directory",
				ph7_function_name(pCtx),zName,PharPathZ(pPhar),zName);
			ph7_result_bool(pCtx,0);
		}else if( bDir || nName < 1 ){
			PH7_VmThrowWarningFmt(pVm,
				"%s(): phar error: cannot create directory \"%s\" in phar \"%s\", "
				"directory already exists",
				ph7_function_name(pCtx),zName,PharPathZ(pPhar));
			ph7_result_bool(pCtx,0);
		}else if( PharAddBytes(pCtx,pPhar,zName,nName,"",0,1) == 0 ){
			PharCommit(pPhar);
			ph7_result_bool(pCtx,1);
		}else{
			ph7_result_bool(pCtx,0);
		}
		break;
	case PHAR_PATHOP_RMDIR:
		if( pPhar->bData ){
			PharNotAPharName(pCtx,pPhar);
			ph7_result_bool(pCtx,0);
		}else if( bFile ){
			PH7_VmThrowWarningFmt(pVm,
				"%s(): phar error: cannot remove directory \"%s\" in phar \"%s\", "
				"phar error: path \"%s\" exists and is a not a directory",
				ph7_function_name(pCtx),zName,PharPathZ(pPhar),zName);
			ph7_result_bool(pCtx,0);
		}else if( !bDir ){
			PH7_VmThrowWarningFmt(pVm,
				"%s(): phar error: cannot remove directory \"%s\" in phar \"%s\", "
				"directory does not exist",
				ph7_function_name(pCtx),zName,PharPathZ(pPhar));
			ph7_result_bool(pCtx,0);
		}else if( PharDirHasChild(pPhar,zName,nName) ){
			PH7_VmThrowWarningFmt(pVm,"%s(): phar error: Directory not empty",
				ph7_function_name(pCtx));
			ph7_result_bool(pCtx,0);
		}else{
			PharEntUnlink(pPhar,pEnt);
			PharCommit(pPhar);
			ph7_result_bool(pCtx,1);
		}
		break;
	default:
		rc = PharRenameOp(pCtx,pPhar,zPath,zDest,zName,nName,pEnt,bDir);
		break;
	}
	SyBlobRelease(&sEnt);
	return rc;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */

#ifdef PH7_DISABLE_BUILTIN_FUNC
/* Tiny build: no phar. The two VM lifecycle hooks are called unconditionally,
 * and with no archive ever opened they have nothing to sweep. */
PH7_PRIVATE void PH7_PharVmReset(ph7_vm *pVm){ (void)pVm; }
PH7_PRIVATE void PH7_PharVmRelease(ph7_vm *pVm){ (void)pVm; }
#endif /* PH7_DISABLE_BUILTIN_FUNC */
