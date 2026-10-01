# src/ph7/vfs_io_driver.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1145/1410 lines (81.21%)

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
|   707 |  102 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|     5 |  103 | `{` |
|     - |  104 | `	ph7_stream_data *pData;` |
|   712 |  105 | `	if( pVm == 0 ){` |
|   ! 0 |  106 | `		return 0;` |
|     - |  107 | `	}` |
|     - |  108 | `	/* Allocate a new instance */` |
|   712 |  109 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|   712 |  110 | `	if( pData == 0 ){` |
|   ! 0 |  111 | `		return 0;` |
|     - |  112 | `	}` |
|     - |  113 | `	/* Zero the structure */` |
|   712 |  114 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|     - |  115 | `	/* Initialize fields */` |
|   712 |  116 | `	pData->iType = iType;` |
|   712 |  117 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|   712 |  118 | `	pData->nCur = 0;` |
|   712 |  119 | `	pData->bReadOnly = 0;` |
|   712 |  120 | `	pData->iPos = 0;` |
|   712 |  121 | `	pData->iFd = -1;` |
|   712 |  122 | `	pData->bNoSeek = 0;` |
|   712 |  123 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|     - |  124 | `		/* Nothing else to set up: the buffer is the stream */` |
|   414 |  125 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|     - |  126 | `		/* Point to the default VM consumer routine. */` |
|     9 |  127 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|     5 |  128 | `	}else{` |
|     - |  129 | `#ifdef __WINNT__` |
|     - |  130 | `		DWORD nChannel;` |
|     4 |  131 | `		switch(iType){` |
|     3 |  132 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|     3 |  133 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|     - |  134 | `		default:` |
|     4 |  135 | `			nChannel = STD_INPUT_HANDLE;` |
|     - |  136 | `			break;` |
|     - |  137 | `		}` |
|     4 |  138 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|     - |  139 | `		/* The CRT descriptor beside that handle, for the positional ops -- but` |
|     - |  140 | `		 * only when there IS a standard handle. A host with none (a GUI process` |
|     - |  141 | `		 * that never had a console) leaves the CRT descriptor unopened, and the` |
|     - |  142 | `		 * CRT's answer to a call on one of those is its invalid-parameter` |
|     - |  143 | `		 * handler rather than an error code. */` |
|     4 |  144 | `		if( pData->x.pHandle != NULL && pData->x.pHandle != INVALID_HANDLE_VALUE ){` |
|     4 |  145 | `			pData->iFd = iType == PH7_IO_STREAM_STDOUT ? 1 : (iType == PH7_IO_STREAM_STDERR ? 2 : 0);` |
|     4 |  146 | `			pData->iPos = (ph7_int64)_lseeki64(pData->iFd,0,SEEK_CUR);` |
|     4 |  147 | `		}else{` |
|   ! 0 |  148 | `			pData->iPos = -1;` |
|     - |  149 | `		}` |
|     4 |  150 | `		pData->bNoSeek = pData->iPos < 0;` |
|     - |  151 | `#else` |
|     - |  152 | `		/* Assume an UNIX system */` |
|   108 |  153 | `		int ifd = STDIN_FILENO;` |
|   108 |  154 | `		switch(iType){` |
|    15 |  155 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|    17 |  156 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|    38 |  157 | `		default:` |
|    76 |  158 | `			break;` |
|     - |  159 | `		}` |
|   108 |  160 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|   108 |  161 | `		pData->iFd = ifd;` |
|     - |  162 | `		/* php asks the descriptor where it is exactly once, at open, and keeps` |
|     - |  163 | `		 * the answer -- including the -1 a pipe or a terminal gives back. */` |
|   108 |  164 | `		pData->iPos = (ph7_int64)lseek(ifd,0,SEEK_CUR);` |
|   108 |  165 | `		pData->bNoSeek = pData->iPos < 0;` |
|     - |  166 | `#endif` |
|     - |  167 | `	}` |
|   712 |  168 | `	pData->pVm = pVm;` |
|   712 |  169 | `	return pData;` |
|   356 |  170 | `}` |
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
|     - |  258 | `/* Does this php:// name say EXACTLY zWant? php matches its sub-stream names` |
|     - |  259 | `` * whole (case-insensitively) rather than by prefix, and only `temp` may carry`` |
|     - |  260 | ` * anything after it. */` |
|  3482 |  261 | `static int PhpStreamNameIs(const SyString *pName,const char *zWant)` |
|     5 |  262 | `{` |
|  3487 |  263 | `	sxu32 n = SyStrlen(zWant);` |
|  3487 |  264 | `	return pName->nByte == n && SyStrnicmp(pName->zString,zWant,n) == 0;` |
|     5 |  265 | `}` |
|     - |  266 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|   635 |  267 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     5 |  268 | `{` |
|     - |  269 | `	ph7_stream_data *pData;` |
|     - |  270 | `	SyString sStream;` |
|   640 |  271 | `	int iOpenFlags = iMode;   /* iMode is overwritten with the sub-stream below */` |
|   640 |  272 | `	int bTemp = 0;` |
|   635 |  273 | `	if( SyStrnicmp(zName,"filter",sizeof("filter")-1) == 0` |
|   344 |  274 | `	 && (zName[6] == '/' \|\| zName[6] == 0) ){` |
|     - |  275 | `		int rc;` |
|    49 |  276 | `		if( zName[6] == 0 ){` |
|     - |  277 | `			/* php://filter with nothing behind it is not a stream at all. */` |
|   ! 0 |  278 | `			if( pResource && pResource->pVm ){` |
|   ! 0 |  279 | `				PH7_VmThrowError(pResource->pVm,pResource->pVm->pCalleeName,` |
|     - |  280 | `					PH7_CTX_WARNING,"Invalid php:// URL specified");` |
|   ! 0 |  281 | `			}` |
|   ! 0 |  282 | `			return -1;` |
|     - |  283 | `		}` |
|    49 |  284 | `		if( pResource == 0 \|\| pResource->pVm == 0 ){` |
|   ! 0 |  285 | `			return -1;` |
|     - |  286 | `		}` |
|    49 |  287 | `		rc = PHPStreamFilterOpen(&zName[7],(int)SyStrlen(&zName[7]),iMode,pResource->pVm,&pData);` |
|    49 |  288 | `		if( rc != PH7_OK ){` |
|     3 |  289 | `			return -1;` |
|     - |  290 | `		}` |
|    46 |  291 | `		*ppHandle = (void *)pData;` |
|    46 |  292 | `		return PH7_OK;` |
|     - |  293 | `	}` |
|   594 |  294 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|     - |  295 | `	/* Trim leading and trailing white spaces */` |
|   594 |  296 | `	SyStringFullTrim(&sStream);` |
|     - |  297 | ``	/* Stream to open. Every name but `temp` has to match EXACTLY: php refuses`` |
|     - |  298 | ``	 * `php://memoryx`, `php://memory/` and `php://inputx` outright, and a`` |
|     - |  299 | ``	 * prefix test accepted all three -- `php://memoryx` opened a memory stream`` |
|     - |  300 | ``	 * where php has none. `temp` is php's one exception, because`` |
|     - |  301 | ``	 * `php://temp/maxmemory:1024` names the same device (and php accepts`` |
|     - |  302 | ``	 * `php://tempx` with it, prefix and all). */`` |
|   594 |  303 | `	if( PhpStreamNameIs(&sStream,"stdin") ){` |
|   ! 0 |  304 | `		iMode = PH7_IO_STREAM_STDIN;` |
|   594 |  305 | `	}else if( PhpStreamNameIs(&sStream,"output") ){` |
|     9 |  306 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|   590 |  307 | `	}else if( PhpStreamNameIs(&sStream,"stdout") ){` |
|   ! 0 |  308 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|   586 |  309 | `	}else if( PhpStreamNameIs(&sStream,"stderr") ){` |
|   ! 0 |  310 | `		iMode = PH7_IO_STREAM_STDERR;` |
|   586 |  311 | `	}else if( PhpStreamNameIs(&sStream,"input") ){` |
|    21 |  312 | `		iMode = PH7_IO_STREAM_INPUT;` |
|   572 |  313 | `	}else if( PhpStreamNameIs(&sStream,"memory")` |
|   311 |  314 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|     - |  315 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|     - |  316 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|   548 |  317 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|   548 |  318 | `		bTemp = SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0;` |
|   275 |  319 | `	}else{` |
|     - |  320 | `		/* An unknown php:// name is php's own diagnostic, raised BESIDE the` |
|     - |  321 | `		 * caller's "Failed to open stream" rather than instead of it — the same` |
|     - |  322 | ``		 * sentence the empty `php://filter` above already raised. */`` |
|    20 |  323 | `		if( pResource && pResource->pVm ){` |
|    20 |  324 | `			PH7_VmThrowError(pResource->pVm,pResource->pVm->pCalleeName,` |
|     - |  325 | `				PH7_CTX_WARNING,"Invalid php:// URL specified");` |
|     9 |  326 | `		}` |
|    20 |  327 | `		return -1;` |
|     - |  328 | `	}` |
|     - |  329 | `	/* Create our handle */` |
|   576 |  330 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|   576 |  331 | `	if( pData == 0 ){` |
|   ! 0 |  332 | `		return -1;` |
|     - |  333 | `	}` |
|   576 |  334 | `	pData->bTemp = bTemp;` |
|   860 |  335 | `	pData->bReadOnly = iMode == PH7_IO_STREAM_INPUT` |
|  1118 |  336 | `		\|\| (iMode == PH7_IO_STREAM_MEMORY` |
|   547 |  337 | `		 && (iOpenFlags & (PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_TRUNC\|PH7_IO_OPEN_APPEND)) == 0);` |
|     - |  338 | `	/* Make the handle public */` |
|   576 |  339 | `	*ppHandle = (void *)pData;` |
|   576 |  340 | `	return PH7_OK;` |
|   321 |  341 | `}` |
|     - |  342 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|  1732 |  343 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|     5 |  344 | `{` |
|  1737 |  345 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  1737 |  346 | `	if( pData == 0 ){` |
|   ! 0 |  347 | `		return -1;` |
|     - |  348 | `	}` |
|  1737 |  349 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     - |  350 | `		/* Through the shared reader, which is where the chain runs. */` |
|    72 |  351 | `		return PH7_StreamRead(pData->pInner,pBuffer,nDatatoRead);` |
|     - |  352 | `	}` |
|  1667 |  353 | `	if( pData->iType == PH7_IO_STREAM_MEMORY \|\| pData->iType == PH7_IO_STREAM_INPUT ){` |
|  1663 |  354 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|     - |  355 | `		sxu32 nRead;` |
|  1663 |  356 | `		if( pData->nCur >= nAvail ){` |
|   287 |  357 | `			return 0; /* EOF */` |
|     - |  358 | `		}` |
|  1381 |  359 | `		nRead = nAvail - pData->nCur;` |
|  1381 |  360 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|  1074 |  361 | `			nRead = (sxu32)nDatatoRead;` |
|   536 |  362 | `		}` |
|  1381 |  363 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|  1381 |  364 | `		pData->nCur += nRead;` |
|  1381 |  365 | `		return (ph7_int64)nRead;` |
|     - |  366 | `	}` |
|     5 |  367 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|     - |  368 | `		/* Forbidden */` |
|   ! 0 |  369 | `		return -1;` |
|     - |  370 | `	}` |
|     - |  371 | `#ifdef __WINNT__` |
|     - |  372 | `	{` |
|     - |  373 | `		DWORD nRd;` |
|     - |  374 | `		BOOL rc;` |
|     1 |  375 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|     1 |  376 | `		if( !rc ){` |
|     - |  377 | `			/* IO error */` |
|   ! 0 |  378 | `			return -1;` |
|     - |  379 | `		}` |
|     1 |  380 | `		pData->iPos += (ph7_int64)nRd;` |
|     1 |  381 | `		return (ph7_int64)nRd;` |
|     - |  382 | `	}` |
|     - |  383 | `#elif defined(__UNIXES__)` |
|     - |  384 | `	{` |
|     - |  385 | `		ssize_t nRd;` |
|     - |  386 | `		int fd;` |
|     4 |  387 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|     4 |  388 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|     4 |  389 | `		if( nRd < 0 ){` |
|   ! 0 |  390 | `			return -1;` |
|     - |  391 | `		}` |
|     4 |  392 | `		pData->iPos += (ph7_int64)nRd;` |
|     - |  393 | `		/* ZERO is end of file, not an error — the contract every other device` |
|     - |  394 | `		 * here keeps. Collapsing the two meant nothing could ever latch EOF on` |
|     - |  395 | ``		 * php://stdin, so `while (!feof(STDIN))` never ended. */`` |
|     4 |  396 | `		return (ph7_int64)nRd;` |
|     - |  397 | `	}` |
|     - |  398 | `#else` |
|     - |  399 | `	return -1;` |
|     - |  400 | `#endif` |
|   871 |  401 | `}` |
|     - |  402 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|   399 |  403 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|     4 |  404 | `{` |
|   403 |  405 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   403 |  406 | `	if( pData == 0 ){` |
|   ! 0 |  407 | `		return -1;` |
|     - |  408 | `	}` |
|   403 |  409 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     7 |  410 | `		return PH7_StreamWrite(pData->pInner,pBuf,nWrite);` |
|     - |  411 | `	}` |
|   397 |  412 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|     - |  413 | `		/* Forbidden */` |
|   ! 0 |  414 | `		return -1;` |
|   397 |  415 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY \|\| pData->iType == PH7_IO_STREAM_INPUT ){` |
|     - |  416 | `		sxu32 nLen,nEnd;` |
|   342 |  417 | `		if( pData->bReadOnly ){` |
|    13 |  418 | `			return -1;` |
|     - |  419 | `		}` |
|   330 |  420 | `		nLen = SyBlobLength(&pData->sMem);` |
|   330 |  421 | `		if( pData->nCur > nLen ){` |
|     - |  422 | `			/* seek past end: php zero-fills the gap */` |
|     - |  423 | `			static const char zZero[64] = {0};` |
|   ! 0 |  424 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|   ! 0 |  425 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|   ! 0 |  426 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|   ! 0 |  427 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|   ! 0 |  428 | `					return -1;` |
|     - |  429 | `				}` |
|   ! 0 |  430 | `			}` |
|   ! 0 |  431 | `			nLen = SyBlobLength(&pData->sMem);` |
|   ! 0 |  432 | `		}` |
|   330 |  433 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|   330 |  434 | `		if( pData->nCur < nLen ){` |
|     - |  435 | `			/* overwrite in place up to the current end */` |
|     8 |  436 | `			sxu32 nOver = nLen - pData->nCur;` |
|     8 |  437 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|    11 |  438 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|    11 |  439 | `			if( nEnd > nLen ){` |
|   ! 0 |  440 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|   ! 0 |  441 | `					return -1;` |
|     - |  442 | `				}` |
|   ! 0 |  443 | `			}` |
|     5 |  444 | `		}else{` |
|   324 |  445 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|   ! 0 |  446 | `				return -1;` |
|     - |  447 | `			}` |
|     - |  448 | `		}` |
|   330 |  449 | `		pData->nCur = nEnd;` |
|   330 |  450 | `		return nWrite;` |
|    56 |  451 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|     3 |  452 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|     - |  453 | `		int rc;` |
|     - |  454 | `		/* Call the vm output consumer */` |
|     3 |  455 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|     3 |  456 | `		if( rc == PH7_ABORT ){` |
|   ! 0 |  457 | `			return -1;` |
|     - |  458 | `		}` |
|     3 |  459 | `		pData->iPos += nWrite;` |
|     3 |  460 | `		return nWrite;` |
|     - |  461 | `	}` |
|     - |  462 | `#ifdef __WINNT__` |
|     - |  463 | `	{` |
|     - |  464 | `		DWORD nWr;` |
|     - |  465 | `		BOOL rc;` |
|   ! 0 |  466 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|   ! 0 |  467 | `		if( !rc ){` |
|     - |  468 | `			/* IO error */` |
|   ! 0 |  469 | `			return -1;` |
|     - |  470 | `		}` |
|   ! 0 |  471 | `		pData->iPos += (ph7_int64)nWr;` |
|   ! 0 |  472 | `		return (ph7_int64)nWr;` |
|     - |  473 | `	}` |
|     - |  474 | `#elif defined(__UNIXES__)` |
|     - |  475 | `	{` |
|     - |  476 | `		ssize_t nWr;` |
|     - |  477 | `		int fd;` |
|    53 |  478 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    53 |  479 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    53 |  480 | `		if( nWr < 1 ){` |
|   ! 0 |  481 | `			return -1;` |
|     - |  482 | `		}` |
|    53 |  483 | `		pData->iPos += (ph7_int64)nWr;` |
|    53 |  484 | `		return (ph7_int64)nWr;` |
|     - |  485 | `	}` |
|     - |  486 | `#else` |
|     - |  487 | `	return -1;` |
|     - |  488 | `#endif` |
|   182 |  489 | `}` |
|     - |  490 | `/* void (*xClose)(void *) */` |
|   571 |  491 | `static void PHPStreamData_Close(void *pHandle)` |
|     5 |  492 | `{` |
|   576 |  493 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     - |  494 | `	ph7_vm *pVm;` |
|   576 |  495 | `	if( pData == 0 ){` |
|   ! 0 |  496 | `		return;` |
|     - |  497 | `	}` |
|   576 |  498 | `	pVm = pData->pVm;` |
|   576 |  499 | `	if( pData->iType == PH7_IO_STREAM_FILTER && pData->pInner ){` |
|     - |  500 | `		/* The write chain closes while the device below is still open. */` |
|    46 |  501 | `		PH7_StreamFilterReleaseChains(pData->pInner);` |
|    46 |  502 | `		if( pData->pInner->pStream ){` |
|    46 |  503 | `			PH7_StreamCloseHandle(pData->pInner->pStream,pData->pInner->pHandle);` |
|    22 |  504 | `		}` |
|    46 |  505 | `		SyBlobRelease(&pData->pInner->sBuffer);` |
|    46 |  506 | `		SyBlobRelease(&pData->pInner->sFilt);` |
|    46 |  507 | `		SyBlobRelease(&pData->pInner->sUri);` |
|    46 |  508 | `		SyMemBackendFree(&pVm->sAllocator,pData->pInner);` |
|    46 |  509 | `		pData->pInner = 0;` |
|    22 |  510 | `	}` |
|   576 |  511 | `	SyBlobRelease(&pData->sMem);` |
|     - |  512 | `	/* Free the instance */` |
|   576 |  513 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|   289 |  514 | `}` |
|     - |  515 | `/* int (*xSeek)(void *,ph7_int64,int) */` |
|   388 |  516 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|     4 |  517 | `{` |
|   392 |  518 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     - |  519 | `	ph7_int64 iNew;` |
|   392 |  520 | `	if( pData == 0 ){` |
|   ! 0 |  521 | `		return -1;` |
|     - |  522 | `	}` |
|   392 |  523 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     9 |  524 | `		return PH7_StreamSeekWrapped(pData->pInner,iOfft,whence);` |
|     - |  525 | `	}` |
|   384 |  526 | `	if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|     - |  527 | ``		/* php's output stream has no seek at all -- `fseek(): Stream does not`` |
|     - |  528 | ``		 * support seeking`, which is what the unsupported code asks for. */`` |
|     7 |  529 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  530 | `	}` |
|   378 |  531 | `	if( pData->iType != PH7_IO_STREAM_MEMORY && pData->iType != PH7_IO_STREAM_INPUT ){` |
|     - |  532 | `		/* A standard descriptor seeks exactly as far as the descriptor does,` |
|     - |  533 | `		 * and php refuses the seek OUTRIGHT when the descriptor could not say` |
|     - |  534 | ``		 * where it was at open -- so `php -r … > pipe` warns while the same`` |
|     - |  535 | `		 * script redirected to a file seeks. */` |
|   ! 0 |  536 | `		if( pData->bNoSeek ){` |
|   ! 0 |  537 | `			return SXERR_NOTIMPLEMENTED;` |
|     - |  538 | `		}` |
|     - |  539 | `		{` |
|     - |  540 | `#ifdef __WINNT__` |
|   ! 0 |  541 | `			ph7_int64 iNewPos = pData->iFd < 0 ? -1` |
|     - |  542 | `				: (ph7_int64)_lseeki64(pData->iFd,iOfft,whence);` |
|     - |  543 | `#else` |
|   ! 0 |  544 | `			ph7_int64 iNewPos = (ph7_int64)lseek(pData->iFd,(off_t)iOfft,whence);` |
|     - |  545 | `#endif` |
|   ! 0 |  546 | `			if( iNewPos < 0 ){` |
|   ! 0 |  547 | `				return -1;` |
|     - |  548 | `			}` |
|   ! 0 |  549 | `			pData->iPos = iNewPos;` |
|   ! 0 |  550 | `			return PH7_OK;` |
|     - |  551 | `		}` |
|     - |  552 | `	}` |
|   378 |  553 | `	switch(whence){` |
|    37 |  554 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|     7 |  555 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|   338 |  556 | `	default:            iNew = iOfft; break;` |
|     - |  557 | `	}` |
|   378 |  558 | `	if( iNew < 0 ){` |
|   ! 0 |  559 | `		return -1;` |
|     - |  560 | `	}` |
|   378 |  561 | `	pData->nCur = (sxu32)iNew;` |
|   378 |  562 | `	return PH7_OK;` |
|   198 |  563 | `}` |
|     - |  564 | `/* ph7_int64 (*xTell)(void *) */` |
|   294 |  565 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|     4 |  566 | `{` |
|   298 |  567 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   298 |  568 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|     - |  569 | `		/* Where the SCRIPT is on the wrapped stream: the device sits past` |
|     - |  570 | `		 * whatever the chain has already produced and nobody has taken. */` |
|    15 |  571 | `		return PH7_StreamLogicalTell(pData->pInner);` |
|     - |  572 | `	}` |
|   284 |  573 | `	if( pData == 0 ){` |
|   ! 0 |  574 | `		return -1;` |
|     - |  575 | `	}` |
|   284 |  576 | `	if( pData->iType != PH7_IO_STREAM_MEMORY && pData->iType != PH7_IO_STREAM_INPUT ){` |
|     - |  577 | `		/* The COUNTER, not the descriptor: php never re-asks, which is why a` |
|     - |  578 | `		 * write to a non-seekable stdout moves the position it reports even` |
|     - |  579 | `		 * though nothing can seek there. */` |
|     3 |  580 | `		return pData->iPos;` |
|     - |  581 | `	}` |
|   282 |  582 | `	return (ph7_int64)pData->nCur;` |
|   151 |  583 | `}` |
|     - |  584 | `/* int (*xSync)(void *)` |
|     - |  585 | ` *` |
|     - |  586 | ` * Nothing on this device buffers, so a flush has nothing to do and SUCCEEDS --` |
|     - |  587 | ` * which is what php answers for php://memory, php://temp, php://output and the` |
|     - |  588 | ` * three standard descriptors. php://input is the one stream in the family whose` |
|     - |  589 | `` * flush op FAILS, and `fflush(fopen(php://input))` is php's `false`. */`` |
|     6 |  590 | `static int PHPStreamData_Sync(void *pHandle)` |
|     1 |  591 | `{` |
|     7 |  592 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     7 |  593 | `	if( pData == 0 ){` |
|   ! 0 |  594 | `		return -1;` |
|     - |  595 | `	}` |
|     7 |  596 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|   ! 0 |  597 | `		io_private *pIn = pData->pInner;` |
|   ! 0 |  598 | `		if( pIn == 0 \|\| pIn->pStream == 0 \|\| pIn->pStream->xSync == 0 ){` |
|   ! 0 |  599 | `			return PH7_OK;` |
|     - |  600 | `		}` |
|   ! 0 |  601 | `		return pIn->pStream->xSync(pIn->pHandle);` |
|     - |  602 | `	}` |
|     7 |  603 | `	return pData->iType == PH7_IO_STREAM_INPUT ? -1 : PH7_OK;` |
|     4 |  604 | `}` |
|     - |  605 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|    26 |  606 | `static int PHPStreamData_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|     1 |  607 | `{` |
|    27 |  608 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    27 |  609 | `	if( pData == 0 ){` |
|   ! 0 |  610 | `		return -1;` |
|     - |  611 | `	}` |
|    27 |  612 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|   ! 0 |  613 | `		io_private *pIn = pData->pInner;` |
|   ! 0 |  614 | `		if( pIn == 0 \|\| pIn->pStream == 0 \|\| pIn->pStream->xStat == 0 ){` |
|   ! 0 |  615 | `			return -1;` |
|     - |  616 | `		}` |
|   ! 0 |  617 | `		return pIn->pStream->xStat(pIn->pHandle,pArray,pWorker);` |
|     - |  618 | `	}` |
|    27 |  619 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     - |  620 | `		/* php's memory streams -- php://memory, php://temp and the data://` |
|     - |  621 | `		 * payloads that share them -- answer a SYNTHETIC record describing no` |
|     - |  622 | `		 * file at all: a regular-file mode of 0666, php's own 0xC device, and` |
|     - |  623 | `		 * -1 in the three fields a buffer cannot have. Only the size is real.` |
|     - |  624 | `		 * Read back from php 8.5.9 and confirmed identical on Windows. */` |
|     - |  625 | `		ph7_int64 aVal[13];` |
|    23 |  626 | `		aVal[0]  = 0xC;                                   /* dev */` |
|    23 |  627 | `		aVal[1]  = 0;                                     /* ino */` |
|    23 |  628 | `		aVal[2]  = 0100000 \| (pData->bReadOnly ? 0444 : 0666); /* mode: S_IFREG\|… */` |
|    23 |  629 | `		aVal[3]  = 1;                                     /* nlink */` |
|    23 |  630 | `		aVal[4]  = 0;                                     /* uid */` |
|    23 |  631 | `		aVal[5]  = 0;                                     /* gid */` |
|    23 |  632 | `		aVal[6]  = -1;                                    /* rdev */` |
|    23 |  633 | `		aVal[7]  = (ph7_int64)SyBlobLength(&pData->sMem); /* size */` |
|    23 |  634 | `		aVal[8]  = 0;                                     /* atime */` |
|    23 |  635 | `		aVal[9]  = 0;                                     /* mtime */` |
|    23 |  636 | `		aVal[10] = 0;                                     /* ctime */` |
|    23 |  637 | `		aVal[11] = -1;                                    /* blksize */` |
|    23 |  638 | `		aVal[12] = -1;                                    /* blocks */` |
|    23 |  639 | `		return PH7_VfsStatFill(pArray,pWorker,aVal);` |
|     - |  640 | `	}` |
|     5 |  641 | `	if( pData->iType == PH7_IO_STREAM_OUTPUT \|\| pData->iType == PH7_IO_STREAM_INPUT ){` |
|     - |  642 | `		/* Neither has a descriptor to stat: php's fstat() is false for both. */` |
|     5 |  643 | `		return -1;` |
|     - |  644 | `	}` |
|   ! 0 |  645 | `	return PH7_VfsStatFromFd(pData->iFd,pArray,pWorker);` |
|    14 |  646 | `}` |
|     - |  647 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|    20 |  648 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|     1 |  649 | `{` |
|    21 |  650 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    21 |  651 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|   ! 0 |  652 | `		io_private *pIn = pData->pInner;` |
|   ! 0 |  653 | `		if( pIn == 0 \|\| pIn->pStream == 0 \|\| pIn->pStream->xTrunc == 0 ){` |
|   ! 0 |  654 | `			return -1;` |
|     - |  655 | `		}` |
|   ! 0 |  656 | `		return pIn->pStream->xTrunc(pIn->pHandle,nLen);` |
|     - |  657 | `	}` |
|    21 |  658 | `	if( pData == 0 ){` |
|   ! 0 |  659 | `		return -1;` |
|     - |  660 | `	}` |
|    21 |  661 | `	if( pData->iType == PH7_IO_STREAM_OUTPUT \|\| pData->iType == PH7_IO_STREAM_INPUT ){` |
|     - |  662 | ``		/* Nothing to truncate: php's `Can't truncate this stream!`. */`` |
|     5 |  663 | `		return SXERR_NOTIMPLEMENTED;` |
|     - |  664 | `	}` |
|    17 |  665 | `	if( pData->iType == PH7_IO_STREAM_MEMORY && pData->bReadOnly ){` |
|     - |  666 | `		/* php's read-only memory stream answers a SILENT false. */` |
|     7 |  667 | `		return -1;` |
|     - |  668 | `	}` |
|    11 |  669 | `	if( pData->iType != PH7_IO_STREAM_MEMORY ){` |
|     - |  670 | `		/* A standard descriptor truncates whatever it points AT -- php does the` |
|     - |  671 | ``		 * same call, so `php … > out.txt` really does empty out.txt through`` |
|     - |  672 | `		 * ftruncate(STDOUT), and a pipe or a terminal answers a SILENT false. */` |
|     - |  673 | `#ifdef __WINNT__` |
|   ! 0 |  674 | `		return pData->iFd >= 0 && _chsize_s(pData->iFd,(__int64)nLen) == 0 ? PH7_OK : -1;` |
|     - |  675 | `#else` |
|   ! 0 |  676 | `		return ftruncate(pData->iFd,(off_t)nLen) == 0 ? PH7_OK : -1;` |
|     - |  677 | `#endif` |
|     - |  678 | `	}` |
|     - |  679 | `	/* php truncates a READ-ONLY memory stream happily -- data:// answers true` |
|     - |  680 | `	 * and its payload really does shrink, even though fwrite() to it is` |
|     - |  681 | `	 * refused. The two are separate permissions there, and they are here. */` |
|    11 |  682 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|     - |  683 | `		/* shrink in place: the blob keeps its allocation */` |
|     5 |  684 | `		pData->sMem.nByte = (sxu32)nLen;` |
|     3 |  685 | `	}else{` |
|     - |  686 | `		static const char zZero[64] = {0};` |
|    13 |  687 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|     7 |  688 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|     7 |  689 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|     7 |  690 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|   ! 0 |  691 | `				return -1;` |
|     - |  692 | `			}` |
|     1 |  693 | `		}` |
|     - |  694 | `	}` |
|    11 |  695 | `	return PH7_OK;` |
|    11 |  696 | `}` |
|     - |  697 | `/*` |
|     - |  698 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|     - |  699 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|     - |  700 | ` * base64). Shares the MEMORY machinery above.` |
|     - |  701 | ` */` |
|    20 |  702 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     2 |  703 | `{` |
|    22 |  704 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|     2 |  705 | `}` |
|    54 |  706 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     4 |  707 | `{` |
|     - |  708 | `	ph7_stream_data *pData;` |
|    58 |  709 | `	const char *zIn = zName;` |
|    58 |  710 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|    58 |  711 | `	const char *zComma = 0;` |
|    58 |  712 | `	int bBase64 = 0;` |
|    27 |  713 | `	SXUNUSED(iMode);` |
|     - |  714 | `	/* Find the comma separating the mediatype from the payload */` |
|   770 |  715 | `	while( zIn < zEnd ){` |
|   764 |  716 | `		if( zIn[0] == ',' ){` |
|    52 |  717 | `			zComma = zIn;` |
|    52 |  718 | `			break;` |
|     - |  719 | `		}` |
|   716 |  720 | `		zIn++;` |
|     4 |  721 | `	}` |
|    58 |  722 | `	if( zComma == 0 ){` |
|     - |  723 | `		/* php's own wording for the one thing its data wrapper checks. */` |
|     7 |  724 | `		if( pResource && pResource->pVm ){` |
|     7 |  725 | `			PH7_StreamSetOpenError(pResource->pVm,"rfc2397: no comma in URL");` |
|     3 |  726 | `		}` |
|     7 |  727 | `		return -1;` |
|     - |  728 | `	}` |
|    48 |  729 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|    50 |  730 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|    10 |  731 | `		bBase64 = 1;` |
|     4 |  732 | `	}` |
|    52 |  733 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|    52 |  734 | `	if( pData == 0 ){` |
|   ! 0 |  735 | `		return -1;` |
|     - |  736 | `	}` |
|    52 |  737 | `	zIn = &zComma[1];` |
|    52 |  738 | `	if( bBase64 ){` |
|    10 |  739 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|   ! 0 |  740 | `			SyBlobRelease(&pData->sMem);` |
|   ! 0 |  741 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|   ! 0 |  742 | `			return -1;` |
|     - |  743 | `		}` |
|     6 |  744 | `	}else{` |
|     - |  745 | `		/* percent-decode the payload */` |
|   248 |  746 | `		while( zIn < zEnd ){` |
|   208 |  747 | `			char c = zIn[0];` |
|   208 |  748 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|     3 |  749 | `				int hi = SyHexToint(zIn[1]);` |
|     3 |  750 | `				int lo = SyHexToint(zIn[2]);` |
|     3 |  751 | `				c = (char)((hi << 4) \| lo);` |
|     3 |  752 | `				zIn += 3;` |
|     2 |  753 | `			}else{` |
|   206 |  754 | `				zIn++;` |
|     - |  755 | `			}` |
|   208 |  756 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|   ! 0 |  757 | `				SyBlobRelease(&pData->sMem);` |
|   ! 0 |  758 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|   ! 0 |  759 | `				return -1;` |
|     - |  760 | `			}` |
|     4 |  761 | `		}` |
|     - |  762 | `	}` |
|    52 |  763 | `	*ppHandle = (void *)pData;` |
|    52 |  764 | `	return PH7_OK;` |
|    31 |  765 | `}` |
|     - |  766 | `PH7_PRIVATE const ph7_io_stream sDATA_Stream = {` |
|     - |  767 | `	"data",` |
|     - |  768 | `	PH7_IO_STREAM_VERSION,` |
|     - |  769 | `	DataStreamData_Open,  /* xOpen */` |
|     - |  770 | `	0,   /* xOpenDir */` |
|     - |  771 | `	PHPStreamData_Close, /* xClose */` |
|     - |  772 | `	0,  /* xCloseDir */` |
|     - |  773 | `	PHPStreamData_Read,  /* xRead */` |
|     - |  774 | `	0,  /* xReadDir */` |
|     - |  775 | `	/* xWrite: NONE. php's RFC 2397 stream has no writer at all, and its` |
|     - |  776 | ``	 * fwrite() is `Stream is not writable` rather than a failed write --`` |
|     - |  777 | `	 * a refusal that only a MISSING slot can produce. */` |
|     - |  778 | `	0,` |
|     - |  779 | `	PHPStreamData_Seek,  /* xSeek */` |
|     - |  780 | `	0,  /* xLock */` |
|     - |  781 | `	0,  /* xRewindDir */` |
|     - |  782 | `	PHPStreamData_Tell,  /* xTell */` |
|     - |  783 | `	PHPStreamData_Trunc, /* xTrunc: a data:// payload really does shrink */` |
|     - |  784 | `	0,  /* xSync */` |
|     - |  785 | `	PHPStreamData_Stat   /* xStat: the synthetic memory-stream record */` |
|     - |  786 | `};` |
|     - |  787 | `/*` |
|     - |  788 | ` * Pipe stream implementation for popen/pclose.` |
|     - |  789 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|     - |  790 | ` * PHP-compatible process I/O functionality.` |
|     - |  791 | ` */` |
|     - |  792 | `typedef struct pipe_private pipe_private;` |
|     - |  793 | `struct pipe_private` |
|     - |  794 | `{` |
|     - |  795 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|     - |  796 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|     - |  797 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|     - |  798 | `#ifdef __WINNT__` |
|     - |  799 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|     - |  800 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|     - |  801 | `#endif` |
|     - |  802 | `};` |
|     - |  803 |  |
|     - |  804 | `#ifdef __WINNT__` |
|     - |  805 | `#include <Windows.h>` |
|     - |  806 | `#include <stdio.h>` |
|     - |  807 | `#include <io.h>` |
|     - |  808 | `#include <fcntl.h>` |
|     - |  809 | `/*` |
|     - |  810 | ` * Custom Windows popen implementation using CreateProcess.` |
|     - |  811 | ` * This allows us to properly wait for process completion.` |
|     - |  812 | ` */` |
|     - |  813 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|     5 |  814 | `{` |
|     5 |  815 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|     5 |  816 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|     5 |  817 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|     - |  818 | `	SECURITY_ATTRIBUTES sa;` |
|     - |  819 | `	STARTUPINFOW si;` |
|     - |  820 | `	PROCESS_INFORMATION pi;` |
|     5 |  821 | `	WCHAR *zWideCmd = NULL;` |
|     5 |  822 | `	FILE *pFile = NULL;` |
|     - |  823 | `	int fd;` |
|     5 |  824 | `	BOOL bRead = (zMode[0] == 'r');` |
|     5 |  825 | `	BOOL bBinary = (strchr(zMode,'b') != NULL);` |
|     - |  826 |  |
|     - |  827 | `	/* Set up security attributes for pipe inheritance */` |
|     5 |  828 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|     5 |  829 | `	sa.bInheritHandle = TRUE;` |
|     5 |  830 | `	sa.lpSecurityDescriptor = NULL;` |
|     - |  831 |  |
|     - |  832 | `	/* Create pipes for child process I/O */` |
|     5 |  833 | `	if( bRead ){` |
|     - |  834 | `		/* Reading from child's stdout */` |
|     5 |  835 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|   ! 0 |  836 | `			return NULL;` |
|     - |  837 | `		}` |
|     - |  838 | `		/* Ensure read handle is not inherited */` |
|     5 |  839 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|     5 |  840 | `		hReadPipe = hChildStdoutRd;` |
|     5 |  841 | `		*phPipe = hChildStdoutRd;` |
|     5 |  842 | `	}else{` |
|     - |  843 | `		/* Writing to child's stdin */` |
|     2 |  844 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|   ! 0 |  845 | `			return NULL;` |
|     - |  846 | `		}` |
|     - |  847 | `		/* Ensure write handle is not inherited */` |
|     2 |  848 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|     2 |  849 | `		hWritePipe = hChildStdinWr;` |
|     2 |  850 | `		*phPipe = hChildStdinWr;` |
|     - |  851 | `	}` |
|     - |  852 |  |
|     - |  853 | `	/* Convert command to wide string */` |
|     - |  854 | `	{` |
|     5 |  855 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|     5 |  856 | `		if( nLen <= 0 ){` |
|   ! 0 |  857 | `			goto cleanup_pipes;` |
|     - |  858 | `		}` |
|     5 |  859 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|     5 |  860 | `		if( !zWideCmd ){` |
|   ! 0 |  861 | `			goto cleanup_pipes;` |
|     - |  862 | `		}` |
|     5 |  863 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|     - |  864 | `	}` |
|     - |  865 |  |
|     - |  866 | `	/* Set up process startup info */` |
|     5 |  867 | `	ZeroMemory(&si, sizeof(si));` |
|     5 |  868 | `	si.cb = sizeof(si);` |
|     5 |  869 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|     5 |  870 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|     5 |  871 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|     5 |  872 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|     5 |  873 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|     - |  874 |  |
|     5 |  875 | `	ZeroMemory(&pi, sizeof(pi));` |
|     - |  876 |  |
|     - |  877 | `	/* Create the child process */` |
|     5 |  878 | `	if( !CreateProcessW(` |
|     - |  879 | `		NULL,           /* Application name */` |
|     - |  880 | `		zWideCmd,       /* Command line */` |
|     - |  881 | `		NULL,           /* Process security attributes */` |
|     - |  882 | `		NULL,           /* Thread security attributes */` |
|     - |  883 | `		TRUE,           /* Inherit handles */` |
|     - |  884 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|     - |  885 | `		NULL,           /* Environment */` |
|     - |  886 | `		NULL,           /* Current directory */` |
|     - |  887 | `		&si,            /* Startup info */` |
|     - |  888 | `		&pi             /* Process info */` |
|     - |  889 | `	)){` |
|   ! 0 |  890 | `		goto cleanup_all;` |
|     - |  891 | `	}` |
|     - |  892 |  |
|     - |  893 | `	/* Close handles we don't need in parent */` |
|     5 |  894 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|     5 |  895 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|     - |  896 |  |
|     - |  897 | `	/* Close thread handle (we only need process handle) */` |
|     5 |  898 | `	CloseHandle(pi.hThread);` |
|     - |  899 |  |
|     - |  900 | `	/* Store process handle for later waiting */` |
|     5 |  901 | `	*phProcess = pi.hProcess;` |
|     - |  902 |  |
|     - |  903 | `	/* Convert OS handle to C file descriptor, then to FILE*. The TRANSLATION mode` |
|     - |  904 | `	 * is the caller's, not a constant: cmd.exe writes CRLF, and a descriptor` |
|     - |  905 | `	 * opened _O_TEXT eats every CR on the way in. php picks it per call —` |
|     - |  906 | `	 * "rb" for exec/system/passthru, so their output is byte-exact, "rt" for` |
|     - |  907 | `	 * shell_exec, and the script's own mode for popen() — and this used to` |
|     - |  908 | ``	 * hardcode _O_TEXT for all of them, so `passthru('type file.bin')` lost every`` |
|     - |  909 | `	 * 0x0D byte and system()'s output came back LF-only where php's is CRLF. */` |
|     5 |  910 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|     - |  911 | `	                     (bRead ? _O_RDONLY : _O_WRONLY)` |
|     - |  912 | `	                     \| (bBinary ? _O_BINARY : _O_TEXT));` |
|     5 |  913 | `	if( fd == -1 ){` |
|   ! 0 |  914 | `		CloseHandle(pi.hProcess);` |
|   ! 0 |  915 | `		*phProcess = NULL;` |
|   ! 0 |  916 | `		goto cleanup_all;` |
|     - |  917 | `	}` |
|     - |  918 |  |
|     - |  919 | `	/* The stream's own translation has to agree with the descriptor's */` |
|     5 |  920 | `	pFile = _fdopen(fd, bBinary ? (bRead ? "rb" : "wb") : (bRead ? "rt" : "wt"));` |
|     5 |  921 | `	if( !pFile ){` |
|   ! 0 |  922 | `		_close(fd); /* This will also close the underlying handle */` |
|   ! 0 |  923 | `		CloseHandle(pi.hProcess);` |
|   ! 0 |  924 | `		*phProcess = NULL;` |
|   ! 0 |  925 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|   ! 0 |  926 | `		return NULL;` |
|     - |  927 | `	}` |
|     - |  928 |  |
|     5 |  929 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|     5 |  930 | `	return pFile;` |
|     - |  931 |  |
|     - |  932 | `cleanup_all:` |
|   ! 0 |  933 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|     - |  934 | `cleanup_pipes:` |
|   ! 0 |  935 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|   ! 0 |  936 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|   ! 0 |  937 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|   ! 0 |  938 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|   ! 0 |  939 | `	return NULL;` |
|     5 |  940 | `}` |
|     - |  941 |  |
|     - |  942 | `/*` |
|     - |  943 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|     - |  944 | ` */` |
|     - |  945 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|     5 |  946 | `{` |
|     5 |  947 | `	DWORD dwExitCode = 0;` |
|     - |  948 | `	int status;` |
|     - |  949 |  |
|     - |  950 | `	/* Close the FILE* (this closes the pipe) */` |
|     5 |  951 | `	fclose(pFile);` |
|     - |  952 |  |
|     5 |  953 | `	if( hProcess ){` |
|     - |  954 | `		/* Wait for the process to complete */` |
|     5 |  955 | `		WaitForSingleObject(hProcess, INFINITE);` |
|     - |  956 |  |
|     5 |  957 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|     5 |  958 | `			status = (int)dwExitCode;` |
|     5 |  959 | `		}else{` |
|   ! 0 |  960 | `			status = -1;` |
|     - |  961 | `		}` |
|     - |  962 |  |
|     - |  963 | `		/* Close process handle */` |
|     5 |  964 | `		CloseHandle(hProcess);` |
|     5 |  965 | `	}else{` |
|   ! 0 |  966 | `		status = -1;` |
|     - |  967 | `	}` |
|     - |  968 |  |
|     5 |  969 | `	return status;` |
|     5 |  970 | `}` |
|     - |  971 | `#endif /* __WINNT__ */` |
|     - |  972 | `/*` |
|     - |  973 | ` * Open a pipe to a process.` |
|     - |  974 | ` * This is called internally by popen(), not through the stream device interface.` |
|     - |  975 | ` */` |
|  6737 |  976 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|     5 |  977 | `{` |
|     - |  978 | `	pipe_private *pPipe;` |
|     - |  979 | `	FILE *pFile;` |
|  6742 |  980 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|   ! 0 |  981 | `		return 0;` |
|     - |  982 | `	}` |
|     - |  983 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|  6742 |  984 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|   ! 0 |  985 | `		return 0;` |
|     - |  986 | `	}` |
|     - |  987 | `	/* Open the pipe using system popen */` |
|     - |  988 | `#ifdef __WINNT__` |
|     - |  989 | `	{` |
|     - |  990 | `		/* Build cmd.exe command wrapper */` |
|     5 |  991 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|     5 |  992 | `		const char *zShellSuffix = "\"";` |
|     5 |  993 | `		size_t nPrefix = strlen(zShellPrefix);` |
|     5 |  994 | `		size_t nSuffix = strlen(zShellSuffix);` |
|     5 |  995 | `		size_t nCmd = strlen(zCommand);` |
|     5 |  996 | `		size_t nQuotes = 0;` |
|     5 |  997 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|     5 |  998 | `			if (zCommand[i] == '"') nQuotes++;` |
|     5 |  999 | `		}` |
|     5 | 1000 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|     5 | 1001 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|     5 | 1002 | `		if (zCmdEsc == NULL) {` |
|   ! 0 | 1003 | `			return 0;` |
|     - | 1004 | `		}` |
|     - | 1005 | `		/* Escape quotes in command */` |
|     5 | 1006 | `		size_t j = 0;` |
|     5 | 1007 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|     5 | 1008 | `			char ch = zCommand[i];` |
|     5 | 1009 | `			if (ch == '"') {` |
|     4 | 1010 | `				zCmdEsc[j++] = '^';` |
|     4 | 1011 | `				zCmdEsc[j++] = '"';` |
|     4 | 1012 | `			} else {` |
|     5 | 1013 | `				zCmdEsc[j++] = ch;` |
|     - | 1014 | `			}` |
|     5 | 1015 | `		}` |
|     5 | 1016 | `		zCmdEsc[j] = '\0';` |
|     5 | 1017 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|     5 | 1018 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|     5 | 1019 | `		if (zWinCmd == NULL) {` |
|   ! 0 | 1020 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|   ! 0 | 1021 | `			return 0;` |
|     - | 1022 | `		}` |
|     5 | 1023 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|     5 | 1024 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|     5 | 1025 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|     5 | 1026 | `		zWinCmd[nTotal - 1] = '\0';` |
|     - | 1027 | `		/* Allocate pipe structure early so we can store handles */` |
|     5 | 1028 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|     5 | 1029 | `		if( pPipe == 0 ){` |
|   ! 0 | 1030 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|   ! 0 | 1031 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|   ! 0 | 1032 | `			return 0;` |
|     - | 1033 | `		}` |
|     - | 1034 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|     5 | 1035 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|     5 | 1036 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|     5 | 1037 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|     5 | 1038 | `		if( pFile == 0 ){` |
|   ! 0 | 1039 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   ! 0 | 1040 | `			return 0;` |
|     - | 1041 | `		}` |
|     - | 1042 | `		/* Initialize remaining fields */` |
|     5 | 1043 | `		pPipe->pFile = pFile;` |
|     5 | 1044 | `		pPipe->pVm = pVm;` |
|     5 | 1045 | `		pPipe->iMode = zMode[0];` |
|     - | 1046 | `	}` |
|     - | 1047 | `#elif defined(__UNIXES__) /* Unix */` |
|     - | 1048 | `	/* The mode goes to popen(3) VERBATIM, exactly as php hands it its own: a mode` |
|     - | 1049 | `	 * popen(3) refuses (anything but "r"/"w" — 'b' is a Windows translation flag` |
|     - | 1050 | `	 * with nothing to translate here) is an open FAILURE, which is php's answer` |
|     - | 1051 | `	 * for it too. */` |
|  6737 | 1052 | `	pFile = popen(zCommand, zMode);` |
|  6737 | 1053 | `	if( pFile == 0 ){` |
|     2 | 1054 | `		return 0;` |
|     - | 1055 | `	}` |
|     - | 1056 | `	/* Allocate pipe private structure */` |
|  6735 | 1057 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|  6735 | 1058 | `	if( pPipe == 0 ){` |
|     - | 1059 | `		/* Out of memory, close the pipe */` |
|   ! 0 | 1060 | `		pclose(pFile);` |
|   ! 0 | 1061 | `		return 0;` |
|     - | 1062 | `	}` |
|     - | 1063 | `	/* Initialize the structure */` |
|  6735 | 1064 | `	pPipe->pFile = pFile;` |
|  6735 | 1065 | `	pPipe->pVm = pVm;` |
|  6735 | 1066 | `	pPipe->iMode = zMode[0];` |
|     - | 1067 | `#else /* OS_OTHER: no process pipes on this platform */` |
|     - | 1068 | `	(void)pFile;` |
|     - | 1069 | `	return 0;` |
|     - | 1070 | `#endif` |
|  6740 | 1071 | `	return pPipe;` |
|  3369 | 1072 | `}` |
|     - | 1073 | `/*` |
|     - | 1074 | ` * Close a pipe and return the exit status of the process.` |
|     - | 1075 | ` * Returns the exit status, or -1 on error.` |
|     - | 1076 | ` */` |
|  6703 | 1077 | `static int PipeClose(pipe_private *pPipe)` |
|     5 | 1078 | `{` |
|     - | 1079 | `	int status;` |
|     - | 1080 | `	ph7_vm *pVm;` |
|  6708 | 1081 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1082 | `		return -1;` |
|     - | 1083 | `	}` |
|  6708 | 1084 | `	pVm = pPipe->pVm;` |
|     - | 1085 | `	/* Close the pipe and get exit status */` |
|     - | 1086 | `#ifdef __WINNT__` |
|     - | 1087 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|     5 | 1088 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|     - | 1089 | `#elif defined(__UNIXES__)` |
|  6703 | 1090 | `	status = pclose(pPipe->pFile);` |
|     - | 1091 | `	/* pclose() answers waitpid()'s raw status. php (php_stream_pclose) translates` |
|     - | 1092 | `	 * exactly ONE case of it — a normal exit becomes its exit CODE — and hands the` |
|     - | 1093 | `	 * raw word back for every other, so a process killed by a signal reports the` |
|     - | 1094 | ``	 * SIGNAL number: `kill -TERM $$` is 15, `kill -9 $$` is 9. PHL used to add the`` |
|     - | 1095 | `	 * shell's own 128 to it (143, 137), a number the same status word can also` |
|     - | 1096 | ``	 * mean as an ordinary `exit 143`, and answered -1 for a stopped child. This is`` |
|     - | 1097 | `	 * pclose()'s answer and, through it, exec()/system()/passthru()'s` |
|     - | 1098 | `	 * $result_code. */` |
|  6703 | 1099 | `	if( status != -1 && WIFEXITED(status) ){` |
|  6699 | 1100 | `		status = WEXITSTATUS(status);` |
|  3345 | 1101 | `	}` |
|     - | 1102 | `#else /* OS_OTHER: no process pipes on this platform */` |
|     - | 1103 | `	status = -1;` |
|     - | 1104 | `#endif` |
|     - | 1105 | `	/* Free the structure */` |
|  6708 | 1106 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|  6708 | 1107 | `	return status;` |
|  3352 | 1108 | `}` |
|     - | 1109 | `/*` |
|     - | 1110 | ` * Pipe stream xClose implementation.` |
|     - | 1111 | ` * Note: This is called by fclose(), not pclose().` |
|     - | 1112 | ` * It closes the pipe but does not return the exit status.` |
|     - | 1113 | ` */` |
|   128 | 1114 | `static void PipeStream_Close(void *pHandle)` |
|     4 | 1115 | `{` |
|   132 | 1116 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|   132 | 1117 | `	if( pPipe ){` |
|   132 | 1118 | `		PipeClose(pPipe);` |
|    64 | 1119 | `	}` |
|   132 | 1120 | `}` |
|     - | 1121 | `/*` |
|     - | 1122 | ` * Pipe stream xRead implementation.` |
|     - | 1123 | ` */` |
|  9520 | 1124 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|     4 | 1125 | `{` |
|  9524 | 1126 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     - | 1127 | `	size_t nRead;` |
|  9524 | 1128 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1129 | `		return -1;` |
|     - | 1130 | `	}` |
|  9524 | 1131 | `	if( pPipe->iMode != 'r' ){` |
|     - | 1132 | `		/* Cannot read from a write-only pipe */` |
|   ! 0 | 1133 | `		return -1;` |
|     - | 1134 | `	}` |
|  9524 | 1135 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|  9524 | 1136 | `	if( nRead == 0 ){` |
|  6104 | 1137 | `		if( feof(pPipe->pFile) ){` |
|  6104 | 1138 | `			return 0; /* EOF */` |
|     - | 1139 | `		}` |
|   ! 0 | 1140 | `		return -1; /* Error */` |
|     - | 1141 | `	}` |
|  3424 | 1142 | `	return (ph7_int64)nRead;` |
|  4760 | 1143 | `}` |
|     - | 1144 | `/*` |
|     - | 1145 | ` * Pipe stream xWrite implementation.` |
|     - | 1146 | ` */` |
|     6 | 1147 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|     1 | 1148 | `{` |
|     7 | 1149 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     - | 1150 | `	size_t nWritten;` |
|     7 | 1151 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1152 | `		return -1;` |
|     - | 1153 | `	}` |
|     7 | 1154 | `	if( pPipe->iMode != 'w' ){` |
|     - | 1155 | `		/* Cannot write to a read-only pipe */` |
|   ! 0 | 1156 | `		return -1;` |
|     - | 1157 | `	}` |
|     7 | 1158 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|     7 | 1159 | `	if( nWritten == 0 && nWrite > 0 ){` |
|   ! 0 | 1160 | `		return -1; /* Error */` |
|     - | 1161 | `	}` |
|     7 | 1162 | `	return (ph7_int64)nWritten;` |
|     4 | 1163 | `}` |
|     - | 1164 | `/*` |
|     - | 1165 | ` * A pipe's descriptor, for the two ops that go past the FILE*. php's popen` |
|     - | 1166 | ` * stream is an ordinary stdio stream, so its truncate and its stat are the` |
|     - | 1167 | ` * plain system calls -- which is exactly why ftruncate() on a pipe is a SILENT` |
|     - | 1168 | ` * false (the call is made and EINVAL comes back) where a socket, which has no` |
|     - | 1169 | ` * truncate at all, warns instead.` |
|     - | 1170 | ` */` |
|   ! 0 | 1171 | `static int PipeStream_Fd(void *pHandle)` |
|   ! 0 | 1172 | `{` |
|   ! 0 | 1173 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|   ! 0 | 1174 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1175 | `		return -1;` |
|     - | 1176 | `	}` |
|     - | 1177 | `#ifdef __WINNT__` |
|   ! 0 | 1178 | `	return _fileno(pPipe->pFile);` |
|     - | 1179 | `#else` |
|   ! 0 | 1180 | `	return fileno(pPipe->pFile);` |
|     - | 1181 | `#endif` |
|   ! 0 | 1182 | `}` |
|     - | 1183 | `/* int (*xTrunc)(void *,ph7_int64) */` |
|   ! 0 | 1184 | `static int PipeStream_Trunc(void *pHandle,ph7_int64 nLen)` |
|   ! 0 | 1185 | `{` |
|   ! 0 | 1186 | `	int fd = PipeStream_Fd(pHandle);` |
|   ! 0 | 1187 | `	if( fd < 0 ){` |
|   ! 0 | 1188 | `		return -1;` |
|     - | 1189 | `	}` |
|     - | 1190 | `#ifdef __WINNT__` |
|   ! 0 | 1191 | `	return _chsize_s(fd,(__int64)nLen) == 0 ? PH7_OK : -1;` |
|     - | 1192 | `#else` |
|   ! 0 | 1193 | `	return ftruncate(fd,(off_t)nLen) == 0 ? PH7_OK : -1;` |
|     - | 1194 | `#endif` |
|   ! 0 | 1195 | `}` |
|     - | 1196 | `/* int (*xStat)(void *,ph7_value *,ph7_value *) */` |
|   ! 0 | 1197 | `static int PipeStream_Stat(void *pHandle,ph7_value *pArray,ph7_value *pWorker)` |
|   ! 0 | 1198 | `{` |
|   ! 0 | 1199 | `	return PH7_VfsStatFromFd(PipeStream_Fd(pHandle),pArray,pWorker);` |
|   ! 0 | 1200 | `}` |
|     - | 1201 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|     - | 1202 | `static const ph7_io_stream sPipe_Stream = {` |
|     - | 1203 | `	"pipe",` |
|     - | 1204 | `	PH7_IO_STREAM_VERSION,` |
|     - | 1205 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|     - | 1206 | `	0,  /* xOpenDir */` |
|     - | 1207 | `	PipeStream_Close,  /* xClose */` |
|     - | 1208 | `	0,  /* xCloseDir */` |
|     - | 1209 | `	PipeStream_Read,   /* xRead */` |
|     - | 1210 | `	0,  /* xReadDir */` |
|     - | 1211 | `	PipeStream_Write,  /* xWrite */` |
|     - | 1212 | `	0,  /* xSeek */` |
|     - | 1213 | `	0,  /* xLock */` |
|     - | 1214 | `	0,  /* xRewindDir */` |
|     - | 1215 | `	0,  /* xTell: php's pipe has none either -- the stream layer COUNTS */` |
|     - | 1216 | `	PipeStream_Trunc, /* xTrunc */` |
|     - | 1217 | `	0,  /* xSync */` |
|     - | 1218 | `	PipeStream_Stat   /* xStat */` |
|     - | 1219 | `};` |
|     - | 1220 | `/*` |
|     - | 1221 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|     - | 1222 | ` * FALSE otherwise.` |
|     - | 1223 | ` */` |
|  6076 | 1224 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|     5 | 1225 | `{` |
|  6081 | 1226 | `	return pStream == &sPipe_Stream;` |
|     5 | 1227 | `}` |
|     - | 1228 | `/*` |
|     - | 1229 | ` * resource popen(string $command, string $mode)` |
|     - | 1230 | ` *  Opens process file pointer.` |
|     - | 1231 | ` * Parameters` |
|     - | 1232 | ` *  $command` |
|     - | 1233 | ` *   The command to execute. Passed to the system shell.` |
|     - | 1234 | ` *  $mode` |
|     - | 1235 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|     - | 1236 | ` *   'r' - Open for reading (read from the command's stdout).` |
|     - | 1237 | ` *   'w' - Open for writing (write to the command's stdin).` |
|     - | 1238 | ` * Return` |
|     - | 1239 | ` *  Returns a file pointer on success, or FALSE on error.` |
|     - | 1240 | ` */` |
|     - | 1241 | `/*` |
|     - | 1242 | `` * The longest command line the platform's shell accepts — php's `cmd_max_len`,`` |
|     - | 1243 | ` * which both escapers refuse to exceed. php reads it once at startup from` |
|     - | 1244 | ` * sysconf(_SC_ARG_MAX) and hardcodes cmd.exe's constant on Windows.` |
|     - | 1245 | ` */` |
|  1030 | 1246 | `static sxu32 ShellMaxCmdLen(void)` |
|     5 | 1247 | `{` |
|     - | 1248 | `#ifdef __WINNT__` |
|     - | 1249 | `	/* An escaped command runs through cmd.exe, whose limit is a constant. */` |
|     5 | 1250 | `	return 8192;` |
|     - | 1251 | `#elif defined(__UNIXES__) && defined(_SC_ARG_MAX)` |
|  1030 | 1252 | `	long iMax = sysconf(_SC_ARG_MAX);` |
|  1030 | 1253 | `	if( iMax <= 0 ){` |
|   ! 0 | 1254 | `		return 4096;   /* php's _POSIX_ARG_MAX fallback */` |
|     - | 1255 | `	}` |
|  1030 | 1256 | `	return (sxu32)iMax;` |
|     - | 1257 | `#else` |
|     - | 1258 | `	return 4096;` |
|     - | 1259 | `#endif` |
|   519 | 1260 | `}` |
|     - | 1261 | `/*` |
|     - | 1262 | ` * How the two escapers WALK their argument, and the one thing they share.` |
|     - | 1263 | ` *` |
|     - | 1264 | ` * php walks it with php_mblen(), the process LC_CTYPE's multibyte reader: a` |
|     - | 1265 | ` * well-formed sequence is copied through untouched (a metacharacter's byte value` |
|     - | 1266 | ` * inside one is NOT a metacharacter), and a byte the encoding cannot start a` |
|     - | 1267 | ` * character with is DROPPED. That reader's answer is platform-shaped, and this` |
|     - | 1268 | ` * follows it on both, because it is what php answers on each:` |
|     - | 1269 | ` *` |
|     - | 1270 | ` *   POSIX    php picks LC_CTYPE up from the environment at startup, so the` |
|     - | 1271 | ` *            everyday answer is a UTF-8 one — a well-formed sequence rides` |
|     - | 1272 | ` *            through and an ill-formed byte is dropped. PHL is UTF-8-only (§10)` |
|     - | 1273 | ` *            and has no setlocale, so PH7_Utf8ReadStrict IS that reader.` |
|     - | 1274 | ` *            (php in the "C" locale glibc falls back to drops every byte >= 0x80` |
|     - | 1275 | ` *            instead, which is why the corpus guards this half on the oracle's` |
|     - | 1276 | ` *            own LC_CTYPE rather than pinning it unconditionally.)` |
|     - | 1277 | ` *   Windows  php reports LC_CTYPE "C", and MSVCRT's C locale is SINGLE-BYTE, not` |
|     - | 1278 | ` *            ASCII: mblen() answers 1 for every byte, so nothing is ever dropped` |
|     - | 1279 | `` *            and `\xFF` reaches the escape table below. Verified against php`` |
|     - | 1280 | ` *            8.5.8 on the gate VM, which does have an oracle — walking UTF-8` |
|     - | 1281 | ` *            there instead deleted bytes php keeps.` |
|     - | 1282 | ` *` |
|     - | 1283 | ` * Neither escaper can see a NUL byte: php parses both parameters with` |
|     - | 1284 | ` * Z_PARAM_PATH and the central screen (VmBuiltinPathMask) refuses one first.` |
|     - | 1285 | ` */` |
| 50315 | 1286 | `static int ShellCharIsWellFormed(const unsigned char *zIn,sxu32 nLeft,sxu32 *pnSeq)` |
|     5 | 1287 | `{` |
|     - | 1288 | `#ifdef __WINNT__` |
|     - | 1289 | `	SXUNUSED(zIn);` |
|     - | 1290 | `	SXUNUSED(nLeft);` |
|     5 | 1291 | `	*pnSeq = 1;` |
|     5 | 1292 | `	return 1;` |
|     - | 1293 | `#else` |
| 50315 | 1294 | `	return PH7_Utf8ReadStrict(zIn,nLeft,pnSeq) >= 0;` |
|     - | 1295 | `#endif` |
|     5 | 1296 | `}` |
|     - | 1297 | `/*` |
|     - | 1298 | ` * php's escapeshellarg(): wrap the whole argument in quotes the shell does not` |
|     - | 1299 | ` * look inside, and neutralise the one byte that could end them.` |
|     - | 1300 | ` *` |
|     - | 1301 | `` * POSIX: single quotes, and a `'` becomes `'\''` — close, escape, reopen.`` |
|     - | 1302 | `` * Windows: double quotes; there is no in-quote escape for `"` on cmd.exe, so php`` |
|     - | 1303 | `` * REPLACES `"` (and `%`/`!`, which cmd.exe still expands inside quotes) with a`` |
|     - | 1304 | ` * space, and doubles a trailing ODD run of backslashes so the last one escapes` |
|     - | 1305 | ` * itself rather than the closing quote.` |
|     - | 1306 | ` */` |
|   964 | 1307 | `static void ShellEscapeArg(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|     5 | 1308 | `{` |
|   969 | 1309 | `	sxu32 i = 0;` |
|     - | 1310 | `#ifdef __WINNT__` |
|     5 | 1311 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|     - | 1312 | `#else` |
|   964 | 1313 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|     - | 1314 | `#endif` |
| 50904 | 1315 | `	while( i < nLen ){` |
| 49940 | 1316 | `		sxu32 nSeq = 1;` |
| 49940 | 1317 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|     - | 1318 | `			/* Ill-formed: php skips the byte rather than escaping it */` |
|    18 | 1319 | `			i += nSeq;` |
|    22 | 1320 | `			continue;` |
|     - | 1321 | `		}` |
| 49922 | 1322 | `		if( nSeq > 1 ){` |
|     8 | 1323 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|     8 | 1324 | `			i += nSeq;` |
|     8 | 1325 | `			continue;` |
|     - | 1326 | `		}` |
|     - | 1327 | `#ifdef __WINNT__` |
|     5 | 1328 | `		if( zIn[i] == '"' \|\| zIn[i] == '%' \|\| zIn[i] == '!' ){` |
|     1 | 1329 | `			SyBlobAppend(pOut," ",sizeof(char));` |
|     1 | 1330 | `		}else{` |
|     5 | 1331 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|     - | 1332 | `		}` |
|     - | 1333 | `#else` |
| 49909 | 1334 | `		if( zIn[i] == '\'' ){` |
|     8 | 1335 | `			SyBlobAppend(pOut,"'\\'",sizeof("'\\'")-1);` |
|     4 | 1336 | `		}` |
| 49909 | 1337 | `		SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|     - | 1338 | `#endif` |
| 49914 | 1339 | `		i++;` |
|     5 | 1340 | `	}` |
|     - | 1341 | `#ifdef __WINNT__` |
|     - | 1342 | `	{` |
|     - | 1343 | `		/* A trailing run of backslashes would escape the closing quote if it is` |
|     - | 1344 | `		 * odd; the opening quote at offset 0 stops the scan the way php's does. */` |
|     5 | 1345 | `		const char *zCur = (const char *)SyBlobData(pOut);` |
|     5 | 1346 | `		sxu32 nCur = SyBlobLength(pOut);` |
|     5 | 1347 | `		sxu32 k = 0;` |
|     5 | 1348 | `		while( k < nCur && zCur[nCur - 1 - k] == '\\' ){` |
|     1 | 1349 | `			k++;` |
|     1 | 1350 | `		}` |
|     5 | 1351 | `		if( (k & 1) != 0 ){` |
|     1 | 1352 | `			SyBlobAppend(pOut,"\\",sizeof(char));` |
|     - | 1353 | `		}` |
|     - | 1354 | `	}` |
|     5 | 1355 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|     - | 1356 | `#else` |
|   964 | 1357 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|     - | 1358 | `#endif` |
|   969 | 1359 | `}` |
|     - | 1360 | `/*` |
|     - | 1361 | ` * php's escapeshellcmd(): the argument is a COMMAND, so it is not quoted at all —` |
|     - | 1362 | ` * every byte that could break out of one is prefixed with the shell's escape` |
|     - | 1363 | `` * character instead (`\` on POSIX, `^` on cmd.exe).`` |
|     - | 1364 | ` *` |
|     - | 1365 | ` * The one shape that is not a straight escape is a quote on POSIX: php leaves a` |
|     - | 1366 | ` * PAIR of them alone (the command may legitimately quote one of its own` |
|     - | 1367 | `` * arguments) and escapes an unpaired one. `pPair` is php's own one-slot state for`` |
|     - | 1368 | ` * that — it remembers the partner it found for the quote currently open, so the` |
|     - | 1369 | ` * closing one is recognised and the pairing resets. cmd.exe has no such rule, so` |
|     - | 1370 | `` * both quote characters (and `%`/`!`) are ordinary escapes there.`` |
|     - | 1371 | ` */` |
|    64 | 1372 | `static void ShellEscapeCmd(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|     1 | 1373 | `{` |
|    65 | 1374 | `	sxu32 i = 0;` |
|     - | 1375 | `#ifndef __WINNT__` |
|    64 | 1376 | `	const unsigned char *pPair = 0;` |
|     - | 1377 | `	static const char zEsc[] = "\\";` |
|     - | 1378 | `#else` |
|     - | 1379 | `	static const char zEsc[] = "^";` |
|     - | 1380 | `#endif` |
|   445 | 1381 | `	while( i < nLen ){` |
|   381 | 1382 | `		sxu32 nSeq = 1;` |
|   381 | 1383 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|    18 | 1384 | `			i += nSeq;` |
|    22 | 1385 | `			continue;` |
|     - | 1386 | `		}` |
|   363 | 1387 | `		if( nSeq > 1 ){` |
|     8 | 1388 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|     8 | 1389 | `			i += nSeq;` |
|     8 | 1390 | `			continue;` |
|     - | 1391 | `		}` |
|   355 | 1392 | `		switch( zIn[i] ){` |
|     - | 1393 | `#ifndef __WINNT__` |
|    12 | 1394 | `		case '"':` |
|     - | 1395 | `		case '\'':` |
|    19 | 1396 | `			if( pPair == 0` |
|    14 | 1397 | `			 && (pPair = (const unsigned char *)memchr(&zIn[i+1],zIn[i],nLen - i - 1)) != 0 ){` |
|     - | 1398 | `				/* This quote opens a pair: leave both of them alone */` |
|    17 | 1399 | `			}else if( pPair != 0 && pPair[0] == zIn[i] ){` |
|     8 | 1400 | `				pPair = 0;   /* the partner: pairing satisfied */` |
|     4 | 1401 | `			}else{` |
|     8 | 1402 | `				SyBlobAppend(pOut,zEsc,sizeof(char));` |
|     - | 1403 | `			}` |
|    24 | 1404 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|    24 | 1405 | `			break;` |
|     - | 1406 | `#else` |
|     - | 1407 | `		/* cmd.exe expands %VAR% and !VAR! even inside quotes, and has no` |
|     - | 1408 | ``		 * in-quote escape, so all four are plain `^` escapes there. */`` |
|     - | 1409 | `		case '%':` |
|     - | 1410 | `		case '!':` |
|     - | 1411 | `		case '"':` |
|     - | 1412 | `		case '\'':` |
|     - | 1413 | `#endif` |
|    21 | 1414 | `		case '#':` |
|     - | 1415 | `		case '&':` |
|     - | 1416 | `		case ';':` |
|     - | 1417 | ``		case '`':`` |
|     - | 1418 | `		case '\|':` |
|     - | 1419 | `		case '*':` |
|     - | 1420 | `		case '?':` |
|     - | 1421 | `		case '~':` |
|     - | 1422 | `		case '<':` |
|     - | 1423 | `		case '>':` |
|     - | 1424 | `		case '^':` |
|     - | 1425 | `		case '(':` |
|     - | 1426 | `		case ')':` |
|     - | 1427 | `		case '[':` |
|     - | 1428 | `		case ']':` |
|     - | 1429 | `		case '{':` |
|     - | 1430 | `		case '}':` |
|     - | 1431 | `		case '$':` |
|     - | 1432 | `		case '\\':` |
|     - | 1433 | `		case 0x0A:` |
|     - | 1434 | `		/* php escapes 0xFF too, and this is the row that decides the walk above` |
|     - | 1435 | `		 * is worth getting right: it is unreachable under the UTF-8 walk (0xF5..` |
|     - | 1436 | `		 * 0xFF is never a lead byte, so the reader drops the byte first) and` |
|     - | 1437 | `		 * REACHED on Windows, where php's single-byte C locale hands it here —` |
|     - | 1438 | ``		 * `escapeshellcmd("a\xffb")` is `a^\xffb` on the oracle. */`` |
|     - | 1439 | `		case 0xFF:` |
|    43 | 1440 | `			SyBlobAppend(pOut,zEsc,sizeof(char));` |
|     - | 1441 | `			/* fall through */` |
|   165 | 1442 | `		default:` |
|   331 | 1443 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|   330 | 1444 | `			break;` |
|     - | 1445 | `		}` |
|   355 | 1446 | `		i++;` |
|     1 | 1447 | `	}` |
|    65 | 1448 | `}` |
|     - | 1449 | `/*` |
|     - | 1450 | ` * string escapeshellarg(string $arg)` |
|     - | 1451 | ` *  Escape an argument so a shell passes it to the command as ONE word, whatever` |
|     - | 1452 | ` *  it contains.` |
|     - | 1453 | ` */` |
|   964 | 1454 | `PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1455 | `{` |
|   969 | 1456 | `	sxu32 nMax = ShellMaxCmdLen();` |
|     - | 1457 | `	const char *zArg;` |
|     - | 1458 | `	SyBlob sOut;` |
|     - | 1459 | `	int nLen;` |
|   481 | 1460 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|   969 | 1461 | `	zArg = ph7_value_to_string(apArg[0],&nLen);` |
|     - | 1462 | `	/* php's own bound: the command line has to hold the two quotes and a NUL */` |
|   969 | 1463 | `	if( nLen > 0 && (sxu32)nLen > nMax - 3 ){` |
|   ! 0 | 1464 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1465 | `			"Argument exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1466 | `	}` |
|   969 | 1467 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   969 | 1468 | `	ShellEscapeArg((const unsigned char *)zArg,(sxu32)nLen,&sOut);` |
|   969 | 1469 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|   ! 0 | 1470 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1471 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1472 | `			"Escaped argument exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1473 | `	}` |
|   969 | 1474 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   969 | 1475 | `	SyBlobRelease(&sOut);` |
|   969 | 1476 | `	return PH7_OK;` |
|   486 | 1477 | `}` |
|     - | 1478 | `/*` |
|     - | 1479 | ` * string escapeshellcmd(string $command)` |
|     - | 1480 | ` *  Escape every character that could break out of a shell command.` |
|     - | 1481 | ` */` |
|    66 | 1482 | `PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1483 | `{` |
|    67 | 1484 | `	sxu32 nMax = ShellMaxCmdLen();` |
|     - | 1485 | `	const char *zCmd;` |
|     - | 1486 | `	SyBlob sOut;` |
|     - | 1487 | `	int nLen;` |
|    33 | 1488 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|    67 | 1489 | `	zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|    67 | 1490 | `	if( nLen < 1 ){` |
|     - | 1491 | `		/* php answers "" without running the escaper at all */` |
|     3 | 1492 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 1493 | `		return PH7_OK;` |
|     - | 1494 | `	}` |
|    65 | 1495 | `	if( (sxu32)nLen > nMax - 3 ){` |
|   ! 0 | 1496 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1497 | `			"Command exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1498 | `	}` |
|    65 | 1499 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    65 | 1500 | `	ShellEscapeCmd((const unsigned char *)zCmd,(sxu32)nLen,&sOut);` |
|    65 | 1501 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|   ! 0 | 1502 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1503 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1504 | `			"Escaped command exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1505 | `	}` |
|    65 | 1506 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    65 | 1507 | `	SyBlobRelease(&sOut);` |
|    65 | 1508 | `	return PH7_OK;` |
|    34 | 1509 | `}` |
|     - | 1510 | `/*` |
|     - | 1511 | ` * php refuses an EMPTY command in all four runners (exec/system/passthru/` |
|     - | 1512 | ` * shell_exec) — the shell would answer success for one, so the refusal is the` |
|     - | 1513 | ` * only way a script hears about a command string that came out empty.` |
|     - | 1514 | ` */` |
|     8 | 1515 | `static sxi32 ShellEmptyCommandError(ph7_context *pCtx)` |
|     1 | 1516 | `{` |
|    13 | 1517 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     4 | 1518 | `		"%s(): Argument #1 ($command) must not be empty",ph7_function_name(pCtx));` |
|     1 | 1519 | `}` |
|     - | 1520 | `/*` |
|     - | 1521 | ` * string\|false\|null shell_exec(string $command)` |
|     - | 1522 | ` *  Execute a command via the shell and return the complete output as a string.` |
|     - | 1523 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|     - | 1524 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|     - | 1525 | ` */` |
|   402 | 1526 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1527 | `{` |
|     - | 1528 | `	const char *zCommand;` |
|     - | 1529 | `	pipe_private *pPipe;` |
|     - | 1530 | `	SyBlob sOut;` |
|     - | 1531 | `	char zBuf[4096];` |
|     - | 1532 | `	size_t nRead;` |
|     - | 1533 | `	int nCmdLen;` |
|   406 | 1534 | `	if( nArg < 1 ){` |
|   ! 0 | 1535 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1536 | `		return PH7_OK;` |
|     - | 1537 | `	}` |
|   406 | 1538 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|   406 | 1539 | `	if( nCmdLen < 1 ){` |
|     - | 1540 | `		/* php refuses an empty command rather than running the shell on it */` |
|     3 | 1541 | `		return ShellEmptyCommandError(pCtx);` |
|     - | 1542 | `	}` |
|   404 | 1543 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|   404 | 1544 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|     - | 1545 | `		/* php's own wording for this one; the three runners below say "Unable to` |
|     - | 1546 | `		 * fork [%s]" instead. Both used to be silent. */` |
|   ! 0 | 1547 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to execute '%s'",` |
|   ! 0 | 1548 | `			ph7_function_name(pCtx),zCommand);` |
|   ! 0 | 1549 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1550 | `		return PH7_OK;` |
|     - | 1551 | `	}` |
|   404 | 1552 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   363 | 1553 | `	for(;;){` |
|   730 | 1554 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|   730 | 1555 | `		if( nRead < 1 ){` |
|   404 | 1556 | `			break;` |
|     - | 1557 | `		}` |
|   330 | 1558 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|     4 | 1559 | `	}` |
|   404 | 1560 | `	PipeClose(pPipe);` |
|   404 | 1561 | `	if( SyBlobLength(&sOut) < 1 ){` |
|     - | 1562 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    77 | 1563 | `		ph7_result_null(pCtx);` |
|    40 | 1564 | `	}else{` |
|   330 | 1565 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     - | 1566 | `	}` |
|   404 | 1567 | `	SyBlobRelease(&sOut);` |
|   404 | 1568 | `	return PH7_OK;` |
|   205 | 1569 | `}` |
|     - | 1570 | `/*` |
|     - | 1571 | ` * php's three command RUNNERS are one routine (php_exec) with a mode, and the` |
|     - | 1572 | ` * mode decides two things: what happens to each LINE of the command's output,` |
|     - | 1573 | ` * and what the call answers.` |
|     - | 1574 | ` *` |
|     - | 1575 | ` *   exec($cmd)           keep nothing, answer the LAST line` |
|     - | 1576 | ` *   exec($cmd, $output)  append every line to the array, answer the last line` |
|     - | 1577 | ` *   system($cmd)         WRITE every line as it arrives, answer the last line` |
|     - | 1578 | ` *   passthru($cmd)       write the raw bytes, answer NULL` |
|     - | 1579 | ` *` |
|     - | 1580 | ` * "The last line" is php's: its trailing WHITESPACE is stripped — spaces and` |
|     - | 1581 | ` * tabs as much as the newline — and so is every element of $output's. A command` |
|     - | 1582 | ` * that printed nothing answers "" rather than false, which is php's documented` |
|     - | 1583 | ` * BC wart and not an error indication; the error indication is FALSE, and only` |
|     - | 1584 | ` * a pipe that could not be opened produces it.` |
|     - | 1585 | ` *` |
|     - | 1586 | ` * All three share the exit status, which is the pipe's close status (php's` |
|     - | 1587 | ` * $result_code out-param) and -1 when there was no process at all.` |
|     - | 1588 | ` */` |
|     - | 1589 | `#ifdef __WINNT__` |
|     - | 1590 | `# define SHELL_RUN_PIPE_MODE "rb"` |
|     - | 1591 | `#else` |
|     - | 1592 | `# define SHELL_RUN_PIPE_MODE "r"` |
|     - | 1593 | `#endif` |
|     - | 1594 | `#define SHELL_RUN_LAST     0   /* exec() with no $output array */` |
|     - | 1595 | `#define SHELL_RUN_ECHO     1   /* system() */` |
|     - | 1596 | `#define SHELL_RUN_COLLECT  2   /* exec() with one */` |
|     - | 1597 | `#define SHELL_RUN_RAW      3   /* passthru() */` |
|     - | 1598 | `/*` |
|     - | 1599 | ` * php's strip_trailing_whitespace(): answers the length that stays.` |
|     - | 1600 | ` */` |
|   205 | 1601 | `static sxu32 ShellStripTrailing(const char *zLine,sxu32 nLine)` |
|     2 | 1602 | `{` |
|   411 | 1603 | `	while( nLine > 0 && SyisSpace((unsigned char)zLine[nLine - 1]) ){` |
|   206 | 1604 | `		nLine--;` |
|     2 | 1605 | `	}` |
|   207 | 1606 | `	return nLine;` |
|     2 | 1607 | `}` |
|     - | 1608 | `/*` |
|     - | 1609 | ` * One complete line of output, dealt with the mode's way. The line still carries` |
|     - | 1610 | ` * its own newline: system() writes it (php hands the whole line to the output` |
|     - | 1611 | ` * layer, so an output buffer catches it like any echo), and the collector strips` |
|     - | 1612 | ` * it along with the rest of the trailing whitespace.` |
|     - | 1613 | ` */` |
|   122 | 1614 | `static sxi32 ShellHandleLine(ph7_context *pCtx,int iType,ph7_value *pArray,` |
|     - | 1615 | `	const char *zLine,sxu32 nLine)` |
|     2 | 1616 | `{` |
|   124 | 1617 | `	if( iType == SHELL_RUN_ECHO ){` |
|    12 | 1618 | `		return ph7_context_output(pCtx,zLine,(int)nLine);` |
|     - | 1619 | `	}` |
|   114 | 1620 | `	if( iType == SHELL_RUN_COLLECT && pArray ){` |
|   114 | 1621 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   114 | 1622 | `		if( pVal == 0 ){` |
|   ! 0 | 1623 | `			return PH7_OK;` |
|     - | 1624 | `		}` |
|   114 | 1625 | `		ph7_value_string(pVal,zLine,(int)ShellStripTrailing(zLine,nLine));` |
|   114 | 1626 | `		ph7_array_add_elem(pArray,0,pVal);` |
|   114 | 1627 | `		ph7_context_release_value(pCtx,pVal);` |
|    56 | 1628 | `	}` |
|   114 | 1629 | `	return PH7_OK;` |
|    63 | 1630 | `}` |
|     - | 1631 | `/*` |
|     - | 1632 | ` * Run $command through the shell in the given mode, fill the by-reference` |
|     - | 1633 | ` * out-params and set the call's result. The three builtins below are this` |
|     - | 1634 | ` * routine plus their own mode.` |
|     - | 1635 | ` */` |
|   105 | 1636 | `static sxi32 ShellRunCommand(ph7_context *pCtx,int iType,int nArg,ph7_value **apArg)` |
|     2 | 1637 | `{` |
|     - | 1638 | `	/* exec() carries $output before $result_code; the other two do not */` |
|   107 | 1639 | `	int iCodeArg = (iType == SHELL_RUN_LAST) ? 2 : 1;` |
|   107 | 1640 | `	ph7_value *pArray = 0, *pOwned = 0;` |
|     - | 1641 | `	const char *zCommand;` |
|     - | 1642 | `	pipe_private *pPipe;` |
|     - | 1643 | `	SyBlob sLine, sLast;` |
|     - | 1644 | `	char zBuf[4096];` |
|     - | 1645 | `	size_t nRead;` |
|   107 | 1646 | `	int nCmdLen, iStatus = -1;` |
|   107 | 1647 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|   107 | 1648 | `	if( nCmdLen < 1 ){` |
|     7 | 1649 | `		return ShellEmptyCommandError(pCtx);` |
|     - | 1650 | `	}` |
|     - | 1651 | `	/* $output turns exec() into the collecting mode. php uses the array the` |
|     - | 1652 | `	 * caller already holds — the manual's "will append to the end of the array" —` |
|     - | 1653 | `	 * and replaces anything else with a fresh one, BEFORE running the command, so` |
|     - | 1654 | `	 * even a failed run leaves the variable an array. */` |
|   101 | 1655 | `	if( iType == SHELL_RUN_LAST && nArg > 1 ){` |
|    87 | 1656 | `		iType = SHELL_RUN_COLLECT;` |
|    87 | 1657 | `		if( ph7_value_is_array(apArg[1]) ){` |
|    61 | 1658 | `			PH7_HashmapCowSeparate(pCtx->pVm,apArg[1]);` |
|    61 | 1659 | `			pArray = apArg[1];` |
|    31 | 1660 | `		}else{` |
|    26 | 1661 | `			pOwned = pArray = ph7_context_new_array(pCtx);` |
|     - | 1662 | `		}` |
|    42 | 1663 | `	}` |
|     - | 1664 | `	/* php_exec's own mode, per platform: the three runners hand back what the` |
|     - | 1665 | `	 * command WROTE, so on Windows the CRs have to survive the pipe (where` |
|     - | 1666 | `	 * shell_exec() takes php's "rt" and does translate them). popen(3) refuses a` |
|     - | 1667 | `	 * 'b' it has nothing to translate, which is why this is not one string. */` |
|   101 | 1668 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,SHELL_RUN_PIPE_MODE);` |
|   101 | 1669 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1670 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to fork [%s]",` |
|   ! 0 | 1671 | `			ph7_function_name(pCtx),zCommand);` |
|   ! 0 | 1672 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1673 | `	}else{` |
|   101 | 1674 | `		int bAbort = 0;   /* the output consumer asked to stop (PH7_ABORT) */` |
|   101 | 1675 | `		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|   101 | 1676 | `		SyBlobInit(&sLast,&pCtx->pVm->sAllocator);` |
|    92 | 1677 | `		for(;;){` |
|   193 | 1678 | `			nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|   193 | 1679 | `			if( nRead < 1 ){` |
|   101 | 1680 | `				break;` |
|     - | 1681 | `			}` |
|    94 | 1682 | `			if( iType == SHELL_RUN_RAW ){` |
|     - | 1683 | `				/* passthru() never looks for a line: php writes what it read */` |
|     8 | 1684 | `				if( ph7_context_output(pCtx,zBuf,(int)nRead) == PH7_ABORT ){` |
|   ! 0 | 1685 | `					break;` |
|     - | 1686 | `				}` |
|     8 | 1687 | `				continue;` |
|     - | 1688 | `			}` |
|     - | 1689 | `			{` |
|    88 | 1690 | `				size_t iOfft = 0;` |
|   198 | 1691 | `				while( iOfft < nRead ){` |
|   128 | 1692 | `					const char *zNl = (const char *)memchr(&zBuf[iOfft],'\n',nRead - iOfft);` |
|   128 | 1693 | `					size_t nChunk = zNl ? (size_t)(zNl - &zBuf[iOfft]) + 1 : nRead - iOfft;` |
|   128 | 1694 | `					SyBlobAppend(&sLine,&zBuf[iOfft],(sxu32)nChunk);` |
|   128 | 1695 | `					iOfft += nChunk;` |
|   128 | 1696 | `					if( zNl == 0 ){` |
|    17 | 1697 | `						break;   /* the line continues in the next read */` |
|     - | 1698 | `					}` |
|   165 | 1699 | `					if( ShellHandleLine(pCtx,iType,pArray,` |
|   167 | 1700 | `						(const char *)SyBlobData(&sLine),SyBlobLength(&sLine)) == PH7_ABORT ){` |
|   ! 0 | 1701 | `						bAbort = 1;` |
|   ! 0 | 1702 | `					}` |
|     - | 1703 | `					/* Keep it: the call answers the last line it saw */` |
|   112 | 1704 | `					SyBlobReset(&sLast);` |
|   112 | 1705 | `					SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|   112 | 1706 | `					SyBlobReset(&sLine);` |
|   112 | 1707 | `					if( bAbort ){` |
|   ! 0 | 1708 | `						break;` |
|     - | 1709 | `					}` |
|     2 | 1710 | `				}` |
|     - | 1711 | `			}` |
|    88 | 1712 | `			if( bAbort ){` |
|   ! 0 | 1713 | `				break;` |
|     - | 1714 | `			}` |
|     2 | 1715 | `		}` |
|     - | 1716 | `		/* Output that ended without a newline is still a line */` |
|   101 | 1717 | `		if( !bAbort && SyBlobLength(&sLine) > 0 ){` |
|    19 | 1718 | `			ShellHandleLine(pCtx,iType,pArray,` |
|    12 | 1719 | `				(const char *)SyBlobData(&sLine),SyBlobLength(&sLine));` |
|    13 | 1720 | `			SyBlobReset(&sLast);` |
|    13 | 1721 | `			SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|     6 | 1722 | `		}` |
|   101 | 1723 | `		iStatus = PipeClose(pPipe);` |
|   101 | 1724 | `		if( iType == SHELL_RUN_RAW ){` |
|     8 | 1725 | `			ph7_result_null(pCtx);` |
|     5 | 1726 | `		}else{` |
|   141 | 1727 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&sLast),` |
|    93 | 1728 | `				(int)ShellStripTrailing((const char *)SyBlobData(&sLast),SyBlobLength(&sLast)));` |
|     - | 1729 | `		}` |
|   101 | 1730 | `		SyBlobRelease(&sLine);` |
|   101 | 1731 | `		SyBlobRelease(&sLast);` |
|     - | 1732 | `	}` |
|   101 | 1733 | `	if( pOwned ){` |
|    26 | 1734 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOwned);` |
|    26 | 1735 | `		ph7_context_release_value(pCtx,pOwned);` |
|    12 | 1736 | `	}` |
|    73 | 1737 | `	if( nArg > iCodeArg ){` |
|     - | 1738 | `		ph7_value sVal;` |
|    44 | 1739 | `		PH7_MemObjInitFromInt(pCtx->pVm,&sVal,iStatus);` |
|    44 | 1740 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iCodeArg],&sVal);` |
|    44 | 1741 | `		PH7_MemObjRelease(&sVal);` |
|    21 | 1742 | `	}` |
|   101 | 1743 | `	return PH7_OK;` |
|    54 | 1744 | `}` |
|     - | 1745 | `/*` |
|     - | 1746 | ` * string\|false exec(string $command, array &$output = null, int &$result_code = null)` |
|     - | 1747 | ` *  Run a command and answer the last line of its output.` |
|     - | 1748 | ` */` |
|    89 | 1749 | `PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1750 | `{` |
|    91 | 1751 | `	return ShellRunCommand(pCtx,SHELL_RUN_LAST,nArg,apArg);` |
|     2 | 1752 | `}` |
|     - | 1753 | `/*` |
|     - | 1754 | ` * string\|false system(string $command, int &$result_code = null)` |
|     - | 1755 | ` *  Run a command, write its output as it arrives, answer the last line.` |
|     - | 1756 | ` */` |
|     8 | 1757 | `PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1758 | `{` |
|    10 | 1759 | `	return ShellRunCommand(pCtx,SHELL_RUN_ECHO,nArg,apArg);` |
|     2 | 1760 | `}` |
|     - | 1761 | `/*` |
|     - | 1762 | ` * ?false passthru(string $command, int &$result_code = null)` |
|     - | 1763 | ` *  Run a command and write its output through, byte for byte.` |
|     - | 1764 | ` */` |
|     8 | 1765 | `PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1766 | `{` |
|    10 | 1767 | `	return ShellRunCommand(pCtx,SHELL_RUN_RAW,nArg,apArg);` |
|     2 | 1768 | `}` |
|     - | 1769 | `/*` |
|     - | 1770 | ` * bool proc_nice(int $priority)` |
|     - | 1771 | ` *  Change the priority of the running process — the last member of php's own` |
|     - | 1772 | ` *  process-execution surface, and the only one of the seven that runs no shell.` |
|     - | 1773 | ` */` |
|     6 | 1774 | `PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1775 | `{` |
|     - | 1776 | `	ph7_int64 iPri;` |
|     3 | 1777 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|     7 | 1778 | `	iPri = ph7_value_to_int64(apArg[0]);` |
|     - | 1779 | `#ifdef __WINNT__` |
|     - | 1780 | `	{` |
|     - | 1781 | `		/* php's own mapping (win32/nice.c): cmd.exe has no nice value, so the` |
|     - | 1782 | `		 * POSIX increment is bucketed into the five priority CLASSES Windows` |
|     - | 1783 | `		 * has. REALTIME is deliberately not reachable there, and neither is it` |
|     - | 1784 | `		 * here. */` |
|     1 | 1785 | `		DWORD dwFlag = NORMAL_PRIORITY_CLASS;` |
|     1 | 1786 | `		if( iPri < -9 ){` |
|   ! 0 | 1787 | `			dwFlag = HIGH_PRIORITY_CLASS;` |
|     1 | 1788 | `		}else if( iPri < -4 ){` |
|   ! 0 | 1789 | `			dwFlag = ABOVE_NORMAL_PRIORITY_CLASS;` |
|     1 | 1790 | `		}else if( iPri > 9 ){` |
|   ! 0 | 1791 | `			dwFlag = IDLE_PRIORITY_CLASS;` |
|     1 | 1792 | `		}else if( iPri > 4 ){` |
|   ! 0 | 1793 | `			dwFlag = BELOW_NORMAL_PRIORITY_CLASS;` |
|     - | 1794 | `		}` |
|     1 | 1795 | `		SetPriorityClass(GetCurrentProcess(),dwFlag);` |
|     - | 1796 | `		/* php answers TRUE whatever that returned: its nice() reports failure` |
|     - | 1797 | `		 * through its return value and leaves errno alone, and proc_nice() reads` |
|     - | 1798 | `		 * only errno. */` |
|     1 | 1799 | `		ph7_result_bool(pCtx,1);` |
|     - | 1800 | `	}` |
|     - | 1801 | `#elif defined(__UNIXES__)` |
|     - | 1802 | `	{` |
|     - | 1803 | `		int iIgnored;` |
|     - | 1804 | `		/* nice() legitimately answers -1 (it returns the NEW nice value), so` |
|     - | 1805 | `		 * errno is the only failure evidence — php clears it first for the same` |
|     - | 1806 | `		 * reason. */` |
|     6 | 1807 | `		errno = 0;` |
|     6 | 1808 | `		iIgnored = nice((int)iPri);` |
|     3 | 1809 | `		(void)iIgnored;` |
|     6 | 1810 | `		if( errno != 0 ){` |
|     3 | 1811 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|     - | 1812 | `				"%s(): Only a super user may attempt to increase the priority of a process",` |
|     1 | 1813 | `				ph7_function_name(pCtx));` |
|     2 | 1814 | `			ph7_result_bool(pCtx,0);` |
|     1 | 1815 | `		}else{` |
|     4 | 1816 | `			ph7_result_bool(pCtx,1);` |
|     - | 1817 | `		}` |
|     - | 1818 | `	}` |
|     - | 1819 | `#else` |
|     - | 1820 | `	ph7_result_bool(pCtx,0);` |
|     - | 1821 | `#endif` |
|     7 | 1822 | `	return PH7_OK;` |
|     1 | 1823 | `}` |
|  6248 | 1824 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1825 | `{` |
|     - | 1826 | `	const char *zCommand, *zMode;` |
|     - | 1827 | `	char zPosix[8];` |
|     - | 1828 | `	pipe_private *pPipe;` |
|     - | 1829 | `	io_private *pDev;` |
|  6253 | 1830 | `	int nCmdLen, nModeLen, nPosix, i, bDropped = 0;` |
|  3120 | 1831 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|     - | 1832 | `	/* Extract the command and mode */` |
|  6253 | 1833 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|  6253 | 1834 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|     - | 1835 | `	/*` |
|     - | 1836 | `	 * php's mode rule, and the only one it has: ONE 'b' — C's binary flag, which` |
|     - | 1837 | `	 * popen(3) itself refuses — is dropped from the mode on POSIX, and what is` |
|     - | 1838 | `	 * left must be exactly "r", "w", "rb" or "wb". PHL used to read mode[0] and` |
|     - | 1839 | `	 * hand the REST to popen(3) unexamined, which was wrong in both directions:` |
|     - | 1840 | ``	 * `popen($cmd, 'rb')`, the ordinary binary spelling, answered FALSE because`` |
|     - | 1841 | ``	 * glibc rejected the 'b', and `popen($cmd, 'rr')` opened a pipe php refuses.`` |
|     - | 1842 | `	 */` |
|  6253 | 1843 | `	nPosix = 0;` |
|     - | 1844 | `#ifdef __WINNT__` |
|     - | 1845 | `	SXUNUSED(bDropped);   /* cmd.exe keeps the 'b': _popen understands it */` |
|     - | 1846 | `#endif` |
| 12515 | 1847 | `	for( i = 0 ; i < nModeLen && nPosix < (int)sizeof(zPosix) - 1 ; ++i ){` |
|     - | 1848 | `#ifndef __WINNT__` |
|  6262 | 1849 | `		if( zMode[i] == 'b' && !bDropped ){` |
|     8 | 1850 | `			bDropped = 1;   /* php drops the FIRST one and only that one */` |
|     8 | 1851 | `			continue;` |
|     - | 1852 | `		}` |
|     - | 1853 | `#endif` |
|  6259 | 1854 | `		zPosix[nPosix++] = zMode[i];` |
|  3128 | 1855 | `	}` |
|  6253 | 1856 | `	zPosix[nPosix] = 0;` |
|  6248 | 1857 | `	if( nPosix > 2` |
|  6248 | 1858 | `	 \|\| (nPosix == 1 && zPosix[0] != 'r' && zPosix[0] != 'w')` |
|  3142 | 1859 | `	 \|\| (nPosix == 2 && SyMemcmp(zPosix,"rb",sizeof("rb")-1) != 0` |
|     7 | 1860 | `	                 && SyMemcmp(zPosix,"wb",sizeof("wb")-1) != 0) ){` |
|     9 | 1861 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1862 | `			"popen(): Argument #2 ($mode) must be one of \"r\", \"rb\", \"w\", or \"wb\"");` |
|     - | 1863 | `	}` |
|     - | 1864 | `	/* Open the pipe. An EMPTY mode passes php's check above and fails HERE, in` |
|     - | 1865 | `	 * popen(3) — php reports it as an open failure and so does this, rather than` |
|     - | 1866 | `	 * letting the platform layer read mode[0] out of an empty string. */` |
|  6245 | 1867 | `	pPipe = nPosix > 0 ? PipeOpen(pCtx->pVm, zCommand, zPosix) : 0;` |
|  6245 | 1868 | `	if( pPipe == 0 ){` |
|     - | 1869 | ``		/* php names both arguments in this one: `popen(cmd,mode): message`. PHL`` |
|     - | 1870 | `		 * answered FALSE in silence, so a script had nothing to report. */` |
|     5 | 1871 | `		if( nPosix < 1 ){` |
|     3 | 1872 | `			errno = EINVAL;` |
|     1 | 1873 | `		}` |
|     7 | 1874 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|     4 | 1875 | `			ph7_function_name(pCtx),zCommand,zPosix,VfsStrerror(errno));` |
|     5 | 1876 | `		ph7_result_bool(pCtx, 0);` |
|     5 | 1877 | `		return PH7_OK;` |
|     - | 1878 | `	}` |
|     - | 1879 | `	/* Allocate an io_private instance to wrap the pipe */` |
|  6241 | 1880 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|  6241 | 1881 | `	if( pDev == 0 ){` |
|   ! 0 | 1882 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|   ! 0 | 1883 | `		PipeClose(pPipe);` |
|   ! 0 | 1884 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 1885 | `		return PH7_OK;` |
|     - | 1886 | `	}` |
|     - | 1887 | `	/* Initialize the io_private structure */` |
|  6241 | 1888 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|     - | 1889 | `	/* A pipe has no wrapper and no path, so php's meta reports the MODE and` |
|     - | 1890 | ``	 * neither `wrapper_type` nor `uri`: an empty URI is what leaves them out. */`` |
|  6241 | 1891 | `	SetIOPrivateOpenedAs(pDev,0,0,zPosix,nPosix);` |
|  6241 | 1892 | `	pDev->pHandle = pPipe;` |
|     - | 1893 | `	/* Return the io_private instance as a resource */` |
|  6241 | 1894 | `	ph7_result_resource(pCtx, pDev);` |
|  6241 | 1895 | `	return PH7_OK;` |
|  3125 | 1896 | `}` |
|     - | 1897 | `/*` |
|     - | 1898 | ` * int pclose(resource $handle)` |
|     - | 1899 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|     - | 1900 | ` * Parameters` |
|     - | 1901 | ` *  $handle` |
|     - | 1902 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|     - | 1903 | ` * Return` |
|     - | 1904 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|     - | 1905 | ` */` |
|  6090 | 1906 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1907 | `{` |
|     - | 1908 | `	const ph7_io_stream *pStream;` |
|     - | 1909 | `	pipe_private *pPipe;` |
|     - | 1910 | `	io_private *pDev;` |
|     - | 1911 | `	int status,rc;` |
|  6095 | 1912 | `	if( nArg < 1 ){` |
|     - | 1913 | `		/* Missing/Invalid arguments, return -1 */` |
|   ! 0 | 1914 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|   ! 0 | 1915 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1916 | `		return PH7_OK;` |
|     - | 1917 | `	}` |
|     - | 1918 | ``	/* php's screen, and it names this one's argument `$handle` rather than`` |
|     - | 1919 | ``	 * `$stream`: a non-resource and an already-CLOSED pipe are both TypeErrors. */`` |
|  6095 | 1920 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"handle",&rc);` |
|  6095 | 1921 | `	if( pDev == 0 ){` |
|    15 | 1922 | `		return rc;` |
|     - | 1923 | `	}` |
|     - | 1924 | `	/* Point to the target IO stream device */` |
|  6081 | 1925 | `	pStream = pDev->pStream;` |
|  6081 | 1926 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|   ! 0 | 1927 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|   ! 0 | 1928 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1929 | `		return PH7_OK;` |
|     - | 1930 | `	}` |
|     - | 1931 | `	/* Get the pipe handle */` |
|  6081 | 1932 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|     - | 1933 | `	/* A write chain gets its closing call while the pipe is still open. */` |
|  6081 | 1934 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     - | 1935 | `	/* Close the pipe and get exit status */` |
|  6081 | 1936 | `	status = PipeClose(pPipe);` |
|     - | 1937 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
|  6081 | 1938 | `	MarkIOPrivateClosed(pDev);` |
|     - | 1939 | `	/* Return the exit status */` |
|  6081 | 1940 | `	ph7_result_int(pCtx, status);` |
|  6081 | 1941 | `	return PH7_OK;` |
|  3046 | 1942 | `}` |
|     - | 1943 | `/*` |
|     - | 1944 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|     - | 1945 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|     - | 1946 | ` *   standard descriptors (php's process-control family). The returned` |
|     - | 1947 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|     - | 1948 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|     - | 1949 | ` *   bounds and report it as a live, non-stream resource.` |
|     - | 1950 | ` */` |
|     - | 1951 | `#ifdef __UNIXES__` |
|     - | 1952 | `#define PROC_MAX_DESC 16` |
|     - | 1953 | `typedef struct proc_private proc_private;` |
|     - | 1954 | `struct proc_private` |
|     - | 1955 | `{` |
|     - | 1956 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|     - | 1957 | `	int pid;           /* child process id */` |
|     - | 1958 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|     - | 1959 | `	int exit_code;     /* cached exit status once reaped */` |
|     - | 1960 | `	/* The parent ends this handle created and handed to $pipes. php's proc resource` |
|     - | 1961 | `	 * OWNS them: its destructor closes every one before it waits, with the comment` |
|     - | 1962 | `	 * "Close all pipes first, so that the child may exit" -- and proc_close() IS that` |
|     - | 1963 | `	 * destructor, so after it the script's own $pipes entries are closed resources.` |
|     - | 1964 | `	 * Without the ownership a child reading its stdin never sees EOF and the wait` |
|     - | 1965 | `	 * never returns: monolog's ProcessHandler suite hung the engine forever there.` |
|     - | 1966 | `	 * The device structs are never freed on close (MarkIOPrivateClosed only flags` |
|     - | 1967 | `	 * them), so these stay valid even when the script closed them first. */` |
|     - | 1968 | `	io_private *apPipe[PROC_MAX_DESC];` |
|     - | 1969 | `	int nPipe;` |
|     - | 1970 | `};` |
|     - | 1971 | `/* Close a parent pipe end this handle still owns, exactly as fclose() would. */` |
|   686 | 1972 | `static void ProcClosePipe(io_private *pEnd)` |
|     - | 1973 | `{` |
|   686 | 1974 | `	if( pEnd == 0 \|\| pEnd->iMagic != IO_PRIVATE_MAGIC \|\| pEnd->pStream == 0 ){` |
|   666 | 1975 | `		return; /* the script closed it already, or it never opened */` |
|     - | 1976 | `	}` |
|    20 | 1977 | `	PH7_StreamFilterReleaseChains(pEnd);` |
|    20 | 1978 | `	PH7_StreamCloseHandle(pEnd->pStream,pEnd->pHandle);` |
|    20 | 1979 | `	MarkIOPrivateClosed(pEnd);` |
|   343 | 1980 | `}` |
|     - | 1981 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|   686 | 1982 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd,int bParentReads)` |
|     - | 1983 | `{` |
|   686 | 1984 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|   686 | 1985 | `	if( pDev == 0 ){` |
|   ! 0 | 1986 | `		return 0;` |
|     - | 1987 | `	}` |
|   686 | 1988 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|     - | 1989 | `	/* Same shape as popen()'s end: a proc_open() pipe carries no path, and its` |
|     - | 1990 | `	 * mode is the direction the PARENT holds — the opposite of the child's. */` |
|   686 | 1991 | `	SetIOPrivateOpenedAs(pDev,0,0,bParentReads ? "r" : "w",1);` |
|   686 | 1992 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|   686 | 1993 | `	return pDev;` |
|   343 | 1994 | `}` |
|     - | 1995 | `/* One parsed descriptor-spec entry. */` |
|     - | 1996 | `struct proc_desc` |
|     - | 1997 | `{` |
|     - | 1998 | `	int child_fd;      /* the array key: which fd the child sees */` |
|     - | 1999 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|     - | 2000 | `	/* pipe */` |
|     - | 2001 | `	int child_end;     /* fd the child must have at child_fd */` |
|     - | 2002 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|     - | 2003 | `	int parent_reads;  /* the parent's end is the READ end (the child writes) */` |
|     - | 2004 | `	/* file */` |
|     - | 2005 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|     - | 2006 | `	/* redirect */` |
|     - | 2007 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|     - | 2008 | `};` |
|   232 | 2009 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2010 | `{` |
|     - | 2011 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|     - | 2012 | `	io_private *apEnd[PROC_MAX_DESC]; /* the parent ends, handed to the handle below */` |
|   232 | 2013 | `	int nEnd = 0;` |
|   232 | 2014 | `	int nDesc = 0;` |
|     - | 2015 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|     - | 2016 | `	ph7_hashmap *pSpecMap;` |
|     - | 2017 | `	ph7_hashmap_node *pNode;` |
|   232 | 2018 | `	ph7_vm *pVm = pCtx->pVm;` |
|   232 | 2019 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|   232 | 2020 | `	int nArgv = 0;` |
|   232 | 2021 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|   232 | 2022 | `	const char *zCwd = 0;` |
|   232 | 2023 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|   232 | 2024 | `	int nEnv = 0;` |
|     - | 2025 | `	proc_private *pProc;` |
|     - | 2026 | `	pid_t pid;` |
|     - | 2027 | `	int i, rc;` |
|   232 | 2028 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 2029 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|   ! 0 | 2030 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2031 | `		return PH7_OK;` |
|     - | 2032 | `	}` |
|     - | 2033 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|   232 | 2034 | `	if( ph7_value_is_array(apArg[0]) ){` |
|   220 | 2035 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   220 | 2036 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|   220 | 2037 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|   220 | 2038 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   220 | 2039 | `		pNode = pCmdMap->pFirst;` |
|   906 | 2040 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|   686 | 2041 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|     - | 2042 | `			int nLen; const char *zs;` |
|   686 | 2043 | `			PH7_MemObjInit(pVm,pv);` |
|   686 | 2044 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|   686 | 2045 | `			zs = ph7_value_to_string(pv,&nLen);` |
|   686 | 2046 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|   686 | 2047 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|   686 | 2048 | `			PH7_MemObjRelease(pv);` |
|   686 | 2049 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|   686 | 2050 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|   343 | 2051 | `		}` |
|   220 | 2052 | `		azArgv[nArgv] = 0;` |
|   110 | 2053 | `	}else{` |
|     - | 2054 | `		int nLen;` |
|    12 | 2055 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|     - | 2056 | `	}` |
|     - | 2057 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|   232 | 2058 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|   ! 0 | 2059 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|   ! 0 | 2060 | `		if( nLen < 1 ){ zCwd = 0; }` |
|   ! 0 | 2061 | `	}` |
|   116 | 2062 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|   ! 0 | 2063 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|   ! 0 | 2064 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|   ! 0 | 2065 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|   ! 0 | 2066 | `		if( azEnv ){` |
|   ! 0 | 2067 | `			pNode = pEnvMap->pFirst;` |
|   ! 0 | 2068 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|     - | 2069 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|   ! 0 | 2070 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|   ! 0 | 2071 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|   ! 0 | 2072 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|   ! 0 | 2073 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|   ! 0 | 2074 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|   ! 0 | 2075 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));` |
|   ! 0 | 2076 | `				if( zPair ){` |
|   ! 0 | 2077 | `					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|   ! 0 | 2078 | `					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|   ! 0 | 2079 | `					azEnv[nEnv++] = zPair;` |
|   ! 0 | 2080 | `				}` |
|   ! 0 | 2081 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|   ! 0 | 2082 | `				pNode = pNode->pPrev;` |
|   ! 0 | 2083 | `			}` |
|   ! 0 | 2084 | `			azEnv[nEnv] = 0;` |
|   ! 0 | 2085 | `		}` |
|   ! 0 | 2086 | `	}` |
|     - | 2087 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|   232 | 2088 | `	pSpec = apArg[1];` |
|   232 | 2089 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|   232 | 2090 | `	pNode = pSpecMap->pFirst;` |
|   922 | 2091 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|   690 | 2092 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|   690 | 2093 | `		PH7_MemObjInit(pVm,&sKey);` |
|   690 | 2094 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|   690 | 2095 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|   690 | 2096 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1; pD->parent_reads = 0;` |
|   690 | 2097 | `		PH7_MemObjRelease(&sKey);` |
|   690 | 2098 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   690 | 2099 | `		PH7_MemObjInit(pVm,pEntry);` |
|   690 | 2100 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|   690 | 2101 | `		if( ph7_value_is_array(pEntry) ){` |
|     - | 2102 | `			int nLen; const char *zType;` |
|     - | 2103 | `			/* pEntry aliases the descriptor array the SCRIPT still holds, so every` |
|     - | 2104 | `			 * member is read through a scratch copy — ph7_value_to_string() would` |
|     - | 2105 | `			 * convert the entry in place and rewrite the script's own $descriptors` |
|     - | 2106 | ``			 * (`[0 => ['pipe', 114]]` came back as `'114'`). */`` |
|     - | 2107 | `			ph7_value sPeek;` |
|   690 | 2108 | `			PH7_MemObjInit(pVm,&sPeek);` |
|   690 | 2109 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|   690 | 2110 | `			zType = pType ? ph7_value_to_string(PH7_ValuePeek(pType,&sPeek),&nLen) : "";` |
|   690 | 2111 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|     - | 2112 | `				int fds[2];` |
|   686 | 2113 | `				if( pipe(fds) == 0 ){` |
|   686 | 2114 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|     - | 2115 | `					{` |
|     - | 2116 | `						ph7_value sMode;` |
|     - | 2117 | `						int nMode; const char *zMode;` |
|   686 | 2118 | `						PH7_MemObjInit(pVm,&sMode);` |
|   686 | 2119 | `						zMode = pParam ? ph7_value_to_string(PH7_ValuePeek(pParam,&sMode),&nMode) : "r";` |
|   686 | 2120 | `						pD->kind = 0;` |
|   686 | 2121 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|     - | 2122 | `							/* child writes -> parent reads: child gets write end */` |
|   458 | 2123 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|   458 | 2124 | `						pD->parent_reads = 1;` |
|   229 | 2125 | `						}else{` |
|     - | 2126 | `							/* child reads -> parent writes: child gets read end */` |
|   228 | 2127 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|   228 | 2128 | `						pD->parent_reads = 0;` |
|     - | 2129 | `						}` |
|     - | 2130 | `						/* The PARENT end is close-on-exec, which is php's own` |
|     - | 2131 | ``						 * `fcntl(descriptors[i].parentend, F_SETFD, FD_CLOEXEC)`:`` |
|     - | 2132 | `						 * without it the NEXT proc_open()'s child inherits a copy of` |
|     - | 2133 | `						 * this pipe, and the first child then never sees EOF on its` |
|     - | 2134 | `						 * stdin however carefully the script closes its own end. Two` |
|     - | 2135 | `						 * such handlers alive at once deadlock the interpreter --` |
|     - | 2136 | `						 * monolog's ProcessHandler suite ran to its last line and` |
|     - | 2137 | `						 * then hung forever in proc_close(). The CHILD's end is` |
|     - | 2138 | `						 * dup2()'d onto 0/1/2, and dup2 clears the flag, so the` |
|     - | 2139 | `						 * process being started keeps exactly what it should. */` |
|     - | 2140 | `#if defined(F_SETFD) && defined(FD_CLOEXEC)` |
|   686 | 2141 | `						fcntl(pD->parent_end,F_SETFD,FD_CLOEXEC);` |
|     - | 2142 | `#endif` |
|   686 | 2143 | `						nDesc++;` |
|   686 | 2144 | `						PH7_MemObjRelease(&sMode);` |
|     - | 2145 | `					}` |
|   343 | 2146 | `				}` |
|   347 | 2147 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|     - | 2148 | `				ph7_value sPath, sMode;` |
|     2 | 2149 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|     2 | 2150 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|     2 | 2151 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|     2 | 2152 | `				PH7_MemObjInit(pVm,&sPath);` |
|     2 | 2153 | `				PH7_MemObjInit(pVm,&sMode);` |
|     2 | 2154 | `				zPath = pPath ? ph7_value_to_string(PH7_ValuePeek(pPath,&sPath),&nLen2) : "";` |
|     2 | 2155 | `				zMode = pMode ? ph7_value_to_string(PH7_ValuePeek(pMode,&sMode),&nLen3) : "r";` |
|     2 | 2156 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|   ! 0 | 2157 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|     2 | 2158 | `				pD->kind = 1;` |
|     2 | 2159 | `				pD->file_fd = open(zPath,oflag,0644);` |
|     2 | 2160 | `				nDesc++;` |
|     2 | 2161 | `				PH7_MemObjRelease(&sPath);` |
|     2 | 2162 | `				PH7_MemObjRelease(&sMode);` |
|     3 | 2163 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|     2 | 2164 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|     2 | 2165 | `				pD->kind = 2;` |
|     2 | 2166 | `				pD->redirect_to = pParam ? (int)PH7_ValuePeekInt64(pParam) : 1;` |
|     2 | 2167 | `				nDesc++;` |
|     1 | 2168 | `			}` |
|   690 | 2169 | `			PH7_MemObjRelease(&sPeek);` |
|   345 | 2170 | `		}` |
|   690 | 2171 | `		PH7_MemObjRelease(pEntry);` |
|   690 | 2172 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|   690 | 2173 | `		pNode = pNode->pPrev;` |
|   345 | 2174 | `	}` |
|     - | 2175 | `	/* --- Fork the child --- */` |
|   232 | 2176 | `	pid = fork();` |
|   348 | 2177 | `	if( pid < 0 ){` |
|   ! 0 | 2178 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"fork() failed");` |
|   ! 0 | 2179 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2180 | `		return PH7_OK;` |
|     - | 2181 | `	}` |
|   464 | 2182 | `	if( pid == 0 ){` |
|     - | 2183 | `		/* Child: wire up descriptors then exec */` |
|   922 | 2184 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|   690 | 2185 | `			struct proc_desc *pD = &aDesc[i];` |
|   690 | 2186 | `			if( pD->kind == 0 ){` |
|   686 | 2187 | `				dup2(pD->child_end,pD->child_fd);` |
|   686 | 2188 | `				close(pD->parent_end);` |
|   686 | 2189 | `				close(pD->child_end);` |
|   347 | 2190 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|     2 | 2191 | `				dup2(pD->file_fd,pD->child_fd);` |
|     2 | 2192 | `				close(pD->file_fd);` |
|     1 | 2193 | `			}` |
|   345 | 2194 | `		}` |
|     - | 2195 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|   922 | 2196 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|   690 | 2197 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|   345 | 2198 | `		}` |
|   232 | 2199 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|   232 | 2200 | `		if( azEnv ){` |
|   ! 0 | 2201 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|   ! 0 | 2202 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|   ! 0 | 2203 | `		}else{` |
|   232 | 2204 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|    12 | 2205 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|     - | 2206 | `		}` |
|   116 | 2207 | `		_exit(127); /* exec failed */` |
|     - | 2208 | `	}` |
|     - | 2209 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|   232 | 2210 | `	pPipes = ph7_context_new_array(pCtx);` |
|   922 | 2211 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|   690 | 2212 | `		struct proc_desc *pD = &aDesc[i];` |
|   690 | 2213 | `		if( pD->kind == 0 ){` |
|     - | 2214 | `			io_private *pEnd;` |
|     - | 2215 | `			ph7_value *pRes;` |
|   686 | 2216 | `			close(pD->child_end);` |
|   686 | 2217 | `			pEnd = ProcWrapFd(pVm,pD->parent_end,pD->parent_reads);` |
|   686 | 2218 | `			pRes = ph7_context_new_scalar(pCtx);` |
|   686 | 2219 | `			if( pEnd && pRes && pPipes ){` |
|   686 | 2220 | `				ph7_value_resource(pRes,pEnd);` |
|   686 | 2221 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|   686 | 2222 | `				apEnd[nEnd++] = pEnd;   /* the handle keeps its own list; see proc_private */` |
|   343 | 2223 | `			}` |
|   686 | 2224 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|   347 | 2225 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|     2 | 2226 | `			close(pD->file_fd);` |
|     1 | 2227 | `		}` |
|   345 | 2228 | `	}` |
|   232 | 2229 | `	if( pPipes ){` |
|   232 | 2230 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|   116 | 2231 | `	}` |
|     - | 2232 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|   238 | 2233 | `	if( azArgv ){` |
|   906 | 2234 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|   220 | 2235 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|   110 | 2236 | `	}` |
|   336 | 2237 | `	if( azEnv ){` |
|   ! 0 | 2238 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|   ! 0 | 2239 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|   ! 0 | 2240 | `	}` |
|     - | 2241 | `	/* Build the process resource */` |
|   232 | 2242 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|   232 | 2243 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   232 | 2244 | `	SyZero(pProc,sizeof(proc_private));` |
|   232 | 2245 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|   232 | 2246 | `	pProc->pid = (int)pid;` |
|   232 | 2247 | `	pProc->running = 1;` |
|   232 | 2248 | `	pProc->exit_code = 0;` |
|   918 | 2249 | `	for( i = 0 ; i < nEnd ; ++i ){ pProc->apPipe[i] = apEnd[i]; }` |
|   232 | 2250 | `	pProc->nPipe = nEnd;` |
|   232 | 2251 | `	ph7_result_resource(pCtx,pProc);` |
|   116 | 2252 | `	(void)rc;` |
|   232 | 2253 | `	return PH7_OK;` |
|   116 | 2254 | `}` |
|     - | 2255 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|   232 | 2256 | `static void ProcReap(proc_private *pProc,int block)` |
|     - | 2257 | `{` |
|   232 | 2258 | `	int status = 0;` |
|     - | 2259 | `	pid_t r;` |
|   232 | 2260 | `	if( !pProc->running ){ return; }` |
|   232 | 2261 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|   232 | 2262 | `	if( r == (pid_t)pProc->pid ){` |
|   232 | 2263 | `		pProc->running = 0;` |
|   232 | 2264 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|    32 | 2265 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|   116 | 2266 | `	}` |
|   116 | 2267 | `}` |
|     - | 2268 | `/*` |
|     - | 2269 | ` * The three verbs all take a LIVE process resource, and php answers anything else --` |
|     - | 2270 | ` * a file handle, an already-closed process, a process resource closed by an earlier` |
|     - | 2271 | ` * proc_close() -- with one catchable sentence naming the function.` |
|     - | 2272 | ` */` |
|   278 | 2273 | `static proc_private * ProcOfArg(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi32 *pRc)` |
|     - | 2274 | `{` |
|     - | 2275 | `	proc_private *pProc;` |
|     - | 2276 | `	char zGiven[128];` |
|   278 | 2277 | `	*pRc = PH7_OK;` |
|   278 | 2278 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|     - | 2279 | `		/* Not a resource at all: php's ordinary argument sentence, naming the type. */` |
|     3 | 2280 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2281 | `			"%z(): Argument #1 ($process) must be of type resource, %s given",` |
|     2 | 2282 | `			&pCtx->pFunc->sName,` |
|     2 | 2283 | `			nArg < 1 ? "none" : VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|     2 | 2284 | `		return 0;` |
|     - | 2285 | `	}` |
|   276 | 2286 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|   276 | 2287 | `	if( pProc && pProc->base.iMagic == PROC_PRIVATE_MAGIC ){` |
|   264 | 2288 | `		return pProc;` |
|     - | 2289 | `	}` |
|     - | 2290 | `	/* A resource, but not a live process one -- a file handle, or a process` |
|     - | 2291 | `	 * handle an earlier proc_close() already closed. */` |
|    24 | 2292 | `	*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 2293 | `		"%z(): supplied resource is not a valid process resource",` |
|    12 | 2294 | `		&pCtx->pFunc->sName);` |
|    12 | 2295 | `	return 0;` |
|   139 | 2296 | `}` |
|   238 | 2297 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2298 | `{` |
|     - | 2299 | `	proc_private *pProc;` |
|     - | 2300 | `	sxi32 rcArg;` |
|     - | 2301 | `	int i;` |
|   238 | 2302 | `	pProc = ProcOfArg(pCtx,nArg,apArg,&rcArg);` |
|   238 | 2303 | `	if( pProc == 0 ){` |
|     6 | 2304 | `		return rcArg;` |
|     - | 2305 | `	}` |
|     - | 2306 | `	/* php's own order, and the reason it has one: the pipes go FIRST so a child` |
|     - | 2307 | `	 * blocked reading its stdin sees EOF and can exit, and only then do we wait` |
|     - | 2308 | `	 * for it. Waiting first is a deadlock with any such child. */` |
|   918 | 2309 | `	for( i = 0 ; i < pProc->nPipe ; ++i ){` |
|   686 | 2310 | `		ProcClosePipe(pProc->apPipe[i]);` |
|   686 | 2311 | `		pProc->apPipe[i] = 0;` |
|   343 | 2312 | `	}` |
|   232 | 2313 | `	pProc->nPipe = 0;` |
|   232 | 2314 | `	ProcReap(pProc,1/*block until it exits*/);` |
|   232 | 2315 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|     - | 2316 | `	/* proc_close() IS the resource's destructor in php, so the handle is closed` |
|     - | 2317 | `	 * afterwards: is_resource() answers false and every verb refuses it. */` |
|   232 | 2318 | `	pProc->base.iMagic = IO_PRIVATE_CLOSED_MAGIC;` |
|   232 | 2319 | `	return PH7_OK;` |
|   119 | 2320 | `}` |
|    36 | 2321 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2322 | `{` |
|     - | 2323 | `	proc_private *pProc;` |
|     - | 2324 | `	sxi32 rcArg;` |
|    36 | 2325 | `	int sig = 15; /* SIGTERM */` |
|    36 | 2326 | `	pProc = ProcOfArg(pCtx,nArg,apArg,&rcArg);` |
|    36 | 2327 | `	if( pProc == 0 ){` |
|     4 | 2328 | `		return rcArg;` |
|     - | 2329 | `	}` |
|    32 | 2330 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|    32 | 2331 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|    32 | 2332 | `	ph7_result_bool(pCtx,1);` |
|    32 | 2333 | `	return PH7_OK;` |
|    18 | 2334 | `}` |
|     4 | 2335 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2336 | `{` |
|     - | 2337 | `	proc_private *pProc;` |
|     - | 2338 | `	ph7_value *pArray, *pVal;` |
|     - | 2339 | `	sxi32 rcArg;` |
|     4 | 2340 | `	pProc = ProcOfArg(pCtx,nArg,apArg,&rcArg);` |
|     4 | 2341 | `	if( pProc == 0 ){` |
|     4 | 2342 | `		return rcArg;` |
|     - | 2343 | `	}` |
|   ! 0 | 2344 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|   ! 0 | 2345 | `	pArray = ph7_context_new_array(pCtx);` |
|   ! 0 | 2346 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 2347 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   ! 0 | 2348 | `	ph7_value_int(pVal,pProc->pid);` |
|   ! 0 | 2349 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|   ! 0 | 2350 | `	ph7_value_bool(pVal,pProc->running);` |
|   ! 0 | 2351 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|   ! 0 | 2352 | `	ph7_value_bool(pVal,0);` |
|   ! 0 | 2353 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|   ! 0 | 2354 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|   ! 0 | 2355 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|   ! 0 | 2356 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|   ! 0 | 2357 | `	ph7_value_int(pVal,0);` |
|   ! 0 | 2358 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|   ! 0 | 2359 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|   ! 0 | 2360 | `	ph7_context_release_value(pCtx,pVal);` |
|   ! 0 | 2361 | `	ph7_result_value(pCtx,pArray);` |
|   ! 0 | 2362 | `	return PH7_OK;` |
|     2 | 2363 | `}` |
|     - | 2364 | `#else /* !__UNIXES__ */` |
|     - | 2365 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2366 | `{` |
|     - | 2367 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2368 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|   ! 0 | 2369 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 2370 | `	return PH7_OK;` |
|   ! 0 | 2371 | `}` |
|     - | 2372 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2373 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|     - | 2374 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2375 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     - | 2376 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2377 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     - | 2378 | `#endif /* __UNIXES__ */` |
|     - | 2379 | `/* Export the php:// stream */` |
|     - | 2380 | `PH7_PRIVATE const ph7_io_stream sPHP_Stream = {` |
|     - | 2381 | `	"php",` |
|     - | 2382 | `	PH7_IO_STREAM_VERSION,` |
|     - | 2383 | `	PHPStreamData_Open,  /* xOpen */` |
|     - | 2384 | `	0,   /* xOpenDir */` |
|     - | 2385 | `	PHPStreamData_Close, /* xClose */` |
|     - | 2386 | `	0,  /* xCloseDir */` |
|     - | 2387 | `	PHPStreamData_Read,  /* xRead */` |
|     - | 2388 | `	0,  /* xReadDir */` |
|     - | 2389 | `	PHPStreamData_Write, /* xWrite */` |
|     - | 2390 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|     - | 2391 | `	0,  /* xLock */` |
|     - | 2392 | `	0,  /* xRewindDir */` |
|     - | 2393 | `	PHPStreamData_Tell,  /* xTell */` |
|     - | 2394 | `	PHPStreamData_Trunc, /* xTrunc */` |
|     - | 2395 | `	PHPStreamData_Sync,  /* xSync */` |
|     - | 2396 | `	PHPStreamData_Stat   /* xStat */` |
|     - | 2397 | `};` |
|     - | 2398 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 2399 | `/*` |
|     - | 2400 | ` * Return TRUE if we are dealing with the php:// stream.` |
|     - | 2401 | ` * FALSE otherwise.` |
|     - | 2402 | ` */` |
|     - | 2403 | `/*` |
|     - | 2404 | ` * The handle a php://filter proxy WRAPS, or 0 for any other php:// stream. The` |
|     - | 2405 | ` * proxy is not a stream of its own — the position, the descriptor, the stat and` |
|     - | 2406 | ` * the lock all belong to the stream underneath — so everything that asks the` |
|     - | 2407 | ` * device such a question has to go through here first.` |
|     - | 2408 | ` */` |
|   120 | 2409 | `PH7_PRIVATE io_private * PH7_PhpStreamInner(void *pHandle)` |
|     3 | 2410 | `{` |
|     - | 2411 | `#ifndef PH7_DISABLE_DISK_IO` |
|   123 | 2412 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   123 | 2413 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|    15 | 2414 | `		return pData->pInner;` |
|     - | 2415 | `	}` |
|     - | 2416 | `#else` |
|     - | 2417 | `	SXUNUSED(pHandle);` |
|     - | 2418 | `#endif` |
|   109 | 2419 | `	return 0;` |
|    61 | 2420 | `}` |
| 12282 | 2421 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream)` |
|     5 | 2422 | `{` |
|     - | 2423 | `#ifndef PH7_DISABLE_DISK_IO` |
| 12287 | 2424 | `	return pStream == &sPHP_Stream;` |
|     - | 2425 | `#else` |
|     - | 2426 | `	SXUNUSED(pStream); /* cc warning */` |
|     - | 2427 | `	return 0;` |
|     - | 2428 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2429 | `}` |
|     - | 2430 | `/*` |
|     - | 2431 | ` * Is this a handle php's plain-files device would own -- a file, a pipe, a` |
|     - | 2432 | ` * standard stream? php sets the blocking mode on those with O_NONBLOCK, which` |
|     - | 2433 | ` * Windows does not have, so there stream_set_blocking() answers FALSE for them.` |
|     - | 2434 | ` */` |
|   ! 0 | 2435 | `PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev)` |
|     1 | 2436 | `{` |
|     - | 2437 | `#ifndef PH7_DISABLE_DISK_IO` |
|     1 | 2438 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| pDev->pStream == 0 \|\| pDev->bDir ){` |
|   ! 0 | 2439 | `		return 0;` |
|     - | 2440 | `	}` |
|     - | 2441 | `#ifdef __WINNT__` |
|     1 | 2442 | `	if( pDev->pStream == &sWinFileStream ){` |
|     1 | 2443 | `		return 1;` |
|     - | 2444 | `	}` |
|     - | 2445 | `#elif defined(__UNIXES__)` |
|   ! 0 | 2446 | `	if( pDev->pStream == &sUnixFileStream ){` |
|   ! 0 | 2447 | `		return 1;` |
|     - | 2448 | `	}` |
|     - | 2449 | `#endif` |
|     1 | 2450 | `	if( pDev->pStream == &sPipe_Stream ){` |
|   ! 0 | 2451 | `		return 1;` |
|     - | 2452 | `	}` |
|     1 | 2453 | `	if( is_php_stream(pDev->pStream) ){` |
|     1 | 2454 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     1 | 2455 | `		return pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|   ! 0 | 2456 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR;` |
|     - | 2457 | `	}` |
|     1 | 2458 | `	return 0;` |
|     - | 2459 | `#else` |
|     - | 2460 | `	SXUNUSED(pDev); /* cc warning */` |
|     - | 2461 | `	return 0;` |
|     - | 2462 | `#endif` |
|     1 | 2463 | `}` |
|     - | 2464 | `/*` |
|     - | 2465 | ` * The POSIX descriptor behind an open handle, or -1 when there is none.` |
|     - | 2466 | ` * php applies blocking mode and timeouts AT the descriptor, so a stream that` |
|     - | 2467 | ` * has no fd — a memory buffer, a data:// payload, a userland wrapper, and` |
|     - | 2468 | ` * every file on Windows, where the device carries a HANDLE — is exactly the` |
|     - | 2469 | ` * set php answers "unsupported" for.` |
|     - | 2470 | ` */` |
|   144 | 2471 | `PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev)` |
|     2 | 2472 | `{` |
|     - | 2473 | `#if !defined(__WINNT__) && !defined(PH7_DISABLE_DISK_IO)` |
|     - | 2474 | `	/* A php://filter handle has no descriptor of its own; the stream it wraps` |
|     - | 2475 | `	 * does, and that is the one blocking mode and locking apply to. */` |
|   144 | 2476 | `	pDev = PH7_StreamUnwrap(pDev);` |
|   144 | 2477 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| pDev->pStream == 0 \|\| pDev->bDir ){` |
|     - | 2478 | `		/* A DIRECTORY handle rides the same ops table as a file and stores a` |
|     - | 2479 | `		 * DIR* where a file stores its descriptor, so reading one as the other` |
|     - | 2480 | `		 * hands fcntl()/lseek() a truncated heap pointer — an arbitrary fd` |
|     - | 2481 | `		 * number belonging to something else in this process. */` |
|   ! 0 | 2482 | `		return -1;` |
|     - | 2483 | `	}` |
|   144 | 2484 | `	if( pDev->pStream == &sUnixFileStream ){` |
|    68 | 2485 | `		return SX_PTR_TO_INT(pDev->pHandle);` |
|     - | 2486 | `	}` |
|    76 | 2487 | `	if( pDev->pStream == &sPipe_Stream ){` |
|   ! 0 | 2488 | `		pipe_private *pPipe = (pipe_private *)pDev->pHandle;` |
|   ! 0 | 2489 | `		return pPipe->pFile ? fileno(pPipe->pFile) : -1;` |
|     - | 2490 | `	}` |
|    76 | 2491 | `	if( is_php_stream(pDev->pStream) ){` |
|    44 | 2492 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|    44 | 2493 | `		if( pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|    41 | 2494 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR ){` |
|     6 | 2495 | `			return SX_PTR_TO_INT(pData->x.pHandle);` |
|     - | 2496 | `		}` |
|    18 | 2497 | `	}` |
|    70 | 2498 | `	return -1;` |
|     - | 2499 | `#else` |
|     - | 2500 | `	SXUNUSED(pDev); /* cc warning */` |
|     2 | 2501 | `	return -1;` |
|     - | 2502 | `#endif` |
|    71 | 2503 | `}` |
|     - | 2504 | `/*` |
|     - | 2505 | `` * Can this handle report a POSITION? php's `seekable` is a fact about what the`` |
|     - | 2506 | ` * handle sits on, not about what the device could do — php://stdout is seekable` |
|     - | 2507 | ` * into a file and not down a pipe — and the descriptor is the only thing that` |
|     - | 2508 | ` * knows. Answers 1 (yes), 0 (no) or -1 (nothing here can tell; the caller falls` |
|     - | 2509 | ` * back on the device's own xTell).` |
|     - | 2510 | ` */` |
|    80 | 2511 | `PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev)` |
|     3 | 2512 | `{` |
|     - | 2513 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2514 | `#ifdef __WINNT__` |
|     - | 2515 | `	/* The file devices carry a HANDLE rather than a descriptor here, and only` |
|     - | 2516 | `	 * the php:// standard streams hold one this can ask. */` |
|     3 | 2517 | `	if( pDev && pDev->pHandle && is_php_stream(pDev->pStream) ){` |
|     3 | 2518 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     - | 2519 | `		if( pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|     3 | 2520 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR ){` |
|     - | 2521 | `			LARGE_INTEGER zero,pos;` |
|     1 | 2522 | `			zero.QuadPart = 0;` |
|     1 | 2523 | `			return SetFilePointerEx((HANDLE)pData->x.pHandle,zero,&pos,FILE_CURRENT) ? 1 : 0;` |
|     - | 2524 | `		}` |
|     - | 2525 | `	}` |
|     3 | 2526 | `	return -1;` |
|     - | 2527 | `#else` |
|    80 | 2528 | `	int fd = PH7_StreamPosixFd(pDev);` |
|    80 | 2529 | `	if( fd < 0 ){` |
|    50 | 2530 | `		return -1;` |
|     - | 2531 | `	}` |
|    30 | 2532 | `	return lseek(fd,0,SEEK_CUR) == (off_t)-1 ? 0 : 1;` |
|     - | 2533 | `#endif` |
|     - | 2534 | `#else` |
|     - | 2535 | `	SXUNUSED(pDev); /* cc warning */` |
|     - | 2536 | `	return -1;` |
|     - | 2537 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    43 | 2538 | `}` |
|     - | 2539 | `/*` |
|     - | 2540 | ` * Which php:// sub-stream a handle opened. stream_get_meta_data() has to tell` |
|     - | 2541 | ` * MEMORY, TEMP and STDIO apart and only the device's own private state knows;` |
|     - | 2542 | ` * everything else answers 0.` |
|     - | 2543 | ` */` |
|  1817 | 2544 | `PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle)` |
|     5 | 2545 | `{` |
|     - | 2546 | `#ifndef PH7_DISABLE_DISK_IO` |
|  1822 | 2547 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  1822 | 2548 | `	return pData ? pData->iType : 0;` |
|     - | 2549 | `#else` |
|     - | 2550 | `	SXUNUSED(pHandle); /* cc warning */` |
|     - | 2551 | `	return 0;` |
|     - | 2552 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2553 | `}` |
|     - | 2554 | `/*` |
|     - | 2555 | ` * Has a php://temp handle just handed out its LAST byte? php's temp stream sits` |
|     - | 2556 | ` * over a memory one and copies that one's eof after every read, and the inner` |
|     - | 2557 | ` * stream raises it as soon as a fill has consumed the buffer -- so a temp` |
|     - | 2558 | ` * handle answers feof() true one read before a bare php://memory does, and` |
|     - | 2559 | `` * `foreach` over an SplTempFileObject stops one row earlier. Answers 0 for`` |
|     - | 2560 | ` * every other device, php://memory included.` |
|     - | 2561 | ` */` |
|   186 | 2562 | `PH7_PRIVATE int PH7_PhpStreamTempDrained(void *pHandle)` |
|     4 | 2563 | `{` |
|     - | 2564 | `#ifndef PH7_DISABLE_DISK_IO` |
|   190 | 2565 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   192 | 2566 | `	return pData && pData->bTemp && pData->iType == PH7_IO_STREAM_MEMORY` |
|   279 | 2567 | `		&& pData->nCur >= SyBlobLength(&pData->sMem);` |
|     - | 2568 | `#else` |
|     - | 2569 | `	SXUNUSED(pHandle); /* cc warning */` |
|     - | 2570 | `	return 0;` |
|     - | 2571 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     4 | 2572 | `}` |
|     - | 2573 | `/*` |
|     - | 2574 | ` * Return TRUE if we are dealing with the data:// stream.` |
|     - | 2575 | ` */` |
|  1570 | 2576 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream)` |
|     5 | 2577 | `{` |
|     - | 2578 | `#ifndef PH7_DISABLE_DISK_IO` |
|  1575 | 2579 | `	return pStream == &sDATA_Stream;` |
|     - | 2580 | `#else` |
|     - | 2581 | `	SXUNUSED(pStream); /* cc warning */` |
|     - | 2582 | `	return 0;` |
|     - | 2583 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2584 | `}` |
|     - | 2585 | `/*` |
|     - | 2586 | ` * bool stream_isatty(resource $stream)` |
|     - | 2587 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|     - | 2588 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|     - | 2589 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|     - | 2590 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|     - | 2591 | ` */` |
|     8 | 2592 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2593 | `{` |
|    10 | 2594 | `	int bTty = 0,rc;` |
|     - | 2595 | `	io_private *pDev;` |
|    10 | 2596 | `	if( nArg < 1 ){` |
|   ! 0 | 2597 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2598 | `		return PH7_OK;` |
|     - | 2599 | `	}` |
|     - | 2600 | `	/* php's screen runs FIRST here too: a closed handle is a TypeError, not the` |
|     - | 2601 | ``	 * `false` a stream that simply is not a terminal answers. */`` |
|    10 | 2602 | `	pDev = PH7_StreamHandleArg(pCtx,apArg[0],1,"stream",&rc);` |
|    10 | 2603 | `	if( pDev == 0 ){` |
|     3 | 2604 | `		return rc;` |
|     - | 2605 | `	}` |
|     3 | 2606 | `	SXUNUSED(pDev); /* a build with no disk IO compiles the block below away */` |
|     - | 2607 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2608 | `	{` |
|     7 | 2609 | `		if( is_php_stream(pDev->pStream) ){` |
|     5 | 2610 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     6 | 2611 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|     4 | 2612 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|     3 | 2613 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|     - | 2614 | `#ifdef __WINNT__` |
|     1 | 2615 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|     - | 2616 | `#else` |
|     4 | 2617 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|     - | 2618 | `#endif` |
|     2 | 2619 | `			}` |
|     2 | 2620 | `		}` |
|     - | 2621 | `	}` |
|     - | 2622 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     7 | 2623 | `	ph7_result_bool(pCtx,bTty);` |
|     7 | 2624 | `	return PH7_OK;` |
|     6 | 2625 | `}` |
|     - | 2626 |  |
|     - | 2627 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2628 | `/*` |
|     - | 2629 | ` * Export the STDIN handle.` |
|     - | 2630 | ` */` |
|   160 | 2631 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|     3 | 2632 | `{` |
|     - | 2633 | `#ifndef PH7_DISABLE_DISK_IO` |
|   163 | 2634 | `	if( pVm->pStdin == 0  ){` |
|     - | 2635 | `		io_private *pIn;` |
|     - | 2636 | `		/* Allocate an IO private instance */` |
|    13 | 2637 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    13 | 2638 | `		if( pIn == 0 ){` |
|   ! 0 | 2639 | `			return 0;` |
|     - | 2640 | `		}` |
|    13 | 2641 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|    13 | 2642 | `		SetIOPrivateOpenedAs(pIn,"php://stdin",(int)sizeof("php://stdin")-1,"rb",2);` |
|     - | 2643 | `		/* Initialize the handle */` |
|    13 | 2644 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|     - | 2645 | `		/* Install the STDIN stream */` |
|    13 | 2646 | `		pVm->pStdin = pIn;` |
|    13 | 2647 | `		return pIn;` |
|   ! 0 | 2648 | `	}else{` |
|     - | 2649 | `		/* NULL or STDIN */` |
|   153 | 2650 | `		return pVm->pStdin;` |
|     - | 2651 | `	}` |
|     - | 2652 | `#else` |
|     - | 2653 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2654 | `	return 0;` |
|     - | 2655 | `#endif` |
|    83 | 2656 | `}` |
|     - | 2657 | `/*` |
|     - | 2658 | ` * Export the STDOUT handle.` |
|     - | 2659 | ` */` |
|   152 | 2660 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|     3 | 2661 | `{` |
|     - | 2662 | `#ifndef PH7_DISABLE_DISK_IO` |
|   155 | 2663 | `	if( pVm->pStdout == 0  ){` |
|     - | 2664 | `		io_private *pOut;` |
|     - | 2665 | `		/* Allocate an IO private instance */` |
|    18 | 2666 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    18 | 2667 | `		if( pOut == 0 ){` |
|   ! 0 | 2668 | `			return 0;` |
|     - | 2669 | `		}` |
|    18 | 2670 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|    18 | 2671 | `		SetIOPrivateOpenedAs(pOut,"php://stdout",(int)sizeof("php://stdout")-1,"wb",2);` |
|     - | 2672 | `		/* Initialize the handle */` |
|    18 | 2673 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|     - | 2674 | `		/* Install the STDOUT stream */` |
|    18 | 2675 | `		pVm->pStdout = pOut;` |
|    18 | 2676 | `		return pOut;` |
|   ! 0 | 2677 | `	}else{` |
|     - | 2678 | `		/* NULL or STDOUT */` |
|   140 | 2679 | `		return pVm->pStdout;` |
|     - | 2680 | `	}` |
|     - | 2681 | `#else` |
|     - | 2682 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2683 | `	return 0;` |
|     - | 2684 | `#endif` |
|    78 | 2685 | `}` |
|     - | 2686 | `/*` |
|     - | 2687 | ` * Export the STDERR handle.` |
|     - | 2688 | ` */` |
|   195 | 2689 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|     3 | 2690 | `{` |
|     - | 2691 | `#ifndef PH7_DISABLE_DISK_IO` |
|   198 | 2692 | `	if( pVm->pStderr == 0  ){` |
|     - | 2693 | `		io_private *pErr;` |
|     - | 2694 | `		/* Allocate an IO private instance */` |
|    20 | 2695 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    20 | 2696 | `		if( pErr == 0 ){` |
|   ! 0 | 2697 | `			return 0;` |
|     - | 2698 | `		}` |
|    20 | 2699 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|    20 | 2700 | `		SetIOPrivateOpenedAs(pErr,"php://stderr",(int)sizeof("php://stderr")-1,"wb",2);` |
|     - | 2701 | `		/* Initialize the handle */` |
|    20 | 2702 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|     - | 2703 | `		/* Install the STDERR stream */` |
|    20 | 2704 | `		pVm->pStderr = pErr;` |
|    20 | 2705 | `		return pErr;` |
|   ! 0 | 2706 | `	}else{` |
|     - | 2707 | `		/* NULL or STDERR */` |
|   181 | 2708 | `		return pVm->pStderr;` |
|     - | 2709 | `	}` |
|     - | 2710 | `#else` |
|     - | 2711 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2712 | `	return 0;` |
|     - | 2713 | `#endif` |
|    79 | 2714 | `}` |
|     - | 2715 |  |
