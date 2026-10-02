# src/ph7/vm_zip.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2328/3006 lines (77.45%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) && defined(PH7_ENABLE_ZLIB)` |
|    - |    7 | `#include <zlib.h>` |
|    - |    8 | `#include <time.h>` |
|    - |    9 | `#include <errno.h>` |
|    - |   10 | `#ifdef PH7_ENABLE_OPENSSL` |
|    - |   11 | `/* WinZip AES is a BUILD question exactly as php's is: a libzip with no crypto` |
|    - |   12 | `` * backend answers `isEncryptionMethodSupported(EM_AES_256)` false and refuses to`` |
|    - |   13 | ` * write one, and so does this engine without ext/openssl. The traditional` |
|    - |   14 | ` * PKWARE cipher needs nothing but the CRC table zlib already carries. */` |
|    - |   15 | `#include <openssl/evp.h>` |
|    - |   16 | `#include <openssl/hmac.h>` |
|    - |   17 | `#include <openssl/rand.h>` |
|    - |   18 | `#endif` |
|    - |   19 | `/*` |
|    - |   20 | ` * Section:` |
|    - |   21 | ` *    php's zip extension: the ZipArchive class, its deprecated procedural half` |
|    - |   22 | `` *    and the read-only `zip://` wrapper.`` |
|    - |   23 | ` * Status:` |
|    - |   24 | ` *    Stable.` |
|    - |   25 | ` *` |
|    - |   26 | ` * WHY THIS IS A DERIVATION AND NOT A BINDING. php's ext/zip is a binding of` |
|    - |   27 | ` * libzip, and every other library-backed extension here (zlib, curl, openssl,` |
|    - |   28 | ` * libxml) is bound rather than re-derived because the LIBRARY's answers are the` |
|    - |   29 | ` * contract -- which ciphers exist, what a malformed document says. ext/zip is` |
|    - |   30 | ` * the case where that argument does not hold: what a script can observe is the` |
|    - |   31 | ` * ZIP FORMAT, which is a published byte layout, plus a fixed table of thirty-two` |
|    - |   32 | ` * error strings that has not changed in libzip's lifetime. Everything a program` |
|    - |   33 | ` * asks -- the entry list, the sizes, the CRCs, the comments, the mtimes -- comes` |
|    - |   34 | ` * out of the file, not out of the library. So this is derived, which is also why` |
|    - |   35 | ` * it is here on Windows and on any build with zlib, where php's own needs a` |
|    - |   36 | ` * bundled libzip.` |
|    - |   37 | ` *` |
|    - |   38 | `` * It rides `PH7_ENABLE_ZLIB` because php's ext/zip requires zlib too: a deflated`` |
|    - |   39 | ` * member is the format's normal case and an extension that could not read one` |
|    - |   40 | ` * would not be the extension.` |
|    - |   41 | ` *` |
|    - |   42 | `` * WHAT AN ARCHIVE IS HERE. One `phl_zip` per open handle, holding the whole FILE`` |
|    - |   43 | ` * in memory and an entry table over it -- the model ext/phar already uses, for` |
|    - |   44 | ` * the same reason: it makes a reader that works over any stream the engine can` |
|    - |   45 | ` * open, and it costs the archive's size in memory.` |
|    - |   46 | ` *` |
|    - |   47 | ` * THE ENTRY TABLE IS libzip's, not the file's. This is the part that is easy to` |
|    - |   48 | ` * get wrong. libzip does not rebuild the archive on every change; it keeps the` |
|    - |   49 | ` * ORIGINAL entries with their indexes and records what a script has asked for on` |
|    - |   50 | `` * top of them, and only `close()` writes. That model is php-visible all over:`` |
|    - |   51 | ` *` |
|    - |   52 | ``  *   - a deleted entry KEEPS its index. `$z->deleteName('b')` leaves `numFiles` `` |
|    - |   53 | `` *     where it was, `statName('b')` false and `getNameIndex(1)` false, until`` |
|    - |   54 | `` *     `close()` compacts them.`` |
|    - |   55 | `` *   - `FL_UNCHANGED` reads the ORIGINAL of a name or a comment, beside the`` |
|    - |   56 | ` *     changed one every other read answers.` |
|    - |   57 | `` *   - `unchangeIndex()` / `unchangeAll()` / `unchangeArchive()` put a change`` |
|    - |   58 | ` *     back, which is only expressible if the original was still there.` |
|    - |   59 | ` *   - an entry ADDED but not yet written reads back as size == comp_size,` |
|    - |   60 | `` *     `crc` 0 and `comp_method` 0 whatever it will be compressed with, and`` |
|    - |   61 | `` *     `getFromName()` on it is FALSE. Nothing has been compressed yet.`` |
|    - |   62 | ` *` |
|    - |   63 | ` * WHAT IS NOT HERE, and why each is honest rather than missing:` |
|    - |   64 | ` *` |
|    - |   65 | ` *   - BZIP2, LZMA, XZ and the other seven compression methods php NAMES as` |
|    - |   66 | `` *     constants. `isCompressionMethodSupported()` answers what this build can`` |
|    - |   67 | ` *     do, exactly as it does on a libzip built without those libraries, and an` |
|    - |   68 | ` *     entry compressed with one is still LISTED and still copied through a` |
|    - |   69 | ` *     rewrite untouched -- only reading its bytes fails.` |
|    - |   70 | ` *   - ZIP64 archives are READ (the locator, the end record and the per-entry` |
|    - |   71 | ` *     extra field) and never written: an archive this engine builds lives in` |
|    - |   72 | ` *     memory first, so the four-gigabyte boundary is not reachable from here.` |
|    - |   73 | ` *   - WinZip AES needs ext/openssl. Without it this build answers` |
|    - |   74 | `` *     `isEncryptionMethodSupported(EM_AES_256)` false, exactly as a libzip`` |
|    - |   75 | ` *     with no crypto backend does; the traditional cipher needs nothing and is` |
|    - |   76 | ` *     always there.` |
|    - |   77 | ` *` |
|    - |   78 | ` * ONE php ANSWER IS DELIBERATELY NOT REPRODUCED. libzip takes the traditional` |
|    - |   79 | ``  * cipher's check byte from the DOS stamp an entry had BEFORE `setMtime*()` `` |
|    - |   80 | ` * moved it and then writes the moved one, so an archive php wrote that way` |
|    - |   81 | ` * cannot be read back by php. This writer uses the stamp the header will` |
|    - |   82 | ` * carry. Reproducing the defect would mean writing an archive nothing can open.` |
|    - |   83 | ` */` |
|    - |   84 | `/* php's ZipArchive::open() flags. */` |
|    - |   85 | `#define ZIP_OPEN_CREATE     1` |
|    - |   86 | `#define ZIP_OPEN_EXCL       2` |
|    - |   87 | `#define ZIP_OPEN_CHECKCONS  4` |
|    - |   88 | `#define ZIP_OPEN_OVERWRITE  8` |
|    - |   89 | `#define ZIP_OPEN_RDONLY     16` |
|    - |   90 | `/* ...and its per-call FL_ flags. */` |
|    - |   91 | `#define ZIP_FL_NOCASE       1` |
|    - |   92 | `#define ZIP_FL_NODIR        2` |
|    - |   93 | `#define ZIP_FL_COMPRESSED   4` |
|    - |   94 | `#define ZIP_FL_UNCHANGED    8` |
|    - |   95 | `#define ZIP_FL_OVERWRITE    8192` |
|    - |   96 | `#define ZIP_FL_ENC_RAW      64` |
|    - |   97 | `#define ZIP_FL_ENC_UTF_8    2048` |
|    - |   98 | `/* The compression methods, as the format numbers them. */` |
|    - |   99 | `#define ZIP_CM_DEFAULT      (-1)` |
|    - |  100 | `#define ZIP_CM_STORE        0` |
|    - |  101 | `#define ZIP_CM_DEFLATE      8` |
|    - |  102 | `#define ZIP_CM_BZIP2        12` |
|    - |  103 | `/* The encryption methods php names. */` |
|    - |  104 | `#define ZIP_EM_NONE         0` |
|    - |  105 | `#define ZIP_EM_TRAD_PKWARE  1` |
|    - |  106 | `#define ZIP_EM_AES_128      257` |
|    - |  107 | `#define ZIP_EM_AES_192      258` |
|    - |  108 | `#define ZIP_EM_AES_256      259` |
|    - |  109 | `#define ZIP_EM_UNKNOWN      65535` |
|    - |  110 | `/* libzip's error codes. They are the values of ZipArchive::ER_*, the numbers` |
|    - |  111 | `` * `open()` answers on failure and what `$z->status` holds. */`` |
|    - |  112 | `#define ZIP_ER_OK              0` |
|    - |  113 | `#define ZIP_ER_MULTIDISK       1` |
|    - |  114 | `#define ZIP_ER_RENAME          2` |
|    - |  115 | `#define ZIP_ER_CLOSE           3` |
|    - |  116 | `#define ZIP_ER_SEEK            4` |
|    - |  117 | `#define ZIP_ER_READ            5` |
|    - |  118 | `#define ZIP_ER_WRITE           6` |
|    - |  119 | `#define ZIP_ER_CRC             7` |
|    - |  120 | `#define ZIP_ER_ZIPCLOSED       8` |
|    - |  121 | `#define ZIP_ER_NOENT           9` |
|    - |  122 | `#define ZIP_ER_EXISTS          10` |
|    - |  123 | `#define ZIP_ER_OPEN            11` |
|    - |  124 | `#define ZIP_ER_TMPOPEN         12` |
|    - |  125 | `#define ZIP_ER_ZLIB            13` |
|    - |  126 | `#define ZIP_ER_MEMORY          14` |
|    - |  127 | `#define ZIP_ER_CHANGED         15` |
|    - |  128 | `#define ZIP_ER_COMPNOTSUPP     16` |
|    - |  129 | `#define ZIP_ER_EOF             17` |
|    - |  130 | `#define ZIP_ER_INVAL           18` |
|    - |  131 | `#define ZIP_ER_NOZIP           19` |
|    - |  132 | `#define ZIP_ER_INTERNAL        20` |
|    - |  133 | `#define ZIP_ER_INCONS          21` |
|    - |  134 | `#define ZIP_ER_REMOVE          22` |
|    - |  135 | `#define ZIP_ER_DELETED         23` |
|    - |  136 | `#define ZIP_ER_ENCRNOTSUPP     24` |
|    - |  137 | `#define ZIP_ER_RDONLY          25` |
|    - |  138 | `#define ZIP_ER_NOPASSWD        26` |
|    - |  139 | `#define ZIP_ER_WRONGPASSWD     27` |
|    - |  140 | `#define ZIP_ER_OPNOTSUPP       28` |
|    - |  141 | `#define ZIP_ER_INUSE           29` |
|    - |  142 | `#define ZIP_ER_TELL            30` |
|    - |  143 | `#define ZIP_ER_COMPRESSED_DATA 31` |
|    - |  144 | `#define ZIP_ER_CANCELLED       32` |
|    - |  145 | `/* Not one of php's codes: the reader's way of saying the file was EMPTY, which` |
|    - |  146 | ` * php opens with a deprecation rather than refusing. It never reaches a` |
|    - |  147 | ` * script -- the two openers turn it into that notice and an empty archive. */` |
|    - |  148 | `#define ZIP_ER_EMPTY_FILE      (-1)` |
|    - |  149 | `/* The method BYTE a WinZip AES member carries, with the real one in its extra` |
|    - |  150 | ` * field, and the extra field's own id. */` |
|    - |  151 | `#define ZIP_CM_WINZIP_AES   99` |
|    - |  152 | `#define ZIP_EXTRA_AES       0x9901` |
|    - |  153 | ``/* The glob(3) flags `addGlob()` takes -- php's own GLOB_AVAILABLE_FLAGS, since`` |
|    - |  154 | ` * that argument is routed into glob() rather than read here. */` |
|    - |  155 | `#define ZIP_GLOB_FLAGS (PH7_GLOB_ERR\|PH7_GLOB_MARK\|PH7_GLOB_NOCHECK\|PH7_GLOB_NOSORT \` |
|    - |  156 | `	\|PH7_GLOB_BRACE\|PH7_GLOB_NOESCAPE\|PH7_GLOB_ONLYDIR)` |
|    - |  157 | `/* php's ZipArchive::AFL_RDONLY, the one archive flag it exposes. */` |
|    - |  158 | `#define ZIP_AFL_RDONLY      2` |
|    - |  159 | ``/* The operating systems the format's `version made by` high byte names. Only`` |
|    - |  160 | ` * the default matters to the writer; the rest are constants a script may hand` |
|    - |  161 | ` * back through setExternalAttributes(). */` |
|    - |  162 | `#define ZIP_OPSYS_UNIX      3` |
|    - |  163 | ``/* The version this writer stamps: libzip's own `3 << 8 \| 63` -- unix, and zip`` |
|    - |  164 | ` * specification 6.3 -- and the 2.0 a reader needs for deflate. Both are` |
|    - |  165 | ` * platform-independent on purpose, exactly as libzip's are: php writes the same` |
|    - |  166 | ` * two numbers from a Windows build. */` |
|    - |  167 | `#define ZIP_VERSION_MADE    ((ZIP_OPSYS_UNIX << 8) \| 63)` |
|    - |  168 | `#define ZIP_VERSION_NEEDED  20` |
|    - |  169 | `/*` |
|    - |  170 | `` * `ZipArchive::LIBZIP_VERSION`. php answers the version of the libzip it was`` |
|    - |  171 | ` * built against, and programs read it to decide which of the class's verbs` |
|    - |  172 | ` * exist. Nothing is linked here, so what this names is the API LEVEL this` |
|    - |  173 | ` * derivation reproduces -- the surface php's ext/zip presents over libzip 1.7,` |
|    - |  174 | ` * which is every method above.` |
|    - |  175 | ` */` |
|    - |  176 | `#define PHL_ZIP_VERSION "1.7.3"` |
|    - |  177 | `/* The external attributes libzip stamps on an entry nobody set them on. */` |
|    - |  178 | `#define ZIP_ATTR_FILE   ((sxu32)0100666 << 16)` |
|    - |  179 | `#define ZIP_ATTR_DIR    ((sxu32)0040777 << 16)` |
|    - |  180 |  |
|    - |  181 | `typedef struct phl_zip phl_zip;` |
|    - |  182 | `typedef struct phl_zip_ent phl_zip_ent;` |
|    - |  183 | `/*` |
|    - |  184 | ` * One member of an archive, in libzip's two-faced shape: what the FILE says and` |
|    - |  185 | `` * what the script has asked for since. A field with an `Orig` twin is one`` |
|    - |  186 | `` * `FL_UNCHANGED` can still read and `unchangeIndex()` can put back.`` |
|    - |  187 | ` */` |
|    - |  188 | `struct phl_zip_ent {` |
|    - |  189 | `	SyBlob sName;        /* the name a read answers */` |
|    - |  190 | `	SyBlob sOrigName;    /* ...and the one the central directory carried */` |
|    - |  191 | `	SyBlob sComment;` |
|    - |  192 | `	SyBlob sOrigComment;` |
|    - |  193 | `	SyBlob sData;        /* the UNCOMPRESSED bytes, once loaded or supplied */` |
|    - |  194 | `	SyBlob sSrcPath;     /* addFile(): the file to read at close, instead of sData */` |
|    - |  195 | `	sxi64 iSrcStart;     /* ...and the slice of it php was asked for */` |
|    - |  196 | `	sxi64 iSrcLen;       /* -1: to the end */` |
|    - |  197 | `	sxu32 nSize;         /* uncompressed length */` |
|    - |  198 | `	sxu32 nCompSize;     /* stored length, as the file has it */` |
|    - |  199 | `	sxu32 nCrc;` |
|    - |  200 | `	sxu32 nFlag;         /* the general-purpose flag word the file carries */` |
|    - |  201 | `	sxu32 nDosTime;      /* ...and its raw MS-DOS time word, which the traditional` |
|    - |  202 | `	                      * cipher's check byte is taken from when the flag says` |
|    - |  203 | `	                      * the sizes are in a trailing descriptor */` |
|    - |  204 | `	int iMethod;         /* the compression method the file uses */` |
|    - |  205 | `	int iSetMethod;      /* what setCompression() asked for, or ZIP_CM_DEFAULT */` |
|    - |  206 | `	int iEncMethod;      /* the encryption method the file uses */` |
|    - |  207 | `	int iSetEncrypt;     /* what setEncryption() asked for, or -1 */` |
|    - |  208 | `	int iRawMethod;      /* the method BYTE the file carries: 99 for WinZip AES,` |
|    - |  209 | `	                      * where iMethod is the real one out of its extra field */` |
|    - |  210 | `	SyBlob sPassword;    /* setEncryption()'s own password, when it named one */` |
|    - |  211 | `	sxu8 bHasPassword;` |
|    - |  212 | `	sxi64 iTime;         /* modification time */` |
|    - |  213 | `	sxi64 iOrigTime;` |
|    - |  214 | `	sxu32 nAttr;         /* external attributes */` |
|    - |  215 | `	sxu32 nOrigAttr;` |
|    - |  216 | `	int iOpsys;          /* ...and the OS whose attributes they are */` |
|    - |  217 | `	int iOrigOpsys;` |
|    - |  218 | `	sxi64 iOffset;       /* where the STORED bytes begin in the archive file */` |
|    - |  219 | ``	sxu8 bDir;           /* the name ended in `/`: an explicit directory entry */`` |
|    - |  220 | `	sxu8 bLoaded;        /* sData holds the uncompressed bytes */` |
|    - |  221 | `	SyBlob sLoadedPw;    /* ...and, for an ENCRYPTED entry, the password that` |
|    - |  222 | `	                      * opened them. The cache is only good for that one:` |
|    - |  223 | `	                      * libzip decrypts on every zip_fopen, so a read with a` |
|    - |  224 | `	                      * DIFFERENT password must refuse rather than answer` |
|    - |  225 | `	                      * plaintext somebody else's key produced. */` |
|    - |  226 | `	sxu8 bNew;           /* added this session: there is no original behind it */` |
|    - |  227 | `	sxu8 bDeleted;` |
|    - |  228 | `	sxu8 bNameHidden;    /* deleted once: the name lookup misses even after an` |
|    - |  229 | `	                      * unchangeIndex(), because libzip drops the name from` |
|    - |  230 | `	                      * its hash on delete and only unchangeAll() puts the` |
|    - |  231 | ``	                      * whole hash back. php-visible: `statIndex()` answers`` |
|    - |  232 | ``	                      * and `statName()` does not. */`` |
|    - |  233 | `	sxu8 bDataChanged;   /* sData/sSrcPath replaces what the file holds */` |
|    - |  234 | `	sxu8 bNameChanged;` |
|    - |  235 | `	sxu8 bCommentChanged;` |
|    - |  236 | `	sxu8 bTimeChanged;` |
|    - |  237 | `	sxu8 bAttrChanged;` |
|    - |  238 | `};` |
|    - |  239 | `/*` |
|    - |  240 | ` * One open archive. It outlives the object that opened it whenever a stream` |
|    - |  241 | ` * getStream() handed out is still being read, which is php's own lifetime:` |
|    - |  242 | `` * `close()` leaves such a stream working and destroying the OBJECT does not.`` |
|    - |  243 | ` */` |
|    - |  244 | `struct phl_zip {` |
|    - |  245 | `	ph7_vm *pVm;` |
|    - |  246 | ``	SyBlob sPath;          /* the expanded filename, as `$z->filename` reads it */`` |
|    - |  247 | `	SyBlob sFile;          /* the whole archive, when one was there to read */` |
|    - |  248 | `	SyBlob sComment;       /* the archive comment a read answers */` |
|    - |  249 | `	SyBlob sOrigComment;` |
|    - |  250 | `	SyBlob sPassword;      /* what setPassword() left */` |
|    - |  251 | `	phl_zip_ent **apEnt;   /* the entry table, in index order */` |
|    - |  252 | `	sxu32 nEnt;` |
|    - |  253 | `	sxu32 nAlloc;` |
|    - |  254 | ``	int iStatus;           /* `$z->status` */`` |
|    - |  255 | ``	int iStatusSys;        /* `$z->statusSys` */`` |
|    - |  256 | ``	int iLastId;           /* `$z->lastId` */`` |
|    - |  257 | `	int iArchiveFlags;     /* setArchiveFlag()'s AFL_ bits */` |
|    - |  258 | `	sxu8 bOpen;            /* an object still holds it */` |
|    - |  259 | `	sxu8 bRdonly;          /* opened ZipArchive::RDONLY */` |
|    - |  260 | `	sxu8 bCommentChanged;` |
|    - |  261 | `	sxu8 bCreated;         /* the file did not exist: nothing to copy through */` |
|    - |  262 | `	sxu8 bBuilding;        /* a write is in flight: what a progress or cancel callback` |
|    - |  263 | `	                        * asks of the same archive must not start a second one */` |
|    - |  264 | `	sxu8 bDead;            /* the OBJECT that owned it went away while it was open:` |
|    - |  265 | `	                        * every stream still reading it stops answering, which` |
|    - |  266 | ``	                        * is php's `Containing zip archive was closed` */`` |
|    - |  267 | `	sxu32 nRef;            /* the object, plus every live getStream() handle */` |
|    - |  268 | `	ph7_value *pProgress;  /* registerProgressCallback() */` |
|    - |  269 | `	ph7_value *pCancel;    /* registerCancelCallback() */` |
|    - |  270 | `	double rProgressRate;` |
|    - |  271 | `	phl_zip *pNext;        /* the per-VM registry */` |
|    - |  272 | `};` |
|    - |  273 | `/* ------------------------------------------------------------------ */` |
|    - |  274 | `/* The format's scalars                                                */` |
|    - |  275 | `/* ------------------------------------------------------------------ */` |
| 9588 |  276 | `static sxu32 ZipGet16(const unsigned char *z)` |
|    3 |  277 | `{` |
| 9591 |  278 | `	return (sxu32)z[0] \| ((sxu32)z[1] << 8);` |
|    3 |  279 | `}` |
| 5020 |  280 | `static sxu32 ZipGet32(const unsigned char *z)` |
|    3 |  281 | `{` |
| 5023 |  282 | `	return (sxu32)z[0] \| ((sxu32)z[1] << 8) \| ((sxu32)z[2] << 16) \| ((sxu32)z[3] << 24);` |
|    3 |  283 | `}` |
|  ! 0 |  284 | `static sxu64 ZipGet64(const unsigned char *z)` |
|  ! 0 |  285 | `{` |
|  ! 0 |  286 | `	return (sxu64)ZipGet32(z) \| ((sxu64)ZipGet32(&z[4]) << 32);` |
|  ! 0 |  287 | `}` |
| 2192 |  288 | `static void ZipPut16(SyBlob *pOut,sxu32 n)` |
|    3 |  289 | `{` |
|    - |  290 | `	unsigned char z[2];` |
| 2195 |  291 | `	z[0] = (unsigned char)(n & 0xFF);` |
| 2195 |  292 | `	z[1] = (unsigned char)((n >> 8) & 0xFF);` |
| 2195 |  293 | `	SyBlobAppend(pOut,z,sizeof(z));` |
| 2195 |  294 | `}` |
|  928 |  295 | `static void ZipPut32(SyBlob *pOut,sxu32 n)` |
|    3 |  296 | `{` |
|    - |  297 | `	unsigned char z[4];` |
|  931 |  298 | `	z[0] = (unsigned char)(n & 0xFF);` |
|  931 |  299 | `	z[1] = (unsigned char)((n >> 8) & 0xFF);` |
|  931 |  300 | `	z[2] = (unsigned char)((n >> 16) & 0xFF);` |
|  931 |  301 | `	z[3] = (unsigned char)((n >> 24) & 0xFF);` |
|  931 |  302 | `	SyBlobAppend(pOut,z,sizeof(z));` |
|  931 |  303 | `}` |
|    - |  304 | `/*` |
|    - |  305 | ` * The MS-DOS timestamp a zip entry carries, both ways.` |
|    - |  306 | ` *` |
|    - |  307 | ` * It is a LOCAL time in two-second steps -- the format has no zone -- so both` |
|    - |  308 | `` * halves go through the C library's `localtime`/`mktime` rather than through`` |
|    - |  309 | ` * this engine's own date machinery. That is libzip's choice and it has to be` |
|    - |  310 | ` * reproduced: a program that sets an mtime and reads it back must get the same` |
|    - |  311 | ` * number on any box, which it does exactly when the two conversions agree with` |
|    - |  312 | `` * each other, and `date.timezone` must not move a stored stamp.`` |
|    - |  313 | ` *` |
|    - |  314 | ` * A year before 1980 has no field to go in. libzip does not clamp the whole` |
|    - |  315 | ` * stamp for one: it writes a ZERO year and keeps the month, day and time, so` |
|    - |  316 | ` * 1970-01-01 comes back as 1980-12-31 (the localtime of the epoch, with 1980` |
|    - |  317 | ` * for its year). Reproduced rather than corrected -- it is what a php-written` |
|    - |  318 | ` * archive holds.` |
|    - |  319 | ` */` |
|  106 |  320 | `static void ZipUnixToDos(sxi64 iTime,sxu32 *pnTime,sxu32 *pnDate)` |
|    3 |  321 | `{` |
|  109 |  322 | `	time_t t = (time_t)iTime;` |
|  109 |  323 | `	struct tm *pTm = localtime(&t);` |
|    - |  324 | `	int iYear;` |
|  109 |  325 | `	if( pTm == 0 ){` |
|  ! 0 |  326 | `		*pnTime = 0;` |
|  ! 0 |  327 | `		*pnDate = (1 << 5) \| 1;` |
|  ! 0 |  328 | `		return;` |
|    - |  329 | `	}` |
|  109 |  330 | `	iYear = pTm->tm_year + 1900 - 1980;` |
|  109 |  331 | `	if( iYear < 0 ){` |
|  ! 0 |  332 | `		iYear = 0;` |
|  ! 0 |  333 | `	}` |
|  162 |  334 | `	*pnDate = (sxu32)((((sxu32)iYear & 0x7F) << 9)` |
|  106 |  335 | `		\| ((sxu32)(pTm->tm_mon + 1) << 5) \| (sxu32)pTm->tm_mday) & 0xFFFF;` |
|  162 |  336 | `	*pnTime = (sxu32)(((sxu32)pTm->tm_hour << 11) \| ((sxu32)pTm->tm_min << 5)` |
|  106 |  337 | `		\| ((sxu32)pTm->tm_sec / 2)) & 0xFFFF;` |
|   56 |  338 | `}` |
|  868 |  339 | `static sxi64 ZipDosToUnix(sxu32 nTime,sxu32 nDate)` |
|    3 |  340 | `{` |
|    - |  341 | `	struct tm sTm;` |
|    - |  342 | `	time_t t;` |
|  871 |  343 | `	SyZero(&sTm,sizeof(sTm));` |
|  871 |  344 | `	sTm.tm_year = (int)((nDate >> 9) & 0x7F) + 80;` |
|  871 |  345 | `	sTm.tm_mon  = (int)((nDate >> 5) & 0x0F) - 1;` |
|  871 |  346 | `	sTm.tm_mday = (int)(nDate & 0x1F);` |
|  871 |  347 | `	sTm.tm_hour = (int)((nTime >> 11) & 0x1F);` |
|  871 |  348 | `	sTm.tm_min  = (int)((nTime >> 5) & 0x3F);` |
|  871 |  349 | `	sTm.tm_sec  = (int)((nTime & 0x1F) * 2);` |
|  871 |  350 | `	sTm.tm_isdst = -1;` |
|  871 |  351 | `	t = mktime(&sTm);` |
|  871 |  352 | `	return (sxi64)t;` |
|    3 |  353 | `}` |
|    - |  354 | `/* Does this name need the format's UTF-8 flag? libzip sets bit 11 exactly when` |
|    - |  355 | ` * the name is not plain ASCII, and nothing else decides it. */` |
|  104 |  356 | `static int ZipNameIsUtf8(const char *z,sxu32 n)` |
|    3 |  357 | `{` |
|    - |  358 | `	sxu32 i;` |
|  765 |  359 | `	for( i = 0 ; i < n ; ++i ){` |
|  661 |  360 | `		if( (unsigned char)z[i] > 0x7F ){` |
|  ! 0 |  361 | `			return 1;` |
|    - |  362 | `		}` |
|  332 |  363 | `	}` |
|  107 |  364 | `	return 0;` |
|   55 |  365 | `}` |
|    - |  366 | `/* ------------------------------------------------------------------ */` |
|    - |  367 | `/* Entries                                                             */` |
|    - |  368 | `/* ------------------------------------------------------------------ */` |
|  954 |  369 | `static void ZipEntFree(ph7_vm *pVm,phl_zip_ent *pEnt)` |
|    3 |  370 | `{` |
|  957 |  371 | `	SyBlobRelease(&pEnt->sName);` |
|  957 |  372 | `	SyBlobRelease(&pEnt->sOrigName);` |
|  957 |  373 | `	SyBlobRelease(&pEnt->sComment);` |
|  957 |  374 | `	SyBlobRelease(&pEnt->sOrigComment);` |
|  957 |  375 | `	SyBlobRelease(&pEnt->sData);` |
|  957 |  376 | `	SyBlobRelease(&pEnt->sSrcPath);` |
|  957 |  377 | `	SyBlobRelease(&pEnt->sPassword);` |
|  957 |  378 | `	SyBlobRelease(&pEnt->sLoadedPw);` |
|  957 |  379 | `	SyMemBackendFree(&pVm->sAllocator,pEnt);` |
|  957 |  380 | `}` |
|    - |  381 | `/* Room for one more index in the table. */` |
|  954 |  382 | `static int ZipGrow(phl_zip *pZip)` |
|    3 |  383 | `{` |
|    - |  384 | `	sxu32 nWant;` |
|    - |  385 | `	phl_zip_ent **apNew;` |
|  957 |  386 | `	if( pZip->nEnt < pZip->nAlloc ){` |
|  611 |  387 | `		return 0;` |
|    - |  388 | `	}` |
|  349 |  389 | `	nWant = pZip->nAlloc < 8 ? 8 : pZip->nAlloc * 2;` |
|  522 |  390 | `	apNew = (phl_zip_ent **)SyMemBackendAlloc(&pZip->pVm->sAllocator,` |
|  173 |  391 | `		nWant * (sxu32)sizeof(phl_zip_ent *));` |
|  349 |  392 | `	if( apNew == 0 ){` |
|  ! 0 |  393 | `		return -1;` |
|    - |  394 | `	}` |
|  349 |  395 | `	if( pZip->nEnt > 0 ){` |
|  ! 0 |  396 | `		SyMemcpy(pZip->apEnt,apNew,pZip->nEnt * (sxu32)sizeof(phl_zip_ent *));` |
|  ! 0 |  397 | `	}` |
|  349 |  398 | `	if( pZip->apEnt ){` |
|  ! 0 |  399 | `		SyMemBackendFree(&pZip->pVm->sAllocator,pZip->apEnt);` |
|  ! 0 |  400 | `	}` |
|  349 |  401 | `	pZip->apEnt = apNew;` |
|  349 |  402 | `	pZip->nAlloc = nWant;` |
|  349 |  403 | `	return 0;` |
|  480 |  404 | `}` |
|    - |  405 | `/* A blank entry, appended to the table. Its index is its position, and it keeps` |
|    - |  406 | ` * that position for the archive's whole life -- a delete blanks it in place. */` |
|  954 |  407 | `static phl_zip_ent * ZipEntNew(phl_zip *pZip,const char *zName,int nName)` |
|    3 |  408 | `{` |
|    - |  409 | `	phl_zip_ent *pEnt;` |
|  957 |  410 | `	if( ZipGrow(pZip) != 0 ){` |
|  ! 0 |  411 | `		return 0;` |
|    - |  412 | `	}` |
|  957 |  413 | `	pEnt = (phl_zip_ent *)SyMemBackendAlloc(&pZip->pVm->sAllocator,sizeof(phl_zip_ent));` |
|  957 |  414 | `	if( pEnt == 0 ){` |
|  ! 0 |  415 | `		return 0;` |
|    - |  416 | `	}` |
|  957 |  417 | `	SyZero(pEnt,sizeof(*pEnt));` |
|  957 |  418 | `	SyBlobInit(&pEnt->sName,&pZip->pVm->sAllocator);` |
|  957 |  419 | `	SyBlobInit(&pEnt->sOrigName,&pZip->pVm->sAllocator);` |
|  957 |  420 | `	SyBlobInit(&pEnt->sComment,&pZip->pVm->sAllocator);` |
|  957 |  421 | `	SyBlobInit(&pEnt->sOrigComment,&pZip->pVm->sAllocator);` |
|  957 |  422 | `	SyBlobInit(&pEnt->sData,&pZip->pVm->sAllocator);` |
|  957 |  423 | `	SyBlobInit(&pEnt->sSrcPath,&pZip->pVm->sAllocator);` |
|  957 |  424 | `	SyBlobInit(&pEnt->sPassword,&pZip->pVm->sAllocator);` |
|  957 |  425 | `	SyBlobInit(&pEnt->sLoadedPw,&pZip->pVm->sAllocator);` |
|  957 |  426 | `	pEnt->iSrcLen = -1;` |
|  957 |  427 | `	pEnt->iSetMethod = ZIP_CM_DEFAULT;` |
|  957 |  428 | `	pEnt->iSetEncrypt = -1;` |
|  957 |  429 | `	pEnt->iOffset = -1;` |
|  957 |  430 | `	if( nName > 0 ){` |
|  953 |  431 | `		SyBlobAppend(&pEnt->sName,zName,(sxu32)nName);` |
|  475 |  432 | `	}` |
|  957 |  433 | `	pZip->apEnt[pZip->nEnt++] = pEnt;` |
|  957 |  434 | `	return pEnt;` |
|  480 |  435 | `}` |
|    - |  436 | `/* The name a read answers, honouring FL_UNCHANGED. */` |
| 1158 |  437 | `static const char * ZipEntName(phl_zip_ent *pEnt,int iFlags,sxu32 *pnName)` |
|    3 |  438 | `{` |
| 1161 |  439 | `	SyBlob *pB = (iFlags & ZIP_FL_UNCHANGED) ? &pEnt->sOrigName : &pEnt->sName;` |
| 1161 |  440 | `	if( (iFlags & ZIP_FL_UNCHANGED) && pEnt->bNew ){` |
|    5 |  441 | `		*pnName = 0;` |
|    5 |  442 | `		return 0;` |
|    - |  443 | `	}` |
| 1157 |  444 | `	*pnName = SyBlobLength(pB);` |
| 1157 |  445 | `	return (const char *)SyBlobData(pB);` |
|  582 |  446 | `}` |
|    - |  447 | `/*` |
|    - |  448 | ` * The entry a NAME resolves to, with php's two lookup flags: FL_NOCASE compares` |
|    - |  449 | ` * case-insensitively, FL_NODIR ignores the directory part of the stored name.` |
|    - |  450 | ` * A deleted entry is not found -- its index is still there and nothing answers` |
|    - |  451 | `` * to it, which is what makes `deleteName()` visible before `close()`.`` |
|    - |  452 | ` */` |
|  290 |  453 | `static phl_zip_ent * ZipFind(phl_zip *pZip,const char *zName,sxu32 nName,int iFlags,sxu32 *pnIdx)` |
|    3 |  454 | `{` |
|    - |  455 | `	sxu32 i;` |
|  525 |  456 | `	for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|  423 |  457 | `		phl_zip_ent *pEnt = pZip->apEnt[i];` |
|    - |  458 | `		const char *z;` |
|  423 |  459 | `		sxu32 n,nOff = 0;` |
|  423 |  460 | `		if( pEnt->bDeleted \|\| pEnt->bNameHidden ){` |
|  121 |  461 | `			continue;` |
|    - |  462 | `		}` |
|  415 |  463 | `		z = ZipEntName(pEnt,iFlags,&n);` |
|  415 |  464 | `		if( z == 0 ){` |
|  ! 0 |  465 | `			continue;` |
|    - |  466 | `		}` |
|  415 |  467 | `		if( iFlags & ZIP_FL_NODIR ){` |
|    - |  468 | `			sxu32 k;` |
|   55 |  469 | `			for( k = 0 ; k < n ; ++k ){` |
|   49 |  470 | `				if( z[k] == '/' ){` |
|    5 |  471 | `					nOff = k + 1;` |
|    2 |  472 | `				}` |
|   25 |  473 | `			}` |
|    3 |  474 | `		}` |
|  415 |  475 | `		if( n - nOff != nName ){` |
|  183 |  476 | `			continue;` |
|    - |  477 | `		}` |
|  235 |  478 | `		if( iFlags & ZIP_FL_NOCASE ){` |
|    3 |  479 | `			if( SyStrnicmp(&z[nOff],zName,nName) != 0 ){` |
|  ! 0 |  480 | `				continue;` |
|    1 |  481 | `			}` |
|  234 |  482 | `		}else if( nName > 0 && SyMemcmp(&z[nOff],zName,nName) != 0 ){` |
|   47 |  483 | `			continue;` |
|    - |  484 | `		}` |
|  190 |  485 | `		if( pnIdx ){` |
|   13 |  486 | `			*pnIdx = i;` |
|    6 |  487 | `		}` |
|  190 |  488 | `		return pEnt;` |
|  ! 0 |  489 | `	}` |
|  105 |  490 | `	return 0;` |
|  148 |  491 | `}` |
|    - |  492 | `/* The entry at an index, or 0 when the index names nothing a read can see. */` |
| 1458 |  493 | `static phl_zip_ent * ZipAt(phl_zip *pZip,sxi64 iIdx)` |
|    2 |  494 | `{` |
|    - |  495 | `	phl_zip_ent *pEnt;` |
| 1460 |  496 | `	if( iIdx < 0 \|\| (sxu64)iIdx >= (sxu64)pZip->nEnt ){` |
|   10 |  497 | `		return 0;` |
|    - |  498 | `	}` |
| 1452 |  499 | `	pEnt = pZip->apEnt[(sxu32)iIdx];` |
| 1452 |  500 | `	return pEnt->bDeleted ? 0 : pEnt;` |
|  731 |  501 | `}` |
|    - |  502 | `/* ------------------------------------------------------------------ */` |
|    - |  503 | `/* Reading an archive                                                  */` |
|    - |  504 | `/* ------------------------------------------------------------------ */` |
|    - |  505 | `/*` |
|    - |  506 | ` * The ZIP64 extra field of one central-directory record. The three big numbers` |
|    - |  507 | ` * are present only when their 32-bit slot is saturated, and IN THAT ORDER, so` |
|    - |  508 | ` * the field cannot be parsed without knowing which ones were.` |
|    - |  509 | ` */` |
|   32 |  510 | `static void ZipReadZip64Extra(const unsigned char *zExtra,sxu32 nExtra,` |
|    - |  511 | `	sxu64 *pnUsz,sxu64 *pnCsz,sxu64 *pnOff)` |
|    1 |  512 | `{` |
|   33 |  513 | `	sxu32 q = 0;` |
|   65 |  514 | `	while( q + 4 <= nExtra ){` |
|   33 |  515 | `		sxu32 nId = ZipGet16(&zExtra[q]);` |
|   33 |  516 | `		sxu32 nLen = ZipGet16(&zExtra[q+2]);` |
|   33 |  517 | `		if( q + 4 + nLen > nExtra ){` |
|  ! 0 |  518 | `			return;` |
|    - |  519 | `		}` |
|   33 |  520 | `		if( nId == 0x0001 ){` |
|  ! 0 |  521 | `			sxu32 p = q + 4;` |
|  ! 0 |  522 | `			if( *pnUsz == 0xFFFFFFFFu && p + 8 <= q + 4 + nLen ){` |
|  ! 0 |  523 | `				*pnUsz = ZipGet64(&zExtra[p]);` |
|  ! 0 |  524 | `				p += 8;` |
|  ! 0 |  525 | `			}` |
|  ! 0 |  526 | `			if( *pnCsz == 0xFFFFFFFFu && p + 8 <= q + 4 + nLen ){` |
|  ! 0 |  527 | `				*pnCsz = ZipGet64(&zExtra[p]);` |
|  ! 0 |  528 | `				p += 8;` |
|  ! 0 |  529 | `			}` |
|  ! 0 |  530 | `			if( *pnOff == 0xFFFFFFFFu && p + 8 <= q + 4 + nLen ){` |
|  ! 0 |  531 | `				*pnOff = ZipGet64(&zExtra[p]);` |
|  ! 0 |  532 | `			}` |
|  ! 0 |  533 | `			return;` |
|    - |  534 | `		}` |
|   33 |  535 | `		q += 4 + nLen;` |
|    1 |  536 | `	}` |
|   17 |  537 | `}` |
|    - |  538 | `/*` |
|    - |  539 | ` * The WinZip AES extra field (0x9901) of one record: a vendor version, the two` |
|    - |  540 | `` * letters `AE`, the key STRENGTH (1/2/3 for 128/192/256) and the compression`` |
|    - |  541 | ` * method the encrypted bytes are really under.` |
|    - |  542 | ` */` |
|   32 |  543 | `static void ZipReadAesExtra(const unsigned char *zExtra,sxu32 nExtra,int *piStrength,int *piMethod)` |
|    1 |  544 | `{` |
|   33 |  545 | `	sxu32 q = 0;` |
|   33 |  546 | `	while( q + 4 <= nExtra ){` |
|   33 |  547 | `		sxu32 nId = ZipGet16(&zExtra[q]);` |
|   33 |  548 | `		sxu32 nLen = ZipGet16(&zExtra[q+2]);` |
|   33 |  549 | `		if( q + 4 + nLen > nExtra ){` |
|  ! 0 |  550 | `			return;` |
|    - |  551 | `		}` |
|   33 |  552 | `		if( nId == ZIP_EXTRA_AES && nLen >= 7 ){` |
|   33 |  553 | `			*piStrength = (int)zExtra[q+4+4];` |
|   33 |  554 | `			*piMethod = (int)ZipGet16(&zExtra[q+4+5]);` |
|   33 |  555 | `			return;` |
|    - |  556 | `		}` |
|  ! 0 |  557 | `		q += 4 + nLen;` |
|  ! 0 |  558 | `	}` |
|   17 |  559 | `}` |
|    - |  560 | `/*` |
|    - |  561 | `` * Read the whole archive out of `pZip->sFile`, from its CENTRAL DIRECTORY --`` |
|    - |  562 | ` * the only authoritative part of the format, which is why an archive with a` |
|    - |  563 | ` * program bolted on the front (a phar, a self-extractor) still reads.` |
|    - |  564 | ` *` |
|    - |  565 | ` * Answers a ZIP_ER_ code: NOZIP for anything that is not one, INCONS for a` |
|    - |  566 | ` * directory that contradicts itself.` |
|    - |  567 | ` */` |
|  332 |  568 | `static int ZipParse(phl_zip *pZip)` |
|    3 |  569 | `{` |
|  335 |  570 | `	const unsigned char *zFile = (const unsigned char *)SyBlobData(&pZip->sFile);` |
|  335 |  571 | `	sxu32 nFile = SyBlobLength(&pZip->sFile);` |
|    - |  572 | `	sxu32 nEocd,i;` |
|    - |  573 | `	sxu64 nEntries,nCdOff,nCdSize;` |
|    - |  574 | `	sxu64 q;` |
|  335 |  575 | `	int bFound = 0;` |
|  335 |  576 | `	if( nFile < 1 ){` |
|    - |  577 | `		/* An EMPTY file is not a zip and php opens it anyway, with a` |
|    - |  578 | ``		 * deprecation the caller raises: `Using empty file as ZipArchive is`` |
|    - |  579 | ``		 * deprecated`. It reads as an archive with no entries. */`` |
|    3 |  580 | `		return ZIP_ER_EMPTY_FILE;` |
|    - |  581 | `	}` |
|  333 |  582 | `	if( nFile < 22 ){` |
|  ! 0 |  583 | `		return ZIP_ER_NOZIP;` |
|    - |  584 | `	}` |
|    - |  585 | `	/* The end record is last, but a trailing COMMENT of up to 64K may follow` |
|    - |  586 | `	 * it, so it is found by scanning backwards for the signature. */` |
| 2457 |  587 | `	for( nEocd = nFile - 22 ; ; --nEocd ){` |
| 2454 |  588 | `		if( zFile[nEocd] == 'P' && zFile[nEocd+1] == 'K'` |
|  327 |  589 | `		 && zFile[nEocd+2] == 5 && zFile[nEocd+3] == 6 ){` |
|  327 |  590 | `			bFound = 1;` |
|  327 |  591 | `			break;` |
|    - |  592 | `		}` |
| 2132 |  593 | `		if( nEocd == 0 \|\| nFile - nEocd > 66000 ){` |
|    5 |  594 | `			break;` |
|    - |  595 | `		}` |
| 1064 |  596 | `	}` |
|  333 |  597 | `	if( !bFound ){` |
|    8 |  598 | `		return ZIP_ER_NOZIP;` |
|    - |  599 | `	}` |
|  327 |  600 | `	if( ZipGet16(&zFile[nEocd+4]) != 0 \|\| ZipGet16(&zFile[nEocd+6]) != 0 ){` |
|    - |  601 | `		/* A member of a multi-part set: php refuses the whole archive. */` |
|    5 |  602 | `		return ZIP_ER_MULTIDISK;` |
|    - |  603 | `	}` |
|  323 |  604 | `	nEntries = ZipGet16(&zFile[nEocd+10]);` |
|  323 |  605 | `	nCdSize = ZipGet32(&zFile[nEocd+12]);` |
|  323 |  606 | `	nCdOff = ZipGet32(&zFile[nEocd+16]);` |
|    - |  607 | `	{` |
|  323 |  608 | `		sxu32 nCommentLen = ZipGet16(&zFile[nEocd+20]);` |
|  323 |  609 | `		if( nCommentLen > 0 && nCommentLen <= nFile - nEocd - 22 ){` |
|  273 |  610 | `			SyBlobAppend(&pZip->sOrigComment,&zFile[nEocd+22],nCommentLen);` |
|  273 |  611 | `			SyBlobAppend(&pZip->sComment,&zFile[nEocd+22],nCommentLen);` |
|  136 |  612 | `		}` |
|    - |  613 | `	}` |
|    - |  614 | `	/* ZIP64: the locator sits immediately before the end record and points at a` |
|    - |  615 | `	 * second end record carrying the counts that did not fit in the first. */` |
|  323 |  616 | `	if( nEocd >= 20 && SyMemcmp(&zFile[nEocd-20],"PK\006\007",4) == 0 ){` |
|  ! 0 |  617 | `		sxu64 iEocd64 = ZipGet64(&zFile[nEocd-20+8]);` |
|  ! 0 |  618 | `		if( iEocd64 + 56 <= (sxu64)nFile` |
|  ! 0 |  619 | `		 && SyMemcmp(&zFile[(sxu32)iEocd64],"PK\006\006",4) == 0 ){` |
|  ! 0 |  620 | `			nEntries = ZipGet64(&zFile[(sxu32)iEocd64+32]);` |
|  ! 0 |  621 | `			nCdSize = ZipGet64(&zFile[(sxu32)iEocd64+40]);` |
|  ! 0 |  622 | `			nCdOff = ZipGet64(&zFile[(sxu32)iEocd64+48]);` |
|  ! 0 |  623 | `		}` |
|  ! 0 |  624 | `	}` |
|    - |  625 | `	/* The directory has to FIT, in front of the end record that describes it.` |
|    - |  626 | ``	 * A size or an offset that does not is php's `Zip archive inconsistent`;`` |
|    - |  627 | `	 * bytes that are not a directory where one should be are the different` |
|    - |  628 | `	 * complaint below. */` |
|  320 |  629 | `	if( nCdOff > (sxu64)nFile \|\| nCdSize > (sxu64)nFile` |
|  319 |  630 | `	 \|\| nCdOff + nCdSize > (sxu64)nEocd ){` |
|    5 |  631 | `		return ZIP_ER_INCONS;` |
|    - |  632 | `	}` |
|  319 |  633 | `	q = nCdOff;` |
| 1187 |  634 | `	for( i = 0 ; i < (sxu32)nEntries ; ++i ){` |
|    - |  635 | `		sxu32 nNameLen,nExtraLen,nCommentLen,nFlag,nMethod,nCrc,nMade;` |
|    - |  636 | `		sxu64 nCsz,nUsz,nLocal;` |
|    - |  637 | `		phl_zip_ent *pEnt;` |
|  909 |  638 | `		if( q + 46 > nCdOff + nCdSize \|\| SyMemcmp(&zFile[(sxu32)q],"PK\001\002",4) != 0 ){` |
|    - |  639 | `			/* Not a directory record where the end record said one would be.` |
|    - |  640 | `			 * That is what an archive with a PREFIX in front of it looks like` |
|    - |  641 | `			 * -- a self-extracting stub leaves every recorded offset short --` |
|    - |  642 | ``			 * and what php answers to one: `Not a zip archive`. */`` |
|   34 |  643 | `			return ZIP_ER_NOZIP;` |
|    - |  644 | `		}` |
|  881 |  645 | `		nMade = ZipGet16(&zFile[(sxu32)q+4]);` |
|  881 |  646 | `		nFlag = ZipGet16(&zFile[(sxu32)q+8]);` |
|  881 |  647 | `		nMethod = ZipGet16(&zFile[(sxu32)q+10]);` |
|  881 |  648 | `		nCrc = ZipGet32(&zFile[(sxu32)q+16]);` |
|  881 |  649 | `		nCsz = ZipGet32(&zFile[(sxu32)q+20]);` |
|  881 |  650 | `		nUsz = ZipGet32(&zFile[(sxu32)q+24]);` |
|  881 |  651 | `		nNameLen = ZipGet16(&zFile[(sxu32)q+28]);` |
|  881 |  652 | `		nExtraLen = ZipGet16(&zFile[(sxu32)q+30]);` |
|  881 |  653 | `		nCommentLen = ZipGet16(&zFile[(sxu32)q+32]);` |
|  881 |  654 | `		nLocal = ZipGet32(&zFile[(sxu32)q+42]);` |
|  881 |  655 | `		if( q + 46 + nNameLen + nExtraLen + nCommentLen > nCdOff + nCdSize ){` |
|   11 |  656 | `			return ZIP_ER_INCONS;` |
|    - |  657 | `		}` |
|  871 |  658 | `		if( nExtraLen > 0 ){` |
|   33 |  659 | `			ZipReadZip64Extra(&zFile[(sxu32)q+46+nNameLen],nExtraLen,&nUsz,&nCsz,&nLocal);` |
|   16 |  660 | `		}` |
|  871 |  661 | `		pEnt = ZipEntNew(pZip,(const char *)&zFile[(sxu32)q+46],(int)nNameLen);` |
|  871 |  662 | `		if( pEnt == 0 ){` |
|  ! 0 |  663 | `			return ZIP_ER_MEMORY;` |
|    - |  664 | `		}` |
|  871 |  665 | `		SyBlobAppend(&pEnt->sOrigName,(const char *)&zFile[(sxu32)q+46],nNameLen);` |
|  871 |  666 | `		if( nCommentLen > 0 ){` |
|  261 |  667 | `			const char *zC = (const char *)&zFile[(sxu32)q+46+nNameLen+nExtraLen];` |
|  261 |  668 | `			SyBlobAppend(&pEnt->sComment,zC,nCommentLen);` |
|  261 |  669 | `			SyBlobAppend(&pEnt->sOrigComment,zC,nCommentLen);` |
|  130 |  670 | `		}` |
|  871 |  671 | `		pEnt->nSize = (sxu32)nUsz;` |
|  871 |  672 | `		pEnt->nCompSize = (sxu32)nCsz;` |
|  871 |  673 | `		pEnt->nCrc = nCrc;` |
|  871 |  674 | `		pEnt->iMethod = (int)nMethod;` |
|  871 |  675 | `		pEnt->iRawMethod = (int)nMethod;` |
|  871 |  676 | `		pEnt->nFlag = nFlag;` |
|  871 |  677 | `		pEnt->nDosTime = ZipGet16(&zFile[(sxu32)q+12]);` |
|  871 |  678 | `		pEnt->iTime = pEnt->iOrigTime =` |
|  868 |  679 | `			ZipDosToUnix(pEnt->nDosTime,ZipGet16(&zFile[(sxu32)q+14]));` |
|  871 |  680 | `		pEnt->nAttr = pEnt->nOrigAttr = ZipGet32(&zFile[(sxu32)q+38]);` |
|  871 |  681 | `		pEnt->iOpsys = pEnt->iOrigOpsys = (int)((nMade >> 8) & 0xFF);` |
|  871 |  682 | `		pEnt->iOffset = (sxi64)nLocal;` |
|    - |  683 | `		/* The encryption bit says only THAT the entry is encrypted; which of` |
|    - |  684 | `		 * php's four methods it is lives in the AES extra field, or is the` |
|    - |  685 | `		 * traditional one when there is none. */` |
|  871 |  686 | `		if( nFlag & 1 ){` |
|   48 |  687 | `			pEnt->iEncMethod = ZIP_EM_TRAD_PKWARE;` |
|   48 |  688 | `			if( nMethod == ZIP_CM_WINZIP_AES ){` |
|    - |  689 | `				/* The method byte is a placeholder: the REAL compression method` |
|    - |  690 | `				 * and the key length live in the 0x9901 extra field, and php` |
|    - |  691 | ``				 * answers both from there -- `comp_method` is 0 or 8 for an AES`` |
|    - |  692 | `				 * member, never 99. */` |
|   33 |  693 | `				int iStrength = 0,iReal = -1;` |
|   33 |  694 | `				ZipReadAesExtra(&zFile[(sxu32)q+46+nNameLen],nExtraLen,&iStrength,&iReal);` |
|   33 |  695 | `				if( iStrength >= 1 && iStrength <= 3 ){` |
|   33 |  696 | `					pEnt->iEncMethod = ZIP_EM_AES_128 + (iStrength - 1);` |
|   33 |  697 | `					pEnt->iMethod = iReal;` |
|   17 |  698 | `				}else{` |
|  ! 0 |  699 | `					pEnt->iEncMethod = ZIP_EM_UNKNOWN;` |
|    - |  700 | `				}` |
|   16 |  701 | `			}` |
|   23 |  702 | `		}` |
|  871 |  703 | `		if( SyBlobLength(&pEnt->sName) > 0 ){` |
|  867 |  704 | `			char *zN = (char *)SyBlobData(&pEnt->sName);` |
|  867 |  705 | `			if( zN[SyBlobLength(&pEnt->sName)-1] == '/' ){` |
|  262 |  706 | `				pEnt->bDir = 1;` |
|  130 |  707 | `			}` |
|  432 |  708 | `		}` |
|  871 |  709 | `		q += 46 + nNameLen + nExtraLen + nCommentLen;` |
|  437 |  710 | `	}` |
|  281 |  711 | `	if( q != nCdOff + nCdSize ){` |
|    - |  712 | `		/* The entry COUNT and the directory SIZE disagree -- one record too` |
|    - |  713 | `		 * many or one too few. php's answer is the same as a missing record's. */` |
|  ! 0 |  714 | `		return ZIP_ER_NOZIP;` |
|    - |  715 | `	}` |
|  281 |  716 | `	return ZIP_ER_OK;` |
|  169 |  717 | `}` |
|    - |  718 |  |
|    - |  719 | `/* ------------------------------------------------------------------ */` |
|    - |  720 | `/* The two ciphers a zip member can be under                           */` |
|    - |  721 | `/* ------------------------------------------------------------------ */` |
|    - |  722 | `/*` |
|    - |  723 | ` * PKWARE's TRADITIONAL cipher, the one every zip tool has spoken since 1990.` |
|    - |  724 | ` * Three 32-bit keys stirred by the password and then by each plaintext byte, a` |
|    - |  725 | ` * keystream byte read off the third, and a twelve-byte random header in front` |
|    - |  726 | ` * of the data whose LAST byte is a check value the reader can test a password` |
|    - |  727 | ` * against without decrypting anything else.` |
|    - |  728 | ` *` |
|    - |  729 | ` * The stirring uses the raw CRC-32 table update -- no pre- or post-complement --` |
|    - |  730 | `` * which zlib's own `crc32()` can be talked into: it computes the complemented`` |
|    - |  731 | ` * form, so complementing both ends gives the raw one back and no second table` |
|    - |  732 | ` * has to live here.` |
|    - |  733 | ` */` |
|    - |  734 | `typedef struct phl_zip_keys phl_zip_keys;` |
|    - |  735 | `struct phl_zip_keys { sxu32 k[3]; };` |
|  980 |  736 | `static sxu32 ZipCrcRaw(sxu32 iCrc,unsigned char b)` |
|    1 |  737 | `{` |
|  981 |  738 | `	return (sxu32)~crc32((uLong)(~iCrc) & 0xFFFFFFFFu,(const Bytef *)&b,1);` |
|    1 |  739 | `}` |
|  490 |  740 | `static void ZipTradUpdate(phl_zip_keys *pK,unsigned char b)` |
|    1 |  741 | `{` |
|  491 |  742 | `	pK->k[0] = ZipCrcRaw(pK->k[0],b);` |
|  491 |  743 | `	pK->k[1] = (pK->k[1] + (pK->k[0] & 0xFF)) & 0xFFFFFFFFu;` |
|  491 |  744 | `	pK->k[1] = ((pK->k[1] * 134775813u) + 1u) & 0xFFFFFFFFu;` |
|  491 |  745 | `	pK->k[2] = ZipCrcRaw(pK->k[2],(unsigned char)((pK->k[1] >> 24) & 0xFF));` |
|  491 |  746 | `}` |
|   22 |  747 | `static void ZipTradInit(phl_zip_keys *pK,const char *zPw,sxu32 nPw)` |
|    1 |  748 | `{` |
|    - |  749 | `	sxu32 i;` |
|   23 |  750 | `	pK->k[0] = 0x12345678u;` |
|   23 |  751 | `	pK->k[1] = 0x23456789u;` |
|   23 |  752 | `	pK->k[2] = 0x34567890u;` |
|  131 |  753 | `	for( i = 0 ; i < nPw ; ++i ){` |
|  109 |  754 | `		ZipTradUpdate(pK,(unsigned char)zPw[i]);` |
|   55 |  755 | `	}` |
|   23 |  756 | `}` |
|  382 |  757 | `static unsigned char ZipTradStream(phl_zip_keys *pK)` |
|    1 |  758 | `{` |
|  383 |  759 | `	sxu32 t = (pK->k[2] \| 2u) & 0xFFFFu;` |
|  383 |  760 | `	return (unsigned char)(((t * (t ^ 1u)) >> 8) & 0xFF);` |
|    1 |  761 | `}` |
|  264 |  762 | `static unsigned char ZipTradDecrypt(phl_zip_keys *pK,unsigned char c)` |
|    1 |  763 | `{` |
|  265 |  764 | `	unsigned char p = (unsigned char)(c ^ ZipTradStream(pK));` |
|  265 |  765 | `	ZipTradUpdate(pK,p);` |
|  265 |  766 | `	return p;` |
|    1 |  767 | `}` |
|  118 |  768 | `static unsigned char ZipTradEncrypt(phl_zip_keys *pK,unsigned char p)` |
|    1 |  769 | `{` |
|  119 |  770 | `	unsigned char c = (unsigned char)(p ^ ZipTradStream(pK));` |
|  119 |  771 | `	ZipTradUpdate(pK,p);` |
|  119 |  772 | `	return c;` |
|    1 |  773 | `}` |
|    - |  774 | `#ifdef PH7_ENABLE_OPENSSL` |
|    - |  775 | `/*` |
|    - |  776 | ` * WinZip AES, the other one php can write. A salt of half the key length, a` |
|    - |  777 | ` * PBKDF2-HMAC-SHA1 over a THOUSAND rounds producing an encryption key, an` |
|    - |  778 | ` * authentication key and two PASSWORD VERIFICATION bytes in that order, the` |
|    - |  779 | ` * data under AES in counter mode, and a ten-byte HMAC-SHA1 of the CIPHERTEXT` |
|    - |  780 | ` * behind it.` |
|    - |  781 | ` *` |
|    - |  782 | ` * The counter is the part a general AES-CTR cannot be used for: WinZip counts` |
|    - |  783 | ` * in LITTLE-endian from 1 in the low four bytes of an otherwise zero block,` |
|    - |  784 | ` * where every CTR implementation counts big-endian over the whole block. So the` |
|    - |  785 | ` * keystream is made a block at a time out of raw ECB.` |
|    - |  786 | ` */` |
|    - |  787 | `#define ZIP_AES_MAXKEY 32` |
|   68 |  788 | `static int ZipAesKeyLen(int iMethod)` |
|    1 |  789 | `{` |
|   69 |  790 | `	switch( iMethod ){` |
|   19 |  791 | `	case ZIP_EM_AES_128: return 16;` |
|   23 |  792 | `	case ZIP_EM_AES_192: return 24;` |
|   29 |  793 | `	case ZIP_EM_AES_256: return 32;` |
|  ! 0 |  794 | `	default: break;` |
|    - |  795 | `	}` |
|  ! 0 |  796 | `	return 0;` |
|   35 |  797 | `}` |
|   42 |  798 | `static const EVP_CIPHER * ZipAesEcb(int nKey)` |
|    1 |  799 | `{` |
|   43 |  800 | `	return nKey == 16 ? EVP_aes_128_ecb() : (nKey == 24 ? EVP_aes_192_ecb() : EVP_aes_256_ecb());` |
|    1 |  801 | `}` |
|    - |  802 | `/* The derived material: [key][auth key][2 verification bytes]. */` |
|   68 |  803 | `static int ZipAesDerive(const char *zPw,sxu32 nPw,const unsigned char *zSalt,int nSalt,` |
|    - |  804 | `	int nKey,unsigned char *zOut)` |
|    1 |  805 | `{` |
|   69 |  806 | `	return PKCS5_PBKDF2_HMAC_SHA1(zPw,(int)nPw,zSalt,nSalt,1000,nKey * 2 + 2,zOut) == 1 ? 0 : -1;` |
|    1 |  807 | `}` |
|    - |  808 | `/* One pass of the counter-mode keystream over a buffer, in place. Encryption` |
|    - |  809 | ` * and decryption are the same operation. */` |
|   42 |  810 | `static int ZipAesCtr(const unsigned char *zKey,int nKey,unsigned char *zData,sxu32 nData)` |
|    1 |  811 | `{` |
|   43 |  812 | `	EVP_CIPHER_CTX *pCtx = EVP_CIPHER_CTX_new();` |
|   43 |  813 | `	sxu32 nDone = 0;` |
|   43 |  814 | `	sxu32 iBlock = 1;` |
|   43 |  815 | `	int rc = 0;` |
|   43 |  816 | `	if( pCtx == 0 ){` |
|  ! 0 |  817 | `		return -1;` |
|    - |  818 | `	}` |
|   43 |  819 | `	if( EVP_EncryptInit_ex(pCtx,ZipAesEcb(nKey),0,zKey,0) != 1 ){` |
|  ! 0 |  820 | `		EVP_CIPHER_CTX_free(pCtx);` |
|  ! 0 |  821 | `		return -1;` |
|    - |  822 | `	}` |
|   43 |  823 | `	EVP_CIPHER_CTX_set_padding(pCtx,0);` |
|   85 |  824 | `	while( nDone < nData ){` |
|    - |  825 | `		unsigned char zCtr[16],zKs[32];` |
|   43 |  826 | `		int nKs = 0;` |
|   43 |  827 | `		sxu32 n = nData - nDone;` |
|    - |  828 | `		sxu32 i;` |
|   43 |  829 | `		SyZero(zCtr,sizeof(zCtr));` |
|   43 |  830 | `		zCtr[0] = (unsigned char)(iBlock & 0xFF);` |
|   43 |  831 | `		zCtr[1] = (unsigned char)((iBlock >> 8) & 0xFF);` |
|   43 |  832 | `		zCtr[2] = (unsigned char)((iBlock >> 16) & 0xFF);` |
|   43 |  833 | `		zCtr[3] = (unsigned char)((iBlock >> 24) & 0xFF);` |
|   43 |  834 | `		if( EVP_EncryptUpdate(pCtx,zKs,&nKs,zCtr,(int)sizeof(zCtr)) != 1 \|\| nKs < 16 ){` |
|  ! 0 |  835 | `			rc = -1;` |
|  ! 0 |  836 | `			break;` |
|    - |  837 | `		}` |
|   43 |  838 | `		if( n > 16 ){` |
|  ! 0 |  839 | `			n = 16;` |
|  ! 0 |  840 | `		}` |
|  393 |  841 | `		for( i = 0 ; i < n ; ++i ){` |
|  351 |  842 | `			zData[nDone + i] = (unsigned char)(zData[nDone + i] ^ zKs[i]);` |
|  176 |  843 | `		}` |
|   43 |  844 | `		nDone += n;` |
|   43 |  845 | `		iBlock++;` |
|    1 |  846 | `	}` |
|   43 |  847 | `	EVP_CIPHER_CTX_free(pCtx);` |
|   43 |  848 | `	return rc;` |
|   22 |  849 | `}` |
|    - |  850 | `/* The ten bytes WinZip puts behind the ciphertext. */` |
|   18 |  851 | `static int ZipAesMac(const unsigned char *zKey,int nKey,const unsigned char *zData,sxu32 nData,` |
|    - |  852 | `	unsigned char *zOut)` |
|    1 |  853 | `{` |
|    - |  854 | `	unsigned char zMd[EVP_MAX_MD_SIZE];` |
|   19 |  855 | `	unsigned int nMd = 0;` |
|   19 |  856 | `	if( HMAC(EVP_sha1(),zKey,nKey,zData,nData,zMd,&nMd) == 0 \|\| nMd < 10 ){` |
|  ! 0 |  857 | `		return -1;` |
|    - |  858 | `	}` |
|   19 |  859 | `	SyMemcpy(zMd,zOut,10);` |
|   19 |  860 | `	return 0;` |
|   10 |  861 | `}` |
|    - |  862 | `#endif /* PH7_ENABLE_OPENSSL */` |
|    - |  863 | `/* Is this a method this BUILD can read and write? php answers from its libzip's` |
|    - |  864 | ` * crypto backend, and this answers from whether ext/openssl is in. */` |
|   66 |  865 | `static int ZipEncSupported(int iMethod)` |
|    2 |  866 | `{` |
|   68 |  867 | `	if( iMethod == ZIP_EM_NONE \|\| iMethod == ZIP_EM_TRAD_PKWARE ){` |
|   18 |  868 | `		return 1;` |
|    - |  869 | `	}` |
|    - |  870 | `#ifdef PH7_ENABLE_OPENSSL` |
|   50 |  871 | `	if( iMethod == ZIP_EM_AES_128 \|\| iMethod == ZIP_EM_AES_192` |
|   35 |  872 | `	 \|\| iMethod == ZIP_EM_AES_256 ){` |
|   46 |  873 | `		return 1;` |
|    - |  874 | `	}` |
|    - |  875 | `#endif` |
|    8 |  876 | `	return 0;` |
|   35 |  877 | `}` |
|    - |  878 | `/*` |
|    - |  879 | ` * Where one entry's STORED bytes begin. The local header repeats the name and` |
|    - |  880 | ` * carries an extra field of its own, so the payload offset is only knowable` |
|    - |  881 | ` * from it -- the central directory records where the HEADER is, not the data.` |
|    - |  882 | ` */` |
|  574 |  883 | `static int ZipEntPayload(phl_zip *pZip,phl_zip_ent *pEnt,const unsigned char **pz,sxu32 *pn)` |
|    3 |  884 | `{` |
|  577 |  885 | `	const unsigned char *zFile = (const unsigned char *)SyBlobData(&pZip->sFile);` |
|  577 |  886 | `	sxu32 nFile = SyBlobLength(&pZip->sFile);` |
|    - |  887 | `	sxu64 iOff;` |
|  577 |  888 | `	if( pEnt->iOffset < 0 \|\| (sxu64)pEnt->iOffset + 30 > (sxu64)nFile ){` |
|    5 |  889 | `		return ZIP_ER_NOZIP;` |
|    - |  890 | `	}` |
|    - |  891 | `	/* The local header's SIGNATURE is deliberately not checked. libzip reads the` |
|    - |  892 | `	 * two lengths out of it without one unless CHECKCONS asked, so an archive` |
|    - |  893 | `	 * whose local signature was damaged still reads every entry under php -- and` |
|    - |  894 | `	 * the bounds below are what actually keeps this safe. */` |
|  858 |  895 | `	iOff = (sxu64)pEnt->iOffset + 30 + ZipGet16(&zFile[pEnt->iOffset+26])` |
|  570 |  896 | `		+ ZipGet16(&zFile[pEnt->iOffset+28]);` |
|  573 |  897 | `	if( iOff > (sxu64)nFile ){` |
|    5 |  898 | `		return ZIP_ER_EOF;` |
|    - |  899 | `	}` |
|    - |  900 | `	/* The recorded length is CLAMPED to what the file holds rather than` |
|    - |  901 | `	 * trusted: a directory that claims more bytes than there are is read as` |
|    - |  902 | `	 * far as it goes, which is what php answers for one. */` |
|  569 |  903 | `	*pn = pEnt->nCompSize;` |
|  569 |  904 | `	if( iOff + *pn > (sxu64)nFile ){` |
|    9 |  905 | `		*pn = (sxu32)((sxu64)nFile - iOff);` |
|    4 |  906 | `	}` |
|  569 |  907 | `	*pz = &zFile[(sxu32)iOff];` |
|  569 |  908 | `	return ZIP_ER_OK;` |
|  290 |  909 | `}` |
|    - |  910 |  |
|    - |  911 | `/*` |
|    - |  912 | ` * The plaintext of one ENCRYPTED entry, or php's reason it cannot be had.` |
|    - |  913 | ` *` |
|    - |  914 | `` * Two reasons and they are php's own words: `No password provided` when nothing`` |
|    - |  915 | `` * has been set on the archive, and `Wrong password provided` when the check the`` |
|    - |  916 | ` * format carries -- the traditional cipher's twelfth header byte, or WinZip` |
|    - |  917 | ` * AES's two verification bytes -- does not agree with the key the password` |
|    - |  918 | ` * makes. Neither costs a decryption of the whole member, which is what those` |
|    - |  919 | ` * two bytes are FOR.` |
|    - |  920 | ` */` |
|   78 |  921 | `static int ZipDecrypt(phl_zip *pZip,phl_zip_ent *pEnt,const unsigned char *zData,` |
|    - |  922 | `	sxu32 nData,SyBlob *pOut)` |
|    2 |  923 | `{` |
|   80 |  924 | `	const char *zPw = (const char *)SyBlobData(&pZip->sPassword);` |
|   80 |  925 | `	sxu32 nPw = SyBlobLength(&pZip->sPassword);` |
|   80 |  926 | `	if( nPw < 1 ){` |
|   14 |  927 | `		return ZIP_ER_NOPASSWD;` |
|    - |  928 | `	}` |
|   67 |  929 | `	if( pEnt->iEncMethod == ZIP_EM_TRAD_PKWARE ){` |
|    - |  930 | `		phl_zip_keys sKeys;` |
|    - |  931 | `		unsigned char zHdr[12];` |
|    - |  932 | `		unsigned char bWant;` |
|    - |  933 | `		sxu32 i;` |
|   17 |  934 | `		if( nData < 12 ){` |
|  ! 0 |  935 | `			return ZIP_ER_EOF;` |
|    - |  936 | `		}` |
|   17 |  937 | `		ZipTradInit(&sKeys,zPw,nPw);` |
|  209 |  938 | `		for( i = 0 ; i < 12 ; ++i ){` |
|  193 |  939 | `			zHdr[i] = ZipTradDecrypt(&sKeys,zData[i]);` |
|   97 |  940 | `		}` |
|    - |  941 | `		/* The check byte is the CRC's high byte -- unless the entry says its` |
|    - |  942 | `		 * sizes are in a trailing descriptor (flag bit 3), in which case the` |
|    - |  943 | `		 * writer did not know the CRC yet and used the DOS TIME's instead.` |
|    - |  944 | `		 * libzip writes the second form, so both have to be accepted. */` |
|   17 |  945 | `		bWant = (pEnt->nFlag & 8)` |
|   16 |  946 | `			? (unsigned char)((pEnt->nDosTime >> 8) & 0xFF)` |
|  ! 0 |  947 | `			: (unsigned char)((pEnt->nCrc >> 24) & 0xFF);` |
|   17 |  948 | `		if( zHdr[11] != bWant ){` |
|    9 |  949 | `			return ZIP_ER_WRONGPASSWD;` |
|    - |  950 | `		}` |
|   17 |  951 | `		for( i = 12 ; i < nData ; ){` |
|    - |  952 | `			unsigned char zBuf[4096];` |
|    9 |  953 | `			sxu32 n = 0;` |
|   81 |  954 | `			while( i < nData && n < (sxu32)sizeof(zBuf) ){` |
|   73 |  955 | `				zBuf[n++] = ZipTradDecrypt(&sKeys,zData[i++]);` |
|    1 |  956 | `			}` |
|    9 |  957 | `			SyBlobAppend(pOut,zBuf,n);` |
|    1 |  958 | `		}` |
|    9 |  959 | `		return ZIP_ER_OK;` |
|    - |  960 | `	}` |
|    - |  961 | `#ifdef PH7_ENABLE_OPENSSL` |
|    - |  962 | `	{` |
|   51 |  963 | `		int nKey = ZipAesKeyLen(pEnt->iEncMethod);` |
|   51 |  964 | `		int nSalt = nKey / 2;` |
|    - |  965 | `		unsigned char zKey[ZIP_AES_MAXKEY * 2 + 2];` |
|    - |  966 | `		sxu32 nCipher,nAt;` |
|   51 |  967 | `		if( nKey == 0 ){` |
|  ! 0 |  968 | `			return ZIP_ER_ENCRNOTSUPP;` |
|    - |  969 | `		}` |
|   51 |  970 | `		if( nData < (sxu32)nSalt + 2 + 10 ){` |
|  ! 0 |  971 | `			return ZIP_ER_EOF;` |
|    - |  972 | `		}` |
|   51 |  973 | `		if( ZipAesDerive(zPw,nPw,zData,nSalt,nKey,zKey) != 0 ){` |
|  ! 0 |  974 | `			return ZIP_ER_INTERNAL;` |
|    - |  975 | `		}` |
|   51 |  976 | `		if( zKey[nKey*2] != zData[nSalt] \|\| zKey[nKey*2+1] != zData[nSalt+1] ){` |
|   27 |  977 | `			return ZIP_ER_WRONGPASSWD;` |
|    - |  978 | `		}` |
|   25 |  979 | `		nCipher = nData - (sxu32)nSalt - 2 - 10;` |
|   25 |  980 | `		nAt = SyBlobLength(pOut);` |
|   25 |  981 | `		SyBlobAppend(pOut,&zData[nSalt+2],nCipher);` |
|   24 |  982 | `		if( nCipher > 0` |
|   25 |  983 | `		 && ZipAesCtr(zKey,nKey,(unsigned char *)SyBlobData(pOut) + nAt,nCipher) != 0 ){` |
|  ! 0 |  984 | `			return ZIP_ER_INTERNAL;` |
|    - |  985 | `		}` |
|   25 |  986 | `		return ZIP_ER_OK;` |
|    - |  987 | `	}` |
|    - |  988 | `#else` |
|    - |  989 | `	return ZIP_ER_ENCRNOTSUPP;` |
|    - |  990 | `#endif` |
|   41 |  991 | `}` |
|    - |  992 | `/*` |
|    - |  993 | ` * Is the cached plaintext of an ENCRYPTED entry still the answer? Only if the` |
|    - |  994 | ` * archive's password is the one that produced it. libzip decrypts on every` |
|    - |  995 | ` * zip_fopen, so php refuses a read whose password is wrong however many times` |
|    - |  996 | ` * the entry was read correctly before -- where a cache with no key on it` |
|    - |  997 | ` * answered the plaintext to anybody who asked twice.` |
|    - |  998 | ` */` |
|   10 |  999 | `static int ZipCachedPwMatches(phl_zip *pZip,phl_zip_ent *pEnt)` |
|    1 | 1000 | `{` |
|   11 | 1001 | `	sxu32 nNow = SyBlobLength(&pZip->sPassword);` |
|   11 | 1002 | `	sxu32 nWas = SyBlobLength(&pEnt->sLoadedPw);` |
|   11 | 1003 | `	if( nNow != nWas ){` |
|    9 | 1004 | `		return 0;` |
|    - | 1005 | `	}` |
|    3 | 1006 | `	return nNow == 0` |
|    2 | 1007 | `		\|\| SyMemcmp(SyBlobData(&pZip->sPassword),SyBlobData(&pEnt->sLoadedPw),nNow) == 0;` |
|    6 | 1008 | `}` |
|    - | 1009 | `/*` |
|    - | 1010 | ` * The uncompressed bytes of one entry, decompressed once and kept: an archive` |
|    - | 1011 | ` * is normally read many times over, and the file is already in memory. An` |
|    - | 1012 | ` * ENCRYPTED entry's cache is only good for the password that opened it.` |
|    - | 1013 | ` */` |
|  794 | 1014 | `static int ZipEntLoad(phl_zip *pZip,phl_zip_ent *pEnt)` |
|    3 | 1015 | `{` |
|    - | 1016 | `	const unsigned char *zData;` |
|  797 | 1017 | `	sxu32 nData = 0;` |
|    - | 1018 | `	SyBlob sPlain;` |
|    - | 1019 | `	int rc;` |
|  797 | 1020 | `	SyBlobInit(&sPlain,&pZip->pVm->sAllocator);` |
|  794 | 1021 | `	if( pEnt->bLoaded && pEnt->iEncMethod != ZIP_EM_NONE && !pEnt->bNew` |
|   13 | 1022 | `	 && !pEnt->bDataChanged && !ZipCachedPwMatches(pZip,pEnt) ){` |
|    - | 1023 | `		/* A different password than the one behind sData: drop it and decrypt` |
|    - | 1024 | `		 * again, so a wrong one refuses the way it would have the first time. */` |
|    9 | 1025 | `		SyBlobReset(&pEnt->sData);` |
|    9 | 1026 | `		pEnt->bLoaded = 0;` |
|    4 | 1027 | `	}` |
|  797 | 1028 | `	if( pEnt->bLoaded \|\| pEnt->bDir ){` |
|  237 | 1029 | `		pEnt->bLoaded = 1;` |
|  237 | 1030 | `		SyBlobRelease(&sPlain);` |
|  237 | 1031 | `		return ZIP_ER_OK;` |
|    - | 1032 | `	}` |
|  563 | 1033 | `	if( pEnt->bNew \|\| pEnt->bDataChanged ){` |
|    - | 1034 | `		/* Added or replaced and not yet written. libzip will not read an entry` |
|    - | 1035 | `		 * out of a source it has not committed, and it says so in its own` |
|    - | 1036 | ``		 * words: `Entry has been changed`, which is what `getFromName()` on a`` |
|    - | 1037 | ``		 * fresh `addFromString()` leaves in `$z->status`. */`` |
|    3 | 1038 | `		SyBlobRelease(&sPlain);` |
|    3 | 1039 | `		return ZIP_ER_CHANGED;` |
|    - | 1040 | `	}` |
|  561 | 1041 | `	rc = ZipEntPayload(pZip,pEnt,&zData,&nData);` |
|  561 | 1042 | `	if( rc != ZIP_ER_OK ){` |
|    9 | 1043 | `		SyBlobRelease(&sPlain);` |
|    9 | 1044 | `		return rc;` |
|    - | 1045 | `	}` |
|  553 | 1046 | `	if( pEnt->iEncMethod != ZIP_EM_NONE ){` |
|    - | 1047 | `		/* The plaintext replaces the slice for everything below. It is the` |
|    - | 1048 | `		 * ARCHIVE's password that opens an entry, never the one a` |
|    - | 1049 | `		 * setEncryption() named -- that one is for the WRITE. */` |
|   80 | 1050 | `		rc = ZipDecrypt(pZip,pEnt,zData,nData,&sPlain);` |
|   80 | 1051 | `		if( rc != ZIP_ER_OK ){` |
|   48 | 1052 | `			SyBlobRelease(&sPlain);` |
|   48 | 1053 | `			return rc;` |
|    - | 1054 | `		}` |
|   33 | 1055 | `		zData = (const unsigned char *)SyBlobData(&sPlain);` |
|   33 | 1056 | `		nData = SyBlobLength(&sPlain);` |
|    - | 1057 | ``		/* Remember WHICH password opened it. Every `bLoaded = 1` below is on`` |
|    - | 1058 | `		 * this side of the decrypt, so one record covers them all. */` |
|   33 | 1059 | `		SyBlobReset(&pEnt->sLoadedPw);` |
|   33 | 1060 | `		if( SyBlobLength(&pZip->sPassword) > 0 ){` |
|   49 | 1061 | `			SyBlobAppend(&pEnt->sLoadedPw,SyBlobData(&pZip->sPassword),` |
|   16 | 1062 | `				SyBlobLength(&pZip->sPassword));` |
|   16 | 1063 | `		}` |
|   16 | 1064 | `	}` |
|  507 | 1065 | `	if( pEnt->iMethod == ZIP_CM_STORE ){` |
|   44 | 1066 | `		SyBlobAppend(&pEnt->sData,zData,nData);` |
|   44 | 1067 | `		SyBlobRelease(&sPlain);` |
|   44 | 1068 | `		pEnt->bLoaded = 1;` |
|   44 | 1069 | `		return ZIP_ER_OK;` |
|    - | 1070 | `	}` |
|  465 | 1071 | `	if( pEnt->iMethod != ZIP_CM_DEFLATE ){` |
|    - | 1072 | `		/* bzip2, lzma, xz, and the six older methods nothing writes any more:` |
|    - | 1073 | `		 * php's answer on a libzip built without the library is this one, and` |
|    - | 1074 | `		 * the entry is still listed and still copied through a rewrite. */` |
|  ! 0 | 1075 | `		SyBlobRelease(&sPlain);` |
|  ! 0 | 1076 | `		return ZIP_ER_COMPNOTSUPP;` |
|    - | 1077 | `	}` |
|    - | 1078 | `	{` |
|    - | 1079 | `		/*` |
|    - | 1080 | `		 * Inflate in CHUNKS rather than in one Z_FINISH, which is not a` |
|    - | 1081 | `		 * performance choice: it is what php answers. A member whose deflate` |
|    - | 1082 | `		 * stream is DAMAGED gives back whatever came out of the calls that` |
|    - | 1083 | `		 * succeeded, and a one-shot finish emits a byte more than a chunked` |
|    - | 1084 | `		 * read does on the very same bytes -- measured by flipping one byte of` |
|    - | 1085 | `		 * a payload and reading it under both engines.` |
|    - | 1086 | `		 */` |
|    - | 1087 | `		z_stream z;` |
|    - | 1088 | `		unsigned char zOut[8192];` |
|  465 | 1089 | `		int zrc = Z_OK;` |
|  465 | 1090 | `		sxu32 nDone = 0;` |
|  465 | 1091 | `		if( pEnt->nSize == 0 ){` |
|  ! 0 | 1092 | `			SyBlobRelease(&sPlain);` |
|  ! 0 | 1093 | `			pEnt->bLoaded = 1;` |
|  ! 0 | 1094 | `			return ZIP_ER_OK;` |
|    - | 1095 | `		}` |
|  465 | 1096 | `		SyZero(&z,sizeof(z));` |
|    - | 1097 | `		/* A zip member is a RAW deflate stream: no zlib header, no trailer. */` |
|  465 | 1098 | `		if( inflateInit2(&z,-MAX_WBITS) != Z_OK ){` |
|  ! 0 | 1099 | `			SyBlobRelease(&sPlain);` |
|  ! 0 | 1100 | `			return ZIP_ER_ZLIB;` |
|    - | 1101 | `		}` |
|  465 | 1102 | `		z.next_in = (Bytef *)zData;` |
|  465 | 1103 | `		z.avail_in = (uInt)nData;` |
|  465 | 1104 | `		while( nDone < pEnt->nSize ){` |
|  465 | 1105 | `			sxu32 nRoom = pEnt->nSize - nDone;` |
|    - | 1106 | `			sxu32 nGot;` |
|  465 | 1107 | `			if( nRoom > (sxu32)sizeof(zOut) ){` |
|  ! 0 | 1108 | `				nRoom = (sxu32)sizeof(zOut);` |
|  ! 0 | 1109 | `			}` |
|  465 | 1110 | `			z.next_out = (Bytef *)zOut;` |
|  465 | 1111 | `			z.avail_out = (uInt)nRoom;` |
|  465 | 1112 | `			zrc = inflate(&z,Z_NO_FLUSH);` |
|  465 | 1113 | `			if( zrc != Z_OK && zrc != Z_STREAM_END && zrc != Z_BUF_ERROR ){` |
|    - | 1114 | `				/* The call that FAILED contributes nothing, which is the half` |
|    - | 1115 | `				 * of this a one-shot finish gets wrong. */` |
|   15 | 1116 | `				break;` |
|    - | 1117 | `			}` |
|  451 | 1118 | `			nGot = nRoom - (sxu32)z.avail_out;` |
|  451 | 1119 | `			if( nGot > 0 ){` |
|  451 | 1120 | `				SyBlobAppend(&pEnt->sData,zOut,nGot);` |
|  451 | 1121 | `				nDone += nGot;` |
|  224 | 1122 | `			}` |
|  451 | 1123 | `			if( zrc == Z_STREAM_END \|\| (nGot == 0 && z.avail_in == 0) \|\| zrc == Z_BUF_ERROR ){` |
|  227 | 1124 | `				break;` |
|    - | 1125 | `			}` |
|  ! 0 | 1126 | `		}` |
|  465 | 1127 | `		inflateEnd(&z);` |
|  465 | 1128 | `		if( zrc == Z_MEM_ERROR ){` |
|  ! 0 | 1129 | `			SyBlobRelease(&sPlain);` |
|  ! 0 | 1130 | `			return ZIP_ER_MEMORY;` |
|    - | 1131 | `		}` |
|  465 | 1132 | `		pEnt->bLoaded = 1;` |
|    - | 1133 | `	}` |
|  465 | 1134 | `	SyBlobRelease(&sPlain);` |
|  465 | 1135 | `	return ZIP_ER_OK;` |
|  400 | 1136 | `}` |
|    - | 1137 | `/* ------------------------------------------------------------------ */` |
|    - | 1138 | `/* Writing an archive                                                  */` |
|    - | 1139 | `/* ------------------------------------------------------------------ */` |
|    - | 1140 | `/*` |
|    - | 1141 | ` * The bytes an entry's SOURCE holds, for one that is being written fresh.` |
|    - | 1142 | ` * addFromString() left them in sData; addFile() left a path, and php reads that` |
|    - | 1143 | ` * file at CLOSE rather than at add -- so a file replaced in between is written` |
|    - | 1144 | ` * as it is now, and one deleted in between fails the close.` |
|    - | 1145 | ` */` |
|   84 | 1146 | `static int ZipEntSource(phl_zip *pZip,phl_zip_ent *pEnt,SyBlob *pOut)` |
|    3 | 1147 | `{` |
|   87 | 1148 | `	if( SyBlobLength(&pEnt->sSrcPath) > 0 ){` |
|    - | 1149 | `		const ph7_io_stream *pStream;` |
|   13 | 1150 | `		const char *zPath = (const char *)SyBlobData(&pEnt->sSrcPath);` |
|    - | 1151 | `		void *pHandle;` |
|    - | 1152 | `		SyBlob sRaw;` |
|   13 | 1153 | `		SyBlobInit(&sRaw,&pZip->pVm->sAllocator);` |
|   19 | 1154 | `		pStream = PH7_VmGetStreamDevice(pZip->pVm,&zPath,` |
|   12 | 1155 | `			(int)SyStrlen((const char *)SyBlobData(&pEnt->sSrcPath)));` |
|   13 | 1156 | `		if( pStream == 0 ){` |
|  ! 0 | 1157 | `			SyBlobRelease(&sRaw);` |
|  ! 0 | 1158 | `			return ZIP_ER_OPEN;` |
|    - | 1159 | `		}` |
|   13 | 1160 | `		pHandle = PH7_StreamOpenHandle(pZip->pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,` |
|    - | 1161 | `			FALSE,0,FALSE,0,0);` |
|   13 | 1162 | `		if( pHandle == 0 ){` |
|  ! 0 | 1163 | `			SyBlobRelease(&sRaw);` |
|  ! 0 | 1164 | `			return ZIP_ER_OPEN;` |
|    - | 1165 | `		}` |
|   13 | 1166 | `		PH7_StreamReadWholeFile(pHandle,pStream,&sRaw);` |
|   13 | 1167 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|    - | 1168 | `		{` |
|    - | 1169 | `			/* The slice addFile() was asked for: a start past the end is an` |
|    - | 1170 | `			 * empty entry rather than a refusal, and a length past it is the` |
|    - | 1171 | `			 * rest of the file. */` |
|   13 | 1172 | `			sxu32 nRaw = SyBlobLength(&sRaw);` |
|   13 | 1173 | `			sxu32 nStart = 0,nWant;` |
|   13 | 1174 | `			if( pEnt->iSrcStart > 0 ){` |
|  ! 0 | 1175 | `				nStart = (sxu64)pEnt->iSrcStart < (sxu64)nRaw` |
|  ! 0 | 1176 | `					? (sxu32)pEnt->iSrcStart : nRaw;` |
|  ! 0 | 1177 | `			}` |
|   13 | 1178 | `			nWant = nRaw - nStart;` |
|   13 | 1179 | `			if( pEnt->iSrcLen > 0 && (sxu64)pEnt->iSrcLen < (sxu64)nWant ){` |
|  ! 0 | 1180 | `				nWant = (sxu32)pEnt->iSrcLen;` |
|  ! 0 | 1181 | `			}` |
|   13 | 1182 | `			SyBlobAppend(pOut,(const char *)SyBlobData(&sRaw) + nStart,nWant);` |
|    - | 1183 | `		}` |
|   13 | 1184 | `		SyBlobRelease(&sRaw);` |
|   13 | 1185 | `		return ZIP_ER_OK;` |
|    - | 1186 | `	}` |
|   75 | 1187 | `	SyBlobAppend(pOut,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));` |
|   75 | 1188 | `	return ZIP_ER_OK;` |
|   45 | 1189 | `}` |
|    - | 1190 | `/*` |
|    - | 1191 | ` * Deflate a buffer into a RAW stream, at the level libzip asks zlib for.` |
|    - | 1192 | `` * The `maximum compression` bit the writer sets in the flag word is what says`` |
|    - | 1193 | ` * so on the wire, so the two have to agree.` |
|    - | 1194 | ` */` |
|   78 | 1195 | `static int ZipDeflate(phl_zip *pZip,const char *zIn,sxu32 nIn,SyBlob *pOut)` |
|    3 | 1196 | `{` |
|    - | 1197 | `	z_stream z;` |
|    - | 1198 | `	unsigned char zBuf[8192];` |
|    - | 1199 | `	int rc;` |
|   81 | 1200 | `	SyZero(&z,sizeof(z));` |
|   81 | 1201 | `	if( deflateInit2(&z,9,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY) != Z_OK ){` |
|  ! 0 | 1202 | `		return ZIP_ER_ZLIB;` |
|    - | 1203 | `	}` |
|   81 | 1204 | `	z.next_in = (Bytef *)zIn;` |
|   81 | 1205 | `	z.avail_in = (uInt)nIn;` |
|   39 | 1206 | `	for(;;){` |
|   81 | 1207 | `		z.next_out = (Bytef *)zBuf;` |
|   81 | 1208 | `		z.avail_out = (uInt)sizeof(zBuf);` |
|   81 | 1209 | `		rc = deflate(&z,Z_FINISH);` |
|   81 | 1210 | `		if( rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR ){` |
|  ! 0 | 1211 | `			deflateEnd(&z);` |
|  ! 0 | 1212 | `			return ZIP_ER_ZLIB;` |
|    - | 1213 | `		}` |
|   81 | 1214 | `		SyBlobAppend(pOut,zBuf,(sxu32)(sizeof(zBuf) - z.avail_out));` |
|   81 | 1215 | `		if( rc == Z_STREAM_END ){` |
|   81 | 1216 | `			break;` |
|    - | 1217 | `		}` |
|  ! 0 | 1218 | `		if( z.avail_out != 0 && rc == Z_BUF_ERROR ){` |
|  ! 0 | 1219 | `			deflateEnd(&z);` |
|  ! 0 | 1220 | `			return ZIP_ER_ZLIB;` |
|    - | 1221 | `		}` |
|  ! 0 | 1222 | `	}` |
|   81 | 1223 | `	deflateEnd(&z);` |
|   39 | 1224 | `	SXUNUSED(pZip);` |
|   81 | 1225 | `	return ZIP_ER_OK;` |
|   42 | 1226 | `}` |
|    - | 1227 | `/*` |
|    - | 1228 | ` * The cipher, applied in place to the bytes an entry is about to be written as.` |
|    - | 1229 | ` *` |
|    - | 1230 | ` * The password is the ENTRY's when setEncryption() named one and the archive's` |
|    - | 1231 | `` * otherwise, and there has to BE one: php lets `setEncryptionName()` succeed`` |
|    - | 1232 | `` * with none and then fails the `close()` with `Invalid argument`, which is what`` |
|    - | 1233 | ` * an entry with no key to write under is.` |
|    - | 1234 | ` */` |
|   26 | 1235 | `static int ZipEncrypt(phl_zip *pZip,phl_zip_ent *pEnt,int iMethod,sxu32 nCrc,` |
|    - | 1236 | `	sxu32 nDosTime,SyBlob *pData)` |
|    1 | 1237 | `{` |
|    - | 1238 | `	const char *zPw;` |
|    - | 1239 | `	sxu32 nPw;` |
|    - | 1240 | `	SyBlob sOut;` |
|   27 | 1241 | `	int rc = ZIP_ER_OK;` |
|   27 | 1242 | `	if( pEnt->bHasPassword ){` |
|    3 | 1243 | `		zPw = (const char *)SyBlobData(&pEnt->sPassword);` |
|    3 | 1244 | `		nPw = SyBlobLength(&pEnt->sPassword);` |
|    2 | 1245 | `	}else{` |
|   25 | 1246 | `		zPw = (const char *)SyBlobData(&pZip->sPassword);` |
|   25 | 1247 | `		nPw = SyBlobLength(&pZip->sPassword);` |
|    - | 1248 | `	}` |
|   27 | 1249 | `	if( nPw < 1 ){` |
|    3 | 1250 | `		return ZIP_ER_INVAL;` |
|    - | 1251 | `	}` |
|   25 | 1252 | `	if( !ZipEncSupported(iMethod) \|\| iMethod == ZIP_EM_NONE ){` |
|  ! 0 | 1253 | `		return ZIP_ER_ENCRNOTSUPP;` |
|    - | 1254 | `	}` |
|   25 | 1255 | `	SyBlobInit(&sOut,&pZip->pVm->sAllocator);` |
|   25 | 1256 | `	if( iMethod == ZIP_EM_TRAD_PKWARE ){` |
|    - | 1257 | `		phl_zip_keys sKeys;` |
|    - | 1258 | `		unsigned char zHdr[12];` |
|    - | 1259 | `		sxu32 i;` |
|    3 | 1260 | `		SXUNUSED(nCrc);` |
|    - | 1261 | `		/* Eleven random bytes and a CHECK byte a reader can test the password` |
|    - | 1262 | `		 * against. The archives this writes carry the trailing-descriptor flag` |
|    - | 1263 | `		 * the way libzip's do, so the check byte is the DOS time's high one --` |
|    - | 1264 | `		 * and it is the time the HEADER will carry, which is the one thing php` |
|    - | 1265 | `		 * gets wrong here: libzip takes the check byte from the stamp the entry` |
|    - | 1266 | ``		 * had before `setMtime*()` moved it, so a php archive written that way`` |
|    - | 1267 | `		 * cannot be read back by php itself. Reproducing that would mean` |
|    - | 1268 | `		 * writing an archive nothing can open. */` |
|   73 | 1269 | `		for( i = 0 ; i < 11 ; ++i ){` |
|   67 | 1270 | `			zHdr[i] = (unsigned char)(PH7_VmRandomNum(pZip->pVm) & 0xFF);` |
|   34 | 1271 | `		}` |
|    7 | 1272 | `		zHdr[11] = (unsigned char)((nDosTime >> 8) & 0xFF);` |
|    7 | 1273 | `		ZipTradInit(&sKeys,zPw,nPw);` |
|   79 | 1274 | `		for( i = 0 ; i < 12 ; ++i ){` |
|   73 | 1275 | `			zHdr[i] = ZipTradEncrypt(&sKeys,zHdr[i]);` |
|   37 | 1276 | `		}` |
|    7 | 1277 | `		SyBlobAppend(&sOut,zHdr,sizeof(zHdr));` |
|    - | 1278 | `		{` |
|    7 | 1279 | `			const unsigned char *zIn = (const unsigned char *)SyBlobData(pData);` |
|    7 | 1280 | `			sxu32 nIn = SyBlobLength(pData);` |
|   13 | 1281 | `			for( i = 0 ; i < nIn ; ){` |
|    - | 1282 | `				unsigned char zBuf[4096];` |
|    7 | 1283 | `				sxu32 n = 0;` |
|   53 | 1284 | `				while( i < nIn && n < (sxu32)sizeof(zBuf) ){` |
|   47 | 1285 | `					zBuf[n++] = ZipTradEncrypt(&sKeys,zIn[i++]);` |
|    1 | 1286 | `				}` |
|    7 | 1287 | `				SyBlobAppend(&sOut,zBuf,n);` |
|    1 | 1288 | `			}` |
|    - | 1289 | `		}` |
|    4 | 1290 | `	}else{` |
|    - | 1291 | `#ifdef PH7_ENABLE_OPENSSL` |
|   19 | 1292 | `		int nKey = ZipAesKeyLen(iMethod);` |
|   19 | 1293 | `		int nSalt = nKey / 2;` |
|    - | 1294 | `		unsigned char zSalt[ZIP_AES_MAXKEY / 2];` |
|    - | 1295 | `		unsigned char zKey[ZIP_AES_MAXKEY * 2 + 2];` |
|    - | 1296 | `		unsigned char zMac[10];` |
|   19 | 1297 | `		sxu32 nBody = SyBlobLength(pData);` |
|   18 | 1298 | `		if( RAND_bytes(zSalt,nSalt) != 1` |
|   19 | 1299 | `		 \|\| ZipAesDerive(zPw,nPw,zSalt,nSalt,nKey,zKey) != 0 ){` |
|  ! 0 | 1300 | `			SyBlobRelease(&sOut);` |
|  ! 0 | 1301 | `			return ZIP_ER_INTERNAL;` |
|    - | 1302 | `		}` |
|   19 | 1303 | `		SyBlobAppend(&sOut,zSalt,(sxu32)nSalt);` |
|   19 | 1304 | `		SyBlobAppend(&sOut,&zKey[nKey*2],2);` |
|   19 | 1305 | `		SyBlobAppend(&sOut,SyBlobData(pData),nBody);` |
|   18 | 1306 | `		if( nBody > 0` |
|   19 | 1307 | `		 && ZipAesCtr(zKey,nKey,(unsigned char *)SyBlobData(&sOut) + nSalt + 2,nBody) != 0 ){` |
|  ! 0 | 1308 | `			SyBlobRelease(&sOut);` |
|  ! 0 | 1309 | `			return ZIP_ER_INTERNAL;` |
|    - | 1310 | `		}` |
|    - | 1311 | `		/* The authentication code is over the CIPHERTEXT, which is why it is` |
|    - | 1312 | `		 * taken after the pass above rather than before it. */` |
|   27 | 1313 | `		if( ZipAesMac(&zKey[nKey],nKey,` |
|   28 | 1314 | `			(const unsigned char *)SyBlobData(&sOut) + nSalt + 2,nBody,zMac) != 0 ){` |
|  ! 0 | 1315 | `			SyBlobRelease(&sOut);` |
|  ! 0 | 1316 | `			return ZIP_ER_INTERNAL;` |
|    - | 1317 | `		}` |
|   19 | 1318 | `		SyBlobAppend(&sOut,zMac,sizeof(zMac));` |
|    - | 1319 | `#else` |
|    - | 1320 | `		SyBlobRelease(&sOut);` |
|    - | 1321 | `		return ZIP_ER_ENCRNOTSUPP;` |
|    - | 1322 | `#endif` |
|    - | 1323 | `	}` |
|   25 | 1324 | `	SyBlobReset(pData);` |
|   25 | 1325 | `	SyBlobAppend(pData,SyBlobData(&sOut),SyBlobLength(&sOut));` |
|   25 | 1326 | `	SyBlobRelease(&sOut);` |
|   25 | 1327 | `	return rc;` |
|   14 | 1328 | `}` |
|    - | 1329 | `/*` |
|    - | 1330 | ` * How one entry's bytes go out, and under which method.` |
|    - | 1331 | ` *` |
|    - | 1332 | ` * Three answers, and the FIRST is the one that matters most: an entry nobody` |
|    - | 1333 | ` * touched is copied through BYTE FOR BYTE, method and CRC included. That is` |
|    - | 1334 | ` * what lets an archive holding a bzip2 or an lzma member -- which this build` |
|    - | 1335 | ` * cannot decode -- survive a rewrite that adds a file beside it, and it is what` |
|    - | 1336 | ` * libzip does too.` |
|    - | 1337 | ` *` |
|    - | 1338 | `` * `CM_DEFAULT` is deflate, unless deflate did not help: libzip stores an entry`` |
|    - | 1339 | ` * whose deflated form is no smaller, which is why a thirteen-byte file comes` |
|    - | 1340 | `` * out `comp_method` 0. An EXPLICIT `CM_DEFLATE` does not get that second look,`` |
|    - | 1341 | `` * so `setCompressionName($n, CM_DEFLATE)` on random bytes really does write`` |
|    - | 1342 | ` * more than it was given.` |
|    - | 1343 | ` */` |
|  106 | 1344 | `static int ZipEntStored(phl_zip *pZip,phl_zip_ent *pEnt,SyBlob *pOut,` |
|    - | 1345 | `	int *piMethod,sxu32 *pnCrc,sxu32 *pnSize,int *piEncrypt,sxu32 nDosTime)` |
|    3 | 1346 | `{` |
|    - | 1347 | `	SyBlob sRaw;` |
|    - | 1348 | `	int iWant,rc;` |
|  109 | 1349 | `	int bTouched = pEnt->bNew \|\| pEnt->bDataChanged;` |
|  109 | 1350 | `	int bReEncrypt = pEnt->iSetEncrypt >= 0 && pEnt->iSetEncrypt != pEnt->iEncMethod;` |
|  109 | 1351 | `	*piEncrypt = pEnt->iSetEncrypt >= 0 ? pEnt->iSetEncrypt : pEnt->iEncMethod;` |
|  106 | 1352 | `	if( !bTouched && !bReEncrypt` |
|   23 | 1353 | `	 && (pEnt->iSetMethod == ZIP_CM_DEFAULT \|\| pEnt->iSetMethod == pEnt->iMethod) ){` |
|    - | 1354 | `		const unsigned char *zData;` |
|   18 | 1355 | `		sxu32 nData = 0;` |
|   18 | 1356 | `		rc = ZipEntPayload(pZip,pEnt,&zData,&nData);` |
|   18 | 1357 | `		if( rc != ZIP_ER_OK ){` |
|  ! 0 | 1358 | `			return rc;` |
|    - | 1359 | `		}` |
|   18 | 1360 | `		SyBlobAppend(pOut,zData,nData);` |
|   18 | 1361 | `		*piMethod = pEnt->iMethod;` |
|   18 | 1362 | `		*pnCrc = pEnt->nCrc;` |
|   18 | 1363 | `		*pnSize = pEnt->nSize;` |
|   18 | 1364 | `		return ZIP_ER_OK;` |
|    - | 1365 | `	}` |
|   93 | 1366 | `	SyBlobInit(&sRaw,&pZip->pVm->sAllocator);` |
|   93 | 1367 | `	if( bTouched ){` |
|   87 | 1368 | `		rc = ZipEntSource(pZip,pEnt,&sRaw);` |
|   45 | 1369 | `	}else{` |
|    8 | 1370 | `		rc = ZipEntLoad(pZip,pEnt);` |
|    8 | 1371 | `		if( rc == ZIP_ER_OK ){` |
|    8 | 1372 | `			SyBlobAppend(&sRaw,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));` |
|    3 | 1373 | `		}` |
|    - | 1374 | `	}` |
|   93 | 1375 | `	if( rc != ZIP_ER_OK ){` |
|  ! 0 | 1376 | `		SyBlobRelease(&sRaw);` |
|  ! 0 | 1377 | `		return rc;` |
|    - | 1378 | `	}` |
|   93 | 1379 | `	*pnSize = SyBlobLength(&sRaw);` |
|  138 | 1380 | `	*pnCrc = (sxu32)crc32(crc32(0L,Z_NULL,0),` |
|   90 | 1381 | `		(const Bytef *)SyBlobData(&sRaw),(uInt)SyBlobLength(&sRaw));` |
|   93 | 1382 | `	iWant = pEnt->iSetMethod;` |
|   93 | 1383 | `	if( pEnt->bDir ){` |
|   12 | 1384 | `		iWant = ZIP_CM_STORE;` |
|    5 | 1385 | `	}` |
|   93 | 1386 | `	if( iWant == ZIP_CM_STORE ){` |
|   14 | 1387 | `		SyBlobAppend(pOut,SyBlobData(&sRaw),SyBlobLength(&sRaw));` |
|   14 | 1388 | `		*piMethod = ZIP_CM_STORE;` |
|   87 | 1389 | `	}else if( iWant != ZIP_CM_DEFAULT && iWant != ZIP_CM_DEFLATE ){` |
|  ! 0 | 1390 | `		SyBlobRelease(&sRaw);` |
|  ! 0 | 1391 | `		return ZIP_ER_COMPNOTSUPP;` |
|  ! 0 | 1392 | `	}else{` |
|    - | 1393 | `		SyBlob sDef;` |
|   81 | 1394 | `		SyBlobInit(&sDef,&pZip->pVm->sAllocator);` |
|   81 | 1395 | `		rc = ZipDeflate(pZip,(const char *)SyBlobData(&sRaw),SyBlobLength(&sRaw),&sDef);` |
|   81 | 1396 | `		if( rc != ZIP_ER_OK ){` |
|  ! 0 | 1397 | `			SyBlobRelease(&sDef);` |
|  ! 0 | 1398 | `			SyBlobRelease(&sRaw);` |
|  ! 0 | 1399 | `			return rc;` |
|    - | 1400 | `		}` |
|   81 | 1401 | `		if( iWant == ZIP_CM_DEFAULT && SyBlobLength(&sDef) >= SyBlobLength(&sRaw) ){` |
|   52 | 1402 | `			SyBlobAppend(pOut,SyBlobData(&sRaw),SyBlobLength(&sRaw));` |
|   52 | 1403 | `			*piMethod = ZIP_CM_STORE;` |
|   27 | 1404 | `		}else{` |
|   31 | 1405 | `			SyBlobAppend(pOut,SyBlobData(&sDef),SyBlobLength(&sDef));` |
|   31 | 1406 | `			*piMethod = ZIP_CM_DEFLATE;` |
|    - | 1407 | `		}` |
|   81 | 1408 | `		SyBlobRelease(&sDef);` |
|    - | 1409 | `	}` |
|   93 | 1410 | `	SyBlobRelease(&sRaw);` |
|    - | 1411 | `	/* The cipher goes over the COMPRESSED bytes, which is the format's order and` |
|    - | 1412 | `	 * the only one a reader can undo. */` |
|   93 | 1413 | `	if( *piEncrypt != ZIP_EM_NONE ){` |
|   27 | 1414 | `		rc = ZipEncrypt(pZip,pEnt,*piEncrypt,*pnCrc,nDosTime,pOut);` |
|   27 | 1415 | `		if( rc != ZIP_ER_OK ){` |
|    3 | 1416 | `			return rc;` |
|    - | 1417 | `		}` |
|   12 | 1418 | `	}` |
|   91 | 1419 | `	return ZIP_ER_OK;` |
|   56 | 1420 | `}` |
|    - | 1421 | `/* php's registerProgressCallback(), asked once per entry.` |
|    - | 1422 | ` *` |
|    - | 1423 | ` * libzip reports i/n BEFORE writing entry i and calls only when the value has` |
|    - | 1424 | ` * advanced by MORE than the rate the script asked for -- strictly more, which` |
|    - | 1425 | ` * is why a rate of 0.5 over six entries reports 0 and then 4/6 rather than 0` |
|    - | 1426 | ` * and 3/6, and why neither run ever ends on 1.0. */` |
|  106 | 1427 | `static void ZipProgress(phl_zip *pZip,double *pLast,sxu32 i,sxu32 n)` |
|    3 | 1428 | `{` |
|  109 | 1429 | `	double r = n > 0 ? (double)i / (double)n : 0.0;` |
|    - | 1430 | `	ph7_value sArg,sRes;` |
|  109 | 1431 | `	ph7_value *pArg = &sArg;` |
|  109 | 1432 | `	if( pZip->pProgress == 0 ){` |
|  109 | 1433 | `		return;` |
|    - | 1434 | `	}` |
|  ! 0 | 1435 | `	if( i > 0 && r - *pLast <= pZip->rProgressRate ){` |
|  ! 0 | 1436 | `		return;` |
|    - | 1437 | `	}` |
|  ! 0 | 1438 | `	*pLast = r;` |
|  ! 0 | 1439 | `	PH7_MemObjInit(pZip->pVm,&sArg);` |
|  ! 0 | 1440 | `	PH7_MemObjInit(pZip->pVm,&sRes);` |
|  ! 0 | 1441 | `	ph7_value_double(&sArg,r);` |
|  ! 0 | 1442 | `	PH7_VmCallUserFunction(pZip->pVm,pZip->pProgress,1,&pArg,&sRes);` |
|  ! 0 | 1443 | `	PH7_MemObjRelease(&sRes);` |
|  ! 0 | 1444 | `	PH7_MemObjRelease(&sArg);` |
|   56 | 1445 | `}` |
|    - | 1446 | `/* php's registerCancelCallback(): a non-zero answer stops the write, and the` |
|    - | 1447 | ` * archive reports ER_CANCELLED and is left as it was. */` |
|  106 | 1448 | `static int ZipCancelled(phl_zip *pZip)` |
|    3 | 1449 | `{` |
|    - | 1450 | `	ph7_value sRet;` |
|    - | 1451 | `	int bStop;` |
|  109 | 1452 | `	if( pZip->pCancel == 0 ){` |
|  109 | 1453 | `		return 0;` |
|    - | 1454 | `	}` |
|  ! 0 | 1455 | `	PH7_MemObjInit(pZip->pVm,&sRet);` |
|  ! 0 | 1456 | `	PH7_VmCallUserFunction(pZip->pVm,pZip->pCancel,0,0,&sRet);` |
|  ! 0 | 1457 | `	PH7_MemObjToInteger(&sRet);` |
|  ! 0 | 1458 | `	bStop = sRet.x.iVal != 0;` |
|  ! 0 | 1459 | `	PH7_MemObjRelease(&sRet);` |
|  ! 0 | 1460 | `	return bStop;` |
|   56 | 1461 | `}` |
|    - | 1462 | `/*` |
|    - | 1463 | ` * Build the whole file: a local header and payload per entry, then the central` |
|    - | 1464 | ` * directory, then the end record with the archive comment behind it.` |
|    - | 1465 | ` */` |
|   50 | 1466 | `static int ZipBuild(phl_zip *pZip,SyBlob *pOut)` |
|    3 | 1467 | `{` |
|    - | 1468 | `	SyBlob sCd,sExtra;` |
|   53 | 1469 | `	sxu32 i,nCdOff,nEntries = 0,nWrite = 0;` |
|   53 | 1470 | `	double rLast = 0.0;` |
|   53 | 1471 | `	int rc = ZIP_ER_OK;` |
|  163 | 1472 | `	for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|  113 | 1473 | `		if( !pZip->apEnt[i]->bDeleted ){` |
|  109 | 1474 | `			nWrite++;` |
|   53 | 1475 | `		}` |
|   58 | 1476 | `	}` |
|   53 | 1477 | `	SyBlobInit(&sCd,&pZip->pVm->sAllocator);` |
|   53 | 1478 | `	SyBlobInit(&sExtra,&pZip->pVm->sAllocator);` |
|  161 | 1479 | `	for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|  113 | 1480 | `		phl_zip_ent *pEnt = pZip->apEnt[i];` |
|    - | 1481 | `		SyBlob sStored;` |
|  113 | 1482 | `		sxu32 nLocal,nName,nCrc = 0,nSize = 0,nTime,nDate,nFlag,nRawMethod,nVersion;` |
|  113 | 1483 | `		int iMethod = ZIP_CM_STORE;` |
|  113 | 1484 | `		int iEncrypt = ZIP_EM_NONE;` |
|  113 | 1485 | `		if( pEnt->bDeleted ){` |
|    5 | 1486 | `			continue;` |
|    - | 1487 | `		}` |
|  109 | 1488 | `		ZipProgress(pZip,&rLast,nEntries,nWrite);` |
|  109 | 1489 | `		if( ZipCancelled(pZip) ){` |
|  ! 0 | 1490 | `			rc = ZIP_ER_CANCELLED;` |
|  ! 0 | 1491 | `			break;` |
|    - | 1492 | `		}` |
|  109 | 1493 | `		nLocal = SyBlobLength(pOut);` |
|  109 | 1494 | `		nName = SyBlobLength(&pEnt->sName);` |
|  109 | 1495 | `		SyBlobInit(&sStored,&pZip->pVm->sAllocator);` |
|  109 | 1496 | `		ZipUnixToDos(pEnt->iTime,&nTime,&nDate);` |
|  109 | 1497 | `		rc = ZipEntStored(pZip,pEnt,&sStored,&iMethod,&nCrc,&nSize,&iEncrypt,nTime);` |
|  109 | 1498 | `		if( rc != ZIP_ER_OK ){` |
|    3 | 1499 | `			SyBlobRelease(&sStored);` |
|    3 | 1500 | `			break;` |
|    - | 1501 | `		}` |
|    - | 1502 | `		/* The flag word: an untouched entry keeps the one it came with, so a` |
|    - | 1503 | `		 * data-descriptor or an encryption bit travels with its payload. */` |
|  107 | 1504 | `		if( pEnt->bNew \|\| pEnt->bDataChanged \|\| pEnt->iSetEncrypt >= 0 ){` |
|   89 | 1505 | `			nFlag = 0;` |
|   46 | 1506 | `		}else{` |
|   20 | 1507 | `			nFlag = pEnt->nFlag;` |
|    - | 1508 | `		}` |
|  104 | 1509 | `		if( iMethod == ZIP_CM_DEFLATE` |
|   70 | 1510 | `		 && (pEnt->bNew \|\| pEnt->bDataChanged \|\| pEnt->iSetEncrypt >= 0) ){` |
|   31 | 1511 | `			nFlag \|= 0x0002;   /* deflate, maximum compression */` |
|   14 | 1512 | `		}` |
|  107 | 1513 | `		if( ZipNameIsUtf8((const char *)SyBlobData(&pEnt->sName),nName) ){` |
|  ! 0 | 1514 | `			nFlag \|= 0x0800;` |
|  ! 0 | 1515 | `		}` |
|  107 | 1516 | `		nRawMethod = iMethod;` |
|  107 | 1517 | `		nVersion = ZIP_VERSION_NEEDED;` |
|  107 | 1518 | `		SyBlobReset(&sExtra);` |
|  107 | 1519 | `		if( iEncrypt != ZIP_EM_NONE ){` |
|   27 | 1520 | `			nFlag \|= 0x0001;` |
|   27 | 1521 | `			if( iEncrypt == ZIP_EM_TRAD_PKWARE ){` |
|    - | 1522 | `				/* The check byte the header carries is the DOS TIME's, which is` |
|    - | 1523 | `				 * only legible to a reader when bit 3 says the CRC was not` |
|    - | 1524 | `				 * known when the entry was written. libzip writes the pair this` |
|    - | 1525 | `				 * way and every reader expects it. */` |
|    7 | 1526 | `				nFlag \|= 0x0008;` |
|    4 | 1527 | `			}else{` |
|    - | 1528 | `				/* WinZip AES: the method BYTE is a placeholder and the real one` |
|    - | 1529 | `				 * moves into an extra field beside the key strength. A member` |
|    - | 1530 | `				 * under twenty bytes writes NO crc -- the WinZip rule, since a` |
|    - | 1531 | `				 * checksum of so little plaintext is a key-recovery hint. */` |
|   21 | 1532 | `				nRawMethod = ZIP_CM_WINZIP_AES;` |
|   21 | 1533 | `				nVersion = 51;` |
|   21 | 1534 | `				ZipPut16(&sExtra,ZIP_EXTRA_AES);` |
|   21 | 1535 | `				ZipPut16(&sExtra,7);` |
|   21 | 1536 | `				ZipPut16(&sExtra,2);   /* AE-2 */` |
|   21 | 1537 | `				SyBlobAppend(&sExtra,"AE",2);` |
|    - | 1538 | `				{` |
|   21 | 1539 | `					unsigned char bStrength =` |
|   20 | 1540 | `						(unsigned char)(iEncrypt - ZIP_EM_AES_128 + 1);` |
|   21 | 1541 | `					SyBlobAppend(&sExtra,&bStrength,1);` |
|    - | 1542 | `				}` |
|   21 | 1543 | `				ZipPut16(&sExtra,(sxu32)iMethod);` |
|   21 | 1544 | `				if( nSize < 20 ){` |
|    9 | 1545 | `					nCrc = 0;` |
|    4 | 1546 | `				}` |
|    - | 1547 | `			}` |
|   13 | 1548 | `		}` |
|  107 | 1549 | `		SyBlobAppend(pOut,"PK\003\004",4);` |
|  107 | 1550 | `		ZipPut16(pOut,nVersion);` |
|  107 | 1551 | `		ZipPut16(pOut,nFlag);` |
|  107 | 1552 | `		ZipPut16(pOut,(sxu32)nRawMethod);` |
|  107 | 1553 | `		ZipPut16(pOut,nTime);` |
|  107 | 1554 | `		ZipPut16(pOut,nDate);` |
|  107 | 1555 | `		ZipPut32(pOut,nCrc);` |
|  107 | 1556 | `		ZipPut32(pOut,SyBlobLength(&sStored));` |
|  107 | 1557 | `		ZipPut32(pOut,nSize);` |
|  107 | 1558 | `		ZipPut16(pOut,nName);` |
|  107 | 1559 | `		ZipPut16(pOut,SyBlobLength(&sExtra));` |
|  107 | 1560 | `		SyBlobAppend(pOut,SyBlobData(&pEnt->sName),nName);` |
|  107 | 1561 | `		SyBlobAppend(pOut,SyBlobData(&sExtra),SyBlobLength(&sExtra));` |
|  107 | 1562 | `		SyBlobAppend(pOut,SyBlobData(&sStored),SyBlobLength(&sStored));` |
|  107 | 1563 | `		SyBlobAppend(&sCd,"PK\001\002",4);` |
|  107 | 1564 | `		ZipPut16(&sCd,(sxu32)((pEnt->iOpsys << 8) \| (ZIP_VERSION_MADE & 0xFF)));` |
|  107 | 1565 | `		ZipPut16(&sCd,nVersion);` |
|  107 | 1566 | `		ZipPut16(&sCd,nFlag);` |
|  107 | 1567 | `		ZipPut16(&sCd,(sxu32)nRawMethod);` |
|  107 | 1568 | `		ZipPut16(&sCd,nTime);` |
|  107 | 1569 | `		ZipPut16(&sCd,nDate);` |
|  107 | 1570 | `		ZipPut32(&sCd,nCrc);` |
|  107 | 1571 | `		ZipPut32(&sCd,SyBlobLength(&sStored));` |
|  107 | 1572 | `		ZipPut32(&sCd,nSize);` |
|  107 | 1573 | `		ZipPut16(&sCd,nName);` |
|  107 | 1574 | `		ZipPut16(&sCd,SyBlobLength(&sExtra));` |
|  107 | 1575 | `		ZipPut16(&sCd,SyBlobLength(&pEnt->sComment));` |
|  107 | 1576 | `		ZipPut16(&sCd,0);` |
|  107 | 1577 | `		ZipPut16(&sCd,0);` |
|  107 | 1578 | `		ZipPut32(&sCd,pEnt->nAttr);` |
|  107 | 1579 | `		ZipPut32(&sCd,nLocal);` |
|  107 | 1580 | `		SyBlobAppend(&sCd,SyBlobData(&pEnt->sName),nName);` |
|  107 | 1581 | `		SyBlobAppend(&sCd,SyBlobData(&sExtra),SyBlobLength(&sExtra));` |
|  107 | 1582 | `		SyBlobAppend(&sCd,SyBlobData(&pEnt->sComment),SyBlobLength(&pEnt->sComment));` |
|  107 | 1583 | `		SyBlobRelease(&sStored);` |
|  107 | 1584 | `		nEntries++;` |
|   55 | 1585 | `	}` |
|   53 | 1586 | `	if( rc == ZIP_ER_OK ){` |
|   51 | 1587 | `		nCdOff = SyBlobLength(pOut);` |
|   51 | 1588 | `		SyBlobAppend(pOut,SyBlobData(&sCd),SyBlobLength(&sCd));` |
|   51 | 1589 | `		SyBlobAppend(pOut,"PK\005\006",4);` |
|   51 | 1590 | `		ZipPut16(pOut,0);` |
|   51 | 1591 | `		ZipPut16(pOut,0);` |
|   51 | 1592 | `		ZipPut16(pOut,nEntries);` |
|   51 | 1593 | `		ZipPut16(pOut,nEntries);` |
|   51 | 1594 | `		ZipPut32(pOut,SyBlobLength(&sCd));` |
|   51 | 1595 | `		ZipPut32(pOut,nCdOff);` |
|   51 | 1596 | `		ZipPut16(pOut,SyBlobLength(&pZip->sComment));` |
|   51 | 1597 | `		if( SyBlobLength(&pZip->sComment) > 0 ){` |
|    9 | 1598 | `			SyBlobAppend(pOut,SyBlobData(&pZip->sComment),SyBlobLength(&pZip->sComment));` |
|    4 | 1599 | `		}` |
|   24 | 1600 | `	}` |
|   53 | 1601 | `	SyBlobRelease(&sCd);` |
|   53 | 1602 | `	SyBlobRelease(&sExtra);` |
|   53 | 1603 | `	return rc;` |
|    3 | 1604 | `}` |
|    - | 1605 | `/* ------------------------------------------------------------------ */` |
|    - | 1606 | `/* Lifetime                                                            */` |
|    - | 1607 | `/* ------------------------------------------------------------------ */` |
|    - | 1608 | `/*` |
|    - | 1609 | ` * The per-VM registry every open archive is on. The object that opened one` |
|    - | 1610 | `` * holds a reference and so does every live `getStream()` handle, which is what`` |
|    - | 1611 | `` * makes php's own lifetime work: `close()` leaves such a stream readable, and`` |
|    - | 1612 | ` * the archive really goes only when the last holder lets go.` |
|    - | 1613 | ` */` |
|  398 | 1614 | `static void ZipRelease(phl_zip *pZip)` |
|    3 | 1615 | `{` |
|  401 | 1616 | `	ph7_vm *pVm = pZip->pVm;` |
|  401 | 1617 | `	phl_zip *p,*pPrev = 0;` |
|    - | 1618 | `	sxu32 i;` |
|  401 | 1619 | `	if( --pZip->nRef > 0 ){` |
|   12 | 1620 | `		return;` |
|    - | 1621 | `	}` |
|  391 | 1622 | `	for( p = (phl_zip *)pVm->pZips ; p ; pPrev = p, p = p->pNext ){` |
|  391 | 1623 | `		if( p != pZip ){` |
|  ! 0 | 1624 | `			continue;` |
|    - | 1625 | `		}` |
|  391 | 1626 | `		if( pPrev ){` |
|  ! 0 | 1627 | `			pPrev->pNext = p->pNext;` |
|  ! 0 | 1628 | `		}else{` |
|  391 | 1629 | `			pVm->pZips = (void *)p->pNext;` |
|    - | 1630 | `		}` |
|  391 | 1631 | `		break;` |
|  ! 0 | 1632 | `	}` |
| 1345 | 1633 | `	for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|  957 | 1634 | `		ZipEntFree(pVm,pZip->apEnt[i]);` |
|  480 | 1635 | `	}` |
|  391 | 1636 | `	if( pZip->apEnt ){` |
|  349 | 1637 | `		SyMemBackendFree(&pVm->sAllocator,pZip->apEnt);` |
|  173 | 1638 | `	}` |
|  391 | 1639 | `	if( pZip->pProgress ){` |
|  ! 0 | 1640 | `		ph7_release_value(pVm,pZip->pProgress);` |
|  ! 0 | 1641 | `	}` |
|  391 | 1642 | `	if( pZip->pCancel ){` |
|  ! 0 | 1643 | `		ph7_release_value(pVm,pZip->pCancel);` |
|  ! 0 | 1644 | `	}` |
|  391 | 1645 | `	SyBlobRelease(&pZip->sPath);` |
|  391 | 1646 | `	SyBlobRelease(&pZip->sFile);` |
|  391 | 1647 | `	SyBlobRelease(&pZip->sComment);` |
|  391 | 1648 | `	SyBlobRelease(&pZip->sOrigComment);` |
|  391 | 1649 | `	SyBlobRelease(&pZip->sPassword);` |
|  391 | 1650 | `	SyMemBackendFree(&pVm->sAllocator,pZip);` |
|  202 | 1651 | `}` |
|  388 | 1652 | `static phl_zip * ZipNew(ph7_vm *pVm)` |
|    3 | 1653 | `{` |
|  391 | 1654 | `	phl_zip *pZip = (phl_zip *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_zip));` |
|  391 | 1655 | `	if( pZip == 0 ){` |
|  ! 0 | 1656 | `		return 0;` |
|    - | 1657 | `	}` |
|  391 | 1658 | `	SyZero(pZip,sizeof(*pZip));` |
|  391 | 1659 | `	pZip->pVm = pVm;` |
|  391 | 1660 | `	SyBlobInit(&pZip->sPath,&pVm->sAllocator);` |
|  391 | 1661 | `	SyBlobInit(&pZip->sFile,&pVm->sAllocator);` |
|  391 | 1662 | `	SyBlobInit(&pZip->sComment,&pVm->sAllocator);` |
|  391 | 1663 | `	SyBlobInit(&pZip->sOrigComment,&pVm->sAllocator);` |
|  391 | 1664 | `	SyBlobInit(&pZip->sPassword,&pVm->sAllocator);` |
|  391 | 1665 | `	pZip->nRef = 1;` |
|  391 | 1666 | `	pZip->bOpen = 1;` |
|  391 | 1667 | `	pZip->pNext = (phl_zip *)pVm->pZips;` |
|  391 | 1668 | `	pVm->pZips = (void *)pZip;` |
|  391 | 1669 | `	return pZip;` |
|  197 | 1670 | `}` |
|    - | 1671 | `/* The VM is going away or being reset: nothing is left to write a pending` |
|    - | 1672 | ` * change to, so every archive is simply dropped. php's own shutdown commits` |
|    - | 1673 | ` * one an object still holds, and the object's teardown is what does that here,` |
|    - | 1674 | ` * running before this sweep. */` |
| 6717 | 1675 | `static void ZipVmSweep(ph7_vm *pVm)` |
|    5 | 1676 | `{` |
| 6724 | 1677 | `	while( pVm->pZips ){` |
|    3 | 1678 | `		phl_zip *pZip = (phl_zip *)pVm->pZips;` |
|    3 | 1679 | `		pZip->nRef = 1;` |
|    3 | 1680 | `		ZipRelease(pZip);` |
|    1 | 1681 | `	}` |
| 6722 | 1682 | `}` |
|   16 | 1683 | `PH7_PRIVATE void PH7_ZipVmReset(ph7_vm *pVm)` |
|  ! 0 | 1684 | `{` |
|   16 | 1685 | `	ZipVmSweep(&(*pVm));` |
|   16 | 1686 | `}` |
| 6701 | 1687 | `PH7_PRIVATE void PH7_ZipVmRelease(ph7_vm *pVm)` |
|    5 | 1688 | `{` |
| 6706 | 1689 | `	ZipVmSweep(&(*pVm));` |
| 6706 | 1690 | `}` |
|    - | 1691 | `/*` |
|    - | 1692 | ` * Write the archive back to the file it names.` |
|    - | 1693 | ` *` |
|    - | 1694 | ` * An archive with NO entries left is REMOVED rather than written: php's` |
|    - | 1695 | `` * `close()` on a freshly CREATEd archive nobody added to leaves no file behind`` |
|    - | 1696 | ` * at all, and so does one whose every entry was deleted.` |
|    - | 1697 | ` */` |
|    - | 1698 | `/* Has anything been asked of this archive that the FILE does not already say?` |
|    - | 1699 | ` * php writes nothing when the answer is no -- an archive opened, read and` |
|    - | 1700 | ` * closed is byte-identical afterwards -- which is also what keeps a close from` |
|    - | 1701 | ` * failing on an entry it could not have re-compressed. */` |
|  306 | 1702 | `static int ZipDirty(phl_zip *pZip)` |
|    3 | 1703 | `{` |
|    - | 1704 | `	sxu32 i;` |
|  309 | 1705 | `	if( pZip->bCommentChanged \|\| pZip->bCreated ){` |
|   35 | 1706 | `		return 1;` |
|    - | 1707 | `	}` |
| 1046 | 1708 | `	for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|  792 | 1709 | `		phl_zip_ent *pEnt = pZip->apEnt[i];` |
|  790 | 1710 | `		if( pEnt->bNew \|\| pEnt->bDeleted \|\| pEnt->bDataChanged \|\| pEnt->bNameChanged` |
|  776 | 1711 | `		 \|\| pEnt->bCommentChanged \|\| pEnt->bTimeChanged \|\| pEnt->bAttrChanged` |
|  776 | 1712 | `		 \|\| (pEnt->iSetMethod != ZIP_CM_DEFAULT && pEnt->iSetMethod != pEnt->iMethod)` |
|  777 | 1713 | `		 \|\| (pEnt->iSetEncrypt >= 0 && pEnt->iSetEncrypt != pEnt->iEncMethod) ){` |
|   20 | 1714 | `			return 1;` |
|    - | 1715 | `		}` |
|  387 | 1716 | `	}` |
|  256 | 1717 | `	return 0;` |
|  155 | 1718 | `}` |
|  314 | 1719 | `static int ZipCommit(phl_zip *pZip)` |
|    3 | 1720 | `{` |
|  317 | 1721 | `	ph7_vm *pVm = pZip->pVm;` |
|    - | 1722 | `	const ph7_io_stream *pStream;` |
|    - | 1723 | `	SyBlob sOut;` |
|    - | 1724 | `	const char *zPath;` |
|    - | 1725 | `	void *pHandle;` |
|  317 | 1726 | `	sxu32 i,nLive = 0;` |
|    - | 1727 | `	int rc;` |
| 1191 | 1728 | `	for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|  877 | 1729 | `		if( !pZip->apEnt[i]->bDeleted ){` |
|  865 | 1730 | `			nLive++;` |
|  431 | 1731 | `		}` |
|  440 | 1732 | `	}` |
|  317 | 1733 | `	SyBlobNullAppend(&pZip->sPath);` |
|  317 | 1734 | `	zPath = (const char *)SyBlobData(&pZip->sPath);` |
|  317 | 1735 | `	if( nLive == 0 ){` |
|   11 | 1736 | `		const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|   11 | 1737 | `		if( pZip->bCreated ){` |
|    5 | 1738 | `			return ZIP_ER_OK;   /* nothing was ever there to remove */` |
|    - | 1739 | `		}` |
|    7 | 1740 | `		if( pVfs && pVfs->xUnlink && pVfs->xUnlink(zPath) != PH7_OK ){` |
|  ! 0 | 1741 | `			pZip->iStatusSys = errno;` |
|  ! 0 | 1742 | `			return ZIP_ER_REMOVE;` |
|    - | 1743 | `		}` |
|    7 | 1744 | `		return ZIP_ER_OK;` |
|    - | 1745 | `	}` |
|  307 | 1746 | `	if( !ZipDirty(pZip) \|\| pZip->bBuilding ){` |
|    - | 1747 | `		/* bBuilding: a progress or cancel callback that closes the very archive` |
|    - | 1748 | `		 * being written would otherwise start a second build inside the first. */` |
|  256 | 1749 | `		return ZIP_ER_OK;` |
|    - | 1750 | `	}` |
|   53 | 1751 | `	pZip->bBuilding = 1;` |
|   53 | 1752 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|   53 | 1753 | `	rc = ZipBuild(pZip,&sOut);` |
|   53 | 1754 | `	pZip->bBuilding = 0;` |
|   53 | 1755 | `	if( rc != ZIP_ER_OK ){` |
|    3 | 1756 | `		SyBlobRelease(&sOut);` |
|    3 | 1757 | `		return rc;` |
|    - | 1758 | `	}` |
|   51 | 1759 | `	pStream = PH7_VmGetStreamDevice(pVm,&zPath,(int)SyBlobLength(&pZip->sPath));` |
|   51 | 1760 | `	if( pStream == 0 ){` |
|  ! 0 | 1761 | `		SyBlobRelease(&sOut);` |
|  ! 0 | 1762 | `		return ZIP_ER_OPEN;` |
|    - | 1763 | `	}` |
|   51 | 1764 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,` |
|    - | 1765 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,0);` |
|   51 | 1766 | `	if( pHandle == 0 ){` |
|  ! 0 | 1767 | `		SyBlobRelease(&sOut);` |
|    - | 1768 | `		/* php's own answer for a directory it cannot write the temporary into:` |
|    - | 1769 | `		 * libzip creates one beside the archive and reports ER_TMPOPEN with the` |
|    - | 1770 | `		 * system errno behind it. */` |
|  ! 0 | 1771 | `		pZip->iStatusSys = errno;` |
|  ! 0 | 1772 | `		return ZIP_ER_TMPOPEN;` |
|    - | 1773 | `	}` |
|   51 | 1774 | `	if( pStream->xWrite ){` |
|   51 | 1775 | `		pStream->xWrite(pHandle,SyBlobData(&sOut),(ph7_int64)SyBlobLength(&sOut));` |
|   24 | 1776 | `	}` |
|   51 | 1777 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|   51 | 1778 | `	SyBlobRelease(&sOut);` |
|   51 | 1779 | `	return ZIP_ER_OK;` |
|  160 | 1780 | `}` |
|    - | 1781 | `/* ------------------------------------------------------------------ */` |
|    - | 1782 | `/* The object behind $this                                             */` |
|    - | 1783 | `/* ------------------------------------------------------------------ */` |
|    - | 1784 | `/* The hidden slot the archive hangs off. */` |
|    - | 1785 | `#define ZIP_RES "__res"` |
| 2952 | 1786 | `static phl_zip * ZipOfInstance(ph7_class_instance *pThis)` |
|    4 | 1787 | `{` |
|    - | 1788 | `	SyString sAttr;` |
|    - | 1789 | `	ph7_value *pRes;` |
| 2956 | 1790 | `	if( pThis == 0 ){` |
|  ! 0 | 1791 | `		return 0;` |
|    - | 1792 | `	}` |
| 2956 | 1793 | `	SyStringInitFromBuf(&sAttr,ZIP_RES,sizeof(ZIP_RES)-1);` |
| 2956 | 1794 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
| 2956 | 1795 | `	if( pRes == 0 \|\| !ph7_value_is_resource(pRes) ){` |
|  740 | 1796 | `		return 0;` |
|    - | 1797 | `	}` |
| 2219 | 1798 | `	return (phl_zip *)ph7_value_to_resource(pRes);` |
| 1480 | 1799 | `}` |
|  628 | 1800 | `static int ZipAttach(ph7_class_instance *pThis,phl_zip *pZip)` |
|    3 | 1801 | `{` |
|    - | 1802 | `	SyString sAttr;` |
|    - | 1803 | `	ph7_value *pRes;` |
|  631 | 1804 | `	if( pThis == 0 ){` |
|  ! 0 | 1805 | `		return -1;` |
|    - | 1806 | `	}` |
|  631 | 1807 | `	SyStringInitFromBuf(&sAttr,ZIP_RES,sizeof(ZIP_RES)-1);` |
|  631 | 1808 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|  631 | 1809 | `	if( pRes == 0 ){` |
|  ! 0 | 1810 | `		return -1;` |
|    - | 1811 | `	}` |
|  631 | 1812 | `	PH7_MemObjRelease(pRes);` |
|  631 | 1813 | `	if( pZip ){` |
|  317 | 1814 | `		pRes->x.pOther = pZip;` |
|  317 | 1815 | `		MemObjSetType(pRes,MEMOBJ_RES);` |
|  157 | 1816 | `	}` |
|  631 | 1817 | `	return 0;` |
|  317 | 1818 | `}` |
|    - | 1819 | `/* One of the six state slots, written from C. They are declared properties on` |
|    - | 1820 | ` * the class and php keeps them live through a read handler; here the C bodies` |
|    - | 1821 | ` * write them after every verb that could move one, which is the same fact from` |
|    - | 1822 | ` * the other side and costs a store rather than a hook. */` |
|    - | 1823 | `#define ZipSetPropInt(pThis,zName,iVal) \` |
|    - | 1824 | `	PH7_NativeSetAttrInt((pThis)->pVm,(pThis),(zName),(iVal))` |
|    - | 1825 | `#define ZipSetPropStr(pThis,zName,z,n) \` |
|    - | 1826 | `	PH7_NativeSetAttrStr((pThis)->pVm,(pThis),(zName),(z),(int)(n))` |
|    - | 1827 | `/*` |
|    - | 1828 | ` * Bring the six php-visible slots up to date. Every verb that can move one ends` |
|    - | 1829 | `` * here, which is why a script never sees a stale `numFiles`.`` |
|    - | 1830 | ` *` |
|    - | 1831 | ` * A CLOSED handle answers the same six as a fresh object: php's read handler` |
|    - | 1832 | ``  * has no archive to ask and falls back to the declared zeroes, so `numFiles` `` |
|    - | 1833 | `` * is 0 and `filename` is "" the moment `close()` returns.`` |
|    - | 1834 | ` */` |
| 1282 | 1835 | `static void ZipRefresh(ph7_class_instance *pThis,phl_zip *pZip)` |
|    4 | 1836 | `{` |
| 1286 | 1837 | `	if( pThis == 0 ){` |
|  ! 0 | 1838 | `		return;` |
|    - | 1839 | `	}` |
| 1286 | 1840 | `	if( pZip == 0 \|\| !pZip->bOpen ){` |
|  734 | 1841 | `		ZipSetPropInt(pThis,"lastId",-1);` |
|  734 | 1842 | `		ZipSetPropInt(pThis,"status",pZip ? pZip->iStatus : 0);` |
|  734 | 1843 | `		ZipSetPropInt(pThis,"statusSys",pZip ? pZip->iStatusSys : 0);` |
|  734 | 1844 | `		ZipSetPropInt(pThis,"numFiles",0);` |
|  734 | 1845 | `		ZipSetPropStr(pThis,"filename","",0);` |
|  734 | 1846 | `		ZipSetPropStr(pThis,"comment","",0);` |
|  734 | 1847 | `		return;` |
|    - | 1848 | `	}` |
|  555 | 1849 | `	ZipSetPropInt(pThis,"lastId",pZip->iLastId);` |
|  555 | 1850 | `	ZipSetPropInt(pThis,"status",pZip->iStatus);` |
|  555 | 1851 | `	ZipSetPropInt(pThis,"statusSys",pZip->iStatusSys);` |
|  555 | 1852 | `	ZipSetPropInt(pThis,"numFiles",(sxi64)pZip->nEnt);` |
|  555 | 1853 | `	ZipSetPropStr(pThis,"filename",(const char *)SyBlobData(&pZip->sPath),` |
|    - | 1854 | `		SyBlobLength(&pZip->sPath));` |
|  555 | 1855 | `	ZipSetPropStr(pThis,"comment",(const char *)SyBlobData(&pZip->sComment),` |
|    - | 1856 | `		SyBlobLength(&pZip->sComment));` |
|  645 | 1857 | `}` |
|    - | 1858 | `/*` |
|    - | 1859 | ` * The archive a method is being asked about, or php's refusal.` |
|    - | 1860 | ` *` |
|    - | 1861 | ` * php's every ZipArchive method but one starts by checking that the object has` |
|    - | 1862 | `` * a live archive behind it and throws `ValueError: Invalid or uninitialized Zip`` |
|    - | 1863 | `` * object` when it does not -- `close()` and `count()` included, which is what`` |
|    - | 1864 | ` * makes calling either on a fresh object an Error rather than a false. The one` |
|    - | 1865 | `` * exception is `getStatusString()`, which answers "No error" for an object that`` |
|    - | 1866 | ` * was never opened.` |
|    - | 1867 | ` */` |
| 2170 | 1868 | `static phl_zip * ZipThis(ph7_context *pCtx,ph7_class_instance **ppThis)` |
|    4 | 1869 | `{` |
| 2174 | 1870 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
| 2174 | 1871 | `	phl_zip *pZip = ZipOfInstance(pThis);` |
| 2174 | 1872 | `	if( ppThis ){` |
|  424 | 1873 | `		*ppThis = pThis;` |
|  210 | 1874 | `	}` |
| 2174 | 1875 | `	if( pZip == 0 \|\| !pZip->bOpen ){` |
|    5 | 1876 | `		PH7_VmThrowException(pCtx,"ValueError","Invalid or uninitialized Zip object");` |
|    5 | 1877 | `		return 0;` |
|    - | 1878 | `	}` |
| 2169 | 1879 | `	return pZip;` |
| 1089 | 1880 | `}` |
|    - | 1881 | `/* The object is going away. php's own teardown COMMITS a still-open archive --` |
|    - | 1882 | ` * a script that adds files and never calls close() still gets its archive --` |
|    - | 1883 | ` * and it invalidates every stream getStream() handed out, which is what makes` |
|    - | 1884 | ``  * reading one after the object dies the `Containing zip archive was closed` `` |
|    - | 1885 | ` * error rather than more bytes. */` |
|  360 | 1886 | `static void ZipInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    4 | 1887 | `{` |
|  364 | 1888 | `	phl_zip *pZip = ZipOfInstance(pThis);` |
|  180 | 1889 | `	SXUNUSED(pVm);` |
|  364 | 1890 | `	if( pZip == 0 ){` |
|  364 | 1891 | `		return;` |
|    - | 1892 | `	}` |
|  ! 0 | 1893 | `	if( pZip->bOpen ){` |
|  ! 0 | 1894 | `		ZipCommit(pZip);` |
|  ! 0 | 1895 | `		pZip->bOpen = 0;` |
|  ! 0 | 1896 | `		pZip->bDead = 1;` |
|  ! 0 | 1897 | `	}` |
|  ! 0 | 1898 | `	ZipAttach(pThis,0);` |
|  ! 0 | 1899 | `	ZipRelease(pZip);` |
|  184 | 1900 | `}` |
|    - | 1901 | `/*` |
|    - | 1902 | ` * A fresh object, before any open. php's read handler has no archive to ask and` |
|    - | 1903 | ` * answers the six with -1, 0, 0, 0, "" and "" -- so those are what the slots` |
|    - | 1904 | ` * start holding here, seeded by the create_object hook. They are a STARTING` |
|    - | 1905 | ` * VALUE and not a declared default, which is why the declarations above carry` |
|    - | 1906 | `` * none and Reflection's `hasDefaultValue()` answers false for all six.`` |
|    - | 1907 | ` */` |
|  362 | 1908 | `static void ZipNewHook(ph7_vm *pVm,ph7_class_instance *pThis)` |
|    4 | 1909 | `{` |
|  181 | 1910 | `	SXUNUSED(pVm);` |
|  366 | 1911 | `	ZipRefresh(pThis,0);` |
|  366 | 1912 | `}` |
|    - | 1913 | `/*` |
|    - | 1914 | ` * php refuses a write to any of the six: its write_property handler answers` |
|    - | 1915 | `` * `Cannot write read-only property ZipArchive::$numFiles` and stores nothing,`` |
|    - | 1916 | ` * while Reflection still reports the property as neither readonly nor virtual.` |
|    - | 1917 | ` * The C bodies above write their slots directly and never pass this filter.` |
|    - | 1918 | ` */` |
|    2 | 1919 | `static void ZipSetHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeSetCtx *pCtx)` |
|    1 | 1920 | `{` |
|    1 | 1921 | `	SXUNUSED(pVm);` |
|    3 | 1922 | `	pCtx->zThrowClass = "Error";` |
|    4 | 1923 | `	SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|    2 | 1924 | `		"Cannot write read-only property %z::$%z",&pThis->pClass->sDisp,pCtx->pName);` |
|    3 | 1925 | `}` |
|    - | 1926 | `/* ------------------------------------------------------------------ */` |
|    - | 1927 | `/* Errors                                                              */` |
|    - | 1928 | `/* ------------------------------------------------------------------ */` |
|    - | 1929 | `/*` |
|    - | 1930 | `` * libzip's error table, which is what `getStatusString()` reads and the only`` |
|    - | 1931 | ` * part of this extension a script can see that is not the FORMAT. It has not` |
|    - | 1932 | ` * changed in the library's lifetime, and php prints these sentences verbatim.` |
|    - | 1933 | ` *` |
|    - | 1934 | ` * Nine of the codes are SYSTEM errors -- the ones raised by a call to the` |
|    - | 1935 | `` * operating system -- and those print as `sentence: strerror(statusSys)`, which`` |
|    - | 1936 | `` * is why a close() that cannot write its temporary file reads `Failure to`` |
|    - | 1937 | `` * create temporary file: Permission denied` rather than half of that.`` |
|    - | 1938 | ` */` |
|   54 | 1939 | `static const char * ZipErrString(int iErr)` |
|    3 | 1940 | `{` |
|   57 | 1941 | `	switch( iErr ){` |
|    6 | 1942 | `	case ZIP_ER_OK:              return "No error";` |
|  ! 0 | 1943 | `	case ZIP_ER_MULTIDISK:       return "Multi-disk zip archives not supported";` |
|  ! 0 | 1944 | `	case ZIP_ER_RENAME:          return "Renaming temporary file failed";` |
|  ! 0 | 1945 | `	case ZIP_ER_CLOSE:           return "Closing zip archive failed";` |
|  ! 0 | 1946 | `	case ZIP_ER_SEEK:            return "Seek error";` |
|  ! 0 | 1947 | `	case ZIP_ER_READ:            return "Read error";` |
|  ! 0 | 1948 | `	case ZIP_ER_WRITE:           return "Write error";` |
|  ! 0 | 1949 | `	case ZIP_ER_CRC:             return "CRC error";` |
|  ! 0 | 1950 | `	case ZIP_ER_ZIPCLOSED:       return "Containing zip archive was closed";` |
|   10 | 1951 | `	case ZIP_ER_NOENT:           return "No such file";` |
|  ! 0 | 1952 | `	case ZIP_ER_EXISTS:          return "File already exists";` |
|  ! 0 | 1953 | `	case ZIP_ER_OPEN:            return "Can't open file";` |
|  ! 0 | 1954 | `	case ZIP_ER_TMPOPEN:         return "Failure to create temporary file";` |
|  ! 0 | 1955 | `	case ZIP_ER_ZLIB:            return "Zlib error";` |
|  ! 0 | 1956 | `	case ZIP_ER_MEMORY:          return "Malloc failure";` |
|    3 | 1957 | `	case ZIP_ER_CHANGED:         return "Entry has been changed";` |
|    3 | 1958 | `	case ZIP_ER_COMPNOTSUPP:     return "Compression method not supported";` |
|  ! 0 | 1959 | `	case ZIP_ER_EOF:             return "Premature end of file";` |
|   12 | 1960 | `	case ZIP_ER_INVAL:           return "Invalid argument";` |
|  ! 0 | 1961 | `	case ZIP_ER_NOZIP:           return "Not a zip archive";` |
|  ! 0 | 1962 | `	case ZIP_ER_INTERNAL:        return "Internal error";` |
|  ! 0 | 1963 | `	case ZIP_ER_INCONS:          return "Zip archive inconsistent";` |
|  ! 0 | 1964 | `	case ZIP_ER_REMOVE:          return "Can't remove file";` |
|  ! 0 | 1965 | `	case ZIP_ER_DELETED:         return "Entry has been deleted";` |
|    5 | 1966 | `	case ZIP_ER_ENCRNOTSUPP:     return "Encryption method not supported";` |
|  ! 0 | 1967 | `	case ZIP_ER_RDONLY:          return "Read-only archive";` |
|    9 | 1968 | `	case ZIP_ER_NOPASSWD:        return "No password provided";` |
|   17 | 1969 | `	case ZIP_ER_WRONGPASSWD:     return "Wrong password provided";` |
|  ! 0 | 1970 | `	case ZIP_ER_OPNOTSUPP:       return "Operation not supported";` |
|  ! 0 | 1971 | `	case ZIP_ER_INUSE:           return "Resource still in use";` |
|  ! 0 | 1972 | `	case ZIP_ER_TELL:            return "Tell error";` |
|  ! 0 | 1973 | `	case ZIP_ER_COMPRESSED_DATA: return "Compressed data invalid";` |
|  ! 0 | 1974 | `	case ZIP_ER_CANCELLED:       return "Operation cancelled";` |
|  ! 0 | 1975 | `	default:                     break;` |
|    - | 1976 | `	}` |
|  ! 0 | 1977 | `	return "Unknown error";` |
|   30 | 1978 | `}` |
|  140 | 1979 | `static int ZipErrIsSys(int iErr)` |
|    3 | 1980 | `{` |
|  143 | 1981 | `	switch( iErr ){` |
|  ! 0 | 1982 | `	case ZIP_ER_RENAME: case ZIP_ER_CLOSE: case ZIP_ER_SEEK:` |
|    - | 1983 | `	case ZIP_ER_READ:   case ZIP_ER_WRITE: case ZIP_ER_OPEN:` |
|    - | 1984 | `	case ZIP_ER_TMPOPEN: case ZIP_ER_REMOVE: case ZIP_ER_TELL:` |
|  ! 0 | 1985 | `		return 1;` |
|   70 | 1986 | `	default:` |
|  143 | 1987 | `		return 0;` |
|    - | 1988 | `	}` |
|   73 | 1989 | `}` |
|   54 | 1990 | `static void ZipErrText(int iErr,int iSys,SyBlob *pOut)` |
|    3 | 1991 | `{` |
|   57 | 1992 | `	const char *z = ZipErrString(iErr);` |
|   57 | 1993 | `	SyBlobAppend(pOut,z,(sxu32)SyStrlen(z));` |
|   57 | 1994 | `	if( ZipErrIsSys(iErr) && iSys != 0 ){` |
|  ! 0 | 1995 | `		const char *zSys = VfsStrerror(iSys);` |
|  ! 0 | 1996 | `		SyBlobAppend(pOut,": ",sizeof(": ")-1);` |
|  ! 0 | 1997 | `		SyBlobAppend(pOut,zSys,(sxu32)SyStrlen(zSys));` |
|  ! 0 | 1998 | `	}` |
|   57 | 1999 | `}` |
|    - | 2000 | `/*` |
|    - | 2001 | `` * Remember a failure on the archive. php's `$z->status` is STICKY: a successful`` |
|    - | 2002 | ` * call after a failed one leaves the old code in place, and only clearError()` |
|    - | 2003 | ` * or the next failure moves it.` |
|    - | 2004 | ` */` |
|   86 | 2005 | `static int ZipFail(ph7_context *pCtx,phl_zip *pZip,int iErr)` |
|    2 | 2006 | `{` |
|   88 | 2007 | `	pZip->iStatus = iErr;` |
|   88 | 2008 | `	if( !ZipErrIsSys(iErr) ){` |
|   88 | 2009 | `		pZip->iStatusSys = 0;` |
|   43 | 2010 | `	}` |
|   88 | 2011 | `	ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|   88 | 2012 | `	ph7_result_bool(pCtx,0);` |
|   88 | 2013 | `	return PH7_OK;` |
|    2 | 2014 | `}` |
|    - | 2015 | `/* ------------------------------------------------------------------ */` |
|    - | 2016 | `/* The archive's own verbs                                             */` |
|    - | 2017 | `/* ------------------------------------------------------------------ */` |
|    - | 2018 | ``/* Which entry a `*Index` method was asked about, with php's refusal when the`` |
|    - | 2019 | ` * index names nothing: ER_INVAL, and false. */` |
| 1454 | 2020 | `static phl_zip_ent * ZipArgIndex(ph7_context *pCtx,phl_zip *pZip,ph7_value *pArg,int *pRc)` |
|    2 | 2021 | `{` |
| 1456 | 2022 | `	phl_zip_ent *pEnt = ZipAt(pZip,ph7_value_to_int64(pArg));` |
| 1456 | 2023 | `	*pRc = PH7_OK;` |
| 1456 | 2024 | `	if( pEnt == 0 ){` |
|    7 | 2025 | `		*pRc = ZipFail(pCtx,pZip,ZIP_ER_INVAL);` |
|    7 | 2026 | `		return 0;` |
|    - | 2027 | `	}` |
| 1450 | 2028 | `	return pEnt;` |
|  729 | 2029 | `}` |
|    - | 2030 | `/*` |
|    - | 2031 | `` * A name php PUTS in the archive is a C string: it hands the library a `char *`,`` |
|    - | 2032 | `` * so `addFromString("x\0y", …)` stores `x` and nothing after it. Six doors that`` |
|    - | 2033 | ` * LOOK a name up screen the NUL instead and throw; the rest simply match the` |
|    - | 2034 | ` * truncated form, which is the same entry.` |
|    - | 2035 | ` */` |
|  396 | 2036 | `static sxu32 ZipCName(const char *zName,int nName)` |
|    3 | 2037 | `{` |
|    - | 2038 | `	int i;` |
| 3552 | 2039 | `	for( i = 0 ; i < nName ; ++i ){` |
| 3182 | 2040 | `		if( zName[i] == 0 ){` |
|   27 | 2041 | `			return (sxu32)i;` |
|    - | 2042 | `		}` |
| 1692 | 2043 | `	}` |
|  373 | 2044 | `	return (sxu32)(nName < 0 ? 0 : nName);` |
|  201 | 2045 | `}` |
|    - | 2046 | `/* ...and the screen those six raise instead. */` |
|  134 | 2047 | `static int ZipNulName(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,int *pRc)` |
|    2 | 2048 | `{` |
|  136 | 2049 | `	int n = 0;` |
|  136 | 2050 | `	const char *z = ph7_value_to_string(pArg,&n);` |
|  136 | 2051 | `	if( n < 1 \|\| (int)ZipCName(z,n) == n ){` |
|  126 | 2052 | `		return 0;` |
|    - | 2053 | `	}` |
|   21 | 2054 | `	*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2055 | `		"%z(): Argument #%d ($%s) must not contain any null bytes",` |
|   10 | 2056 | `		&pCtx->pFunc->sName,iPos,zName);` |
|   11 | 2057 | `	return 1;` |
|   69 | 2058 | `}` |
|    - | 2059 | `/*` |
|    - | 2060 | `` * php's empty-NAME screen. Most of the `*Name` doors refuse one outright with a`` |
|    - | 2061 | `` * ValueError that names the argument -- and four of them do not: `locateName`,`` |
|    - | 2062 | `` * `deleteName`, `unchangeName` and `addEmptyDir` answer false, and`` |
|    - | 2063 | `` * `addFromString` takes the empty name and stores it.`` |
|    - | 2064 | ` */` |
|  170 | 2065 | `static int ZipEmptyName(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,int *pRc)` |
|    2 | 2066 | `{` |
|  172 | 2067 | `	int n = 0;` |
|  172 | 2068 | `	ph7_value_to_string(pArg,&n);` |
|  172 | 2069 | `	if( n > 0 ){` |
|  172 | 2070 | `		return 0;` |
|    - | 2071 | `	}` |
|  ! 0 | 2072 | `	*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 2073 | `		"%z(): Argument #%d ($%s) must not be empty",&pCtx->pFunc->sName,iPos,zName);` |
|  ! 0 | 2074 | `	return 1;` |
|   87 | 2075 | `}` |
|    - | 2076 | ``/* ...and the same for a `*Name` method, whose miss is ER_NOENT. */`` |
|  172 | 2077 | `static phl_zip_ent * ZipArgName(ph7_context *pCtx,phl_zip *pZip,ph7_value *pArg,int iFlags,int *pRc)` |
|    2 | 2078 | `{` |
|  174 | 2079 | `	int nName = 0;` |
|  174 | 2080 | `	const char *zName = ph7_value_to_string(pArg,&nName);` |
|    - | 2081 | `	/* The doors that do NOT screen a NUL still hand the library a C string, so` |
|    - | 2082 | `	 * the name they look up is the one before it. */` |
|  174 | 2083 | `	phl_zip_ent *pEnt = ZipFind(pZip,zName,ZipCName(zName,nName),iFlags,0);` |
|  174 | 2084 | `	*pRc = PH7_OK;` |
|  174 | 2085 | `	if( pEnt == 0 ){` |
|    6 | 2086 | `		*pRc = ZipFail(pCtx,pZip,ZIP_ER_NOENT);` |
|    6 | 2087 | `		return 0;` |
|    - | 2088 | `	}` |
|  170 | 2089 | `	return pEnt;` |
|   88 | 2090 | `}` |
|    - | 2091 | `/*` |
|    - | 2092 | `` * `ZipArchive::open()`.`` |
|    - | 2093 | ` *` |
|    - | 2094 | ` * Answers TRUE, or one of php's ER_ codes as an INT -- not false, which is why` |
|    - | 2095 | `` * `if (!$z->open(...))` is the wrong test and `!== true` the right one.`` |
|    - | 2096 | ` *` |
|    - | 2097 | ` * Opening on an object that already holds an archive silently CLOSES the first,` |
|    - | 2098 | ` * commits included, and only then looks at the new name.` |
|    - | 2099 | ` */` |
|  370 | 2100 | `static int vm_builtin_ZipArchive_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 2101 | `{` |
|  373 | 2102 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  373 | 2103 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|  373 | 2104 | `	phl_zip *pOld = ZipOfInstance(pThis);` |
|    - | 2105 | `	phl_zip *pZip;` |
|    - | 2106 | `	const char *zName;` |
|  373 | 2107 | `	int nName = 0,iFlags = 0,rc;` |
|    - | 2108 | `	SyBlob sPath;` |
|  185 | 2109 | `	SXUNUSED(nArg);` |
|  373 | 2110 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|  373 | 2111 | `	if( nName < 1 ){` |
|  ! 0 | 2112 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 2113 | `			"%z(): Argument #1 ($filename) must not be empty",&pCtx->pFunc->sName);` |
|    - | 2114 | `	}` |
|  373 | 2115 | `	if( SyByteFind(zName,(sxu32)nName,0,0) == SXRET_OK ){` |
|  ! 0 | 2116 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 2117 | `			"%z(): Argument #1 ($filename) must not contain any null bytes",` |
|  ! 0 | 2118 | `			&pCtx->pFunc->sName);` |
|    - | 2119 | `	}` |
|  373 | 2120 | `	if( nArg > 1 ){` |
|   51 | 2121 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|   24 | 2122 | `	}` |
|  373 | 2123 | `	if( pOld ){` |
|    3 | 2124 | `		if( pOld->bOpen ){` |
|    3 | 2125 | `			ZipCommit(pOld);` |
|    3 | 2126 | `			pOld->bOpen = 0;` |
|    1 | 2127 | `		}` |
|    3 | 2128 | `		ZipAttach(pThis,0);` |
|    3 | 2129 | `		ZipRelease(pOld);` |
|    1 | 2130 | `	}` |
|  373 | 2131 | `	PH7_VfsExpandPath(pCtx,zName,nName,&sPath);` |
|  373 | 2132 | `	pZip = ZipNew(pCtx->pVm);` |
|  373 | 2133 | `	if( pZip == 0 ){` |
|  ! 0 | 2134 | `		SyBlobRelease(&sPath);` |
|  ! 0 | 2135 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2136 | `	}` |
|  373 | 2137 | `	SyBlobAppend(&pZip->sPath,SyBlobData(&sPath),SyBlobLength(&sPath));` |
|  373 | 2138 | `	SyBlobRelease(&sPath);` |
|  373 | 2139 | `	SyBlobNullAppend(&pZip->sPath);` |
|  373 | 2140 | `	pZip->bRdonly = (iFlags & ZIP_OPEN_RDONLY) != 0;` |
|    - | 2141 | `	{` |
|  373 | 2142 | `		const char *zPath = (const char *)SyBlobData(&pZip->sPath);` |
|    - | 2143 | `		int bExists,iWhy;` |
|  373 | 2144 | `		errno = 0;` |
|  373 | 2145 | `		bExists = pVfs && pVfs->xFileExists && pVfs->xFileExists(zPath) == PH7_OK;` |
|  373 | 2146 | `		iWhy = errno;` |
|  373 | 2147 | `		if( pVfs && pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK ){` |
|    - | 2148 | `			/* A directory is not an archive and never could be: php's answer is` |
|    - | 2149 | `			 * the one it gives to any source it cannot use at all. */` |
|    3 | 2150 | `			rc = ZIP_ER_OPNOTSUPP;` |
|    3 | 2151 | `			goto failed;` |
|    - | 2152 | `		}` |
|  371 | 2153 | `		if( bExists && (iFlags & ZIP_OPEN_EXCL) && (iFlags & ZIP_OPEN_CREATE) ){` |
|    3 | 2154 | `			rc = ZIP_ER_EXISTS;` |
|    3 | 2155 | `			goto failed;` |
|    - | 2156 | `		}` |
|  369 | 2157 | `		if( !bExists ){` |
|   41 | 2158 | `			if( iWhy != 0 && iWhy != ENOENT ){` |
|    - | 2159 | `				/* The name could not be LOOKED at -- a directory on the way to` |
|    - | 2160 | `				 * it refuses to be searched. php answers the reason it could` |
|    - | 2161 | `				 * not read rather than "no such file", whatever the flags say:` |
|    - | 2162 | `				 * a CREATE cannot help either. */` |
|  ! 0 | 2163 | `				rc = ZIP_ER_READ;` |
|  ! 0 | 2164 | `				goto failed;` |
|    - | 2165 | `			}` |
|   41 | 2166 | `			if( (iFlags & ZIP_OPEN_CREATE) == 0 ){` |
|    3 | 2167 | `				rc = ZIP_ER_NOENT;` |
|    3 | 2168 | `				goto failed;` |
|    - | 2169 | `			}` |
|   39 | 2170 | `			pZip->bCreated = 1;` |
|  348 | 2171 | `		}else if( iFlags & ZIP_OPEN_OVERWRITE ){` |
|    - | 2172 | `			/* The file is there and its contents are not: php opens an EMPTY` |
|    - | 2173 | `			 * archive over it, and a close with nothing added removes it. */` |
|    5 | 2174 | `		}else{` |
|    - | 2175 | `			const ph7_io_stream *pStream;` |
|    - | 2176 | `			void *pHandle;` |
|  322 | 2177 | `			const char *zTail = zPath;` |
|  322 | 2178 | `			pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTail,(int)SyBlobLength(&pZip->sPath));` |
|  322 | 2179 | `			pHandle = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zTail,` |
|  160 | 2180 | `				PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,0) : 0;` |
|  322 | 2181 | `			if( pHandle == 0 ){` |
|  ! 0 | 2182 | `				rc = ZIP_ER_READ;` |
|  ! 0 | 2183 | `				goto failed;` |
|    - | 2184 | `			}` |
|  322 | 2185 | `			PH7_StreamReadWholeFile(pHandle,pStream,&pZip->sFile);` |
|  322 | 2186 | `			PH7_StreamCloseHandle(pStream,pHandle);` |
|  322 | 2187 | `			rc = ZipParse(pZip);` |
|  322 | 2188 | `			if( rc == ZIP_ER_EMPTY_FILE ){` |
|    3 | 2189 | `				ph7_context_throw_error_format(pCtx,8192 /* E_DEPRECATED */,` |
|    - | 2190 | `					"Using empty file as ZipArchive is deprecated");` |
|  321 | 2191 | `			}else if( rc != ZIP_ER_OK ){` |
|   51 | 2192 | `				goto failed;` |
|    - | 2193 | `			}` |
|    - | 2194 | `		}` |
|    - | 2195 | `	}` |
|  317 | 2196 | `	if( ZipAttach(pThis,pZip) != 0 ){` |
|  ! 0 | 2197 | `		ZipRelease(pZip);` |
|  ! 0 | 2198 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 2199 | `	}` |
|  317 | 2200 | `	pZip->iLastId = -1;` |
|  317 | 2201 | `	ZipRefresh(pThis,pZip);` |
|  317 | 2202 | `	ph7_result_bool(pCtx,1);` |
|  317 | 2203 | `	return PH7_OK;` |
|   28 | 2204 | `failed:` |
|    - | 2205 | `	/* An open that fails leaves the object exactly as it was -- it does not` |
|    - | 2206 | ``	 * even record the code in `status`, which is why the RETURN is the only`` |
|    - | 2207 | `	 * place the reason appears. */` |
|   57 | 2208 | `	ZipRelease(pZip);` |
|   57 | 2209 | `	ZipRefresh(pThis,0);` |
|   57 | 2210 | `	ph7_result_int(pCtx,rc);` |
|   57 | 2211 | `	return PH7_OK;` |
|  188 | 2212 | `}` |
|    - | 2213 | `/*` |
|    - | 2214 | `` * `close()`. It is the only place an archive is written, and a failure is BOTH`` |
|    - | 2215 | ` * a warning and a false -- php names the sentence libzip left behind. The` |
|    - | 2216 | ` * archive goes either way: a script cannot retry a close.` |
|    - | 2217 | ` */` |
|  314 | 2218 | `static int vm_builtin_ZipArchive_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    4 | 2219 | `{` |
|    - | 2220 | `	ph7_class_instance *pThis;` |
|  318 | 2221 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    - | 2222 | `	int rc;` |
|  157 | 2223 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|  318 | 2224 | `	if( pZip == 0 ){` |
|    3 | 2225 | `		return PH7_OK;` |
|    - | 2226 | `	}` |
|  315 | 2227 | `	rc = pZip->bRdonly ? ZIP_ER_OK : ZipCommit(pZip);` |
|  315 | 2228 | `	pZip->bOpen = 0;` |
|  315 | 2229 | `	if( rc == ZIP_ER_OK ){` |
|    - | 2230 | `		/* A close that WORKED clears the archive's error, which is the one` |
|    - | 2231 | ``		 * place php's otherwise-sticky `status` goes back to zero on its own. */`` |
|  313 | 2232 | `		pZip->iStatus = 0;` |
|  313 | 2233 | `		pZip->iStatusSys = 0;` |
|  155 | 2234 | `	}` |
|  315 | 2235 | `	if( rc != ZIP_ER_OK ){` |
|    - | 2236 | `		SyBlob sMsg;` |
|    3 | 2237 | `		pZip->iStatus = rc;` |
|    3 | 2238 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    3 | 2239 | `		ZipErrText(rc,pZip->iStatusSys,&sMsg);` |
|    3 | 2240 | `		SyBlobNullAppend(&sMsg);` |
|    4 | 2241 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",` |
|    2 | 2242 | `			(const char *)SyBlobData(&sMsg));` |
|    3 | 2243 | `		SyBlobRelease(&sMsg);` |
|    1 | 2244 | `	}` |
|    - | 2245 | `	/* The six slots go back to what a fresh object shows -- except the two the` |
|    - | 2246 | `	 * failure just wrote, which php leaves on the object to be read. */` |
|  315 | 2247 | `	ZipRefresh(pThis,0);` |
|  315 | 2248 | `	ZipSetPropInt(pThis,"status",pZip->iStatus);` |
|  315 | 2249 | `	ZipSetPropInt(pThis,"statusSys",pZip->iStatusSys);` |
|  315 | 2250 | `	ZipAttach(pThis,0);` |
|  315 | 2251 | `	ZipRelease(pZip);` |
|  315 | 2252 | `	ph7_result_bool(pCtx,rc == ZIP_ER_OK);` |
|  315 | 2253 | `	return PH7_OK;` |
|  161 | 2254 | `}` |
|    - | 2255 | `/* php's Countable half: the INDEX SPACE, deleted entries included, which is the` |
|    - | 2256 | `` * same number `numFiles` answers. */`` |
|    6 | 2257 | `static int vm_builtin_ZipArchive_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2258 | `{` |
|    8 | 2259 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    3 | 2260 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    8 | 2261 | `	if( pZip == 0 ){` |
|    3 | 2262 | `		return PH7_OK;` |
|    - | 2263 | `	}` |
|    5 | 2264 | `	ph7_result_int64(pCtx,(ph7_int64)pZip->nEnt);` |
|    5 | 2265 | `	return PH7_OK;` |
|    5 | 2266 | `}` |
|    - | 2267 | `/*` |
|    - | 2268 | `` * `getStatusString()`. The one verb that does NOT insist on a live archive:`` |
|    - | 2269 | ` * php answers "No error" for an object nobody ever opened, and keeps answering` |
|    - | 2270 | ` * for one whose close() failed -- which is the only way to read why it did.` |
|    - | 2271 | ` */` |
|   52 | 2272 | `static int vm_builtin_ZipArchive_getStatusString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 2273 | `{` |
|   55 | 2274 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   55 | 2275 | `	phl_zip *pZip = ZipOfInstance(pThis);` |
|    - | 2276 | `	SyBlob sMsg;` |
|   55 | 2277 | `	int iErr = 0,iSys = 0;` |
|   26 | 2278 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   55 | 2279 | `	if( pZip && pZip->bOpen ){` |
|   50 | 2280 | `		iErr = pZip->iStatus;` |
|   50 | 2281 | `		iSys = pZip->iStatusSys;` |
|   30 | 2282 | `	}else if( pThis ){` |
|    6 | 2283 | `		iErr = (int)PH7_NativeAttrInt(pThis,"status");` |
|    6 | 2284 | `		iSys = (int)PH7_NativeAttrInt(pThis,"statusSys");` |
|    2 | 2285 | `	}` |
|   55 | 2286 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|   55 | 2287 | `	ZipErrText(iErr,iSys,&sMsg);` |
|   55 | 2288 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sMsg),(int)SyBlobLength(&sMsg));` |
|   55 | 2289 | `	SyBlobRelease(&sMsg);` |
|   55 | 2290 | `	return PH7_OK;` |
|    3 | 2291 | `}` |
|   12 | 2292 | `static int vm_builtin_ZipArchive_clearError(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2293 | `{` |
|    - | 2294 | `	ph7_class_instance *pThis;` |
|   14 | 2295 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    6 | 2296 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   14 | 2297 | `	if( pZip == 0 ){` |
|  ! 0 | 2298 | `		return PH7_OK;` |
|    - | 2299 | `	}` |
|   14 | 2300 | `	pZip->iStatus = 0;` |
|   14 | 2301 | `	pZip->iStatusSys = 0;` |
|   14 | 2302 | `	ZipRefresh(pThis,pZip);` |
|   14 | 2303 | `	return PH7_OK;` |
|    8 | 2304 | `}` |
|    - | 2305 | `/* ------------------------------------------------------------------ */` |
|    - | 2306 | `/* Adding, replacing, removing                                         */` |
|    - | 2307 | `/* ------------------------------------------------------------------ */` |
|    - | 2308 | `/*` |
|    - | 2309 | ` * The slot an add is going to use: an existing entry when the caller asked for` |
|    - | 2310 | ` * an overwrite, and a fresh index otherwise. php's default flags for the two` |
|    - | 2311 | `` * add doors are FL_OVERWRITE, so `addFromString()` on a name already there`` |
|    - | 2312 | `` * REPLACES it and only an explicit `0` refuses with ER_EXISTS.`` |
|    - | 2313 | ` */` |
|   90 | 2314 | `static phl_zip_ent * ZipAddSlot(ph7_context *pCtx,phl_zip *pZip,const char *zName,` |
|    - | 2315 | `	sxu32 nName,int iFlags,int *pRc)` |
|    3 | 2316 | `{` |
|   93 | 2317 | `	sxu32 nIdx = 0;` |
|   93 | 2318 | `	phl_zip_ent *pEnt = ZipFind(pZip,zName,nName,0,&nIdx);` |
|   93 | 2319 | `	*pRc = PH7_OK;` |
|   93 | 2320 | `	if( pEnt ){` |
|    5 | 2321 | `		if( (iFlags & ZIP_FL_OVERWRITE) == 0 ){` |
|    - | 2322 | `			/* php stores whatever the library's add ANSWERED, and a refused one` |
|    - | 2323 | ``			 * answers -1: `lastId` is not the last successful index but the`` |
|    - | 2324 | `			 * last answer, so a duplicate add leaves it at -1. */` |
|    3 | 2325 | `			pZip->iLastId = -1;` |
|    3 | 2326 | `			*pRc = ZipFail(pCtx,pZip,ZIP_ER_EXISTS);` |
|    3 | 2327 | `			return 0;` |
|    - | 2328 | `		}` |
|    3 | 2329 | `		SyBlobReset(&pEnt->sData);` |
|    3 | 2330 | `		SyBlobReset(&pEnt->sSrcPath);` |
|    3 | 2331 | `		pEnt->iSrcStart = 0;` |
|    3 | 2332 | `		pEnt->iSrcLen = -1;` |
|    3 | 2333 | `		pEnt->bDataChanged = 1;` |
|    3 | 2334 | `		pEnt->bLoaded = 0;` |
|    3 | 2335 | `		pZip->iLastId = (int)nIdx;` |
|    3 | 2336 | `		return pEnt;` |
|    - | 2337 | `	}` |
|   89 | 2338 | `	pEnt = ZipEntNew(pZip,zName,(int)nName);` |
|   89 | 2339 | `	if( pEnt == 0 ){` |
|  ! 0 | 2340 | `		*pRc = ZipFail(pCtx,pZip,ZIP_ER_MEMORY);` |
|  ! 0 | 2341 | `		return 0;` |
|    - | 2342 | `	}` |
|   89 | 2343 | `	pEnt->bNew = 1;` |
|   89 | 2344 | `	pEnt->iTime = (sxi64)time(0);` |
|   89 | 2345 | `	pEnt->iOpsys = ZIP_OPSYS_UNIX;` |
|   89 | 2346 | `	pEnt->nAttr = ZIP_ATTR_FILE;` |
|   89 | 2347 | `	pZip->iLastId = (int)(pZip->nEnt - 1);` |
|   89 | 2348 | `	return pEnt;` |
|   48 | 2349 | `}` |
|   68 | 2350 | `static int vm_builtin_ZipArchive_addFromString(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 2351 | `{` |
|    - | 2352 | `	ph7_class_instance *pThis;` |
|   71 | 2353 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    - | 2354 | `	phl_zip_ent *pEnt;` |
|    - | 2355 | `	const char *zName,*zData;` |
|   71 | 2356 | `	int nName = 0,nData = 0,iFlags = ZIP_FL_OVERWRITE,rc;` |
|   71 | 2357 | `	if( pZip == 0 ){` |
|  ! 0 | 2358 | `		return PH7_OK;` |
|    - | 2359 | `	}` |
|   71 | 2360 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   71 | 2361 | `	zData = ph7_value_to_string(apArg[1],&nData);` |
|   71 | 2362 | `	if( nArg > 2 ){` |
|    3 | 2363 | `		iFlags = (int)ph7_value_to_int(apArg[2]);` |
|    1 | 2364 | `	}` |
|   71 | 2365 | `	pEnt = ZipAddSlot(pCtx,pZip,zName,ZipCName(zName,nName),iFlags,&rc);` |
|   71 | 2366 | `	if( pEnt == 0 ){` |
|    3 | 2367 | `		return rc;` |
|    - | 2368 | `	}` |
|   69 | 2369 | `	if( nData > 0 ){` |
|   69 | 2370 | `		SyBlobAppend(&pEnt->sData,zData,(sxu32)nData);` |
|   33 | 2371 | `	}` |
|   69 | 2372 | `	pEnt->nSize = (sxu32)(nData > 0 ? nData : 0);` |
|   69 | 2373 | `	pEnt->nCompSize = pEnt->nSize;` |
|   69 | 2374 | `	pEnt->nCrc = 0;` |
|   69 | 2375 | `	pEnt->iMethod = ZIP_CM_STORE;` |
|   69 | 2376 | `	pEnt->bDir = 0;` |
|   69 | 2377 | `	ZipRefresh(pThis,pZip);` |
|   69 | 2378 | `	ph7_result_bool(pCtx,1);` |
|   69 | 2379 | `	return PH7_OK;` |
|   37 | 2380 | `}` |
|    - | 2381 | `/*` |
|    - | 2382 | `` * `addEmptyDir()`. php stores a directory as a zero-length entry whose name`` |
|    - | 2383 | `` * ends in `/` -- and that slash is part of the name every reader answers with,`` |
|    - | 2384 | `` * so `addEmptyDir('sub')` is `getNameIndex()` "sub/".`` |
|    - | 2385 | ` */` |
|   10 | 2386 | `static int vm_builtin_ZipArchive_addEmptyDir(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2387 | `{` |
|    - | 2388 | `	ph7_class_instance *pThis;` |
|   12 | 2389 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    - | 2390 | `	phl_zip_ent *pEnt;` |
|    - | 2391 | `	const char *zName;` |
|    - | 2392 | `	SyBlob sName;` |
|   12 | 2393 | `	int nName = 0,iFlags = 0,rc;` |
|   12 | 2394 | `	if( pZip == 0 ){` |
|  ! 0 | 2395 | `		return PH7_OK;` |
|    - | 2396 | `	}` |
|   12 | 2397 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   12 | 2398 | `	if( nArg > 1 ){` |
|  ! 0 | 2399 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|  ! 0 | 2400 | `	}` |
|   12 | 2401 | `	if( nName < 1 ){` |
|  ! 0 | 2402 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2403 | `		return PH7_OK;` |
|    - | 2404 | `	}` |
|   12 | 2405 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|   12 | 2406 | `	nName = (int)ZipCName(zName,nName);` |
|   12 | 2407 | `	if( nName < 1 ){` |
|  ! 0 | 2408 | `		SyBlobRelease(&sName);` |
|  ! 0 | 2409 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2410 | `		return PH7_OK;` |
|    - | 2411 | `	}` |
|   12 | 2412 | `	SyBlobAppend(&sName,zName,(sxu32)nName);` |
|   12 | 2413 | `	if( zName[nName-1] != '/' ){` |
|   12 | 2414 | `		SyBlobAppend(&sName,"/",sizeof(char));` |
|    5 | 2415 | `	}` |
|   17 | 2416 | `	pEnt = ZipAddSlot(pCtx,pZip,(const char *)SyBlobData(&sName),SyBlobLength(&sName),` |
|    5 | 2417 | `		iFlags,&rc);` |
|   12 | 2418 | `	SyBlobRelease(&sName);` |
|   12 | 2419 | `	if( pEnt == 0 ){` |
|  ! 0 | 2420 | `		return rc;` |
|    - | 2421 | `	}` |
|   12 | 2422 | `	pEnt->bDir = 1;` |
|   12 | 2423 | `	pEnt->nSize = 0;` |
|   12 | 2424 | `	pEnt->nCompSize = 0;` |
|   12 | 2425 | `	pEnt->iMethod = ZIP_CM_STORE;` |
|   12 | 2426 | `	if( pEnt->bNew ){` |
|   12 | 2427 | `		pEnt->nAttr = ZIP_ATTR_DIR;` |
|    5 | 2428 | `	}` |
|   12 | 2429 | `	ZipRefresh(pThis,pZip);` |
|   12 | 2430 | `	ph7_result_bool(pCtx,1);` |
|   12 | 2431 | `	return PH7_OK;` |
|    7 | 2432 | `}` |
|    - | 2433 | `/*` |
|    - | 2434 | `` * `addFile()` and `replaceFile()`, which are the same verb told the slot two`` |
|    - | 2435 | ` * different ways. php reads the FILE at close() rather than here -- what is` |
|    - | 2436 | ` * recorded now is the path and the slice -- so it only has to prove the name` |
|    - | 2437 | ` * can be opened, and reports a name that cannot with the system's own sentence.` |
|    - | 2438 | ` */` |
|  ! 0 | 2439 | `static int ZipAddFileImpl(ph7_context *pCtx,int nArg,ph7_value **apArg,int bReplace)` |
|  ! 0 | 2440 | `{` |
|    - | 2441 | `	ph7_class_instance *pThis;` |
|  ! 0 | 2442 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|  ! 0 | 2443 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|    - | 2444 | `	phl_zip_ent *pEnt;` |
|    - | 2445 | `	const char *zPath;` |
|    - | 2446 | `	SyBlob sFull;` |
|  ! 0 | 2447 | `	int nPath = 0,rc;` |
|  ! 0 | 2448 | `	sxi64 iStart = 0,iLen = 0;` |
|  ! 0 | 2449 | `	int iFlags = bReplace ? 0 : ZIP_FL_OVERWRITE;` |
|  ! 0 | 2450 | `	if( pZip == 0 ){` |
|  ! 0 | 2451 | `		return PH7_OK;` |
|    - | 2452 | `	}` |
|  ! 0 | 2453 | `	if( ZipEmptyName(pCtx,apArg[0],1,"filepath",&rc) ){` |
|  ! 0 | 2454 | `		return rc;` |
|    - | 2455 | `	}` |
|  ! 0 | 2456 | `	zPath = ph7_value_to_string(apArg[0],&nPath);` |
|  ! 0 | 2457 | `	if( nArg > 2 ){` |
|  ! 0 | 2458 | `		iStart = ph7_value_to_int64(apArg[2]);` |
|  ! 0 | 2459 | `	}` |
|  ! 0 | 2460 | `	if( nArg > 3 ){` |
|  ! 0 | 2461 | `		iLen = ph7_value_to_int64(apArg[3]);` |
|  ! 0 | 2462 | `	}` |
|  ! 0 | 2463 | `	if( nArg > 4 ){` |
|  ! 0 | 2464 | `		iFlags = (int)ph7_value_to_int(apArg[4]);` |
|  ! 0 | 2465 | `	}` |
|  ! 0 | 2466 | `	PH7_VfsExpandPath(pCtx,zPath,nPath,&sFull);` |
|  ! 0 | 2467 | `	if( pVfs == 0 \|\| pVfs->xFileExists == 0` |
|  ! 0 | 2468 | `	 \|\| pVfs->xFileExists((const char *)SyBlobData(&sFull)) != PH7_OK ){` |
|  ! 0 | 2469 | `		SyBlobRelease(&sFull);` |
|  ! 0 | 2470 | `		errno = 2 /* ENOENT */;` |
|  ! 0 | 2471 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",VfsStrerror(errno));` |
|  ! 0 | 2472 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2473 | `		return PH7_OK;` |
|    - | 2474 | `	}` |
|  ! 0 | 2475 | `	if( bReplace ){` |
|  ! 0 | 2476 | `		pEnt = ZipArgIndex(pCtx,pZip,apArg[1],&rc);` |
|  ! 0 | 2477 | `		if( pEnt == 0 ){` |
|  ! 0 | 2478 | `			SyBlobRelease(&sFull);` |
|  ! 0 | 2479 | `			return rc;` |
|    - | 2480 | `		}` |
|  ! 0 | 2481 | `		SyBlobReset(&pEnt->sData);` |
|  ! 0 | 2482 | `		SyBlobReset(&pEnt->sSrcPath);` |
|  ! 0 | 2483 | `		pEnt->bDataChanged = 1;` |
|  ! 0 | 2484 | `		pEnt->bLoaded = 0;` |
|  ! 0 | 2485 | `	}else{` |
|  ! 0 | 2486 | `		const char *zEnt = zPath;` |
|  ! 0 | 2487 | `		int nEnt = nPath;` |
|  ! 0 | 2488 | `		if( nArg > 1 ){` |
|  ! 0 | 2489 | `			int n = 0;` |
|  ! 0 | 2490 | `			const char *z = ph7_value_to_string(apArg[1],&n);` |
|  ! 0 | 2491 | `			if( n > 0 ){` |
|  ! 0 | 2492 | `				zEnt = z;` |
|  ! 0 | 2493 | `				nEnt = n;` |
|  ! 0 | 2494 | `			}` |
|  ! 0 | 2495 | `		}` |
|  ! 0 | 2496 | `		pEnt = ZipAddSlot(pCtx,pZip,zEnt,ZipCName(zEnt,nEnt),iFlags,&rc);` |
|  ! 0 | 2497 | `		if( pEnt == 0 ){` |
|  ! 0 | 2498 | `			SyBlobRelease(&sFull);` |
|  ! 0 | 2499 | `			return rc;` |
|    - | 2500 | `		}` |
|    - | 2501 | `	}` |
|  ! 0 | 2502 | `	SyBlobAppend(&pEnt->sSrcPath,SyBlobData(&sFull),SyBlobLength(&sFull));` |
|  ! 0 | 2503 | `	SyBlobNullAppend(&pEnt->sSrcPath);` |
|  ! 0 | 2504 | `	pEnt->iSrcStart = iStart;` |
|  ! 0 | 2505 | `	pEnt->iSrcLen = iLen > 0 ? iLen : -1;` |
|  ! 0 | 2506 | `	pEnt->bDir = 0;` |
|    - | 2507 | `	{` |
|    - | 2508 | `		/* php reports the file's size for an entry it has not written yet, and` |
|    - | 2509 | `		 * the slice's when it was asked for one. */` |
|  ! 0 | 2510 | `		ph7_int64 iSize = pVfs->xFileSize` |
|  ! 0 | 2511 | `			? pVfs->xFileSize((const char *)SyBlobData(&sFull)) : 0;` |
|  ! 0 | 2512 | `		if( iSize < 0 ){` |
|  ! 0 | 2513 | `			iSize = 0;` |
|  ! 0 | 2514 | `		}` |
|  ! 0 | 2515 | `		if( iStart > 0 ){` |
|  ! 0 | 2516 | `			iSize = iStart < iSize ? iSize - iStart : 0;` |
|  ! 0 | 2517 | `		}` |
|  ! 0 | 2518 | `		if( iLen > 0 && iLen < iSize ){` |
|  ! 0 | 2519 | `			iSize = iLen;` |
|  ! 0 | 2520 | `		}` |
|  ! 0 | 2521 | `		pEnt->nSize = (sxu32)iSize;` |
|  ! 0 | 2522 | `		pEnt->nCompSize = pEnt->nSize;` |
|  ! 0 | 2523 | `		pEnt->nCrc = 0;` |
|  ! 0 | 2524 | `		pEnt->iMethod = ZIP_CM_STORE;` |
|    - | 2525 | `	}` |
|  ! 0 | 2526 | `	SyBlobRelease(&sFull);` |
|  ! 0 | 2527 | `	ZipRefresh(pThis,pZip);` |
|  ! 0 | 2528 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 | 2529 | `	return PH7_OK;` |
|  ! 0 | 2530 | `}` |
|  ! 0 | 2531 | `static int vm_builtin_ZipArchive_addFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 2532 | `{` |
|  ! 0 | 2533 | `	return ZipAddFileImpl(pCtx,nArg,apArg,0);` |
|  ! 0 | 2534 | `}` |
|  ! 0 | 2535 | `static int vm_builtin_ZipArchive_replaceFile(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 2536 | `{` |
|  ! 0 | 2537 | `	return ZipAddFileImpl(pCtx,nArg,apArg,1);` |
|  ! 0 | 2538 | `}` |
|    - | 2539 | ``/* `deleteIndex()` / `deleteName()`. The index stays: only close() compacts. */`` |
|   12 | 2540 | `static int ZipDeleteEnt(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt)` |
|    1 | 2541 | `{` |
|   13 | 2542 | `	pEnt->bDeleted = 1;` |
|   13 | 2543 | `	pEnt->bNameHidden = 1;` |
|   13 | 2544 | `	ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|   13 | 2545 | `	ph7_result_bool(pCtx,1);` |
|   13 | 2546 | `	return PH7_OK;` |
|    1 | 2547 | `}` |
|    8 | 2548 | `static int vm_builtin_ZipArchive_deleteIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2549 | `{` |
|    9 | 2550 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2551 | `	phl_zip_ent *pEnt;` |
|    - | 2552 | `	int rc;` |
|    4 | 2553 | `	SXUNUSED(nArg);` |
|    9 | 2554 | `	if( pZip == 0 ){` |
|  ! 0 | 2555 | `		return PH7_OK;` |
|    - | 2556 | `	}` |
|    9 | 2557 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|    9 | 2558 | `	return pEnt ? ZipDeleteEnt(pCtx,pZip,pEnt) : rc;` |
|    5 | 2559 | `}` |
|    4 | 2560 | `static int vm_builtin_ZipArchive_deleteName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2561 | `{` |
|    5 | 2562 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2563 | `	phl_zip_ent *pEnt;` |
|    - | 2564 | `	int rc;` |
|    2 | 2565 | `	SXUNUSED(nArg);` |
|    5 | 2566 | `	if( pZip == 0 ){` |
|  ! 0 | 2567 | `		return PH7_OK;` |
|    - | 2568 | `	}` |
|    5 | 2569 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|    5 | 2570 | `	return pEnt ? ZipDeleteEnt(pCtx,pZip,pEnt) : rc;` |
|    3 | 2571 | `}` |
|    - | 2572 | ``/* `renameIndex()` / `renameName()`. A rename onto a name already in the archive`` |
|    - | 2573 | ` * is ER_EXISTS; a rename onto its OWN name is a no-op that answers true. */` |
|    4 | 2574 | `static int ZipRenameEnt(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pNew)` |
|    1 | 2575 | `{` |
|    5 | 2576 | `	int nNew = 0;` |
|    5 | 2577 | `	const char *zNew = ph7_value_to_string(pNew,&nNew);` |
|    - | 2578 | `	phl_zip_ent *pClash;` |
|    5 | 2579 | `	nNew = (int)ZipCName(zNew,nNew);` |
|    5 | 2580 | `	if( nNew < 1 ){` |
|  ! 0 | 2581 | `		return ZipFail(pCtx,pZip,ZIP_ER_INVAL);` |
|    - | 2582 | `	}` |
|    - | 2583 | `	/* The trailing slash IS the entry's kind, so a rename may not change it:` |
|    - | 2584 | ``	 * a file renamed to a name ending in `/`, or a directory to one that does`` |
|    - | 2585 | ``	 * not, is php's `Invalid argument` rather than a rename that would leave`` |
|    - | 2586 | `	 * an archive describing itself wrongly. */` |
|    5 | 2587 | `	if( (zNew[nNew-1] == '/') != (pEnt->bDir ? 1 : 0) ){` |
|    3 | 2588 | `		return ZipFail(pCtx,pZip,ZIP_ER_INVAL);` |
|    - | 2589 | `	}` |
|    3 | 2590 | `	pClash = ZipFind(pZip,zNew,(sxu32)nNew,0,0);` |
|    3 | 2591 | `	if( pClash && pClash != pEnt ){` |
|  ! 0 | 2592 | `		return ZipFail(pCtx,pZip,ZIP_ER_EXISTS);` |
|    - | 2593 | `	}` |
|    3 | 2594 | `	SyBlobReset(&pEnt->sName);` |
|    3 | 2595 | `	SyBlobAppend(&pEnt->sName,zNew,(sxu32)nNew);` |
|    3 | 2596 | `	pEnt->bNameChanged = 1;` |
|    3 | 2597 | `	ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|    3 | 2598 | `	ph7_result_bool(pCtx,1);` |
|    3 | 2599 | `	return PH7_OK;` |
|    3 | 2600 | `}` |
|  ! 0 | 2601 | `static int vm_builtin_ZipArchive_renameIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 2602 | `{` |
|  ! 0 | 2603 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2604 | `	phl_zip_ent *pEnt;` |
|    - | 2605 | `	int rc;` |
|  ! 0 | 2606 | `	SXUNUSED(nArg);` |
|  ! 0 | 2607 | `	if( pZip == 0 ){` |
|  ! 0 | 2608 | `		return PH7_OK;` |
|    - | 2609 | `	}` |
|  ! 0 | 2610 | `	if( ZipEmptyName(pCtx,apArg[1],2,"new_name",&rc) ){` |
|  ! 0 | 2611 | `		return rc;` |
|    - | 2612 | `	}` |
|  ! 0 | 2613 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|  ! 0 | 2614 | `	return pEnt ? ZipRenameEnt(pCtx,pZip,pEnt,apArg[1]) : rc;` |
|  ! 0 | 2615 | `}` |
|    4 | 2616 | `static int vm_builtin_ZipArchive_renameName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2617 | `{` |
|    5 | 2618 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2619 | `	phl_zip_ent *pEnt;` |
|    - | 2620 | `	int rc;` |
|    2 | 2621 | `	SXUNUSED(nArg);` |
|    5 | 2622 | `	if( pZip == 0 ){` |
|  ! 0 | 2623 | `		return PH7_OK;` |
|    - | 2624 | `	}` |
|    5 | 2625 | `	if( ZipEmptyName(pCtx,apArg[1],2,"new_name",&rc) ){` |
|  ! 0 | 2626 | `		return rc;` |
|    - | 2627 | `	}` |
|    5 | 2628 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|    5 | 2629 | `	return pEnt ? ZipRenameEnt(pCtx,pZip,pEnt,apArg[1]) : rc;` |
|    3 | 2630 | `}` |
|    - | 2631 | `/* ------------------------------------------------------------------ */` |
|    - | 2632 | `/* Reading the table                                                   */` |
|    - | 2633 | `/* ------------------------------------------------------------------ */` |
|    - | 2634 | `/*` |
|    - | 2635 | `` * `statIndex()` / `statName()`: php's eight-key description of one entry.`` |
|    - | 2636 | ` *` |
|    - | 2637 | ` * An entry that has not been WRITTEN yet -- one an add put there this session,` |
|    - | 2638 | ` * or one whose bytes were replaced -- describes what it will be made of rather` |
|    - | 2639 | `` * than what it is: `comp_size` equals `size`, `crc` is 0 and `comp_method` is`` |
|    - | 2640 | ` * 0, whatever it will be compressed with. Nothing has been compressed yet, and` |
|    - | 2641 | ` * libzip answers from the source rather than from a guess.` |
|    - | 2642 | ` */` |
|  728 | 2643 | `static int ZipStatEnt(ph7_context *pCtx,phl_zip_ent *pEnt,sxu32 nIdx,int iFlags)` |
|    2 | 2644 | `{` |
|    - | 2645 | `	ph7_value *pArray,*pVal;` |
|    - | 2646 | `	sxu32 nName;` |
|  730 | 2647 | `	const char *zName = ZipEntName(pEnt,iFlags,&nName);` |
|  730 | 2648 | `	int bFresh = pEnt->bNew \|\| pEnt->bDataChanged;` |
|  730 | 2649 | `	if( zName == 0 ){` |
|    - | 2650 | `		/* FL_UNCHANGED asked about an entry that has no original: php has` |
|    - | 2651 | `		 * nothing to describe and answers false rather than an empty name. */` |
|    3 | 2652 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2653 | `		return PH7_OK;` |
|    - | 2654 | `	}` |
|  728 | 2655 | `	pArray = ph7_context_new_array(pCtx);` |
|  728 | 2656 | `	pVal = ph7_context_new_scalar(pCtx);` |
|  728 | 2657 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 | 2658 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 2659 | `		return PH7_OK;` |
|    - | 2660 | `	}` |
|  728 | 2661 | `	ph7_value_string(pVal,zName,(int)nName);` |
|  728 | 2662 | `	ph7_array_add_strkey_elem(pArray,"name",pVal);` |
|  728 | 2663 | `	ph7_value_reset_string_cursor(pVal);` |
|  728 | 2664 | `	ph7_value_int64(pVal,(ph7_int64)nIdx);` |
|  728 | 2665 | `	ph7_array_add_strkey_elem(pArray,"index",pVal);` |
|  728 | 2666 | `	ph7_value_int64(pVal,(ph7_int64)(bFresh ? 0 : pEnt->nCrc));` |
|  728 | 2667 | `	ph7_array_add_strkey_elem(pArray,"crc",pVal);` |
|  728 | 2668 | `	ph7_value_int64(pVal,(ph7_int64)pEnt->nSize);` |
|  728 | 2669 | `	ph7_array_add_strkey_elem(pArray,"size",pVal);` |
|  728 | 2670 | `	ph7_value_int64(pVal,(ph7_int64)pEnt->iTime);` |
|  728 | 2671 | `	ph7_array_add_strkey_elem(pArray,"mtime",pVal);` |
|  728 | 2672 | `	ph7_value_int64(pVal,(ph7_int64)(bFresh ? pEnt->nSize : pEnt->nCompSize));` |
|  728 | 2673 | `	ph7_array_add_strkey_elem(pArray,"comp_size",pVal);` |
|  728 | 2674 | `	ph7_value_int64(pVal,(ph7_int64)(bFresh ? ZIP_CM_STORE : pEnt->iMethod));` |
|  728 | 2675 | `	ph7_array_add_strkey_elem(pArray,"comp_method",pVal);` |
|    - | 2676 | `	/* A pending setEncryption() is REPORTED, the way a pending compression` |
|    - | 2677 | `	 * method is not: an entry whose bytes are already written can be told to` |
|    - | 2678 | `	 * change cipher, and php answers the one it will have. An entry that has no` |
|    - | 2679 | `	 * bytes yet answers 0 with the rest of its unwritten description. */` |
| 1448 | 2680 | `	ph7_value_int64(pVal,(ph7_int64)(bFresh ? ZIP_EM_NONE` |
|  720 | 2681 | `		: (pEnt->iSetEncrypt >= 0 ? pEnt->iSetEncrypt : pEnt->iEncMethod)));` |
|  728 | 2682 | `	ph7_array_add_strkey_elem(pArray,"encryption_method",pVal);` |
|  728 | 2683 | `	ph7_result_value(pCtx,pArray);` |
|  728 | 2684 | `	ph7_context_release_value(pCtx,pVal);` |
|  728 | 2685 | `	return PH7_OK;` |
|  366 | 2686 | `}` |
|  728 | 2687 | `static int vm_builtin_ZipArchive_statIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 2688 | `{` |
|  730 | 2689 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2690 | `	phl_zip_ent *pEnt;` |
|  730 | 2691 | `	int iFlags = 0,rc;` |
|  730 | 2692 | `	if( pZip == 0 ){` |
|  ! 0 | 2693 | `		return PH7_OK;` |
|    - | 2694 | `	}` |
|  730 | 2695 | `	if( nArg > 1 ){` |
|   35 | 2696 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|   17 | 2697 | `	}` |
|  730 | 2698 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|  730 | 2699 | `	if( pEnt == 0 ){` |
|  ! 0 | 2700 | `		return rc;` |
|    - | 2701 | `	}` |
|  730 | 2702 | `	return ZipStatEnt(pCtx,pEnt,(sxu32)ph7_value_to_int64(apArg[0]),iFlags);` |
|  366 | 2703 | `}` |
|    6 | 2704 | `static int vm_builtin_ZipArchive_statName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2705 | `{` |
|    7 | 2706 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2707 | `	phl_zip_ent *pEnt;` |
|    - | 2708 | `	const char *zName;` |
|    7 | 2709 | `	sxu32 nIdx = 0;` |
|    7 | 2710 | `	int nName = 0,iFlags = 0;` |
|    - | 2711 | `	int rc;` |
|    7 | 2712 | `	if( pZip == 0 ){` |
|  ! 0 | 2713 | `		return PH7_OK;` |
|    - | 2714 | `	}` |
|    7 | 2715 | `	if( ZipNulName(pCtx,apArg[0],1,"name",&rc) ){` |
|    3 | 2716 | `		return rc;` |
|    - | 2717 | `	}` |
|    5 | 2718 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 2719 | `		return rc;` |
|    - | 2720 | `	}` |
|    5 | 2721 | `	if( nArg > 1 ){` |
|  ! 0 | 2722 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|  ! 0 | 2723 | `	}` |
|    5 | 2724 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    5 | 2725 | `	pEnt = ZipFind(pZip,zName,(sxu32)(nName < 0 ? 0 : nName),iFlags,&nIdx);` |
|    5 | 2726 | `	if( pEnt == 0 ){` |
|    5 | 2727 | `		return ZipFail(pCtx,pZip,ZIP_ER_NOENT);` |
|    - | 2728 | `	}` |
|  ! 0 | 2729 | `	return ZipStatEnt(pCtx,pEnt,nIdx,iFlags);` |
|    4 | 2730 | `}` |
|   12 | 2731 | `static int vm_builtin_ZipArchive_locateName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2732 | `{` |
|   13 | 2733 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2734 | `	phl_zip_ent *pEnt;` |
|    - | 2735 | `	const char *zName;` |
|   13 | 2736 | `	sxu32 nIdx = 0;` |
|   13 | 2737 | `	int nName = 0,iFlags = 0;` |
|    - | 2738 | `	int rc;` |
|   13 | 2739 | `	if( pZip == 0 ){` |
|  ! 0 | 2740 | `		return PH7_OK;` |
|    - | 2741 | `	}` |
|   13 | 2742 | `	if( ZipNulName(pCtx,apArg[0],1,"name",&rc) ){` |
|    3 | 2743 | `		return rc;` |
|    - | 2744 | `	}` |
|   11 | 2745 | `	if( nArg > 1 ){` |
|    5 | 2746 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|    2 | 2747 | `	}` |
|   11 | 2748 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   11 | 2749 | `	pEnt = ZipFind(pZip,zName,(sxu32)(nName < 0 ? 0 : nName),iFlags,&nIdx);` |
|   11 | 2750 | `	if( pEnt == 0 ){` |
|    3 | 2751 | `		return ZipFail(pCtx,pZip,ZIP_ER_NOENT);` |
|    - | 2752 | `	}` |
|    9 | 2753 | `	ph7_result_int64(pCtx,(ph7_int64)nIdx);` |
|    9 | 2754 | `	return PH7_OK;` |
|    7 | 2755 | `}` |
|   22 | 2756 | `static int vm_builtin_ZipArchive_getNameIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2757 | `{` |
|   23 | 2758 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2759 | `	phl_zip_ent *pEnt;` |
|    - | 2760 | `	const char *zName;` |
|   23 | 2761 | `	sxu32 nName = 0;` |
|   23 | 2762 | `	int iFlags = 0,rc;` |
|   23 | 2763 | `	if( pZip == 0 ){` |
|  ! 0 | 2764 | `		return PH7_OK;` |
|    - | 2765 | `	}` |
|   23 | 2766 | `	if( nArg > 1 ){` |
|    5 | 2767 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|    2 | 2768 | `	}` |
|   23 | 2769 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|   23 | 2770 | `	if( pEnt == 0 ){` |
|    5 | 2771 | `		return rc;` |
|    - | 2772 | `	}` |
|   19 | 2773 | `	zName = ZipEntName(pEnt,iFlags,&nName);` |
|   19 | 2774 | `	if( zName == 0 ){` |
|    3 | 2775 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2776 | `		return PH7_OK;` |
|    - | 2777 | `	}` |
|   17 | 2778 | `	ph7_result_string(pCtx,zName,(int)nName);` |
|   17 | 2779 | `	return PH7_OK;` |
|   12 | 2780 | `}` |
|    - | 2781 | `/* ------------------------------------------------------------------ */` |
|    - | 2782 | `/* Comments, times, attributes, compression                            */` |
|    - | 2783 | `/* ------------------------------------------------------------------ */` |
|    8 | 2784 | `static int vm_builtin_ZipArchive_setArchiveComment(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2785 | `{` |
|    - | 2786 | `	ph7_class_instance *pThis;` |
|    9 | 2787 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    - | 2788 | `	const char *z;` |
|    9 | 2789 | `	int n = 0;` |
|    4 | 2790 | `	SXUNUSED(nArg);` |
|    9 | 2791 | `	if( pZip == 0 ){` |
|  ! 0 | 2792 | `		return PH7_OK;` |
|    - | 2793 | `	}` |
|    9 | 2794 | `	z = ph7_value_to_string(apArg[0],&n);` |
|    9 | 2795 | `	if( n > 0 && (int)ZipCName(z,n) != n ){` |
|    - | 2796 | `		/* The archive comment is the one string php refuses a NUL in outright:` |
|    - | 2797 | `		 * an entry NAME is truncated at one and this answers false. */` |
|    3 | 2798 | `		ph7_result_bool(pCtx,0);` |
|    3 | 2799 | `		return PH7_OK;` |
|    - | 2800 | `	}` |
|    7 | 2801 | `	SyBlobReset(&pZip->sComment);` |
|    7 | 2802 | `	if( n > 0 ){` |
|    7 | 2803 | `		SyBlobAppend(&pZip->sComment,z,(sxu32)n);` |
|    3 | 2804 | `	}` |
|    - | 2805 | `	/* Only a comment that really DIFFERS is a change: php rewrites nothing for` |
|    - | 2806 | `	 * a setArchiveComment() that sets what was already there. */` |
|    6 | 2807 | `	if( SyBlobLength(&pZip->sComment) != SyBlobLength(&pZip->sOrigComment)` |
|    4 | 2808 | `	 \|\| (SyBlobLength(&pZip->sComment) > 0` |
|  ! 0 | 2809 | `	     && SyMemcmp(SyBlobData(&pZip->sComment),SyBlobData(&pZip->sOrigComment),` |
|  ! 0 | 2810 | `	                 SyBlobLength(&pZip->sComment)) != 0) ){` |
|    7 | 2811 | `		pZip->bCommentChanged = 1;` |
|    3 | 2812 | `	}` |
|    7 | 2813 | `	ZipRefresh(pThis,pZip);` |
|    7 | 2814 | `	ph7_result_bool(pCtx,1);` |
|    7 | 2815 | `	return PH7_OK;` |
|    5 | 2816 | `}` |
|    2 | 2817 | `static int vm_builtin_ZipArchive_getArchiveComment(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2818 | `{` |
|    3 | 2819 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2820 | `	SyBlob *pB;` |
|    3 | 2821 | `	int iFlags = 0;` |
|    3 | 2822 | `	if( pZip == 0 ){` |
|  ! 0 | 2823 | `		return PH7_OK;` |
|    - | 2824 | `	}` |
|    3 | 2825 | `	if( nArg > 0 ){` |
|  ! 0 | 2826 | `		iFlags = (int)ph7_value_to_int(apArg[0]);` |
|  ! 0 | 2827 | `	}` |
|    3 | 2828 | `	pB = (iFlags & ZIP_FL_UNCHANGED) ? &pZip->sOrigComment : &pZip->sComment;` |
|    3 | 2829 | `	ph7_result_string(pCtx,(const char *)SyBlobData(pB),(int)SyBlobLength(pB));` |
|    3 | 2830 | `	return PH7_OK;` |
|    2 | 2831 | `}` |
|    - | 2832 | `/*` |
|    - | 2833 | `` * `setArchiveFlag()` / `getArchiveFlag()`. The one flag php exposes is`` |
|    - | 2834 | ` * AFL_RDONLY, and libzip refuses to set it on an archive with UNWRITTEN` |
|    - | 2835 | ` * changes -- turning an archive read-only would strand them.` |
|    - | 2836 | ` */` |
|  ! 0 | 2837 | `static int vm_builtin_ZipArchive_setArchiveFlag(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 2838 | `{` |
|    - | 2839 | `	ph7_class_instance *pThis;` |
|  ! 0 | 2840 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    - | 2841 | `	int iFlag,iValue;` |
|  ! 0 | 2842 | `	SXUNUSED(nArg);` |
|  ! 0 | 2843 | `	if( pZip == 0 ){` |
|  ! 0 | 2844 | `		return PH7_OK;` |
|    - | 2845 | `	}` |
|  ! 0 | 2846 | `	iFlag = (int)ph7_value_to_int(apArg[0]);` |
|  ! 0 | 2847 | `	iValue = (int)ph7_value_to_int(apArg[1]);` |
|  ! 0 | 2848 | `	if( (iFlag & ZIP_AFL_RDONLY) && iValue ){` |
|    - | 2849 | `		sxu32 i;` |
|  ! 0 | 2850 | `		for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|  ! 0 | 2851 | `			phl_zip_ent *pEnt = pZip->apEnt[i];` |
|  ! 0 | 2852 | `			if( pEnt->bNew \|\| pEnt->bDeleted \|\| pEnt->bDataChanged` |
|  ! 0 | 2853 | `			 \|\| pEnt->bNameChanged \|\| pEnt->bCommentChanged` |
|  ! 0 | 2854 | `			 \|\| pEnt->bTimeChanged \|\| pEnt->bAttrChanged ){` |
|  ! 0 | 2855 | `				return ZipFail(pCtx,pZip,ZIP_ER_CHANGED);` |
|    - | 2856 | `			}` |
|  ! 0 | 2857 | `		}` |
|  ! 0 | 2858 | `		if( pZip->bCommentChanged ){` |
|  ! 0 | 2859 | `			return ZipFail(pCtx,pZip,ZIP_ER_CHANGED);` |
|    - | 2860 | `		}` |
|  ! 0 | 2861 | `	}` |
|  ! 0 | 2862 | `	if( iValue ){` |
|  ! 0 | 2863 | `		pZip->iArchiveFlags \|= iFlag;` |
|  ! 0 | 2864 | `	}else{` |
|  ! 0 | 2865 | `		pZip->iArchiveFlags &= ~iFlag;` |
|    - | 2866 | `	}` |
|  ! 0 | 2867 | `	if( iFlag & ZIP_AFL_RDONLY ){` |
|  ! 0 | 2868 | `		pZip->bRdonly = iValue ? 1 : (sxu8)0;` |
|  ! 0 | 2869 | `	}` |
|  ! 0 | 2870 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 | 2871 | `	return PH7_OK;` |
|  ! 0 | 2872 | `}` |
|  ! 0 | 2873 | `static int vm_builtin_ZipArchive_getArchiveFlag(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 2874 | `{` |
|  ! 0 | 2875 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2876 | `	int iFlag;` |
|  ! 0 | 2877 | `	SXUNUSED(nArg);` |
|  ! 0 | 2878 | `	if( pZip == 0 ){` |
|  ! 0 | 2879 | `		return PH7_OK;` |
|    - | 2880 | `	}` |
|  ! 0 | 2881 | `	iFlag = (int)ph7_value_to_int(apArg[0]);` |
|  ! 0 | 2882 | `	ph7_result_int(pCtx,(pZip->iArchiveFlags & iFlag) ? iFlag : 0);` |
|  ! 0 | 2883 | `	return PH7_OK;` |
|  ! 0 | 2884 | `}` |
|    6 | 2885 | `static int ZipSetComment(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pVal)` |
|    1 | 2886 | `{` |
|    7 | 2887 | `	int n = 0;` |
|    7 | 2888 | `	const char *z = ph7_value_to_string(pVal,&n);` |
|    7 | 2889 | `	SyBlobReset(&pEnt->sComment);` |
|    7 | 2890 | `	if( n > 0 ){` |
|    7 | 2891 | `		SyBlobAppend(&pEnt->sComment,z,(sxu32)n);` |
|    3 | 2892 | `	}` |
|    7 | 2893 | `	pEnt->bCommentChanged = 1;` |
|    7 | 2894 | `	ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|    7 | 2895 | `	ph7_result_bool(pCtx,1);` |
|    7 | 2896 | `	return PH7_OK;` |
|    1 | 2897 | `}` |
|  ! 0 | 2898 | `static int vm_builtin_ZipArchive_setCommentIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 2899 | `{` |
|  ! 0 | 2900 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2901 | `	phl_zip_ent *pEnt;` |
|    - | 2902 | `	int rc;` |
|  ! 0 | 2903 | `	SXUNUSED(nArg);` |
|  ! 0 | 2904 | `	if( pZip == 0 ){` |
|  ! 0 | 2905 | `		return PH7_OK;` |
|    - | 2906 | `	}` |
|  ! 0 | 2907 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|  ! 0 | 2908 | `	return pEnt ? ZipSetComment(pCtx,pZip,pEnt,apArg[1]) : rc;` |
|  ! 0 | 2909 | `}` |
|    6 | 2910 | `static int vm_builtin_ZipArchive_setCommentName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2911 | `{` |
|    7 | 2912 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2913 | `	phl_zip_ent *pEnt;` |
|    - | 2914 | `	int rc;` |
|    3 | 2915 | `	SXUNUSED(nArg);` |
|    7 | 2916 | `	if( pZip == 0 ){` |
|  ! 0 | 2917 | `		return PH7_OK;` |
|    - | 2918 | `	}` |
|    7 | 2919 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 2920 | `		return rc;` |
|    - | 2921 | `	}` |
|    7 | 2922 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|    7 | 2923 | `	return pEnt ? ZipSetComment(pCtx,pZip,pEnt,apArg[1]) : rc;` |
|    4 | 2924 | `}` |
|   34 | 2925 | `static int ZipGetComment(ph7_context *pCtx,phl_zip_ent *pEnt,int iFlags)` |
|    1 | 2926 | `{` |
|   35 | 2927 | `	SyBlob *pB = (iFlags & ZIP_FL_UNCHANGED) ? &pEnt->sOrigComment : &pEnt->sComment;` |
|   35 | 2928 | `	ph7_result_string(pCtx,(const char *)SyBlobData(pB),(int)SyBlobLength(pB));` |
|   35 | 2929 | `	return PH7_OK;` |
|    1 | 2930 | `}` |
|   32 | 2931 | `static int vm_builtin_ZipArchive_getCommentIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2932 | `{` |
|   33 | 2933 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2934 | `	phl_zip_ent *pEnt;` |
|   33 | 2935 | `	int iFlags = 0,rc;` |
|   33 | 2936 | `	if( pZip == 0 ){` |
|  ! 0 | 2937 | `		return PH7_OK;` |
|    - | 2938 | `	}` |
|   33 | 2939 | `	if( nArg > 1 ){` |
|  ! 0 | 2940 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|  ! 0 | 2941 | `	}` |
|   33 | 2942 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|   33 | 2943 | `	return pEnt ? ZipGetComment(pCtx,pEnt,iFlags) : rc;` |
|   17 | 2944 | `}` |
|    2 | 2945 | `static int vm_builtin_ZipArchive_getCommentName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2946 | `{` |
|    3 | 2947 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2948 | `	phl_zip_ent *pEnt;` |
|    3 | 2949 | `	int iFlags = 0,rc;` |
|    3 | 2950 | `	if( pZip == 0 ){` |
|  ! 0 | 2951 | `		return PH7_OK;` |
|    - | 2952 | `	}` |
|    3 | 2953 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 2954 | `		return rc;` |
|    - | 2955 | `	}` |
|    3 | 2956 | `	if( nArg > 1 ){` |
|  ! 0 | 2957 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|  ! 0 | 2958 | `	}` |
|    3 | 2959 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|    3 | 2960 | `	return pEnt ? ZipGetComment(pCtx,pEnt,iFlags) : rc;` |
|    2 | 2961 | `}` |
|   16 | 2962 | `static int ZipSetMtime(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pVal)` |
|    1 | 2963 | `{` |
|   17 | 2964 | `	pEnt->iTime = ph7_value_to_int64(pVal);` |
|   17 | 2965 | `	pEnt->bTimeChanged = 1;` |
|   17 | 2966 | `	ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|   17 | 2967 | `	ph7_result_bool(pCtx,1);` |
|   17 | 2968 | `	return PH7_OK;` |
|    1 | 2969 | `}` |
|  ! 0 | 2970 | `static int vm_builtin_ZipArchive_setMtimeIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 2971 | `{` |
|  ! 0 | 2972 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2973 | `	phl_zip_ent *pEnt;` |
|    - | 2974 | `	int rc;` |
|  ! 0 | 2975 | `	SXUNUSED(nArg);` |
|  ! 0 | 2976 | `	if( pZip == 0 ){` |
|  ! 0 | 2977 | `		return PH7_OK;` |
|    - | 2978 | `	}` |
|  ! 0 | 2979 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|  ! 0 | 2980 | `	return pEnt ? ZipSetMtime(pCtx,pZip,pEnt,apArg[1]) : rc;` |
|  ! 0 | 2981 | `}` |
|   16 | 2982 | `static int vm_builtin_ZipArchive_setMtimeName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 2983 | `{` |
|   17 | 2984 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 2985 | `	phl_zip_ent *pEnt;` |
|    - | 2986 | `	int rc;` |
|    8 | 2987 | `	SXUNUSED(nArg);` |
|   17 | 2988 | `	if( pZip == 0 ){` |
|  ! 0 | 2989 | `		return PH7_OK;` |
|    - | 2990 | `	}` |
|   17 | 2991 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 2992 | `		return rc;` |
|    - | 2993 | `	}` |
|   17 | 2994 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|   17 | 2995 | `	return pEnt ? ZipSetMtime(pCtx,pZip,pEnt,apArg[1]) : rc;` |
|    9 | 2996 | `}` |
|    - | 2997 | `/*` |
|    - | 2998 | ` * The external attributes, which on the unix side ARE the mode: php hands back` |
|    - | 2999 | ` * the raw 32-bit word, and a script that wants permissions shifts it right` |
|    - | 3000 | ` * sixteen. An entry nobody set them on carries libzip's own 0100666 (0040777` |
|    - | 3001 | ` * for a directory), whatever the file it came from was.` |
|    - | 3002 | ` */` |
|  ! 0 | 3003 | `static int ZipSetExtAttr(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,` |
|    - | 3004 | `	ph7_value *pOpsys,ph7_value *pAttr)` |
|  ! 0 | 3005 | `{` |
|  ! 0 | 3006 | `	pEnt->iOpsys = (int)ph7_value_to_int(pOpsys);` |
|  ! 0 | 3007 | `	pEnt->nAttr = (sxu32)ph7_value_to_int64(pAttr);` |
|  ! 0 | 3008 | `	pEnt->bAttrChanged = 1;` |
|  ! 0 | 3009 | `	ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|  ! 0 | 3010 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 | 3011 | `	return PH7_OK;` |
|  ! 0 | 3012 | `}` |
|  ! 0 | 3013 | `static int vm_builtin_ZipArchive_setExternalAttributesIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3014 | `{` |
|  ! 0 | 3015 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3016 | `	phl_zip_ent *pEnt;` |
|    - | 3017 | `	int rc;` |
|  ! 0 | 3018 | `	SXUNUSED(nArg);` |
|  ! 0 | 3019 | `	if( pZip == 0 ){` |
|  ! 0 | 3020 | `		return PH7_OK;` |
|    - | 3021 | `	}` |
|  ! 0 | 3022 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|  ! 0 | 3023 | `	return pEnt ? ZipSetExtAttr(pCtx,pZip,pEnt,apArg[1],apArg[2]) : rc;` |
|  ! 0 | 3024 | `}` |
|  ! 0 | 3025 | `static int vm_builtin_ZipArchive_setExternalAttributesName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3026 | `{` |
|  ! 0 | 3027 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3028 | `	phl_zip_ent *pEnt;` |
|    - | 3029 | `	int rc;` |
|  ! 0 | 3030 | `	SXUNUSED(nArg);` |
|  ! 0 | 3031 | `	if( pZip == 0 ){` |
|  ! 0 | 3032 | `		return PH7_OK;` |
|    - | 3033 | `	}` |
|  ! 0 | 3034 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 3035 | `		return rc;` |
|    - | 3036 | `	}` |
|  ! 0 | 3037 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|  ! 0 | 3038 | `	return pEnt ? ZipSetExtAttr(pCtx,pZip,pEnt,apArg[1],apArg[2]) : rc;` |
|  ! 0 | 3039 | `}` |
|    2 | 3040 | `static int ZipGetExtAttr(ph7_context *pCtx,phl_zip_ent *pEnt,ph7_value *pOpsys,ph7_value *pAttr)` |
|    1 | 3041 | `{` |
|    3 | 3042 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 3043 | `	ph7_value sVal;` |
|    3 | 3044 | `	PH7_MemObjInit(pVm,&sVal);` |
|    3 | 3045 | `	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)pEnt->iOpsys);` |
|    3 | 3046 | `	PH7_VmStoreArgByRef(pVm,pOpsys,&sVal);` |
|    3 | 3047 | `	PH7_MemObjRelease(&sVal);` |
|    3 | 3048 | `	PH7_MemObjInitFromInt(pVm,&sVal,(sxi64)pEnt->nAttr);` |
|    3 | 3049 | `	PH7_VmStoreArgByRef(pVm,pAttr,&sVal);` |
|    3 | 3050 | `	PH7_MemObjRelease(&sVal);` |
|    3 | 3051 | `	ph7_result_bool(pCtx,1);` |
|    3 | 3052 | `	return PH7_OK;` |
|    1 | 3053 | `}` |
|    2 | 3054 | `static int vm_builtin_ZipArchive_getExternalAttributesIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3055 | `{` |
|    3 | 3056 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3057 | `	phl_zip_ent *pEnt;` |
|    - | 3058 | `	int rc;` |
|    1 | 3059 | `	SXUNUSED(nArg);` |
|    3 | 3060 | `	if( pZip == 0 ){` |
|  ! 0 | 3061 | `		return PH7_OK;` |
|    - | 3062 | `	}` |
|    3 | 3063 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|    3 | 3064 | `	return pEnt ? ZipGetExtAttr(pCtx,pEnt,apArg[1],apArg[2]) : rc;` |
|    2 | 3065 | `}` |
|  ! 0 | 3066 | `static int vm_builtin_ZipArchive_getExternalAttributesName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3067 | `{` |
|  ! 0 | 3068 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3069 | `	phl_zip_ent *pEnt;` |
|    - | 3070 | `	int rc;` |
|  ! 0 | 3071 | `	SXUNUSED(nArg);` |
|  ! 0 | 3072 | `	if( pZip == 0 ){` |
|  ! 0 | 3073 | `		return PH7_OK;` |
|    - | 3074 | `	}` |
|  ! 0 | 3075 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 3076 | `		return rc;` |
|    - | 3077 | `	}` |
|  ! 0 | 3078 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|  ! 0 | 3079 | `	return pEnt ? ZipGetExtAttr(pCtx,pEnt,apArg[1],apArg[2]) : rc;` |
|  ! 0 | 3080 | `}` |
|    - | 3081 | `/*` |
|    - | 3082 | `` * `setCompressionIndex()` / `setCompressionName()`. A method this build cannot`` |
|    - | 3083 | ` * write is refused HERE rather than at close, with php's own ER_COMPNOTSUPP --` |
|    - | 3084 | ` * which is also what a libzip built without bzip2 answers.` |
|    - | 3085 | ` */` |
|    4 | 3086 | `static int ZipSetCompression(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pVal)` |
|    1 | 3087 | `{` |
|    5 | 3088 | `	int iMethod = (int)ph7_value_to_int(pVal);` |
|    5 | 3089 | `	if( iMethod != ZIP_CM_DEFAULT && iMethod != ZIP_CM_STORE && iMethod != ZIP_CM_DEFLATE ){` |
|    3 | 3090 | `		return ZipFail(pCtx,pZip,ZIP_ER_COMPNOTSUPP);` |
|    - | 3091 | `	}` |
|    3 | 3092 | `	pEnt->iSetMethod = iMethod;` |
|    3 | 3093 | `	ph7_result_bool(pCtx,1);` |
|    3 | 3094 | `	return PH7_OK;` |
|    3 | 3095 | `}` |
|    4 | 3096 | `static int vm_builtin_ZipArchive_setCompressionIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3097 | `{` |
|    5 | 3098 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3099 | `	phl_zip_ent *pEnt;` |
|    - | 3100 | `	int rc;` |
|    2 | 3101 | `	SXUNUSED(nArg);` |
|    5 | 3102 | `	if( pZip == 0 ){` |
|  ! 0 | 3103 | `		return PH7_OK;` |
|    - | 3104 | `	}` |
|    5 | 3105 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|    5 | 3106 | `	return pEnt ? ZipSetCompression(pCtx,pZip,pEnt,apArg[1]) : rc;` |
|    3 | 3107 | `}` |
|    2 | 3108 | `static int vm_builtin_ZipArchive_setCompressionName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3109 | `{` |
|    3 | 3110 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3111 | `	phl_zip_ent *pEnt;` |
|    - | 3112 | `	int rc;` |
|    1 | 3113 | `	SXUNUSED(nArg);` |
|    3 | 3114 | `	if( pZip == 0 ){` |
|  ! 0 | 3115 | `		return PH7_OK;` |
|    - | 3116 | `	}` |
|    3 | 3117 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 3118 | `		return rc;` |
|    - | 3119 | `	}` |
|    3 | 3120 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|    3 | 3121 | `	return pEnt ? ZipSetCompression(pCtx,pZip,pEnt,apArg[1]) : rc;` |
|    2 | 3122 | `}` |
|    - | 3123 | `/* ------------------------------------------------------------------ */` |
|    - | 3124 | `/* Putting a change back                                               */` |
|    - | 3125 | `/* ------------------------------------------------------------------ */` |
|    - | 3126 | `/*` |
|    - | 3127 | ` * One entry's changes, undone. What comes back is the NAME, the comment, the` |
|    - | 3128 | ` * time and the attributes -- and, for an entry that was DELETED, the entry` |
|    - | 3129 | ` * itself, but not its name: libzip drops a deleted name from the hash it looks` |
|    - | 3130 | `` * names up in and only `unchangeAll()` puts that hash back, so an entry`` |
|    - | 3131 | `` * restored this way answers `statIndex()` and not `statName()`. Reproduced`` |
|    - | 3132 | ` * because it is what a program sees.` |
|    - | 3133 | ` *` |
|    - | 3134 | ` * An entry that was ADDED has no original to go back to, so unchanging it` |
|    - | 3135 | `` * REMOVES it -- while leaving the index it took, which `numFiles` still counts.`` |
|    - | 3136 | ` */` |
|   16 | 3137 | `static void ZipUnchangeEnt(phl_zip_ent *pEnt,int bAll)` |
|    1 | 3138 | `{` |
|   17 | 3139 | `	if( pEnt->bNew ){` |
|    3 | 3140 | `		pEnt->bDeleted = 1;` |
|    3 | 3141 | `		pEnt->bNameHidden = 1;` |
|    3 | 3142 | `		return;` |
|    - | 3143 | `	}` |
|   15 | 3144 | `	SyBlobReset(&pEnt->sName);` |
|   15 | 3145 | `	SyBlobAppend(&pEnt->sName,SyBlobData(&pEnt->sOrigName),SyBlobLength(&pEnt->sOrigName));` |
|   15 | 3146 | `	SyBlobReset(&pEnt->sComment);` |
|   22 | 3147 | `	SyBlobAppend(&pEnt->sComment,SyBlobData(&pEnt->sOrigComment),` |
|    7 | 3148 | `		SyBlobLength(&pEnt->sOrigComment));` |
|   15 | 3149 | `	SyBlobReset(&pEnt->sData);` |
|   15 | 3150 | `	SyBlobReset(&pEnt->sSrcPath);` |
|   15 | 3151 | `	pEnt->iSrcStart = 0;` |
|   15 | 3152 | `	pEnt->iSrcLen = -1;` |
|   15 | 3153 | `	pEnt->iTime = pEnt->iOrigTime;` |
|   15 | 3154 | `	pEnt->nAttr = pEnt->nOrigAttr;` |
|   15 | 3155 | `	pEnt->iOpsys = pEnt->iOrigOpsys;` |
|   15 | 3156 | `	pEnt->iSetMethod = ZIP_CM_DEFAULT;` |
|   15 | 3157 | `	pEnt->iSetEncrypt = -1;` |
|   15 | 3158 | `	pEnt->bHasPassword = 0;` |
|   15 | 3159 | `	SyBlobReset(&pEnt->sPassword);` |
|   15 | 3160 | `	pEnt->bLoaded = 0;` |
|   15 | 3161 | `	pEnt->bDataChanged = 0;` |
|   15 | 3162 | `	pEnt->bNameChanged = 0;` |
|   15 | 3163 | `	pEnt->bCommentChanged = 0;` |
|   15 | 3164 | `	pEnt->bTimeChanged = 0;` |
|   15 | 3165 | `	pEnt->bAttrChanged = 0;` |
|   15 | 3166 | `	pEnt->bDeleted = 0;` |
|   29 | 3167 | `	pEnt->bDir = SyBlobLength(&pEnt->sName) > 0` |
|   14 | 3168 | `		&& ((const char *)SyBlobData(&pEnt->sName))[SyBlobLength(&pEnt->sName)-1] == '/';` |
|   15 | 3169 | `	if( bAll ){` |
|   13 | 3170 | `		pEnt->bNameHidden = 0;` |
|    6 | 3171 | `	}` |
|    9 | 3172 | `}` |
|    4 | 3173 | `static int vm_builtin_ZipArchive_unchangeAll(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3174 | `{` |
|    - | 3175 | `	ph7_class_instance *pThis;` |
|    5 | 3176 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    - | 3177 | `	sxu32 i;` |
|    2 | 3178 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    5 | 3179 | `	if( pZip == 0 ){` |
|  ! 0 | 3180 | `		return PH7_OK;` |
|    - | 3181 | `	}` |
|   19 | 3182 | `	for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|   15 | 3183 | `		ZipUnchangeEnt(pZip->apEnt[i],1);` |
|    8 | 3184 | `	}` |
|    5 | 3185 | `	ZipRefresh(pThis,pZip);` |
|    5 | 3186 | `	ph7_result_bool(pCtx,1);` |
|    5 | 3187 | `	return PH7_OK;` |
|    3 | 3188 | `}` |
|    - | 3189 | ``/* php's `unchangeArchive()` is NOT the same verb: it puts back what belongs to`` |
|    - | 3190 | ` * the ARCHIVE -- its comment -- and leaves every entry alone, a deleted one` |
|    - | 3191 | ` * included. */` |
|    2 | 3192 | `static int vm_builtin_ZipArchive_unchangeArchive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3193 | `{` |
|    - | 3194 | `	ph7_class_instance *pThis;` |
|    3 | 3195 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    1 | 3196 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|    3 | 3197 | `	if( pZip == 0 ){` |
|  ! 0 | 3198 | `		return PH7_OK;` |
|    - | 3199 | `	}` |
|    3 | 3200 | `	SyBlobReset(&pZip->sComment);` |
|    4 | 3201 | `	SyBlobAppend(&pZip->sComment,SyBlobData(&pZip->sOrigComment),` |
|    1 | 3202 | `		SyBlobLength(&pZip->sOrigComment));` |
|    3 | 3203 | `	pZip->bCommentChanged = 0;` |
|    3 | 3204 | `	ZipRefresh(pThis,pZip);` |
|    3 | 3205 | `	ph7_result_bool(pCtx,1);` |
|    3 | 3206 | `	return PH7_OK;` |
|    2 | 3207 | `}` |
|  ! 0 | 3208 | `static int vm_builtin_ZipArchive_unchangeIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3209 | `{` |
|    - | 3210 | `	ph7_class_instance *pThis;` |
|  ! 0 | 3211 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    - | 3212 | `	phl_zip_ent *pEnt;` |
|    - | 3213 | `	sxi64 iIdx;` |
|  ! 0 | 3214 | `	SXUNUSED(nArg);` |
|  ! 0 | 3215 | `	if( pZip == 0 ){` |
|  ! 0 | 3216 | `		return PH7_OK;` |
|    - | 3217 | `	}` |
|  ! 0 | 3218 | `	iIdx = ph7_value_to_int64(apArg[0]);` |
|  ! 0 | 3219 | `	if( iIdx < 0 \|\| (sxu64)iIdx >= (sxu64)pZip->nEnt ){` |
|  ! 0 | 3220 | `		return ZipFail(pCtx,pZip,ZIP_ER_INVAL);` |
|    - | 3221 | `	}` |
|  ! 0 | 3222 | `	pEnt = pZip->apEnt[(sxu32)iIdx];` |
|  ! 0 | 3223 | `	ZipUnchangeEnt(pEnt,0);` |
|  ! 0 | 3224 | `	ZipRefresh(pThis,pZip);` |
|  ! 0 | 3225 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 | 3226 | `	return PH7_OK;` |
|  ! 0 | 3227 | `}` |
|    2 | 3228 | `static int vm_builtin_ZipArchive_unchangeName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3229 | `{` |
|    - | 3230 | `	ph7_class_instance *pThis;` |
|    3 | 3231 | `	phl_zip *pZip = ZipThis(pCtx,&pThis);` |
|    - | 3232 | `	phl_zip_ent *pEnt;` |
|    - | 3233 | `	int rc;` |
|    1 | 3234 | `	SXUNUSED(nArg);` |
|    3 | 3235 | `	if( pZip == 0 ){` |
|  ! 0 | 3236 | `		return PH7_OK;` |
|    - | 3237 | `	}` |
|    3 | 3238 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|    3 | 3239 | `	if( pEnt == 0 ){` |
|  ! 0 | 3240 | `		return rc;` |
|    - | 3241 | `	}` |
|    3 | 3242 | `	ZipUnchangeEnt(pEnt,0);` |
|    3 | 3243 | `	ZipRefresh(pThis,pZip);` |
|    3 | 3244 | `	ph7_result_bool(pCtx,1);` |
|    3 | 3245 | `	return PH7_OK;` |
|    2 | 3246 | `}` |
|    - | 3247 | `/* ------------------------------------------------------------------ */` |
|    - | 3248 | `/* Reading an entry's bytes                                            */` |
|    - | 3249 | `/* ------------------------------------------------------------------ */` |
|  748 | 3250 | `static int ZipGetFrom(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,ph7_value *pLen)` |
|    2 | 3251 | `{` |
|  750 | 3252 | `	sxi64 iLen = pLen ? ph7_value_to_int64(pLen) : 0;` |
|    - | 3253 | `	sxu32 nWant;` |
|  750 | 3254 | `	int rc = ZipEntLoad(pZip,pEnt);` |
|  750 | 3255 | `	if( rc != ZIP_ER_OK ){` |
|   58 | 3256 | `		return ZipFail(pCtx,pZip,rc);` |
|    - | 3257 | `	}` |
|  694 | 3258 | `	nWant = SyBlobLength(&pEnt->sData);` |
|  694 | 3259 | `	if( iLen > 0 && (sxu64)iLen < (sxu64)nWant ){` |
|    3 | 3260 | `		nWant = (sxu32)iLen;` |
|    1 | 3261 | `	}` |
|  694 | 3262 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pEnt->sData),(int)nWant);` |
|  694 | 3263 | `	return PH7_OK;` |
|  376 | 3264 | `}` |
|  656 | 3265 | `static int vm_builtin_ZipArchive_getFromIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3266 | `{` |
|  657 | 3267 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3268 | `	phl_zip_ent *pEnt;` |
|    - | 3269 | `	int rc;` |
|  657 | 3270 | `	if( pZip == 0 ){` |
|  ! 0 | 3271 | `		return PH7_OK;` |
|    - | 3272 | `	}` |
|  657 | 3273 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|  657 | 3274 | `	return pEnt ? ZipGetFrom(pCtx,pZip,pEnt,nArg > 1 ? apArg[1] : 0) : rc;` |
|  329 | 3275 | `}` |
|   94 | 3276 | `static int vm_builtin_ZipArchive_getFromName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 3277 | `{` |
|   96 | 3278 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3279 | `	phl_zip_ent *pEnt;` |
|   96 | 3280 | `	int iFlags = 0,rc;` |
|   96 | 3281 | `	if( pZip == 0 ){` |
|  ! 0 | 3282 | `		return PH7_OK;` |
|    - | 3283 | `	}` |
|   96 | 3284 | `	if( ZipNulName(pCtx,apArg[0],1,"name",&rc) ){` |
|    3 | 3285 | `		return rc;` |
|    - | 3286 | `	}` |
|   94 | 3287 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 3288 | `		return rc;` |
|    - | 3289 | `	}` |
|   94 | 3290 | `	if( nArg > 2 ){` |
|  ! 0 | 3291 | `		iFlags = (int)ph7_value_to_int(apArg[2]);` |
|  ! 0 | 3292 | `	}` |
|   94 | 3293 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],iFlags,&rc);` |
|   94 | 3294 | `	return pEnt ? ZipGetFrom(pCtx,pZip,pEnt,nArg > 1 ? apArg[1] : 0) : rc;` |
|   49 | 3295 | `}` |
|    - | 3296 | `/* ------------------------------------------------------------------ */` |
|    - | 3297 | `/* Extracting                                                          */` |
|    - | 3298 | `/* ------------------------------------------------------------------ */` |
|    - | 3299 | `/*` |
|    - | 3300 | ` * Where one entry lands under a destination directory.` |
|    - | 3301 | ` *` |
|    - | 3302 | ` * php resolves the entry name against the destination the way a path is` |
|    - | 3303 | `` * resolved against a root: `.` and empty segments go, `..` pops, and a pop at`` |
|    - | 3304 | ``  * the root is a pop of nothing -- so `../evil.txt` extracts to `evil.txt` `` |
|    - | 3305 | `` * INSIDE the destination and `/abs.txt` to `abs.txt`, which is what keeps an`` |
|    - | 3306 | ` * archive from writing outside where it was told to.` |
|    - | 3307 | ` *` |
|    - | 3308 | ` * Answers 0 when the name collapses to nothing at all, which is not a file.` |
|    - | 3309 | ` */` |
|   16 | 3310 | `static int ZipExtractPath(ph7_vm *pVm,const char *zBase,sxu32 nBase,` |
|    - | 3311 | `	const char *zName,sxu32 nName,SyBlob *pOut,SyBlob *pDir)` |
|    1 | 3312 | `{` |
|    - | 3313 | `	sxu32 n;` |
|    - | 3314 | `	sxu32 nRoot;` |
|   17 | 3315 | `	SyBlobInit(pOut,&pVm->sAllocator);` |
|   17 | 3316 | `	SyBlobInit(pDir,&pVm->sAllocator);` |
|   17 | 3317 | `	SyBlobAppend(pOut,zBase,nBase);` |
|   24 | 3318 | `	while( SyBlobLength(pOut) > 1` |
|   17 | 3319 | `	 && (((const char *)SyBlobData(pOut))[SyBlobLength(pOut)-1] == '/'` |
|   16 | 3320 | `	  \|\| ((const char *)SyBlobData(pOut))[SyBlobLength(pOut)-1] == PH7_PATH_SEP) ){` |
|  ! 0 | 3321 | `		pOut->nByte--;` |
|  ! 0 | 3322 | `	}` |
|   17 | 3323 | `	nRoot = SyBlobLength(pOut);` |
|   49 | 3324 | `	for( n = 0 ; n < nName ; ){` |
|   33 | 3325 | `		sxu32 nStart = n,nSeg;` |
|  186 | 3326 | `		while( n < nName && zName[n] != '/'` |
|    - | 3327 | `#ifdef __WINNT__` |
|    1 | 3328 | `			&& zName[n] != '\\'` |
|    - | 3329 | `#endif` |
|    - | 3330 | `		){` |
|  155 | 3331 | `			++n;` |
|    1 | 3332 | `		}` |
|   33 | 3333 | `		nSeg = n - nStart;` |
|   33 | 3334 | `		if( n < nName ){` |
|   19 | 3335 | `			++n;` |
|    9 | 3336 | `		}` |
|   33 | 3337 | `		if( nSeg == 0 \|\| (nSeg == 1 && zName[nStart] == '.') ){` |
|    3 | 3338 | `			continue;` |
|    - | 3339 | `		}` |
|   31 | 3340 | `		if( nSeg == 2 && zName[nStart] == '.' && zName[nStart+1] == '.' ){` |
|    5 | 3341 | `			sxu32 nHave = SyBlobLength(pOut);` |
|    9 | 3342 | `			while( nHave > nRoot && ((const char *)SyBlobData(pOut))[nHave-1] != '/' ){` |
|    5 | 3343 | `				--nHave;` |
|    1 | 3344 | `			}` |
|    5 | 3345 | `			if( nHave > nRoot ){` |
|    3 | 3346 | `				--nHave;` |
|    1 | 3347 | `			}` |
|    5 | 3348 | `			pOut->nByte = nHave;` |
|    5 | 3349 | `			continue;` |
|    - | 3350 | `		}` |
|    - | 3351 | `		/* Whatever is in front of this segment is the DIRECTORY the entry needs` |
|    - | 3352 | `		 * to exist, which is the last thing recorded before the final one. */` |
|   27 | 3353 | `		SyBlobReset(pDir);` |
|   27 | 3354 | `		SyBlobAppend(pDir,SyBlobData(pOut),SyBlobLength(pOut));` |
|   27 | 3355 | `		SyBlobAppend(pOut,"/",sizeof(char));` |
|   27 | 3356 | `		SyBlobAppend(pOut,&zName[nStart],nSeg);` |
|    1 | 3357 | `	}` |
|   17 | 3358 | `	if( SyBlobLength(pOut) <= nRoot ){` |
|  ! 0 | 3359 | `		SyBlobRelease(pOut);` |
|  ! 0 | 3360 | `		SyBlobRelease(pDir);` |
|  ! 0 | 3361 | `		return 0;` |
|    - | 3362 | `	}` |
|   17 | 3363 | `	SyBlobNullAppend(pOut);` |
|   17 | 3364 | `	SyBlobNullAppend(pDir);` |
|   17 | 3365 | `	return 1;` |
|    9 | 3366 | `}` |
|    - | 3367 | `/*` |
|    - | 3368 | ` * Create a directory and every directory above it. php's extractor asks its` |
|    - | 3369 | ` * stream layer for a RECURSIVE mkdir; the VFS here has one door that makes a` |
|    - | 3370 | ` * single level, so the walk is spelled out. A component that is already there` |
|    - | 3371 | ` * is not a failure -- only the last one's absence would be, and the caller` |
|    - | 3372 | ` * finds that out when the file will not open.` |
|    - | 3373 | ` */` |
|   16 | 3374 | `static void ZipMkdirAll(ph7_vm *pVm,SyBlob *pDir)` |
|    1 | 3375 | `{` |
|   17 | 3376 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|   17 | 3377 | `	char *z = (char *)SyBlobData(pDir);` |
|   17 | 3378 | `	sxu32 n = SyBlobLength(pDir);` |
|    - | 3379 | `	sxu32 i;` |
|   17 | 3380 | `	if( pVfs == 0 \|\| pVfs->xMkdir == 0 \|\| n < 1 ){` |
|  ! 0 | 3381 | `		return;` |
|    - | 3382 | `	}` |
|  757 | 3383 | `	for( i = 1 ; i <= n ; ++i ){` |
|    - | 3384 | `		char c;` |
|  741 | 3385 | `		if( i < n && z[i] != '/' && z[i] != PH7_PATH_SEP ){` |
|  651 | 3386 | `			continue;` |
|    - | 3387 | `		}` |
|   91 | 3388 | `		c = z[i];` |
|   91 | 3389 | `		z[i] = 0;` |
|   91 | 3390 | `		if( pVfs->xFileExists == 0 \|\| pVfs->xFileExists(z) != PH7_OK ){` |
|   15 | 3391 | `			pVfs->xMkdir(z,0777,FALSE);` |
|    7 | 3392 | `		}` |
|   91 | 3393 | `		z[i] = c;` |
|   62 | 3394 | `	}` |
|    9 | 3395 | `}` |
|    - | 3396 | `/* One entry onto the filesystem: the directories in front of it first, then` |
|    - | 3397 | ` * the bytes, then the modification time php restores and the mode it does not` |
|    - | 3398 | ` * (an extracted file gets the process umask, never the entry's attributes). */` |
|   16 | 3399 | `static int ZipExtractOne(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,` |
|    - | 3400 | `	const char *zBase,sxu32 nBase)` |
|    1 | 3401 | `{` |
|   17 | 3402 | `	ph7_vm *pVm = pCtx->pVm;` |
|   17 | 3403 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|    - | 3404 | `	SyBlob sPath,sDir;` |
|    - | 3405 | `	const char *zPath;` |
|    - | 3406 | `	int rc;` |
|   25 | 3407 | `	if( !ZipExtractPath(pVm,zBase,nBase,(const char *)SyBlobData(&pEnt->sName),` |
|    8 | 3408 | `		SyBlobLength(&pEnt->sName),&sPath,&sDir) ){` |
|  ! 0 | 3409 | `		return 1;` |
|    - | 3410 | `	}` |
|   17 | 3411 | `	zPath = (const char *)SyBlobData(&sPath);` |
|   17 | 3412 | `	if( pEnt->bDir ){` |
|    3 | 3413 | `		ZipMkdirAll(pVm,&sPath);` |
|    3 | 3414 | `		SyBlobRelease(&sPath);` |
|    3 | 3415 | `		SyBlobRelease(&sDir);` |
|    3 | 3416 | `		return 1;` |
|    - | 3417 | `	}` |
|   15 | 3418 | `	if( SyBlobLength(&sDir) > 0 ){` |
|   15 | 3419 | `		ZipMkdirAll(pVm,&sDir);` |
|    7 | 3420 | `	}` |
|   15 | 3421 | `	rc = ZipEntLoad(pZip,pEnt);` |
|   15 | 3422 | `	if( rc != ZIP_ER_OK ){` |
|  ! 0 | 3423 | `		pZip->iStatus = rc;` |
|  ! 0 | 3424 | `		SyBlobRelease(&sPath);` |
|  ! 0 | 3425 | `		SyBlobRelease(&sDir);` |
|  ! 0 | 3426 | `		return 0;` |
|    - | 3427 | `	}` |
|    - | 3428 | `	{` |
|    - | 3429 | `		const ph7_io_stream *pStream;` |
|   15 | 3430 | `		const char *zDev = zPath;` |
|    - | 3431 | `		void *pHandle;` |
|   15 | 3432 | `		pStream = PH7_VmGetStreamDevice(pVm,&zDev,(int)SyBlobLength(&sPath));` |
|   15 | 3433 | `		pHandle = pStream ? PH7_StreamOpenHandle(pVm,pStream,zDev,` |
|    7 | 3434 | `			PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,0) : 0;` |
|   15 | 3435 | `		if( pHandle == 0 ){` |
|    - | 3436 | `			/* php's own sentence, with the whole destination path in it. */` |
|  ! 0 | 3437 | `			PH7_VmThrowWarningFmt(pVm,"%z(%s): Failed to open stream: %s",` |
|  ! 0 | 3438 | `				&pCtx->pFunc->sName,zPath,PH7_VfsOpenStrerror(errno));` |
|  ! 0 | 3439 | `			SyBlobRelease(&sPath);` |
|  ! 0 | 3440 | `			SyBlobRelease(&sDir);` |
|  ! 0 | 3441 | `			return 0;` |
|    - | 3442 | `		}` |
|   15 | 3443 | `		if( pStream->xWrite && SyBlobLength(&pEnt->sData) > 0 ){` |
|   22 | 3444 | `			pStream->xWrite(pHandle,SyBlobData(&pEnt->sData),` |
|   14 | 3445 | `				(ph7_int64)SyBlobLength(&pEnt->sData));` |
|    7 | 3446 | `		}` |
|   15 | 3447 | `		PH7_StreamCloseHandle(pStream,pHandle);` |
|    - | 3448 | `	}` |
|   15 | 3449 | `	if( pVfs && pVfs->xTouch ){` |
|   15 | 3450 | `		pVfs->xTouch(zPath,(ph7_int64)pEnt->iTime,(ph7_int64)pEnt->iTime);` |
|    7 | 3451 | `	}` |
|   15 | 3452 | `	SyBlobRelease(&sPath);` |
|   15 | 3453 | `	SyBlobRelease(&sDir);` |
|   15 | 3454 | `	return 1;` |
|    9 | 3455 | `}` |
|   12 | 3456 | `static int vm_builtin_ZipArchive_extractTo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3457 | `{` |
|   13 | 3458 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3459 | `	const char *zBase;` |
|   13 | 3460 | `	int nBase = 0;` |
|    - | 3461 | `	sxu32 i;` |
|    - | 3462 | `	int rc;` |
|   13 | 3463 | `	if( pZip == 0 ){` |
|  ! 0 | 3464 | `		return PH7_OK;` |
|    - | 3465 | `	}` |
|   13 | 3466 | `	if( ZipNulName(pCtx,apArg[0],1,"pathto",&rc) ){` |
|    3 | 3467 | `		return rc;` |
|    - | 3468 | `	}` |
|   11 | 3469 | `	zBase = ph7_value_to_string(apArg[0],&nBase);` |
|   11 | 3470 | `	if( nBase < 1 ){` |
|  ! 0 | 3471 | `		return ZipFail(pCtx,pZip,ZIP_ER_INVAL);` |
|    - | 3472 | `	}` |
|   11 | 3473 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|    - | 3474 | `		/* php takes one name or a list of them, and a name it cannot find stops` |
|    - | 3475 | `		 * the whole extraction with ER_NOENT and no warning. */` |
|    7 | 3476 | `		ph7_value *pList = apArg[1];` |
|    7 | 3477 | `		if( ph7_value_is_array(pList) ){` |
|    7 | 3478 | `			ph7_hashmap *pMap = (ph7_hashmap *)pList->x.pOther;` |
|    - | 3479 | `			ph7_hashmap_node *pNode;` |
|    7 | 3480 | `			if( pMap == 0 \|\| pMap->nEntry < 1 ){` |
|    - | 3481 | `				/* php refuses an EMPTY list outright rather than extracting` |
|    - | 3482 | ``				 * nothing: `extractTo($d, [])` is false. */`` |
|    3 | 3483 | `				ph7_result_bool(pCtx,0);` |
|    3 | 3484 | `				return PH7_OK;` |
|    - | 3485 | `			}` |
|    5 | 3486 | `			pMap->pCur = pMap->pFirst;` |
|    7 | 3487 | `			while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|    - | 3488 | `				ph7_value sVal;` |
|    - | 3489 | `				phl_zip_ent *pEnt;` |
|    - | 3490 | `				const char *zName;` |
|    5 | 3491 | `				int nName = 0;` |
|    5 | 3492 | `				PH7_MemObjInit(pCtx->pVm,&sVal);` |
|    5 | 3493 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|    5 | 3494 | `				zName = ph7_value_to_string(&sVal,&nName);` |
|    5 | 3495 | `				pEnt = ZipFind(pZip,zName,(sxu32)(nName < 0 ? 0 : nName),0,0);` |
|    5 | 3496 | `				if( pEnt == 0 \|\| !ZipExtractOne(pCtx,pZip,pEnt,zBase,(sxu32)nBase) ){` |
|    3 | 3497 | `					if( pEnt == 0 ){` |
|    3 | 3498 | `						pZip->iStatus = ZIP_ER_NOENT;` |
|    1 | 3499 | `					}` |
|    3 | 3500 | `					PH7_MemObjRelease(&sVal);` |
|    3 | 3501 | `					ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|    3 | 3502 | `					ph7_result_bool(pCtx,0);` |
|    3 | 3503 | `					return PH7_OK;` |
|    - | 3504 | `				}` |
|    3 | 3505 | `				PH7_MemObjRelease(&sVal);` |
|    1 | 3506 | `			}` |
|    3 | 3507 | `			ph7_result_bool(pCtx,1);` |
|    3 | 3508 | `			return PH7_OK;` |
|  ! 0 | 3509 | `		}else{` |
|  ! 0 | 3510 | `			int nName = 0;` |
|  ! 0 | 3511 | `			const char *zName = ph7_value_to_string(pList,&nName);` |
|  ! 0 | 3512 | `			phl_zip_ent *pEnt = ZipFind(pZip,zName,(sxu32)(nName < 0 ? 0 : nName),0,0);` |
|  ! 0 | 3513 | `			if( pEnt == 0 ){` |
|  ! 0 | 3514 | `				return ZipFail(pCtx,pZip,ZIP_ER_NOENT);` |
|    - | 3515 | `			}` |
|  ! 0 | 3516 | `			ph7_result_bool(pCtx,ZipExtractOne(pCtx,pZip,pEnt,zBase,(sxu32)nBase));` |
|  ! 0 | 3517 | `			return PH7_OK;` |
|    - | 3518 | `		}` |
|    - | 3519 | `	}` |
|   19 | 3520 | `	for( i = 0 ; i < pZip->nEnt ; ++i ){` |
|   15 | 3521 | `		phl_zip_ent *pEnt = pZip->apEnt[i];` |
|   15 | 3522 | `		if( pEnt->bDeleted ){` |
|  ! 0 | 3523 | `			continue;` |
|    - | 3524 | `		}` |
|   15 | 3525 | `		if( !ZipExtractOne(pCtx,pZip,pEnt,zBase,(sxu32)nBase) ){` |
|  ! 0 | 3526 | `			ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|  ! 0 | 3527 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 3528 | `			return PH7_OK;` |
|    - | 3529 | `		}` |
|    8 | 3530 | `	}` |
|    5 | 3531 | `	ph7_result_bool(pCtx,1);` |
|    5 | 3532 | `	return PH7_OK;` |
|    7 | 3533 | `}` |
|    - | 3534 | `/* ------------------------------------------------------------------ */` |
|    - | 3535 | `/* addGlob() and addPattern()                                          */` |
|    - | 3536 | `/* ------------------------------------------------------------------ */` |
|    - | 3537 | ``/* One option out of the `$options` array both doors take. */`` |
|   12 | 3538 | `static int ZipOptStr(ph7_value *pOpt,const char *zKey,SyBlob *pOut)` |
|    1 | 3539 | `{` |
|    - | 3540 | `	ph7_value *pVal;` |
|    - | 3541 | `	const char *z;` |
|   13 | 3542 | `	int n = 0;` |
|   13 | 3543 | `	if( pOpt == 0 \|\| !ph7_value_is_array(pOpt) ){` |
|  ! 0 | 3544 | `		return 0;` |
|    - | 3545 | `	}` |
|   13 | 3546 | `	pVal = ph7_array_fetch(pOpt,zKey,-1);` |
|   13 | 3547 | `	if( pVal == 0 ){` |
|   13 | 3548 | `		return 0;` |
|    - | 3549 | `	}` |
|  ! 0 | 3550 | `	z = ph7_value_to_string(pVal,&n);` |
|  ! 0 | 3551 | `	if( n > 0 ){` |
|  ! 0 | 3552 | `		SyBlobAppend(pOut,z,(sxu32)n);` |
|  ! 0 | 3553 | `	}` |
|  ! 0 | 3554 | `	return 1;` |
|    7 | 3555 | `}` |
|   12 | 3556 | `static int ZipOptBool(ph7_value *pOpt,const char *zKey)` |
|    1 | 3557 | `{` |
|    - | 3558 | `	ph7_value *pVal;` |
|   13 | 3559 | `	if( pOpt == 0 \|\| !ph7_value_is_array(pOpt) ){` |
|  ! 0 | 3560 | `		return 0;` |
|    - | 3561 | `	}` |
|   13 | 3562 | `	pVal = ph7_array_fetch(pOpt,zKey,-1);` |
|   13 | 3563 | `	return pVal != 0 && ph7_value_to_bool(pVal) != 0;` |
|    7 | 3564 | `}` |
|   12 | 3565 | `static int ZipOptInt(ph7_value *pOpt,const char *zKey,int iDefault)` |
|    1 | 3566 | `{` |
|    - | 3567 | `	ph7_value *pVal;` |
|   13 | 3568 | `	if( pOpt == 0 \|\| !ph7_value_is_array(pOpt) ){` |
|    3 | 3569 | `		return iDefault;` |
|    - | 3570 | `	}` |
|   11 | 3571 | `	pVal = ph7_array_fetch(pOpt,zKey,-1);` |
|   11 | 3572 | `	return pVal ? (int)ph7_value_to_int(pVal) : iDefault;` |
|    7 | 3573 | `}` |
|    - | 3574 | `/*` |
|    - | 3575 | ` * The entry name one matched PATH gets, under php's three path options:` |
|    - | 3576 | `` * `remove_all_path` keeps the basename only, `remove_path` strips a PREFIX,`` |
|    - | 3577 | `` * and `add_path` is put in front of whatever is left. They are applied in that`` |
|    - | 3578 | ` * order and the first two are exclusive.` |
|    - | 3579 | ` */` |
|   12 | 3580 | `static void ZipGlobEntryName(ph7_value *pOpt,const char *zPath,sxu32 nPath,SyBlob *pOut)` |
|    1 | 3581 | `{` |
|    - | 3582 | `	SyBlob sAdd,sRem;` |
|   13 | 3583 | `	sxu32 nStart = 0;` |
|   13 | 3584 | `	SyBlobInit(&sAdd,pOut->pAllocator);` |
|   13 | 3585 | `	SyBlobInit(&sRem,pOut->pAllocator);` |
|   13 | 3586 | `	if( ZipOptBool(pOpt,"remove_all_path") ){` |
|    - | 3587 | `		sxu32 i;` |
|  619 | 3588 | `		for( i = 0 ; i < nPath ; ++i ){` |
|  607 | 3589 | `			if( zPath[i] == '/' \|\| zPath[i] == PH7_PATH_SEP ){` |
|   73 | 3590 | `				nStart = i + 1;` |
|   48 | 3591 | `			}` |
|  439 | 3592 | `		}` |
|    7 | 3593 | `	}else if( ZipOptStr(pOpt,"remove_path",&sRem) && SyBlobLength(&sRem) > 0` |
|  ! 0 | 3594 | `	       && SyBlobLength(&sRem) <= nPath` |
|  ! 0 | 3595 | `	       && SyMemcmp(zPath,SyBlobData(&sRem),SyBlobLength(&sRem)) == 0 ){` |
|  ! 0 | 3596 | `		nStart = SyBlobLength(&sRem);` |
|  ! 0 | 3597 | `	}` |
|   13 | 3598 | `	if( ZipOptStr(pOpt,"add_path",&sAdd) && SyBlobLength(&sAdd) > 0 ){` |
|  ! 0 | 3599 | `		SyBlobAppend(pOut,SyBlobData(&sAdd),SyBlobLength(&sAdd));` |
|  ! 0 | 3600 | `	}` |
|   13 | 3601 | `	SyBlobAppend(pOut,&zPath[nStart],nPath - nStart);` |
|   13 | 3602 | `	SyBlobRelease(&sAdd);` |
|   13 | 3603 | `	SyBlobRelease(&sRem);` |
|   13 | 3604 | `}` |
|    - | 3605 | `/*` |
|    - | 3606 | `` * The shared half of `addGlob()` and `addPattern()`: a list of PATHS, each`` |
|    - | 3607 | ` * added under the name the options make of it. Directories are skipped -- php` |
|    - | 3608 | ` * stats every candidate and takes only regular files, which is also what keeps` |
|    - | 3609 | `` * `.` and `..` out of a pattern walk.`` |
|    - | 3610 | ` *` |
|    - | 3611 | ` * The answer is the list of matched PATHS, not of entry names, and one add that` |
|    - | 3612 | ` * fails makes the whole call FALSE.` |
|    - | 3613 | ` */` |
|   12 | 3614 | `static int ZipAddMatches(ph7_context *pCtx,phl_zip *pZip,SyBlob *pHit,SySet *pSet,` |
|    - | 3615 | `	const char *zDir,sxu32 nDir,ph7_value *pOpt,int iFlags)` |
|    1 | 3616 | `{` |
|   13 | 3617 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|    - | 3618 | `	ph7_value *pArray,*pVal;` |
|   13 | 3619 | `	PH7_GlobHit *aHit = (PH7_GlobHit *)SySetBasePtr(pSet);` |
|   13 | 3620 | `	sxu32 n,nHit = SySetUsed(pSet);` |
|   13 | 3621 | `	int iMethod = ZipOptInt(pOpt,"comp_method",ZIP_CM_DEFAULT);` |
|   13 | 3622 | `	pArray = ph7_context_new_array(pCtx);` |
|   13 | 3623 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   13 | 3624 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|  ! 0 | 3625 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 3626 | `		return PH7_OK;` |
|    - | 3627 | `	}` |
|   25 | 3628 | `	for( n = 0 ; n < nHit ; ++n ){` |
|   13 | 3629 | `		const char *zName = (const char *)SyBlobData(pHit) + aHit[n].nOfs;` |
|   13 | 3630 | `		sxu32 nName = aHit[n].nLen;` |
|    - | 3631 | `		SyBlob sPath,sEntry;` |
|    - | 3632 | `		phl_zip_ent *pEnt;` |
|    - | 3633 | `		int rc;` |
|   13 | 3634 | `		SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|   13 | 3635 | `		if( nDir > 0 ){` |
|  ! 0 | 3636 | `			SyBlobAppend(&sPath,zDir,nDir);` |
|  ! 0 | 3637 | `			SyBlobAppend(&sPath,"/",sizeof(char));` |
|  ! 0 | 3638 | `		}` |
|   13 | 3639 | `		SyBlobAppend(&sPath,zName,nName);` |
|   13 | 3640 | `		SyBlobNullAppend(&sPath);` |
|   12 | 3641 | `		if( pVfs == 0 \|\| pVfs->xIsfile == 0` |
|   13 | 3642 | `		 \|\| pVfs->xIsfile((const char *)SyBlobData(&sPath)) != PH7_OK ){` |
|  ! 0 | 3643 | `			SyBlobRelease(&sPath);` |
|  ! 0 | 3644 | `			continue;` |
|    - | 3645 | `		}` |
|   13 | 3646 | `		SyBlobInit(&sEntry,&pCtx->pVm->sAllocator);` |
|   13 | 3647 | `		ZipGlobEntryName(pOpt,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath),&sEntry);` |
|   19 | 3648 | `		pEnt = ZipAddSlot(pCtx,pZip,(const char *)SyBlobData(&sEntry),` |
|    6 | 3649 | `			SyBlobLength(&sEntry),iFlags,&rc);` |
|   13 | 3650 | `		SyBlobRelease(&sEntry);` |
|   13 | 3651 | `		if( pEnt == 0 ){` |
|  ! 0 | 3652 | `			SyBlobRelease(&sPath);` |
|  ! 0 | 3653 | `			ph7_context_release_value(pCtx,pVal);` |
|  ! 0 | 3654 | `			ph7_context_release_value(pCtx,pArray);` |
|  ! 0 | 3655 | `			return rc;` |
|    - | 3656 | `		}` |
|   13 | 3657 | `		SyBlobAppend(&pEnt->sSrcPath,SyBlobData(&sPath),SyBlobLength(&sPath));` |
|   13 | 3658 | `		SyBlobNullAppend(&pEnt->sSrcPath);` |
|   13 | 3659 | `		pEnt->iSrcStart = 0;` |
|   13 | 3660 | `		pEnt->iSrcLen = -1;` |
|   13 | 3661 | `		pEnt->bDir = 0;` |
|   13 | 3662 | `		pEnt->iSetMethod = iMethod;` |
|    - | 3663 | `		{` |
|   19 | 3664 | `			ph7_int64 iSize = pVfs->xFileSize` |
|   12 | 3665 | `				? pVfs->xFileSize((const char *)SyBlobData(&sPath)) : 0;` |
|   13 | 3666 | `			pEnt->nSize = (sxu32)(iSize > 0 ? iSize : 0);` |
|   13 | 3667 | `			pEnt->nCompSize = pEnt->nSize;` |
|   13 | 3668 | `			pEnt->iMethod = ZIP_CM_STORE;` |
|    - | 3669 | `		}` |
|   13 | 3670 | `		ph7_value_reset_string_cursor(pVal);` |
|   13 | 3671 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sPath),(int)SyBlobLength(&sPath));` |
|   13 | 3672 | `		ph7_array_add_elem(pArray,0,pVal);` |
|   13 | 3673 | `		SyBlobRelease(&sPath);` |
|    7 | 3674 | `	}` |
|   13 | 3675 | `	ZipRefresh(PH7_ContextThis(pCtx),pZip);` |
|   13 | 3676 | `	ph7_result_value(pCtx,pArray);` |
|   13 | 3677 | `	ph7_context_release_value(pCtx,pVal);` |
|   13 | 3678 | `	return PH7_OK;` |
|    7 | 3679 | `}` |
|    - | 3680 | `/*` |
|    - | 3681 | `` * `addGlob()`. Its `$flags` are **glob()'s**, not the FL_ ones every other door`` |
|    - | 3682 | `` * takes -- `GLOB_BRACE`, `GLOB_NOSORT` and the rest -- so the expansion is`` |
|    - | 3683 | `` * PHL's own `glob()` rather than a second walk of its own. That keeps the two`` |
|    - | 3684 | ` * from drifting on the one thing this argument decides, and it costs a call` |
|    - | 3685 | ` * into php: the function is the prelude's, and its flag screen and brace` |
|    - | 3686 | ` * expansion are already php's.` |
|    - | 3687 | ` *` |
|    - | 3688 | ` * The only thing spelled here is the REFUSAL, because php's belongs to this` |
|    - | 3689 | `` * method and names it: `ZipArchive::addGlob(): At least one of the passed flags`` |
|    - | 3690 | `` * is invalid...`, where the plain function would have said `glob(): …`.`` |
|    - | 3691 | ` */` |
|   16 | 3692 | `static int vm_builtin_ZipArchive_addGlob(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3693 | `{` |
|   17 | 3694 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3695 | `	ph7_value sName,sPat,sFlags,sRet;` |
|    - | 3696 | `	ph7_value *apCall[2];` |
|    - | 3697 | `	SyString sFn;` |
|    - | 3698 | `	const char *zPat;` |
|    - | 3699 | `	SyBlob sHit;` |
|    - | 3700 | `	SySet aHit;` |
|   17 | 3701 | `	int nPat = 0,iFlags = 0,rc;` |
|   17 | 3702 | `	if( pZip == 0 ){` |
|  ! 0 | 3703 | `		return PH7_OK;` |
|    - | 3704 | `	}` |
|   17 | 3705 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|   17 | 3706 | `	if( nArg > 1 ){` |
|   15 | 3707 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|    7 | 3708 | `	}` |
|   17 | 3709 | `	if( iFlags & ~ZIP_GLOB_FLAGS ){` |
|    5 | 3710 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    - | 3711 | `			"At least one of the passed flags is invalid or not supported on this platform");` |
|    5 | 3712 | `		ph7_result_bool(pCtx,0);` |
|    5 | 3713 | `		return PH7_OK;` |
|    - | 3714 | `	}` |
|   13 | 3715 | `	PH7_MemObjInit(pCtx->pVm,&sName);` |
|   13 | 3716 | `	PH7_MemObjInit(pCtx->pVm,&sPat);` |
|   13 | 3717 | `	PH7_MemObjInit(pCtx->pVm,&sFlags);` |
|   13 | 3718 | `	PH7_MemObjInit(pCtx->pVm,&sRet);` |
|   13 | 3719 | `	SyStringInitFromBuf(&sFn,"glob",sizeof("glob")-1);` |
|   13 | 3720 | `	PH7_MemObjInitFromString(pCtx->pVm,&sName,&sFn);` |
|   13 | 3721 | `	ph7_value_string(&sPat,zPat,nPat);` |
|   13 | 3722 | `	ph7_value_int(&sFlags,iFlags);` |
|   13 | 3723 | `	apCall[0] = &sPat;` |
|   13 | 3724 | `	apCall[1] = &sFlags;` |
|   13 | 3725 | `	PH7_VmCallUserFunction(pCtx->pVm,&sName,2,apCall,&sRet);` |
|   13 | 3726 | `	SyBlobInit(&sHit,&pCtx->pVm->sAllocator);` |
|   13 | 3727 | `	SySetInit(&aHit,&pCtx->pVm->sAllocator,sizeof(PH7_GlobHit));` |
|   13 | 3728 | `	if( ph7_value_is_array(&sRet) ){` |
|   13 | 3729 | `		ph7_hashmap *pMap = (ph7_hashmap *)sRet.x.pOther;` |
|    - | 3730 | `		ph7_hashmap_node *pNode;` |
|   13 | 3731 | `		if( pMap ){` |
|   13 | 3732 | `			pMap->pCur = pMap->pFirst;` |
|   25 | 3733 | `			while( (pNode = PH7_HashmapGetNextEntry(pMap)) != 0 ){` |
|    - | 3734 | `				ph7_value sVal;` |
|    - | 3735 | `				const char *zHit;` |
|   13 | 3736 | `				int nHit = 0;` |
|    - | 3737 | `				PH7_GlobHit sRec;` |
|   13 | 3738 | `				PH7_MemObjInit(pCtx->pVm,&sVal);` |
|   13 | 3739 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|   13 | 3740 | `				zHit = ph7_value_to_string(&sVal,&nHit);` |
|   13 | 3741 | `				if( nHit > 0 ){` |
|   13 | 3742 | `					sRec.nOfs = SyBlobLength(&sHit);` |
|   13 | 3743 | `					sRec.nLen = (sxu32)nHit;` |
|   13 | 3744 | `					SyBlobAppend(&sHit,zHit,(sxu32)nHit);` |
|   13 | 3745 | `					SySetPut(&aHit,(const void *)&sRec);` |
|    6 | 3746 | `				}` |
|   13 | 3747 | `				PH7_MemObjRelease(&sVal);` |
|    1 | 3748 | `			}` |
|    6 | 3749 | `		}` |
|   13 | 3750 | `		rc = ZipAddMatches(pCtx,pZip,&sHit,&aHit,0,0,nArg > 2 ? apArg[2] : 0,0);` |
|    7 | 3751 | `	}else{` |
|    - | 3752 | `		/* php's other refusal: the expansion itself failed. */` |
|  ! 0 | 3753 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"No such file or directory");` |
|  ! 0 | 3754 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 3755 | `		rc = PH7_OK;` |
|    - | 3756 | `	}` |
|   13 | 3757 | `	SyBlobRelease(&sHit);` |
|   13 | 3758 | `	SySetRelease(&aHit);` |
|   13 | 3759 | `	PH7_MemObjRelease(&sRet);` |
|   13 | 3760 | `	PH7_MemObjRelease(&sFlags);` |
|   13 | 3761 | `	PH7_MemObjRelease(&sPat);` |
|   13 | 3762 | `	PH7_MemObjRelease(&sName);` |
|   13 | 3763 | `	return rc;` |
|    9 | 3764 | `}` |
|  ! 0 | 3765 | `static int vm_builtin_ZipArchive_addPattern(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3766 | `{` |
|  ! 0 | 3767 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|  ! 0 | 3768 | `	const char *zPat,*zDir = ".";` |
|    - | 3769 | `	SyBlob sHit,sKeep;` |
|    - | 3770 | `	SySet aHit,aKeep;` |
|  ! 0 | 3771 | `	int nPat = 0,nDir = 1,rc;` |
|  ! 0 | 3772 | `	if( pZip == 0 ){` |
|  ! 0 | 3773 | `		return PH7_OK;` |
|    - | 3774 | `	}` |
|  ! 0 | 3775 | `	zPat = ph7_value_to_string(apArg[0],&nPat);` |
|  ! 0 | 3776 | `	if( nArg > 1 ){` |
|  ! 0 | 3777 | `		int n = 0;` |
|  ! 0 | 3778 | `		const char *z = ph7_value_to_string(apArg[1],&n);` |
|  ! 0 | 3779 | `		if( n > 0 ){` |
|  ! 0 | 3780 | `			zDir = z;` |
|  ! 0 | 3781 | `			nDir = n;` |
|  ! 0 | 3782 | `		}` |
|  ! 0 | 3783 | `	}` |
|  ! 0 | 3784 | `	SyBlobInit(&sHit,&pCtx->pVm->sAllocator);` |
|  ! 0 | 3785 | `	SySetInit(&aHit,&pCtx->pVm->sAllocator,sizeof(PH7_GlobHit));` |
|  ! 0 | 3786 | `	SyBlobInit(&sKeep,&pCtx->pVm->sAllocator);` |
|  ! 0 | 3787 | `	SySetInit(&aKeep,&pCtx->pVm->sAllocator,sizeof(PH7_GlobHit));` |
|  ! 0 | 3788 | `	if( PH7_VfsListDir(pCtx->pVm,zDir,nDir,&sHit,&aHit) == SXRET_OK ){` |
|  ! 0 | 3789 | `		PH7_GlobHit *aRec = (PH7_GlobHit *)SySetBasePtr(&aHit);` |
|    - | 3790 | `		sxu32 n;` |
|  ! 0 | 3791 | `		for( n = 0 ; n < SySetUsed(&aHit) ; ++n ){` |
|  ! 0 | 3792 | `			const char *zName = (const char *)SyBlobData(&sHit) + aRec[n].nOfs;` |
|    - | 3793 | `			PH7_GlobHit sKept;` |
|  ! 0 | 3794 | `			int bMatch = 0;` |
|    - | 3795 | `#ifdef PH7_ENABLE_PCRE` |
|    - | 3796 | `			/* php matches the pattern against the ENTRY name, never against the` |
|    - | 3797 | `			 * path it will be added under. */` |
|  ! 0 | 3798 | `			if( PH7_PcreMatchQuiet(pCtx,zPat,nPat,zName,(int)aRec[n].nLen,&bMatch)` |
|  ! 0 | 3799 | `				!= SXRET_OK ){` |
|  ! 0 | 3800 | `				bMatch = 0;` |
|  ! 0 | 3801 | `			}` |
|    - | 3802 | `#else` |
|    - | 3803 | `			SXUNUSED(zPat); SXUNUSED(nPat);` |
|    - | 3804 | `#endif` |
|  ! 0 | 3805 | `			if( !bMatch ){` |
|  ! 0 | 3806 | `				continue;` |
|    - | 3807 | `			}` |
|  ! 0 | 3808 | `			sKept.nOfs = SyBlobLength(&sKeep);` |
|  ! 0 | 3809 | `			sKept.nLen = aRec[n].nLen;` |
|  ! 0 | 3810 | `			SyBlobAppend(&sKeep,zName,aRec[n].nLen);` |
|  ! 0 | 3811 | `			SySetPut(&aKeep,(const void *)&sKept);` |
|  ! 0 | 3812 | `		}` |
|  ! 0 | 3813 | `	}` |
|  ! 0 | 3814 | `	rc = ZipAddMatches(pCtx,pZip,&sKeep,&aKeep,zDir,(sxu32)nDir,` |
|  ! 0 | 3815 | `		nArg > 2 ? apArg[2] : 0,0);` |
|  ! 0 | 3816 | `	SyBlobRelease(&sHit);` |
|  ! 0 | 3817 | `	SySetRelease(&aHit);` |
|  ! 0 | 3818 | `	SyBlobRelease(&sKeep);` |
|  ! 0 | 3819 | `	SySetRelease(&aKeep);` |
|  ! 0 | 3820 | `	return rc;` |
|  ! 0 | 3821 | `}` |
|    - | 3822 | `/* ------------------------------------------------------------------ */` |
|    - | 3823 | `/* The callbacks, the password and the two static questions            */` |
|    - | 3824 | `/* ------------------------------------------------------------------ */` |
|  ! 0 | 3825 | `static int ZipRegisterCallback(ph7_context *pCtx,ph7_value **ppSlot,ph7_value *pFunc)` |
|  ! 0 | 3826 | `{` |
|  ! 0 | 3827 | `	ph7_vm *pVm = pCtx->pVm;` |
|  ! 0 | 3828 | `	if( *ppSlot ){` |
|  ! 0 | 3829 | `		ph7_release_value(pVm,*ppSlot);` |
|  ! 0 | 3830 | `		*ppSlot = 0;` |
|  ! 0 | 3831 | `	}` |
|  ! 0 | 3832 | `	*ppSlot = ph7_new_scalar(pVm);` |
|  ! 0 | 3833 | `	if( *ppSlot == 0 ){` |
|  ! 0 | 3834 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 3835 | `	}` |
|  ! 0 | 3836 | `	PH7_MemObjStore(pFunc,*ppSlot);` |
|  ! 0 | 3837 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 | 3838 | `	return PH7_OK;` |
|  ! 0 | 3839 | `}` |
|  ! 0 | 3840 | `static int vm_builtin_ZipArchive_registerProgressCallback(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3841 | `{` |
|  ! 0 | 3842 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|  ! 0 | 3843 | `	SXUNUSED(nArg);` |
|  ! 0 | 3844 | `	if( pZip == 0 ){` |
|  ! 0 | 3845 | `		return PH7_OK;` |
|    - | 3846 | `	}` |
|  ! 0 | 3847 | `	pZip->rProgressRate = (double)ph7_value_to_double(apArg[0]);` |
|  ! 0 | 3848 | `	return ZipRegisterCallback(pCtx,&pZip->pProgress,apArg[1]);` |
|  ! 0 | 3849 | `}` |
|  ! 0 | 3850 | `static int vm_builtin_ZipArchive_registerCancelCallback(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 3851 | `{` |
|  ! 0 | 3852 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|  ! 0 | 3853 | `	SXUNUSED(nArg);` |
|  ! 0 | 3854 | `	if( pZip == 0 ){` |
|  ! 0 | 3855 | `		return PH7_OK;` |
|    - | 3856 | `	}` |
|  ! 0 | 3857 | `	return ZipRegisterCallback(pCtx,&pZip->pCancel,apArg[0]);` |
|  ! 0 | 3858 | `}` |
|   64 | 3859 | `static int vm_builtin_ZipArchive_setPassword(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3860 | `{` |
|   65 | 3861 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3862 | `	const char *z;` |
|   65 | 3863 | `	int n = 0;` |
|   32 | 3864 | `	SXUNUSED(nArg);` |
|   65 | 3865 | `	if( pZip == 0 ){` |
|  ! 0 | 3866 | `		return PH7_OK;` |
|    - | 3867 | `	}` |
|   65 | 3868 | `	z = ph7_value_to_string(apArg[0],&n);` |
|   65 | 3869 | `	if( n < 1 ){` |
|    - | 3870 | `		/* php refuses the EMPTY password rather than clearing the one that is` |
|    - | 3871 | `		 * there: there is no way to un-set one through this door. */` |
|  ! 0 | 3872 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 3873 | `		return PH7_OK;` |
|    - | 3874 | `	}` |
|   65 | 3875 | `	SyBlobReset(&pZip->sPassword);` |
|   65 | 3876 | `	SyBlobAppend(&pZip->sPassword,z,(sxu32)n);` |
|   65 | 3877 | `	ph7_result_bool(pCtx,1);` |
|   65 | 3878 | `	return PH7_OK;` |
|   33 | 3879 | `}` |
|    - | 3880 | `/*` |
|    - | 3881 | `` * `setEncryptionIndex()` / `setEncryptionName()`.`` |
|    - | 3882 | ` *` |
|    - | 3883 | ` * Three refusals, and php words each differently because each comes from a` |
|    - | 3884 | `` * different place. A method no build can write is `Encryption method not`` |
|    - | 3885 | `` * supported` and a plain false. A NAME nothing answers to is `No such file`,`` |
|    - | 3886 | ` * also plain. And an INDEX out of range is the odd one: php's stub hands the` |
|    - | 3887 | ` * library a NULL password on the way past and the library refusing THAT is what` |
|    - | 3888 | `` * the `password reset failed` warning is, with `Invalid argument` behind it.`` |
|    - | 3889 | ` *` |
|    - | 3890 | ` * The password a call names belongs to the ENTRY and outranks the archive's for` |
|    - | 3891 | ` * this entry's WRITE -- and reading is the other way round, since only the` |
|    - | 3892 | ` * archive's opens anything.` |
|    - | 3893 | ` */` |
|   34 | 3894 | `static int ZipSetEncryption(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt,` |
|    - | 3895 | `	int nArg,ph7_value **apArg)` |
|    1 | 3896 | `{` |
|   35 | 3897 | `	int iMethod = (int)ph7_value_to_int(apArg[1]);` |
|   35 | 3898 | `	if( iMethod != ZIP_EM_NONE && !ZipEncSupported(iMethod) ){` |
|    5 | 3899 | `		return ZipFail(pCtx,pZip,ZIP_ER_ENCRNOTSUPP);` |
|    - | 3900 | `	}` |
|   31 | 3901 | `	pEnt->iSetEncrypt = iMethod;` |
|   31 | 3902 | `	pEnt->bHasPassword = 0;` |
|   31 | 3903 | `	SyBlobReset(&pEnt->sPassword);` |
|   31 | 3904 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    3 | 3905 | `		int nPw = 0;` |
|    3 | 3906 | `		const char *zPw = ph7_value_to_string(apArg[2],&nPw);` |
|    3 | 3907 | `		if( nPw > 0 ){` |
|    3 | 3908 | `			SyBlobAppend(&pEnt->sPassword,zPw,(sxu32)nPw);` |
|    3 | 3909 | `			pEnt->bHasPassword = 1;` |
|    1 | 3910 | `		}` |
|    1 | 3911 | `	}` |
|   31 | 3912 | `	ph7_result_bool(pCtx,1);` |
|   31 | 3913 | `	return PH7_OK;` |
|   18 | 3914 | `}` |
|    - | 3915 | ``/* The `password reset failed` half: an INDEX that names nothing, which is the`` |
|    - | 3916 | ` * only door of the two that reaches the library before it has an entry. */` |
|    4 | 3917 | `static int ZipEncryptionBadIndex(ph7_context *pCtx,phl_zip *pZip)` |
|    1 | 3918 | `{` |
|    5 | 3919 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"password reset failed");` |
|    5 | 3920 | `	return ZipFail(pCtx,pZip,ZIP_ER_INVAL);` |
|    1 | 3921 | `}` |
|    4 | 3922 | `static int vm_builtin_ZipArchive_setEncryptionIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3923 | `{` |
|    5 | 3924 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3925 | `	phl_zip_ent *pEnt;` |
|    5 | 3926 | `	if( pZip == 0 ){` |
|  ! 0 | 3927 | `		return PH7_OK;` |
|    - | 3928 | `	}` |
|    5 | 3929 | `	pEnt = ZipAt(pZip,ph7_value_to_int64(apArg[0]));` |
|    5 | 3930 | `	if( pEnt == 0 ){` |
|    5 | 3931 | `		return ZipEncryptionBadIndex(pCtx,pZip);` |
|    - | 3932 | `	}` |
|  ! 0 | 3933 | `	return ZipSetEncryption(pCtx,pZip,pEnt,nArg,apArg);` |
|    3 | 3934 | `}` |
|   36 | 3935 | `static int vm_builtin_ZipArchive_setEncryptionName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3936 | `{` |
|   37 | 3937 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 3938 | `	phl_zip_ent *pEnt;` |
|    - | 3939 | `	int rc;` |
|   37 | 3940 | `	if( pZip == 0 ){` |
|  ! 0 | 3941 | `		return PH7_OK;` |
|    - | 3942 | `	}` |
|   37 | 3943 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 3944 | `		return rc;` |
|    - | 3945 | `	}` |
|   37 | 3946 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],0,&rc);` |
|   37 | 3947 | `	if( pEnt == 0 ){` |
|    3 | 3948 | `		return rc;` |
|    - | 3949 | `	}` |
|   35 | 3950 | `	return ZipSetEncryption(pCtx,pZip,pEnt,nArg,apArg);` |
|   19 | 3951 | `}` |
|    - | 3952 | `/* What this BUILD can do, which is what php answers too: its own reply comes` |
|    - | 3953 | ` * from the libzip it was linked against, so a method php names a constant for` |
|    - | 3954 | ` * is not a method php can necessarily write. */` |
|    6 | 3955 | `static int vm_builtin_ZipArchive_isCompressionMethodSupported(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3956 | `{` |
|    7 | 3957 | `	int iMethod = (int)ph7_value_to_int(apArg[0]);` |
|    3 | 3958 | `	SXUNUSED(nArg);` |
|    8 | 3959 | `	ph7_result_bool(pCtx,iMethod == ZIP_CM_DEFAULT \|\| iMethod == ZIP_CM_STORE` |
|    3 | 3960 | `		\|\| iMethod == ZIP_CM_DEFLATE);` |
|    7 | 3961 | `	return PH7_OK;` |
|    1 | 3962 | `}` |
|   12 | 3963 | `static int vm_builtin_ZipArchive_isEncryptionMethodSupported(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 3964 | `{` |
|   13 | 3965 | `	int iMethod = (int)ph7_value_to_int(apArg[0]);` |
|    6 | 3966 | `	SXUNUSED(nArg);` |
|   13 | 3967 | `	ph7_result_bool(pCtx,ZipEncSupported(iMethod));` |
|   13 | 3968 | `	return PH7_OK;` |
|    1 | 3969 | `}` |
|    - | 3970 | `/* ------------------------------------------------------------------ */` |
|    - | 3971 | ``/* The stream: getStream() and the read-only `zip://` wrapper          */`` |
|    - | 3972 | `/* ------------------------------------------------------------------ */` |
|    - | 3973 | `/*` |
|    - | 3974 | ` * One open entry. The bytes are copied at open so nothing the script does to` |
|    - | 3975 | ` * the archive afterwards moves them, and the ARCHIVE is kept alive alongside --` |
|    - | 3976 | `` * php's `close()` leaves a stream getStream() handed out readable, and only the`` |
|    - | 3977 | ` * OBJECT dying takes it down.` |
|    - | 3978 | ` */` |
|    - | 3979 | `typedef struct phl_zip_io phl_zip_io;` |
|    - | 3980 | `struct phl_zip_io {` |
|    - | 3981 | `	phl_zip *pZip;      /* the archive, held; see ZipStreamRead for what ends the read */` |
|    - | 3982 | `	SyBlob sData;` |
|    - | 3983 | `	sxu32 nCur;` |
|    - | 3984 | ``	int bWrapper;       /* opened through `zip://`, which php names in the meta */`` |
|    - | 3985 | `	int bRead;          /* a read has already been served from this handle */` |
|    - | 3986 | `};` |
|    - | 3987 | `/*` |
|    - | 3988 | `` * `zip://archive.zip#entry`, php's read-only wrapper. It is an opener and`` |
|    - | 3989 | `` * NOTHING else: php gives it no url_stat (so `file_exists('zip://…')` is false)`` |
|    - | 3990 | ` * and no directory door, which is why an archive can only be LISTED through the` |
|    - | 3991 | ` * class.` |
|    - | 3992 | ` */` |
|   14 | 3993 | `static int ZipStreamOpen(const char *zName,int iMode,ph7_value *pResource,void **ppHandle)` |
|    1 | 3994 | `{` |
|   15 | 3995 | `	ph7_vm *pVm = pResource ? pResource->pVm : 0;` |
|    - | 3996 | `	phl_zip *pZip;` |
|    - | 3997 | `	phl_zip_io *pIo;` |
|    - | 3998 | `	phl_zip_ent *pEnt;` |
|   15 | 3999 | `	const char *zHash = 0;` |
|    - | 4000 | `	SyBlob sPath;` |
|    - | 4001 | `	const ph7_io_stream *pStream;` |
|    - | 4002 | `	void *pFile;` |
|   15 | 4003 | `	sxu32 i,nName = (sxu32)SyStrlen(zName);` |
|    - | 4004 | `	int rc;` |
|   15 | 4005 | `	if( pVm == 0 ){` |
|  ! 0 | 4006 | `		return -1;` |
|    - | 4007 | `	}` |
|   15 | 4008 | `	if( iMode & (PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND\|PH7_IO_OPEN_CREATE) ){` |
|    3 | 4009 | `		PH7_StreamSetOpenError(pVm,"operation not supported");` |
|    3 | 4010 | `		return -1;` |
|    - | 4011 | `	}` |
|  589 | 4012 | `	for( i = 0 ; i < nName ; ++i ){` |
|  587 | 4013 | `		if( zName[i] == '#' ){` |
|    - | 4014 | ``			/* the FIRST one: an entry name may hold a `#` of its own, and php`` |
|    - | 4015 | `			 * gives the whole remainder to the entry rather than to the path */` |
|   11 | 4016 | `			zHash = &zName[i];` |
|   11 | 4017 | `			break;` |
|    - | 4018 | `		}` |
|  424 | 4019 | `	}` |
|   13 | 4020 | `	if( zHash == 0 ){` |
|    3 | 4021 | `		PH7_StreamSetOpenError(pVm,"operation failed");` |
|    3 | 4022 | `		return -1;` |
|    - | 4023 | `	}` |
|   11 | 4024 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|   11 | 4025 | `	SyBlobAppend(&sPath,zName,(sxu32)(zHash - zName));` |
|   11 | 4026 | `	SyBlobNullAppend(&sPath);` |
|   11 | 4027 | `	pZip = ZipNew(pVm);` |
|   11 | 4028 | `	if( pZip == 0 ){` |
|  ! 0 | 4029 | `		SyBlobRelease(&sPath);` |
|  ! 0 | 4030 | `		return -1;` |
|    - | 4031 | `	}` |
|   11 | 4032 | `	SyBlobAppend(&pZip->sPath,SyBlobData(&sPath),SyBlobLength(&sPath));` |
|   11 | 4033 | `	SyBlobNullAppend(&pZip->sPath);` |
|    - | 4034 | `	{` |
|   11 | 4035 | `		const char *zTail = (const char *)SyBlobData(&sPath);` |
|   11 | 4036 | `		pStream = PH7_VmGetStreamDevice(pVm,&zTail,(int)SyBlobLength(&sPath));` |
|   11 | 4037 | `		pFile = pStream ? PH7_StreamOpenHandle(pVm,pStream,zTail,PH7_IO_OPEN_RDONLY,` |
|    5 | 4038 | `			FALSE,0,FALSE,0,0) : 0;` |
|   11 | 4039 | `		if( pFile == 0 ){` |
|    3 | 4040 | `			SyBlobRelease(&sPath);` |
|    3 | 4041 | `			ZipRelease(pZip);` |
|    3 | 4042 | `			PH7_StreamSetOpenError(pVm,"operation failed");` |
|    3 | 4043 | `			return -1;` |
|    - | 4044 | `		}` |
|    9 | 4045 | `		PH7_StreamReadWholeFile(pFile,pStream,&pZip->sFile);` |
|    9 | 4046 | `		PH7_StreamCloseHandle(pStream,pFile);` |
|    - | 4047 | `	}` |
|    9 | 4048 | `	SyBlobRelease(&sPath);` |
|    - | 4049 | `	{` |
|    9 | 4050 | `		int rcParse = ZipParse(pZip);` |
|    9 | 4051 | `		if( rcParse != ZIP_ER_OK && rcParse != ZIP_ER_EMPTY_FILE ){` |
|  ! 0 | 4052 | `			ZipRelease(pZip);` |
|  ! 0 | 4053 | `			PH7_StreamSetOpenError(pVm,"operation failed");` |
|  ! 0 | 4054 | `			return -1;` |
|    - | 4055 | `		}` |
|    - | 4056 | `	}` |
|    9 | 4057 | `	pEnt = ZipFind(pZip,&zHash[1],nName - (sxu32)(zHash - zName) - 1,0,0);` |
|    9 | 4058 | `	if( pEnt == 0 \|\| (rc = ZipEntLoad(pZip,pEnt)) != ZIP_ER_OK ){` |
|    3 | 4059 | `		ZipRelease(pZip);` |
|    3 | 4060 | `		PH7_StreamSetOpenError(pVm,"operation failed");` |
|    3 | 4061 | `		return -1;` |
|    - | 4062 | `	}` |
|    7 | 4063 | `	pIo = (phl_zip_io *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_zip_io));` |
|    7 | 4064 | `	if( pIo == 0 ){` |
|  ! 0 | 4065 | `		ZipRelease(pZip);` |
|  ! 0 | 4066 | `		return -1;` |
|    - | 4067 | `	}` |
|    7 | 4068 | `	SyZero(pIo,sizeof(*pIo));` |
|    7 | 4069 | `	pIo->pZip = pZip;` |
|    7 | 4070 | `	pIo->bWrapper = 1;` |
|    7 | 4071 | `	SyBlobInit(&pIo->sData,&pVm->sAllocator);` |
|    7 | 4072 | `	SyBlobAppend(&pIo->sData,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));` |
|    7 | 4073 | `	*ppHandle = (void *)pIo;` |
|    7 | 4074 | `	return PH7_OK;` |
|    8 | 4075 | `}` |
|   14 | 4076 | `static void ZipStreamClose(void *pHandle)` |
|    1 | 4077 | `{` |
|   15 | 4078 | `	phl_zip_io *pIo = (phl_zip_io *)pHandle;` |
|    - | 4079 | `	ph7_vm *pVm;` |
|   15 | 4080 | `	if( pIo == 0 ){` |
|  ! 0 | 4081 | `		return;` |
|    - | 4082 | `	}` |
|   15 | 4083 | `	pVm = pIo->pZip->pVm;` |
|   15 | 4084 | `	SyBlobRelease(&pIo->sData);` |
|   15 | 4085 | `	ZipRelease(pIo->pZip);` |
|   15 | 4086 | `	SyMemBackendFree(&pVm->sAllocator,pIo);` |
|    8 | 4087 | `}` |
|    - | 4088 | `/*` |
|    - | 4089 | ` * A read from a getStream() handle, and the one rule that decides whether it` |
|    - | 4090 | ` * still works after the archive was closed.` |
|    - | 4091 | ` *` |
|    - | 4092 | ` * php's answer is its library's read BUFFER, and it is visible three ways: read` |
|    - | 4093 | ` * a byte, close the archive, and the rest of the entry still comes out; close` |
|    - | 4094 | `` * the archive without reading first, and the read is `Containing zip archive`` |
|    - | 4095 | `` * was closed`; and destroying the OBJECT ends it either way. The bytes are held`` |
|    - | 4096 | ` * here rather than buffered, so the rule is stated instead of inherited -- a` |
|    - | 4097 | ` * handle nothing has been read from dies with its archive, and one that has` |
|    - | 4098 | ` * been read from outlives the close.` |
|    - | 4099 | ` */` |
|   26 | 4100 | `static ph7_int64 ZipStreamRead(void *pHandle,void *pBuffer,ph7_int64 nWant)` |
|    1 | 4101 | `{` |
|   27 | 4102 | `	phl_zip_io *pIo = (phl_zip_io *)pHandle;` |
|    - | 4103 | `	sxu32 nLeft;` |
|   27 | 4104 | `	if( pIo == 0 \|\| pIo->pZip->bDead ){` |
|  ! 0 | 4105 | `		return -1;` |
|    - | 4106 | `	}` |
|   27 | 4107 | `	if( !pIo->bWrapper && !pIo->pZip->bOpen && !pIo->bRead ){` |
|    3 | 4108 | `		return -1;` |
|    - | 4109 | `	}` |
|   25 | 4110 | `	nLeft = SyBlobLength(&pIo->sData) - pIo->nCur;` |
|   25 | 4111 | `	if( nLeft < 1 ){` |
|   11 | 4112 | `		return 0;` |
|    - | 4113 | `	}` |
|   15 | 4114 | `	if( (sxu64)nWant > (sxu64)nLeft ){` |
|   13 | 4115 | `		nWant = (ph7_int64)nLeft;` |
|    6 | 4116 | `	}` |
|   15 | 4117 | `	SyMemcpy((const char *)SyBlobData(&pIo->sData) + pIo->nCur,pBuffer,(sxu32)nWant);` |
|   15 | 4118 | `	pIo->nCur += (sxu32)nWant;` |
|   15 | 4119 | `	pIo->bRead = 1;` |
|   15 | 4120 | `	return nWant;` |
|   14 | 4121 | `}` |
|   10 | 4122 | `static ph7_int64 ZipStreamTell(void *pHandle)` |
|    1 | 4123 | `{` |
|   11 | 4124 | `	phl_zip_io *pIo = (phl_zip_io *)pHandle;` |
|   11 | 4125 | `	return pIo ? (ph7_int64)pIo->nCur : -1;` |
|    1 | 4126 | `}` |
|    - | 4127 | `/* php's zip stream is NOT seekable -- a member is decompressed forwards -- and` |
|    - | 4128 | `` * a script sees that both in `stream_get_meta_data()` and in the warning an`` |
|    - | 4129 | ` * fseek() on one raises. */` |
|    - | 4130 | `PH7_PRIVATE const ph7_io_stream sZIP_Stream = {` |
|    - | 4131 | `	"zip",` |
|    - | 4132 | `	PH7_IO_STREAM_VERSION,` |
|    - | 4133 | `	ZipStreamOpen,   /* xOpen */` |
|    - | 4134 | `	0,               /* xOpenDir */` |
|    - | 4135 | `	ZipStreamClose,  /* xClose */` |
|    - | 4136 | `	0,               /* xCloseDir */` |
|    - | 4137 | `	ZipStreamRead,   /* xRead */` |
|    - | 4138 | `	0,               /* xReadDir */` |
|    - | 4139 | `	0,               /* xWrite */` |
|    - | 4140 | `	0,               /* xSeek */` |
|    - | 4141 | `	0,               /* xLock */` |
|    - | 4142 | `	0,               /* xRewindDir */` |
|    - | 4143 | `	ZipStreamTell,   /* xTell */` |
|    - | 4144 | `	0,               /* xTrunc */` |
|    - | 4145 | `	0,               /* xSync */` |
|    - | 4146 | `	0                /* xStat */` |
|    - | 4147 | `};` |
|  110 | 4148 | `PH7_PRIVATE int PH7_ZipStreamIs(const ph7_io_stream *pStream)` |
|    5 | 4149 | `{` |
|  115 | 4150 | `	return pStream == &sZIP_Stream;` |
|    5 | 4151 | `}` |
|    - | 4152 | ``/* Was this handle opened through the `zip://` wrapper, or handed out by`` |
|    - | 4153 | `` * getStream()? php reports a `wrapper_type` for the first and none for the`` |
|    - | 4154 | ` * second, which is the only thing that tells them apart. */` |
|    4 | 4155 | `PH7_PRIVATE int PH7_ZipStreamViaWrapper(void *pHandle)` |
|    1 | 4156 | `{` |
|    5 | 4157 | `	phl_zip_io *pIo = (phl_zip_io *)pHandle;` |
|    5 | 4158 | `	return pIo != 0 && pIo->bWrapper;` |
|    1 | 4159 | `}` |
|    - | 4160 | ``/* `getStream()`, `getStreamName()` and `getStreamIndex()`, which are one verb`` |
|    - | 4161 | ` * told the entry three ways. The stream php hands back names the ENTRY as its` |
|    - | 4162 | ` * uri and no wrapper at all. */` |
|    8 | 4163 | `static int ZipStreamOf(ph7_context *pCtx,phl_zip *pZip,phl_zip_ent *pEnt)` |
|    1 | 4164 | `{` |
|    - | 4165 | `	io_private *pDev;` |
|    - | 4166 | `	phl_zip_io *pIo;` |
|    9 | 4167 | `	int rc = ZipEntLoad(pZip,pEnt);` |
|    9 | 4168 | `	if( rc != ZIP_ER_OK ){` |
|  ! 0 | 4169 | `		return ZipFail(pCtx,pZip,rc);` |
|    - | 4170 | `	}` |
|    9 | 4171 | `	pIo = (phl_zip_io *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_zip_io));` |
|    9 | 4172 | `	if( pIo == 0 ){` |
|  ! 0 | 4173 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4174 | `	}` |
|    9 | 4175 | `	SyZero(pIo,sizeof(*pIo));` |
|    9 | 4176 | `	pIo->pZip = pZip;` |
|    9 | 4177 | `	pZip->nRef++;` |
|    9 | 4178 | `	SyBlobInit(&pIo->sData,&pCtx->pVm->sAllocator);` |
|    9 | 4179 | `	SyBlobAppend(&pIo->sData,SyBlobData(&pEnt->sData),SyBlobLength(&pEnt->sData));` |
|    9 | 4180 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE);` |
|    9 | 4181 | `	if( pDev == 0 ){` |
|  ! 0 | 4182 | `		SyBlobRelease(&pIo->sData);` |
|  ! 0 | 4183 | `		ZipRelease(pZip);` |
|  ! 0 | 4184 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pIo);` |
|  ! 0 | 4185 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4186 | `	}` |
|    9 | 4187 | `	InitIOPrivate(pCtx->pVm,&sZIP_Stream,pDev);` |
|    9 | 4188 | `	pDev->pHandle = pIo;` |
|   13 | 4189 | `	SetIOPrivateOpenedAs(pDev,(const char *)SyBlobData(&pEnt->sName),` |
|    8 | 4190 | `		(int)SyBlobLength(&pEnt->sName),"rb",2);` |
|    9 | 4191 | `	ph7_result_resource(pCtx,pDev);` |
|    9 | 4192 | `	return PH7_OK;` |
|    5 | 4193 | `}` |
|    2 | 4194 | `static int vm_builtin_ZipArchive_getStreamIndex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4195 | `{` |
|    3 | 4196 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 4197 | `	phl_zip_ent *pEnt;` |
|    - | 4198 | `	int rc;` |
|    1 | 4199 | `	SXUNUSED(nArg);` |
|    3 | 4200 | `	if( pZip == 0 ){` |
|  ! 0 | 4201 | `		return PH7_OK;` |
|    - | 4202 | `	}` |
|    3 | 4203 | `	pEnt = ZipArgIndex(pCtx,pZip,apArg[0],&rc);` |
|    3 | 4204 | `	return pEnt ? ZipStreamOf(pCtx,pZip,pEnt) : rc;` |
|    2 | 4205 | `}` |
|   10 | 4206 | `static int vm_builtin_ZipArchive_getStreamName(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4207 | `{` |
|   11 | 4208 | `	phl_zip *pZip = ZipThis(pCtx,0);` |
|    - | 4209 | `	phl_zip_ent *pEnt;` |
|   11 | 4210 | `	int iFlags = 0,rc;` |
|   11 | 4211 | `	if( pZip == 0 ){` |
|  ! 0 | 4212 | `		return PH7_OK;` |
|    - | 4213 | `	}` |
|   11 | 4214 | `	if( ZipNulName(pCtx,apArg[0],1,"name",&rc) ){` |
|    3 | 4215 | `		return rc;` |
|    - | 4216 | `	}` |
|    9 | 4217 | `	if( ZipEmptyName(pCtx,apArg[0],1,"name",&rc) ){` |
|  ! 0 | 4218 | `		return rc;` |
|    - | 4219 | `	}` |
|    9 | 4220 | `	if( nArg > 1 ){` |
|  ! 0 | 4221 | `		iFlags = (int)ph7_value_to_int(apArg[1]);` |
|  ! 0 | 4222 | `	}` |
|    9 | 4223 | `	pEnt = ZipArgName(pCtx,pZip,apArg[0],iFlags,&rc);` |
|    9 | 4224 | `	return pEnt ? ZipStreamOf(pCtx,pZip,pEnt) : rc;` |
|    6 | 4225 | `}` |
|    - | 4226 | `/* ------------------------------------------------------------------ */` |
|    - | 4227 | `/* php's deprecated procedural half                                    */` |
|    - | 4228 | `/* ------------------------------------------------------------------ */` |
|    - | 4229 | `/*` |
|    - | 4230 | `` * `zip_open()` and the seven verbs around it. php deprecated all ten in 8.0 and`` |
|    - | 4231 | ` * has kept them ever since, so they are here with the notice the call itself` |
|    - | 4232 | ` * raises (aDeprecatedFunc[] in vm_arg_check.c) rather than a body of their own.` |
|    - | 4233 | ` *` |
|    - | 4234 | `` * They are a CURSOR over the entry table -- `zip_read()` hands out the next`` |
|    - | 4235 | ` * entry and there is no way back -- which is why php's replacement advice is` |
|    - | 4236 | `` * `statIndex()` and not any one of them. The two handles are resources rather`` |
|    - | 4237 | `` * than objects, and php names them `Zip Directory` and `Zip Entry`.`` |
|    - | 4238 | ` */` |
|    - | 4239 | `/*` |
|    - | 4240 | `` * Both handles carry an `io_private`-compatible header, the way the stream`` |
|    - | 4241 | ` * context and the filter handles do: get_resource_type() probes the magic in` |
|    - | 4242 | ` * THAT field on any resource it is handed, so a record with a smaller header` |
|    - | 4243 | ` * would be read past its end.` |
|    - | 4244 | ` */` |
|    - | 4245 | `typedef struct phl_zip_dir phl_zip_dir;` |
|    - | 4246 | `typedef struct phl_zip_res phl_zip_res;` |
|    - | 4247 | `struct phl_zip_dir {` |
|    - | 4248 | `	io_private base;   /* base.iMagic == ZIP_DIR_MAGIC */` |
|    - | 4249 | `	phl_zip *pZip;` |
|    - | 4250 | `	sxu32 nCur;` |
|    - | 4251 | `};` |
|    - | 4252 | `struct phl_zip_res {` |
|    - | 4253 | `	io_private base;   /* base.iMagic == ZIP_ENT_MAGIC */` |
|    - | 4254 | `	phl_zip *pZip;` |
|    - | 4255 | `	phl_zip_ent *pEnt;` |
|    - | 4256 | `	sxu32 nCur;        /* zip_entry_read()'s cursor */` |
|    - | 4257 | `	int bOpen;` |
|    - | 4258 | `};` |
|    - | 4259 | `#define ZIP_DIR_MAGIC 0x5A44495Au   /* 'ZDIZ' */` |
|    - | 4260 | `#define ZIP_ENT_MAGIC 0x5A454E54u   /* 'ZENT' */` |
|    - | 4261 | `/*` |
|    - | 4262 | `` * php's own argument screen for the ten. Their declarations say `$zip` with no`` |
|    - | 4263 | ` * type at all, so the refusal is not ZPP's: each body checks the value is a` |
|    - | 4264 | ` * RESOURCE and words the TypeError itself. A resource of the wrong kind falls` |
|    - | 4265 | ` * through to the body, which answers false rather than throwing.` |
|    - | 4266 | ` */` |
|   72 | 4267 | `static int ZipHandleArg(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,int *pRc)` |
|    1 | 4268 | `{` |
|    - | 4269 | `	char zGiven[64];` |
|   73 | 4270 | `	*pRc = PH7_OK;` |
|   73 | 4271 | `	if( pArg != 0 && ph7_value_is_resource(pArg) ){` |
|   55 | 4272 | `		return 1;` |
|    - | 4273 | `	}` |
|   28 | 4274 | `	*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|    - | 4275 | `		"%z(): Argument #%d ($%s) must be of type resource, %s given",` |
|   18 | 4276 | `		&pCtx->pFunc->sName,iPos,zName,` |
|    9 | 4277 | `		VmValueGivenName(pArg,zGiven,sizeof(zGiven)));` |
|   19 | 4278 | `	return 0;` |
|   37 | 4279 | `}` |
|   16 | 4280 | `static phl_zip_dir * ZipDirArg(ph7_value *pArg)` |
|    1 | 4281 | `{` |
|    - | 4282 | `	phl_zip_dir *pDir;` |
|   17 | 4283 | `	if( pArg == 0 \|\| !ph7_value_is_resource(pArg) ){` |
|    5 | 4284 | `		return 0;` |
|    - | 4285 | `	}` |
|   13 | 4286 | `	pDir = (phl_zip_dir *)ph7_value_to_resource(pArg);` |
|   13 | 4287 | `	return (pDir && pDir->base.iMagic == ZIP_DIR_MAGIC) ? pDir : 0;` |
|    9 | 4288 | `}` |
|   56 | 4289 | `static phl_zip_res * ZipEntArg(ph7_value *pArg)` |
|    1 | 4290 | `{` |
|    - | 4291 | `	phl_zip_res *pRes;` |
|   57 | 4292 | `	if( pArg == 0 \|\| !ph7_value_is_resource(pArg) ){` |
|   15 | 4293 | `		return 0;` |
|    - | 4294 | `	}` |
|   43 | 4295 | `	pRes = (phl_zip_res *)ph7_value_to_resource(pArg);` |
|   43 | 4296 | `	return (pRes && pRes->base.iMagic == ZIP_ENT_MAGIC) ? pRes : 0;` |
|   29 | 4297 | `}` |
|   14 | 4298 | `PH7_PRIVATE const char * PH7_ZipResourceType(void *pResource)` |
|    3 | 4299 | `{` |
|   17 | 4300 | `	io_private *pDev = (io_private *)pResource;` |
|   17 | 4301 | `	if( pDev == 0 ){` |
|  ! 0 | 4302 | `		return 0;` |
|    - | 4303 | `	}` |
|   17 | 4304 | `	if( pDev->iMagic == ZIP_DIR_MAGIC ){` |
|    5 | 4305 | `		return "Zip Directory";` |
|    - | 4306 | `	}` |
|   13 | 4307 | `	if( pDev->iMagic == ZIP_ENT_MAGIC ){` |
|    7 | 4308 | `		return "Zip Entry";` |
|    - | 4309 | `	}` |
|    6 | 4310 | `	return 0;` |
|   10 | 4311 | `}` |
|   10 | 4312 | `static int PH7_builtin_zip_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4313 | `{` |
|    - | 4314 | `	phl_zip *pZip;` |
|    - | 4315 | `	phl_zip_dir *pDir;` |
|   11 | 4316 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|    - | 4317 | `	const char *zName;` |
|    - | 4318 | `	SyBlob sPath;` |
|   11 | 4319 | `	int nName = 0,rc;` |
|    5 | 4320 | `	SXUNUSED(nArg);` |
|   11 | 4321 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|   11 | 4322 | `	if( nName < 1 ){` |
|    4 | 4323 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    2 | 4324 | `			"%z(): Argument #1 ($filename) must not be empty",&pCtx->pFunc->sName);` |
|    - | 4325 | `	}` |
|    9 | 4326 | `	PH7_VfsExpandPath(pCtx,zName,nName,&sPath);` |
|    9 | 4327 | `	pZip = ZipNew(pCtx->pVm);` |
|    9 | 4328 | `	if( pZip == 0 ){` |
|  ! 0 | 4329 | `		SyBlobRelease(&sPath);` |
|  ! 0 | 4330 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4331 | `	}` |
|    9 | 4332 | `	SyBlobAppend(&pZip->sPath,SyBlobData(&sPath),SyBlobLength(&sPath));` |
|    9 | 4333 | `	SyBlobRelease(&sPath);` |
|    9 | 4334 | `	SyBlobNullAppend(&pZip->sPath);` |
|    - | 4335 | `	{` |
|    9 | 4336 | `		const char *zPath = (const char *)SyBlobData(&pZip->sPath);` |
|    - | 4337 | `		const ph7_io_stream *pStream;` |
|    - | 4338 | `		void *pFile;` |
|    9 | 4339 | `		const char *zTail = zPath;` |
|    9 | 4340 | `		if( pVfs && pVfs->xIsdir && pVfs->xIsdir(zPath) == PH7_OK ){` |
|    3 | 4341 | `			rc = ZIP_ER_OPNOTSUPP;` |
|    5 | 4342 | `			goto failed;` |
|    - | 4343 | `		}` |
|    7 | 4344 | `		if( pVfs == 0 \|\| pVfs->xFileExists == 0 \|\| pVfs->xFileExists(zPath) != PH7_OK ){` |
|    3 | 4345 | `			rc = ZIP_ER_NOENT;` |
|    3 | 4346 | `			goto failed;` |
|    - | 4347 | `		}` |
|    5 | 4348 | `		pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zTail,(int)SyBlobLength(&pZip->sPath));` |
|    5 | 4349 | `		pFile = pStream ? PH7_StreamOpenHandle(pCtx->pVm,pStream,zTail,PH7_IO_OPEN_RDONLY,` |
|    2 | 4350 | `			FALSE,0,FALSE,0,0) : 0;` |
|    5 | 4351 | `		if( pFile == 0 ){` |
|  ! 0 | 4352 | `			rc = ZIP_ER_READ;` |
|  ! 0 | 4353 | `			goto failed;` |
|    - | 4354 | `		}` |
|    5 | 4355 | `		PH7_StreamReadWholeFile(pFile,pStream,&pZip->sFile);` |
|    5 | 4356 | `		PH7_StreamCloseHandle(pStream,pFile);` |
|    5 | 4357 | `		rc = ZipParse(pZip);` |
|    5 | 4358 | `		if( rc == ZIP_ER_EMPTY_FILE ){` |
|  ! 0 | 4359 | `			ph7_context_throw_error_format(pCtx,8192 /* E_DEPRECATED */,` |
|    - | 4360 | `				"Using empty file as ZipArchive is deprecated");` |
|    5 | 4361 | `		}else if( rc != ZIP_ER_OK ){` |
|    3 | 4362 | `			goto failed;` |
|    - | 4363 | `		}` |
|    - | 4364 | `	}` |
|    3 | 4365 | `	pDir = (phl_zip_dir *)ph7_context_alloc_chunk(pCtx,sizeof(phl_zip_dir),TRUE,FALSE);` |
|    3 | 4366 | `	if( pDir == 0 ){` |
|  ! 0 | 4367 | `		ZipRelease(pZip);` |
|  ! 0 | 4368 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4369 | `	}` |
|    3 | 4370 | `	pDir->base.iMagic = ZIP_DIR_MAGIC;` |
|    3 | 4371 | `	pDir->pZip = pZip;` |
|    3 | 4372 | `	pDir->nCur = 0;` |
|    3 | 4373 | `	ph7_result_resource(pCtx,pDir);` |
|    3 | 4374 | `	return PH7_OK;` |
|    3 | 4375 | `failed:` |
|    7 | 4376 | `	ZipRelease(pZip);` |
|    7 | 4377 | `	ph7_result_int(pCtx,rc);` |
|    7 | 4378 | `	return PH7_OK;` |
|    6 | 4379 | `}` |
|    4 | 4380 | `static int PH7_builtin_zip_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4381 | `{` |
|    5 | 4382 | `	phl_zip_dir *pDir = ZipDirArg(apArg[0]);` |
|    - | 4383 | `	int rcArg;` |
|    2 | 4384 | `	SXUNUSED(nArg); SXUNUSED(pCtx);` |
|    5 | 4385 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip",&rcArg) ){` |
|    3 | 4386 | `		return rcArg;` |
|    - | 4387 | `	}` |
|    3 | 4388 | `	if( pDir == 0 ){` |
|  ! 0 | 4389 | `		return PH7_OK;` |
|    - | 4390 | `	}` |
|    3 | 4391 | `	if( pDir->pZip ){` |
|    3 | 4392 | `		ZipRelease(pDir->pZip);` |
|    3 | 4393 | `		pDir->pZip = 0;` |
|    1 | 4394 | `	}` |
|    3 | 4395 | `	return PH7_OK;` |
|    3 | 4396 | `}` |
|   12 | 4397 | `static int PH7_builtin_zip_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4398 | `{` |
|   13 | 4399 | `	phl_zip_dir *pDir = ZipDirArg(apArg[0]);` |
|    - | 4400 | `	phl_zip_res *pRes;` |
|   13 | 4401 | `	phl_zip_ent *pEnt = 0;` |
|    - | 4402 | `	int rcArg;` |
|    6 | 4403 | `	SXUNUSED(nArg);` |
|   13 | 4404 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip",&rcArg) ){` |
|    3 | 4405 | `		return rcArg;` |
|    - | 4406 | `	}` |
|   11 | 4407 | `	if( pDir == 0 \|\| pDir->pZip == 0 ){` |
|  ! 0 | 4408 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4409 | `		return PH7_OK;` |
|    - | 4410 | `	}` |
|   11 | 4411 | `	while( pDir->nCur < pDir->pZip->nEnt ){` |
|    7 | 4412 | `		phl_zip_ent *p = pDir->pZip->apEnt[pDir->nCur++];` |
|    7 | 4413 | `		if( !p->bDeleted ){` |
|    7 | 4414 | `			pEnt = p;` |
|    7 | 4415 | `			break;` |
|    - | 4416 | `		}` |
|  ! 0 | 4417 | `	}` |
|   11 | 4418 | `	if( pEnt == 0 ){` |
|    5 | 4419 | `		ph7_result_bool(pCtx,0);` |
|    5 | 4420 | `		return PH7_OK;` |
|    - | 4421 | `	}` |
|    7 | 4422 | `	pRes = (phl_zip_res *)ph7_context_alloc_chunk(pCtx,sizeof(phl_zip_res),TRUE,FALSE);` |
|    7 | 4423 | `	if( pRes == 0 ){` |
|  ! 0 | 4424 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 4425 | `	}` |
|    7 | 4426 | `	pRes->base.iMagic = ZIP_ENT_MAGIC;` |
|    - | 4427 | ``	/* The entry holds the archive: `zip_close()` may come before the last read`` |
|    - | 4428 | `	 * of an entry it handed out, and php reading freed memory there is not` |
|    - | 4429 | `	 * something to reproduce. The reference is never dropped -- a resource has` |
|    - | 4430 | `	 * no destructor to drop it in -- so the archive lives until the VM sweep,` |
|    - | 4431 | `	 * exactly as php's does until the request ends. */` |
|    7 | 4432 | `	pRes->pZip = pDir->pZip;` |
|    7 | 4433 | `	pDir->pZip->nRef++;` |
|    7 | 4434 | `	pRes->pEnt = pEnt;` |
|    7 | 4435 | `	pRes->nCur = 0;` |
|    7 | 4436 | `	pRes->bOpen = 0;` |
|    7 | 4437 | `	ph7_result_resource(pCtx,pRes);` |
|    7 | 4438 | `	return PH7_OK;` |
|    7 | 4439 | `}` |
|    8 | 4440 | `static int PH7_builtin_zip_entry_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4441 | `{` |
|    9 | 4442 | `	phl_zip_res *pRes = ZipEntArg(nArg > 1 ? apArg[1] : 0);` |
|    - | 4443 | `	int rcArg;` |
|    4 | 4444 | `	SXUNUSED(nArg);` |
|    9 | 4445 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_dp",&rcArg) ){` |
|    3 | 4446 | `		return rcArg;` |
|    - | 4447 | `	}` |
|    7 | 4448 | `	if( pRes == 0 ){` |
|  ! 0 | 4449 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4450 | `		return PH7_OK;` |
|    - | 4451 | `	}` |
|    7 | 4452 | `	pRes->bOpen = 1;` |
|    7 | 4453 | `	pRes->nCur = 0;` |
|    7 | 4454 | `	ph7_result_bool(pCtx,1);` |
|    7 | 4455 | `	return PH7_OK;` |
|    5 | 4456 | `}` |
|    8 | 4457 | `static int PH7_builtin_zip_entry_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4458 | `{` |
|    9 | 4459 | `	phl_zip_res *pRes = ZipEntArg(apArg[0]);` |
|    - | 4460 | `	int rcArg;` |
|    4 | 4461 | `	SXUNUSED(nArg);` |
|    9 | 4462 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){` |
|    3 | 4463 | `		return rcArg;` |
|    - | 4464 | `	}` |
|    7 | 4465 | `	if( pRes == 0 ){` |
|  ! 0 | 4466 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4467 | `		return PH7_OK;` |
|    - | 4468 | `	}` |
|    7 | 4469 | `	pRes->bOpen = 0;` |
|    7 | 4470 | `	ph7_result_bool(pCtx,1);` |
|    7 | 4471 | `	return PH7_OK;` |
|    5 | 4472 | `}` |
|   14 | 4473 | `static int PH7_builtin_zip_entry_read(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4474 | `{` |
|   15 | 4475 | `	phl_zip_res *pRes = ZipEntArg(apArg[0]);` |
|   15 | 4476 | `	sxi64 iWant = 1024;` |
|    - | 4477 | `	sxu32 nLeft;` |
|    - | 4478 | `	int rcArg;` |
|   15 | 4479 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){` |
|    3 | 4480 | `		return rcArg;` |
|    - | 4481 | `	}` |
|   13 | 4482 | `	if( pRes == 0 \|\| ZipEntLoad(pRes->pZip,pRes->pEnt) != ZIP_ER_OK ){` |
|  ! 0 | 4483 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4484 | `		return PH7_OK;` |
|    - | 4485 | `	}` |
|   13 | 4486 | `	if( nArg > 1 ){` |
|   13 | 4487 | `		iWant = ph7_value_to_int64(apArg[1]);` |
|    6 | 4488 | `	}` |
|   13 | 4489 | `	if( iWant < 0 ){` |
|  ! 0 | 4490 | `		iWant = 0;` |
|  ! 0 | 4491 | `	}` |
|   13 | 4492 | `	nLeft = SyBlobLength(&pRes->pEnt->sData) - pRes->nCur;` |
|   13 | 4493 | `	if( (sxu64)iWant > (sxu64)nLeft ){` |
|    9 | 4494 | `		iWant = (sxi64)nLeft;` |
|    4 | 4495 | `	}` |
|   19 | 4496 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pRes->pEnt->sData) + pRes->nCur,` |
|    6 | 4497 | `		(int)iWant);` |
|   13 | 4498 | `	pRes->nCur += (sxu32)iWant;` |
|   13 | 4499 | `	return PH7_OK;` |
|    8 | 4500 | `}` |
|    8 | 4501 | `static int PH7_builtin_zip_entry_name(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4502 | `{` |
|    9 | 4503 | `	phl_zip_res *pRes = ZipEntArg(apArg[0]);` |
|    - | 4504 | `	int rcArg;` |
|    4 | 4505 | `	SXUNUSED(nArg);` |
|    9 | 4506 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){` |
|    3 | 4507 | `		return rcArg;` |
|    - | 4508 | `	}` |
|    7 | 4509 | `	if( pRes == 0 ){` |
|  ! 0 | 4510 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4511 | `		return PH7_OK;` |
|    - | 4512 | `	}` |
|   10 | 4513 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pRes->pEnt->sName),` |
|    6 | 4514 | `		(int)SyBlobLength(&pRes->pEnt->sName));` |
|    7 | 4515 | `	return PH7_OK;` |
|    5 | 4516 | `}` |
|    8 | 4517 | `static int PH7_builtin_zip_entry_filesize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4518 | `{` |
|    9 | 4519 | `	phl_zip_res *pRes = ZipEntArg(apArg[0]);` |
|    - | 4520 | `	int rcArg;` |
|    4 | 4521 | `	SXUNUSED(nArg);` |
|    9 | 4522 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){` |
|    3 | 4523 | `		return rcArg;` |
|    - | 4524 | `	}` |
|    7 | 4525 | `	if( pRes == 0 ){` |
|  ! 0 | 4526 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4527 | `		return PH7_OK;` |
|    - | 4528 | `	}` |
|    7 | 4529 | `	ph7_result_int64(pCtx,(ph7_int64)pRes->pEnt->nSize);` |
|    7 | 4530 | `	return PH7_OK;` |
|    5 | 4531 | `}` |
|    2 | 4532 | `static int PH7_builtin_zip_entry_compressedsize(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4533 | `{` |
|    3 | 4534 | `	phl_zip_res *pRes = ZipEntArg(apArg[0]);` |
|    - | 4535 | `	int rcArg;` |
|    1 | 4536 | `	SXUNUSED(nArg);` |
|    3 | 4537 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){` |
|    3 | 4538 | `		return rcArg;` |
|    - | 4539 | `	}` |
|  ! 0 | 4540 | `	if( pRes == 0 ){` |
|  ! 0 | 4541 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4542 | `		return PH7_OK;` |
|    - | 4543 | `	}` |
|  ! 0 | 4544 | `	ph7_result_int64(pCtx,(ph7_int64)pRes->pEnt->nCompSize);` |
|  ! 0 | 4545 | `	return PH7_OK;` |
|    2 | 4546 | `}` |
|    - | 4547 | `/* php names the method rather than numbering it here, and it names only the` |
|    - | 4548 | ` * two the format actually uses today; anything else is "unknown". */` |
|    8 | 4549 | `static int PH7_builtin_zip_entry_compressionmethod(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 4550 | `{` |
|    9 | 4551 | `	phl_zip_res *pRes = ZipEntArg(apArg[0]);` |
|    - | 4552 | `	const char *zName;` |
|    - | 4553 | `	int rcArg;` |
|    4 | 4554 | `	SXUNUSED(nArg);` |
|    9 | 4555 | `	if( !ZipHandleArg(pCtx,apArg[0],1,"zip_entry",&rcArg) ){` |
|    3 | 4556 | `		return rcArg;` |
|    - | 4557 | `	}` |
|    7 | 4558 | `	if( pRes == 0 ){` |
|  ! 0 | 4559 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4560 | `		return PH7_OK;` |
|    - | 4561 | `	}` |
|    7 | 4562 | `	switch( pRes->pEnt->iMethod ){` |
|    3 | 4563 | `	case 0:  zName = "stored";   break;` |
|  ! 0 | 4564 | `	case 1:  zName = "shrunk";   break;` |
|  ! 0 | 4565 | `	case 2:` |
|    - | 4566 | `	case 3:` |
|    - | 4567 | `	case 4:` |
|  ! 0 | 4568 | `	case 5:  zName = "reduced";  break;` |
|  ! 0 | 4569 | `	case 6:  zName = "imploded"; break;` |
|    5 | 4570 | `	case 8:  zName = "deflated"; break;` |
|  ! 0 | 4571 | `	case 9:  zName = "deflatedX";break;` |
|  ! 0 | 4572 | `	case 10: zName = "implodedX";break;` |
|  ! 0 | 4573 | `	default:` |
|    - | 4574 | `		/* php's table stops there -- a bzip2, LZMA or XZ member has no name in` |
|    - | 4575 | `		 * it and the answer is FALSE, not "unknown". */` |
|  ! 0 | 4576 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 4577 | `		return PH7_OK;` |
|    - | 4578 | `	}` |
|    7 | 4579 | `	ph7_result_string(pCtx,zName,-1);` |
|    7 | 4580 | `	return PH7_OK;` |
|    5 | 4581 | `}` |
|    - | 4582 | `/* ------------------------------------------------------------------ */` |
|    - | 4583 | `/* Mounting the extension                                              */` |
|    - | 4584 | `/* ------------------------------------------------------------------ */` |
|    - | 4585 | `#define ZIP_ICONST(NAME,VALUE) \` |
|    - | 4586 | `	{ NAME, PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, (VALUE), 0, 0.0 }` |
| 7925 | 4587 | `PH7_PRIVATE sxi32 PH7_VmInstallZip(ph7_vm *pVm)` |
|    5 | 4588 | `{` |
|    - | 4589 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|    - | 4590 | `		{ "open", PH7_MOD_PUBLIC, "string $filename, int $flags = 0", "@int\|bool",` |
|    - | 4591 | `		  vm_builtin_ZipArchive_open },` |
|    - | 4592 | `		{ "setPassword", PH7_MOD_PUBLIC, "string $password", "@bool",` |
|    - | 4593 | `		  vm_builtin_ZipArchive_setPassword },` |
|    - | 4594 | `		{ "close", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ZipArchive_close },` |
|    - | 4595 | `		{ "count", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ZipArchive_count },` |
|    - | 4596 | `		{ "getStatusString", PH7_MOD_PUBLIC, "", "@string",` |
|    - | 4597 | `		  vm_builtin_ZipArchive_getStatusString },` |
|    - | 4598 | `		{ "clearError", PH7_MOD_PUBLIC, "", "void", vm_builtin_ZipArchive_clearError },` |
|    - | 4599 | `		{ "addEmptyDir", PH7_MOD_PUBLIC, "string $dirname, int $flags = 0", "@bool",` |
|    - | 4600 | `		  vm_builtin_ZipArchive_addEmptyDir },` |
|    - | 4601 | `		{ "addFromString", PH7_MOD_PUBLIC,` |
|    - | 4602 | `		  "string $name, string $content, int $flags = ZipArchive::FL_OVERWRITE", "@bool",` |
|    - | 4603 | `		  vm_builtin_ZipArchive_addFromString },` |
|    - | 4604 | `		{ "addFile", PH7_MOD_PUBLIC,` |
|    - | 4605 | `		  "string $filepath, string $entryname = \"\", int $start = 0, "` |
|    - | 4606 | `		  "int $length = ZipArchive::LENGTH_TO_END, int $flags = ZipArchive::FL_OVERWRITE",` |
|    - | 4607 | `		  "@bool", vm_builtin_ZipArchive_addFile },` |
|    - | 4608 | `		{ "replaceFile", PH7_MOD_PUBLIC,` |
|    - | 4609 | `		  "string $filepath, int $index, int $start = 0, "` |
|    - | 4610 | `		  "int $length = ZipArchive::LENGTH_TO_END, int $flags = 0",` |
|    - | 4611 | `		  "@bool", vm_builtin_ZipArchive_replaceFile },` |
|    - | 4612 | `		{ "addGlob", PH7_MOD_PUBLIC, "string $pattern, int $flags = 0, array $options = []",` |
|    - | 4613 | `		  "@array\|false", vm_builtin_ZipArchive_addGlob },` |
|    - | 4614 | `		{ "addPattern", PH7_MOD_PUBLIC,` |
|    - | 4615 | `		  "string $pattern, string $path = \".\", array $options = []",` |
|    - | 4616 | `		  "@array\|false", vm_builtin_ZipArchive_addPattern },` |
|    - | 4617 | `		{ "renameIndex", PH7_MOD_PUBLIC, "int $index, string $new_name", "@bool",` |
|    - | 4618 | `		  vm_builtin_ZipArchive_renameIndex },` |
|    - | 4619 | `		{ "renameName", PH7_MOD_PUBLIC, "string $name, string $new_name", "@bool",` |
|    - | 4620 | `		  vm_builtin_ZipArchive_renameName },` |
|    - | 4621 | `		{ "setArchiveComment", PH7_MOD_PUBLIC, "string $comment", "@bool",` |
|    - | 4622 | `		  vm_builtin_ZipArchive_setArchiveComment },` |
|    - | 4623 | `		{ "getArchiveComment", PH7_MOD_PUBLIC, "int $flags = 0", "@string\|false",` |
|    - | 4624 | `		  vm_builtin_ZipArchive_getArchiveComment },` |
|    - | 4625 | `		{ "setArchiveFlag", PH7_MOD_PUBLIC, "int $flag, int $value", "bool",` |
|    - | 4626 | `		  vm_builtin_ZipArchive_setArchiveFlag },` |
|    - | 4627 | `		{ "getArchiveFlag", PH7_MOD_PUBLIC, "int $flag, int $flags = 0", "int",` |
|    - | 4628 | `		  vm_builtin_ZipArchive_getArchiveFlag },` |
|    - | 4629 | `		{ "setCommentIndex", PH7_MOD_PUBLIC, "int $index, string $comment", "@bool",` |
|    - | 4630 | `		  vm_builtin_ZipArchive_setCommentIndex },` |
|    - | 4631 | `		{ "setCommentName", PH7_MOD_PUBLIC, "string $name, string $comment", "@bool",` |
|    - | 4632 | `		  vm_builtin_ZipArchive_setCommentName },` |
|    - | 4633 | `		{ "setMtimeIndex", PH7_MOD_PUBLIC, "int $index, int $timestamp, int $flags = 0",` |
|    - | 4634 | `		  "@bool", vm_builtin_ZipArchive_setMtimeIndex },` |
|    - | 4635 | `		{ "setMtimeName", PH7_MOD_PUBLIC, "string $name, int $timestamp, int $flags = 0",` |
|    - | 4636 | `		  "@bool", vm_builtin_ZipArchive_setMtimeName },` |
|    - | 4637 | `		{ "getCommentIndex", PH7_MOD_PUBLIC, "int $index, int $flags = 0", "@string\|false",` |
|    - | 4638 | `		  vm_builtin_ZipArchive_getCommentIndex },` |
|    - | 4639 | `		{ "getCommentName", PH7_MOD_PUBLIC, "string $name, int $flags = 0", "@string\|false",` |
|    - | 4640 | `		  vm_builtin_ZipArchive_getCommentName },` |
|    - | 4641 | `		{ "deleteIndex", PH7_MOD_PUBLIC, "int $index", "@bool",` |
|    - | 4642 | `		  vm_builtin_ZipArchive_deleteIndex },` |
|    - | 4643 | `		{ "deleteName", PH7_MOD_PUBLIC, "string $name", "@bool",` |
|    - | 4644 | `		  vm_builtin_ZipArchive_deleteName },` |
|    - | 4645 | `		{ "statName", PH7_MOD_PUBLIC, "string $name, int $flags = 0", "@array\|false",` |
|    - | 4646 | `		  vm_builtin_ZipArchive_statName },` |
|    - | 4647 | `		{ "statIndex", PH7_MOD_PUBLIC, "int $index, int $flags = 0", "@array\|false",` |
|    - | 4648 | `		  vm_builtin_ZipArchive_statIndex },` |
|    - | 4649 | `		{ "locateName", PH7_MOD_PUBLIC, "string $name, int $flags = 0", "@int\|false",` |
|    - | 4650 | `		  vm_builtin_ZipArchive_locateName },` |
|    - | 4651 | `		{ "getNameIndex", PH7_MOD_PUBLIC, "int $index, int $flags = 0", "@string\|false",` |
|    - | 4652 | `		  vm_builtin_ZipArchive_getNameIndex },` |
|    - | 4653 | `		{ "unchangeArchive", PH7_MOD_PUBLIC, "", "@bool",` |
|    - | 4654 | `		  vm_builtin_ZipArchive_unchangeArchive },` |
|    - | 4655 | `		{ "unchangeAll", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ZipArchive_unchangeAll },` |
|    - | 4656 | `		{ "unchangeIndex", PH7_MOD_PUBLIC, "int $index", "@bool",` |
|    - | 4657 | `		  vm_builtin_ZipArchive_unchangeIndex },` |
|    - | 4658 | `		{ "unchangeName", PH7_MOD_PUBLIC, "string $name", "@bool",` |
|    - | 4659 | `		  vm_builtin_ZipArchive_unchangeName },` |
|    - | 4660 | `		{ "extractTo", PH7_MOD_PUBLIC, "string $pathto, array\|string\|null $files = null",` |
|    - | 4661 | `		  "@bool", vm_builtin_ZipArchive_extractTo },` |
|    - | 4662 | `		{ "getFromName", PH7_MOD_PUBLIC, "string $name, int $len = 0, int $flags = 0",` |
|    - | 4663 | `		  "@string\|false", vm_builtin_ZipArchive_getFromName },` |
|    - | 4664 | `		{ "getFromIndex", PH7_MOD_PUBLIC, "int $index, int $len = 0, int $flags = 0",` |
|    - | 4665 | `		  "@string\|false", vm_builtin_ZipArchive_getFromIndex },` |
|    - | 4666 | `		{ "getStreamIndex", PH7_MOD_PUBLIC, "int $index, int $flags = 0", 0,` |
|    - | 4667 | `		  vm_builtin_ZipArchive_getStreamIndex },` |
|    - | 4668 | `		{ "getStreamName", PH7_MOD_PUBLIC, "string $name, int $flags = 0", 0,` |
|    - | 4669 | `		  vm_builtin_ZipArchive_getStreamName },` |
|    - | 4670 | `		{ "getStream", PH7_MOD_PUBLIC, "string $name", 0,` |
|    - | 4671 | `		  vm_builtin_ZipArchive_getStreamName },` |
|    - | 4672 | `		{ "setExternalAttributesName", PH7_MOD_PUBLIC,` |
|    - | 4673 | `		  "string $name, int $opsys, int $attr, int $flags = 0", "@bool",` |
|    - | 4674 | `		  vm_builtin_ZipArchive_setExternalAttributesName },` |
|    - | 4675 | `		{ "setExternalAttributesIndex", PH7_MOD_PUBLIC,` |
|    - | 4676 | `		  "int $index, int $opsys, int $attr, int $flags = 0", "@bool",` |
|    - | 4677 | `		  vm_builtin_ZipArchive_setExternalAttributesIndex },` |
|    - | 4678 | `		{ "getExternalAttributesName", PH7_MOD_PUBLIC,` |
|    - | 4679 | `		  "string $name, &$opsys, &$attr, int $flags = 0", "@bool",` |
|    - | 4680 | `		  vm_builtin_ZipArchive_getExternalAttributesName },` |
|    - | 4681 | `		{ "getExternalAttributesIndex", PH7_MOD_PUBLIC,` |
|    - | 4682 | `		  "int $index, &$opsys, &$attr, int $flags = 0", "@bool",` |
|    - | 4683 | `		  vm_builtin_ZipArchive_getExternalAttributesIndex },` |
|    - | 4684 | `		{ "setCompressionName", PH7_MOD_PUBLIC,` |
|    - | 4685 | `		  "string $name, int $method, int $compflags = 0", "@bool",` |
|    - | 4686 | `		  vm_builtin_ZipArchive_setCompressionName },` |
|    - | 4687 | `		{ "setCompressionIndex", PH7_MOD_PUBLIC,` |
|    - | 4688 | `		  "int $index, int $method, int $compflags = 0", "@bool",` |
|    - | 4689 | `		  vm_builtin_ZipArchive_setCompressionIndex },` |
|    - | 4690 | `		{ "setEncryptionName", PH7_MOD_PUBLIC,` |
|    - | 4691 | `		  "string $name, int $method, ?string $password = null", "@bool",` |
|    - | 4692 | `		  vm_builtin_ZipArchive_setEncryptionName },` |
|    - | 4693 | `		{ "setEncryptionIndex", PH7_MOD_PUBLIC,` |
|    - | 4694 | `		  "int $index, int $method, ?string $password = null", "@bool",` |
|    - | 4695 | `		  vm_builtin_ZipArchive_setEncryptionIndex },` |
|    - | 4696 | `		{ "registerProgressCallback", PH7_MOD_PUBLIC, "float $rate, callable $callback",` |
|    - | 4697 | `		  "@bool", vm_builtin_ZipArchive_registerProgressCallback },` |
|    - | 4698 | `		{ "registerCancelCallback", PH7_MOD_PUBLIC, "callable $callback", "@bool",` |
|    - | 4699 | `		  vm_builtin_ZipArchive_registerCancelCallback },` |
|    - | 4700 | `		{ "isCompressionMethodSupported", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|    - | 4701 | `		  "int $method, bool $enc = true", "bool",` |
|    - | 4702 | `		  vm_builtin_ZipArchive_isCompressionMethodSupported },` |
|    - | 4703 | `		{ "isEncryptionMethodSupported", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|    - | 4704 | `		  "int $method, bool $enc = true", "bool",` |
|    - | 4705 | `		  vm_builtin_ZipArchive_isEncryptionMethodSupported }` |
|    - | 4706 | `	};` |
|    - | 4707 | `	/*` |
|    - | 4708 | `	 * The six php DECLARES and answers through a handler. They are real slots` |
|    - | 4709 | `	 * here, written by the C bodies after every verb that could move one -- the` |
|    - | 4710 | ``	 * same fact from the other side, which is what makes `var_dump()`, the`` |
|    - | 4711 | ``	 * `(array)` cast, `json_encode()`, `foreach`, `serialize()` and`` |
|    - | 4712 | ``	 * `get_object_vars()` all show the six live values php shows.`` |
|    - | 4713 | `	 *` |
|    - | 4714 | `	 * They carry NO default, which is php's own declaration: Reflection reports` |
|    - | 4715 | ``	 * `hasDefaultValue()` false for each, and neither readonly nor virtual --`` |
|    - | 4716 | `	 * the write refusal is the handler's (ZipSetHook), not a modifier's.` |
|    - | 4717 | `	 */` |
|    - | 4718 | `	static const PH7_NativePropDef aProp[] = {` |
|    - | 4719 | `		{ "lastId",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|    - | 4720 | `		{ "status",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|    - | 4721 | `		{ "statusSys", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|    - | 4722 | `		{ "numFiles",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|    - | 4723 | `		{ "filename",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 4724 | `		{ "comment",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|    - | 4725 | `		{ ZIP_RES, PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN,` |
|    - | 4726 | `		  { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }` |
|    - | 4727 | `	};` |
|    - | 4728 | `	/*` |
|    - | 4729 | `	 * php's constants, in php's own order: the five open flags, the fifteen` |
|    - | 4730 | `	 * per-call FL_ flags, the nineteen compression methods, the thirty-three` |
|    - | 4731 | `	 * error codes, the one archive flag, the twenty operating systems, the six` |
|    - | 4732 | `	 * encryption methods, the libzip version and LENGTH_TO_END.` |
|    - | 4733 | `	 *` |
|    - | 4734 | `	 * The compression and encryption tables are NAMES rather than capabilities:` |
|    - | 4735 | `	 * php declares every one of them whatever its libzip can do, and` |
|    - | 4736 | ``	 * `isCompressionMethodSupported()` is the separate question.`` |
|    - | 4737 | `	 */` |
|    - | 4738 | `	static const PH7_NativeConstDef aConst[] = {` |
|    - | 4739 | `		ZIP_ICONST("CREATE",            ZIP_OPEN_CREATE),` |
|    - | 4740 | `		ZIP_ICONST("EXCL",              ZIP_OPEN_EXCL),` |
|    - | 4741 | `		ZIP_ICONST("CHECKCONS",         ZIP_OPEN_CHECKCONS),` |
|    - | 4742 | `		ZIP_ICONST("OVERWRITE",         ZIP_OPEN_OVERWRITE),` |
|    - | 4743 | `		ZIP_ICONST("RDONLY",            ZIP_OPEN_RDONLY),` |
|    - | 4744 | `		ZIP_ICONST("FL_NOCASE",         ZIP_FL_NOCASE),` |
|    - | 4745 | `		ZIP_ICONST("FL_NODIR",          ZIP_FL_NODIR),` |
|    - | 4746 | `		ZIP_ICONST("FL_COMPRESSED",     ZIP_FL_COMPRESSED),` |
|    - | 4747 | `		ZIP_ICONST("FL_UNCHANGED",      ZIP_FL_UNCHANGED),` |
|    - | 4748 | `		ZIP_ICONST("FL_RECOMPRESS",     16),` |
|    - | 4749 | `		ZIP_ICONST("FL_ENCRYPTED",      32),` |
|    - | 4750 | `		ZIP_ICONST("FL_OVERWRITE",      ZIP_FL_OVERWRITE),` |
|    - | 4751 | `		ZIP_ICONST("FL_LOCAL",          256),` |
|    - | 4752 | `		ZIP_ICONST("FL_CENTRAL",        512),` |
|    - | 4753 | `		ZIP_ICONST("FL_ENC_GUESS",      0),` |
|    - | 4754 | `		ZIP_ICONST("FL_ENC_RAW",        ZIP_FL_ENC_RAW),` |
|    - | 4755 | `		ZIP_ICONST("FL_ENC_STRICT",     128),` |
|    - | 4756 | `		ZIP_ICONST("FL_ENC_UTF_8",      ZIP_FL_ENC_UTF_8),` |
|    - | 4757 | `		ZIP_ICONST("FL_ENC_CP437",      4096),` |
|    - | 4758 | `		ZIP_ICONST("FL_OPEN_FILE_NOW",  1073741824),` |
|    - | 4759 | `		ZIP_ICONST("CM_DEFAULT",        ZIP_CM_DEFAULT),` |
|    - | 4760 | `		ZIP_ICONST("CM_STORE",          ZIP_CM_STORE),` |
|    - | 4761 | `		ZIP_ICONST("CM_SHRINK",         1),` |
|    - | 4762 | `		ZIP_ICONST("CM_REDUCE_1",       2),` |
|    - | 4763 | `		ZIP_ICONST("CM_REDUCE_2",       3),` |
|    - | 4764 | `		ZIP_ICONST("CM_REDUCE_3",       4),` |
|    - | 4765 | `		ZIP_ICONST("CM_REDUCE_4",       5),` |
|    - | 4766 | `		ZIP_ICONST("CM_IMPLODE",        6),` |
|    - | 4767 | `		ZIP_ICONST("CM_DEFLATE",        ZIP_CM_DEFLATE),` |
|    - | 4768 | `		ZIP_ICONST("CM_DEFLATE64",      9),` |
|    - | 4769 | `		ZIP_ICONST("CM_PKWARE_IMPLODE", 10),` |
|    - | 4770 | `		ZIP_ICONST("CM_BZIP2",          ZIP_CM_BZIP2),` |
|    - | 4771 | `		ZIP_ICONST("CM_LZMA",           14),` |
|    - | 4772 | `		ZIP_ICONST("CM_LZMA2",          33),` |
|    - | 4773 | `		ZIP_ICONST("CM_XZ",             95),` |
|    - | 4774 | `		ZIP_ICONST("CM_TERSE",          18),` |
|    - | 4775 | `		ZIP_ICONST("CM_LZ77",           19),` |
|    - | 4776 | `		ZIP_ICONST("CM_WAVPACK",        97),` |
|    - | 4777 | `		ZIP_ICONST("CM_PPMD",           98),` |
|    - | 4778 | `		ZIP_ICONST("ER_OK",             ZIP_ER_OK),` |
|    - | 4779 | `		ZIP_ICONST("ER_MULTIDISK",      ZIP_ER_MULTIDISK),` |
|    - | 4780 | `		ZIP_ICONST("ER_RENAME",         ZIP_ER_RENAME),` |
|    - | 4781 | `		ZIP_ICONST("ER_CLOSE",          ZIP_ER_CLOSE),` |
|    - | 4782 | `		ZIP_ICONST("ER_SEEK",           ZIP_ER_SEEK),` |
|    - | 4783 | `		ZIP_ICONST("ER_READ",           ZIP_ER_READ),` |
|    - | 4784 | `		ZIP_ICONST("ER_WRITE",          ZIP_ER_WRITE),` |
|    - | 4785 | `		ZIP_ICONST("ER_CRC",            ZIP_ER_CRC),` |
|    - | 4786 | `		ZIP_ICONST("ER_ZIPCLOSED",      ZIP_ER_ZIPCLOSED),` |
|    - | 4787 | `		ZIP_ICONST("ER_NOENT",          ZIP_ER_NOENT),` |
|    - | 4788 | `		ZIP_ICONST("ER_EXISTS",         ZIP_ER_EXISTS),` |
|    - | 4789 | `		ZIP_ICONST("ER_OPEN",           ZIP_ER_OPEN),` |
|    - | 4790 | `		ZIP_ICONST("ER_TMPOPEN",        ZIP_ER_TMPOPEN),` |
|    - | 4791 | `		ZIP_ICONST("ER_ZLIB",           ZIP_ER_ZLIB),` |
|    - | 4792 | `		ZIP_ICONST("ER_MEMORY",         ZIP_ER_MEMORY),` |
|    - | 4793 | `		ZIP_ICONST("ER_CHANGED",        ZIP_ER_CHANGED),` |
|    - | 4794 | `		ZIP_ICONST("ER_COMPNOTSUPP",    ZIP_ER_COMPNOTSUPP),` |
|    - | 4795 | `		ZIP_ICONST("ER_EOF",            ZIP_ER_EOF),` |
|    - | 4796 | `		ZIP_ICONST("ER_INVAL",          ZIP_ER_INVAL),` |
|    - | 4797 | `		ZIP_ICONST("ER_NOZIP",          ZIP_ER_NOZIP),` |
|    - | 4798 | `		ZIP_ICONST("ER_INTERNAL",       ZIP_ER_INTERNAL),` |
|    - | 4799 | `		ZIP_ICONST("ER_INCONS",         ZIP_ER_INCONS),` |
|    - | 4800 | `		ZIP_ICONST("ER_REMOVE",         ZIP_ER_REMOVE),` |
|    - | 4801 | `		ZIP_ICONST("ER_DELETED",        ZIP_ER_DELETED),` |
|    - | 4802 | `		ZIP_ICONST("ER_ENCRNOTSUPP",    ZIP_ER_ENCRNOTSUPP),` |
|    - | 4803 | `		ZIP_ICONST("ER_RDONLY",         ZIP_ER_RDONLY),` |
|    - | 4804 | `		ZIP_ICONST("ER_NOPASSWD",       ZIP_ER_NOPASSWD),` |
|    - | 4805 | `		ZIP_ICONST("ER_WRONGPASSWD",    ZIP_ER_WRONGPASSWD),` |
|    - | 4806 | `		ZIP_ICONST("ER_OPNOTSUPP",      ZIP_ER_OPNOTSUPP),` |
|    - | 4807 | `		ZIP_ICONST("ER_INUSE",          ZIP_ER_INUSE),` |
|    - | 4808 | `		ZIP_ICONST("ER_TELL",           ZIP_ER_TELL),` |
|    - | 4809 | `		ZIP_ICONST("ER_COMPRESSED_DATA",ZIP_ER_COMPRESSED_DATA),` |
|    - | 4810 | `		ZIP_ICONST("ER_CANCELLED",      ZIP_ER_CANCELLED),` |
|    - | 4811 | `		ZIP_ICONST("AFL_RDONLY",        ZIP_AFL_RDONLY),` |
|    - | 4812 | `		ZIP_ICONST("OPSYS_DOS",             0),` |
|    - | 4813 | `		ZIP_ICONST("OPSYS_AMIGA",           1),` |
|    - | 4814 | `		ZIP_ICONST("OPSYS_OPENVMS",         2),` |
|    - | 4815 | `		ZIP_ICONST("OPSYS_UNIX",            ZIP_OPSYS_UNIX),` |
|    - | 4816 | `		ZIP_ICONST("OPSYS_VM_CMS",          4),` |
|    - | 4817 | `		ZIP_ICONST("OPSYS_ATARI_ST",        5),` |
|    - | 4818 | `		ZIP_ICONST("OPSYS_OS_2",            6),` |
|    - | 4819 | `		ZIP_ICONST("OPSYS_MACINTOSH",       7),` |
|    - | 4820 | `		ZIP_ICONST("OPSYS_Z_SYSTEM",        8),` |
|    - | 4821 | `		ZIP_ICONST("OPSYS_CPM",             9),` |
|    - | 4822 | `		ZIP_ICONST("OPSYS_WINDOWS_NTFS",    10),` |
|    - | 4823 | `		ZIP_ICONST("OPSYS_MVS",             11),` |
|    - | 4824 | `		ZIP_ICONST("OPSYS_VSE",             12),` |
|    - | 4825 | `		ZIP_ICONST("OPSYS_ACORN_RISC",      13),` |
|    - | 4826 | `		ZIP_ICONST("OPSYS_VFAT",            14),` |
|    - | 4827 | `		ZIP_ICONST("OPSYS_ALTERNATE_MVS",   15),` |
|    - | 4828 | `		ZIP_ICONST("OPSYS_BEOS",            16),` |
|    - | 4829 | `		ZIP_ICONST("OPSYS_TANDEM",          17),` |
|    - | 4830 | `		ZIP_ICONST("OPSYS_OS_400",          18),` |
|    - | 4831 | `		ZIP_ICONST("OPSYS_OS_X",            19),` |
|    - | 4832 | `		ZIP_ICONST("OPSYS_DEFAULT",         ZIP_OPSYS_UNIX),` |
|    - | 4833 | `		ZIP_ICONST("EM_NONE",           ZIP_EM_NONE),` |
|    - | 4834 | `		ZIP_ICONST("EM_TRAD_PKWARE",    ZIP_EM_TRAD_PKWARE),` |
|    - | 4835 | `		ZIP_ICONST("EM_AES_128",        ZIP_EM_AES_128),` |
|    - | 4836 | `		ZIP_ICONST("EM_AES_192",        ZIP_EM_AES_192),` |
|    - | 4837 | `		ZIP_ICONST("EM_AES_256",        ZIP_EM_AES_256),` |
|    - | 4838 | `		ZIP_ICONST("EM_UNKNOWN",        ZIP_EM_UNKNOWN),` |
|    - | 4839 | `		{ "LIBZIP_VERSION", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_STRING, 0, PHL_ZIP_VERSION, 0.0 },` |
|    - | 4840 | `		ZIP_ICONST("LENGTH_TO_END",     0)` |
|    - | 4841 | `	};` |
|    - | 4842 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|    - | 4843 | `		{ "ZipArchive", 0, "Countable", PH7_CLASS_NOCLONE,` |
|    - | 4844 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|    - | 4845 | `		  aProp, SX_ARRAYSIZE(aProp), ZipInstanceRelease, 0, 0 }` |
|    - | 4846 | `	};` |
|    - | 4847 | `	sxi32 rc;` |
| 7930 | 4848 | `	pVm->pZips = 0;` |
| 7930 | 4849 | `	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
| 7930 | 4850 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 4851 | `		return rc;` |
|    - | 4852 | `	}` |
| 7930 | 4853 | `	rc = PH7_NativeClassInstallSetHook(&(*pVm),"ZipArchive",ZipSetHook);` |
| 7930 | 4854 | `	if( rc != SXRET_OK ){` |
|  ! 0 | 4855 | `		return rc;` |
|    - | 4856 | `	}` |
| 7930 | 4857 | `	return PH7_NativeClassInstallNewHook(&(*pVm),"ZipArchive",ZipNewHook);` |
| 3962 | 4858 | `}` |
| 7925 | 4859 | `PH7_PRIVATE const ph7_builtin_func * PH7_ZipFuncTable(sxu32 *pnEntry)` |
|    5 | 4860 | `{` |
|    - | 4861 | `	static const ph7_builtin_func aFunc[] = {` |
|    - | 4862 | `		{ "zip_open",                     PH7_builtin_zip_open                     },` |
|    - | 4863 | `		{ "zip_close",                    PH7_builtin_zip_close                    },` |
|    - | 4864 | `		{ "zip_read",                     PH7_builtin_zip_read                     },` |
|    - | 4865 | `		{ "zip_entry_open",               PH7_builtin_zip_entry_open               },` |
|    - | 4866 | `		{ "zip_entry_close",              PH7_builtin_zip_entry_close              },` |
|    - | 4867 | `		{ "zip_entry_read",               PH7_builtin_zip_entry_read               },` |
|    - | 4868 | `		{ "zip_entry_name",               PH7_builtin_zip_entry_name               },` |
|    - | 4869 | `		{ "zip_entry_compressedsize",     PH7_builtin_zip_entry_compressedsize     },` |
|    - | 4870 | `		{ "zip_entry_filesize",           PH7_builtin_zip_entry_filesize           },` |
|    - | 4871 | `		{ "zip_entry_compressionmethod",  PH7_builtin_zip_entry_compressionmethod  }` |
|    - | 4872 | `	};` |
| 7930 | 4873 | `	*pnEntry = (sxu32)SX_ARRAYSIZE(aFunc);` |
| 7930 | 4874 | `	return aFunc;` |
|    5 | 4875 | `}` |
|    - | 4876 | `#else /* !PH7_ENABLE_ZLIB \|\| PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 4877 | `/* No zlib means no ext/zip, exactly as php's own build has none: the two VM` |
|    - | 4878 | ` * lifecycle hooks are called unconditionally and have nothing to sweep. */` |
|    - | 4879 | `PH7_PRIVATE void PH7_ZipVmReset(ph7_vm *pVm){ (void)pVm; }` |
|    - | 4880 | `PH7_PRIVATE void PH7_ZipVmRelease(ph7_vm *pVm){ (void)pVm; }` |
|    - | 4881 | `PH7_PRIVATE const char * PH7_ZipResourceType(void *pResource){ (void)pResource; return 0; }` |
|    - | 4882 | `#endif /* PH7_ENABLE_ZLIB && !PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 4883 |  |
