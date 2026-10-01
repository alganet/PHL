# src/ph7/vm_phar.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2085/3206 lines (65.03%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |    7 | `#include <stdio.h>` |
|     - |    8 | `#include <time.h>   /* an entry's modification time, stamped as php stamps it */` |
|     - |    9 | `#ifdef PH7_ENABLE_ZLIB` |
|     - |   10 | `#include <zlib.h>` |
|     - |   11 | `#endif` |
|     - |   12 | `#ifdef PH7_ENABLE_OPENSSL` |
|     - |   13 | `/* php's three OpenSSL signature flavours are a BUILD question: an archive` |
|     - |   14 | ` * signed with one is refused outright by a php with no ext/openssl, and by` |
|     - |   15 | ` * this engine without one too. */` |
|     - |   16 | `#include <openssl/evp.h>` |
|     - |   17 | `#include <openssl/pem.h>` |
|     - |   18 | `#include <openssl/err.h>` |
|     - |   19 | `#endif` |
|     - |   20 | `/*` |
|     - |   21 | ` * Section:` |
|     - |   22 | ` *    php's phar extension: an archive that is also a php program.` |
|     - |   23 | ` * Status:` |
|     - |   24 | ` *    Stable.` |
|     - |   25 | ` *` |
|     - |   26 | ` * A .phar is one file with three parts: a php STUB that runs when the file is` |
|     - |   27 | ` * executed, a MANIFEST describing what is inside, and the entries' bytes. The` |
|     - |   28 | `` * stub ends in `__halt_compiler();`, which is what stops php reading the binary`` |
|     - |   29 | ` * behind it as code -- so the whole format rests on that one language feature,` |
|     - |   30 | ` * and it is why the compiler learned it in the same commit.` |
|     - |   31 | ` *` |
|     - |   32 | ` * That shape is the reason this extension matters out of proportion to its` |
|     - |   33 | `` * size: `composer.phar` and `phpunit.phar` are how nearly everyone actually`` |
|     - |   34 | ` * INSTALLS those two tools, and neither could be run at all without it.` |
|     - |   35 | ` *` |
|     - |   36 | `` * WHAT AN ARCHIVE IS HERE. One `phl_phar` per open archive, holding the whole`` |
|     - |   37 | ` * FILE in memory and an entry table over it. php keeps the file open and seeks;` |
|     - |   38 | ` * this reads it once, which costs the archive's size in memory (6MB for` |
|     - |   39 | ` * phpunit.phar) and buys a reader that works over any stream the engine can` |
|     - |   40 | ` * open -- an archive inside another archive included.` |
|     - |   41 | ` *` |
|     - |   42 | ` * THREE FORMATS, ONE MODEL. php's Phar reads its own format, ustar TAR and ZIP,` |
|     - |   43 | ` * and a PharData is the same reader with execution turned off. The three` |
|     - |   44 | ` * readers below fill the same entry table, so everything above them --- the` |
|     - |   45 | ` * wrapper, the classes, the iteration -- is written once.` |
|     - |   46 | ` *` |
|     - |   47 | ` * WHAT IS REPRODUCED, and where it is worth saying why:` |
|     - |   48 | ` *` |
|     - |   49 | ` *   - The MANIFEST, exactly: php's api version, the global and per-entry flag` |
|     - |   50 | ` *     words, the alias, the serialized metadata (both levels), and the` |
|     - |   51 | `` *     signature block whose trailing `GBMB` is what says an archive is signed.`` |
|     - |   52 | ``  *   - The SIGNATURE is verified on open, as php does with `phar.require_hash` `` |
|     - |   53 | ` *     on: a corrupt archive is refused rather than half-read.` |
|     - |   54 | `` *   - `phar.readonly`, php's own default, which is why every write door here`` |
|     - |   55 | ` *     answers a refusal until the ini is turned off -- and the refusals are` |
|     - |   56 | ` *     php's own five different sentences, one per door.` |
|     - |   57 | ` *   - The dual nature of a Phar OBJECT: it is an archive AND a directory` |
|     - |   58 | `` *     iterator over its own entries (php's `spl_filesystem_object`), so`` |
|     - |   59 | `` *     `getPath()` answers the archive while `getFilename()` answers whichever`` |
|     - |   60 | ` *     entry the cursor is on.` |
|     - |   61 | ` *` |
|     - |   62 | ` * WHAT IS NOT HERE, and why each is honest rather than missing:` |
|     - |   63 | ` *` |
|     - |   64 | ` *   - OpenSSL signatures. php refuses them too when its own build has no` |
|     - |   65 | `` *     openssl (`getSupportedSignatures()` lists what a build can do, and this`` |
|     - |   66 | ` *     one lists the four hashes it has).` |
|     - |   67 | `` *   - BZ2 compression, for the same reason: `Phar::canCompress(Phar::BZ2)` is`` |
|     - |   68 | ` *     false here exactly as it is in a php built without ext/bz2.` |
|     - |   69 | `` *   - `webPhar()` and `mungServer()`, which route a WEB request into an`` |
|     - |   70 | ` *     archive. They are a SAPI feature rather than an archive one.` |
|     - |   71 | ` */` |
|     - |   72 | `#define PHAR_FORMAT_PHAR 1` |
|     - |   73 | `#define PHAR_FORMAT_TAR  2` |
|     - |   74 | `#define PHAR_FORMAT_ZIP  3` |
|     - |   75 | `/* php's Phar:: class constants, which are also its wire values. */` |
|     - |   76 | `#define PHAR_C_NONE   0x0000` |
|     - |   77 | `#define PHAR_C_GZ     0x1000` |
|     - |   78 | `#define PHAR_C_BZ2    0x2000` |
|     - |   79 | `#define PHAR_C_MASK   0xF000` |
|     - |   80 | `/* The manifest's global flags. */` |
|     - |   81 | `#define PHAR_GF_SIGNED 0x00010000` |
|     - |   82 | `/* Signature kinds. These are php's Phar::MD5/SHA1/SHA256/SHA512 class constants` |
|     - |   83 | ` * AND the value written in the trailing block -- one number, two faces. */` |
|     - |   84 | `#define PHAR_SIG_MD5    1` |
|     - |   85 | `#define PHAR_SIG_SHA1   2` |
|     - |   86 | `#define PHAR_SIG_SHA256 3` |
|     - |   87 | `#define PHAR_SIG_SHA512 4` |
|     - |   88 | `#define PHAR_SIG_OPENSSL 16` |
|     - |   89 | `#define PHAR_SIG_OPENSSL_SHA256 17` |
|     - |   90 | `#define PHAR_SIG_OPENSSL_SHA512 18` |
|     - |   91 | `/* php's own api version for the archives it writes: 1.1.1 in the class's` |
|     - |   92 | `` * `apiVersion()`, and 1.1.0 in the manifest word (php stores the api version it`` |
|     - |   93 | ` * WRITES, which is one revision behind the reader's). */` |
|     - |   94 | `#define PHAR_API_WORD   0x1100` |
|     - |   95 | `#define PHAR_API_STRING "1.1.1"` |
|     - |   96 |  |
|     - |   97 | `typedef struct phl_phar phl_phar;` |
|     - |   98 | `typedef struct phl_phar_ent phl_phar_ent;` |
|     - |   99 | `/* One member of an archive. A DIRECTORY entry has no bytes; php writes them for` |
|     - |  100 | ` * an explicitly-added empty directory and infers every other directory from the` |
|     - |  101 | `` * names, which is why `is_dir('phar://x.phar/src')` is true in an archive that`` |
|     - |  102 | `` * has no `src` entry at all. */`` |
|     - |  103 | `struct phl_phar_ent {` |
|     - |  104 | ``	SyBlob sName;        /* the name inside the archive, `/`-separated, no leading slash */`` |
|     - |  105 | `	SyBlob sData;        /* the UNCOMPRESSED bytes, filled on first read */` |
|     - |  106 | `	SyBlob sMeta;        /* php-serialized per-entry metadata, or empty */` |
|     - |  107 | `	sxu32 nSize;         /* uncompressed length */` |
|     - |  108 | `	sxu32 nCompSize;     /* stored length */` |
|     - |  109 | `	sxu32 nCrc;          /* crc32 of the uncompressed bytes */` |
|     - |  110 | `	sxu32 nFlags;        /* permission bits + the compression nibble */` |
|     - |  111 | `	sxi64 iTime;         /* modification time */` |
|     - |  112 | `	sxi64 iOffset;       /* where the stored bytes start in the archive file */` |
|     - |  113 | `	sxu8 bLoaded;        /* sData holds the bytes */` |
|     - |  114 | `	sxu8 bDir;           /* an explicit directory entry */` |
|     - |  115 | `	phl_phar_ent *pNext; /* manifest order */` |
|     - |  116 | `};` |
|     - |  117 | `struct phl_phar {` |
|     - |  118 | `	ph7_vm *pVm;` |
|     - |  119 | `	SyBlob sPath;        /* the archive file, as the engine resolved it */` |
|     - |  120 | `	SyBlob sAlias;       /* the alias its manifest carries, or one mapPhar set */` |
|     - |  121 | `	SyBlob sStub;        /* everything before the manifest */` |
|     - |  122 | `	SyBlob sMeta;        /* php-serialized archive metadata */` |
|     - |  123 | `	SyBlob sFile;        /* the whole archive, as read */` |
|     - |  124 | `	SyBlob sSig;         /* the signature bytes, as stored */` |
|     - |  125 | `	SyBlob sPrivKey;     /* the PEM setSignatureAlgorithm() was handed, for an OpenSSL` |
|     - |  126 | `	                      * signature -- the only kind that needs a key to WRITE one */` |
|     - |  127 | `	const char *zSigReason;  /* why a signature check failed, in php's own words */` |
|     - |  128 | `	int iFormat;` |
|     - |  129 | `	int iSigType;` |
|     - |  130 | `	int bData;           /* a PharData: never executable */` |
|     - |  131 | `	int bAliasFromManifest;` |
|     - |  132 | `	sxu32 nApi;` |
|     - |  133 | `	phl_phar_ent *pFirst,*pLast;` |
|     - |  134 | `	sxu32 nEnt;` |
|     - |  135 | `	phl_phar *pNext;     /* the VM's registry chain */` |
|     - |  136 | `};` |
|     - |  137 | `/* ------------------------------------------------------------------ */` |
|     - |  138 | `/* The registry                                                        */` |
|     - |  139 | `/* ------------------------------------------------------------------ */` |
|    79 |  140 | `static void PharEntFree(ph7_vm *pVm,phl_phar_ent *pEnt)` |
|   ! 0 |  141 | `{` |
|    79 |  142 | `	SyBlobRelease(&pEnt->sName);` |
|    79 |  143 | `	SyBlobRelease(&pEnt->sData);` |
|    79 |  144 | `	SyBlobRelease(&pEnt->sMeta);` |
|    79 |  145 | `	SyMemBackendFree(&pVm->sAllocator,pEnt);` |
|    79 |  146 | `}` |
|    43 |  147 | `static void PharFree(phl_phar *pPhar)` |
|   ! 0 |  148 | `{` |
|    43 |  149 | `	ph7_vm *pVm = pPhar->pVm;` |
|    43 |  150 | `	phl_phar_ent *pEnt = pPhar->pFirst;` |
|   110 |  151 | `	while( pEnt ){` |
|    67 |  152 | `		phl_phar_ent *pNext = pEnt->pNext;` |
|    67 |  153 | `		PharEntFree(pVm,pEnt);` |
|    67 |  154 | `		pEnt = pNext;` |
|   ! 0 |  155 | `	}` |
|    43 |  156 | `	SyBlobRelease(&pPhar->sPath);` |
|    43 |  157 | `	SyBlobRelease(&pPhar->sAlias);` |
|    43 |  158 | `	SyBlobRelease(&pPhar->sStub);` |
|    43 |  159 | `	SyBlobRelease(&pPhar->sMeta);` |
|    43 |  160 | `	SyBlobRelease(&pPhar->sFile);` |
|    43 |  161 | `	SyBlobRelease(&pPhar->sSig);` |
|    43 |  162 | `	SyBlobRelease(&pPhar->sPrivKey);` |
|    43 |  163 | `	SyMemBackendFree(&pVm->sAllocator,pPhar);` |
|    43 |  164 | `}` |
|     - |  165 | `/*` |
|     - |  166 | ` * Every archive this run opened, freed together. php's own cache is per-request` |
|     - |  167 | ` * and behaves the same way: an archive opened by one request is not the next` |
|     - |  168 | ` * one's, which for the -S server is the difference between a stale manifest and` |
|     - |  169 | ` * a fresh read.` |
|     - |  170 | ` */` |
|  5645 |  171 | `static void PharVmSweep(ph7_vm *pVm)` |
|     5 |  172 | `{` |
|  5650 |  173 | `	phl_phar *p = (phl_phar *)pVm->pPhars;` |
|  5685 |  174 | `	while( p ){` |
|    35 |  175 | `		phl_phar *pNext = p->pNext;` |
|    35 |  176 | `		PharFree(p);` |
|    35 |  177 | `		p = pNext;` |
|   ! 0 |  178 | `	}` |
|  5650 |  179 | `	pVm->pPhars = 0;` |
|  5650 |  180 | `	SyBlobRelease(&pVm->sPharRunning);` |
|  5650 |  181 | `	SyBlobRelease(&pVm->sPharErr);` |
|  5650 |  182 | `}` |
|    16 |  183 | `PH7_PRIVATE void PH7_PharVmReset(ph7_vm *pVm)` |
|   ! 0 |  184 | `{` |
|    16 |  185 | `	PharVmSweep(&(*pVm));` |
|    16 |  186 | `}` |
|  5629 |  187 | `PH7_PRIVATE void PH7_PharVmRelease(ph7_vm *pVm)` |
|     5 |  188 | `{` |
|  5634 |  189 | `	PharVmSweep(&(*pVm));` |
|  5634 |  190 | `}` |
|     - |  191 | `/* The archive already open under this exact path, or 0. */` |
|   951 |  192 | `static phl_phar * PharFindByPath(ph7_vm *pVm,const char *zPath,int nPath)` |
|   ! 0 |  193 | `{` |
|     - |  194 | `	phl_phar *p;` |
|  2340 |  195 | `	for( p = (phl_phar *)pVm->pPhars ; p ; p = p->pNext ){` |
|  1584 |  196 | `		if( (int)SyBlobLength(&p->sPath) == nPath` |
|  1155 |  197 | `		 && SyMemcmp(SyBlobData(&p->sPath),zPath,(sxu32)nPath) == 0 ){` |
|   195 |  198 | `			return p;` |
|     - |  199 | `		}` |
|   952 |  200 | `	}` |
|   756 |  201 | `	return 0;` |
|   616 |  202 | `}` |
|     - |  203 | `` /* ...or under this alias, which is what `phar://composer.phar/bin/composer` `` |
|     - |  204 | `` * names once the stub has run `Phar::mapPhar('composer.phar')`. */`` |
|   190 |  205 | `static phl_phar * PharFindByAlias(ph7_vm *pVm,const char *zAlias,int nAlias)` |
|   ! 0 |  206 | `{` |
|     - |  207 | `	phl_phar *p;` |
|   190 |  208 | `	if( nAlias < 1 ){` |
|   184 |  209 | `		return 0;` |
|     - |  210 | `	}` |
|     6 |  211 | `	for( p = (phl_phar *)pVm->pPhars ; p ; p = p->pNext ){` |
|     6 |  212 | `		if( (int)SyBlobLength(&p->sAlias) == nAlias` |
|     6 |  213 | `		 && SyMemcmp(SyBlobData(&p->sAlias),zAlias,(sxu32)nAlias) == 0 ){` |
|     6 |  214 | `			return p;` |
|     - |  215 | `		}` |
|   ! 0 |  216 | `	}` |
|   ! 0 |  217 | `	return 0;` |
|    87 |  218 | `}` |
|     - |  219 | `/*` |
|     - |  220 | ` * The name an entry is looked up under. php normalizes a phar path before it` |
|     - |  221 | `` * looks: `bin/../src/bootstrap.php` IS `src/bootstrap.php` inside the archive,`` |
|     - |  222 | ``  * and every real stub relies on it -- composer's `require __DIR__.'/../src/…'` `` |
|     - |  223 | ` * is exactly that shape, since __DIR__ inside an archive is a phar:// url.` |
|     - |  224 | ` */` |
|   425 |  225 | `static void PharNormalize(const char *zIn,int nIn,SyBlob *pOut)` |
|   ! 0 |  226 | `{` |
|   425 |  227 | `	int i = 0;` |
|   425 |  228 | `	SyBlobReset(pOut);` |
|  1020 |  229 | `	while( i < nIn ){` |
|     - |  230 | `		int nSeg;` |
|   920 |  231 | `		while( i < nIn && zIn[i] == '/' ){` |
|   325 |  232 | `			i++;` |
|   ! 0 |  233 | `		}` |
|   595 |  234 | `		nSeg = 0;` |
|  3144 |  235 | `		while( i + nSeg < nIn && zIn[i+nSeg] != '/' ){` |
|  2549 |  236 | `			nSeg++;` |
|   ! 0 |  237 | `		}` |
|   595 |  238 | `		if( nSeg == 0 ){` |
|   ! 0 |  239 | `			break;` |
|     - |  240 | `		}` |
|   595 |  241 | `		if( nSeg == 1 && zIn[i] == '.' ){` |
|     4 |  242 | `			i += nSeg;` |
|     4 |  243 | `			continue;` |
|     - |  244 | `		}` |
|   591 |  245 | `		if( nSeg == 2 && zIn[i] == '.' && zIn[i+1] == '.' ){` |
|     - |  246 | `			/* Step back over the last segment kept, if any. */` |
|    18 |  247 | `			sxu32 n = SyBlobLength(pOut);` |
|    84 |  248 | `			while( n > 0 && ((const char *)SyBlobData(pOut))[n-1] != '/' ){` |
|    66 |  249 | `				n--;` |
|   ! 0 |  250 | `			}` |
|    18 |  251 | `			pOut->nByte = n > 0 ? n - 1 : 0;` |
|    18 |  252 | `			i += nSeg;` |
|    18 |  253 | `			continue;` |
|     - |  254 | `		}` |
|   573 |  255 | `		if( SyBlobLength(pOut) > 0 ){` |
|   130 |  256 | `			SyBlobAppend(pOut,"/",1);` |
|    61 |  257 | `		}` |
|   573 |  258 | `		SyBlobAppend(pOut,&zIn[i],(sxu32)nSeg);` |
|   573 |  259 | `		i += nSeg;` |
|   ! 0 |  260 | `	}` |
|   425 |  261 | `}` |
|   306 |  262 | `static phl_phar_ent * PharFindEnt(phl_phar *pPhar,const char *zName,int nName)` |
|   ! 0 |  263 | `{` |
|     - |  264 | `	phl_phar_ent *pEnt;` |
|     - |  265 | `	SyBlob sNorm;` |
|   306 |  266 | `	SyBlobInit(&sNorm,&pPhar->pVm->sAllocator);` |
|   306 |  267 | `	PharNormalize(zName,nName,&sNorm);` |
|   306 |  268 | `	zName = (const char *)SyBlobData(&sNorm);` |
|   306 |  269 | `	nName = (int)SyBlobLength(&sNorm);` |
|   306 |  270 | `	pEnt = 0;` |
|     - |  271 | `	{` |
|     - |  272 | `		phl_phar_ent *p;` |
|   632 |  273 | `		for( p = pPhar->pFirst ; p ; p = p->pNext ){` |
|   492 |  274 | `			if( (int)SyBlobLength(&p->sName) == nName` |
|   335 |  275 | `			 && SyMemcmp(SyBlobData(&p->sName),zName,(sxu32)nName) == 0 ){` |
|   166 |  276 | `				pEnt = p;` |
|   166 |  277 | `				break;` |
|     - |  278 | `			}` |
|   160 |  279 | `		}` |
|     - |  280 | `	}` |
|   306 |  281 | `	SyBlobRelease(&sNorm);` |
|   306 |  282 | `	return pEnt;` |
|   ! 0 |  283 | `}` |
|     - |  284 | `/* Is this name a DIRECTORY in the archive -- an entry of its own, or the prefix` |
|     - |  285 | ` * of one? php infers the second kind, which is why every archive has directories` |
|     - |  286 | ` * its manifest never mentions. */` |
|    30 |  287 | `static int PharIsDir(phl_phar *pPhar,const char *zName,int nName)` |
|   ! 0 |  288 | `{` |
|     - |  289 | `	phl_phar_ent *pEnt;` |
|     - |  290 | `	SyBlob sNorm;` |
|    30 |  291 | `	int bAnswer = 0;` |
|    30 |  292 | `	SyBlobInit(&sNorm,&pPhar->pVm->sAllocator);` |
|    30 |  293 | `	PharNormalize(zName,nName,&sNorm);` |
|    30 |  294 | `	zName = (const char *)SyBlobData(&sNorm);` |
|    30 |  295 | `	nName = (int)SyBlobLength(&sNorm);` |
|    30 |  296 | `	if( nName < 1 ){` |
|   ! 0 |  297 | `		SyBlobRelease(&sNorm);` |
|   ! 0 |  298 | `		return 1;   /* the archive root */` |
|     - |  299 | `	}` |
|    98 |  300 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|    78 |  301 | `		const char *z = (const char *)SyBlobData(&pEnt->sName);` |
|    78 |  302 | `		sxu32 n = SyBlobLength(&pEnt->sName);` |
|    78 |  303 | `		if( pEnt->bDir && (int)n == nName && SyMemcmp(z,zName,(sxu32)nName) == 0 ){` |
|     2 |  304 | `			bAnswer = 1;` |
|     2 |  305 | `			break;` |
|     - |  306 | `		}` |
|    76 |  307 | `		if( (int)n > nName && z[nName] == '/' && SyMemcmp(z,zName,(sxu32)nName) == 0 ){` |
|     8 |  308 | `			bAnswer = 1;` |
|     8 |  309 | `			break;` |
|     - |  310 | `		}` |
|    34 |  311 | `	}` |
|    30 |  312 | `	SyBlobRelease(&sNorm);` |
|    30 |  313 | `	return bAnswer;` |
|    15 |  314 | `}` |
|     - |  315 | `/* ------------------------------------------------------------------ */` |
|     - |  316 | `/* Reading the file                                                    */` |
|     - |  317 | `/* ------------------------------------------------------------------ */` |
|     - |  318 | ``/* Drop the last byte of a blob: the trailing `/` that marks a directory entry`` |
|     - |  319 | ` * in every one of the three formats. */` |
|   ! 0 |  320 | `static void PharChopSlash(SyBlob *pBlob)` |
|   ! 0 |  321 | `{` |
|   ! 0 |  322 | `	if( SyBlobLength(pBlob) > 0 ){` |
|   ! 0 |  323 | `		pBlob->nByte--;` |
|   ! 0 |  324 | `	}` |
|   ! 0 |  325 | `}` |
|   170 |  326 | `static sxu32 PharGet32(const unsigned char *z)` |
|   ! 0 |  327 | `{` |
|   170 |  328 | `	return (sxu32)z[0] \| ((sxu32)z[1] << 8) \| ((sxu32)z[2] << 16) \| ((sxu32)z[3] << 24);` |
|   ! 0 |  329 | `}` |
|   ! 0 |  330 | `static sxu32 PharGet16(const unsigned char *z)` |
|   ! 0 |  331 | `{` |
|   ! 0 |  332 | `	return (sxu32)z[0] \| ((sxu32)z[1] << 8);` |
|   ! 0 |  333 | `}` |
|     - |  334 | `/* php stores the manifest's api version MSB first, alone among its fields. */` |
|     9 |  335 | `static sxu32 PharGet16BE(const unsigned char *z)` |
|   ! 0 |  336 | `{` |
|     9 |  337 | `	return ((sxu32)z[0] << 8) \| (sxu32)z[1];` |
|   ! 0 |  338 | `}` |
|     - |  339 | `/*` |
|     - |  340 | ` * Where the manifest starts: php looks for the halt token and then steps over` |
|     - |  341 | `` * an optional ` ?>` and ONE line ending, which is what its own writer emits.`` |
|     - |  342 | ` * A file with no token at all is not a phar-format archive.` |
|     - |  343 | ` */` |
|    12 |  344 | `static int PharFindManifest(const unsigned char *zFile,sxu32 nFile,sxu32 *pnOff)` |
|   ! 0 |  345 | `{` |
|     - |  346 | `	static const char zTok[] = "__HALT_COMPILER();";` |
|    12 |  347 | `	sxu32 nTok = (sxu32)sizeof(zTok)-1;` |
|     - |  348 | `	sxu32 i;` |
|  3239 |  349 | `	for( i = 0 ; i + nTok <= nFile ; ++i ){` |
|  3236 |  350 | `		if( zFile[i] != '_' ){` |
|  3227 |  351 | `			continue;` |
|     - |  352 | `		}` |
|     9 |  353 | `		if( SyStrnicmp((const char *)&zFile[i],zTok,nTok) != 0 ){` |
|   ! 0 |  354 | `			continue;` |
|     - |  355 | `		}` |
|     9 |  356 | `		i += nTok;` |
|     - |  357 | ``		/* php's writer puts ` ?>` and a line ending here; a hand-made stub may`` |
|     - |  358 | `		 * put any run of blanks, or nothing at all. */` |
|    22 |  359 | `		while( i < nFile && (zFile[i] == ' ' \|\| zFile[i] == '\t') ){` |
|     9 |  360 | `			i++;` |
|   ! 0 |  361 | `		}` |
|     9 |  362 | `		if( i + 1 < nFile && zFile[i] == '?' && zFile[i+1] == '>' ){` |
|     9 |  363 | `			i += 2;` |
|     4 |  364 | `		}` |
|     9 |  365 | `		if( i < nFile && zFile[i] == '\r' ){` |
|     9 |  366 | `			i++;` |
|     4 |  367 | `		}` |
|     9 |  368 | `		if( i < nFile && zFile[i] == '\n' ){` |
|     9 |  369 | `			i++;` |
|     4 |  370 | `		}` |
|     9 |  371 | `		*pnOff = i;` |
|     9 |  372 | `		return 1;` |
|   ! 0 |  373 | `	}` |
|     3 |  374 | `	return 0;` |
|     5 |  375 | `}` |
|    79 |  376 | `static phl_phar_ent * PharNewEnt(phl_phar *pPhar,const char *zName,int nName)` |
|   ! 0 |  377 | `{` |
|    79 |  378 | `	ph7_vm *pVm = pPhar->pVm;` |
|    79 |  379 | `	phl_phar_ent *pEnt = (phl_phar_ent *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar_ent));` |
|    79 |  380 | `	if( pEnt == 0 ){` |
|   ! 0 |  381 | `		return 0;` |
|     - |  382 | `	}` |
|    79 |  383 | `	SyZero(pEnt,sizeof(*pEnt));` |
|    79 |  384 | `	SyBlobInit(&pEnt->sName,&pVm->sAllocator);` |
|    79 |  385 | `	SyBlobInit(&pEnt->sData,&pVm->sAllocator);` |
|    79 |  386 | `	SyBlobInit(&pEnt->sMeta,&pVm->sAllocator);` |
|    79 |  387 | `	while( nName > 0 && zName[0] == '/' ){` |
|   ! 0 |  388 | `		zName++; nName--;` |
|   ! 0 |  389 | `	}` |
|    79 |  390 | `	SyBlobAppend(&pEnt->sName,zName,(sxu32)nName);` |
|    79 |  391 | `	if( pPhar->pLast ){` |
|    38 |  392 | `		pPhar->pLast->pNext = pEnt;` |
|    17 |  393 | `	}else{` |
|    41 |  394 | `		pPhar->pFirst = pEnt;` |
|     - |  395 | `	}` |
|    79 |  396 | `	pPhar->pLast = pEnt;` |
|    79 |  397 | `	pPhar->nEnt++;` |
|    79 |  398 | `	return pEnt;` |
|    35 |  399 | `}` |
|     - |  400 | `/*` |
|     - |  401 | ` * php's own format. Everything is little-endian but the api version, lengths` |
|     - |  402 | ` * are unsigned 32-bit, and the per-entry records come first as a block with the` |
|     - |  403 | ` * stored bytes behind them in the same order.` |
|     - |  404 | ` */` |
|     9 |  405 | `static int PharParsePhar(phl_phar *pPhar,const unsigned char *zFile,sxu32 nFile,` |
|     - |  406 | `	sxu32 nOff,const char **pzErr)` |
|   ! 0 |  407 | `{` |
|     - |  408 | `	sxu32 nManifest,nFiles,nFlags,nAliasLen,nMetaLen,i,q,nDataAt;` |
|     9 |  409 | `	if( nOff > nFile \|\| nFile - nOff < 18 ){` |
|   ! 0 |  410 | `		*pzErr = "internal corruption of phar (truncated manifest header)";` |
|   ! 0 |  411 | `		return -1;` |
|     - |  412 | `	}` |
|     9 |  413 | `	nManifest = PharGet32(&zFile[nOff]);` |
|     9 |  414 | `	nFiles = PharGet32(&zFile[nOff+4]);` |
|     9 |  415 | `	pPhar->nApi = PharGet16BE(&zFile[nOff+8]);` |
|     9 |  416 | `	nFlags = PharGet32(&zFile[nOff+10]);` |
|     9 |  417 | `	nAliasLen = PharGet32(&zFile[nOff+14]);` |
|     9 |  418 | `	if( nManifest < 1 \|\| nFile - nOff < 4 \|\| nManifest > nFile - nOff - 4 ){` |
|   ! 0 |  419 | `		*pzErr = "internal corruption of phar (truncated manifest header)";` |
|   ! 0 |  420 | `		return -1;` |
|     - |  421 | `	}` |
|     9 |  422 | `	q = nOff + 18;` |
|     9 |  423 | `	if( q > nFile \|\| nAliasLen > nFile - q ){` |
|   ! 0 |  424 | `		*pzErr = "internal corruption of phar (truncated manifest header)";` |
|   ! 0 |  425 | `		return -1;` |
|     - |  426 | `	}` |
|     9 |  427 | `	if( nAliasLen > 0 ){` |
|   ! 0 |  428 | `		SyBlobAppend(&pPhar->sAlias,&zFile[q],nAliasLen);` |
|   ! 0 |  429 | `		pPhar->bAliasFromManifest = 1;` |
|   ! 0 |  430 | `	}` |
|     9 |  431 | `	q += nAliasLen;` |
|     9 |  432 | `	if( q > nFile \|\| nFile - q < 4 ){` |
|   ! 0 |  433 | `		*pzErr = "internal corruption of phar (truncated manifest header)";` |
|   ! 0 |  434 | `		return -1;` |
|     - |  435 | `	}` |
|     9 |  436 | `	nMetaLen = PharGet32(&zFile[q]);` |
|     9 |  437 | `	q += 4;` |
|     9 |  438 | `	if( nMetaLen > nFile - q ){` |
|   ! 0 |  439 | `		*pzErr = "internal corruption of phar (truncated manifest header)";` |
|   ! 0 |  440 | `		return -1;` |
|     - |  441 | `	}` |
|     9 |  442 | `	if( nMetaLen > 0 ){` |
|     2 |  443 | `		SyBlobAppend(&pPhar->sMeta,&zFile[q],nMetaLen);` |
|     1 |  444 | `	}` |
|     9 |  445 | `	q += nMetaLen;` |
|     - |  446 | `	/* The entry records, then their bytes in the same order. Every bound below` |
|     - |  447 | ``	 * is written as a REMAINDER (`n > nFile - q`) rather than a sum: the lengths`` |
|     - |  448 | ``	 * come out of the file being parsed, and `q + n > nFile` wraps for one near`` |
|     - |  449 | `	 * 2^32 and lets the read through. */` |
|     9 |  450 | `	nDataAt = nOff + 4 + nManifest;` |
|    25 |  451 | `	for( i = 0 ; i < nFiles ; ++i ){` |
|     - |  452 | `		phl_phar_ent *pEnt;` |
|     - |  453 | `		sxu32 nNameLen,nEntMeta;` |
|    16 |  454 | `		if( q > nFile \|\| nFile - q < 4 ){` |
|   ! 0 |  455 | `			*pzErr = "internal corruption of phar (truncated manifest entry)";` |
|   ! 0 |  456 | `			return -1;` |
|     - |  457 | `		}` |
|    16 |  458 | `		nNameLen = PharGet32(&zFile[q]);` |
|    16 |  459 | `		q += 4;` |
|    16 |  460 | `		if( nNameLen > nFile - q \|\| nFile - q - nNameLen < 24 ){` |
|   ! 0 |  461 | `			*pzErr = "internal corruption of phar (truncated manifest entry)";` |
|   ! 0 |  462 | `			return -1;` |
|     - |  463 | `		}` |
|    16 |  464 | `		pEnt = PharNewEnt(pPhar,(const char *)&zFile[q],(int)nNameLen);` |
|    16 |  465 | `		if( pEnt == 0 ){` |
|   ! 0 |  466 | `			*pzErr = "internal corruption of phar (out of memory)";` |
|   ! 0 |  467 | `			return -1;` |
|     - |  468 | `		}` |
|    16 |  469 | `		q += nNameLen;` |
|    16 |  470 | `		pEnt->nSize = PharGet32(&zFile[q]);` |
|    16 |  471 | `		pEnt->iTime = (sxi64)PharGet32(&zFile[q+4]);` |
|    16 |  472 | `		pEnt->nCompSize = PharGet32(&zFile[q+8]);` |
|    16 |  473 | `		pEnt->nCrc = PharGet32(&zFile[q+12]);` |
|    16 |  474 | `		pEnt->nFlags = PharGet32(&zFile[q+16]);` |
|    16 |  475 | `		nEntMeta = PharGet32(&zFile[q+20]);` |
|    16 |  476 | `		q += 24;` |
|    16 |  477 | `		if( nEntMeta > nFile - q ){` |
|   ! 0 |  478 | `			*pzErr = "internal corruption of phar (truncated manifest entry)";` |
|   ! 0 |  479 | `			return -1;` |
|     - |  480 | `		}` |
|    16 |  481 | `		if( nEntMeta > 0 ){` |
|     2 |  482 | `			SyBlobAppend(&pEnt->sMeta,&zFile[q],nEntMeta);` |
|     1 |  483 | `		}` |
|    16 |  484 | `		q += nEntMeta;` |
|    16 |  485 | `		pEnt->iOffset = (sxi64)nDataAt;` |
|    16 |  486 | `		if( nDataAt > nFile \|\| pEnt->nCompSize > nFile - nDataAt ){` |
|   ! 0 |  487 | `			*pzErr = "internal corruption of phar (truncated entry)";` |
|   ! 0 |  488 | `			return -1;` |
|     - |  489 | `		}` |
|    16 |  490 | `		nDataAt += pEnt->nCompSize;` |
|     - |  491 | `		/* php marks a directory entry by a trailing slash in its NAME. */` |
|    16 |  492 | `		if( SyBlobLength(&pEnt->sName) > 0 ){` |
|    16 |  493 | `			char *zN = (char *)SyBlobData(&pEnt->sName);` |
|    16 |  494 | `			sxu32 nN = SyBlobLength(&pEnt->sName);` |
|    16 |  495 | `			if( zN[nN-1] == '/' ){` |
|   ! 0 |  496 | `				pEnt->bDir = 1;` |
|   ! 0 |  497 | `				PharChopSlash(&pEnt->sName);` |
|   ! 0 |  498 | `			}` |
|     7 |  499 | `		}` |
|     7 |  500 | `	}` |
|     9 |  501 | `	if( nFlags & PHAR_GF_SIGNED ){` |
|     - |  502 | `		/* The signature block sits at the very end: the digest, its kind and` |
|     - |  503 | ``		 * `GBMB`. */`` |
|     9 |  504 | `		if( nFile >= 8 && SyMemcmp(&zFile[nFile-4],"GBMB",4) == 0 ){` |
|     9 |  505 | `			sxu32 nSigType = PharGet32(&zFile[nFile-8]);` |
|     9 |  506 | `			sxu32 nSigLen = 0;` |
|     9 |  507 | `			pPhar->iSigType = (int)nSigType;` |
|     9 |  508 | `			switch( nSigType ){` |
|   ! 0 |  509 | `			case PHAR_SIG_MD5:    nSigLen = 16; break;` |
|   ! 0 |  510 | `			case PHAR_SIG_SHA1:   nSigLen = 20; break;` |
|     1 |  511 | `			case PHAR_SIG_SHA256: nSigLen = 32; break;` |
|     4 |  512 | `			case PHAR_SIG_SHA512: nSigLen = 64; break;` |
|     2 |  513 | `			case PHAR_SIG_OPENSSL:` |
|     - |  514 | `			case PHAR_SIG_OPENSSL_SHA256:` |
|     - |  515 | `			case PHAR_SIG_OPENSSL_SHA512:` |
|     - |  516 | `				/* An RSA signature is as wide as the key, so its block carries` |
|     - |  517 | ``				 * its own LENGTH: `<signature><uint32 len><uint32 flags>GBMB`.`` |
|     - |  518 | `				 * That is four bytes more than every hash block, and reading it` |
|     - |  519 | `				 * as one is how an OpenSSL-signed archive used to be accepted` |
|     - |  520 | `				 * with no verification at all. */` |
|     4 |  521 | `				if( nFile >= 12 ){` |
|     4 |  522 | `					sxu32 nDeclared = PharGet32(&zFile[nFile-12]);` |
|     4 |  523 | `					if( nDeclared > 0 && nDeclared <= 65536 && nFile >= nDeclared + 12 ){` |
|     4 |  524 | `						SyBlobAppend(&pPhar->sSig,&zFile[nFile-12-nDeclared],nDeclared);` |
|     2 |  525 | `					}` |
|     2 |  526 | `				}` |
|     4 |  527 | `				nSigLen = 0;` |
|     4 |  528 | `				break;` |
|   ! 0 |  529 | `			default: break;` |
|     - |  530 | `			}` |
|     9 |  531 | `			if( nSigLen > 0 && nFile >= nSigLen + 8 ){` |
|     5 |  532 | `				SyBlobAppend(&pPhar->sSig,&zFile[nFile-8-nSigLen],nSigLen);` |
|     2 |  533 | `			}` |
|     4 |  534 | `		}` |
|     4 |  535 | `	}` |
|     9 |  536 | `	SyBlobAppend(&pPhar->sStub,zFile,nOff);` |
|     9 |  537 | `	pPhar->iFormat = PHAR_FORMAT_PHAR;` |
|     9 |  538 | `	return 0;` |
|     4 |  539 | `}` |
|     - |  540 | `#ifdef PH7_ENABLE_OPENSSL` |
|     - |  541 | `/*` |
|     - |  542 | ` * An RSA signature over the archive, verified against the key in` |
|     - |  543 | `` * `<archive>.pubkey`. php's three flavours differ only in the digest: 16 is`` |
|     - |  544 | ` * SHA-1, 17 SHA-256, 18 SHA-512.` |
|     - |  545 | ` *` |
|     - |  546 | ` * The failure REASONS are php's own two sentences and the caller prints them` |
|     - |  547 | `` * verbatim: a key file that is missing or will not parse is `openssl public`` |
|     - |  548 | `` * key could not be read`, and a signature that does not check out is `broken`` |
|     - |  549 | `` * openssl signature` -- not the plain `broken signature` the hash algorithms`` |
|     - |  550 | ` * give.` |
|     - |  551 | ` */` |
|     4 |  552 | `static const char * PharOpenSslReason(phl_phar *pPhar,sxu32 nCovered)` |
|   ! 0 |  553 | `{` |
|     - |  554 | `	SyBlob sKey;` |
|     4 |  555 | `	BIO *pBio = 0;` |
|     4 |  556 | `	EVP_PKEY *pKey = 0;` |
|     4 |  557 | `	EVP_MD_CTX *pMdCtx = 0;` |
|     - |  558 | `	const EVP_MD *pMd;` |
|     4 |  559 | `	const char *zReason = "openssl public key could not be read";` |
|     4 |  560 | `	int nPath = (int)SyBlobLength(&pPhar->sPath);` |
|     - |  561 | `	char *zPub;` |
|     4 |  562 | `	SyBlobInit(&sKey,&pPhar->pVm->sAllocator);` |
|     4 |  563 | `	zPub = (char *)SyMemBackendAlloc(&pPhar->pVm->sAllocator,(sxu32)(nPath + 8));` |
|     4 |  564 | `	if( zPub == 0 ){` |
|   ! 0 |  565 | `		SyBlobRelease(&sKey);` |
|   ! 0 |  566 | `		return zReason;` |
|     - |  567 | `	}` |
|     4 |  568 | `	SyMemcpy(SyBlobData(&pPhar->sPath),zPub,(sxu32)nPath);` |
|     4 |  569 | `	SyMemcpy(".pubkey",&zPub[nPath],sizeof(".pubkey"));` |
|     - |  570 | `	{` |
|     - |  571 | `		/* Read the key file the way the ARCHIVE was read -- through the engine's` |
|     - |  572 | `		 * stream layer, so a phar inside a userland wrapper finds its key. */` |
|     4 |  573 | `		const char *zTarget = zPub;` |
|     4 |  574 | `		const ph7_io_stream *pStream = PH7_VmGetStreamDevice(pPhar->pVm,&zTarget,nPath + 7);` |
|     4 |  575 | `		void *pHandle = pStream ? PH7_StreamOpenHandle(pPhar->pVm,pStream,zTarget,` |
|     2 |  576 | `			PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,0) : 0;` |
|     4 |  577 | `		if( pHandle ){` |
|     2 |  578 | `			PH7_StreamReadWholeFile(pHandle,pStream,&sKey);` |
|     2 |  579 | `			PH7_StreamCloseHandle(pStream,pHandle);` |
|     1 |  580 | `		}` |
|     - |  581 | `	}` |
|     4 |  582 | `	SyMemBackendFree(&pPhar->pVm->sAllocator,zPub);` |
|     4 |  583 | `	if( SyBlobLength(&sKey) > 0 ){` |
|     2 |  584 | `		pBio = BIO_new_mem_buf(SyBlobData(&sKey),(int)SyBlobLength(&sKey));` |
|     2 |  585 | `		if( pBio ){` |
|     2 |  586 | `			pKey = PEM_read_bio_PUBKEY(pBio,0,0,0);` |
|     2 |  587 | `			BIO_free(pBio);` |
|     1 |  588 | `		}` |
|     1 |  589 | `	}` |
|     4 |  590 | `	ERR_clear_error();` |
|     4 |  591 | `	if( pKey == 0 ){` |
|     2 |  592 | `		SyBlobRelease(&sKey);` |
|     2 |  593 | `		return zReason;` |
|     - |  594 | `	}` |
|     2 |  595 | `	switch( pPhar->iSigType ){` |
|   ! 0 |  596 | `		case PHAR_SIG_OPENSSL_SHA256: pMd = EVP_sha256(); break;` |
|   ! 0 |  597 | `		case PHAR_SIG_OPENSSL_SHA512: pMd = EVP_sha512(); break;` |
|     2 |  598 | `		default:                      pMd = EVP_sha1();   break;` |
|     - |  599 | `	}` |
|     2 |  600 | `	pMdCtx = EVP_MD_CTX_new();` |
|     2 |  601 | `	zReason = "broken openssl signature";` |
|     2 |  602 | `	if( pMdCtx && EVP_DigestVerifyInit(pMdCtx,0,pMd,0,pKey) == 1` |
|     2 |  603 | `	 && EVP_DigestVerify(pMdCtx,(const unsigned char *)SyBlobData(&pPhar->sSig),` |
|     2 |  604 | `			(size_t)SyBlobLength(&pPhar->sSig),` |
|     3 |  605 | `			(const unsigned char *)SyBlobData(&pPhar->sFile),(size_t)nCovered) == 1 ){` |
|   ! 0 |  606 | `		zReason = 0;` |
|   ! 0 |  607 | `	}` |
|     2 |  608 | `	ERR_clear_error();` |
|     2 |  609 | `	if( pMdCtx ){ EVP_MD_CTX_free(pMdCtx); }` |
|     2 |  610 | `	EVP_PKEY_free(pKey);` |
|     2 |  611 | `	SyBlobRelease(&sKey);` |
|     2 |  612 | `	return zReason;` |
|     2 |  613 | `}` |
|     4 |  614 | `static int PharVerifyOpenSsl(phl_phar *pPhar,sxu32 nCovered,SyBlob *pHashOut)` |
|   ! 0 |  615 | `{` |
|     - |  616 | `	const char *zReason;` |
|     4 |  617 | `	if( SyBlobLength(&pPhar->sSig) < 1 ){` |
|   ! 0 |  618 | `		pPhar->zSigReason = "broken openssl signature";` |
|   ! 0 |  619 | `		return -1;` |
|     - |  620 | `	}` |
|     4 |  621 | `	zReason = PharOpenSslReason(pPhar,nCovered);` |
|     4 |  622 | `	if( zReason ){` |
|     4 |  623 | `		pPhar->zSigReason = zReason;` |
|     4 |  624 | `		return -1;` |
|     - |  625 | `	}` |
|   ! 0 |  626 | `	if( pHashOut ){` |
|     - |  627 | ``		/* getSignature()'s `hash` for an OpenSSL archive is the SIGNATURE`` |
|     - |  628 | `		 * itself, hex-uppercased -- not a digest of anything. */` |
|   ! 0 |  629 | `		SyBlobAppend(pHashOut,SyBlobData(&pPhar->sSig),SyBlobLength(&pPhar->sSig));` |
|   ! 0 |  630 | `	}` |
|   ! 0 |  631 | `	return 0;` |
|     2 |  632 | `}` |
|     - |  633 | `#endif /* PH7_ENABLE_OPENSSL */` |
|     - |  634 | `/*` |
|     - |  635 | ` * php's name for a signature ALGORITHM inside a diagnostic, which is not the` |
|     - |  636 | `` * one `getSignature()` reports: the message says `SHA512` where the array says`` |
|     - |  637 | `` * `SHA-512`.`` |
|     - |  638 | ` */` |
|     6 |  639 | `static const char * PharSigAlgoName(int iSig)` |
|   ! 0 |  640 | `{` |
|     6 |  641 | `	switch( iSig ){` |
|   ! 0 |  642 | `		case PHAR_SIG_MD5:            return "MD5";` |
|   ! 0 |  643 | `		case PHAR_SIG_SHA1:           return "SHA1";` |
|   ! 0 |  644 | `		case PHAR_SIG_SHA256:         return "SHA256";` |
|     2 |  645 | `		case PHAR_SIG_SHA512:         return "SHA512";` |
|     2 |  646 | `		case PHAR_SIG_OPENSSL:` |
|     - |  647 | `		case PHAR_SIG_OPENSSL_SHA256:` |
|     4 |  648 | `		case PHAR_SIG_OPENSSL_SHA512: return "openssl";` |
|   ! 0 |  649 | `		default:                      return "Unknown";` |
|     - |  650 | `	}` |
|     3 |  651 | `}` |
|     - |  652 | `/*` |
|     - |  653 | ` * Everything after the archive's name in php's refusal. It is assembled into` |
|     - |  654 | `` * the VM's own `sPharErr` rather than the record's storage, because the caller`` |
|     - |  655 | ` * FREES the record and then prints this -- the same lifetime trap the wrapper's` |
|     - |  656 | ` * open-failure sentence records.` |
|     - |  657 | ` */` |
|     6 |  658 | `static const char * PharSigFailure(phl_phar *pPhar)` |
|   ! 0 |  659 | `{` |
|     6 |  660 | `	ph7_vm *pVm = pPhar->pVm;` |
|     6 |  661 | `	SyBlobReset(&pVm->sPharErr);` |
|     9 |  662 | `	SyBlobFormat(&pVm->sPharErr,"%s signature could not be verified: %s",` |
|     3 |  663 | `		PharSigAlgoName(pPhar->iSigType),` |
|     6 |  664 | `		pPhar->zSigReason ? pPhar->zSigReason : "broken signature");` |
|     6 |  665 | `	SyBlobNullAppend(&pVm->sPharErr);` |
|     6 |  666 | `	if( SyBlobLength(&pVm->sPharErr) < 2 ){` |
|   ! 0 |  667 | `		return "signature could not be verified: broken signature";` |
|     - |  668 | `	}` |
|     6 |  669 | `	return (const char *)SyBlobData(&pVm->sPharErr);` |
|     3 |  670 | `}` |
|     - |  671 | `/*` |
|     - |  672 | `` * The signature php checks on every open with `phar.require_hash` on (its own`` |
|     - |  673 | ` * default): the digest covers the whole file EXCEPT the trailing block, so a` |
|     - |  674 | ` * changed byte anywhere -- stub, manifest or an entry -- is caught.` |
|     - |  675 | ` */` |
|     9 |  676 | `static int PharVerifySignature(phl_phar *pPhar,SyBlob *pHashOut)` |
|   ! 0 |  677 | `{` |
|     9 |  678 | `	const unsigned char *zFile = (const unsigned char *)SyBlobData(&pPhar->sFile);` |
|     9 |  679 | `	sxu32 nFile = SyBlobLength(&pPhar->sFile);` |
|     9 |  680 | `	sxu32 nSig = SyBlobLength(&pPhar->sSig);` |
|     - |  681 | `	unsigned char zDigest[64];` |
|     9 |  682 | `	sxu32 nDigest = 0;` |
|     - |  683 | `	sxu32 nCovered;` |
|     9 |  684 | `	if( pPhar->iSigType == 0 \|\| nSig == 0 ){` |
|   ! 0 |  685 | `		return 0;` |
|     - |  686 | `	}` |
|     9 |  687 | `	if( nFile < nSig + 8 ){` |
|   ! 0 |  688 | `		return -1;` |
|     - |  689 | `	}` |
|     9 |  690 | `	nCovered = nFile - nSig - 8;` |
|     9 |  691 | `	switch( pPhar->iSigType ){` |
|   ! 0 |  692 | `	case PHAR_SIG_MD5: {` |
|     - |  693 | `		MD5Context sMd5;` |
|   ! 0 |  694 | `		MD5Init(&sMd5);` |
|   ! 0 |  695 | `		MD5Update(&sMd5,zFile,nCovered);` |
|   ! 0 |  696 | `		MD5Final(zDigest,&sMd5);` |
|   ! 0 |  697 | `		nDigest = 16;` |
|   ! 0 |  698 | `		break; }` |
|   ! 0 |  699 | `	case PHAR_SIG_SHA1: {` |
|     - |  700 | `		SHA1Context sSha;` |
|   ! 0 |  701 | `		SHA1Init(&sSha);` |
|   ! 0 |  702 | `		SHA1Update(&sSha,zFile,nCovered);` |
|   ! 0 |  703 | `		SHA1Final(&sSha,zDigest);` |
|   ! 0 |  704 | `		nDigest = 20;` |
|   ! 0 |  705 | `		break; }` |
|     1 |  706 | `	case PHAR_SIG_SHA256: {` |
|     - |  707 | `		SHA256Context sSha;` |
|     1 |  708 | `		SHA256Init(&sSha);` |
|     1 |  709 | `		SHA256Update(&sSha,zFile,nCovered);` |
|     1 |  710 | `		SHA256Final(&sSha,zDigest);` |
|     1 |  711 | `		nDigest = 32;` |
|     1 |  712 | `		break; }` |
|     2 |  713 | `	case PHAR_SIG_SHA512: {` |
|     - |  714 | `		SHA512Context sSha;` |
|     4 |  715 | `		SHA512Init(&sSha);` |
|     4 |  716 | `		SHA512Update(&sSha,zFile,nCovered);` |
|     4 |  717 | `		SHA512Final(&sSha,zDigest);` |
|     4 |  718 | `		nDigest = 64;` |
|     4 |  719 | `		break; }` |
|     - |  720 | `#ifdef PH7_ENABLE_OPENSSL` |
|     2 |  721 | `	case PHAR_SIG_OPENSSL:` |
|     - |  722 | `	case PHAR_SIG_OPENSSL_SHA256:` |
|     - |  723 | `	case PHAR_SIG_OPENSSL_SHA512:` |
|     - |  724 | `		/* The public half is NOT in the archive: php reads it from a` |
|     - |  725 | ``		 * `<archive>.pubkey` file beside it, and an archive whose key file is`` |
|     - |  726 | `		 * missing is refused rather than trusted. The covered range is twelve` |
|     - |  727 | `		 * bytes shorter than a hash block's, because the length rides with it. */` |
|     4 |  728 | `		return PharVerifyOpenSsl(pPhar,nFile - nSig - 12,pHashOut);` |
|     - |  729 | `#endif` |
|   ! 0 |  730 | `	default:` |
|     - |  731 | `		/* An OpenSSL signature in a build with no crypto: php's own build` |
|     - |  732 | `		 * refuses it without the extension, and so does this one. */` |
|   ! 0 |  733 | `		return -1;` |
|     - |  734 | `	}` |
|     5 |  735 | `	if( pHashOut ){` |
|   ! 0 |  736 | `		SyBlobAppend(pHashOut,zDigest,nDigest);` |
|   ! 0 |  737 | `	}` |
|     5 |  738 | `	if( nDigest != nSig \|\| SyMemcmp(zDigest,SyBlobData(&pPhar->sSig),nDigest) != 0 ){` |
|     2 |  739 | `		return -1;` |
|     - |  740 | `	}` |
|     3 |  741 | `	return 0;` |
|     4 |  742 | `}` |
|     - |  743 | `/*` |
|     - |  744 | `` * ustar. php reads a tar with `PharData`, and the format is simple enough that`` |
|     - |  745 | ` * the whole of it is here: the 512-byte header, its octal fields, the GNU long` |
|     - |  746 | ` * name extension and the two zero blocks that end the archive.` |
|     - |  747 | ` */` |
|     6 |  748 | `static sxi64 PharTarOctal(const char *z,int n)` |
|   ! 0 |  749 | `{` |
|     6 |  750 | `	sxi64 v = 0;` |
|     - |  751 | `	int i;` |
|    64 |  752 | `	for( i = 0 ; i < n ; ++i ){` |
|    64 |  753 | `		if( z[i] >= '0' && z[i] <= '7' ){` |
|    58 |  754 | `			v = v * 8 + (z[i] - '0');` |
|     6 |  755 | `		}else if( z[i] == ' ' \|\| z[i] == 0 ){` |
|     6 |  756 | `			if( v != 0 \|\| i > 0 ){` |
|   ! 0 |  757 | `				break;` |
|     - |  758 | `			}` |
|   ! 0 |  759 | `		}` |
|   ! 0 |  760 | `	}` |
|     6 |  761 | `	return v;` |
|   ! 0 |  762 | `}` |
|     3 |  763 | `static int PharParseTar(phl_phar *pPhar,const unsigned char *zFile,sxu32 nFile,const char **pzErr)` |
|   ! 0 |  764 | `{` |
|     3 |  765 | `	sxu32 nPos = 0;` |
|     - |  766 | `	SyBlob sLongName;` |
|     3 |  767 | `	int bAny = 0;` |
|     3 |  768 | `	SyBlobInit(&sLongName,&pPhar->pVm->sAllocator);` |
|     - |  769 | `	/* Every bound is a REMAINDER against nFile: the sizes come out of the file` |
|     - |  770 | `	 * being parsed, and a sum overflows for one a hostile archive can spell. */` |
|     5 |  771 | `	while( nPos <= nFile && nFile - nPos >= 512 ){` |
|     3 |  772 | `		const char *zHdr = (const char *)&zFile[nPos];` |
|     - |  773 | `		sxi64 nSize;` |
|     - |  774 | `		sxu32 nAdv;` |
|     - |  775 | `		int nName;` |
|     - |  776 | `		char cType;` |
|     3 |  777 | `		if( zHdr[0] == 0 ){` |
|     1 |  778 | `			break;   /* the end-of-archive blocks */` |
|     - |  779 | `		}` |
|     2 |  780 | `		if( SyMemcmp(&zHdr[257],"ustar",5) != 0 ){` |
|   ! 0 |  781 | `			SyBlobRelease(&sLongName);` |
|   ! 0 |  782 | `			*pzErr = "truncated entry";` |
|   ! 0 |  783 | `			return -1;` |
|     - |  784 | `		}` |
|     2 |  785 | `		nSize = PharTarOctal(&zHdr[124],12);` |
|     2 |  786 | `		if( nSize < 0 \|\| nSize > (sxi64)nFile ){` |
|   ! 0 |  787 | `			SyBlobRelease(&sLongName);` |
|   ! 0 |  788 | `			*pzErr = "truncated entry";` |
|   ! 0 |  789 | `			return -1;` |
|     - |  790 | `		}` |
|     2 |  791 | `		cType = zHdr[156];` |
|    16 |  792 | `		for( nName = 0 ; nName < 100 && zHdr[nName] ; ++nName ){}` |
|     2 |  793 | `		nPos += 512;` |
|     2 |  794 | `		nAdv = (sxu32)((nSize + 511) & ~(sxi64)511);` |
|     2 |  795 | `		if( cType == 'L' ){` |
|     - |  796 | `			/* GNU long name: the NAME is the next entry's payload. */` |
|   ! 0 |  797 | `			SyBlobReset(&sLongName);` |
|   ! 0 |  798 | `			if( (sxu32)nSize <= nFile - nPos ){` |
|   ! 0 |  799 | `				sxu32 n = (sxu32)nSize;` |
|   ! 0 |  800 | `				while( n > 0 && zFile[nPos+n-1] == 0 ){` |
|   ! 0 |  801 | `					n--;` |
|   ! 0 |  802 | `				}` |
|   ! 0 |  803 | `				SyBlobAppend(&sLongName,&zFile[nPos],n);` |
|   ! 0 |  804 | `			}` |
|   ! 0 |  805 | `			if( nAdv > nFile - nPos ){` |
|   ! 0 |  806 | `				break;` |
|     - |  807 | `			}` |
|   ! 0 |  808 | `			nPos += nAdv;` |
|   ! 0 |  809 | `			continue;` |
|     - |  810 | `		}` |
|     2 |  811 | `		if( cType == '0' \|\| cType == 0 \|\| cType == '5' ){` |
|     - |  812 | `			phl_phar_ent *pEnt;` |
|     2 |  813 | `			if( SyBlobLength(&sLongName) > 0 ){` |
|   ! 0 |  814 | `				pEnt = PharNewEnt(pPhar,(const char *)SyBlobData(&sLongName),` |
|   ! 0 |  815 | `					(int)SyBlobLength(&sLongName));` |
|   ! 0 |  816 | `				SyBlobReset(&sLongName);` |
|   ! 0 |  817 | `			}else{` |
|     2 |  818 | `				pEnt = PharNewEnt(pPhar,zHdr,nName);` |
|     - |  819 | `			}` |
|     2 |  820 | `			if( pEnt == 0 ){` |
|   ! 0 |  821 | `				SyBlobRelease(&sLongName);` |
|   ! 0 |  822 | `				*pzErr = "internal corruption of tar (out of memory)";` |
|   ! 0 |  823 | `				return -1;` |
|     - |  824 | `			}` |
|     2 |  825 | `			if( cType == '5' ){` |
|   ! 0 |  826 | `				pEnt->bDir = 1;` |
|   ! 0 |  827 | `				if( SyBlobLength(&pEnt->sName) > 0 ){` |
|   ! 0 |  828 | `					char *zN = (char *)SyBlobData(&pEnt->sName);` |
|   ! 0 |  829 | `					if( zN[SyBlobLength(&pEnt->sName)-1] == '/' ){` |
|   ! 0 |  830 | `						PharChopSlash(&pEnt->sName);` |
|   ! 0 |  831 | `					}` |
|   ! 0 |  832 | `				}` |
|   ! 0 |  833 | `				nSize = 0;` |
|   ! 0 |  834 | `			}` |
|     2 |  835 | `			pEnt->nSize = (sxu32)nSize;` |
|     2 |  836 | `			pEnt->nCompSize = (sxu32)nSize;` |
|     2 |  837 | `			pEnt->iTime = PharTarOctal(&zHdr[136],12);` |
|     2 |  838 | `			pEnt->nFlags = (sxu32)(PharTarOctal(&zHdr[100],8) & 0x1FF);` |
|     2 |  839 | `			pEnt->iOffset = (sxi64)nPos;` |
|     2 |  840 | `			bAny = 1;` |
|   ! 0 |  841 | `		}` |
|     2 |  842 | `		if( nAdv > nFile - nPos ){` |
|   ! 0 |  843 | `			break;` |
|     - |  844 | `		}` |
|     2 |  845 | `		nPos += nAdv;` |
|   ! 0 |  846 | `	}` |
|     3 |  847 | `	SyBlobRelease(&sLongName);` |
|     3 |  848 | `	if( !bAny ){` |
|     - |  849 | `		/* php's own sentence for a file its tar reader cannot even start on. */` |
|     2 |  850 | `		*pzErr = "truncated entry";` |
|     2 |  851 | `		return -1;` |
|     - |  852 | `	}` |
|     1 |  853 | `	SyBlobAppend(&pPhar->sStub,"",0);` |
|     1 |  854 | `	pPhar->iFormat = PHAR_FORMAT_TAR;` |
|     1 |  855 | `	return 0;` |
|     1 |  856 | `}` |
|     - |  857 | `/*` |
|     - |  858 | ` * ZIP, read from its CENTRAL DIRECTORY -- the only part of the format that is` |
|     - |  859 | ` * authoritative, which is why an appended or partly-rewritten zip still reads.` |
|     - |  860 | ` * Entries are either stored or deflated, and the deflate is ext/zlib's.` |
|     - |  861 | ` */` |
|   ! 0 |  862 | `static int PharParseZip(phl_phar *pPhar,const unsigned char *zFile,sxu32 nFile,const char **pzErr)` |
|   ! 0 |  863 | `{` |
|     - |  864 | `	sxu32 nEocd,nEntries,nCdOff,i,q;` |
|   ! 0 |  865 | `	int bFound = 0;` |
|   ! 0 |  866 | `	if( nFile < 22 ){` |
|   ! 0 |  867 | `		*pzErr = "internal corruption of zip (truncated)";` |
|   ! 0 |  868 | `		return -1;` |
|     - |  869 | `	}` |
|   ! 0 |  870 | `	for( nEocd = nFile - 22 ; ; --nEocd ){` |
|   ! 0 |  871 | `		if( zFile[nEocd] == 'P' && zFile[nEocd+1] == 'K'` |
|   ! 0 |  872 | `		 && zFile[nEocd+2] == 5 && zFile[nEocd+3] == 6 ){` |
|   ! 0 |  873 | `			bFound = 1;` |
|   ! 0 |  874 | `			break;` |
|     - |  875 | `		}` |
|   ! 0 |  876 | `		if( nEocd == 0 \|\| nFile - nEocd > 66000 ){` |
|   ! 0 |  877 | `			break;` |
|     - |  878 | `		}` |
|   ! 0 |  879 | `	}` |
|   ! 0 |  880 | `	if( !bFound ){` |
|   ! 0 |  881 | `		*pzErr = "internal corruption of zip (end of central directory not found)";` |
|   ! 0 |  882 | `		return -1;` |
|     - |  883 | `	}` |
|   ! 0 |  884 | `	nEntries = PharGet16(&zFile[nEocd+10]);` |
|   ! 0 |  885 | `	nCdOff = PharGet32(&zFile[nEocd+16]);` |
|     - |  886 | `	{` |
|   ! 0 |  887 | `		sxu32 nCommentLen = PharGet16(&zFile[nEocd+20]);` |
|   ! 0 |  888 | `		if( nCommentLen > 0 && nCommentLen <= nFile - nEocd - 22 ){` |
|     - |  889 | `			/* php keeps a zip's comment as the archive's stub. */` |
|   ! 0 |  890 | `			SyBlobAppend(&pPhar->sStub,&zFile[nEocd+22],nCommentLen);` |
|   ! 0 |  891 | `		}` |
|     - |  892 | `	}` |
|   ! 0 |  893 | `	q = nCdOff;` |
|   ! 0 |  894 | `	for( i = 0 ; i < nEntries ; ++i ){` |
|     - |  895 | `		sxu32 nNameLen,nExtraLen,nCommentLen,nLocal,nMethod,nCsz,nUsz,nCrc,nAttr;` |
|     - |  896 | `		phl_phar_ent *pEnt;` |
|     - |  897 | `		/* Remainders again: nCdOff and every length below come out of the file. */` |
|   ! 0 |  898 | `		if( q > nFile \|\| nFile - q < 46 \|\| SyMemcmp(&zFile[q],"PK\001\002",4) != 0 ){` |
|   ! 0 |  899 | `			*pzErr = "internal corruption of zip (truncated central directory)";` |
|   ! 0 |  900 | `			return -1;` |
|     - |  901 | `		}` |
|   ! 0 |  902 | `		nMethod = PharGet16(&zFile[q+10]);` |
|   ! 0 |  903 | `		nCrc = PharGet32(&zFile[q+16]);` |
|   ! 0 |  904 | `		nCsz = PharGet32(&zFile[q+20]);` |
|   ! 0 |  905 | `		nUsz = PharGet32(&zFile[q+24]);` |
|   ! 0 |  906 | `		nNameLen = PharGet16(&zFile[q+28]);` |
|   ! 0 |  907 | `		nExtraLen = PharGet16(&zFile[q+30]);` |
|   ! 0 |  908 | `		nCommentLen = PharGet16(&zFile[q+32]);` |
|   ! 0 |  909 | `		nAttr = PharGet32(&zFile[q+38]);` |
|   ! 0 |  910 | `		nLocal = PharGet32(&zFile[q+42]);` |
|   ! 0 |  911 | `		if( nNameLen > nFile - q - 46 ){` |
|   ! 0 |  912 | `			*pzErr = "internal corruption of zip (truncated central directory)";` |
|   ! 0 |  913 | `			return -1;` |
|     - |  914 | `		}` |
|   ! 0 |  915 | `		pEnt = PharNewEnt(pPhar,(const char *)&zFile[q+46],(int)nNameLen);` |
|   ! 0 |  916 | `		if( pEnt == 0 ){` |
|   ! 0 |  917 | `			*pzErr = "internal corruption of zip (out of memory)";` |
|   ! 0 |  918 | `			return -1;` |
|     - |  919 | `		}` |
|   ! 0 |  920 | `		pEnt->nSize = nUsz;` |
|   ! 0 |  921 | `		pEnt->nCompSize = nCsz;` |
|   ! 0 |  922 | `		pEnt->nCrc = nCrc;` |
|   ! 0 |  923 | `		pEnt->nFlags = ((nAttr >> 16) & 0x1FF);` |
|   ! 0 |  924 | `		if( pEnt->nFlags == 0 ){` |
|   ! 0 |  925 | `			pEnt->nFlags = 0666;` |
|   ! 0 |  926 | `		}` |
|   ! 0 |  927 | `		if( nMethod == 8 ){` |
|   ! 0 |  928 | `			pEnt->nFlags \|= PHAR_C_GZ;` |
|   ! 0 |  929 | `		}` |
|   ! 0 |  930 | `		if( SyBlobLength(&pEnt->sName) > 0 ){` |
|   ! 0 |  931 | `			char *zN = (char *)SyBlobData(&pEnt->sName);` |
|   ! 0 |  932 | `			if( zN[SyBlobLength(&pEnt->sName)-1] == '/' ){` |
|   ! 0 |  933 | `				pEnt->bDir = 1;` |
|   ! 0 |  934 | `				PharChopSlash(&pEnt->sName);` |
|   ! 0 |  935 | `			}` |
|   ! 0 |  936 | `		}` |
|     - |  937 | `		/* The local header repeats the name and carries its own extra field,` |
|     - |  938 | `		 * so the payload's offset is only knowable from it. */` |
|   ! 0 |  939 | `		if( nLocal > nFile \|\| nFile - nLocal < 30` |
|   ! 0 |  940 | `		 \|\| SyMemcmp(&zFile[nLocal],"PK\003\004",4) != 0 ){` |
|   ! 0 |  941 | `			*pzErr = "internal corruption of zip (bad local header)";` |
|   ! 0 |  942 | `			return -1;` |
|     - |  943 | `		}` |
|   ! 0 |  944 | `		pEnt->iOffset = (sxi64)(nLocal + 30 + PharGet16(&zFile[nLocal+26])` |
|   ! 0 |  945 | `			+ PharGet16(&zFile[nLocal+28]));` |
|   ! 0 |  946 | `		q += 46 + nNameLen + nExtraLen + nCommentLen;` |
|   ! 0 |  947 | `	}` |
|   ! 0 |  948 | `	pPhar->iFormat = PHAR_FORMAT_ZIP;` |
|   ! 0 |  949 | `	return 0;` |
|   ! 0 |  950 | `}` |
|     - |  951 | `/*` |
|     - |  952 | ` * The bytes of one entry, decompressed if they are stored compressed. Kept on` |
|     - |  953 | ` * the entry once read: an archive is normally read many times over.` |
|     - |  954 | ` */` |
|   295 |  955 | `static int PharEntLoad(phl_phar *pPhar,phl_phar_ent *pEnt)` |
|   ! 0 |  956 | `{` |
|     - |  957 | `	const unsigned char *zFile;` |
|     - |  958 | `	sxu32 nFile;` |
|   295 |  959 | `	if( pEnt->bLoaded \|\| pEnt->bDir ){` |
|   291 |  960 | `		return 0;` |
|     - |  961 | `	}` |
|     4 |  962 | `	zFile = (const unsigned char *)SyBlobData(&pPhar->sFile);` |
|     4 |  963 | `	nFile = SyBlobLength(&pPhar->sFile);` |
|     4 |  964 | `	if( pEnt->iOffset < 0 \|\| (sxu32)pEnt->iOffset + pEnt->nCompSize > nFile ){` |
|   ! 0 |  965 | `		return -1;` |
|     - |  966 | `	}` |
|     4 |  967 | `	if( (pEnt->nFlags & PHAR_C_MASK) == 0 ){` |
|     4 |  968 | `		SyBlobAppend(&pEnt->sData,&zFile[pEnt->iOffset],pEnt->nCompSize);` |
|     4 |  969 | `		pEnt->bLoaded = 1;` |
|     4 |  970 | `		return 0;` |
|     - |  971 | `	}` |
|     - |  972 | `#ifdef PH7_ENABLE_ZLIB` |
|   ! 0 |  973 | `	if( (pEnt->nFlags & PHAR_C_MASK) == PHAR_C_GZ ){` |
|     - |  974 | `		z_stream z;` |
|     - |  975 | `		unsigned char *zOut;` |
|     - |  976 | `		int rc;` |
|   ! 0 |  977 | `		if( pEnt->nSize == 0 ){` |
|   ! 0 |  978 | `			pEnt->bLoaded = 1;` |
|   ! 0 |  979 | `			return 0;` |
|     - |  980 | `		}` |
|   ! 0 |  981 | `		zOut = (unsigned char *)SyMemBackendAlloc(&pPhar->pVm->sAllocator,pEnt->nSize);` |
|   ! 0 |  982 | `		if( zOut == 0 ){` |
|   ! 0 |  983 | `			return -1;` |
|     - |  984 | `		}` |
|   ! 0 |  985 | `		SyZero(&z,sizeof(z));` |
|     - |  986 | `		/* A phar's and a zip's compressed member is a RAW deflate stream. */` |
|   ! 0 |  987 | `		if( inflateInit2(&z,-MAX_WBITS) != Z_OK ){` |
|   ! 0 |  988 | `			SyMemBackendFree(&pPhar->pVm->sAllocator,zOut);` |
|   ! 0 |  989 | `			return -1;` |
|     - |  990 | `		}` |
|   ! 0 |  991 | `		z.next_in = (Bytef *)&zFile[pEnt->iOffset];` |
|   ! 0 |  992 | `		z.avail_in = (uInt)pEnt->nCompSize;` |
|   ! 0 |  993 | `		z.next_out = (Bytef *)zOut;` |
|   ! 0 |  994 | `		z.avail_out = (uInt)pEnt->nSize;` |
|   ! 0 |  995 | `		rc = inflate(&z,Z_FINISH);` |
|   ! 0 |  996 | `		inflateEnd(&z);` |
|   ! 0 |  997 | `		if( rc != Z_STREAM_END ){` |
|   ! 0 |  998 | `			SyMemBackendFree(&pPhar->pVm->sAllocator,zOut);` |
|   ! 0 |  999 | `			return -1;` |
|     - | 1000 | `		}` |
|   ! 0 | 1001 | `		SyBlobAppend(&pEnt->sData,zOut,pEnt->nSize - z.avail_out);` |
|   ! 0 | 1002 | `		SyMemBackendFree(&pPhar->pVm->sAllocator,zOut);` |
|   ! 0 | 1003 | `		pEnt->bLoaded = 1;` |
|   ! 0 | 1004 | `		return 0;` |
|     - | 1005 | `	}` |
|     - | 1006 | `#endif` |
|     - | 1007 | `	/* BZ2, or a compression this build has no library for: php's own answer` |
|     - | 1008 | `	 * when its build lacks the extension. */` |
|   ! 0 | 1009 | `	return -1;` |
|   141 | 1010 | `}` |
|     - | 1011 | `/*` |
|     - | 1012 | ` * Open an archive from a PATH, through the engine's own stream layer -- so an` |
|     - | 1013 | ` * archive inside another archive, or one over http://, reads like any other.` |
|     - | 1014 | ` * The whole file is taken at once; see the section comment.` |
|     - | 1015 | ` */` |
|     - | 1016 | `/*` |
|     - | 1017 | ``  * Would php open this NAME for writing? Its write-mode url check wants a `.phar` `` |
|     - | 1018 | `` * component in the path; a `.tar` or a `.zip` is a DATA archive, and the two`` |
|     - | 1019 | `` * differ in what the doors answer -- `phar.readonly` does not gate a data`` |
|     - | 1020 | ` * archive's entries, and its directory doors are refused outright. An archive` |
|     - | 1021 | ` * the WRAPPER opened has no object to have been told which it is, so the name` |
|     - | 1022 | ` * is what says so, exactly as it does for php.` |
|     - | 1023 | ` */` |
|    10 | 1024 | `static int PharNameIsExecutable(const char *zPath,int nPath)` |
|   ! 0 | 1025 | `{` |
|     - | 1026 | `	int i;` |
|   462 | 1027 | `	for( i = 0 ; i + (int)sizeof(".phar")-1 <= nPath ; ++i ){` |
|   461 | 1028 | `		if( zPath[i] != '.' \|\| SyMemcmp(&zPath[i],".phar",sizeof(".phar")-1) != 0 ){` |
|   452 | 1029 | `			continue;` |
|     - | 1030 | `		}` |
|     9 | 1031 | `		i += (int)sizeof(".phar")-1;` |
|     9 | 1032 | `		if( i == nPath \|\| zPath[i] == '/' \|\| zPath[i] == '\\' ){` |
|     9 | 1033 | `			return 1;` |
|     - | 1034 | `		}` |
|   ! 0 | 1035 | `		return 0;` |
|   ! 0 | 1036 | `	}` |
|     1 | 1037 | `	return 0;` |
|     4 | 1038 | `}` |
|    28 | 1039 | `static phl_phar * PharOpenPath(ph7_vm *pVm,const char *zPath,int nPath,int bData,` |
|     - | 1040 | `	const char **pzErr)` |
|   ! 0 | 1041 | `{` |
|     - | 1042 | `	const ph7_io_stream *pStream;` |
|     - | 1043 | `	const char *zTail;` |
|     - | 1044 | `	phl_phar *pPhar;` |
|     - | 1045 | `	void *pHandle;` |
|     - | 1046 | `	SyBlob sPath;` |
|     - | 1047 | `	int rc;` |
|    28 | 1048 | `	*pzErr = 0;` |
|    28 | 1049 | `	pPhar = PharFindByPath(pVm,zPath,nPath);` |
|    28 | 1050 | `	if( pPhar ){` |
|    16 | 1051 | `		return pPhar;` |
|     - | 1052 | `	}` |
|    12 | 1053 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|    12 | 1054 | `	SyBlobAppend(&sPath,zPath,(sxu32)nPath);` |
|    12 | 1055 | `	SyBlobNullAppend(&sPath);` |
|    12 | 1056 | `	zTail = (const char *)SyBlobData(&sPath);` |
|    12 | 1057 | `	pStream = PH7_VmGetStreamDevice(pVm,&zTail,nPath);` |
|    12 | 1058 | `	if( pStream == 0 ){` |
|   ! 0 | 1059 | `		SyBlobRelease(&sPath);` |
|   ! 0 | 1060 | `		*pzErr = "unable to open phar for reading";` |
|   ! 0 | 1061 | `		return 0;` |
|     - | 1062 | `	}` |
|    12 | 1063 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,zTail,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,0);` |
|    12 | 1064 | `	if( pHandle == 0 ){` |
|   ! 0 | 1065 | `		SyBlobRelease(&sPath);` |
|   ! 0 | 1066 | `		*pzErr = "unable to open phar for reading";` |
|   ! 0 | 1067 | `		return 0;` |
|     - | 1068 | `	}` |
|    12 | 1069 | `	pPhar = (phl_phar *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar));` |
|    12 | 1070 | `	if( pPhar == 0 ){` |
|   ! 0 | 1071 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|   ! 0 | 1072 | `		SyBlobRelease(&sPath);` |
|   ! 0 | 1073 | `		*pzErr = "unable to open phar for reading";` |
|   ! 0 | 1074 | `		return 0;` |
|     - | 1075 | `	}` |
|    12 | 1076 | `	SyZero(pPhar,sizeof(*pPhar));` |
|    12 | 1077 | `	pPhar->pVm = pVm;` |
|    12 | 1078 | `	pPhar->bData = bData \|\| !PharNameIsExecutable(zPath,nPath);` |
|    12 | 1079 | `	SyBlobInit(&pPhar->sPath,&pVm->sAllocator);` |
|    12 | 1080 | `	SyBlobInit(&pPhar->sAlias,&pVm->sAllocator);` |
|    12 | 1081 | `	SyBlobInit(&pPhar->sStub,&pVm->sAllocator);` |
|    12 | 1082 | `	SyBlobInit(&pPhar->sMeta,&pVm->sAllocator);` |
|    12 | 1083 | `	SyBlobInit(&pPhar->sFile,&pVm->sAllocator);` |
|    12 | 1084 | `	SyBlobInit(&pPhar->sSig,&pVm->sAllocator);` |
|    12 | 1085 | `	SyBlobInit(&pPhar->sPrivKey,&pVm->sAllocator);` |
|    12 | 1086 | `	SyBlobAppend(&pPhar->sPath,zPath,(sxu32)nPath);` |
|    12 | 1087 | `	PH7_StreamReadWholeFile(pHandle,pStream,&pPhar->sFile);` |
|    12 | 1088 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    12 | 1089 | `	SyBlobRelease(&sPath);` |
|     - | 1090 | `	{` |
|    12 | 1091 | `		const unsigned char *zFile = (const unsigned char *)SyBlobData(&pPhar->sFile);` |
|    12 | 1092 | `		sxu32 nFile = SyBlobLength(&pPhar->sFile);` |
|    12 | 1093 | `		sxu32 nOff = 0;` |
|    12 | 1094 | `		const char *zErr = "internal corruption of phar (truncated manifest header)";` |
|    12 | 1095 | `		if( nFile < 4 ){` |
|   ! 0 | 1096 | `			rc = -1;` |
|    12 | 1097 | `		}else if( PharFindManifest(zFile,nFile,&nOff) ){` |
|     9 | 1098 | `			rc = PharParsePhar(pPhar,zFile,nFile,nOff,&zErr);` |
|     7 | 1099 | `		}else if( nFile > 4 && zFile[0] == 'P' && zFile[1] == 'K' ){` |
|   ! 0 | 1100 | `			rc = PharParseZip(pPhar,zFile,nFile,&zErr);` |
|   ! 0 | 1101 | `		}else{` |
|     - | 1102 | `			/* Anything else is offered to the TAR reader, which is php's last` |
|     - | 1103 | `			 * resort too -- and its refusal is what php reports for a file that` |
|     - | 1104 | `			 * is no archive at all. */` |
|     3 | 1105 | `			rc = PharParseTar(pPhar,zFile,nFile,&zErr);` |
|     - | 1106 | `		}` |
|    12 | 1107 | `		if( rc != 0 ){` |
|     2 | 1108 | `			*pzErr = zErr;` |
|     2 | 1109 | `			PharFree(pPhar);` |
|     2 | 1110 | `			return 0;` |
|     - | 1111 | `		}` |
|     - | 1112 | `	}` |
|    10 | 1113 | `	if( pPhar->iFormat == PHAR_FORMAT_PHAR && pPhar->iSigType != 0 ){` |
|     9 | 1114 | `		if( PharVerifySignature(pPhar,0) != 0 ){` |
|     - | 1115 | `			/* php names the ALGORITHM and its own reason:` |
|     - | 1116 | ``			 * `phar "…" SHA512 signature could not be verified: broken`` |
|     - | 1117 | ``			 * signature`. The caller assembles the sentence; what travels here`` |
|     - | 1118 | `			 * is everything after the archive's name. */` |
|     6 | 1119 | `			*pzErr = PharSigFailure(pPhar);` |
|     6 | 1120 | `			PharFree(pPhar);` |
|     6 | 1121 | `			return 0;` |
|     - | 1122 | `		}` |
|     1 | 1123 | `	}` |
|     4 | 1124 | `	pPhar->pNext = (phl_phar *)pVm->pPhars;` |
|     4 | 1125 | `	pVm->pPhars = pPhar;` |
|     4 | 1126 | `	return pPhar;` |
|    13 | 1127 | `}` |
|     - | 1128 | `/* ------------------------------------------------------------------ */` |
|     - | 1129 | `/* Writing an archive back out                                         */` |
|     - | 1130 | `/* ------------------------------------------------------------------ */` |
|  1745 | 1131 | `static void PharPut32(SyBlob *pOut,sxu32 v)` |
|   ! 0 | 1132 | `{` |
|     - | 1133 | `	unsigned char z[4];` |
|  1745 | 1134 | `	z[0] = (unsigned char)(v & 0xFF);` |
|  1745 | 1135 | `	z[1] = (unsigned char)((v >> 8) & 0xFF);` |
|  1745 | 1136 | `	z[2] = (unsigned char)((v >> 16) & 0xFF);` |
|  1745 | 1137 | `	z[3] = (unsigned char)((v >> 24) & 0xFF);` |
|  1745 | 1138 | `	SyBlobAppend(pOut,z,4);` |
|  1745 | 1139 | `}` |
|     - | 1140 | `#ifdef PH7_ENABLE_OPENSSL` |
|     - | 1141 | `/*` |
|     - | 1142 | ` * Sign what has been written so far with the private key` |
|     - | 1143 | `` * `setSignatureAlgorithm()` was handed. Answers -1 without touching the output`` |
|     - | 1144 | ` * when there is no key or the key will not parse, which is what makes php's` |
|     - | 1145 | ` * two refusals ("unable to write to phar … with requested openssl signature"` |
|     - | 1146 | ` * for no key, "unable to process private key" for a bad one) two different` |
|     - | 1147 | ` * sentences.` |
|     - | 1148 | ` */` |
|     8 | 1149 | `static int PharSignOpenSsl(phl_phar *pPhar,SyBlob *pOut,int iSig,sxu32 *pnSigLen)` |
|   ! 0 | 1150 | `{` |
|     - | 1151 | `	BIO *pBio;` |
|     8 | 1152 | `	EVP_PKEY *pKey = 0;` |
|     - | 1153 | `	EVP_MD_CTX *pMdCtx;` |
|     - | 1154 | `	const EVP_MD *pMd;` |
|     8 | 1155 | `	unsigned char *zSig = 0;` |
|     8 | 1156 | `	size_t nSig = 0;` |
|     8 | 1157 | `	int rc = -1;` |
|     8 | 1158 | `	if( SyBlobLength(&pPhar->sPrivKey) < 1 ){` |
|   ! 0 | 1159 | `		return -1;` |
|     - | 1160 | `	}` |
|     8 | 1161 | `	pBio = BIO_new_mem_buf(SyBlobData(&pPhar->sPrivKey),(int)SyBlobLength(&pPhar->sPrivKey));` |
|     8 | 1162 | `	if( pBio ){` |
|     8 | 1163 | `		pKey = PEM_read_bio_PrivateKey(pBio,0,0,0);` |
|     8 | 1164 | `		BIO_free(pBio);` |
|     4 | 1165 | `	}` |
|     8 | 1166 | `	ERR_clear_error();` |
|     8 | 1167 | `	if( pKey == 0 ){` |
|     2 | 1168 | `		return -1;` |
|     - | 1169 | `	}` |
|     6 | 1170 | `	switch( iSig ){` |
|     2 | 1171 | `		case PHAR_SIG_OPENSSL_SHA256: pMd = EVP_sha256(); break;` |
|     2 | 1172 | `		case PHAR_SIG_OPENSSL_SHA512: pMd = EVP_sha512(); break;` |
|     2 | 1173 | `		default:                      pMd = EVP_sha1();   break;` |
|     - | 1174 | `	}` |
|     6 | 1175 | `	pMdCtx = EVP_MD_CTX_new();` |
|     6 | 1176 | `	if( pMdCtx && EVP_DigestSignInit(pMdCtx,0,pMd,0,pKey) == 1` |
|     6 | 1177 | `	 && EVP_DigestSign(pMdCtx,0,&nSig,(const unsigned char *)SyBlobData(pOut),` |
|     9 | 1178 | `			(size_t)SyBlobLength(pOut)) == 1 ){` |
|     6 | 1179 | `		zSig = (unsigned char *)SyMemBackendAlloc(&pPhar->pVm->sAllocator,(sxu32)nSig);` |
|     9 | 1180 | `		if( zSig && EVP_DigestSign(pMdCtx,zSig,&nSig,(const unsigned char *)SyBlobData(pOut),` |
|     9 | 1181 | `				(size_t)SyBlobLength(pOut)) == 1 ){` |
|     6 | 1182 | `			SyBlobAppend(pOut,zSig,(sxu32)nSig);` |
|     6 | 1183 | `			*pnSigLen = (sxu32)nSig;` |
|     6 | 1184 | `			rc = 0;` |
|     3 | 1185 | `		}` |
|     3 | 1186 | `	}` |
|     6 | 1187 | `	if( zSig ){ SyMemBackendFree(&pPhar->pVm->sAllocator,zSig); }` |
|     6 | 1188 | `	if( pMdCtx ){ EVP_MD_CTX_free(pMdCtx); }` |
|     6 | 1189 | `	EVP_PKEY_free(pKey);` |
|     6 | 1190 | `	ERR_clear_error();` |
|     6 | 1191 | `	return rc;` |
|     4 | 1192 | `}` |
|     - | 1193 | `#endif /* PH7_ENABLE_OPENSSL */` |
|   128 | 1194 | `static void PharPut16(SyBlob *pOut,sxu32 v)` |
|   ! 0 | 1195 | `{` |
|     - | 1196 | `	unsigned char z[2];` |
|   128 | 1197 | `	z[0] = (unsigned char)(v & 0xFF);` |
|   128 | 1198 | `	z[1] = (unsigned char)((v >> 8) & 0xFF);` |
|   128 | 1199 | `	SyBlobAppend(pOut,z,2);` |
|   128 | 1200 | `}` |
|    86 | 1201 | `static void PharPut16BE(SyBlob *pOut,sxu32 v)` |
|   ! 0 | 1202 | `{` |
|     - | 1203 | `	unsigned char z[2];` |
|    86 | 1204 | `	z[0] = (unsigned char)((v >> 8) & 0xFF);` |
|    86 | 1205 | `	z[1] = (unsigned char)(v & 0xFF);` |
|    86 | 1206 | `	SyBlobAppend(pOut,z,2);` |
|    86 | 1207 | `}` |
|   173 | 1208 | `static sxu32 PharCrc32(const unsigned char *z,sxu32 n)` |
|   ! 0 | 1209 | `{` |
|     - | 1210 | `	SumContext sCtx;` |
|     - | 1211 | `	unsigned char zOut[4];` |
|   173 | 1212 | `	SumInit(&sCtx,SUM_CRC32B);` |
|   173 | 1213 | `	SumUpdate(&sCtx,z,n);` |
|   173 | 1214 | `	SumFinal(&sCtx,zOut);` |
|     - | 1215 | `	/* SumFinal writes the digest MSB first; the wire wants the NUMBER. */` |
|   258 | 1216 | `	return ((sxu32)zOut[0] << 24) \| ((sxu32)zOut[1] << 16)` |
|   173 | 1217 | `	     \| ((sxu32)zOut[2] << 8) \| (sxu32)zOut[3];` |
|   ! 0 | 1218 | `}` |
|     - | 1219 | `/* The bytes an entry is STORED as, compressing them when its flags say so. */` |
|   173 | 1220 | `static int PharEntStored(phl_phar *pPhar,phl_phar_ent *pEnt,SyBlob *pOut)` |
|   ! 0 | 1221 | `{` |
|   173 | 1222 | `	if( PharEntLoad(pPhar,pEnt) != 0 ){` |
|   ! 0 | 1223 | `		return -1;` |
|     - | 1224 | `	}` |
|   173 | 1225 | `	if( (pEnt->nFlags & PHAR_C_MASK) == 0 \|\| pEnt->bDir ){` |
|   165 | 1226 | `		SyBlobAppend(pOut,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));` |
|   165 | 1227 | `		return 0;` |
|     - | 1228 | `	}` |
|     - | 1229 | `#ifdef PH7_ENABLE_ZLIB` |
|     8 | 1230 | `	if( (pEnt->nFlags & PHAR_C_MASK) == PHAR_C_GZ ){` |
|     - | 1231 | `		z_stream z;` |
|     - | 1232 | `		unsigned char *zBuf;` |
|     - | 1233 | `		uLong nBound;` |
|     - | 1234 | `		int rc;` |
|     8 | 1235 | `		SyZero(&z,sizeof(z));` |
|     8 | 1236 | `		if( deflateInit2(&z,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,8,` |
|     4 | 1237 | `				Z_DEFAULT_STRATEGY) != Z_OK ){` |
|   ! 0 | 1238 | `			return -1;` |
|     - | 1239 | `		}` |
|     8 | 1240 | `		nBound = deflateBound(&z,(uLong)SyBlobLength(&pEnt->sData)) + 32;` |
|     8 | 1241 | `		zBuf = (unsigned char *)SyMemBackendAlloc(&pPhar->pVm->sAllocator,(sxu32)nBound);` |
|     8 | 1242 | `		if( zBuf == 0 ){` |
|   ! 0 | 1243 | `			deflateEnd(&z);` |
|   ! 0 | 1244 | `			return -1;` |
|     - | 1245 | `		}` |
|     8 | 1246 | `		z.next_in = (Bytef *)SyBlobData(&pEnt->sData);` |
|     8 | 1247 | `		z.avail_in = (uInt)SyBlobLength(&pEnt->sData);` |
|     8 | 1248 | `		z.next_out = (Bytef *)zBuf;` |
|     8 | 1249 | `		z.avail_out = (uInt)nBound;` |
|     8 | 1250 | `		rc = deflate(&z,Z_FINISH);` |
|     8 | 1251 | `		deflateEnd(&z);` |
|     8 | 1252 | `		if( rc != Z_STREAM_END ){` |
|   ! 0 | 1253 | `			SyMemBackendFree(&pPhar->pVm->sAllocator,zBuf);` |
|   ! 0 | 1254 | `			return -1;` |
|     - | 1255 | `		}` |
|     8 | 1256 | `		SyBlobAppend(pOut,zBuf,(sxu32)(nBound - z.avail_out));` |
|     8 | 1257 | `		SyMemBackendFree(&pPhar->pVm->sAllocator,zBuf);` |
|     8 | 1258 | `		return 0;` |
|     - | 1259 | `	}` |
|     - | 1260 | `#endif` |
|   ! 0 | 1261 | `	return -1;` |
|    85 | 1262 | `}` |
|     - | 1263 | `/* php's default stub, which is what an archive gets when nobody set one. */` |
|    57 | 1264 | `static void PharDefaultStub(SyBlob *pOut)` |
|   ! 0 | 1265 | `{` |
|     - | 1266 | `	static const char zStub[] =` |
|     - | 1267 | `		"<?php __HALT_COMPILER(); ?>\r\n";` |
|    57 | 1268 | `	SyBlobAppend(pOut,zStub,(sxu32)sizeof(zStub)-1);` |
|    57 | 1269 | `}` |
|     - | 1270 | `/*` |
|     - | 1271 | ` * php's own format, written the way php writes it: the stub, then the manifest` |
|     - | 1272 | ` * (whose first word is the length of everything after it), then every entry's` |
|     - | 1273 | ` * stored bytes in manifest order, then the signature block.` |
|     - | 1274 | ` */` |
|    86 | 1275 | `static int PharBuildPhar(phl_phar *pPhar,SyBlob *pOut)` |
|   ! 0 | 1276 | `{` |
|     - | 1277 | `	SyBlob sManifest,sData;` |
|     - | 1278 | `	phl_phar_ent *pEnt;` |
|    86 | 1279 | `	ph7_vm *pVm = pPhar->pVm;` |
|    86 | 1280 | `	sxu32 nFlags = PHAR_GF_SIGNED;` |
|    86 | 1281 | `	int rc = 0;` |
|    86 | 1282 | `	SyBlobInit(&sManifest,&pVm->sAllocator);` |
|    86 | 1283 | `	SyBlobInit(&sData,&pVm->sAllocator);` |
|    86 | 1284 | `	if( SyBlobLength(&pPhar->sStub) > 0 ){` |
|    30 | 1285 | `		SyBlobAppend(pOut,SyBlobData(&pPhar->sStub),SyBlobLength(&pPhar->sStub));` |
|    15 | 1286 | `	}else{` |
|    56 | 1287 | `		PharDefaultStub(pOut);` |
|     - | 1288 | `	}` |
|    86 | 1289 | `	PharPut32(&sManifest,pPhar->nEnt);` |
|    86 | 1290 | `	PharPut16BE(&sManifest,PHAR_API_WORD);` |
|    86 | 1291 | `	PharPut32(&sManifest,nFlags);` |
|    86 | 1292 | `	PharPut32(&sManifest,SyBlobLength(&pPhar->sAlias));` |
|    86 | 1293 | `	if( SyBlobLength(&pPhar->sAlias) > 0 ){` |
|   ! 0 | 1294 | `		SyBlobAppend(&sManifest,SyBlobData(&pPhar->sAlias),SyBlobLength(&pPhar->sAlias));` |
|   ! 0 | 1295 | `	}` |
|    86 | 1296 | `	PharPut32(&sManifest,SyBlobLength(&pPhar->sMeta));` |
|    86 | 1297 | `	if( SyBlobLength(&pPhar->sMeta) > 0 ){` |
|    18 | 1298 | `		SyBlobAppend(&sManifest,SyBlobData(&pPhar->sMeta),SyBlobLength(&pPhar->sMeta));` |
|     9 | 1299 | `	}` |
|   253 | 1300 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|     - | 1301 | `		SyBlob sStored;` |
|     - | 1302 | `		sxu32 nStored;` |
|   167 | 1303 | `		SyBlobInit(&sStored,&pVm->sAllocator);` |
|   167 | 1304 | `		if( PharEntStored(pPhar,pEnt,&sStored) != 0 ){` |
|   ! 0 | 1305 | `			SyBlobRelease(&sStored);` |
|   ! 0 | 1306 | `			rc = -1;` |
|   ! 0 | 1307 | `			break;` |
|     - | 1308 | `		}` |
|   167 | 1309 | `		nStored = SyBlobLength(&sStored);` |
|   167 | 1310 | `		pEnt->nSize = SyBlobLength(&pEnt->sData);` |
|   167 | 1311 | `		pEnt->nCompSize = pEnt->bDir ? 0 : nStored;` |
|   249 | 1312 | `		pEnt->nCrc = PharCrc32((const unsigned char *)SyBlobData(&pEnt->sData),` |
|    82 | 1313 | `			SyBlobLength(&pEnt->sData));` |
|   167 | 1314 | `		PharPut32(&sManifest,SyBlobLength(&pEnt->sName) + (pEnt->bDir ? 1 : 0));` |
|   167 | 1315 | `		SyBlobAppend(&sManifest,SyBlobData(&pEnt->sName),SyBlobLength(&pEnt->sName));` |
|   167 | 1316 | `		if( pEnt->bDir ){` |
|     2 | 1317 | `			SyBlobAppend(&sManifest,"/",1);` |
|     1 | 1318 | `		}` |
|   167 | 1319 | `		PharPut32(&sManifest,pEnt->nSize);` |
|   167 | 1320 | `		PharPut32(&sManifest,(sxu32)pEnt->iTime);` |
|   167 | 1321 | `		PharPut32(&sManifest,pEnt->nCompSize);` |
|   167 | 1322 | `		PharPut32(&sManifest,pEnt->nCrc);` |
|   167 | 1323 | `		PharPut32(&sManifest,pEnt->nFlags);` |
|   167 | 1324 | `		PharPut32(&sManifest,SyBlobLength(&pEnt->sMeta));` |
|   167 | 1325 | `		if( SyBlobLength(&pEnt->sMeta) > 0 ){` |
|    16 | 1326 | `			SyBlobAppend(&sManifest,SyBlobData(&pEnt->sMeta),SyBlobLength(&pEnt->sMeta));` |
|     8 | 1327 | `		}` |
|   167 | 1328 | `		if( !pEnt->bDir ){` |
|   165 | 1329 | `			SyBlobAppend(&sData,SyBlobData(&sStored),nStored);` |
|    81 | 1330 | `		}` |
|   167 | 1331 | `		SyBlobRelease(&sStored);` |
|    82 | 1332 | `	}` |
|    86 | 1333 | `	if( rc == 0 ){` |
|    86 | 1334 | `		PharPut32(pOut,SyBlobLength(&sManifest));` |
|    86 | 1335 | `		SyBlobAppend(pOut,SyBlobData(&sManifest),SyBlobLength(&sManifest));` |
|    86 | 1336 | `		SyBlobAppend(pOut,SyBlobData(&sData),SyBlobLength(&sData));` |
|     - | 1337 | `		/* The signature covers everything written so far. */` |
|     - | 1338 | `		{` |
|     - | 1339 | `			SHA1Context sSha;` |
|     - | 1340 | `			unsigned char zDigest[20];` |
|     - | 1341 | `			sxu32 nSigLen;` |
|     - | 1342 | `			/* php 8 signs a new archive with SHA-256 unless it was told` |
|     - | 1343 | `			 * otherwise. */` |
|    86 | 1344 | `			int iSig = pPhar->iSigType ? pPhar->iSigType : PHAR_SIG_SHA256;` |
|    86 | 1345 | `			nSigLen = 20;` |
|    86 | 1346 | `			if( iSig == PHAR_SIG_SHA512 ){` |
|     - | 1347 | `				SHA512Context sS;` |
|     - | 1348 | `				unsigned char zD[64];` |
|    14 | 1349 | `				SHA512Init(&sS);` |
|    14 | 1350 | `				SHA512Update(&sS,(const unsigned char *)SyBlobData(pOut),SyBlobLength(pOut));` |
|    14 | 1351 | `				SHA512Final(&sS,zD);` |
|    14 | 1352 | `				SyBlobAppend(pOut,zD,64);` |
|    14 | 1353 | `				nSigLen = 64;` |
|    79 | 1354 | `			}else if( iSig == PHAR_SIG_SHA256 ){` |
|     - | 1355 | `				SHA256Context sS;` |
|     - | 1356 | `				unsigned char zD[32];` |
|    64 | 1357 | `				SHA256Init(&sS);` |
|    64 | 1358 | `				SHA256Update(&sS,(const unsigned char *)SyBlobData(pOut),SyBlobLength(pOut));` |
|    64 | 1359 | `				SHA256Final(&sS,zD);` |
|    64 | 1360 | `				SyBlobAppend(pOut,zD,32);` |
|    64 | 1361 | `				nSigLen = 32;` |
|    39 | 1362 | `			}else if( iSig == PHAR_SIG_MD5 ){` |
|     - | 1363 | `				MD5Context sM;` |
|     - | 1364 | `				unsigned char zD[16];` |
|   ! 0 | 1365 | `				MD5Init(&sM);` |
|   ! 0 | 1366 | `				MD5Update(&sM,(const unsigned char *)SyBlobData(pOut),SyBlobLength(pOut));` |
|   ! 0 | 1367 | `				MD5Final(zD,&sM);` |
|   ! 0 | 1368 | `				SyBlobAppend(pOut,zD,16);` |
|   ! 0 | 1369 | `				nSigLen = 16;` |
|     - | 1370 | `#ifdef PH7_ENABLE_OPENSSL` |
|     8 | 1371 | `			}else if( iSig == PHAR_SIG_OPENSSL \|\| iSig == PHAR_SIG_OPENSSL_SHA256` |
|     3 | 1372 | `			       \|\| iSig == PHAR_SIG_OPENSSL_SHA512 ){` |
|     - | 1373 | `				/* An RSA signature over everything written so far, and the only` |
|     - | 1374 | `				 * signature that needs a KEY. Its block carries its own length` |
|     - | 1375 | `				 * ahead of the flags, which is four bytes more than a hash's. */` |
|     9 | 1376 | `				if( PharSignOpenSsl(pPhar,pOut,iSig,&nSigLen) != 0 ){` |
|     2 | 1377 | `					rc = -1;` |
|     1 | 1378 | `				}` |
|     - | 1379 | `#endif` |
|     4 | 1380 | `			}else{` |
|   ! 0 | 1381 | `				iSig = PHAR_SIG_SHA1;` |
|   ! 0 | 1382 | `				SHA1Init(&sSha);` |
|   ! 0 | 1383 | `				SHA1Update(&sSha,(const unsigned char *)SyBlobData(pOut),SyBlobLength(pOut));` |
|   ! 0 | 1384 | `				SHA1Final(&sSha,zDigest);` |
|   ! 0 | 1385 | `				SyBlobAppend(pOut,zDigest,20);` |
|     - | 1386 | `			}` |
|    86 | 1387 | `			if( rc == 0 ){` |
|    84 | 1388 | `				int bWide = 0;` |
|     - | 1389 | `#ifdef PH7_ENABLE_OPENSSL` |
|   122 | 1390 | `				bWide = iSig == PHAR_SIG_OPENSSL \|\| iSig == PHAR_SIG_OPENSSL_SHA256` |
|   126 | 1391 | `				     \|\| iSig == PHAR_SIG_OPENSSL_SHA512;` |
|    84 | 1392 | `				if( bWide ){` |
|     6 | 1393 | `					PharPut32(pOut,nSigLen);` |
|     3 | 1394 | `				}` |
|     - | 1395 | `#endif` |
|    84 | 1396 | `				PharPut32(pOut,(sxu32)iSig);` |
|    84 | 1397 | `				SyBlobAppend(pOut,"GBMB",4);` |
|    84 | 1398 | `				pPhar->iSigType = iSig;` |
|     - | 1399 | ``				/* Keep the signature: `getSignature()` answers it without`` |
|     - | 1400 | `				 * re-reading the file. */` |
|    84 | 1401 | `				SyBlobReset(&pPhar->sSig);` |
|   168 | 1402 | `				SyBlobAppend(&pPhar->sSig,` |
|    84 | 1403 | `					(const char *)SyBlobData(pOut) + SyBlobLength(pOut) - 8` |
|    84 | 1404 | `						- (bWide ? 4 : 0) - nSigLen,nSigLen);` |
|    41 | 1405 | `			}` |
|     - | 1406 | `		}` |
|    42 | 1407 | `	}` |
|    86 | 1408 | `	SyBlobRelease(&sManifest);` |
|    86 | 1409 | `	SyBlobRelease(&sData);` |
|    86 | 1410 | `	return rc;` |
|   ! 0 | 1411 | `}` |
|     - | 1412 | `/* ustar, with a GNU long-name record for anything past 100 bytes. */` |
|    27 | 1413 | `static void PharTarField(SyBlob *pOut,const char *zVal,int nVal,int nWidth)` |
|   ! 0 | 1414 | `{` |
|     - | 1415 | `	static const char zZero[512] = { 0 };` |
|    27 | 1416 | `	if( nVal > nWidth ){` |
|   ! 0 | 1417 | `		nVal = nWidth;` |
|   ! 0 | 1418 | `	}` |
|    27 | 1419 | `	SyBlobAppend(pOut,zVal,(sxu32)nVal);` |
|    27 | 1420 | `	SyBlobAppend(pOut,zZero,(sxu32)(nWidth - nVal));` |
|    27 | 1421 | `}` |
|   135 | 1422 | `static void PharTarOct(SyBlob *pOut,sxi64 iVal,int nWidth)` |
|   ! 0 | 1423 | `{` |
|     - | 1424 | `	char zBuf[24];` |
|     - | 1425 | `	int i;` |
|  1296 | 1426 | `	for( i = nWidth - 2 ; i >= 0 ; --i ){` |
|  1161 | 1427 | `		zBuf[i] = (char)('0' + (int)(iVal & 7));` |
|  1161 | 1428 | `		iVal >>= 3;` |
|   473 | 1429 | `	}` |
|   135 | 1430 | `	zBuf[nWidth-1] = 0;` |
|   135 | 1431 | `	SyBlobAppend(pOut,zBuf,(sxu32)nWidth);` |
|   135 | 1432 | `}` |
|    27 | 1433 | `static void PharTarHeader(SyBlob *pOut,const char *zName,int nName,sxi64 nSize,` |
|     - | 1434 | `	sxi64 iTime,sxu32 nMode,char cType)` |
|   ! 0 | 1435 | `{` |
|     - | 1436 | `	static const char zZero[512] = { 0 };` |
|    27 | 1437 | `	sxu32 nStart = SyBlobLength(pOut);` |
|     - | 1438 | `	unsigned char *z;` |
|    27 | 1439 | `	sxu32 nSum = 0,i;` |
|    27 | 1440 | `	PharTarField(pOut,zName,nName,100);` |
|    27 | 1441 | `	PharTarOct(pOut,(sxi64)(nMode & 0777),8);` |
|    27 | 1442 | `	PharTarOct(pOut,0,8);` |
|    27 | 1443 | `	PharTarOct(pOut,0,8);` |
|    27 | 1444 | `	PharTarOct(pOut,nSize,12);` |
|    27 | 1445 | `	PharTarOct(pOut,iTime,12);` |
|    27 | 1446 | `	SyBlobAppend(pOut,"        ",8);   /* checksum field, blank while computed */` |
|    27 | 1447 | `	SyBlobAppend(pOut,&cType,1);` |
|    27 | 1448 | `	SyBlobAppend(pOut,zZero,100);      /* link name */` |
|    27 | 1449 | `	SyBlobAppend(pOut,"ustar",5);` |
|    27 | 1450 | `	SyBlobAppend(pOut,zZero,1);` |
|    27 | 1451 | `	SyBlobAppend(pOut,"00",2);` |
|    27 | 1452 | `	SyBlobAppend(pOut,zZero,32+32+8+8+155+12);` |
|    27 | 1453 | `	z = (unsigned char *)SyBlobData(pOut) + nStart;` |
| 13851 | 1454 | `	for( i = 0 ; i < 512 ; ++i ){` |
| 13824 | 1455 | `		nSum += z[i];` |
|  5632 | 1456 | `	}` |
|     - | 1457 | `	{` |
|     - | 1458 | `		char zSum[8];` |
|    27 | 1459 | `		sxi64 v = (sxi64)nSum;` |
|     - | 1460 | `		int k;` |
|   189 | 1461 | `		for( k = 5 ; k >= 0 ; --k ){` |
|   162 | 1462 | `			zSum[k] = (char)('0' + (int)(v & 7));` |
|   162 | 1463 | `			v >>= 3;` |
|    66 | 1464 | `		}` |
|    27 | 1465 | `		zSum[6] = 0;` |
|    27 | 1466 | `		zSum[7] = ' ';` |
|    27 | 1467 | `		SyMemcpy(zSum,&z[148],8);` |
|     - | 1468 | `	}` |
|    27 | 1469 | `}` |
|    27 | 1470 | `static void PharTarPad(SyBlob *pOut)` |
|   ! 0 | 1471 | `{` |
|     - | 1472 | `	static const char zZero[512] = { 0 };` |
|    27 | 1473 | `	sxu32 n = SyBlobLength(pOut) % 512;` |
|    27 | 1474 | `	if( n ){` |
|    27 | 1475 | `		SyBlobAppend(pOut,zZero,512 - n);` |
|    11 | 1476 | `	}` |
|    27 | 1477 | `}` |
|    17 | 1478 | `static int PharBuildTar(phl_phar *pPhar,SyBlob *pOut)` |
|   ! 0 | 1479 | `{` |
|     - | 1480 | `	static const char zZero[512] = { 0 };` |
|     - | 1481 | `	phl_phar_ent *pEnt;` |
|    44 | 1482 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|    27 | 1483 | `		const char *zName = (const char *)SyBlobData(&pEnt->sName);` |
|    27 | 1484 | `		int nName = (int)SyBlobLength(&pEnt->sName);` |
|    27 | 1485 | `		if( PharEntLoad(pPhar,pEnt) != 0 ){` |
|   ! 0 | 1486 | `			return -1;` |
|     - | 1487 | `		}` |
|    27 | 1488 | `		if( nName > 100 ){` |
|     - | 1489 | `			/* GNU's long-name record: the name travels as a payload of its own. */` |
|   ! 0 | 1490 | `			PharTarHeader(pOut,"././@LongLink",13,(sxi64)nName + 1,0,0644,'L');` |
|   ! 0 | 1491 | `			SyBlobAppend(pOut,zName,(sxu32)nName);` |
|   ! 0 | 1492 | `			SyBlobAppend(pOut,zZero,1);` |
|   ! 0 | 1493 | `			PharTarPad(pOut);` |
|   ! 0 | 1494 | `		}` |
|    27 | 1495 | `		if( pEnt->bDir ){` |
|   ! 0 | 1496 | `			PharTarHeader(pOut,zName,nName,0,pEnt->iTime,pEnt->nFlags & 0777,'5');` |
|   ! 0 | 1497 | `			continue;` |
|     - | 1498 | `		}` |
|    38 | 1499 | `		PharTarHeader(pOut,zName,nName,(sxi64)SyBlobLength(&pEnt->sData),pEnt->iTime,` |
|    27 | 1500 | `			pEnt->nFlags & 0777,'0');` |
|    27 | 1501 | `		SyBlobAppend(pOut,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));` |
|    27 | 1502 | `		PharTarPad(pOut);` |
|    11 | 1503 | `	}` |
|    17 | 1504 | `	SyBlobAppend(pOut,zZero,512);` |
|    17 | 1505 | `	SyBlobAppend(pOut,zZero,512);` |
|    17 | 1506 | `	return 0;` |
|     6 | 1507 | `}` |
|     - | 1508 | `/* ZIP: a local header and payload per entry, then the central directory. */` |
|     4 | 1509 | `static int PharBuildZip(phl_phar *pPhar,SyBlob *pOut)` |
|   ! 0 | 1510 | `{` |
|     - | 1511 | `	phl_phar_ent *pEnt;` |
|     - | 1512 | `	SyBlob sCd;` |
|     4 | 1513 | `	ph7_vm *pVm = pPhar->pVm;` |
|     4 | 1514 | `	sxu32 nCdOff,nEntries = 0;` |
|     4 | 1515 | `	int rc = 0;` |
|     4 | 1516 | `	SyBlobInit(&sCd,&pVm->sAllocator);` |
|    10 | 1517 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|     - | 1518 | `		SyBlob sStored;` |
|     6 | 1519 | `		sxu32 nLocal = SyBlobLength(pOut);` |
|     6 | 1520 | `		sxu32 nName = SyBlobLength(&pEnt->sName);` |
|     - | 1521 | `		sxu32 nCrc;` |
|     6 | 1522 | `		int bDeflate = (pEnt->nFlags & PHAR_C_MASK) == PHAR_C_GZ;` |
|     6 | 1523 | `		SyBlobInit(&sStored,&pVm->sAllocator);` |
|     6 | 1524 | `		if( PharEntStored(pPhar,pEnt,&sStored) != 0 ){` |
|   ! 0 | 1525 | `			SyBlobRelease(&sStored);` |
|   ! 0 | 1526 | `			rc = -1;` |
|   ! 0 | 1527 | `			break;` |
|     - | 1528 | `		}` |
|     9 | 1529 | `		nCrc = PharCrc32((const unsigned char *)SyBlobData(&pEnt->sData),` |
|     3 | 1530 | `			SyBlobLength(&pEnt->sData));` |
|     6 | 1531 | `		SyBlobAppend(pOut,"PK\003\004",4);` |
|     6 | 1532 | `		PharPut16(pOut,20);` |
|     6 | 1533 | `		PharPut16(pOut,0);` |
|     6 | 1534 | `		PharPut16(pOut,bDeflate ? 8 : 0);` |
|     6 | 1535 | `		PharPut16(pOut,0);` |
|     6 | 1536 | `		PharPut16(pOut,0);` |
|     6 | 1537 | `		PharPut32(pOut,nCrc);` |
|     6 | 1538 | `		PharPut32(pOut,SyBlobLength(&sStored));` |
|     6 | 1539 | `		PharPut32(pOut,SyBlobLength(&pEnt->sData));` |
|     6 | 1540 | `		PharPut16(pOut,nName + (pEnt->bDir ? 1 : 0));` |
|     6 | 1541 | `		PharPut16(pOut,0);` |
|     6 | 1542 | `		SyBlobAppend(pOut,SyBlobData(&pEnt->sName),nName);` |
|     6 | 1543 | `		if( pEnt->bDir ){` |
|   ! 0 | 1544 | `			SyBlobAppend(pOut,"/",1);` |
|   ! 0 | 1545 | `		}` |
|     6 | 1546 | `		SyBlobAppend(pOut,SyBlobData(&sStored),SyBlobLength(&sStored));` |
|     6 | 1547 | `		SyBlobAppend(&sCd,"PK\001\002",4);` |
|     6 | 1548 | `		PharPut16(&sCd,20);` |
|     6 | 1549 | `		PharPut16(&sCd,20);` |
|     6 | 1550 | `		PharPut16(&sCd,0);` |
|     6 | 1551 | `		PharPut16(&sCd,bDeflate ? 8 : 0);` |
|     6 | 1552 | `		PharPut16(&sCd,0);` |
|     6 | 1553 | `		PharPut16(&sCd,0);` |
|     6 | 1554 | `		PharPut32(&sCd,nCrc);` |
|     6 | 1555 | `		PharPut32(&sCd,SyBlobLength(&sStored));` |
|     6 | 1556 | `		PharPut32(&sCd,SyBlobLength(&pEnt->sData));` |
|     6 | 1557 | `		PharPut16(&sCd,nName + (pEnt->bDir ? 1 : 0));` |
|     6 | 1558 | `		PharPut16(&sCd,0);` |
|     6 | 1559 | `		PharPut16(&sCd,0);` |
|     6 | 1560 | `		PharPut16(&sCd,0);` |
|     6 | 1561 | `		PharPut16(&sCd,0);` |
|     6 | 1562 | `		PharPut32(&sCd,((pEnt->nFlags & 0777) \| (pEnt->bDir ? 0040000 : 0100000)) << 16);` |
|     6 | 1563 | `		PharPut32(&sCd,nLocal);` |
|     6 | 1564 | `		SyBlobAppend(&sCd,SyBlobData(&pEnt->sName),nName);` |
|     6 | 1565 | `		if( pEnt->bDir ){` |
|   ! 0 | 1566 | `			SyBlobAppend(&sCd,"/",1);` |
|   ! 0 | 1567 | `		}` |
|     6 | 1568 | `		SyBlobRelease(&sStored);` |
|     6 | 1569 | `		nEntries++;` |
|     3 | 1570 | `	}` |
|     4 | 1571 | `	if( rc == 0 ){` |
|     4 | 1572 | `		nCdOff = SyBlobLength(pOut);` |
|     4 | 1573 | `		SyBlobAppend(pOut,SyBlobData(&sCd),SyBlobLength(&sCd));` |
|     4 | 1574 | `		SyBlobAppend(pOut,"PK\005\006",4);` |
|     4 | 1575 | `		PharPut16(pOut,0);` |
|     4 | 1576 | `		PharPut16(pOut,0);` |
|     4 | 1577 | `		PharPut16(pOut,nEntries);` |
|     4 | 1578 | `		PharPut16(pOut,nEntries);` |
|     4 | 1579 | `		PharPut32(pOut,SyBlobLength(&sCd));` |
|     4 | 1580 | `		PharPut32(pOut,nCdOff);` |
|     4 | 1581 | `		PharPut16(pOut,SyBlobLength(&pPhar->sStub));` |
|     4 | 1582 | `		if( SyBlobLength(&pPhar->sStub) > 0 ){` |
|   ! 0 | 1583 | `			SyBlobAppend(pOut,SyBlobData(&pPhar->sStub),SyBlobLength(&pPhar->sStub));` |
|   ! 0 | 1584 | `		}` |
|     2 | 1585 | `	}` |
|     4 | 1586 | `	SyBlobRelease(&sCd);` |
|     4 | 1587 | `	return rc;` |
|   ! 0 | 1588 | `}` |
|     - | 1589 | `/*` |
|     - | 1590 | ` * Write the archive back to the file it was opened from. Every write door ends` |
|     - | 1591 | `` * here -- php's `stopBuffering` does the same thing, and its buffering pair is`` |
|     - | 1592 | ` * only a way to do it ONCE for a run of changes.` |
|     - | 1593 | ` */` |
|   107 | 1594 | `static int PharCommit(phl_phar *pPhar)` |
|   ! 0 | 1595 | `{` |
|   107 | 1596 | `	ph7_vm *pVm = pPhar->pVm;` |
|     - | 1597 | `	const ph7_io_stream *pStream;` |
|     - | 1598 | `	SyBlob sOut;` |
|     - | 1599 | `	const char *zPath;` |
|     - | 1600 | `	void *pHandle;` |
|     - | 1601 | `	int rc;` |
|   107 | 1602 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|   105 | 1603 | `	rc = pPhar->iFormat == PHAR_FORMAT_TAR ? PharBuildTar(pPhar,&sOut)` |
|   147 | 1604 | `	   : (pPhar->iFormat == PHAR_FORMAT_ZIP ? PharBuildZip(pPhar,&sOut)` |
|    88 | 1605 | `	                                        : PharBuildPhar(pPhar,&sOut));` |
|   107 | 1606 | `	if( rc != 0 ){` |
|     2 | 1607 | `		SyBlobRelease(&sOut);` |
|     2 | 1608 | `		return -1;` |
|     - | 1609 | `	}` |
|     - | 1610 | `	/* NUL-terminate for the device lookup WITHOUT counting the byte:` |
|     - | 1611 | `	 * SyBlobNullAppend already keeps nByte where it was. */` |
|   105 | 1612 | `	SyBlobNullAppend(&pPhar->sPath);` |
|   105 | 1613 | `	zPath = (const char *)SyBlobData(&pPhar->sPath);` |
|   105 | 1614 | `	pStream = PH7_VmGetStreamDevice(pVm,&zPath,(int)SyBlobLength(&pPhar->sPath));` |
|   105 | 1615 | `	if( pStream == 0 ){` |
|   ! 0 | 1616 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1617 | `		return -1;` |
|     - | 1618 | `	}` |
|   105 | 1619 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,` |
|     - | 1620 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,0);` |
|   105 | 1621 | `	if( pHandle == 0 ){` |
|   ! 0 | 1622 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1623 | `		return -1;` |
|     - | 1624 | `	}` |
|   105 | 1625 | `	if( pStream->xWrite ){` |
|   105 | 1626 | `		pStream->xWrite(pHandle,SyBlobData(&sOut),(ph7_int64)SyBlobLength(&sOut));` |
|    49 | 1627 | `	}` |
|   105 | 1628 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     - | 1629 | `	/* The in-memory copy is the file again. */` |
|   105 | 1630 | `	SyBlobReset(&pPhar->sFile);` |
|   105 | 1631 | `	SyBlobAppend(&pPhar->sFile,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|   105 | 1632 | `	SyBlobRelease(&sOut);` |
|   105 | 1633 | `	return 0;` |
|    50 | 1634 | `}` |
|     - | 1635 | `/* ------------------------------------------------------------------ */` |
|     - | 1636 | `/* Splitting a phar:// url                                             */` |
|     - | 1637 | `/* ------------------------------------------------------------------ */` |
|     - | 1638 | `/* Does this name a regular FILE on the platform's own filesystem? The url` |
|     - | 1639 | ` * splitter asks it of every prefix, and only the OS vfs can answer. */` |
|   768 | 1640 | `static int PharPathIsFile(ph7_vm *pVm,const char *zPath)` |
|   ! 0 | 1641 | `{` |
|   768 | 1642 | `	const ph7_vfs *pVfs = pVm->pEngine ? pVm->pEngine->pVfs : 0;` |
|   768 | 1643 | `	if( pVfs == 0 \|\| pVfs->xIsfile == 0 ){` |
|   ! 0 | 1644 | `		return 0;` |
|     - | 1645 | `	}` |
|   768 | 1646 | `	return pVfs->xIsfile(zPath) == PH7_OK;` |
|   532 | 1647 | `}` |
|     - | 1648 | `/*` |
|     - | 1649 | `` * `phar://<archive>/<entry>`. Which part is which is not spelled in the url, so`` |
|     - | 1650 | ` * php works it out: the first component may be an ALIAS a running archive` |
|     - | 1651 | `` * registered (`phar://composer.phar/bin/composer` inside composer.phar itself),`` |
|     - | 1652 | ` * and otherwise the archive is the longest leading run of components that names` |
|     - | 1653 | ` * a FILE. Walking left to right and stopping at the first file is the same` |
|     - | 1654 | ` * answer for every well-formed url and needs no table of extensions.` |
|     - | 1655 | ` */` |
|   190 | 1656 | `static phl_phar * PharResolveUrl(ph7_vm *pVm,const char *zUrl,int nUrl,` |
|     - | 1657 | `	const char **pzEnt,int *pnEnt,const char **pzErr)` |
|   ! 0 | 1658 | `{` |
|     - | 1659 | `	phl_phar *pPhar;` |
|     - | 1660 | `	int i,nFirst;` |
|   190 | 1661 | `	*pzErr = 0;` |
|   190 | 1662 | `	*pzEnt = "";` |
|   190 | 1663 | `	*pnEnt = 0;` |
|   190 | 1664 | `	while( nUrl > 0 && zUrl[0] == '/' && nUrl > 1 && zUrl[1] == '/' ){` |
|   ! 0 | 1665 | `		zUrl++; nUrl--;` |
|   ! 0 | 1666 | `	}` |
|     - | 1667 | ``	/* The alias form: one component, then `/`. */`` |
|   226 | 1668 | `	for( nFirst = 0 ; nFirst < nUrl && zUrl[nFirst] != '/' ; ++nFirst ){}` |
|   190 | 1669 | `	pPhar = PharFindByAlias(pVm,zUrl,nFirst);` |
|   190 | 1670 | `	if( pPhar ){` |
|     6 | 1671 | `		*pzEnt = &zUrl[nFirst];` |
|     6 | 1672 | `		*pnEnt = nUrl - nFirst;` |
|     6 | 1673 | `		return pPhar;` |
|     - | 1674 | `	}` |
|     - | 1675 | `	/* ...otherwise a path. Try every prefix, shortest first. */` |
|  8982 | 1676 | `	for( i = 1 ; i <= nUrl ; ++i ){` |
|  8979 | 1677 | `		if( i < nUrl && zUrl[i] != '/' ){` |
|  8088 | 1678 | `			continue;` |
|     - | 1679 | `		}` |
|   891 | 1680 | `		pPhar = PharFindByPath(pVm,zUrl,i);` |
|   891 | 1681 | `		if( pPhar == 0 ){` |
|     - | 1682 | `			SyBlob sTry;` |
|     - | 1683 | `			int rc;` |
|   712 | 1684 | `			SyBlobInit(&sTry,&pVm->sAllocator);` |
|   712 | 1685 | `			SyBlobAppend(&sTry,zUrl,(sxu32)i);` |
|   712 | 1686 | `			SyBlobNullAppend(&sTry);` |
|   712 | 1687 | `			rc = PharPathIsFile(pVm,(const char *)SyBlobData(&sTry));` |
|   712 | 1688 | `			if( rc ){` |
|     2 | 1689 | `				const char *zErr = 0;` |
|     2 | 1690 | `				pPhar = PharOpenPath(pVm,(const char *)SyBlobData(&sTry),i,0,&zErr);` |
|     2 | 1691 | `				if( pPhar == 0 ){` |
|   ! 0 | 1692 | `					*pzErr = zErr;` |
|   ! 0 | 1693 | `					SyBlobRelease(&sTry);` |
|   ! 0 | 1694 | `					return 0;` |
|     - | 1695 | `				}` |
|   ! 0 | 1696 | `			}` |
|   712 | 1697 | `			SyBlobRelease(&sTry);` |
|   506 | 1698 | `		}` |
|   891 | 1699 | `		if( pPhar ){` |
|   181 | 1700 | `			*pzEnt = &zUrl[i];` |
|   181 | 1701 | `			*pnEnt = nUrl - i;` |
|   181 | 1702 | `			return pPhar;` |
|     - | 1703 | `		}` |
|   506 | 1704 | `	}` |
|     3 | 1705 | `	*pzErr = "unable to open phar for reading";` |
|     3 | 1706 | `	return 0;` |
|    87 | 1707 | `}` |
|     - | 1708 | `/*` |
|     - | 1709 | `` * The name a phar entry is RECORDED under -- `__FILE__`, `__DIR__`,`` |
|     - | 1710 | `` * `get_included_files()` and the once-registry's key. php canonicalizes it, and`` |
|     - | 1711 | ` * that is not decoration: two spellings of one entry are two entries in the` |
|     - | 1712 | `` * registry, so `require_once __DIR__.'/../inc.php'` and`` |
|     - | 1713 | `` * `require_once __DIR__.'/../vendor/../inc.php'` included the same file TWICE`` |
|     - | 1714 | `` * and the second one died on `Cannot redeclare`. It is exactly how phpstan.phar`` |
|     - | 1715 | `` * reaches its own `src/Analyser/InternalScopeFactory.php` from `preload.php`, so`` |
|     - | 1716 | ` * no phpstan run started. The same two spellings on the FILESYSTEM dedupe (the` |
|     - | 1717 | ` * push realpath()s them); a phar:// url has no realpath() to ask, and the` |
|     - | 1718 | ` * archive's own reader normalizes only what it looks an ENTRY up by.` |
|     - | 1719 | ` *` |
|     - | 1720 | ` * The ARCHIVE half is left exactly as the url spells it -- an alias stays an` |
|     - | 1721 | `` * alias -- because the entry is the half a `..` can be written in.`` |
|     - | 1722 | ` */` |
| 18091 | 1723 | `PH7_PRIVATE int PH7_PharCanonicalUrl(ph7_vm *pVm,const char *zPath,int nPath,SyBlob *pOut)` |
|     5 | 1724 | `{` |
|     - | 1725 | `	const char *zEnt,*zErr;` |
|     - | 1726 | `	phl_phar *pPhar;` |
|     - | 1727 | `	SyBlob sNorm;` |
|     - | 1728 | `	int nEnt,nHead;` |
| 18091 | 1729 | `	if( nPath <= (int)(sizeof("phar://")-1)` |
| 18096 | 1730 | `	 \|\| SyStrnicmp(zPath,"phar://",sizeof("phar://")-1) != 0 ){` |
| 18076 | 1731 | `		return 0;` |
|     - | 1732 | `	}` |
|    20 | 1733 | `	nHead = (int)(sizeof("phar://")-1);` |
|    20 | 1734 | `	pPhar = PharResolveUrl(pVm,&zPath[nHead],nPath - nHead,&zEnt,&nEnt,&zErr);` |
|    20 | 1735 | `	if( pPhar == 0 ){` |
|   ! 0 | 1736 | `		return 0;` |
|     - | 1737 | `	}` |
|    20 | 1738 | `	SyBlobInit(&sNorm,&pVm->sAllocator);` |
|    20 | 1739 | `	PharNormalize(zEnt,nEnt,&sNorm);` |
|    20 | 1740 | `	SyBlobReset(pOut);` |
|    20 | 1741 | `	SyBlobAppend(pOut,zPath,(sxu32)(zEnt - zPath));` |
|    20 | 1742 | `	if( SyBlobLength(&sNorm) > 0 ){` |
|    20 | 1743 | `		SyBlobAppend(pOut,"/",1);` |
|    20 | 1744 | `		SyBlobAppend(pOut,SyBlobData(&sNorm),SyBlobLength(&sNorm));` |
|    10 | 1745 | `	}` |
|    20 | 1746 | `	SyBlobRelease(&sNorm);` |
|    20 | 1747 | `	return 1;` |
|  9046 | 1748 | `}` |
|     - | 1749 | `/* ------------------------------------------------------------------ */` |
|     - | 1750 | `/* The phar:// device                                                  */` |
|     - | 1751 | `/* ------------------------------------------------------------------ */` |
|     - | 1752 | `typedef struct phl_phar_io phl_phar_io;` |
|     - | 1753 | `struct phl_phar_io {` |
|     - | 1754 | `	phl_phar *pPhar;` |
|     - | 1755 | `	phl_phar_ent *pEnt;` |
|     - | 1756 | `	sxu32 nPos;` |
|     - | 1757 | `	int bWrite;` |
|     - | 1758 | `	SyBlob sWrite;` |
|     - | 1759 | `};` |
|     - | 1760 | `typedef struct phl_phar_dir phl_phar_dir;` |
|     - | 1761 | `struct phl_phar_dir {` |
|     - | 1762 | `	ph7_vm *pVm;` |
|     - | 1763 | `	SySet aName;      /* SyBlob per name, in manifest order */` |
|     - | 1764 | `	sxu32 nCur;` |
|     - | 1765 | `};` |
|     - | 1766 | `/* Arm an open failure whose text is BUILT rather than a literal: the engine` |
|     - | 1767 | ` * keeps the pointer and the caller prints it once the open has returned, so the` |
|     - | 1768 | ` * bytes live on the VM. */` |
|     9 | 1769 | `static void PharSetOpenError(ph7_vm *pVm,const char *zFmt,...)` |
|   ! 0 | 1770 | `{` |
|     - | 1771 | `	va_list ap;` |
|     9 | 1772 | `	SyBlobReset(&pVm->sPharErr);` |
|     9 | 1773 | `	va_start(ap,zFmt);` |
|     9 | 1774 | `	SyBlobFormatAp(&pVm->sPharErr,zFmt,ap);` |
|     9 | 1775 | `	va_end(ap);` |
|     9 | 1776 | `	SyBlobNullAppend(&pVm->sPharErr);` |
|     9 | 1777 | `	PH7_StreamSetOpenError(pVm,(const char *)SyBlobData(&pVm->sPharErr));` |
|     9 | 1778 | `}` |
|   199 | 1779 | `static int PharReadonly(ph7_vm *pVm)` |
|   ! 0 | 1780 | `{` |
|   199 | 1781 | `	return PH7_VmIniGetBool(pVm,"phar.readonly",1);` |
|   ! 0 | 1782 | `}` |
|     - | 1783 | `/* php's refusal for every write door the ini closes. */` |
|     - | 1784 | `static const char * const zPharReadonlyStream =` |
|     - | 1785 | `	"phar error: write operations disabled by the php.ini setting phar.readonly";` |
|     - | 1786 |  |
|    67 | 1787 | `static int PharStreamOpen(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|   ! 0 | 1788 | `{` |
|    67 | 1789 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|     - | 1790 | `	phl_phar *pPhar;` |
|     - | 1791 | `	phl_phar_ent *pEnt;` |
|     - | 1792 | `	phl_phar_io *pIo;` |
|     - | 1793 | `	const char *zEnt,*zErr;` |
|    67 | 1794 | `	int nEnt = 0,bWrite;` |
|    67 | 1795 | `	if( pVm == 0 ){` |
|   ! 0 | 1796 | `		return -1;` |
|     - | 1797 | `	}` |
|    67 | 1798 | `	bWrite = (iMode & (PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND)) != 0;` |
|    67 | 1799 | `	pPhar = PharResolveUrl(pVm,zName,(int)SyStrlen(zName),&zEnt,&nEnt,&zErr);` |
|    67 | 1800 | `	if( pPhar == 0 ){` |
|     1 | 1801 | `		if( bWrite && PharReadonly(pVm) ){` |
|     - | 1802 | `			/* php asks the DIRECTIVE before it asks whether there is an archive:` |
|     - | 1803 | `			 * a write into one that does not exist yet is refused for the same` |
|     - | 1804 | `			 * reason creating it would be. */` |
|     1 | 1805 | `			PH7_StreamSetOpenError(pVm,zPharReadonlyStream);` |
|     1 | 1806 | `			return -1;` |
|     - | 1807 | `		}` |
|   ! 0 | 1808 | `		PharSetOpenError(pVm,"phar error: %s",zErr ? zErr : "unable to open phar");` |
|   ! 0 | 1809 | `		return -1;` |
|     - | 1810 | `	}` |
|    66 | 1811 | `	if( bWrite && PharReadonly(pVm) && !pPhar->bData ){` |
|   ! 0 | 1812 | `		PH7_StreamSetOpenError(pVm,zPharReadonlyStream);` |
|   ! 0 | 1813 | `		return -1;` |
|     - | 1814 | `	}` |
|    66 | 1815 | `	pEnt = PharFindEnt(pPhar,zEnt,nEnt);` |
|    66 | 1816 | `	if( pEnt == 0 \|\| pEnt->bDir ){` |
|     9 | 1817 | `		if( !bWrite ){` |
|     - | 1818 | `			/* php's own sentence, which names the entry and the archive. */` |
|     9 | 1819 | `			const char *zE = zEnt;` |
|     9 | 1820 | `			int nE = nEnt;` |
|    18 | 1821 | `			while( nE > 0 && zE[0] == '/' ){ zE++; nE--; }` |
|    13 | 1822 | `			PharSetOpenError(pVm,"phar error: \"%.*s\" is not a file in phar \"%.*s\"",` |
|     9 | 1823 | `				nE,zE,(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));` |
|     9 | 1824 | `			return -1;` |
|     - | 1825 | `		}` |
|   ! 0 | 1826 | `		pEnt = 0;` |
|   ! 0 | 1827 | `	}` |
|    57 | 1828 | `	if( pEnt && PharEntLoad(pPhar,pEnt) != 0 ){` |
|   ! 0 | 1829 | `		PH7_StreamSetOpenError(pVm,"phar error: internal corruption of phar (unable to read entry)");` |
|   ! 0 | 1830 | `		return -1;` |
|     - | 1831 | `	}` |
|    57 | 1832 | `	pIo = (phl_phar_io *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar_io));` |
|    57 | 1833 | `	if( pIo == 0 ){` |
|   ! 0 | 1834 | `		return -1;` |
|     - | 1835 | `	}` |
|    57 | 1836 | `	SyZero(pIo,sizeof(*pIo));` |
|    57 | 1837 | `	pIo->pPhar = pPhar;` |
|    57 | 1838 | `	pIo->pEnt = pEnt;` |
|    57 | 1839 | `	pIo->bWrite = bWrite;` |
|    57 | 1840 | `	SyBlobInit(&pIo->sWrite,&pVm->sAllocator);` |
|    57 | 1841 | `	if( bWrite ){` |
|     - | 1842 | `		/* A write door open on a name: the bytes land on the entry at close,` |
|     - | 1843 | `		 * which is where an archive is rewritten. */` |
|   ! 0 | 1844 | `		SyBlobAppend(&pIo->sWrite,"",0);` |
|   ! 0 | 1845 | `		if( pEnt && (iMode & PH7_IO_OPEN_APPEND) ){` |
|   ! 0 | 1846 | `			SyBlobAppend(&pIo->sWrite,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));` |
|   ! 0 | 1847 | `		}` |
|   ! 0 | 1848 | `		if( pEnt == 0 ){` |
|     - | 1849 | `			SyBlob sName;` |
|   ! 0 | 1850 | `			SyBlobInit(&sName,&pVm->sAllocator);` |
|   ! 0 | 1851 | `			SyBlobAppend(&sName,zEnt,(sxu32)nEnt);` |
|   ! 0 | 1852 | `			pIo->pEnt = PharNewEnt(pPhar,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName));` |
|   ! 0 | 1853 | `			SyBlobRelease(&sName);` |
|   ! 0 | 1854 | `			if( pIo->pEnt == 0 ){` |
|   ! 0 | 1855 | `				SyBlobRelease(&pIo->sWrite);` |
|   ! 0 | 1856 | `				SyMemBackendFree(&pVm->sAllocator,pIo);` |
|   ! 0 | 1857 | `				return -1;` |
|     - | 1858 | `			}` |
|   ! 0 | 1859 | `			pIo->pEnt->nFlags = 0666;` |
|   ! 0 | 1860 | `			pIo->pEnt->bLoaded = 1;` |
|   ! 0 | 1861 | `		}` |
|   ! 0 | 1862 | `	}` |
|    57 | 1863 | `	*ppHandle = (void *)pIo;` |
|    57 | 1864 | `	return PH7_OK;` |
|    30 | 1865 | `}` |
|    90 | 1866 | `static ph7_int64 PharStreamRead(void *pHandle,void *pBuffer,ph7_int64 nWant)` |
|   ! 0 | 1867 | `{` |
|    90 | 1868 | `	phl_phar_io *pIo = (phl_phar_io *)pHandle;` |
|     - | 1869 | `	sxu32 nHave;` |
|    90 | 1870 | `	if( pIo == 0 \|\| pIo->pEnt == 0 \|\| pIo->bWrite ){` |
|   ! 0 | 1871 | `		return -1;` |
|     - | 1872 | `	}` |
|    90 | 1873 | `	nHave = SyBlobLength(&pIo->pEnt->sData);` |
|    90 | 1874 | `	if( pIo->nPos >= nHave ){` |
|    45 | 1875 | `		return 0;` |
|     - | 1876 | `	}` |
|    45 | 1877 | `	if( (ph7_int64)(nHave - pIo->nPos) < nWant ){` |
|    45 | 1878 | `		nWant = (ph7_int64)(nHave - pIo->nPos);` |
|    21 | 1879 | `	}` |
|    45 | 1880 | `	SyMemcpy((const char *)SyBlobData(&pIo->pEnt->sData) + pIo->nPos,pBuffer,(sxu32)nWant);` |
|    45 | 1881 | `	pIo->nPos += (sxu32)nWant;` |
|    45 | 1882 | `	return nWant;` |
|    42 | 1883 | `}` |
|   ! 0 | 1884 | `static ph7_int64 PharStreamWrite(void *pHandle,const void *pData,ph7_int64 nLen)` |
|   ! 0 | 1885 | `{` |
|   ! 0 | 1886 | `	phl_phar_io *pIo = (phl_phar_io *)pHandle;` |
|   ! 0 | 1887 | `	if( pIo == 0 \|\| !pIo->bWrite ){` |
|   ! 0 | 1888 | `		return -1;` |
|     - | 1889 | `	}` |
|   ! 0 | 1890 | `	SyBlobAppend(&pIo->sWrite,pData,(sxu32)nLen);` |
|   ! 0 | 1891 | `	return nLen;` |
|   ! 0 | 1892 | `}` |
|   ! 0 | 1893 | `static int PharStreamSeek(void *pHandle,ph7_int64 iOfft,int whence)` |
|   ! 0 | 1894 | `{` |
|   ! 0 | 1895 | `	phl_phar_io *pIo = (phl_phar_io *)pHandle;` |
|     - | 1896 | `	ph7_int64 iTarget;` |
|     - | 1897 | `	sxu32 nLen;` |
|   ! 0 | 1898 | `	if( pIo == 0 \|\| pIo->pEnt == 0 ){` |
|   ! 0 | 1899 | `		return -1;` |
|     - | 1900 | `	}` |
|   ! 0 | 1901 | `	nLen = pIo->bWrite ? SyBlobLength(&pIo->sWrite) : SyBlobLength(&pIo->pEnt->sData);` |
|   ! 0 | 1902 | `	iTarget = whence == 1 ? (ph7_int64)pIo->nPos + iOfft` |
|   ! 0 | 1903 | `	       : (whence == 2 ? (ph7_int64)nLen + iOfft : iOfft);` |
|   ! 0 | 1904 | `	if( iTarget < 0 ){` |
|   ! 0 | 1905 | `		return -1;` |
|     - | 1906 | `	}` |
|   ! 0 | 1907 | `	pIo->nPos = (sxu32)iTarget;` |
|   ! 0 | 1908 | `	return PH7_OK;` |
|   ! 0 | 1909 | `}` |
|   ! 0 | 1910 | `static ph7_int64 PharStreamTell(void *pHandle)` |
|   ! 0 | 1911 | `{` |
|   ! 0 | 1912 | `	phl_phar_io *pIo = (phl_phar_io *)pHandle;` |
|   ! 0 | 1913 | `	return pIo ? (ph7_int64)pIo->nPos : -1;` |
|   ! 0 | 1914 | `}` |
|     - | 1915 | `/* Fill php's thirteen stat fields for one entry (or for a directory). */` |
|    10 | 1916 | `static void PharFillStat(phl_phar *pPhar,phl_phar_ent *pEnt,int bDir,ph7_int64 *aVal)` |
|   ! 0 | 1917 | `{` |
|     - | 1918 | `	int i;` |
|   140 | 1919 | `	for( i = 0 ; i < 13 ; ++i ){` |
|   130 | 1920 | `		aVal[i] = 0;` |
|    65 | 1921 | `	}` |
|    10 | 1922 | `	aVal[2] = bDir ? (ph7_int64)(PH7_S_IFDIR \| 0777) : (ph7_int64)(PH7_S_IFREG \| 0666);` |
|    10 | 1923 | `	aVal[3] = 1;` |
|    10 | 1924 | `	aVal[7] = (pEnt && !bDir) ? (ph7_int64)pEnt->nSize : 0;` |
|    10 | 1925 | `	aVal[8] = aVal[9] = aVal[10] = pEnt ? pEnt->iTime : 0;` |
|    10 | 1926 | `	aVal[11] = 512;` |
|    10 | 1927 | `	aVal[12] = (aVal[7] + 511) / 512;` |
|     5 | 1928 | `	SXUNUSED(pPhar);` |
|    10 | 1929 | `}` |
|   ! 0 | 1930 | `static int PharStreamStat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|   ! 0 | 1931 | `{` |
|   ! 0 | 1932 | `	phl_phar_io *pIo = (phl_phar_io *)pHandle;` |
|     - | 1933 | `	ph7_int64 aVal[13];` |
|   ! 0 | 1934 | `	if( pIo == 0 \|\| pIo->pEnt == 0 ){` |
|   ! 0 | 1935 | `		return -1;` |
|     - | 1936 | `	}` |
|   ! 0 | 1937 | `	PharFillStat(pIo->pPhar,pIo->pEnt,0,aVal);` |
|   ! 0 | 1938 | `	return PH7_VfsStatFill(pArray,pWorker,aVal);` |
|   ! 0 | 1939 | `}` |
|     - | 1940 | `/*` |
|     - | 1941 | ` * A DIRECTORY inside an archive: php lists the names that sit directly under it,` |
|     - | 1942 | `` * with no `.` or `..` -- an archive has no such entries to report.`` |
|     - | 1943 | ` */` |
|     4 | 1944 | `static int PharDirOpen(const char *zName,ph7_value *pResource,void **ppHandle)` |
|   ! 0 | 1945 | `{` |
|     4 | 1946 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|     - | 1947 | `	phl_phar *pPhar;` |
|     - | 1948 | `	phl_phar_dir *pDir;` |
|     - | 1949 | `	phl_phar_ent *pEnt;` |
|     - | 1950 | `	const char *zEnt,*zErr;` |
|     4 | 1951 | `	int nEnt = 0;` |
|     4 | 1952 | `	if( pVm == 0 ){` |
|   ! 0 | 1953 | `		return -1;` |
|     - | 1954 | `	}` |
|     4 | 1955 | `	pPhar = PharResolveUrl(pVm,zName,(int)SyStrlen(zName),&zEnt,&nEnt,&zErr);` |
|     4 | 1956 | `	if( pPhar == 0 ){` |
|   ! 0 | 1957 | `		return -1;` |
|     - | 1958 | `	}` |
|     6 | 1959 | `	while( nEnt > 0 && zEnt[0] == '/' ){ zEnt++; nEnt--; }` |
|     4 | 1960 | `	while( nEnt > 0 && zEnt[nEnt-1] == '/' ){ nEnt--; }` |
|     4 | 1961 | `	if( nEnt > 0 && !PharIsDir(pPhar,zEnt,nEnt) ){` |
|   ! 0 | 1962 | `		return -1;` |
|     - | 1963 | `	}` |
|     4 | 1964 | `	pDir = (phl_phar_dir *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar_dir));` |
|     4 | 1965 | `	if( pDir == 0 ){` |
|   ! 0 | 1966 | `		return -1;` |
|     - | 1967 | `	}` |
|     4 | 1968 | `	SyZero(pDir,sizeof(*pDir));` |
|     4 | 1969 | `	pDir->pVm = pVm;` |
|     4 | 1970 | `	SySetInit(&pDir->aName,&pVm->sAllocator,sizeof(SyBlob));` |
|    16 | 1971 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|    12 | 1972 | `		const char *z = (const char *)SyBlobData(&pEnt->sName);` |
|    12 | 1973 | `		int n = (int)SyBlobLength(&pEnt->sName);` |
|     - | 1974 | `		const char *zRest;` |
|     - | 1975 | `		int nRest,i;` |
|    12 | 1976 | `		if( nEnt > 0 ){` |
|     6 | 1977 | `			if( n <= nEnt \|\| z[nEnt] != '/' \|\| SyMemcmp(z,zEnt,(sxu32)nEnt) != 0 ){` |
|     2 | 1978 | `				continue;` |
|     - | 1979 | `			}` |
|     4 | 1980 | `			zRest = &z[nEnt+1];` |
|     4 | 1981 | `			nRest = n - nEnt - 1;` |
|     2 | 1982 | `		}else{` |
|     6 | 1983 | `			zRest = z;` |
|     6 | 1984 | `			nRest = n;` |
|     - | 1985 | `		}` |
|    36 | 1986 | `		for( i = 0 ; i < nRest && zRest[i] != '/' ; ++i ){}` |
|     - | 1987 | `		{` |
|     - | 1988 | `			/* One name per child, however many entries sit under it. */` |
|    10 | 1989 | `			SyBlob *aSeen = (SyBlob *)SySetBasePtr(&pDir->aName);` |
|    10 | 1990 | `			sxu32 k,nSeen = SySetUsed(&pDir->aName);` |
|    10 | 1991 | `			int bDup = 0;` |
|    16 | 1992 | `			for( k = 0 ; k < nSeen ; ++k ){` |
|     8 | 1993 | `				if( (int)SyBlobLength(&aSeen[k]) == i` |
|     5 | 1994 | `				 && SyMemcmp(SyBlobData(&aSeen[k]),zRest,(sxu32)i) == 0 ){` |
|     2 | 1995 | `					bDup = 1;` |
|     2 | 1996 | `					break;` |
|     - | 1997 | `				}` |
|     3 | 1998 | `			}` |
|    10 | 1999 | `			if( !bDup && i > 0 ){` |
|     - | 2000 | `				SyBlob sName;` |
|     8 | 2001 | `				SyBlobInit(&sName,&pVm->sAllocator);` |
|     8 | 2002 | `				SyBlobAppend(&sName,zRest,(sxu32)i);` |
|     8 | 2003 | `				SySetPut(&pDir->aName,(const void *)&sName);` |
|     4 | 2004 | `			}` |
|     - | 2005 | `		}` |
|     5 | 2006 | `	}` |
|     4 | 2007 | `	*ppHandle = (void *)pDir;` |
|     4 | 2008 | `	return PH7_OK;` |
|     2 | 2009 | `}` |
|    12 | 2010 | `static int PharDirRead(void *pHandle,ph7_context *pCtx)` |
|   ! 0 | 2011 | `{` |
|    12 | 2012 | `	phl_phar_dir *pDir = (phl_phar_dir *)pHandle;` |
|     - | 2013 | `	SyBlob *aName;` |
|    12 | 2014 | `	if( pDir == 0 \|\| pDir->nCur >= SySetUsed(&pDir->aName) ){` |
|     4 | 2015 | `		return -1;` |
|     - | 2016 | `	}` |
|     8 | 2017 | `	aName = (SyBlob *)SySetBasePtr(&pDir->aName);` |
|    12 | 2018 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&aName[pDir->nCur]),` |
|     8 | 2019 | `		(int)SyBlobLength(&aName[pDir->nCur]));` |
|     8 | 2020 | `	pDir->nCur++;` |
|     8 | 2021 | `	return PH7_OK;` |
|     6 | 2022 | `}` |
|   ! 0 | 2023 | `static void PharDirRewind(void *pHandle)` |
|   ! 0 | 2024 | `{` |
|   ! 0 | 2025 | `	phl_phar_dir *pDir = (phl_phar_dir *)pHandle;` |
|   ! 0 | 2026 | `	if( pDir ){` |
|   ! 0 | 2027 | `		pDir->nCur = 0;` |
|   ! 0 | 2028 | `	}` |
|   ! 0 | 2029 | `}` |
|     4 | 2030 | `static void PharDirClose(void *pHandle)` |
|   ! 0 | 2031 | `{` |
|     4 | 2032 | `	phl_phar_dir *pDir = (phl_phar_dir *)pHandle;` |
|     - | 2033 | `	SyBlob *aName;` |
|     - | 2034 | `	sxu32 n,i;` |
|     4 | 2035 | `	if( pDir == 0 ){` |
|   ! 0 | 2036 | `		return;` |
|     - | 2037 | `	}` |
|     4 | 2038 | `	aName = (SyBlob *)SySetBasePtr(&pDir->aName);` |
|     4 | 2039 | `	n = SySetUsed(&pDir->aName);` |
|    12 | 2040 | `	for( i = 0 ; i < n ; ++i ){` |
|     8 | 2041 | `		SyBlobRelease(&aName[i]);` |
|     4 | 2042 | `	}` |
|     4 | 2043 | `	SySetRelease(&pDir->aName);` |
|     4 | 2044 | `	SyMemBackendFree(&pDir->pVm->sAllocator,pDir);` |
|     2 | 2045 | `}` |
|     - | 2046 | `/*` |
|     - | 2047 | ` * Closing a handle opened for WRITING is what commits the bytes to the entry --` |
|     - | 2048 | ` * and, for a phar-format archive, rewrites the file. php buffers the same way` |
|     - | 2049 | `` * (its `startBuffering`/`stopBuffering` pair is the explicit form of it).`` |
|     - | 2050 | ` */` |
|    57 | 2051 | `static void PharStreamClose(void *pHandle)` |
|   ! 0 | 2052 | `{` |
|    57 | 2053 | `	phl_phar_io *pIo = (phl_phar_io *)pHandle;` |
|     - | 2054 | `	ph7_vm *pVm;` |
|    57 | 2055 | `	if( pIo == 0 ){` |
|   ! 0 | 2056 | `		return;` |
|     - | 2057 | `	}` |
|    57 | 2058 | `	pVm = pIo->pPhar->pVm;` |
|    57 | 2059 | `	if( pIo->bWrite && pIo->pEnt ){` |
|   ! 0 | 2060 | `		SyBlobReset(&pIo->pEnt->sData);` |
|   ! 0 | 2061 | `		SyBlobAppend(&pIo->pEnt->sData,SyBlobData(&pIo->sWrite),SyBlobLength(&pIo->sWrite));` |
|   ! 0 | 2062 | `		pIo->pEnt->nSize = SyBlobLength(&pIo->pEnt->sData);` |
|   ! 0 | 2063 | `		pIo->pEnt->nCompSize = pIo->pEnt->nSize;` |
|   ! 0 | 2064 | `		pIo->pEnt->nFlags &= ~(sxu32)PHAR_C_MASK;` |
|   ! 0 | 2065 | `		pIo->pEnt->bLoaded = 1;` |
|   ! 0 | 2066 | `		pIo->pEnt->iTime = (sxi64)time(0);` |
|   ! 0 | 2067 | `		PharCommit(pIo->pPhar);` |
|   ! 0 | 2068 | `	}` |
|    57 | 2069 | `	SyBlobRelease(&pIo->sWrite);` |
|    57 | 2070 | `	SyMemBackendFree(&pVm->sAllocator,pIo);` |
|    26 | 2071 | `}` |
|     - | 2072 | `PH7_PRIVATE const ph7_io_stream sPHAR_Stream = {` |
|     - | 2073 | `	"phar",` |
|     - | 2074 | `	PH7_IO_STREAM_VERSION,` |
|     - | 2075 | `	PharStreamOpen,   /* xOpen */` |
|     - | 2076 | `	PharDirOpen,      /* xOpenDir */` |
|     - | 2077 | `	PharStreamClose,  /* xClose */` |
|     - | 2078 | `	PharDirClose,     /* xCloseDir */` |
|     - | 2079 | `	PharStreamRead,   /* xRead */` |
|     - | 2080 | `	PharDirRead,      /* xReadDir */` |
|     - | 2081 | `	PharStreamWrite,  /* xWrite */` |
|     - | 2082 | `	PharStreamSeek,   /* xSeek */` |
|     - | 2083 | `	0,                /* xLock */` |
|     - | 2084 | `	PharDirRewind,    /* xRewindDir */` |
|     - | 2085 | `	PharStreamTell,   /* xTell */` |
|     - | 2086 | `	0,                /* xTrunc */` |
|     - | 2087 | `	0,                /* xSync */` |
|     - | 2088 | `	PharStreamStat    /* xStat */` |
|     - | 2089 | `};` |
|   113 | 2090 | `PH7_PRIVATE int PH7_PharStreamIs(const ph7_io_stream *pStream)` |
|     2 | 2091 | `{` |
|   115 | 2092 | `	return pStream == &sPHAR_Stream;` |
|     2 | 2093 | `}` |
|     - | 2094 | `/*` |
|     - | 2095 | `` * The stat family over a `phar://` path. php routes every one of its members`` |
|     - | 2096 | `` * through the wrapper's url_stat, which is what makes `file_exists()`,`` |
|     - | 2097 | `` * `is_dir()` and `filesize()` answer about the ARCHIVE's contents rather than`` |
|     - | 2098 | ` * about a file of that name on disk -- there is none.` |
|     - | 2099 | ` */` |
|    16 | 2100 | `PH7_PRIVATE int PH7_PharUrlStat(ph7_vm *pVm,const char *zPath,ph7_int64 *aVal)` |
|   ! 0 | 2101 | `{` |
|     - | 2102 | `	phl_phar *pPhar;` |
|     - | 2103 | `	phl_phar_ent *pEnt;` |
|     - | 2104 | `	const char *zEnt,*zErr;` |
|    16 | 2105 | `	int nEnt = 0;` |
|    16 | 2106 | `	pPhar = PharResolveUrl(pVm,zPath,(int)SyStrlen(zPath),&zEnt,&nEnt,&zErr);` |
|    16 | 2107 | `	if( pPhar == 0 ){` |
|   ! 0 | 2108 | `		return -1;` |
|     - | 2109 | `	}` |
|    32 | 2110 | `	while( nEnt > 0 && zEnt[0] == '/' ){ zEnt++; nEnt--; }` |
|    16 | 2111 | `	while( nEnt > 0 && zEnt[nEnt-1] == '/' ){ nEnt--; }` |
|    16 | 2112 | `	pEnt = PharFindEnt(pPhar,zEnt,nEnt);` |
|    16 | 2113 | `	if( pEnt && !pEnt->bDir ){` |
|     4 | 2114 | `		PharFillStat(pPhar,pEnt,0,aVal);` |
|     4 | 2115 | `		return 0;` |
|     - | 2116 | `	}` |
|    12 | 2117 | `	if( PharIsDir(pPhar,zEnt,nEnt) ){` |
|     6 | 2118 | `		PharFillStat(pPhar,pEnt,1,aVal);` |
|     6 | 2119 | `		return 0;` |
|     - | 2120 | `	}` |
|     6 | 2121 | `	return -1;` |
|     8 | 2122 | `}` |
|     - | 2123 | `/* ------------------------------------------------------------------ */` |
|     - | 2124 | `/* The classes                                                         */` |
|     - | 2125 | `/* ------------------------------------------------------------------ */` |
|     - | 2126 | `/*` |
|     - | 2127 | ``  * A Phar OBJECT is two things at once, and php's own `spl_filesystem_object` `` |
|     - | 2128 | ` * is why: an ARCHIVE (what every verb below asks about) and a DIRECTORY` |
|     - | 2129 | ``  * ITERATOR positioned on one of its entries. That is what makes `getPath()` `` |
|     - | 2130 | `` * answer the archive while `getFilename()` answers the entry the cursor is on,`` |
|     - | 2131 | ` * and it is the whole reason Phar extends RecursiveDirectoryIterator rather` |
|     - | 2132 | ` * than holding one.` |
|     - | 2133 | ` */` |
|     - | 2134 | `#define PHAR_SLOT_RES  "__res"    /* the phl_phar */` |
|     - | 2135 | `#define PHAR_SLOT_CUR  "__cur"    /* the iteration cursor */` |
|     - | 2136 | `#define PHAR_SLOT_DIR  "__dir"    /* the directory this iterator walks ("" = the root) */` |
|     - | 2137 | `#define PHAR_SLOT_ENT  "__ent"    /* PharFileInfo: the entry name */` |
|     - | 2138 | `#define PHAR_SLOT_PHAR "__phar"   /* PharFileInfo: the archive's path */` |
|     - | 2139 |  |
|     - | 2140 | `/*` |
|     - | 2141 | ` * The two slots php's SplFileInfo keeps its pathname in. A Phar and a` |
|     - | 2142 | ` * PharFileInfo are both SplFileInfos, and every inherited accessor reads them,` |
|     - | 2143 | `` * so filling them is what makes `getBasename()`, `getExtension()` and`` |
|     - | 2144 | `` * `__toString()` answer without a line of code here.`` |
|     - | 2145 | ` */` |
|    77 | 2146 | `static void PharSetSplPath(ph7_vm *pVm,ph7_class_instance *pThis,const char *zPath,int nPath)` |
|   ! 0 | 2147 | `{` |
|    77 | 2148 | `	int nFile = nPath,nDir;` |
|   114 | 2149 | `	while( nFile > 1 && (zPath[nFile-1] == '/' \|\| zPath[nFile-1] == '\\') ){` |
|   ! 0 | 2150 | `		nFile--;` |
|   ! 0 | 2151 | `	}` |
|    77 | 2152 | `	nDir = nFile;` |
|   540 | 2153 | `	while( nDir > 1 && zPath[nDir-1] != '/' && zPath[nDir-1] != '\\' ){` |
|   463 | 2154 | `		nDir--;` |
|   ! 0 | 2155 | `	}` |
|    77 | 2156 | `	if( nDir > 0 ){` |
|    77 | 2157 | `		nDir--;` |
|    37 | 2158 | `	}` |
|    77 | 2159 | `	PH7_NativeSetAttrStr(pVm,pThis,"__n",zPath,nFile);` |
|    77 | 2160 | `	PH7_NativeSetAttrStr(pVm,pThis,"__p",zPath,nDir);` |
|    77 | 2161 | `}` |
|   326 | 2162 | `static phl_phar * PharOfInstance(ph7_class_instance *pThis)` |
|   ! 0 | 2163 | `{` |
|     - | 2164 | `	ph7_value *pRes;` |
|   326 | 2165 | `	if( pThis == 0 ){` |
|   ! 0 | 2166 | `		return 0;` |
|     - | 2167 | `	}` |
|   326 | 2168 | `	pRes = PH7_NativeAttr(pThis,PHAR_SLOT_RES);` |
|   326 | 2169 | `	if( pRes == 0 \|\| (pRes->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 2170 | `		return 0;` |
|     - | 2171 | `	}` |
|   326 | 2172 | `	return (phl_phar *)pRes->x.pOther;` |
|   159 | 2173 | `}` |
|    77 | 2174 | `static void PharAttach(ph7_class_instance *pThis,phl_phar *pPhar)` |
|   ! 0 | 2175 | `{` |
|    77 | 2176 | `	ph7_value *pRes = PH7_NativeAttr(pThis,PHAR_SLOT_RES);` |
|    77 | 2177 | `	if( pRes ){` |
|    77 | 2178 | `		PH7_MemObjRelease(pRes);` |
|    77 | 2179 | `		pRes->x.pOther = pPhar;` |
|    77 | 2180 | `		MemObjSetType(pRes,MEMOBJ_RES);` |
|    37 | 2181 | `	}` |
|    77 | 2182 | `}` |
|    73 | 2183 | `static void PharSetInt(ph7_class_instance *pThis,const char *zSlot,sxi64 iVal)` |
|   ! 0 | 2184 | `{` |
|    73 | 2185 | `	ph7_value *pVal = PH7_NativeAttr(pThis,zSlot);` |
|    73 | 2186 | `	if( pVal ){` |
|    73 | 2187 | `		PH7_MemObjRelease(pVal);` |
|    73 | 2188 | `		PH7_MemObjInitFromInt(pThis->pVm,pVal,iVal);` |
|    35 | 2189 | `	}` |
|    73 | 2190 | `}` |
|    78 | 2191 | `static void PharSetStr(ph7_class_instance *pThis,const char *zSlot,const char *zVal,int nVal)` |
|   ! 0 | 2192 | `{` |
|    78 | 2193 | `	ph7_value *pVal = PH7_NativeAttr(pThis,zSlot);` |
|    78 | 2194 | `	if( pVal ){` |
|    78 | 2195 | `		PH7_MemObjRelease(pVal);` |
|    78 | 2196 | `		MemObjSetType(pVal,MEMOBJ_STRING);` |
|    78 | 2197 | `		SyBlobReset(&pVal->sBlob);` |
|    78 | 2198 | `		if( nVal > 0 ){` |
|    56 | 2199 | `			SyBlobAppend(&pVal->sBlob,zVal,(sxu32)nVal);` |
|    28 | 2200 | `		}` |
|    39 | 2201 | `	}` |
|    78 | 2202 | `}` |
|     - | 2203 | `/* The archive a method was called on, with php's refusal for an object whose` |
|     - | 2204 | ` * constructor never ran. */` |
|   187 | 2205 | `static phl_phar * PharThis(ph7_context *pCtx)` |
|   ! 0 | 2206 | `{` |
|   187 | 2207 | `	phl_phar *pPhar = PharOfInstance(PH7_ContextThis(pCtx));` |
|   187 | 2208 | `	if( pPhar == 0 ){` |
|   ! 0 | 2209 | `		PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 2210 | `			"Cannot call method on an uninitialized Phar object");` |
|   ! 0 | 2211 | `		return 0;` |
|     - | 2212 | `	}` |
|   187 | 2213 | `	return pPhar;` |
|    90 | 2214 | `}` |
|     - | 2215 | `/* php's five write refusals, one per door. Each names its own door's verb,` |
|     - | 2216 | ` * which is why they cannot share a sentence. */` |
|     - | 2217 | `#define PHAR_RO_WRITE   0   /* "Cannot write out phar archive, phar is read-only" */` |
|     - | 2218 | `#define PHAR_RO_SETTING 1   /* "Write operations disabled by the php.ini setting phar.readonly" */` |
|     - | 2219 | `#define PHAR_RO_STUB    2   /* "Cannot change stub, phar is read-only" */` |
|     - | 2220 | `#define PHAR_RO_COMPRESS 3  /* "Phar is readonly, cannot change compression" */` |
|    49 | 2221 | `static int PharRefuseWrite(ph7_context *pCtx,int eKind)` |
|   ! 0 | 2222 | `{` |
|    49 | 2223 | `	phl_phar *pPhar = PharOfInstance(PH7_ContextThis(pCtx));` |
|    49 | 2224 | `	if( !PharReadonly(pCtx->pVm) \|\| (pPhar && pPhar->bData) ){` |
|     - | 2225 | ``		/* `phar.readonly` guards the archives that can be EXECUTED. A PharData`` |
|     - | 2226 | `		 * is a plain tar or zip and php lets a script write one whatever the` |
|     - | 2227 | `		 * directive says -- which is what makes it the class an installer uses` |
|     - | 2228 | `		 * to build a distribution archive on a stock php. */` |
|    49 | 2229 | `		return 0;` |
|     - | 2230 | `	}` |
|   ! 0 | 2231 | `	switch( eKind ){` |
|   ! 0 | 2232 | `	case PHAR_RO_SETTING:` |
|   ! 0 | 2233 | `		PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 2234 | `			"Write operations disabled by the php.ini setting phar.readonly");` |
|   ! 0 | 2235 | `		break;` |
|   ! 0 | 2236 | `	case PHAR_RO_STUB:` |
|   ! 0 | 2237 | `		PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 2238 | `			"Cannot change stub, phar is read-only");` |
|   ! 0 | 2239 | `		break;` |
|   ! 0 | 2240 | `	case PHAR_RO_COMPRESS:` |
|   ! 0 | 2241 | `		PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 2242 | `			"Phar is readonly, cannot change compression");` |
|   ! 0 | 2243 | `		break;` |
|   ! 0 | 2244 | `	default:` |
|   ! 0 | 2245 | `		PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 2246 | `			"Cannot write out phar archive, phar is read-only");` |
|   ! 0 | 2247 | `		break;` |
|     - | 2248 | `	}` |
|   ! 0 | 2249 | `	return 1;` |
|    24 | 2250 | `}` |
|     - | 2251 | `/*` |
|     - | 2252 | ` * A Phar object iterates a DIRECTORY of the archive, not its manifest: php's` |
|     - | 2253 | `` * `foreach ($phar as $f)` yields the five names at the archive's root, and a`` |
|     - | 2254 | ` * RecursiveIteratorIterator over it descends into them to reach all 831. The` |
|     - | 2255 | ` * children of a directory are the distinct first segments of the entry names` |
|     - | 2256 | ` * under it, which is the same rule the wrapper's opendir() follows -- a` |
|     - | 2257 | ` * directory nothing declares still has children.` |
|     - | 2258 | ` *` |
|     - | 2259 | ` * The list is walked rather than built: an archive's manifest is short enough` |
|     - | 2260 | ` * that the Nth child costs one pass, and the alternative is a per-instance` |
|     - | 2261 | ` * cache the engine would have to free.` |
|     - | 2262 | ` */` |
|    58 | 2263 | `static int PharNthChild(phl_phar *pPhar,const char *zDir,int nDir,sxi64 iIdx,` |
|     - | 2264 | `	SyBlob *pName,int *pbDir)` |
|   ! 0 | 2265 | `{` |
|     - | 2266 | `	phl_phar_ent *pEnt;` |
|     - | 2267 | `	SyBlob sSeen;` |
|    58 | 2268 | `	int bFound = 0;` |
|     - | 2269 | `	/*` |
|     - | 2270 | `` 	 * php lists a phar directory SORTED by name (its own `phar_compare_dir_name` `` |
|     - | 2271 | `	 * over the whole listing), not in manifest order -- composer.phar's manifest` |
|     - | 2272 | ``	 * starts at `src/...` and php's first child is `LICENSE`. The walk below`` |
|     - | 2273 | `	 * therefore counts children in byte order rather than in the order the` |
|     - | 2274 | `	 * entries appear.` |
|     - | 2275 | `	 */` |
|    58 | 2276 | `	SyBlobInit(&sSeen,&pPhar->pVm->sAllocator);` |
|   232 | 2277 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|   174 | 2278 | `		const char *z = (const char *)SyBlobData(&pEnt->sName);` |
|   174 | 2279 | `		int n = (int)SyBlobLength(&pEnt->sName);` |
|     - | 2280 | `		const char *zRest;` |
|   174 | 2281 | `		int nRest,nSeg,bDup = 0;` |
|     - | 2282 | `		sxu32 k;` |
|   174 | 2283 | `		if( nDir > 0 ){` |
|    78 | 2284 | `			if( n <= nDir \|\| z[nDir] != '/' \|\| SyMemcmp(z,zDir,(sxu32)nDir) != 0 ){` |
|    36 | 2285 | `				continue;` |
|     - | 2286 | `			}` |
|    42 | 2287 | `			zRest = &z[nDir+1];` |
|    42 | 2288 | `			nRest = n - nDir - 1;` |
|    21 | 2289 | `		}else{` |
|    96 | 2290 | `			zRest = z;` |
|    96 | 2291 | `			nRest = n;` |
|     - | 2292 | `		}` |
|   508 | 2293 | `		for( nSeg = 0 ; nSeg < nRest && zRest[nSeg] != '/' ; ++nSeg ){}` |
|   138 | 2294 | `		if( nSeg < 1 ){` |
|   ! 0 | 2295 | `			continue;` |
|     - | 2296 | `		}` |
|     - | 2297 | `		/* One row per NAME, whatever sits under it. */` |
|   218 | 2298 | `		for( k = 0 ; k + 1 < SyBlobLength(&sSeen) ; ){` |
|   112 | 2299 | `			int nThis = (int)((const unsigned char *)SyBlobData(&sSeen))[k];` |
|   112 | 2300 | `			if( nThis == nSeg` |
|    72 | 2301 | `			 && SyMemcmp((const char *)SyBlobData(&sSeen) + k + 1,zRest,(sxu32)nSeg) == 0 ){` |
|    32 | 2302 | `				bDup = 1;` |
|    32 | 2303 | `				break;` |
|     - | 2304 | `			}` |
|    80 | 2305 | `			k += (sxu32)nThis + 1;` |
|   ! 0 | 2306 | `		}` |
|   138 | 2307 | `		if( bDup ){` |
|    32 | 2308 | `			continue;` |
|     - | 2309 | `		}` |
|   106 | 2310 | `		if( nSeg < 256 ){` |
|   106 | 2311 | `			unsigned char cLen = (unsigned char)nSeg;` |
|   106 | 2312 | `			SyBlobAppend(&sSeen,&cLen,1);` |
|   106 | 2313 | `			SyBlobAppend(&sSeen,zRest,(sxu32)nSeg);` |
|    53 | 2314 | `		}` |
|     - | 2315 | `		/* Every child is collected; which one the cursor wants is decided` |
|     - | 2316 | `		 * after the walk, in NAME order. */` |
|     - | 2317 | `		(void)0;` |
|    53 | 2318 | `	}` |
|     - | 2319 | `	{` |
|     - | 2320 | `		/* Pick the iIdx-th smallest name out of what was collected. */` |
|    58 | 2321 | `		const unsigned char *z = (const unsigned char *)SyBlobData(&sSeen);` |
|    58 | 2322 | `		sxu32 nAll = SyBlobLength(&sSeen);` |
|     - | 2323 | `		sxu32 k;` |
|    58 | 2324 | `		const char *zBest = 0;` |
|    58 | 2325 | `		int nBest = 0;` |
|    65 | 2326 | `		for(;;){` |
|    94 | 2327 | `			const char *zMin = 0;` |
|    94 | 2328 | `			int nMin = 0;` |
|   270 | 2329 | `			for( k = 0 ; k + 1 <= nAll ; ){` |
|   176 | 2330 | `				int nThis = (int)z[k];` |
|   176 | 2331 | `				const char *zThis = (const char *)&z[k+1];` |
|   211 | 2332 | `				int bAfterPrev = zBest == 0` |
|   123 | 2333 | `					\|\| SyMemcmp(zThis,zBest,(sxu32)(nThis < nBest ? nThis : nBest)) > 0` |
|   247 | 2334 | `					\|\| (SyMemcmp(zThis,zBest,(sxu32)(nThis < nBest ? nThis : nBest)) == 0` |
|    40 | 2335 | `						&& nThis > nBest);` |
|   176 | 2336 | `				if( bAfterPrev ){` |
|   156 | 2337 | `					int bLess = zMin == 0` |
|    90 | 2338 | `						\|\| SyMemcmp(zThis,zMin,(sxu32)(nThis < nMin ? nThis : nMin)) < 0` |
|   156 | 2339 | `						\|\| (SyMemcmp(zThis,zMin,(sxu32)(nThis < nMin ? nThis : nMin)) == 0` |
|    24 | 2340 | `							&& nThis < nMin);` |
|   132 | 2341 | `					if( bLess ){` |
|    84 | 2342 | `						zMin = zThis;` |
|    84 | 2343 | `						nMin = nThis;` |
|    42 | 2344 | `					}` |
|    66 | 2345 | `				}` |
|   176 | 2346 | `				k += (sxu32)nThis + 1;` |
|   ! 0 | 2347 | `			}` |
|    94 | 2348 | `			if( zMin == 0 ){` |
|    10 | 2349 | `				break;` |
|     - | 2350 | `			}` |
|    84 | 2351 | `			if( iIdx == 0 ){` |
|    48 | 2352 | `				SyBlobReset(pName);` |
|    48 | 2353 | `				if( nDir > 0 ){` |
|    22 | 2354 | `					SyBlobAppend(pName,zDir,(sxu32)nDir);` |
|    22 | 2355 | `					SyBlobAppend(pName,"/",1);` |
|    11 | 2356 | `				}` |
|    48 | 2357 | `				SyBlobAppend(pName,zMin,(sxu32)nMin);` |
|    48 | 2358 | `				if( pbDir ){` |
|     - | 2359 | `					SyBlob sFull;` |
|    24 | 2360 | `					SyBlobInit(&sFull,&pPhar->pVm->sAllocator);` |
|    24 | 2361 | `					SyBlobAppend(&sFull,SyBlobData(pName),SyBlobLength(pName));` |
|    55 | 2362 | `					*pbDir = PharFindEnt(pPhar,(const char *)SyBlobData(&sFull),` |
|    36 | 2363 | `						(int)SyBlobLength(&sFull)) == 0` |
|    31 | 2364 | `						\|\| PharIsDir(pPhar,(const char *)SyBlobData(&sFull),` |
|    14 | 2365 | `							(int)SyBlobLength(&sFull));` |
|    24 | 2366 | `					SyBlobRelease(&sFull);` |
|    12 | 2367 | `				}` |
|    48 | 2368 | `				bFound = 1;` |
|    48 | 2369 | `				break;` |
|     - | 2370 | `			}` |
|    36 | 2371 | `			iIdx--;` |
|    36 | 2372 | `			zBest = zMin;` |
|    36 | 2373 | `			nBest = nMin;` |
|   ! 0 | 2374 | `		}` |
|     - | 2375 | `	}` |
|    58 | 2376 | `	SyBlobRelease(&sSeen);` |
|    58 | 2377 | `	return bFound;` |
|   ! 0 | 2378 | `}` |
|     - | 2379 | `/* The directory an instance walks, and the child its cursor is on. */` |
|    58 | 2380 | `static void PharCursorDir(ph7_class_instance *pThis,SyBlob *pDir)` |
|   ! 0 | 2381 | `{` |
|    58 | 2382 | `	ph7_value *pVal = PH7_NativeAttr(pThis,PHAR_SLOT_DIR);` |
|    58 | 2383 | `	SyBlobReset(pDir);` |
|    58 | 2384 | `	if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){` |
|    26 | 2385 | `		SyBlobAppend(pDir,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|    13 | 2386 | `	}` |
|    58 | 2387 | `}` |
|    58 | 2388 | `static int PharCursorChild(ph7_class_instance *pThis,phl_phar *pPhar,SyBlob *pName,int *pbDir)` |
|   ! 0 | 2389 | `{` |
|     - | 2390 | `	SyBlob sDir;` |
|     - | 2391 | `	int rc;` |
|    58 | 2392 | `	SyBlobInit(&sDir,&pPhar->pVm->sAllocator);` |
|    58 | 2393 | `	PharCursorDir(pThis,&sDir);` |
|    87 | 2394 | `	rc = PharNthChild(pPhar,(const char *)SyBlobData(&sDir),(int)SyBlobLength(&sDir),` |
|    29 | 2395 | `		PH7_NativeAttrInt(pThis,PHAR_SLOT_CUR),pName,pbDir);` |
|    58 | 2396 | `	SyBlobRelease(&sDir);` |
|    58 | 2397 | `	return rc;` |
|   ! 0 | 2398 | `}` |
|     - | 2399 | ``/* Build `phar://<archive>/<entry>`, the name every phar path is written as. */`` |
|    30 | 2400 | `static void PharUrl(phl_phar *pPhar,const char *zEnt,int nEnt,SyBlob *pOut)` |
|   ! 0 | 2401 | `{` |
|    30 | 2402 | `	SyBlobAppend(pOut,"phar://",sizeof("phar://")-1);` |
|    30 | 2403 | `	SyBlobAppend(pOut,SyBlobData(&pPhar->sPath),SyBlobLength(&pPhar->sPath));` |
|    30 | 2404 | `	SyBlobAppend(pOut,"/",1);` |
|    30 | 2405 | `	if( nEnt > 0 ){` |
|    30 | 2406 | `		SyBlobAppend(pOut,zEnt,(sxu32)nEnt);` |
|    15 | 2407 | `	}` |
|    30 | 2408 | `}` |
|     - | 2409 | `/* A PharFileInfo for one entry: php hands one back from offsetGet() and from` |
|     - | 2410 | ` * every step of the iteration. */` |
|    24 | 2411 | `static ph7_class_instance * PharNewFileInfo(ph7_context *pCtx,phl_phar *pPhar,` |
|     - | 2412 | `	phl_phar_ent *pEnt)` |
|   ! 0 | 2413 | `{` |
|    24 | 2414 | `	ph7_vm *pVm = pCtx->pVm;` |
|    24 | 2415 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,"PharFileInfo",sizeof("PharFileInfo")-1,FALSE,0);` |
|     - | 2416 | `	ph7_class_instance *pThis;` |
|     - | 2417 | `	SyBlob sUrl;` |
|    24 | 2418 | `	if( pClass == 0 ){` |
|   ! 0 | 2419 | `		return 0;` |
|     - | 2420 | `	}` |
|    24 | 2421 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|    24 | 2422 | `	if( pThis == 0 ){` |
|   ! 0 | 2423 | `		return 0;` |
|     - | 2424 | `	}` |
|    24 | 2425 | `	SyBlobInit(&sUrl,&pVm->sAllocator);` |
|    24 | 2426 | `	PharUrl(pPhar,(const char *)SyBlobData(&pEnt->sName),(int)SyBlobLength(&pEnt->sName),&sUrl);` |
|     - | 2427 | `	/* The inherited SplFileInfo half answers about the ENTRY's url, which is` |
|     - | 2428 | ``	 * what php shows for `getPathname()`. */`` |
|    24 | 2429 | `	PharSetSplPath(pVm,pThis,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|    36 | 2430 | `	PharSetStr(pThis,PHAR_SLOT_ENT,(const char *)SyBlobData(&pEnt->sName),` |
|    24 | 2431 | `		(int)SyBlobLength(&pEnt->sName));` |
|    36 | 2432 | `	PharSetStr(pThis,PHAR_SLOT_PHAR,(const char *)SyBlobData(&pPhar->sPath),` |
|    24 | 2433 | `		(int)SyBlobLength(&pPhar->sPath));` |
|    24 | 2434 | `	PharAttach(pThis,pPhar);` |
|    24 | 2435 | `	SyBlobRelease(&sUrl);` |
|    24 | 2436 | `	return pThis;` |
|    12 | 2437 | `}` |
|     - | 2438 | `/* The entry a PharFileInfo names. */` |
|    26 | 2439 | `static phl_phar_ent * PharFileInfoEnt(ph7_context *pCtx,phl_phar **ppPhar)` |
|   ! 0 | 2440 | `{` |
|    26 | 2441 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    26 | 2442 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 2443 | `	ph7_value *pName;` |
|    26 | 2444 | `	if( pPhar == 0 ){` |
|   ! 0 | 2445 | `		PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 2446 | `			"Cannot call method on an uninitialized PharFileInfo object");` |
|   ! 0 | 2447 | `		return 0;` |
|     - | 2448 | `	}` |
|    26 | 2449 | `	pName = PH7_NativeAttr(pThis,PHAR_SLOT_ENT);` |
|    26 | 2450 | `	if( ppPhar ){` |
|    26 | 2451 | `		*ppPhar = pPhar;` |
|    13 | 2452 | `	}` |
|    26 | 2453 | `	if( pName == 0 ){` |
|   ! 0 | 2454 | `		return 0;` |
|     - | 2455 | `	}` |
|    39 | 2456 | `	return PharFindEnt(pPhar,(const char *)SyBlobData(&pName->sBlob),` |
|    26 | 2457 | `		(int)SyBlobLength(&pName->sBlob));` |
|    13 | 2458 | `}` |
|     - | 2459 | `/*` |
|     - | 2460 | `` * Which format a NAME asks for. php reads the extension: `.phar` is its own`` |
|     - | 2461 | ``  * format and only an executable archive may carry it, `.tar`/`.tar.gz`/`.tgz` `` |
|     - | 2462 | `` * are tar and `.zip` is zip. Anything else is not an archive name at all.`` |
|     - | 2463 | ` */` |
|    78 | 2464 | `static int PharFormatFromName(const char *zPath,int nPath,int bData)` |
|   ! 0 | 2465 | `{` |
|     - | 2466 | `	static const struct { const char *zExt; int iFmt; int bDataOnly; } aExt[] = {` |
|     - | 2467 | `		{ ".phar", PHAR_FORMAT_PHAR, 0 },` |
|     - | 2468 | `		{ ".tar",  PHAR_FORMAT_TAR,  1 },` |
|     - | 2469 | `		{ ".tgz",  PHAR_FORMAT_TAR,  1 },` |
|     - | 2470 | `		{ ".zip",  PHAR_FORMAT_ZIP,  1 }` |
|     - | 2471 | `	};` |
|     - | 2472 | `	sxu32 k;` |
|   106 | 2473 | `	for( k = 0 ; k < SX_ARRAYSIZE(aExt) ; ++k ){` |
|   102 | 2474 | `		int nExt = (int)SyStrlen(aExt[k].zExt);` |
|     - | 2475 | `		int i;` |
|  4679 | 2476 | `		for( i = 0 ; i + nExt <= nPath ; ++i ){` |
|  4651 | 2477 | `			if( SyStrnicmp(&zPath[i],aExt[k].zExt,(sxu32)nExt) != 0 ){` |
|  4577 | 2478 | `				continue;` |
|     - | 2479 | `			}` |
|    74 | 2480 | `			if( aExt[k].bDataOnly && !bData ){` |
|     - | 2481 | ``				/* php lets a Phar carry a `.phar.tar` name; a bare `.tar` is a`` |
|     - | 2482 | `				 * data archive's. */` |
|   ! 0 | 2483 | `				continue;` |
|     - | 2484 | `			}` |
|    74 | 2485 | `			if( !aExt[k].bDataOnly && bData ){` |
|     - | 2486 | ``				/* ...and a PharData may not be a `.phar`. */`` |
|   ! 0 | 2487 | `				return 0;` |
|     - | 2488 | `			}` |
|    74 | 2489 | `			return aExt[k].iFmt;` |
|   ! 0 | 2490 | `		}` |
|    13 | 2491 | `	}` |
|     4 | 2492 | `	return 0;` |
|    36 | 2493 | `}` |
|     - | 2494 | `/* An archive with nothing in it, of the format the name asked for. */` |
|    31 | 2495 | `static phl_phar * PharNewEmpty(ph7_vm *pVm,const char *zPath,int nPath,int iFmt,int bData)` |
|   ! 0 | 2496 | `{` |
|    31 | 2497 | `	phl_phar *pPhar = (phl_phar *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_phar));` |
|    31 | 2498 | `	if( pPhar == 0 ){` |
|   ! 0 | 2499 | `		return 0;` |
|     - | 2500 | `	}` |
|    31 | 2501 | `	SyZero(pPhar,sizeof(*pPhar));` |
|    31 | 2502 | `	pPhar->pVm = pVm;` |
|    31 | 2503 | `	pPhar->bData = bData;` |
|    31 | 2504 | `	pPhar->iFormat = iFmt;` |
|    31 | 2505 | `	pPhar->nApi = PHAR_API_WORD;` |
|    31 | 2506 | `	SyBlobInit(&pPhar->sPath,&pVm->sAllocator);` |
|    31 | 2507 | `	SyBlobInit(&pPhar->sAlias,&pVm->sAllocator);` |
|    31 | 2508 | `	SyBlobInit(&pPhar->sStub,&pVm->sAllocator);` |
|    31 | 2509 | `	SyBlobInit(&pPhar->sMeta,&pVm->sAllocator);` |
|    31 | 2510 | `	SyBlobInit(&pPhar->sFile,&pVm->sAllocator);` |
|    31 | 2511 | `	SyBlobInit(&pPhar->sSig,&pVm->sAllocator);` |
|    31 | 2512 | `	SyBlobInit(&pPhar->sPrivKey,&pVm->sAllocator);` |
|    31 | 2513 | `	SyBlobAppend(&pPhar->sPath,zPath,(sxu32)nPath);` |
|    31 | 2514 | `	pPhar->pNext = (phl_phar *)pVm->pPhars;` |
|    31 | 2515 | `	pVm->pPhars = pPhar;` |
|    31 | 2516 | `	return pPhar;` |
|    14 | 2517 | `}` |
|     - | 2518 | `/*` |
|     - | 2519 | ` * Is this reason the signature one? Every algorithm's sentence carries the` |
|     - | 2520 | ` * same middle, and matching on it is what keeps the three shapes of php's` |
|     - | 2521 | ` * refusal apart without a second out-parameter.` |
|     - | 2522 | ` */` |
|     8 | 2523 | `static int PharSigSentence(const char *zErr)` |
|   ! 0 | 2524 | `{` |
|     8 | 2525 | `	const char *z = zErr;` |
|    84 | 2526 | `	while( *z ){` |
|    82 | 2527 | `		if( *z == 's' && SyStrncmp(z,"signature could not be verified",` |
|     7 | 2528 | `				sizeof("signature could not be verified") - 1) == 0 ){` |
|     6 | 2529 | `			return 1;` |
|     - | 2530 | `		}` |
|    76 | 2531 | `		++z;` |
|   ! 0 | 2532 | `	}` |
|     2 | 2533 | `	return 0;` |
|     4 | 2534 | `}` |
|     - | 2535 | `/* ---- Phar::__construct ---- */` |
|    60 | 2536 | `static int vm_builtin_Phar_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2537 | `{` |
|    60 | 2538 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 2539 | `	phl_phar *pPhar;` |
|     - | 2540 | `	const char *zPath,*zErr;` |
|    60 | 2541 | `	int nPath = 0,bData;` |
|    60 | 2542 | `	if( nArg < 1 \|\| pThis == 0 ){` |
|   ! 0 | 2543 | `		return PH7_OK;` |
|     - | 2544 | `	}` |
|     - | 2545 | `	{` |
|    60 | 2546 | `		ph7_class *pData = PH7_VmExtractClass(pCtx->pVm,"PharData",sizeof("PharData")-1,FALSE,0);` |
|    60 | 2547 | `		bData = pData != 0 && pThis->pClass != 0 && PH7_VmInstanceOf(pThis->pClass,pData);` |
|     - | 2548 | `	}` |
|    60 | 2549 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|    60 | 2550 | `	if( !bData && PharFormatFromName(zPath,nPath,0) == 0 ){` |
|     - | 2551 | `		/* php screens the NAME of an executable archive before it looks at the` |
|     - | 2552 | ``		 * file: a `.phar` somewhere in the last component is what makes a name`` |
|     - | 2553 | `		 * one at all. A PharData is not screened -- it opens whatever it is` |
|     - | 2554 | `		 * given and reports what the format reader found. */` |
|     6 | 2555 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 2556 | `			"Cannot create phar '%.*s', file extension (or combination) not recognised or the directory does not exist",` |
|     2 | 2557 | `			nPath,zPath);` |
|     - | 2558 | `	}` |
|    56 | 2559 | `	if( !PharPathIsFile(pCtx->pVm,zPath) && PharFindByPath(pCtx->pVm,zPath,nPath) == 0 ){` |
|     - | 2560 | `		/*` |
|     - | 2561 | `		 * A name with no file behind it is a NEW archive, and php decides three` |
|     - | 2562 | `		 * things about it before creating one: whether the extension names a` |
|     - | 2563 | `		 * format at all, whether an EXECUTABLE archive may be created (that is` |
|     - | 2564 | ``		 * what `phar.readonly` guards -- a PharData is exempt, which is why an`` |
|     - | 2565 | `		 * installer can build a .tar on a stock php), and which of the three` |
|     - | 2566 | `		 * formats the name asks for. Nothing is written until something is` |
|     - | 2567 | `		 * added: php's file does not appear at construction either.` |
|     - | 2568 | `		 */` |
|    32 | 2569 | `		int iFmt = PharFormatFromName(zPath,nPath,bData);` |
|    32 | 2570 | `		if( iFmt == 0 ){` |
|   ! 0 | 2571 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 2572 | `				"Cannot create phar '%.*s', file extension (or combination) not recognised or the directory does not exist",` |
|   ! 0 | 2573 | `				nPath,zPath);` |
|     - | 2574 | `		}` |
|    32 | 2575 | `		if( !bData && PharReadonly(pCtx->pVm) ){` |
|     1 | 2576 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 2577 | `				"creating archive \"%.*s\" disabled by the php.ini setting phar.readonly",` |
|   ! 0 | 2578 | `				nPath,zPath);` |
|     - | 2579 | `		}` |
|    31 | 2580 | `		pPhar = PharNewEmpty(pCtx->pVm,zPath,nPath,iFmt,bData);` |
|    31 | 2581 | `		if( pPhar == 0 ){` |
|   ! 0 | 2582 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 2583 | `				"Cannot create phar '%.*s', file extension (or combination) not recognised or the directory does not exist",` |
|   ! 0 | 2584 | `				nPath,zPath);` |
|     - | 2585 | `		}` |
|    31 | 2586 | `		PharAttach(pThis,pPhar);` |
|    31 | 2587 | `		PharSetInt(pThis,PHAR_SLOT_CUR,0);` |
|    31 | 2588 | `		PharSetSplPath(pCtx->pVm,pThis,zPath,nPath);` |
|    31 | 2589 | `		return PH7_OK;` |
|     - | 2590 | `	}` |
|    24 | 2591 | `	pPhar = PharOpenPath(pCtx->pVm,zPath,nPath,bData,&zErr);` |
|    24 | 2592 | `	if( pPhar == 0 ){` |
|     - | 2593 | `		/* php's own two sentences: one for a file it cannot read at all, one for` |
|     - | 2594 | `		 * a file whose contents are not an archive. */` |
|     8 | 2595 | `		if( zErr && SyStrncmp(zErr,"unable to open",sizeof("unable to open")-1) == 0 ){` |
|   ! 0 | 2596 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|   ! 0 | 2597 | `				"Cannot open phar file \"%.*s\": %s",nPath,zPath,zErr);` |
|     - | 2598 | `		}` |
|     8 | 2599 | `		if( zErr && PharSigSentence(zErr) ){` |
|     - | 2600 | `			/* php's third sentence, and the only one that names the archive` |
|     - | 2601 | ``			 * FIRST: `phar "…" SHA512 signature could not be verified: …`. */`` |
|     9 | 2602 | `			return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     3 | 2603 | `				"phar \"%.*s\" %s",nPath,zPath,zErr);` |
|     - | 2604 | `		}` |
|     2 | 2605 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     1 | 2606 | `			"internal corruption of phar \"%.*s\" (%s)",nPath,zPath,` |
|     2 | 2607 | `			zErr ? zErr : "truncated entry");` |
|     - | 2608 | `	}` |
|    16 | 2609 | `	if( bData ){` |
|     4 | 2610 | `		pPhar->bData = 1;` |
|     2 | 2611 | `	}` |
|    16 | 2612 | `	PharAttach(pThis,pPhar);` |
|    16 | 2613 | `	PharSetInt(pThis,PHAR_SLOT_CUR,0);` |
|     - | 2614 | `	/* The SplFileInfo half is the ARCHIVE itself. */` |
|    16 | 2615 | `	PharSetSplPath(pCtx->pVm,pThis,zPath,nPath);` |
|    16 | 2616 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|   ! 0 | 2617 | `		int nAlias = 0;` |
|   ! 0 | 2618 | `		const char *zAlias = ph7_value_to_string(apArg[2],&nAlias);` |
|   ! 0 | 2619 | `		SyBlobReset(&pPhar->sAlias);` |
|   ! 0 | 2620 | `		SyBlobAppend(&pPhar->sAlias,zAlias,(sxu32)nAlias);` |
|   ! 0 | 2621 | `	}` |
|    16 | 2622 | `	return PH7_OK;` |
|    28 | 2623 | `}` |
|     - | 2624 | `/* ---- the read verbs ---- */` |
|    13 | 2625 | `static int vm_builtin_Phar_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2626 | `{` |
|    13 | 2627 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     6 | 2628 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    13 | 2629 | `	ph7_result_int64(pCtx,pPhar ? (sxi64)pPhar->nEnt : 0);` |
|    13 | 2630 | `	return PH7_OK;` |
|   ! 0 | 2631 | `}` |
|   ! 0 | 2632 | `static int vm_builtin_Phar_getAlias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2633 | `{` |
|   ! 0 | 2634 | `	phl_phar *pPhar = PharThis(pCtx);` |
|   ! 0 | 2635 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2636 | `	if( pPhar == 0 \|\| SyBlobLength(&pPhar->sAlias) < 1 ){` |
|   ! 0 | 2637 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2638 | `		return PH7_OK;` |
|     - | 2639 | `	}` |
|   ! 0 | 2640 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pPhar->sAlias),` |
|   ! 0 | 2641 | `		(int)SyBlobLength(&pPhar->sAlias));` |
|   ! 0 | 2642 | `	return PH7_OK;` |
|   ! 0 | 2643 | `}` |
|   ! 0 | 2644 | `static int vm_builtin_Phar_getPath(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2645 | `{` |
|   ! 0 | 2646 | `	phl_phar *pPhar = PharThis(pCtx);` |
|   ! 0 | 2647 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2648 | `	if( pPhar == 0 ){` |
|   ! 0 | 2649 | `		return PH7_OK;` |
|     - | 2650 | `	}` |
|   ! 0 | 2651 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pPhar->sPath),` |
|   ! 0 | 2652 | `		(int)SyBlobLength(&pPhar->sPath));` |
|   ! 0 | 2653 | `	return PH7_OK;` |
|   ! 0 | 2654 | `}` |
|   ! 0 | 2655 | `static int vm_builtin_Phar_getVersion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2656 | `{` |
|   ! 0 | 2657 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     - | 2658 | `	char zBuf[16];` |
|   ! 0 | 2659 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2660 | `	if( pPhar == 0 ){` |
|   ! 0 | 2661 | `		return PH7_OK;` |
|     - | 2662 | `	}` |
|   ! 0 | 2663 | `	if( pPhar->iFormat != PHAR_FORMAT_PHAR ){` |
|   ! 0 | 2664 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 2665 | `		return PH7_OK;` |
|     - | 2666 | `	}` |
|   ! 0 | 2667 | `	SyBufferFormat(zBuf,sizeof(zBuf),"%d.%d.%d",` |
|   ! 0 | 2668 | `		(int)((pPhar->nApi >> 12) & 0xF),(int)((pPhar->nApi >> 8) & 0xF),` |
|   ! 0 | 2669 | `		(int)((pPhar->nApi >> 4) & 0xF));` |
|   ! 0 | 2670 | `	ph7_result_string(pCtx,zBuf,-1);` |
|   ! 0 | 2671 | `	return PH7_OK;` |
|   ! 0 | 2672 | `}` |
|    14 | 2673 | `static int vm_builtin_Phar_getSignature(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2674 | `{` |
|    14 | 2675 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     - | 2676 | `	ph7_value *pArray,*pVal;` |
|     - | 2677 | `	const char *zType;` |
|     - | 2678 | `	SyBlob sHex;` |
|     - | 2679 | `	sxu32 i;` |
|     7 | 2680 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    14 | 2681 | `	if( pPhar == 0 ){` |
|   ! 0 | 2682 | `		return PH7_OK;` |
|     - | 2683 | `	}` |
|    14 | 2684 | `	if( pPhar->iSigType == 0 \|\| SyBlobLength(&pPhar->sSig) < 1 ){` |
|   ! 0 | 2685 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2686 | `		return PH7_OK;` |
|     - | 2687 | `	}` |
|    14 | 2688 | `	switch( pPhar->iSigType ){` |
|   ! 0 | 2689 | `	case PHAR_SIG_MD5:    zType = "MD5";     break;` |
|   ! 0 | 2690 | `	case PHAR_SIG_SHA1:   zType = "SHA-1";   break;` |
|     4 | 2691 | `	case PHAR_SIG_SHA256: zType = "SHA-256"; break;` |
|     4 | 2692 | `	case PHAR_SIG_SHA512: zType = "SHA-512"; break;` |
|     2 | 2693 | `	case PHAR_SIG_OPENSSL:        zType = "OpenSSL";        break;` |
|     2 | 2694 | `	case PHAR_SIG_OPENSSL_SHA256: zType = "OpenSSL_SHA256"; break;` |
|     2 | 2695 | `	case PHAR_SIG_OPENSSL_SHA512: zType = "OpenSSL_SHA512"; break;` |
|   ! 0 | 2696 | `	default:              zType = "OpenSSL"; break;` |
|     - | 2697 | `	}` |
|    14 | 2698 | `	pArray = ph7_context_new_array(pCtx);` |
|    14 | 2699 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    14 | 2700 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 2701 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2702 | `		return PH7_OK;` |
|     - | 2703 | `	}` |
|     - | 2704 | `	/* php prints the digest in UPPER-case hex. */` |
|    14 | 2705 | `	SyBlobInit(&sHex,&pCtx->pVm->sAllocator);` |
|  1166 | 2706 | `	for( i = 0 ; i < SyBlobLength(&pPhar->sSig) ; ++i ){` |
|     - | 2707 | `		static const char zHex[] = "0123456789ABCDEF";` |
|  1152 | 2708 | `		unsigned char c = ((const unsigned char *)SyBlobData(&pPhar->sSig))[i];` |
|     - | 2709 | `		char zPair[2];` |
|  1152 | 2710 | `		zPair[0] = zHex[(c >> 4) & 0xF];` |
|  1152 | 2711 | `		zPair[1] = zHex[c & 0xF];` |
|  1152 | 2712 | `		SyBlobAppend(&sHex,zPair,2);` |
|   576 | 2713 | `	}` |
|    14 | 2714 | `	ph7_value_string(pVal,(const char *)SyBlobData(&sHex),(int)SyBlobLength(&sHex));` |
|    14 | 2715 | `	ph7_array_add_strkey_elem(pArray,"hash",pVal);` |
|    14 | 2716 | `	ph7_value_reset_string_cursor(pVal);` |
|    14 | 2717 | `	ph7_value_string(pVal,zType,-1);` |
|    14 | 2718 | `	ph7_array_add_strkey_elem(pArray,"hash_type",pVal);` |
|    14 | 2719 | `	ph7_result_value(pCtx,pArray);` |
|    14 | 2720 | `	ph7_context_release_value(pCtx,pVal);` |
|    14 | 2721 | `	SyBlobRelease(&sHex);` |
|    14 | 2722 | `	return PH7_OK;` |
|     7 | 2723 | `}` |
|     6 | 2724 | `static int vm_builtin_Phar_getStub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2725 | `{` |
|     6 | 2726 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     3 | 2727 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     6 | 2728 | `	if( pPhar == 0 ){` |
|   ! 0 | 2729 | `		return PH7_OK;` |
|     - | 2730 | `	}` |
|     9 | 2731 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pPhar->sStub),` |
|     6 | 2732 | `		(int)SyBlobLength(&pPhar->sStub));` |
|     6 | 2733 | `	return PH7_OK;` |
|     3 | 2734 | `}` |
|     - | 2735 | `/* php-serialized metadata, handed back as the VALUE it was. */` |
|     6 | 2736 | `static void PharMetaResult(ph7_context *pCtx,SyBlob *pMeta)` |
|   ! 0 | 2737 | `{` |
|     - | 2738 | `	ph7_value sVal;` |
|     6 | 2739 | `	int nRead = 0;` |
|     6 | 2740 | `	if( SyBlobLength(pMeta) < 1 ){` |
|     2 | 2741 | `		ph7_result_null(pCtx);` |
|     2 | 2742 | `		return;` |
|     - | 2743 | `	}` |
|     4 | 2744 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|     6 | 2745 | `	if( PH7_VmUnserializeOne(pCtx,(const char *)SyBlobData(pMeta),` |
|     6 | 2746 | `			(int)SyBlobLength(pMeta),&nRead,&sVal) != SXRET_OK ){` |
|   ! 0 | 2747 | `		PH7_MemObjRelease(&sVal);` |
|   ! 0 | 2748 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2749 | `		return;` |
|     - | 2750 | `	}` |
|     4 | 2751 | `	ph7_result_value(pCtx,&sVal);` |
|     4 | 2752 | `	PH7_MemObjRelease(&sVal);` |
|     3 | 2753 | `}` |
|     4 | 2754 | `static int vm_builtin_Phar_getMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2755 | `{` |
|     4 | 2756 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     2 | 2757 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     4 | 2758 | `	if( pPhar ){` |
|     4 | 2759 | `		PharMetaResult(pCtx,&pPhar->sMeta);` |
|     2 | 2760 | `	}` |
|     4 | 2761 | `	return PH7_OK;` |
|   ! 0 | 2762 | `}` |
|     4 | 2763 | `static int vm_builtin_Phar_hasMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2764 | `{` |
|     4 | 2765 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     2 | 2766 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     4 | 2767 | `	ph7_result_bool(pCtx,pPhar && SyBlobLength(&pPhar->sMeta) > 0);` |
|     4 | 2768 | `	return PH7_OK;` |
|   ! 0 | 2769 | `}` |
|    16 | 2770 | `static int vm_builtin_Phar_isFileFormat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2771 | `{` |
|    16 | 2772 | `	phl_phar *pPhar = PharThis(pCtx);` |
|    16 | 2773 | `	sxi64 iFmt = nArg > 0 ? ph7_value_to_int64(apArg[0]) : -1;` |
|    16 | 2774 | `	if( pPhar == 0 ){` |
|   ! 0 | 2775 | `		return PH7_OK;` |
|     - | 2776 | `	}` |
|    16 | 2777 | `	if( iFmt != 1 && iFmt != 2 && iFmt != 3 ){` |
|   ! 0 | 2778 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2779 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException","Unknown file format specified");` |
|     - | 2780 | `	}` |
|    16 | 2781 | `	ph7_result_bool(pCtx,(int)iFmt == pPhar->iFormat);` |
|    16 | 2782 | `	return PH7_OK;` |
|     8 | 2783 | `}` |
|   ! 0 | 2784 | `static int vm_builtin_Phar_isCompressed(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2785 | `{` |
|   ! 0 | 2786 | `	phl_phar *pPhar = PharThis(pCtx);` |
|   ! 0 | 2787 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2788 | `	SXUNUSED(pPhar);` |
|     - | 2789 | ``	/* A whole-archive compression is a `.phar.gz`, which this build opens by`` |
|     - | 2790 | `	 * decompressing it: the OPEN archive is never itself compressed. */` |
|   ! 0 | 2791 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 2792 | `	return PH7_OK;` |
|   ! 0 | 2793 | `}` |
|   ! 0 | 2794 | `static int vm_builtin_Phar_isWritable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2795 | `{` |
|   ! 0 | 2796 | `	phl_phar *pPhar = PharThis(pCtx);` |
|   ! 0 | 2797 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2798 | `	ph7_result_bool(pCtx,pPhar != 0 && (pPhar->bData \|\| !PharReadonly(pCtx->pVm)));` |
|   ! 0 | 2799 | `	return PH7_OK;` |
|   ! 0 | 2800 | `}` |
|   ! 0 | 2801 | `static int vm_builtin_Phar_false(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2802 | `{` |
|   ! 0 | 2803 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2804 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 2805 | `	return PH7_OK;` |
|   ! 0 | 2806 | `}` |
|    78 | 2807 | `static int vm_builtin_Phar_void(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2808 | `{` |
|    37 | 2809 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    78 | 2810 | `	ph7_result_null(pCtx);` |
|    78 | 2811 | `	return PH7_OK;` |
|   ! 0 | 2812 | `}` |
|   ! 0 | 2813 | `static int vm_builtin_Phar_true(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2814 | `{` |
|   ! 0 | 2815 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2816 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 | 2817 | `	return PH7_OK;` |
|   ! 0 | 2818 | `}` |
|     - | 2819 | `/* ---- ArrayAccess ---- */` |
|     8 | 2820 | `static int vm_builtin_Phar_offsetExists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2821 | `{` |
|     8 | 2822 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     - | 2823 | `	const char *zName;` |
|     8 | 2824 | `	int nName = 0;` |
|     8 | 2825 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 2826 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2827 | `		return PH7_OK;` |
|     - | 2828 | `	}` |
|     8 | 2829 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     8 | 2830 | `	ph7_result_bool(pCtx,PharFindEnt(pPhar,zName,nName) != 0);` |
|     8 | 2831 | `	return PH7_OK;` |
|     4 | 2832 | `}` |
|    18 | 2833 | `static int vm_builtin_Phar_offsetGet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2834 | `{` |
|    18 | 2835 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     - | 2836 | `	phl_phar_ent *pEnt;` |
|     - | 2837 | `	ph7_class_instance *pInfo;` |
|     - | 2838 | `	const char *zName;` |
|    18 | 2839 | `	int nName = 0;` |
|    18 | 2840 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 2841 | `		return PH7_OK;` |
|     - | 2842 | `	}` |
|    18 | 2843 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    18 | 2844 | `	pEnt = PharFindEnt(pPhar,zName,nName);` |
|    18 | 2845 | `	if( pEnt == 0 ){` |
|     3 | 2846 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     1 | 2847 | `			"Entry %.*s does not exist",nName,zName);` |
|     - | 2848 | `	}` |
|    16 | 2849 | `	pInfo = PharNewFileInfo(pCtx,pPhar,pEnt);` |
|    16 | 2850 | `	if( pInfo == 0 ){` |
|   ! 0 | 2851 | `		return PH7_OK;` |
|     - | 2852 | `	}` |
|    16 | 2853 | `	PH7_NativeResultObject(pCtx,pInfo);` |
|    16 | 2854 | `	return PH7_OK;` |
|     9 | 2855 | `}` |
|     2 | 2856 | `static int vm_builtin_Phar_offsetSet(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2857 | `{` |
|     - | 2858 | `	phl_phar *pPhar;` |
|     - | 2859 | `	phl_phar_ent *pEnt;` |
|     - | 2860 | `	const char *zName,*zData;` |
|     2 | 2861 | `	int nName = 0,nData = 0;` |
|     2 | 2862 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 2863 | `		return PH7_OK;` |
|     - | 2864 | `	}` |
|     2 | 2865 | `	pPhar = PharThis(pCtx);` |
|     2 | 2866 | `	if( pPhar == 0 \|\| nArg < 2 ){` |
|   ! 0 | 2867 | `		return PH7_OK;` |
|     - | 2868 | `	}` |
|     2 | 2869 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|     2 | 2870 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|     2 | 2871 | `	pEnt = PharFindEnt(pPhar,zName,nName);` |
|     2 | 2872 | `	if( pEnt == 0 ){` |
|     2 | 2873 | `		pEnt = PharNewEnt(pPhar,zName,nName);` |
|     2 | 2874 | `		if( pEnt == 0 ){` |
|   ! 0 | 2875 | `			return PH7_OK;` |
|     - | 2876 | `		}` |
|     2 | 2877 | `		pEnt->nFlags = 0666;` |
|     1 | 2878 | `	}` |
|     2 | 2879 | `	SyBlobReset(&pEnt->sData);` |
|     2 | 2880 | `	SyBlobAppend(&pEnt->sData,zData,(sxu32)nData);` |
|     2 | 2881 | `	pEnt->bLoaded = 1;` |
|     2 | 2882 | `	pEnt->bDir = 0;` |
|     2 | 2883 | `	pEnt->nSize = (sxu32)nData;` |
|     2 | 2884 | `	pEnt->iTime = (sxi64)time(0);` |
|     2 | 2885 | `	PharCommit(pPhar);` |
|     2 | 2886 | `	return PH7_OK;` |
|     1 | 2887 | `}` |
|   ! 0 | 2888 | `static int vm_builtin_Phar_offsetUnset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2889 | `{` |
|     - | 2890 | `	phl_phar *pPhar;` |
|   ! 0 | 2891 | `	phl_phar_ent *pEnt,*pPrev = 0;` |
|     - | 2892 | `	const char *zName;` |
|   ! 0 | 2893 | `	int nName = 0;` |
|   ! 0 | 2894 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 2895 | `		return PH7_OK;` |
|     - | 2896 | `	}` |
|   ! 0 | 2897 | `	pPhar = PharThis(pCtx);` |
|   ! 0 | 2898 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 2899 | `		return PH7_OK;` |
|     - | 2900 | `	}` |
|   ! 0 | 2901 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   ! 0 | 2902 | `	for( pEnt = pPhar->pFirst ; pEnt ; pPrev = pEnt, pEnt = pEnt->pNext ){` |
|   ! 0 | 2903 | `		if( (int)SyBlobLength(&pEnt->sName) == nName` |
|   ! 0 | 2904 | `		 && SyMemcmp(SyBlobData(&pEnt->sName),zName,(sxu32)nName) == 0 ){` |
|   ! 0 | 2905 | `			if( pPrev ){` |
|   ! 0 | 2906 | `				pPrev->pNext = pEnt->pNext;` |
|   ! 0 | 2907 | `			}else{` |
|   ! 0 | 2908 | `				pPhar->pFirst = pEnt->pNext;` |
|     - | 2909 | `			}` |
|   ! 0 | 2910 | `			if( pPhar->pLast == pEnt ){` |
|   ! 0 | 2911 | `				pPhar->pLast = pPrev;` |
|   ! 0 | 2912 | `			}` |
|   ! 0 | 2913 | `			pPhar->nEnt--;` |
|   ! 0 | 2914 | `			PharEntFree(pPhar->pVm,pEnt);` |
|   ! 0 | 2915 | `			PharCommit(pPhar);` |
|   ! 0 | 2916 | `			break;` |
|     - | 2917 | `		}` |
|   ! 0 | 2918 | `	}` |
|   ! 0 | 2919 | `	return PH7_OK;` |
|   ! 0 | 2920 | `}` |
|     - | 2921 | `/* ---- the iterator half ---- */` |
|     8 | 2922 | `static int vm_builtin_Phar_rewind(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2923 | `{` |
|     8 | 2924 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     4 | 2925 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     8 | 2926 | `	if( pThis ){` |
|     8 | 2927 | `		PharSetInt(pThis,PHAR_SLOT_CUR,0);` |
|     - | 2928 | `		/* php rebuilds the SplFileInfo name from the iterator here, with an` |
|     - | 2929 | ``		 * empty directory part -- which is why `getPathname()` on a rewound`` |
|     - | 2930 | `		 * Phar is the bare entry name. */` |
|     8 | 2931 | `		PharSetStr(pThis,"__p","",0);` |
|     4 | 2932 | `	}` |
|     8 | 2933 | `	ph7_result_null(pCtx);` |
|     8 | 2934 | `	return PH7_OK;` |
|   ! 0 | 2935 | `}` |
|    30 | 2936 | `static int vm_builtin_Phar_valid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2937 | `{` |
|    30 | 2938 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    30 | 2939 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 2940 | `	SyBlob sName;` |
|    30 | 2941 | `	int bOk = 0;` |
|    15 | 2942 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    30 | 2943 | `	if( pPhar ){` |
|    30 | 2944 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    30 | 2945 | `		bOk = PharCursorChild(pThis,pPhar,&sName,0);` |
|    30 | 2946 | `		SyBlobRelease(&sName);` |
|    15 | 2947 | `	}` |
|    30 | 2948 | `	ph7_result_bool(pCtx,bOk);` |
|    30 | 2949 | `	return PH7_OK;` |
|   ! 0 | 2950 | `}` |
|    14 | 2951 | `static int vm_builtin_Phar_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2952 | `{` |
|    14 | 2953 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 | 2954 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    14 | 2955 | `	if( pThis ){` |
|    14 | 2956 | `		PharSetInt(pThis,PHAR_SLOT_CUR,PH7_NativeAttrInt(pThis,PHAR_SLOT_CUR) + 1);` |
|    14 | 2957 | `		PharSetStr(pThis,"__p","",0);` |
|     7 | 2958 | `	}` |
|    14 | 2959 | `	ph7_result_null(pCtx);` |
|    14 | 2960 | `	return PH7_OK;` |
|   ! 0 | 2961 | `}` |
|     4 | 2962 | `static int vm_builtin_Phar_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2963 | `{` |
|     4 | 2964 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     4 | 2965 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 2966 | `	SyBlob sUrl,sName;` |
|     2 | 2967 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     4 | 2968 | `	if( pPhar == 0 ){` |
|   ! 0 | 2969 | `		return PH7_OK;` |
|     - | 2970 | `	}` |
|     4 | 2971 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|     4 | 2972 | `	if( !PharCursorChild(pThis,pPhar,&sName,0) ){` |
|   ! 0 | 2973 | `		SyBlobRelease(&sName);` |
|   ! 0 | 2974 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2975 | `		return PH7_OK;` |
|     - | 2976 | `	}` |
|     - | 2977 | ``	/* php's key is the whole `phar://` url of the entry. */`` |
|     4 | 2978 | `	SyBlobInit(&sUrl,&pCtx->pVm->sAllocator);` |
|     4 | 2979 | `	PharUrl(pPhar,(const char *)SyBlobData(&sName),(int)SyBlobLength(&sName),&sUrl);` |
|     4 | 2980 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|     4 | 2981 | `	SyBlobRelease(&sUrl);` |
|     4 | 2982 | `	SyBlobRelease(&sName);` |
|     4 | 2983 | `	return PH7_OK;` |
|     2 | 2984 | `}` |
|     - | 2985 | `/*` |
|     - | 2986 | ` * A PharFileInfo for the child the cursor is on. A DIRECTORY child gets one` |
|     - | 2987 | `` * too -- php answers `isDir()` from it -- even when the manifest has no entry`` |
|     - | 2988 | `` * of that name, which is the ordinary case for `src` in an archive that only`` |
|     - | 2989 | ` * lists files under it.` |
|     - | 2990 | ` */` |
|    10 | 2991 | `static ph7_class_instance * PharChildInfo(ph7_context *pCtx,phl_phar *pPhar,` |
|     - | 2992 | `	const char *zName,int nName,int bDir)` |
|   ! 0 | 2993 | `{` |
|    10 | 2994 | `	phl_phar_ent *pEnt = PharFindEnt(pPhar,zName,nName);` |
|    10 | 2995 | `	if( pEnt ){` |
|     8 | 2996 | `		return PharNewFileInfo(pCtx,pPhar,pEnt);` |
|     - | 2997 | `	}` |
|     2 | 2998 | `	if( bDir ){` |
|     - | 2999 | `		/* A directory the names IMPLY. Its info object carries the name and the` |
|     - | 3000 | `		 * archive, and every question about it is answered from those. */` |
|     2 | 3001 | `		ph7_vm *pVm = pCtx->pVm;` |
|     2 | 3002 | `		ph7_class *pClass = PH7_VmExtractClass(pVm,"PharFileInfo",sizeof("PharFileInfo")-1,FALSE,0);` |
|     2 | 3003 | `		ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|     - | 3004 | `		SyBlob sUrl;` |
|     2 | 3005 | `		if( pThis == 0 ){` |
|   ! 0 | 3006 | `			return 0;` |
|     - | 3007 | `		}` |
|     2 | 3008 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|     2 | 3009 | `		PharUrl(pPhar,zName,nName,&sUrl);` |
|     2 | 3010 | `		PharSetSplPath(pVm,pThis,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|     2 | 3011 | `		PharSetStr(pThis,PHAR_SLOT_ENT,zName,nName);` |
|     3 | 3012 | `		PharSetStr(pThis,PHAR_SLOT_PHAR,(const char *)SyBlobData(&pPhar->sPath),` |
|     2 | 3013 | `			(int)SyBlobLength(&pPhar->sPath));` |
|     2 | 3014 | `		PharAttach(pThis,pPhar);` |
|     2 | 3015 | `		SyBlobRelease(&sUrl);` |
|     2 | 3016 | `		return pThis;` |
|     - | 3017 | `	}` |
|   ! 0 | 3018 | `	return 0;` |
|     5 | 3019 | `}` |
|    10 | 3020 | `static int vm_builtin_Phar_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3021 | `{` |
|    10 | 3022 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    10 | 3023 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 3024 | `	ph7_class_instance *pInfo;` |
|     - | 3025 | `	SyBlob sName;` |
|    10 | 3026 | `	int bDir = 0;` |
|     5 | 3027 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    10 | 3028 | `	if( pPhar == 0 ){` |
|   ! 0 | 3029 | `		return PH7_OK;` |
|     - | 3030 | `	}` |
|    10 | 3031 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    10 | 3032 | `	if( !PharCursorChild(pThis,pPhar,&sName,&bDir) ){` |
|   ! 0 | 3033 | `		SyBlobRelease(&sName);` |
|   ! 0 | 3034 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3035 | `		return PH7_OK;` |
|     - | 3036 | `	}` |
|    15 | 3037 | `	pInfo = PharChildInfo(pCtx,pPhar,(const char *)SyBlobData(&sName),` |
|    10 | 3038 | `		(int)SyBlobLength(&sName),bDir);` |
|    10 | 3039 | `	SyBlobRelease(&sName);` |
|    10 | 3040 | `	if( pInfo ){` |
|    10 | 3041 | `		PH7_NativeResultObject(pCtx,pInfo);` |
|     5 | 3042 | `	}` |
|    10 | 3043 | `	return PH7_OK;` |
|     5 | 3044 | `}` |
|     - | 3045 | `/* The RECURSIVE half: a directory child has children, and descending into one` |
|     - | 3046 | ` * answers another Phar -- php hands back the same class, positioned on the` |
|     - | 3047 | ` * subdirectory. */` |
|    10 | 3048 | `static int vm_builtin_Phar_hasChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3049 | `{` |
|    10 | 3050 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    10 | 3051 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 3052 | `	SyBlob sName;` |
|    10 | 3053 | `	int bDir = 0;` |
|     5 | 3054 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    10 | 3055 | `	if( pPhar ){` |
|    10 | 3056 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    10 | 3057 | `		if( !PharCursorChild(pThis,pPhar,&sName,&bDir) ){` |
|   ! 0 | 3058 | `			bDir = 0;` |
|   ! 0 | 3059 | `		}` |
|    10 | 3060 | `		SyBlobRelease(&sName);` |
|     5 | 3061 | `	}` |
|    10 | 3062 | `	ph7_result_bool(pCtx,bDir);` |
|    10 | 3063 | `	return PH7_OK;` |
|   ! 0 | 3064 | `}` |
|     4 | 3065 | `static int vm_builtin_Phar_getChildren(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3066 | `{` |
|     4 | 3067 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     4 | 3068 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 3069 | `	ph7_class_instance *pKid;` |
|     - | 3070 | `	SyBlob sName;` |
|     4 | 3071 | `	int bDir = 0;` |
|     2 | 3072 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     4 | 3073 | `	if( pPhar == 0 ){` |
|   ! 0 | 3074 | `		return PH7_OK;` |
|     - | 3075 | `	}` |
|     4 | 3076 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|     4 | 3077 | `	if( !PharCursorChild(pThis,pPhar,&sName,&bDir) \|\| !bDir ){` |
|   ! 0 | 3078 | `		SyBlobRelease(&sName);` |
|   ! 0 | 3079 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3080 | `		return PH7_OK;` |
|     - | 3081 | `	}` |
|     4 | 3082 | `	pKid = pThis->pClass ? PH7_NewClassInstance(pCtx->pVm,pThis->pClass) : 0;` |
|     4 | 3083 | `	if( pKid ){` |
|     4 | 3084 | `		PharAttach(pKid,pPhar);` |
|     4 | 3085 | `		PharSetInt(pKid,PHAR_SLOT_CUR,0);` |
|     6 | 3086 | `		PharSetStr(pKid,PHAR_SLOT_DIR,(const char *)SyBlobData(&sName),` |
|     4 | 3087 | `			(int)SyBlobLength(&sName));` |
|     6 | 3088 | `		PharSetSplPath(pCtx->pVm,pKid,(const char *)SyBlobData(&pPhar->sPath),` |
|     4 | 3089 | `			(int)SyBlobLength(&pPhar->sPath));` |
|     4 | 3090 | `		PH7_NativeResultObject(pCtx,pKid);` |
|     2 | 3091 | `	}` |
|     4 | 3092 | `	SyBlobRelease(&sName);` |
|     4 | 3093 | `	return PH7_OK;` |
|     2 | 3094 | `}` |
|     - | 3095 | `/*` |
|     - | 3096 | `` * `getFilename()` on a Phar is the ITERATOR's name, not the archive's, and`` |
|     - | 3097 | `` * `getPathname()` is php's own quirk: the archive's path and the entry while`` |
|     - | 3098 | ` * the object is fresh, and the bare entry once the iterator has been rewound` |
|     - | 3099 | ` * (php rebuilds the slot from an empty directory part there). Both are what a` |
|     - | 3100 | `` * program printing `$phar` sees.`` |
|     - | 3101 | ` */` |
|   ! 0 | 3102 | `static int vm_builtin_Phar_getFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3103 | `{` |
|   ! 0 | 3104 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   ! 0 | 3105 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 3106 | `	SyBlob sName;` |
|     - | 3107 | `	const char *z;` |
|     - | 3108 | `	int n,i;` |
|   ! 0 | 3109 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 3110 | `	if( pPhar == 0 ){` |
|   ! 0 | 3111 | `		return PH7_OK;` |
|     - | 3112 | `	}` |
|   ! 0 | 3113 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|   ! 0 | 3114 | `	if( !PharCursorChild(pThis,pPhar,&sName,0) ){` |
|   ! 0 | 3115 | `		SyBlobRelease(&sName);` |
|   ! 0 | 3116 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 3117 | `		return PH7_OK;` |
|     - | 3118 | `	}` |
|   ! 0 | 3119 | `	z = (const char *)SyBlobData(&sName);` |
|   ! 0 | 3120 | `	n = (int)SyBlobLength(&sName);` |
|   ! 0 | 3121 | `	for( i = n ; i > 0 ; --i ){` |
|   ! 0 | 3122 | `		if( z[i-1] == '/' ){` |
|   ! 0 | 3123 | `			break;` |
|     - | 3124 | `		}` |
|   ! 0 | 3125 | `	}` |
|   ! 0 | 3126 | `	ph7_result_string(pCtx,&z[i],n - i);` |
|   ! 0 | 3127 | `	SyBlobRelease(&sName);` |
|   ! 0 | 3128 | `	return PH7_OK;` |
|   ! 0 | 3129 | `}` |
|   ! 0 | 3130 | `static int vm_builtin_Phar_getPathname(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3131 | `{` |
|   ! 0 | 3132 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   ! 0 | 3133 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 3134 | `	ph7_value *pPath;` |
|     - | 3135 | `	SyBlob sName,sOut;` |
|   ! 0 | 3136 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 3137 | `	if( pPhar == 0 ){` |
|   ! 0 | 3138 | `		return PH7_OK;` |
|     - | 3139 | `	}` |
|   ! 0 | 3140 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|   ! 0 | 3141 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   ! 0 | 3142 | `	pPath = PH7_NativeAttr(pThis,"__p");` |
|   ! 0 | 3143 | `	if( pPath && (pPath->iFlags & MEMOBJ_STRING) && SyBlobLength(&pPath->sBlob) > 0 ){` |
|   ! 0 | 3144 | `		SyBlobAppend(&sOut,SyBlobData(&pPath->sBlob),SyBlobLength(&pPath->sBlob));` |
|   ! 0 | 3145 | `		SyBlobAppend(&sOut,"/",1);` |
|   ! 0 | 3146 | `	}` |
|   ! 0 | 3147 | `	if( PharCursorChild(pThis,pPhar,&sName,0) ){` |
|   ! 0 | 3148 | `		const char *z = (const char *)SyBlobData(&sName);` |
|   ! 0 | 3149 | `		int n = (int)SyBlobLength(&sName),i;` |
|   ! 0 | 3150 | `		for( i = n ; i > 0 ; --i ){` |
|   ! 0 | 3151 | `			if( z[i-1] == '/' ){` |
|   ! 0 | 3152 | `				break;` |
|     - | 3153 | `			}` |
|   ! 0 | 3154 | `		}` |
|   ! 0 | 3155 | `		SyBlobAppend(&sOut,&z[i],(sxu32)(n - i));` |
|   ! 0 | 3156 | `	}` |
|   ! 0 | 3157 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   ! 0 | 3158 | `	SyBlobRelease(&sName);` |
|   ! 0 | 3159 | `	SyBlobRelease(&sOut);` |
|   ! 0 | 3160 | `	return PH7_OK;` |
|   ! 0 | 3161 | `}` |
|     - | 3162 | `/* ---- extraction ---- */` |
|     - | 3163 | `/*` |
|     - | 3164 | `` * php's `extractTo()`: write the archive's entries out as real files. It is the`` |
|     - | 3165 | ` * one READ verb that touches the filesystem, and the one Composer's tar` |
|     - | 3166 | ` * downloader calls.` |
|     - | 3167 | ` */` |
|     8 | 3168 | `static int PharExtractOne(ph7_context *pCtx,phl_phar *pPhar,phl_phar_ent *pEnt,` |
|     - | 3169 | `	const char *zDir,int nDir,int bOverwrite)` |
|   ! 0 | 3170 | `{` |
|     8 | 3171 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 3172 | `	const ph7_io_stream *pStream;` |
|     - | 3173 | `	SyBlob sPath;` |
|     - | 3174 | `	const char *zPath;` |
|     - | 3175 | `	void *pHandle;` |
|     8 | 3176 | `	int rc = 0;` |
|     8 | 3177 | `	if( PharEntLoad(pPhar,pEnt) != 0 ){` |
|   ! 0 | 3178 | `		return -1;` |
|     - | 3179 | `	}` |
|     8 | 3180 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|     8 | 3181 | `	SyBlobAppend(&sPath,zDir,(sxu32)nDir);` |
|     8 | 3182 | `	SyBlobAppend(&sPath,"/",1);` |
|     8 | 3183 | `	SyBlobAppend(&sPath,SyBlobData(&pEnt->sName),SyBlobLength(&pEnt->sName));` |
|     8 | 3184 | `	SyBlobNullAppend(&sPath);` |
|     8 | 3185 | `	zPath = (const char *)SyBlobData(&sPath);` |
|     - | 3186 | `	/* Every directory on the way, as php makes them. */` |
|     - | 3187 | `	{` |
|     8 | 3188 | `		const ph7_vfs *pVfs = pVm->pEngine ? pVm->pEngine->pVfs : 0;` |
|     - | 3189 | `		sxu32 i;` |
|    66 | 3190 | `		for( i = (sxu32)nDir + 1 ; i < SyBlobLength(&sPath) ; ++i ){` |
|    58 | 3191 | `			if( zPath[i] != '/' ){` |
|    52 | 3192 | `				continue;` |
|     - | 3193 | `			}` |
|     6 | 3194 | `			((char *)zPath)[i] = 0;` |
|     6 | 3195 | `			if( pVfs && pVfs->xMkdir ){` |
|     6 | 3196 | `				pVfs->xMkdir(zPath,0777,FALSE);` |
|     3 | 3197 | `			}` |
|     6 | 3198 | `			((char *)zPath)[i] = '/';` |
|     3 | 3199 | `		}` |
|     8 | 3200 | `		if( pEnt->bDir ){` |
|   ! 0 | 3201 | `			if( pVfs && pVfs->xMkdir ){` |
|   ! 0 | 3202 | `				pVfs->xMkdir(zPath,0777,FALSE);` |
|   ! 0 | 3203 | `			}` |
|   ! 0 | 3204 | `			SyBlobRelease(&sPath);` |
|   ! 0 | 3205 | `			return 0;` |
|     - | 3206 | `		}` |
|     8 | 3207 | `		if( !bOverwrite && pVfs && pVfs->xFileExists && pVfs->xFileExists(zPath) == PH7_OK ){` |
|   ! 0 | 3208 | `			SyBlobRelease(&sPath);` |
|   ! 0 | 3209 | `			return 1;   /* php refuses to clobber unless asked */` |
|     - | 3210 | `		}` |
|     - | 3211 | `	}` |
|     8 | 3212 | `	pStream = PH7_VmGetStreamDevice(pVm,&zPath,(int)SyBlobLength(&sPath));` |
|     8 | 3213 | `	pHandle = pStream ? PH7_StreamOpenHandle(pVm,pStream,zPath,` |
|     4 | 3214 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,0) : 0;` |
|     8 | 3215 | `	if( pHandle == 0 ){` |
|   ! 0 | 3216 | `		rc = -1;` |
|   ! 0 | 3217 | `	}else{` |
|     8 | 3218 | `		if( pStream->xWrite && SyBlobLength(&pEnt->sData) > 0 ){` |
|    12 | 3219 | `			pStream->xWrite(pHandle,SyBlobData(&pEnt->sData),` |
|     8 | 3220 | `				(ph7_int64)SyBlobLength(&pEnt->sData));` |
|     4 | 3221 | `		}` |
|     8 | 3222 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|     - | 3223 | `	}` |
|     8 | 3224 | `	SyBlobRelease(&sPath);` |
|     8 | 3225 | `	return rc;` |
|     4 | 3226 | `}` |
|     2 | 3227 | `static int vm_builtin_Phar_extractTo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3228 | `{` |
|     2 | 3229 | `	phl_phar *pPhar = PharThis(pCtx);` |
|     - | 3230 | `	phl_phar_ent *pEnt;` |
|     - | 3231 | `	const char *zDir;` |
|     2 | 3232 | `	int nDir = 0,bOverwrite = 0,bAll = 1;` |
|     2 | 3233 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3234 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3235 | `		return PH7_OK;` |
|     - | 3236 | `	}` |
|     2 | 3237 | `	zDir = ph7_value_to_string(apArg[0],&nDir);` |
|     2 | 3238 | `	while( nDir > 1 && zDir[nDir-1] == '/' ){` |
|   ! 0 | 3239 | `		nDir--;` |
|   ! 0 | 3240 | `	}` |
|     2 | 3241 | `	if( nArg > 2 ){` |
|     2 | 3242 | `		bOverwrite = ph7_value_to_bool(apArg[2]);` |
|     1 | 3243 | `	}` |
|     2 | 3244 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|   ! 0 | 3245 | `		bAll = 0;` |
|   ! 0 | 3246 | `	}` |
|     - | 3247 | `	{` |
|     2 | 3248 | `		const ph7_vfs *pVfs = pCtx->pVm->pEngine ? pCtx->pVm->pEngine->pVfs : 0;` |
|     - | 3249 | `		SyBlob sDir;` |
|     2 | 3250 | `		SyBlobInit(&sDir,&pCtx->pVm->sAllocator);` |
|     2 | 3251 | `		SyBlobAppend(&sDir,zDir,(sxu32)nDir);` |
|     2 | 3252 | `		SyBlobNullAppend(&sDir);` |
|     2 | 3253 | `		if( pVfs && pVfs->xMkdir ){` |
|     2 | 3254 | `			pVfs->xMkdir((const char *)SyBlobData(&sDir),0777,TRUE);` |
|     1 | 3255 | `		}` |
|     2 | 3256 | `		SyBlobRelease(&sDir);` |
|     - | 3257 | `	}` |
|    10 | 3258 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|     8 | 3259 | `		if( !bAll ){` |
|     - | 3260 | `			/* php takes one name or a list of them. */` |
|   ! 0 | 3261 | `			int bWanted = 0;` |
|   ! 0 | 3262 | `			if( ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 3263 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     - | 3264 | `				ph7_hashmap_node *pNode;` |
|   ! 0 | 3265 | `				pMap->pCur = pMap->pFirst;` |
|   ! 0 | 3266 | `				while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|     - | 3267 | `					ph7_value sVal;` |
|     - | 3268 | `					const char *zWant;` |
|   ! 0 | 3269 | `					int nWant = 0;` |
|   ! 0 | 3270 | `					PH7_MemObjInit(pCtx->pVm,&sVal);` |
|   ! 0 | 3271 | `					PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|   ! 0 | 3272 | `					zWant = ph7_value_to_string(&sVal,&nWant);` |
|   ! 0 | 3273 | `					if( (int)SyBlobLength(&pEnt->sName) == nWant` |
|   ! 0 | 3274 | `					 && SyMemcmp(SyBlobData(&pEnt->sName),zWant,(sxu32)nWant) == 0 ){` |
|   ! 0 | 3275 | `						bWanted = 1;` |
|   ! 0 | 3276 | `					}` |
|   ! 0 | 3277 | `					PH7_MemObjRelease(&sVal);` |
|   ! 0 | 3278 | `				}` |
|   ! 0 | 3279 | `			}else{` |
|   ! 0 | 3280 | `				int nWant = 0;` |
|   ! 0 | 3281 | `				const char *zWant = ph7_value_to_string(apArg[1],&nWant);` |
|   ! 0 | 3282 | `				bWanted = (int)SyBlobLength(&pEnt->sName) == nWant` |
|   ! 0 | 3283 | `					&& SyMemcmp(SyBlobData(&pEnt->sName),zWant,(sxu32)nWant) == 0;` |
|     - | 3284 | `			}` |
|   ! 0 | 3285 | `			if( !bWanted ){` |
|   ! 0 | 3286 | `				continue;` |
|     - | 3287 | `			}` |
|   ! 0 | 3288 | `		}` |
|     8 | 3289 | `		if( PharExtractOne(pCtx,pPhar,pEnt,zDir,nDir,bOverwrite) < 0 ){` |
|   ! 0 | 3290 | `			return PH7_VmThrowException(pCtx,"PharException",` |
|     - | 3291 | `				"Extraction from phar \"%.*s\" failed: Cannot extract \"%.*s\"",` |
|   ! 0 | 3292 | `				(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath),` |
|   ! 0 | 3293 | `				(int)SyBlobLength(&pEnt->sName),(const char *)SyBlobData(&pEnt->sName));` |
|     - | 3294 | `		}` |
|     4 | 3295 | `	}` |
|     2 | 3296 | `	ph7_result_bool(pCtx,1);` |
|     2 | 3297 | `	return PH7_OK;` |
|     1 | 3298 | `}` |
|     - | 3299 | `/* ---- the write verbs ---- */` |
|    59 | 3300 | `static int PharAddBytes(ph7_context *pCtx,phl_phar *pPhar,const char *zName,int nName,` |
|     - | 3301 | `	const char *zData,int nData,int bDir)` |
|   ! 0 | 3302 | `{` |
|    59 | 3303 | `	phl_phar_ent *pEnt = PharFindEnt(pPhar,zName,nName);` |
|    59 | 3304 | `	if( pEnt == 0 ){` |
|    59 | 3305 | `		pEnt = PharNewEnt(pPhar,zName,nName);` |
|    59 | 3306 | `		if( pEnt == 0 ){` |
|   ! 0 | 3307 | `			return -1;` |
|     - | 3308 | `		}` |
|    59 | 3309 | `		pEnt->nFlags = bDir ? 0777 : 0666;` |
|    27 | 3310 | `	}` |
|    59 | 3311 | `	SyBlobReset(&pEnt->sData);` |
|    59 | 3312 | `	if( nData > 0 ){` |
|    57 | 3313 | `		SyBlobAppend(&pEnt->sData,zData,(sxu32)nData);` |
|    26 | 3314 | `	}` |
|    59 | 3315 | `	pEnt->bLoaded = 1;` |
|    59 | 3316 | `	pEnt->bDir = (sxu8)bDir;` |
|    59 | 3317 | `	pEnt->nSize = (sxu32)(nData > 0 ? nData : 0);` |
|    59 | 3318 | `	pEnt->iTime = (sxi64)time(0);` |
|    27 | 3319 | `	SXUNUSED(pCtx);` |
|    59 | 3320 | `	return 0;` |
|    27 | 3321 | `}` |
|    55 | 3322 | `static int vm_builtin_Phar_addFromString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3323 | `{` |
|     - | 3324 | `	phl_phar *pPhar;` |
|     - | 3325 | `	const char *zName,*zData;` |
|    55 | 3326 | `	int nName = 0,nData = 0;` |
|    55 | 3327 | `	pPhar = PharThis(pCtx);` |
|    55 | 3328 | `	if( pPhar == 0 \|\| nArg < 2 ){` |
|   ! 0 | 3329 | `		return PH7_OK;` |
|     - | 3330 | `	}` |
|    55 | 3331 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    55 | 3332 | `	if( PharReadonly(pCtx->pVm) && !pPhar->bData ){` |
|     - | 3333 | `		/* php's addFromString names the ENTRY and the archive. */` |
|   ! 0 | 3334 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 3335 | `			"Entry %.*s does not exist and cannot be created: phar error: file \"%.*s\" in phar \"%.*s\" cannot be opened for writing, disabled by ini setting",` |
|   ! 0 | 3336 | `			nName,zName,nName,zName,` |
|   ! 0 | 3337 | `			(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));` |
|     - | 3338 | `	}` |
|    55 | 3339 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|    55 | 3340 | `	if( PharAddBytes(pCtx,pPhar,zName,nName,zData,nData,0) == 0 ){` |
|    55 | 3341 | `		PharCommit(pPhar);` |
|    25 | 3342 | `	}` |
|    55 | 3343 | `	ph7_result_null(pCtx);` |
|    55 | 3344 | `	return PH7_OK;` |
|    25 | 3345 | `}` |
|   ! 0 | 3346 | `static int vm_builtin_Phar_addFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3347 | `{` |
|     - | 3348 | `	phl_phar *pPhar;` |
|     - | 3349 | `	const ph7_io_stream *pStream;` |
|     - | 3350 | `	const char *zFile,*zLocal,*zPath;` |
|     - | 3351 | `	SyBlob sData;` |
|     - | 3352 | `	void *pHandle;` |
|   ! 0 | 3353 | `	int nFile = 0,nLocal = 0;` |
|   ! 0 | 3354 | `	pPhar = PharThis(pCtx);` |
|   ! 0 | 3355 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3356 | `		return PH7_OK;` |
|     - | 3357 | `	}` |
|   ! 0 | 3358 | `	zFile = ph7_value_to_string(apArg[0],&nFile);` |
|   ! 0 | 3359 | `	if( PharReadonly(pCtx->pVm) && !pPhar->bData ){` |
|   ! 0 | 3360 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 3361 | `			"Write operations disabled by the php.ini setting phar.readonly");` |
|     - | 3362 | `	}` |
|   ! 0 | 3363 | `	zLocal = zFile;` |
|   ! 0 | 3364 | `	nLocal = nFile;` |
|   ! 0 | 3365 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|   ! 0 | 3366 | `		zLocal = ph7_value_to_string(apArg[1],&nLocal);` |
|   ! 0 | 3367 | `	}` |
|   ! 0 | 3368 | `	zPath = zFile;` |
|   ! 0 | 3369 | `	pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zPath,nFile);` |
|   ! 0 | 3370 | `	pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,` |
|   ! 0 | 3371 | `		FALSE,0,FALSE,0,0) : 0;` |
|   ! 0 | 3372 | `	if( pHandle == 0 ){` |
|   ! 0 | 3373 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|   ! 0 | 3374 | `			"phar error: unable to open file \"%.*s\" to add to phar archive",nFile,zFile);` |
|     - | 3375 | `	}` |
|   ! 0 | 3376 | `	SyBlobInit(&sData,&pCtx->pVm->sAllocator);` |
|   ! 0 | 3377 | `	PH7_StreamReadWholeFile(pHandle,pStream,&sData);` |
|   ! 0 | 3378 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|   ! 0 | 3379 | `	if( PharAddBytes(pCtx,pPhar,zLocal,nLocal,(const char *)SyBlobData(&sData),` |
|   ! 0 | 3380 | `			(int)SyBlobLength(&sData),0) == 0 ){` |
|   ! 0 | 3381 | `		PharCommit(pPhar);` |
|   ! 0 | 3382 | `	}` |
|   ! 0 | 3383 | `	SyBlobRelease(&sData);` |
|   ! 0 | 3384 | `	ph7_result_null(pCtx);` |
|   ! 0 | 3385 | `	return PH7_OK;` |
|   ! 0 | 3386 | `}` |
|   ! 0 | 3387 | `static int vm_builtin_Phar_addEmptyDir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3388 | `{` |
|     - | 3389 | `	phl_phar *pPhar;` |
|     - | 3390 | `	const char *zName;` |
|   ! 0 | 3391 | `	int nName = 0;` |
|   ! 0 | 3392 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 3393 | `		return PH7_OK;` |
|     - | 3394 | `	}` |
|   ! 0 | 3395 | `	pPhar = PharThis(pCtx);` |
|   ! 0 | 3396 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3397 | `		return PH7_OK;` |
|     - | 3398 | `	}` |
|   ! 0 | 3399 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   ! 0 | 3400 | `	if( PharAddBytes(pCtx,pPhar,zName,nName,"",0,1) == 0 ){` |
|   ! 0 | 3401 | `		PharCommit(pPhar);` |
|   ! 0 | 3402 | `	}` |
|   ! 0 | 3403 | `	ph7_result_null(pCtx);` |
|   ! 0 | 3404 | `	return PH7_OK;` |
|   ! 0 | 3405 | `}` |
|     4 | 3406 | `static int vm_builtin_Phar_delete(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3407 | `{` |
|     - | 3408 | `	phl_phar *pPhar;` |
|     4 | 3409 | `	phl_phar_ent *pEnt,*pPrev = 0;` |
|     - | 3410 | `	const char *zName;` |
|     4 | 3411 | `	int nName = 0;` |
|     4 | 3412 | `	if( PharRefuseWrite(pCtx,PHAR_RO_WRITE) ){` |
|   ! 0 | 3413 | `		return PH7_OK;` |
|     - | 3414 | `	}` |
|     4 | 3415 | `	pPhar = PharThis(pCtx);` |
|     4 | 3416 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3417 | `		return PH7_OK;` |
|     - | 3418 | `	}` |
|     4 | 3419 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    20 | 3420 | `	for( pEnt = pPhar->pFirst ; pEnt ; pPrev = pEnt, pEnt = pEnt->pNext ){` |
|    18 | 3421 | `		if( (int)SyBlobLength(&pEnt->sName) == nName` |
|    11 | 3422 | `		 && SyMemcmp(SyBlobData(&pEnt->sName),zName,(sxu32)nName) == 0 ){` |
|     2 | 3423 | `			if( pPrev ){` |
|     2 | 3424 | `				pPrev->pNext = pEnt->pNext;` |
|     1 | 3425 | `			}else{` |
|   ! 0 | 3426 | `				pPhar->pFirst = pEnt->pNext;` |
|     - | 3427 | `			}` |
|     2 | 3428 | `			if( pPhar->pLast == pEnt ){` |
|     2 | 3429 | `				pPhar->pLast = pPrev;` |
|     1 | 3430 | `			}` |
|     2 | 3431 | `			pPhar->nEnt--;` |
|     2 | 3432 | `			PharEntFree(pPhar->pVm,pEnt);` |
|     2 | 3433 | `			PharCommit(pPhar);` |
|     2 | 3434 | `			ph7_result_bool(pCtx,1);` |
|     2 | 3435 | `			return PH7_OK;` |
|     - | 3436 | `		}` |
|     8 | 3437 | `	}` |
|     3 | 3438 | `	return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     1 | 3439 | `		"Entry %.*s does not exist and cannot be deleted",nName,zName);` |
|     2 | 3440 | `}` |
|    15 | 3441 | `static int vm_builtin_Phar_setStub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3442 | `{` |
|     - | 3443 | `	phl_phar *pPhar;` |
|     - | 3444 | `	const char *zStub;` |
|    15 | 3445 | `	int nStub = 0;` |
|    15 | 3446 | `	if( PharRefuseWrite(pCtx,PHAR_RO_STUB) ){` |
|   ! 0 | 3447 | `		return PH7_OK;` |
|     - | 3448 | `	}` |
|    15 | 3449 | `	pPhar = PharThis(pCtx);` |
|    15 | 3450 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3451 | `		return PH7_OK;` |
|     - | 3452 | `	}` |
|    15 | 3453 | `	zStub = ph7_value_to_string(apArg[0],&nStub);` |
|    15 | 3454 | `	if( pPhar->iFormat != PHAR_FORMAT_PHAR ){` |
|     - | 3455 | `		/* php refuses a stub on a plain archive: there is nowhere to put it. */` |
|     7 | 3456 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 3457 | `			"A Phar stub cannot be set in a plain %s archive",` |
|     5 | 3458 | `			pPhar->iFormat == PHAR_FORMAT_ZIP ? "zip" : "tar");` |
|     - | 3459 | `	}` |
|    10 | 3460 | `	SyBlobReset(&pPhar->sStub);` |
|    10 | 3461 | `	SyBlobAppend(&pPhar->sStub,zStub,(sxu32)nStub);` |
|     - | 3462 | `	/* php closes the stub itself when the script did not: the manifest has to` |
|     - | 3463 | ``	 * start after a `?>` and a line ending, and `getStub()` shows the closed`` |
|     - | 3464 | `	 * form. */` |
|    10 | 3465 | `	if( nStub < 3 \|\| SyMemcmp(&zStub[nStub-2],"?>",2) != 0 ){` |
|    10 | 3466 | `		SyBlobAppend(&pPhar->sStub," ?>\r\n",sizeof(" ?>\r\n")-1);` |
|     5 | 3467 | `	}` |
|    10 | 3468 | `	PharCommit(pPhar);` |
|    10 | 3469 | `	ph7_result_bool(pCtx,1);` |
|    10 | 3470 | `	return PH7_OK;` |
|     7 | 3471 | `}` |
|   ! 0 | 3472 | `static int vm_builtin_Phar_setAlias(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3473 | `{` |
|     - | 3474 | `	phl_phar *pPhar;` |
|     - | 3475 | `	const char *zAlias;` |
|   ! 0 | 3476 | `	int nAlias = 0;` |
|   ! 0 | 3477 | `	if( PharRefuseWrite(pCtx,PHAR_RO_WRITE) ){` |
|   ! 0 | 3478 | `		return PH7_OK;` |
|     - | 3479 | `	}` |
|   ! 0 | 3480 | `	pPhar = PharThis(pCtx);` |
|   ! 0 | 3481 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3482 | `		return PH7_OK;` |
|     - | 3483 | `	}` |
|   ! 0 | 3484 | `	zAlias = ph7_value_to_string(apArg[0],&nAlias);` |
|   ! 0 | 3485 | `	SyBlobReset(&pPhar->sAlias);` |
|   ! 0 | 3486 | `	SyBlobAppend(&pPhar->sAlias,zAlias,(sxu32)nAlias);` |
|   ! 0 | 3487 | `	PharCommit(pPhar);` |
|   ! 0 | 3488 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 | 3489 | `	return PH7_OK;` |
|   ! 0 | 3490 | `}` |
|     2 | 3491 | `static int vm_builtin_Phar_setMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3492 | `{` |
|     - | 3493 | `	phl_phar *pPhar;` |
|     2 | 3494 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 3495 | `		return PH7_OK;` |
|     - | 3496 | `	}` |
|     2 | 3497 | `	pPhar = PharThis(pCtx);` |
|     2 | 3498 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3499 | `		return PH7_OK;` |
|     - | 3500 | `	}` |
|     2 | 3501 | `	SyBlobReset(&pPhar->sMeta);` |
|     2 | 3502 | `	PH7_VmSerializeValue(pCtx,apArg[0],&pPhar->sMeta);` |
|     2 | 3503 | `	PharCommit(pPhar);` |
|     2 | 3504 | `	ph7_result_null(pCtx);` |
|     2 | 3505 | `	return PH7_OK;` |
|     1 | 3506 | `}` |
|   ! 0 | 3507 | `static int vm_builtin_Phar_delMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3508 | `{` |
|     - | 3509 | `	phl_phar *pPhar;` |
|   ! 0 | 3510 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 3511 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 3512 | `		return PH7_OK;` |
|     - | 3513 | `	}` |
|   ! 0 | 3514 | `	pPhar = PharThis(pCtx);` |
|   ! 0 | 3515 | `	if( pPhar ){` |
|   ! 0 | 3516 | `		SyBlobReset(&pPhar->sMeta);` |
|   ! 0 | 3517 | `		PharCommit(pPhar);` |
|   ! 0 | 3518 | `	}` |
|   ! 0 | 3519 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 | 3520 | `	return PH7_OK;` |
|   ! 0 | 3521 | `}` |
|     2 | 3522 | `static int vm_builtin_Phar_compressFiles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3523 | `{` |
|     - | 3524 | `	phl_phar *pPhar;` |
|     - | 3525 | `	phl_phar_ent *pEnt;` |
|     - | 3526 | `	sxi64 iComp;` |
|     2 | 3527 | `	if( PharRefuseWrite(pCtx,PHAR_RO_COMPRESS) ){` |
|   ! 0 | 3528 | `		return PH7_OK;` |
|     - | 3529 | `	}` |
|     2 | 3530 | `	pPhar = PharThis(pCtx);` |
|     2 | 3531 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3532 | `		return PH7_OK;` |
|     - | 3533 | `	}` |
|     2 | 3534 | `	iComp = ph7_value_to_int64(apArg[0]);` |
|     2 | 3535 | `	if( iComp != PHAR_C_GZ && iComp != PHAR_C_NONE ){` |
|   ! 0 | 3536 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 3537 | `			"Cannot compress with Bzip2 compression, bz2 extension is not enabled");` |
|     - | 3538 | `	}` |
|    10 | 3539 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|     8 | 3540 | `		if( PharEntLoad(pPhar,pEnt) != 0 ){` |
|   ! 0 | 3541 | `			continue;` |
|     - | 3542 | `		}` |
|     8 | 3543 | `		pEnt->nFlags = (pEnt->nFlags & ~(sxu32)PHAR_C_MASK) \| (sxu32)iComp;` |
|     4 | 3544 | `	}` |
|     2 | 3545 | `	PharCommit(pPhar);` |
|     2 | 3546 | `	ph7_result_null(pCtx);` |
|     2 | 3547 | `	return PH7_OK;` |
|     1 | 3548 | `}` |
|     2 | 3549 | `static int vm_builtin_Phar_decompressFiles(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3550 | `{` |
|     - | 3551 | `	phl_phar *pPhar;` |
|     - | 3552 | `	phl_phar_ent *pEnt;` |
|     1 | 3553 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     2 | 3554 | `	if( PharRefuseWrite(pCtx,PHAR_RO_COMPRESS) ){` |
|   ! 0 | 3555 | `		return PH7_OK;` |
|     - | 3556 | `	}` |
|     2 | 3557 | `	pPhar = PharThis(pCtx);` |
|     2 | 3558 | `	if( pPhar == 0 ){` |
|   ! 0 | 3559 | `		return PH7_OK;` |
|     - | 3560 | `	}` |
|    10 | 3561 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|     8 | 3562 | `		if( PharEntLoad(pPhar,pEnt) != 0 ){` |
|   ! 0 | 3563 | `			continue;` |
|     - | 3564 | `		}` |
|     8 | 3565 | `		pEnt->nFlags &= ~(sxu32)PHAR_C_MASK;` |
|     4 | 3566 | `	}` |
|     2 | 3567 | `	PharCommit(pPhar);` |
|     2 | 3568 | `	ph7_result_bool(pCtx,1);` |
|     2 | 3569 | `	return PH7_OK;` |
|     1 | 3570 | `}` |
|     2 | 3571 | `static int vm_builtin_Phar_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3572 | `{` |
|     - | 3573 | `	phl_phar *pPhar;` |
|     - | 3574 | `	phl_phar_ent *pFrom;` |
|     - | 3575 | `	const char *zFrom,*zTo;` |
|     2 | 3576 | `	int nFrom = 0,nTo = 0;` |
|     2 | 3577 | `	if( PharRefuseWrite(pCtx,PHAR_RO_WRITE) ){` |
|   ! 0 | 3578 | `		return PH7_OK;` |
|     - | 3579 | `	}` |
|     2 | 3580 | `	pPhar = PharThis(pCtx);` |
|     2 | 3581 | `	if( pPhar == 0 \|\| nArg < 2 ){` |
|   ! 0 | 3582 | `		return PH7_OK;` |
|     - | 3583 | `	}` |
|     2 | 3584 | `	zFrom = ph7_value_to_string(apArg[0],&nFrom);` |
|     2 | 3585 | `	zTo = ph7_value_to_string(apArg[1],&nTo);` |
|     2 | 3586 | `	pFrom = PharFindEnt(pPhar,zFrom,nFrom);` |
|     2 | 3587 | `	if( pFrom == 0 \|\| PharEntLoad(pPhar,pFrom) != 0 ){` |
|   ! 0 | 3588 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|   ! 0 | 3589 | `			"file \"%.*s\" does not exist in phar \"%.*s\"",nFrom,zFrom,` |
|   ! 0 | 3590 | `			(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));` |
|     - | 3591 | `	}` |
|     3 | 3592 | `	if( PharAddBytes(pCtx,pPhar,zTo,nTo,(const char *)SyBlobData(&pFrom->sData),` |
|     3 | 3593 | `			(int)SyBlobLength(&pFrom->sData),0) == 0 ){` |
|     2 | 3594 | `		PharCommit(pPhar);` |
|     1 | 3595 | `	}` |
|     2 | 3596 | `	ph7_result_bool(pCtx,1);` |
|     2 | 3597 | `	return PH7_OK;` |
|     1 | 3598 | `}` |
|     - | 3599 | `/* php's buffering pair only decides WHEN the file is rewritten; every door here` |
|     - | 3600 | ` * rewrites at once, so these two are the no-ops php's own are for an archive` |
|     - | 3601 | ` * that is not being written. */` |
|   ! 0 | 3602 | `static int vm_builtin_Phar_startBuffering(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3603 | `{` |
|   ! 0 | 3604 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 3605 | `	ph7_result_null(pCtx);` |
|   ! 0 | 3606 | `	return PH7_OK;` |
|   ! 0 | 3607 | `}` |
|    18 | 3608 | `static int vm_builtin_Phar_setSignatureAlgorithm(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3609 | `{` |
|     - | 3610 | `	phl_phar *pPhar;` |
|     - | 3611 | `	sxi64 iAlgo;` |
|     - | 3612 | `	int iPrev;` |
|    18 | 3613 | `	if( PharRefuseWrite(pCtx,PHAR_RO_WRITE) ){` |
|   ! 0 | 3614 | `		return PH7_OK;` |
|     - | 3615 | `	}` |
|    18 | 3616 | `	pPhar = PharThis(pCtx);` |
|    18 | 3617 | `	if( pPhar == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3618 | `		return PH7_OK;` |
|     - | 3619 | `	}` |
|    18 | 3620 | `	iAlgo = ph7_value_to_int64(apArg[0]);` |
|    18 | 3621 | `	if( iAlgo != PHAR_SIG_MD5 && iAlgo != PHAR_SIG_SHA1` |
|    18 | 3622 | `	 && iAlgo != PHAR_SIG_SHA256 && iAlgo != PHAR_SIG_SHA512` |
|     - | 3623 | `#ifdef PH7_ENABLE_OPENSSL` |
|    16 | 3624 | `	 && iAlgo != PHAR_SIG_OPENSSL && iAlgo != PHAR_SIG_OPENSSL_SHA256` |
|     7 | 3625 | `	 && iAlgo != PHAR_SIG_OPENSSL_SHA512` |
|     - | 3626 | `#endif` |
|     - | 3627 | `	  ){` |
|     - | 3628 | `		/* A build with no crypto refuses the three OpenSSL numbers here, which` |
|     - | 3629 | `		 * is what php's own no-ext/openssl build does with them. */` |
|     4 | 3630 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 3631 | `			"Unknown signature algorithm specified");` |
|     - | 3632 | `	}` |
|    14 | 3633 | `	iPrev = pPhar->iSigType;` |
|    14 | 3634 | `	pPhar->iSigType = (int)iAlgo;` |
|     - | 3635 | `#ifdef PH7_ENABLE_OPENSSL` |
|    14 | 3636 | `	SyBlobReset(&pPhar->sPrivKey);` |
|    14 | 3637 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|     8 | 3638 | `		int nKey = 0;` |
|     8 | 3639 | `		const char *zKey = ph7_value_to_string(apArg[1],&nKey);` |
|     8 | 3640 | `		if( zKey && nKey > 0 ){` |
|     8 | 3641 | `			SyBlobAppend(&pPhar->sPrivKey,zKey,(sxu32)nKey);` |
|     4 | 3642 | `		}` |
|     4 | 3643 | `	}` |
|    14 | 3644 | `	if( (iAlgo == PHAR_SIG_OPENSSL \|\| iAlgo == PHAR_SIG_OPENSSL_SHA256` |
|    14 | 3645 | `	  \|\| iAlgo == PHAR_SIG_OPENSSL_SHA512) && SyBlobLength(&pPhar->sPrivKey) < 1 ){` |
|     - | 3646 | `		/* php's own sentence for "you asked for an openssl signature and gave` |
|     - | 3647 | `		 * me nothing to sign with". */` |
|     2 | 3648 | `		pPhar->iSigType = iPrev;` |
|     3 | 3649 | `		return PH7_VmThrowException(pCtx,"PharException",` |
|     - | 3650 | `			"phar error: unable to write signature: unable to write to phar \"%.*s\" with requested openssl signature",` |
|     2 | 3651 | `			(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));` |
|     - | 3652 | `	}` |
|     - | 3653 | `#endif` |
|    12 | 3654 | `	if( PharCommit(pPhar) != 0 ){` |
|     - | 3655 | `#ifdef PH7_ENABLE_OPENSSL` |
|     2 | 3656 | `		if( iAlgo == PHAR_SIG_OPENSSL \|\| iAlgo == PHAR_SIG_OPENSSL_SHA256` |
|   ! 0 | 3657 | `		 \|\| iAlgo == PHAR_SIG_OPENSSL_SHA512 ){` |
|     - | 3658 | `			/* The only way the writer refuses an archive it was going to write` |
|     - | 3659 | `			 * anyway: the key parsed as nothing. php says so in its own words. */` |
|     2 | 3660 | `			pPhar->iSigType = iPrev;` |
|     2 | 3661 | `			SyBlobReset(&pPhar->sPrivKey);` |
|     2 | 3662 | `			return PH7_VmThrowException(pCtx,"PharException",` |
|     - | 3663 | `				"phar error: unable to write signature: unable to process private key");` |
|     - | 3664 | `		}` |
|     - | 3665 | `#endif` |
|   ! 0 | 3666 | `	}` |
|    10 | 3667 | `	ph7_result_null(pCtx);` |
|    10 | 3668 | `	return PH7_OK;` |
|     9 | 3669 | `}` |
|     - | 3670 | `/* ---- the statics ---- */` |
|     1 | 3671 | `static int vm_builtin_Phar_apiVersion(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3672 | `{` |
|   ! 0 | 3673 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     1 | 3674 | `	ph7_result_string(pCtx,PHAR_API_STRING,-1);` |
|     1 | 3675 | `	return PH7_OK;` |
|   ! 0 | 3676 | `}` |
|   ! 0 | 3677 | `static int vm_builtin_Phar_canCompress(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3678 | `{` |
|   ! 0 | 3679 | `	sxi64 iComp = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 3680 | `#ifdef PH7_ENABLE_ZLIB` |
|   ! 0 | 3681 | `	int bGz = 1;` |
|     - | 3682 | `#else` |
|     - | 3683 | `	int bGz = 0;` |
|     - | 3684 | `#endif` |
|     - | 3685 | `	/* php answers for the build it is: BZ2 is false here, exactly as it is in a` |
|     - | 3686 | `	 * php compiled without ext/bz2. */` |
|   ! 0 | 3687 | `	if( iComp == PHAR_C_BZ2 ){` |
|   ! 0 | 3688 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3689 | `	}else if( iComp == PHAR_C_GZ \|\| iComp == 0 ){` |
|   ! 0 | 3690 | `		ph7_result_bool(pCtx,bGz);` |
|   ! 0 | 3691 | `	}else{` |
|   ! 0 | 3692 | `		ph7_result_bool(pCtx,0);` |
|     - | 3693 | `	}` |
|   ! 0 | 3694 | `	return PH7_OK;` |
|   ! 0 | 3695 | `}` |
|     3 | 3696 | `static int vm_builtin_Phar_canWrite(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3697 | `{` |
|     1 | 3698 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 3699 | `	ph7_result_bool(pCtx,!PharReadonly(pCtx->pVm));` |
|     3 | 3700 | `	return PH7_OK;` |
|   ! 0 | 3701 | `}` |
|   ! 0 | 3702 | `static int vm_builtin_Phar_getSupportedCompression(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3703 | `{` |
|   ! 0 | 3704 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|   ! 0 | 3705 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 3706 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 3707 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 3708 | `		return PH7_OK;` |
|     - | 3709 | `	}` |
|     - | 3710 | `#ifdef PH7_ENABLE_ZLIB` |
|   ! 0 | 3711 | `	ph7_value_string(pVal,"GZ",-1);` |
|   ! 0 | 3712 | `	ph7_array_add_elem(pArray,0,pVal);` |
|     - | 3713 | `#endif` |
|   ! 0 | 3714 | `	ph7_result_value(pCtx,pArray);` |
|   ! 0 | 3715 | `	ph7_context_release_value(pCtx,pVal);` |
|   ! 0 | 3716 | `	return PH7_OK;` |
|   ! 0 | 3717 | `}` |
|     3 | 3718 | `static int vm_builtin_Phar_getSupportedSignatures(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3719 | `{` |
|     - | 3720 | `	/* What the BUILD can do, which is php's own answer here: a php with no` |
|     - | 3721 | `	 * ext/openssl lists four and this engine did too until it linked one. */` |
|     - | 3722 | `	static const char * const azSig[] = { "MD5", "SHA-1", "SHA-256", "SHA-512"` |
|     - | 3723 | `#ifdef PH7_ENABLE_OPENSSL` |
|     - | 3724 | `		, "OpenSSL", "OpenSSL_SHA256", "OpenSSL_SHA512"` |
|     - | 3725 | `#endif` |
|     - | 3726 | `	};` |
|     3 | 3727 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|     3 | 3728 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     - | 3729 | `	sxu32 i;` |
|     1 | 3730 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     3 | 3731 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 3732 | `		return PH7_OK;` |
|     - | 3733 | `	}` |
|    24 | 3734 | `	for( i = 0 ; i < SX_ARRAYSIZE(azSig) ; ++i ){` |
|    21 | 3735 | `		ph7_value_reset_string_cursor(pVal);` |
|    21 | 3736 | `		ph7_value_string(pVal,azSig[i],-1);` |
|    21 | 3737 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     7 | 3738 | `	}` |
|     3 | 3739 | `	ph7_result_value(pCtx,pArray);` |
|     3 | 3740 | `	ph7_context_release_value(pCtx,pVal);` |
|     3 | 3741 | `	return PH7_OK;` |
|     1 | 3742 | `}` |
|     1 | 3743 | `static int vm_builtin_Phar_createDefaultStub(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3744 | `{` |
|     - | 3745 | `	SyBlob sStub;` |
|   ! 0 | 3746 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     1 | 3747 | `	SyBlobInit(&sStub,&pCtx->pVm->sAllocator);` |
|     1 | 3748 | `	PharDefaultStub(&sStub);` |
|     1 | 3749 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sStub),(int)SyBlobLength(&sStub));` |
|     1 | 3750 | `	SyBlobRelease(&sStub);` |
|     1 | 3751 | `	return PH7_OK;` |
|   ! 0 | 3752 | `}` |
|     - | 3753 | `/*` |
|     - | 3754 | `` * `Phar::isValidPharFilename()`: whether php would treat this NAME as an`` |
|     - | 3755 | ` * archive. Two rules, and they are not each other's mirror:` |
|     - | 3756 | ` *` |
|     - | 3757 | `` *   - an EXECUTABLE archive's name must carry `.phar` -- spelled in lower case,`` |
|     - | 3758 | `` *     which is why `a.PHAR` is not one -- and its directory part, if it has`` |
|     - | 3759 | `` *     one, must be absolute or dot-relative (`dir/b.phar` is refused where`` |
|     - | 3760 | `` *     `./b.phar` and `/tmp/b.phar` are taken);`` |
|     - | 3761 | `` *   - a DATA archive's name is anything with an extension that is NOT `.phar`.`` |
|     - | 3762 | `` *     php does not screen WHICH extension: `a.txt` is a valid PharData name and`` |
|     - | 3763 | ` *     the format is decided by what the file turns out to hold -- but a name` |
|     - | 3764 | `` *     with no extension at all (`phar`) is refused.`` |
|     - | 3765 | ` */` |
|     7 | 3766 | `static int vm_builtin_Phar_isValidPharFilename(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3767 | `{` |
|     - | 3768 | `	const char *z;` |
|     7 | 3769 | `	int n = 0,i,iSlash = -1,bExec = 1,bPhar = 0,bDot = 0;` |
|     7 | 3770 | `	if( nArg < 1 ){` |
|   ! 0 | 3771 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3772 | `		return PH7_OK;` |
|     - | 3773 | `	}` |
|     7 | 3774 | `	z = ph7_value_to_string(apArg[0],&n);` |
|     7 | 3775 | `	if( nArg > 1 ){` |
|     7 | 3776 | `		bExec = ph7_value_to_bool(apArg[1]);` |
|   ! 0 | 3777 | `	}` |
|     7 | 3778 | `	while( n > 0 && (z[n-1] == '/' \|\| z[n-1] == '\\') ){` |
|   ! 0 | 3779 | `		n--;   /* php tolerates a trailing separator */` |
|   ! 0 | 3780 | `	}` |
|    53 | 3781 | `	for( i = 0 ; i < n ; ++i ){` |
|    46 | 3782 | `		if( z[i] == '/' \|\| z[i] == '\\' ){` |
|     1 | 3783 | `			iSlash = i;` |
|   ! 0 | 3784 | `		}` |
|   ! 0 | 3785 | `	}` |
|     - | 3786 | ``	/* `.phar` has to sit BEHIND something: a name that is only the extension`` |
|     - | 3787 | `	 * names no archive. */` |
|     7 | 3788 | `	for( i = iSlash + 2 ; i + 5 <= n ; ++i ){` |
|     3 | 3789 | `		if( SyMemcmp(&z[i],".phar",5) == 0 ){` |
|     3 | 3790 | `			bPhar = 1;` |
|     3 | 3791 | `			break;` |
|     - | 3792 | `		}` |
|   ! 0 | 3793 | `	}` |
|     7 | 3794 | `	for( i = iSlash + 2 ; i < n ; ++i ){` |
|     7 | 3795 | `		if( z[i] == '.' ){` |
|     7 | 3796 | `			bDot = 1;` |
|     7 | 3797 | `			break;` |
|     - | 3798 | `		}` |
|   ! 0 | 3799 | `	}` |
|     7 | 3800 | `	if( bExec ){` |
|     - | 3801 | `		/* A directory part php will not resolve: only an absolute path or one` |
|     - | 3802 | ``		 * that starts at `.` names a file it can create. */`` |
|     4 | 3803 | `		if( iSlash > 0 && !(z[0] == '/' \|\| z[0] == '\\' \|\| z[0] == '.'` |
|     - | 3804 | `#ifdef __WINNT__` |
|     - | 3805 | `			\|\| (n > 2 && z[1] == ':')` |
|     - | 3806 | `#endif` |
|     - | 3807 | `			) ){` |
|     1 | 3808 | `			ph7_result_bool(pCtx,0);` |
|     1 | 3809 | `			return PH7_OK;` |
|     - | 3810 | `		}` |
|     3 | 3811 | `		ph7_result_bool(pCtx,bPhar);` |
|     3 | 3812 | `		return PH7_OK;` |
|     - | 3813 | `	}` |
|     - | 3814 | `	/* A data archive's name is a bare one or a dot-relative one -- php answers` |
|     - | 3815 | `	 * false for an absolute path here, where it answers true for an executable` |
|     - | 3816 | `	 * archive's. */` |
|     3 | 3817 | `	if( iSlash >= 0 && z[0] != '.' ){` |
|   ! 0 | 3818 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3819 | `		return PH7_OK;` |
|     - | 3820 | `	}` |
|     3 | 3821 | `	ph7_result_bool(pCtx,bDot && !bPhar);` |
|     3 | 3822 | `	return PH7_OK;` |
|   ! 0 | 3823 | `}` |
|     - | 3824 | `/*` |
|     - | 3825 | `` * `Phar::mapPhar()` -- what every executable archive's stub calls. It opens the`` |
|     - | 3826 | ` * archive the RUNNING script is, registers its alias, and from then on` |
|     - | 3827 | `` * `phar://<alias>/entry` names an entry inside it.`` |
|     - | 3828 | ` */` |
|     2 | 3829 | `static int vm_builtin_Phar_mapPhar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3830 | `{` |
|     2 | 3831 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 3832 | `	phl_phar *pPhar;` |
|     - | 3833 | `	SyString *pFile;` |
|     - | 3834 | `	const char *zErr;` |
|     2 | 3835 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     2 | 3836 | `	if( pFile == 0 \|\| pFile->nByte < 1 ){` |
|   ! 0 | 3837 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3838 | `		return PH7_VmThrowException(pCtx,"PharException",` |
|     - | 3839 | `			"Cannot call mapPhar() from outside a phar archive");` |
|     - | 3840 | `	}` |
|     2 | 3841 | `	pPhar = PharOpenPath(pVm,pFile->zString,(int)pFile->nByte,0,&zErr);` |
|     2 | 3842 | `	if( pPhar == 0 ){` |
|   ! 0 | 3843 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3844 | `		return PH7_VmThrowException(pCtx,"PharException",` |
|   ! 0 | 3845 | `			"internal corruption of phar \"%z\" (%s)",pFile,zErr ? zErr : "not a phar archive");` |
|     - | 3846 | `	}` |
|     2 | 3847 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     2 | 3848 | `		int nAlias = 0;` |
|     2 | 3849 | `		const char *zAlias = ph7_value_to_string(apArg[0],&nAlias);` |
|     2 | 3850 | `		SyBlobReset(&pPhar->sAlias);` |
|     2 | 3851 | `		SyBlobAppend(&pPhar->sAlias,zAlias,(sxu32)nAlias);` |
|     1 | 3852 | `	}` |
|     - | 3853 | ``	/* `Phar::running()` answers this from here on. */`` |
|     2 | 3854 | `	SyBlobReset(&pVm->sPharRunning);` |
|     2 | 3855 | `	SyBlobAppend(&pVm->sPharRunning,SyBlobData(&pPhar->sPath),SyBlobLength(&pPhar->sPath));` |
|     2 | 3856 | `	ph7_result_bool(pCtx,1);` |
|     2 | 3857 | `	return PH7_OK;` |
|     1 | 3858 | `}` |
|   ! 0 | 3859 | `static int vm_builtin_Phar_loadPhar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3860 | `{` |
|     - | 3861 | `	phl_phar *pPhar;` |
|     - | 3862 | `	const char *zPath,*zErr;` |
|   ! 0 | 3863 | `	int nPath = 0;` |
|   ! 0 | 3864 | `	if( nArg < 1 ){` |
|   ! 0 | 3865 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3866 | `		return PH7_OK;` |
|     - | 3867 | `	}` |
|   ! 0 | 3868 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|   ! 0 | 3869 | `	pPhar = PharOpenPath(pCtx->pVm,zPath,nPath,0,&zErr);` |
|   ! 0 | 3870 | `	if( pPhar == 0 ){` |
|   ! 0 | 3871 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3872 | `		return PH7_VmThrowException(pCtx,"PharException",` |
|   ! 0 | 3873 | `			"Unknown phar archive \"%.*s\": %s",nPath,zPath,zErr ? zErr : "unable to open phar");` |
|     - | 3874 | `	}` |
|   ! 0 | 3875 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|   ! 0 | 3876 | `		int nAlias = 0;` |
|   ! 0 | 3877 | `		const char *zAlias = ph7_value_to_string(apArg[1],&nAlias);` |
|   ! 0 | 3878 | `		SyBlobReset(&pPhar->sAlias);` |
|   ! 0 | 3879 | `		SyBlobAppend(&pPhar->sAlias,zAlias,(sxu32)nAlias);` |
|   ! 0 | 3880 | `	}` |
|   ! 0 | 3881 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 | 3882 | `	return PH7_OK;` |
|   ! 0 | 3883 | `}` |
|     - | 3884 | `/*` |
|     - | 3885 | `` * `Phar::running()`: the archive the CURRENT script is running from, as a`` |
|     - | 3886 | `` * `phar://` url with `$returnPhar` (the default) and as the file's own path`` |
|     - | 3887 | ` * without it. Outside an archive both are the empty string -- never false,` |
|     - | 3888 | `` * which is what a `if (Phar::running())` guard reads.`` |
|     - | 3889 | ` */` |
|     6 | 3890 | `static int vm_builtin_Phar_running(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3891 | `{` |
|     6 | 3892 | `	ph7_vm *pVm = pCtx->pVm;` |
|     6 | 3893 | `	int bUrl = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 1;` |
|     6 | 3894 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     - | 3895 | `	SyBlob sPath;` |
|     6 | 3896 | `	int bFound = 0;` |
|     6 | 3897 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|     - | 3898 | `	/*` |
|     - | 3899 | `	 * php answers from the file that is EXECUTING: any code included through a` |
|     - | 3900 | ``	 * `phar://` url is "running from" that archive, whether or not a stub ever`` |
|     - | 3901 | `	 * called mapPhar(). The mapped archive is the answer for the STUB itself,` |
|     - | 3902 | `	 * whose own path carries no scheme.` |
|     - | 3903 | `	 */` |
|     6 | 3904 | `	if( pFile && pFile->nByte > sizeof("phar://")-1` |
|     6 | 3905 | `	 && SyStrnicmp(pFile->zString,"phar://",sizeof("phar://")-1) == 0 ){` |
|     - | 3906 | `		const char *zEnt,*zErr;` |
|     4 | 3907 | `		int nEnt = 0;` |
|     6 | 3908 | `		phl_phar *pPhar = PharResolveUrl(pVm,` |
|     4 | 3909 | `			&pFile->zString[sizeof("phar://")-1],` |
|     4 | 3910 | `			(int)pFile->nByte - (int)sizeof("phar://") + 1,&zEnt,&nEnt,&zErr);` |
|     4 | 3911 | `		if( pPhar ){` |
|     4 | 3912 | `			SyBlobAppend(&sPath,SyBlobData(&pPhar->sPath),SyBlobLength(&pPhar->sPath));` |
|     4 | 3913 | `			bFound = 1;` |
|     2 | 3914 | `		}` |
|     2 | 3915 | `	}` |
|     6 | 3916 | `	if( !bFound && SyBlobLength(&pVm->sPharRunning) > 0 ){` |
|   ! 0 | 3917 | `		SyBlobAppend(&sPath,SyBlobData(&pVm->sPharRunning),SyBlobLength(&pVm->sPharRunning));` |
|   ! 0 | 3918 | `		bFound = 1;` |
|   ! 0 | 3919 | `	}` |
|     6 | 3920 | `	if( !bFound ){` |
|     2 | 3921 | `		SyBlobRelease(&sPath);` |
|     2 | 3922 | `		ph7_result_string(pCtx,"",0);` |
|     2 | 3923 | `		return PH7_OK;` |
|     - | 3924 | `	}` |
|     4 | 3925 | `	if( bUrl ){` |
|     - | 3926 | `		SyBlob sUrl;` |
|     4 | 3927 | `		SyBlobInit(&sUrl,&pVm->sAllocator);` |
|     4 | 3928 | `		SyBlobAppend(&sUrl,"phar://",sizeof("phar://")-1);` |
|     4 | 3929 | `		SyBlobAppend(&sUrl,SyBlobData(&sPath),SyBlobLength(&sPath));` |
|     4 | 3930 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sUrl),(int)SyBlobLength(&sUrl));` |
|     4 | 3931 | `		SyBlobRelease(&sUrl);` |
|     2 | 3932 | `	}else{` |
|   ! 0 | 3933 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));` |
|     - | 3934 | `	}` |
|     4 | 3935 | `	SyBlobRelease(&sPath);` |
|     4 | 3936 | `	return PH7_OK;` |
|     2 | 3937 | `}` |
|   ! 0 | 3938 | `static int vm_builtin_Phar_unlinkArchive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3939 | `{` |
|     - | 3940 | `	phl_phar *pPhar;` |
|   ! 0 | 3941 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine ? pCtx->pVm->pEngine->pVfs : 0;` |
|     - | 3942 | `	const char *zPath,*zErr;` |
|   ! 0 | 3943 | `	int nPath = 0;` |
|   ! 0 | 3944 | `	if( nArg < 1 ){` |
|   ! 0 | 3945 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3946 | `		return PH7_OK;` |
|     - | 3947 | `	}` |
|   ! 0 | 3948 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|   ! 0 | 3949 | `	pPhar = PharOpenPath(pCtx->pVm,zPath,nPath,0,&zErr);` |
|   ! 0 | 3950 | `	if( pPhar == 0 ){` |
|   ! 0 | 3951 | `		return PH7_VmThrowException(pCtx,"PharException",` |
|   ! 0 | 3952 | `			"Unknown phar archive \"%.*s\": %s \"%.*s\"",nPath,zPath,` |
|   ! 0 | 3953 | `			zErr ? zErr : "unable to open phar for reading",nPath,zPath);` |
|     - | 3954 | `	}` |
|   ! 0 | 3955 | `	if( pVfs && pVfs->xUnlink ){` |
|     - | 3956 | `		SyBlob sPath;` |
|   ! 0 | 3957 | `		SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|   ! 0 | 3958 | `		SyBlobAppend(&sPath,zPath,(sxu32)nPath);` |
|   ! 0 | 3959 | `		SyBlobNullAppend(&sPath);` |
|   ! 0 | 3960 | `		pVfs->xUnlink((const char *)SyBlobData(&sPath));` |
|   ! 0 | 3961 | `		SyBlobRelease(&sPath);` |
|   ! 0 | 3962 | `	}` |
|   ! 0 | 3963 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 | 3964 | `	return PH7_OK;` |
|   ! 0 | 3965 | `}` |
|   ! 0 | 3966 | `static int vm_builtin_Phar_mount(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3967 | `{` |
|   ! 0 | 3968 | `	const char *zIn = "",*zOut = "";` |
|   ! 0 | 3969 | `	int nIn = 0,nOut = 0;` |
|   ! 0 | 3970 | `	if( nArg > 1 ){` |
|   ! 0 | 3971 | `		zIn = ph7_value_to_string(apArg[0],&nIn);` |
|   ! 0 | 3972 | `		zOut = ph7_value_to_string(apArg[1],&nOut);` |
|   ! 0 | 3973 | `	}` |
|     - | 3974 | `	/* php's own refusal when the mount cannot be made -- and a phar mounted` |
|     - | 3975 | `	 * over a real directory is a face this build does not carry, so it is` |
|     - | 3976 | `	 * always this one. */` |
|   ! 0 | 3977 | `	return PH7_VmThrowException(pCtx,"PharException",` |
|   ! 0 | 3978 | `		"Mounting of %.*s to %.*s failed",nIn,zIn,nOut,zOut);` |
|   ! 0 | 3979 | `}` |
|     - | 3980 | `/* ---- PharFileInfo ---- */` |
|   ! 0 | 3981 | `static int vm_builtin_PharFileInfo_construct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 3982 | `{` |
|   ! 0 | 3983 | `	ph7_vm *pVm = pCtx->pVm;` |
|   ! 0 | 3984 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3985 | `	phl_phar *pPhar;` |
|     - | 3986 | `	phl_phar_ent *pEnt;` |
|     - | 3987 | `	const char *zUrl,*zEnt,*zErr,*zPath;` |
|   ! 0 | 3988 | `	int nUrl = 0,nEnt = 0;` |
|   ! 0 | 3989 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3990 | `		return PH7_OK;` |
|     - | 3991 | `	}` |
|   ! 0 | 3992 | `	zUrl = ph7_value_to_string(apArg[0],&nUrl);` |
|   ! 0 | 3993 | `	zPath = zUrl;` |
|   ! 0 | 3994 | `	if( nUrl > 7 && SyStrnicmp(zUrl,"phar://",7) == 0 ){` |
|   ! 0 | 3995 | `		zPath = &zUrl[7];` |
|   ! 0 | 3996 | `		nUrl -= 7;` |
|   ! 0 | 3997 | `	}else{` |
|   ! 0 | 3998 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|   ! 0 | 3999 | `			"'%.*s' is not a phar archive URL (phar://...)",nUrl,zUrl);` |
|     - | 4000 | `	}` |
|   ! 0 | 4001 | `	pPhar = PharResolveUrl(pVm,zPath,nUrl,&zEnt,&nEnt,&zErr);` |
|   ! 0 | 4002 | `	if( pPhar == 0 ){` |
|   ! 0 | 4003 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|   ! 0 | 4004 | `			"Cannot open phar file '%.*s': %s",nUrl,zPath,zErr ? zErr : "unable to open phar");` |
|     - | 4005 | `	}` |
|   ! 0 | 4006 | `	pEnt = PharFindEnt(pPhar,zEnt,nEnt);` |
|   ! 0 | 4007 | `	if( pEnt == 0 ){` |
|   ! 0 | 4008 | `		return PH7_VmThrowException(pCtx,"UnexpectedValueException",` |
|     - | 4009 | `			"Cannot access phar file entry '%.*s' in archive '%.*s'",` |
|   ! 0 | 4010 | `			nEnt,zEnt,(int)SyBlobLength(&pPhar->sPath),(const char *)SyBlobData(&pPhar->sPath));` |
|     - | 4011 | `	}` |
|   ! 0 | 4012 | `	PharSetSplPath(pVm,pThis,ph7_value_to_string(apArg[0],0),` |
|   ! 0 | 4013 | `		(int)SyStrlen(ph7_value_to_string(apArg[0],0)));` |
|   ! 0 | 4014 | `	PharSetStr(pThis,PHAR_SLOT_ENT,(const char *)SyBlobData(&pEnt->sName),` |
|   ! 0 | 4015 | `		(int)SyBlobLength(&pEnt->sName));` |
|   ! 0 | 4016 | `	PharAttach(pThis,pPhar);` |
|   ! 0 | 4017 | `	return PH7_OK;` |
|   ! 0 | 4018 | `}` |
|     2 | 4019 | `static int vm_builtin_PharFileInfo_getSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4020 | `{` |
|     2 | 4021 | `	phl_phar *pPhar = 0;` |
|     2 | 4022 | `	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|     1 | 4023 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     2 | 4024 | `	ph7_result_int64(pCtx,pEnt ? (sxi64)pEnt->nSize : 0);` |
|     2 | 4025 | `	return PH7_OK;` |
|   ! 0 | 4026 | `}` |
|     2 | 4027 | `static int vm_builtin_PharFileInfo_getCompressedSize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4028 | `{` |
|     2 | 4029 | `	phl_phar *pPhar = 0;` |
|     2 | 4030 | `	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|     1 | 4031 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     2 | 4032 | `	ph7_result_int64(pCtx,pEnt ? (sxi64)pEnt->nCompSize : 0);` |
|     2 | 4033 | `	return PH7_OK;` |
|   ! 0 | 4034 | `}` |
|     2 | 4035 | `static int vm_builtin_PharFileInfo_getCRC32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4036 | `{` |
|     2 | 4037 | `	phl_phar *pPhar = 0;` |
|     2 | 4038 | `	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|     1 | 4039 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     2 | 4040 | `	ph7_result_int64(pCtx,pEnt ? (sxi64)pEnt->nCrc : 0);` |
|     2 | 4041 | `	return PH7_OK;` |
|   ! 0 | 4042 | `}` |
|     2 | 4043 | `static int vm_builtin_PharFileInfo_getPharFlags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4044 | `{` |
|     1 | 4045 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     - | 4046 | `	/* php's per-entry phar flags: zero for every archive this reader opens. */` |
|     2 | 4047 | `	ph7_result_int64(pCtx,0);` |
|     2 | 4048 | `	return PH7_OK;` |
|   ! 0 | 4049 | `}` |
|     8 | 4050 | `static int vm_builtin_PharFileInfo_isCompressed(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4051 | `{` |
|     8 | 4052 | `	phl_phar *pPhar = 0;` |
|     8 | 4053 | `	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|     8 | 4054 | `	sxi64 iWant = nArg > 0 && !ph7_value_is_null(apArg[0]) ? ph7_value_to_int64(apArg[0]) : -1;` |
|     8 | 4055 | `	if( pEnt == 0 ){` |
|   ! 0 | 4056 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4057 | `		return PH7_OK;` |
|     - | 4058 | `	}` |
|     8 | 4059 | `	if( iWant < 0 ){` |
|     6 | 4060 | `		ph7_result_bool(pCtx,(pEnt->nFlags & PHAR_C_MASK) != 0);` |
|     3 | 4061 | `	}else{` |
|     2 | 4062 | `		ph7_result_bool(pCtx,(sxi64)(pEnt->nFlags & PHAR_C_MASK) == iWant);` |
|     - | 4063 | `	}` |
|     8 | 4064 | `	return PH7_OK;` |
|     4 | 4065 | `}` |
|     6 | 4066 | `static int vm_builtin_PharFileInfo_getContent(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4067 | `{` |
|     6 | 4068 | `	phl_phar *pPhar = 0;` |
|     6 | 4069 | `	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|     3 | 4070 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     6 | 4071 | `	if( pEnt == 0 \|\| pPhar == 0 \|\| PharEntLoad(pPhar,pEnt) != 0 ){` |
|   ! 0 | 4072 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4073 | `		return PH7_OK;` |
|     - | 4074 | `	}` |
|     9 | 4075 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pEnt->sData),` |
|     6 | 4076 | `		(int)SyBlobLength(&pEnt->sData));` |
|     6 | 4077 | `	return PH7_OK;` |
|     3 | 4078 | `}` |
|     2 | 4079 | `static int vm_builtin_PharFileInfo_getMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4080 | `{` |
|     2 | 4081 | `	phl_phar *pPhar = 0;` |
|     2 | 4082 | `	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|     1 | 4083 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     2 | 4084 | `	if( pEnt ){` |
|     2 | 4085 | `		PharMetaResult(pCtx,&pEnt->sMeta);` |
|     1 | 4086 | `	}` |
|     2 | 4087 | `	return PH7_OK;` |
|   ! 0 | 4088 | `}` |
|   ! 0 | 4089 | `static int vm_builtin_PharFileInfo_hasMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4090 | `{` |
|   ! 0 | 4091 | `	phl_phar *pPhar = 0;` |
|   ! 0 | 4092 | `	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|   ! 0 | 4093 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 4094 | `	ph7_result_bool(pCtx,pEnt != 0 && SyBlobLength(&pEnt->sMeta) > 0);` |
|   ! 0 | 4095 | `	return PH7_OK;` |
|   ! 0 | 4096 | `}` |
|     - | 4097 | `/* A directory the entry names IMPLY has no manifest row of its own, so both` |
|     - | 4098 | ` * questions are answered from the archive rather than from an entry. */` |
|     6 | 4099 | `static int PharInfoIsDir(ph7_context *pCtx)` |
|   ! 0 | 4100 | `{` |
|     6 | 4101 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     6 | 4102 | `	phl_phar *pPhar = PharOfInstance(pThis);` |
|     - | 4103 | `	ph7_value *pName;` |
|     - | 4104 | `	phl_phar_ent *pEnt;` |
|     6 | 4105 | `	if( pPhar == 0 ){` |
|   ! 0 | 4106 | `		return 0;` |
|     - | 4107 | `	}` |
|     6 | 4108 | `	pName = PH7_NativeAttr(pThis,PHAR_SLOT_ENT);` |
|     6 | 4109 | `	if( pName == 0 ){` |
|   ! 0 | 4110 | `		return 0;` |
|     - | 4111 | `	}` |
|     9 | 4112 | `	pEnt = PharFindEnt(pPhar,(const char *)SyBlobData(&pName->sBlob),` |
|     6 | 4113 | `		(int)SyBlobLength(&pName->sBlob));` |
|     6 | 4114 | `	if( pEnt ){` |
|     4 | 4115 | `		return pEnt->bDir;` |
|     - | 4116 | `	}` |
|     3 | 4117 | `	return PharIsDir(pPhar,(const char *)SyBlobData(&pName->sBlob),` |
|     2 | 4118 | `		(int)SyBlobLength(&pName->sBlob));` |
|     3 | 4119 | `}` |
|     6 | 4120 | `static int vm_builtin_PharFileInfo_isDir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4121 | `{` |
|     3 | 4122 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     6 | 4123 | `	ph7_result_bool(pCtx,PharInfoIsDir(pCtx));` |
|     6 | 4124 | `	return PH7_OK;` |
|   ! 0 | 4125 | `}` |
|     2 | 4126 | `static int vm_builtin_PharFileInfo_isFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4127 | `{` |
|     2 | 4128 | `	phl_phar *pPhar = 0;` |
|     2 | 4129 | `	phl_phar_ent *pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|     1 | 4130 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|     2 | 4131 | `	ph7_result_bool(pCtx,pEnt != 0 && !pEnt->bDir);` |
|     2 | 4132 | `	return PH7_OK;` |
|   ! 0 | 4133 | `}` |
|   ! 0 | 4134 | `static int vm_builtin_PharFileInfo_setCompression(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4135 | `{` |
|   ! 0 | 4136 | `	phl_phar *pPhar = 0;` |
|     - | 4137 | `	phl_phar_ent *pEnt;` |
|   ! 0 | 4138 | `	sxi64 iComp = nArg > 0 ? ph7_value_to_int64(apArg[0]) : PHAR_C_NONE;` |
|   ! 0 | 4139 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 4140 | `		return PH7_OK;` |
|     - | 4141 | `	}` |
|   ! 0 | 4142 | `	pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|   ! 0 | 4143 | `	if( pEnt == 0 \|\| pPhar == 0 ){` |
|   ! 0 | 4144 | `		return PH7_OK;` |
|     - | 4145 | `	}` |
|   ! 0 | 4146 | `	if( iComp == PHAR_C_BZ2 ){` |
|   ! 0 | 4147 | `		return PH7_VmThrowException(pCtx,"BadMethodCallException",` |
|     - | 4148 | `			"Cannot compress with Bzip2 compression, bz2 extension is not enabled");` |
|     - | 4149 | `	}` |
|   ! 0 | 4150 | `	if( PharEntLoad(pPhar,pEnt) == 0 ){` |
|   ! 0 | 4151 | `		pEnt->nFlags = (pEnt->nFlags & ~(sxu32)PHAR_C_MASK) \| (sxu32)iComp;` |
|   ! 0 | 4152 | `		PharCommit(pPhar);` |
|   ! 0 | 4153 | `	}` |
|   ! 0 | 4154 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 | 4155 | `	return PH7_OK;` |
|   ! 0 | 4156 | `}` |
|     2 | 4157 | `static int vm_builtin_PharFileInfo_setMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4158 | `{` |
|     2 | 4159 | `	phl_phar *pPhar = 0;` |
|     - | 4160 | `	phl_phar_ent *pEnt;` |
|     2 | 4161 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 4162 | `		return PH7_OK;` |
|     - | 4163 | `	}` |
|     2 | 4164 | `	pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|     2 | 4165 | `	if( pEnt == 0 \|\| nArg < 1 ){` |
|   ! 0 | 4166 | `		return PH7_OK;` |
|     - | 4167 | `	}` |
|     2 | 4168 | `	SyBlobReset(&pEnt->sMeta);` |
|     2 | 4169 | `	PH7_VmSerializeValue(pCtx,apArg[0],&pEnt->sMeta);` |
|     2 | 4170 | `	PharCommit(pPhar);` |
|     2 | 4171 | `	ph7_result_null(pCtx);` |
|     2 | 4172 | `	return PH7_OK;` |
|     1 | 4173 | `}` |
|   ! 0 | 4174 | `static int vm_builtin_PharFileInfo_delMetadata(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4175 | `{` |
|   ! 0 | 4176 | `	phl_phar *pPhar = 0;` |
|     - | 4177 | `	phl_phar_ent *pEnt;` |
|   ! 0 | 4178 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 4179 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 4180 | `		return PH7_OK;` |
|     - | 4181 | `	}` |
|   ! 0 | 4182 | `	pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|   ! 0 | 4183 | `	if( pEnt ){` |
|   ! 0 | 4184 | `		SyBlobReset(&pEnt->sMeta);` |
|   ! 0 | 4185 | `		PharCommit(pPhar);` |
|   ! 0 | 4186 | `	}` |
|   ! 0 | 4187 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 | 4188 | `	return PH7_OK;` |
|   ! 0 | 4189 | `}` |
|   ! 0 | 4190 | `static int vm_builtin_PharFileInfo_chmod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 4191 | `{` |
|   ! 0 | 4192 | `	phl_phar *pPhar = 0;` |
|     - | 4193 | `	phl_phar_ent *pEnt;` |
|   ! 0 | 4194 | `	if( PharRefuseWrite(pCtx,PHAR_RO_SETTING) ){` |
|   ! 0 | 4195 | `		return PH7_OK;` |
|     - | 4196 | `	}` |
|   ! 0 | 4197 | `	pEnt = PharFileInfoEnt(pCtx,&pPhar);` |
|   ! 0 | 4198 | `	if( pEnt == 0 \|\| nArg < 1 ){` |
|   ! 0 | 4199 | `		return PH7_OK;` |
|     - | 4200 | `	}` |
|   ! 0 | 4201 | `	pEnt->nFlags = (pEnt->nFlags & ~(sxu32)0777)` |
|   ! 0 | 4202 | `		\| ((sxu32)ph7_value_to_int64(apArg[0]) & 0777);` |
|   ! 0 | 4203 | `	PharCommit(pPhar);` |
|   ! 0 | 4204 | `	ph7_result_null(pCtx);` |
|   ! 0 | 4205 | `	return PH7_OK;` |
|   ! 0 | 4206 | `}` |
|     - | 4207 | `/* ------------------------------------------------------------------ */` |
|     - | 4208 | `/* Registration                                                        */` |
|     - | 4209 | `/* ------------------------------------------------------------------ */` |
|     - | 4210 | `/*` |
|     - | 4211 | ` * php's Phar and PharData both EXTEND RecursiveDirectoryIterator: the archive` |
|     - | 4212 | ` * object is a directory iterator over its own entries, and everything` |
|     - | 4213 | ` * SplFileInfo answers about a path it answers about the archive FILE. That is` |
|     - | 4214 | ` * why the two classes are declared with a parent rather than standing alone --` |
|     - | 4215 | `` * `$phar->getBasename()` and `(string) $phar` come from it for free.`` |
|     - | 4216 | ` *` |
|     - | 4217 | ` * The iteration methods, the ArrayAccess four and the SplFileInfo accessors` |
|     - | 4218 | ` * that must speak about an ENTRY instead are overridden below; everything else` |
|     - | 4219 | ` * the parent answers is already php's answer.` |
|     - | 4220 | ` */` |
|    90 | 4221 | `static void PharInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|   ! 0 | 4222 | `{` |
|     - | 4223 | `	/* The archive itself belongs to the per-VM registry, which frees it: two` |
|     - | 4224 | `	 * objects may name one archive (php caches them the same way), so an` |
|     - | 4225 | `	 * instance going away must not take it with it. */` |
|    43 | 4226 | `	SXUNUSED(pVm);` |
|    43 | 4227 | `	SXUNUSED(pThis);` |
|    90 | 4228 | `}` |
|  6721 | 4229 | `PH7_PRIVATE sxi32 PH7_VmInstallPhar(ph7_vm *pVm)` |
|     5 | 4230 | `{` |
|     - | 4231 | `	static const PH7_NativePropDef aProp[] = {` |
|     - | 4232 | `		{ PHAR_SLOT_RES, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 4233 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 4234 | `		{ PHAR_SLOT_CUR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 4235 | `		  { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 },` |
|     - | 4236 | `		{ PHAR_SLOT_DIR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 4237 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }` |
|     - | 4238 | `	};` |
|     - | 4239 | `	static const PH7_NativePropDef aInfoProp[] = {` |
|     - | 4240 | `		{ PHAR_SLOT_RES,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 4241 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 4242 | `		{ PHAR_SLOT_ENT,  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 4243 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 4244 | `		{ PHAR_SLOT_PHAR, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|     - | 4245 | `		  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 }` |
|     - | 4246 | `	};` |
|     - | 4247 | `	/* php's own order for the class, which is what Reflection lists. */` |
|     - | 4248 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|     - | 4249 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - | 4250 | `		  "string $filename, int $flags = FilesystemIterator::SKIP_DOTS\|FilesystemIterator::UNIX_PATHS, ?string $alias = null",` |
|     - | 4251 | `		  0, vm_builtin_Phar_construct },` |
|     - | 4252 | `		{ "__destruct", PH7_MOD_PUBLIC, "", 0, vm_builtin_Phar_void },` |
|     - | 4253 | `		{ "addEmptyDir", PH7_MOD_PUBLIC, "string $directory", "@void",` |
|     - | 4254 | `		  vm_builtin_Phar_addEmptyDir },` |
|     - | 4255 | `		{ "addFile", PH7_MOD_PUBLIC, "string $filename, ?string $localName = null", "@void",` |
|     - | 4256 | `		  vm_builtin_Phar_addFile },` |
|     - | 4257 | `		{ "addFromString", PH7_MOD_PUBLIC, "string $localName, string $contents", "@void",` |
|     - | 4258 | `		  vm_builtin_Phar_addFromString },` |
|     - | 4259 | `		{ "compressFiles", PH7_MOD_PUBLIC, "int $compression", "@void",` |
|     - | 4260 | `		  vm_builtin_Phar_compressFiles },` |
|     - | 4261 | `		{ "decompressFiles", PH7_MOD_PUBLIC, "", "@true", vm_builtin_Phar_decompressFiles },` |
|     - | 4262 | `		{ "copy", PH7_MOD_PUBLIC, "string $from, string $to", "@true", vm_builtin_Phar_copy },` |
|     - | 4263 | `		{ "count", PH7_MOD_PUBLIC, "int $mode = COUNT_NORMAL", "@int", vm_builtin_Phar_count },` |
|     - | 4264 | `		{ "delete", PH7_MOD_PUBLIC, "string $localName", "@true", vm_builtin_Phar_delete },` |
|     - | 4265 | `		{ "delMetadata", PH7_MOD_PUBLIC, "", "@true", vm_builtin_Phar_delMetadata },` |
|     - | 4266 | `		{ "extractTo", PH7_MOD_PUBLIC,` |
|     - | 4267 | `		  "string $directory, array\|string\|null $files = null, bool $overwrite = false",` |
|     - | 4268 | `		  "@bool", vm_builtin_Phar_extractTo },` |
|     - | 4269 | `		{ "getAlias", PH7_MOD_PUBLIC, "", "@?string", vm_builtin_Phar_getAlias },` |
|     - | 4270 | `		{ "getPath", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getPath },` |
|     - | 4271 | `		{ "getMetadata", PH7_MOD_PUBLIC, "array $unserializeOptions = []", "@mixed",` |
|     - | 4272 | `		  vm_builtin_Phar_getMetadata },` |
|     - | 4273 | `		{ "getModified", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_false },` |
|     - | 4274 | `		{ "getSignature", PH7_MOD_PUBLIC, "", "@array\|false", vm_builtin_Phar_getSignature },` |
|     - | 4275 | `		{ "getStub", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getStub },` |
|     - | 4276 | `		{ "getVersion", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getVersion },` |
|     - | 4277 | `		{ "hasMetadata", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_hasMetadata },` |
|     - | 4278 | `		{ "isBuffering", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_false },` |
|     - | 4279 | `		{ "isCompressed", PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_Phar_isCompressed },` |
|     - | 4280 | `		{ "isFileFormat", PH7_MOD_PUBLIC, "int $format", "@bool", vm_builtin_Phar_isFileFormat },` |
|     - | 4281 | `		{ "isWritable", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_isWritable },` |
|     - | 4282 | `		{ "offsetExists", PH7_MOD_PUBLIC, "$localName", "@bool", vm_builtin_Phar_offsetExists },` |
|     - | 4283 | `		{ "offsetGet", PH7_MOD_PUBLIC, "$localName", "@SplFileInfo", vm_builtin_Phar_offsetGet },` |
|     - | 4284 | `		{ "offsetSet", PH7_MOD_PUBLIC, "$localName, $value", "@void", vm_builtin_Phar_offsetSet },` |
|     - | 4285 | `		{ "offsetUnset", PH7_MOD_PUBLIC, "$localName", "@void", vm_builtin_Phar_offsetUnset },` |
|     - | 4286 | `		{ "setAlias", PH7_MOD_PUBLIC, "string $alias", "@true", vm_builtin_Phar_setAlias },` |
|     - | 4287 | `		{ "setDefaultStub", PH7_MOD_PUBLIC, "?string $index = null, ?string $webIndex = null",` |
|     - | 4288 | `		  "@true", vm_builtin_Phar_true },` |
|     - | 4289 | `		{ "setMetadata", PH7_MOD_PUBLIC, "mixed $metadata", "@void", vm_builtin_Phar_setMetadata },` |
|     - | 4290 | `		{ "setSignatureAlgorithm", PH7_MOD_PUBLIC, "int $algo, ?string $privateKey = null",` |
|     - | 4291 | `		  "@void", vm_builtin_Phar_setSignatureAlgorithm },` |
|     - | 4292 | `		{ "setStub", PH7_MOD_PUBLIC, "$stub, int $length = -1", "@true", vm_builtin_Phar_setStub },` |
|     - | 4293 | `		{ "startBuffering", PH7_MOD_PUBLIC, "", "@void", vm_builtin_Phar_startBuffering },` |
|     - | 4294 | `		{ "stopBuffering", PH7_MOD_PUBLIC, "", "@void", vm_builtin_Phar_startBuffering },` |
|     - | 4295 | `		/* The iterator half, overriding the parent's directory walk. */` |
|     - | 4296 | `		{ "rewind", PH7_MOD_PUBLIC, "", "@void", vm_builtin_Phar_rewind },` |
|     - | 4297 | `		{ "valid", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_valid },` |
|     - | 4298 | `		{ "key", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Phar_key },` |
|     - | 4299 | `		{ "current", PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_Phar_current },` |
|     - | 4300 | `		{ "next", PH7_MOD_PUBLIC, "", "@void", vm_builtin_Phar_next },` |
|     - | 4301 | `		{ "getFilename", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getFilename },` |
|     - | 4302 | `		{ "getPathname", PH7_MOD_PUBLIC, "", "@string", vm_builtin_Phar_getPathname },` |
|     - | 4303 | `		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_hasChildren },` |
|     - | 4304 | `		{ "getChildren", PH7_MOD_PUBLIC, "", "@RecursiveDirectoryIterator",` |
|     - | 4305 | `		  vm_builtin_Phar_getChildren },` |
|     - | 4306 | `		/* The statics. */` |
|     - | 4307 | `		{ "apiVersion", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL, "", "string",` |
|     - | 4308 | `		  vm_builtin_Phar_apiVersion },` |
|     - | 4309 | `		{ "canCompress", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL, "int $compression = 0",` |
|     - | 4310 | `		  "bool", vm_builtin_Phar_canCompress },` |
|     - | 4311 | `		{ "canWrite", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL, "", "bool",` |
|     - | 4312 | `		  vm_builtin_Phar_canWrite },` |
|     - | 4313 | `		{ "createDefaultStub", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL,` |
|     - | 4314 | `		  "?string $index = null, ?string $webIndex = null", "string",` |
|     - | 4315 | `		  vm_builtin_Phar_createDefaultStub },` |
|     - | 4316 | `		{ "getSupportedCompression", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL, "", "array",` |
|     - | 4317 | `		  vm_builtin_Phar_getSupportedCompression },` |
|     - | 4318 | `		{ "getSupportedSignatures", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL, "", "array",` |
|     - | 4319 | `		  vm_builtin_Phar_getSupportedSignatures },` |
|     - | 4320 | `		{ "interceptFileFuncs", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL, "", "void",` |
|     - | 4321 | `		  vm_builtin_Phar_void },` |
|     - | 4322 | `		{ "isValidPharFilename", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL,` |
|     - | 4323 | `		  "string $filename, bool $executable = true", "bool",` |
|     - | 4324 | `		  vm_builtin_Phar_isValidPharFilename },` |
|     - | 4325 | `		{ "loadPhar", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL,` |
|     - | 4326 | `		  "string $filename, ?string $alias = null", "bool", vm_builtin_Phar_loadPhar },` |
|     - | 4327 | `		{ "mapPhar", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL,` |
|     - | 4328 | `		  "?string $alias = null, int $offset = 0", "bool", vm_builtin_Phar_mapPhar },` |
|     - | 4329 | `		{ "running", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL, "bool $returnPhar = true",` |
|     - | 4330 | `		  "string", vm_builtin_Phar_running },` |
|     - | 4331 | `		{ "mount", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL,` |
|     - | 4332 | `		  "string $pharPath, string $externalPath", "void", vm_builtin_Phar_mount },` |
|     - | 4333 | `		{ "unlinkArchive", PH7_MOD_PUBLIC\|PH7_MOD_STATIC\|PH7_MOD_FINAL, "string $filename",` |
|     - | 4334 | `		  "true", vm_builtin_Phar_unlinkArchive }` |
|     - | 4335 | `	};` |
|     - | 4336 | `	static const PH7_NativeMethodDef aInfoMethod[] = {` |
|     - | 4337 | `		{ "__construct", PH7_MOD_PUBLIC, "string $filename", 0,` |
|     - | 4338 | `		  vm_builtin_PharFileInfo_construct },` |
|     - | 4339 | `		{ "__destruct", PH7_MOD_PUBLIC, "", 0, vm_builtin_Phar_void },` |
|     - | 4340 | `		{ "chmod", PH7_MOD_PUBLIC, "int $perms", "@void", vm_builtin_PharFileInfo_chmod },` |
|     - | 4341 | `		{ "compress", PH7_MOD_PUBLIC, "int $compression", "@bool",` |
|     - | 4342 | `		  vm_builtin_PharFileInfo_setCompression },` |
|     - | 4343 | `		{ "decompress", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_setCompression },` |
|     - | 4344 | `		{ "delMetadata", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_delMetadata },` |
|     - | 4345 | `		{ "getCompressedSize", PH7_MOD_PUBLIC, "", "@int",` |
|     - | 4346 | `		  vm_builtin_PharFileInfo_getCompressedSize },` |
|     - | 4347 | `		{ "getContent", PH7_MOD_PUBLIC, "", "@string", vm_builtin_PharFileInfo_getContent },` |
|     - | 4348 | `		{ "getCRC32", PH7_MOD_PUBLIC, "", "@int", vm_builtin_PharFileInfo_getCRC32 },` |
|     - | 4349 | `		{ "getMetadata", PH7_MOD_PUBLIC, "array $unserializeOptions = []", "@mixed",` |
|     - | 4350 | `		  vm_builtin_PharFileInfo_getMetadata },` |
|     - | 4351 | `		{ "getPharFlags", PH7_MOD_PUBLIC, "", "@int", vm_builtin_PharFileInfo_getPharFlags },` |
|     - | 4352 | `		{ "getSize", PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_PharFileInfo_getSize },` |
|     - | 4353 | `		{ "hasMetadata", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_hasMetadata },` |
|     - | 4354 | `		{ "isCompressed", PH7_MOD_PUBLIC, "?int $compression = null", "@bool",` |
|     - | 4355 | `		  vm_builtin_PharFileInfo_isCompressed },` |
|     - | 4356 | `		{ "isCRCChecked", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_Phar_true },` |
|     - | 4357 | `		{ "isDir", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_isDir },` |
|     - | 4358 | `		{ "isFile", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_PharFileInfo_isFile },` |
|     - | 4359 | `		{ "setMetadata", PH7_MOD_PUBLIC, "mixed $metadata", "@void",` |
|     - | 4360 | `		  vm_builtin_PharFileInfo_setMetadata }` |
|     - | 4361 | `	};` |
|     - | 4362 | `	/* php's Phar:: constants: the three formats, the three compressions, the` |
|     - | 4363 | `	 * two stub kinds and the seven signature algorithms. */` |
|     - | 4364 | `	static const PH7_NativeConstDef aConst[] = {` |
|     - | 4365 | `		{ "BZ2",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_C_BZ2, 0, 0.0 },` |
|     - | 4366 | `		{ "GZ",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_C_GZ, 0, 0.0 },` |
|     - | 4367 | `		{ "NONE",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_C_NONE, 0, 0.0 },` |
|     - | 4368 | `		{ "PHAR",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_FORMAT_PHAR, 0, 0.0 },` |
|     - | 4369 | `		{ "TAR",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_FORMAT_TAR, 0, 0.0 },` |
|     - | 4370 | `		{ "ZIP",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_FORMAT_ZIP, 0, 0.0 },` |
|     - | 4371 | `		{ "COMPRESSED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_C_MASK, 0, 0.0 },` |
|     - | 4372 | `		{ "PHP",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 0, 0, 0.0 },` |
|     - | 4373 | `		{ "PHPS",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1, 0, 0.0 },` |
|     - | 4374 | `		{ "MD5",            PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_MD5, 0, 0.0 },` |
|     - | 4375 | `		{ "OPENSSL",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_OPENSSL, 0, 0.0 },` |
|     - | 4376 | `		{ "OPENSSL_SHA256", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 17, 0, 0.0 },` |
|     - | 4377 | `		{ "OPENSSL_SHA512", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 18, 0, 0.0 },` |
|     - | 4378 | `		{ "SHA1",           PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_SHA1, 0, 0.0 },` |
|     - | 4379 | `		{ "SHA256",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_SHA256, 0, 0.0 },` |
|     - | 4380 | `		{ "SHA512",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, PHAR_SIG_SHA512, 0, 0.0 }` |
|     - | 4381 | `	};` |
|     - | 4382 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 4383 | `		{ "PharException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 4384 | `		{ "Phar", "RecursiveDirectoryIterator", "Countable,ArrayAccess", 0,` |
|     - | 4385 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - | 4386 | `		  aProp, SX_ARRAYSIZE(aProp), PharInstanceRelease, 0, 0 },` |
|     - | 4387 | `		{ "PharData", "RecursiveDirectoryIterator", "Countable,ArrayAccess", 0,` |
|     - | 4388 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - | 4389 | `		  aProp, SX_ARRAYSIZE(aProp), PharInstanceRelease, 0, 0 },` |
|     - | 4390 | `		{ "PharFileInfo", "SplFileInfo", 0, 0,` |
|     - | 4391 | `		  aInfoMethod, SX_ARRAYSIZE(aInfoMethod), 0, 0,` |
|     - | 4392 | `		  aInfoProp, SX_ARRAYSIZE(aInfoProp), PharInstanceRelease, 0, 0 }` |
|     - | 4393 | `	};` |
|  6726 | 4394 | `	pVm->pPhars = 0;` |
|  6726 | 4395 | `	SyBlobInit(&pVm->sPharRunning,&pVm->sAllocator);` |
|  6726 | 4396 | `	SyBlobInit(&pVm->sPharErr,&pVm->sAllocator);` |
|  6726 | 4397 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     5 | 4398 | `}` |
|     - | 4399 | `/* ------------------------------------------------------------------ */` |
|     - | 4400 | `/* The path operations over an archive                                 */` |
|     - | 4401 | `/* ------------------------------------------------------------------ */` |
|     - | 4402 | `/*` |
|     - | 4403 | `` * `unlink('phar://x.phar/f')`, `mkdir`, `rmdir`, `rename` and the chmod family`` |
|     - | 4404 | ` * over a phar url. php implements the first four for a WRITABLE archive and` |
|     - | 4405 | `` * refuses all of them with one sentence when `phar.readonly` is on -- which is`` |
|     - | 4406 | ` * php's default, and so the sentence a program normally sees.` |
|     - | 4407 | ` *` |
|     - | 4408 | ` * Answers 1 when it handled the call (the result is set), 0 to let the caller` |
|     - | 4409 | ` * carry on -- which never happens for a phar url.` |
|     - | 4410 | ` */` |
|     - | 4411 | `/* The archive path, NUL-terminated, for a message that names the FILE. */` |
|    24 | 4412 | `static const char * PharPathZ(phl_phar *pPhar)` |
|   ! 0 | 4413 | `{` |
|    24 | 4414 | `	SyBlobNullAppend(&pPhar->sPath);` |
|    24 | 4415 | `	return (const char *)SyBlobData(&pPhar->sPath);` |
|   ! 0 | 4416 | `}` |
|     - | 4417 | `/* php's sentence for a write-mode url whose archive is not a writable phar` |
|     - | 4418 | `` * name: a `.tar` or a `.zip` can have an entry unlinked, but a directory is`` |
|     - | 4419 | ` * created through a door that first insists on being able to CREATE the phar. */` |
|    16 | 4420 | `static void PharNotAPharName(ph7_context *pCtx,phl_phar *pPhar)` |
|   ! 0 | 4421 | `{` |
|    23 | 4422 | `	PH7_VmThrowWarningFmt(pCtx->pVm,` |
|     - | 4423 | `		"%s(): Cannot create phar '%s', file extension (or combination) not "` |
|     - | 4424 | `		"recognised or the directory does not exist",` |
|     7 | 4425 | `		ph7_function_name(pCtx),PharPathZ(pPhar));` |
|    16 | 4426 | `}` |
|     - | 4427 | `/* Does anything live UNDER this directory name? An archive's directories are` |
|     - | 4428 | ` * mostly implied by the entry names, so "empty" means no entry has it as a` |
|     - | 4429 | ` * prefix -- an explicit entry of its own does not count. */` |
|    65 | 4430 | `static int PharDirHasChild(phl_phar *pPhar,const char *zDir,int nDir)` |
|   ! 0 | 4431 | `{` |
|     - | 4432 | `	phl_phar_ent *pEnt;` |
|   178 | 4433 | `	for( pEnt = pPhar->pFirst ; pEnt ; pEnt = pEnt->pNext ){` |
|   132 | 4434 | `		const char *z = (const char *)SyBlobData(&pEnt->sName);` |
|   132 | 4435 | `		int n = (int)SyBlobLength(&pEnt->sName);` |
|   132 | 4436 | `		if( n > nDir + 1 && z[nDir] == '/' && SyMemcmp(z,zDir,(sxu32)nDir) == 0 ){` |
|    19 | 4437 | `			return 1;` |
|     - | 4438 | `		}` |
|    54 | 4439 | `	}` |
|    46 | 4440 | `	return 0;` |
|    30 | 4441 | `}` |
|     - | 4442 | `/* Unhook one entry from the manifest chain and free it. */` |
|    10 | 4443 | `static void PharEntUnlink(phl_phar *pPhar,phl_phar_ent *pEnt)` |
|   ! 0 | 4444 | `{` |
|    10 | 4445 | `	phl_phar_ent *p,*pPrev = 0;` |
|    16 | 4446 | `	for( p = pPhar->pFirst ; p ; pPrev = p, p = p->pNext ){` |
|    16 | 4447 | `		if( p != pEnt ){` |
|     6 | 4448 | `			continue;` |
|     - | 4449 | `		}` |
|    10 | 4450 | `		if( pPrev ){` |
|     4 | 4451 | `			pPrev->pNext = p->pNext;` |
|     2 | 4452 | `		}else{` |
|     6 | 4453 | `			pPhar->pFirst = p->pNext;` |
|     - | 4454 | `		}` |
|    10 | 4455 | `		if( pPhar->pLast == p ){` |
|     5 | 4456 | `			pPhar->pLast = pPrev;` |
|     2 | 4457 | `		}` |
|    10 | 4458 | `		pPhar->nEnt--;` |
|    10 | 4459 | `		PharEntFree(pPhar->pVm,p);` |
|    10 | 4460 | `		return;` |
|   ! 0 | 4461 | `	}` |
|     4 | 4462 | `}` |
|     - | 4463 | `/*` |
|     - | 4464 | ` * rename() over a phar url. It is the one path operation php tells the DESTINATION` |
|     - | 4465 | ` * about, and the only one that can move a whole directory: every entry under the` |
|     - | 4466 | ` * old name is re-prefixed, since a directory is mostly just the names beneath it.` |
|     - | 4467 | ` * Both ends have to be inside the SAME archive -- php refuses the pair rather` |
|     - | 4468 | ` * than copying between two of them.` |
|     - | 4469 | ` */` |
|    19 | 4470 | `static int PharRenameOp(ph7_context *pCtx,phl_phar *pPhar,const char *zPath,` |
|     - | 4471 | `	const char *zDest,const char *zName,int nName,phl_phar_ent *pEnt,int bDir)` |
|   ! 0 | 4472 | `{` |
|    19 | 4473 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 4474 | `	phl_phar *pTo;` |
|     - | 4475 | `	phl_phar_ent *p;` |
|     - | 4476 | `	const char *zToEnt,*zErr,*zTo;` |
|     - | 4477 | `	SyBlob sTo;` |
|    19 | 4478 | `	int nToEnt = 0,nTo;` |
|    19 | 4479 | `	if( zDest == 0 \|\| SyStrnicmp(zDest,"phar://",sizeof("phar://")-1) != 0 ){` |
|     - | 4480 | `		/* php's own sentence, and its rule: a rename never crosses wrappers. */` |
|   ! 0 | 4481 | `		PH7_VmThrowWarningFmt(pVm,"%s(): Cannot rename a file across wrapper types",` |
|   ! 0 | 4482 | `			ph7_function_name(pCtx));` |
|   ! 0 | 4483 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4484 | `		return 1;` |
|     - | 4485 | `	}` |
|    19 | 4486 | `	if( pPhar->bData ){` |
|     - | 4487 | `		/* A rename is a WRITE-mode url, and php only ever opens one on a name it` |
|     - | 4488 | ``		 * could have created the phar under -- which a `.tar` or a `.zip` is not,`` |
|     - | 4489 | ``		 * whatever `phar.readonly` says. */`` |
|    10 | 4490 | `		PH7_VmThrowWarningFmt(pVm,` |
|     - | 4491 | `			"%s(): phar error: cannot rename \"%s\" to \"%s\": invalid or non-writable url \"%s\"",` |
|     3 | 4492 | `			ph7_function_name(pCtx),zPath,zDest,zPath);` |
|     7 | 4493 | `		ph7_result_bool(pCtx,0);` |
|     7 | 4494 | `		return 1;` |
|     - | 4495 | `	}` |
|    18 | 4496 | `	pTo = PharResolveUrl(pVm,&zDest[sizeof("phar://")-1],` |
|    12 | 4497 | `		(int)SyStrlen(zDest) - (int)sizeof("phar://") + 1,&zToEnt,&nToEnt,&zErr);` |
|    12 | 4498 | `	if( pTo && pTo->bData ){` |
|     - | 4499 | `		/* php asks the same question of the DESTINATION, and names that end. */` |
|     3 | 4500 | `		PH7_VmThrowWarningFmt(pVm,` |
|     - | 4501 | `			"%s(): phar error: cannot rename \"%s\" to \"%s\": invalid or non-writable url \"%s\"",` |
|     1 | 4502 | `			ph7_function_name(pCtx),zPath,zDest,zDest);` |
|     2 | 4503 | `		ph7_result_bool(pCtx,0);` |
|     2 | 4504 | `		return 1;` |
|     - | 4505 | `	}` |
|    10 | 4506 | `	if( pTo != pPhar ){` |
|     3 | 4507 | `		PH7_VmThrowWarningFmt(pVm,` |
|     - | 4508 | `			"%s(): phar error: cannot rename \"%s\" to \"%s\", not within the same phar archive",` |
|     1 | 4509 | `			ph7_function_name(pCtx),zPath,zDest);` |
|     2 | 4510 | `		ph7_result_bool(pCtx,0);` |
|     2 | 4511 | `		return 1;` |
|     - | 4512 | `	}` |
|     8 | 4513 | `	if( pEnt == 0 && !bDir ){` |
|     3 | 4514 | `		PH7_VmThrowWarningFmt(pVm,` |
|     - | 4515 | `			"%s(): phar error: cannot rename \"%s\" to \"%s\" from extracted phar archive, "` |
|     1 | 4516 | `			"source does not exist",ph7_function_name(pCtx),zPath,zDest);` |
|     2 | 4517 | `		ph7_result_bool(pCtx,0);` |
|     2 | 4518 | `		return 1;` |
|     - | 4519 | `	}` |
|     6 | 4520 | `	SyBlobInit(&sTo,&pVm->sAllocator);` |
|     6 | 4521 | `	PharNormalize(zToEnt,nToEnt,&sTo);` |
|     6 | 4522 | `	zTo = (const char *)SyBlobData(&sTo);` |
|     6 | 4523 | `	nTo = (int)SyBlobLength(&sTo);` |
|     6 | 4524 | `	if( nTo < 1 ){` |
|   ! 0 | 4525 | `		SyBlobRelease(&sTo);` |
|   ! 0 | 4526 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4527 | `		return 1;` |
|     - | 4528 | `	}` |
|     - | 4529 | `	/* A name the destination already holds is replaced, as it is on a filesystem. */` |
|     6 | 4530 | `	p = PharFindEnt(pPhar,zTo,nTo);` |
|     6 | 4531 | `	if( p && p != pEnt ){` |
|     2 | 4532 | `		PharEntUnlink(pPhar,p);` |
|     1 | 4533 | `	}` |
|    16 | 4534 | `	for( p = pPhar->pFirst ; p ; p = p->pNext ){` |
|    10 | 4535 | `		const char *z = (const char *)SyBlobData(&p->sName);` |
|    10 | 4536 | `		int n = (int)SyBlobLength(&p->sName);` |
|     - | 4537 | `		SyBlob sNew;` |
|    10 | 4538 | `		if( n < nName \|\| SyMemcmp(z,zName,(sxu32)nName) != 0` |
|     7 | 4539 | `		 \|\| (n > nName && z[nName] != '/') ){` |
|     4 | 4540 | `			continue;` |
|     - | 4541 | `		}` |
|     6 | 4542 | `		SyBlobInit(&sNew,&pVm->sAllocator);` |
|     6 | 4543 | `		SyBlobAppend(&sNew,zTo,(sxu32)nTo);` |
|     6 | 4544 | `		if( n > nName ){` |
|     2 | 4545 | `			SyBlobAppend(&sNew,&z[nName],(sxu32)(n - nName));` |
|     1 | 4546 | `		}` |
|     - | 4547 | `		/* The bytes have to be in memory before the manifest is rewritten: what` |
|     - | 4548 | `		 * is on disk is about to stop being where this entry's offset points. */` |
|     6 | 4549 | `		PharEntLoad(pPhar,p);` |
|     6 | 4550 | `		SyBlobReset(&p->sName);` |
|     6 | 4551 | `		SyBlobAppend(&p->sName,SyBlobData(&sNew),SyBlobLength(&sNew));` |
|     6 | 4552 | `		SyBlobRelease(&sNew);` |
|     3 | 4553 | `	}` |
|     6 | 4554 | `	SyBlobRelease(&sTo);` |
|     6 | 4555 | `	PharCommit(pPhar);` |
|     6 | 4556 | `	ph7_result_bool(pCtx,1);` |
|     6 | 4557 | `	return 1;` |
|     9 | 4558 | `}` |
|     - | 4559 | `/*` |
|     - | 4560 | `` * unlink(), rename(), mkdir() and rmdir() over a `phar://` url. php gives its`` |
|     - | 4561 | ` * wrapper all four, and each has its own refusal -- which is what the table` |
|     - | 4562 | ` * below is: the doors do not share one sentence, they do not share the` |
|     - | 4563 | `` * `phar.readonly` rule (a data archive's unlink is allowed and its mkdir is`` |
|     - | 4564 | ` * not), and only rename is told about a second path.` |
|     - | 4565 | ` *` |
|     - | 4566 | ` * chmod(), chown() and chgrp() are NOT here: php's phar wrapper implements no` |
|     - | 4567 | ` * stream_metadata, so they take the engine's own "Cannot call chmod() for a` |
|     - | 4568 | ` * non-standard stream" -- the sentence vfs.c already writes for every wrapper` |
|     - | 4569 | ` * that has none.` |
|     - | 4570 | ` */` |
|    67 | 4571 | `PH7_PRIVATE int PH7_PharPathOp(ph7_context *pCtx,const char *zPath,const char *zDest,int eOp)` |
|   ! 0 | 4572 | `{` |
|    67 | 4573 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 4574 | `	phl_phar *pPhar;` |
|     - | 4575 | `	phl_phar_ent *pEnt;` |
|     - | 4576 | `	const char *zEnt,*zErr,*zName;` |
|     - | 4577 | `	SyBlob sEnt;` |
|    67 | 4578 | `	int nEnt = 0,nName,bDir,bFile,rc = 1;` |
|    96 | 4579 | `	pPhar = PharResolveUrl(pVm,&zPath[sizeof("phar://")-1],` |
|    67 | 4580 | `		(int)SyStrlen(zPath) - (int)sizeof("phar://") + 1,&zEnt,&nEnt,&zErr);` |
|    67 | 4581 | `	if( pPhar == 0 ){` |
|   ! 0 | 4582 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 4583 | `		return 1;` |
|     - | 4584 | `	}` |
|     - | 4585 | `	/* The directive closes an executable archive's write doors; a PharData's` |
|     - | 4586 | `	 * are open, and it is the NAME rather than the directive that stops the two` |
|     - | 4587 | `	 * directory ones below. */` |
|    67 | 4588 | `	if( PharReadonly(pVm) && !pPhar->bData ){` |
|     4 | 4589 | `		switch( eOp ){` |
|     1 | 4590 | `		case PHAR_PATHOP_MKDIR:` |
|     1 | 4591 | `			PH7_VmThrowWarningFmt(pVm,` |
|     - | 4592 | `				"%s(): phar error: cannot create directory \"%s\", write operations disabled",` |
|   ! 0 | 4593 | `				ph7_function_name(pCtx),zPath);` |
|     1 | 4594 | `			break;` |
|     1 | 4595 | `		case PHAR_PATHOP_RMDIR:` |
|     1 | 4596 | `			PH7_VmThrowWarningFmt(pVm,` |
|     - | 4597 | `				"%s(): phar error: cannot rmdir directory \"%s\", write operations disabled",` |
|   ! 0 | 4598 | `				ph7_function_name(pCtx),zPath);` |
|     1 | 4599 | `			break;` |
|     1 | 4600 | `		case PHAR_PATHOP_RENAME:` |
|     1 | 4601 | `			PH7_VmThrowWarningFmt(pVm,` |
|     - | 4602 | `				"%s(): phar error: cannot rename \"%s\" to \"%s\": invalid or non-writable url \"%s\"",` |
|   ! 0 | 4603 | `				ph7_function_name(pCtx),zPath,zDest ? zDest : "",zPath);` |
|     1 | 4604 | `			break;` |
|     1 | 4605 | `		default:` |
|     1 | 4606 | `			PH7_VmThrowWarningFmt(pVm,"%s(): %s",ph7_function_name(pCtx),zPharReadonlyStream);` |
|     1 | 4607 | `			break;` |
|     - | 4608 | `		}` |
|     4 | 4609 | `		ph7_result_bool(pCtx,0);` |
|     4 | 4610 | `		return 1;` |
|     - | 4611 | `	}` |
|    63 | 4612 | `	SyBlobInit(&sEnt,&pVm->sAllocator);` |
|    63 | 4613 | `	PharNormalize(zEnt,nEnt,&sEnt);` |
|    63 | 4614 | `	SyBlobNullAppend(&sEnt);` |
|    63 | 4615 | `	zName = (const char *)SyBlobData(&sEnt);` |
|    63 | 4616 | `	nName = (int)SyBlobLength(&sEnt);` |
|    63 | 4617 | `	pEnt = nName > 0 ? PharFindEnt(pPhar,zName,nName) : 0;` |
|    63 | 4618 | `	bFile = pEnt != 0 && !pEnt->bDir;` |
|    63 | 4619 | `	bDir = (pEnt != 0 && pEnt->bDir) \|\| PharDirHasChild(pPhar,zName,nName);` |
|    63 | 4620 | `	switch( eOp ){` |
|     8 | 4621 | `	case PHAR_PATHOP_UNLINK:` |
|     - | 4622 | `		/* A directory is not a file, and php words its absence the same way. */` |
|    14 | 4623 | `		if( bFile ){` |
|     6 | 4624 | `			PharEntUnlink(pPhar,pEnt);` |
|     6 | 4625 | `			PharCommit(pPhar);` |
|     6 | 4626 | `			ph7_result_bool(pCtx,1);` |
|     2 | 4627 | `		}else{` |
|    12 | 4628 | `			PH7_VmThrowWarningFmt(pVm,"%s(): unlink of \"%s\" failed, file does not exist",` |
|     4 | 4629 | `				ph7_function_name(pCtx),zPath);` |
|     8 | 4630 | `			ph7_result_bool(pCtx,0);` |
|     - | 4631 | `		}` |
|    14 | 4632 | `		break;` |
|     7 | 4633 | `	case PHAR_PATHOP_MKDIR:` |
|    13 | 4634 | `		if( pPhar->bData ){` |
|     7 | 4635 | `			PharNotAPharName(pCtx,pPhar);` |
|     7 | 4636 | `			ph7_result_bool(pCtx,0);` |
|     9 | 4637 | `		}else if( bFile ){` |
|     3 | 4638 | `			PH7_VmThrowWarningFmt(pVm,` |
|     - | 4639 | `				"%s(): phar error: cannot create directory \"%s\" in phar \"%s\", "` |
|     - | 4640 | `				"phar error: path \"%s\" exists and is a not a directory",` |
|     1 | 4641 | `				ph7_function_name(pCtx),zName,PharPathZ(pPhar),zName);` |
|     2 | 4642 | `			ph7_result_bool(pCtx,0);` |
|     5 | 4643 | `		}else if( bDir \|\| nName < 1 ){` |
|     3 | 4644 | `			PH7_VmThrowWarningFmt(pVm,` |
|     - | 4645 | `				"%s(): phar error: cannot create directory \"%s\" in phar \"%s\", "` |
|     - | 4646 | `				"directory already exists",` |
|     1 | 4647 | `				ph7_function_name(pCtx),zName,PharPathZ(pPhar));` |
|     2 | 4648 | `			ph7_result_bool(pCtx,0);` |
|     3 | 4649 | `		}else if( PharAddBytes(pCtx,pPhar,zName,nName,"",0,1) == 0 ){` |
|     2 | 4650 | `			PharCommit(pPhar);` |
|     2 | 4651 | `			ph7_result_bool(pCtx,1);` |
|     1 | 4652 | `		}else{` |
|   ! 0 | 4653 | `			ph7_result_bool(pCtx,0);` |
|     - | 4654 | `		}` |
|    13 | 4655 | `		break;` |
|     9 | 4656 | `	case PHAR_PATHOP_RMDIR:` |
|    17 | 4657 | `		if( pPhar->bData ){` |
|     9 | 4658 | `			PharNotAPharName(pCtx,pPhar);` |
|     9 | 4659 | `			ph7_result_bool(pCtx,0);` |
|    12 | 4660 | `		}else if( bFile ){` |
|     3 | 4661 | `			PH7_VmThrowWarningFmt(pVm,` |
|     - | 4662 | `				"%s(): phar error: cannot remove directory \"%s\" in phar \"%s\", "` |
|     - | 4663 | `				"phar error: path \"%s\" exists and is a not a directory",` |
|     1 | 4664 | `				ph7_function_name(pCtx),zName,PharPathZ(pPhar),zName);` |
|     2 | 4665 | `			ph7_result_bool(pCtx,0);` |
|     7 | 4666 | `		}else if( !bDir ){` |
|     3 | 4667 | `			PH7_VmThrowWarningFmt(pVm,` |
|     - | 4668 | `				"%s(): phar error: cannot remove directory \"%s\" in phar \"%s\", "` |
|     - | 4669 | `				"directory does not exist",` |
|     1 | 4670 | `				ph7_function_name(pCtx),zName,PharPathZ(pPhar));` |
|     2 | 4671 | `			ph7_result_bool(pCtx,0);` |
|     5 | 4672 | `		}else if( PharDirHasChild(pPhar,zName,nName) ){` |
|     3 | 4673 | `			PH7_VmThrowWarningFmt(pVm,"%s(): phar error: Directory not empty",` |
|     1 | 4674 | `				ph7_function_name(pCtx));` |
|     2 | 4675 | `			ph7_result_bool(pCtx,0);` |
|     1 | 4676 | `		}else{` |
|     2 | 4677 | `			PharEntUnlink(pPhar,pEnt);` |
|     2 | 4678 | `			PharCommit(pPhar);` |
|     2 | 4679 | `			ph7_result_bool(pCtx,1);` |
|     - | 4680 | `		}` |
|    17 | 4681 | `		break;` |
|    10 | 4682 | `	default:` |
|    19 | 4683 | `		rc = PharRenameOp(pCtx,pPhar,zPath,zDest,zName,nName,pEnt,bDir);` |
|    19 | 4684 | `		break;` |
|     - | 4685 | `	}` |
|    63 | 4686 | `	SyBlobRelease(&sEnt);` |
|    63 | 4687 | `	return rc;` |
|    29 | 4688 | `}` |
|     - | 4689 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 4690 |  |
|     - | 4691 | `#ifdef PH7_DISABLE_BUILTIN_FUNC` |
|     - | 4692 | `/* Tiny build: no phar. The two VM lifecycle hooks are called unconditionally,` |
|     - | 4693 | ` * and with no archive ever opened they have nothing to sweep. */` |
|     - | 4694 | `PH7_PRIVATE void PH7_PharVmReset(ph7_vm *pVm){ (void)pVm; }` |
|     - | 4695 | `PH7_PRIVATE void PH7_PharVmRelease(ph7_vm *pVm){ (void)pVm; }` |
|     - | 4696 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 4697 |  |
