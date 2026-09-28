# src/ph7/vfs_io_driver.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1035/1290 lines (80.23%)

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
|     - |   67 | `	int bTemp;       /* MEMORY type: opened as php://temp rather than php://memory.` |
|     - |   68 | `	                  * php's temp stream is a WRAPPER whose read copies the inner` |
|     - |   69 | `	                  * memory stream's eof flag, so it reports the end one read` |
|     - |   70 | `	                  * EARLIER than a bare php://memory does -- the two devices` |
|     - |   71 | `	                  * are one here, and this is the difference between them. */` |
|     - |   72 | `	/* FILTER type: php://filter/…/resource=… is not a stream of its own — it is` |
|     - |   73 | ``	 * the stream named by `resource=` with a chain wrapped around it. The chain`` |
|     - |   74 | `	 * lives on this io_private, so every read and write below goes through` |
|     - |   75 | `	 * PH7_StreamRead/PH7_StreamWrite and is filtered on the way. */` |
|     - |   76 | `	io_private *pInner;` |
|     - |   77 | `};` |
|     - |   78 | `/*` |
|     - |   79 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|     - |   80 | ` */` |
|   530 |   81 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|     5 |   82 | `{` |
|     - |   83 | `	ph7_stream_data *pData;` |
|   535 |   84 | `	if( pVm == 0 ){` |
|   ! 0 |   85 | `		return 0;` |
|     - |   86 | `	}` |
|     - |   87 | `	/* Allocate a new instance */` |
|   535 |   88 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|   535 |   89 | `	if( pData == 0 ){` |
|   ! 0 |   90 | `		return 0;` |
|     - |   91 | `	}` |
|     - |   92 | `	/* Zero the structure */` |
|   535 |   93 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|     - |   94 | `	/* Initialize fields */` |
|   535 |   95 | `	pData->iType = iType;` |
|   535 |   96 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|   535 |   97 | `	pData->nCur = 0;` |
|   535 |   98 | `	pData->bReadOnly = 0;` |
|   535 |   99 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|     - |  100 | `		/* Nothing else to set up: the buffer is the stream */` |
|   312 |  101 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|     - |  102 | `		/* Point to the default VM consumer routine. */` |
|     5 |  103 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|     3 |  104 | `	}else{` |
|     - |  105 | `#ifdef __WINNT__` |
|     - |  106 | `		DWORD nChannel;` |
|     5 |  107 | `		switch(iType){` |
|     4 |  108 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|     4 |  109 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|     - |  110 | `		default:` |
|     5 |  111 | `			nChannel = STD_INPUT_HANDLE;` |
|     - |  112 | `			break;` |
|     - |  113 | `		}` |
|     5 |  114 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|     - |  115 | `#else` |
|     - |  116 | `		/* Assume an UNIX system */` |
|    80 |  117 | `		int ifd = STDIN_FILENO;` |
|    80 |  118 | `		switch(iType){` |
|    12 |  119 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|    14 |  120 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|    27 |  121 | `		default:` |
|    54 |  122 | `			break;` |
|     - |  123 | `		}` |
|    80 |  124 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|     - |  125 | `#endif` |
|     - |  126 | `	}` |
|   535 |  127 | `	pData->pVm = pVm;` |
|   535 |  128 | `	return pData;` |
|   270 |  129 | `}` |
|     - |  130 | `/*` |
|     - |  131 | ` * Implementation of the php:// IO streams routines` |
|     - |  132 | ` * Status:` |
|     - |  133 | ` *   Stable.` |
|     - |  134 | ` */` |
|     - |  135 | `/*` |
|     - |  136 | ` * php://filter/<spec>/resource=<uri>. The resource is opened through the` |
|     - |  137 | ` * ordinary device dispatch and the spec's filters are attached to it; what this` |
|     - |  138 | ` * device hands back is a PROXY whose reads and writes go through that handle,` |
|     - |  139 | ` * which is what makes the chain apply to file_get_contents(), include and every` |
|     - |  140 | ` * other opener without one of them knowing about filters at all.` |
|     - |  141 | ` */` |
|     - |  142 | `static void PHPStreamData_Close(void *pHandle);` |
|    46 |  143 | `static int PHPStreamFilterOpen(const char *zSpec,int nSpec,int iMode,ph7_vm *pVm,` |
|     - |  144 | `	ph7_stream_data **ppData)` |
|     3 |  145 | `{` |
|     - |  146 | `	const ph7_io_stream *pInnerStream;` |
|    49 |  147 | `	const char *zRes = 0;` |
|     - |  148 | `	ph7_stream_data *pData;` |
|     - |  149 | `	io_private *pInner;` |
|     - |  150 | `	SyBlob sRes;` |
|    49 |  151 | `	int nRes = 0,i,iChains = 0;` |
|     - |  152 | ``	/* php looks for `/resource=` and cuts the filter list there. When the path`` |
|     - |  153 | ``	 * BEGINS with `resource=` it takes the resource and leaves the list alone —`` |
|     - |  154 | `	 * so the resource's own path segments are then tried as filter names, which` |
|     - |  155 | `	 * is exactly what php warns about. */` |
|   949 |  156 | `	for( i = 0 ; i + 10 <= nSpec ; i++ ){` |
|   947 |  157 | `		if( zSpec[i] == '/' && SyMemcmp(&zSpec[i+1],"resource=",9) == 0 ){` |
|    46 |  158 | `			zRes = &zSpec[i+10];` |
|    46 |  159 | `			nRes = nSpec - (i + 10);` |
|    46 |  160 | `			nSpec = i;` |
|    46 |  161 | `			break;` |
|     - |  162 | `		}` |
|   453 |  163 | `	}` |
|    69 |  164 | `	if( zRes == 0 ){` |
|     3 |  165 | `		if( nSpec >= 9 && SyMemcmp(zSpec,"resource=",9) == 0 ){` |
|     3 |  166 | `			zRes = &zSpec[9];` |
|     3 |  167 | `			nRes = nSpec - 9;` |
|     2 |  168 | `		}else{` |
|   ! 0 |  169 | `			PH7_VmThrowError(pVm,pVm->pCalleeName,PH7_CTX_WARNING,"No URL resource specified");` |
|   ! 0 |  170 | `			return -1;` |
|     - |  171 | `		}` |
|     1 |  172 | `	}` |
|    46 |  173 | `	if( iMode & (PH7_IO_OPEN_RDONLY\|PH7_IO_OPEN_RDWR) ){` |
|    43 |  174 | `		iChains \|= PHL_STREAM_FILTER_READ;` |
|    20 |  175 | `	}` |
|    63 |  176 | `	if( iMode & (PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_RDWR\|PH7_IO_OPEN_APPEND) ){` |
|     7 |  177 | `		iChains \|= PHL_STREAM_FILTER_WRITE;` |
|     3 |  178 | `	}` |
|    49 |  179 | `	pData = PHPStreamDataInit(pVm,PH7_IO_STREAM_FILTER);` |
|    49 |  180 | `	if( pData == 0 ){` |
|   ! 0 |  181 | `		return -1;` |
|     - |  182 | `	}` |
|    49 |  183 | `	pInner = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    49 |  184 | `	if( pInner ){` |
|    49 |  185 | `		SyZero(pInner,sizeof(io_private));` |
|    23 |  186 | `	}` |
|    49 |  187 | `	if( pInner == 0 ){` |
|   ! 0 |  188 | `		PHPStreamData_Close((void *)pData);` |
|   ! 0 |  189 | `		return -1;` |
|     - |  190 | `	}` |
|     - |  191 | `	/* The dispatch wants a NUL-terminated URI and mutates the pointer it is` |
|     - |  192 | `	 * handed, so the resource is copied out of the path first. */` |
|    49 |  193 | `	SyBlobInit(&sRes,&pVm->sAllocator);` |
|    49 |  194 | `	SyBlobAppend(&sRes,zRes,(sxu32)nRes);` |
|    49 |  195 | `	SyBlobNullAppend(&sRes);` |
|     - |  196 | `	{` |
|    49 |  197 | `		const char *zPath = (const char *)SyBlobData(&sRes);` |
|    49 |  198 | `		pInnerStream = PH7_VmGetStreamDevice(pVm,&zPath,nRes);` |
|    49 |  199 | `		InitIOPrivate(pVm,pInnerStream,pInner);` |
|    49 |  200 | `		pInner->pHandle = pInnerStream` |
|    46 |  201 | `			? PH7_StreamOpenHandle(pVm,pInnerStream,zPath,iMode,FALSE,0,FALSE,0,0)` |
|    23 |  202 | `			: 0;` |
|    49 |  203 | `		if( pInner->pHandle == 0 ){` |
|     3 |  204 | `			SyBlobRelease(&sRes);` |
|     3 |  205 | `			SyMemBackendFree(&pVm->sAllocator,pInner);` |
|     3 |  206 | `			PHPStreamData_Close((void *)pData);` |
|     3 |  207 | `			return -1;` |
|     - |  208 | `		}` |
|    46 |  209 | `		SetIOPrivateOpenedAs(pInner,zRes,nRes,"r",1);` |
|     - |  210 | `	}` |
|    46 |  211 | `	SyBlobRelease(&sRes);` |
|    46 |  212 | `	pData->pInner = pInner;` |
|    46 |  213 | `	PH7_StreamFilterParseUrl(pVm,zSpec,nSpec,pInner,iChains);` |
|    46 |  214 | `	*ppData = pData;` |
|    46 |  215 | `	return PH7_OK;` |
|    26 |  216 | `}` |
|     - |  217 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|   468 |  218 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     5 |  219 | `{` |
|     - |  220 | `	ph7_stream_data *pData;` |
|     - |  221 | `	SyString sStream;` |
|   473 |  222 | `	int bTemp = 0;` |
|   468 |  223 | `	if( SyStrnicmp(zName,"filter",sizeof("filter")-1) == 0` |
|   262 |  224 | `	 && (zName[6] == '/' \|\| zName[6] == 0) ){` |
|     - |  225 | `		int rc;` |
|    49 |  226 | `		if( zName[6] == 0 ){` |
|     - |  227 | `			/* php://filter with nothing behind it is not a stream at all. */` |
|   ! 0 |  228 | `			if( pResource && pResource->pVm ){` |
|   ! 0 |  229 | `				PH7_VmThrowError(pResource->pVm,pResource->pVm->pCalleeName,` |
|     - |  230 | `					PH7_CTX_WARNING,"Invalid php:// URL specified");` |
|   ! 0 |  231 | `			}` |
|   ! 0 |  232 | `			return -1;` |
|     - |  233 | `		}` |
|    49 |  234 | `		if( pResource == 0 \|\| pResource->pVm == 0 ){` |
|   ! 0 |  235 | `			return -1;` |
|     - |  236 | `		}` |
|    49 |  237 | `		rc = PHPStreamFilterOpen(&zName[7],(int)SyStrlen(&zName[7]),iMode,pResource->pVm,&pData);` |
|    49 |  238 | `		if( rc != PH7_OK ){` |
|     3 |  239 | `			return -1;` |
|     - |  240 | `		}` |
|    46 |  241 | `		*ppHandle = (void *)pData;` |
|    46 |  242 | `		return PH7_OK;` |
|     - |  243 | `	}` |
|   427 |  244 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|     - |  245 | `	/* Trim leading and trailing white spaces */` |
|   427 |  246 | `	SyStringFullTrim(&sStream);` |
|     - |  247 | `	/* Stream to open */` |
|   427 |  248 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|   ! 0 |  249 | `		iMode = PH7_IO_STREAM_STDIN;` |
|   427 |  250 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|     5 |  251 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|   425 |  252 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|   ! 0 |  253 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|   423 |  254 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|   ! 0 |  255 | `		iMode = PH7_IO_STREAM_STDERR;` |
|   418 |  256 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|   229 |  257 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|     - |  258 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|     - |  259 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|   419 |  260 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|   419 |  261 | `		bTemp = SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0;` |
|   212 |  262 | `	}else{` |
|     - |  263 | `		/* An unknown php:// name is php's own diagnostic, raised BESIDE the` |
|     - |  264 | `		 * caller's "Failed to open stream" rather than instead of it — the same` |
|     - |  265 | ``		 * sentence the empty `php://filter` above already raised. */`` |
|     5 |  266 | `		if( pResource && pResource->pVm ){` |
|     5 |  267 | `			PH7_VmThrowError(pResource->pVm,pResource->pVm->pCalleeName,` |
|     - |  268 | `				PH7_CTX_WARNING,"Invalid php:// URL specified");` |
|     2 |  269 | `		}` |
|     5 |  270 | `		return -1;` |
|     - |  271 | `	}` |
|     - |  272 | `	/* Create our handle */` |
|   423 |  273 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|   423 |  274 | `	if( pData == 0 ){` |
|   ! 0 |  275 | `		return -1;` |
|     - |  276 | `	}` |
|   423 |  277 | `	pData->bTemp = bTemp;` |
|     - |  278 | `	/* Make the handle public */` |
|   423 |  279 | `	*ppHandle = (void *)pData;` |
|   423 |  280 | `	return PH7_OK;` |
|   239 |  281 | `}` |
|     - |  282 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|  1704 |  283 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|     5 |  284 | `{` |
|  1709 |  285 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  1709 |  286 | `	if( pData == 0 ){` |
|   ! 0 |  287 | `		return -1;` |
|     - |  288 | `	}` |
|  1709 |  289 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     - |  290 | `		/* Through the shared reader, which is where the chain runs. */` |
|    72 |  291 | `		return PH7_StreamRead(pData->pInner,pBuffer,nDatatoRead);` |
|     - |  292 | `	}` |
|  1638 |  293 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|  1634 |  294 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|     - |  295 | `		sxu32 nRead;` |
|  1634 |  296 | `		if( pData->nCur >= nAvail ){` |
|   268 |  297 | `			return 0; /* EOF */` |
|     - |  298 | `		}` |
|  1370 |  299 | `		nRead = nAvail - pData->nCur;` |
|  1370 |  300 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|  1074 |  301 | `			nRead = (sxu32)nDatatoRead;` |
|   536 |  302 | `		}` |
|  1370 |  303 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|  1370 |  304 | `		pData->nCur += nRead;` |
|  1370 |  305 | `		return (ph7_int64)nRead;` |
|     - |  306 | `	}` |
|     5 |  307 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|     - |  308 | `		/* Forbidden */` |
|   ! 0 |  309 | `		return -1;` |
|     - |  310 | `	}` |
|     - |  311 | `#ifdef __WINNT__` |
|     - |  312 | `	{` |
|     - |  313 | `		DWORD nRd;` |
|     - |  314 | `		BOOL rc;` |
|     1 |  315 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|     1 |  316 | `		if( !rc ){` |
|     - |  317 | `			/* IO error */` |
|   ! 0 |  318 | `			return -1;` |
|     - |  319 | `		}` |
|     1 |  320 | `		return (ph7_int64)nRd;` |
|     - |  321 | `	}` |
|     - |  322 | `#elif defined(__UNIXES__)` |
|     - |  323 | `	{` |
|     - |  324 | `		ssize_t nRd;` |
|     - |  325 | `		int fd;` |
|     4 |  326 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|     4 |  327 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|     4 |  328 | `		if( nRd < 0 ){` |
|   ! 0 |  329 | `			return -1;` |
|     - |  330 | `		}` |
|     - |  331 | `		/* ZERO is end of file, not an error — the contract every other device` |
|     - |  332 | `		 * here keeps. Collapsing the two meant nothing could ever latch EOF on` |
|     - |  333 | ``		 * php://stdin, so `while (!feof(STDIN))` never ended. */`` |
|     4 |  334 | `		return (ph7_int64)nRd;` |
|     - |  335 | `	}` |
|     - |  336 | `#else` |
|     - |  337 | `	return -1;` |
|     - |  338 | `#endif` |
|   857 |  339 | `}` |
|     - |  340 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|   332 |  341 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|     4 |  342 | `{` |
|   336 |  343 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   336 |  344 | `	if( pData == 0 ){` |
|   ! 0 |  345 | `		return -1;` |
|     - |  346 | `	}` |
|   336 |  347 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     7 |  348 | `		return PH7_StreamWrite(pData->pInner,pBuf,nWrite);` |
|     - |  349 | `	}` |
|   330 |  350 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|     - |  351 | `		/* Forbidden */` |
|   ! 0 |  352 | `		return -1;` |
|   330 |  353 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|     - |  354 | `		sxu32 nLen,nEnd;` |
|   318 |  355 | `		if( pData->bReadOnly ){` |
|   ! 0 |  356 | `			return -1;` |
|     - |  357 | `		}` |
|   318 |  358 | `		nLen = SyBlobLength(&pData->sMem);` |
|   318 |  359 | `		if( pData->nCur > nLen ){` |
|     - |  360 | `			/* seek past end: php zero-fills the gap */` |
|     - |  361 | `			static const char zZero[64] = {0};` |
|   ! 0 |  362 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|   ! 0 |  363 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|   ! 0 |  364 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|   ! 0 |  365 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|   ! 0 |  366 | `					return -1;` |
|     - |  367 | `				}` |
|   ! 0 |  368 | `			}` |
|   ! 0 |  369 | `			nLen = SyBlobLength(&pData->sMem);` |
|   ! 0 |  370 | `		}` |
|   318 |  371 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|   318 |  372 | `		if( pData->nCur < nLen ){` |
|     - |  373 | `			/* overwrite in place up to the current end */` |
|     8 |  374 | `			sxu32 nOver = nLen - pData->nCur;` |
|     8 |  375 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|    11 |  376 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|    11 |  377 | `			if( nEnd > nLen ){` |
|   ! 0 |  378 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|   ! 0 |  379 | `					return -1;` |
|     - |  380 | `				}` |
|   ! 0 |  381 | `			}` |
|     5 |  382 | `		}else{` |
|   312 |  383 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|   ! 0 |  384 | `				return -1;` |
|     - |  385 | `			}` |
|     - |  386 | `		}` |
|   318 |  387 | `		pData->nCur = nEnd;` |
|   318 |  388 | `		return nWrite;` |
|    13 |  389 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|     3 |  390 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|     - |  391 | `		int rc;` |
|     - |  392 | `		/* Call the vm output consumer */` |
|     3 |  393 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|     3 |  394 | `		if( rc == PH7_ABORT ){` |
|   ! 0 |  395 | `			return -1;` |
|     - |  396 | `		}` |
|     3 |  397 | `		return nWrite;` |
|     - |  398 | `	}` |
|     - |  399 | `#ifdef __WINNT__` |
|     - |  400 | `	{` |
|     - |  401 | `		DWORD nWr;` |
|     - |  402 | `		BOOL rc;` |
|   ! 0 |  403 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|   ! 0 |  404 | `		if( !rc ){` |
|     - |  405 | `			/* IO error */` |
|   ! 0 |  406 | `			return -1;` |
|     - |  407 | `		}` |
|   ! 0 |  408 | `		return (ph7_int64)nWr;` |
|     - |  409 | `	}` |
|     - |  410 | `#elif defined(__UNIXES__)` |
|     - |  411 | `	{` |
|     - |  412 | `		ssize_t nWr;` |
|     - |  413 | `		int fd;` |
|    10 |  414 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|    10 |  415 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|    10 |  416 | `		if( nWr < 1 ){` |
|   ! 0 |  417 | `			return -1;` |
|     - |  418 | `		}` |
|    10 |  419 | `		return (ph7_int64)nWr;` |
|     - |  420 | `	}` |
|     - |  421 | `#else` |
|     - |  422 | `	return -1;` |
|     - |  423 | `#endif` |
|   170 |  424 | `}` |
|     - |  425 | `/* void (*xClose)(void *) */` |
|   412 |  426 | `static void PHPStreamData_Close(void *pHandle)` |
|     5 |  427 | `{` |
|   417 |  428 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     - |  429 | `	ph7_vm *pVm;` |
|   417 |  430 | `	if( pData == 0 ){` |
|   ! 0 |  431 | `		return;` |
|     - |  432 | `	}` |
|   417 |  433 | `	pVm = pData->pVm;` |
|   417 |  434 | `	if( pData->iType == PH7_IO_STREAM_FILTER && pData->pInner ){` |
|     - |  435 | `		/* The write chain closes while the device below is still open. */` |
|    46 |  436 | `		PH7_StreamFilterReleaseChains(pData->pInner);` |
|    46 |  437 | `		if( pData->pInner->pStream ){` |
|    46 |  438 | `			PH7_StreamCloseHandle(pData->pInner->pStream,pData->pInner->pHandle);` |
|    22 |  439 | `		}` |
|    46 |  440 | `		SyBlobRelease(&pData->pInner->sBuffer);` |
|    46 |  441 | `		SyBlobRelease(&pData->pInner->sFilt);` |
|    46 |  442 | `		SyBlobRelease(&pData->pInner->sUri);` |
|    46 |  443 | `		SyMemBackendFree(&pVm->sAllocator,pData->pInner);` |
|    46 |  444 | `		pData->pInner = 0;` |
|    22 |  445 | `	}` |
|   417 |  446 | `	SyBlobRelease(&pData->sMem);` |
|     - |  447 | `	/* Free the instance */` |
|   417 |  448 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|   211 |  449 | `}` |
|     - |  450 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|   356 |  451 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|     4 |  452 | `{` |
|   360 |  453 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     - |  454 | `	ph7_int64 iNew;` |
|   360 |  455 | `	if( pData == 0 ){` |
|   ! 0 |  456 | `		return -1;` |
|     - |  457 | `	}` |
|   360 |  458 | `	if( pData->iType == PH7_IO_STREAM_FILTER ){` |
|     9 |  459 | `		return PH7_StreamSeekWrapped(pData->pInner,iOfft,whence);` |
|     - |  460 | `	}` |
|   352 |  461 | `	if( pData->iType != PH7_IO_STREAM_MEMORY ){` |
|   ! 0 |  462 | `		return -1;` |
|     - |  463 | `	}` |
|   352 |  464 | `	switch(whence){` |
|    13 |  465 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|     7 |  466 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|   336 |  467 | `	default:            iNew = iOfft; break;` |
|     - |  468 | `	}` |
|   352 |  469 | `	if( iNew < 0 ){` |
|   ! 0 |  470 | `		return -1;` |
|     - |  471 | `	}` |
|   352 |  472 | `	pData->nCur = (sxu32)iNew;` |
|   352 |  473 | `	return PH7_OK;` |
|   182 |  474 | `}` |
|     - |  475 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|   300 |  476 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|     4 |  477 | `{` |
|   304 |  478 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   304 |  479 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|     - |  480 | `		/* Where the SCRIPT is on the wrapped stream: the device sits past` |
|     - |  481 | `		 * whatever the chain has already produced and nobody has taken. */` |
|    15 |  482 | `		return PH7_StreamLogicalTell(pData->pInner);` |
|     - |  483 | `	}` |
|   290 |  484 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|     3 |  485 | `		return -1;` |
|     - |  486 | `	}` |
|   288 |  487 | `	return (ph7_int64)pData->nCur;` |
|   154 |  488 | `}` |
|     - |  489 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|     2 |  490 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|     1 |  491 | `{` |
|     3 |  492 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|     3 |  493 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|   ! 0 |  494 | `		io_private *pIn = pData->pInner;` |
|   ! 0 |  495 | `		if( pIn == 0 \|\| pIn->pStream == 0 \|\| pIn->pStream->xTrunc == 0 ){` |
|   ! 0 |  496 | `			return -1;` |
|     - |  497 | `		}` |
|   ! 0 |  498 | `		return pIn->pStream->xTrunc(pIn->pHandle,nLen);` |
|     - |  499 | `	}` |
|     3 |  500 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|   ! 0 |  501 | `		return -1;` |
|     - |  502 | `	}` |
|     3 |  503 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|     - |  504 | `		/* shrink in place: the blob keeps its allocation */` |
|     3 |  505 | `		pData->sMem.nByte = (sxu32)nLen;` |
|     2 |  506 | `	}else{` |
|     - |  507 | `		static const char zZero[64] = {0};` |
|   ! 0 |  508 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|   ! 0 |  509 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|   ! 0 |  510 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|   ! 0 |  511 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|   ! 0 |  512 | `				return -1;` |
|     - |  513 | `			}` |
|   ! 0 |  514 | `		}` |
|     - |  515 | `	}` |
|     3 |  516 | `	return PH7_OK;` |
|     2 |  517 | `}` |
|     - |  518 | `/*` |
|     - |  519 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|     - |  520 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|     - |  521 | ` * base64). Shares the MEMORY machinery above.` |
|     - |  522 | ` */` |
|    12 |  523 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     1 |  524 | `{` |
|    13 |  525 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|     1 |  526 | `}` |
|    38 |  527 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|     3 |  528 | `{` |
|     - |  529 | `	ph7_stream_data *pData;` |
|    41 |  530 | `	const char *zIn = zName;` |
|    41 |  531 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|    41 |  532 | `	const char *zComma = 0;` |
|    41 |  533 | `	int bBase64 = 0;` |
|    19 |  534 | `	SXUNUSED(iMode);` |
|     - |  535 | `	/* Find the comma separating the mediatype from the payload */` |
|   581 |  536 | `	while( zIn < zEnd ){` |
|   575 |  537 | `		if( zIn[0] == ',' ){` |
|    35 |  538 | `			zComma = zIn;` |
|    35 |  539 | `			break;` |
|     - |  540 | `		}` |
|   543 |  541 | `		zIn++;` |
|     3 |  542 | `	}` |
|    41 |  543 | `	if( zComma == 0 ){` |
|     - |  544 | `		/* php's own wording for the one thing its data wrapper checks. */` |
|     7 |  545 | `		if( pResource && pResource->pVm ){` |
|     7 |  546 | `			PH7_StreamSetOpenError(pResource->pVm,"rfc2397: no comma in URL");` |
|     3 |  547 | `		}` |
|     7 |  548 | `		return -1;` |
|     - |  549 | `	}` |
|    32 |  550 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|    33 |  551 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|     7 |  552 | `		bBase64 = 1;` |
|     3 |  553 | `	}` |
|    35 |  554 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|    35 |  555 | `	if( pData == 0 ){` |
|   ! 0 |  556 | `		return -1;` |
|     - |  557 | `	}` |
|    35 |  558 | `	pData->bReadOnly = 1;` |
|    35 |  559 | `	zIn = &zComma[1];` |
|    35 |  560 | `	if( bBase64 ){` |
|     7 |  561 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|   ! 0 |  562 | `			SyBlobRelease(&pData->sMem);` |
|   ! 0 |  563 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|   ! 0 |  564 | `			return -1;` |
|     - |  565 | `		}` |
|     4 |  566 | `	}else{` |
|     - |  567 | `		/* percent-decode the payload */` |
|   163 |  568 | `		while( zIn < zEnd ){` |
|   137 |  569 | `			char c = zIn[0];` |
|   137 |  570 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|     3 |  571 | `				int hi = SyHexToint(zIn[1]);` |
|     3 |  572 | `				int lo = SyHexToint(zIn[2]);` |
|     3 |  573 | `				c = (char)((hi << 4) \| lo);` |
|     3 |  574 | `				zIn += 3;` |
|     2 |  575 | `			}else{` |
|   135 |  576 | `				zIn++;` |
|     - |  577 | `			}` |
|   137 |  578 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|   ! 0 |  579 | `				SyBlobRelease(&pData->sMem);` |
|   ! 0 |  580 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|   ! 0 |  581 | `				return -1;` |
|     - |  582 | `			}` |
|     3 |  583 | `		}` |
|     - |  584 | `	}` |
|    35 |  585 | `	*ppHandle = (void *)pData;` |
|    35 |  586 | `	return PH7_OK;` |
|    22 |  587 | `}` |
|     - |  588 | `/* data:// rejects writes outright */` |
|   ! 0 |  589 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|   ! 0 |  590 | `{` |
|   ! 0 |  591 | `	SXUNUSED(pHandle);` |
|   ! 0 |  592 | `	SXUNUSED(pBuf);` |
|   ! 0 |  593 | `	SXUNUSED(nWrite);` |
|   ! 0 |  594 | `	return -1;` |
|   ! 0 |  595 | `}` |
|     - |  596 | `PH7_PRIVATE const ph7_io_stream sDATA_Stream = {` |
|     - |  597 | `	"data",` |
|     - |  598 | `	PH7_IO_STREAM_VERSION,` |
|     - |  599 | `	DataStreamData_Open,  /* xOpen */` |
|     - |  600 | `	0,   /* xOpenDir */` |
|     - |  601 | `	PHPStreamData_Close, /* xClose */` |
|     - |  602 | `	0,  /* xCloseDir */` |
|     - |  603 | `	PHPStreamData_Read,  /* xRead */` |
|     - |  604 | `	0,  /* xReadDir */` |
|     - |  605 | `	DataStreamData_Write, /* xWrite */` |
|     - |  606 | `	PHPStreamData_Seek,  /* xSeek */` |
|     - |  607 | `	0,  /* xLock */` |
|     - |  608 | `	0,  /* xRewindDir */` |
|     - |  609 | `	PHPStreamData_Tell,  /* xTell */` |
|     - |  610 | `	0,  /* xTrunc */` |
|     - |  611 | `	0,  /* xSync */` |
|     - |  612 | `	0   /* xStat */` |
|     - |  613 | `};` |
|     - |  614 | `/*` |
|     - |  615 | ` * Pipe stream implementation for popen/pclose.` |
|     - |  616 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|     - |  617 | ` * PHP-compatible process I/O functionality.` |
|     - |  618 | ` */` |
|     - |  619 | `typedef struct pipe_private pipe_private;` |
|     - |  620 | `struct pipe_private` |
|     - |  621 | `{` |
|     - |  622 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|     - |  623 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|     - |  624 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|     - |  625 | `#ifdef __WINNT__` |
|     - |  626 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|     - |  627 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|     - |  628 | `#endif` |
|     - |  629 | `};` |
|     - |  630 |  |
|     - |  631 | `#ifdef __WINNT__` |
|     - |  632 | `#include <Windows.h>` |
|     - |  633 | `#include <stdio.h>` |
|     - |  634 | `#include <io.h>` |
|     - |  635 | `#include <fcntl.h>` |
|     - |  636 | `/*` |
|     - |  637 | ` * Custom Windows popen implementation using CreateProcess.` |
|     - |  638 | ` * This allows us to properly wait for process completion.` |
|     - |  639 | ` */` |
|     - |  640 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|     5 |  641 | `{` |
|     5 |  642 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|     5 |  643 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|     5 |  644 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|     - |  645 | `	SECURITY_ATTRIBUTES sa;` |
|     - |  646 | `	STARTUPINFOW si;` |
|     - |  647 | `	PROCESS_INFORMATION pi;` |
|     5 |  648 | `	WCHAR *zWideCmd = NULL;` |
|     5 |  649 | `	FILE *pFile = NULL;` |
|     - |  650 | `	int fd;` |
|     5 |  651 | `	BOOL bRead = (zMode[0] == 'r');` |
|     5 |  652 | `	BOOL bBinary = (strchr(zMode,'b') != NULL);` |
|     - |  653 |  |
|     - |  654 | `	/* Set up security attributes for pipe inheritance */` |
|     5 |  655 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|     5 |  656 | `	sa.bInheritHandle = TRUE;` |
|     5 |  657 | `	sa.lpSecurityDescriptor = NULL;` |
|     - |  658 |  |
|     - |  659 | `	/* Create pipes for child process I/O */` |
|     5 |  660 | `	if( bRead ){` |
|     - |  661 | `		/* Reading from child's stdout */` |
|     5 |  662 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|   ! 0 |  663 | `			return NULL;` |
|     - |  664 | `		}` |
|     - |  665 | `		/* Ensure read handle is not inherited */` |
|     5 |  666 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|     5 |  667 | `		hReadPipe = hChildStdoutRd;` |
|     5 |  668 | `		*phPipe = hChildStdoutRd;` |
|     5 |  669 | `	}else{` |
|     - |  670 | `		/* Writing to child's stdin */` |
|     2 |  671 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|   ! 0 |  672 | `			return NULL;` |
|     - |  673 | `		}` |
|     - |  674 | `		/* Ensure write handle is not inherited */` |
|     2 |  675 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|     2 |  676 | `		hWritePipe = hChildStdinWr;` |
|     2 |  677 | `		*phPipe = hChildStdinWr;` |
|     - |  678 | `	}` |
|     - |  679 |  |
|     - |  680 | `	/* Convert command to wide string */` |
|     - |  681 | `	{` |
|     5 |  682 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|     5 |  683 | `		if( nLen <= 0 ){` |
|   ! 0 |  684 | `			goto cleanup_pipes;` |
|     - |  685 | `		}` |
|     5 |  686 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|     5 |  687 | `		if( !zWideCmd ){` |
|   ! 0 |  688 | `			goto cleanup_pipes;` |
|     - |  689 | `		}` |
|     5 |  690 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|     - |  691 | `	}` |
|     - |  692 |  |
|     - |  693 | `	/* Set up process startup info */` |
|     5 |  694 | `	ZeroMemory(&si, sizeof(si));` |
|     5 |  695 | `	si.cb = sizeof(si);` |
|     5 |  696 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|     5 |  697 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|     5 |  698 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|     5 |  699 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|     5 |  700 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|     - |  701 |  |
|     5 |  702 | `	ZeroMemory(&pi, sizeof(pi));` |
|     - |  703 |  |
|     - |  704 | `	/* Create the child process */` |
|     5 |  705 | `	if( !CreateProcessW(` |
|     - |  706 | `		NULL,           /* Application name */` |
|     - |  707 | `		zWideCmd,       /* Command line */` |
|     - |  708 | `		NULL,           /* Process security attributes */` |
|     - |  709 | `		NULL,           /* Thread security attributes */` |
|     - |  710 | `		TRUE,           /* Inherit handles */` |
|     - |  711 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|     - |  712 | `		NULL,           /* Environment */` |
|     - |  713 | `		NULL,           /* Current directory */` |
|     - |  714 | `		&si,            /* Startup info */` |
|     - |  715 | `		&pi             /* Process info */` |
|     - |  716 | `	)){` |
|   ! 0 |  717 | `		goto cleanup_all;` |
|     - |  718 | `	}` |
|     - |  719 |  |
|     - |  720 | `	/* Close handles we don't need in parent */` |
|     5 |  721 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|     5 |  722 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|     - |  723 |  |
|     - |  724 | `	/* Close thread handle (we only need process handle) */` |
|     5 |  725 | `	CloseHandle(pi.hThread);` |
|     - |  726 |  |
|     - |  727 | `	/* Store process handle for later waiting */` |
|     5 |  728 | `	*phProcess = pi.hProcess;` |
|     - |  729 |  |
|     - |  730 | `	/* Convert OS handle to C file descriptor, then to FILE*. The TRANSLATION mode` |
|     - |  731 | `	 * is the caller's, not a constant: cmd.exe writes CRLF, and a descriptor` |
|     - |  732 | `	 * opened _O_TEXT eats every CR on the way in. php picks it per call —` |
|     - |  733 | `	 * "rb" for exec/system/passthru, so their output is byte-exact, "rt" for` |
|     - |  734 | `	 * shell_exec, and the script's own mode for popen() — and this used to` |
|     - |  735 | ``	 * hardcode _O_TEXT for all of them, so `passthru('type file.bin')` lost every`` |
|     - |  736 | `	 * 0x0D byte and system()'s output came back LF-only where php's is CRLF. */` |
|     5 |  737 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|     - |  738 | `	                     (bRead ? _O_RDONLY : _O_WRONLY)` |
|     - |  739 | `	                     \| (bBinary ? _O_BINARY : _O_TEXT));` |
|     5 |  740 | `	if( fd == -1 ){` |
|   ! 0 |  741 | `		CloseHandle(pi.hProcess);` |
|   ! 0 |  742 | `		*phProcess = NULL;` |
|   ! 0 |  743 | `		goto cleanup_all;` |
|     - |  744 | `	}` |
|     - |  745 |  |
|     - |  746 | `	/* The stream's own translation has to agree with the descriptor's */` |
|     5 |  747 | `	pFile = _fdopen(fd, bBinary ? (bRead ? "rb" : "wb") : (bRead ? "rt" : "wt"));` |
|     5 |  748 | `	if( !pFile ){` |
|   ! 0 |  749 | `		_close(fd); /* This will also close the underlying handle */` |
|   ! 0 |  750 | `		CloseHandle(pi.hProcess);` |
|   ! 0 |  751 | `		*phProcess = NULL;` |
|   ! 0 |  752 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|   ! 0 |  753 | `		return NULL;` |
|     - |  754 | `	}` |
|     - |  755 |  |
|     5 |  756 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|     5 |  757 | `	return pFile;` |
|     - |  758 |  |
|     - |  759 | `cleanup_all:` |
|   ! 0 |  760 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|     - |  761 | `cleanup_pipes:` |
|   ! 0 |  762 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|   ! 0 |  763 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|   ! 0 |  764 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|   ! 0 |  765 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|   ! 0 |  766 | `	return NULL;` |
|     5 |  767 | `}` |
|     - |  768 |  |
|     - |  769 | `/*` |
|     - |  770 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|     - |  771 | ` */` |
|     - |  772 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|     5 |  773 | `{` |
|     5 |  774 | `	DWORD dwExitCode = 0;` |
|     - |  775 | `	int status;` |
|     - |  776 |  |
|     - |  777 | `	/* Close the FILE* (this closes the pipe) */` |
|     5 |  778 | `	fclose(pFile);` |
|     - |  779 |  |
|     5 |  780 | `	if( hProcess ){` |
|     - |  781 | `		/* Wait for the process to complete */` |
|     5 |  782 | `		WaitForSingleObject(hProcess, INFINITE);` |
|     - |  783 |  |
|     5 |  784 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|     5 |  785 | `			status = (int)dwExitCode;` |
|     5 |  786 | `		}else{` |
|   ! 0 |  787 | `			status = -1;` |
|     - |  788 | `		}` |
|     - |  789 |  |
|     - |  790 | `		/* Close process handle */` |
|     5 |  791 | `		CloseHandle(hProcess);` |
|     5 |  792 | `	}else{` |
|   ! 0 |  793 | `		status = -1;` |
|     - |  794 | `	}` |
|     - |  795 |  |
|     5 |  796 | `	return status;` |
|     5 |  797 | `}` |
|     - |  798 | `#endif /* __WINNT__ */` |
|     - |  799 | `/*` |
|     - |  800 | ` * Open a pipe to a process.` |
|     - |  801 | ` * This is called internally by popen(), not through the stream device interface.` |
|     - |  802 | ` */` |
|  5922 |  803 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|     5 |  804 | `{` |
|     - |  805 | `	pipe_private *pPipe;` |
|     - |  806 | `	FILE *pFile;` |
|  5927 |  807 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|   ! 0 |  808 | `		return 0;` |
|     - |  809 | `	}` |
|     - |  810 | `	/* Validate mode - only 'r' or 'w' allowed */` |
|  5927 |  811 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|   ! 0 |  812 | `		return 0;` |
|     - |  813 | `	}` |
|     - |  814 | `	/* Open the pipe using system popen */` |
|     - |  815 | `#ifdef __WINNT__` |
|     - |  816 | `	{` |
|     - |  817 | `		/* Build cmd.exe command wrapper */` |
|     5 |  818 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|     5 |  819 | `		const char *zShellSuffix = "\"";` |
|     5 |  820 | `		size_t nPrefix = strlen(zShellPrefix);` |
|     5 |  821 | `		size_t nSuffix = strlen(zShellSuffix);` |
|     5 |  822 | `		size_t nCmd = strlen(zCommand);` |
|     5 |  823 | `		size_t nQuotes = 0;` |
|     5 |  824 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|     5 |  825 | `			if (zCommand[i] == '"') nQuotes++;` |
|     5 |  826 | `		}` |
|     5 |  827 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|     5 |  828 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|     5 |  829 | `		if (zCmdEsc == NULL) {` |
|   ! 0 |  830 | `			return 0;` |
|     - |  831 | `		}` |
|     - |  832 | `		/* Escape quotes in command */` |
|     5 |  833 | `		size_t j = 0;` |
|     5 |  834 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|     5 |  835 | `			char ch = zCommand[i];` |
|     5 |  836 | `			if (ch == '"') {` |
|     4 |  837 | `				zCmdEsc[j++] = '^';` |
|     4 |  838 | `				zCmdEsc[j++] = '"';` |
|     4 |  839 | `			} else {` |
|     5 |  840 | `				zCmdEsc[j++] = ch;` |
|     - |  841 | `			}` |
|     5 |  842 | `		}` |
|     5 |  843 | `		zCmdEsc[j] = '\0';` |
|     5 |  844 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|     5 |  845 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|     5 |  846 | `		if (zWinCmd == NULL) {` |
|   ! 0 |  847 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|   ! 0 |  848 | `			return 0;` |
|     - |  849 | `		}` |
|     5 |  850 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|     5 |  851 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|     5 |  852 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|     5 |  853 | `		zWinCmd[nTotal - 1] = '\0';` |
|     - |  854 | `		/* Allocate pipe structure early so we can store handles */` |
|     5 |  855 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|     5 |  856 | `		if( pPipe == 0 ){` |
|   ! 0 |  857 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|   ! 0 |  858 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|   ! 0 |  859 | `			return 0;` |
|     - |  860 | `		}` |
|     - |  861 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|     5 |  862 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|     5 |  863 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|     5 |  864 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|     5 |  865 | `		if( pFile == 0 ){` |
|   ! 0 |  866 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|   ! 0 |  867 | `			return 0;` |
|     - |  868 | `		}` |
|     - |  869 | `		/* Initialize remaining fields */` |
|     5 |  870 | `		pPipe->pFile = pFile;` |
|     5 |  871 | `		pPipe->pVm = pVm;` |
|     5 |  872 | `		pPipe->iMode = zMode[0];` |
|     - |  873 | `	}` |
|     - |  874 | `#elif defined(__UNIXES__) /* Unix */` |
|     - |  875 | `	/* The mode goes to popen(3) VERBATIM, exactly as php hands it its own: a mode` |
|     - |  876 | `	 * popen(3) refuses (anything but "r"/"w" — 'b' is a Windows translation flag` |
|     - |  877 | `	 * with nothing to translate here) is an open FAILURE, which is php's answer` |
|     - |  878 | `	 * for it too. */` |
|  5922 |  879 | `	pFile = popen(zCommand, zMode);` |
|  5922 |  880 | `	if( pFile == 0 ){` |
|     2 |  881 | `		return 0;` |
|     - |  882 | `	}` |
|     - |  883 | `	/* Allocate pipe private structure */` |
|  5920 |  884 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|  5920 |  885 | `	if( pPipe == 0 ){` |
|     - |  886 | `		/* Out of memory, close the pipe */` |
|   ! 0 |  887 | `		pclose(pFile);` |
|   ! 0 |  888 | `		return 0;` |
|     - |  889 | `	}` |
|     - |  890 | `	/* Initialize the structure */` |
|  5920 |  891 | `	pPipe->pFile = pFile;` |
|  5920 |  892 | `	pPipe->pVm = pVm;` |
|  5920 |  893 | `	pPipe->iMode = zMode[0];` |
|     - |  894 | `#else /* OS_OTHER: no process pipes on this platform */` |
|     - |  895 | `	(void)pFile;` |
|     - |  896 | `	return 0;` |
|     - |  897 | `#endif` |
|  5925 |  898 | `	return pPipe;` |
|  2966 |  899 | `}` |
|     - |  900 | `/*` |
|     - |  901 | ` * Close a pipe and return the exit status of the process.` |
|     - |  902 | ` * Returns the exit status, or -1 on error.` |
|     - |  903 | ` */` |
|  5888 |  904 | `static int PipeClose(pipe_private *pPipe)` |
|     5 |  905 | `{` |
|     - |  906 | `	int status;` |
|     - |  907 | `	ph7_vm *pVm;` |
|  5893 |  908 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 |  909 | `		return -1;` |
|     - |  910 | `	}` |
|  5893 |  911 | `	pVm = pPipe->pVm;` |
|     - |  912 | `	/* Close the pipe and get exit status */` |
|     - |  913 | `#ifdef __WINNT__` |
|     - |  914 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|     5 |  915 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|     - |  916 | `#elif defined(__UNIXES__)` |
|  5888 |  917 | `	status = pclose(pPipe->pFile);` |
|     - |  918 | `	/* pclose() answers waitpid()'s raw status. php (php_stream_pclose) translates` |
|     - |  919 | `	 * exactly ONE case of it — a normal exit becomes its exit CODE — and hands the` |
|     - |  920 | `	 * raw word back for every other, so a process killed by a signal reports the` |
|     - |  921 | ``	 * SIGNAL number: `kill -TERM $$` is 15, `kill -9 $$` is 9. PHL used to add the`` |
|     - |  922 | `	 * shell's own 128 to it (143, 137), a number the same status word can also` |
|     - |  923 | ``	 * mean as an ordinary `exit 143`, and answered -1 for a stopped child. This is`` |
|     - |  924 | `	 * pclose()'s answer and, through it, exec()/system()/passthru()'s` |
|     - |  925 | `	 * $result_code. */` |
|  5888 |  926 | `	if( status != -1 && WIFEXITED(status) ){` |
|  5884 |  927 | `		status = WEXITSTATUS(status);` |
|  2942 |  928 | `	}` |
|     - |  929 | `#else /* OS_OTHER: no process pipes on this platform */` |
|     - |  930 | `	status = -1;` |
|     - |  931 | `#endif` |
|     - |  932 | `	/* Free the structure */` |
|  5893 |  933 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|  5893 |  934 | `	return status;` |
|  2949 |  935 | `}` |
|     - |  936 | `/*` |
|     - |  937 | ` * Pipe stream xClose implementation.` |
|     - |  938 | ` * Note: This is called by fclose(), not pclose().` |
|     - |  939 | ` * It closes the pipe but does not return the exit status.` |
|     - |  940 | ` */` |
|   128 |  941 | `static void PipeStream_Close(void *pHandle)` |
|     4 |  942 | `{` |
|   132 |  943 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|   132 |  944 | `	if( pPipe ){` |
|   132 |  945 | `		PipeClose(pPipe);` |
|    64 |  946 | `	}` |
|   132 |  947 | `}` |
|     - |  948 | `/*` |
|     - |  949 | ` * Pipe stream xRead implementation.` |
|     - |  950 | ` */` |
|  8276 |  951 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|     4 |  952 | `{` |
|  8280 |  953 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     - |  954 | `	size_t nRead;` |
|  8280 |  955 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 |  956 | `		return -1;` |
|     - |  957 | `	}` |
|  8280 |  958 | `	if( pPipe->iMode != 'r' ){` |
|     - |  959 | `		/* Cannot read from a write-only pipe */` |
|   ! 0 |  960 | `		return -1;` |
|     - |  961 | `	}` |
|  8280 |  962 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
|  8280 |  963 | `	if( nRead == 0 ){` |
|  5360 |  964 | `		if( feof(pPipe->pFile) ){` |
|  5360 |  965 | `			return 0; /* EOF */` |
|     - |  966 | `		}` |
|   ! 0 |  967 | `		return -1; /* Error */` |
|     - |  968 | `	}` |
|  2924 |  969 | `	return (ph7_int64)nRead;` |
|  4142 |  970 | `}` |
|     - |  971 | `/*` |
|     - |  972 | ` * Pipe stream xWrite implementation.` |
|     - |  973 | ` */` |
|     6 |  974 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|     1 |  975 | `{` |
|     7 |  976 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|     - |  977 | `	size_t nWritten;` |
|     7 |  978 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 |  979 | `		return -1;` |
|     - |  980 | `	}` |
|     7 |  981 | `	if( pPipe->iMode != 'w' ){` |
|     - |  982 | `		/* Cannot write to a read-only pipe */` |
|   ! 0 |  983 | `		return -1;` |
|     - |  984 | `	}` |
|     7 |  985 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|     7 |  986 | `	if( nWritten == 0 && nWrite > 0 ){` |
|   ! 0 |  987 | `		return -1; /* Error */` |
|     - |  988 | `	}` |
|     7 |  989 | `	return (ph7_int64)nWritten;` |
|     4 |  990 | `}` |
|     - |  991 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|     - |  992 | `static const ph7_io_stream sPipe_Stream = {` |
|     - |  993 | `	"pipe",` |
|     - |  994 | `	PH7_IO_STREAM_VERSION,` |
|     - |  995 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|     - |  996 | `	0,  /* xOpenDir */` |
|     - |  997 | `	PipeStream_Close,  /* xClose */` |
|     - |  998 | `	0,  /* xCloseDir */` |
|     - |  999 | `	PipeStream_Read,   /* xRead */` |
|     - | 1000 | `	0,  /* xReadDir */` |
|     - | 1001 | `	PipeStream_Write,  /* xWrite */` |
|     - | 1002 | `	0,  /* xSeek */` |
|     - | 1003 | `	0,  /* xLock */` |
|     - | 1004 | `	0,  /* xRewindDir */` |
|     - | 1005 | `	0,  /* xTell */` |
|     - | 1006 | `	0,  /* xTrunc */` |
|     - | 1007 | `	0,  /* xSync */` |
|     - | 1008 | `	0   /* xStat */` |
|     - | 1009 | `};` |
|     - | 1010 | `/*` |
|     - | 1011 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|     - | 1012 | ` * FALSE otherwise.` |
|     - | 1013 | ` */` |
|  5332 | 1014 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|     5 | 1015 | `{` |
|  5337 | 1016 | `	return pStream == &sPipe_Stream;` |
|     5 | 1017 | `}` |
|     - | 1018 | `/*` |
|     - | 1019 | ` * resource popen(string $command, string $mode)` |
|     - | 1020 | ` *  Opens process file pointer.` |
|     - | 1021 | ` * Parameters` |
|     - | 1022 | ` *  $command` |
|     - | 1023 | ` *   The command to execute. Passed to the system shell.` |
|     - | 1024 | ` *  $mode` |
|     - | 1025 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|     - | 1026 | ` *   'r' - Open for reading (read from the command's stdout).` |
|     - | 1027 | ` *   'w' - Open for writing (write to the command's stdin).` |
|     - | 1028 | ` * Return` |
|     - | 1029 | ` *  Returns a file pointer on success, or FALSE on error.` |
|     - | 1030 | ` */` |
|     - | 1031 | `/*` |
|     - | 1032 | `` * The longest command line the platform's shell accepts — php's `cmd_max_len`,`` |
|     - | 1033 | ` * which both escapers refuse to exceed. php reads it once at startup from` |
|     - | 1034 | ` * sysconf(_SC_ARG_MAX) and hardcodes cmd.exe's constant on Windows.` |
|     - | 1035 | ` */` |
|   894 | 1036 | `static sxu32 ShellMaxCmdLen(void)` |
|     5 | 1037 | `{` |
|     - | 1038 | `#ifdef __WINNT__` |
|     - | 1039 | `	/* An escaped command runs through cmd.exe, whose limit is a constant. */` |
|     5 | 1040 | `	return 8192;` |
|     - | 1041 | `#elif defined(__UNIXES__) && defined(_SC_ARG_MAX)` |
|   894 | 1042 | `	long iMax = sysconf(_SC_ARG_MAX);` |
|   894 | 1043 | `	if( iMax <= 0 ){` |
|   ! 0 | 1044 | `		return 4096;   /* php's _POSIX_ARG_MAX fallback */` |
|     - | 1045 | `	}` |
|   894 | 1046 | `	return (sxu32)iMax;` |
|     - | 1047 | `#else` |
|     - | 1048 | `	return 4096;` |
|     - | 1049 | `#endif` |
|   452 | 1050 | `}` |
|     - | 1051 | `/*` |
|     - | 1052 | ` * How the two escapers WALK their argument, and the one thing they share.` |
|     - | 1053 | ` *` |
|     - | 1054 | ` * php walks it with php_mblen(), the process LC_CTYPE's multibyte reader: a` |
|     - | 1055 | ` * well-formed sequence is copied through untouched (a metacharacter's byte value` |
|     - | 1056 | ` * inside one is NOT a metacharacter), and a byte the encoding cannot start a` |
|     - | 1057 | ` * character with is DROPPED. That reader's answer is platform-shaped, and this` |
|     - | 1058 | ` * follows it on both, because it is what php answers on each:` |
|     - | 1059 | ` *` |
|     - | 1060 | ` *   POSIX    php picks LC_CTYPE up from the environment at startup, so the` |
|     - | 1061 | ` *            everyday answer is a UTF-8 one — a well-formed sequence rides` |
|     - | 1062 | ` *            through and an ill-formed byte is dropped. PHL is UTF-8-only (§10)` |
|     - | 1063 | ` *            and has no setlocale, so PH7_Utf8ReadStrict IS that reader.` |
|     - | 1064 | ` *            (php in the "C" locale glibc falls back to drops every byte >= 0x80` |
|     - | 1065 | ` *            instead, which is why the corpus guards this half on the oracle's` |
|     - | 1066 | ` *            own LC_CTYPE rather than pinning it unconditionally.)` |
|     - | 1067 | ` *   Windows  php reports LC_CTYPE "C", and MSVCRT's C locale is SINGLE-BYTE, not` |
|     - | 1068 | ` *            ASCII: mblen() answers 1 for every byte, so nothing is ever dropped` |
|     - | 1069 | `` *            and `\xFF` reaches the escape table below. Verified against php`` |
|     - | 1070 | ` *            8.5.8 on the gate VM, which does have an oracle — walking UTF-8` |
|     - | 1071 | ` *            there instead deleted bytes php keeps.` |
|     - | 1072 | ` *` |
|     - | 1073 | ` * Neither escaper can see a NUL byte: php parses both parameters with` |
|     - | 1074 | ` * Z_PARAM_PATH and the central screen (VmBuiltinPathMask) refuses one first.` |
|     - | 1075 | ` */` |
| 42077 | 1076 | `static int ShellCharIsWellFormed(const unsigned char *zIn,sxu32 nLeft,sxu32 *pnSeq)` |
|     5 | 1077 | `{` |
|     - | 1078 | `#ifdef __WINNT__` |
|     - | 1079 | `	SXUNUSED(zIn);` |
|     - | 1080 | `	SXUNUSED(nLeft);` |
|     5 | 1081 | `	*pnSeq = 1;` |
|     5 | 1082 | `	return 1;` |
|     - | 1083 | `#else` |
| 42077 | 1084 | `	return PH7_Utf8ReadStrict(zIn,nLeft,pnSeq) >= 0;` |
|     - | 1085 | `#endif` |
|     5 | 1086 | `}` |
|     - | 1087 | `/*` |
|     - | 1088 | ` * php's escapeshellarg(): wrap the whole argument in quotes the shell does not` |
|     - | 1089 | ` * look inside, and neutralise the one byte that could end them.` |
|     - | 1090 | ` *` |
|     - | 1091 | `` * POSIX: single quotes, and a `'` becomes `'\''` — close, escape, reopen.`` |
|     - | 1092 | `` * Windows: double quotes; there is no in-quote escape for `"` on cmd.exe, so php`` |
|     - | 1093 | `` * REPLACES `"` (and `%`/`!`, which cmd.exe still expands inside quotes) with a`` |
|     - | 1094 | ` * space, and doubles a trailing ODD run of backslashes so the last one escapes` |
|     - | 1095 | ` * itself rather than the closing quote.` |
|     - | 1096 | ` */` |
|   828 | 1097 | `static void ShellEscapeArg(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|     5 | 1098 | `{` |
|   833 | 1099 | `	sxu32 i = 0;` |
|     - | 1100 | `#ifdef __WINNT__` |
|     5 | 1101 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|     - | 1102 | `#else` |
|   828 | 1103 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|     - | 1104 | `#endif` |
| 42530 | 1105 | `	while( i < nLen ){` |
| 41702 | 1106 | `		sxu32 nSeq = 1;` |
| 41702 | 1107 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|     - | 1108 | `			/* Ill-formed: php skips the byte rather than escaping it */` |
|    18 | 1109 | `			i += nSeq;` |
|    22 | 1110 | `			continue;` |
|     - | 1111 | `		}` |
| 41684 | 1112 | `		if( nSeq > 1 ){` |
|     8 | 1113 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|     8 | 1114 | `			i += nSeq;` |
|     8 | 1115 | `			continue;` |
|     - | 1116 | `		}` |
|     - | 1117 | `#ifdef __WINNT__` |
|     5 | 1118 | `		if( zIn[i] == '"' \|\| zIn[i] == '%' \|\| zIn[i] == '!' ){` |
|     1 | 1119 | `			SyBlobAppend(pOut," ",sizeof(char));` |
|     1 | 1120 | `		}else{` |
|     5 | 1121 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|     - | 1122 | `		}` |
|     - | 1123 | `#else` |
| 41671 | 1124 | `		if( zIn[i] == '\'' ){` |
|     8 | 1125 | `			SyBlobAppend(pOut,"'\\'",sizeof("'\\'")-1);` |
|     4 | 1126 | `		}` |
| 41671 | 1127 | `		SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|     - | 1128 | `#endif` |
| 41676 | 1129 | `		i++;` |
|     5 | 1130 | `	}` |
|     - | 1131 | `#ifdef __WINNT__` |
|     - | 1132 | `	{` |
|     - | 1133 | `		/* A trailing run of backslashes would escape the closing quote if it is` |
|     - | 1134 | `		 * odd; the opening quote at offset 0 stops the scan the way php's does. */` |
|     5 | 1135 | `		const char *zCur = (const char *)SyBlobData(pOut);` |
|     5 | 1136 | `		sxu32 nCur = SyBlobLength(pOut);` |
|     5 | 1137 | `		sxu32 k = 0;` |
|     5 | 1138 | `		while( k < nCur && zCur[nCur - 1 - k] == '\\' ){` |
|     1 | 1139 | `			k++;` |
|     1 | 1140 | `		}` |
|     5 | 1141 | `		if( (k & 1) != 0 ){` |
|     1 | 1142 | `			SyBlobAppend(pOut,"\\",sizeof(char));` |
|     - | 1143 | `		}` |
|     - | 1144 | `	}` |
|     5 | 1145 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|     - | 1146 | `#else` |
|   828 | 1147 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|     - | 1148 | `#endif` |
|   833 | 1149 | `}` |
|     - | 1150 | `/*` |
|     - | 1151 | ` * php's escapeshellcmd(): the argument is a COMMAND, so it is not quoted at all —` |
|     - | 1152 | ` * every byte that could break out of one is prefixed with the shell's escape` |
|     - | 1153 | `` * character instead (`\` on POSIX, `^` on cmd.exe).`` |
|     - | 1154 | ` *` |
|     - | 1155 | ` * The one shape that is not a straight escape is a quote on POSIX: php leaves a` |
|     - | 1156 | ` * PAIR of them alone (the command may legitimately quote one of its own` |
|     - | 1157 | `` * arguments) and escapes an unpaired one. `pPair` is php's own one-slot state for`` |
|     - | 1158 | ` * that — it remembers the partner it found for the quote currently open, so the` |
|     - | 1159 | ` * closing one is recognised and the pairing resets. cmd.exe has no such rule, so` |
|     - | 1160 | `` * both quote characters (and `%`/`!`) are ordinary escapes there.`` |
|     - | 1161 | ` */` |
|    64 | 1162 | `static void ShellEscapeCmd(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|     1 | 1163 | `{` |
|    65 | 1164 | `	sxu32 i = 0;` |
|     - | 1165 | `#ifndef __WINNT__` |
|    64 | 1166 | `	const unsigned char *pPair = 0;` |
|     - | 1167 | `	static const char zEsc[] = "\\";` |
|     - | 1168 | `#else` |
|     - | 1169 | `	static const char zEsc[] = "^";` |
|     - | 1170 | `#endif` |
|   445 | 1171 | `	while( i < nLen ){` |
|   381 | 1172 | `		sxu32 nSeq = 1;` |
|   381 | 1173 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|    18 | 1174 | `			i += nSeq;` |
|    22 | 1175 | `			continue;` |
|     - | 1176 | `		}` |
|   363 | 1177 | `		if( nSeq > 1 ){` |
|     8 | 1178 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|     8 | 1179 | `			i += nSeq;` |
|     8 | 1180 | `			continue;` |
|     - | 1181 | `		}` |
|   355 | 1182 | `		switch( zIn[i] ){` |
|     - | 1183 | `#ifndef __WINNT__` |
|    12 | 1184 | `		case '"':` |
|     - | 1185 | `		case '\'':` |
|    19 | 1186 | `			if( pPair == 0` |
|    14 | 1187 | `			 && (pPair = (const unsigned char *)memchr(&zIn[i+1],zIn[i],nLen - i - 1)) != 0 ){` |
|     - | 1188 | `				/* This quote opens a pair: leave both of them alone */` |
|    17 | 1189 | `			}else if( pPair != 0 && pPair[0] == zIn[i] ){` |
|     8 | 1190 | `				pPair = 0;   /* the partner: pairing satisfied */` |
|     4 | 1191 | `			}else{` |
|     8 | 1192 | `				SyBlobAppend(pOut,zEsc,sizeof(char));` |
|     - | 1193 | `			}` |
|    24 | 1194 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|    24 | 1195 | `			break;` |
|     - | 1196 | `#else` |
|     - | 1197 | `		/* cmd.exe expands %VAR% and !VAR! even inside quotes, and has no` |
|     - | 1198 | ``		 * in-quote escape, so all four are plain `^` escapes there. */`` |
|     - | 1199 | `		case '%':` |
|     - | 1200 | `		case '!':` |
|     - | 1201 | `		case '"':` |
|     - | 1202 | `		case '\'':` |
|     - | 1203 | `#endif` |
|    21 | 1204 | `		case '#':` |
|     - | 1205 | `		case '&':` |
|     - | 1206 | `		case ';':` |
|     - | 1207 | ``		case '`':`` |
|     - | 1208 | `		case '\|':` |
|     - | 1209 | `		case '*':` |
|     - | 1210 | `		case '?':` |
|     - | 1211 | `		case '~':` |
|     - | 1212 | `		case '<':` |
|     - | 1213 | `		case '>':` |
|     - | 1214 | `		case '^':` |
|     - | 1215 | `		case '(':` |
|     - | 1216 | `		case ')':` |
|     - | 1217 | `		case '[':` |
|     - | 1218 | `		case ']':` |
|     - | 1219 | `		case '{':` |
|     - | 1220 | `		case '}':` |
|     - | 1221 | `		case '$':` |
|     - | 1222 | `		case '\\':` |
|     - | 1223 | `		case 0x0A:` |
|     - | 1224 | `		/* php escapes 0xFF too, and this is the row that decides the walk above` |
|     - | 1225 | `		 * is worth getting right: it is unreachable under the UTF-8 walk (0xF5..` |
|     - | 1226 | `		 * 0xFF is never a lead byte, so the reader drops the byte first) and` |
|     - | 1227 | `		 * REACHED on Windows, where php's single-byte C locale hands it here —` |
|     - | 1228 | ``		 * `escapeshellcmd("a\xffb")` is `a^\xffb` on the oracle. */`` |
|     - | 1229 | `		case 0xFF:` |
|    43 | 1230 | `			SyBlobAppend(pOut,zEsc,sizeof(char));` |
|     - | 1231 | `			/* fall through */` |
|   165 | 1232 | `		default:` |
|   331 | 1233 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|   330 | 1234 | `			break;` |
|     - | 1235 | `		}` |
|   355 | 1236 | `		i++;` |
|     1 | 1237 | `	}` |
|    65 | 1238 | `}` |
|     - | 1239 | `/*` |
|     - | 1240 | ` * string escapeshellarg(string $arg)` |
|     - | 1241 | ` *  Escape an argument so a shell passes it to the command as ONE word, whatever` |
|     - | 1242 | ` *  it contains.` |
|     - | 1243 | ` */` |
|   828 | 1244 | `PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1245 | `{` |
|   833 | 1246 | `	sxu32 nMax = ShellMaxCmdLen();` |
|     - | 1247 | `	const char *zArg;` |
|     - | 1248 | `	SyBlob sOut;` |
|     - | 1249 | `	int nLen;` |
|   414 | 1250 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|   833 | 1251 | `	zArg = ph7_value_to_string(apArg[0],&nLen);` |
|     - | 1252 | `	/* php's own bound: the command line has to hold the two quotes and a NUL */` |
|   833 | 1253 | `	if( nLen > 0 && (sxu32)nLen > nMax - 3 ){` |
|   ! 0 | 1254 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1255 | `			"Argument exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1256 | `	}` |
|   833 | 1257 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   833 | 1258 | `	ShellEscapeArg((const unsigned char *)zArg,(sxu32)nLen,&sOut);` |
|   833 | 1259 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|   ! 0 | 1260 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1261 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1262 | `			"Escaped argument exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1263 | `	}` |
|   833 | 1264 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   833 | 1265 | `	SyBlobRelease(&sOut);` |
|   833 | 1266 | `	return PH7_OK;` |
|   419 | 1267 | `}` |
|     - | 1268 | `/*` |
|     - | 1269 | ` * string escapeshellcmd(string $command)` |
|     - | 1270 | ` *  Escape every character that could break out of a shell command.` |
|     - | 1271 | ` */` |
|    66 | 1272 | `PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1273 | `{` |
|    67 | 1274 | `	sxu32 nMax = ShellMaxCmdLen();` |
|     - | 1275 | `	const char *zCmd;` |
|     - | 1276 | `	SyBlob sOut;` |
|     - | 1277 | `	int nLen;` |
|    33 | 1278 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|    67 | 1279 | `	zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|    67 | 1280 | `	if( nLen < 1 ){` |
|     - | 1281 | `		/* php answers "" without running the escaper at all */` |
|     3 | 1282 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 1283 | `		return PH7_OK;` |
|     - | 1284 | `	}` |
|    65 | 1285 | `	if( (sxu32)nLen > nMax - 3 ){` |
|   ! 0 | 1286 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1287 | `			"Command exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1288 | `	}` |
|    65 | 1289 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    65 | 1290 | `	ShellEscapeCmd((const unsigned char *)zCmd,(sxu32)nLen,&sOut);` |
|    65 | 1291 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|   ! 0 | 1292 | `		SyBlobRelease(&sOut);` |
|   ! 0 | 1293 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 | 1294 | `			"Escaped command exceeds the allowed length of %d bytes",(int)nMax);` |
|     - | 1295 | `	}` |
|    65 | 1296 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    65 | 1297 | `	SyBlobRelease(&sOut);` |
|    65 | 1298 | `	return PH7_OK;` |
|    34 | 1299 | `}` |
|     - | 1300 | `/*` |
|     - | 1301 | ` * php refuses an EMPTY command in all four runners (exec/system/passthru/` |
|     - | 1302 | ` * shell_exec) — the shell would answer success for one, so the refusal is the` |
|     - | 1303 | ` * only way a script hears about a command string that came out empty.` |
|     - | 1304 | ` */` |
|     8 | 1305 | `static sxi32 ShellEmptyCommandError(ph7_context *pCtx)` |
|     1 | 1306 | `{` |
|    13 | 1307 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     4 | 1308 | `		"%s(): Argument #1 ($command) must not be empty",ph7_function_name(pCtx));` |
|     1 | 1309 | `}` |
|     - | 1310 | `/*` |
|     - | 1311 | ` * string\|false\|null shell_exec(string $command)` |
|     - | 1312 | ` *  Execute a command via the shell and return the complete output as a string.` |
|     - | 1313 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|     - | 1314 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|     - | 1315 | ` */` |
|   390 | 1316 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1317 | `{` |
|     - | 1318 | `	const char *zCommand;` |
|     - | 1319 | `	pipe_private *pPipe;` |
|     - | 1320 | `	SyBlob sOut;` |
|     - | 1321 | `	char zBuf[4096];` |
|     - | 1322 | `	size_t nRead;` |
|     - | 1323 | `	int nCmdLen;` |
|   394 | 1324 | `	if( nArg < 1 ){` |
|   ! 0 | 1325 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1326 | `		return PH7_OK;` |
|     - | 1327 | `	}` |
|   394 | 1328 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|   394 | 1329 | `	if( nCmdLen < 1 ){` |
|     - | 1330 | `		/* php refuses an empty command rather than running the shell on it */` |
|     3 | 1331 | `		return ShellEmptyCommandError(pCtx);` |
|     - | 1332 | `	}` |
|   391 | 1333 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|   391 | 1334 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|     - | 1335 | `		/* php's own wording for this one; the three runners below say "Unable to` |
|     - | 1336 | `		 * fork [%s]" instead. Both used to be silent. */` |
|   ! 0 | 1337 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to execute '%s'",` |
|   ! 0 | 1338 | `			ph7_function_name(pCtx),zCommand);` |
|   ! 0 | 1339 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1340 | `		return PH7_OK;` |
|     - | 1341 | `	}` |
|   391 | 1342 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   351 | 1343 | `	for(;;){` |
|   705 | 1344 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|   705 | 1345 | `		if( nRead < 1 ){` |
|   391 | 1346 | `			break;` |
|     - | 1347 | `		}` |
|   317 | 1348 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|     3 | 1349 | `	}` |
|   391 | 1350 | `	PipeClose(pPipe);` |
|   391 | 1351 | `	if( SyBlobLength(&sOut) < 1 ){` |
|     - | 1352 | `		/* php answers NULL, not "", when the command printed nothing */` |
|    76 | 1353 | `		ph7_result_null(pCtx);` |
|    39 | 1354 | `	}else{` |
|   317 | 1355 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     - | 1356 | `	}` |
|   391 | 1357 | `	SyBlobRelease(&sOut);` |
|   391 | 1358 | `	return PH7_OK;` |
|   199 | 1359 | `}` |
|     - | 1360 | `/*` |
|     - | 1361 | ` * php's three command RUNNERS are one routine (php_exec) with a mode, and the` |
|     - | 1362 | ` * mode decides two things: what happens to each LINE of the command's output,` |
|     - | 1363 | ` * and what the call answers.` |
|     - | 1364 | ` *` |
|     - | 1365 | ` *   exec($cmd)           keep nothing, answer the LAST line` |
|     - | 1366 | ` *   exec($cmd, $output)  append every line to the array, answer the last line` |
|     - | 1367 | ` *   system($cmd)         WRITE every line as it arrives, answer the last line` |
|     - | 1368 | ` *   passthru($cmd)       write the raw bytes, answer NULL` |
|     - | 1369 | ` *` |
|     - | 1370 | ` * "The last line" is php's: its trailing WHITESPACE is stripped — spaces and` |
|     - | 1371 | ` * tabs as much as the newline — and so is every element of $output's. A command` |
|     - | 1372 | ` * that printed nothing answers "" rather than false, which is php's documented` |
|     - | 1373 | ` * BC wart and not an error indication; the error indication is FALSE, and only` |
|     - | 1374 | ` * a pipe that could not be opened produces it.` |
|     - | 1375 | ` *` |
|     - | 1376 | ` * All three share the exit status, which is the pipe's close status (php's` |
|     - | 1377 | ` * $result_code out-param) and -1 when there was no process at all.` |
|     - | 1378 | ` */` |
|     - | 1379 | `#ifdef __WINNT__` |
|     - | 1380 | `# define SHELL_RUN_PIPE_MODE "rb"` |
|     - | 1381 | `#else` |
|     - | 1382 | `# define SHELL_RUN_PIPE_MODE "r"` |
|     - | 1383 | `#endif` |
|     - | 1384 | `#define SHELL_RUN_LAST     0   /* exec() with no $output array */` |
|     - | 1385 | `#define SHELL_RUN_ECHO     1   /* system() */` |
|     - | 1386 | `#define SHELL_RUN_COLLECT  2   /* exec() with one */` |
|     - | 1387 | `#define SHELL_RUN_RAW      3   /* passthru() */` |
|     - | 1388 | `/*` |
|     - | 1389 | ` * php's strip_trailing_whitespace(): answers the length that stays.` |
|     - | 1390 | ` */` |
|    74 | 1391 | `static sxu32 ShellStripTrailing(const char *zLine,sxu32 nLine)` |
|     2 | 1392 | `{` |
|   174 | 1393 | `	while( nLine > 0 && SyisSpace((unsigned char)zLine[nLine - 1]) ){` |
|   100 | 1394 | `		nLine--;` |
|     2 | 1395 | `	}` |
|    76 | 1396 | `	return nLine;` |
|     2 | 1397 | `}` |
|     - | 1398 | `/*` |
|     - | 1399 | ` * One complete line of output, dealt with the mode's way. The line still carries` |
|     - | 1400 | ` * its own newline: system() writes it (php hands the whole line to the output` |
|     - | 1401 | ` * layer, so an output buffer catches it like any echo), and the collector strips` |
|     - | 1402 | ` * it along with the rest of the trailing whitespace.` |
|     - | 1403 | ` */` |
|    50 | 1404 | `static sxi32 ShellHandleLine(ph7_context *pCtx,int iType,ph7_value *pArray,` |
|     - | 1405 | `	const char *zLine,sxu32 nLine)` |
|     2 | 1406 | `{` |
|    52 | 1407 | `	if( iType == SHELL_RUN_ECHO ){` |
|    12 | 1408 | `		return ph7_context_output(pCtx,zLine,(int)nLine);` |
|     - | 1409 | `	}` |
|    41 | 1410 | `	if( iType == SHELL_RUN_COLLECT && pArray ){` |
|    41 | 1411 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    41 | 1412 | `		if( pVal == 0 ){` |
|   ! 0 | 1413 | `			return PH7_OK;` |
|     - | 1414 | `		}` |
|    41 | 1415 | `		ph7_value_string(pVal,zLine,(int)ShellStripTrailing(zLine,nLine));` |
|    41 | 1416 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    41 | 1417 | `		ph7_context_release_value(pCtx,pVal);` |
|    20 | 1418 | `	}` |
|    41 | 1419 | `	return PH7_OK;` |
|    27 | 1420 | `}` |
|     - | 1421 | `/*` |
|     - | 1422 | ` * Run $command through the shell in the given mode, fill the by-reference` |
|     - | 1423 | ` * out-params and set the call's result. The three builtins below are this` |
|     - | 1424 | ` * routine plus their own mode.` |
|     - | 1425 | ` */` |
|    46 | 1426 | `static sxi32 ShellRunCommand(ph7_context *pCtx,int iType,int nArg,ph7_value **apArg)` |
|     2 | 1427 | `{` |
|     - | 1428 | `	/* exec() carries $output before $result_code; the other two do not */` |
|    48 | 1429 | `	int iCodeArg = (iType == SHELL_RUN_LAST) ? 2 : 1;` |
|    48 | 1430 | `	ph7_value *pArray = 0, *pOwned = 0;` |
|     - | 1431 | `	const char *zCommand;` |
|     - | 1432 | `	pipe_private *pPipe;` |
|     - | 1433 | `	SyBlob sLine, sLast;` |
|     - | 1434 | `	char zBuf[4096];` |
|     - | 1435 | `	size_t nRead;` |
|    48 | 1436 | `	int nCmdLen, iStatus = -1;` |
|    48 | 1437 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|    48 | 1438 | `	if( nCmdLen < 1 ){` |
|     7 | 1439 | `		return ShellEmptyCommandError(pCtx);` |
|     - | 1440 | `	}` |
|     - | 1441 | `	/* $output turns exec() into the collecting mode. php uses the array the` |
|     - | 1442 | `	 * caller already holds — the manual's "will append to the end of the array" —` |
|     - | 1443 | `	 * and replaces anything else with a fresh one, BEFORE running the command, so` |
|     - | 1444 | `	 * even a failed run leaves the variable an array. */` |
|    42 | 1445 | `	if( iType == SHELL_RUN_LAST && nArg > 1 ){` |
|    27 | 1446 | `		iType = SHELL_RUN_COLLECT;` |
|    27 | 1447 | `		if( ph7_value_is_array(apArg[1]) ){` |
|     2 | 1448 | `			PH7_HashmapCowSeparate(pCtx->pVm,apArg[1]);` |
|     2 | 1449 | `			pArray = apArg[1];` |
|     1 | 1450 | `		}else{` |
|    25 | 1451 | `			pOwned = pArray = ph7_context_new_array(pCtx);` |
|     - | 1452 | `		}` |
|    13 | 1453 | `	}` |
|     - | 1454 | `	/* php_exec's own mode, per platform: the three runners hand back what the` |
|     - | 1455 | `	 * command WROTE, so on Windows the CRs have to survive the pipe (where` |
|     - | 1456 | `	 * shell_exec() takes php's "rt" and does translate them). popen(3) refuses a` |
|     - | 1457 | `	 * 'b' it has nothing to translate, which is why this is not one string. */` |
|    42 | 1458 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,SHELL_RUN_PIPE_MODE);` |
|    42 | 1459 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|   ! 0 | 1460 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to fork [%s]",` |
|   ! 0 | 1461 | `			ph7_function_name(pCtx),zCommand);` |
|   ! 0 | 1462 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1463 | `	}else{` |
|    42 | 1464 | `		int bAbort = 0;   /* the output consumer asked to stop (PH7_ABORT) */` |
|    42 | 1465 | `		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|    42 | 1466 | `		SyBlobInit(&sLast,&pCtx->pVm->sAllocator);` |
|    36 | 1467 | `		for(;;){` |
|    80 | 1468 | `			nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|    80 | 1469 | `			if( nRead < 1 ){` |
|    42 | 1470 | `				break;` |
|     - | 1471 | `			}` |
|    40 | 1472 | `			if( iType == SHELL_RUN_RAW ){` |
|     - | 1473 | `				/* passthru() never looks for a line: php writes what it read */` |
|     8 | 1474 | `				if( ph7_context_output(pCtx,zBuf,(int)nRead) == PH7_ABORT ){` |
|   ! 0 | 1475 | `					break;` |
|     - | 1476 | `				}` |
|     8 | 1477 | `				continue;` |
|     - | 1478 | `			}` |
|     - | 1479 | `			{` |
|    34 | 1480 | `				size_t iOfft = 0;` |
|    82 | 1481 | `				while( iOfft < nRead ){` |
|    56 | 1482 | `					const char *zNl = (const char *)memchr(&zBuf[iOfft],'\n',nRead - iOfft);` |
|    56 | 1483 | `					size_t nChunk = zNl ? (size_t)(zNl - &zBuf[iOfft]) + 1 : nRead - iOfft;` |
|    56 | 1484 | `					SyBlobAppend(&sLine,&zBuf[iOfft],(sxu32)nChunk);` |
|    56 | 1485 | `					iOfft += nChunk;` |
|    56 | 1486 | `					if( zNl == 0 ){` |
|     6 | 1487 | `						break;   /* the line continues in the next read */` |
|     - | 1488 | `					}` |
|    72 | 1489 | `					if( ShellHandleLine(pCtx,iType,pArray,` |
|    74 | 1490 | `						(const char *)SyBlobData(&sLine),SyBlobLength(&sLine)) == PH7_ABORT ){` |
|   ! 0 | 1491 | `						bAbort = 1;` |
|   ! 0 | 1492 | `					}` |
|     - | 1493 | `					/* Keep it: the call answers the last line it saw */` |
|    50 | 1494 | `					SyBlobReset(&sLast);` |
|    50 | 1495 | `					SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|    50 | 1496 | `					SyBlobReset(&sLine);` |
|    50 | 1497 | `					if( bAbort ){` |
|   ! 0 | 1498 | `						break;` |
|     - | 1499 | `					}` |
|     2 | 1500 | `				}` |
|     - | 1501 | `			}` |
|    34 | 1502 | `			if( bAbort ){` |
|   ! 0 | 1503 | `				break;` |
|     - | 1504 | `			}` |
|     2 | 1505 | `		}` |
|     - | 1506 | `		/* Output that ended without a newline is still a line */` |
|    42 | 1507 | `		if( !bAbort && SyBlobLength(&sLine) > 0 ){` |
|     3 | 1508 | `			ShellHandleLine(pCtx,iType,pArray,` |
|     2 | 1509 | `				(const char *)SyBlobData(&sLine),SyBlobLength(&sLine));` |
|     2 | 1510 | `			SyBlobReset(&sLast);` |
|     2 | 1511 | `			SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|     1 | 1512 | `		}` |
|    42 | 1513 | `		iStatus = PipeClose(pPipe);` |
|    42 | 1514 | `		if( iType == SHELL_RUN_RAW ){` |
|     8 | 1515 | `			ph7_result_null(pCtx);` |
|     5 | 1516 | `		}else{` |
|    53 | 1517 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&sLast),` |
|    34 | 1518 | `				(int)ShellStripTrailing((const char *)SyBlobData(&sLast),SyBlobLength(&sLast)));` |
|     - | 1519 | `		}` |
|    42 | 1520 | `		SyBlobRelease(&sLine);` |
|    42 | 1521 | `		SyBlobRelease(&sLast);` |
|     - | 1522 | `	}` |
|    42 | 1523 | `	if( pOwned ){` |
|    25 | 1524 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOwned);` |
|    25 | 1525 | `		ph7_context_release_value(pCtx,pOwned);` |
|    12 | 1526 | `	}` |
|    37 | 1527 | `	if( nArg > iCodeArg ){` |
|     - | 1528 | `		ph7_value sVal;` |
|    20 | 1529 | `		PH7_MemObjInitFromInt(pCtx->pVm,&sVal,iStatus);` |
|    20 | 1530 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iCodeArg],&sVal);` |
|    20 | 1531 | `		PH7_MemObjRelease(&sVal);` |
|     9 | 1532 | `	}` |
|    42 | 1533 | `	return PH7_OK;` |
|    25 | 1534 | `}` |
|     - | 1535 | `/*` |
|     - | 1536 | ` * string\|false exec(string $command, array &$output = null, int &$result_code = null)` |
|     - | 1537 | ` *  Run a command and answer the last line of its output.` |
|     - | 1538 | ` */` |
|    30 | 1539 | `PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1540 | `{` |
|    31 | 1541 | `	return ShellRunCommand(pCtx,SHELL_RUN_LAST,nArg,apArg);` |
|     1 | 1542 | `}` |
|     - | 1543 | `/*` |
|     - | 1544 | ` * string\|false system(string $command, int &$result_code = null)` |
|     - | 1545 | ` *  Run a command, write its output as it arrives, answer the last line.` |
|     - | 1546 | ` */` |
|     8 | 1547 | `PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1548 | `{` |
|    10 | 1549 | `	return ShellRunCommand(pCtx,SHELL_RUN_ECHO,nArg,apArg);` |
|     2 | 1550 | `}` |
|     - | 1551 | `/*` |
|     - | 1552 | ` * ?false passthru(string $command, int &$result_code = null)` |
|     - | 1553 | ` *  Run a command and write its output through, byte for byte.` |
|     - | 1554 | ` */` |
|     8 | 1555 | `PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1556 | `{` |
|    10 | 1557 | `	return ShellRunCommand(pCtx,SHELL_RUN_RAW,nArg,apArg);` |
|     2 | 1558 | `}` |
|     - | 1559 | `/*` |
|     - | 1560 | ` * bool proc_nice(int $priority)` |
|     - | 1561 | ` *  Change the priority of the running process — the last member of php's own` |
|     - | 1562 | ` *  process-execution surface, and the only one of the seven that runs no shell.` |
|     - | 1563 | ` */` |
|     6 | 1564 | `PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1565 | `{` |
|     - | 1566 | `	ph7_int64 iPri;` |
|     3 | 1567 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|     7 | 1568 | `	iPri = ph7_value_to_int64(apArg[0]);` |
|     - | 1569 | `#ifdef __WINNT__` |
|     - | 1570 | `	{` |
|     - | 1571 | `		/* php's own mapping (win32/nice.c): cmd.exe has no nice value, so the` |
|     - | 1572 | `		 * POSIX increment is bucketed into the five priority CLASSES Windows` |
|     - | 1573 | `		 * has. REALTIME is deliberately not reachable there, and neither is it` |
|     - | 1574 | `		 * here. */` |
|     1 | 1575 | `		DWORD dwFlag = NORMAL_PRIORITY_CLASS;` |
|     1 | 1576 | `		if( iPri < -9 ){` |
|   ! 0 | 1577 | `			dwFlag = HIGH_PRIORITY_CLASS;` |
|     1 | 1578 | `		}else if( iPri < -4 ){` |
|   ! 0 | 1579 | `			dwFlag = ABOVE_NORMAL_PRIORITY_CLASS;` |
|     1 | 1580 | `		}else if( iPri > 9 ){` |
|   ! 0 | 1581 | `			dwFlag = IDLE_PRIORITY_CLASS;` |
|     1 | 1582 | `		}else if( iPri > 4 ){` |
|   ! 0 | 1583 | `			dwFlag = BELOW_NORMAL_PRIORITY_CLASS;` |
|     - | 1584 | `		}` |
|     1 | 1585 | `		SetPriorityClass(GetCurrentProcess(),dwFlag);` |
|     - | 1586 | `		/* php answers TRUE whatever that returned: its nice() reports failure` |
|     - | 1587 | `		 * through its return value and leaves errno alone, and proc_nice() reads` |
|     - | 1588 | `		 * only errno. */` |
|     1 | 1589 | `		ph7_result_bool(pCtx,1);` |
|     - | 1590 | `	}` |
|     - | 1591 | `#elif defined(__UNIXES__)` |
|     - | 1592 | `	{` |
|     - | 1593 | `		int iIgnored;` |
|     - | 1594 | `		/* nice() legitimately answers -1 (it returns the NEW nice value), so` |
|     - | 1595 | `		 * errno is the only failure evidence — php clears it first for the same` |
|     - | 1596 | `		 * reason. */` |
|     6 | 1597 | `		errno = 0;` |
|     6 | 1598 | `		iIgnored = nice((int)iPri);` |
|     3 | 1599 | `		(void)iIgnored;` |
|     6 | 1600 | `		if( errno != 0 ){` |
|     3 | 1601 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|     - | 1602 | `				"%s(): Only a super user may attempt to increase the priority of a process",` |
|     1 | 1603 | `				ph7_function_name(pCtx));` |
|     2 | 1604 | `			ph7_result_bool(pCtx,0);` |
|     1 | 1605 | `		}else{` |
|     4 | 1606 | `			ph7_result_bool(pCtx,1);` |
|     - | 1607 | `		}` |
|     - | 1608 | `	}` |
|     - | 1609 | `#else` |
|     - | 1610 | `	ph7_result_bool(pCtx,0);` |
|     - | 1611 | `#endif` |
|     7 | 1612 | `	return PH7_OK;` |
|     1 | 1613 | `}` |
|  5504 | 1614 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1615 | `{` |
|     - | 1616 | `	const char *zCommand, *zMode;` |
|     - | 1617 | `	char zPosix[8];` |
|     - | 1618 | `	pipe_private *pPipe;` |
|     - | 1619 | `	io_private *pDev;` |
|  5509 | 1620 | `	int nCmdLen, nModeLen, nPosix, i, bDropped = 0;` |
|  2752 | 1621 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|     - | 1622 | `	/* Extract the command and mode */` |
|  5509 | 1623 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
|  5509 | 1624 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|     - | 1625 | `	/*` |
|     - | 1626 | `	 * php's mode rule, and the only one it has: ONE 'b' — C's binary flag, which` |
|     - | 1627 | `	 * popen(3) itself refuses — is dropped from the mode on POSIX, and what is` |
|     - | 1628 | `	 * left must be exactly "r", "w", "rb" or "wb". PHL used to read mode[0] and` |
|     - | 1629 | `	 * hand the REST to popen(3) unexamined, which was wrong in both directions:` |
|     - | 1630 | ``	 * `popen($cmd, 'rb')`, the ordinary binary spelling, answered FALSE because`` |
|     - | 1631 | ``	 * glibc rejected the 'b', and `popen($cmd, 'rr')` opened a pipe php refuses.`` |
|     - | 1632 | `	 */` |
|  5509 | 1633 | `	nPosix = 0;` |
|     - | 1634 | `#ifdef __WINNT__` |
|     - | 1635 | `	SXUNUSED(bDropped);   /* cmd.exe keeps the 'b': _popen understands it */` |
|     - | 1636 | `#endif` |
| 11027 | 1637 | `	for( i = 0 ; i < nModeLen && nPosix < (int)sizeof(zPosix) - 1 ; ++i ){` |
|     - | 1638 | `#ifndef __WINNT__` |
|  5518 | 1639 | `		if( zMode[i] == 'b' && !bDropped ){` |
|     8 | 1640 | `			bDropped = 1;   /* php drops the FIRST one and only that one */` |
|     8 | 1641 | `			continue;` |
|     - | 1642 | `		}` |
|     - | 1643 | `#endif` |
|  5515 | 1644 | `		zPosix[nPosix++] = zMode[i];` |
|  2760 | 1645 | `	}` |
|  5509 | 1646 | `	zPosix[nPosix] = 0;` |
|  5504 | 1647 | `	if( nPosix > 2` |
|  5504 | 1648 | `	 \|\| (nPosix == 1 && zPosix[0] != 'r' && zPosix[0] != 'w')` |
|  2766 | 1649 | `	 \|\| (nPosix == 2 && SyMemcmp(zPosix,"rb",sizeof("rb")-1) != 0` |
|     7 | 1650 | `	                 && SyMemcmp(zPosix,"wb",sizeof("wb")-1) != 0) ){` |
|     9 | 1651 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1652 | `			"popen(): Argument #2 ($mode) must be one of \"r\", \"rb\", \"w\", or \"wb\"");` |
|     - | 1653 | `	}` |
|     - | 1654 | `	/* Open the pipe. An EMPTY mode passes php's check above and fails HERE, in` |
|     - | 1655 | `	 * popen(3) — php reports it as an open failure and so does this, rather than` |
|     - | 1656 | `	 * letting the platform layer read mode[0] out of an empty string. */` |
|  5501 | 1657 | `	pPipe = nPosix > 0 ? PipeOpen(pCtx->pVm, zCommand, zPosix) : 0;` |
|  5501 | 1658 | `	if( pPipe == 0 ){` |
|     - | 1659 | ``		/* php names both arguments in this one: `popen(cmd,mode): message`. PHL`` |
|     - | 1660 | `		 * answered FALSE in silence, so a script had nothing to report. */` |
|     5 | 1661 | `		if( nPosix < 1 ){` |
|     3 | 1662 | `			errno = EINVAL;` |
|     1 | 1663 | `		}` |
|     7 | 1664 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|     4 | 1665 | `			ph7_function_name(pCtx),zCommand,zPosix,VfsStrerror(errno));` |
|     5 | 1666 | `		ph7_result_bool(pCtx, 0);` |
|     5 | 1667 | `		return PH7_OK;` |
|     - | 1668 | `	}` |
|     - | 1669 | `	/* Allocate an io_private instance to wrap the pipe */` |
|  5497 | 1670 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
|  5497 | 1671 | `	if( pDev == 0 ){` |
|   ! 0 | 1672 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|   ! 0 | 1673 | `		PipeClose(pPipe);` |
|   ! 0 | 1674 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 1675 | `		return PH7_OK;` |
|     - | 1676 | `	}` |
|     - | 1677 | `	/* Initialize the io_private structure */` |
|  5497 | 1678 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
|     - | 1679 | `	/* A pipe has no wrapper and no path, so php's meta reports the MODE and` |
|     - | 1680 | ``	 * neither `wrapper_type` nor `uri`: an empty URI is what leaves them out. */`` |
|  5497 | 1681 | `	SetIOPrivateOpenedAs(pDev,0,0,zPosix,nPosix);` |
|  5497 | 1682 | `	pDev->pHandle = pPipe;` |
|     - | 1683 | `	/* Return the io_private instance as a resource */` |
|  5497 | 1684 | `	ph7_result_resource(pCtx, pDev);` |
|  5497 | 1685 | `	return PH7_OK;` |
|  2757 | 1686 | `}` |
|     - | 1687 | `/*` |
|     - | 1688 | ` * int pclose(resource $handle)` |
|     - | 1689 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|     - | 1690 | ` * Parameters` |
|     - | 1691 | ` *  $handle` |
|     - | 1692 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|     - | 1693 | ` * Return` |
|     - | 1694 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|     - | 1695 | ` */` |
|  5332 | 1696 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1697 | `{` |
|     - | 1698 | `	const ph7_io_stream *pStream;` |
|     - | 1699 | `	pipe_private *pPipe;` |
|     - | 1700 | `	io_private *pDev;` |
|     - | 1701 | `	int status;` |
|  5337 | 1702 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|     - | 1703 | `		/* Missing/Invalid arguments, return -1 */` |
|   ! 0 | 1704 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|   ! 0 | 1705 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1706 | `		return PH7_OK;` |
|     - | 1707 | `	}` |
|     - | 1708 | `	/* Extract our private data */` |
|  5337 | 1709 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     - | 1710 | `	/* Make sure we are dealing with a valid io_private instance */` |
|  5337 | 1711 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|   ! 0 | 1712 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|   ! 0 | 1713 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1714 | `		return PH7_OK;` |
|     - | 1715 | `	}` |
|     - | 1716 | `	/* Point to the target IO stream device */` |
|  5337 | 1717 | `	pStream = pDev->pStream;` |
|  5337 | 1718 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|   ! 0 | 1719 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|   ! 0 | 1720 | `		ph7_result_int(pCtx, -1);` |
|   ! 0 | 1721 | `		return PH7_OK;` |
|     - | 1722 | `	}` |
|     - | 1723 | `	/* Get the pipe handle */` |
|  5337 | 1724 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|     - | 1725 | `	/* A write chain gets its closing call while the pipe is still open. */` |
|  5337 | 1726 | `	PH7_StreamFilterReleaseChains(pDev);` |
|     - | 1727 | `	/* Close the pipe and get exit status */` |
|  5337 | 1728 | `	status = PipeClose(pPipe);` |
|     - | 1729 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
|  5337 | 1730 | `	MarkIOPrivateClosed(pDev);` |
|     - | 1731 | `	/* Return the exit status */` |
|  5337 | 1732 | `	ph7_result_int(pCtx, status);` |
|  5337 | 1733 | `	return PH7_OK;` |
|  2671 | 1734 | `}` |
|     - | 1735 | `/*` |
|     - | 1736 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|     - | 1737 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|     - | 1738 | ` *   standard descriptors (php's process-control family). The returned` |
|     - | 1739 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|     - | 1740 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|     - | 1741 | ` *   bounds and report it as a live, non-stream resource.` |
|     - | 1742 | ` */` |
|     - | 1743 | `#ifdef __UNIXES__` |
|     - | 1744 | `#define PROC_MAX_DESC 16` |
|     - | 1745 | `typedef struct proc_private proc_private;` |
|     - | 1746 | `struct proc_private` |
|     - | 1747 | `{` |
|     - | 1748 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|     - | 1749 | `	int pid;           /* child process id */` |
|     - | 1750 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|     - | 1751 | `	int exit_code;     /* cached exit status once reaped */` |
|     - | 1752 | `};` |
|     - | 1753 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|    92 | 1754 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd,int bParentReads)` |
|     - | 1755 | `{` |
|    92 | 1756 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    92 | 1757 | `	if( pDev == 0 ){` |
|   ! 0 | 1758 | `		return 0;` |
|     - | 1759 | `	}` |
|    92 | 1760 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|     - | 1761 | `	/* Same shape as popen()'s end: a proc_open() pipe carries no path, and its` |
|     - | 1762 | `	 * mode is the direction the PARENT holds — the opposite of the child's. */` |
|    92 | 1763 | `	SetIOPrivateOpenedAs(pDev,0,0,bParentReads ? "r" : "w",1);` |
|    92 | 1764 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|    92 | 1765 | `	return pDev;` |
|    46 | 1766 | `}` |
|     - | 1767 | `/* One parsed descriptor-spec entry. */` |
|     - | 1768 | `struct proc_desc` |
|     - | 1769 | `{` |
|     - | 1770 | `	int child_fd;      /* the array key: which fd the child sees */` |
|     - | 1771 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|     - | 1772 | `	/* pipe */` |
|     - | 1773 | `	int child_end;     /* fd the child must have at child_fd */` |
|     - | 1774 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|     - | 1775 | `	int parent_reads;  /* the parent's end is the READ end (the child writes) */` |
|     - | 1776 | `	/* file */` |
|     - | 1777 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|     - | 1778 | `	/* redirect */` |
|     - | 1779 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|     - | 1780 | `};` |
|    34 | 1781 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 1782 | `{` |
|     - | 1783 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|    34 | 1784 | `	int nDesc = 0;` |
|     - | 1785 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|     - | 1786 | `	ph7_hashmap *pSpecMap;` |
|     - | 1787 | `	ph7_hashmap_node *pNode;` |
|    34 | 1788 | `	ph7_vm *pVm = pCtx->pVm;` |
|    34 | 1789 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|    34 | 1790 | `	int nArgv = 0;` |
|    34 | 1791 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|    34 | 1792 | `	const char *zCwd = 0;` |
|    34 | 1793 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|    34 | 1794 | `	int nEnv = 0;` |
|     - | 1795 | `	proc_private *pProc;` |
|     - | 1796 | `	pid_t pid;` |
|     - | 1797 | `	int i, rc;` |
|    34 | 1798 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|   ! 0 | 1799 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|   ! 0 | 1800 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1801 | `		return PH7_OK;` |
|     - | 1802 | `	}` |
|     - | 1803 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|    34 | 1804 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    32 | 1805 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    32 | 1806 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|    32 | 1807 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|    32 | 1808 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    32 | 1809 | `		pNode = pCmdMap->pFirst;` |
|   126 | 1810 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|    94 | 1811 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|     - | 1812 | `			int nLen; const char *zs;` |
|    94 | 1813 | `			PH7_MemObjInit(pVm,pv);` |
|    94 | 1814 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|    94 | 1815 | `			zs = ph7_value_to_string(pv,&nLen);` |
|    94 | 1816 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|    94 | 1817 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|    94 | 1818 | `			PH7_MemObjRelease(pv);` |
|    94 | 1819 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|    94 | 1820 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|    47 | 1821 | `		}` |
|    32 | 1822 | `		azArgv[nArgv] = 0;` |
|    16 | 1823 | `	}else{` |
|     - | 1824 | `		int nLen;` |
|     2 | 1825 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|     - | 1826 | `	}` |
|     - | 1827 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|    34 | 1828 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|   ! 0 | 1829 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|   ! 0 | 1830 | `		if( nLen < 1 ){ zCwd = 0; }` |
|   ! 0 | 1831 | `	}` |
|    17 | 1832 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|   ! 0 | 1833 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|   ! 0 | 1834 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|   ! 0 | 1835 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|   ! 0 | 1836 | `		if( azEnv ){` |
|   ! 0 | 1837 | `			pNode = pEnvMap->pFirst;` |
|   ! 0 | 1838 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|     - | 1839 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|   ! 0 | 1840 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|   ! 0 | 1841 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|   ! 0 | 1842 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|   ! 0 | 1843 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|   ! 0 | 1844 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|   ! 0 | 1845 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));` |
|   ! 0 | 1846 | `				if( zPair ){` |
|   ! 0 | 1847 | `					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|   ! 0 | 1848 | `					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|   ! 0 | 1849 | `					azEnv[nEnv++] = zPair;` |
|   ! 0 | 1850 | `				}` |
|   ! 0 | 1851 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|   ! 0 | 1852 | `				pNode = pNode->pPrev;` |
|   ! 0 | 1853 | `			}` |
|   ! 0 | 1854 | `			azEnv[nEnv] = 0;` |
|   ! 0 | 1855 | `		}` |
|   ! 0 | 1856 | `	}` |
|     - | 1857 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|    34 | 1858 | `	pSpec = apArg[1];` |
|    34 | 1859 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|    34 | 1860 | `	pNode = pSpecMap->pFirst;` |
|   130 | 1861 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|    96 | 1862 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|    96 | 1863 | `		PH7_MemObjInit(pVm,&sKey);` |
|    96 | 1864 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|    96 | 1865 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|    96 | 1866 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1; pD->parent_reads = 0;` |
|    96 | 1867 | `		PH7_MemObjRelease(&sKey);` |
|    96 | 1868 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|    96 | 1869 | `		PH7_MemObjInit(pVm,pEntry);` |
|    96 | 1870 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|    96 | 1871 | `		if( ph7_value_is_array(pEntry) ){` |
|     - | 1872 | `			int nLen; const char *zType;` |
|     - | 1873 | `			/* pEntry aliases the descriptor array the SCRIPT still holds, so every` |
|     - | 1874 | `			 * member is read through a scratch copy — ph7_value_to_string() would` |
|     - | 1875 | `			 * convert the entry in place and rewrite the script's own $descriptors` |
|     - | 1876 | ``			 * (`[0 => ['pipe', 114]]` came back as `'114'`). */`` |
|     - | 1877 | `			ph7_value sPeek;` |
|    96 | 1878 | `			PH7_MemObjInit(pVm,&sPeek);` |
|    96 | 1879 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|    96 | 1880 | `			zType = pType ? ph7_value_to_string(PH7_ValuePeek(pType,&sPeek),&nLen) : "";` |
|    96 | 1881 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|     - | 1882 | `				int fds[2];` |
|    92 | 1883 | `				if( pipe(fds) == 0 ){` |
|    92 | 1884 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|     - | 1885 | `					{` |
|     - | 1886 | `						ph7_value sMode;` |
|     - | 1887 | `						int nMode; const char *zMode;` |
|    92 | 1888 | `						PH7_MemObjInit(pVm,&sMode);` |
|    92 | 1889 | `						zMode = pParam ? ph7_value_to_string(PH7_ValuePeek(pParam,&sMode),&nMode) : "r";` |
|    92 | 1890 | `						pD->kind = 0;` |
|    92 | 1891 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|     - | 1892 | `							/* child writes -> parent reads: child gets write end */` |
|    62 | 1893 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|    62 | 1894 | `						pD->parent_reads = 1;` |
|    31 | 1895 | `						}else{` |
|     - | 1896 | `							/* child reads -> parent writes: child gets read end */` |
|    30 | 1897 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|    30 | 1898 | `						pD->parent_reads = 0;` |
|     - | 1899 | `						}` |
|    92 | 1900 | `						nDesc++;` |
|    92 | 1901 | `						PH7_MemObjRelease(&sMode);` |
|     - | 1902 | `					}` |
|    46 | 1903 | `				}` |
|    50 | 1904 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|     - | 1905 | `				ph7_value sPath, sMode;` |
|     2 | 1906 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|     2 | 1907 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|     2 | 1908 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|     2 | 1909 | `				PH7_MemObjInit(pVm,&sPath);` |
|     2 | 1910 | `				PH7_MemObjInit(pVm,&sMode);` |
|     2 | 1911 | `				zPath = pPath ? ph7_value_to_string(PH7_ValuePeek(pPath,&sPath),&nLen2) : "";` |
|     2 | 1912 | `				zMode = pMode ? ph7_value_to_string(PH7_ValuePeek(pMode,&sMode),&nLen3) : "r";` |
|     2 | 1913 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|   ! 0 | 1914 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|     2 | 1915 | `				pD->kind = 1;` |
|     2 | 1916 | `				pD->file_fd = open(zPath,oflag,0644);` |
|     2 | 1917 | `				nDesc++;` |
|     2 | 1918 | `				PH7_MemObjRelease(&sPath);` |
|     2 | 1919 | `				PH7_MemObjRelease(&sMode);` |
|     3 | 1920 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|     2 | 1921 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|     2 | 1922 | `				pD->kind = 2;` |
|     2 | 1923 | `				pD->redirect_to = pParam ? (int)PH7_ValuePeekInt64(pParam) : 1;` |
|     2 | 1924 | `				nDesc++;` |
|     1 | 1925 | `			}` |
|    96 | 1926 | `			PH7_MemObjRelease(&sPeek);` |
|    48 | 1927 | `		}` |
|    96 | 1928 | `		PH7_MemObjRelease(pEntry);` |
|    96 | 1929 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|    96 | 1930 | `		pNode = pNode->pPrev;` |
|    48 | 1931 | `	}` |
|     - | 1932 | `	/* --- Fork the child --- */` |
|    34 | 1933 | `	pid = fork();` |
|    51 | 1934 | `	if( pid < 0 ){` |
|   ! 0 | 1935 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"fork() failed");` |
|   ! 0 | 1936 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1937 | `		return PH7_OK;` |
|     - | 1938 | `	}` |
|    68 | 1939 | `	if( pid == 0 ){` |
|     - | 1940 | `		/* Child: wire up descriptors then exec */` |
|   130 | 1941 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|    96 | 1942 | `			struct proc_desc *pD = &aDesc[i];` |
|    96 | 1943 | `			if( pD->kind == 0 ){` |
|    92 | 1944 | `				dup2(pD->child_end,pD->child_fd);` |
|    92 | 1945 | `				close(pD->parent_end);` |
|    92 | 1946 | `				close(pD->child_end);` |
|    50 | 1947 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|     2 | 1948 | `				dup2(pD->file_fd,pD->child_fd);` |
|     2 | 1949 | `				close(pD->file_fd);` |
|     1 | 1950 | `			}` |
|    48 | 1951 | `		}` |
|     - | 1952 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|   130 | 1953 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|    96 | 1954 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|    48 | 1955 | `		}` |
|    34 | 1956 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|    34 | 1957 | `		if( azEnv ){` |
|   ! 0 | 1958 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|   ! 0 | 1959 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|   ! 0 | 1960 | `		}else{` |
|    34 | 1961 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|     2 | 1962 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|     - | 1963 | `		}` |
|    17 | 1964 | `		_exit(127); /* exec failed */` |
|     - | 1965 | `	}` |
|     - | 1966 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|    34 | 1967 | `	pPipes = ph7_context_new_array(pCtx);` |
|   130 | 1968 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|    96 | 1969 | `		struct proc_desc *pD = &aDesc[i];` |
|    96 | 1970 | `		if( pD->kind == 0 ){` |
|     - | 1971 | `			io_private *pEnd;` |
|     - | 1972 | `			ph7_value *pRes;` |
|    92 | 1973 | `			close(pD->child_end);` |
|    92 | 1974 | `			pEnd = ProcWrapFd(pVm,pD->parent_end,pD->parent_reads);` |
|    92 | 1975 | `			pRes = ph7_context_new_scalar(pCtx);` |
|    92 | 1976 | `			if( pEnd && pRes && pPipes ){` |
|    92 | 1977 | `				ph7_value_resource(pRes,pEnd);` |
|    92 | 1978 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|    46 | 1979 | `			}` |
|    92 | 1980 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|    50 | 1981 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|     2 | 1982 | `			close(pD->file_fd);` |
|     1 | 1983 | `		}` |
|    48 | 1984 | `	}` |
|    34 | 1985 | `	if( pPipes ){` |
|    34 | 1986 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|    17 | 1987 | `	}` |
|     - | 1988 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|    35 | 1989 | `	if( azArgv ){` |
|   126 | 1990 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|    32 | 1991 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|    16 | 1992 | `	}` |
|    49 | 1993 | `	if( azEnv ){` |
|   ! 0 | 1994 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|   ! 0 | 1995 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|   ! 0 | 1996 | `	}` |
|     - | 1997 | `	/* Build the process resource */` |
|    34 | 1998 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|    34 | 1999 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    34 | 2000 | `	SyZero(pProc,sizeof(proc_private));` |
|    34 | 2001 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|    34 | 2002 | `	pProc->pid = (int)pid;` |
|    34 | 2003 | `	pProc->running = 1;` |
|    34 | 2004 | `	pProc->exit_code = 0;` |
|    34 | 2005 | `	ph7_result_resource(pCtx,pProc);` |
|    17 | 2006 | `	(void)rc;` |
|    34 | 2007 | `	return PH7_OK;` |
|    17 | 2008 | `}` |
|     - | 2009 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|    34 | 2010 | `static void ProcReap(proc_private *pProc,int block)` |
|     - | 2011 | `{` |
|    34 | 2012 | `	int status = 0;` |
|     - | 2013 | `	pid_t r;` |
|    34 | 2014 | `	if( !pProc->running ){ return; }` |
|    34 | 2015 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|    34 | 2016 | `	if( r == (pid_t)pProc->pid ){` |
|    34 | 2017 | `		pProc->running = 0;` |
|    34 | 2018 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|    18 | 2019 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|    17 | 2020 | `	}` |
|    17 | 2021 | `}` |
|    34 | 2022 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2023 | `{` |
|     - | 2024 | `	proc_private *pProc;` |
|    34 | 2025 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 2026 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 2027 | `		return PH7_OK;` |
|     - | 2028 | `	}` |
|    34 | 2029 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    34 | 2030 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|   ! 0 | 2031 | `		ph7_result_int(pCtx,-1);` |
|   ! 0 | 2032 | `		return PH7_OK;` |
|     - | 2033 | `	}` |
|    34 | 2034 | `	ProcReap(pProc,1/*block until it exits*/);` |
|    34 | 2035 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|    34 | 2036 | `	return PH7_OK;` |
|    17 | 2037 | `}` |
|    18 | 2038 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2039 | `{` |
|     - | 2040 | `	proc_private *pProc;` |
|    18 | 2041 | `	int sig = 15; /* SIGTERM */` |
|    18 | 2042 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 2043 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2044 | `		return PH7_OK;` |
|     - | 2045 | `	}` |
|    18 | 2046 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|    18 | 2047 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|   ! 0 | 2048 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2049 | `		return PH7_OK;` |
|     - | 2050 | `	}` |
|    18 | 2051 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|    18 | 2052 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|    18 | 2053 | `	ph7_result_bool(pCtx,1);` |
|    18 | 2054 | `	return PH7_OK;` |
|     9 | 2055 | `}` |
|   ! 0 | 2056 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - | 2057 | `{` |
|     - | 2058 | `	proc_private *pProc;` |
|     - | 2059 | `	ph7_value *pArray, *pVal;` |
|   ! 0 | 2060 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 2061 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2062 | `		return PH7_OK;` |
|     - | 2063 | `	}` |
|   ! 0 | 2064 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|   ! 0 | 2065 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|   ! 0 | 2066 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2067 | `		return PH7_OK;` |
|     - | 2068 | `	}` |
|   ! 0 | 2069 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|   ! 0 | 2070 | `	pArray = ph7_context_new_array(pCtx);` |
|   ! 0 | 2071 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 2072 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   ! 0 | 2073 | `	ph7_value_int(pVal,pProc->pid);` |
|   ! 0 | 2074 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|   ! 0 | 2075 | `	ph7_value_bool(pVal,pProc->running);` |
|   ! 0 | 2076 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|   ! 0 | 2077 | `	ph7_value_bool(pVal,0);` |
|   ! 0 | 2078 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|   ! 0 | 2079 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|   ! 0 | 2080 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|   ! 0 | 2081 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|   ! 0 | 2082 | `	ph7_value_int(pVal,0);` |
|   ! 0 | 2083 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|   ! 0 | 2084 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|   ! 0 | 2085 | `	ph7_context_release_value(pCtx,pVal);` |
|   ! 0 | 2086 | `	ph7_result_value(pCtx,pArray);` |
|   ! 0 | 2087 | `	return PH7_OK;` |
|   ! 0 | 2088 | `}` |
|     - | 2089 | `#else /* !__UNIXES__ */` |
|     - | 2090 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2091 | `{` |
|     - | 2092 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|   ! 0 | 2093 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|   ! 0 | 2094 | `	ph7_result_bool(pCtx,0);` |
|   ! 0 | 2095 | `	return PH7_OK;` |
|   ! 0 | 2096 | `}` |
|     - | 2097 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2098 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|     - | 2099 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2100 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     - | 2101 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|   ! 0 | 2102 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|     - | 2103 | `#endif /* __UNIXES__ */` |
|     - | 2104 | `/* Export the php:// stream */` |
|     - | 2105 | `PH7_PRIVATE const ph7_io_stream sPHP_Stream = {` |
|     - | 2106 | `	"php",` |
|     - | 2107 | `	PH7_IO_STREAM_VERSION,` |
|     - | 2108 | `	PHPStreamData_Open,  /* xOpen */` |
|     - | 2109 | `	0,   /* xOpenDir */` |
|     - | 2110 | `	PHPStreamData_Close, /* xClose */` |
|     - | 2111 | `	0,  /* xCloseDir */` |
|     - | 2112 | `	PHPStreamData_Read,  /* xRead */` |
|     - | 2113 | `	0,  /* xReadDir */` |
|     - | 2114 | `	PHPStreamData_Write, /* xWrite */` |
|     - | 2115 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|     - | 2116 | `	0,  /* xLock */` |
|     - | 2117 | `	0,  /* xRewindDir */` |
|     - | 2118 | `	PHPStreamData_Tell,  /* xTell */` |
|     - | 2119 | `	PHPStreamData_Trunc, /* xTrunc */` |
|     - | 2120 | `	0,  /* xSync */` |
|     - | 2121 | `	0   /* xStat */` |
|     - | 2122 | `};` |
|     - | 2123 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     - | 2124 | `/*` |
|     - | 2125 | ` * Return TRUE if we are dealing with the php:// stream.` |
|     - | 2126 | ` * FALSE otherwise.` |
|     - | 2127 | ` */` |
|     - | 2128 | `/*` |
|     - | 2129 | ` * The handle a php://filter proxy WRAPS, or 0 for any other php:// stream. The` |
|     - | 2130 | ` * proxy is not a stream of its own — the position, the descriptor, the stat and` |
|     - | 2131 | ` * the lock all belong to the stream underneath — so everything that asks the` |
|     - | 2132 | ` * device such a question has to go through here first.` |
|     - | 2133 | ` */` |
|    52 | 2134 | `PH7_PRIVATE io_private * PH7_PhpStreamInner(void *pHandle)` |
|     1 | 2135 | `{` |
|     - | 2136 | `#ifndef PH7_DISABLE_DISK_IO` |
|    53 | 2137 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    53 | 2138 | `	if( pData && pData->iType == PH7_IO_STREAM_FILTER ){` |
|    15 | 2139 | `		return pData->pInner;` |
|     - | 2140 | `	}` |
|     - | 2141 | `#else` |
|     - | 2142 | `	SXUNUSED(pHandle);` |
|     - | 2143 | `#endif` |
|    39 | 2144 | `	return 0;` |
|    27 | 2145 | `}` |
|  8471 | 2146 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream)` |
|     5 | 2147 | `{` |
|     - | 2148 | `#ifndef PH7_DISABLE_DISK_IO` |
|  8476 | 2149 | `	return pStream == &sPHP_Stream;` |
|     - | 2150 | `#else` |
|     - | 2151 | `	SXUNUSED(pStream); /* cc warning */` |
|     - | 2152 | `	return 0;` |
|     - | 2153 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2154 | `}` |
|     - | 2155 | `/*` |
|     - | 2156 | ` * Is this a handle php's plain-files device would own -- a file, a pipe, a` |
|     - | 2157 | ` * standard stream? php sets the blocking mode on those with O_NONBLOCK, which` |
|     - | 2158 | ` * Windows does not have, so there stream_set_blocking() answers FALSE for them.` |
|     - | 2159 | ` */` |
|   ! 0 | 2160 | `PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev)` |
|     1 | 2161 | `{` |
|     - | 2162 | `#ifndef PH7_DISABLE_DISK_IO` |
|     1 | 2163 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| pDev->pStream == 0 \|\| pDev->bDir ){` |
|   ! 0 | 2164 | `		return 0;` |
|     - | 2165 | `	}` |
|     - | 2166 | `#ifdef __WINNT__` |
|     1 | 2167 | `	if( pDev->pStream == &sWinFileStream ){` |
|     1 | 2168 | `		return 1;` |
|     - | 2169 | `	}` |
|     - | 2170 | `#elif defined(__UNIXES__)` |
|   ! 0 | 2171 | `	if( pDev->pStream == &sUnixFileStream ){` |
|   ! 0 | 2172 | `		return 1;` |
|     - | 2173 | `	}` |
|     - | 2174 | `#endif` |
|     1 | 2175 | `	if( pDev->pStream == &sPipe_Stream ){` |
|   ! 0 | 2176 | `		return 1;` |
|     - | 2177 | `	}` |
|     1 | 2178 | `	if( is_php_stream(pDev->pStream) ){` |
|     1 | 2179 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     1 | 2180 | `		return pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|   ! 0 | 2181 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR;` |
|     - | 2182 | `	}` |
|     1 | 2183 | `	return 0;` |
|     - | 2184 | `#else` |
|     - | 2185 | `	SXUNUSED(pDev); /* cc warning */` |
|     - | 2186 | `	return 0;` |
|     - | 2187 | `#endif` |
|     1 | 2188 | `}` |
|     - | 2189 | `/*` |
|     - | 2190 | ` * The POSIX descriptor behind an open handle, or -1 when there is none.` |
|     - | 2191 | ` * php applies blocking mode and timeouts AT the descriptor, so a stream that` |
|     - | 2192 | ` * has no fd — a memory buffer, a data:// payload, a userland wrapper, and` |
|     - | 2193 | ` * every file on Windows, where the device carries a HANDLE — is exactly the` |
|     - | 2194 | ` * set php answers "unsupported" for.` |
|     - | 2195 | ` */` |
|   116 | 2196 | `PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev)` |
|     2 | 2197 | `{` |
|     - | 2198 | `#if !defined(__WINNT__) && !defined(PH7_DISABLE_DISK_IO)` |
|     - | 2199 | `	/* A php://filter handle has no descriptor of its own; the stream it wraps` |
|     - | 2200 | `	 * does, and that is the one blocking mode and locking apply to. */` |
|   116 | 2201 | `	pDev = PH7_StreamUnwrap(pDev);` |
|   116 | 2202 | `	if( pDev == 0 \|\| pDev->pHandle == 0 \|\| pDev->pStream == 0 \|\| pDev->bDir ){` |
|     - | 2203 | `		/* A DIRECTORY handle rides the same ops table as a file and stores a` |
|     - | 2204 | `		 * DIR* where a file stores its descriptor, so reading one as the other` |
|     - | 2205 | `		 * hands fcntl()/lseek() a truncated heap pointer — an arbitrary fd` |
|     - | 2206 | `		 * number belonging to something else in this process. */` |
|   ! 0 | 2207 | `		return -1;` |
|     - | 2208 | `	}` |
|   116 | 2209 | `	if( pDev->pStream == &sUnixFileStream ){` |
|    60 | 2210 | `		return SX_PTR_TO_INT(pDev->pHandle);` |
|     - | 2211 | `	}` |
|    56 | 2212 | `	if( pDev->pStream == &sPipe_Stream ){` |
|   ! 0 | 2213 | `		pipe_private *pPipe = (pipe_private *)pDev->pHandle;` |
|   ! 0 | 2214 | `		return pPipe->pFile ? fileno(pPipe->pFile) : -1;` |
|     - | 2215 | `	}` |
|    56 | 2216 | `	if( is_php_stream(pDev->pStream) ){` |
|    30 | 2217 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|    30 | 2218 | `		if( pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|    29 | 2219 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR ){` |
|     4 | 2220 | `			return SX_PTR_TO_INT(pData->x.pHandle);` |
|     - | 2221 | `		}` |
|    13 | 2222 | `	}` |
|    52 | 2223 | `	return -1;` |
|     - | 2224 | `#else` |
|     - | 2225 | `	SXUNUSED(pDev); /* cc warning */` |
|     2 | 2226 | `	return -1;` |
|     - | 2227 | `#endif` |
|    60 | 2228 | `}` |
|     - | 2229 | `/*` |
|     - | 2230 | `` * Can this handle report a POSITION? php's `seekable` is a fact about what the`` |
|     - | 2231 | ` * handle sits on, not about what the device could do — php://stdout is seekable` |
|     - | 2232 | ` * into a file and not down a pipe — and the descriptor is the only thing that` |
|     - | 2233 | ` * knows. Answers 1 (yes), 0 (no) or -1 (nothing here can tell; the caller falls` |
|     - | 2234 | ` * back on the device's own xTell).` |
|     - | 2235 | ` */` |
|    64 | 2236 | `PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev)` |
|     3 | 2237 | `{` |
|     - | 2238 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2239 | `#ifdef __WINNT__` |
|     - | 2240 | `	/* The file devices carry a HANDLE rather than a descriptor here, and only` |
|     - | 2241 | `	 * the php:// standard streams hold one this can ask. */` |
|     3 | 2242 | `	if( pDev && pDev->pHandle && is_php_stream(pDev->pStream) ){` |
|     3 | 2243 | `		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     - | 2244 | `		if( pData->iType == PH7_IO_STREAM_STDIN \|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|     3 | 2245 | `		 \|\| pData->iType == PH7_IO_STREAM_STDERR ){` |
|     - | 2246 | `			LARGE_INTEGER zero,pos;` |
|     1 | 2247 | `			zero.QuadPart = 0;` |
|     1 | 2248 | `			return SetFilePointerEx((HANDLE)pData->x.pHandle,zero,&pos,FILE_CURRENT) ? 1 : 0;` |
|     - | 2249 | `		}` |
|     - | 2250 | `	}` |
|     2 | 2251 | `	return -1;` |
|     - | 2252 | `#else` |
|    64 | 2253 | `	int fd = PH7_StreamPosixFd(pDev);` |
|    64 | 2254 | `	if( fd < 0 ){` |
|    34 | 2255 | `		return -1;` |
|     - | 2256 | `	}` |
|    30 | 2257 | `	return lseek(fd,0,SEEK_CUR) == (off_t)-1 ? 0 : 1;` |
|     - | 2258 | `#endif` |
|     - | 2259 | `#else` |
|     - | 2260 | `	SXUNUSED(pDev); /* cc warning */` |
|     - | 2261 | `	return -1;` |
|     - | 2262 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    35 | 2263 | `}` |
|     - | 2264 | `/*` |
|     - | 2265 | ` * Which php:// sub-stream a handle opened. stream_get_meta_data() has to tell` |
|     - | 2266 | ` * MEMORY, TEMP and STDIO apart and only the device's own private state knows;` |
|     - | 2267 | ` * everything else answers 0.` |
|     - | 2268 | ` */` |
|   932 | 2269 | `PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle)` |
|     5 | 2270 | `{` |
|     - | 2271 | `#ifndef PH7_DISABLE_DISK_IO` |
|   937 | 2272 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   937 | 2273 | `	return pData ? pData->iType : 0;` |
|     - | 2274 | `#else` |
|     - | 2275 | `	SXUNUSED(pHandle); /* cc warning */` |
|     - | 2276 | `	return 0;` |
|     - | 2277 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2278 | `}` |
|     - | 2279 | `/*` |
|     - | 2280 | ` * Has a php://temp handle just handed out its LAST byte? php's temp stream sits` |
|     - | 2281 | ` * over a memory one and copies that one's eof after every read, and the inner` |
|     - | 2282 | ` * stream raises it as soon as a fill has consumed the buffer -- so a temp` |
|     - | 2283 | ` * handle answers feof() true one read before a bare php://memory does, and` |
|     - | 2284 | `` * `foreach` over an SplTempFileObject stops one row earlier. Answers 0 for`` |
|     - | 2285 | ` * every other device, php://memory included.` |
|     - | 2286 | ` */` |
|   186 | 2287 | `PH7_PRIVATE int PH7_PhpStreamTempDrained(void *pHandle)` |
|     4 | 2288 | `{` |
|     - | 2289 | `#ifndef PH7_DISABLE_DISK_IO` |
|   190 | 2290 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   192 | 2291 | `	return pData && pData->bTemp && pData->iType == PH7_IO_STREAM_MEMORY` |
|   279 | 2292 | `		&& pData->nCur >= SyBlobLength(&pData->sMem);` |
|     - | 2293 | `#else` |
|     - | 2294 | `	SXUNUSED(pHandle); /* cc warning */` |
|     - | 2295 | `	return 0;` |
|     - | 2296 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     4 | 2297 | `}` |
|     - | 2298 | `/*` |
|     - | 2299 | ` * Return TRUE if we are dealing with the data:// stream.` |
|     - | 2300 | ` */` |
|  1242 | 2301 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream)` |
|     5 | 2302 | `{` |
|     - | 2303 | `#ifndef PH7_DISABLE_DISK_IO` |
|  1247 | 2304 | `	return pStream == &sDATA_Stream;` |
|     - | 2305 | `#else` |
|     - | 2306 | `	SXUNUSED(pStream); /* cc warning */` |
|     - | 2307 | `	return 0;` |
|     - | 2308 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     5 | 2309 | `}` |
|     - | 2310 | `/*` |
|     - | 2311 | ` * bool stream_isatty(resource $stream)` |
|     - | 2312 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|     - | 2313 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|     - | 2314 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|     - | 2315 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|     - | 2316 | ` */` |
|     6 | 2317 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2318 | `{` |
|     7 | 2319 | `	int bTty = 0;` |
|     7 | 2320 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|   ! 0 | 2321 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2322 | `		return PH7_OK;` |
|     - | 2323 | `	}` |
|     - | 2324 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - | 2325 | `	{` |
|     7 | 2326 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|     7 | 2327 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|     5 | 2328 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|     6 | 2329 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|     4 | 2330 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|     3 | 2331 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|     - | 2332 | `#ifdef __WINNT__` |
|     1 | 2333 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|     - | 2334 | `#else` |
|     4 | 2335 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|     - | 2336 | `#endif` |
|     2 | 2337 | `			}` |
|     2 | 2338 | `		}` |
|     - | 2339 | `	}` |
|     - | 2340 | `#endif /* PH7_DISABLE_DISK_IO */` |
|     7 | 2341 | `	ph7_result_bool(pCtx,bTty);` |
|     7 | 2342 | `	return PH7_OK;` |
|     4 | 2343 | `}` |
|     - | 2344 |  |
|     - | 2345 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2346 | `/*` |
|     - | 2347 | ` * Export the STDIN handle.` |
|     - | 2348 | ` */` |
|   144 | 2349 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|     3 | 2350 | `{` |
|     - | 2351 | `#ifndef PH7_DISABLE_DISK_IO` |
|   147 | 2352 | `	if( pVm->pStdin == 0  ){` |
|     - | 2353 | `		io_private *pIn;` |
|     - | 2354 | `		/* Allocate an IO private instance */` |
|    11 | 2355 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    11 | 2356 | `		if( pIn == 0 ){` |
|   ! 0 | 2357 | `			return 0;` |
|     - | 2358 | `		}` |
|    11 | 2359 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|    11 | 2360 | `		SetIOPrivateOpenedAs(pIn,"php://stdin",(int)sizeof("php://stdin")-1,"rb",2);` |
|     - | 2361 | `		/* Initialize the handle */` |
|    11 | 2362 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|     - | 2363 | `		/* Install the STDIN stream */` |
|    11 | 2364 | `		pVm->pStdin = pIn;` |
|    11 | 2365 | `		return pIn;` |
|   ! 0 | 2366 | `	}else{` |
|     - | 2367 | `		/* NULL or STDIN */` |
|   139 | 2368 | `		return pVm->pStdin;` |
|     - | 2369 | `	}` |
|     - | 2370 | `#else` |
|     - | 2371 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2372 | `	return 0;` |
|     - | 2373 | `#endif` |
|    75 | 2374 | `}` |
|     - | 2375 | `/*` |
|     - | 2376 | ` * Export the STDOUT handle.` |
|     - | 2377 | ` */` |
|   134 | 2378 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|     4 | 2379 | `{` |
|     - | 2380 | `#ifndef PH7_DISABLE_DISK_IO` |
|   138 | 2381 | `	if( pVm->pStdout == 0  ){` |
|     - | 2382 | `		io_private *pOut;` |
|     - | 2383 | `		/* Allocate an IO private instance */` |
|    16 | 2384 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    16 | 2385 | `		if( pOut == 0 ){` |
|   ! 0 | 2386 | `			return 0;` |
|     - | 2387 | `		}` |
|    16 | 2388 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|    16 | 2389 | `		SetIOPrivateOpenedAs(pOut,"php://stdout",(int)sizeof("php://stdout")-1,"wb",2);` |
|     - | 2390 | `		/* Initialize the handle */` |
|    16 | 2391 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|     - | 2392 | `		/* Install the STDOUT stream */` |
|    16 | 2393 | `		pVm->pStdout = pOut;` |
|    16 | 2394 | `		return pOut;` |
|   ! 0 | 2395 | `	}else{` |
|     - | 2396 | `		/* NULL or STDOUT */` |
|   125 | 2397 | `		return pVm->pStdout;` |
|     - | 2398 | `	}` |
|     - | 2399 | `#else` |
|     - | 2400 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2401 | `	return 0;` |
|     - | 2402 | `#endif` |
|    71 | 2403 | `}` |
|     - | 2404 | `/*` |
|     - | 2405 | ` * Export the STDERR handle.` |
|     - | 2406 | ` */` |
|   136 | 2407 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|     4 | 2408 | `{` |
|     - | 2409 | `#ifndef PH7_DISABLE_DISK_IO` |
|   140 | 2410 | `	if( pVm->pStderr == 0  ){` |
|     - | 2411 | `		io_private *pErr;` |
|     - | 2412 | `		/* Allocate an IO private instance */` |
|    18 | 2413 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    18 | 2414 | `		if( pErr == 0 ){` |
|   ! 0 | 2415 | `			return 0;` |
|     - | 2416 | `		}` |
|    18 | 2417 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|    18 | 2418 | `		SetIOPrivateOpenedAs(pErr,"php://stderr",(int)sizeof("php://stderr")-1,"wb",2);` |
|     - | 2419 | `		/* Initialize the handle */` |
|    18 | 2420 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|     - | 2421 | `		/* Install the STDERR stream */` |
|    18 | 2422 | `		pVm->pStderr = pErr;` |
|    18 | 2423 | `		return pErr;` |
|   ! 0 | 2424 | `	}else{` |
|     - | 2425 | `		/* NULL or STDERR */` |
|   125 | 2426 | `		return pVm->pStderr;` |
|     - | 2427 | `	}` |
|     - | 2428 | `#else` |
|     - | 2429 | `	SXUNUSED(pVm); /* cc warning */` |
|     - | 2430 | `	return 0;` |
|     - | 2431 | `#endif` |
|    72 | 2432 | `}` |
|     - | 2433 |  |
