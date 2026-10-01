/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#if !defined(PH7_DISABLE_BUILTIN_FUNC) && defined(PH7_ENABLE_ZLIB)
#include <zlib.h>
#include <time.h>
#include <errno.h>
#ifdef PH7_ENABLE_OPENSSL
/* WinZip AES is a BUILD question exactly as php's is: a libzip with no crypto
 * backend answers `isEncryptionMethodSupported(EM_AES_256)` false and refuses to
 * write one, and so does this engine without ext/openssl. The traditional
 * PKWARE cipher needs nothing but the CRC table zlib already carries. */
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#endif
/*
 * Section:
 *    php's zip extension: the ZipArchive class, its deprecated procedural half
 *    and the read-only `zip://` wrapper.
 * Status:
 *    Stable.
 *
 * WHY THIS IS A DERIVATION AND NOT A BINDING. php's ext/zip is a binding of
 * libzip, and every other library-backed extension here (zlib, curl, openssl,
 * libxml) is bound rather than re-derived because the LIBRARY's answers are the
 * contract -- which ciphers exist, what a malformed document says. ext/zip is
 * the case where that argument does not hold: what a script can observe is the
 * ZIP FORMAT, which is a published byte layout, plus a fixed table of thirty-two
 * error strings that has not changed in libzip's lifetime. Everything a program
 * asks -- the entry list, the sizes, the CRCs, the comments, the mtimes -- comes
 * out of the file, not out of the library. So this is derived, which is also why
 * it is here on Windows and on any build with zlib, where php's own needs a
 * bundled libzip.
 *
 * It rides `PH7_ENABLE_ZLIB` because php's ext/zip requires zlib too: a deflated
 * member is the format's normal case and an extension that could not read one
 * would not be the extension.
 *
 * WHAT AN ARCHIVE IS HERE. One `phl_zip` per open handle, holding the whole FILE
 * in memory and an entry table over it -- the model ext/phar already uses, for
 * the same reason: it makes a reader that works over any stream the engine can
 * open, and it costs the archive's size in memory.
 *
 * THE ENTRY TABLE IS libzip's, not the file's. This is the part that is easy to
 * get wrong. libzip does not rebuild the archive on every change; it keeps the
 * ORIGINAL entries with their indexes and records what a script has asked for on
 * top of them, and only `close()` writes. That model is php-visible all over:
 *
 *   - a deleted entry KEEPS its index. `$z->deleteName('b')` leaves `numFiles`
 *     where it was, `statName('b')` false and `getNameIndex(1)` false, until
 *     `close()` compacts them.
 *   - `FL_UNCHANGED` reads the ORIGINAL of a name or a comment, beside the
 *     changed one every other read answers.
 *   - `unchangeIndex()` / `unchangeAll()` / `unchangeArchive()` put a change
 *     back, which is only expressible if the original was still there.
 *   - an entry ADDED but not yet written reads back as size == comp_size,
 *     `crc` 0 and `comp_method` 0 whatever it will be compressed with, and
 *     `getFromName()` on it is FALSE. Nothing has been compressed yet.
 *
 * WHAT IS NOT HERE, and why each is honest rather than missing:
 *
 *   - BZIP2, LZMA, XZ and the other seven compression methods php NAMES as
 *     constants. `isCompressionMethodSupported()` answers what this build can
 *     do, exactly as it does on a libzip built without those libraries, and an
 *     entry compressed with one is still LISTED and still copied through a
 *     rewrite untouched -- only reading its bytes fails.
 *   - ZIP64 archives are READ (the locator, the end record and the per-entry
 *     extra field) and never written: an archive this engine builds lives in
 *     memory first, so the four-gigabyte boundary is not reachable from here.
 *   - WinZip AES needs ext/openssl. Without it this build answers
 *     `isEncryptionMethodSupported(EM_AES_256)` false, exactly as a libzip
 *     with no crypto backend does; the traditional cipher needs nothing and is
 *     always there.
 *
 * ONE php ANSWER IS DELIBERATELY NOT REPRODUCED. libzip takes the traditional
 * cipher's check byte from the DOS stamp an entry had BEFORE `setMtime*()`
 * moved it and then writes the moved one, so an archive php wrote that way
 * cannot be read back by php. This writer uses the stamp the header will
 * carry. Reproducing the defect would mean writing an archive nothing can open.
 */
/* php's ZipArchive::open() flags. */
#define ZIP_OPEN_CREATE     1
#define ZIP_OPEN_EXCL       2
#define ZIP_OPEN_CHECKCONS  4
#define ZIP_OPEN_OVERWRITE  8
#define ZIP_OPEN_RDONLY     16
/* ...and its per-call FL_ flags. */
#define ZIP_FL_NOCASE       1
#define ZIP_FL_NODIR        2
#define ZIP_FL_COMPRESSED   4
#define ZIP_FL_UNCHANGED    8
#define ZIP_FL_OVERWRITE    8192
#define ZIP_FL_ENC_RAW      64
#define ZIP_FL_ENC_UTF_8    2048
/* The compression methods, as the format numbers them. */
#define ZIP_CM_DEFAULT      (-1)
#define ZIP_CM_STORE        0
#define ZIP_CM_DEFLATE      8
#define ZIP_CM_BZIP2        12
/* The encryption methods php names. */
#define ZIP_EM_NONE         0
#define ZIP_EM_TRAD_PKWARE  1
#define ZIP_EM_AES_128      257
#define ZIP_EM_AES_192      258
#define ZIP_EM_AES_256      259
#define ZIP_EM_UNKNOWN      65535
/* libzip's error codes. They are the values of ZipArchive::ER_*, the numbers
 * `open()` answers on failure and what `$z->status` holds. */
#define ZIP_ER_OK              0
#define ZIP_ER_MULTIDISK       1
#define ZIP_ER_RENAME          2
#define ZIP_ER_CLOSE           3
#define ZIP_ER_SEEK            4
#define ZIP_ER_READ            5
#define ZIP_ER_WRITE           6
#define ZIP_ER_CRC             7
#define ZIP_ER_ZIPCLOSED       8
#define ZIP_ER_NOENT           9
#define ZIP_ER_EXISTS          10
#define ZIP_ER_OPEN            11
#define ZIP_ER_TMPOPEN         12
#define ZIP_ER_ZLIB            13
#define ZIP_ER_MEMORY          14
#define ZIP_ER_CHANGED         15
#define ZIP_ER_COMPNOTSUPP     16
#define ZIP_ER_EOF             17
#define ZIP_ER_INVAL           18
#define ZIP_ER_NOZIP           19
#define ZIP_ER_INTERNAL        20
#define ZIP_ER_INCONS          21
#define ZIP_ER_REMOVE          22
#define ZIP_ER_DELETED         23
#define ZIP_ER_ENCRNOTSUPP     24
#define ZIP_ER_RDONLY          25
#define ZIP_ER_NOPASSWD        26
#define ZIP_ER_WRONGPASSWD     27
#define ZIP_ER_OPNOTSUPP       28
#define ZIP_ER_INUSE           29
#define ZIP_ER_TELL            30
#define ZIP_ER_COMPRESSED_DATA 31
#define ZIP_ER_CANCELLED       32
/* Not one of php's codes: the reader's way of saying the file was EMPTY, which
 * php opens with a deprecation rather than refusing. It never reaches a
 * script -- the two openers turn it into that notice and an empty archive. */
#define ZIP_ER_EMPTY_FILE      (-1)
/* The method BYTE a WinZip AES member carries, with the real one in its extra
 * field, and the extra field's own id. */
#define ZIP_CM_WINZIP_AES   99
#define ZIP_EXTRA_AES       0x9901
/* The glob(3) flags `addGlob()` takes -- php's own GLOB_AVAILABLE_FLAGS, since
 * that argument is routed into glob() rather than read here. */
#define ZIP_GLOB_FLAGS (PH7_GLOB_ERR|PH7_GLOB_MARK|PH7_GLOB_NOCHECK|PH7_GLOB_NOSORT \
	|PH7_GLOB_BRACE|PH7_GLOB_NOESCAPE|PH7_GLOB_ONLYDIR)
/* php's ZipArchive::AFL_RDONLY, the one archive flag it exposes. */
#define ZIP_AFL_RDONLY      2
/* The operating systems the format's `version made by` high byte names. Only
 * the default matters to the writer; the rest are constants a script may hand
 * back through setExternalAttributes(). */
#define ZIP_OPSYS_UNIX      3
/* The version this writer stamps: libzip's own `3 << 8 | 63` -- unix, and zip
 * specification 6.3 -- and the 2.0 a reader needs for deflate. Both are
 * platform-independent on purpose, exactly as libzip's are: php writes the same
 * two numbers from a Windows build. */
#define ZIP_VERSION_MADE    ((ZIP_OPSYS_UNIX << 8) | 63)
#define ZIP_VERSION_NEEDED  20
/*
 * `ZipArchive::LIBZIP_VERSION`. php answers the version of the libzip it was
 * built against, and programs read it to decide which of the class's verbs
 * exist. Nothing is linked here, so what this names is the API LEVEL this
 * derivation reproduces -- the surface php's ext/zip presents over libzip 1.7,
 * which is every method above.
 */
#define PHL_ZIP_VERSION "1.7.3"
/* The external attributes libzip stamps on an entry nobody set them on. */
#define ZIP_ATTR_FILE   ((sxu32)0100666 << 16)
#define ZIP_ATTR_DIR    ((sxu32)0040777 << 16)

typedef struct phl_zip phl_zip;
typedef struct phl_zip_ent phl_zip_ent;
/*
 * One member of an archive, in libzip's two-faced shape: what the FILE says and
 * what the script has asked for since. A field with an `Orig` twin is one
 * `FL_UNCHANGED` can still read and `unchangeIndex()` can put back.
 */
struct phl_zip_ent {
	SyBlob sName;        /* the name a read answers */
	SyBlob sOrigName;    /* ...and the one the central directory carried */
	SyBlob sComment;
	SyBlob sOrigComment;
	SyBlob sData;        /* the UNCOMPRESSED bytes, once loaded or supplied */
	SyBlob sSrcPath;     /* addFile(): the file to read at close, instead of sData */
	sxi64 iSrcStart;     /* ...and the slice of it php was asked for */
	sxi64 iSrcLen;       /* -1: to the end */
	sxu32 nSize;         /* uncompressed length */
	sxu32 nCompSize;     /* stored length, as the file has it */
	sxu32 nCrc;
	sxu32 nFlag;         /* the general-purpose flag word the file carries */
	sxu32 nDosTime;      /* ...and its raw MS-DOS time word, which the traditional
	                      * cipher's check byte is taken from when the flag says
	                      * the sizes are in a trailing descriptor */
	int iMethod;         /* the compression method the file uses */
	int iSetMethod;      /* what setCompression() asked for, or ZIP_CM_DEFAULT */
	int iEncMethod;      /* the encryption method the file uses */
	int iSetEncrypt;     /* what setEncryption() asked for, or -1 */
	int iRawMethod;      /* the method BYTE the file carries: 99 for WinZip AES,
	                      * where iMethod is the real one out of its extra field */
	SyBlob sPassword;    /* setEncryption()'s own password, when it named one */
	sxu8 bHasPassword;
	sxi64 iTime;         /* modification time */
	sxi64 iOrigTime;
	sxu32 nAttr;         /* external attributes */
	sxu32 nOrigAttr;
	int iOpsys;          /* ...and the OS whose attributes they are */
	int iOrigOpsys;
	sxi64 iOffset;       /* where the STORED bytes begin in the archive file */
	sxu8 bDir;           /* the name ended in `/`: an explicit directory entry */
	sxu8 bLoaded;        /* sData holds the uncompressed bytes */
	SyBlob sLoadedPw;    /* ...and, for an ENCRYPTED entry, the password that
	                      * opened them. The cache is only good for that one:
	                      * libzip decrypts on every zip_fopen, so a read with a
	                      * DIFFERENT password must refuse rather than answer
	                      * plaintext somebody else's key produced. */
	sxu8 bNew;           /* added this session: there is no original behind it */
	sxu8 bDeleted;
	sxu8 bNameHidden;    /* deleted once: the name lookup misses even after an
	                      * unchangeIndex(), because libzip drops the name from
	                      * its hash on delete and only unchangeAll() puts the
	                      * whole hash back. php-visible: `statIndex()` answers
	                      * and `statName()` does not. */
	sxu8 bDataChanged;   /* sData/sSrcPath replaces what the file holds */
	sxu8 bNameChanged;
	sxu8 bCommentChanged;
	sxu8 bTimeChanged;
	sxu8 bAttrChanged;
};
/*
 * One open archive. It outlives the object that opened it whenever a stream
 * getStream() handed out is still being read, which is php's own lifetime:
 * `close()` leaves such a stream working and destroying the OBJECT does not.
 */
struct phl_zip {
	ph7_vm *pVm;
	SyBlob sPath;          /* the expanded filename, as `$z->filename` reads it */
	SyBlob sFile;          /* the whole archive, when one was there to read */
	SyBlob sComment;       /* the archive comment a read answers */
	SyBlob sOrigComment;
	SyBlob sPassword;      /* what setPassword() left */
	phl_zip_ent **apEnt;   /* the entry table, in index order */
	sxu32 nEnt;
	sxu32 nAlloc;
	int iStatus;           /* `$z->status` */
	int iStatusSys;        /* `$z->statusSys` */
	int iLastId;           /* `$z->lastId` */
	int iArchiveFlags;     /* setArchiveFlag()'s AFL_ bits */
	sxu8 bOpen;            /* an object still holds it */
	sxu8 bRdonly;          /* opened ZipArchive::RDONLY */
	sxu8 bCommentChanged;
	sxu8 bCreated;         /* the file did not exist: nothing to copy through */
	sxu8 bBuilding;        /* a write is in flight: what a progress or cancel callback
	                        * asks of the same archive must not start a second one */
	sxu8 bDead;            /* the OBJECT that owned it went away while it was open:
	                        * every stream still reading it stops answering, which
	                        * is php's `Containing zip archive was closed` */
	sxu32 nRef;            /* the object, plus every live getStream() handle */
	ph7_value *pProgress;  /* registerProgressCallback() */
	ph7_value *pCancel;    /* registerCancelCallback() */
	double rProgressRate;
	phl_zip *pNext;        /* the per-VM registry */
};
/* ------------------------------------------------------------------ */
/* The format's scalars                                                */
/* ------------------------------------------------------------------ */
static sxu32 ZipGet16(const unsigned char *z)
{
	return (sxu32)z[0] | ((sxu32)z[1] << 8);
}
static sxu32 ZipGet32(const unsigned char *z)
{
	return (sxu32)z[0] | ((sxu32)z[1] << 8) | ((sxu32)z[2] << 16) | ((sxu32)z[3] << 24);
}
static sxu64 ZipGet64(const unsigned char *z)
{
	return (sxu64)ZipGet32(z) | ((sxu64)ZipGet32(&z[4]) << 32);
}
static void ZipPut16(SyBlob *pOut,sxu32 n)
{
	unsigned char z[2];
	z[0] = (unsigned char)(n & 0xFF);
	z[1] = (unsigned char)((n >> 8) & 0xFF);
	SyBlobAppend(pOut,z,sizeof(z));
}
static void ZipPut32(SyBlob *pOut,sxu32 n)
{
	unsigned char z[4];
	z[0] = (unsigned char)(n & 0xFF);
	z[1] = (unsigned char)((n >> 8) & 0xFF);
	z[2] = (unsigned char)((n >> 16) & 0xFF);
	z[3] = (unsigned char)((n >> 24) & 0xFF);
	SyBlobAppend(pOut,z,sizeof(z));
}
/*
 * The MS-DOS timestamp a zip entry carries, both ways.
 *
 * It is a LOCAL time in two-second steps -- the format has no zone -- so both
 * halves go through the C library's `localtime`/`mktime` rather than through
 * this engine's own date machinery. That is libzip's choice and it has to be
 * reproduced: a program that sets an mtime and reads it back must get the same
 * number on any box, which it does exactly when the two conversions agree with
 * each other, and `date.timezone` must not move a stored stamp.
 *
 * A year before 1980 has no field to go in. libzip does not clamp the whole
 * stamp for one: it writes a ZERO year and keeps the month, day and time, so
 * 1970-01-01 comes back as 1980-12-31 (the localtime of the epoch, with 1980
 * for its year). Reproduced rather than corrected -- it is what a php-written
 * archive holds.
 */
static void ZipUnixToDos(sxi64 iTime,sxu32 *pnTime,sxu32 *pnDate)
{
	time_t t = (time_t)iTime;
	struct tm *pTm = localtime(&t);
	int iYear;
	if( pTm == 0 ){
		*pnTime = 0;
		*pnDate = (1 << 5) | 1;
		return;
	}
	iYear = pTm->tm_year + 1900 - 1980;
	if( iYear < 0 ){
		iYear = 0;
	}
	*pnDate = (sxu32)((((sxu32)iYear & 0x7F) << 9)
		| ((sxu32)(pTm->tm_mon + 1) << 5) | (sxu32)pTm->tm_mday) & 0xFFFF;
	*pnTime = (sxu32)(((sxu32)pTm->tm_hour << 11) | ((sxu32)pTm->tm_min << 5)
		| ((sxu32)pTm->tm_sec / 2)) & 0xFFFF;
}
static sxi64 ZipDosToUnix(sxu32 nTime,sxu32 nDate)
{
	struct tm sTm;
	time_t t;
	SyZero(&sTm,sizeof(sTm));
	sTm.tm_year = (int)((nDate >> 9) & 0x7F) + 80;
	sTm.tm_mon  = (int)((nDate >> 5) & 0x0F) - 1;
	sTm.tm_mday = (int)(nDate & 0x1F);
	sTm.tm_hour = (int)((nTime >> 11) & 0x1F);
	sTm.tm_min  = (int)((nTime >> 5) & 0x3F);
	sTm.tm_sec  = (int)((nTime & 0x1F) * 2);
	sTm.tm_isdst = -1;
	t = mktime(&sTm);
	return (sxi64)t;
}
/* Does this name need the format's UTF-8 flag? libzip sets bit 11 exactly when
 * the name is not plain ASCII, and nothing else decides it. */
static int ZipNameIsUtf8(const char *z,sxu32 n)
{
	sxu32 i;
	for( i = 0 ; i < n ; ++i ){
		if( (unsigned char)z[i] > 0x7F ){
			return 1;
		}
	}
	return 0;
}
/* ------------------------------------------------------------------ */
/* Entries                                                             */
/* ------------------------------------------------------------------ */
static void ZipEntFree(ph7_vm *pVm,phl_zip_ent *pEnt)
{
	SyBlobRelease(&pEnt->sName);
	SyBlobRelease(&pEnt->sOrigName);
	SyBlobRelease(&pEnt->sComment);
	SyBlobRelease(&pEnt->sOrigComment);
	SyBlobRelease(&pEnt->sData);
	SyBlobRelease(&pEnt->sSrcPath);
	SyBlobRelease(&pEnt->sPassword);
	SyBlobRelease(&pEnt->sLoadedPw);
	SyMemBackendFree(&pVm->sAllocator,pEnt);
}
/* Room for one more index in the table. */
static int ZipGrow(phl_zip *pZip)
{
	sxu32 nWant;
	phl_zip_ent **apNew;
	if( pZip->nEnt < pZip->nAlloc ){
		return 0;
	}
	nWant = pZip->nAlloc < 8 ? 8 : pZip->nAlloc * 2;
	apNew = (phl_zip_ent **)SyMemBackendAlloc(&pZip->pVm->sAllocator,
		nWant * (sxu32)sizeof(phl_zip_ent *));
	if( apNew == 0 ){
		return -1;
	}
	if( pZip->nEnt > 0 ){
		SyMemcpy(pZip->apEnt,apNew,pZip->nEnt * (sxu32)sizeof(phl_zip_ent *));
	}
	if( pZip->apEnt ){
		SyMemBackendFree(&pZip->pVm->sAllocator,pZip->apEnt);
	}
	pZip->apEnt = apNew;
	pZip->nAlloc = nWant;
	return 0;
}
/* A blank entry, appended to the table. Its index is its position, and it keeps
 * that position for the archive's whole life -- a delete blanks it in place. */
static phl_zip_ent * ZipEntNew(phl_zip *pZip,const char *zName,int nName)
{
	phl_zip_ent *pEnt;
	if( ZipGrow(pZip) != 0 ){
		return 0;
	}
	pEnt = (phl_zip_ent *)SyMemBackendAlloc(&pZip->pVm->sAllocator,sizeof(phl_zip_ent));
	if( pEnt == 0 ){
		return 0;
	}
	SyZero(pEnt,sizeof(*pEnt));
	SyBlobInit(&pEnt->sName,&pZip->pVm->sAllocator);
	SyBlobInit(&pEnt->sOrigName,&pZip->pVm->sAllocator);
	SyBlobInit(&pEnt->sComment,&pZip->pVm->sAllocator);
	SyBlobInit(&pEnt->sOrigComment,&pZip->pVm->sAllocator);
	SyBlobInit(&pEnt->sData,&pZip->pVm->sAllocator);
	SyBlobInit(&pEnt->sSrcPath,&pZip->pVm->sAllocator);
	SyBlobInit(&pEnt->sPassword,&pZip->pVm->sAllocator);
	SyBlobInit(&pEnt->sLoadedPw,&pZip->pVm->sAllocator);
	pEnt->iSrcLen = -1;
	pEnt->iSetMethod = ZIP_CM_DEFAULT;
	pEnt->iSetEncrypt = -1;
	pEnt->iOffset = -1;
	if( nName > 0 ){
		SyBlobAppend(&pEnt->sName,zName,(sxu32)nName);
	}
	pZip->apEnt[pZip->nEnt++] = pEnt;
	return pEnt;
}
/* The name a read answers, honouring FL_UNCHANGED. */
static const char * ZipEntName(phl_zip_ent *pEnt,int iFlags,sxu32 *pnName)
{
	SyBlob *pB = (iFlags & ZIP_FL_UNCHANGED) ? &pEnt->sOrigName : &pEnt->sName;
	if( (iFlags & ZIP_FL_UNCHANGED) && pEnt->bNew ){
		*pnName = 0;
		return 0;
	}
	*pnName = SyBlobLength(pB);
	return (const char *)SyBlobData(pB);
}
/*
 * The entry a NAME resolves to, with php's two lookup flags: FL_NOCASE compares
 * case-insensitively, FL_NODIR ignores the directory part of the stored name.
 * A deleted entry is not found -- its index is still there and nothing answers
 * to it, which is what makes `deleteName()` visible before `close()`.
 */
static phl_zip_ent * ZipFind(phl_zip *pZip,const char *zName,sxu32 nName,int iFlags,sxu32 *pnIdx)
{
	sxu32 i;
	for( i = 0 ; i < pZip->nEnt ; ++i ){
		phl_zip_ent *pEnt = pZip->apEnt[i];
		const char *z;
		sxu32 n,nOff = 0;
		if( pEnt->bDeleted || pEnt->bNameHidden ){
			continue;
		}
		z = ZipEntName(pEnt,iFlags,&n);
		if( z == 0 ){
			continue;
		}
		if( iFlags & ZIP_FL_NODIR ){
			sxu32 k;
			for( k = 0 ; k < n ; ++k ){
				if( z[k] == '/' ){
					nOff = k + 1;
				}
			}
		}
		if( n - nOff != nName ){
			continue;
		}
		if( iFlags & ZIP_FL_NOCASE ){
			if( SyStrnicmp(&z[nOff],zName,nName) != 0 ){
				continue;
			}
		}else if( nName > 0 && SyMemcmp(&z[nOff],zName,nName) != 0 ){
			continue;
		}
		if( pnIdx ){
			*pnIdx = i;
		}
		return pEnt;
	}
	return 0;
}
/* The entry at an index, or 0 when the index names nothing a read can see. */
static phl_zip_ent * ZipAt(phl_zip *pZip,sxi64 iIdx)
{
	phl_zip_ent *pEnt;
	if( iIdx < 0 || (sxu64)iIdx >= (sxu64)pZip->nEnt ){
		return 0;
	}
	pEnt = pZip->apEnt[(sxu32)iIdx];
	return pEnt->bDeleted ? 0 : pEnt;
}
/* ------------------------------------------------------------------ */
/* Reading an archive                                                  */
/* ------------------------------------------------------------------ */
/*
 * The ZIP64 extra field of one central-directory record. The three big numbers
 * are present only when their 32-bit slot is saturated, and IN THAT ORDER, so
 * the field cannot be parsed without knowing which ones were.
 */
static void ZipReadZip64Extra(const unsigned char *zExtra,sxu32 nExtra,
	sxu64 *pnUsz,sxu64 *pnCsz,sxu64 *pnOff)
{
	sxu32 q = 0;
	while( q + 4 <= nExtra ){
		sxu32 nId = ZipGet16(&zExtra[q]);
		sxu32 nLen = ZipGet16(&zExtra[q+2]);
		if( q + 4 + nLen > nExtra ){
			return;
		}
		if( nId == 0x0001 ){
			sxu32 p = q + 4;
			if( *pnUsz == 0xFFFFFFFFu && p + 8 <= q + 4 + nLen ){
				*pnUsz = ZipGet64(&zExtra[p]);
				p += 8;
			}
			if( *pnCsz == 0xFFFFFFFFu && p + 8 <= q + 4 + nLen ){
				*pnCsz = ZipGet64(&zExtra[p]);
				p += 8;
			}
			if( *pnOff == 0xFFFFFFFFu && p + 8 <= q + 4 + nLen ){
				*pnOff = ZipGet64(&zExtra[p]);
			}
			return;
		}
		q += 4 + nLen;
	}
}
/*
 * The WinZip AES extra field (0x9901) of one record: a vendor version, the two
 * letters `AE`, the key STRENGTH (1/2/3 for 128/192/256) and the compression
 * method the encrypted bytes are really under.
 */
static void ZipReadAesExtra(const unsigned char *zExtra,sxu32 nExtra,int *piStrength,int *piMethod)
{
	sxu32 q = 0;
	while( q + 4 <= nExtra ){
		sxu32 nId = ZipGet16(&zExtra[q]);
		sxu32 nLen = ZipGet16(&zExtra[q+2]);
		if( q + 4 + nLen > nExtra ){
			return;
		}
		if( nId == ZIP_EXTRA_AES && nLen >= 7 ){
			*piStrength = (int)zExtra[q+4+4];
			*piMethod = (int)ZipGet16(&zExtra[q+4+5]);
			return;
		}
		q += 4 + nLen;
	}
}
/*
 * Read the whole archive out of `pZip->sFile`, from its CENTRAL DIRECTORY --
 * the only authoritative part of the format, which is why an archive with a
 * program bolted on the front (a phar, a self-extractor) still reads.
 *
 * Answers a ZIP_ER_ code: NOZIP for anything that is not one, INCONS for a
 * directory that contradicts itself.
 */
static int ZipParse(phl_zip *pZip)
{
	const unsigned char *zFile = (const unsigned char *)SyBlobData(&pZip->sFile);
	sxu32 nFile = SyBlobLength(&pZip->sFile);
	sxu32 nEocd,i;
	sxu64 nEntries,nCdOff,nCdSize;
	sxu64 q;
	int bFound = 0;
	if( nFile < 1 ){
		/* An EMPTY file is not a zip and php opens it anyway, with a
		 * deprecation the caller raises: `Using empty file as ZipArchive is
		 * deprecated`. It reads as an archive with no entries. */
		return ZIP_ER_EMPTY_FILE;
	}
	if( nFile < 22 ){
		return ZIP_ER_NOZIP;
	}
	/* The end record is last, but a trailing COMMENT of up to 64K may follow
	 * it, so it is found by scanning backwards for the signature. */
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
		return ZIP_ER_NOZIP;
	}
	if( ZipGet16(&zFile[nEocd+4]) != 0 || ZipGet16(&zFile[nEocd+6]) != 0 ){
		/* A member of a multi-part set: php refuses the whole archive. */
		return ZIP_ER_MULTIDISK;
	}
	nEntries = ZipGet16(&zFile[nEocd+10]);
	nCdSize = ZipGet32(&zFile[nEocd+12]);
	nCdOff = ZipGet32(&zFile[nEocd+16]);
	{
		sxu32 nCommentLen = ZipGet16(&zFile[nEocd+20]);
		if( nCommentLen > 0 && nCommentLen <= nFile - nEocd - 22 ){
			SyBlobAppend(&pZip->sOrigComment,&zFile[nEocd+22],nCommentLen);
			SyBlobAppend(&pZip->sComment,&zFile[nEocd+22],nCommentLen);
		}
	}
	/* ZIP64: the locator sits immediately before the end record and points at a
	 * second end record carrying the counts that did not fit in the first. */
	if( nEocd >= 20 && SyMemcmp(&zFile[nEocd-20],"PK\006\007",4) == 0 ){
		sxu64 iEocd64 = ZipGet64(&zFile[nEocd-20+8]);
		if( iEocd64 + 56 <= (sxu64)nFile
		 && SyMemcmp(&zFile[(sxu32)iEocd64],"PK\006\006",4) == 0 ){
			nEntries = ZipGet64(&zFile[(sxu32)iEocd64+32]);
			nCdSize = ZipGet64(&zFile[(sxu32)iEocd64+40]);
			nCdOff = ZipGet64(&zFile[(sxu32)iEocd64+48]);
		}
	}
	/* The directory has to FIT, in front of the end record that describes it.
	 * A size or an offset that does not is php's `Zip archive inconsistent`;
	 * bytes that are not a directory where one should be are the different
	 * complaint below. */
	if( nCdOff > (sxu64)nFile || nCdSize > (sxu64)nFile
	 || nCdOff + nCdSize > (sxu64)nEocd ){
		return ZIP_ER_INCONS;
	}
	q = nCdOff;
	for( i = 0 ; i < (sxu32)nEntries ; ++i ){
		sxu32 nNameLen,nExtraLen,nCommentLen,nFlag,nMethod,nCrc,nMade;
		sxu64 nCsz,nUsz,nLocal;
		phl_zip_ent *pEnt;
		if( q + 46 > nCdOff + nCdSize || SyMemcmp(&zFile[(sxu32)q],"PK\001\002",4) != 0 ){
			/* Not a directory record where the end record said one would be.
			 * That is what an archive with a PREFIX in front of it looks like
			 * -- a self-extracting stub leaves every recorded offset short --
			 * and what php answers to one: `Not a zip archive`. */
			return ZIP_ER_NOZIP;
		}
		nMade = ZipGet16(&zFile[(sxu32)q+4]);
		nFlag = ZipGet16(&zFile[(sxu32)q+8]);
		nMethod = ZipGet16(&zFile[(sxu32)q+10]);
		nCrc = ZipGet32(&zFile[(sxu32)q+16]);
		nCsz = ZipGet32(&zFile[(sxu32)q+20]);
		nUsz = ZipGet32(&zFile[(sxu32)q+24]);
		nNameLen = ZipGet16(&zFile[(sxu32)q+28]);
		nExtraLen = ZipGet16(&zFile[(sxu32)q+30]);
		nCommentLen = ZipGet16(&zFile[(sxu32)q+32]);
		nLocal = ZipGet32(&zFile[(sxu32)q+42]);
		if( q + 46 + nNameLen + nExtraLen + nCommentLen > nCdOff + nCdSize ){
			return ZIP_ER_INCONS;
		}
		if( nExtraLen > 0 ){
			ZipReadZip64Extra(&zFile[(sxu32)q+46+nNameLen],nExtraLen,&nUsz,&nCsz,&nLocal);
		}
		pEnt = ZipEntNew(pZip,(const char *)&zFile[(sxu32)q+46],(int)nNameLen);
		if( pEnt == 0 ){
			return ZIP_ER_MEMORY;
		}
		SyBlobAppend(&pEnt->sOrigName,(const char *)&zFile[(sxu32)q+46],nNameLen);
		if( nCommentLen > 0 ){
			const char *zC = (const char *)&zFile[(sxu32)q+46+nNameLen+nExtraLen];
			SyBlobAppend(&pEnt->sComment,zC,nCommentLen);
			SyBlobAppend(&pEnt->sOrigComment,zC,nCommentLen);
		}
		pEnt->nSize = (sxu32)nUsz;
		pEnt->nCompSize = (sxu32)nCsz;
		pEnt->nCrc = nCrc;
		pEnt->iMethod = (int)nMethod;
		pEnt->iRawMethod = (int)nMethod;
		pEnt->nFlag = nFlag;
		pEnt->nDosTime = ZipGet16(&zFile[(sxu32)q+12]);
		pEnt->iTime = pEnt->iOrigTime =
			ZipDosToUnix(pEnt->nDosTime,ZipGet16(&zFile[(sxu32)q+14]));
		pEnt->nAttr = pEnt->nOrigAttr = ZipGet32(&zFile[(sxu32)q+38]);
		pEnt->iOpsys = pEnt->iOrigOpsys = (int)((nMade >> 8) & 0xFF);
		pEnt->iOffset = (sxi64)nLocal;
		/* The encryption bit says only THAT the entry is encrypted; which of
		 * php's four methods it is lives in the AES extra field, or is the
		 * traditional one when there is none. */
		if( nFlag & 1 ){
			pEnt->iEncMethod = ZIP_EM_TRAD_PKWARE;
			if( nMethod == ZIP_CM_WINZIP_AES ){
				/* The method byte is a placeholder: the REAL compression method
				 * and the key length live in the 0x9901 extra field, and php
				 * answers both from there -- `comp_method` is 0 or 8 for an AES
				 * member, never 99. */
				int iStrength = 0,iReal = -1;
				ZipReadAesExtra(&zFile[(sxu32)q+46+nNameLen],nExtraLen,&iStrength,&iReal);
				if( iStrength >= 1 && iStrength <= 3 ){
					pEnt->iEncMethod = ZIP_EM_AES_128 + (iStrength - 1);
					pEnt->iMethod = iReal;
				}else{
					pEnt->iEncMethod = ZIP_EM_UNKNOWN;
				}
			}
		}
		if( SyBlobLength(&pEnt->sName) > 0 ){
			char *zN = (char *)SyBlobData(&pEnt->sName);
			if( zN[SyBlobLength(&pEnt->sName)-1] == '/' ){
				pEnt->bDir = 1;
			}
		}
		q += 46 + nNameLen + nExtraLen + nCommentLen;
	}
	if( q != nCdOff + nCdSize ){
		/* The entry COUNT and the directory SIZE disagree -- one record too
		 * many or one too few. php's answer is the same as a missing record's. */
		return ZIP_ER_NOZIP;
	}
	return ZIP_ER_OK;
}

/* ------------------------------------------------------------------ */
/* The two ciphers a zip member can be under                           */
/* ------------------------------------------------------------------ */
/*
 * PKWARE's TRADITIONAL cipher, the one every zip tool has spoken since 1990.
 * Three 32-bit keys stirred by the password and then by each plaintext byte, a
 * keystream byte read off the third, and a twelve-byte random header in front
 * of the data whose LAST byte is a check value the reader can test a password
 * against without decrypting anything else.
 *
 * The stirring uses the raw CRC-32 table update -- no pre- or post-complement --
 * which zlib's own `crc32()` can be talked into: it computes the complemented
 * form, so complementing both ends gives the raw one back and no second table
 * has to live here.
 */
typedef struct phl_zip_keys phl_zip_keys;
struct phl_zip_keys { sxu32 k[3]; };
static sxu32 ZipCrcRaw(sxu32 iCrc,unsigned char b)
{
	return (sxu32)~crc32((uLong)(~iCrc) & 0xFFFFFFFFu,(const Bytef *)&b,1);
}
static void ZipTradUpdate(phl_zip_keys *pK,unsigned char b)
{
	pK->k[0] = ZipCrcRaw(pK->k[0],b);
	pK->k[1] = (pK->k[1] + (pK->k[0] & 0xFF)) & 0xFFFFFFFFu;
	pK->k[1] = ((pK->k[1] * 134775813u) + 1u) & 0xFFFFFFFFu;
	pK->k[2] = ZipCrcRaw(pK->k[2],(unsigned char)((pK->k[1] >> 24) & 0xFF));
}
static void ZipTradInit(phl_zip_keys *pK,const char *zPw,sxu32 nPw)
{
	sxu32 i;
	pK->k[0] = 0x12345678u;
	pK->k[1] = 0x23456789u;
	pK->k[2] = 0x34567890u;
	for( i = 0 ; i < nPw ; ++i ){
		ZipTradUpdate(pK,(unsigned char)zPw[i]);
	}
}
static unsigned char ZipTradStream(phl_zip_keys *pK)
{
	sxu32 t = (pK->k[2] | 2u) & 0xFFFFu;
	return (unsigned char)(((t * (t ^ 1u)) >> 8) & 0xFF);
}
static unsigned char ZipTradDecrypt(phl_zip_keys *pK,unsigned char c)
{
	unsigned char p = (unsigned char)(c ^ ZipTradStream(pK));
	ZipTradUpdate(pK,p);
	return p;
}
static unsigned char ZipTradEncrypt(phl_zip_keys *pK,unsigned char p)
{
	unsigned char c = (unsigned char)(p ^ ZipTradStream(pK));
	ZipTradUpdate(pK,p);
	return c;
}
#ifdef PH7_ENABLE_OPENSSL
/*
 * WinZip AES, the other one php can write. A salt of half the key length, a
 * PBKDF2-HMAC-SHA1 over a THOUSAND rounds producing an encryption key, an
 * authentication key and two PASSWORD VERIFICATION bytes in that order, the
 * data under AES in counter mode, and a ten-byte HMAC-SHA1 of the CIPHERTEXT
 * behind it.
 *
 * The counter is the part a general AES-CTR cannot be used for: WinZip counts
 * in LITTLE-endian from 1 in the low four bytes of an otherwise zero block,
 * where every CTR implementation counts big-endian over the whole block. So the
 * keystream is made a block at a time out of raw ECB.
 */
#define ZIP_AES_MAXKEY 32
static int ZipAesKeyLen(int iMethod)
{
	switch( iMethod ){
	case ZIP_EM_AES_128: return 16;
	case ZIP_EM_AES_192: return 24;
	case ZIP_EM_AES_256: return 32;
	default: break;
	}
	return 0;
}
static const EVP_CIPHER * ZipAesEcb(int nKey)
{
	return nKey == 16 ? EVP_aes_128_ecb() : (nKey == 24 ? EVP_aes_192_ecb() : EVP_aes_256_ecb());
}
/* The derived material: [key][auth key][2 verification bytes]. */
static int ZipAesDerive(const char *zPw,sxu32 nPw,const unsigned char *zSalt,int nSalt,
	int nKey,unsigned char *zOut)
{
	return PKCS5_PBKDF2_HMAC_SHA1(zPw,(int)nPw,zSalt,nSalt,1000,nKey * 2 + 2,zOut) == 1 ? 0 : -1;
}
/* One pass of the counter-mode keystream over a buffer, in place. Encryption
 * and decryption are the same operation. */
static int ZipAesCtr(const unsigned char *zKey,int nKey,unsigned char *zData,sxu32 nData)
{
	EVP_CIPHER_CTX *pCtx = EVP_CIPHER_CTX_new();
	sxu32 nDone = 0;
	sxu32 iBlock = 1;
	int rc = 0;
	if( pCtx == 0 ){
		return -1;
	}
	if( EVP_EncryptInit_ex(pCtx,ZipAesEcb(nKey),0,zKey,0) != 1 ){
		EVP_CIPHER_CTX_free(pCtx);
		return -1;
	}
	EVP_CIPHER_CTX_set_padding(pCtx,0);
	while( nDone < nData ){
		unsigned char zCtr[16],zKs[32];
		int nKs = 0;
		sxu32 n = nData - nDone;
		sxu32 i;
		SyZero(zCtr,sizeof(zCtr));
		zCtr[0] = (unsigned char)(iBlock & 0xFF);
		zCtr[1] = (unsigned char)((iBlock >> 8) & 0xFF);
		zCtr[2] = (unsigned char)((iBlock >> 16) & 0xFF);
		zCtr[3] = (unsigned char)((iBlock >> 24) & 0xFF);
		if( EVP_EncryptUpdate(pCtx,zKs,&nKs,zCtr,(int)sizeof(zCtr)) != 1 || nKs < 16 ){
			rc = -1;
			break;
		}
		if( n > 16 ){
			n = 16;
		}
		for( i = 0 ; i < n ; ++i ){
			zData[nDone + i] = (unsigned char)(zData[nDone + i] ^ zKs[i]);
		}
		nDone += n;
		iBlock++;
	}
	EVP_CIPHER_CTX_free(pCtx);
	return rc;
}
/* The ten bytes WinZip puts behind the ciphertext. */
static int ZipAesMac(const unsigned char *zKey,int nKey,const unsigned char *zData,sxu32 nData,
	unsigned char *zOut)
{
	unsigned char zMd[EVP_MAX_MD_SIZE];
	unsigned int nMd = 0;
	if( HMAC(EVP_sha1(),zKey,nKey,zData,nData,zMd,&nMd) == 0 || nMd < 10 ){
		return -1;
	}
	SyMemcpy(zMd,zOut,10);
	return 0;
}
#endif /* PH7_ENABLE_OPENSSL */
/* Is this a method this BUILD can read and write? php answers from its libzip's
 * crypto backend, and this answers from whether ext/openssl is in. */
static int ZipEncSupported(int iMethod)
{
	if( iMethod == ZIP_EM_NONE || iMethod == ZIP_EM_TRAD_PKWARE ){
		return 1;
	}
#ifdef PH7_ENABLE_OPENSSL
	if( iMethod == ZIP_EM_AES_128 || iMethod == ZIP_EM_AES_192
	 || iMethod == ZIP_EM_AES_256 ){
		return 1;
	}
#endif
	return 0;
}
/*
 * Where one entry's STORED bytes begin. The local header repeats the name and
 * carries an extra field of its own, so the payload offset is only knowable
 * from it -- the central directory records where the HEADER is, not the data.
 */
static int ZipEntPayload(phl_zip *pZip,phl_zip_ent *pEnt,const unsigned char **pz,sxu32 *pn)
{
	const unsigned char *zFile = (const unsigned char *)SyBlobData(&pZip->sFile);
	sxu32 nFile = SyBlobLength(&pZip->sFile);
	sxu64 iOff;
	if( pEnt->iOffset < 0 || (sxu64)pEnt->iOffset + 30 > (sxu64)nFile ){
		return ZIP_ER_NOZIP;
	}
	/* The local header's SIGNATURE is deliberately not checked. libzip reads the
	 * two lengths out of it without one unless CHECKCONS asked, so an archive
	 * whose local signature was damaged still reads every entry under php -- and
	 * the bounds below are what actually keeps this safe. */
	iOff = (sxu64)pEnt->iOffset + 30 + ZipGet16(&zFile[pEnt->iOffset+26])
		+ ZipGet16(&zFile[pEnt->iOffset+28]);
	if( iOff > (sxu64)nFile ){
		return ZIP_ER_EOF;
	}
	/* The recorded length is CLAMPED to what the file holds rather than
	 * trusted: a directory that claims more bytes than there are is read as
	 * far as it goes, which is what php answers for one. */
	*pn = pEnt->nCompSize;
	if( iOff + *pn > (sxu64)nFile ){
		*pn = (sxu32)((sxu64)nFile - iOff);
	}
	*pz = &zFile[(sxu32)iOff];
	return ZIP_ER_OK;
}

/*
 * The plaintext of one ENCRYPTED entry, or php's reason it cannot be had.
 *
 * Two reasons and they are php's own words: `No password provided` when nothing
 * has been set on the archive, and `Wrong password provided` when the check the
 * format carries -- the traditional cipher's twelfth header byte, or WinZip
 * AES's two verification bytes -- does not agree with the key the password
 * makes. Neither costs a decryption of the whole member, which is what those
 * two bytes are FOR.
 */
static int ZipDecrypt(phl_zip *pZip,phl_zip_ent *pEnt,const unsigned char *zData,
	sxu32 nData,SyBlob *pOut)
{
	const char *zPw = (const char *)SyBlobData(&pZip->sPassword);
	sxu32 nPw = SyBlobLength(&pZip->sPassword);
	if( nPw < 1 ){
		return ZIP_ER_NOPASSWD;
	}
	if( pEnt->iEncMethod == ZIP_EM_TRAD_PKWARE ){
		phl_zip_keys sKeys;
		unsigned char zHdr[12];
		unsigned char bWant;
		sxu32 i;
		if( nData < 12 ){
			return ZIP_ER_EOF;
		}
		ZipTradInit(&sKeys,zPw,nPw);
		for( i = 0 ; i < 12 ; ++i ){
			zHdr[i] = ZipTradDecrypt(&sKeys,zData[i]);
		}
		/* The check byte is the CRC's high byte -- unless the entry says its
		 * sizes are in a trailing descriptor (flag bit 3), in which case the
		 * writer did not know the CRC yet and used the DOS TIME's instead.
		 * libzip writes the second form, so both have to be accepted. */
		bWant = (pEnt->nFlag & 8)
			? (unsigned char)((pEnt->nDosTime >> 8) & 0xFF)
			: (unsigned char)((pEnt->nCrc >> 24) & 0xFF);
		if( zHdr[11] != bWant ){
			return ZIP_ER_WRONGPASSWD;
		}
		for( i = 12 ; i < nData ; ){
			unsigned char zBuf[4096];
			sxu32 n = 0;
			while( i < nData && n < (sxu32)sizeof(zBuf) ){
				zBuf[n++] = ZipTradDecrypt(&sKeys,zData[i++]);
			}
			SyBlobAppend(pOut,zBuf,n);
		}
		return ZIP_ER_OK;
	}
#ifdef PH7_ENABLE_OPENSSL
	{
		int nKey = ZipAesKeyLen(pEnt->iEncMethod);
		int nSalt = nKey / 2;
		unsigned char zKey[ZIP_AES_MAXKEY * 2 + 2];
		sxu32 nCipher,nAt;
		if( nKey == 0 ){
			return ZIP_ER_ENCRNOTSUPP;
		}
		if( nData < (sxu32)nSalt + 2 + 10 ){
			return ZIP_ER_EOF;
		}
		if( ZipAesDerive(zPw,nPw,zData,nSalt,nKey,zKey) != 0 ){
			return ZIP_ER_INTERNAL;
		}
		if( zKey[nKey*2] != zData[nSalt] || zKey[nKey*2+1] != zData[nSalt+1] ){
			return ZIP_ER_WRONGPASSWD;
		}
		nCipher = nData - (sxu32)nSalt - 2 - 10;
		nAt = SyBlobLength(pOut);
		SyBlobAppend(pOut,&zData[nSalt+2],nCipher);
		if( nCipher > 0
		 && ZipAesCtr(zKey,nKey,(unsigned char *)SyBlobData(pOut) + nAt,nCipher) != 0 ){
			return ZIP_ER_INTERNAL;
		}
		return ZIP_ER_OK;
	}
#else
	return ZIP_ER_ENCRNOTSUPP;
#endif
}
/*
 * Is the cached plaintext of an ENCRYPTED entry still the answer? Only if the
 * archive's password is the one that produced it. libzip decrypts on every
 * zip_fopen, so php refuses a read whose password is wrong however many times
 * the entry was read correctly before -- where a cache with no key on it
 * answered the plaintext to anybody who asked twice.
 */
static int ZipCachedPwMatches(phl_zip *pZip,phl_zip_ent *pEnt)
{
	sxu32 nNow = SyBlobLength(&pZip->sPassword);
	sxu32 nWas = SyBlobLength(&pEnt->sLoadedPw);
	if( nNow != nWas ){
		return 0;
	}
	return nNow == 0
		|| SyMemcmp(SyBlobData(&pZip->sPassword),SyBlobData(&pEnt->sLoadedPw),nNow) == 0;
}
/*
 * The uncompressed bytes of one entry, decompressed once and kept: an archive
 * is normally read many times over, and the file is already in memory. An
 * ENCRYPTED entry's cache is only good for the password that opened it.
 */
static int ZipEntLoad(phl_zip *pZip,phl_zip_ent *pEnt)
{
	const unsigned char *zData;
	sxu32 nData = 0;
	SyBlob sPlain;
	int rc;
	SyBlobInit(&sPlain,&pZip->pVm->sAllocator);
	if( pEnt->bLoaded && pEnt->iEncMethod != ZIP_EM_NONE && !pEnt->bNew
	 && !pEnt->bDataChanged && !ZipCachedPwMatches(pZip,pEnt) ){
		/* A different password than the one behind sData: drop it and decrypt
		 * again, so a wrong one refuses the way it would have the first time. */
		SyBlobReset(&pEnt->sData);
		pEnt->bLoaded = 0;
	}
	if( pEnt->bLoaded || pEnt->bDir ){
		pEnt->bLoaded = 1;
		SyBlobRelease(&sPlain);
		return ZIP_ER_OK;
	}
	if( pEnt->bNew || pEnt->bDataChanged ){
		/* Added or replaced and not yet written. libzip will not read an entry
		 * out of a source it has not committed, and it says so in its own
		 * words: `Entry has been changed`, which is what `getFromName()` on a
		 * fresh `addFromString()` leaves in `$z->status`. */
		SyBlobRelease(&sPlain);
		return ZIP_ER_CHANGED;
	}
	rc = ZipEntPayload(pZip,pEnt,&zData,&nData);
	if( rc != ZIP_ER_OK ){
		SyBlobRelease(&sPlain);
		return rc;
	}
	if( pEnt->iEncMethod != ZIP_EM_NONE ){
		/* The plaintext replaces the slice for everything below. It is the
		 * ARCHIVE's password that opens an entry, never the one a
		 * setEncryption() named -- that one is for the WRITE. */
		rc = ZipDecrypt(pZip,pEnt,zData,nData,&sPlain);
		if( rc != ZIP_ER_OK ){
			SyBlobRelease(&sPlain);
			return rc;
		}
		zData = (const unsigned char *)SyBlobData(&sPlain);
		nData = SyBlobLength(&sPlain);
		/* Remember WHICH password opened it. Every `bLoaded = 1` below is on
		 * this side of the decrypt, so one record covers them all. */
		SyBlobReset(&pEnt->sLoadedPw);
		if( SyBlobLength(&pZip->sPassword) > 0 ){
			SyBlobAppend(&pEnt->sLoadedPw,SyBlobData(&pZip->sPassword),
				SyBlobLength(&pZip->sPassword));
		}
	}
	if( pEnt->iMethod == ZIP_CM_STORE ){
		SyBlobAppend(&pEnt->sData,zData,nData);
		SyBlobRelease(&sPlain);
		pEnt->bLoaded = 1;
		return ZIP_ER_OK;
	}
	if( pEnt->iMethod != ZIP_CM_DEFLATE ){
		/* bzip2, lzma, xz, and the six older methods nothing writes any more:
		 * php's answer on a libzip built without the library is this one, and
		 * the entry is still listed and still copied through a rewrite. */
		SyBlobRelease(&sPlain);
		return ZIP_ER_COMPNOTSUPP;
	}
	{
		/*
		 * Inflate in CHUNKS rather than in one Z_FINISH, which is not a
		 * performance choice: it is what php answers. A member whose deflate
		 * stream is DAMAGED gives back whatever came out of the calls that
		 * succeeded, and a one-shot finish emits a byte more than a chunked
		 * read does on the very same bytes -- measured by flipping one byte of
		 * a payload and reading it under both engines.
		 */
		z_stream z;
		unsigned char zOut[8192];
		int zrc = Z_OK;
		sxu32 nDone = 0;
		if( pEnt->nSize == 0 ){
			SyBlobRelease(&sPlain);
			pEnt->bLoaded = 1;
			return ZIP_ER_OK;
		}
		SyZero(&z,sizeof(z));
		/* A zip member is a RAW deflate stream: no zlib header, no trailer. */
		if( inflateInit2(&z,-MAX_WBITS) != Z_OK ){
			SyBlobRelease(&sPlain);
			return ZIP_ER_ZLIB;
		}
		z.next_in = (Bytef *)zData;
		z.avail_in = (uInt)nData;
		while( nDone < pEnt->nSize ){
			sxu32 nRoom = pEnt->nSize - nDone;
			sxu32 nGot;
			if( nRoom > (sxu32)sizeof(zOut) ){
				nRoom = (sxu32)sizeof(zOut);
			}
			z.next_out = (Bytef *)zOut;
			z.avail_out = (uInt)nRoom;
			zrc = inflate(&z,Z_NO_FLUSH);
			if( zrc != Z_OK && zrc != Z_STREAM_END && zrc != Z_BUF_ERROR ){
				/* The call that FAILED contributes nothing, which is the half
				 * of this a one-shot finish gets wrong. */
				break;
			}
			nGot = nRoom - (sxu32)z.avail_out;
			if( nGot > 0 ){
				SyBlobAppend(&pEnt->sData,zOut,nGot);
				nDone += nGot;
			}
			if( zrc == Z_STREAM_END || (nGot == 0 && z.avail_in == 0) || zrc == Z_BUF_ERROR ){
				break;
			}
		}
		inflateEnd(&z);
		if( zrc == Z_MEM_ERROR ){
			SyBlobRelease(&sPlain);
			return ZIP_ER_MEMORY;
		}
		pEnt->bLoaded = 1;
	}
	SyBlobRelease(&sPlain);
	return ZIP_ER_OK;
}
/* ------------------------------------------------------------------ */
/* Writing an archive                                                  */
/* ------------------------------------------------------------------ */
/*
 * The bytes an entry's SOURCE holds, for one that is being written fresh.
 * addFromString() left them in sData; addFile() left a path, and php reads that
 * file at CLOSE rather than at add -- so a file replaced in between is written
 * as it is now, and one deleted in between fails the close.
 */
static int ZipEntSource(phl_zip *pZip,phl_zip_ent *pEnt,SyBlob *pOut)
{
	if( SyBlobLength(&pEnt->sSrcPath) > 0 ){
		const ph7_io_stream *pStream;
		const char *zPath = (const char *)SyBlobData(&pEnt->sSrcPath);
		void *pHandle;
		SyBlob sRaw;
		SyBlobInit(&sRaw,&pZip->pVm->sAllocator);
		pStream = PH7_VmGetStreamDevice(pZip->pVm,&zPath,
			(int)SyStrlen((const char *)SyBlobData(&pEnt->sSrcPath)));
		if( pStream == 0 ){
			SyBlobRelease(&sRaw);
			return ZIP_ER_OPEN;
		}
		pHandle = PH7_StreamOpenHandle(pZip->pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,
			FALSE,0,FALSE,0,0);
		if( pHandle == 0 ){
			SyBlobRelease(&sRaw);
			return ZIP_ER_OPEN;
		}
		PH7_StreamReadWholeFile(pHandle,pStream,&sRaw);
		PH7_StreamCloseHandle(pStream,pHandle);
		{
			/* The slice addFile() was asked for: a start past the end is an
			 * empty entry rather than a refusal, and a length past it is the
			 * rest of the file. */
			sxu32 nRaw = SyBlobLength(&sRaw);
			sxu32 nStart = 0,nWant;
			if( pEnt->iSrcStart > 0 ){
				nStart = (sxu64)pEnt->iSrcStart < (sxu64)nRaw
					? (sxu32)pEnt->iSrcStart : nRaw;
			}
			nWant = nRaw - nStart;
			if( pEnt->iSrcLen > 0 && (sxu64)pEnt->iSrcLen < (sxu64)nWant ){
				nWant = (sxu32)pEnt->iSrcLen;
			}
			SyBlobAppend(pOut,(const char *)SyBlobData(&sRaw) + nStart,nWant);
		}
		SyBlobRelease(&sRaw);
		return ZIP_ER_OK;
	}
	SyBlobAppend(pOut,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));
	return ZIP_ER_OK;
}
/*
 * Deflate a buffer into a RAW stream, at the level libzip asks zlib for.
 * The `maximum compression` bit the writer sets in the flag word is what says
 * so on the wire, so the two have to agree.
 */
static int ZipDeflate(phl_zip *pZip,const char *zIn,sxu32 nIn,SyBlob *pOut)
{
	z_stream z;
	unsigned char zBuf[8192];
	int rc;
	SyZero(&z,sizeof(z));
	if( deflateInit2(&z,9,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY) != Z_OK ){
		return ZIP_ER_ZLIB;
	}
	z.next_in = (Bytef *)zIn;
	z.avail_in = (uInt)nIn;
	for(;;){
		z.next_out = (Bytef *)zBuf;
		z.avail_out = (uInt)sizeof(zBuf);
		rc = deflate(&z,Z_FINISH);
		if( rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR ){
			deflateEnd(&z);
			return ZIP_ER_ZLIB;
		}
		SyBlobAppend(pOut,zBuf,(sxu32)(sizeof(zBuf) - z.avail_out));
		if( rc == Z_STREAM_END ){
			break;
		}
		if( z.avail_out != 0 && rc == Z_BUF_ERROR ){
			deflateEnd(&z);
			return ZIP_ER_ZLIB;
		}
	}
	deflateEnd(&z);
	SXUNUSED(pZip);
	return ZIP_ER_OK;
}
/*
 * The cipher, applied in place to the bytes an entry is about to be written as.
 *
 * The password is the ENTRY's when setEncryption() named one and the archive's
 * otherwise, and there has to BE one: php lets `setEncryptionName()` succeed
 * with none and then fails the `close()` with `Invalid argument`, which is what
 * an entry with no key to write under is.
 */
static int ZipEncrypt(phl_zip *pZip,phl_zip_ent *pEnt,int iMethod,sxu32 nCrc,
	sxu32 nDosTime,SyBlob *pData)
{
	const char *zPw;
	sxu32 nPw;
	SyBlob sOut;
	int rc = ZIP_ER_OK;
	if( pEnt->bHasPassword ){
		zPw = (const char *)SyBlobData(&pEnt->sPassword);
		nPw = SyBlobLength(&pEnt->sPassword);
	}else{
		zPw = (const char *)SyBlobData(&pZip->sPassword);
		nPw = SyBlobLength(&pZip->sPassword);
	}
	if( nPw < 1 ){
		return ZIP_ER_INVAL;
	}
	if( !ZipEncSupported(iMethod) || iMethod == ZIP_EM_NONE ){
		return ZIP_ER_ENCRNOTSUPP;
	}
	SyBlobInit(&sOut,&pZip->pVm->sAllocator);
	if( iMethod == ZIP_EM_TRAD_PKWARE ){
		phl_zip_keys sKeys;
		unsigned char zHdr[12];
		sxu32 i;
		SXUNUSED(nCrc);
		/* Eleven random bytes and a CHECK byte a reader can test the password
		 * against. The archives this writes carry the trailing-descriptor flag
		 * the way libzip's do, so the check byte is the DOS time's high one --
		 * and it is the time the HEADER will carry, which is the one thing php
		 * gets wrong here: libzip takes the check byte from the stamp the entry
		 * had before `setMtime*()` moved it, so a php archive written that way
		 * cannot be read back by php itself. Reproducing that would mean
		 * writing an archive nothing can open. */
		for( i = 0 ; i < 11 ; ++i ){
			zHdr[i] = (unsigned char)(PH7_VmRandomNum(pZip->pVm) & 0xFF);
		}
		zHdr[11] = (unsigned char)((nDosTime >> 8) & 0xFF);
		ZipTradInit(&sKeys,zPw,nPw);
		for( i = 0 ; i < 12 ; ++i ){
			zHdr[i] = ZipTradEncrypt(&sKeys,zHdr[i]);
		}
		SyBlobAppend(&sOut,zHdr,sizeof(zHdr));
		{
			const unsigned char *zIn = (const unsigned char *)SyBlobData(pData);
			sxu32 nIn = SyBlobLength(pData);
			for( i = 0 ; i < nIn ; ){
				unsigned char zBuf[4096];
				sxu32 n = 0;
				while( i < nIn && n < (sxu32)sizeof(zBuf) ){
					zBuf[n++] = ZipTradEncrypt(&sKeys,zIn[i++]);
				}
				SyBlobAppend(&sOut,zBuf,n);
			}
		}
	}else{
#ifdef PH7_ENABLE_OPENSSL
		int nKey = ZipAesKeyLen(iMethod);
		int nSalt = nKey / 2;
		unsigned char zSalt[ZIP_AES_MAXKEY / 2];
		unsigned char zKey[ZIP_AES_MAXKEY * 2 + 2];
		unsigned char zMac[10];
		sxu32 nBody = SyBlobLength(pData);
		if( RAND_bytes(zSalt,nSalt) != 1
		 || ZipAesDerive(zPw,nPw,zSalt,nSalt,nKey,zKey) != 0 ){
			SyBlobRelease(&sOut);
			return ZIP_ER_INTERNAL;
		}
		SyBlobAppend(&sOut,zSalt,(sxu32)nSalt);
		SyBlobAppend(&sOut,&zKey[nKey*2],2);
		SyBlobAppend(&sOut,SyBlobData(pData),nBody);
		if( nBody > 0
		 && ZipAesCtr(zKey,nKey,(unsigned char *)SyBlobData(&sOut) + nSalt + 2,nBody) != 0 ){
			SyBlobRelease(&sOut);
			return ZIP_ER_INTERNAL;
		}
		/* The authentication code is over the CIPHERTEXT, which is why it is
		 * taken after the pass above rather than before it. */
		if( ZipAesMac(&zKey[nKey],nKey,
			(const unsigned char *)SyBlobData(&sOut) + nSalt + 2,nBody,zMac) != 0 ){
			SyBlobRelease(&sOut);
			return ZIP_ER_INTERNAL;
		}
		SyBlobAppend(&sOut,zMac,sizeof(zMac));
#else
		SyBlobRelease(&sOut);
		return ZIP_ER_ENCRNOTSUPP;
#endif
	}
	SyBlobReset(pData);
	SyBlobAppend(pData,SyBlobData(&sOut),SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return rc;
}
/*
 * How one entry's bytes go out, and under which method.
 *
 * Three answers, and the FIRST is the one that matters most: an entry nobody
 * touched is copied through BYTE FOR BYTE, method and CRC included. That is
 * what lets an archive holding a bzip2 or an lzma member -- which this build
 * cannot decode -- survive a rewrite that adds a file beside it, and it is what
 * libzip does too.
 *
 * `CM_DEFAULT` is deflate, unless deflate did not help: libzip stores an entry
 * whose deflated form is no smaller, which is why a thirteen-byte file comes
 * out `comp_method` 0. An EXPLICIT `CM_DEFLATE` does not get that second look,
 * so `setCompressionName($n, CM_DEFLATE)` on random bytes really does write
 * more than it was given.
 */
static int ZipEntStored(phl_zip *pZip,phl_zip_ent *pEnt,SyBlob *pOut,
	int *piMethod,sxu32 *pnCrc,sxu32 *pnSize,int *piEncrypt,sxu32 nDosTime)
{
	SyBlob sRaw;
	int iWant,rc;
	int bTouched = pEnt->bNew || pEnt->bDataChanged;
	int bReEncrypt = pEnt->iSetEncrypt >= 0 && pEnt->iSetEncrypt != pEnt->iEncMethod;
	*piEncrypt = pEnt->iSetEncrypt >= 0 ? pEnt->iSetEncrypt : pEnt->iEncMethod;
	if( !bTouched && !bReEncrypt
	 && (pEnt->iSetMethod == ZIP_CM_DEFAULT || pEnt->iSetMethod == pEnt->iMethod) ){
		const unsigned char *zData;
		sxu32 nData = 0;
		rc = ZipEntPayload(pZip,pEnt,&zData,&nData);
		if( rc != ZIP_ER_OK ){
			return rc;
		}
		SyBlobAppend(pOut,zData,nData);
		*piMethod = pEnt->iMethod;
		*pnCrc = pEnt->nCrc;
		*pnSize = pEnt->nSize;
		return ZIP_ER_OK;
	}
	SyBlobInit(&sRaw,&pZip->pVm->sAllocator);
	if( bTouched ){
		rc = ZipEntSource(pZip,pEnt,&sRaw);
	}else{
		rc = ZipEntLoad(pZip,pEnt);
		if( rc == ZIP_ER_OK ){
			SyBlobAppend(&sRaw,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));
		}
	}
	if( rc != ZIP_ER_OK ){
		SyBlobRelease(&sRaw);
		return rc;
	}
	*pnSize = SyBlobLength(&sRaw);
	*pnCrc = (sxu32)crc32(crc32(0L,Z_NULL,0),
		(const Bytef *)SyBlobData(&sRaw),(uInt)SyBlobLength(&sRaw));
	iWant = pEnt->iSetMethod;
	if( pEnt->bDir ){
		iWant = ZIP_CM_STORE;
	}
	if( iWant == ZIP_CM_STORE ){
		SyBlobAppend(pOut,SyBlobData(&sRaw),SyBlobLength(&sRaw));
		*piMethod = ZIP_CM_STORE;
	}else if( iWant != ZIP_CM_DEFAULT && iWant != ZIP_CM_DEFLATE ){
		SyBlobRelease(&sRaw);
		return ZIP_ER_COMPNOTSUPP;
	}else{
		SyBlob sDef;
		SyBlobInit(&sDef,&pZip->pVm->sAllocator);
		rc = ZipDeflate(pZip,(const char *)SyBlobData(&sRaw),SyBlobLength(&sRaw),&sDef);
		if( rc != ZIP_ER_OK ){
			SyBlobRelease(&sDef);
			SyBlobRelease(&sRaw);
			return rc;
		}
		if( iWant == ZIP_CM_DEFAULT && SyBlobLength(&sDef) >= SyBlobLength(&sRaw) ){
			SyBlobAppend(pOut,SyBlobData(&sRaw),SyBlobLength(&sRaw));
			*piMethod = ZIP_CM_STORE;
		}else{
			SyBlobAppend(pOut,SyBlobData(&sDef),SyBlobLength(&sDef));
			*piMethod = ZIP_CM_DEFLATE;
		}
		SyBlobRelease(&sDef);
	}
	SyBlobRelease(&sRaw);
	/* The cipher goes over the COMPRESSED bytes, which is the format's order and
	 * the only one a reader can undo. */
	if( *piEncrypt != ZIP_EM_NONE ){
		rc = ZipEncrypt(pZip,pEnt,*piEncrypt,*pnCrc,nDosTime,pOut);
		if( rc != ZIP_ER_OK ){
			return rc;
		}
	}
	return ZIP_ER_OK;
}
/* php's registerProgressCallback(), asked once per entry.
 *
 * libzip reports i/n BEFORE writing entry i and calls only when the value has
 * advanced by MORE than the rate the script asked for -- strictly more, which
 * is why a rate of 0.5 over six entries reports 0 and then 4/6 rather than 0
 * and 3/6, and why neither run ever ends on 1.0. */
static void ZipProgress(phl_zip *pZip,double *pLast,sxu32 i,sxu32 n)
{
	double r = n > 0 ? (double)i / (double)n : 0.0;
	ph7_value sArg,sRes;
	ph7_value *pArg = &sArg;
	if( pZip->pProgress == 0 ){
		return;
	}
	if( i > 0 && r - *pLast <= pZip->rProgressRate ){
		return;
	}
	*pLast = r;
	PH7_MemObjInit(pZip->pVm,&sArg);
	PH7_MemObjInit(pZip->pVm,&sRes);
	ph7_value_double(&sArg,r);
	PH7_VmCallUserFunction(pZip->pVm,pZip->pProgress,1,&pArg,&sRes);
	PH7_MemObjRelease(&sRes);
	PH7_MemObjRelease(&sArg);
}
/* php's registerCancelCallback(): a non-zero answer stops the write, and the
 * archive reports ER_CANCELLED and is left as it was. */
static int ZipCancelled(phl_zip *pZip)
{
	ph7_value sRet;
	int bStop;
	if( pZip->pCancel == 0 ){
		return 0;
	}
	PH7_MemObjInit(pZip->pVm,&sRet);
	PH7_VmCallUserFunction(pZip->pVm,pZip->pCancel,0,0,&sRet);
	PH7_MemObjToInteger(&sRet);
	bStop = sRet.x.iVal != 0;
	PH7_MemObjRelease(&sRet);
	return bStop;
}
/*
 * Build the whole file: a local header and payload per entry, then the central
 * directory, then the end record with the archive comment behind it.
 */
static int ZipBuild(phl_zip *pZip,SyBlob *pOut)
{
	SyBlob sCd,sExtra;
	sxu32 i,nCdOff,nEntries = 0,nWrite = 0;
	double rLast = 0.0;
	int rc = ZIP_ER_OK;
	for( i = 0 ; i < pZip->nEnt ; ++i ){
		if( !pZip->apEnt[i]->bDeleted ){
			nWrite++;
		}
	}
	SyBlobInit(&sCd,&pZip->pVm->sAllocator);
	SyBlobInit(&sExtra,&pZip->pVm->sAllocator);
	for( i = 0 ; i < pZip->nEnt ; ++i ){
		phl_zip_ent *pEnt = pZip->apEnt[i];
		SyBlob sStored;
		sxu32 nLocal,nName,nCrc = 0,nSize = 0,nTime,nDate,nFlag,nRawMethod,nVersion;
		int iMethod = ZIP_CM_STORE;
		int iEncrypt = ZIP_EM_NONE;
		if( pEnt->bDeleted ){
			continue;
		}
		ZipProgress(pZip,&rLast,nEntries,nWrite);
		if( ZipCancelled(pZip) ){
			rc = ZIP_ER_CANCELLED;
			break;
		}
		nLocal = SyBlobLength(pOut);
		nName = SyBlobLength(&pEnt->sName);
		SyBlobInit(&sStored,&pZip->pVm->sAllocator);
		ZipUnixToDos(pEnt->iTime,&nTime,&nDate);
		rc = ZipEntStored(pZip,pEnt,&sStored,&iMethod,&nCrc,&nSize,&iEncrypt,nTime);
		if( rc != ZIP_ER_OK ){
			SyBlobRelease(&sStored);
			break;
		}
		/* The flag word: an untouched entry keeps the one it came with, so a
		 * data-descriptor or an encryption bit travels with its payload. */
		if( pEnt->bNew || pEnt->bDataChanged || pEnt->iSetEncrypt >= 0 ){
			nFlag = 0;
		}else{
			nFlag = pEnt->nFlag;
		}
		if( iMethod == ZIP_CM_DEFLATE
		 && (pEnt->bNew || pEnt->bDataChanged || pEnt->iSetEncrypt >= 0) ){
			nFlag |= 0x0002;   /* deflate, maximum compression */
		}
		if( ZipNameIsUtf8((const char *)SyBlobData(&pEnt->sName),nName) ){
			nFlag |= 0x0800;
		}
		nRawMethod = iMethod;
		nVersion = ZIP_VERSION_NEEDED;
		SyBlobReset(&sExtra);
		if( iEncrypt != ZIP_EM_NONE ){
			nFlag |= 0x0001;
			if( iEncrypt == ZIP_EM_TRAD_PKWARE ){
				/* The check byte the header carries is the DOS TIME's, which is
				 * only legible to a reader when bit 3 says the CRC was not
				 * known when the entry was written. libzip writes the pair this
				 * way and every reader expects it. */
				nFlag |= 0x0008;
			}else{
				/* WinZip AES: the method BYTE is a placeholder and the real one
				 * moves into an extra field beside the key strength. A member
				 * under twenty bytes writes NO crc -- the WinZip rule, since a
				 * checksum of so little plaintext is a key-recovery hint. */
				nRawMethod = ZIP_CM_WINZIP_AES;
				nVersion = 51;
				ZipPut16(&sExtra,ZIP_EXTRA_AES);
				ZipPut16(&sExtra,7);
				ZipPut16(&sExtra,2);   /* AE-2 */
				SyBlobAppend(&sExtra,"AE",2);
				{
					unsigned char bStrength =
						(unsigned char)(iEncrypt - ZIP_EM_AES_128 + 1);
					SyBlobAppend(&sExtra,&bStrength,1);
				}
				ZipPut16(&sExtra,(sxu32)iMethod);
				if( nSize < 20 ){
					nCrc = 0;
				}
			}
		}
		SyBlobAppend(pOut,"PK\003\004",4);
		ZipPut16(pOut,nVersion);
		ZipPut16(pOut,nFlag);
		ZipPut16(pOut,(sxu32)nRawMethod);
		ZipPut16(pOut,nTime);
		ZipPut16(pOut,nDate);
		ZipPut32(pOut,nCrc);
		ZipPut32(pOut,SyBlobLength(&sStored));
		ZipPut32(pOut,nSize);
		ZipPut16(pOut,nName);
		ZipPut16(pOut,SyBlobLength(&sExtra));
		SyBlobAppend(pOut,SyBlobData(&pEnt->sName),nName);
		SyBlobAppend(pOut,SyBlobData(&sExtra),SyBlobLength(&sExtra));
		SyBlobAppend(pOut,SyBlobData(&sStored),SyBlobLength(&sStored));
		SyBlobAppend(&sCd,"PK\001\002",4);
		ZipPut16(&sCd,(sxu32)((pEnt->iOpsys << 8) | (ZIP_VERSION_MADE & 0xFF)));
		ZipPut16(&sCd,nVersion);
		ZipPut16(&sCd,nFlag);
		ZipPut16(&sCd,(sxu32)nRawMethod);
		ZipPut16(&sCd,nTime);
		ZipPut16(&sCd,nDate);
		ZipPut32(&sCd,nCrc);
		ZipPut32(&sCd,SyBlobLength(&sStored));
		ZipPut32(&sCd,nSize);
		ZipPut16(&sCd,nName);
		ZipPut16(&sCd,SyBlobLength(&sExtra));
		ZipPut16(&sCd,SyBlobLength(&pEnt->sComment));
		ZipPut16(&sCd,0);
		ZipPut16(&sCd,0);
		ZipPut32(&sCd,pEnt->nAttr);
		ZipPut32(&sCd,nLocal);
		SyBlobAppend(&sCd,SyBlobData(&pEnt->sName),nName);
		SyBlobAppend(&sCd,SyBlobData(&sExtra),SyBlobLength(&sExtra));
		SyBlobAppend(&sCd,SyBlobData(&pEnt->sComment),SyBlobLength(&pEnt->sComment));
		SyBlobRelease(&sStored);
		nEntries++;
	}
	if( rc == ZIP_ER_OK ){
		nCdOff = SyBlobLength(pOut);
		SyBlobAppend(pOut,SyBlobData(&sCd),SyBlobLength(&sCd));
		SyBlobAppend(pOut,"PK\005\006",4);
		ZipPut16(pOut,0);
		ZipPut16(pOut,0);
		ZipPut16(pOut,nEntries);
		ZipPut16(pOut,nEntries);
		ZipPut32(pOut,SyBlobLength(&sCd));
		ZipPut32(pOut,nCdOff);
		ZipPut16(pOut,SyBlobLength(&pZip->sComment));
		if( SyBlobLength(&pZip->sComment) > 0 ){
			SyBlobAppend(pOut,SyBlobData(&pZip->sComment),SyBlobLength(&pZip->sComment));
		}
	}
	SyBlobRelease(&sCd);
	SyBlobRelease(&sExtra);
	return rc;
}
/* ------------------------------------------------------------------ */
/* Lifetime                                                            */
/* ------------------------------------------------------------------ */
/*
 * The per-VM registry every open archive is on. The object that opened one
 * holds a reference and so does every live `getStream()` handle, which is what
 * makes php's own lifetime work: `close()` leaves such a stream readable, and
 * the archive really goes only when the last holder lets go.
 */
static void ZipRelease(phl_zip *pZip)
{
	ph7_vm *pVm = pZip->pVm;
	phl_zip *p,*pPrev = 0;
	sxu32 i;
	if( --pZip->nRef > 0 ){
		return;
	}
	for( p = (phl_zip *)pVm->pZips ; p ; pPrev = p, p = p->pNext ){
		if( p != pZip ){
			continue;
		}
		if( pPrev ){
			pPrev->pNext = p->pNext;
		}else{
			pVm->pZips = (void *)p->pNext;
		}
		break;
	}
	for( i = 0 ; i < pZip->nEnt ; ++i ){
		ZipEntFree(pVm,pZip->apEnt[i]);
	}
	if( pZip->apEnt ){
		SyMemBackendFree(&pVm->sAllocator,pZip->apEnt);
	}
	if( pZip->pProgress ){
		ph7_release_value(pVm,pZip->pProgress);
	}
	if( pZip->pCancel ){
		ph7_release_value(pVm,pZip->pCancel);
	}
	SyBlobRelease(&pZip->sPath);
	SyBlobRelease(&pZip->sFile);
	SyBlobRelease(&pZip->sComment);
	SyBlobRelease(&pZip->sOrigComment);
	SyBlobRelease(&pZip->sPassword);
	SyMemBackendFree(&pVm->sAllocator,pZip);
}
static phl_zip * ZipNew(ph7_vm *pVm)
{
	phl_zip *pZip = (phl_zip *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_zip));
	if( pZip == 0 ){
		return 0;
	}
	SyZero(pZip,sizeof(*pZip));
	pZip->pVm = pVm;
	SyBlobInit(&pZip->sPath,&pVm->sAllocator);
	SyBlobInit(&pZip->sFile,&pVm->sAllocator);
	SyBlobInit(&pZip->sComment,&pVm->sAllocator);
	SyBlobInit(&pZip->sOrigComment,&pVm->sAllocator);
	SyBlobInit(&pZip->sPassword,&pVm->sAllocator);
	pZip->nRef = 1;
	pZip->bOpen = 1;
	pZip->pNext = (phl_zip *)pVm->pZips;
	pVm->pZips = (void *)pZip;
	return pZip;
}
/* The VM is going away or being reset: nothing is left to write a pending
 * change to, so every archive is simply dropped. php's own shutdown commits
 * one an object still holds, and the object's teardown is what does that here,
 * running before this sweep. */
static void ZipVmSweep(ph7_vm *pVm)
{
	while( pVm->pZips ){
		phl_zip *pZip = (phl_zip *)pVm->pZips;
		pZip->nRef = 1;
		ZipRelease(pZip);
	}
}
PH7_PRIVATE void PH7_ZipVmReset(ph7_vm *pVm)
{
	ZipVmSweep(&(*pVm));
}
PH7_PRIVATE void PH7_ZipVmRelease(ph7_vm *pVm)
{
	ZipVmSweep(&(*pVm));
}
/*
 * Write the archive back to the file it names.
 *
 * An archive with NO entries left is REMOVED rather than written: php's
 * `close()` on a freshly CREATEd archive nobody added to leaves no file behind
 * at all, and so does one whose every entry was deleted.
 */
/* Has anything been asked of this archive that the FILE does not already say?
 * php writes nothing when the answer is no -- an archive opened, read and
 * closed is byte-identical afterwards -- which is also what keeps a close from
 * failing on an entry it could not have re-compressed. */
static int ZipDirty(phl_zip *pZip)
{
	sxu32 i;
	if( pZip->bCommentChanged || pZip->bCreated ){
		return 1;
	}
	for( i = 0 ; i < pZip->nEnt ; ++i ){
		phl_zip_ent *pEnt = pZip->apEnt[i];
		if( pEnt->bNew || pEnt->bDeleted || pEnt->bDataChanged || pEnt->bNameChanged
		 || pEnt->bCommentChanged || pEnt->bTimeChanged || pEnt->bAttrChanged
		 || (pEnt->iSetMethod != ZIP_CM_DEFAULT && pEnt->iSetMethod != pEnt->iMethod)
		 || (pEnt->iSetEncrypt >= 0 && pEnt->iSetEncrypt != pEnt->iEncMethod) ){
			return 1;
		}
	}
	return 0;
}
static int ZipCommit(phl_zip *pZip)
{
	ph7_vm *pVm = pZip->pVm;
	const ph7_io_stream *pStream;
	SyBlob sOut;
	const char *zPath;
	void *pHandle;
	sxu32 i,nLive = 0;
	int rc;
	for( i = 0 ; i < pZip->nEnt ; ++i ){
		if( !pZip->apEnt[i]->bDeleted ){
			nLive++;
		}
	}
	SyBlobNullAppend(&pZip->sPath);
	zPath = (const char *)SyBlobData(&pZip->sPath);
	if( nLive == 0 ){
		const ph7_vfs *pVfs = pVm->pEngine->pVfs;
		if( pZip->bCreated ){
			return ZIP_ER_OK;   /* nothing was ever there to remove */
		}
		if( pVfs && pVfs->xUnlink && pVfs->xUnlink(zPath) != PH7_OK ){
			pZip->iStatusSys = errno;
			return ZIP_ER_REMOVE;
		}
		return ZIP_ER_OK;
	}
	if( !ZipDirty(pZip) || pZip->bBuilding ){
		/* bBuilding: a progress or cancel callback that closes the very archive
		 * being written would otherwise start a second build inside the first. */
		return ZIP_ER_OK;
	}
	pZip->bBuilding = 1;
	SyBlobInit(&sOut,&pVm->sAllocator);
	rc = ZipBuild(pZip,&sOut);
	pZip->bBuilding = 0;
	if( rc != ZIP_ER_OK ){
		SyBlobRelease(&sOut);
		return rc;
	}
	pStream = PH7_VmGetStreamDevice(pVm,&zPath,(int)SyBlobLength(&pZip->sPath));
	if( pStream == 0 ){
		SyBlobRelease(&sOut);
		return ZIP_ER_OPEN;
	}
	pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,
		PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,0);
	if( pHandle == 0 ){
		SyBlobRelease(&sOut);
		/* php's own answer for a directory it cannot write the temporary into:
		 * libzip creates one beside the archive and reports ER_TMPOPEN with the
		 * system errno behind it. */
		pZip->iStatusSys = errno;
		return ZIP_ER_TMPOPEN;
	}
	if( pStream->xWrite ){
		pStream->xWrite(pHandle,SyBlobData(&sOut),(ph7_int64)SyBlobLength(&sOut));
	}
	PH7_StreamCloseHandle(pStream,pHandle);
	SyBlobRelease(&sOut);
	return ZIP_ER_OK;
}
/* ------------------------------------------------------------------ */
/* The object behind $this                                             */
/* ------------------------------------------------------------------ */
/* The hidden slot the archive hangs off. */
#define ZIP_RES "__res"
static phl_zip * ZipOfInstance(ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr,ZIP_RES,sizeof(ZIP_RES)-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 || !ph7_value_is_resource(pRes) ){
		return 0;
	}
	return (phl_zip *)ph7_value_to_resource(pRes);
}
static int ZipAttach(ph7_class_instance *pThis,phl_zip *pZip)
{
	SyString sAttr;
	ph7_value *pRes;
	if( pThis == 0 ){
		return -1;
	}
	SyStringInitFromBuf(&sAttr,ZIP_RES,sizeof(ZIP_RES)-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 ){
		return -1;
	}
	PH7_MemObjRelease(pRes);
	if( pZip ){
		pRes->x.pOther = pZip;
		MemObjSetType(pRes,MEMOBJ_RES);
	}
	return 0;
}
/* One of the six state slots, written from C. They are declared properties on
 * the class and php keeps them live through a read handler; here the C bodies
 * write them after every verb that could move one, which is the same fact from
 * the other side and costs a store rather than a hook. */
#define ZipSetPropInt(pThis,zName,iVal) \
	PH7_NativeSetAttrInt((pThis)->pVm,(pThis),(zName),(iVal))
#define ZipSetPropStr(pThis,zName,z,n) \
	PH7_NativeSetAttrStr((pThis)->pVm,(pThis),(zName),(z),(int)(n))
/*
 * Bring the six php-visible slots up to date. Every verb that can move one ends
 * here, which is why a script never sees a stale `numFiles`.
 *
 * A CLOSED handle answers the same six as a fresh object: php's read handler
 * has no archive to ask and falls back to the declared zeroes, so `numFiles`
 * is 0 and `filename` is "" the moment `close()` returns.
 */
static void ZipRefresh(ph7_class_instance *pThis,phl_zip *pZip)
{
	if( pThis == 0 ){
		return;
	}
	if( pZip == 0 || !pZip->bOpen ){
		ZipSetPropInt(pThis,"lastId",-1);
		ZipSetPropInt(pThis,"status",pZip ? pZip->iStatus : 0);
		ZipSetPropInt(pThis,"statusSys",pZip ? pZip->iStatusSys : 0);
		ZipSetPropInt(pThis,"numFiles",0);
		ZipSetPropStr(pThis,"filename","",0);
		ZipSetPropStr(pThis,"comment","",0);
		return;
	}
	ZipSetPropInt(pThis,"lastId",pZip->iLastId);
	ZipSetPropInt(pThis,"status",pZip->iStatus);
	ZipSetPropInt(pThis,"statusSys",pZip->iStatusSys);
	ZipSetPropInt(pThis,"numFiles",(sxi64)pZip->nEnt);
	ZipSetPropStr(pThis,"filename",(const char *)SyBlobData(&pZip->sPath),
		SyBlobLength(&pZip->sPath));
	ZipSetPropStr(pThis,"comment",(const char *)SyBlobData(&pZip->sComment),
		SyBlobLength(&pZip->sComment));
}
/*
 * The archive a method is being asked about, or php's refusal.
 *
 * php's every ZipArchive method but one starts by checking that the object has
 * a live archive behind it and throws `ValueError: Invalid or uninitialized Zip
 * object` when it does not -- `close()` and `count()` included, which is what
 * makes calling either on a fresh object an Error rather than a false. The one
 * exception is `getStatusString()`, which answers "No error" for an object that
 * was never opened.
 */
static phl_zip * ZipThis(ph7_context *pCtx,ph7_class_instance **ppThis)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_zip *pZip = ZipOfInstance(pThis);
	if( ppThis ){
		*ppThis = pThis;
	}
	if( pZip == 0 || !pZip->bOpen ){
		PH7_VmThrowException(pCtx,"ValueError","Invalid or uninitialized Zip object");
		return 0;
	}
	return pZip;
}
/* The object is going away. php's own teardown COMMITS a still-open archive --
 * a script that adds files and never calls close() still gets its archive --
 * and it invalidates every stream getStream() handed out, which is what makes
 * reading one after the object dies the `Containing zip archive was closed`
 * error rather than more bytes. */
static void ZipInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	phl_zip *pZip = ZipOfInstance(pThis);
	SXUNUSED(pVm);
	if( pZip == 0 ){
		return;
	}
	if( pZip->bOpen ){
		ZipCommit(pZip);
		pZip->bOpen = 0;
		pZip->bDead = 1;
	}
	ZipAttach(pThis,0);
	ZipRelease(pZip);
}
/*
 * A fresh object, before any open. php's read handler has no archive to ask and
 * answers the six with -1, 0, 0, 0, "" and "" -- so those are what the slots
 * start holding here, seeded by the create_object hook. They are a STARTING
 * VALUE and not a declared default, which is why the declarations above carry
 * none and Reflection's `hasDefaultValue()` answers false for all six.
 */
static void ZipNewHook(ph7_vm *pVm,ph7_class_instance *pThis)
{
	SXUNUSED(pVm);
	ZipRefresh(pThis,0);
}
/*
 * php refuses a write to any of the six: its write_property handler answers
 * `Cannot write read-only property ZipArchive::$numFiles` and stores nothing,
 * while Reflection still reports the property as neither readonly nor virtual.
 * The C bodies above write their slots directly and never pass this filter.
 */
static void ZipSetHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)
{
	SXUNUSED(pVm);
	pCtx->zThrowClass = "Error";
	SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
		"Cannot write read-only property %z::$%z",&pThis->pClass->sDisp,pCtx->pName);
}
/* ------------------------------------------------------------------ */
/* Errors                                                              */
/* ------------------------------------------------------------------ */
/*
 * libzip's error table, which is what `getStatusString()` reads and the only
 * part of this extension a script can see that is not the FORMAT. It has not
 * changed in the library's lifetime, and php prints these sentences verbatim.
 *
 * Nine of the codes are SYSTEM errors -- the ones raised by a call to the
 * operating system -- and those print as `sentence: strerror(statusSys)`, which
 * is why a close() that cannot write its temporary file reads `Failure to
 * create temporary file: Permission denied` rather than half of that.
 */
static const char * ZipErrString(int iErr)
{
	switch( iErr ){
	case ZIP_ER_OK:              return "No error";
	case ZIP_ER_MULTIDISK:       return "Multi-disk zip archives not supported";
	case ZIP_ER_RENAME:          return "Renaming temporary file failed";
	case ZIP_ER_CLOSE:           return "Closing zip archive failed";
	case ZIP_ER_SEEK:            return "Seek error";
	case ZIP_ER_READ:            return "Read error";
	case ZIP_ER_WRITE:           return "Write error";
	case ZIP_ER_CRC:             return "CRC error";
	case ZIP_ER_ZIPCLOSED:       return "Containing zip archive was closed";
	case ZIP_ER_NOENT:           return "No such file";
	case ZIP_ER_EXISTS:          return "File already exists";
	case ZIP_ER_OPEN:            return "Can't open file";
	case ZIP_ER_TMPOPEN:         return "Failure to create temporary file";
	case ZIP_ER_ZLIB:            return "Zlib error";
	case ZIP_ER_MEMORY:          return "Malloc failure";
	case ZIP_ER_CHANGED:         return "Entry has been changed";
	case ZIP_ER_COMPNOTSUPP:     return "Compression method not supported";
	case ZIP_ER_EOF:             return "Premature end of file";
	case ZIP_ER_INVAL:           return "Invalid argument";
	case ZIP_ER_NOZIP:           return "Not a zip archive";
	case ZIP_ER_INTERNAL:        return "Internal error";
	case ZIP_ER_INCONS:          return "Zip archive inconsistent";
	case ZIP_ER_REMOVE:          return "Can't remove file";
	case ZIP_ER_DELETED:         return "Entry has been deleted";
	case ZIP_ER_ENCRNOTSUPP:     return "Encryption method not supported";
	case ZIP_ER_RDONLY:          return "Read-only archive";
	case ZIP_ER_NOPASSWD:        return "No password provided";
	case ZIP_ER_WRONGPASSWD:     return "Wrong password provided";
	case ZIP_ER_OPNOTSUPP:       return "Operation not supported";
	case ZIP_ER_INUSE:           return "Resource still in use";
	case ZIP_ER_TELL:            return "Tell error";
	case ZIP_ER_COMPRESSED_DATA: return "Compressed data invalid";
	case ZIP_ER_CANCELLED:       return "Operation cancelled";
	default:                     break;
	}
	return "Unknown error";
}
static int ZipErrIsSys(int iErr)
{
	switch( iErr ){
	case ZIP_ER_RENAME: case ZIP_ER_CLOSE: case ZIP_ER_SEEK:
	case ZIP_ER_READ:   case ZIP_ER_WRITE: case ZIP_ER_OPEN:
	case ZIP_ER_TMPOPEN: case ZIP_ER_REMOVE: case ZIP_ER_TELL:
		return 1;
	default:
		return 0;
	}
}
static void ZipErrText(int iErr,int iSys,SyBlob *pOut)
{
	const char *z = ZipErrString(iErr);
	SyBlobAppend(pOut,z,(sxu32)SyStrlen(z));
	if( ZipErrIsSys(iErr) && iSys != 0 ){
		const char *zSys = VfsStrerror(iSys);
		SyBlobAppend(pOut,": ",sizeof(": ")-1);
		SyBlobAppend(pOut,zSys,(sxu32)SyStrlen(zSys));
	}
}
/*
 * Remember a failure on the archive. php's `$z->status` is STICKY: a successful
 * call after a failed one leaves the old code in place, and only clearError()
 * or the next failure moves it.
 */
static int ZipFail(ph7_context *pCtx,phl_zip *pZip,int iErr)
{
	pZip->iStatus = iErr;
	if( !ZipErrIsSys(iErr) ){
		pZip->iStatusSys = 0;
	}
	ZipRefresh(PH7_ContextThis(pCtx),pZip);
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* The archive's own verbs                                             */
/* ------------------------------------------------------------------ */
/* Which entry a `*Index` method was asked about, with php's refusal when the
 * index names nothing: ER_INVAL, and false. */
static phl_zip_ent * ZipArgIndex(ph7_context *pCtx,phl_zip *pZip,ph7_value *pArg,int *pRc)
{
	phl_zip_ent *pEnt = ZipAt(pZip,ph7_value_to_int64(pArg));
	*pRc = PH7_OK;
	if( pEnt == 0 ){
		*pRc = ZipFail(pCtx,pZip,ZIP_ER_INVAL);
		return 0;
	}
	return pEnt;
}
/*
 * A name php PUTS in the archive is a C string: it hands the library a `char *`,
 * so `addFromString("x\0y", …)` stores `x` and nothing after it. Six doors that
 * LOOK a name up screen the NUL instead and throw; the rest simply match the
 * truncated form, which is the same entry.
 */
static sxu32 ZipCName(const char *zName,int nName)
{
	int i;
	for( i = 0 ; i < nName ; ++i ){
		if( zName[i] == 0 ){
			return (sxu32)i;
		}
	}
	return (sxu32)(nName < 0 ? 0 : nName);
}
/* ...and the screen those six raise instead. */
static int ZipNulName(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,int *pRc)
{
	int n = 0;
	const char *z = ph7_value_to_string(pArg,&n);
	if( n < 1 || (int)ZipCName(z,n) == n ){
		return 0;
	}
	*pRc = PH7_VmThrowException(pCtx,"ValueError",
		"%z(): Argument #%d ($%s) must not contain any null bytes",
		&pCtx->pFunc->sName,iPos,zName);
	return 1;
}
/*
 * php's empty-NAME screen. Most of the `*Name` doors refuse one outright with a
 * ValueError that names the argument -- and four of them do not: `locateName`,
 * `deleteName`, `unchangeName` and `addEmptyDir` answer false, and
 * `addFromString` takes the empty name and stores it.
 */
static int ZipEmptyName(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,int *pRc)
{
	int n = 0;
	ph7_value_to_string(pArg,&n);
	if( n > 0 ){
		return 0;
	}
	*pRc = PH7_VmThrowException(pCtx,"ValueError",
		"%z(): Argument #%d ($%s) must not be empty",&pCtx->pFunc->sName,iPos,zName);
	return 1;
}
/* ...and the same for a `*Name` method, whose miss is ER_NOENT. */
static phl_zip_ent * ZipArgName(ph7_context *pCtx,phl_zip *pZip,ph7_value *pArg,int iFlags,int *pRc)
{
	int nName = 0;
	const char *zName = ph7_value_to_string(pArg,&nName);
	/* The doors that do NOT screen a NUL still hand the library a C string, so
	 * the name they look up is the one before it. */
	phl_zip_ent *pEnt = ZipFind(pZip,zName,ZipCName(zName,nName),iFlags,0);
	*pRc = PH7_OK;
	if( pEnt == 0 ){
		*pRc = ZipFail(pCtx,pZip,ZIP_ER_NOENT);
		return 0;
	}
	return pEnt;
}
/*
 * `ZipArchive::open()`.
 *
 * Answers TRUE, or one of php's ER_ codes as an INT -- not false, which is why
 * `if (!$z->open(...))` is the wrong test and `!== true` the right one.
 *
 * Opening on an object that already holds an archive silently CLOSES the first,
 * commits included, and only then looks at the new name.
 */
static int vm_builtin_ZipArchive_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	phl_zip *pOld = ZipOfInstance(pThis);
	phl_zip *pZip;
	const char *zName;
	int nName = 0,iFlags = 0,rc;
	SyBlob sPath;
	SXUNUSED(nArg);
	zName = ph7_value_to_string(apArg[0],&nName);
	if( nName < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #1 ($filename) must not be empty",&pCtx->pFunc->sName);
	}
	if( SyByteFind(zName,(sxu32)nName,0,0) == SXRET_OK ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #1 ($filename) must not contain any null bytes",
			&pCtx->pFunc->sName);
	}
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	if( pOld ){
		if( pOld->bOpen ){
			ZipCommit(pOld);
			pOld->bOpen = 0;
		}
		ZipAttach(pThis,0);
		ZipRelease(pOld);
	}
	PH7_VfsExpandPath(pCtx,zName,nName,&sPath);
	pZip = ZipNew(pCtx->pVm);
	if( pZip == 0 ){
		SyBlobRelease(&sPath);
		return PH7_ContextMemoryError(pCtx);
	}
	SyBlobAppend(&pZip->sPath,SyBlobData(&sPath),SyBlobLength(&sPath));
	SyBlobRelease(&sPath);
	SyBlobNullAppend(&pZip->sPath);
	pZip->bRdonly = (iFlags & ZIP_OPEN_RDONLY) != 0;
	{
		const char *zPath = (const char *)SyBlobData(&pZip->sPath);
		int bExists,iWhy;
		errno = 0;
		bExists = pVfs && pVfs->xFileExists && pVfs->xFileExists(zPath) == PH7_OK;
		iWhy = errno;
		if( pVfs && pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK ){
			/* A directory is not an archive and never could be: php's answer is
			 * the one it gives to any source it cannot use at all. */
			rc = ZIP_ER_OPNOTSUPP;
			goto failed;
		}
		if( bExists && (iFlags & ZIP_OPEN_EXCL) && (iFlags & ZIP_OPEN_CREATE) ){
			rc = ZIP_ER_EXISTS;
			goto failed;
		}
		if( !bExists ){
			if( iWhy != 0 && iWhy != ENOENT ){
				/* The name could not be LOOKED at -- a directory on the way to
				 * it refuses to be searched. php answers the reason it could
				 * not read rather than "no such file", whatever the flags say:
				 * a CREATE cannot help either. */
				rc = ZIP_ER_READ;
				goto failed;
			}
			if( (iFlags & ZIP_OPEN_CREATE) == 0 ){
				rc = ZIP_ER_NOENT;
				goto failed;
			}
			pZip->bCreated = 1;
		}else if( iFlags & ZIP_OPEN_OVERWRITE ){
			/* The file is there and its contents are not: php opens an EMPTY
			 * archive over it, and a close with nothing added removes it. */
		}else{
			const ph7_io_stream *pStream;
			void *pHandle;
			const char *zTail = zPath;
			pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTail,(int)SyBlobLength(&pZip->sPath));
			pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zTail,
				PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,0) : 0;
			if( pHandle == 0 ){
				rc = ZIP_ER_READ;
				goto failed;
			}
			PH7_StreamReadWholeFile(pHandle,pStream,&pZip->sFile);
			PH7_StreamCloseHandle(pStream,pHandle);
			rc = ZipParse(pZip);
			if( rc == ZIP_ER_EMPTY_FILE ){
				ph7_context_throw_error_format(pCtx,8192 /* E_DEPRECATED */,
					"Using empty file as ZipArchive is deprecated");
			}else if( rc != ZIP_ER_OK ){
				goto failed;
			}
		}
	}
	if( ZipAttach(pThis,pZip) != 0 ){
		ZipRelease(pZip);
		return PH7_ContextMemoryError(pCtx);
	}
	pZip->iLastId = -1;
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
failed:
	/* An open that fails leaves the object exactly as it was -- it does not
	 * even record the code in `status`, which is why the RETURN is the only
	 * place the reason appears. */
	ZipRelease(pZip);
	ZipRefresh(pThis,0);
	ph7_result_int(pCtx,rc);
	return PH7_OK;
}
/*
 * `close()`. It is the only place an archive is written, and a failure is BOTH
 * a warning and a false -- php names the sentence libzip left behind. The
 * archive goes either way: a script cannot retry a close.
 */
static int vm_builtin_ZipArchive_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	int rc;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	rc = pZip->bRdonly ? ZIP_ER_OK : ZipCommit(pZip);
	pZip->bOpen = 0;
	if( rc == ZIP_ER_OK ){
		/* A close that WORKED clears the archive's error, which is the one
		 * place php's otherwise-sticky `status` goes back to zero on its own. */
		pZip->iStatus = 0;
		pZip->iStatusSys = 0;
	}
	if( rc != ZIP_ER_OK ){
		SyBlob sMsg;
		pZip->iStatus = rc;
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		ZipErrText(rc,pZip->iStatusSys,&sMsg);
		SyBlobNullAppend(&sMsg);
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",
			(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
	}
	/* The six slots go back to what a fresh object shows -- except the two the
	 * failure just wrote, which php leaves on the object to be read. */
	ZipRefresh(pThis,0);
	ZipSetPropInt(pThis,"status",pZip->iStatus);
	ZipSetPropInt(pThis,"statusSys",pZip->iStatusSys);
	ZipAttach(pThis,0);
	ZipRelease(pZip);
	ph7_result_bool(pCtx,rc == ZIP_ER_OK);
	return PH7_OK;
}
/* php's Countable half: the INDEX SPACE, deleted entries included, which is the
 * same number `numFiles` answers. */
static int vm_builtin_ZipArchive_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(ph7_int64)pZip->nEnt);
	return PH7_OK;
}
/*
 * `getStatusString()`. The one verb that does NOT insist on a live archive:
 * php answers "No error" for an object nobody ever opened, and keeps answering
 * for one whose close() failed -- which is the only way to read why it did.
 */
static int vm_builtin_ZipArchive_getStatusString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_zip *pZip = ZipOfInstance(pThis);
	SyBlob sMsg;
	int iErr = 0,iSys = 0;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pZip && pZip->bOpen ){
		iErr = pZip->iStatus;
		iSys = pZip->iStatusSys;
	}else if( pThis ){
		iErr = (int)PH7_NativeAttrInt(pThis,"status");
		iSys = (int)PH7_NativeAttrInt(pThis,"statusSys");
	}
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	ZipErrText(iErr,iSys,&sMsg);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sMsg),(int)SyBlobLength(&sMsg));
	SyBlobRelease(&sMsg);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_clearError(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pZip->iStatus = 0;
	pZip->iStatusSys = 0;
	ZipRefresh(pThis,pZip);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* Adding, replacing, removing                                         */
/* ------------------------------------------------------------------ */
/*
 * The slot an add is going to use: an existing entry when the caller asked for
 * an overwrite, and a fresh index otherwise. php's default flags for the two
 * add doors are FL_OVERWRITE, so `addFromString()` on a name already there
 * REPLACES it and only an explicit `0` refuses with ER_EXISTS.
 */
static phl_zip_ent * ZipAddSlot(ph7_context *pCtx,phl_zip *pZip,const char *zName,
	sxu32 nName,int iFlags,int *pRc)
{
	sxu32 nIdx = 0;
	phl_zip_ent *pEnt = ZipFind(pZip,zName,nName,0,&nIdx);
	*pRc = PH7_OK;
	if( pEnt ){
		if( (iFlags & ZIP_FL_OVERWRITE) == 0 ){
			/* php stores whatever the library's add ANSWERED, and a refused one
			 * answers -1: `lastId` is not the last successful index but the
			 * last answer, so a duplicate add leaves it at -1. */
			pZip->iLastId = -1;
			*pRc = ZipFail(pCtx,pZip,ZIP_ER_EXISTS);
			return 0;
		}
		SyBlobReset(&pEnt->sData);
		SyBlobReset(&pEnt->sSrcPath);
		pEnt->iSrcStart = 0;
		pEnt->iSrcLen = -1;
		pEnt->bDataChanged = 1;
		pEnt->bLoaded = 0;
		pZip->iLastId = (int)nIdx;
		return pEnt;
	}
	pEnt = ZipEntNew(pZip,zName,(int)nName);
	if( pEnt == 0 ){
		*pRc = ZipFail(pCtx,pZip,ZIP_ER_MEMORY);
		return 0;
	}
	pEnt->bNew = 1;
	pEnt->iTime = (sxi64)time(0);
	pEnt->iOpsys = ZIP_OPSYS_UNIX;
	pEnt->nAttr = ZIP_ATTR_FILE;
	pZip->iLastId = (int)(pZip->nEnt - 1);
	return pEnt;
}
static int vm_builtin_ZipArchive_addFromString(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	phl_zip_ent *pEnt;
	const char *zName,*zData;
	int nName = 0,nData = 0,iFlags = ZIP_FL_OVERWRITE,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	zData = ph7_value_to_string(apArg[1],&nData);
	if( nArg > 2 ){
		iFlags = (int)ph7_value_to_int(apArg[2]);
	}
	pEnt = ZipAddSlot(pCtx,pZip,zName,ZipCName(zName,nName),iFlags,&rc);
	if( pEnt == 0 ){
		return rc;
	}
	if( nData > 0 ){
		SyBlobAppend(&pEnt->sData,zData,(sxu32)nData);
	}
	pEnt->nSize = (sxu32)(nData > 0 ? nData : 0);
	pEnt->nCompSize = pEnt->nSize;
	pEnt->nCrc = 0;
	pEnt->iMethod = ZIP_CM_STORE;
	pEnt->bDir = 0;
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * `addEmptyDir()`. php stores a directory as a zero-length entry whose name
 * ends in `/` -- and that slash is part of the name every reader answers with,
 * so `addEmptyDir('sub')` is `getNameIndex()` "sub/".
 */
static int vm_builtin_ZipArchive_addEmptyDir(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	phl_zip_ent *pEnt;
	const char *zName;
	SyBlob sName;
	int nName = 0,iFlags = 0,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	if( nName < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sName,&pCtx->pVm->sAllocator);
	nName = (int)ZipCName(zName,nName);
	if( nName < 1 ){
		SyBlobRelease(&sName);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobAppend(&sName,zName,(sxu32)nName);
	if( zName[nName-1] != '/' ){
		SyBlobAppend(&sName,"/",sizeof(char));
	}
	pEnt = ZipAddSlot(pCtx,pZip,(const char *)SyBlobData(&sName),SyBlobLength(&sName),
		iFlags,&rc);
	SyBlobRelease(&sName);
	if( pEnt == 0 ){
		return rc;
	}
	pEnt->bDir = 1;
	pEnt->nSize = 0;
	pEnt->nCompSize = 0;
	pEnt->iMethod = ZIP_CM_STORE;
	if( pEnt->bNew ){
		pEnt->nAttr = ZIP_ATTR_DIR;
	}
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * `addFile()` and `replaceFile()`, which are the same verb told the slot two
 * different ways. php reads the FILE at close() rather than here -- what is
 * recorded now is the path and the slice -- so it only has to prove the name
 * can be opened, and reports a name that cannot with the system's own sentence.
 */
static int ZipAddFileImpl(ph7_context *pCtx,int nArg,ph7_value **apArg,int bReplace)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	phl_zip_ent *pEnt;
	const char *zPath;
	SyBlob sFull;
	int nPath = 0,rc;
	sxi64 iStart = 0,iLen = 0;
	int iFlags = bReplace ? 0 : ZIP_FL_OVERWRITE;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"filepath",&rc) ){
		return rc;
	}
	zPath = ph7_value_to_string(apArg[0],&nPath);
	if( nArg > 2 ){
		iStart = ph7_value_to_int64(apArg[2]);
	}
	if( nArg > 3 ){
		iLen = ph7_value_to_int64(apArg[3]);
	}
	if( nArg > 4 ){
		iFlags = (int)ph7_value_to_int(apArg[4]);
	}
	PH7_VfsExpandPath(pCtx,zPath,nPath,&sFull);
	if( pVfs == 0 || pVfs->xFileExists == 0
	 || pVfs->xFileExists((const char *)SyBlobData(&sFull)) != PH7_OK ){
		SyBlobRelease(&sFull);
		errno = 2 /* ENOENT */;
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",VfsStrerror(errno));
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( bReplace ){
		pEnt = ZipArgIndex(pCtx,pZip,apArg[1],&rc);
		if( pEnt == 0 ){
			SyBlobRelease(&sFull);
			return rc;
		}
		SyBlobReset(&pEnt->sData);
		SyBlobReset(&pEnt->sSrcPath);
		pEnt->bDataChanged = 1;
		pEnt->bLoaded = 0;
	}else{
		const char *zEnt = zPath;
		int nEnt = nPath;
		if( nArg > 1 ){
			int n = 0;
			const char *z = ph7_value_to_string(apArg[1],&n);
			if( n > 0 ){
				zEnt = z;
				nEnt = n;
			}
		}
		pEnt = ZipAddSlot(pCtx,pZip,zEnt,ZipCName(zEnt,nEnt),iFlags,&rc);
		if( pEnt == 0 ){
			SyBlobRelease(&sFull);
			return rc;
		}
	}
	SyBlobAppend(&pEnt->sSrcPath,SyBlobData(&sFull),SyBlobLength(&sFull));
	SyBlobNullAppend(&pEnt->sSrcPath);
	pEnt->iSrcStart = iStart;
	pEnt->iSrcLen = iLen > 0 ? iLen : -1;
	pEnt->bDir = 0;
	{
		/* php reports the file's size for an entry it has not written yet, and
		 * the slice's when it was asked for one. */
		ph7_int64 iSize = pVfs->xFileSize
			? pVfs->xFileSize((const char *)SyBlobData(&sFull)) : 0;
		if( iSize < 0 ){
			iSize = 0;
		}
		if( iStart > 0 ){
			iSize = iStart < iSize ? iSize - iStart : 0;
		}
		if( iLen > 0 && iLen < iSize ){
			iSize = iLen;
		}
		pEnt->nSize = (sxu32)iSize;
		pEnt->nCompSize = pEnt->nSize;
		pEnt->nCrc = 0;
		pEnt->iMethod = ZIP_CM_STORE;
	}
	SyBlobRelease(&sFull);
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_addFile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZipAddFileImpl(pCtx,nArg,apArg,0);
}
static int vm_builtin_ZipArchive_replaceFile(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ZipAddFileImpl(pCtx,nArg,apArg,1);
}
/* `deleteIndex()` / `deleteName()`. The index stays: only close() compacts. */
static int ZipDeleteEnt(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt)
{
	pEnt->bDeleted = 1;
	pEnt->bNameHidden = 1;
	ZipRefresh(PH7_ContextThis(pCtx),pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_deleteIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipDeleteEnt(pCtx,pZip,pEnt) : rc;
}
static int vm_builtin_ZipArchive_deleteName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	return pEnt ? ZipDeleteEnt(pCtx,pZip,pEnt) : rc;
}
/* `renameIndex()` / `renameName()`. A rename onto a name already in the archive
 * is ER_EXISTS; a rename onto its OWN name is a no-op that answers true. */
static int ZipRenameEnt(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pNew)
{
	int nNew = 0;
	const char *zNew = ph7_value_to_string(pNew,&nNew);
	phl_zip_ent *pClash;
	nNew = (int)ZipCName(zNew,nNew);
	if( nNew < 1 ){
		return ZipFail(pCtx,pZip,ZIP_ER_INVAL);
	}
	/* The trailing slash IS the entry's kind, so a rename may not change it:
	 * a file renamed to a name ending in `/`, or a directory to one that does
	 * not, is php's `Invalid argument` rather than a rename that would leave
	 * an archive describing itself wrongly. */
	if( (zNew[nNew-1] == '/') != (pEnt->bDir ? 1 : 0) ){
		return ZipFail(pCtx,pZip,ZIP_ER_INVAL);
	}
	pClash = ZipFind(pZip,zNew,(sxu32)nNew,0,0);
	if( pClash && pClash != pEnt ){
		return ZipFail(pCtx,pZip,ZIP_ER_EXISTS);
	}
	SyBlobReset(&pEnt->sName);
	SyBlobAppend(&pEnt->sName,zNew,(sxu32)nNew);
	pEnt->bNameChanged = 1;
	ZipRefresh(PH7_ContextThis(pCtx),pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_renameIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[1],2,"new_name",&rc) ){
		return rc;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipRenameEnt(pCtx,pZip,pEnt,apArg[1]) : rc;
}
static int vm_builtin_ZipArchive_renameName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[1],2,"new_name",&rc) ){
		return rc;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	return pEnt ? ZipRenameEnt(pCtx,pZip,pEnt,apArg[1]) : rc;
}
/* ------------------------------------------------------------------ */
/* Reading the table                                                   */
/* ------------------------------------------------------------------ */
/*
 * `statIndex()` / `statName()`: php's eight-key description of one entry.
 *
 * An entry that has not been WRITTEN yet -- one an add put there this session,
 * or one whose bytes were replaced -- describes what it will be made of rather
 * than what it is: `comp_size` equals `size`, `crc` is 0 and `comp_method` is
 * 0, whatever it will be compressed with. Nothing has been compressed yet, and
 * libzip answers from the source rather than from a guess.
 */
static int ZipStatEnt(ph7_context *pCtx,phl_zip_ent *pEnt,sxu32 nIdx,int iFlags)
{
	ph7_value *pArray,*pVal;
	sxu32 nName;
	const char *zName = ZipEntName(pEnt,iFlags,&nName);
	int bFresh = pEnt->bNew || pEnt->bDataChanged;
	if( zName == 0 ){
		/* FL_UNCHANGED asked about an entry that has no original: php has
		 * nothing to describe and answers false rather than an empty name. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_value_string(pVal,zName,(int)nName);
	ph7_array_add_strkey_elem(pArray,"name",pVal);
	ph7_value_reset_string_cursor(pVal);
	ph7_value_int64(pVal,(ph7_int64)nIdx);
	ph7_array_add_strkey_elem(pArray,"index",pVal);
	ph7_value_int64(pVal,(ph7_int64)(bFresh ? 0 : pEnt->nCrc));
	ph7_array_add_strkey_elem(pArray,"crc",pVal);
	ph7_value_int64(pVal,(ph7_int64)pEnt->nSize);
	ph7_array_add_strkey_elem(pArray,"size",pVal);
	ph7_value_int64(pVal,(ph7_int64)pEnt->iTime);
	ph7_array_add_strkey_elem(pArray,"mtime",pVal);
	ph7_value_int64(pVal,(ph7_int64)(bFresh ? pEnt->nSize : pEnt->nCompSize));
	ph7_array_add_strkey_elem(pArray,"comp_size",pVal);
	ph7_value_int64(pVal,(ph7_int64)(bFresh ? ZIP_CM_STORE : pEnt->iMethod));
	ph7_array_add_strkey_elem(pArray,"comp_method",pVal);
	/* A pending setEncryption() is REPORTED, the way a pending compression
	 * method is not: an entry whose bytes are already written can be told to
	 * change cipher, and php answers the one it will have. An entry that has no
	 * bytes yet answers 0 with the rest of its unwritten description. */
	ph7_value_int64(pVal,(ph7_int64)(bFresh ? ZIP_EM_NONE
		: (pEnt->iSetEncrypt >= 0 ? pEnt->iSetEncrypt : pEnt->iEncMethod)));
	ph7_array_add_strkey_elem(pArray,"encryption_method",pVal);
	ph7_result_value(pCtx,pArray);
	ph7_context_release_value(pCtx,pVal);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_statIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int iFlags = 0,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	if( pEnt == 0 ){
		return rc;
	}
	return ZipStatEnt(pCtx,pEnt,(sxu32)ph7_value_to_int64(apArg[0]),iFlags);
}
static int vm_builtin_ZipArchive_statName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	const char *zName;
	sxu32 nIdx = 0;
	int nName = 0,iFlags = 0;
	int rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipNulName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	pEnt = ZipFind(pZip,zName,(sxu32)(nName < 0 ? 0 : nName),iFlags,&nIdx);
	if( pEnt == 0 ){
		return ZipFail(pCtx,pZip,ZIP_ER_NOENT);
	}
	return ZipStatEnt(pCtx,pEnt,nIdx,iFlags);
}
static int vm_builtin_ZipArchive_locateName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	const char *zName;
	sxu32 nIdx = 0;
	int nName = 0,iFlags = 0;
	int rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipNulName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	zName = ph7_value_to_string(apArg[0],&nName);
	pEnt = ZipFind(pZip,zName,(sxu32)(nName < 0 ? 0 : nName),iFlags,&nIdx);
	if( pEnt == 0 ){
		return ZipFail(pCtx,pZip,ZIP_ER_NOENT);
	}
	ph7_result_int64(pCtx,(ph7_int64)nIdx);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_getNameIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	const char *zName;
	sxu32 nName = 0;
	int iFlags = 0,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	if( pEnt == 0 ){
		return rc;
	}
	zName = ZipEntName(pEnt,iFlags,&nName);
	if( zName == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,zName,(int)nName);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* Comments, times, attributes, compression                            */
/* ------------------------------------------------------------------ */
static int vm_builtin_ZipArchive_setArchiveComment(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	const char *z;
	int n = 0;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	z = ph7_value_to_string(apArg[0],&n);
	if( n > 0 && (int)ZipCName(z,n) != n ){
		/* The archive comment is the one string php refuses a NUL in outright:
		 * an entry NAME is truncated at one and this answers false. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobReset(&pZip->sComment);
	if( n > 0 ){
		SyBlobAppend(&pZip->sComment,z,(sxu32)n);
	}
	/* Only a comment that really DIFFERS is a change: php rewrites nothing for
	 * a setArchiveComment() that sets what was already there. */
	if( SyBlobLength(&pZip->sComment) != SyBlobLength(&pZip->sOrigComment)
	 || (SyBlobLength(&pZip->sComment) > 0
	     && SyMemcmp(SyBlobData(&pZip->sComment),SyBlobData(&pZip->sOrigComment),
	                 SyBlobLength(&pZip->sComment)) != 0) ){
		pZip->bCommentChanged = 1;
	}
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_getArchiveComment(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	SyBlob *pB;
	int iFlags = 0;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( nArg > 0 ){
		iFlags = (int)ph7_value_to_int(apArg[0]);
	}
	pB = (iFlags & ZIP_FL_UNCHANGED) ? &pZip->sOrigComment : &pZip->sComment;
	ph7_result_string(pCtx,(const char *)SyBlobData(pB),(int)SyBlobLength(pB));
	return PH7_OK;
}
/*
 * `setArchiveFlag()` / `getArchiveFlag()`. The one flag php exposes is
 * AFL_RDONLY, and libzip refuses to set it on an archive with UNWRITTEN
 * changes -- turning an archive read-only would strand them.
 */
static int vm_builtin_ZipArchive_setArchiveFlag(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	int iFlag,iValue;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	iFlag = (int)ph7_value_to_int(apArg[0]);
	iValue = (int)ph7_value_to_int(apArg[1]);
	if( (iFlag & ZIP_AFL_RDONLY) && iValue ){
		sxu32 i;
		for( i = 0 ; i < pZip->nEnt ; ++i ){
			phl_zip_ent *pEnt = pZip->apEnt[i];
			if( pEnt->bNew || pEnt->bDeleted || pEnt->bDataChanged
			 || pEnt->bNameChanged || pEnt->bCommentChanged
			 || pEnt->bTimeChanged || pEnt->bAttrChanged ){
				return ZipFail(pCtx,pZip,ZIP_ER_CHANGED);
			}
		}
		if( pZip->bCommentChanged ){
			return ZipFail(pCtx,pZip,ZIP_ER_CHANGED);
		}
	}
	if( iValue ){
		pZip->iArchiveFlags |= iFlag;
	}else{
		pZip->iArchiveFlags &= ~iFlag;
	}
	if( iFlag & ZIP_AFL_RDONLY ){
		pZip->bRdonly = iValue ? 1 : (sxu8)0;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_getArchiveFlag(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	int iFlag;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	iFlag = (int)ph7_value_to_int(apArg[0]);
	ph7_result_int(pCtx,(pZip->iArchiveFlags & iFlag) ? iFlag : 0);
	return PH7_OK;
}
static int ZipSetComment(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pVal)
{
	int n = 0;
	const char *z = ph7_value_to_string(pVal,&n);
	SyBlobReset(&pEnt->sComment);
	if( n > 0 ){
		SyBlobAppend(&pEnt->sComment,z,(sxu32)n);
	}
	pEnt->bCommentChanged = 1;
	ZipRefresh(PH7_ContextThis(pCtx),pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_setCommentIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipSetComment(pCtx,pZip,pEnt,apArg[1]) : rc;
}
static int vm_builtin_ZipArchive_setCommentName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	return pEnt ? ZipSetComment(pCtx,pZip,pEnt,apArg[1]) : rc;
}
static int ZipGetComment(ph7_context *pCtx,phl_zip_ent *pEnt,int iFlags)
{
	SyBlob *pB = (iFlags & ZIP_FL_UNCHANGED) ? &pEnt->sOrigComment : &pEnt->sComment;
	ph7_result_string(pCtx,(const char *)SyBlobData(pB),(int)SyBlobLength(pB));
	return PH7_OK;
}
static int vm_builtin_ZipArchive_getCommentIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int iFlags = 0,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipGetComment(pCtx,pEnt,iFlags) : rc;
}
static int vm_builtin_ZipArchive_getCommentName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int iFlags = 0,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	return pEnt ? ZipGetComment(pCtx,pEnt,iFlags) : rc;
}
static int ZipSetMtime(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pVal)
{
	pEnt->iTime = ph7_value_to_int64(pVal);
	pEnt->bTimeChanged = 1;
	ZipRefresh(PH7_ContextThis(pCtx),pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_setMtimeIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipSetMtime(pCtx,pZip,pEnt,apArg[1]) : rc;
}
static int vm_builtin_ZipArchive_setMtimeName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	return pEnt ? ZipSetMtime(pCtx,pZip,pEnt,apArg[1]) : rc;
}
/*
 * The external attributes, which on the unix side ARE the mode: php hands back
 * the raw 32-bit word, and a script that wants permissions shifts it right
 * sixteen. An entry nobody set them on carries libzip's own 0100666 (0040777
 * for a directory), whatever the file it came from was.
 */
static int ZipSetExtAttr(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,
	ph7_value *pOpsys,ph7_value *pAttr)
{
	pEnt->iOpsys = (int)ph7_value_to_int(pOpsys);
	pEnt->nAttr = (sxu32)ph7_value_to_int64(pAttr);
	pEnt->bAttrChanged = 1;
	ZipRefresh(PH7_ContextThis(pCtx),pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_setExternalAttributesIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipSetExtAttr(pCtx,pZip,pEnt,apArg[1],apArg[2]) : rc;
}
static int vm_builtin_ZipArchive_setExternalAttributesName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	return pEnt ? ZipSetExtAttr(pCtx,pZip,pEnt,apArg[1],apArg[2]) : rc;
}
static int ZipGetExtAttr(ph7_context *pCtx,phl_zip_ent *pEnt,ph7_value *pOpsys,ph7_value *pAttr)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value sVal;
	PH7_MemObjInit(pVm,&sVal);
	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)pEnt->iOpsys);
	PH7_VmStoreArgByRef(pVm,pOpsys,&sVal);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)pEnt->nAttr);
	PH7_VmStoreArgByRef(pVm,pAttr,&sVal);
	PH7_MemObjRelease(&sVal);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_getExternalAttributesIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipGetExtAttr(pCtx,pEnt,apArg[1],apArg[2]) : rc;
}
static int vm_builtin_ZipArchive_getExternalAttributesName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	return pEnt ? ZipGetExtAttr(pCtx,pEnt,apArg[1],apArg[2]) : rc;
}
/*
 * `setCompressionIndex()` / `setCompressionName()`. A method this build cannot
 * write is refused HERE rather than at close, with php's own ER_COMPNOTSUPP --
 * which is also what a libzip built without bzip2 answers.
 */
static int ZipSetCompression(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pVal)
{
	int iMethod = (int)ph7_value_to_int(pVal);
	if( iMethod != ZIP_CM_DEFAULT && iMethod != ZIP_CM_STORE && iMethod != ZIP_CM_DEFLATE ){
		return ZipFail(pCtx,pZip,ZIP_ER_COMPNOTSUPP);
	}
	pEnt->iSetMethod = iMethod;
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_setCompressionIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipSetCompression(pCtx,pZip,pEnt,apArg[1]) : rc;
}
static int vm_builtin_ZipArchive_setCompressionName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	return pEnt ? ZipSetCompression(pCtx,pZip,pEnt,apArg[1]) : rc;
}
/* ------------------------------------------------------------------ */
/* Putting a change back                                               */
/* ------------------------------------------------------------------ */
/*
 * One entry's changes, undone. What comes back is the NAME, the comment, the
 * time and the attributes -- and, for an entry that was DELETED, the entry
 * itself, but not its name: libzip drops a deleted name from the hash it looks
 * names up in and only `unchangeAll()` puts that hash back, so an entry
 * restored this way answers `statIndex()` and not `statName()`. Reproduced
 * because it is what a program sees.
 *
 * An entry that was ADDED has no original to go back to, so unchanging it
 * REMOVES it -- while leaving the index it took, which `numFiles` still counts.
 */
static void ZipUnchangeEnt(phl_zip_ent *pEnt,int bAll)
{
	if( pEnt->bNew ){
		pEnt->bDeleted = 1;
		pEnt->bNameHidden = 1;
		return;
	}
	SyBlobReset(&pEnt->sName);
	SyBlobAppend(&pEnt->sName,SyBlobData(&pEnt->sOrigName),SyBlobLength(&pEnt->sOrigName));
	SyBlobReset(&pEnt->sComment);
	SyBlobAppend(&pEnt->sComment,SyBlobData(&pEnt->sOrigComment),
		SyBlobLength(&pEnt->sOrigComment));
	SyBlobReset(&pEnt->sData);
	SyBlobReset(&pEnt->sSrcPath);
	pEnt->iSrcStart = 0;
	pEnt->iSrcLen = -1;
	pEnt->iTime = pEnt->iOrigTime;
	pEnt->nAttr = pEnt->nOrigAttr;
	pEnt->iOpsys = pEnt->iOrigOpsys;
	pEnt->iSetMethod = ZIP_CM_DEFAULT;
	pEnt->iSetEncrypt = -1;
	pEnt->bHasPassword = 0;
	SyBlobReset(&pEnt->sPassword);
	pEnt->bLoaded = 0;
	pEnt->bDataChanged = 0;
	pEnt->bNameChanged = 0;
	pEnt->bCommentChanged = 0;
	pEnt->bTimeChanged = 0;
	pEnt->bAttrChanged = 0;
	pEnt->bDeleted = 0;
	pEnt->bDir = SyBlobLength(&pEnt->sName) > 0
		&& ((const char *)SyBlobData(&pEnt->sName))[SyBlobLength(&pEnt->sName)-1] == '/';
	if( bAll ){
		pEnt->bNameHidden = 0;
	}
}
static int vm_builtin_ZipArchive_unchangeAll(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	sxu32 i;
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	for( i = 0 ; i < pZip->nEnt ; ++i ){
		ZipUnchangeEnt(pZip->apEnt[i],1);
	}
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* php's `unchangeArchive()` is NOT the same verb: it puts back what belongs to
 * the ARCHIVE -- its comment -- and leaves every entry alone, a deleted one
 * included. */
static int vm_builtin_ZipArchive_unchangeArchive(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	SXUNUSED(nArg); SXUNUSED(apArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	SyBlobReset(&pZip->sComment);
	SyBlobAppend(&pZip->sComment,SyBlobData(&pZip->sOrigComment),
		SyBlobLength(&pZip->sOrigComment));
	pZip->bCommentChanged = 0;
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_unchangeIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	phl_zip_ent *pEnt;
	sxi64 iIdx;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	iIdx = ph7_value_to_int64(apArg[0]);
	if( iIdx < 0 || (sxu64)iIdx >= (sxu64)pZip->nEnt ){
		return ZipFail(pCtx,pZip,ZIP_ER_INVAL);
	}
	pEnt = pZip->apEnt[(sxu32)iIdx];
	ZipUnchangeEnt(pEnt,0);
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_unchangeName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis;
	phl_zip *pZip = ZipThis(pCtx,&pThis);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	if( pEnt == 0 ){
		return rc;
	}
	ZipUnchangeEnt(pEnt,0);
	ZipRefresh(pThis,pZip);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* Reading an entry's bytes                                            */
/* ------------------------------------------------------------------ */
static int ZipGetFrom(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pLen)
{
	sxi64 iLen = pLen ? ph7_value_to_int64(pLen) : 0;
	sxu32 nWant;
	int rc = ZipEntLoad(pZip,pEnt);
	if( rc != ZIP_ER_OK ){
		return ZipFail(pCtx,pZip,rc);
	}
	nWant = SyBlobLength(&pEnt->sData);
	if( iLen > 0 && (sxu64)iLen < (sxu64)nWant ){
		nWant = (sxu32)iLen;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pEnt->sData),(int)nWant);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_getFromIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipGetFrom(pCtx,pZip,pEnt,nArg > 1 ? apArg[1] : 0) : rc;
}
static int vm_builtin_ZipArchive_getFromName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int iFlags = 0,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipNulName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	if( nArg > 2 ){
		iFlags = (int)ph7_value_to_int(apArg[2]);
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],iFlags,&rc);
	return pEnt ? ZipGetFrom(pCtx,pZip,pEnt,nArg > 1 ? apArg[1] : 0) : rc;
}
/* ------------------------------------------------------------------ */
/* Extracting                                                          */
/* ------------------------------------------------------------------ */
/*
 * Where one entry lands under a destination directory.
 *
 * php resolves the entry name against the destination the way a path is
 * resolved against a root: `.` and empty segments go, `..` pops, and a pop at
 * the root is a pop of nothing -- so `../evil.txt` extracts to `evil.txt`
 * INSIDE the destination and `/abs.txt` to `abs.txt`, which is what keeps an
 * archive from writing outside where it was told to.
 *
 * Answers 0 when the name collapses to nothing at all, which is not a file.
 */
static int ZipExtractPath(ph7_vm *pVm,const char *zBase,sxu32 nBase,
	const char *zName,sxu32 nName,SyBlob *pOut,SyBlob *pDir)
{
	sxu32 n;
	sxu32 nRoot;
	SyBlobInit(pOut,&pVm->sAllocator);
	SyBlobInit(pDir,&pVm->sAllocator);
	SyBlobAppend(pOut,zBase,nBase);
	while( SyBlobLength(pOut) > 1
	 && (((const char *)SyBlobData(pOut))[SyBlobLength(pOut)-1] == '/'
	  || ((const char *)SyBlobData(pOut))[SyBlobLength(pOut)-1] == PH7_PATH_SEP) ){
		pOut->nByte--;
	}
	nRoot = SyBlobLength(pOut);
	for( n = 0 ; n < nName ; ){
		sxu32 nStart = n,nSeg;
		while( n < nName && zName[n] != '/'
#ifdef __WINNT__
			&& zName[n] != '\\'
#endif
		){
			++n;
		}
		nSeg = n - nStart;
		if( n < nName ){
			++n;
		}
		if( nSeg == 0 || (nSeg == 1 && zName[nStart] == '.') ){
			continue;
		}
		if( nSeg == 2 && zName[nStart] == '.' && zName[nStart+1] == '.' ){
			sxu32 nHave = SyBlobLength(pOut);
			while( nHave > nRoot && ((const char *)SyBlobData(pOut))[nHave-1] != '/' ){
				--nHave;
			}
			if( nHave > nRoot ){
				--nHave;
			}
			pOut->nByte = nHave;
			continue;
		}
		/* Whatever is in front of this segment is the DIRECTORY the entry needs
		 * to exist, which is the last thing recorded before the final one. */
		SyBlobReset(pDir);
		SyBlobAppend(pDir,SyBlobData(pOut),SyBlobLength(pOut));
		SyBlobAppend(pOut,"/",sizeof(char));
		SyBlobAppend(pOut,&zName[nStart],nSeg);
	}
	if( SyBlobLength(pOut) <= nRoot ){
		SyBlobRelease(pOut);
		SyBlobRelease(pDir);
		return 0;
	}
	SyBlobNullAppend(pOut);
	SyBlobNullAppend(pDir);
	return 1;
}
/*
 * Create a directory and every directory above it. php's extractor asks its
 * stream layer for a RECURSIVE mkdir; the VFS here has one door that makes a
 * single level, so the walk is spelled out. A component that is already there
 * is not a failure -- only the last one's absence would be, and the caller
 * finds that out when the file will not open.
 */
static void ZipMkdirAll(ph7_vm *pVm,SyBlob *pDir)
{
	const ph7_vfs *pVfs = pVm->pEngine->pVfs;
	char *z = (char *)SyBlobData(pDir);
	sxu32 n = SyBlobLength(pDir);
	sxu32 i;
	if( pVfs == 0 || pVfs->xMkdir == 0 || n < 1 ){
		return;
	}
	for( i = 1 ; i <= n ; ++i ){
		char c;
		if( i < n && z[i] != '/' && z[i] != PH7_PATH_SEP ){
			continue;
		}
		c = z[i];
		z[i] = 0;
		if( pVfs->xFileExists == 0 || pVfs->xFileExists(z) != PH7_OK ){
			pVfs->xMkdir(z,0777,FALSE);
		}
		z[i] = c;
	}
}
/* One entry onto the filesystem: the directories in front of it first, then
 * the bytes, then the modification time php restores and the mode it does not
 * (an extracted file gets the process umask, never the entry's attributes). */
static int ZipExtractOne(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,
	const char *zBase,sxu32 nBase)
{
	ph7_vm *pVm = pCtx->pVm;
	const ph7_vfs *pVfs = pVm->pEngine->pVfs;
	SyBlob sPath,sDir;
	const char *zPath;
	int rc;
	if( !ZipExtractPath(pVm,zBase,nBase,(const char *)SyBlobData(&pEnt->sName),
		SyBlobLength(&pEnt->sName),&sPath,&sDir) ){
		return 1;
	}
	zPath = (const char *)SyBlobData(&sPath);
	if( pEnt->bDir ){
		ZipMkdirAll(pVm,&sPath);
		SyBlobRelease(&sPath);
		SyBlobRelease(&sDir);
		return 1;
	}
	if( SyBlobLength(&sDir) > 0 ){
		ZipMkdirAll(pVm,&sDir);
	}
	rc = ZipEntLoad(pZip,pEnt);
	if( rc != ZIP_ER_OK ){
		pZip->iStatus = rc;
		SyBlobRelease(&sPath);
		SyBlobRelease(&sDir);
		return 0;
	}
	{
		const ph7_io_stream *pStream;
		const char *zDev = zPath;
		void *pHandle;
		pStream = PH7_VmGetStreamDevice(pVm,&zDev,(int)SyBlobLength(&sPath));
		pHandle = pStream ? PH7_StreamOpenHandle(pVm,pStream,zDev,
			PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,0) : 0;
		if( pHandle == 0 ){
			/* php's own sentence, with the whole destination path in it. */
			PH7_VmThrowWarningFmt(pVm,"%z(%s): Failed to open stream: %s",
				&pCtx->pFunc->sName,zPath,PH7_VfsOpenStrerror(errno));
			SyBlobRelease(&sPath);
			SyBlobRelease(&sDir);
			return 0;
		}
		if( pStream->xWrite && SyBlobLength(&pEnt->sData) > 0 ){
			pStream->xWrite(pHandle,SyBlobData(&pEnt->sData),
				(ph7_int64)SyBlobLength(&pEnt->sData));
		}
		PH7_StreamCloseHandle(pStream,pHandle);
	}
	if( pVfs && pVfs->xTouch ){
		pVfs->xTouch(zPath,(ph7_int64)pEnt->iTime,(ph7_int64)pEnt->iTime);
	}
	SyBlobRelease(&sPath);
	SyBlobRelease(&sDir);
	return 1;
}
static int vm_builtin_ZipArchive_extractTo(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	const char *zBase;
	int nBase = 0;
	sxu32 i;
	int rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipNulName(pCtx,apArg[0],1,"pathto",&rc) ){
		return rc;
	}
	zBase = ph7_value_to_string(apArg[0],&nBase);
	if( nBase < 1 ){
		return ZipFail(pCtx,pZip,ZIP_ER_INVAL);
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		/* php takes one name or a list of them, and a name it cannot find stops
		 * the whole extraction with ER_NOENT and no warning. */
		ph7_value *pList = apArg[1];
		if( ph7_value_is_array(pList) ){
			ph7_hashmap *pMap = (ph7_hashmap *)pList->x.pOther;
			ph7_hashmap_node *pNode;
			if( pMap == 0 || pMap->nEntry < 1 ){
				/* php refuses an EMPTY list outright rather than extracting
				 * nothing: `extractTo($d, [])` is false. */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			pMap->pCur = pMap->pFirst;
			while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){
				ph7_value sVal;
				phl_zip_ent *pEnt;
				const char *zName;
				int nName = 0;
				PH7_MemObjInit(pCtx->pVm,&sVal);
				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
				zName = ph7_value_to_string(&sVal,&nName);
				pEnt = ZipFind(pZip,zName,(sxu32)(nName < 0 ? 0 : nName),0,0);
				if( pEnt == 0 || !ZipExtractOne(pCtx,pZip,pEnt,zBase,(sxu32)nBase) ){
					if( pEnt == 0 ){
						pZip->iStatus = ZIP_ER_NOENT;
					}
					PH7_MemObjRelease(&sVal);
					ZipRefresh(PH7_ContextThis(pCtx),pZip);
					ph7_result_bool(pCtx,0);
					return PH7_OK;
				}
				PH7_MemObjRelease(&sVal);
			}
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}else{
			int nName = 0;
			const char *zName = ph7_value_to_string(pList,&nName);
			phl_zip_ent *pEnt = ZipFind(pZip,zName,(sxu32)(nName < 0 ? 0 : nName),0,0);
			if( pEnt == 0 ){
				return ZipFail(pCtx,pZip,ZIP_ER_NOENT);
			}
			ph7_result_bool(pCtx,ZipExtractOne(pCtx,pZip,pEnt,zBase,(sxu32)nBase));
			return PH7_OK;
		}
	}
	for( i = 0 ; i < pZip->nEnt ; ++i ){
		phl_zip_ent *pEnt = pZip->apEnt[i];
		if( pEnt->bDeleted ){
			continue;
		}
		if( !ZipExtractOne(pCtx,pZip,pEnt,zBase,(sxu32)nBase) ){
			ZipRefresh(PH7_ContextThis(pCtx),pZip);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* addGlob() and addPattern()                                          */
/* ------------------------------------------------------------------ */
/* One option out of the `$options` array both doors take. */
static int ZipOptStr(ph7_value *pOpt,const char *zKey,SyBlob *pOut)
{
	ph7_value *pVal;
	const char *z;
	int n = 0;
	if( pOpt == 0 || !ph7_value_is_array(pOpt) ){
		return 0;
	}
	pVal = ph7_array_fetch(pOpt,zKey,-1);
	if( pVal == 0 ){
		return 0;
	}
	z = ph7_value_to_string(pVal,&n);
	if( n > 0 ){
		SyBlobAppend(pOut,z,(sxu32)n);
	}
	return 1;
}
static int ZipOptBool(ph7_value *pOpt,const char *zKey)
{
	ph7_value *pVal;
	if( pOpt == 0 || !ph7_value_is_array(pOpt) ){
		return 0;
	}
	pVal = ph7_array_fetch(pOpt,zKey,-1);
	return pVal != 0 && ph7_value_to_bool(pVal) != 0;
}
static int ZipOptInt(ph7_value *pOpt,const char *zKey,int iDefault)
{
	ph7_value *pVal;
	if( pOpt == 0 || !ph7_value_is_array(pOpt) ){
		return iDefault;
	}
	pVal = ph7_array_fetch(pOpt,zKey,-1);
	return pVal ? (int)ph7_value_to_int(pVal) : iDefault;
}
/*
 * The entry name one matched PATH gets, under php's three path options:
 * `remove_all_path` keeps the basename only, `remove_path` strips a PREFIX,
 * and `add_path` is put in front of whatever is left. They are applied in that
 * order and the first two are exclusive.
 */
static void ZipGlobEntryName(ph7_value *pOpt,const char *zPath,sxu32 nPath,SyBlob *pOut)
{
	SyBlob sAdd,sRem;
	sxu32 nStart = 0;
	SyBlobInit(&sAdd,pOut->pAllocator);
	SyBlobInit(&sRem,pOut->pAllocator);
	if( ZipOptBool(pOpt,"remove_all_path") ){
		sxu32 i;
		for( i = 0 ; i < nPath ; ++i ){
			if( zPath[i] == '/' || zPath[i] == PH7_PATH_SEP ){
				nStart = i + 1;
			}
		}
	}else if( ZipOptStr(pOpt,"remove_path",&sRem) && SyBlobLength(&sRem) > 0
	       && SyBlobLength(&sRem) <= nPath
	       && SyMemcmp(zPath,SyBlobData(&sRem),SyBlobLength(&sRem)) == 0 ){
		nStart = SyBlobLength(&sRem);
	}
	if( ZipOptStr(pOpt,"add_path",&sAdd) && SyBlobLength(&sAdd) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&sAdd),SyBlobLength(&sAdd));
	}
	SyBlobAppend(pOut,&zPath[nStart],nPath - nStart);
	SyBlobRelease(&sAdd);
	SyBlobRelease(&sRem);
}
/*
 * The shared half of `addGlob()` and `addPattern()`: a list of PATHS, each
 * added under the name the options make of it. Directories are skipped -- php
 * stats every candidate and takes only regular files, which is also what keeps
 * `.` and `..` out of a pattern walk.
 *
 * The answer is the list of matched PATHS, not of entry names, and one add that
 * fails makes the whole call FALSE.
 */
static int ZipAddMatches(ph7_context *pCtx,phl_zip *pZip,SyBlob *pHit,SySet *pSet,
	const char *zDir,sxu32 nDir,ph7_value *pOpt,int iFlags)
{
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	ph7_value *pArray,*pVal;
	PH7_GlobHit *aHit = (PH7_GlobHit *)SySetBasePtr(pSet);
	sxu32 n,nHit = SySetUsed(pSet);
	int iMethod = ZipOptInt(pOpt,"comp_method",ZIP_CM_DEFAULT);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	for( n = 0 ; n < nHit ; ++n ){
		const char *zName = (const char *)SyBlobData(pHit) + aHit[n].nOfs;
		sxu32 nName = aHit[n].nLen;
		SyBlob sPath,sEntry;
		phl_zip_ent *pEnt;
		int rc;
		SyBlobInit(&sPath,&pCtx->pVm->sAllocator);
		if( nDir > 0 ){
			SyBlobAppend(&sPath,zDir,nDir);
			SyBlobAppend(&sPath,"/",sizeof(char));
		}
		SyBlobAppend(&sPath,zName,nName);
		SyBlobNullAppend(&sPath);
		if( pVfs == 0 || pVfs->xIsfile == 0
		 || pVfs->xIsfile((const char *)SyBlobData(&sPath)) != PH7_OK ){
			SyBlobRelease(&sPath);
			continue;
		}
		SyBlobInit(&sEntry,&pCtx->pVm->sAllocator);
		ZipGlobEntryName(pOpt,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath),&sEntry);
		pEnt = ZipAddSlot(pCtx,pZip,(const char *)SyBlobData(&sEntry),
			SyBlobLength(&sEntry),iFlags,&rc);
		SyBlobRelease(&sEntry);
		if( pEnt == 0 ){
			SyBlobRelease(&sPath);
			ph7_context_release_value(pCtx,pVal);
			ph7_context_release_value(pCtx,pArray);
			return rc;
		}
		SyBlobAppend(&pEnt->sSrcPath,SyBlobData(&sPath),SyBlobLength(&sPath));
		SyBlobNullAppend(&pEnt->sSrcPath);
		pEnt->iSrcStart = 0;
		pEnt->iSrcLen = -1;
		pEnt->bDir = 0;
		pEnt->iSetMethod = iMethod;
		{
			ph7_int64 iSize = pVfs->xFileSize
				? pVfs->xFileSize((const char *)SyBlobData(&sPath)) : 0;
			pEnt->nSize = (sxu32)(iSize > 0 ? iSize : 0);
			pEnt->nCompSize = pEnt->nSize;
			pEnt->iMethod = ZIP_CM_STORE;
		}
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));
		ph7_array_add_elem(pArray,0,pVal);
		SyBlobRelease(&sPath);
	}
	ZipRefresh(PH7_ContextThis(pCtx),pZip);
	ph7_result_value(pCtx,pArray);
	ph7_context_release_value(pCtx,pVal);
	return PH7_OK;
}
/*
 * `addGlob()`. Its `$flags` are **glob()'s**, not the FL_ ones every other door
 * takes -- `GLOB_BRACE`, `GLOB_NOSORT` and the rest -- so the expansion is
 * PHL's own `glob()` rather than a second walk of its own. That keeps the two
 * from drifting on the one thing this argument decides, and it costs a call
 * into php: the function is the prelude's, and its flag screen and brace
 * expansion are already php's.
 *
 * The only thing spelled here is the REFUSAL, because php's belongs to this
 * method and names it: `ZipArchive::addGlob(): At least one of the passed flags
 * is invalid...`, where the plain function would have said `glob(): …`.
 */
static int vm_builtin_ZipArchive_addGlob(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	ph7_value sName,sPat,sFlags,sRet;
	ph7_value *apCall[2];
	SyString sFn;
	const char *zPat;
	SyBlob sHit;
	SySet aHit;
	int nPat = 0,iFlags = 0,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	zPat = ph7_value_to_string(apArg[0],&nPat);
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	if( iFlags & ~ZIP_GLOB_FLAGS ){
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"At least one of the passed flags is invalid or not supported on this platform");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sName);
	PH7_MemObjInit(pCtx->pVm,&sPat);
	PH7_MemObjInit(pCtx->pVm,&sFlags);
	PH7_MemObjInit(pCtx->pVm,&sRet);
	SyStringInitFromBuf(&sFn,"glob",sizeof("glob")-1);
	PH7_MemObjInitFromString(pCtx->pVm,&sName,&sFn);
	ph7_value_string(&sPat,zPat,nPat);
	ph7_value_int(&sFlags,iFlags);
	apCall[0] = &sPat;
	apCall[1] = &sFlags;
	PH7_VmCallUserFunction(pCtx->pVm,&sName,2,apCall,&sRet);
	SyBlobInit(&sHit,&pCtx->pVm->sAllocator);
	SySetInit(&aHit,&pCtx->pVm->sAllocator,sizeof(PH7_GlobHit));
	if( ph7_value_is_array(&sRet) ){
		ph7_hashmap *pMap = (ph7_hashmap *)sRet.x.pOther;
		ph7_hashmap_node *pNode;
		if( pMap ){
			pMap->pCur = pMap->pFirst;
			while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){
				ph7_value sVal;
				const char *zHit;
				int nHit = 0;
				PH7_GlobHit sRec;
				PH7_MemObjInit(pCtx->pVm,&sVal);
				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
				zHit = ph7_value_to_string(&sVal,&nHit);
				if( nHit > 0 ){
					sRec.nOfs = SyBlobLength(&sHit);
					sRec.nLen = (sxu32)nHit;
					SyBlobAppend(&sHit,zHit,(sxu32)nHit);
					SySetPut(&aHit,(const void *)&sRec);
				}
				PH7_MemObjRelease(&sVal);
			}
		}
		rc = ZipAddMatches(pCtx,pZip,&sHit,&aHit,0,0,nArg > 2 ? apArg[2] : 0,0);
	}else{
		/* php's other refusal: the expansion itself failed. */
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"No such file or directory");
		ph7_result_bool(pCtx,0);
		rc = PH7_OK;
	}
	SyBlobRelease(&sHit);
	SySetRelease(&aHit);
	PH7_MemObjRelease(&sRet);
	PH7_MemObjRelease(&sFlags);
	PH7_MemObjRelease(&sPat);
	PH7_MemObjRelease(&sName);
	return rc;
}
static int vm_builtin_ZipArchive_addPattern(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	const char *zPat,*zDir = ".";
	SyBlob sHit,sKeep;
	SySet aHit,aKeep;
	int nPat = 0,nDir = 1,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	zPat = ph7_value_to_string(apArg[0],&nPat);
	if( nArg > 1 ){
		int n = 0;
		const char *z = ph7_value_to_string(apArg[1],&n);
		if( n > 0 ){
			zDir = z;
			nDir = n;
		}
	}
	SyBlobInit(&sHit,&pCtx->pVm->sAllocator);
	SySetInit(&aHit,&pCtx->pVm->sAllocator,sizeof(PH7_GlobHit));
	SyBlobInit(&sKeep,&pCtx->pVm->sAllocator);
	SySetInit(&aKeep,&pCtx->pVm->sAllocator,sizeof(PH7_GlobHit));
	if( PH7_VfsListDir(pCtx->pVm,zDir,nDir,&sHit,&aHit) == SXRET_OK ){
		PH7_GlobHit *aRec = (PH7_GlobHit *)SySetBasePtr(&aHit);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(&aHit) ; ++n ){
			const char *zName = (const char *)SyBlobData(&sHit) + aRec[n].nOfs;
			PH7_GlobHit sKept;
			int bMatch = 0;
#ifdef PH7_ENABLE_PCRE
			/* php matches the pattern against the ENTRY name, never against the
			 * path it will be added under. */
			if( PH7_PcreMatchQuiet(pCtx,zPat,nPat,zName,(int)aRec[n].nLen,&bMatch)
				!= SXRET_OK ){
				bMatch = 0;
			}
#else
			SXUNUSED(zPat); SXUNUSED(nPat);
#endif
			if( !bMatch ){
				continue;
			}
			sKept.nOfs = SyBlobLength(&sKeep);
			sKept.nLen = aRec[n].nLen;
			SyBlobAppend(&sKeep,zName,aRec[n].nLen);
			SySetPut(&aKeep,(const void *)&sKept);
		}
	}
	rc = ZipAddMatches(pCtx,pZip,&sKeep,&aKeep,zDir,(sxu32)nDir,
		nArg > 2 ? apArg[2] : 0,0);
	SyBlobRelease(&sHit);
	SySetRelease(&aHit);
	SyBlobRelease(&sKeep);
	SySetRelease(&aKeep);
	return rc;
}
/* ------------------------------------------------------------------ */
/* The callbacks, the password and the two static questions            */
/* ------------------------------------------------------------------ */
static int ZipRegisterCallback(ph7_context *pCtx,ph7_value **ppSlot,ph7_value *pFunc)
{
	ph7_vm *pVm = pCtx->pVm;
	if( *ppSlot ){
		ph7_release_value(pVm,*ppSlot);
		*ppSlot = 0;
	}
	*ppSlot = ph7_new_scalar(pVm);
	if( *ppSlot == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_MemObjStore(pFunc,*ppSlot);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_registerProgressCallback(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pZip->rProgressRate = (double)ph7_value_to_double(apArg[0]);
	return ZipRegisterCallback(pCtx,&pZip->pProgress,apArg[1]);
}
static int vm_builtin_ZipArchive_registerCancelCallback(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	return ZipRegisterCallback(pCtx,&pZip->pCancel,apArg[0]);
}
static int vm_builtin_ZipArchive_setPassword(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	const char *z;
	int n = 0;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	z = ph7_value_to_string(apArg[0],&n);
	if( n < 1 ){
		/* php refuses the EMPTY password rather than clearing the one that is
		 * there: there is no way to un-set one through this door. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobReset(&pZip->sPassword);
	SyBlobAppend(&pZip->sPassword,z,(sxu32)n);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * `setEncryptionIndex()` / `setEncryptionName()`.
 *
 * Three refusals, and php words each differently because each comes from a
 * different place. A method no build can write is `Encryption method not
 * supported` and a plain false. A NAME nothing answers to is `No such file`,
 * also plain. And an INDEX out of range is the odd one: php's stub hands the
 * library a NULL password on the way past and the library refusing THAT is what
 * the `password reset failed` warning is, with `Invalid argument` behind it.
 *
 * The password a call names belongs to the ENTRY and outranks the archive's for
 * this entry's WRITE -- and reading is the other way round, since only the
 * archive's opens anything.
 */
static int ZipSetEncryption(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,
	int nArg,ph7_value **apArg)
{
	int iMethod = (int)ph7_value_to_int(apArg[1]);
	if( iMethod != ZIP_EM_NONE && !ZipEncSupported(iMethod) ){
		return ZipFail(pCtx,pZip,ZIP_ER_ENCRNOTSUPP);
	}
	pEnt->iSetEncrypt = iMethod;
	pEnt->bHasPassword = 0;
	SyBlobReset(&pEnt->sPassword);
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		int nPw = 0;
		const char *zPw = ph7_value_to_string(apArg[2],&nPw);
		if( nPw > 0 ){
			SyBlobAppend(&pEnt->sPassword,zPw,(sxu32)nPw);
			pEnt->bHasPassword = 1;
		}
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* The `password reset failed` half: an INDEX that names nothing, which is the
 * only door of the two that reaches the library before it has an entry. */
static int ZipEncryptionBadIndex(ph7_context *pCtx,phl_zip *pZip)
{
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"password reset failed");
	return ZipFail(pCtx,pZip,ZIP_ER_INVAL);
}
static int vm_builtin_ZipArchive_setEncryptionIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipAt(pZip,ph7_value_to_int64(apArg[0]));
	if( pEnt == 0 ){
		return ZipEncryptionBadIndex(pCtx,pZip);
	}
	return ZipSetEncryption(pCtx,pZip,pEnt,nArg,apArg);
}
static int vm_builtin_ZipArchive_setEncryptionName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);
	if( pEnt == 0 ){
		return rc;
	}
	return ZipSetEncryption(pCtx,pZip,pEnt,nArg,apArg);
}
/* What this BUILD can do, which is what php answers too: its own reply comes
 * from the libzip it was linked against, so a method php names a constant for
 * is not a method php can necessarily write. */
static int vm_builtin_ZipArchive_isCompressionMethodSupported(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iMethod = (int)ph7_value_to_int(apArg[0]);
	SXUNUSED(nArg);
	ph7_result_bool(pCtx,iMethod == ZIP_CM_DEFAULT || iMethod == ZIP_CM_STORE
		|| iMethod == ZIP_CM_DEFLATE);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_isEncryptionMethodSupported(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iMethod = (int)ph7_value_to_int(apArg[0]);
	SXUNUSED(nArg);
	ph7_result_bool(pCtx,ZipEncSupported(iMethod));
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* The stream: getStream() and the read-only `zip://` wrapper          */
/* ------------------------------------------------------------------ */
/*
 * One open entry. The bytes are copied at open so nothing the script does to
 * the archive afterwards moves them, and the ARCHIVE is kept alive alongside --
 * php's `close()` leaves a stream getStream() handed out readable, and only the
 * OBJECT dying takes it down.
 */
typedef struct phl_zip_io phl_zip_io;
struct phl_zip_io {
	phl_zip *pZip;      /* the archive, held; see ZipStreamRead for what ends the read */
	SyBlob sData;
	sxu32 nCur;
	int bWrapper;       /* opened through `zip://`, which php names in the meta */
	int bRead;          /* a read has already been served from this handle */
};
/*
 * `zip://archive.zip#entry`, php's read-only wrapper. It is an opener and
 * NOTHING else: php gives it no url_stat (so `file_exists('zip://…')` is false)
 * and no directory door, which is why an archive can only be LISTED through the
 * class.
 */
static int ZipStreamOpen(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)
{
	ph7_vm *pVm = pResource ? pResource->pVm : 0;
	phl_zip *pZip;
	phl_zip_io *pIo;
	phl_zip_ent *pEnt;
	const char *zHash = 0;
	SyBlob sPath;
	const ph7_io_stream *pStream;
	void *pFile;
	sxu32 i,nName = (sxu32)SyStrlen(zName);
	int rc;
	if( pVm == 0 ){
		return -1;
	}
	if( iMode & (PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_RDWR|PH7_IO_OPEN_APPEND|PH7_IO_OPEN_CREATE) ){
		PH7_StreamSetOpenError(pVm,"operation not supported");
		return -1;
	}
	for( i = 0 ; i < nName ; ++i ){
		if( zName[i] == '#' ){
			/* the FIRST one: an entry name may hold a `#` of its own, and php
			 * gives the whole remainder to the entry rather than to the path */
			zHash = &zName[i];
			break;
		}
	}
	if( zHash == 0 ){
		PH7_StreamSetOpenError(pVm,"operation failed");
		return -1;
	}
	SyBlobInit(&sPath,&pVm->sAllocator);
	SyBlobAppend(&sPath,zName,(sxu32)(zHash - zName));
	SyBlobNullAppend(&sPath);
	pZip = ZipNew(pVm);
	if( pZip == 0 ){
		SyBlobRelease(&sPath);
		return -1;
	}
	SyBlobAppend(&pZip->sPath,SyBlobData(&sPath),SyBlobLength(&sPath));
	SyBlobNullAppend(&pZip->sPath);
	{
		const char *zTail = (const char *)SyBlobData(&sPath);
		pStream = PH7_VmGetStreamDevice(pVm,&zTail,(int)SyBlobLength(&sPath));
		pFile = pStream ? PH7_StreamOpenHandle(pVm,pStream,zTail,PH7_IO_OPEN_RDONLY,
			FALSE,0,FALSE,0,0) : 0;
		if( pFile == 0 ){
			SyBlobRelease(&sPath);
			ZipRelease(pZip);
			PH7_StreamSetOpenError(pVm,"operation failed");
			return -1;
		}
		PH7_StreamReadWholeFile(pFile,pStream,&pZip->sFile);
		PH7_StreamCloseHandle(pStream,pFile);
	}
	SyBlobRelease(&sPath);
	{
		int rcParse = ZipParse(pZip);
		if( rcParse != ZIP_ER_OK && rcParse != ZIP_ER_EMPTY_FILE ){
			ZipRelease(pZip);
			PH7_StreamSetOpenError(pVm,"operation failed");
			return -1;
		}
	}
	pEnt = ZipFind(pZip,&zHash[1],nName - (sxu32)(zHash - zName) - 1,0,0);
	if( pEnt == 0 || (rc = ZipEntLoad(pZip,pEnt)) != ZIP_ER_OK ){
		ZipRelease(pZip);
		PH7_StreamSetOpenError(pVm,"operation failed");
		return -1;
	}
	pIo = (phl_zip_io *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_zip_io));
	if( pIo == 0 ){
		ZipRelease(pZip);
		return -1;
	}
	SyZero(pIo,sizeof(*pIo));
	pIo->pZip = pZip;
	pIo->bWrapper = 1;
	SyBlobInit(&pIo->sData,&pVm->sAllocator);
	SyBlobAppend(&pIo->sData,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));
	*ppHandle = (void *)pIo;
	return PH7_OK;
}
static void ZipStreamClose(void *pHandle)
{
	phl_zip_io *pIo = (phl_zip_io *)pHandle;
	ph7_vm *pVm;
	if( pIo == 0 ){
		return;
	}
	pVm = pIo->pZip->pVm;
	SyBlobRelease(&pIo->sData);
	ZipRelease(pIo->pZip);
	SyMemBackendFree(&pVm->sAllocator,pIo);
}
/*
 * A read from a getStream() handle, and the one rule that decides whether it
 * still works after the archive was closed.
 *
 * php's answer is its library's read BUFFER, and it is visible three ways: read
 * a byte, close the archive, and the rest of the entry still comes out; close
 * the archive without reading first, and the read is `Containing zip archive
 * was closed`; and destroying the OBJECT ends it either way. The bytes are held
 * here rather than buffered, so the rule is stated instead of inherited -- a
 * handle nothing has been read from dies with its archive, and one that has
 * been read from outlives the close.
 */
static ph7_int64 ZipStreamRead(void *pHandle,void *pBuffer,ph7_int64 nWant)
{
	phl_zip_io *pIo = (phl_zip_io *)pHandle;
	sxu32 nLeft;
	if( pIo == 0 || pIo->pZip->bDead ){
		return -1;
	}
	if( !pIo->bWrapper && !pIo->pZip->bOpen && !pIo->bRead ){
		return -1;
	}
	nLeft = SyBlobLength(&pIo->sData) - pIo->nCur;
	if( nLeft < 1 ){
		return 0;
	}
	if( (sxu64)nWant > (sxu64)nLeft ){
		nWant = (ph7_int64)nLeft;
	}
	SyMemcpy((const char *)SyBlobData(&pIo->sData) + pIo->nCur,pBuffer,(sxu32)nWant);
	pIo->nCur += (sxu32)nWant;
	pIo->bRead = 1;
	return nWant;
}
static ph7_int64 ZipStreamTell(void *pHandle)
{
	phl_zip_io *pIo = (phl_zip_io *)pHandle;
	return pIo ? (ph7_int64)pIo->nCur : -1;
}
/* php's zip stream is NOT seekable -- a member is decompressed forwards -- and
 * a script sees that both in `stream_get_meta_data()` and in the warning an
 * fseek() on one raises. */
PH7_PRIVATE const ph7_io_stream sZIP_Stream = {
	"zip",
	PH7_IO_STREAM_VERSION,
	ZipStreamOpen,   /* xOpen */
	0,               /* xOpenDir */
	ZipStreamClose,  /* xClose */
	0,               /* xCloseDir */
	ZipStreamRead,   /* xRead */
	0,               /* xReadDir */
	0,               /* xWrite */
	0,               /* xSeek */
	0,               /* xLock */
	0,               /* xRewindDir */
	ZipStreamTell,   /* xTell */
	0,               /* xTrunc */
	0,               /* xSync */
	0                /* xStat */
};
PH7_PRIVATE int PH7_ZipStreamIs(const ph7_io_stream *pStream)
{
	return pStream == &sZIP_Stream;
}
/* Was this handle opened through the `zip://` wrapper, or handed out by
 * getStream()? php reports a `wrapper_type` for the first and none for the
 * second, which is the only thing that tells them apart. */
PH7_PRIVATE int PH7_ZipStreamViaWrapper(void *pHandle)
{
	phl_zip_io *pIo = (phl_zip_io *)pHandle;
	return pIo != 0 && pIo->bWrapper;
}
/* `getStream()`, `getStreamName()` and `getStreamIndex()`, which are one verb
 * told the entry three ways. The stream php hands back names the ENTRY as its
 * uri and no wrapper at all. */
static int ZipStreamOf(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt)
{
	io_private *pDev;
	phl_zip_io *pIo;
	int rc = ZipEntLoad(pZip,pEnt);
	if( rc != ZIP_ER_OK ){
		return ZipFail(pCtx,pZip,rc);
	}
	pIo = (phl_zip_io *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_zip_io));
	if( pIo == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	SyZero(pIo,sizeof(*pIo));
	pIo->pZip = pZip;
	pZip->nRef++;
	SyBlobInit(&pIo->sData,&pCtx->pVm->sAllocator);
	SyBlobAppend(&pIo->sData,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);
	if( pDev == 0 ){
		SyBlobRelease(&pIo->sData);
		ZipRelease(pZip);
		SyMemBackendFree(&pCtx->pVm->sAllocator,pIo);
		return PH7_ContextMemoryError(pCtx);
	}
	InitIOPrivate(pCtx->pVm,&sZIP_Stream,pDev);
	pDev->pHandle = pIo;
	SetIOPrivateOpenedAs(pDev,(const char *)SyBlobData(&pEnt->sName),
		(int)SyBlobLength(&pEnt->sName),"rb",2);
	ph7_result_resource(pCtx,pDev);
	return PH7_OK;
}
static int vm_builtin_ZipArchive_getStreamIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int rc;
	SXUNUSED(nArg);
	if( pZip == 0 ){
		return PH7_OK;
	}
	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);
	return pEnt ? ZipStreamOf(pCtx,pZip,pEnt) : rc;
}
static int vm_builtin_ZipArchive_getStreamName(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip = ZipThis(pCtx,0);
	phl_zip_ent *pEnt;
	int iFlags = 0,rc;
	if( pZip == 0 ){
		return PH7_OK;
	}
	if( ZipNulName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){
		return rc;
	}
	if( nArg > 1 ){
		iFlags = (int)ph7_value_to_int(apArg[1]);
	}
	pEnt = ZipArgName(pCtx,pZip,apArg[0],iFlags,&rc);
	return pEnt ? ZipStreamOf(pCtx,pZip,pEnt) : rc;
}
/* ------------------------------------------------------------------ */
/* php's deprecated procedural half                                    */
/* ------------------------------------------------------------------ */
/*
 * `zip_open()` and the seven verbs around it. php deprecated all ten in 8.0 and
 * has kept them ever since, so they are here with the notice the call itself
 * raises (aDeprecatedFunc[] in vm_arg_check.c) rather than a body of their own.
 *
 * They are a CURSOR over the entry table -- `zip_read()` hands out the next
 * entry and there is no way back -- which is why php's replacement advice is
 * `statIndex()` and not any one of them. The two handles are resources rather
 * than objects, and php names them `Zip Directory` and `Zip Entry`.
 */
/*
 * Both handles carry an `io_private`-compatible header, the way the stream
 * context and the filter handles do: get_resource_type() probes the magic in
 * THAT field on any resource it is handed, so a record with a smaller header
 * would be read past its end.
 */
typedef struct phl_zip_dir phl_zip_dir;
typedef struct phl_zip_res phl_zip_res;
struct phl_zip_dir {
	io_private base;   /* base.iMagic == ZIP_DIR_MAGIC */
	phl_zip *pZip;
	sxu32 nCur;
};
struct phl_zip_res {
	io_private base;   /* base.iMagic == ZIP_ENT_MAGIC */
	phl_zip *pZip;
	phl_zip_ent *pEnt;
	sxu32 nCur;        /* zip_entry_read()'s cursor */
	int bOpen;
};
#define ZIP_DIR_MAGIC 0x5A44495Au   /* 'ZDIZ' */
#define ZIP_ENT_MAGIC 0x5A454E54u   /* 'ZENT' */
/*
 * php's own argument screen for the ten. Their declarations say `$zip` with no
 * type at all, so the refusal is not ZPP's: each body checks the value is a
 * RESOURCE and words the TypeError itself. A resource of the wrong kind falls
 * through to the body, which answers false rather than throwing.
 */
static int ZipHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,int *pRc)
{
	char zGiven[64];
	*pRc = PH7_OK;
	if( pArg != 0 && ph7_value_is_resource(pArg) ){
		return 1;
	}
	*pRc = PH7_VmThrowException(pCtx,"TypeError",
		"%z(): Argument #%d ($%s) must be of type resource, %s given",
		&pCtx->pFunc->sName,iPos,zName,
		VmValueGivenName(pArg,zGiven,sizeof(zGiven)));
	return 0;
}
static phl_zip_dir * ZipDirArg(ph7_value *pArg)
{
	phl_zip_dir *pDir;
	if( pArg == 0 || !ph7_value_is_resource(pArg) ){
		return 0;
	}
	pDir = (phl_zip_dir *)ph7_value_to_resource(pArg);
	return (pDir && pDir->base.iMagic == ZIP_DIR_MAGIC) ? pDir : 0;
}
static phl_zip_res * ZipEntArg(ph7_value *pArg)
{
	phl_zip_res *pRes;
	if( pArg == 0 || !ph7_value_is_resource(pArg) ){
		return 0;
	}
	pRes = (phl_zip_res *)ph7_value_to_resource(pArg);
	return (pRes && pRes->base.iMagic == ZIP_ENT_MAGIC) ? pRes : 0;
}
PH7_PRIVATE const char * PH7_ZipResourceType(void *pResource)
{
	io_private *pDev = (io_private *)pResource;
	if( pDev == 0 ){
		return 0;
	}
	if( pDev->iMagic == ZIP_DIR_MAGIC ){
		return "Zip Directory";
	}
	if( pDev->iMagic == ZIP_ENT_MAGIC ){
		return "Zip Entry";
	}
	return 0;
}
static int PH7_builtin_zip_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip *pZip;
	phl_zip_dir *pDir;
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	const char *zName;
	SyBlob sPath;
	int nName = 0,rc;
	SXUNUSED(nArg);
	zName = ph7_value_to_string(apArg[0],&nName);
	if( nName < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%z(): Argument #1 ($filename) must not be empty",&pCtx->pFunc->sName);
	}
	PH7_VfsExpandPath(pCtx,zName,nName,&sPath);
	pZip = ZipNew(pCtx->pVm);
	if( pZip == 0 ){
		SyBlobRelease(&sPath);
		return PH7_ContextMemoryError(pCtx);
	}
	SyBlobAppend(&pZip->sPath,SyBlobData(&sPath),SyBlobLength(&sPath));
	SyBlobRelease(&sPath);
	SyBlobNullAppend(&pZip->sPath);
	{
		const char *zPath = (const char *)SyBlobData(&pZip->sPath);
		const ph7_io_stream *pStream;
		void *pFile;
		const char *zTail = zPath;
		if( pVfs && pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK ){
			rc = ZIP_ER_OPNOTSUPP;
			goto failed;
		}
		if( pVfs == 0 || pVfs->xFileExists == 0 || pVfs->xFileExists(zPath) != PH7_OK ){
			rc = ZIP_ER_NOENT;
			goto failed;
		}
		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTail,(int)SyBlobLength(&pZip->sPath));
		pFile = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zTail,PH7_IO_OPEN_RDONLY,
			FALSE,0,FALSE,0,0) : 0;
		if( pFile == 0 ){
			rc = ZIP_ER_READ;
			goto failed;
		}
		PH7_StreamReadWholeFile(pFile,pStream,&pZip->sFile);
		PH7_StreamCloseHandle(pStream,pFile);
		rc = ZipParse(pZip);
		if( rc == ZIP_ER_EMPTY_FILE ){
			ph7_context_throw_error_format(pCtx,8192 /* E_DEPRECATED */,
				"Using empty file as ZipArchive is deprecated");
		}else if( rc != ZIP_ER_OK ){
			goto failed;
		}
	}
	pDir = (phl_zip_dir *)ph7_context_alloc_chunk(pCtx,sizeof(phl_zip_dir),TRUE,FALSE);
	if( pDir == 0 ){
		ZipRelease(pZip);
		return PH7_ContextMemoryError(pCtx);
	}
	pDir->base.iMagic = ZIP_DIR_MAGIC;
	pDir->pZip = pZip;
	pDir->nCur = 0;
	ph7_result_resource(pCtx,pDir);
	return PH7_OK;
failed:
	ZipRelease(pZip);
	ph7_result_int(pCtx,rc);
	return PH7_OK;
}
static int PH7_builtin_zip_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_dir *pDir = ZipDirArg(apArg[0]);
	int rcArg;
	SXUNUSED(nArg); SXUNUSED(pCtx);
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip",&rcArg) ){
		return rcArg;
	}
	if( pDir == 0 ){
		return PH7_OK;
	}
	if( pDir->pZip ){
		ZipRelease(pDir->pZip);
		pDir->pZip = 0;
	}
	return PH7_OK;
}
static int PH7_builtin_zip_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_dir *pDir = ZipDirArg(apArg[0]);
	phl_zip_res *pRes;
	phl_zip_ent *pEnt = 0;
	int rcArg;
	SXUNUSED(nArg);
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip",&rcArg) ){
		return rcArg;
	}
	if( pDir == 0 || pDir->pZip == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	while( pDir->nCur < pDir->pZip->nEnt ){
		phl_zip_ent *p = pDir->pZip->apEnt[pDir->nCur++];
		if( !p->bDeleted ){
			pEnt = p;
			break;
		}
	}
	if( pEnt == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRes = (phl_zip_res *)ph7_context_alloc_chunk(pCtx,sizeof(phl_zip_res),TRUE,FALSE);
	if( pRes == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pRes->base.iMagic = ZIP_ENT_MAGIC;
	/* The entry holds the archive: `zip_close()` may come before the last read
	 * of an entry it handed out, and php reading freed memory there is not
	 * something to reproduce. The reference is never dropped -- a resource has
	 * no destructor to drop it in -- so the archive lives until the VM sweep,
	 * exactly as php's does until the request ends. */
	pRes->pZip = pDir->pZip;
	pDir->pZip->nRef++;
	pRes->pEnt = pEnt;
	pRes->nCur = 0;
	pRes->bOpen = 0;
	ph7_result_resource(pCtx,pRes);
	return PH7_OK;
}
static int PH7_builtin_zip_entry_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_res *pRes = ZipEntArg(nArg > 1 ? apArg[1] : 0);
	int rcArg;
	SXUNUSED(nArg);
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_dp",&rcArg) ){
		return rcArg;
	}
	if( pRes == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRes->bOpen = 1;
	pRes->nCur = 0;
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int PH7_builtin_zip_entry_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_res *pRes = ZipEntArg(apArg[0]);
	int rcArg;
	SXUNUSED(nArg);
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){
		return rcArg;
	}
	if( pRes == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pRes->bOpen = 0;
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int PH7_builtin_zip_entry_read(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_res *pRes = ZipEntArg(apArg[0]);
	sxi64 iWant = 1024;
	sxu32 nLeft;
	int rcArg;
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){
		return rcArg;
	}
	if( pRes == 0 || ZipEntLoad(pRes->pZip,pRes->pEnt) != ZIP_ER_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){
		iWant = ph7_value_to_int64(apArg[1]);
	}
	if( iWant < 0 ){
		iWant = 0;
	}
	nLeft = SyBlobLength(&pRes->pEnt->sData) - pRes->nCur;
	if( (sxu64)iWant > (sxu64)nLeft ){
		iWant = (sxi64)nLeft;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pRes->pEnt->sData) + pRes->nCur,
		(int)iWant);
	pRes->nCur += (sxu32)iWant;
	return PH7_OK;
}
static int PH7_builtin_zip_entry_name(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_res *pRes = ZipEntArg(apArg[0]);
	int rcArg;
	SXUNUSED(nArg);
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){
		return rcArg;
	}
	if( pRes == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pRes->pEnt->sName),
		(int)SyBlobLength(&pRes->pEnt->sName));
	return PH7_OK;
}
static int PH7_builtin_zip_entry_filesize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_res *pRes = ZipEntArg(apArg[0]);
	int rcArg;
	SXUNUSED(nArg);
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){
		return rcArg;
	}
	if( pRes == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(ph7_int64)pRes->pEnt->nSize);
	return PH7_OK;
}
static int PH7_builtin_zip_entry_compressedsize(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_res *pRes = ZipEntArg(apArg[0]);
	int rcArg;
	SXUNUSED(nArg);
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){
		return rcArg;
	}
	if( pRes == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(ph7_int64)pRes->pEnt->nCompSize);
	return PH7_OK;
}
/* php names the method rather than numbering it here, and it names only the
 * two the format actually uses today; anything else is "unknown". */
static int PH7_builtin_zip_entry_compressionmethod(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_zip_res *pRes = ZipEntArg(apArg[0]);
	const char *zName;
	int rcArg;
	SXUNUSED(nArg);
	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){
		return rcArg;
	}
	if( pRes == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	switch( pRes->pEnt->iMethod ){
	case 0:  zName = "stored";   break;
	case 1:  zName = "shrunk";   break;
	case 2:
	case 3:
	case 4:
	case 5:  zName = "reduced";  break;
	case 6:  zName = "imploded"; break;
	case 8:  zName = "deflated"; break;
	case 9:  zName = "deflatedX";break;
	case 10: zName = "implodedX";break;
	default:
		/* php's table stops there -- a bzip2, LZMA or XZ member has no name in
		 * it and the answer is FALSE, not "unknown". */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,zName,-1);
	return PH7_OK;
}
/* ------------------------------------------------------------------ */
/* Mounting the extension                                              */
/* ------------------------------------------------------------------ */
#define ZIP_ICONST(NAME,VALUE) \
	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (VALUE), 0, 0.0 }
PH7_PRIVATE sxi32 PH7_VmInstallZip(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aMethod[] = {
		{ "open", PH7_MOD_PUBLIC, "string $filename, int $flags = 0", "@int|bool",
		  vm_builtin_ZipArchive_open },
		{ "setPassword", PH7_MOD_PUBLIC, "string $password", "@bool",
		  vm_builtin_ZipArchive_setPassword },
		{ "close", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ZipArchive_close },
		{ "count", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ZipArchive_count },
		{ "getStatusString", PH7_MOD_PUBLIC, "", "@string",
		  vm_builtin_ZipArchive_getStatusString },
		{ "clearError", PH7_MOD_PUBLIC, "", "void", vm_builtin_ZipArchive_clearError },
		{ "addEmptyDir", PH7_MOD_PUBLIC, "string $dirname, int $flags = 0", "@bool",
		  vm_builtin_ZipArchive_addEmptyDir },
		{ "addFromString", PH7_MOD_PUBLIC,
		  "string $name, string $content, int $flags = ZipArchive::FL_OVERWRITE", "@bool",
		  vm_builtin_ZipArchive_addFromString },
		{ "addFile", PH7_MOD_PUBLIC,
		  "string $filepath, string $entryname = \"\", int $start = 0, "
		  "int $length = ZipArchive::LENGTH_TO_END, int $flags = ZipArchive::FL_OVERWRITE",
		  "@bool", vm_builtin_ZipArchive_addFile },
		{ "replaceFile", PH7_MOD_PUBLIC,
		  "string $filepath, int $index, int $start = 0, "
		  "int $length = ZipArchive::LENGTH_TO_END, int $flags = 0",
		  "@bool", vm_builtin_ZipArchive_replaceFile },
		{ "addGlob", PH7_MOD_PUBLIC, "string $pattern, int $flags = 0, array $options = []",
		  "@array|false", vm_builtin_ZipArchive_addGlob },
		{ "addPattern", PH7_MOD_PUBLIC,
		  "string $pattern, string $path = \".\", array $options = []",
		  "@array|false", vm_builtin_ZipArchive_addPattern },
		{ "renameIndex", PH7_MOD_PUBLIC, "int $index, string $new_name", "@bool",
		  vm_builtin_ZipArchive_renameIndex },
		{ "renameName", PH7_MOD_PUBLIC, "string $name, string $new_name", "@bool",
		  vm_builtin_ZipArchive_renameName },
		{ "setArchiveComment", PH7_MOD_PUBLIC, "string $comment", "@bool",
		  vm_builtin_ZipArchive_setArchiveComment },
		{ "getArchiveComment", PH7_MOD_PUBLIC, "int $flags = 0", "@string|false",
		  vm_builtin_ZipArchive_getArchiveComment },
		{ "setArchiveFlag", PH7_MOD_PUBLIC, "int $flag, int $value", "bool",
		  vm_builtin_ZipArchive_setArchiveFlag },
		{ "getArchiveFlag", PH7_MOD_PUBLIC, "int $flag, int $flags = 0", "int",
		  vm_builtin_ZipArchive_getArchiveFlag },
		{ "setCommentIndex", PH7_MOD_PUBLIC, "int $index, string $comment", "@bool",
		  vm_builtin_ZipArchive_setCommentIndex },
		{ "setCommentName", PH7_MOD_PUBLIC, "string $name, string $comment", "@bool",
		  vm_builtin_ZipArchive_setCommentName },
		{ "setMtimeIndex", PH7_MOD_PUBLIC, "int $index, int $timestamp, int $flags = 0",
		  "@bool", vm_builtin_ZipArchive_setMtimeIndex },
		{ "setMtimeName", PH7_MOD_PUBLIC, "string $name, int $timestamp, int $flags = 0",
		  "@bool", vm_builtin_ZipArchive_setMtimeName },
		{ "getCommentIndex", PH7_MOD_PUBLIC, "int $index, int $flags = 0", "@string|false",
		  vm_builtin_ZipArchive_getCommentIndex },
		{ "getCommentName", PH7_MOD_PUBLIC, "string $name, int $flags = 0", "@string|false",
		  vm_builtin_ZipArchive_getCommentName },
		{ "deleteIndex", PH7_MOD_PUBLIC, "int $index", "@bool",
		  vm_builtin_ZipArchive_deleteIndex },
		{ "deleteName", PH7_MOD_PUBLIC, "string $name", "@bool",
		  vm_builtin_ZipArchive_deleteName },
		{ "statName", PH7_MOD_PUBLIC, "string $name, int $flags = 0", "@array|false",
		  vm_builtin_ZipArchive_statName },
		{ "statIndex", PH7_MOD_PUBLIC, "int $index, int $flags = 0", "@array|false",
		  vm_builtin_ZipArchive_statIndex },
		{ "locateName", PH7_MOD_PUBLIC, "string $name, int $flags = 0", "@int|false",
		  vm_builtin_ZipArchive_locateName },
		{ "getNameIndex", PH7_MOD_PUBLIC, "int $index, int $flags = 0", "@string|false",
		  vm_builtin_ZipArchive_getNameIndex },
		{ "unchangeArchive", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_ZipArchive_unchangeArchive },
		{ "unchangeAll", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ZipArchive_unchangeAll },
		{ "unchangeIndex", PH7_MOD_PUBLIC, "int $index", "@bool",
		  vm_builtin_ZipArchive_unchangeIndex },
		{ "unchangeName", PH7_MOD_PUBLIC, "string $name", "@bool",
		  vm_builtin_ZipArchive_unchangeName },
		{ "extractTo", PH7_MOD_PUBLIC, "string $pathto, array|string|null $files = null",
		  "@bool", vm_builtin_ZipArchive_extractTo },
		{ "getFromName", PH7_MOD_PUBLIC, "string $name, int $len = 0, int $flags = 0",
		  "@string|false", vm_builtin_ZipArchive_getFromName },
		{ "getFromIndex", PH7_MOD_PUBLIC, "int $index, int $len = 0, int $flags = 0",
		  "@string|false", vm_builtin_ZipArchive_getFromIndex },
		{ "getStreamIndex", PH7_MOD_PUBLIC, "int $index, int $flags = 0", 0,
		  vm_builtin_ZipArchive_getStreamIndex },
		{ "getStreamName", PH7_MOD_PUBLIC, "string $name, int $flags = 0", 0,
		  vm_builtin_ZipArchive_getStreamName },
		{ "getStream", PH7_MOD_PUBLIC, "string $name", 0,
		  vm_builtin_ZipArchive_getStreamName },
		{ "setExternalAttributesName", PH7_MOD_PUBLIC,
		  "string $name, int $opsys, int $attr, int $flags = 0", "@bool",
		  vm_builtin_ZipArchive_setExternalAttributesName },
		{ "setExternalAttributesIndex", PH7_MOD_PUBLIC,
		  "int $index, int $opsys, int $attr, int $flags = 0", "@bool",
		  vm_builtin_ZipArchive_setExternalAttributesIndex },
		{ "getExternalAttributesName", PH7_MOD_PUBLIC,
		  "string $name, &$opsys, &$attr, int $flags = 0", "@bool",
		  vm_builtin_ZipArchive_getExternalAttributesName },
		{ "getExternalAttributesIndex", PH7_MOD_PUBLIC,
		  "int $index, &$opsys, &$attr, int $flags = 0", "@bool",
		  vm_builtin_ZipArchive_getExternalAttributesIndex },
		{ "setCompressionName", PH7_MOD_PUBLIC,
		  "string $name, int $method, int $compflags = 0", "@bool",
		  vm_builtin_ZipArchive_setCompressionName },
		{ "setCompressionIndex", PH7_MOD_PUBLIC,
		  "int $index, int $method, int $compflags = 0", "@bool",
		  vm_builtin_ZipArchive_setCompressionIndex },
		{ "setEncryptionName", PH7_MOD_PUBLIC,
		  "string $name, int $method, ?string $password = null", "@bool",
		  vm_builtin_ZipArchive_setEncryptionName },
		{ "setEncryptionIndex", PH7_MOD_PUBLIC,
		  "int $index, int $method, ?string $password = null", "@bool",
		  vm_builtin_ZipArchive_setEncryptionIndex },
		{ "registerProgressCallback", PH7_MOD_PUBLIC, "float $rate, callable $callback",
		  "@bool", vm_builtin_ZipArchive_registerProgressCallback },
		{ "registerCancelCallback", PH7_MOD_PUBLIC, "callable $callback", "@bool",
		  vm_builtin_ZipArchive_registerCancelCallback },
		{ "isCompressionMethodSupported", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "int $method, bool $enc = true", "bool",
		  vm_builtin_ZipArchive_isCompressionMethodSupported },
		{ "isEncryptionMethodSupported", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "int $method, bool $enc = true", "bool",
		  vm_builtin_ZipArchive_isEncryptionMethodSupported }
	};
	/*
	 * The six php DECLARES and answers through a handler. They are real slots
	 * here, written by the C bodies after every verb that could move one -- the
	 * same fact from the other side, which is what makes `var_dump()`, the
	 * `(array)` cast, `json_encode()`, `foreach`, `serialize()` and
	 * `get_object_vars()` all show the six live values php shows.
	 *
	 * They carry NO default, which is php's own declaration: Reflection reports
	 * `hasDefaultValue()` false for each, and neither readonly nor virtual --
	 * the write refusal is the handler's (ZipSetHook), not a modifier's.
	 */
	static const PH7_NativePropDef aProp[] = {
		{ "lastId",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
		{ "status",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
		{ "statusSys", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
		{ "numFiles",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },
		{ "filename",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		{ "comment",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		{ ZIP_RES, PH7_MOD_PRIVATE|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }
	};
	/*
	 * php's constants, in php's own order: the five open flags, the fifteen
	 * per-call FL_ flags, the nineteen compression methods, the thirty-three
	 * error codes, the one archive flag, the twenty operating systems, the six
	 * encryption methods, the libzip version and LENGTH_TO_END.
	 *
	 * The compression and encryption tables are NAMES rather than capabilities:
	 * php declares every one of them whatever its libzip can do, and
	 * `isCompressionMethodSupported()` is the separate question.
	 */
	static const PH7_NativeConstDef aConst[] = {
		ZIP_ICONST("CREATE",            ZIP_OPEN_CREATE),
		ZIP_ICONST("EXCL",              ZIP_OPEN_EXCL),
		ZIP_ICONST("CHECKCONS",         ZIP_OPEN_CHECKCONS),
		ZIP_ICONST("OVERWRITE",         ZIP_OPEN_OVERWRITE),
		ZIP_ICONST("RDONLY",            ZIP_OPEN_RDONLY),
		ZIP_ICONST("FL_NOCASE",         ZIP_FL_NOCASE),
		ZIP_ICONST("FL_NODIR",          ZIP_FL_NODIR),
		ZIP_ICONST("FL_COMPRESSED",     ZIP_FL_COMPRESSED),
		ZIP_ICONST("FL_UNCHANGED",      ZIP_FL_UNCHANGED),
		ZIP_ICONST("FL_RECOMPRESS",     16),
		ZIP_ICONST("FL_ENCRYPTED",      32),
		ZIP_ICONST("FL_OVERWRITE",      ZIP_FL_OVERWRITE),
		ZIP_ICONST("FL_LOCAL",          256),
		ZIP_ICONST("FL_CENTRAL",        512),
		ZIP_ICONST("FL_ENC_GUESS",      0),
		ZIP_ICONST("FL_ENC_RAW",        ZIP_FL_ENC_RAW),
		ZIP_ICONST("FL_ENC_STRICT",     128),
		ZIP_ICONST("FL_ENC_UTF_8",      ZIP_FL_ENC_UTF_8),
		ZIP_ICONST("FL_ENC_CP437",      4096),
		ZIP_ICONST("FL_OPEN_FILE_NOW",  1073741824),
		ZIP_ICONST("CM_DEFAULT",        ZIP_CM_DEFAULT),
		ZIP_ICONST("CM_STORE",          ZIP_CM_STORE),
		ZIP_ICONST("CM_SHRINK",         1),
		ZIP_ICONST("CM_REDUCE_1",       2),
		ZIP_ICONST("CM_REDUCE_2",       3),
		ZIP_ICONST("CM_REDUCE_3",       4),
		ZIP_ICONST("CM_REDUCE_4",       5),
		ZIP_ICONST("CM_IMPLODE",        6),
		ZIP_ICONST("CM_DEFLATE",        ZIP_CM_DEFLATE),
		ZIP_ICONST("CM_DEFLATE64",      9),
		ZIP_ICONST("CM_PKWARE_IMPLODE", 10),
		ZIP_ICONST("CM_BZIP2",          ZIP_CM_BZIP2),
		ZIP_ICONST("CM_LZMA",           14),
		ZIP_ICONST("CM_LZMA2",          33),
		ZIP_ICONST("CM_XZ",             95),
		ZIP_ICONST("CM_TERSE",          18),
		ZIP_ICONST("CM_LZ77",           19),
		ZIP_ICONST("CM_WAVPACK",        97),
		ZIP_ICONST("CM_PPMD",           98),
		ZIP_ICONST("ER_OK",             ZIP_ER_OK),
		ZIP_ICONST("ER_MULTIDISK",      ZIP_ER_MULTIDISK),
		ZIP_ICONST("ER_RENAME",         ZIP_ER_RENAME),
		ZIP_ICONST("ER_CLOSE",          ZIP_ER_CLOSE),
		ZIP_ICONST("ER_SEEK",           ZIP_ER_SEEK),
		ZIP_ICONST("ER_READ",           ZIP_ER_READ),
		ZIP_ICONST("ER_WRITE",          ZIP_ER_WRITE),
		ZIP_ICONST("ER_CRC",            ZIP_ER_CRC),
		ZIP_ICONST("ER_ZIPCLOSED",      ZIP_ER_ZIPCLOSED),
		ZIP_ICONST("ER_NOENT",          ZIP_ER_NOENT),
		ZIP_ICONST("ER_EXISTS",         ZIP_ER_EXISTS),
		ZIP_ICONST("ER_OPEN",           ZIP_ER_OPEN),
		ZIP_ICONST("ER_TMPOPEN",        ZIP_ER_TMPOPEN),
		ZIP_ICONST("ER_ZLIB",           ZIP_ER_ZLIB),
		ZIP_ICONST("ER_MEMORY",         ZIP_ER_MEMORY),
		ZIP_ICONST("ER_CHANGED",        ZIP_ER_CHANGED),
		ZIP_ICONST("ER_COMPNOTSUPP",    ZIP_ER_COMPNOTSUPP),
		ZIP_ICONST("ER_EOF",            ZIP_ER_EOF),
		ZIP_ICONST("ER_INVAL",          ZIP_ER_INVAL),
		ZIP_ICONST("ER_NOZIP",          ZIP_ER_NOZIP),
		ZIP_ICONST("ER_INTERNAL",       ZIP_ER_INTERNAL),
		ZIP_ICONST("ER_INCONS",         ZIP_ER_INCONS),
		ZIP_ICONST("ER_REMOVE",         ZIP_ER_REMOVE),
		ZIP_ICONST("ER_DELETED",        ZIP_ER_DELETED),
		ZIP_ICONST("ER_ENCRNOTSUPP",    ZIP_ER_ENCRNOTSUPP),
		ZIP_ICONST("ER_RDONLY",         ZIP_ER_RDONLY),
		ZIP_ICONST("ER_NOPASSWD",       ZIP_ER_NOPASSWD),
		ZIP_ICONST("ER_WRONGPASSWD",    ZIP_ER_WRONGPASSWD),
		ZIP_ICONST("ER_OPNOTSUPP",      ZIP_ER_OPNOTSUPP),
		ZIP_ICONST("ER_INUSE",          ZIP_ER_INUSE),
		ZIP_ICONST("ER_TELL",           ZIP_ER_TELL),
		ZIP_ICONST("ER_COMPRESSED_DATA",ZIP_ER_COMPRESSED_DATA),
		ZIP_ICONST("ER_CANCELLED",      ZIP_ER_CANCELLED),
		ZIP_ICONST("AFL_RDONLY",        ZIP_AFL_RDONLY),
		ZIP_ICONST("OPSYS_DOS",             0),
		ZIP_ICONST("OPSYS_AMIGA",           1),
		ZIP_ICONST("OPSYS_OPENVMS",         2),
		ZIP_ICONST("OPSYS_UNIX",            ZIP_OPSYS_UNIX),
		ZIP_ICONST("OPSYS_VM_CMS",          4),
		ZIP_ICONST("OPSYS_ATARI_ST",        5),
		ZIP_ICONST("OPSYS_OS_2",            6),
		ZIP_ICONST("OPSYS_MACINTOSH",       7),
		ZIP_ICONST("OPSYS_Z_SYSTEM",        8),
		ZIP_ICONST("OPSYS_CPM",             9),
		ZIP_ICONST("OPSYS_WINDOWS_NTFS",    10),
		ZIP_ICONST("OPSYS_MVS",             11),
		ZIP_ICONST("OPSYS_VSE",             12),
		ZIP_ICONST("OPSYS_ACORN_RISC",      13),
		ZIP_ICONST("OPSYS_VFAT",            14),
		ZIP_ICONST("OPSYS_ALTERNATE_MVS",   15),
		ZIP_ICONST("OPSYS_BEOS",            16),
		ZIP_ICONST("OPSYS_TANDEM",          17),
		ZIP_ICONST("OPSYS_OS_400",          18),
		ZIP_ICONST("OPSYS_OS_X",            19),
		ZIP_ICONST("OPSYS_DEFAULT",         ZIP_OPSYS_UNIX),
		ZIP_ICONST("EM_NONE",           ZIP_EM_NONE),
		ZIP_ICONST("EM_TRAD_PKWARE",    ZIP_EM_TRAD_PKWARE),
		ZIP_ICONST("EM_AES_128",        ZIP_EM_AES_128),
		ZIP_ICONST("EM_AES_192",        ZIP_EM_AES_192),
		ZIP_ICONST("EM_AES_256",        ZIP_EM_AES_256),
		ZIP_ICONST("EM_UNKNOWN",        ZIP_EM_UNKNOWN),
		{ "LIBZIP_VERSION", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, PHL_ZIP_VERSION, 0.0 },
		ZIP_ICONST("LENGTH_TO_END",     0)
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ZipArchive", 0, "Countable", PH7_CLASS_NOCLONE,
		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),
		  aProp, SX_ARRAYSIZE(aProp), ZipInstanceRelease, 0, 0 }
	};
	sxi32 rc;
	pVm->pZips = 0;
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = PH7_NativeClassInstallSetHook(&(*pVm),"ZipArchive",ZipSetHook);
	if( rc != SXRET_OK ){
		return rc;
	}
	return PH7_NativeClassInstallNewHook(&(*pVm),"ZipArchive",ZipNewHook);
}
PH7_PRIVATE const ph7_builtin_func * PH7_ZipFuncTable(sxu32 *pnEntry)
{
	static const ph7_builtin_func aFunc[] = {
		{ "zip_open",                     PH7_builtin_zip_open                     },
		{ "zip_close",                    PH7_builtin_zip_close                    },
		{ "zip_read",                     PH7_builtin_zip_read                     },
		{ "zip_entry_open",               PH7_builtin_zip_entry_open               },
		{ "zip_entry_close",              PH7_builtin_zip_entry_close              },
		{ "zip_entry_read",               PH7_builtin_zip_entry_read               },
		{ "zip_entry_name",               PH7_builtin_zip_entry_name               },
		{ "zip_entry_compressedsize",     PH7_builtin_zip_entry_compressedsize     },
		{ "zip_entry_filesize",           PH7_builtin_zip_entry_filesize           },
		{ "zip_entry_compressionmethod",  PH7_builtin_zip_entry_compressionmethod  }
	};
	*pnEntry = (sxu32)SX_ARRAYSIZE(aFunc);
	return aFunc;
}
#else /* !PH7_ENABLE_ZLIB || PH7_DISABLE_BUILTIN_FUNC */
/* No zlib means no ext/zip, exactly as php's own build has none: the two VM
 * lifecycle hooks are called unconditionally and have nothing to sweep. */
PH7_PRIVATE void PH7_ZipVmReset(ph7_vm *pVm){ (void)pVm; }
PH7_PRIVATE void PH7_ZipVmRelease(ph7_vm *pVm){ (void)pVm; }
PH7_PRIVATE const char * PH7_ZipResourceType(void *pResource){ (void)pResource; return 0; }
#endif /* PH7_ENABLE_ZLIB && !PH7_DISABLE_BUILTIN_FUNC */
