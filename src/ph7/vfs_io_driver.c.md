# src/ph7/vfs_io_driver.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 995/1275 lines (78.04%)

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
|     - |   17 | `/*` |
|     - |   18 | ` * Section:` |
|     - |   19 | ` *    Built-in IO stream drivers: php://, data://, pipe (popen) and the` |
|     - |   20 | ` *    standard stream exporters. The file:// driver lives in` |
|     - |   21 | ` *    vfs_unix.c/vfs_win.c; vfs.c owns registration.` |
|     - |   22 | ` * Status:` |
|     - |   23 | ` *    Stable.` |
|     - |   24 | ` */` |
|     - |   25 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|     - |   26 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - |   27 | `/*` |
|     - |   28 | ` * The following defines are mostly used by the UNIX built and have` |
|     - |   29 | ` * no particular meaning on windows.` |
|     - |   30 | ` */` |
|     - |   31 | `#ifndef STDIN_FILENO` |
|     - |   32 | `#define STDIN_FILENO	0` |
|     - |   33 | `#endif` |
|     - |   34 | `#ifndef STDOUT_FILENO` |
|     - |   35 | `#define STDOUT_FILENO	1` |
|     - |   36 | `#endif` |
|     - |   37 | `#ifndef STDERR_FILENO` |
|     - |   38 | `#define STDERR_FILENO	2` |
|     - |   39 | `#endif` |
|     - |   40 | `/*` |
|     - |   41 | ` * php:// Accessing various I/O streams` |
|     - |   42 | ` * According to the PHP langage reference manual` |
|     - |   43 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|     - |   44 | ` * and output streams, the standard input, output and error file descriptors.` |
|     - |   45 | ` * php://stdin, php://stdout and php://stderr:` |
|     - |   46 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|     - |   47 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|     - |   48 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|     - |   49 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|     - |   50 | ` * php://output` |
|     - |   51 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|     - |   52 | ` *  mechanism in the same way as print and echo.` |
|     - |   53 | ` */` |
|     - |   54 | `typedef struct ph7_stream_data ph7_stream_data;` |
|     - |   55 | ` /* The following structure is the private data associated with the php:// stream */` |
|     - |   56 | `struct ph7_stream_data` |
|     - |   57 | `{` |
|     - |   58 | `	ph7_vm *pVm; /* VM that own this instance */` |
|     - |   59 | `	int iType;   /* Stream type */` |
|     - |   60 | `	union{` |
|     - |   61 | `		void *pHandle; /* Stream handle */` |
|     - |   62 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|     - |   63 | `	}x;` |
|     - |   64 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|     - |   65 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|     - |   66 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|     - |   67 | `	/* FILTER type: php://filter/…/resource=… is not a stream of its own — it is` |
|     - |   68 | ``	 * the stream named by `resource=` with a chain wrapped around it. The chain`` |
|     - |   69 | `	 * lives on this io_private, so every read and write below goes through` |
|     - |   70 | `	 * PH7_StreamRead/PH7_StreamWrite and is filtered on the way. */` |
|     - |   71 | `	io_private *pInner;` |
|     - |   72 | `};` |
|     - |   73 | `/*` |
|     - |   74 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|     - |   75 | ` */` |
|   490 |   76 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|     5 |   77 | `{` |
|     - |   78 | `	ph7_stream_data *pData;` |
|   495 |   79 | `	if( pVm == 0 ){` |
|   ! 0 |   80 | `		return 0;` |
|     - |   81 | `	}` |
|     - |   82 | `	/* Allocate a new instance */` |
|   495 |   83 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|   495 |   84 | `	if( pData == 0 ){` |
|   ! 0 |   85 | `		return 0;` |
|     - |   86 | `	}` |
|     - |   87 | `	/* Zero the structure */` |
|   495 |   88 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|     - |   89 | `	/* Initialize fields */` |
|   495 |   90 | `	pData->iType = iType;` |
|   495 |   91 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|   495 |   92 | `	pData->nCur = 0;` |
|   495 |   93 | `	pData->bReadOnly = 0;` |
|   495 |   94 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|     - |   95 | `		/* Nothing else to set up: the buffer is the stream */` |
|   290 |   96 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|     - |   97 | `		/* Point to the default VM consumer routine. */` |
|     5 |   98 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|     3 |   99 | `	}else{` |
|     - |  100 | `#ifdef __WINNT__` |
|     - |  101 | `		DWORD nChannel;` |
|     4 |  102 | `		switch(iType){` |
|     3 |  103 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|     3 |  104 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|     - |  105 | `		default:` |
|     4 |  106 | `			nChannel = STD_INPUT_HANDLE;` |
|     - |  107 | `			break;` |
|     - |  108 | `		}` |
|     4 |  109 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|     - |  110 | `#else` |
|     - |  111 | `		/* Assume an UNIX system */` |
|    78 |  112 | `		int ifd = STDIN_FILENO;` |
|    78 |  113 | `		switch(iType){` |
|    12 |  114 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|    14 |  115 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|    26 |  116 | `		default:` |
|    52 |  117 | `			break;` |
|     - |  118 | `		}` |
|    78 |  119 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|     - |  120 | `#endif` |
|     - |  121 | `	}` |
|   495 |  122 | `	pData->pVm = pVm;` |
|   495 |  123 | `	return pData;` |
|   250 |  124 | `}` |
|     - |  125 | `/*` |
|     - |  126 | ` * Implementation of the php:// IO streams routines` |
|     - |  127 | ` * Status:` |
|     - |  128 | ` *   Stable.` |
|     - |  129 | ` */` |
|     - |  130 | `/*` |
|     - |  131 | ` * php://filter/<spec>/resource=<uri>. The resource is opened through the` |
|     - |  132 | ` * ordinary device dispatch and the spec's filters are attached to it; what this` |
|     - |  133 | ` * device hands back is a PROXY whose reads and writes go through that handle,` |
|     - |  134 | ` * which is what makes the chain apply to file_get_contents(), include and every` |
|     - |  135 | ` * other opener without one of them knowing about filters at all.` |
|     - |  136 | ` */` |
|     - |  137 | `static void PHPStreamData_Close(void *pHandle);` |
|    44 |  138 | `static int PHPStreamFilterOpen(const char *zSpec,int nSpec,int iMode,ph7_vm *pVm,` |
|     - |  139 | `	ph7_stream_data **ppData)` |
|     2 |  140 | `{` |
|     - |  141 | `	const ph7_io_stream *pInnerStream;` |
|    46 |  142 | `	const char *zRes = 0;` |
|     - |  143 | `	ph7_stream_data *pData;` |
|     - |  144 | `	io_private *pInner;` |
|     - |  145 | `	SyBlob sRes;` |
|    46 |  146 | `	int nRes = 0,i,iChains = 0;` |
|     - |  147 | ``	/* php looks for `/resource=` and cuts the filter list there. When the path`` |
|     - |  148 | ``	 * BEGINS with `resource=` it takes the resource and leaves the list alone —`` |
|     - |  149 | `	 * so the resource's own path segments are then tried as filter names, which` |
|     - |  150 | `	 * is exactly what php warns about. */` |
|   906 |  151 | `	for( i = 0 ; i + 10 <= nSpec ; i++ ){` |
|   906 |  152 | `		if( zSpec[i] == '/' && SyMemcmp(&zSpec[i+1],"resource=",9) == 0 ){` |
|    46 |  153 | `			zRes = &zSpec[i+10];` |
|    46 |  154 | `			nRes = nSpec - (i + 10);` |
|    46 |  155 | `			nSpec = i;` |
|    46 |  156 | `			break;` |
|     - |  157 | `		}` |
|   432 |  158 | `	}` |
|    68 |  159 | `	if( zRes == 0 ){` |
|   ! 0 |  160 | `		if( nSpec >= 9 && SyMemcmp(zSpec,"resource=",9) == 0 ){` |
|   ! 0 |  161 | `			zRes = &zSpec[9];` |
|   ! 0 |  162 | `			nRes = nSpec - 9;` |
|   ! 0 |  163 | `		}else{` |
|   ! 0 |  164 | `			PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,"No URL resource specified");` |
|   ! 0 |  165 | `			return -1;` |
|     - |  166 | `		}` |
|   ! 0 |  167 | `	}` |
|    43 |  168 | `	if( iMode & (PH7_IO_OPEN_RDONLY\|PH7_IO_OPEN_RDWR) ){` |
|    40 |  169 | `		iChains \|= PHL_STREAM_FILTER_READ;` |
|    19 |  170 | `	}` |
|    59 |  171 | `	if( iMode & (PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND) ){` |
|     7 |  172 | `		iChains \|= PHL_STREAM_FILTER_WRITE;` |
|     3 |  173 | `	}` |
|    46 |  174 | `	pData = PHPStreamDataInit(pVm,PH7_IO_STREAM_FILTER);` |
|    46 |  175 | `	if( pData == 0 ){` |
|   ! 0 |  176 | `		return -1;` |
|     - |  177 | `	}` |
|    46 |  178 | `	pInner = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    46 |  179 | `	if( pInner ){` |
|    46 |  180 | `		SyZero(pInner,sizeof(io_private));` |
|    22 |  181 | `	}` |
|    46 |  182 | `	if( pInner == 0 ){` |
|   ! 0 |  183 | `		PHPStreamData_Close((void *)pData);` |
|   ! 0 |  184 | `		return -1;` |
|     - |  185 | `	}` |
|     - |  186 | `	/* The dispatch wants a NUL-terminated URI and mutates the pointer it is` |
|     - |  187 | `	 * handed, so the resource is copied out of the path first. */` |
|    46 |  188 | `	SyBlobInit(&sRes,&pVm->sAllocator);` |
|    46 |  189 | `	SyBlobAppend(&sRes,zRes,(sxu32)nRes);` |
|    46 |  190 | `	SyBlobNullAppend(&sRes);` |
|     - |  191 | `	{` |
|    46 |  192 | `		const char *zPath = (const char *)SyBlobData(&sRes);` |
|    46 |  193 | `		pInnerStream = PH7_VmGetStreamDevice(pVm,&zPath,nRes);` |
|    46 |  194 | `		InitIOPrivate(pVm,pInnerStream,pInner);` |
|    46 |  195 | `		pInner->pHandle = pInnerStream` |
|    44 |  196 | `			? PH7_StreamOpenHandle(pVm,pInnerStream,zPath,iMode,FALSE,0,FALSE,0,0)` |
|    22 |  197 | `			: 0;` |
|    46 |  198 | `		if( pInner->pHandle == 0 ){` |
|   ! 0 |  199 | `			SyBlobRelease(&sRes);` |
|   ! 0 |  200 | `			SyMemBackendFree(&pVm->sAllocator,pInner);` |
|   ! 0 |  201 | `			PHPStreamData_Close((void *)pData);` |
|   ! 0 |  202 | `			return -1;` |
|     - |  203 | `		}` |
|    46 |  204 | `		SetIOPrivateOpenedAs(pInner,zRes,nRes,"r",1);` |
|     - |  205 | `	}` |
|    46 |  206 | `	SyBlobRelease(&sRes);` |
|    46 |  207 | `	pData->pInner = pInner;` |
|    46 |  208 | `	PH7_StreamFilterParseUrl(pVm,zSpec,nSpec,pInner,iChains);` |
|    46 |  209 | `	*ppData = pData;` |
|    46 |  210 | `	return PH7_OK;` |
|    24 |  211 | `}` |
|     - |  212 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|   428 |  213 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     5 |  214 | `{` |
|     - |  215 | `	ph7_stream_data *pData;` |
|     - |  216 | `	SyString sStream;` |
|   428 |  217 | `	if( SyStrnicmp(zName,"filter",sizeof("filter")-1) == 0` |
|   241 |  218 | `	 && (zName[6] == '/' \|\| zName[6] == 0) ){` |
|     - |  219 | `		int rc;` |
|    46 |  220 | `		if( zName[6] == 0 ){` |
|     - |  221 | `			/* php://filter with nothing behind it is not a stream at all. */` |
|   ! 0 |  222 | `			if( pResource && pResource->pVm ){` |
|   ! 0 |  223 | `				PH7_VmThrowError(pResource->pVm,pResource->pVm->pCalleeName,` |
|     - |  224 | `					PH7_CTX_WARNING,"Invalid php:// URL specified");` |
|   ! 0 |  225 | `			}` |
|   ! 0 |  226 | `			return -1;` |
|     - |  227 | `		}` |
|    46 |  228 | `		if( pResource == 0 \|\| pResource->pVm == 0 ){` |
|   ! 0 |  229 | `			return -1;` |
|     - |  230 | `		}` |
|    46 |  231 | `		rc = PHPStreamFilterOpen(&zName[7],(int)SyStrlen(&zName[7]),iMode,pResource->pVm,&pData);` |
|    46 |  232 | `		if( rc != PH7_OK ){` |
|   ! 0 |  233 | `			return -1;` |
|     - |  234 | `		}` |
|    46 |  235 | `		*ppHandle = (void *)pData;` |
|    46 |  236 | `		return PH7_OK;` |
|     - |  237 | `	}` |
|   389 |  238 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|     - |  239 | `	/* Trim leading and trailing white spaces */` |
|   389 |  240 | `	SyStringFullTrim(&sStream);` |
|     - |  241 | `	/* Stream to open */` |
|   389 |  242 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|   ! 0 |  243 | `		iMode = PH7_IO_STREAM_STDIN;` |
|   389 |  244 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|     5 |  245 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|   387 |  246 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|   ! 0 |  247 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|   385 |  248 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|   ! 0 |  249 | `		iMode = PH7_IO_STREAM_STDERR;` |
|   380 |  250 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|   198 |  251 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|     - |  252 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|     - |  253 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|   385 |  254 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|   195 |  255 | `	}else{` |
|     - |  256 | `		/* unknown stream name */` |
|   ! 0 |  257 | `		return -1;` |
|     - |  258 | `	}` |
|     - |  259 | `	/* Create our handle */` |
|   389 |  260 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|   389 |  261 | `	if( pData == 0 ){` |
|   ! 0 |  262 | `		return -1;` |
|     - |  263 | `	}` |
|     - |  264 | `	/* Make the handle public */` |
|   389 |  265 | `	*ppHandle = (void *)pData;` |
|   389 |  266 | `	return PH7_OK;` |
|   219 |  267 | `}` |
|     - |  268 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|  1688 |  269 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|     5 |  270 | `{` |
|  1693 |  271 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  1693 |  272 | `	if( pData == 0 ){` |
|   ! 0 |  273 | `		return -1;` |
|     - |  274 | `	}` |
|  1693 |  275 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     - |  276 | `		/* Through the shared reader, which is where the chain runs. */` |
|    72 |  277 | `		return PH7_StreamRead(pData->pInner,pBuffer,nDatatoRead);` |
|     - |  278 | `	}` |
|  1623 |  279 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|  1619 |  280 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|     - |  281 | `		sxu32 nRead;` |
|  1619 |  282 | `		if( pData->nCur >= nAvail ){` |
|   263 |  283 | `			return 0; /* EOF */` |
|     - |  284 | `		}` |
|  1360 |  285 | `		nRead = nAvail - pData->nCur;` |
|  1360 |  286 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|  1074 |  287 | `			nRead = (sxu32)nDatatoRead;` |
|   536 |  288 | `		}` |
|  1360 |  289 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|  1360 |  290 | `		pData->nCur += nRead;` |
|  1360 |  291 | `		return (ph7_int64)nRead;` |
|     - |  292 | `	}` |
|     5 |  293 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|     - |  294 | `		/* Forbidden */` |
|   ! 0 |  295 | `		return -1;` |
|     - |  296 | `	}` |
|     - |  297 | `#ifdef __WINNT__` |
|     - |  298 | `	{` |
|     - |  299 | `		DWORD nRd;` |
|     - |  300 | `		BOOL rc;` |
|     1 |  301 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|     1 |  302 | `		if( !rc ){` |
|     - |  303 | `			/* IO error */` |
|   ! 0 |  304 | `			return -1;` |
|     - |  305 | `		}` |
|     1 |  306 | `		return (ph7_int64)nRd;` |
|     - |  307 | `	}` |
|     - |  308 | `#elif defined(__UNIXES__)` |
|     - |  309 | `	{` |
|     - |  310 | `		ssize_t nRd;` |
|     - |  311 | `		int fd;` |
|     4 |  312 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|     4 |  313 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|     4 |  314 | `		if( nRd < 0 ){` |
|   ! 0 |  315 | `			return -1;` |
|     - |  316 | `		}` |
|     - |  317 | `		/* ZERO is end of file, not an error — the contract every other device` |
|     - |  318 | `		 * here keeps. Collapsing the two meant nothing could ever latch EOF on` |
|     - |  319 | ``		 * php://stdin, so `while (!feof(STDIN))` never ended. */`` |
|     4 |  320 | `		return (ph7_int64)nRd;` |
|     - |  321 | `	}` |
|     - |  322 | `#else` |
|     - |  323 | `	return -1;` |
|     - |  324 | `#endif` |
|   849 |  325 | `}` |
|     - |  326 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|   324 |  327 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|     4 |  328 | `{` |
|   328 |  329 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   328 |  330 | `	if( pData == 0 ){` |
|   ! 0 |  331 | `		return -1;` |
|     - |  332 | `	}` |
|   328 |  333 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     7 |  334 | `		return PH7_StreamWrite(pData->pInner,pBuf,nWrite);` |
|     - |  335 | `	}` |
|   322 |  336 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|     - |  337 | `		/* Forbidden */` |
|   ! 0 |  338 | `		return -1;` |
|   322 |  339 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     - |  340 | `		sxu32 nLen,nEnd;` |
|   310 |  341 | `		if( pData->bReadOnly ){` |
|   ! 0 |  342 | `			return -1;` |
|     - |  343 | `		}` |
|   310 |  344 | `		nLen = SyBlobLength(&pData->sMem);` |
|   310 |  345 | `		if( pData->nCur > nLen ){` |
|     - |  346 | `			/* seek past end: php zero-fills the gap */` |
|     - |  347 | `			static const char zZero[64] = {0};` |
|   ! 0 |  348 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|   ! 0 |  349 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|   ! 0 |  350 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|   ! 0 |  351 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|   ! 0 |  352 | `					return -1;` |
|     - |  353 | `				}` |
|   ! 0 |  354 | `			}` |
|   ! 0 |  355 | `			nLen = SyBlobLength(&pData->sMem);` |
|   ! 0 |  356 | `		}` |
|   310 |  357 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|   310 |  358 | `		if( pData->nCur < nLen ){` |
|     - |  359 | `			/* overwrite in place up to the current end */` |
|     8 |  360 | `			sxu32 nOver = nLen - pData->nCur;` |
|     8 |  361 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|    11 |  362 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|    11 |  363 | `			if( nEnd > nLen ){` |
|   ! 0 |  364 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|   ! 0 |  365 | `					return -1;` |
|     - |  366 | `				}` |
|   ! 0 |  367 | `			}` |
|     5 |  368 | `		}else{` |
|   304 |  369 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|   ! 0 |  370 | `				return -1;` |
|     - |  371 | `			}` |
|     - |  372 | `		}` |
|   310 |  373 | `		pData->nCur = nEnd;` |
|   310 |  374 | `		return nWrite;` |
|    13 |  375 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|     3 |  376 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|     - |  377 | `		int rc;` |
|     - |  378 | `		/* Call the vm output consumer */` |
|     3 |  379 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|     3 |  380 | `		if( rc == PH7_ABORT ){` |
|   ! 0 |  381 | `			return -1;` |
|     - |  382 | `		}` |
|     3 |  383 | `		return nWrite;` |
|     - |  384 | `	}` |
|     - |  385 | `#ifdef __WINNT__` |
|     - |  386 | `	{` |
|     - |  387 | `		DWORD nWr;` |
|     - |  388 | `		BOOL rc;` |
|   ! 0 |  389 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|   ! 0 |  390 | `		if( !rc ){` |
|     - |  391 | `			/* IO error */` |
|   ! 0 |  392 | `			return -1;` |
|     - |  393 | `		}` |
|   ! 0 |  394 | `		return (ph7_int64)nWr;` |
|     - |  395 | `	}` |
|     - |  396 | `#elif defined(__UNIXES__)` |
|     - |  397 | `	{` |
|     - |  398 | `		ssize_t nWr;` |
|     - |  399 | `		int fd;` |
|    10 |  400 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    10 |  401 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    10 |  402 | `		if( nWr < 1 ){` |
|   ! 0 |  403 | `			return -1;` |
|     - |  404 | `		}` |
|    10 |  405 | `		return (ph7_int64)nWr;` |
|     - |  406 | `	}` |
|     - |  407 | `#else` |
|     - |  408 | `	return -1;` |
|     - |  409 | `#endif` |
|   166 |  410 | `}` |
|     - |  411 | `/* void (*xClose)(void *) */` |
|   374 |  412 | `static void PHPStreamData_Close(void *pHandle)` |
|     5 |  413 | `{` |
|   379 |  414 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     - |  415 | `	ph7_vm *pVm;` |
|   379 |  416 | `	if( pData == 0 ){` |
|   ! 0 |  417 | `		return;` |
|     - |  418 | `	}` |
|   379 |  419 | `	pVm = pData->pVm;` |
|   379 |  420 | `	if( pData->iType == PH7_IO_STREAM_FILTER && pData->pInner ){` |
|     - |  421 | `		/* The write chain closes while the device below is still open. */` |
|    46 |  422 | `		PH7_StreamFilterReleaseChains(pData->pInner);` |
|    46 |  423 | `		if( pData->pInner->pStream ){` |
|    46 |  424 | `			PH7_StreamCloseHandle(pData->pInner->pStream,pData->pInner->pHandle);` |
|    22 |  425 | `		}` |
|    46 |  426 | `		SyBlobRelease(&pData->pInner->sBuffer);` |
|    46 |  427 | `		SyBlobRelease(&pData->pInner->sFilt);` |
|    46 |  428 | `		SyBlobRelease(&pData->pInner->sUri);` |
|    46 |  429 | `		SyMemBackendFree(&pVm->sAllocator,pData->pInner);` |
|    46 |  430 | `		pData->pInner = 0;` |
|    22 |  431 | `	}` |
|   379 |  432 | `	SyBlobRelease(&pData->sMem);` |
|     - |  433 | `	/* Free the instance */` |
|   379 |  434 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|   192 |  435 | `}` |
|     - |  436 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|   344 |  437 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|     4 |  438 | `{` |
|   348 |  439 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     - |  440 | `	ph7_int64 iNew;` |
|   348 |  441 | `	if( pData == 0 ){` |
|   ! 0 |  442 | `		return -1;` |
|     - |  443 | `	}` |
|   348 |  444 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     9 |  445 | `		return PH7_StreamSeekWrapped(pData->pInner,iOfft,whence);` |
|     - |  446 | `	}` |
|   340 |  447 | `	if( pData->iType != PH7_IO_STREAM_MEMORY ){` |
|   ! 0 |  448 | `		return -1;` |
|     - |  449 | `	}` |
|   340 |  450 | `	switch(whence){` |
|    13 |  451 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|     5 |  452 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|   326 |  453 | `	default:            iNew = iOfft; break;` |
|     - |  454 | `	}` |
|   340 |  455 | `	if( iNew < 0 ){` |
|   ! 0 |  456 | `		return -1;` |
|     - |  457 | `	}` |
|   340 |  458 | `	pData->nCur = (sxu32)iNew;` |
|   340 |  459 | `	return PH7_OK;` |
|   176 |  460 | `}` |
|     - |  461 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|   296 |  462 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|     4 |  463 | `{` |
|   300 |  464 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   300 |  465 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|     - |  466 | `		/* Where the SCRIPT is on the wrapped stream: the device sits past` |
|     - |  467 | `		 * whatever the chain has already produced and nobody has taken. */` |
|    15 |  468 | `		return PH7_StreamLogicalTell(pData->pInner);` |
|     - |  469 | `	}` |
|   286 |  470 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|     3 |  471 | `		return -1;` |
|     - |  472 | `	}` |
|   284 |  473 | `	return (ph7_int64)pData->nCur;` |
|   152 |  474 | `}` |
|     - |  475 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|     2 |  476 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|     1 |  477 | `{` |
|     3 |  478 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     3 |  479 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|   ! 0 |  480 | `		io_private *pIn = pData->pInner;` |
|   ! 0 |  481 | `		if( pIn == 0 \|\| pIn->pStream == 0 \|\| pIn->pStream->xTrunc == 0 ){` |
|   ! 0 |  482 | `			return -1;` |
|     - |  483 | `		}` |
|   ! 0 |  484 | `		return pIn->pStream->xTrunc(pIn->pHandle,nLen);` |
|     - |  485 | `	}` |
|     3 |  486 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|   ! 0 |  487 | `		return -1;` |
|     - |  488 | `	}` |
|     3 |  489 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|     - |  490 | `		/* shrink in place: the blob keeps its allocation */` |
|     3 |  491 | `		pData->sMem.nByte = (sxu32)nLen;` |
|     2 |  492 | `	}else{` |
|     - |  493 | `		static const char zZero[64] = {0};` |
|   ! 0 |  494 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|   ! 0 |  495 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|   ! 0 |  496 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|   ! 0 |  497 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|   ! 0 |  498 | `				return -1;` |
|     - |  499 | `			}` |
|   ! 0 |  500 | `		}` |
|     - |  501 | `	}` |
|     3 |  502 | `	return PH7_OK;` |
|     2 |  503 | `}` |
|     - |  504 | `/*` |
|     - |  505 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|     - |  506 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|     - |  507 | ` * base64). Shares the MEMORY machinery above.` |
|     - |  508 | ` */` |
|    12 |  509 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     1 |  510 | `{` |
|    13 |  511 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|     1 |  512 | `}` |
|    28 |  513 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     2 |  514 | `{` |
|     - |  515 | `	ph7_stream_data *pData;` |
|    30 |  516 | `	const char *zIn = zName;` |
|    30 |  517 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|    30 |  518 | `	const char *zComma = 0;` |
|    30 |  519 | `	int bBase64 = 0;` |
|    14 |  520 | `	SXUNUSED(iMode);` |
|     - |  521 | `	/* Find the comma separating the mediatype from the payload */` |
|   500 |  522 | `	while( zIn < zEnd ){` |
|   500 |  523 | `		if( zIn[0] == ',' ){` |
|    30 |  524 | `			zComma = zIn;` |
|    30 |  525 | `			break;` |
|     - |  526 | `		}` |
|   472 |  527 | `		zIn++;` |
|     2 |  528 | `	}` |
|    30 |  529 | `	if( zComma == 0 ){` |
|   ! 0 |  530 | `		return -1;` |
|     - |  531 | `	}` |
|    28 |  532 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|    28 |  533 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|     7 |  534 | `		bBase64 = 1;` |
|     3 |  535 | `	}` |
|    30 |  536 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|    30 |  537 | `	if( pData == 0 ){` |
|   ! 0 |  538 | `		return -1;` |
|     - |  539 | `	}` |
|    30 |  540 | `	pData->bReadOnly = 1;` |
|    30 |  541 | `	zIn = &zComma[1];` |
|    30 |  542 | `	if( bBase64 ){` |
|     7 |  543 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|   ! 0 |  544 | `			SyBlobRelease(&pData->sMem);` |
|   ! 0 |  545 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|   ! 0 |  546 | `			return -1;` |
|     - |  547 | `		}` |
|     4 |  548 | `	}else{` |
|     - |  549 | `		/* percent-decode the payload */` |
|   126 |  550 | `		while( zIn < zEnd ){` |
|   104 |  551 | `			char c = zIn[0];` |
|   104 |  552 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|     3 |  553 | `				int hi = SyHexToint(zIn[1]);` |
|     3 |  554 | `				int lo = SyHexToint(zIn[2]);` |
|     3 |  555 | `				c = (char)((hi << 4) \| lo);` |
|     3 |  556 | `				zIn += 3;` |
|     2 |  557 | `			}else{` |
|   102 |  558 | `				zIn++;` |
|     - |  559 | `			}` |
|   104 |  560 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|   ! 0 |  561 | `				SyBlobRelease(&pData->sMem);` |
|   ! 0 |  562 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|   ! 0 |  563 | `				return -1;` |
|     - |  564 | `			}` |
|     2 |  565 | `		}` |
|     - |  566 | `	}` |
|    30 |  567 | `	*ppHandle = (void *)pData;` |
|    30 |  568 | `	return PH7_OK;` |
|    16 |  569 | `}` |
|     - |  570 | `/* data:// rejects writes outright */` |
|   ! 0 |  571 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|   ! 0 |  572 | `{` |
|   ! 0 |  573 | `	SXUNUSED(pHandle);` |
|   ! 0 |  574 | `	SXUNUSED(pBuf);` |
|   ! 0 |  575 | `	SXUNUSED(nWrite);` |
|   ! 0 |  576 | `	return -1;` |
|   ! 0 |  577 | `}` |
|     - |  578 | `PH7_PRIVATE const ph7_io_stream sDATA_Stream = {` |
|     - |  579 | `	"data",` |
|     - |  580 | `	PH7_IO_STREAM_VERSION,` |
|     - |  581 | `	DataStreamData_Open,  /* xOpen */` |
|     - |  582 | `	0,   /* xOpenDir */` |
|     - |  583 | `	PHPStreamData_Close, /* xClose */` |
|     - |  584 | `	0,  /* xCloseDir */` |
|     - |  585 | `	PHPStreamData_Read,  /* xRead */` |
|     - |  586 | `	0,  /* xReadDir */` |
|     - |  587 | `	DataStreamData_Write, /* xWrite */` |
|     - |  588 | `	PHPStreamData_Seek,  /* xSeek */` |
|     - |  589 | `	0,  /* xLock */` |
|     - |  590 | `	0,  /* xRewindDir */` |
|     - |  591 | `	PHPStreamData_Tell,  /* xTell */` |
|     - |  592 | `	0,  /* xTrunc */` |
|     - |  593 | `	0,  /* xSync */` |
|     - |  594 | `	0   /* xStat */` |
|     - |  595 | `};` |
|     - |  596 | `/*` |
|     - |  597 | ` * Pipe stream implementation for popen/pclose.` |
|     - |  598 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|     - |  599 | ` * PHP-compatible process I/O functionality.` |
|     - |  600 | ` */` |
|     - |  601 | `typedef struct pipe_private pipe_private;` |
|     - |  602 | `struct pipe_private` |
|     - |  603 | `{` |
|     - |  604 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|     - |  605 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|     - |  606 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|     - |  607 | `#ifdef __WINNT__` |
|     - |  608 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|     - |  609 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|     - |  610 | `#endif` |
|     - |  611 | `};` |
|     - |  612 |  |
|     - |  613 | `#ifdef __WINNT__` |
|     - |  614 | `#include <Windows.h>` |
|     - |  615 | `#include <stdio.h>` |
|     - |  616 | `#include <io.h>` |
|     - |  617 | `#include <fcntl.h>` |
|     - |  618 | `/*` |
|     - |  619 | ` * Custom Windows popen implementation using CreateProcess.` |
|     - |  620 | ` * This allows us to properly wait for process completion.` |
|     - |  621 | ` */` |
|     - |  622 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|     5 |  623 | `{` |
|     5 |  624 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|     5 |  625 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|     5 |  626 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|     - |  627 | `	SECURITY_ATTRIBUTES sa;` |
|     - |  628 | `	STARTUPINFOW si;` |
|     - |  629 | `	PROCESS_INFORMATION pi;` |
|     5 |  630 | `	WCHAR *zWideCmd = NULL;` |
|     5 |  631 | `	FILE *pFile = NULL;` |
|     - |  632 | `	int fd;` |
|     5 |  633 | `	BOOL bRead = (zMode[0] == 'r');` |
|     5 |  634 | `	BOOL bBinary = (strchr(zMode,'b') != NULL);` |
|     - |  635 |  |
|     - |  636 | `	/* Set up security attributes for pipe inheritance */` |
|     5 |  637 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|     5 |  638 | `	sa.bInheritHandle = TRUE;` |
|     5 |  639 | `	sa.lpSecurityDescriptor = NULL;` |
|     - |  640 |  |
|     - |  641 | `	/* Create pipes for child process I/O */` |
|     5 |  642 | `	if( bRead ){` |
|     - |  643 | `		/* Reading from child's stdout */` |
|     5 |  644 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|   ! 0 |  645 | `			return NULL;` |
|     - |  646 | `		}` |
|     - |  647 | `		/* Ensure read handle is not inherited */` |
|     5 |  648 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|     5 |  649 | `		hReadPipe = hChildStdoutRd;` |
|     5 |  650 | `		*phPipe = hChildStdoutRd;` |
|     5 |  651 | `	}else{` |
|     - |  652 | `		/* Writing to child's stdin */` |
|     2 |  653 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|   ! 0 |  654 | `			return NULL;` |
|     - |  655 | `		}` |
|     - |  656 | `		/* Ensure write handle is not inherited */` |
|     2 |  657 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|     2 |  658 | `		hWritePipe = hChildStdinWr;` |
|     2 |  659 | `		*phPipe = hChildStdinWr;` |
|     - |  660 | `	}` |
|     - |  661 |  |
|     - |  662 | `	/* Convert command to wide string */` |
|     - |  663 | `	{` |
|     5 |  664 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|     5 |  665 | `		if( nLen <= 0 ){` |
|   ! 0 |  666 | `			goto cleanup_pipes;` |
|     - |  667 | `		}` |
|     5 |  668 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|     5 |  669 | `		if( !zWideCmd ){` |
|   ! 0 |  670 | `			goto cleanup_pipes;` |
|     - |  671 | `		}` |
|     5 |  672 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|     - |  673 | `	}` |
|     - |  674 |  |
|     - |  675 | `	/* Set up process startup info */` |
|     5 |  676 | `	ZeroMemory(&si, sizeof(si));` |
|     5 |  677 | `	si.cb = sizeof(si);` |
|     5 |  678 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|     5 |  679 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|     5 |  680 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|     5 |  681 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|     5 |  682 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|     - |  683 |  |
|     5 |  684 | `	ZeroMemory(&pi, sizeof(pi));` |
|     - |  685 |  |
|     - |  686 | `	/* Create the child process */` |
|     5 |  687 | `	if( !CreateProcessW(` |
|     - |  688 | `		NULL,           /* Application name */` |
|     - |  689 | `		zWideCmd,       /* Command line */` |
|     - |  690 | `		NULL,           /* Process security attributes */` |
|     - |  691 | `		NULL,           /* Thread security attributes */` |
|     - |  692 | `		TRUE,           /* Inherit handles */` |
|     - |  693 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|     - |  694 | `		NULL,           /* Environment */` |
|     - |  695 | `		NULL,           /* Current directory */` |
|     - |  696 | `		&si,            /* Startup info */` |
|     - |  697 | `		&pi             /* Process info */` |
|     - |  698 | `	)){` |
|   ! 0 |  699 | `		goto cleanup_all;` |
|     - |  700 | `	}` |
|     - |  701 |  |
|     - |  702 | `	/* Close handles we don't need in parent */` |
|     5 |  703 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|     5 |  704 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|     - |  705 |  |
|     - |  706 | `	/* Close thread handle (we only need process handle) */` |
|     5 |  707 | `	CloseHandle(pi.hThread);` |
|     - |  708 |  |
|     - |  709 | `	/* Store process handle for later waiting */` |
|     5 |  710 | `	*phProcess = pi.hProcess;` |
|     - |  711 |  |
|     - |  712 | `	/* Convert OS handle to C file descriptor, then to FILE*. The TRANSLATION mode` |
|     - |  713 | `	 * is the caller's, not a constant: cmd.exe writes CRLF, and a descriptor` |
|     - |  714 | `	 * opened _O_TEXT eats every CR on the way in. php picks it per call —` |
|     - |  715 | `	 * "rb" for exec/system/passthru, so their output is byte-exact, "rt" for` |
|     - |  716 | `	 * shell_exec, and the script's own mode for popen() — and this used to` |
|     - |  717 | ``	 * hardcode _O_TEXT for all of them, so `passthru('type file.bin')` lost every`` |
|     - |  718 | `	 * 0x0D byte and system()'s output came back LF-only where php's is CRLF. */` |
|     5 |  719 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|     - |  720 | `	                     (bRead ? _O_RDONLY : _O_WRONLY)` |
|     - |  721 | `	                     \| (bBinary ? _O_BINARY : _O_TEXT));` |
|     5 |  722 | `	if( fd == -1 ){` |
|   ! 0 |  723 | `		CloseHandle(pi.hProcess);` |
|   ! 0 |  724 | `		*phProcess = NULL;` |
|   ! 0 |  725 | `		goto cleanup_all;` |
|     - |  726 | `	}` |
|     - |  727 |  |
|     - |  728 | `	/* The stream's own translation has to agree with the descriptor's */` |
|     5 |  729 | `	pFile = _fdopen(fd, bBinary ? (bRead ? "rb" : "wb") : (bRead ? "rt" : "wt"));` |
|     5 |  730 | `	if( !pFile ){` |
|   ! 0 |  731 | `		_close(fd); /* This will also close the underlying handle */` |
|   ! 0 |  732 | `		CloseHandle(pi.hProcess);` |
|   ! 0 |  733 | `		*phProcess = NULL;` |
|   ! 0 |  734 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|   ! 0 |  735 | `		return NULL;` |
|     - |  736 | `	}` |
|     - |  737 |  |
|     5 |  738 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|     5 |  739 | `	return pFile;` |
|     - |  740 |  |
|     - |  741 | `cleanup_all:` |
|   ! 0 |  742 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|     - |  743 | `cleanup_pipes:` |
|   ! 0 |  744 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|   ! 0 |  745 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|   ! 0 |  746 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|   ! 0 |  747 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|   ! 0 |  748 | `	return NULL;` |
|     5 |  749 | `}` |
|     - |  750 |  |
|     - |  751 | `/*` |
|     - |  752 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|     - |  753 | ` */` |
|     - |  754 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|     5 |  755 | `{` |
|     5 |  756 | `	DWORD dwExitCode = 0;` |
|     - |  757 | `	int status;` |
|     - |  758 |  |
|     - |  759 | `	/* Close the FILE* (this closes the pipe) */` |
|     5 |  760 | `	fclose(pFile);` |
|     - |  761 |  |
|     5 |  762 | `	if( hProcess ){` |
|     - |  763 | `		/* Wait for the process to complete */` |
|     5 |  764 | `		WaitForSingleObject(hProcess, INFINITE);` |
|     - |  765 |  |
|     5 |  766 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|     5 |  767 | `			status = (int)dwExitCode;` |
|     5 |  768 | `		}else{` |
|   ! 0 |  769 | `			status = -1;` |
|     - |  770 | `		}` |
|     - |  771 |  |
|     - |  772 | `		/* Close process handle */` |
|     5 |  773 | `		CloseHandle(hProcess);` |
|     5 |  774 | `	}else{` |
|   ! 0 |  775 | `		status = -1;` |
|     - |  776 | `	}` |
|     - |  777 |  |
|     5 |  778 | `	return status;` |
|     5 |  779 | `}` |
|     - |  780 | `#endif /* __WINNT__ */` |
|     - |  781 | `/*` |
|     - |  782 | ` * Open a pipe to a process.` |
|     - |  783 | ` * This is called internally by popen(), not through the stream device interface.` |
|     - |  784 | ` */` |
|  5328 |  785 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|     5 |  786 | `{` |
|     - |  787 | `	pipe_private *pPipe;` |
|     - |  788 | `	FILE *pFile;` |
|  5333 |  789 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|   ! 0 |  790 | `		return 0;` |
|     - |  791 | `	}` |
|     - |  792 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|  5333 |  793 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|   ! 0 |  794 | `		return 0;` |
|     - |  795 | `	}` |
|     - |  796 | `	/* Open the pipe using system popen */` |
|     - |  797 | `#ifdef __WINNT__` |
|     - |  798 | `	{` |
|     - |  799 | `		/* Build cmd.exe command wrapper */` |
|     5 |  800 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|     5 |  801 | `		const char *zShellSuffix = "\"";` |
|     5 |  802 | `		size_t nPrefix = strlen(zShellPrefix);` |
|     5 |  803 | `		size_t nSuffix = strlen(zShellSuffix);` |
|     5 |  804 | `		size_t nCmd = strlen(zCommand);` |
|     5 |  805 | `		size_t nQuotes = 0;` |
|     5 |  806 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|     5 |  807 | `			if (zCommand[i] == '"') nQuotes++;` |
|     5 |  808 | `		}` |
|     5 |  809 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|     5 |  810 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|     5 |  811 | `		if (zCmdEsc == NULL) {` |
|   ! 0 |  812 | `			return 0;` |
|     - |  813 | `		}` |
|     - |  814 | `		/* Escape quotes in command */` |
|     5 |  815 | `		size_t j = 0;` |
|     5 |  816 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|     5 |  817 | `			char ch = zCommand[i];` |
|     5 |  818 | `			if (ch == '"') {` |
|     4 |  819 | `				zCmdEsc[j++] = '^';` |
|     4 |  820 | `				zCmdEsc[j++] = '"';` |
|     4 |  821 | `			} else {` |
|     5 |  822 | `				zCmdEsc[j++] = ch;` |
|     - |  823 | `			}` |
|     5 |  824 | `		}` |
|     5 |  825 | `		zCmdEsc[j] = '\0';` |
|     5 |  826 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|     5 |  827 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|     5 |  828 | `		if (zWinCmd == NULL) {` |
|   ! 0 |  829 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|   ! 0 |  830 | `			return 0;` |
|     - |  831 | `		}` |
|     5 |  832 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|     5 |  833 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|     5 |  834 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|     5 |  835 | `		zWinCmd[nTotal - 1] = '\0';` |
|     - |  836 | `		/* Allocate pipe structure early so we can store handles */` |
|     5 |  837 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|     5 |  838 | `		if( pPipe == 0 ){` |
|   ! 0 |  839 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|   ! 0 |  840 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|   ! 0 |  841 | `			return 0;` |
|     - |  842 | `		}` |
|     - |  843 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|     5 |  844 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|     5 |  845 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|     5 |  846 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|     5 |  847 | `		if( pFile == 0 ){` |
|   ! 0 |  848 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   ! 0 |  849 | `			return 0;` |
|     - |  850 | `		}` |
|     - |  851 | `		/* Initialize remaining fields */` |
|     5 |  852 | `		pPipe->pFile = pFile;` |
|     5 |  853 | `		pPipe->pVm = pVm;` |
|     5 |  854 | `		pPipe->iMode = zMode[0];` |
|     - |  855 | `	}` |
|     - |  856 | `#elif defined(__UNIXES__) /* Unix */` |
|     - |  857 | `	/* The mode goes to popen(3) VERBATIM, exactly as php hands it its own: a mode` |
|     - |  858 | `	 * popen(3) refuses (anything but "r"/"w" — 'b' is a Windows translation flag` |
|     - |  859 | `	 * with nothing to translate here) is an open FAILURE, which is php's answer` |
|     - |  860 | `	 * for it too. */` |
|  5328 |  861 | `	pFile = popen(zCommand, zMode);` |
|  5328 |  862 | `	if( pFile == 0 ){` |
|     2 |  863 | `		return 0;` |
|     - |  864 | `	}` |
|     - |  865 | `	/* Allocate pipe private structure */` |
|  5326 |  866 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|  5326 |  867 | `	if( pPipe == 0 ){` |
|     - |  868 | `		/* Out of memory, close the pipe */` |
|   ! 0 |  869 | `		pclose(pFile);` |
|   ! 0 |  870 | `		return 0;` |
|     - |  871 | `	}` |
|     - |  872 | `	/* Initialize the structure */` |
|  5326 |  873 | `	pPipe->pFile = pFile;` |
|  5326 |  874 | `	pPipe->pVm = pVm;` |
|  5326 |  875 | `	pPipe->iMode = zMode[0];` |
|     - |  876 | `#else /* OS_OTHER: no process pipes on this platform */` |
|     - |  877 | `	(void)pFile;` |
|     - |  878 | `	return 0;` |
|     - |  879 | `#endif` |
|  5331 |  880 | `	return pPipe;` |
|  2669 |  881 | `}` |
|     - |  882 | `/*` |
|     - |  883 | ` * Close a pipe and return the exit status of the process.` |
|     - |  884 | ` * Returns the exit status, or -1 on error.` |
|     - |  885 | ` */` |
|  5294 |  886 | `static int PipeClose(pipe_private *pPipe)` |
|     5 |  887 | `{` |
|     - |  888 | `	int status;` |
|     - |  889 | `	ph7_vm *pVm;` |
|  5299 |  890 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 |  891 | `		return -1;` |
|     - |  892 | `	}` |
|  5299 |  893 | `	pVm = pPipe->pVm;` |
|     - |  894 | `	/* Close the pipe and get exit status */` |
|     - |  895 | `#ifdef __WINNT__` |
|     - |  896 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|     5 |  897 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|     - |  898 | `#elif defined(__UNIXES__)` |
|  5294 |  899 | `	status = pclose(pPipe->pFile);` |
|     - |  900 | `	/* pclose() answers waitpid()'s raw status. php (php_stream_pclose) translates` |
|     - |  901 | `	 * exactly ONE case of it — a normal exit becomes its exit CODE — and hands the` |
|     - |  902 | `	 * raw word back for every other, so a process killed by a signal reports the` |
|     - |  903 | ``	 * SIGNAL number: `kill -TERM $$` is 15, `kill -9 $$` is 9. PHL used to add the`` |
|     - |  904 | `	 * shell's own 128 to it (143, 137), a number the same status word can also` |
|     - |  905 | ``	 * mean as an ordinary `exit 143`, and answered -1 for a stopped child. This is`` |
|     - |  906 | `	 * pclose()'s answer and, through it, exec()/system()/passthru()'s` |
|     - |  907 | `	 * $result_code. */` |
|  5294 |  908 | `	if( status != -1 && WIFEXITED(status) ){` |
|  5290 |  909 | `		status = WEXITSTATUS(status);` |
|  2645 |  910 | `	}` |
|     - |  911 | `#else /* OS_OTHER: no process pipes on this platform */` |
|     - |  912 | `	status = -1;` |
|     - |  913 | `#endif` |
|     - |  914 | `	/* Free the structure */` |
|  5299 |  915 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|  5299 |  916 | `	return status;` |
|  2652 |  917 | `}` |
|     - |  918 | `/*` |
|     - |  919 | ` * Pipe stream xClose implementation.` |
|     - |  920 | ` * Note: This is called by fclose(), not pclose().` |
|     - |  921 | ` * It closes the pipe but does not return the exit status.` |
|     - |  922 | ` */` |
|   128 |  923 | `static void PipeStream_Close(void *pHandle)` |
|     4 |  924 | `{` |
|   132 |  925 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|   132 |  926 | `	if( pPipe ){` |
|   132 |  927 | `		PipeClose(pPipe);` |
|    64 |  928 | `	}` |
|   132 |  929 | `}` |
|     - |  930 | `/*` |
|     - |  931 | ` * Pipe stream xRead implementation.` |
|     - |  932 | ` */` |
|  7820 |  933 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|     4 |  934 | `{` |
|  7824 |  935 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     - |  936 | `	size_t nRead;` |
|  7824 |  937 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 |  938 | `		return -1;` |
|     - |  939 | `	}` |
|  7824 |  940 | `	if( pPipe->iMode != 'r' ){` |
|     - |  941 | `		/* Cannot read from a write-only pipe */` |
|   ! 0 |  942 | `		return -1;` |
|     - |  943 | `	}` |
|  7824 |  944 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|  7824 |  945 | `	if( nRead == 0 ){` |
|  5080 |  946 | `		if( feof(pPipe->pFile) ){` |
|  5080 |  947 | `			return 0; /* EOF */` |
|     - |  948 | `		}` |
|   ! 0 |  949 | `		return -1; /* Error */` |
|     - |  950 | `	}` |
|  2748 |  951 | `	return (ph7_int64)nRead;` |
|  3914 |  952 | `}` |
|     - |  953 | `/*` |
|     - |  954 | ` * Pipe stream xWrite implementation.` |
|     - |  955 | ` */` |
|     6 |  956 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|     1 |  957 | `{` |
|     7 |  958 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     - |  959 | `	size_t nWritten;` |
|     7 |  960 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 |  961 | `		return -1;` |
|     - |  962 | `	}` |
|     7 |  963 | `	if( pPipe->iMode != 'w' ){` |
|     - |  964 | `		/* Cannot write to a read-only pipe */` |
|   ! 0 |  965 | `		return -1;` |
|     - |  966 | `	}` |
|     7 |  967 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|     7 |  968 | `	if( nWritten == 0 && nWrite > 0 ){` |
|   ! 0 |  969 | `		return -1; /* Error */` |
|     - |  970 | `	}` |
|     7 |  971 | `	return (ph7_int64)nWritten;` |
|     4 |  972 | `}` |
|     - |  973 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|     - |  974 | `static const ph7_io_stream sPipe_Stream = {` |
|     - |  975 | `	"pipe",` |
|     - |  976 | `	PH7_IO_STREAM_VERSION,` |
|     - |  977 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|     - |  978 | `	0,  /* xOpenDir */` |
|     - |  979 | `	PipeStream_Close,  /* xClose */` |
|     - |  980 | `	0,  /* xCloseDir */` |
|     - |  981 | `	PipeStream_Read,   /* xRead */` |
|     - |  982 | `	0,  /* xReadDir */` |
|     - |  983 | `	PipeStream_Write,  /* xWrite */` |
|     - |  984 | `	0,  /* xSeek */` |
|     - |  985 | `	0,  /* xLock */` |
|     - |  986 | `	0,  /* xRewindDir */` |
|     - |  987 | `	0,  /* xTell */` |
|     - |  988 | `	0,  /* xTrunc */` |
|     - |  989 | `	0,  /* xSync */` |
|     - |  990 | `	0   /* xStat */` |
|     - |  991 | `};` |
|     - |  992 | `/*` |
|     - |  993 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|     - |  994 | ` * FALSE otherwise.` |
|     - |  995 | ` */` |
|  5052 |  996 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|     5 |  997 | `{` |
|  5057 |  998 | `	return pStream == &sPipe_Stream;` |
|     5 |  999 | `}` |
|     - | 1000 | `/*` |
|     - | 1001 | ` * resource popen(string $command, string $mode)` |
|     - | 1002 | ` *  Opens process file pointer.` |
|     - | 1003 | ` * Parameters` |
|     - | 1004 | ` *  $command` |
|     - | 1005 | ` *   The command to execute. Passed to the system shell.` |
|     - | 1006 | ` *  $mode` |
|     - | 1007 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|     - | 1008 | ` *   'r' - Open for reading (read from the command's stdout).` |
|     - | 1009 | ` *   'w' - Open for writing (write to the command's stdin).` |
|     - | 1010 | ` * Return` |
|     - | 1011 | ` *  Returns a file pointer on success, or FALSE on error.` |
|     - | 1012 | ` */` |
|     - | 1013 | `/*` |
|     - | 1014 | `` * The longest command line the platform's shell accepts — php's `cmd_max_len`,`` |
|     - | 1015 | ` * which both escapers refuse to exceed. php reads it once at startup from` |
|     - | 1016 | ` * sysconf(_SC_ARG_MAX) and hardcodes cmd.exe's constant on Windows.` |
|     - | 1017 | ` */` |
|   266 | 1018 | `static sxu32 ShellMaxCmdLen(void)` |
|     4 | 1019 | `{` |
|     - | 1020 | `#ifdef __WINNT__` |
|     - | 1021 | `	/* An escaped command runs through cmd.exe, whose limit is a constant. */` |
|     4 | 1022 | `	return 8192;` |
|     - | 1023 | `#elif defined(__UNIXES__) && defined(_SC_ARG_MAX)` |
|   266 | 1024 | `	long iMax = sysconf(_SC_ARG_MAX);` |
|   266 | 1025 | `	if( iMax <= 0 ){` |
|   ! 0 | 1026 | `		return 4096;   /* php's _POSIX_ARG_MAX fallback */` |
|     - | 1027 | `	}` |
|   266 | 1028 | `	return (sxu32)iMax;` |
|     - | 1029 | `#else` |
|     - | 1030 | `	return 4096;` |
|     - | 1031 | `#endif` |
|   137 | 1032 | `}` |
|     - | 1033 | `/*` |
|     - | 1034 | ` * How the two escapers WALK their argument, and the one thing they share.` |
|     - | 1035 | ` *` |
|     - | 1036 | ` * php walks it with php_mblen(), the process LC_CTYPE's multibyte reader: a` |
|     - | 1037 | ` * well-formed sequence is copied through untouched (a metacharacter's byte value` |
|     - | 1038 | ` * inside one is NOT a metacharacter), and a byte the encoding cannot start a` |
|     - | 1039 | ` * character with is DROPPED. That reader's answer is platform-shaped, and this` |
|     - | 1040 | ` * follows it on both, because it is what php answers on each:` |
|     - | 1041 | ` *` |
|     - | 1042 | ` *   POSIX    php picks LC_CTYPE up from the environment at startup, so the` |
|     - | 1043 | ` *            everyday answer is a UTF-8 one — a well-formed sequence rides` |
|     - | 1044 | ` *            through and an ill-formed byte is dropped. PHL is UTF-8-only (§10)` |
|     - | 1045 | ` *            and has no setlocale, so PH7_Utf8ReadStrict IS that reader.` |
|     - | 1046 | ` *            (php in the "C" locale glibc falls back to drops every byte >= 0x80` |
|     - | 1047 | ` *            instead, which is why the corpus guards this half on the oracle's` |
|     - | 1048 | ` *            own LC_CTYPE rather than pinning it unconditionally.)` |
|     - | 1049 | ` *   Windows  php reports LC_CTYPE "C", and MSVCRT's C locale is SINGLE-BYTE, not` |
|     - | 1050 | ` *            ASCII: mblen() answers 1 for every byte, so nothing is ever dropped` |
|     - | 1051 | `` *            and `\xFF` reaches the escape table below. Verified against php`` |
|     - | 1052 | ` *            8.5.8 on the gate VM, which does have an oracle — walking UTF-8` |
|     - | 1053 | ` *            there instead deleted bytes php keeps.` |
|     - | 1054 | ` *` |
|     - | 1055 | ` * Neither escaper can see a NUL byte: php parses both parameters with` |
|     - | 1056 | ` * Z_PARAM_PATH and the central screen (VmBuiltinPathMask) refuses one first.` |
|     - | 1057 | ` */` |
|  8636 | 1058 | `static int ShellCharIsWellFormed(const unsigned char *zIn,sxu32 nLeft,sxu32 *pnSeq)` |
|     4 | 1059 | `{` |
|     - | 1060 | `#ifdef __WINNT__` |
|     - | 1061 | `	SXUNUSED(zIn);` |
|     - | 1062 | `	SXUNUSED(nLeft);` |
|     4 | 1063 | `	*pnSeq = 1;` |
|     4 | 1064 | `	return 1;` |
|     - | 1065 | `#else` |
|  8636 | 1066 | `	return PH7_Utf8ReadStrict(zIn,nLeft,pnSeq) >= 0;` |
|     - | 1067 | `#endif` |
|     4 | 1068 | `}` |
|     - | 1069 | `/*` |
|     - | 1070 | ` * php's escapeshellarg(): wrap the whole argument in quotes the shell does not` |
|     - | 1071 | ` * look inside, and neutralise the one byte that could end them.` |
|     - | 1072 | ` *` |
|     - | 1073 | `` * POSIX: single quotes, and a `'` becomes `'\''` — close, escape, reopen.`` |
|     - | 1074 | `` * Windows: double quotes; there is no in-quote escape for `"` on cmd.exe, so php`` |
|     - | 1075 | `` * REPLACES `"` (and `%`/`!`, which cmd.exe still expands inside quotes) with a`` |
|     - | 1076 | ` * space, and doubles a trailing ODD run of backslashes so the last one escapes` |
|     - | 1077 | ` * itself rather than the closing quote.` |
|     - | 1078 | ` */` |
|   200 | 1079 | `static void ShellEscapeArg(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|     4 | 1080 | `{` |
|   204 | 1081 | `	sxu32 i = 0;` |
|     - | 1082 | `#ifdef __WINNT__` |
|     4 | 1083 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|     - | 1084 | `#else` |
|   200 | 1085 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|     - | 1086 | `#endif` |
|  8460 | 1087 | `	while( i < nLen ){` |
|  8260 | 1088 | `		sxu32 nSeq = 1;` |
|  8260 | 1089 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|     - | 1090 | `			/* Ill-formed: php skips the byte rather than escaping it */` |
|    18 | 1091 | `			i += nSeq;` |
|    22 | 1092 | `			continue;` |
|     - | 1093 | `		}` |
|  8242 | 1094 | `		if( nSeq > 1 ){` |
|     8 | 1095 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|     8 | 1096 | `			i += nSeq;` |
|     8 | 1097 | `			continue;` |
|     - | 1098 | `		}` |
|     - | 1099 | `#ifdef __WINNT__` |
|     4 | 1100 | `		if( zIn[i] == '"' \|\| zIn[i] == '%' \|\| zIn[i] == '!' ){` |
|     1 | 1101 | `			SyBlobAppend(pOut," ",sizeof(char));` |
|     1 | 1102 | `		}else{` |
|     4 | 1103 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|     - | 1104 | `		}` |
|     - | 1105 | `#else` |
|  8230 | 1106 | `		if( zIn[i] == '\'' ){` |
|     8 | 1107 | `			SyBlobAppend(pOut,"'\\'",sizeof("'\\'")-1);` |
|     4 | 1108 | `		}` |
|  8230 | 1109 | `		SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|     - | 1110 | `#endif` |
|  8234 | 1111 | `		i++;` |
|     4 | 1112 | `	}` |
|     - | 1113 | `#ifdef __WINNT__` |
|     - | 1114 | `	{` |
|     - | 1115 | `		/* A trailing run of backslashes would escape the closing quote if it is` |
|     - | 1116 | `		 * odd; the opening quote at offset 0 stops the scan the way php's does. */` |
|     4 | 1117 | `		const char *zCur = (const char *)SyBlobData(pOut);` |
|     4 | 1118 | `		sxu32 nCur = SyBlobLength(pOut);` |
|     4 | 1119 | `		sxu32 k = 0;` |
|     4 | 1120 | `		while( k < nCur && zCur[nCur - 1 - k] == '\\' ){` |
|     1 | 1121 | `			k++;` |
|     1 | 1122 | `		}` |
|     4 | 1123 | `		if( (k & 1) != 0 ){` |
|     1 | 1124 | `			SyBlobAppend(pOut,"\\",sizeof(char));` |
|     - | 1125 | `		}` |
|     - | 1126 | `	}` |
|     4 | 1127 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|     - | 1128 | `#else` |
|   200 | 1129 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|     - | 1130 | `#endif` |
|   204 | 1131 | `}` |
|     - | 1132 | `/*` |
|     - | 1133 | ` * php's escapeshellcmd(): the argument is a COMMAND, so it is not quoted at all —` |
|     - | 1134 | ` * every byte that could break out of one is prefixed with the shell's escape` |
|     - | 1135 | `` * character instead (`\` on POSIX, `^` on cmd.exe).`` |
|     - | 1136 | ` *` |
|     - | 1137 | ` * The one shape that is not a straight escape is a quote on POSIX: php leaves a` |
|     - | 1138 | ` * PAIR of them alone (the command may legitimately quote one of its own` |
|     - | 1139 | `` * arguments) and escapes an unpaired one. `pPair` is php's own one-slot state for`` |
|     - | 1140 | ` * that — it remembers the partner it found for the quote currently open, so the` |
|     - | 1141 | ` * closing one is recognised and the pairing resets. cmd.exe has no such rule, so` |
|     - | 1142 | `` * both quote characters (and `%`/`!`) are ordinary escapes there.`` |
|     - | 1143 | ` */` |
|    64 | 1144 | `static void ShellEscapeCmd(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|     1 | 1145 | `{` |
|    65 | 1146 | `	sxu32 i = 0;` |
|     - | 1147 | `#ifndef __WINNT__` |
|    64 | 1148 | `	const unsigned char *pPair = 0;` |
|     - | 1149 | `	static const char zEsc[] = "\\";` |
|     - | 1150 | `#else` |
|     - | 1151 | `	static const char zEsc[] = "^";` |
|     - | 1152 | `#endif` |
|   445 | 1153 | `	while( i < nLen ){` |
|   381 | 1154 | `		sxu32 nSeq = 1;` |
|   381 | 1155 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|    18 | 1156 | `			i += nSeq;` |
|    22 | 1157 | `			continue;` |
|     - | 1158 | `		}` |
|   363 | 1159 | `		if( nSeq > 1 ){` |
|     8 | 1160 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|     8 | 1161 | `			i += nSeq;` |
|     8 | 1162 | `			continue;` |
|     - | 1163 | `		}` |
|   355 | 1164 | `		switch( zIn[i] ){` |
|     - | 1165 | `#ifndef __WINNT__` |
|    12 | 1166 | `		case '"':` |
|     - | 1167 | `		case '\'':` |
|    19 | 1168 | `			if( pPair == 0` |
|    14 | 1169 | `			 && (pPair = (const unsigned char *)memchr(&zIn[i+1],zIn[i],nLen - i - 1)) != 0 ){` |
|     - | 1170 | `				/* This quote opens a pair: leave both of them alone */` |
|    17 | 1171 | `			}else if( pPair != 0 && pPair[0] == zIn[i] ){` |
|     8 | 1172 | `				pPair = 0;   /* the partner: pairing satisfied */` |
|     4 | 1173 | `			}else{` |
|     8 | 1174 | `				SyBlobAppend(pOut,zEsc,sizeof(char));` |
|     - | 1175 | `			}` |
|    24 | 1176 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|    24 | 1177 | `			break;` |
|     - | 1178 | `#else` |
|     - | 1179 | `		/* cmd.exe expands %VAR% and !VAR! even inside quotes, and has no` |
|     - | 1180 | ``		 * in-quote escape, so all four are plain `^` escapes there. */`` |
|     - | 1181 | `		case '%':` |
|     - | 1182 | `		case '!':` |
|     - | 1183 | `		case '"':` |
|     - | 1184 | `		case '\'':` |
|     - | 1185 | `#endif` |
|    21 | 1186 | `		case '#':` |
|     - | 1187 | `		case '&':` |
|     - | 1188 | `		case ';':` |
|     - | 1189 | ``		case '`':`` |
|     - | 1190 | `		case '\|':` |
|     - | 1191 | `		case '*':` |
|     - | 1192 | `		case '?':` |
|     - | 1193 | `		case '~':` |
|     - | 1194 | `		case '<':` |
|     - | 1195 | `		case '>':` |
|     - | 1196 | `		case '^':` |
|     - | 1197 | `		case '(':` |
|     - | 1198 | `		case ')':` |
|     - | 1199 | `		case '[':` |
|     - | 1200 | `		case ']':` |
|     - | 1201 | `		case '{':` |
|     - | 1202 | `		case '}':` |
|     - | 1203 | `		case '$':` |
|     - | 1204 | `		case '\\':` |
|     - | 1205 | `		case 0x0A:` |
|     - | 1206 | `		/* php escapes 0xFF too, and this is the row that decides the walk above` |
|     - | 1207 | `		 * is worth getting right: it is unreachable under the UTF-8 walk (0xF5..` |
|     - | 1208 | `		 * 0xFF is never a lead byte, so the reader drops the byte first) and` |
|     - | 1209 | `		 * REACHED on Windows, where php's single-byte C locale hands it here —` |
|     - | 1210 | ``		 * `escapeshellcmd("a\xffb")` is `a^\xffb` on the oracle. */`` |
|     - | 1211 | `		case 0xFF:` |
|    43 | 1212 | `			SyBlobAppend(pOut,zEsc,sizeof(char));` |
|     - | 1213 | `			/* fall through */` |
|   165 | 1214 | `		default:` |
|   331 | 1215 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|   330 | 1216 | `			break;` |
|     - | 1217 | `		}` |
|   355 | 1218 | `		i++;` |
|     1 | 1219 | `	}` |
|    65 | 1220 | `}` |
|     - | 1221 | `/*` |
|     - | 1222 | ` * string escapeshellarg(string $arg)` |
|     - | 1223 | ` *  Escape an argument so a shell passes it to the command as ONE word, whatever` |
|     - | 1224 | ` *  it contains.` |
|     - | 1225 | ` */` |
|   200 | 1226 | `PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1227 | `{` |
|   204 | 1228 | `	sxu32 nMax = ShellMaxCmdLen();` |
|     - | 1229 | `	const char *zArg;` |
|     - | 1230 | `	SyBlob sOut;` |
|     - | 1231 | `	int nLen;` |
|   100 | 1232 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|   204 | 1233 | `	zArg = ph7_value_to_string(apArg[0],&nLen);` |
|     - | 1234 | `	/* php's own bound: the command line has to hold the two quotes and a NUL */` |
|   204 | 1235 | `	if( nLen > 0 && (sxu32)nLen > nMax - 3 ){` |
|   ! 0 | 1236 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1237 | `			"Argument exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1238 | `	}` |
|   204 | 1239 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   204 | 1240 | `	ShellEscapeArg((const unsigned char *)zArg,(sxu32)nLen,&sOut);` |
|   204 | 1241 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|   ! 0 | 1242 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1243 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1244 | `			"Escaped argument exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1245 | `	}` |
|   204 | 1246 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   204 | 1247 | `	SyBlobRelease(&sOut);` |
|   204 | 1248 | `	return PH7_OK;` |
|   104 | 1249 | `}` |
|     - | 1250 | `/*` |
|     - | 1251 | ` * string escapeshellcmd(string $command)` |
|     - | 1252 | ` *  Escape every character that could break out of a shell command.` |
|     - | 1253 | ` */` |
|    66 | 1254 | `PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1255 | `{` |
|    67 | 1256 | `	sxu32 nMax = ShellMaxCmdLen();` |
|     - | 1257 | `	const char *zCmd;` |
|     - | 1258 | `	SyBlob sOut;` |
|     - | 1259 | `	int nLen;` |
|    33 | 1260 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|    67 | 1261 | `	zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|    67 | 1262 | `	if( nLen < 1 ){` |
|     - | 1263 | `		/* php answers "" without running the escaper at all */` |
|     3 | 1264 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 1265 | `		return PH7_OK;` |
|     - | 1266 | `	}` |
|    65 | 1267 | `	if( (sxu32)nLen > nMax - 3 ){` |
|   ! 0 | 1268 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1269 | `			"Command exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1270 | `	}` |
|    65 | 1271 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    65 | 1272 | `	ShellEscapeCmd((const unsigned char *)zCmd,(sxu32)nLen,&sOut);` |
|    65 | 1273 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|   ! 0 | 1274 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1275 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1276 | `			"Escaped command exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1277 | `	}` |
|    65 | 1278 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    65 | 1279 | `	SyBlobRelease(&sOut);` |
|    65 | 1280 | `	return PH7_OK;` |
|    34 | 1281 | `}` |
|     - | 1282 | `/*` |
|     - | 1283 | ` * php refuses an EMPTY command in all four runners (exec/system/passthru/` |
|     - | 1284 | ` * shell_exec) — the shell would answer success for one, so the refusal is the` |
|     - | 1285 | ` * only way a script hears about a command string that came out empty.` |
|     - | 1286 | ` */` |
|     8 | 1287 | `static sxi32 ShellEmptyCommandError(ph7_context *pCtx)` |
|     1 | 1288 | `{` |
|    13 | 1289 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     4 | 1290 | `		"%s(): Argument #1 ($command) must not be empty",ph7_function_name(pCtx));` |
|     1 | 1291 | `}` |
|     - | 1292 | `/*` |
|     - | 1293 | ` * string\|false\|null shell_exec(string $command)` |
|     - | 1294 | ` *  Execute a command via the shell and return the complete output as a string.` |
|     - | 1295 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|     - | 1296 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|     - | 1297 | ` */` |
|    76 | 1298 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1299 | `{` |
|     - | 1300 | `	const char *zCommand;` |
|     - | 1301 | `	pipe_private *pPipe;` |
|     - | 1302 | `	SyBlob sOut;` |
|     - | 1303 | `	char zBuf[4096];` |
|     - | 1304 | `	size_t nRead;` |
|     - | 1305 | `	int nCmdLen;` |
|    79 | 1306 | `	if( nArg < 1 ){` |
|   ! 0 | 1307 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1308 | `		return PH7_OK;` |
|     - | 1309 | `	}` |
|    79 | 1310 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|    79 | 1311 | `	if( nCmdLen < 1 ){` |
|     - | 1312 | `		/* php refuses an empty command rather than running the shell on it */` |
|     3 | 1313 | `		return ShellEmptyCommandError(pCtx);` |
|     - | 1314 | `	}` |
|    77 | 1315 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|    77 | 1316 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|     - | 1317 | `		/* php's own wording for this one; the three runners below say "Unable to` |
|     - | 1318 | `		 * fork [%s]" instead. Both used to be silent. */` |
|   ! 0 | 1319 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to execute '%s'",` |
|   ! 0 | 1320 | `			ph7_function_name(pCtx),zCommand);` |
|   ! 0 | 1321 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1322 | `		return PH7_OK;` |
|     - | 1323 | `	}` |
|    77 | 1324 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    74 | 1325 | `	for(;;){` |
|   151 | 1326 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|   151 | 1327 | `		if( nRead < 1 ){` |
|    77 | 1328 | `			break;` |
|     - | 1329 | `		}` |
|    77 | 1330 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|     3 | 1331 | `	}` |
|    77 | 1332 | `	PipeClose(pPipe);` |
|    77 | 1333 | `	if( SyBlobLength(&sOut) < 1 ){` |
|     - | 1334 | `		/* php answers NULL, not "", when the command printed nothing */` |
|   ! 0 | 1335 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1336 | `	}else{` |
|    77 | 1337 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     - | 1338 | `	}` |
|    77 | 1339 | `	SyBlobRelease(&sOut);` |
|    77 | 1340 | `	return PH7_OK;` |
|    41 | 1341 | `}` |
|     - | 1342 | `/*` |
|     - | 1343 | ` * php's three command RUNNERS are one routine (php_exec) with a mode, and the` |
|     - | 1344 | ` * mode decides two things: what happens to each LINE of the command's output,` |
|     - | 1345 | ` * and what the call answers.` |
|     - | 1346 | ` *` |
|     - | 1347 | ` *   exec($cmd)           keep nothing, answer the LAST line` |
|     - | 1348 | ` *   exec($cmd, $output)  append every line to the array, answer the last line` |
|     - | 1349 | ` *   system($cmd)         WRITE every line as it arrives, answer the last line` |
|     - | 1350 | ` *   passthru($cmd)       write the raw bytes, answer NULL` |
|     - | 1351 | ` *` |
|     - | 1352 | ` * "The last line" is php's: its trailing WHITESPACE is stripped — spaces and` |
|     - | 1353 | ` * tabs as much as the newline — and so is every element of $output's. A command` |
|     - | 1354 | ` * that printed nothing answers "" rather than false, which is php's documented` |
|     - | 1355 | ` * BC wart and not an error indication; the error indication is FALSE, and only` |
|     - | 1356 | ` * a pipe that could not be opened produces it.` |
|     - | 1357 | ` *` |
|     - | 1358 | ` * All three share the exit status, which is the pipe's close status (php's` |
|     - | 1359 | ` * $result_code out-param) and -1 when there was no process at all.` |
|     - | 1360 | ` */` |
|     - | 1361 | `#ifdef __WINNT__` |
|     - | 1362 | `# define SHELL_RUN_PIPE_MODE "rb"` |
|     - | 1363 | `#else` |
|     - | 1364 | `# define SHELL_RUN_PIPE_MODE "r"` |
|     - | 1365 | `#endif` |
|     - | 1366 | `#define SHELL_RUN_LAST     0   /* exec() with no $output array */` |
|     - | 1367 | `#define SHELL_RUN_ECHO     1   /* system() */` |
|     - | 1368 | `#define SHELL_RUN_COLLECT  2   /* exec() with one */` |
|     - | 1369 | `#define SHELL_RUN_RAW      3   /* passthru() */` |
|     - | 1370 | `/*` |
|     - | 1371 | ` * php's strip_trailing_whitespace(): answers the length that stays.` |
|     - | 1372 | ` */` |
|    74 | 1373 | `static sxu32 ShellStripTrailing(const char *zLine,sxu32 nLine)` |
|     2 | 1374 | `{` |
|   174 | 1375 | `	while( nLine > 0 && SyisSpace((unsigned char)zLine[nLine - 1]) ){` |
|   100 | 1376 | `		nLine--;` |
|     2 | 1377 | `	}` |
|    76 | 1378 | `	return nLine;` |
|     2 | 1379 | `}` |
|     - | 1380 | `/*` |
|     - | 1381 | ` * One complete line of output, dealt with the mode's way. The line still carries` |
|     - | 1382 | ` * its own newline: system() writes it (php hands the whole line to the output` |
|     - | 1383 | ` * layer, so an output buffer catches it like any echo), and the collector strips` |
|     - | 1384 | ` * it along with the rest of the trailing whitespace.` |
|     - | 1385 | ` */` |
|    50 | 1386 | `static sxi32 ShellHandleLine(ph7_context *pCtx,int iType,ph7_value *pArray,` |
|     - | 1387 | `	const char *zLine,sxu32 nLine)` |
|     2 | 1388 | `{` |
|    52 | 1389 | `	if( iType == SHELL_RUN_ECHO ){` |
|    12 | 1390 | `		return ph7_context_output(pCtx,zLine,(int)nLine);` |
|     - | 1391 | `	}` |
|    41 | 1392 | `	if( iType == SHELL_RUN_COLLECT && pArray ){` |
|    41 | 1393 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    41 | 1394 | `		if( pVal == 0 ){` |
|   ! 0 | 1395 | `			return PH7_OK;` |
|     - | 1396 | `		}` |
|    41 | 1397 | `		ph7_value_string(pVal,zLine,(int)ShellStripTrailing(zLine,nLine));` |
|    41 | 1398 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    41 | 1399 | `		ph7_context_release_value(pCtx,pVal);` |
|    20 | 1400 | `	}` |
|    41 | 1401 | `	return PH7_OK;` |
|    27 | 1402 | `}` |
|     - | 1403 | `/*` |
|     - | 1404 | ` * Run $command through the shell in the given mode, fill the by-reference` |
|     - | 1405 | ` * out-params and set the call's result. The three builtins below are this` |
|     - | 1406 | ` * routine plus their own mode.` |
|     - | 1407 | ` */` |
|    46 | 1408 | `static sxi32 ShellRunCommand(ph7_context *pCtx,int iType,int nArg,ph7_value **apArg)` |
|     2 | 1409 | `{` |
|     - | 1410 | `	/* exec() carries $output before $result_code; the other two do not */` |
|    48 | 1411 | `	int iCodeArg = (iType == SHELL_RUN_LAST) ? 2 : 1;` |
|    48 | 1412 | `	ph7_value *pArray = 0, *pOwned = 0;` |
|     - | 1413 | `	const char *zCommand;` |
|     - | 1414 | `	pipe_private *pPipe;` |
|     - | 1415 | `	SyBlob sLine, sLast;` |
|     - | 1416 | `	char zBuf[4096];` |
|     - | 1417 | `	size_t nRead;` |
|    48 | 1418 | `	int nCmdLen, iStatus = -1;` |
|    48 | 1419 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|    48 | 1420 | `	if( nCmdLen < 1 ){` |
|     7 | 1421 | `		return ShellEmptyCommandError(pCtx);` |
|     - | 1422 | `	}` |
|     - | 1423 | `	/* $output turns exec() into the collecting mode. php uses the array the` |
|     - | 1424 | `	 * caller already holds — the manual's "will append to the end of the array" —` |
|     - | 1425 | `	 * and replaces anything else with a fresh one, BEFORE running the command, so` |
|     - | 1426 | `	 * even a failed run leaves the variable an array. */` |
|    42 | 1427 | `	if( iType == SHELL_RUN_LAST && nArg > 1 ){` |
|    27 | 1428 | `		iType = SHELL_RUN_COLLECT;` |
|    27 | 1429 | `		if( ph7_value_is_array(apArg[1]) ){` |
|     2 | 1430 | `			PH7_HashmapCowSeparate(pCtx->pVm,apArg[1]);` |
|     2 | 1431 | `			pArray = apArg[1];` |
|     1 | 1432 | `		}else{` |
|    25 | 1433 | `			pOwned = pArray = ph7_context_new_array(pCtx);` |
|     - | 1434 | `		}` |
|    13 | 1435 | `	}` |
|     - | 1436 | `	/* php_exec's own mode, per platform: the three runners hand back what the` |
|     - | 1437 | `	 * command WROTE, so on Windows the CRs have to survive the pipe (where` |
|     - | 1438 | `	 * shell_exec() takes php's "rt" and does translate them). popen(3) refuses a` |
|     - | 1439 | `	 * 'b' it has nothing to translate, which is why this is not one string. */` |
|    42 | 1440 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,SHELL_RUN_PIPE_MODE);` |
|    42 | 1441 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1442 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to fork [%s]",` |
|   ! 0 | 1443 | `			ph7_function_name(pCtx),zCommand);` |
|   ! 0 | 1444 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1445 | `	}else{` |
|    42 | 1446 | `		int bAbort = 0;   /* the output consumer asked to stop (PH7_ABORT) */` |
|    42 | 1447 | `		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|    42 | 1448 | `		SyBlobInit(&sLast,&pCtx->pVm->sAllocator);` |
|    36 | 1449 | `		for(;;){` |
|    80 | 1450 | `			nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|    80 | 1451 | `			if( nRead < 1 ){` |
|    42 | 1452 | `				break;` |
|     - | 1453 | `			}` |
|    40 | 1454 | `			if( iType == SHELL_RUN_RAW ){` |
|     - | 1455 | `				/* passthru() never looks for a line: php writes what it read */` |
|     8 | 1456 | `				if( ph7_context_output(pCtx,zBuf,(int)nRead) == PH7_ABORT ){` |
|   ! 0 | 1457 | `					break;` |
|     - | 1458 | `				}` |
|     8 | 1459 | `				continue;` |
|     - | 1460 | `			}` |
|     - | 1461 | `			{` |
|    34 | 1462 | `				size_t iOfft = 0;` |
|    82 | 1463 | `				while( iOfft < nRead ){` |
|    56 | 1464 | `					const char *zNl = (const char *)memchr(&zBuf[iOfft],'\n',nRead - iOfft);` |
|    56 | 1465 | `					size_t nChunk = zNl ? (size_t)(zNl - &zBuf[iOfft]) + 1 : nRead - iOfft;` |
|    56 | 1466 | `					SyBlobAppend(&sLine,&zBuf[iOfft],(sxu32)nChunk);` |
|    56 | 1467 | `					iOfft += nChunk;` |
|    56 | 1468 | `					if( zNl == 0 ){` |
|     6 | 1469 | `						break;   /* the line continues in the next read */` |
|     - | 1470 | `					}` |
|    72 | 1471 | `					if( ShellHandleLine(pCtx,iType,pArray,` |
|    74 | 1472 | `						(const char *)SyBlobData(&sLine),SyBlobLength(&sLine)) == PH7_ABORT ){` |
|   ! 0 | 1473 | `						bAbort = 1;` |
|   ! 0 | 1474 | `					}` |
|     - | 1475 | `					/* Keep it: the call answers the last line it saw */` |
|    50 | 1476 | `					SyBlobReset(&sLast);` |
|    50 | 1477 | `					SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|    50 | 1478 | `					SyBlobReset(&sLine);` |
|    50 | 1479 | `					if( bAbort ){` |
|   ! 0 | 1480 | `						break;` |
|     - | 1481 | `					}` |
|     2 | 1482 | `				}` |
|     - | 1483 | `			}` |
|    34 | 1484 | `			if( bAbort ){` |
|   ! 0 | 1485 | `				break;` |
|     - | 1486 | `			}` |
|     2 | 1487 | `		}` |
|     - | 1488 | `		/* Output that ended without a newline is still a line */` |
|    42 | 1489 | `		if( !bAbort && SyBlobLength(&sLine) > 0 ){` |
|     3 | 1490 | `			ShellHandleLine(pCtx,iType,pArray,` |
|     2 | 1491 | `				(const char *)SyBlobData(&sLine),SyBlobLength(&sLine));` |
|     2 | 1492 | `			SyBlobReset(&sLast);` |
|     2 | 1493 | `			SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|     1 | 1494 | `		}` |
|    42 | 1495 | `		iStatus = PipeClose(pPipe);` |
|    42 | 1496 | `		if( iType == SHELL_RUN_RAW ){` |
|     8 | 1497 | `			ph7_result_null(pCtx);` |
|     5 | 1498 | `		}else{` |
|    53 | 1499 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&sLast),` |
|    34 | 1500 | `				(int)ShellStripTrailing((const char *)SyBlobData(&sLast),SyBlobLength(&sLast)));` |
|     - | 1501 | `		}` |
|    42 | 1502 | `		SyBlobRelease(&sLine);` |
|    42 | 1503 | `		SyBlobRelease(&sLast);` |
|     - | 1504 | `	}` |
|    42 | 1505 | `	if( pOwned ){` |
|    25 | 1506 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOwned);` |
|    25 | 1507 | `		ph7_context_release_value(pCtx,pOwned);` |
|    12 | 1508 | `	}` |
|    37 | 1509 | `	if( nArg > iCodeArg ){` |
|     - | 1510 | `		ph7_value sVal;` |
|    20 | 1511 | `		PH7_MemObjInitFromInt(pCtx->pVm,&sVal,iStatus);` |
|    20 | 1512 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iCodeArg],&sVal);` |
|    20 | 1513 | `		PH7_MemObjRelease(&sVal);` |
|     9 | 1514 | `	}` |
|    42 | 1515 | `	return PH7_OK;` |
|    25 | 1516 | `}` |
|     - | 1517 | `/*` |
|     - | 1518 | ` * string\|false exec(string $command, array &$output = null, int &$result_code = null)` |
|     - | 1519 | ` *  Run a command and answer the last line of its output.` |
|     - | 1520 | ` */` |
|    30 | 1521 | `PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1522 | `{` |
|    31 | 1523 | `	return ShellRunCommand(pCtx,SHELL_RUN_LAST,nArg,apArg);` |
|     1 | 1524 | `}` |
|     - | 1525 | `/*` |
|     - | 1526 | ` * string\|false system(string $command, int &$result_code = null)` |
|     - | 1527 | ` *  Run a command, write its output as it arrives, answer the last line.` |
|     - | 1528 | ` */` |
|     8 | 1529 | `PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1530 | `{` |
|    10 | 1531 | `	return ShellRunCommand(pCtx,SHELL_RUN_ECHO,nArg,apArg);` |
|     2 | 1532 | `}` |
|     - | 1533 | `/*` |
|     - | 1534 | ` * ?false passthru(string $command, int &$result_code = null)` |
|     - | 1535 | ` *  Run a command and write its output through, byte for byte.` |
|     - | 1536 | ` */` |
|     8 | 1537 | `PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1538 | `{` |
|    10 | 1539 | `	return ShellRunCommand(pCtx,SHELL_RUN_RAW,nArg,apArg);` |
|     2 | 1540 | `}` |
|     - | 1541 | `/*` |
|     - | 1542 | ` * bool proc_nice(int $priority)` |
|     - | 1543 | ` *  Change the priority of the running process — the last member of php's own` |
|     - | 1544 | ` *  process-execution surface, and the only one of the seven that runs no shell.` |
|     - | 1545 | ` */` |
|     6 | 1546 | `PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1547 | `{` |
|     - | 1548 | `	ph7_int64 iPri;` |
|     3 | 1549 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|     7 | 1550 | `	iPri = ph7_value_to_int64(apArg[0]);` |
|     - | 1551 | `#ifdef __WINNT__` |
|     - | 1552 | `	{` |
|     - | 1553 | `		/* php's own mapping (win32/nice.c): cmd.exe has no nice value, so the` |
|     - | 1554 | `		 * POSIX increment is bucketed into the five priority CLASSES Windows` |
|     - | 1555 | `		 * has. REALTIME is deliberately not reachable there, and neither is it` |
|     - | 1556 | `		 * here. */` |
|     1 | 1557 | `		DWORD dwFlag = NORMAL_PRIORITY_CLASS;` |
|     1 | 1558 | `		if( iPri < -9 ){` |
|   ! 0 | 1559 | `			dwFlag = HIGH_PRIORITY_CLASS;` |
|     1 | 1560 | `		}else if( iPri < -4 ){` |
|   ! 0 | 1561 | `			dwFlag = ABOVE_NORMAL_PRIORITY_CLASS;` |
|     1 | 1562 | `		}else if( iPri > 9 ){` |
|   ! 0 | 1563 | `			dwFlag = IDLE_PRIORITY_CLASS;` |
|     1 | 1564 | `		}else if( iPri > 4 ){` |
|   ! 0 | 1565 | `			dwFlag = BELOW_NORMAL_PRIORITY_CLASS;` |
|     - | 1566 | `		}` |
|     1 | 1567 | `		SetPriorityClass(GetCurrentProcess(),dwFlag);` |
|     - | 1568 | `		/* php answers TRUE whatever that returned: its nice() reports failure` |
|     - | 1569 | `		 * through its return value and leaves errno alone, and proc_nice() reads` |
|     - | 1570 | `		 * only errno. */` |
|     1 | 1571 | `		ph7_result_bool(pCtx,1);` |
|     - | 1572 | `	}` |
|     - | 1573 | `#elif defined(__UNIXES__)` |
|     - | 1574 | `	{` |
|     - | 1575 | `		int iIgnored;` |
|     - | 1576 | `		/* nice() legitimately answers -1 (it returns the NEW nice value), so` |
|     - | 1577 | `		 * errno is the only failure evidence — php clears it first for the same` |
|     - | 1578 | `		 * reason. */` |
|     6 | 1579 | `		errno = 0;` |
|     6 | 1580 | `		iIgnored = nice((int)iPri);` |
|     3 | 1581 | `		(void)iIgnored;` |
|     6 | 1582 | `		if( errno != 0 ){` |
|     3 | 1583 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|     - | 1584 | `				"%s(): Only a super user may attempt to increase the priority of a process",` |
|     1 | 1585 | `				ph7_function_name(pCtx));` |
|     2 | 1586 | `			ph7_result_bool(pCtx,0);` |
|     1 | 1587 | `		}else{` |
|     4 | 1588 | `			ph7_result_bool(pCtx,1);` |
|     - | 1589 | `		}` |
|     - | 1590 | `	}` |
|     - | 1591 | `#else` |
|     - | 1592 | `	ph7_result_bool(pCtx,0);` |
|     - | 1593 | `#endif` |
|     7 | 1594 | `	return PH7_OK;` |
|     1 | 1595 | `}` |
|  5224 | 1596 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1597 | `{` |
|     - | 1598 | `	const char *zCommand, *zMode;` |
|     - | 1599 | `	char zPosix[8];` |
|     - | 1600 | `	pipe_private *pPipe;` |
|     - | 1601 | `	io_private *pDev;` |
|  5229 | 1602 | `	int nCmdLen, nModeLen, nPosix, i, bDropped = 0;` |
|  2612 | 1603 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|     - | 1604 | `	/* Extract the command and mode */` |
|  5229 | 1605 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|  5229 | 1606 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|     - | 1607 | `	/*` |
|     - | 1608 | `	 * php's mode rule, and the only one it has: ONE 'b' — C's binary flag, which` |
|     - | 1609 | `	 * popen(3) itself refuses — is dropped from the mode on POSIX, and what is` |
|     - | 1610 | `	 * left must be exactly "r", "w", "rb" or "wb". PHL used to read mode[0] and` |
|     - | 1611 | `	 * hand the REST to popen(3) unexamined, which was wrong in both directions:` |
|     - | 1612 | ``	 * `popen($cmd, 'rb')`, the ordinary binary spelling, answered FALSE because`` |
|     - | 1613 | ``	 * glibc rejected the 'b', and `popen($cmd, 'rr')` opened a pipe php refuses.`` |
|     - | 1614 | `	 */` |
|  5229 | 1615 | `	nPosix = 0;` |
|     - | 1616 | `#ifdef __WINNT__` |
|     - | 1617 | `	SXUNUSED(bDropped);   /* cmd.exe keeps the 'b': _popen understands it */` |
|     - | 1618 | `#endif` |
| 10467 | 1619 | `	for( i = 0 ; i < nModeLen && nPosix < (int)sizeof(zPosix) - 1 ; ++i ){` |
|     - | 1620 | `#ifndef __WINNT__` |
|  5238 | 1621 | `		if( zMode[i] == 'b' && !bDropped ){` |
|     8 | 1622 | `			bDropped = 1;   /* php drops the FIRST one and only that one */` |
|     8 | 1623 | `			continue;` |
|     - | 1624 | `		}` |
|     - | 1625 | `#endif` |
|  5235 | 1626 | `		zPosix[nPosix++] = zMode[i];` |
|  2620 | 1627 | `	}` |
|  5229 | 1628 | `	zPosix[nPosix] = 0;` |
|  5224 | 1629 | `	if( nPosix > 2` |
|  5224 | 1630 | `	 \|\| (nPosix == 1 && zPosix[0] != 'r' && zPosix[0] != 'w')` |
|  2626 | 1631 | `	 \|\| (nPosix == 2 && SyMemcmp(zPosix,"rb",sizeof("rb")-1) != 0` |
|     7 | 1632 | `	                 && SyMemcmp(zPosix,"wb",sizeof("wb")-1) != 0) ){` |
|     9 | 1633 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1634 | `			"popen(): Argument #2 ($mode) must be one of \"r\", \"rb\", \"w\", or \"wb\"");` |
|     - | 1635 | `	}` |
|     - | 1636 | `	/* Open the pipe. An EMPTY mode passes php's check above and fails HERE, in` |
|     - | 1637 | `	 * popen(3) — php reports it as an open failure and so does this, rather than` |
|     - | 1638 | `	 * letting the platform layer read mode[0] out of an empty string. */` |
|  5221 | 1639 | `	pPipe = nPosix > 0 ? PipeOpen(pCtx->pVm, zCommand, zPosix) : 0;` |
|  5221 | 1640 | `	if( pPipe == 0 ){` |
|     - | 1641 | ``		/* php names both arguments in this one: `popen(cmd,mode): message`. PHL`` |
|     - | 1642 | `		 * answered FALSE in silence, so a script had nothing to report. */` |
|     5 | 1643 | `		if( nPosix < 1 ){` |
|     3 | 1644 | `			errno = EINVAL;` |
|     1 | 1645 | `		}` |
|     7 | 1646 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|     4 | 1647 | `			ph7_function_name(pCtx),zCommand,zPosix,VfsStrerror(errno));` |
|     5 | 1648 | `		ph7_result_bool(pCtx, 0);` |
|     5 | 1649 | `		return PH7_OK;` |
|     - | 1650 | `	}` |
|     - | 1651 | `	/* Allocate an io_private instance to wrap the pipe */` |
|  5217 | 1652 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|  5217 | 1653 | `	if( pDev == 0 ){` |
|   ! 0 | 1654 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|   ! 0 | 1655 | `		PipeClose(pPipe);` |
|   ! 0 | 1656 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 1657 | `		return PH7_OK;` |
|     - | 1658 | `	}` |
|     - | 1659 | `	/* Initialize the io_private structure */` |
|  5217 | 1660 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|     - | 1661 | `	/* A pipe has no wrapper and no path, so php's meta reports the MODE and` |
|     - | 1662 | ``	 * neither `wrapper_type` nor `uri`: an empty URI is what leaves them out. */`` |
|  5217 | 1663 | `	SetIOPrivateOpenedAs(pDev,0,0,zPosix,nPosix);` |
|  5217 | 1664 | `	pDev->pHandle = pPipe;` |
|     - | 1665 | `	/* Return the io_private instance as a resource */` |
|  5217 | 1666 | `	ph7_result_resource(pCtx, pDev);` |
|  5217 | 1667 | `	return PH7_OK;` |
|  2617 | 1668 | `}` |
|     - | 1669 | `/*` |
|     - | 1670 | ` * int pclose(resource $handle)` |
|     - | 1671 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|     - | 1672 | ` * Parameters` |
|     - | 1673 | ` *  $handle` |
|     - | 1674 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|     - | 1675 | ` * Return` |
|     - | 1676 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|     - | 1677 | ` */` |
|  5052 | 1678 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1679 | `{` |
|     - | 1680 | `	const ph7_io_stream *pStream;` |
|     - | 1681 | `	pipe_private *pPipe;` |
|     - | 1682 | `	io_private *pDev;` |
|     - | 1683 | `	int status;` |
|  5057 | 1684 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|     - | 1685 | `		/* Missing/Invalid arguments, return -1 */` |
|   ! 0 | 1686 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|   ! 0 | 1687 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1688 | `		return PH7_OK;` |
|     - | 1689 | `	}` |
|     - | 1690 | `	/* Extract our private data */` |
|  5057 | 1691 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     - | 1692 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  5057 | 1693 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|   ! 0 | 1694 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|   ! 0 | 1695 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1696 | `		return PH7_OK;` |
|     - | 1697 | `	}` |
|     - | 1698 | `	/* Point to the target IO stream device */` |
|  5057 | 1699 | `	pStream = pDev->pStream;` |
|  5057 | 1700 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|   ! 0 | 1701 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|   ! 0 | 1702 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1703 | `		return PH7_OK;` |
|     - | 1704 | `	}` |
|     - | 1705 | `	/* Get the pipe handle */` |
|  5057 | 1706 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|     - | 1707 | `	/* A write chain gets its closing call while the pipe is still open. */` |
|  5057 | 1708 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     - | 1709 | `	/* Close the pipe and get exit status */` |
|  5057 | 1710 | `	status = PipeClose(pPipe);` |
|     - | 1711 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
|  5057 | 1712 | `	MarkIOPrivateClosed(pDev);` |
|     - | 1713 | `	/* Return the exit status */` |
|  5057 | 1714 | `	ph7_result_int(pCtx, status);` |
|  5057 | 1715 | `	return PH7_OK;` |
|  2531 | 1716 | `}` |
|     - | 1717 | `/*` |
|     - | 1718 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|     - | 1719 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|     - | 1720 | ` *   standard descriptors (php's process-control family). The returned` |
|     - | 1721 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|     - | 1722 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|     - | 1723 | ` *   bounds and report it as a live, non-stream resource.` |
|     - | 1724 | ` */` |
|     - | 1725 | `#ifdef __UNIXES__` |
|     - | 1726 | `#define PROC_MAX_DESC 16` |
|     - | 1727 | `typedef struct proc_private proc_private;` |
|     - | 1728 | `struct proc_private` |
|     - | 1729 | `{` |
|     - | 1730 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|     - | 1731 | `	int pid;           /* child process id */` |
|     - | 1732 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|     - | 1733 | `	int exit_code;     /* cached exit status once reaped */` |
|     - | 1734 | `};` |
|     - | 1735 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|    38 | 1736 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd,int bParentReads)` |
|     - | 1737 | `{` |
|    38 | 1738 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    38 | 1739 | `	if( pDev == 0 ){` |
|   ! 0 | 1740 | `		return 0;` |
|     - | 1741 | `	}` |
|    38 | 1742 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|     - | 1743 | `	/* Same shape as popen()'s end: a proc_open() pipe carries no path, and its` |
|     - | 1744 | `	 * mode is the direction the PARENT holds — the opposite of the child's. */` |
|    38 | 1745 | `	SetIOPrivateOpenedAs(pDev,0,0,bParentReads ? "r" : "w",1);` |
|    38 | 1746 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|    38 | 1747 | `	return pDev;` |
|    19 | 1748 | `}` |
|     - | 1749 | `/* One parsed descriptor-spec entry. */` |
|     - | 1750 | `struct proc_desc` |
|     - | 1751 | `{` |
|     - | 1752 | `	int child_fd;      /* the array key: which fd the child sees */` |
|     - | 1753 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|     - | 1754 | `	/* pipe */` |
|     - | 1755 | `	int child_end;     /* fd the child must have at child_fd */` |
|     - | 1756 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|     - | 1757 | `	int parent_reads;  /* the parent's end is the READ end (the child writes) */` |
|     - | 1758 | `	/* file */` |
|     - | 1759 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|     - | 1760 | `	/* redirect */` |
|     - | 1761 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|     - | 1762 | `};` |
|    16 | 1763 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 1764 | `{` |
|     - | 1765 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|    16 | 1766 | `	int nDesc = 0;` |
|     - | 1767 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|     - | 1768 | `	ph7_hashmap *pSpecMap;` |
|     - | 1769 | `	ph7_hashmap_node *pNode;` |
|    16 | 1770 | `	ph7_vm *pVm = pCtx->pVm;` |
|    16 | 1771 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|    16 | 1772 | `	int nArgv = 0;` |
|    16 | 1773 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|    16 | 1774 | `	const char *zCwd = 0;` |
|    16 | 1775 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|    16 | 1776 | `	int nEnv = 0;` |
|     - | 1777 | `	proc_private *pProc;` |
|     - | 1778 | `	pid_t pid;` |
|     - | 1779 | `	int i, rc;` |
|    16 | 1780 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1781 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|   ! 0 | 1782 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1783 | `		return PH7_OK;` |
|     - | 1784 | `	}` |
|     - | 1785 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|    16 | 1786 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    14 | 1787 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    14 | 1788 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|    14 | 1789 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|    14 | 1790 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    14 | 1791 | `		pNode = pCmdMap->pFirst;` |
|    36 | 1792 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|    22 | 1793 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|     - | 1794 | `			int nLen; const char *zs;` |
|    22 | 1795 | `			PH7_MemObjInit(pVm,pv);` |
|    22 | 1796 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|    22 | 1797 | `			zs = ph7_value_to_string(pv,&nLen);` |
|    22 | 1798 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|    22 | 1799 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|    22 | 1800 | `			PH7_MemObjRelease(pv);` |
|    22 | 1801 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|    22 | 1802 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|    11 | 1803 | `		}` |
|    14 | 1804 | `		azArgv[nArgv] = 0;` |
|     7 | 1805 | `	}else{` |
|     - | 1806 | `		int nLen;` |
|     2 | 1807 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|     - | 1808 | `	}` |
|     - | 1809 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|    16 | 1810 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|   ! 0 | 1811 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|   ! 0 | 1812 | `		if( nLen < 1 ){ zCwd = 0; }` |
|   ! 0 | 1813 | `	}` |
|     8 | 1814 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|   ! 0 | 1815 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|   ! 0 | 1816 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|   ! 0 | 1817 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|   ! 0 | 1818 | `		if( azEnv ){` |
|   ! 0 | 1819 | `			pNode = pEnvMap->pFirst;` |
|   ! 0 | 1820 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|     - | 1821 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|   ! 0 | 1822 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|   ! 0 | 1823 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|   ! 0 | 1824 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|   ! 0 | 1825 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|   ! 0 | 1826 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|   ! 0 | 1827 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));` |
|   ! 0 | 1828 | `				if( zPair ){` |
|   ! 0 | 1829 | `					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|   ! 0 | 1830 | `					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|   ! 0 | 1831 | `					azEnv[nEnv++] = zPair;` |
|   ! 0 | 1832 | `				}` |
|   ! 0 | 1833 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1834 | `				pNode = pNode->pPrev;` |
|   ! 0 | 1835 | `			}` |
|   ! 0 | 1836 | `			azEnv[nEnv] = 0;` |
|   ! 0 | 1837 | `		}` |
|   ! 0 | 1838 | `	}` |
|     - | 1839 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|    16 | 1840 | `	pSpec = apArg[1];` |
|    16 | 1841 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|    16 | 1842 | `	pNode = pSpecMap->pFirst;` |
|    58 | 1843 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|    42 | 1844 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|    42 | 1845 | `		PH7_MemObjInit(pVm,&sKey);` |
|    42 | 1846 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|    42 | 1847 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|    42 | 1848 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1; pD->parent_reads = 0;` |
|    42 | 1849 | `		PH7_MemObjRelease(&sKey);` |
|    42 | 1850 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|    42 | 1851 | `		PH7_MemObjInit(pVm,pEntry);` |
|    42 | 1852 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|    42 | 1853 | `		if( ph7_value_is_array(pEntry) ){` |
|     - | 1854 | `			int nLen; const char *zType;` |
|     - | 1855 | `			/* pEntry aliases the descriptor array the SCRIPT still holds, so every` |
|     - | 1856 | `			 * member is read through a scratch copy — ph7_value_to_string() would` |
|     - | 1857 | `			 * convert the entry in place and rewrite the script's own $descriptors` |
|     - | 1858 | ``			 * (`[0 => ['pipe', 114]]` came back as `'114'`). */`` |
|     - | 1859 | `			ph7_value sPeek;` |
|    42 | 1860 | `			PH7_MemObjInit(pVm,&sPeek);` |
|    42 | 1861 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|    42 | 1862 | `			zType = pType ? ph7_value_to_string(PH7_ValuePeek(pType,&sPeek),&nLen) : "";` |
|    42 | 1863 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|     - | 1864 | `				int fds[2];` |
|    38 | 1865 | `				if( pipe(fds) == 0 ){` |
|    38 | 1866 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|     - | 1867 | `					{` |
|     - | 1868 | `						ph7_value sMode;` |
|     - | 1869 | `						int nMode; const char *zMode;` |
|    38 | 1870 | `						PH7_MemObjInit(pVm,&sMode);` |
|    38 | 1871 | `						zMode = pParam ? ph7_value_to_string(PH7_ValuePeek(pParam,&sMode),&nMode) : "r";` |
|    38 | 1872 | `						pD->kind = 0;` |
|    38 | 1873 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|     - | 1874 | `							/* child writes -> parent reads: child gets write end */` |
|    26 | 1875 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|    26 | 1876 | `						pD->parent_reads = 1;` |
|    13 | 1877 | `						}else{` |
|     - | 1878 | `							/* child reads -> parent writes: child gets read end */` |
|    12 | 1879 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|    12 | 1880 | `						pD->parent_reads = 0;` |
|     - | 1881 | `						}` |
|    38 | 1882 | `						nDesc++;` |
|    38 | 1883 | `						PH7_MemObjRelease(&sMode);` |
|     - | 1884 | `					}` |
|    19 | 1885 | `				}` |
|    23 | 1886 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|     - | 1887 | `				ph7_value sPath, sMode;` |
|     2 | 1888 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|     2 | 1889 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|     2 | 1890 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|     2 | 1891 | `				PH7_MemObjInit(pVm,&sPath);` |
|     2 | 1892 | `				PH7_MemObjInit(pVm,&sMode);` |
|     2 | 1893 | `				zPath = pPath ? ph7_value_to_string(PH7_ValuePeek(pPath,&sPath),&nLen2) : "";` |
|     2 | 1894 | `				zMode = pMode ? ph7_value_to_string(PH7_ValuePeek(pMode,&sMode),&nLen3) : "r";` |
|     2 | 1895 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|   ! 0 | 1896 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|     2 | 1897 | `				pD->kind = 1;` |
|     2 | 1898 | `				pD->file_fd = open(zPath,oflag,0644);` |
|     2 | 1899 | `				nDesc++;` |
|     2 | 1900 | `				PH7_MemObjRelease(&sPath);` |
|     2 | 1901 | `				PH7_MemObjRelease(&sMode);` |
|     3 | 1902 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|     2 | 1903 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|     2 | 1904 | `				pD->kind = 2;` |
|     2 | 1905 | `				pD->redirect_to = pParam ? (int)PH7_ValuePeekInt64(pParam) : 1;` |
|     2 | 1906 | `				nDesc++;` |
|     1 | 1907 | `			}` |
|    42 | 1908 | `			PH7_MemObjRelease(&sPeek);` |
|    21 | 1909 | `		}` |
|    42 | 1910 | `		PH7_MemObjRelease(pEntry);` |
|    42 | 1911 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|    42 | 1912 | `		pNode = pNode->pPrev;` |
|    21 | 1913 | `	}` |
|     - | 1914 | `	/* --- Fork the child --- */` |
|    16 | 1915 | `	pid = fork();` |
|    24 | 1916 | `	if( pid < 0 ){` |
|   ! 0 | 1917 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"fork() failed");` |
|   ! 0 | 1918 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1919 | `		return PH7_OK;` |
|     - | 1920 | `	}` |
|    32 | 1921 | `	if( pid == 0 ){` |
|     - | 1922 | `		/* Child: wire up descriptors then exec */` |
|    58 | 1923 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|    42 | 1924 | `			struct proc_desc *pD = &aDesc[i];` |
|    42 | 1925 | `			if( pD->kind == 0 ){` |
|    38 | 1926 | `				dup2(pD->child_end,pD->child_fd);` |
|    38 | 1927 | `				close(pD->parent_end);` |
|    38 | 1928 | `				close(pD->child_end);` |
|    23 | 1929 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|     2 | 1930 | `				dup2(pD->file_fd,pD->child_fd);` |
|     2 | 1931 | `				close(pD->file_fd);` |
|     1 | 1932 | `			}` |
|    21 | 1933 | `		}` |
|     - | 1934 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|    58 | 1935 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|    42 | 1936 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|    21 | 1937 | `		}` |
|    16 | 1938 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|    16 | 1939 | `		if( azEnv ){` |
|   ! 0 | 1940 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|   ! 0 | 1941 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|   ! 0 | 1942 | `		}else{` |
|    16 | 1943 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|     2 | 1944 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|     - | 1945 | `		}` |
|     8 | 1946 | `		_exit(127); /* exec failed */` |
|     - | 1947 | `	}` |
|     - | 1948 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|    16 | 1949 | `	pPipes = ph7_context_new_array(pCtx);` |
|    58 | 1950 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|    42 | 1951 | `		struct proc_desc *pD = &aDesc[i];` |
|    42 | 1952 | `		if( pD->kind == 0 ){` |
|     - | 1953 | `			io_private *pEnd;` |
|     - | 1954 | `			ph7_value *pRes;` |
|    38 | 1955 | `			close(pD->child_end);` |
|    38 | 1956 | `			pEnd = ProcWrapFd(pVm,pD->parent_end,pD->parent_reads);` |
|    38 | 1957 | `			pRes = ph7_context_new_scalar(pCtx);` |
|    38 | 1958 | `			if( pEnd && pRes && pPipes ){` |
|    38 | 1959 | `				ph7_value_resource(pRes,pEnd);` |
|    38 | 1960 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|    19 | 1961 | `			}` |
|    38 | 1962 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|    23 | 1963 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|     2 | 1964 | `			close(pD->file_fd);` |
|     1 | 1965 | `		}` |
|    21 | 1966 | `	}` |
|    16 | 1967 | `	if( pPipes ){` |
|    16 | 1968 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|     8 | 1969 | `	}` |
|     - | 1970 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|    17 | 1971 | `	if( azArgv ){` |
|    36 | 1972 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|    14 | 1973 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|     7 | 1974 | `	}` |
|    22 | 1975 | `	if( azEnv ){` |
|   ! 0 | 1976 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|   ! 0 | 1977 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|   ! 0 | 1978 | `	}` |
|     - | 1979 | `	/* Build the process resource */` |
|    16 | 1980 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|    16 | 1981 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    16 | 1982 | `	SyZero(pProc,sizeof(proc_private));` |
|    16 | 1983 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|    16 | 1984 | `	pProc->pid = (int)pid;` |
|    16 | 1985 | `	pProc->running = 1;` |
|    16 | 1986 | `	pProc->exit_code = 0;` |
|    16 | 1987 | `	ph7_result_resource(pCtx,pProc);` |
|     8 | 1988 | `	(void)rc;` |
|    16 | 1989 | `	return PH7_OK;` |
|     8 | 1990 | `}` |
|     - | 1991 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|    16 | 1992 | `static void ProcReap(proc_private *pProc,int block)` |
|     - | 1993 | `{` |
|    16 | 1994 | `	int status = 0;` |
|     - | 1995 | `	pid_t r;` |
|    16 | 1996 | `	if( !pProc->running ){ return; }` |
|    16 | 1997 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|    16 | 1998 | `	if( r == (pid_t)pProc->pid ){` |
|    16 | 1999 | `		pProc->running = 0;` |
|    16 | 2000 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|   ! 0 | 2001 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|     8 | 2002 | `	}` |
|     8 | 2003 | `}` |
|    16 | 2004 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2005 | `{` |
|     - | 2006 | `	proc_private *pProc;` |
|    16 | 2007 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 2008 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 2009 | `		return PH7_OK;` |
|     - | 2010 | `	}` |
|    16 | 2011 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    16 | 2012 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|   ! 0 | 2013 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 2014 | `		return PH7_OK;` |
|     - | 2015 | `	}` |
|    16 | 2016 | `	ProcReap(pProc,1/*block until it exits*/);` |
|    16 | 2017 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|    16 | 2018 | `	return PH7_OK;` |
|     8 | 2019 | `}` |
|   ! 0 | 2020 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2021 | `{` |
|     - | 2022 | `	proc_private *pProc;` |
|   ! 0 | 2023 | `	int sig = 15; /* SIGTERM */` |
|   ! 0 | 2024 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 2025 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2026 | `		return PH7_OK;` |
|     - | 2027 | `	}` |
|   ! 0 | 2028 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|   ! 0 | 2029 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|   ! 0 | 2030 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2031 | `		return PH7_OK;` |
|     - | 2032 | `	}` |
|   ! 0 | 2033 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|   ! 0 | 2034 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|   ! 0 | 2035 | `	ph7_result_bool(pCtx,1);` |
|   ! 0 | 2036 | `	return PH7_OK;` |
|   ! 0 | 2037 | `}` |
|   ! 0 | 2038 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2039 | `{` |
|     - | 2040 | `	proc_private *pProc;` |
|     - | 2041 | `	ph7_value *pArray, *pVal;` |
|   ! 0 | 2042 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 2043 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2044 | `		return PH7_OK;` |
|     - | 2045 | `	}` |
|   ! 0 | 2046 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|   ! 0 | 2047 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|   ! 0 | 2048 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2049 | `		return PH7_OK;` |
|     - | 2050 | `	}` |
|   ! 0 | 2051 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|   ! 0 | 2052 | `	pArray = ph7_context_new_array(pCtx);` |
|   ! 0 | 2053 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 2054 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   ! 0 | 2055 | `	ph7_value_int(pVal,pProc->pid);` |
|   ! 0 | 2056 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|   ! 0 | 2057 | `	ph7_value_bool(pVal,pProc->running);` |
|   ! 0 | 2058 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|   ! 0 | 2059 | `	ph7_value_bool(pVal,0);` |
|   ! 0 | 2060 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|   ! 0 | 2061 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|   ! 0 | 2062 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|   ! 0 | 2063 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|   ! 0 | 2064 | `	ph7_value_int(pVal,0);` |
|   ! 0 | 2065 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|   ! 0 | 2066 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|   ! 0 | 2067 | `	ph7_context_release_value(pCtx,pVal);` |
|   ! 0 | 2068 | `	ph7_result_value(pCtx,pArray);` |
|   ! 0 | 2069 | `	return PH7_OK;` |
|   ! 0 | 2070 | `}` |
|     - | 2071 | `#else /* !__UNIXES__ */` |
|     - | 2072 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2073 | `{` |
|     - | 2074 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2075 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|   ! 0 | 2076 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 2077 | `	return PH7_OK;` |
|   ! 0 | 2078 | `}` |
|     - | 2079 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2080 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|     - | 2081 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2082 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     - | 2083 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2084 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     - | 2085 | `#endif /* __UNIXES__ */` |
|     - | 2086 | `/* Export the php:// stream */` |
|     - | 2087 | `PH7_PRIVATE const ph7_io_stream sPHP_Stream = {` |
|     - | 2088 | `	"php",` |
|     - | 2089 | `	PH7_IO_STREAM_VERSION,` |
|     - | 2090 | `	PHPStreamData_Open,  /* xOpen */` |
|     - | 2091 | `	0,   /* xOpenDir */` |
|     - | 2092 | `	PHPStreamData_Close, /* xClose */` |
|     - | 2093 | `	0,  /* xCloseDir */` |
|     - | 2094 | `	PHPStreamData_Read,  /* xRead */` |
|     - | 2095 | `	0,  /* xReadDir */` |
|     - | 2096 | `	PHPStreamData_Write, /* xWrite */` |
|     - | 2097 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|     - | 2098 | `	0,  /* xLock */` |
|     - | 2099 | `	0,  /* xRewindDir */` |
|     - | 2100 | `	PHPStreamData_Tell,  /* xTell */` |
|     - | 2101 | `	PHPStreamData_Trunc, /* xTrunc */` |
|     - | 2102 | `	0,  /* xSync */` |
|     - | 2103 | `	0   /* xStat */` |
|     - | 2104 | `};` |
|     - | 2105 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 2106 | `/*` |
|     - | 2107 | ` * Return TRUE if we are dealing with the php:// stream.` |
|     - | 2108 | ` * FALSE otherwise.` |
|     - | 2109 | ` */` |
|     - | 2110 | `/*` |
|     - | 2111 | ` * The handle a php://filter proxy WRAPS, or 0 for any other php:// stream. The` |
|     - | 2112 | ` * proxy is not a stream of its own — the position, the descriptor, the stat and` |
|     - | 2113 | ` * the lock all belong to the stream underneath — so everything that asks the` |
|     - | 2114 | ` * device such a question has to go through here first.` |
|     - | 2115 | ` */` |
|    52 | 2116 | `PH7_PRIVATE io_private * PH7_PhpStreamInner(void *pHandle)` |
|     1 | 2117 | `{` |
|     - | 2118 | `#ifndef PH7_DISABLE_DISK_IO` |
|    53 | 2119 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    53 | 2120 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|    15 | 2121 | `		return pData->pInner;` |
|     - | 2122 | `	}` |
|     - | 2123 | `#else` |
|     - | 2124 | `	SXUNUSED(pHandle);` |
|     - | 2125 | `#endif` |
|    39 | 2126 | `	return 0;` |
|    27 | 2127 | `}` |
|  3110 | 2128 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream)` |
|     5 | 2129 | `{` |
|     - | 2130 | `#ifndef PH7_DISABLE_DISK_IO` |
|  3115 | 2131 | `	return pStream == &sPHP_Stream;` |
|     - | 2132 | `#else` |
|     - | 2133 | `	SXUNUSED(pStream); /* cc warning */` |
|     - | 2134 | `	return 0;` |
|     - | 2135 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2136 | `}` |
|     - | 2137 | `/*` |
|     - | 2138 | ` * Is this a handle php's plain-files device would own -- a file, a pipe, a` |
|     - | 2139 | ` * standard stream? php sets the blocking mode on those with O_NONBLOCK, which` |
|     - | 2140 | ` * Windows does not have, so there stream_set_blocking() answers FALSE for them.` |
|     - | 2141 | ` */` |
|   ! 0 | 2142 | `PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev)` |
|     1 | 2143 | `{` |
|     - | 2144 | `#ifndef PH7_DISABLE_DISK_IO` |
|     1 | 2145 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| pDev->pStream == 0 \|\| pDev->bDir ){` |
|   ! 0 | 2146 | `		return 0;` |
|     - | 2147 | `	}` |
|     - | 2148 | `#ifdef __WINNT__` |
|     1 | 2149 | `	if( pDev->pStream == &sWinFileStream ){` |
|     1 | 2150 | `		return 1;` |
|     - | 2151 | `	}` |
|     - | 2152 | `#elif defined(__UNIXES__)` |
|   ! 0 | 2153 | `	if( pDev->pStream == &sUnixFileStream ){` |
|   ! 0 | 2154 | `		return 1;` |
|     - | 2155 | `	}` |
|     - | 2156 | `#endif` |
|     1 | 2157 | `	if( pDev->pStream == &sPipe_Stream ){` |
|   ! 0 | 2158 | `		return 1;` |
|     - | 2159 | `	}` |
|     1 | 2160 | `	if( is_php_stream(pDev->pStream) ){` |
|     1 | 2161 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     1 | 2162 | `		return pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|   ! 0 | 2163 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR;` |
|     - | 2164 | `	}` |
|     1 | 2165 | `	return 0;` |
|     - | 2166 | `#else` |
|     - | 2167 | `	SXUNUSED(pDev); /* cc warning */` |
|     - | 2168 | `	return 0;` |
|     - | 2169 | `#endif` |
|     1 | 2170 | `}` |
|     - | 2171 | `/*` |
|     - | 2172 | ` * The POSIX descriptor behind an open handle, or -1 when there is none.` |
|     - | 2173 | ` * php applies blocking mode and timeouts AT the descriptor, so a stream that` |
|     - | 2174 | ` * has no fd — a memory buffer, a data:// payload, a userland wrapper, and` |
|     - | 2175 | ` * every file on Windows, where the device carries a HANDLE — is exactly the` |
|     - | 2176 | ` * set php answers "unsupported" for.` |
|     - | 2177 | ` */` |
|   110 | 2178 | `PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev)` |
|     2 | 2179 | `{` |
|     - | 2180 | `#if !defined(__WINNT__) && !defined(PH7_DISABLE_DISK_IO)` |
|     - | 2181 | `	/* A php://filter handle has no descriptor of its own; the stream it wraps` |
|     - | 2182 | `	 * does, and that is the one blocking mode and locking apply to. */` |
|   110 | 2183 | `	pDev = PH7_StreamUnwrap(pDev);` |
|   110 | 2184 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| pDev->pStream == 0 \|\| pDev->bDir ){` |
|     - | 2185 | `		/* A DIRECTORY handle rides the same ops table as a file and stores a` |
|     - | 2186 | `		 * DIR* where a file stores its descriptor, so reading one as the other` |
|     - | 2187 | `		 * hands fcntl()/lseek() a truncated heap pointer — an arbitrary fd` |
|     - | 2188 | `		 * number belonging to something else in this process. */` |
|   ! 0 | 2189 | `		return -1;` |
|     - | 2190 | `	}` |
|   110 | 2191 | `	if( pDev->pStream == &sUnixFileStream ){` |
|    60 | 2192 | `		return SX_PTR_TO_INT(pDev->pHandle);` |
|     - | 2193 | `	}` |
|    50 | 2194 | `	if( pDev->pStream == &sPipe_Stream ){` |
|   ! 0 | 2195 | `		pipe_private *pPipe = (pipe_private *)pDev->pHandle;` |
|   ! 0 | 2196 | `		return pPipe->pFile ? fileno(pPipe->pFile) : -1;` |
|     - | 2197 | `	}` |
|    50 | 2198 | `	if( is_php_stream(pDev->pStream) ){` |
|    30 | 2199 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|    30 | 2200 | `		if( pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|    29 | 2201 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR ){` |
|     4 | 2202 | `			return SX_PTR_TO_INT(pData->x.pHandle);` |
|     - | 2203 | `		}` |
|    13 | 2204 | `	}` |
|    46 | 2205 | `	return -1;` |
|     - | 2206 | `#else` |
|     - | 2207 | `	SXUNUSED(pDev); /* cc warning */` |
|     2 | 2208 | `	return -1;` |
|     - | 2209 | `#endif` |
|    57 | 2210 | `}` |
|     - | 2211 | `/*` |
|     - | 2212 | `` * Can this handle report a POSITION? php's `seekable` is a fact about what the`` |
|     - | 2213 | ` * handle sits on, not about what the device could do — php://stdout is seekable` |
|     - | 2214 | ` * into a file and not down a pipe — and the descriptor is the only thing that` |
|     - | 2215 | ` * knows. Answers 1 (yes), 0 (no) or -1 (nothing here can tell; the caller falls` |
|     - | 2216 | ` * back on the device's own xTell).` |
|     - | 2217 | ` */` |
|    58 | 2218 | `PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev)` |
|     3 | 2219 | `{` |
|     - | 2220 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2221 | `#ifdef __WINNT__` |
|     - | 2222 | `	/* The file devices carry a HANDLE rather than a descriptor here, and only` |
|     - | 2223 | `	 * the php:// standard streams hold one this can ask. */` |
|     3 | 2224 | `	if( pDev && pDev->pHandle && is_php_stream(pDev->pStream) ){` |
|     3 | 2225 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     - | 2226 | `		if( pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|     3 | 2227 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR ){` |
|     - | 2228 | `			LARGE_INTEGER zero,pos;` |
|     1 | 2229 | `			zero.QuadPart = 0;` |
|     1 | 2230 | `			return SetFilePointerEx((HANDLE)pData->x.pHandle,zero,&pos,FILE_CURRENT) ? 1 : 0;` |
|     - | 2231 | `		}` |
|     - | 2232 | `	}` |
|     2 | 2233 | `	return -1;` |
|     - | 2234 | `#else` |
|    58 | 2235 | `	int fd = PH7_StreamPosixFd(pDev);` |
|    58 | 2236 | `	if( fd < 0 ){` |
|    28 | 2237 | `		return -1;` |
|     - | 2238 | `	}` |
|    30 | 2239 | `	return lseek(fd,0,SEEK_CUR) == (off_t)-1 ? 0 : 1;` |
|     - | 2240 | `#endif` |
|     - | 2241 | `#else` |
|     - | 2242 | `	SXUNUSED(pDev); /* cc warning */` |
|     - | 2243 | `	return -1;` |
|     - | 2244 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    32 | 2245 | `}` |
|     - | 2246 | `/*` |
|     - | 2247 | ` * Which php:// sub-stream a handle opened. stream_get_meta_data() has to tell` |
|     - | 2248 | ` * MEMORY, TEMP and STDIO apart and only the device's own private state knows;` |
|     - | 2249 | ` * everything else answers 0.` |
|     - | 2250 | ` */` |
|   864 | 2251 | `PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle)` |
|     5 | 2252 | `{` |
|     - | 2253 | `#ifndef PH7_DISABLE_DISK_IO` |
|   869 | 2254 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   869 | 2255 | `	return pData ? pData->iType : 0;` |
|     - | 2256 | `#else` |
|     - | 2257 | `	SXUNUSED(pHandle); /* cc warning */` |
|     - | 2258 | `	return 0;` |
|     - | 2259 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2260 | `}` |
|     - | 2261 | `/*` |
|     - | 2262 | ` * Return TRUE if we are dealing with the data:// stream.` |
|     - | 2263 | ` */` |
|   660 | 2264 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream)` |
|     5 | 2265 | `{` |
|     - | 2266 | `#ifndef PH7_DISABLE_DISK_IO` |
|   665 | 2267 | `	return pStream == &sDATA_Stream;` |
|     - | 2268 | `#else` |
|     - | 2269 | `	SXUNUSED(pStream); /* cc warning */` |
|     - | 2270 | `	return 0;` |
|     - | 2271 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2272 | `}` |
|     - | 2273 | `/*` |
|     - | 2274 | ` * bool stream_isatty(resource $stream)` |
|     - | 2275 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|     - | 2276 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|     - | 2277 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|     - | 2278 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|     - | 2279 | ` */` |
|     6 | 2280 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2281 | `{` |
|     7 | 2282 | `	int bTty = 0;` |
|     7 | 2283 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 2284 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2285 | `		return PH7_OK;` |
|     - | 2286 | `	}` |
|     - | 2287 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2288 | `	{` |
|     7 | 2289 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     7 | 2290 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|     5 | 2291 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     6 | 2292 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|     4 | 2293 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|     3 | 2294 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|     - | 2295 | `#ifdef __WINNT__` |
|     1 | 2296 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|     - | 2297 | `#else` |
|     4 | 2298 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|     - | 2299 | `#endif` |
|     2 | 2300 | `			}` |
|     2 | 2301 | `		}` |
|     - | 2302 | `	}` |
|     - | 2303 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     7 | 2304 | `	ph7_result_bool(pCtx,bTty);` |
|     7 | 2305 | `	return PH7_OK;` |
|     4 | 2306 | `}` |
|     - | 2307 |  |
|     - | 2308 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2309 | `/*` |
|     - | 2310 | ` * Export the STDIN handle.` |
|     - | 2311 | ` */` |
|   144 | 2312 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|     4 | 2313 | `{` |
|     - | 2314 | `#ifndef PH7_DISABLE_DISK_IO` |
|   148 | 2315 | `	if( pVm->pStdin == 0  ){` |
|     - | 2316 | `		io_private *pIn;` |
|     - | 2317 | `		/* Allocate an IO private instance */` |
|    12 | 2318 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    12 | 2319 | `		if( pIn == 0 ){` |
|   ! 0 | 2320 | `			return 0;` |
|     - | 2321 | `		}` |
|    12 | 2322 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|    12 | 2323 | `		SetIOPrivateOpenedAs(pIn,"php://stdin",(int)sizeof("php://stdin")-1,"rb",2);` |
|     - | 2324 | `		/* Initialize the handle */` |
|    12 | 2325 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|     - | 2326 | `		/* Install the STDIN stream */` |
|    12 | 2327 | `		pVm->pStdin = pIn;` |
|    12 | 2328 | `		return pIn;` |
|   ! 0 | 2329 | `	}else{` |
|     - | 2330 | `		/* NULL or STDIN */` |
|   140 | 2331 | `		return pVm->pStdin;` |
|     - | 2332 | `	}` |
|     - | 2333 | `#else` |
|     - | 2334 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2335 | `	return 0;` |
|     - | 2336 | `#endif` |
|    76 | 2337 | `}` |
|     - | 2338 | `/*` |
|     - | 2339 | ` * Export the STDOUT handle.` |
|     - | 2340 | ` */` |
|   134 | 2341 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|     3 | 2342 | `{` |
|     - | 2343 | `#ifndef PH7_DISABLE_DISK_IO` |
|   137 | 2344 | `	if( pVm->pStdout == 0  ){` |
|     - | 2345 | `		io_private *pOut;` |
|     - | 2346 | `		/* Allocate an IO private instance */` |
|    15 | 2347 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    15 | 2348 | `		if( pOut == 0 ){` |
|   ! 0 | 2349 | `			return 0;` |
|     - | 2350 | `		}` |
|    15 | 2351 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|    15 | 2352 | `		SetIOPrivateOpenedAs(pOut,"php://stdout",(int)sizeof("php://stdout")-1,"wb",2);` |
|     - | 2353 | `		/* Initialize the handle */` |
|    15 | 2354 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|     - | 2355 | `		/* Install the STDOUT stream */` |
|    15 | 2356 | `		pVm->pStdout = pOut;` |
|    15 | 2357 | `		return pOut;` |
|   ! 0 | 2358 | `	}else{` |
|     - | 2359 | `		/* NULL or STDOUT */` |
|   125 | 2360 | `		return pVm->pStdout;` |
|     - | 2361 | `	}` |
|     - | 2362 | `#else` |
|     - | 2363 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2364 | `	return 0;` |
|     - | 2365 | `#endif` |
|    70 | 2366 | `}` |
|     - | 2367 | `/*` |
|     - | 2368 | ` * Export the STDERR handle.` |
|     - | 2369 | ` */` |
|   136 | 2370 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|     3 | 2371 | `{` |
|     - | 2372 | `#ifndef PH7_DISABLE_DISK_IO` |
|   139 | 2373 | `	if( pVm->pStderr == 0  ){` |
|     - | 2374 | `		io_private *pErr;` |
|     - | 2375 | `		/* Allocate an IO private instance */` |
|    17 | 2376 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    17 | 2377 | `		if( pErr == 0 ){` |
|   ! 0 | 2378 | `			return 0;` |
|     - | 2379 | `		}` |
|    17 | 2380 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|    17 | 2381 | `		SetIOPrivateOpenedAs(pErr,"php://stderr",(int)sizeof("php://stderr")-1,"wb",2);` |
|     - | 2382 | `		/* Initialize the handle */` |
|    17 | 2383 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|     - | 2384 | `		/* Install the STDERR stream */` |
|    17 | 2385 | `		pVm->pStderr = pErr;` |
|    17 | 2386 | `		return pErr;` |
|   ! 0 | 2387 | `	}else{` |
|     - | 2388 | `		/* NULL or STDERR */` |
|   125 | 2389 | `		return pVm->pStderr;` |
|     - | 2390 | `	}` |
|     - | 2391 | `#else` |
|     - | 2392 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2393 | `	return 0;` |
|     - | 2394 | `#endif` |
|    71 | 2395 | `}` |
|     - | 2396 |  |
