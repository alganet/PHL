# src/ph7/vfs_io_driver.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1210/1448 lines (83.56%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `#include <stdio.h>` |
|     - |    8 | `#include <errno.h>` |
|     - |    9 | `#include <string.h>` |
|     - |   10 |  |
|     - |   11 | `#ifdef __UNIXES__` |
|     - |   12 | `#include <unistd.h>` |
|     - |   13 | `#include <sys/wait.h>` |
|     - |   14 | `#include <fcntl.h>` |
|     - |   15 | `#include <signal.h>` |
|     - |   16 | `#endif` |
|     - |   17 | `#ifdef __WINNT__` |
|     - |   18 | `#include <io.h>     /* _lseeki64/_chsize_s: the positional ops of a std descriptor */` |
|     - |   19 | `#endif` |
|     - |   20 | `/*` |
|     - |   21 | ` * Section:` |
|     - |   22 | ` *    Built-in IO stream drivers: php://, data://, pipe (popen) and the` |
|     - |   23 | ` *    standard stream exporters. The file:// driver lives in` |
|     - |   24 | ` *    vfs_unix.c/vfs_win.c; vfs.c owns registration.` |
|     - |   25 | ` * Status:` |
|     - |   26 | ` *    Stable.` |
|     - |   27 | ` */` |
|     - |   28 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|     - |   29 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - |   30 | `/*` |
|     - |   31 | ` * The following defines are mostly used by the UNIX built and have` |
|     - |   32 | ` * no particular meaning on windows.` |
|     - |   33 | ` */` |
|     - |   34 | `#ifndef STDIN_FILENO` |
|     - |   35 | `#define STDIN_FILENO	0` |
|     - |   36 | `#endif` |
|     - |   37 | `#ifndef STDOUT_FILENO` |
|     - |   38 | `#define STDOUT_FILENO	1` |
|     - |   39 | `#endif` |
|     - |   40 | `#ifndef STDERR_FILENO` |
|     - |   41 | `#define STDERR_FILENO	2` |
|     - |   42 | `#endif` |
|     - |   43 | `/*` |
|     - |   44 | ` * php:// Accessing various I/O streams` |
|     - |   45 | ` * According to the PHP langage reference manual` |
|     - |   46 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|     - |   47 | ` * and output streams, the standard input, output and error file descriptors.` |
|     - |   48 | ` * php://stdin, php://stdout and php://stderr:` |
|     - |   49 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|     - |   50 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|     - |   51 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|     - |   52 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|     - |   53 | ` * php://output` |
|     - |   54 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|     - |   55 | ` *  mechanism in the same way as print and echo.` |
|     - |   56 | ` */` |
|     - |   57 | `typedef struct ph7_stream_data ph7_stream_data;` |
|     - |   58 | ` /* The following structure is the private data associated with the php:// stream */` |
|     - |   59 | `struct ph7_stream_data` |
|     - |   60 | `{` |
|     - |   61 | `	ph7_vm *pVm; /* VM that own this instance */` |
|     - |   62 | `	int iType;   /* Stream type */` |
|     - |   63 | `	union{` |
|     - |   64 | `		void *pHandle; /* Stream handle */` |
|     - |   65 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|     - |   66 | `	}x;` |
|     - |   67 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|     - |   68 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|     - |   69 | `	/* MEMORY type: php's TEMP_STREAM_READONLY -- a php://memory or php://temp` |
|     - |   70 | ``	 * whose MODE STRING held none of `w`, `a` or `+`. Such a stream refuses`` |
|     - |   71 | `	 * both writes and truncation, silently, and stats as 0100444 where a` |
|     - |   72 | ``	 * writable one is 0100666. `x` and `c` land on the read-only side of that`` |
|     - |   73 | `	 * test even though they are write modes, which is php's own rule and not a` |
|     - |   74 | `	 * transcription slip; in flag terms it is "no RDWR, no TRUNC, no APPEND".` |
|     - |   75 | `	 * A data:// payload is NOT one of these -- php builds it writable and only` |
|     - |   76 | `	 * its wrapper's missing writer refuses fwrite(). */` |
|     - |   77 | `	int bReadOnly;` |
|     - |   78 | `	int bTemp;       /* MEMORY type: opened as php://temp rather than php://memory.` |
|     - |   79 | `	                  * php's temp stream is a WRAPPER whose read copies the inner` |
|     - |   80 | `	                  * memory stream's eof flag, so it reports the end one read` |
|     - |   81 | `	                  * EARLIER than a bare php://memory does -- the two devices` |
|     - |   82 | `	                  * are one here, and this is the difference between them. */` |
|     - |   83 | `	/* FILTER type: php://filter/…/resource=… is not a stream of its own — it is` |
|     - |   84 | ``	 * the stream named by `resource=` with a chain wrapped around it. The chain`` |
|     - |   85 | `	 * lives on this io_private, so every read and write below goes through` |
|     - |   86 | `	 * PH7_StreamRead/PH7_StreamWrite and is filtered on the way. */` |
|     - |   87 | `	io_private *pInner;` |
|     - |   88 | `	/* STDIN/STDOUT/STDERR and OUTPUT: where php says the stream IS. php's` |
|     - |   89 | `	 * stream layer keeps a position for every stream and only asks the device` |
|     - |   90 | `	 * when it seeks, so this is a COUNTER rather than a query -- and it starts` |
|     - |   91 | `	 * at whatever the descriptor answered when it was opened, which is -1 when` |
|     - |   92 | `	 * the descriptor is a pipe or a terminal. That negative start is visible:` |
|     - |   93 | ``	 * php's ftell(php://stdout) is `false` on a pipe and `1` after a two-byte`` |
|     - |   94 | `	 * write, and both fall straight out of counting from -1. */` |
|     - |   95 | `	ph7_int64 iPos;` |
|     - |   96 | `	int iFd;      /* STD* types: the descriptor number behind the handle */` |
|     - |   97 | `	int bNoSeek;  /* STD* types: that descriptor could not say where it was */` |
|     - |   98 | `};` |
|     - |   99 | `/*` |
|     - |  100 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|     - |  101 | ` */` |
|   904 |  102 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|     5 |  103 | `{` |
|     - |  104 | `	ph7_stream_data *pData;` |
|   909 |  105 | `	if( pVm == 0 ){` |
|   ! 0 |  106 | `		return 0;` |
|     - |  107 | `	}` |
|     - |  108 | `	/* Allocate a new instance */` |
|   909 |  109 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|   909 |  110 | `	if( pData == 0 ){` |
|   ! 0 |  111 | `		return 0;` |
|     - |  112 | `	}` |
|     - |  113 | `	/* Zero the structure */` |
|   909 |  114 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|     - |  115 | `	/* Initialize fields */` |
|   909 |  116 | `	pData->iType = iType;` |
|   909 |  117 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|   909 |  118 | `	pData->nCur = 0;` |
|   909 |  119 | `	pData->bReadOnly = 0;` |
|   909 |  120 | `	pData->iPos = 0;` |
|   909 |  121 | `	pData->iFd = -1;` |
|   909 |  122 | `	pData->bNoSeek = 0;` |
|   909 |  123 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|     - |  124 | `		/* Nothing else to set up: the buffer is the stream */` |
|   521 |  125 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|     - |  126 | `		/* Point to the default VM consumer routine. */` |
|    12 |  127 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|     7 |  128 | `	}else{` |
|     - |  129 | `#ifdef __WINNT__` |
|     - |  130 | `		DWORD nChannel;` |
|     5 |  131 | `		switch(iType){` |
|     5 |  132 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|     5 |  133 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|     - |  134 | `		default:` |
|     5 |  135 | `			nChannel = STD_INPUT_HANDLE;` |
|     - |  136 | `			break;` |
|     - |  137 | `		}` |
|     5 |  138 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|     - |  139 | `		/* The CRT descriptor beside that handle, for the positional ops -- but` |
|     - |  140 | `		 * only when there IS a standard handle. A host with none (a GUI process` |
|     - |  141 | `		 * that never had a console) leaves the CRT descriptor unopened, and the` |
|     - |  142 | `		 * CRT's answer to a call on one of those is its invalid-parameter` |
|     - |  143 | `		 * handler rather than an error code. */` |
|     5 |  144 | `		if( pData->x.pHandle != NULL && pData->x.pHandle != INVALID_HANDLE_VALUE ){` |
|     5 |  145 | `			pData->iFd = iType == PH7_IO_STREAM_STDOUT ? 1 : (iType == PH7_IO_STREAM_STDERR ? 2 : 0);` |
|     5 |  146 | `			pData->iPos = (ph7_int64)_lseeki64(pData->iFd,0,SEEK_CUR);` |
|     5 |  147 | `		}else{` |
|   ! 0 |  148 | `			pData->iPos = -1;` |
|     - |  149 | `		}` |
|     5 |  150 | `		pData->bNoSeek = pData->iPos < 0;` |
|     - |  151 | `#else` |
|     - |  152 | `		/* Assume an UNIX system */` |
|   121 |  153 | `		int ifd = STDIN_FILENO;` |
|   121 |  154 | `		switch(iType){` |
|    20 |  155 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|    21 |  156 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|    40 |  157 | `		default:` |
|    80 |  158 | `			break;` |
|     - |  159 | `		}` |
|   121 |  160 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|   121 |  161 | `		pData->iFd = ifd;` |
|     - |  162 | `		/* php asks the descriptor where it is exactly once, at open, and keeps` |
|     - |  163 | `		 * the answer -- including the -1 a pipe or a terminal gives back. */` |
|   121 |  164 | `		pData->iPos = (ph7_int64)lseek(ifd,0,SEEK_CUR);` |
|   121 |  165 | `		pData->bNoSeek = pData->iPos < 0;` |
|     - |  166 | `#endif` |
|     - |  167 | `	}` |
|   909 |  168 | `	pData->pVm = pVm;` |
|   909 |  169 | `	return pData;` |
|   454 |  170 | `}` |
|     - |  171 | `/*` |
|     - |  172 | ` * Implementation of the php:// IO streams routines` |
|     - |  173 | ` * Status:` |
|     - |  174 | ` *   Stable.` |
|     - |  175 | ` */` |
|     - |  176 | `/*` |
|     - |  177 | ` * php://filter/<spec>/resource=<uri>. The resource is opened through the` |
|     - |  178 | ` * ordinary device dispatch and the spec's filters are attached to it; what this` |
|     - |  179 | ` * device hands back is a PROXY whose reads and writes go through that handle,` |
|     - |  180 | ` * which is what makes the chain apply to file_get_contents(), include and every` |
|     - |  181 | ` * other opener without one of them knowing about filters at all.` |
|     - |  182 | ` */` |
|     - |  183 | `static void PHPStreamData_Close(void *pHandle);` |
|    46 |  184 | `static int PHPStreamFilterOpen(const char *zSpec,int nSpec,int iMode,ph7_vm *pVm,` |
|     - |  185 | `	ph7_stream_data **ppData)` |
|     3 |  186 | `{` |
|     - |  187 | `	const ph7_io_stream *pInnerStream;` |
|    49 |  188 | `	const char *zRes = 0;` |
|     - |  189 | `	ph7_stream_data *pData;` |
|     - |  190 | `	io_private *pInner;` |
|     - |  191 | `	SyBlob sRes;` |
|    49 |  192 | `	int nRes = 0,i,iChains = 0;` |
|     - |  193 | ``	/* php looks for `/resource=` and cuts the filter list there. When the path`` |
|     - |  194 | ``	 * BEGINS with `resource=` it takes the resource and leaves the list alone —`` |
|     - |  195 | `	 * so the resource's own path segments are then tried as filter names, which` |
|     - |  196 | `	 * is exactly what php warns about. */` |
|   949 |  197 | `	for( i = 0 ; i + 10 <= nSpec ; i++ ){` |
|   947 |  198 | `		if( zSpec[i] == '/' && SyMemcmp(&zSpec[i+1],"resource=",9) == 0 ){` |
|    46 |  199 | `			zRes = &zSpec[i+10];` |
|    46 |  200 | `			nRes = nSpec - (i + 10);` |
|    46 |  201 | `			nSpec = i;` |
|    46 |  202 | `			break;` |
|     - |  203 | `		}` |
|   453 |  204 | `	}` |
|    69 |  205 | `	if( zRes == 0 ){` |
|     3 |  206 | `		if( nSpec >= 9 && SyMemcmp(zSpec,"resource=",9) == 0 ){` |
|     3 |  207 | `			zRes = &zSpec[9];` |
|     3 |  208 | `			nRes = nSpec - 9;` |
|     2 |  209 | `		}else{` |
|   ! 0 |  210 | `			PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,"No URL resource specified");` |
|   ! 0 |  211 | `			return -1;` |
|     - |  212 | `		}` |
|     1 |  213 | `	}` |
|    46 |  214 | `	if( iMode & (PH7_IO_OPEN_RDONLY\|PH7_IO_OPEN_RDWR) ){` |
|    43 |  215 | `		iChains \|= PHL_STREAM_FILTER_READ;` |
|    20 |  216 | `	}` |
|    63 |  217 | `	if( iMode & (PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND) ){` |
|     7 |  218 | `		iChains \|= PHL_STREAM_FILTER_WRITE;` |
|     3 |  219 | `	}` |
|    49 |  220 | `	pData = PHPStreamDataInit(pVm,PH7_IO_STREAM_FILTER);` |
|    49 |  221 | `	if( pData == 0 ){` |
|   ! 0 |  222 | `		return -1;` |
|     - |  223 | `	}` |
|    49 |  224 | `	pInner = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    49 |  225 | `	if( pInner ){` |
|    49 |  226 | `		SyZero(pInner,sizeof(io_private));` |
|    23 |  227 | `	}` |
|    49 |  228 | `	if( pInner == 0 ){` |
|   ! 0 |  229 | `		PHPStreamData_Close((void *)pData);` |
|   ! 0 |  230 | `		return -1;` |
|     - |  231 | `	}` |
|     - |  232 | `	/* The dispatch wants a NUL-terminated URI and mutates the pointer it is` |
|     - |  233 | `	 * handed, so the resource is copied out of the path first. */` |
|    49 |  234 | `	SyBlobInit(&sRes,&pVm->sAllocator);` |
|    49 |  235 | `	SyBlobAppend(&sRes,zRes,(sxu32)nRes);` |
|    49 |  236 | `	SyBlobNullAppend(&sRes);` |
|     - |  237 | `	{` |
|    49 |  238 | `		const char *zPath = (const char *)SyBlobData(&sRes);` |
|    49 |  239 | `		pInnerStream = PH7_VmGetStreamDevice(pVm,&zPath,nRes);` |
|    49 |  240 | `		InitIOPrivate(pVm,pInnerStream,pInner);` |
|    49 |  241 | `		pInner->pHandle = pInnerStream` |
|    46 |  242 | `			? PH7_StreamOpenHandle(pVm,pInnerStream,zPath,iMode,FALSE,0,FALSE,0,0)` |
|    23 |  243 | `			: 0;` |
|    49 |  244 | `		if( pInner->pHandle == 0 ){` |
|     3 |  245 | `			SyBlobRelease(&sRes);` |
|     3 |  246 | `			SyMemBackendFree(&pVm->sAllocator,pInner);` |
|     3 |  247 | `			PHPStreamData_Close((void *)pData);` |
|     3 |  248 | `			return -1;` |
|     - |  249 | `		}` |
|    46 |  250 | `		SetIOPrivateOpenedAs(pInner,zRes,nRes,"r",1);` |
|     - |  251 | `	}` |
|    46 |  252 | `	SyBlobRelease(&sRes);` |
|    46 |  253 | `	pData->pInner = pInner;` |
|    46 |  254 | `	PH7_StreamFilterParseUrl(pVm,zSpec,nSpec,pInner,iChains);` |
|    46 |  255 | `	*ppData = pData;` |
|    46 |  256 | `	return PH7_OK;` |
|    26 |  257 | `}` |
|     - |  258 | `/*` |
|     - |  259 | ` * php's memory devices do not read a mode the way every other stream does.` |
|     - |  260 | `` * `php_stream_url_wrap_php` asks one question of the whole string --`` |
|     - |  261 | `` * `strpbrk(mode, "wa+")` -- so a `w`, an `a` or a `+` ANYWHERE in it builds a`` |
|     - |  262 | ` * writable buffer and nothing else does. Position is not part of it and neither` |
|     - |  263 | `` * is the ordinary first-character grammar: `"rw"`, `"+r"` and `"bw"` are all`` |
|     - |  264 | `` * writable, and `"r"`, `"rb"`, `"x"` and `"c"` are all read-only even though`` |
|     - |  265 | ` * two of those are write modes anywhere else. The test is case-SENSITIVE --` |
|     - |  266 | `` * `"W"` and `"A"` are read-only -- and the mode the stream then REPORTS is`` |
|     - |  267 | `` * decided the same way: any lowercase `a` in the string makes it `a+b`, any`` |
|     - |  268 | `` * other writable spelling `w+b`, and a read-only one `rb`.`` |
|     - |  269 | ` *` |
|     - |  270 | ` * Both answers come from here so the two cannot drift: reading the parsed FLAG` |
|     - |  271 | `` * bits instead cost `"rw"` its writes -- composer's BufferIO opens exactly that`` |
|     - |  272 | ` * and every line it captured came back empty.` |
|     - |  273 | ` */` |
|  1422 |  274 | `PH7_PRIVATE void PH7_PhpMemoryMode(const char *zMode,int nMode,int *pbWrite,int *pbAppend)` |
|     5 |  275 | `{` |
|  1427 |  276 | `	int i,bWrite = 0,bAppend = 0;` |
|  4009 |  277 | `	for( i = 0 ; zMode && i < nMode && zMode[i] != 0 ; ++i ){` |
|  2587 |  278 | `		if( zMode[i] == 'a' ){` |
|   106 |  279 | `			bAppend = 1;` |
|   106 |  280 | `			bWrite = 1;` |
|  2535 |  281 | `		}else if( zMode[i] == 'w' \|\| zMode[i] == '+' ){` |
|  1444 |  282 | `			bWrite = 1;` |
|   720 |  283 | `		}` |
|  1293 |  284 | `	}` |
|  1427 |  285 | `	if( pbWrite ){` |
|  1427 |  286 | `		*pbWrite = bWrite;` |
|   708 |  287 | `	}` |
|  1780 |  288 | `	if( pbAppend ){` |
|   718 |  289 | `		*pbAppend = bAppend;` |
|   355 |  290 | `	}` |
|  1427 |  291 | `}` |
|     - |  292 | `/* Does this php:// name say EXACTLY zWant? php matches its sub-stream names` |
|     - |  293 | `` * whole (case-insensitively) rather than by prefix, and only `temp` may carry`` |
|     - |  294 | ` * anything after it. */` |
|  4578 |  295 | `static int PhpStreamNameIs(const SyString *pName,const char *zWant)` |
|     5 |  296 | `{` |
|  4583 |  297 | `	sxu32 n = SyStrlen(zWant);` |
|  4583 |  298 | `	return pName->nByte == n && SyStrnicmp(pName->zString,zWant,n) == 0;` |
|     5 |  299 | `}` |
|     - |  300 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|   819 |  301 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     5 |  302 | `{` |
|     - |  303 | `	ph7_stream_data *pData;` |
|     - |  304 | `	SyString sStream;` |
|   824 |  305 | `	int iOpenFlags = iMode;   /* iMode is overwritten with the sub-stream below */` |
|   824 |  306 | `	int bTemp = 0;` |
|   819 |  307 | `	if( SyStrnicmp(zName,"filter",sizeof("filter")-1) == 0` |
|   436 |  308 | `	 && (zName[6] == '/' \|\| zName[6] == 0) ){` |
|     - |  309 | `		int rc;` |
|    49 |  310 | `		if( zName[6] == 0 ){` |
|     - |  311 | `			/* php://filter with nothing behind it is not a stream at all. */` |
|   ! 0 |  312 | `			if( pResource && pResource->pVm ){` |
|   ! 0 |  313 | `				PH7_VmThrowError(pResource->pVm,pResource->pVm->pCalleeName,` |
|     - |  314 | `					PH7_CTX_WARNING,"Invalid php:// URL specified");` |
|   ! 0 |  315 | `			}` |
|   ! 0 |  316 | `			return -1;` |
|     - |  317 | `		}` |
|    49 |  318 | `		if( pResource == 0 \|\| pResource->pVm == 0 ){` |
|   ! 0 |  319 | `			return -1;` |
|     - |  320 | `		}` |
|    49 |  321 | `		rc = PHPStreamFilterOpen(&zName[7],(int)SyStrlen(&zName[7]),iMode,pResource->pVm,&pData);` |
|    49 |  322 | `		if( rc != PH7_OK ){` |
|     3 |  323 | `			return -1;` |
|     - |  324 | `		}` |
|    46 |  325 | `		*ppHandle = (void *)pData;` |
|    46 |  326 | `		return PH7_OK;` |
|     - |  327 | `	}` |
|   778 |  328 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|     - |  329 | `	/* Trim leading and trailing white spaces */` |
|   778 |  330 | `	SyStringFullTrim(&sStream);` |
|     - |  331 | ``	/* Stream to open. Every name but `temp` has to match EXACTLY: php refuses`` |
|     - |  332 | ``	 * `php://memoryx`, `php://memory/` and `php://inputx` outright, and a`` |
|     - |  333 | ``	 * prefix test accepted all three -- `php://memoryx` opened a memory stream`` |
|     - |  334 | ``	 * where php has none. `temp` is php's one exception, because`` |
|     - |  335 | ``	 * `php://temp/maxmemory:1024` names the same device (and php accepts`` |
|     - |  336 | ``	 * `php://tempx` with it, prefix and all). */`` |
|   778 |  337 | `	if( PhpStreamNameIs(&sStream,"stdin") ){` |
|   ! 0 |  338 | `		iMode = PH7_IO_STREAM_STDIN;` |
|   778 |  339 | `	}else if( PhpStreamNameIs(&sStream,"output") ){` |
|    12 |  340 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|   773 |  341 | `	}else if( PhpStreamNameIs(&sStream,"stdout") ){` |
|   ! 0 |  342 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|   768 |  343 | `	}else if( PhpStreamNameIs(&sStream,"stderr") ){` |
|   ! 0 |  344 | `		iMode = PH7_IO_STREAM_STDERR;` |
|   768 |  345 | `	}else if( PhpStreamNameIs(&sStream,"input") ){` |
|    21 |  346 | `		iMode = PH7_IO_STREAM_INPUT;` |
|   754 |  347 | `	}else if( PhpStreamNameIs(&sStream,"memory")` |
|   444 |  348 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|     - |  349 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|     - |  350 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|   730 |  351 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|   730 |  352 | `		bTemp = SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0;` |
|   366 |  353 | `	}else{` |
|     - |  354 | `		/* An unknown php:// name is php's own diagnostic, raised BESIDE the` |
|     - |  355 | `		 * caller's "Failed to open stream" rather than instead of it — the same` |
|     - |  356 | ``		 * sentence the empty `php://filter` above already raised. */`` |
|    20 |  357 | `		if( pResource && pResource->pVm ){` |
|    20 |  358 | `			PH7_VmThrowError(pResource->pVm,pResource->pVm->pCalleeName,` |
|     - |  359 | `				PH7_CTX_WARNING,"Invalid php:// URL specified");` |
|     9 |  360 | `		}` |
|    20 |  361 | `		return -1;` |
|     - |  362 | `	}` |
|     - |  363 | `	/* Create our handle */` |
|   760 |  364 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|   760 |  365 | `	if( pData == 0 ){` |
|   ! 0 |  366 | `		return -1;` |
|     - |  367 | `	}` |
|   760 |  368 | `	pData->bTemp = bTemp;` |
|   760 |  369 | `	pData->bReadOnly = iMode == PH7_IO_STREAM_INPUT;` |
|   760 |  370 | `	if( iMode == PH7_IO_STREAM_MEMORY ){` |
|     - |  371 | `		/* The caller's own spelling, armed for the length of this open. Only an` |
|     - |  372 | `		 * opener that HAS one arms it; every other is C with fixed flags, and` |
|     - |  373 | `		 * for those the flag bits are the only thing there is to ask. */` |
|   730 |  374 | `		const char *zAsk = (pResource && pResource->pVm) ? pResource->pVm->zOpenMode : 0;` |
|   730 |  375 | `		if( zAsk && zAsk[0] ){` |
|   714 |  376 | `			int bWrite = 0;` |
|   714 |  377 | `			PH7_PhpMemoryMode(zAsk,(int)SyStrlen(zAsk),&bWrite,0);` |
|   714 |  378 | `			pData->bReadOnly = !bWrite;` |
|   358 |  379 | `		}else{` |
|    20 |  380 | `			pData->bReadOnly =` |
|    16 |  381 | `				(iOpenFlags & (PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_APPEND)) == 0;` |
|     - |  382 | `		}` |
|   361 |  383 | `	}` |
|     - |  384 | `	/* Make the handle public */` |
|   760 |  385 | `	*ppHandle = (void *)pData;` |
|   760 |  386 | `	return PH7_OK;` |
|   413 |  387 | `}` |
|     - |  388 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|  2008 |  389 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|     5 |  390 | `{` |
|  2013 |  391 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  2013 |  392 | `	if( pData == 0 ){` |
|   ! 0 |  393 | `		return -1;` |
|     - |  394 | `	}` |
|  2013 |  395 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     - |  396 | `		/* Through the shared reader, which is where the chain runs. */` |
|    72 |  397 | `		return PH7_StreamRead(pData->pInner,pBuffer,nDatatoRead);` |
|     - |  398 | `	}` |
|  1943 |  399 | `	if( pData->iType == PH7_IO_STREAM_MEMORY \|\| pData->iType == PH7_IO_STREAM_INPUT ){` |
|  1939 |  400 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|     - |  401 | `		sxu32 nRead;` |
|  1939 |  402 | `		if( pData->nCur >= nAvail ){` |
|   455 |  403 | `			return 0; /* EOF */` |
|     - |  404 | `		}` |
|  1489 |  405 | `		nRead = nAvail - pData->nCur;` |
|  1489 |  406 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|  1074 |  407 | `			nRead = (sxu32)nDatatoRead;` |
|   536 |  408 | `		}` |
|  1489 |  409 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|  1489 |  410 | `		pData->nCur += nRead;` |
|  1489 |  411 | `		return (ph7_int64)nRead;` |
|     - |  412 | `	}` |
|     5 |  413 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|     - |  414 | `		/* Forbidden */` |
|   ! 0 |  415 | `		return -1;` |
|     - |  416 | `	}` |
|     - |  417 | `#ifdef __WINNT__` |
|     - |  418 | `	{` |
|     - |  419 | `		DWORD nRd;` |
|     - |  420 | `		BOOL rc;` |
|     1 |  421 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|     1 |  422 | `		if( !rc ){` |
|     - |  423 | `			/* IO error */` |
|   ! 0 |  424 | `			return -1;` |
|     - |  425 | `		}` |
|     1 |  426 | `		pData->iPos += (ph7_int64)nRd;` |
|     1 |  427 | `		return (ph7_int64)nRd;` |
|     - |  428 | `	}` |
|     - |  429 | `#elif defined(__UNIXES__)` |
|     - |  430 | `	{` |
|     - |  431 | `		ssize_t nRd;` |
|     - |  432 | `		int fd;` |
|     4 |  433 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|     4 |  434 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|     4 |  435 | `		if( nRd < 0 ){` |
|   ! 0 |  436 | `			return -1;` |
|     - |  437 | `		}` |
|     4 |  438 | `		pData->iPos += (ph7_int64)nRd;` |
|     - |  439 | `		/* ZERO is end of file, not an error — the contract every other device` |
|     - |  440 | `		 * here keeps. Collapsing the two meant nothing could ever latch EOF on` |
|     - |  441 | ``		 * php://stdin, so `while (!feof(STDIN))` never ended. */`` |
|     4 |  442 | `		return (ph7_int64)nRd;` |
|     - |  443 | `	}` |
|     - |  444 | `#else` |
|     - |  445 | `	return -1;` |
|     - |  446 | `#endif` |
|  1009 |  447 | `}` |
|     - |  448 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|   579 |  449 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|     4 |  450 | `{` |
|   583 |  451 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   583 |  452 | `	if( pData == 0 ){` |
|   ! 0 |  453 | `		return -1;` |
|     - |  454 | `	}` |
|   583 |  455 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     7 |  456 | `		return PH7_StreamWrite(pData->pInner,pBuf,nWrite);` |
|     - |  457 | `	}` |
|   577 |  458 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|     - |  459 | `		/* Forbidden */` |
|   ! 0 |  460 | `		return -1;` |
|   577 |  461 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY \|\| pData->iType == PH7_IO_STREAM_INPUT ){` |
|     - |  462 | `		sxu32 nLen,nEnd;` |
|   510 |  463 | `		if( pData->bReadOnly ){` |
|    74 |  464 | `			return -1;` |
|     - |  465 | `		}` |
|   438 |  466 | `		nLen = SyBlobLength(&pData->sMem);` |
|   438 |  467 | `		if( pData->nCur > nLen ){` |
|     - |  468 | `			/* seek past end: php zero-fills the gap */` |
|     - |  469 | `			static const char zZero[64] = {0};` |
|   ! 0 |  470 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|   ! 0 |  471 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|   ! 0 |  472 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|   ! 0 |  473 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|   ! 0 |  474 | `					return -1;` |
|     - |  475 | `				}` |
|   ! 0 |  476 | `			}` |
|   ! 0 |  477 | `			nLen = SyBlobLength(&pData->sMem);` |
|   ! 0 |  478 | `		}` |
|   438 |  479 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|   438 |  480 | `		if( pData->nCur < nLen ){` |
|     - |  481 | `			/* overwrite in place up to the current end */` |
|     8 |  482 | `			sxu32 nOver = nLen - pData->nCur;` |
|     8 |  483 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|    11 |  484 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|    11 |  485 | `			if( nEnd > nLen ){` |
|   ! 0 |  486 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|   ! 0 |  487 | `					return -1;` |
|     - |  488 | `				}` |
|   ! 0 |  489 | `			}` |
|     5 |  490 | `		}else{` |
|   432 |  491 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|   ! 0 |  492 | `				return -1;` |
|     - |  493 | `			}` |
|     - |  494 | `		}` |
|   438 |  495 | `		pData->nCur = nEnd;` |
|   438 |  496 | `		return nWrite;` |
|    69 |  497 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|    16 |  498 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|     - |  499 | `		int rc;` |
|     - |  500 | `		/* Call the vm output consumer */` |
|    16 |  501 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|    16 |  502 | `		if( rc == PH7_ABORT ){` |
|   ! 0 |  503 | `			return -1;` |
|     - |  504 | `		}` |
|    16 |  505 | `		pData->iPos += nWrite;` |
|    16 |  506 | `		return nWrite;` |
|     - |  507 | `	}` |
|     - |  508 | `#ifdef __WINNT__` |
|     - |  509 | `	{` |
|     - |  510 | `		DWORD nWr;` |
|     - |  511 | `		BOOL rc;` |
|   ! 0 |  512 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|   ! 0 |  513 | `		if( !rc ){` |
|     - |  514 | `			/* IO error */` |
|   ! 0 |  515 | `			return -1;` |
|     - |  516 | `		}` |
|   ! 0 |  517 | `		pData->iPos += (ph7_int64)nWr;` |
|   ! 0 |  518 | `		return (ph7_int64)nWr;` |
|     - |  519 | `	}` |
|     - |  520 | `#elif defined(__UNIXES__)` |
|     - |  521 | `	{` |
|     - |  522 | `		ssize_t nWr;` |
|     - |  523 | `		int fd;` |
|    53 |  524 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    53 |  525 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    53 |  526 | `		if( nWr < 1 ){` |
|   ! 0 |  527 | `			return -1;` |
|     - |  528 | `		}` |
|    53 |  529 | `		pData->iPos += (ph7_int64)nWr;` |
|    53 |  530 | `		return (ph7_int64)nWr;` |
|     - |  531 | `	}` |
|     - |  532 | `#else` |
|     - |  533 | `	return -1;` |
|     - |  534 | `#endif` |
|   272 |  535 | `}` |
|     - |  536 | `/* void (*xClose)(void *) */` |
|   819 |  537 | `static void PHPStreamData_Close(void *pHandle)` |
|     5 |  538 | `{` |
|   824 |  539 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     - |  540 | `	ph7_vm *pVm;` |
|   824 |  541 | `	if( pData == 0 ){` |
|   ! 0 |  542 | `		return;` |
|     - |  543 | `	}` |
|   824 |  544 | `	pVm = pData->pVm;` |
|   824 |  545 | `	if( pData->iType == PH7_IO_STREAM_FILTER && pData->pInner ){` |
|     - |  546 | `		/* The write chain closes while the device below is still open. */` |
|    46 |  547 | `		PH7_StreamFilterReleaseChains(pData->pInner);` |
|    46 |  548 | `		if( pData->pInner->pStream ){` |
|    46 |  549 | `			PH7_StreamCloseHandle(pData->pInner->pStream,pData->pInner->pHandle);` |
|    22 |  550 | `		}` |
|    46 |  551 | `		SyBlobRelease(&pData->pInner->sBuffer);` |
|    46 |  552 | `		SyBlobRelease(&pData->pInner->sFilt);` |
|    46 |  553 | `		SyBlobRelease(&pData->pInner->sUri);` |
|    46 |  554 | `		SyMemBackendFree(&pVm->sAllocator,pData->pInner);` |
|    46 |  555 | `		pData->pInner = 0;` |
|    22 |  556 | `	}` |
|   824 |  557 | `	SyBlobRelease(&pData->sMem);` |
|     - |  558 | `	/* Free the instance */` |
|   824 |  559 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|   413 |  560 | `}` |
|     - |  561 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|   720 |  562 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|     3 |  563 | `{` |
|   723 |  564 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     - |  565 | `	ph7_int64 iNew;` |
|   723 |  566 | `	if( pData == 0 ){` |
|   ! 0 |  567 | `		return -1;` |
|     - |  568 | `	}` |
|   723 |  569 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     9 |  570 | `		return PH7_StreamSeekWrapped(pData->pInner,iOfft,whence);` |
|     - |  571 | `	}` |
|   715 |  572 | `	if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|     - |  573 | ``		/* php's output stream has no seek at all -- `fseek(): Stream does not`` |
|     - |  574 | ``		 * support seeking`, which is what the unsupported code asks for. */`` |
|     7 |  575 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  576 | `	}` |
|   709 |  577 | `	if( pData->iType != PH7_IO_STREAM_MEMORY && pData->iType != PH7_IO_STREAM_INPUT ){` |
|     - |  578 | `		/* A standard descriptor seeks exactly as far as the descriptor does,` |
|     - |  579 | `		 * and php refuses the seek OUTRIGHT when the descriptor could not say` |
|     - |  580 | ``		 * where it was at open -- so `php -r … > pipe` warns while the same`` |
|     - |  581 | `		 * script redirected to a file seeks. */` |
|   ! 0 |  582 | `		if( pData->bNoSeek ){` |
|   ! 0 |  583 | `			return SXERR_NOTIMPLEMENTED;` |
|     - |  584 | `		}` |
|     - |  585 | `		{` |
|     - |  586 | `#ifdef __WINNT__` |
|   ! 0 |  587 | `			ph7_int64 iNewPos = pData->iFd < 0 ? -1` |
|     - |  588 | `				: (ph7_int64)_lseeki64(pData->iFd,iOfft,whence);` |
|     - |  589 | `#else` |
|   ! 0 |  590 | `			ph7_int64 iNewPos = (ph7_int64)lseek(pData->iFd,(off_t)iOfft,whence);` |
|     - |  591 | `#endif` |
|   ! 0 |  592 | `			if( iNewPos < 0 ){` |
|   ! 0 |  593 | `				return -1;` |
|     - |  594 | `			}` |
|   ! 0 |  595 | `			pData->iPos = iNewPos;` |
|   ! 0 |  596 | `			return PH7_OK;` |
|     - |  597 | `		}` |
|     - |  598 | `	}` |
|   709 |  599 | `	switch(whence){` |
|   195 |  600 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|     7 |  601 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|   511 |  602 | `	default:            iNew = iOfft; break;` |
|     - |  603 | `	}` |
|   709 |  604 | `	if( iNew < 0 ){` |
|   ! 0 |  605 | `		return -1;` |
|     - |  606 | `	}` |
|   709 |  607 | `	pData->nCur = (sxu32)iNew;` |
|   709 |  608 | `	return PH7_OK;` |
|   363 |  609 | `}` |
|     - |  610 | `/* ph7_int64 (*xTell)(void *) */` |
|   869 |  611 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|     4 |  612 | `{` |
|   873 |  613 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   873 |  614 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|     - |  615 | `		/* Where the SCRIPT is on the wrapped stream: the device sits past` |
|     - |  616 | `		 * whatever the chain has already produced and nobody has taken. */` |
|    17 |  617 | `		return PH7_StreamLogicalTell(pData->pInner);` |
|     - |  618 | `	}` |
|   857 |  619 | `	if( pData == 0 ){` |
|   ! 0 |  620 | `		return -1;` |
|     - |  621 | `	}` |
|   857 |  622 | `	if( pData->iType != PH7_IO_STREAM_MEMORY && pData->iType != PH7_IO_STREAM_INPUT ){` |
|     - |  623 | `		/* The COUNTER, not the descriptor: php never re-asks, which is why a` |
|     - |  624 | `		 * write to a non-seekable stdout moves the position it reports even` |
|     - |  625 | `		 * though nothing can seek there. */` |
|    25 |  626 | `		return pData->iPos;` |
|     - |  627 | `	}` |
|   834 |  628 | `	return (ph7_int64)pData->nCur;` |
|   438 |  629 | `}` |
|     - |  630 | `/* int (*xSync)(void *)` |
|     - |  631 | ` *` |
|     - |  632 | ` * Nothing on this device buffers, so a flush has nothing to do and SUCCEEDS --` |
|     - |  633 | ` * which is what php answers for php://memory, php://temp, php://output and the` |
|     - |  634 | ` * three standard descriptors. php://input is the one stream in the family whose` |
|     - |  635 | `` * flush op FAILS, and `fflush(fopen(php://input))` is php's `false`. */`` |
|     6 |  636 | `static int PHPStreamData_Sync(void *pHandle)` |
|     1 |  637 | `{` |
|     7 |  638 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     7 |  639 | `	if( pData == 0 ){` |
|   ! 0 |  640 | `		return -1;` |
|     - |  641 | `	}` |
|     7 |  642 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|   ! 0 |  643 | `		io_private *pIn = pData->pInner;` |
|   ! 0 |  644 | `		if( pIn == 0 \|\| pIn->pStream == 0 \|\| pIn->pStream->xSync == 0 ){` |
|   ! 0 |  645 | `			return PH7_OK;` |
|     - |  646 | `		}` |
|   ! 0 |  647 | `		return pIn->pStream->xSync(pIn->pHandle);` |
|     - |  648 | `	}` |
|     7 |  649 | `	return pData->iType == PH7_IO_STREAM_INPUT ? -1 : PH7_OK;` |
|     4 |  650 | `}` |
|     - |  651 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    30 |  652 | `static int PHPStreamData_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|     2 |  653 | `{` |
|    32 |  654 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    32 |  655 | `	if( pData == 0 ){` |
|   ! 0 |  656 | `		return -1;` |
|     - |  657 | `	}` |
|    32 |  658 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|   ! 0 |  659 | `		io_private *pIn = pData->pInner;` |
|   ! 0 |  660 | `		if( pIn == 0 \|\| pIn->pStream == 0 \|\| pIn->pStream->xStat == 0 ){` |
|   ! 0 |  661 | `			return -1;` |
|     - |  662 | `		}` |
|   ! 0 |  663 | `		return pIn->pStream->xStat(pIn->pHandle,pArray,pWorker);` |
|     - |  664 | `	}` |
|    32 |  665 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     - |  666 | `		/* php's memory streams -- php://memory, php://temp and the data://` |
|     - |  667 | `		 * payloads that share them -- answer a SYNTHETIC record describing no` |
|     - |  668 | `		 * file at all: a regular-file mode of 0666, php's own 0xC device, and` |
|     - |  669 | `		 * -1 in the three fields a buffer cannot have. Only the size is real.` |
|     - |  670 | `		 * Read back from php 8.5.9 and confirmed identical on Windows. */` |
|     - |  671 | `		ph7_int64 aVal[13];` |
|    28 |  672 | `		aVal[0]  = 0xC;                                   /* dev */` |
|    28 |  673 | `		aVal[1]  = 0;                                     /* ino */` |
|    28 |  674 | `		aVal[2]  = 0100000 \| (pData->bReadOnly ? 0444 : 0666); /* mode: S_IFREG\|… */` |
|    28 |  675 | `		aVal[3]  = 1;                                     /* nlink */` |
|    28 |  676 | `		aVal[4]  = 0;                                     /* uid */` |
|    28 |  677 | `		aVal[5]  = 0;                                     /* gid */` |
|    28 |  678 | `		aVal[6]  = -1;                                    /* rdev */` |
|    28 |  679 | `		aVal[7]  = (ph7_int64)SyBlobLength(&pData->sMem); /* size */` |
|    28 |  680 | `		aVal[8]  = 0;                                     /* atime */` |
|    28 |  681 | `		aVal[9]  = 0;                                     /* mtime */` |
|    28 |  682 | `		aVal[10] = 0;                                     /* ctime */` |
|    28 |  683 | `		aVal[11] = -1;                                    /* blksize */` |
|    28 |  684 | `		aVal[12] = -1;                                    /* blocks */` |
|    28 |  685 | `		return PH7_VfsStatFill(pArray,pWorker,aVal);` |
|     - |  686 | `	}` |
|     5 |  687 | `	if( pData->iType == PH7_IO_STREAM_OUTPUT \|\| pData->iType == PH7_IO_STREAM_INPUT ){` |
|     - |  688 | `		/* Neither has a descriptor to stat: php's fstat() is false for both. */` |
|     5 |  689 | `		return -1;` |
|     - |  690 | `	}` |
|   ! 0 |  691 | `	return PH7_VfsStatFromFd(pData->iFd,pArray,pWorker);` |
|    17 |  692 | `}` |
|     - |  693 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|    24 |  694 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|     2 |  695 | `{` |
|    26 |  696 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    26 |  697 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|   ! 0 |  698 | `		io_private *pIn = pData->pInner;` |
|   ! 0 |  699 | `		if( pIn == 0 \|\| pIn->pStream == 0 \|\| pIn->pStream->xTrunc == 0 ){` |
|   ! 0 |  700 | `			return -1;` |
|     - |  701 | `		}` |
|   ! 0 |  702 | `		return pIn->pStream->xTrunc(pIn->pHandle,nLen);` |
|     - |  703 | `	}` |
|    26 |  704 | `	if( pData == 0 ){` |
|   ! 0 |  705 | `		return -1;` |
|     - |  706 | `	}` |
|    26 |  707 | `	if( pData->iType == PH7_IO_STREAM_OUTPUT \|\| pData->iType == PH7_IO_STREAM_INPUT ){` |
|     - |  708 | ``		/* Nothing to truncate: php's `Can't truncate this stream!`. */`` |
|     5 |  709 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  710 | `	}` |
|    22 |  711 | `	if( pData->iType == PH7_IO_STREAM_MEMORY && pData->bReadOnly ){` |
|     - |  712 | `		/* php's read-only memory stream answers a SILENT false. */` |
|    10 |  713 | `		return -1;` |
|     - |  714 | `	}` |
|    14 |  715 | `	if( pData->iType != PH7_IO_STREAM_MEMORY ){` |
|     - |  716 | `		/* A standard descriptor truncates whatever it points AT -- php does the` |
|     - |  717 | ``		 * same call, so `php … > out.txt` really does empty out.txt through`` |
|     - |  718 | `		 * ftruncate(STDOUT), and a pipe or a terminal answers a SILENT false. */` |
|     - |  719 | `#ifdef __WINNT__` |
|   ! 0 |  720 | `		return pData->iFd >= 0 && _chsize_s(pData->iFd,(__int64)nLen) == 0 ? PH7_OK : -1;` |
|     - |  721 | `#else` |
|   ! 0 |  722 | `		return ftruncate(pData->iFd,(off_t)nLen) == 0 ? PH7_OK : -1;` |
|     - |  723 | `#endif` |
|     - |  724 | `	}` |
|     - |  725 | `	/* php truncates a READ-ONLY memory stream happily -- data:// answers true` |
|     - |  726 | `	 * and its payload really does shrink, even though fwrite() to it is` |
|     - |  727 | `	 * refused. The two are separate permissions there, and they are here. */` |
|    14 |  728 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|     - |  729 | `		/* shrink in place: the blob keeps its allocation */` |
|     5 |  730 | `		pData->sMem.nByte = (sxu32)nLen;` |
|     3 |  731 | `	}else{` |
|     - |  732 | `		static const char zZero[64] = {0};` |
|    16 |  733 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|     7 |  734 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|     7 |  735 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|     7 |  736 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|   ! 0 |  737 | `				return -1;` |
|     - |  738 | `			}` |
|     1 |  739 | `		}` |
|     - |  740 | `	}` |
|    14 |  741 | `	return PH7_OK;` |
|    14 |  742 | `}` |
|     - |  743 | `/*` |
|     - |  744 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|     - |  745 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|     - |  746 | ` * base64). Shares the MEMORY machinery above.` |
|     - |  747 | ` */` |
|    20 |  748 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     2 |  749 | `{` |
|    22 |  750 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|     2 |  751 | `}` |
|    54 |  752 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     4 |  753 | `{` |
|     - |  754 | `	ph7_stream_data *pData;` |
|    58 |  755 | `	const char *zIn = zName;` |
|    58 |  756 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|    58 |  757 | `	const char *zComma = 0;` |
|    58 |  758 | `	int bBase64 = 0;` |
|    27 |  759 | `	SXUNUSED(iMode);` |
|     - |  760 | `	/* Find the comma separating the mediatype from the payload */` |
|   770 |  761 | `	while( zIn < zEnd ){` |
|   764 |  762 | `		if( zIn[0] == ',' ){` |
|    52 |  763 | `			zComma = zIn;` |
|    52 |  764 | `			break;` |
|     - |  765 | `		}` |
|   716 |  766 | `		zIn++;` |
|     4 |  767 | `	}` |
|    58 |  768 | `	if( zComma == 0 ){` |
|     - |  769 | `		/* php's own wording for the one thing its data wrapper checks. */` |
|     7 |  770 | `		if( pResource && pResource->pVm ){` |
|     7 |  771 | `			PH7_StreamSetOpenError(pResource->pVm,"rfc2397: no comma in URL");` |
|     3 |  772 | `		}` |
|     7 |  773 | `		return -1;` |
|     - |  774 | `	}` |
|    48 |  775 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|    50 |  776 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|    10 |  777 | `		bBase64 = 1;` |
|     4 |  778 | `	}` |
|    52 |  779 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|    52 |  780 | `	if( pData == 0 ){` |
|   ! 0 |  781 | `		return -1;` |
|     - |  782 | `	}` |
|    52 |  783 | `	zIn = &zComma[1];` |
|    52 |  784 | `	if( bBase64 ){` |
|    10 |  785 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|   ! 0 |  786 | `			SyBlobRelease(&pData->sMem);` |
|   ! 0 |  787 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|   ! 0 |  788 | `			return -1;` |
|     - |  789 | `		}` |
|     6 |  790 | `	}else{` |
|     - |  791 | `		/* percent-decode the payload */` |
|   248 |  792 | `		while( zIn < zEnd ){` |
|   208 |  793 | `			char c = zIn[0];` |
|   208 |  794 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|     3 |  795 | `				int hi = SyHexToint(zIn[1]);` |
|     3 |  796 | `				int lo = SyHexToint(zIn[2]);` |
|     3 |  797 | `				c = (char)((hi << 4) \| lo);` |
|     3 |  798 | `				zIn += 3;` |
|     2 |  799 | `			}else{` |
|   206 |  800 | `				zIn++;` |
|     - |  801 | `			}` |
|   208 |  802 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|   ! 0 |  803 | `				SyBlobRelease(&pData->sMem);` |
|   ! 0 |  804 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|   ! 0 |  805 | `				return -1;` |
|     - |  806 | `			}` |
|     4 |  807 | `		}` |
|     - |  808 | `	}` |
|    52 |  809 | `	*ppHandle = (void *)pData;` |
|    52 |  810 | `	return PH7_OK;` |
|    31 |  811 | `}` |
|     - |  812 | `PH7_PRIVATE const ph7_io_stream sDATA_Stream = {` |
|     - |  813 | `	"data",` |
|     - |  814 | `	PH7_IO_STREAM_VERSION,` |
|     - |  815 | `	DataStreamData_Open,  /* xOpen */` |
|     - |  816 | `	0,   /* xOpenDir */` |
|     - |  817 | `	PHPStreamData_Close, /* xClose */` |
|     - |  818 | `	0,  /* xCloseDir */` |
|     - |  819 | `	PHPStreamData_Read,  /* xRead */` |
|     - |  820 | `	0,  /* xReadDir */` |
|     - |  821 | `	/* xWrite: NONE. php's RFC 2397 stream has no writer at all, and its` |
|     - |  822 | ``	 * fwrite() is `Stream is not writable` rather than a failed write --`` |
|     - |  823 | `	 * a refusal that only a MISSING slot can produce. */` |
|     - |  824 | `	0,` |
|     - |  825 | `	PHPStreamData_Seek,  /* xSeek */` |
|     - |  826 | `	0,  /* xLock */` |
|     - |  827 | `	0,  /* xRewindDir */` |
|     - |  828 | `	PHPStreamData_Tell,  /* xTell */` |
|     - |  829 | `	PHPStreamData_Trunc, /* xTrunc: a data:// payload really does shrink */` |
|     - |  830 | `	0,  /* xSync */` |
|     - |  831 | `	PHPStreamData_Stat   /* xStat: the synthetic memory-stream record */` |
|     - |  832 | `};` |
|     - |  833 | `/*` |
|     - |  834 | ` * Pipe stream implementation for popen/pclose.` |
|     - |  835 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|     - |  836 | ` * PHP-compatible process I/O functionality.` |
|     - |  837 | ` */` |
|     - |  838 | `typedef struct pipe_private pipe_private;` |
|     - |  839 | `struct pipe_private` |
|     - |  840 | `{` |
|     - |  841 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|     - |  842 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|     - |  843 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|     - |  844 | `#ifdef __WINNT__` |
|     - |  845 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|     - |  846 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|     - |  847 | `#endif` |
|     - |  848 | `};` |
|     - |  849 |  |
|     - |  850 | `#ifdef __WINNT__` |
|     - |  851 | `#include <Windows.h>` |
|     - |  852 | `#include <stdio.h>` |
|     - |  853 | `#include <io.h>` |
|     - |  854 | `#include <fcntl.h>` |
|     - |  855 | `/*` |
|     - |  856 | ` * Custom Windows popen implementation using CreateProcess.` |
|     - |  857 | ` * This allows us to properly wait for process completion.` |
|     - |  858 | ` */` |
|     - |  859 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|     5 |  860 | `{` |
|     5 |  861 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|     5 |  862 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|     5 |  863 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|     - |  864 | `	SECURITY_ATTRIBUTES sa;` |
|     - |  865 | `	STARTUPINFOW si;` |
|     - |  866 | `	PROCESS_INFORMATION pi;` |
|     5 |  867 | `	WCHAR *zWideCmd = NULL;` |
|     5 |  868 | `	FILE *pFile = NULL;` |
|     - |  869 | `	int fd;` |
|     5 |  870 | `	BOOL bRead = (zMode[0] == 'r');` |
|     5 |  871 | `	BOOL bBinary = (strchr(zMode,'b') != NULL);` |
|     - |  872 |  |
|     - |  873 | `	/* Set up security attributes for pipe inheritance */` |
|     5 |  874 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|     5 |  875 | `	sa.bInheritHandle = TRUE;` |
|     5 |  876 | `	sa.lpSecurityDescriptor = NULL;` |
|     - |  877 |  |
|     - |  878 | `	/* Create pipes for child process I/O */` |
|     5 |  879 | `	if( bRead ){` |
|     - |  880 | `		/* Reading from child's stdout */` |
|     5 |  881 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|   ! 0 |  882 | `			return NULL;` |
|     - |  883 | `		}` |
|     - |  884 | `		/* Ensure read handle is not inherited */` |
|     5 |  885 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|     5 |  886 | `		hReadPipe = hChildStdoutRd;` |
|     5 |  887 | `		*phPipe = hChildStdoutRd;` |
|     5 |  888 | `	}else{` |
|     - |  889 | `		/* Writing to child's stdin */` |
|     2 |  890 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|   ! 0 |  891 | `			return NULL;` |
|     - |  892 | `		}` |
|     - |  893 | `		/* Ensure write handle is not inherited */` |
|     2 |  894 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|     2 |  895 | `		hWritePipe = hChildStdinWr;` |
|     2 |  896 | `		*phPipe = hChildStdinWr;` |
|     - |  897 | `	}` |
|     - |  898 |  |
|     - |  899 | `	/* Convert command to wide string */` |
|     - |  900 | `	{` |
|     5 |  901 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|     5 |  902 | `		if( nLen <= 0 ){` |
|   ! 0 |  903 | `			goto cleanup_pipes;` |
|     - |  904 | `		}` |
|     5 |  905 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|     5 |  906 | `		if( !zWideCmd ){` |
|   ! 0 |  907 | `			goto cleanup_pipes;` |
|     - |  908 | `		}` |
|     5 |  909 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|     - |  910 | `	}` |
|     - |  911 |  |
|     - |  912 | `	/* Set up process startup info */` |
|     5 |  913 | `	ZeroMemory(&si, sizeof(si));` |
|     5 |  914 | `	si.cb = sizeof(si);` |
|     5 |  915 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|     5 |  916 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|     5 |  917 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|     5 |  918 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|     5 |  919 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|     - |  920 |  |
|     5 |  921 | `	ZeroMemory(&pi, sizeof(pi));` |
|     - |  922 |  |
|     - |  923 | `	/* Create the child process */` |
|     5 |  924 | `	if( !CreateProcessW(` |
|     - |  925 | `		NULL,           /* Application name */` |
|     - |  926 | `		zWideCmd,       /* Command line */` |
|     - |  927 | `		NULL,           /* Process security attributes */` |
|     - |  928 | `		NULL,           /* Thread security attributes */` |
|     - |  929 | `		TRUE,           /* Inherit handles */` |
|     - |  930 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|     - |  931 | `		NULL,           /* Environment */` |
|     - |  932 | `		NULL,           /* Current directory */` |
|     - |  933 | `		&si,            /* Startup info */` |
|     - |  934 | `		&pi             /* Process info */` |
|     - |  935 | `	)){` |
|   ! 0 |  936 | `		goto cleanup_all;` |
|     - |  937 | `	}` |
|     - |  938 |  |
|     - |  939 | `	/* Close handles we don't need in parent */` |
|     5 |  940 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|     5 |  941 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|     - |  942 |  |
|     - |  943 | `	/* Close thread handle (we only need process handle) */` |
|     5 |  944 | `	CloseHandle(pi.hThread);` |
|     - |  945 |  |
|     - |  946 | `	/* Store process handle for later waiting */` |
|     5 |  947 | `	*phProcess = pi.hProcess;` |
|     - |  948 |  |
|     - |  949 | `	/* Convert OS handle to C file descriptor, then to FILE*. The TRANSLATION mode` |
|     - |  950 | `	 * is the caller's, not a constant: cmd.exe writes CRLF, and a descriptor` |
|     - |  951 | `	 * opened _O_TEXT eats every CR on the way in. php picks it per call —` |
|     - |  952 | `	 * "rb" for exec/system/passthru, so their output is byte-exact, "rt" for` |
|     - |  953 | `	 * shell_exec, and the script's own mode for popen() — and this used to` |
|     - |  954 | ``	 * hardcode _O_TEXT for all of them, so `passthru('type file.bin')` lost every`` |
|     - |  955 | `	 * 0x0D byte and system()'s output came back LF-only where php's is CRLF. */` |
|     5 |  956 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|     - |  957 | `	                     (bRead ? _O_RDONLY : _O_WRONLY)` |
|     - |  958 | `	                     \| (bBinary ? _O_BINARY : _O_TEXT));` |
|     5 |  959 | `	if( fd == -1 ){` |
|   ! 0 |  960 | `		CloseHandle(pi.hProcess);` |
|   ! 0 |  961 | `		*phProcess = NULL;` |
|   ! 0 |  962 | `		goto cleanup_all;` |
|     - |  963 | `	}` |
|     - |  964 |  |
|     - |  965 | `	/* The stream's own translation has to agree with the descriptor's */` |
|     5 |  966 | `	pFile = _fdopen(fd, bBinary ? (bRead ? "rb" : "wb") : (bRead ? "rt" : "wt"));` |
|     5 |  967 | `	if( !pFile ){` |
|   ! 0 |  968 | `		_close(fd); /* This will also close the underlying handle */` |
|   ! 0 |  969 | `		CloseHandle(pi.hProcess);` |
|   ! 0 |  970 | `		*phProcess = NULL;` |
|   ! 0 |  971 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|   ! 0 |  972 | `		return NULL;` |
|     - |  973 | `	}` |
|     - |  974 |  |
|     5 |  975 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|     5 |  976 | `	return pFile;` |
|     - |  977 |  |
|     - |  978 | `cleanup_all:` |
|   ! 0 |  979 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|     - |  980 | `cleanup_pipes:` |
|   ! 0 |  981 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|   ! 0 |  982 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|   ! 0 |  983 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|   ! 0 |  984 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|   ! 0 |  985 | `	return NULL;` |
|     5 |  986 | `}` |
|     - |  987 |  |
|     - |  988 | `/*` |
|     - |  989 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|     - |  990 | ` */` |
|     - |  991 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|     5 |  992 | `{` |
|     5 |  993 | `	DWORD dwExitCode = 0;` |
|     - |  994 | `	int status;` |
|     - |  995 |  |
|     - |  996 | `	/* Close the FILE* (this closes the pipe) */` |
|     5 |  997 | `	fclose(pFile);` |
|     - |  998 |  |
|     5 |  999 | `	if( hProcess ){` |
|     - | 1000 | `		/* Wait for the process to complete */` |
|     5 | 1001 | `		WaitForSingleObject(hProcess, INFINITE);` |
|     - | 1002 |  |
|     5 | 1003 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|     5 | 1004 | `			status = (int)dwExitCode;` |
|     5 | 1005 | `		}else{` |
|   ! 0 | 1006 | `			status = -1;` |
|     - | 1007 | `		}` |
|     - | 1008 |  |
|     - | 1009 | `		/* Close process handle */` |
|     5 | 1010 | `		CloseHandle(hProcess);` |
|     5 | 1011 | `	}else{` |
|   ! 0 | 1012 | `		status = -1;` |
|     - | 1013 | `	}` |
|     - | 1014 |  |
|     5 | 1015 | `	return status;` |
|     5 | 1016 | `}` |
|     - | 1017 | `#endif /* __WINNT__ */` |
|     - | 1018 | `/*` |
|     - | 1019 | ` * Open a pipe to a process.` |
|     - | 1020 | ` * This is called internally by popen(), not through the stream device interface.` |
|     - | 1021 | ` */` |
|  8301 | 1022 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|     5 | 1023 | `{` |
|     - | 1024 | `	pipe_private *pPipe;` |
|     - | 1025 | `	FILE *pFile;` |
|  8306 | 1026 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|   ! 0 | 1027 | `		return 0;` |
|     - | 1028 | `	}` |
|     - | 1029 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|  8306 | 1030 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|   ! 0 | 1031 | `		return 0;` |
|     - | 1032 | `	}` |
|     - | 1033 | `	/* Open the pipe using system popen */` |
|     - | 1034 | `#ifdef __WINNT__` |
|     - | 1035 | `	{` |
|     - | 1036 | `		/* Build cmd.exe command wrapper */` |
|     5 | 1037 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|     5 | 1038 | `		const char *zShellSuffix = "\"";` |
|     5 | 1039 | `		size_t nPrefix = strlen(zShellPrefix);` |
|     5 | 1040 | `		size_t nSuffix = strlen(zShellSuffix);` |
|     5 | 1041 | `		size_t nCmd = strlen(zCommand);` |
|     5 | 1042 | `		size_t nQuotes = 0;` |
|     5 | 1043 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|     5 | 1044 | `			if (zCommand[i] == '"') nQuotes++;` |
|     5 | 1045 | `		}` |
|     5 | 1046 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|     5 | 1047 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|     5 | 1048 | `		if (zCmdEsc == NULL) {` |
|   ! 0 | 1049 | `			return 0;` |
|     - | 1050 | `		}` |
|     - | 1051 | `		/* Escape quotes in command */` |
|     5 | 1052 | `		size_t j = 0;` |
|     5 | 1053 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|     5 | 1054 | `			char ch = zCommand[i];` |
|     5 | 1055 | `			if (ch == '"') {` |
|     5 | 1056 | `				zCmdEsc[j++] = '^';` |
|     5 | 1057 | `				zCmdEsc[j++] = '"';` |
|     5 | 1058 | `			} else {` |
|     5 | 1059 | `				zCmdEsc[j++] = ch;` |
|     - | 1060 | `			}` |
|     5 | 1061 | `		}` |
|     5 | 1062 | `		zCmdEsc[j] = '\0';` |
|     5 | 1063 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|     5 | 1064 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|     5 | 1065 | `		if (zWinCmd == NULL) {` |
|   ! 0 | 1066 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|   ! 0 | 1067 | `			return 0;` |
|     - | 1068 | `		}` |
|     5 | 1069 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|     5 | 1070 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|     5 | 1071 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|     5 | 1072 | `		zWinCmd[nTotal - 1] = '\0';` |
|     - | 1073 | `		/* Allocate pipe structure early so we can store handles */` |
|     5 | 1074 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|     5 | 1075 | `		if( pPipe == 0 ){` |
|   ! 0 | 1076 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|   ! 0 | 1077 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|   ! 0 | 1078 | `			return 0;` |
|     - | 1079 | `		}` |
|     - | 1080 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|     5 | 1081 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|     5 | 1082 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|     5 | 1083 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|     5 | 1084 | `		if( pFile == 0 ){` |
|   ! 0 | 1085 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   ! 0 | 1086 | `			return 0;` |
|     - | 1087 | `		}` |
|     - | 1088 | `		/* Initialize remaining fields */` |
|     5 | 1089 | `		pPipe->pFile = pFile;` |
|     5 | 1090 | `		pPipe->pVm = pVm;` |
|     5 | 1091 | `		pPipe->iMode = zMode[0];` |
|     - | 1092 | `	}` |
|     - | 1093 | `#elif defined(__UNIXES__) /* Unix */` |
|     - | 1094 | `	/* The mode goes to popen(3) VERBATIM, exactly as php hands it its own: a mode` |
|     - | 1095 | `	 * popen(3) refuses (anything but "r"/"w" — 'b' is a Windows translation flag` |
|     - | 1096 | `	 * with nothing to translate here) is an open FAILURE, which is php's answer` |
|     - | 1097 | `	 * for it too. */` |
|  8301 | 1098 | `	pFile = popen(zCommand, zMode);` |
|  8301 | 1099 | `	if( pFile == 0 ){` |
|     2 | 1100 | `		return 0;` |
|     - | 1101 | `	}` |
|     - | 1102 | `	/* Allocate pipe private structure */` |
|  8299 | 1103 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|  8299 | 1104 | `	if( pPipe == 0 ){` |
|     - | 1105 | `		/* Out of memory, close the pipe */` |
|   ! 0 | 1106 | `		pclose(pFile);` |
|   ! 0 | 1107 | `		return 0;` |
|     - | 1108 | `	}` |
|     - | 1109 | `	/* Initialize the structure */` |
|  8299 | 1110 | `	pPipe->pFile = pFile;` |
|  8299 | 1111 | `	pPipe->pVm = pVm;` |
|  8299 | 1112 | `	pPipe->iMode = zMode[0];` |
|     - | 1113 | `#else /* OS_OTHER: no process pipes on this platform */` |
|     - | 1114 | `	(void)pFile;` |
|     - | 1115 | `	return 0;` |
|     - | 1116 | `#endif` |
|  8304 | 1117 | `	return pPipe;` |
|  4150 | 1118 | `}` |
|     - | 1119 | `/*` |
|     - | 1120 | ` * Close a pipe and return the exit status of the process.` |
|     - | 1121 | ` * Returns the exit status, or -1 on error.` |
|     - | 1122 | ` */` |
|  8299 | 1123 | `static int PipeClose(pipe_private *pPipe)` |
|     5 | 1124 | `{` |
|     - | 1125 | `	int status;` |
|     - | 1126 | `	ph7_vm *pVm;` |
|  8304 | 1127 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1128 | `		return -1;` |
|     - | 1129 | `	}` |
|  8304 | 1130 | `	pVm = pPipe->pVm;` |
|     - | 1131 | `	/* Close the pipe and get exit status */` |
|     - | 1132 | `#ifdef __WINNT__` |
|     - | 1133 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|     5 | 1134 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|     - | 1135 | `#elif defined(__UNIXES__)` |
|  8299 | 1136 | `	status = pclose(pPipe->pFile);` |
|     - | 1137 | `	/* pclose() answers waitpid()'s raw status. php (php_stream_pclose) translates` |
|     - | 1138 | `	 * exactly ONE case of it — a normal exit becomes its exit CODE — and hands the` |
|     - | 1139 | `	 * raw word back for every other, so a process killed by a signal reports the` |
|     - | 1140 | ``	 * SIGNAL number: `kill -TERM $$` is 15, `kill -9 $$` is 9. PHL used to add the`` |
|     - | 1141 | `	 * shell's own 128 to it (143, 137), a number the same status word can also` |
|     - | 1142 | ``	 * mean as an ordinary `exit 143`, and answered -1 for a stopped child. This is`` |
|     - | 1143 | `	 * pclose()'s answer and, through it, exec()/system()/passthru()'s` |
|     - | 1144 | `	 * $result_code. */` |
|  8299 | 1145 | `	if( status != -1 && WIFEXITED(status) ){` |
|  8295 | 1146 | `		status = WEXITSTATUS(status);` |
|  4142 | 1147 | `	}` |
|     - | 1148 | `#else /* OS_OTHER: no process pipes on this platform */` |
|     - | 1149 | `	status = -1;` |
|     - | 1150 | `#endif` |
|     - | 1151 | `	/* Free the structure */` |
|  8304 | 1152 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|  8304 | 1153 | `	return status;` |
|  4149 | 1154 | `}` |
|     - | 1155 | `/*` |
|     - | 1156 | ` * Pipe stream xClose implementation.` |
|     - | 1157 | ` * Note: This is called by fclose(), not pclose().` |
|     - | 1158 | ` * It closes the pipe but does not return the exit status.` |
|     - | 1159 | ` */` |
|   160 | 1160 | `static void PipeStream_Close(void *pHandle)` |
|     4 | 1161 | `{` |
|   164 | 1162 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|   164 | 1163 | `	if( pPipe ){` |
|   164 | 1164 | `		PipeClose(pPipe);` |
|    80 | 1165 | `	}` |
|   164 | 1166 | `}` |
|     - | 1167 | `/*` |
|     - | 1168 | ` * Pipe stream xRead implementation.` |
|     - | 1169 | ` */` |
| 11838 | 1170 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|     4 | 1171 | `{` |
| 11842 | 1172 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     - | 1173 | `	size_t nRead;` |
| 11842 | 1174 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1175 | `		return -1;` |
|     - | 1176 | `	}` |
| 11842 | 1177 | `	if( pPipe->iMode != 'r' ){` |
|     - | 1178 | `		/* Cannot read from a write-only pipe */` |
|   ! 0 | 1179 | `		return -1;` |
|     - | 1180 | `	}` |
| 11842 | 1181 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
| 11842 | 1182 | `	if( nRead == 0 ){` |
|  7380 | 1183 | `		if( feof(pPipe->pFile) ){` |
|  7380 | 1184 | `			return 0; /* EOF */` |
|     - | 1185 | `		}` |
|   ! 0 | 1186 | `		return -1; /* Error */` |
|     - | 1187 | `	}` |
|  4466 | 1188 | `	return (ph7_int64)nRead;` |
|  5918 | 1189 | `}` |
|     - | 1190 | `/*` |
|     - | 1191 | ` * Pipe stream xWrite implementation.` |
|     - | 1192 | ` */` |
|     6 | 1193 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|     1 | 1194 | `{` |
|     7 | 1195 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     - | 1196 | `	size_t nWritten;` |
|     7 | 1197 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1198 | `		return -1;` |
|     - | 1199 | `	}` |
|     7 | 1200 | `	if( pPipe->iMode != 'w' ){` |
|     - | 1201 | `		/* Cannot write to a read-only pipe */` |
|   ! 0 | 1202 | `		return -1;` |
|     - | 1203 | `	}` |
|     7 | 1204 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|     7 | 1205 | `	if( nWritten == 0 && nWrite > 0 ){` |
|   ! 0 | 1206 | `		return -1; /* Error */` |
|     - | 1207 | `	}` |
|     7 | 1208 | `	return (ph7_int64)nWritten;` |
|     4 | 1209 | `}` |
|     - | 1210 | `/*` |
|     - | 1211 | ` * A pipe's descriptor, for the two ops that go past the FILE*. php's popen` |
|     - | 1212 | ` * stream is an ordinary stdio stream, so its truncate and its stat are the` |
|     - | 1213 | ` * plain system calls -- which is exactly why ftruncate() on a pipe is a SILENT` |
|     - | 1214 | ` * false (the call is made and EINVAL comes back) where a socket, which has no` |
|     - | 1215 | ` * truncate at all, warns instead.` |
|     - | 1216 | ` */` |
|   ! 0 | 1217 | `static int PipeStream_Fd(void *pHandle)` |
|   ! 0 | 1218 | `{` |
|   ! 0 | 1219 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|   ! 0 | 1220 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1221 | `		return -1;` |
|     - | 1222 | `	}` |
|     - | 1223 | `#ifdef __WINNT__` |
|   ! 0 | 1224 | `	return _fileno(pPipe->pFile);` |
|     - | 1225 | `#else` |
|   ! 0 | 1226 | `	return fileno(pPipe->pFile);` |
|     - | 1227 | `#endif` |
|   ! 0 | 1228 | `}` |
|     - | 1229 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|   ! 0 | 1230 | `static int PipeStream_Trunc(void *pHandle,ph7_int64 nLen)` |
|   ! 0 | 1231 | `{` |
|   ! 0 | 1232 | `	int fd = PipeStream_Fd(pHandle);` |
|   ! 0 | 1233 | `	if( fd < 0 ){` |
|   ! 0 | 1234 | `		return -1;` |
|     - | 1235 | `	}` |
|     - | 1236 | `#ifdef __WINNT__` |
|   ! 0 | 1237 | `	return _chsize_s(fd,(__int64)nLen) == 0 ? PH7_OK : -1;` |
|     - | 1238 | `#else` |
|   ! 0 | 1239 | `	return ftruncate(fd,(off_t)nLen) == 0 ? PH7_OK : -1;` |
|     - | 1240 | `#endif` |
|   ! 0 | 1241 | `}` |
|     - | 1242 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|   ! 0 | 1243 | `static int PipeStream_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|   ! 0 | 1244 | `{` |
|   ! 0 | 1245 | `	return PH7_VfsStatFromFd(PipeStream_Fd(pHandle),pArray,pWorker);` |
|   ! 0 | 1246 | `}` |
|     - | 1247 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|     - | 1248 | `static const ph7_io_stream sPipe_Stream = {` |
|     - | 1249 | `	"pipe",` |
|     - | 1250 | `	PH7_IO_STREAM_VERSION,` |
|     - | 1251 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|     - | 1252 | `	0,  /* xOpenDir */` |
|     - | 1253 | `	PipeStream_Close,  /* xClose */` |
|     - | 1254 | `	0,  /* xCloseDir */` |
|     - | 1255 | `	PipeStream_Read,   /* xRead */` |
|     - | 1256 | `	0,  /* xReadDir */` |
|     - | 1257 | `	PipeStream_Write,  /* xWrite */` |
|     - | 1258 | `	0,  /* xSeek */` |
|     - | 1259 | `	0,  /* xLock */` |
|     - | 1260 | `	0,  /* xRewindDir */` |
|     - | 1261 | `	0,  /* xTell: php's pipe has none either -- the stream layer COUNTS */` |
|     - | 1262 | `	PipeStream_Trunc, /* xTrunc */` |
|     - | 1263 | `	0,  /* xSync */` |
|     - | 1264 | `	PipeStream_Stat   /* xStat */` |
|     - | 1265 | `};` |
|     - | 1266 | `/*` |
|     - | 1267 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|     - | 1268 | ` * FALSE otherwise.` |
|     - | 1269 | ` */` |
|  7352 | 1270 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|     5 | 1271 | `{` |
|  7357 | 1272 | `	return pStream == &sPipe_Stream;` |
|     5 | 1273 | `}` |
|     - | 1274 | `/*` |
|     - | 1275 | ` * resource popen(string $command, string $mode)` |
|     - | 1276 | ` *  Opens process file pointer.` |
|     - | 1277 | ` * Parameters` |
|     - | 1278 | ` *  $command` |
|     - | 1279 | ` *   The command to execute. Passed to the system shell.` |
|     - | 1280 | ` *  $mode` |
|     - | 1281 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|     - | 1282 | ` *   'r' - Open for reading (read from the command's stdout).` |
|     - | 1283 | ` *   'w' - Open for writing (write to the command's stdin).` |
|     - | 1284 | ` * Return` |
|     - | 1285 | ` *  Returns a file pointer on success, or FALSE on error.` |
|     - | 1286 | ` */` |
|     - | 1287 | `/*` |
|     - | 1288 | `` * The longest command line the platform's shell accepts — php's `cmd_max_len`,`` |
|     - | 1289 | ` * which both escapers refuse to exceed. php reads it once at startup from` |
|     - | 1290 | ` * sysconf(_SC_ARG_MAX) and hardcodes cmd.exe's constant on Windows.` |
|     - | 1291 | ` */` |
|  1860 | 1292 | `static sxu32 ShellMaxCmdLen(void)` |
|     5 | 1293 | `{` |
|     - | 1294 | `#ifdef __WINNT__` |
|     - | 1295 | `	/* An escaped command runs through cmd.exe, whose limit is a constant. */` |
|     5 | 1296 | `	return 8192;` |
|     - | 1297 | `#elif defined(__UNIXES__) && defined(_SC_ARG_MAX)` |
|  1860 | 1298 | `	long iMax = sysconf(_SC_ARG_MAX);` |
|  1860 | 1299 | `	if( iMax <= 0 ){` |
|   ! 0 | 1300 | `		return 4096;   /* php's _POSIX_ARG_MAX fallback */` |
|     - | 1301 | `	}` |
|  1860 | 1302 | `	return (sxu32)iMax;` |
|     - | 1303 | `#else` |
|     - | 1304 | `	return 4096;` |
|     - | 1305 | `#endif` |
|   934 | 1306 | `}` |
|     - | 1307 | `/*` |
|     - | 1308 | ` * How the two escapers WALK their argument, and the one thing they share.` |
|     - | 1309 | ` *` |
|     - | 1310 | ` * php walks it with php_mblen(), the process LC_CTYPE's multibyte reader: a` |
|     - | 1311 | ` * well-formed sequence is copied through untouched (a metacharacter's byte value` |
|     - | 1312 | ` * inside one is NOT a metacharacter), and a byte the encoding cannot start a` |
|     - | 1313 | ` * character with is DROPPED. That reader's answer is platform-shaped, and this` |
|     - | 1314 | ` * follows it on both, because it is what php answers on each:` |
|     - | 1315 | ` *` |
|     - | 1316 | ` *   POSIX    php picks LC_CTYPE up from the environment at startup, so the` |
|     - | 1317 | ` *            everyday answer is a UTF-8 one — a well-formed sequence rides` |
|     - | 1318 | ` *            through and an ill-formed byte is dropped. PHL is UTF-8-only (the scope policy)` |
|     - | 1319 | ` *            and has no setlocale, so PH7_Utf8ReadStrict IS that reader.` |
|     - | 1320 | ` *            (php in the "C" locale glibc falls back to drops every byte >= 0x80` |
|     - | 1321 | ` *            instead, which is why the corpus guards this half on the oracle's` |
|     - | 1322 | ` *            own LC_CTYPE rather than pinning it unconditionally.)` |
|     - | 1323 | ` *   Windows  php reports LC_CTYPE "C", and MSVCRT's C locale is SINGLE-BYTE, not` |
|     - | 1324 | ` *            ASCII: mblen() answers 1 for every byte, so nothing is ever dropped` |
|     - | 1325 | `` *            and `\xFF` reaches the escape table below. Verified against php`` |
|     - | 1326 | ` *            8.5.8 on the gate VM, which does have an oracle — walking UTF-8` |
|     - | 1327 | ` *            there instead deleted bytes php keeps.` |
|     - | 1328 | ` *` |
|     - | 1329 | ` * Neither escaper can see a NUL byte: php parses both parameters with` |
|     - | 1330 | ` * Z_PARAM_PATH and the central screen (VmBuiltinPathMask) refuses one first.` |
|     - | 1331 | ` */` |
| 89605 | 1332 | `static int ShellCharIsWellFormed(const unsigned char *zIn,sxu32 nLeft,sxu32 *pnSeq)` |
|     5 | 1333 | `{` |
|     - | 1334 | `#ifdef __WINNT__` |
|     - | 1335 | `	SXUNUSED(zIn);` |
|     - | 1336 | `	SXUNUSED(nLeft);` |
|     5 | 1337 | `	*pnSeq = 1;` |
|     5 | 1338 | `	return 1;` |
|     - | 1339 | `#else` |
| 89605 | 1340 | `	return PH7_Utf8ReadStrict(zIn,nLeft,pnSeq) >= 0;` |
|     - | 1341 | `#endif` |
|     5 | 1342 | `}` |
|     - | 1343 | `/*` |
|     - | 1344 | ` * php's escapeshellarg(): wrap the whole argument in quotes the shell does not` |
|     - | 1345 | ` * look inside, and neutralise the one byte that could end them.` |
|     - | 1346 | ` *` |
|     - | 1347 | `` * POSIX: single quotes, and a `'` becomes `'\''` — close, escape, reopen.`` |
|     - | 1348 | `` * Windows: double quotes; there is no in-quote escape for `"` on cmd.exe, so php`` |
|     - | 1349 | `` * REPLACES `"` (and `%`/`!`, which cmd.exe still expands inside quotes) with a`` |
|     - | 1350 | ` * space, and doubles a trailing ODD run of backslashes so the last one escapes` |
|     - | 1351 | ` * itself rather than the closing quote.` |
|     - | 1352 | ` */` |
|  1794 | 1353 | `static void ShellEscapeArg(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|     5 | 1354 | `{` |
|  1799 | 1355 | `	sxu32 i = 0;` |
|     - | 1356 | `#ifdef __WINNT__` |
|     5 | 1357 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|     - | 1358 | `#else` |
|  1794 | 1359 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|     - | 1360 | `#endif` |
| 91024 | 1361 | `	while( i < nLen ){` |
| 89230 | 1362 | `		sxu32 nSeq = 1;` |
| 89230 | 1363 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|     - | 1364 | `			/* Ill-formed: php skips the byte rather than escaping it */` |
|    18 | 1365 | `			i += nSeq;` |
|    22 | 1366 | `			continue;` |
|     - | 1367 | `		}` |
| 89212 | 1368 | `		if( nSeq > 1 ){` |
|     8 | 1369 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|     8 | 1370 | `			i += nSeq;` |
|     8 | 1371 | `			continue;` |
|     - | 1372 | `		}` |
|     - | 1373 | `#ifdef __WINNT__` |
|     5 | 1374 | `		if( zIn[i] == '"' \|\| zIn[i] == '%' \|\| zIn[i] == '!' ){` |
|     1 | 1375 | `			SyBlobAppend(pOut," ",sizeof(char));` |
|     1 | 1376 | `		}else{` |
|     5 | 1377 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|     - | 1378 | `		}` |
|     - | 1379 | `#else` |
| 89199 | 1380 | `		if( zIn[i] == '\'' ){` |
|    12 | 1381 | `			SyBlobAppend(pOut,"'\\'",sizeof("'\\'")-1);` |
|     6 | 1382 | `		}` |
| 89199 | 1383 | `		SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|     - | 1384 | `#endif` |
| 89204 | 1385 | `		i++;` |
|     5 | 1386 | `	}` |
|     - | 1387 | `#ifdef __WINNT__` |
|     - | 1388 | `	{` |
|     - | 1389 | `		/* A trailing run of backslashes would escape the closing quote if it is` |
|     - | 1390 | `		 * odd; the opening quote at offset 0 stops the scan the way php's does. */` |
|     5 | 1391 | `		const char *zCur = (const char *)SyBlobData(pOut);` |
|     5 | 1392 | `		sxu32 nCur = SyBlobLength(pOut);` |
|     5 | 1393 | `		sxu32 k = 0;` |
|     5 | 1394 | `		while( k < nCur && zCur[nCur - 1 - k] == '\\' ){` |
|     1 | 1395 | `			k++;` |
|     1 | 1396 | `		}` |
|     5 | 1397 | `		if( (k & 1) != 0 ){` |
|     1 | 1398 | `			SyBlobAppend(pOut,"\\",sizeof(char));` |
|     - | 1399 | `		}` |
|     - | 1400 | `	}` |
|     5 | 1401 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|     - | 1402 | `#else` |
|  1794 | 1403 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|     - | 1404 | `#endif` |
|  1799 | 1405 | `}` |
|     - | 1406 | `/*` |
|     - | 1407 | ` * php's escapeshellcmd(): the argument is a COMMAND, so it is not quoted at all —` |
|     - | 1408 | ` * every byte that could break out of one is prefixed with the shell's escape` |
|     - | 1409 | `` * character instead (`\` on POSIX, `^` on cmd.exe).`` |
|     - | 1410 | ` *` |
|     - | 1411 | ` * The one shape that is not a straight escape is a quote on POSIX: php leaves a` |
|     - | 1412 | ` * PAIR of them alone (the command may legitimately quote one of its own` |
|     - | 1413 | `` * arguments) and escapes an unpaired one. `pPair` is php's own one-slot state for`` |
|     - | 1414 | ` * that — it remembers the partner it found for the quote currently open, so the` |
|     - | 1415 | ` * closing one is recognised and the pairing resets. cmd.exe has no such rule, so` |
|     - | 1416 | `` * both quote characters (and `%`/`!`) are ordinary escapes there.`` |
|     - | 1417 | ` */` |
|    64 | 1418 | `static void ShellEscapeCmd(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|     1 | 1419 | `{` |
|    65 | 1420 | `	sxu32 i = 0;` |
|     - | 1421 | `#ifndef __WINNT__` |
|    64 | 1422 | `	const unsigned char *pPair = 0;` |
|     - | 1423 | `	static const char zEsc[] = "\\";` |
|     - | 1424 | `#else` |
|     - | 1425 | `	static const char zEsc[] = "^";` |
|     - | 1426 | `#endif` |
|   445 | 1427 | `	while( i < nLen ){` |
|   381 | 1428 | `		sxu32 nSeq = 1;` |
|   381 | 1429 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|    18 | 1430 | `			i += nSeq;` |
|    22 | 1431 | `			continue;` |
|     - | 1432 | `		}` |
|   363 | 1433 | `		if( nSeq > 1 ){` |
|     8 | 1434 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|     8 | 1435 | `			i += nSeq;` |
|     8 | 1436 | `			continue;` |
|     - | 1437 | `		}` |
|   355 | 1438 | `		switch( zIn[i] ){` |
|     - | 1439 | `#ifndef __WINNT__` |
|    12 | 1440 | `		case '"':` |
|     - | 1441 | `		case '\'':` |
|    19 | 1442 | `			if( pPair == 0` |
|    14 | 1443 | `			 && (pPair = (const unsigned char *)memchr(&zIn[i+1],zIn[i],nLen - i - 1)) != 0 ){` |
|     - | 1444 | `				/* This quote opens a pair: leave both of them alone */` |
|    17 | 1445 | `			}else if( pPair != 0 && pPair[0] == zIn[i] ){` |
|     8 | 1446 | `				pPair = 0;   /* the partner: pairing satisfied */` |
|     4 | 1447 | `			}else{` |
|     8 | 1448 | `				SyBlobAppend(pOut,zEsc,sizeof(char));` |
|     - | 1449 | `			}` |
|    24 | 1450 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|    24 | 1451 | `			break;` |
|     - | 1452 | `#else` |
|     - | 1453 | `		/* cmd.exe expands %VAR% and !VAR! even inside quotes, and has no` |
|     - | 1454 | ``		 * in-quote escape, so all four are plain `^` escapes there. */`` |
|     - | 1455 | `		case '%':` |
|     - | 1456 | `		case '!':` |
|     - | 1457 | `		case '"':` |
|     - | 1458 | `		case '\'':` |
|     - | 1459 | `#endif` |
|    21 | 1460 | `		case '#':` |
|     - | 1461 | `		case '&':` |
|     - | 1462 | `		case ';':` |
|     - | 1463 | ``		case '`':`` |
|     - | 1464 | `		case '\|':` |
|     - | 1465 | `		case '*':` |
|     - | 1466 | `		case '?':` |
|     - | 1467 | `		case '~':` |
|     - | 1468 | `		case '<':` |
|     - | 1469 | `		case '>':` |
|     - | 1470 | `		case '^':` |
|     - | 1471 | `		case '(':` |
|     - | 1472 | `		case ')':` |
|     - | 1473 | `		case '[':` |
|     - | 1474 | `		case ']':` |
|     - | 1475 | `		case '{':` |
|     - | 1476 | `		case '}':` |
|     - | 1477 | `		case '$':` |
|     - | 1478 | `		case '\\':` |
|     - | 1479 | `		case 0x0A:` |
|     - | 1480 | `		/* php escapes 0xFF too, and this is the row that decides the walk above` |
|     - | 1481 | `		 * is worth getting right: it is unreachable under the UTF-8 walk (0xF5..` |
|     - | 1482 | `		 * 0xFF is never a lead byte, so the reader drops the byte first) and` |
|     - | 1483 | `		 * REACHED on Windows, where php's single-byte C locale hands it here —` |
|     - | 1484 | ``		 * `escapeshellcmd("a\xffb")` is `a^\xffb` on the oracle. */`` |
|     - | 1485 | `		case 0xFF:` |
|    43 | 1486 | `			SyBlobAppend(pOut,zEsc,sizeof(char));` |
|     - | 1487 | `			/* fall through */` |
|   165 | 1488 | `		default:` |
|   331 | 1489 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|   330 | 1490 | `			break;` |
|     - | 1491 | `		}` |
|   355 | 1492 | `		i++;` |
|     1 | 1493 | `	}` |
|    65 | 1494 | `}` |
|     - | 1495 | `/*` |
|     - | 1496 | ` * string escapeshellarg(string $arg)` |
|     - | 1497 | ` *  Escape an argument so a shell passes it to the command as ONE word, whatever` |
|     - | 1498 | ` *  it contains.` |
|     - | 1499 | ` */` |
|  1794 | 1500 | `PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1501 | `{` |
|  1799 | 1502 | `	sxu32 nMax = ShellMaxCmdLen();` |
|     - | 1503 | `	const char *zArg;` |
|     - | 1504 | `	SyBlob sOut;` |
|     - | 1505 | `	int nLen;` |
|   896 | 1506 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|  1799 | 1507 | `	zArg = ph7_value_to_string(apArg[0],&nLen);` |
|     - | 1508 | `	/* php's own bound: the command line has to hold the two quotes and a NUL */` |
|  1799 | 1509 | `	if( nLen > 0 && (sxu32)nLen > nMax - 3 ){` |
|   ! 0 | 1510 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1511 | `			"Argument exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1512 | `	}` |
|  1799 | 1513 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  1799 | 1514 | `	ShellEscapeArg((const unsigned char *)zArg,(sxu32)nLen,&sOut);` |
|  1799 | 1515 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|   ! 0 | 1516 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1517 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1518 | `			"Escaped argument exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1519 | `	}` |
|  1799 | 1520 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|  1799 | 1521 | `	SyBlobRelease(&sOut);` |
|  1799 | 1522 | `	return PH7_OK;` |
|   901 | 1523 | `}` |
|     - | 1524 | `/*` |
|     - | 1525 | ` * string escapeshellcmd(string $command)` |
|     - | 1526 | ` *  Escape every character that could break out of a shell command.` |
|     - | 1527 | ` */` |
|    66 | 1528 | `PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1529 | `{` |
|    67 | 1530 | `	sxu32 nMax = ShellMaxCmdLen();` |
|     - | 1531 | `	const char *zCmd;` |
|     - | 1532 | `	SyBlob sOut;` |
|     - | 1533 | `	int nLen;` |
|    33 | 1534 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|    67 | 1535 | `	zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|    67 | 1536 | `	if( nLen < 1 ){` |
|     - | 1537 | `		/* php answers "" without running the escaper at all */` |
|     3 | 1538 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 1539 | `		return PH7_OK;` |
|     - | 1540 | `	}` |
|    65 | 1541 | `	if( (sxu32)nLen > nMax - 3 ){` |
|   ! 0 | 1542 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1543 | `			"Command exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1544 | `	}` |
|    65 | 1545 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    65 | 1546 | `	ShellEscapeCmd((const unsigned char *)zCmd,(sxu32)nLen,&sOut);` |
|    65 | 1547 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|   ! 0 | 1548 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1549 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1550 | `			"Escaped command exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1551 | `	}` |
|    65 | 1552 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    65 | 1553 | `	SyBlobRelease(&sOut);` |
|    65 | 1554 | `	return PH7_OK;` |
|    34 | 1555 | `}` |
|     - | 1556 | `/*` |
|     - | 1557 | ` * php refuses an EMPTY command in all four runners (exec/system/passthru/` |
|     - | 1558 | ` * shell_exec) — the shell would answer success for one, so the refusal is the` |
|     - | 1559 | ` * only way a script hears about a command string that came out empty.` |
|     - | 1560 | ` */` |
|     8 | 1561 | `static sxi32 ShellEmptyCommandError(ph7_context *pCtx)` |
|     1 | 1562 | `{` |
|    13 | 1563 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     4 | 1564 | `		"%s(): Argument #1 ($command) must not be empty",ph7_function_name(pCtx));` |
|     1 | 1565 | `}` |
|     - | 1566 | `/*` |
|     - | 1567 | ` * string\|false\|null shell_exec(string $command)` |
|     - | 1568 | ` *  Execute a command via the shell and return the complete output as a string.` |
|     - | 1569 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|     - | 1570 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|     - | 1571 | ` */` |
|   530 | 1572 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1573 | `{` |
|     - | 1574 | `	const char *zCommand;` |
|     - | 1575 | `	pipe_private *pPipe;` |
|     - | 1576 | `	SyBlob sOut;` |
|     - | 1577 | `	char zBuf[4096];` |
|     - | 1578 | `	size_t nRead;` |
|     - | 1579 | `	int nCmdLen;` |
|   535 | 1580 | `	if( nArg < 1 ){` |
|   ! 0 | 1581 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1582 | `		return PH7_OK;` |
|     - | 1583 | `	}` |
|   535 | 1584 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|   535 | 1585 | `	if( nCmdLen < 1 ){` |
|     - | 1586 | `		/* php refuses an empty command rather than running the shell on it */` |
|     3 | 1587 | `		return ShellEmptyCommandError(pCtx);` |
|     - | 1588 | `	}` |
|   533 | 1589 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|   533 | 1590 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|     - | 1591 | `		/* php's own wording for this one; the three runners below say "Unable to` |
|     - | 1592 | `		 * fork [%s]" instead. Both used to be silent. */` |
|   ! 0 | 1593 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to execute '%s'",` |
|   ! 0 | 1594 | `			ph7_function_name(pCtx),zCommand);` |
|   ! 0 | 1595 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1596 | `		return PH7_OK;` |
|     - | 1597 | `	}` |
|   533 | 1598 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   489 | 1599 | `	for(;;){` |
|   983 | 1600 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|   983 | 1601 | `		if( nRead < 1 ){` |
|   533 | 1602 | `			break;` |
|     - | 1603 | `		}` |
|   455 | 1604 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|     5 | 1605 | `	}` |
|   533 | 1606 | `	PipeClose(pPipe);` |
|   533 | 1607 | `	if( SyBlobLength(&sOut) < 1 ){` |
|     - | 1608 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    81 | 1609 | `		ph7_result_null(pCtx);` |
|    42 | 1610 | `	}else{` |
|   455 | 1611 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     - | 1612 | `	}` |
|   533 | 1613 | `	SyBlobRelease(&sOut);` |
|   533 | 1614 | `	return PH7_OK;` |
|   270 | 1615 | `}` |
|     - | 1616 | `/*` |
|     - | 1617 | ` * php's three command RUNNERS are one routine (php_exec) with a mode, and the` |
|     - | 1618 | ` * mode decides two things: what happens to each LINE of the command's output,` |
|     - | 1619 | ` * and what the call answers.` |
|     - | 1620 | ` *` |
|     - | 1621 | ` *   exec($cmd)           keep nothing, answer the LAST line` |
|     - | 1622 | ` *   exec($cmd, $output)  append every line to the array, answer the last line` |
|     - | 1623 | ` *   system($cmd)         WRITE every line as it arrives, answer the last line` |
|     - | 1624 | ` *   passthru($cmd)       write the raw bytes, answer NULL` |
|     - | 1625 | ` *` |
|     - | 1626 | ` * "The last line" is php's: its trailing WHITESPACE is stripped — spaces and` |
|     - | 1627 | ` * tabs as much as the newline — and so is every element of $output's. A command` |
|     - | 1628 | ` * that printed nothing answers "" rather than false, which is php's documented` |
|     - | 1629 | ` * BC wart and not an error indication; the error indication is FALSE, and only` |
|     - | 1630 | ` * a pipe that could not be opened produces it.` |
|     - | 1631 | ` *` |
|     - | 1632 | ` * All three share the exit status, which is the pipe's close status (php's` |
|     - | 1633 | ` * $result_code out-param) and -1 when there was no process at all.` |
|     - | 1634 | ` */` |
|     - | 1635 | `#ifdef __WINNT__` |
|     - | 1636 | `# define SHELL_RUN_PIPE_MODE "rb"` |
|     - | 1637 | `#else` |
|     - | 1638 | `# define SHELL_RUN_PIPE_MODE "r"` |
|     - | 1639 | `#endif` |
|     - | 1640 | `#define SHELL_RUN_LAST     0   /* exec() with no $output array */` |
|     - | 1641 | `#define SHELL_RUN_ECHO     1   /* system() */` |
|     - | 1642 | `#define SHELL_RUN_COLLECT  2   /* exec() with one */` |
|     - | 1643 | `#define SHELL_RUN_RAW      3   /* passthru() */` |
|     - | 1644 | `/*` |
|     - | 1645 | ` * php's strip_trailing_whitespace(): answers the length that stays.` |
|     - | 1646 | ` */` |
|   741 | 1647 | `static sxu32 ShellStripTrailing(const char *zLine,sxu32 nLine)` |
|     4 | 1648 | `{` |
|  1485 | 1649 | `	while( nLine > 0 && SyisSpace((unsigned char)zLine[nLine - 1]) ){` |
|   744 | 1650 | `		nLine--;` |
|     4 | 1651 | `	}` |
|   745 | 1652 | `	return nLine;` |
|     4 | 1653 | `}` |
|     - | 1654 | `/*` |
|     - | 1655 | ` * One complete line of output, dealt with the mode's way. The line still carries` |
|     - | 1656 | ` * its own newline: system() writes it (php hands the whole line to the output` |
|     - | 1657 | ` * layer, so an output buffer catches it like any echo), and the collector strips` |
|     - | 1658 | ` * it along with the rest of the trailing whitespace.` |
|     - | 1659 | ` */` |
|   498 | 1660 | `static sxi32 ShellHandleLine(ph7_context *pCtx,int iType,ph7_value *pArray,` |
|     - | 1661 | `	const char *zLine,sxu32 nLine)` |
|     4 | 1662 | `{` |
|   502 | 1663 | `	if( iType == SHELL_RUN_ECHO ){` |
|    12 | 1664 | `		return ph7_context_output(pCtx,zLine,(int)nLine);` |
|     - | 1665 | `	}` |
|   491 | 1666 | `	if( iType == SHELL_RUN_COLLECT && pArray ){` |
|   491 | 1667 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   491 | 1668 | `		if( pVal == 0 ){` |
|   ! 0 | 1669 | `			return PH7_OK;` |
|     - | 1670 | `		}` |
|   491 | 1671 | `		ph7_value_string(pVal,zLine,(int)ShellStripTrailing(zLine,nLine));` |
|   491 | 1672 | `		ph7_array_add_elem(pArray,0,pVal);` |
|   491 | 1673 | `		ph7_context_release_value(pCtx,pVal);` |
|   244 | 1674 | `	}` |
|   491 | 1675 | `	return PH7_OK;` |
|   253 | 1676 | `}` |
|     - | 1677 | `/*` |
|     - | 1678 | ` * Run $command through the shell in the given mode, fill the by-reference` |
|     - | 1679 | ` * out-params and set the call's result. The three builtins below are this` |
|     - | 1680 | ` * routine plus their own mode.` |
|     - | 1681 | ` */` |
|   265 | 1682 | `static sxi32 ShellRunCommand(ph7_context *pCtx,int iType,int nArg,ph7_value **apArg)` |
|     4 | 1683 | `{` |
|     - | 1684 | `	/* exec() carries $output before $result_code; the other two do not */` |
|   269 | 1685 | `	int iCodeArg = (iType == SHELL_RUN_LAST) ? 2 : 1;` |
|   269 | 1686 | `	ph7_value *pArray = 0, *pOwned = 0;` |
|     - | 1687 | `	const char *zCommand;` |
|     - | 1688 | `	pipe_private *pPipe;` |
|     - | 1689 | `	SyBlob sLine, sLast;` |
|     - | 1690 | `	char zBuf[4096];` |
|     - | 1691 | `	size_t nRead;` |
|   269 | 1692 | `	int nCmdLen, iStatus = -1;` |
|   269 | 1693 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|   269 | 1694 | `	if( nCmdLen < 1 ){` |
|     7 | 1695 | `		return ShellEmptyCommandError(pCtx);` |
|     - | 1696 | `	}` |
|     - | 1697 | `	/* $output turns exec() into the collecting mode. php uses the array the` |
|     - | 1698 | `	 * caller already holds — the manual's "will append to the end of the array" —` |
|     - | 1699 | `	 * and replaces anything else with a fresh one, BEFORE running the command, so` |
|     - | 1700 | `	 * even a failed run leaves the variable an array. */` |
|   263 | 1701 | `	if( iType == SHELL_RUN_LAST && nArg > 1 ){` |
|   248 | 1702 | `		iType = SHELL_RUN_COLLECT;` |
|   248 | 1703 | `		if( ph7_value_is_array(apArg[1]) ){` |
|   223 | 1704 | `			PH7_HashmapCowSeparate(pCtx->pVm,apArg[1]);` |
|   223 | 1705 | `			pArray = apArg[1];` |
|   113 | 1706 | `		}else{` |
|    26 | 1707 | `			pOwned = pArray = ph7_context_new_array(pCtx);` |
|     - | 1708 | `		}` |
|   122 | 1709 | `	}` |
|     - | 1710 | `	/* php_exec's own mode, per platform: the three runners hand back what the` |
|     - | 1711 | `	 * command WROTE, so on Windows the CRs have to survive the pipe (where` |
|     - | 1712 | `	 * shell_exec() takes php's "rt" and does translate them). popen(3) refuses a` |
|     - | 1713 | `	 * 'b' it has nothing to translate, which is why this is not one string. */` |
|   263 | 1714 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,SHELL_RUN_PIPE_MODE);` |
|   263 | 1715 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1716 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to fork [%s]",` |
|   ! 0 | 1717 | `			ph7_function_name(pCtx),zCommand);` |
|   ! 0 | 1718 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1719 | `	}else{` |
|   263 | 1720 | `		int bAbort = 0;   /* the output consumer asked to stop (PH7_ABORT) */` |
|   263 | 1721 | `		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|   263 | 1722 | `		SyBlobInit(&sLast,&pCtx->pVm->sAllocator);` |
|   252 | 1723 | `		for(;;){` |
|   515 | 1724 | `			nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|   515 | 1725 | `			if( nRead < 1 ){` |
|   263 | 1726 | `				break;` |
|     - | 1727 | `			}` |
|   256 | 1728 | `			if( iType == SHELL_RUN_RAW ){` |
|     - | 1729 | `				/* passthru() never looks for a line: php writes what it read */` |
|     8 | 1730 | `				if( ph7_context_output(pCtx,zBuf,(int)nRead) == PH7_ABORT ){` |
|   ! 0 | 1731 | `					break;` |
|     - | 1732 | `				}` |
|     8 | 1733 | `				continue;` |
|     - | 1734 | `			}` |
|     - | 1735 | `			{` |
|   250 | 1736 | `				size_t iOfft = 0;` |
|   736 | 1737 | `				while( iOfft < nRead ){` |
|   506 | 1738 | `					const char *zNl = (const char *)memchr(&zBuf[iOfft],'\n',nRead - iOfft);` |
|   506 | 1739 | `					size_t nChunk = zNl ? (size_t)(zNl - &zBuf[iOfft]) + 1 : nRead - iOfft;` |
|   506 | 1740 | `					SyBlobAppend(&sLine,&zBuf[iOfft],(sxu32)nChunk);` |
|   506 | 1741 | `					iOfft += nChunk;` |
|   506 | 1742 | `					if( zNl == 0 ){` |
|    17 | 1743 | `						break;   /* the line continues in the next read */` |
|     - | 1744 | `					}` |
|   729 | 1745 | `					if( ShellHandleLine(pCtx,iType,pArray,` |
|   733 | 1746 | `						(const char *)SyBlobData(&sLine),SyBlobLength(&sLine)) == PH7_ABORT ){` |
|   ! 0 | 1747 | `						bAbort = 1;` |
|   ! 0 | 1748 | `					}` |
|     - | 1749 | `					/* Keep it: the call answers the last line it saw */` |
|   490 | 1750 | `					SyBlobReset(&sLast);` |
|   490 | 1751 | `					SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|   490 | 1752 | `					SyBlobReset(&sLine);` |
|   490 | 1753 | `					if( bAbort ){` |
|   ! 0 | 1754 | `						break;` |
|     - | 1755 | `					}` |
|     4 | 1756 | `				}` |
|     - | 1757 | `			}` |
|   250 | 1758 | `			if( bAbort ){` |
|   ! 0 | 1759 | `				break;` |
|     - | 1760 | `			}` |
|     4 | 1761 | `		}` |
|     - | 1762 | `		/* Output that ended without a newline is still a line */` |
|   263 | 1763 | `		if( !bAbort && SyBlobLength(&sLine) > 0 ){` |
|    19 | 1764 | `			ShellHandleLine(pCtx,iType,pArray,` |
|    12 | 1765 | `				(const char *)SyBlobData(&sLine),SyBlobLength(&sLine));` |
|    13 | 1766 | `			SyBlobReset(&sLast);` |
|    13 | 1767 | `			SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|     6 | 1768 | `		}` |
|   263 | 1769 | `		iStatus = PipeClose(pPipe);` |
|   263 | 1770 | `		if( iType == SHELL_RUN_RAW ){` |
|     8 | 1771 | `			ph7_result_null(pCtx);` |
|     5 | 1772 | `		}else{` |
|   383 | 1773 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&sLast),` |
|   253 | 1774 | `				(int)ShellStripTrailing((const char *)SyBlobData(&sLast),SyBlobLength(&sLast)));` |
|     - | 1775 | `		}` |
|   263 | 1776 | `		SyBlobRelease(&sLine);` |
|   263 | 1777 | `		SyBlobRelease(&sLast);` |
|     - | 1778 | `	}` |
|   263 | 1779 | `	if( pOwned ){` |
|    26 | 1780 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOwned);` |
|    26 | 1781 | `		ph7_context_release_value(pCtx,pOwned);` |
|    12 | 1782 | `	}` |
|   208 | 1783 | `	if( nArg > iCodeArg ){` |
|     - | 1784 | `		ph7_value sVal;` |
|   151 | 1785 | `		PH7_MemObjInitFromInt(pCtx->pVm,&sVal,iStatus);` |
|   151 | 1786 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iCodeArg],&sVal);` |
|   151 | 1787 | `		PH7_MemObjRelease(&sVal);` |
|    74 | 1788 | `	}` |
|   263 | 1789 | `	return PH7_OK;` |
|   136 | 1790 | `}` |
|     - | 1791 | `/*` |
|     - | 1792 | ` * string\|false exec(string $command, array &$output = null, int &$result_code = null)` |
|     - | 1793 | ` *  Run a command and answer the last line of its output.` |
|     - | 1794 | ` */` |
|   249 | 1795 | `PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1796 | `{` |
|   252 | 1797 | `	return ShellRunCommand(pCtx,SHELL_RUN_LAST,nArg,apArg);` |
|     3 | 1798 | `}` |
|     - | 1799 | `/*` |
|     - | 1800 | ` * string\|false system(string $command, int &$result_code = null)` |
|     - | 1801 | ` *  Run a command, write its output as it arrives, answer the last line.` |
|     - | 1802 | ` */` |
|     8 | 1803 | `PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1804 | `{` |
|    10 | 1805 | `	return ShellRunCommand(pCtx,SHELL_RUN_ECHO,nArg,apArg);` |
|     2 | 1806 | `}` |
|     - | 1807 | `/*` |
|     - | 1808 | ` * ?false passthru(string $command, int &$result_code = null)` |
|     - | 1809 | ` *  Run a command and write its output through, byte for byte.` |
|     - | 1810 | ` */` |
|     8 | 1811 | `PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1812 | `{` |
|    10 | 1813 | `	return ShellRunCommand(pCtx,SHELL_RUN_RAW,nArg,apArg);` |
|     2 | 1814 | `}` |
|     - | 1815 | `/*` |
|     - | 1816 | ` * bool proc_nice(int $priority)` |
|     - | 1817 | ` *  Change the priority of the running process — the last member of php's own` |
|     - | 1818 | ` *  process-execution surface, and the only one of the seven that runs no shell.` |
|     - | 1819 | ` */` |
|     6 | 1820 | `PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1821 | `{` |
|     - | 1822 | `	ph7_int64 iPri;` |
|     3 | 1823 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|     7 | 1824 | `	iPri = ph7_value_to_int64(apArg[0]);` |
|     - | 1825 | `#ifdef __WINNT__` |
|     - | 1826 | `	{` |
|     - | 1827 | `		/* php's own mapping (win32/nice.c): cmd.exe has no nice value, so the` |
|     - | 1828 | `		 * POSIX increment is bucketed into the five priority CLASSES Windows` |
|     - | 1829 | `		 * has. REALTIME is deliberately not reachable there, and neither is it` |
|     - | 1830 | `		 * here. */` |
|     1 | 1831 | `		DWORD dwFlag = NORMAL_PRIORITY_CLASS;` |
|     1 | 1832 | `		if( iPri < -9 ){` |
|   ! 0 | 1833 | `			dwFlag = HIGH_PRIORITY_CLASS;` |
|     1 | 1834 | `		}else if( iPri < -4 ){` |
|   ! 0 | 1835 | `			dwFlag = ABOVE_NORMAL_PRIORITY_CLASS;` |
|     1 | 1836 | `		}else if( iPri > 9 ){` |
|   ! 0 | 1837 | `			dwFlag = IDLE_PRIORITY_CLASS;` |
|     1 | 1838 | `		}else if( iPri > 4 ){` |
|   ! 0 | 1839 | `			dwFlag = BELOW_NORMAL_PRIORITY_CLASS;` |
|     - | 1840 | `		}` |
|     1 | 1841 | `		SetPriorityClass(GetCurrentProcess(),dwFlag);` |
|     - | 1842 | `		/* php answers TRUE whatever that returned: its nice() reports failure` |
|     - | 1843 | `		 * through its return value and leaves errno alone, and proc_nice() reads` |
|     - | 1844 | `		 * only errno. */` |
|     1 | 1845 | `		ph7_result_bool(pCtx,1);` |
|     - | 1846 | `	}` |
|     - | 1847 | `#elif defined(__UNIXES__)` |
|     - | 1848 | `	{` |
|     - | 1849 | `		int iIgnored;` |
|     - | 1850 | `		/* nice() legitimately answers -1 (it returns the NEW nice value), so` |
|     - | 1851 | `		 * errno is the only failure evidence — php clears it first for the same` |
|     - | 1852 | `		 * reason. */` |
|     6 | 1853 | `		errno = 0;` |
|     6 | 1854 | `		iIgnored = nice((int)iPri);` |
|     3 | 1855 | `		(void)iIgnored;` |
|     6 | 1856 | `		if( errno != 0 ){` |
|     3 | 1857 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|     - | 1858 | `				"%s(): Only a super user may attempt to increase the priority of a process",` |
|     1 | 1859 | `				ph7_function_name(pCtx));` |
|     2 | 1860 | `			ph7_result_bool(pCtx,0);` |
|     1 | 1861 | `		}else{` |
|     4 | 1862 | `			ph7_result_bool(pCtx,1);` |
|     - | 1863 | `		}` |
|     - | 1864 | `	}` |
|     - | 1865 | `#else` |
|     - | 1866 | `	ph7_result_bool(pCtx,0);` |
|     - | 1867 | `#endif` |
|     7 | 1868 | `	return PH7_OK;` |
|     1 | 1869 | `}` |
|  7524 | 1870 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1871 | `{` |
|     - | 1872 | `	const char *zCommand, *zMode;` |
|     - | 1873 | `	char zPosix[8];` |
|     - | 1874 | `	pipe_private *pPipe;` |
|     - | 1875 | `	io_private *pDev;` |
|  7529 | 1876 | `	int nCmdLen, nModeLen, nPosix, i, bDropped = 0;` |
|  3757 | 1877 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|     - | 1878 | `	/* Extract the command and mode */` |
|  7529 | 1879 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|  7529 | 1880 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|     - | 1881 | `	/*` |
|     - | 1882 | `	 * php's mode rule, and the only one it has: ONE 'b' — C's binary flag, which` |
|     - | 1883 | `	 * popen(3) itself refuses — is dropped from the mode on POSIX, and what is` |
|     - | 1884 | `	 * left must be exactly "r", "w", "rb" or "wb". PHL used to read mode[0] and` |
|     - | 1885 | `	 * hand the REST to popen(3) unexamined, which was wrong in both directions:` |
|     - | 1886 | ``	 * `popen($cmd, 'rb')`, the ordinary binary spelling, answered FALSE because`` |
|     - | 1887 | ``	 * glibc rejected the 'b', and `popen($cmd, 'rr')` opened a pipe php refuses.`` |
|     - | 1888 | `	 */` |
|  7529 | 1889 | `	nPosix = 0;` |
|     - | 1890 | `#ifdef __WINNT__` |
|     - | 1891 | `	SXUNUSED(bDropped);   /* cmd.exe keeps the 'b': _popen understands it */` |
|     - | 1892 | `#endif` |
| 15067 | 1893 | `	for( i = 0 ; i < nModeLen && nPosix < (int)sizeof(zPosix) - 1 ; ++i ){` |
|     - | 1894 | `#ifndef __WINNT__` |
|  7538 | 1895 | `		if( zMode[i] == 'b' && !bDropped ){` |
|     8 | 1896 | `			bDropped = 1;   /* php drops the FIRST one and only that one */` |
|     8 | 1897 | `			continue;` |
|     - | 1898 | `		}` |
|     - | 1899 | `#endif` |
|  7535 | 1900 | `		zPosix[nPosix++] = zMode[i];` |
|  3765 | 1901 | `	}` |
|  7529 | 1902 | `	zPosix[nPosix] = 0;` |
|  7524 | 1903 | `	if( nPosix > 2` |
|  7524 | 1904 | `	 \|\| (nPosix == 1 && zPosix[0] != 'r' && zPosix[0] != 'w')` |
|  3781 | 1905 | `	 \|\| (nPosix == 2 && SyMemcmp(zPosix,"rb",sizeof("rb")-1) != 0` |
|     7 | 1906 | `	                 && SyMemcmp(zPosix,"wb",sizeof("wb")-1) != 0) ){` |
|     9 | 1907 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1908 | `			"popen(): Argument #2 ($mode) must be one of \"r\", \"rb\", \"w\", or \"wb\"");` |
|     - | 1909 | `	}` |
|     - | 1910 | `	/* Open the pipe. An EMPTY mode passes php's check above and fails HERE, in` |
|     - | 1911 | `	 * popen(3) — php reports it as an open failure and so does this, rather than` |
|     - | 1912 | `	 * letting the platform layer read mode[0] out of an empty string. */` |
|  7521 | 1913 | `	pPipe = nPosix > 0 ? PipeOpen(pCtx->pVm, zCommand, zPosix) : 0;` |
|  7521 | 1914 | `	if( pPipe == 0 ){` |
|     - | 1915 | ``		/* php names both arguments in this one: `popen(cmd,mode): message`. PHL`` |
|     - | 1916 | `		 * answered FALSE in silence, so a script had nothing to report. */` |
|     5 | 1917 | `		if( nPosix < 1 ){` |
|     3 | 1918 | `			errno = EINVAL;` |
|     1 | 1919 | `		}` |
|     7 | 1920 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|     4 | 1921 | `			ph7_function_name(pCtx),zCommand,zPosix,VfsStrerror(errno));` |
|     5 | 1922 | `		ph7_result_bool(pCtx, 0);` |
|     5 | 1923 | `		return PH7_OK;` |
|     - | 1924 | `	}` |
|     - | 1925 | `	/* Allocate an io_private instance to wrap the pipe */` |
|  7517 | 1926 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|  7517 | 1927 | `	if( pDev == 0 ){` |
|   ! 0 | 1928 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|   ! 0 | 1929 | `		PipeClose(pPipe);` |
|   ! 0 | 1930 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 1931 | `		return PH7_OK;` |
|     - | 1932 | `	}` |
|     - | 1933 | `	/* Initialize the io_private structure */` |
|  7517 | 1934 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|     - | 1935 | `	/* A pipe has no wrapper and no path, so php's meta reports the MODE and` |
|     - | 1936 | ``	 * neither `wrapper_type` nor `uri`: an empty URI is what leaves them out. */`` |
|  7517 | 1937 | `	SetIOPrivateOpenedAs(pDev,0,0,zPosix,nPosix);` |
|  7517 | 1938 | `	pDev->pHandle = pPipe;` |
|     - | 1939 | `	/* Return the io_private instance as a resource */` |
|  7517 | 1940 | `	ph7_result_resource(pCtx, pDev);` |
|  7517 | 1941 | `	return PH7_OK;` |
|  3762 | 1942 | `}` |
|     - | 1943 | `/*` |
|     - | 1944 | ` * int pclose(resource $handle)` |
|     - | 1945 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|     - | 1946 | ` * Parameters` |
|     - | 1947 | ` *  $handle` |
|     - | 1948 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|     - | 1949 | ` * Return` |
|     - | 1950 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|     - | 1951 | ` */` |
|  7366 | 1952 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1953 | `{` |
|     - | 1954 | `	const ph7_io_stream *pStream;` |
|     - | 1955 | `	pipe_private *pPipe;` |
|     - | 1956 | `	io_private *pDev;` |
|     - | 1957 | `	int status,rc;` |
|  7371 | 1958 | `	if( nArg < 1 ){` |
|     - | 1959 | `		/* Missing/Invalid arguments, return -1 */` |
|   ! 0 | 1960 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|   ! 0 | 1961 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1962 | `		return PH7_OK;` |
|     - | 1963 | `	}` |
|     - | 1964 | ``	/* php's screen, and it names this one's argument `$handle` rather than`` |
|     - | 1965 | ``	 * `$stream`: a non-resource and an already-CLOSED pipe are both TypeErrors. */`` |
|  7371 | 1966 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"handle",&rc);` |
|  7371 | 1967 | `	if( pDev == 0 ){` |
|    15 | 1968 | `		return rc;` |
|     - | 1969 | `	}` |
|     - | 1970 | `	/* Point to the target IO stream device */` |
|  7357 | 1971 | `	pStream = pDev->pStream;` |
|  7357 | 1972 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|   ! 0 | 1973 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|   ! 0 | 1974 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1975 | `		return PH7_OK;` |
|     - | 1976 | `	}` |
|     - | 1977 | `	/* Get the pipe handle */` |
|  7357 | 1978 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|     - | 1979 | `	/* A write chain gets its closing call while the pipe is still open. */` |
|  7357 | 1980 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     - | 1981 | `	/* Close the pipe and get exit status */` |
|  7357 | 1982 | `	status = PipeClose(pPipe);` |
|     - | 1983 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
|  7357 | 1984 | `	MarkIOPrivateClosed(pDev);` |
|     - | 1985 | `	/* Return the exit status */` |
|  7357 | 1986 | `	ph7_result_int(pCtx, status);` |
|  7357 | 1987 | `	return PH7_OK;` |
|  3683 | 1988 | `}` |
|     - | 1989 | `/*` |
|     - | 1990 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|     - | 1991 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|     - | 1992 | ` *   standard descriptors (php's process-control family). The returned` |
|     - | 1993 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|     - | 1994 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|     - | 1995 | ` *   bounds and report it as a live, non-stream resource.` |
|     - | 1996 | ` */` |
|     - | 1997 | `#ifdef __UNIXES__` |
|     - | 1998 | `#define PROC_MAX_DESC 16` |
|     - | 1999 | `typedef struct proc_private proc_private;` |
|     - | 2000 | `struct proc_private` |
|     - | 2001 | `{` |
|     - | 2002 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|     - | 2003 | `	int pid;           /* child process id */` |
|     - | 2004 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|     - | 2005 | `	int exit_code;     /* cached exit status once reaped */` |
|     - | 2006 | `	/* The parent ends this handle created and handed to $pipes. php's proc resource` |
|     - | 2007 | `	 * OWNS them: its destructor closes every one before it waits, with the comment` |
|     - | 2008 | `	 * "Close all pipes first, so that the child may exit" -- and proc_close() IS that` |
|     - | 2009 | `	 * destructor, so after it the script's own $pipes entries are closed resources.` |
|     - | 2010 | `	 * Without the ownership a child reading its stdin never sees EOF and the wait` |
|     - | 2011 | `	 * never returns: monolog's ProcessHandler suite hung the engine forever there.` |
|     - | 2012 | `	 * The device structs are never freed on close (MarkIOPrivateClosed only flags` |
|     - | 2013 | `	 * them), so these stay valid even when the script closed them first. */` |
|     - | 2014 | `	io_private *apPipe[PROC_MAX_DESC];` |
|     - | 2015 | `	int nPipe;` |
|     - | 2016 | `};` |
|     - | 2017 | `/* Close a parent pipe end this handle still owns, exactly as fclose() would. */` |
|  1336 | 2018 | `static void ProcClosePipe(io_private *pEnd)` |
|     - | 2019 | `{` |
|  1336 | 2020 | `	if( pEnd == 0 \|\| pEnd->iMagic != IO_PRIVATE_MAGIC \|\| pEnd->pStream == 0 ){` |
|  1312 | 2021 | `		return; /* the script closed it already, or it never opened */` |
|     - | 2022 | `	}` |
|    24 | 2023 | `	PH7_StreamFilterReleaseChains(pEnd);` |
|    24 | 2024 | `	PH7_StreamCloseHandle(pEnd->pStream,pEnd->pHandle);` |
|    24 | 2025 | `	MarkIOPrivateClosed(pEnd);` |
|   668 | 2026 | `}` |
|     - | 2027 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|  1336 | 2028 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd,int bParentReads)` |
|     - | 2029 | `{` |
|  1336 | 2030 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|  1336 | 2031 | `	if( pDev == 0 ){` |
|   ! 0 | 2032 | `		return 0;` |
|     - | 2033 | `	}` |
|  1336 | 2034 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|     - | 2035 | `	/* Same shape as popen()'s end: a proc_open() pipe carries no path, and its` |
|     - | 2036 | `	 * mode is the direction the PARENT holds — the opposite of the child's. */` |
|  1336 | 2037 | `	SetIOPrivateOpenedAs(pDev,0,0,bParentReads ? "r" : "w",1);` |
|  1336 | 2038 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|  1336 | 2039 | `	return pDev;` |
|   668 | 2040 | `}` |
|     - | 2041 | `/* One parsed descriptor-spec entry. */` |
|     - | 2042 | `struct proc_desc` |
|     - | 2043 | `{` |
|     - | 2044 | `	int child_fd;      /* the array key: which fd the child sees */` |
|     - | 2045 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|     - | 2046 | `	/* pipe */` |
|     - | 2047 | `	int child_end;     /* fd the child must have at child_fd */` |
|     - | 2048 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|     - | 2049 | `	int parent_reads;  /* the parent's end is the READ end (the child writes) */` |
|     - | 2050 | `	/* file */` |
|     - | 2051 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|     - | 2052 | `	/* redirect */` |
|     - | 2053 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|     - | 2054 | `};` |
|   452 | 2055 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2056 | `{` |
|     - | 2057 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|     - | 2058 | `	io_private *apEnd[PROC_MAX_DESC]; /* the parent ends, handed to the handle below */` |
|   452 | 2059 | `	int nEnd = 0;` |
|   452 | 2060 | `	int nDesc = 0;` |
|     - | 2061 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|     - | 2062 | `	ph7_hashmap *pSpecMap;` |
|     - | 2063 | `	ph7_hashmap_node *pNode;` |
|   452 | 2064 | `	ph7_vm *pVm = pCtx->pVm;` |
|   452 | 2065 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|   452 | 2066 | `	int nArgv = 0;` |
|   452 | 2067 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|   452 | 2068 | `	const char *zCwd = 0;` |
|   452 | 2069 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|   452 | 2070 | `	int nEnv = 0;` |
|     - | 2071 | `	proc_private *pProc;` |
|     - | 2072 | `	pid_t pid;` |
|     - | 2073 | `	int i, rc;` |
|   452 | 2074 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 2075 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|   ! 0 | 2076 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2077 | `		return PH7_OK;` |
|     - | 2078 | `	}` |
|     - | 2079 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|   452 | 2080 | `	if( ph7_value_is_array(apArg[0]) ){` |
|   440 | 2081 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   440 | 2082 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|   440 | 2083 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|   440 | 2084 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   440 | 2085 | `		pNode = pCmdMap->pFirst;` |
|  2258 | 2086 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|  1818 | 2087 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|     - | 2088 | `			int nLen; const char *zs;` |
|  1818 | 2089 | `			PH7_MemObjInit(pVm,pv);` |
|  1818 | 2090 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|  1818 | 2091 | `			zs = ph7_value_to_string(pv,&nLen);` |
|  1818 | 2092 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|  1818 | 2093 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|  1818 | 2094 | `			PH7_MemObjRelease(pv);` |
|  1818 | 2095 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|  1818 | 2096 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|   909 | 2097 | `		}` |
|   440 | 2098 | `		azArgv[nArgv] = 0;` |
|   220 | 2099 | `	}else{` |
|     - | 2100 | `		int nLen;` |
|    12 | 2101 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|     - | 2102 | `	}` |
|     - | 2103 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|   452 | 2104 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|   ! 0 | 2105 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|   ! 0 | 2106 | `		if( nLen < 1 ){ zCwd = 0; }` |
|   ! 0 | 2107 | `	}` |
|   306 | 2108 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|   160 | 2109 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|   160 | 2110 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|   160 | 2111 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|   160 | 2112 | `		if( azEnv ){` |
|   160 | 2113 | `			pNode = pEnvMap->pFirst;` |
|   410 | 2114 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|     - | 2115 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|     - | 2116 | `				int bNamed;` |
|   250 | 2117 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|   250 | 2118 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|   250 | 2119 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|   250 | 2120 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|   250 | 2121 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|     - | 2122 | `				/* php drops an entry whose stringified VALUE is empty rather than` |
|     - | 2123 | ``				 * exporting `NAME=`: the child's getenv() answers false for it and`` |
|     - | 2124 | `				 * $_ENV has no such key. Passing '' is how a caller UNSETS a name` |
|     - | 2125 | `				 * the parent holds, so exporting it empty is a different child. */` |
|   250 | 2126 | `				if( nv < 1 ){` |
|     4 | 2127 | `					PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|     4 | 2128 | `					pNode = pNode->pPrev;` |
|     4 | 2129 | `					continue;` |
|     - | 2130 | `				}` |
|     - | 2131 | `				/* Only a non-empty STRING key names the variable. php reads an` |
|     - | 2132 | `` 				 * integer-keyed (or ''-keyed) entry as a ready-made `NAME=VALUE` `` |
|     - | 2133 | `				 * string and exports the value verbatim, so a list array like` |
|     - | 2134 | `				 * ['PATH=/bin'] is a valid environment there -- where prefixing` |
|     - | 2135 | ``				 * the synthesised key made it `0=PATH=/bin`. */`` |
|   246 | 2136 | `				bNamed = (pNode->iType == HASHMAP_BLOB_NODE && nk > 0);` |
|   492 | 2137 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,` |
|   246 | 2138 | `					(sxu32)(bNamed ? nk+nv+2 : nv+1));` |
|   246 | 2139 | `				if( zPair ){` |
|   246 | 2140 | `					if( bNamed ){` |
|   238 | 2141 | `						SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|   238 | 2142 | `						SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|   119 | 2143 | `					}else{` |
|     8 | 2144 | `						SyMemcpy(zv,zPair,(sxu32)nv); zPair[nv] = 0;` |
|     - | 2145 | `					}` |
|   246 | 2146 | `					azEnv[nEnv++] = zPair;` |
|   123 | 2147 | `				}` |
|   246 | 2148 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|   246 | 2149 | `				pNode = pNode->pPrev;` |
|   123 | 2150 | `			}` |
|   160 | 2151 | `			azEnv[nEnv] = 0;` |
|    80 | 2152 | `		}` |
|    80 | 2153 | `	}` |
|     - | 2154 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|   452 | 2155 | `	pSpec = apArg[1];` |
|   452 | 2156 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|   452 | 2157 | `	pNode = pSpecMap->pFirst;` |
|  1792 | 2158 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|  1340 | 2159 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|  1340 | 2160 | `		PH7_MemObjInit(pVm,&sKey);` |
|  1340 | 2161 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|  1340 | 2162 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|  1340 | 2163 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1; pD->parent_reads = 0;` |
|  1340 | 2164 | `		PH7_MemObjRelease(&sKey);` |
|  1340 | 2165 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|  1340 | 2166 | `		PH7_MemObjInit(pVm,pEntry);` |
|  1340 | 2167 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|  1340 | 2168 | `		if( ph7_value_is_array(pEntry) ){` |
|     - | 2169 | `			int nLen; const char *zType;` |
|     - | 2170 | `			/* pEntry aliases the descriptor array the SCRIPT still holds, so every` |
|     - | 2171 | `			 * member is read through a scratch copy — ph7_value_to_string() would` |
|     - | 2172 | `			 * convert the entry in place and rewrite the script's own $descriptors` |
|     - | 2173 | ``			 * (`[0 => ['pipe', 114]]` came back as `'114'`). */`` |
|     - | 2174 | `			ph7_value sPeek;` |
|  1340 | 2175 | `			PH7_MemObjInit(pVm,&sPeek);` |
|  1340 | 2176 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|  1340 | 2177 | `			zType = pType ? ph7_value_to_string(PH7_ValuePeek(pType,&sPeek),&nLen) : "";` |
|  1340 | 2178 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|     - | 2179 | `				int fds[2];` |
|  1336 | 2180 | `				if( pipe(fds) == 0 ){` |
|  1336 | 2181 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|     - | 2182 | `					{` |
|     - | 2183 | `						ph7_value sMode;` |
|     - | 2184 | `						int nMode; const char *zMode;` |
|  1336 | 2185 | `						PH7_MemObjInit(pVm,&sMode);` |
|  1336 | 2186 | `						zMode = pParam ? ph7_value_to_string(PH7_ValuePeek(pParam,&sMode),&nMode) : "r";` |
|  1336 | 2187 | `						pD->kind = 0;` |
|  1336 | 2188 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|     - | 2189 | `							/* child writes -> parent reads: child gets write end */` |
|   898 | 2190 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|   898 | 2191 | `						pD->parent_reads = 1;` |
|   449 | 2192 | `						}else{` |
|     - | 2193 | `							/* child reads -> parent writes: child gets read end */` |
|   438 | 2194 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|   438 | 2195 | `						pD->parent_reads = 0;` |
|     - | 2196 | `						}` |
|     - | 2197 | `						/* The PARENT end is close-on-exec, which is php's own` |
|     - | 2198 | ``						 * `fcntl(descriptors[i].parentend, F_SETFD, FD_CLOEXEC)`:`` |
|     - | 2199 | `						 * without it the NEXT proc_open()'s child inherits a copy of` |
|     - | 2200 | `						 * this pipe, and the first child then never sees EOF on its` |
|     - | 2201 | `						 * stdin however carefully the script closes its own end. Two` |
|     - | 2202 | `						 * such handlers alive at once deadlock the interpreter --` |
|     - | 2203 | `						 * monolog's ProcessHandler suite ran to its last line and` |
|     - | 2204 | `						 * then hung forever in proc_close(). The CHILD's end is` |
|     - | 2205 | `						 * dup2()'d onto 0/1/2, and dup2 clears the flag, so the` |
|     - | 2206 | `						 * process being started keeps exactly what it should. */` |
|     - | 2207 | `#if defined(F_SETFD) && defined(FD_CLOEXEC)` |
|  1336 | 2208 | `						fcntl(pD->parent_end,F_SETFD,FD_CLOEXEC);` |
|     - | 2209 | `#endif` |
|  1336 | 2210 | `						nDesc++;` |
|  1336 | 2211 | `						PH7_MemObjRelease(&sMode);` |
|     - | 2212 | `					}` |
|   668 | 2213 | `				}` |
|   672 | 2214 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|     - | 2215 | `				ph7_value sPath, sMode;` |
|     2 | 2216 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|     2 | 2217 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|     2 | 2218 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|     2 | 2219 | `				PH7_MemObjInit(pVm,&sPath);` |
|     2 | 2220 | `				PH7_MemObjInit(pVm,&sMode);` |
|     2 | 2221 | `				zPath = pPath ? ph7_value_to_string(PH7_ValuePeek(pPath,&sPath),&nLen2) : "";` |
|     2 | 2222 | `				zMode = pMode ? ph7_value_to_string(PH7_ValuePeek(pMode,&sMode),&nLen3) : "r";` |
|     2 | 2223 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|   ! 0 | 2224 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|     2 | 2225 | `				pD->kind = 1;` |
|     2 | 2226 | `				pD->file_fd = open(zPath,oflag,0644);` |
|     2 | 2227 | `				nDesc++;` |
|     2 | 2228 | `				PH7_MemObjRelease(&sPath);` |
|     2 | 2229 | `				PH7_MemObjRelease(&sMode);` |
|     3 | 2230 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|     2 | 2231 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|     2 | 2232 | `				pD->kind = 2;` |
|     2 | 2233 | `				pD->redirect_to = pParam ? (int)PH7_ValuePeekInt64(pParam) : 1;` |
|     2 | 2234 | `				nDesc++;` |
|     1 | 2235 | `			}` |
|  1340 | 2236 | `			PH7_MemObjRelease(&sPeek);` |
|   670 | 2237 | `		}` |
|  1340 | 2238 | `		PH7_MemObjRelease(pEntry);` |
|  1340 | 2239 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|  1340 | 2240 | `		pNode = pNode->pPrev;` |
|   670 | 2241 | `	}` |
|     - | 2242 | `	/* --- Fork the child --- */` |
|   452 | 2243 | `	pid = fork();` |
|   678 | 2244 | `	if( pid < 0 ){` |
|   ! 0 | 2245 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"fork() failed");` |
|   ! 0 | 2246 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2247 | `		return PH7_OK;` |
|     - | 2248 | `	}` |
|   904 | 2249 | `	if( pid == 0 ){` |
|     - | 2250 | `		/* Child: wire up descriptors then exec */` |
|  1792 | 2251 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|  1340 | 2252 | `			struct proc_desc *pD = &aDesc[i];` |
|  1340 | 2253 | `			if( pD->kind == 0 ){` |
|  1336 | 2254 | `				dup2(pD->child_end,pD->child_fd);` |
|  1336 | 2255 | `				close(pD->parent_end);` |
|  1336 | 2256 | `				close(pD->child_end);` |
|   672 | 2257 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|     2 | 2258 | `				dup2(pD->file_fd,pD->child_fd);` |
|     2 | 2259 | `				close(pD->file_fd);` |
|     1 | 2260 | `			}` |
|   670 | 2261 | `		}` |
|     - | 2262 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|  1792 | 2263 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|  1340 | 2264 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|   670 | 2265 | `		}` |
|   452 | 2266 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|   452 | 2267 | `		if( azEnv ){` |
|   160 | 2268 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|   ! 0 | 2269 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|   ! 0 | 2270 | `		}else{` |
|   292 | 2271 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|    12 | 2272 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|     - | 2273 | `		}` |
|   226 | 2274 | `		_exit(127); /* exec failed */` |
|     - | 2275 | `	}` |
|     - | 2276 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|   452 | 2277 | `	pPipes = ph7_context_new_array(pCtx);` |
|  1792 | 2278 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|  1340 | 2279 | `		struct proc_desc *pD = &aDesc[i];` |
|  1340 | 2280 | `		if( pD->kind == 0 ){` |
|     - | 2281 | `			io_private *pEnd;` |
|     - | 2282 | `			ph7_value *pRes;` |
|  1336 | 2283 | `			close(pD->child_end);` |
|  1336 | 2284 | `			pEnd = ProcWrapFd(pVm,pD->parent_end,pD->parent_reads);` |
|  1336 | 2285 | `			pRes = ph7_context_new_scalar(pCtx);` |
|  1336 | 2286 | `			if( pEnd && pRes && pPipes ){` |
|  1336 | 2287 | `				ph7_value_resource(pRes,pEnd);` |
|  1336 | 2288 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|  1336 | 2289 | `				apEnd[nEnd++] = pEnd;   /* the handle keeps its own list; see proc_private */` |
|   668 | 2290 | `			}` |
|  1336 | 2291 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|   672 | 2292 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|     2 | 2293 | `			close(pD->file_fd);` |
|     1 | 2294 | `		}` |
|   670 | 2295 | `	}` |
|   452 | 2296 | `	if( pPipes ){` |
|   452 | 2297 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|   226 | 2298 | `	}` |
|     - | 2299 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|   458 | 2300 | `	if( azArgv ){` |
|  2258 | 2301 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|   440 | 2302 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|   220 | 2303 | `	}` |
|   586 | 2304 | `	if( azEnv ){` |
|   406 | 2305 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|   160 | 2306 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|    80 | 2307 | `	}` |
|     - | 2308 | `	/* Build the process resource */` |
|   452 | 2309 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|   452 | 2310 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   452 | 2311 | `	SyZero(pProc,sizeof(proc_private));` |
|   452 | 2312 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|   452 | 2313 | `	pProc->pid = (int)pid;` |
|   452 | 2314 | `	pProc->running = 1;` |
|   452 | 2315 | `	pProc->exit_code = 0;` |
|  1788 | 2316 | `	for( i = 0 ; i < nEnd ; ++i ){ pProc->apPipe[i] = apEnd[i]; }` |
|   452 | 2317 | `	pProc->nPipe = nEnd;` |
|   452 | 2318 | `	ph7_result_resource(pCtx,pProc);` |
|   226 | 2319 | `	(void)rc;` |
|   452 | 2320 | `	return PH7_OK;` |
|   226 | 2321 | `}` |
|     - | 2322 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|   452 | 2323 | `static void ProcReap(proc_private *pProc,int block)` |
|     - | 2324 | `{` |
|   452 | 2325 | `	int status = 0;` |
|     - | 2326 | `	pid_t r;` |
|   452 | 2327 | `	if( !pProc->running ){ return; }` |
|   452 | 2328 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|   452 | 2329 | `	if( r == (pid_t)pProc->pid ){` |
|   452 | 2330 | `		pProc->running = 0;` |
|   452 | 2331 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|    92 | 2332 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|   226 | 2333 | `	}` |
|   226 | 2334 | `}` |
|     - | 2335 | `/*` |
|     - | 2336 | ` * The three verbs all take a LIVE process resource, and php answers anything else --` |
|     - | 2337 | ` * a file handle, an already-closed process, a process resource closed by an earlier` |
|     - | 2338 | ` * proc_close() -- with one catchable sentence naming the function.` |
|     - | 2339 | ` */` |
|   558 | 2340 | `static proc_private * ProcOfArg(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi32 *pRc)` |
|     - | 2341 | `{` |
|     - | 2342 | `	proc_private *pProc;` |
|     - | 2343 | `	char zGiven[128];` |
|   558 | 2344 | `	*pRc = PH7_OK;` |
|   558 | 2345 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|     - | 2346 | `		/* Not a resource at all: php's ordinary argument sentence, naming the type. */` |
|     3 | 2347 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2348 | `			"%z(): Argument #1 ($process) must be of type resource, %s given",` |
|     2 | 2349 | `			&pCtx->pFunc->sName,` |
|     2 | 2350 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     2 | 2351 | `		return 0;` |
|     - | 2352 | `	}` |
|   556 | 2353 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|   556 | 2354 | `	if( pProc && pProc->base.iMagic == PROC_PRIVATE_MAGIC ){` |
|   544 | 2355 | `		return pProc;` |
|     - | 2356 | `	}` |
|     - | 2357 | `	/* A resource, but not a live process one -- a file handle, or a process` |
|     - | 2358 | `	 * handle an earlier proc_close() already closed. */` |
|    24 | 2359 | `	*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2360 | `		"%z(): supplied resource is not a valid process resource",` |
|    12 | 2361 | `		&pCtx->pFunc->sName);` |
|    12 | 2362 | `	return 0;` |
|   279 | 2363 | `}` |
|   458 | 2364 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2365 | `{` |
|     - | 2366 | `	proc_private *pProc;` |
|     - | 2367 | `	sxi32 rcArg;` |
|     - | 2368 | `	int i;` |
|   458 | 2369 | `	pProc = ProcOfArg(pCtx,nArg,apArg,&rcArg);` |
|   458 | 2370 | `	if( pProc == 0 ){` |
|     6 | 2371 | `		return rcArg;` |
|     - | 2372 | `	}` |
|     - | 2373 | `	/* php's own order, and the reason it has one: the pipes go FIRST so a child` |
|     - | 2374 | `	 * blocked reading its stdin sees EOF and can exit, and only then do we wait` |
|     - | 2375 | `	 * for it. Waiting first is a deadlock with any such child. */` |
|  1788 | 2376 | `	for( i = 0 ; i < pProc->nPipe ; ++i ){` |
|  1336 | 2377 | `		ProcClosePipe(pProc->apPipe[i]);` |
|  1336 | 2378 | `		pProc->apPipe[i] = 0;` |
|   668 | 2379 | `	}` |
|   452 | 2380 | `	pProc->nPipe = 0;` |
|   452 | 2381 | `	ProcReap(pProc,1/*block until it exits*/);` |
|   452 | 2382 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|     - | 2383 | `	/* proc_close() IS the resource's destructor in php, so the handle is closed` |
|     - | 2384 | `	 * afterwards: is_resource() answers false and every verb refuses it. */` |
|   452 | 2385 | `	pProc->base.iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   452 | 2386 | `	return PH7_OK;` |
|   229 | 2387 | `}` |
|    96 | 2388 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2389 | `{` |
|     - | 2390 | `	proc_private *pProc;` |
|     - | 2391 | `	sxi32 rcArg;` |
|    96 | 2392 | `	int sig = 15; /* SIGTERM */` |
|    96 | 2393 | `	pProc = ProcOfArg(pCtx,nArg,apArg,&rcArg);` |
|    96 | 2394 | `	if( pProc == 0 ){` |
|     4 | 2395 | `		return rcArg;` |
|     - | 2396 | `	}` |
|    92 | 2397 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|    92 | 2398 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|    92 | 2399 | `	ph7_result_bool(pCtx,1);` |
|    92 | 2400 | `	return PH7_OK;` |
|    48 | 2401 | `}` |
|     4 | 2402 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2403 | `{` |
|     - | 2404 | `	proc_private *pProc;` |
|     - | 2405 | `	ph7_value *pArray, *pVal;` |
|     - | 2406 | `	sxi32 rcArg;` |
|     4 | 2407 | `	pProc = ProcOfArg(pCtx,nArg,apArg,&rcArg);` |
|     4 | 2408 | `	if( pProc == 0 ){` |
|     4 | 2409 | `		return rcArg;` |
|     - | 2410 | `	}` |
|   ! 0 | 2411 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|   ! 0 | 2412 | `	pArray = ph7_context_new_array(pCtx);` |
|   ! 0 | 2413 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 2414 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   ! 0 | 2415 | `	ph7_value_int(pVal,pProc->pid);` |
|   ! 0 | 2416 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|   ! 0 | 2417 | `	ph7_value_bool(pVal,pProc->running);` |
|   ! 0 | 2418 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|   ! 0 | 2419 | `	ph7_value_bool(pVal,0);` |
|   ! 0 | 2420 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|   ! 0 | 2421 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|   ! 0 | 2422 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|   ! 0 | 2423 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|   ! 0 | 2424 | `	ph7_value_int(pVal,0);` |
|   ! 0 | 2425 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|   ! 0 | 2426 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|   ! 0 | 2427 | `	ph7_context_release_value(pCtx,pVal);` |
|   ! 0 | 2428 | `	ph7_result_value(pCtx,pArray);` |
|   ! 0 | 2429 | `	return PH7_OK;` |
|     2 | 2430 | `}` |
|     - | 2431 | `#else /* !__UNIXES__ */` |
|     - | 2432 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2433 | `{` |
|     - | 2434 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2435 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|   ! 0 | 2436 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 2437 | `	return PH7_OK;` |
|   ! 0 | 2438 | `}` |
|     - | 2439 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2440 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|     - | 2441 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2442 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     - | 2443 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2444 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     - | 2445 | `#endif /* __UNIXES__ */` |
|     - | 2446 | `/* Export the php:// stream */` |
|     - | 2447 | `PH7_PRIVATE const ph7_io_stream sPHP_Stream = {` |
|     - | 2448 | `	"php",` |
|     - | 2449 | `	PH7_IO_STREAM_VERSION,` |
|     - | 2450 | `	PHPStreamData_Open,  /* xOpen */` |
|     - | 2451 | `	0,   /* xOpenDir */` |
|     - | 2452 | `	PHPStreamData_Close, /* xClose */` |
|     - | 2453 | `	0,  /* xCloseDir */` |
|     - | 2454 | `	PHPStreamData_Read,  /* xRead */` |
|     - | 2455 | `	0,  /* xReadDir */` |
|     - | 2456 | `	PHPStreamData_Write, /* xWrite */` |
|     - | 2457 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|     - | 2458 | `	0,  /* xLock */` |
|     - | 2459 | `	0,  /* xRewindDir */` |
|     - | 2460 | `	PHPStreamData_Tell,  /* xTell */` |
|     - | 2461 | `	PHPStreamData_Trunc, /* xTrunc */` |
|     - | 2462 | `	PHPStreamData_Sync,  /* xSync */` |
|     - | 2463 | `	PHPStreamData_Stat   /* xStat */` |
|     - | 2464 | `};` |
|     - | 2465 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 2466 | `/*` |
|     - | 2467 | ` * Return TRUE if we are dealing with the php:// stream.` |
|     - | 2468 | ` * FALSE otherwise.` |
|     - | 2469 | ` */` |
|     - | 2470 | `/*` |
|     - | 2471 | ` * The handle a php://filter proxy WRAPS, or 0 for any other php:// stream. The` |
|     - | 2472 | ` * proxy is not a stream of its own — the position, the descriptor, the stat and` |
|     - | 2473 | ` * the lock all belong to the stream underneath — so everything that asks the` |
|     - | 2474 | ` * device such a question has to go through here first.` |
|     - | 2475 | ` */` |
|   452 | 2476 | `PH7_PRIVATE io_private * PH7_PhpStreamInner(void *pHandle)` |
|     3 | 2477 | `{` |
|     - | 2478 | `#ifndef PH7_DISABLE_DISK_IO` |
|   455 | 2479 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   455 | 2480 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|    15 | 2481 | `		return pData->pInner;` |
|     - | 2482 | `	}` |
|     - | 2483 | `#else` |
|     - | 2484 | `	SXUNUSED(pHandle);` |
|     - | 2485 | `#endif` |
|   441 | 2486 | `	return 0;` |
|   227 | 2487 | `}` |
| 16384 | 2488 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream)` |
|     5 | 2489 | `{` |
|     - | 2490 | `#ifndef PH7_DISABLE_DISK_IO` |
| 16389 | 2491 | `	return pStream == &sPHP_Stream;` |
|     - | 2492 | `#else` |
|     - | 2493 | `	SXUNUSED(pStream); /* cc warning */` |
|     - | 2494 | `	return 0;` |
|     - | 2495 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2496 | `}` |
|     - | 2497 | `/*` |
|     - | 2498 | ` * Is this a handle php's plain-files device would own -- a file, a pipe, a` |
|     - | 2499 | ` * standard stream? php sets the blocking mode on those with O_NONBLOCK, which` |
|     - | 2500 | ` * Windows does not have, so there stream_set_blocking() answers FALSE for them.` |
|     - | 2501 | ` */` |
|   ! 0 | 2502 | `PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev)` |
|     1 | 2503 | `{` |
|     - | 2504 | `#ifndef PH7_DISABLE_DISK_IO` |
|     1 | 2505 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| pDev->pStream == 0 \|\| pDev->bDir ){` |
|   ! 0 | 2506 | `		return 0;` |
|     - | 2507 | `	}` |
|     - | 2508 | `#ifdef __WINNT__` |
|     1 | 2509 | `	if( pDev->pStream == &sWinFileStream ){` |
|     1 | 2510 | `		return 1;` |
|     - | 2511 | `	}` |
|     - | 2512 | `#elif defined(__UNIXES__)` |
|   ! 0 | 2513 | `	if( pDev->pStream == &sUnixFileStream ){` |
|   ! 0 | 2514 | `		return 1;` |
|     - | 2515 | `	}` |
|     - | 2516 | `#endif` |
|     1 | 2517 | `	if( pDev->pStream == &sPipe_Stream ){` |
|   ! 0 | 2518 | `		return 1;` |
|     - | 2519 | `	}` |
|     1 | 2520 | `	if( is_php_stream(pDev->pStream) ){` |
|     1 | 2521 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     1 | 2522 | `		return pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|   ! 0 | 2523 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR;` |
|     - | 2524 | `	}` |
|     1 | 2525 | `	return 0;` |
|     - | 2526 | `#else` |
|     - | 2527 | `	SXUNUSED(pDev); /* cc warning */` |
|     - | 2528 | `	return 0;` |
|     - | 2529 | `#endif` |
|     1 | 2530 | `}` |
|     - | 2531 | `/*` |
|     - | 2532 | ` * The POSIX descriptor behind an open handle, or -1 when there is none.` |
|     - | 2533 | ` * php applies blocking mode and timeouts AT the descriptor, so a stream that` |
|     - | 2534 | ` * has no fd — a memory buffer, a data:// payload, a userland wrapper, and` |
|     - | 2535 | ` * every file on Windows, where the device carries a HANDLE — is exactly the` |
|     - | 2536 | ` * set php answers "unsupported" for.` |
|     - | 2537 | ` */` |
|   368 | 2538 | `PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev)` |
|     2 | 2539 | `{` |
|     - | 2540 | `#if !defined(__WINNT__) && !defined(PH7_DISABLE_DISK_IO)` |
|     - | 2541 | `	/* A php://filter handle has no descriptor of its own; the stream it wraps` |
|     - | 2542 | `	 * does, and that is the one blocking mode and locking apply to. */` |
|   368 | 2543 | `	pDev = PH7_StreamUnwrap(pDev);` |
|   368 | 2544 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| pDev->pStream == 0 \|\| pDev->bDir ){` |
|     - | 2545 | `		/* A DIRECTORY handle rides the same ops table as a file and stores a` |
|     - | 2546 | `		 * DIR* where a file stores its descriptor, so reading one as the other` |
|     - | 2547 | `		 * hands fcntl()/lseek() a truncated heap pointer — an arbitrary fd` |
|     - | 2548 | `		 * number belonging to something else in this process. */` |
|   ! 0 | 2549 | `		return -1;` |
|     - | 2550 | `	}` |
|   368 | 2551 | `	if( pDev->pStream == &sUnixFileStream ){` |
|   128 | 2552 | `		return SX_PTR_TO_INT(pDev->pHandle);` |
|     - | 2553 | `	}` |
|   240 | 2554 | `	if( pDev->pStream == &sPipe_Stream ){` |
|   ! 0 | 2555 | `		pipe_private *pPipe = (pipe_private *)pDev->pHandle;` |
|   ! 0 | 2556 | `		return pPipe->pFile ? fileno(pPipe->pFile) : -1;` |
|     - | 2557 | `	}` |
|   240 | 2558 | `	if( is_php_stream(pDev->pStream) ){` |
|   208 | 2559 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|   208 | 2560 | `		if( pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|   205 | 2561 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR ){` |
|     6 | 2562 | `			return SX_PTR_TO_INT(pData->x.pHandle);` |
|     - | 2563 | `		}` |
|   100 | 2564 | `	}` |
|   234 | 2565 | `	return -1;` |
|     - | 2566 | `#else` |
|     - | 2567 | `	SXUNUSED(pDev); /* cc warning */` |
|     2 | 2568 | `	return -1;` |
|     - | 2569 | `#endif` |
|   183 | 2570 | `}` |
|     - | 2571 | `/*` |
|     - | 2572 | `` * Can this handle report a POSITION? php's `seekable` is a fact about what the`` |
|     - | 2573 | ` * handle sits on, not about what the device could do — php://stdout is seekable` |
|     - | 2574 | ` * into a file and not down a pipe — and the descriptor is the only thing that` |
|     - | 2575 | ` * knows. Answers 1 (yes), 0 (no) or -1 (nothing here can tell; the caller falls` |
|     - | 2576 | ` * back on the device's own xTell).` |
|     - | 2577 | ` */` |
|   246 | 2578 | `PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev)` |
|     5 | 2579 | `{` |
|     - | 2580 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2581 | `#ifdef __WINNT__` |
|     - | 2582 | `	/* The file devices carry a HANDLE rather than a descriptor here, and only` |
|     - | 2583 | `	 * the php:// standard streams hold one this can ask. */` |
|     5 | 2584 | `	if( pDev && pDev->pHandle && is_php_stream(pDev->pStream) ){` |
|     3 | 2585 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     - | 2586 | `		if( pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|     3 | 2587 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR ){` |
|     - | 2588 | `			LARGE_INTEGER zero,pos;` |
|     1 | 2589 | `			zero.QuadPart = 0;` |
|     1 | 2590 | `			return SetFilePointerEx((HANDLE)pData->x.pHandle,zero,&pos,FILE_CURRENT) ? 1 : 0;` |
|     - | 2591 | `		}` |
|     - | 2592 | `	}` |
|     5 | 2593 | `	return -1;` |
|     - | 2594 | `#else` |
|   246 | 2595 | `	int fd = PH7_StreamPosixFd(pDev);` |
|   246 | 2596 | `	if( fd < 0 ){` |
|   214 | 2597 | `		return -1;` |
|     - | 2598 | `	}` |
|    32 | 2599 | `	return lseek(fd,0,SEEK_CUR) == (off_t)-1 ? 0 : 1;` |
|     - | 2600 | `#endif` |
|     - | 2601 | `#else` |
|     - | 2602 | `	SXUNUSED(pDev); /* cc warning */` |
|     - | 2603 | `	return -1;` |
|     - | 2604 | `#endif /* PH7_DISABLE_DISK_IO */` |
|   128 | 2605 | `}` |
|     - | 2606 | `/*` |
|     - | 2607 | ` * Which php:// sub-stream a handle opened. stream_get_meta_data() has to tell` |
|     - | 2608 | ` * MEMORY, TEMP and STDIO apart and only the device's own private state knows;` |
|     - | 2609 | ` * everything else answers 0.` |
|     - | 2610 | ` */` |
|  3021 | 2611 | `PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle)` |
|     5 | 2612 | `{` |
|     - | 2613 | `#ifndef PH7_DISABLE_DISK_IO` |
|  3026 | 2614 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  3026 | 2615 | `	return pData ? pData->iType : 0;` |
|     - | 2616 | `#else` |
|     - | 2617 | `	SXUNUSED(pHandle); /* cc warning */` |
|     - | 2618 | `	return 0;` |
|     - | 2619 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2620 | `}` |
|     - | 2621 | `/*` |
|     - | 2622 | ` * Has a php://temp handle just handed out its LAST byte? php's temp stream sits` |
|     - | 2623 | ` * over a memory one and copies that one's eof after every read, and the inner` |
|     - | 2624 | ` * stream raises it as soon as a fill has consumed the buffer -- so a temp` |
|     - | 2625 | ` * handle answers feof() true one read before a bare php://memory does, and` |
|     - | 2626 | `` * `foreach` over an SplTempFileObject stops one row earlier. Answers 0 for`` |
|     - | 2627 | ` * every other device, php://memory included.` |
|     - | 2628 | ` */` |
|   294 | 2629 | `PH7_PRIVATE int PH7_PhpStreamTempDrained(void *pHandle)` |
|     3 | 2630 | `{` |
|     - | 2631 | `#ifndef PH7_DISABLE_DISK_IO` |
|   297 | 2632 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   325 | 2633 | `	return pData && pData->bTemp && pData->iType == PH7_IO_STREAM_MEMORY` |
|   441 | 2634 | `		&& pData->nCur >= SyBlobLength(&pData->sMem);` |
|     - | 2635 | `#else` |
|     - | 2636 | `	SXUNUSED(pHandle); /* cc warning */` |
|     - | 2637 | `	return 0;` |
|     - | 2638 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     3 | 2639 | `}` |
|     - | 2640 | `/*` |
|     - | 2641 | ` * Return TRUE if we are dealing with the data:// stream.` |
|     - | 2642 | ` */` |
|  1984 | 2643 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream)` |
|     5 | 2644 | `{` |
|     - | 2645 | `#ifndef PH7_DISABLE_DISK_IO` |
|  1989 | 2646 | `	return pStream == &sDATA_Stream;` |
|     - | 2647 | `#else` |
|     - | 2648 | `	SXUNUSED(pStream); /* cc warning */` |
|     - | 2649 | `	return 0;` |
|     - | 2650 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2651 | `}` |
|     - | 2652 | `/*` |
|     - | 2653 | ` * bool stream_isatty(resource $stream)` |
|     - | 2654 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|     - | 2655 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|     - | 2656 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|     - | 2657 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|     - | 2658 | ` */` |
|     8 | 2659 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2660 | `{` |
|    10 | 2661 | `	int bTty = 0,rc;` |
|     - | 2662 | `	io_private *pDev;` |
|    10 | 2663 | `	if( nArg < 1 ){` |
|   ! 0 | 2664 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2665 | `		return PH7_OK;` |
|     - | 2666 | `	}` |
|     - | 2667 | `	/* php's screen runs FIRST here too: a closed handle is a TypeError, not the` |
|     - | 2668 | ``	 * `false` a stream that simply is not a terminal answers. */`` |
|    10 | 2669 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|    10 | 2670 | `	if( pDev == 0 ){` |
|     3 | 2671 | `		return rc;` |
|     - | 2672 | `	}` |
|     3 | 2673 | `	SXUNUSED(pDev); /* a build with no disk IO compiles the block below away */` |
|     - | 2674 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2675 | `	{` |
|     7 | 2676 | `		if( is_php_stream(pDev->pStream) ){` |
|     5 | 2677 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     6 | 2678 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|     4 | 2679 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|     3 | 2680 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|     - | 2681 | `#ifdef __WINNT__` |
|     1 | 2682 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|     - | 2683 | `#else` |
|     4 | 2684 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|     - | 2685 | `#endif` |
|     2 | 2686 | `			}` |
|     2 | 2687 | `		}` |
|     - | 2688 | `	}` |
|     - | 2689 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     7 | 2690 | `	ph7_result_bool(pCtx,bTty);` |
|     7 | 2691 | `	return PH7_OK;` |
|     6 | 2692 | `}` |
|     - | 2693 |  |
|     - | 2694 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2695 | `/*` |
|     - | 2696 | ` * Export the STDIN handle.` |
|     - | 2697 | ` */` |
|   186 | 2698 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|     5 | 2699 | `{` |
|     - | 2700 | `#ifndef PH7_DISABLE_DISK_IO` |
|   191 | 2701 | `	if( pVm->pStdin == 0  ){` |
|     - | 2702 | `		io_private *pIn;` |
|     - | 2703 | `		/* Allocate an IO private instance */` |
|    19 | 2704 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    19 | 2705 | `		if( pIn == 0 ){` |
|   ! 0 | 2706 | `			return 0;` |
|     - | 2707 | `		}` |
|    19 | 2708 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|    19 | 2709 | `		SetIOPrivateOpenedAs(pIn,"php://stdin",(int)sizeof("php://stdin")-1,"rb",2);` |
|     - | 2710 | `		/* Initialize the handle */` |
|    19 | 2711 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|     - | 2712 | `		/* Install the STDIN stream */` |
|    19 | 2713 | `		pVm->pStdin = pIn;` |
|     - | 2714 | `` 		/* The VM itself holds one: a script that does `$x = STDOUT; $x = null;` `` |
|     - | 2715 | `		 * must not close the process's own output -- php's standard handles` |
|     - | 2716 | `		 * outlive every value that names them. */` |
|    19 | 2717 | `		PH7_StreamValueRef(pIn);` |
|    19 | 2718 | `		return pIn;` |
|   ! 0 | 2719 | `	}else{` |
|     - | 2720 | `		/* NULL or STDIN */` |
|   177 | 2721 | `		return pVm->pStdin;` |
|     - | 2722 | `	}` |
|     - | 2723 | `#else` |
|     - | 2724 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2725 | `	return 0;` |
|     - | 2726 | `#endif` |
|    98 | 2727 | `}` |
|     - | 2728 | `/*` |
|     - | 2729 | ` * Export the STDOUT handle.` |
|     - | 2730 | ` */` |
|   177 | 2731 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|     5 | 2732 | `{` |
|     - | 2733 | `#ifndef PH7_DISABLE_DISK_IO` |
|   182 | 2734 | `	if( pVm->pStdout == 0  ){` |
|     - | 2735 | `		io_private *pOut;` |
|     - | 2736 | `		/* Allocate an IO private instance */` |
|    25 | 2737 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    25 | 2738 | `		if( pOut == 0 ){` |
|   ! 0 | 2739 | `			return 0;` |
|     - | 2740 | `		}` |
|    25 | 2741 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|    25 | 2742 | `		SetIOPrivateOpenedAs(pOut,"php://stdout",(int)sizeof("php://stdout")-1,"wb",2);` |
|     - | 2743 | `		/* Initialize the handle */` |
|    25 | 2744 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|     - | 2745 | `		/* Install the STDOUT stream */` |
|    25 | 2746 | `		pVm->pStdout = pOut;` |
|     - | 2747 | `` 		/* The VM itself holds one: a script that does `$x = STDOUT; $x = null;` `` |
|     - | 2748 | `		 * must not close the process's own output -- php's standard handles` |
|     - | 2749 | `		 * outlive every value that names them. */` |
|    25 | 2750 | `		PH7_StreamValueRef(pOut);` |
|    25 | 2751 | `		return pOut;` |
|   ! 0 | 2752 | `	}else{` |
|     - | 2753 | `		/* NULL or STDOUT */` |
|   162 | 2754 | `		return pVm->pStdout;` |
|     - | 2755 | `	}` |
|     - | 2756 | `#else` |
|     - | 2757 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2758 | `	return 0;` |
|     - | 2759 | `#endif` |
|    92 | 2760 | `}` |
|     - | 2761 | `/*` |
|     - | 2762 | ` * Export the STDERR handle.` |
|     - | 2763 | ` */` |
|   219 | 2764 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|     5 | 2765 | `{` |
|     - | 2766 | `#ifndef PH7_DISABLE_DISK_IO` |
|   224 | 2767 | `	if( pVm->pStderr == 0  ){` |
|     - | 2768 | `		io_private *pErr;` |
|     - | 2769 | `		/* Allocate an IO private instance */` |
|    26 | 2770 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    26 | 2771 | `		if( pErr == 0 ){` |
|   ! 0 | 2772 | `			return 0;` |
|     - | 2773 | `		}` |
|    26 | 2774 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|    26 | 2775 | `		SetIOPrivateOpenedAs(pErr,"php://stderr",(int)sizeof("php://stderr")-1,"wb",2);` |
|     - | 2776 | `		/* Initialize the handle */` |
|    26 | 2777 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|     - | 2778 | `		/* Install the STDERR stream */` |
|    26 | 2779 | `		pVm->pStderr = pErr;` |
|     - | 2780 | `` 		/* The VM itself holds one: a script that does `$x = STDOUT; $x = null;` `` |
|     - | 2781 | `		 * must not close the process's own output -- php's standard handles` |
|     - | 2782 | `		 * outlive every value that names them. */` |
|    26 | 2783 | `		PH7_StreamValueRef(pErr);` |
|    26 | 2784 | `		return pErr;` |
|   ! 0 | 2785 | `	}else{` |
|     - | 2786 | `		/* NULL or STDERR */` |
|   203 | 2787 | `		return pVm->pStderr;` |
|     - | 2788 | `	}` |
|     - | 2789 | `#else` |
|     - | 2790 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2791 | `	return 0;` |
|     - | 2792 | `#endif` |
|    93 | 2793 | `}` |
|     - | 2794 |  |
