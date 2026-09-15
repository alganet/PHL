# src/ph7/vfs_io_driver.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 539/836 lines (64.47%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|    - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    5 | ` */` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#include <stdio.h>` |
|    - |    8 | `#include <errno.h>` |
|    - |    9 | `#include <string.h>` |
|    - |   10 |  |
|    - |   11 | `#ifdef __UNIXES__` |
|    - |   12 | `#include <unistd.h>` |
|    - |   13 | `#include <sys/wait.h>` |
|    - |   14 | `#include <fcntl.h>` |
|    - |   15 | `#include <signal.h>` |
|    - |   16 | `#endif` |
|    - |   17 | `/*` |
|    - |   18 | ` * Section:` |
|    - |   19 | ` *    Built-in IO stream drivers: php://, data://, pipe (popen) and the` |
|    - |   20 | ` *    standard stream exporters. The file:// driver lives in` |
|    - |   21 | ` *    vfs_unix.c/vfs_win.c; vfs.c owns registration.` |
|    - |   22 | ` * Status:` |
|    - |   23 | ` *    Stable.` |
|    - |   24 | ` */` |
|    - |   25 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|    - |   26 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - |   27 | `/*` |
|    - |   28 | ` * The following defines are mostly used by the UNIX built and have` |
|    - |   29 | ` * no particular meaning on windows.` |
|    - |   30 | ` */` |
|    - |   31 | `#ifndef STDIN_FILENO` |
|    - |   32 | `#define STDIN_FILENO	0` |
|    - |   33 | `#endif` |
|    - |   34 | `#ifndef STDOUT_FILENO` |
|    - |   35 | `#define STDOUT_FILENO	1` |
|    - |   36 | `#endif` |
|    - |   37 | `#ifndef STDERR_FILENO` |
|    - |   38 | `#define STDERR_FILENO	2` |
|    - |   39 | `#endif` |
|    - |   40 | `/*` |
|    - |   41 | ` * php:// Accessing various I/O streams` |
|    - |   42 | ` * According to the PHP langage reference manual` |
|    - |   43 | ` * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input` |
|    - |   44 | ` * and output streams, the standard input, output and error file descriptors.` |
|    - |   45 | ` * php://stdin, php://stdout and php://stderr:` |
|    - |   46 | ` *  Allow direct access to the corresponding input or output stream of the PHP process.` |
|    - |   47 | ` *  The stream references a duplicate file descriptor, so if you open php://stdin and later` |
|    - |   48 | ` *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.` |
|    - |   49 | ` *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.` |
|    - |   50 | ` * php://output` |
|    - |   51 | ` *  php://output is a write-only stream that allows you to write to the output buffer` |
|    - |   52 | ` *  mechanism in the same way as print and echo.` |
|    - |   53 | ` */` |
|    - |   54 | `typedef struct ph7_stream_data ph7_stream_data;` |
|    - |   55 | `/* Supported IO streams */` |
|    - |   56 | `#define PH7_IO_STREAM_STDIN  1 /* php://stdin */` |
|    - |   57 | `#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */` |
|    - |   58 | `#define PH7_IO_STREAM_STDERR 3 /* php://stderr */` |
|    - |   59 | `#define PH7_IO_STREAM_OUTPUT 4 /* php://output */` |
|    - |   60 | `#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */` |
|    - |   61 | ` /* The following structure is the private data associated with the php:// stream */` |
|    - |   62 | `struct ph7_stream_data` |
|    - |   63 | `{` |
|    - |   64 | `	ph7_vm *pVm; /* VM that own this instance */` |
|    - |   65 | `	int iType;   /* Stream type */` |
|    - |   66 | `	union{` |
|    - |   67 | `		void *pHandle; /* Stream handle */` |
|    - |   68 | `		ph7_output_consumer sConsumer; /* VM output consumer */` |
|    - |   69 | `	}x;` |
|    - |   70 | `	SyBlob sMem;     /* MEMORY type: backing buffer */` |
|    - |   71 | `	sxu32 nCur;      /* MEMORY type: read/write cursor */` |
|    - |   72 | `	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */` |
|    - |   73 | `};` |
|    - |   74 | `/*` |
|    - |   75 | ` * Allocate a new instance of the ph7_stream_data structure.` |
|    - |   76 | ` */` |
|   40 |   77 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|    1 |   78 | `{` |
|    - |   79 | `	ph7_stream_data *pData;` |
|   41 |   80 | `	if( pVm == 0 ){` |
|  ! 0 |   81 | `		return 0;` |
|    - |   82 | `	}` |
|    - |   83 | `	/* Allocate a new instance */` |
|   41 |   84 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|   41 |   85 | `	if( pData == 0 ){` |
|  ! 0 |   86 | `		return 0;` |
|    - |   87 | `	}` |
|    - |   88 | `	/* Zero the structure */` |
|   41 |   89 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|    - |   90 | `	/* Initialize fields */` |
|   41 |   91 | `	pData->iType = iType;` |
|   41 |   92 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|   41 |   93 | `	pData->nCur = 0;` |
|   41 |   94 | `	pData->bReadOnly = 0;` |
|   41 |   95 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|    - |   96 | `		/* Nothing else to set up: the buffer is the stream */` |
|   30 |   97 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
|    - |   98 | `		/* Point to the default VM consumer routine. */` |
|    3 |   99 | `		pData->x.sConsumer = pVm->sVmConsumer;` |
|    2 |  100 | `	}else{` |
|    - |  101 | `#ifdef __WINNT__` |
|    - |  102 | `		DWORD nChannel;` |
|    1 |  103 | `		switch(iType){` |
|    1 |  104 | `		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;` |
|    1 |  105 | `		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;` |
|    - |  106 | `		default:` |
|    1 |  107 | `			nChannel = STD_INPUT_HANDLE;` |
|    - |  108 | `			break;` |
|    - |  109 | `		}` |
|    1 |  110 | `		pData->x.pHandle = GetStdHandle(nChannel);` |
|    - |  111 | `#else` |
|    - |  112 | `		/* Assume an UNIX system */` |
|   16 |  113 | `		int ifd = STDIN_FILENO;` |
|   16 |  114 | `		switch(iType){` |
|    6 |  115 | `		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;` |
|    8 |  116 | `		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;` |
|    1 |  117 | `		default:` |
|    2 |  118 | `			break;` |
|    - |  119 | `		}` |
|   16 |  120 | `		pData->x.pHandle = SX_INT_TO_PTR(ifd);` |
|    - |  121 | `#endif` |
|    - |  122 | `	}` |
|   41 |  123 | `	pData->pVm = pVm;` |
|   41 |  124 | `	return pData;` |
|   21 |  125 | `}` |
|    - |  126 | `/*` |
|    - |  127 | ` * Implementation of the php:// IO streams routines` |
|    - |  128 | ` * Status:` |
|    - |  129 | ` *   Stable.` |
|    - |  130 | ` */` |
|    - |  131 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|   14 |  132 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    1 |  133 | `{` |
|    - |  134 | `	ph7_stream_data *pData;` |
|    - |  135 | `	SyString sStream;` |
|   15 |  136 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|    - |  137 | `	/* Trim leading and trailing white spaces */` |
|   15 |  138 | `	SyStringFullTrim(&sStream);` |
|    - |  139 | `	/* Stream to open */` |
|   15 |  140 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|  ! 0 |  141 | `		iMode = PH7_IO_STREAM_STDIN;` |
|   15 |  142 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|    3 |  143 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|   14 |  144 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|  ! 0 |  145 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|   13 |  146 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|  ! 0 |  147 | `		iMode = PH7_IO_STREAM_STDERR;` |
|   12 |  148 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|    8 |  149 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|    - |  150 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|    - |  151 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|   13 |  152 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|    7 |  153 | `	}else{` |
|    - |  154 | `		/* unknown stream name */` |
|  ! 0 |  155 | `		return -1;` |
|    - |  156 | `	}` |
|    - |  157 | `	/* Create our handle */` |
|   15 |  158 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|   15 |  159 | `	if( pData == 0 ){` |
|  ! 0 |  160 | `		return -1;` |
|    - |  161 | `	}` |
|    - |  162 | `	/* Make the handle public */` |
|   15 |  163 | `	*ppHandle = (void *)pData;` |
|   15 |  164 | `	return PH7_OK;` |
|    8 |  165 | `}` |
|    - |  166 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|   42 |  167 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|    1 |  168 | `{` |
|   43 |  169 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   43 |  170 | `	if( pData == 0 ){` |
|  ! 0 |  171 | `		return -1;` |
|    - |  172 | `	}` |
|   43 |  173 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|   43 |  174 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|    - |  175 | `		sxu32 nRead;` |
|   43 |  176 | `		if( pData->nCur >= nAvail ){` |
|   15 |  177 | `			return 0; /* EOF */` |
|    - |  178 | `		}` |
|   29 |  179 | `		nRead = nAvail - pData->nCur;` |
|   29 |  180 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|    7 |  181 | `			nRead = (sxu32)nDatatoRead;` |
|    3 |  182 | `		}` |
|   29 |  183 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|   29 |  184 | `		pData->nCur += nRead;` |
|   29 |  185 | `		return (ph7_int64)nRead;` |
|    - |  186 | `	}` |
|  ! 0 |  187 | `	if( pData->iType != PH7_IO_STREAM_STDIN ){` |
|    - |  188 | `		/* Forbidden */` |
|  ! 0 |  189 | `		return -1;` |
|    - |  190 | `	}` |
|    - |  191 | `#ifdef __WINNT__` |
|    - |  192 | `	{` |
|    - |  193 | `		DWORD nRd;` |
|    - |  194 | `		BOOL rc;` |
|  ! 0 |  195 | `		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);` |
|  ! 0 |  196 | `		if( !rc ){` |
|    - |  197 | `			/* IO error */` |
|  ! 0 |  198 | `			return -1;` |
|    - |  199 | `		}` |
|  ! 0 |  200 | `		return (ph7_int64)nRd;` |
|    - |  201 | `	}` |
|    - |  202 | `#elif defined(__UNIXES__)` |
|    - |  203 | `	{` |
|    - |  204 | `		ssize_t nRd;` |
|    - |  205 | `		int fd;` |
|  ! 0 |  206 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|  ! 0 |  207 | `		nRd = read(fd,pBuffer,(size_t)nDatatoRead);` |
|  ! 0 |  208 | `		if( nRd < 1 ){` |
|  ! 0 |  209 | `			return -1;` |
|    - |  210 | `		}` |
|  ! 0 |  211 | `		return (ph7_int64)nRd;` |
|    - |  212 | `	}` |
|    - |  213 | `#else` |
|    - |  214 | `	return -1;` |
|    - |  215 | `#endif` |
|   22 |  216 | `}` |
|    - |  217 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|   22 |  218 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    1 |  219 | `{` |
|   23 |  220 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   23 |  221 | `	if( pData == 0 ){` |
|  ! 0 |  222 | `		return -1;` |
|    - |  223 | `	}` |
|   23 |  224 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|    - |  225 | `		/* Forbidden */` |
|  ! 0 |  226 | `		return -1;` |
|   23 |  227 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|    - |  228 | `		sxu32 nLen,nEnd;` |
|   11 |  229 | `		if( pData->bReadOnly ){` |
|  ! 0 |  230 | `			return -1;` |
|    - |  231 | `		}` |
|   11 |  232 | `		nLen = SyBlobLength(&pData->sMem);` |
|   11 |  233 | `		if( pData->nCur > nLen ){` |
|    - |  234 | `			/* seek past end: php zero-fills the gap */` |
|    - |  235 | `			static const char zZero[64] = {0};` |
|  ! 0 |  236 | `			while( SyBlobLength(&pData->sMem) < pData->nCur ){` |
|  ! 0 |  237 | `				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);` |
|  ! 0 |  238 | `				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|  ! 0 |  239 | `				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|  ! 0 |  240 | `					return -1;` |
|    - |  241 | `				}` |
|  ! 0 |  242 | `			}` |
|  ! 0 |  243 | `			nLen = SyBlobLength(&pData->sMem);` |
|  ! 0 |  244 | `		}` |
|   11 |  245 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|   11 |  246 | `		if( pData->nCur < nLen ){` |
|    - |  247 | `			/* overwrite in place up to the current end */` |
|    3 |  248 | `			sxu32 nOver = nLen - pData->nCur;` |
|    3 |  249 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|    4 |  250 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|    4 |  251 | `			if( nEnd > nLen ){` |
|  ! 0 |  252 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|  ! 0 |  253 | `					return -1;` |
|    - |  254 | `				}` |
|  ! 0 |  255 | `			}` |
|    2 |  256 | `		}else{` |
|    9 |  257 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|  ! 0 |  258 | `				return -1;` |
|    - |  259 | `			}` |
|    - |  260 | `		}` |
|   11 |  261 | `		pData->nCur = nEnd;` |
|   11 |  262 | `		return nWrite;` |
|   13 |  263 | `	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){` |
|    3 |  264 | `		ph7_output_consumer *pCons = &pData->x.sConsumer;` |
|    - |  265 | `		int rc;` |
|    - |  266 | `		/* Call the vm output consumer */` |
|    3 |  267 | `		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);` |
|    3 |  268 | `		if( rc == PH7_ABORT ){` |
|  ! 0 |  269 | `			return -1;` |
|    - |  270 | `		}` |
|    3 |  271 | `		return nWrite;` |
|    - |  272 | `	}` |
|    - |  273 | `#ifdef __WINNT__` |
|    - |  274 | `	{` |
|    - |  275 | `		DWORD nWr;` |
|    - |  276 | `		BOOL rc;` |
|  ! 0 |  277 | `		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);` |
|  ! 0 |  278 | `		if( !rc ){` |
|    - |  279 | `			/* IO error */` |
|  ! 0 |  280 | `			return -1;` |
|    - |  281 | `		}` |
|  ! 0 |  282 | `		return (ph7_int64)nWr;` |
|    - |  283 | `	}` |
|    - |  284 | `#elif defined(__UNIXES__)` |
|    - |  285 | `	{` |
|    - |  286 | `		ssize_t nWr;` |
|    - |  287 | `		int fd;` |
|   10 |  288 | `		fd = SX_PTR_TO_INT(pData->x.pHandle);` |
|   10 |  289 | `		nWr = write(fd,pBuf,(size_t)nWrite);` |
|   10 |  290 | `		if( nWr < 1 ){` |
|  ! 0 |  291 | `			return -1;` |
|    - |  292 | `		}` |
|   10 |  293 | `		return (ph7_int64)nWr;` |
|    - |  294 | `	}` |
|    - |  295 | `#else` |
|    - |  296 | `	return -1;` |
|    - |  297 | `#endif` |
|   12 |  298 | `}` |
|    - |  299 | `/* void (*xClose)(void *) */` |
|   20 |  300 | `static void PHPStreamData_Close(void *pHandle)` |
|    1 |  301 | `{` |
|   21 |  302 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    - |  303 | `	ph7_vm *pVm;` |
|   21 |  304 | `	if( pData == 0 ){` |
|  ! 0 |  305 | `		return;` |
|    - |  306 | `	}` |
|   21 |  307 | `	pVm = pData->pVm;` |
|   21 |  308 | `	SyBlobRelease(&pData->sMem);` |
|    - |  309 | `	/* Free the instance */` |
|   21 |  310 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|   11 |  311 | `}` |
|    - |  312 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|   20 |  313 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|    1 |  314 | `{` |
|   21 |  315 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    - |  316 | `	ph7_int64 iNew;` |
|   21 |  317 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|  ! 0 |  318 | `		return -1;` |
|    - |  319 | `	}` |
|   21 |  320 | `	switch(whence){` |
|  ! 0 |  321 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|    3 |  322 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|   19 |  323 | `	default:            iNew = iOfft; break;` |
|    - |  324 | `	}` |
|   21 |  325 | `	if( iNew < 0 ){` |
|  ! 0 |  326 | `		return -1;` |
|    - |  327 | `	}` |
|   21 |  328 | `	pData->nCur = (sxu32)iNew;` |
|   21 |  329 | `	return PH7_OK;` |
|   11 |  330 | `}` |
|    - |  331 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|    4 |  332 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|    1 |  333 | `{` |
|    5 |  334 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    5 |  335 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|  ! 0 |  336 | `		return -1;` |
|    - |  337 | `	}` |
|    5 |  338 | `	return (ph7_int64)pData->nCur;` |
|    3 |  339 | `}` |
|    - |  340 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|  ! 0 |  341 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|  ! 0 |  342 | `{` |
|  ! 0 |  343 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  ! 0 |  344 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|  ! 0 |  345 | `		return -1;` |
|    - |  346 | `	}` |
|  ! 0 |  347 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|    - |  348 | `		/* shrink in place: the blob keeps its allocation */` |
|  ! 0 |  349 | `		pData->sMem.nByte = (sxu32)nLen;` |
|  ! 0 |  350 | `	}else{` |
|    - |  351 | `		static const char zZero[64] = {0};` |
|  ! 0 |  352 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|  ! 0 |  353 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|  ! 0 |  354 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|  ! 0 |  355 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|  ! 0 |  356 | `				return -1;` |
|    - |  357 | `			}` |
|  ! 0 |  358 | `		}` |
|    - |  359 | `	}` |
|  ! 0 |  360 | `	return PH7_OK;` |
|  ! 0 |  361 | `}` |
|    - |  362 | `/*` |
|    - |  363 | ` * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs` |
|    - |  364 | ` * (data://[mediatype][;base64],payload — the payload percent-decodes unless` |
|    - |  365 | ` * base64). Shares the MEMORY machinery above.` |
|    - |  366 | ` */` |
|    8 |  367 | `static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|    1 |  368 | `{` |
|    9 |  369 | `	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);` |
|    1 |  370 | `}` |
|   10 |  371 | `static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    1 |  372 | `{` |
|    - |  373 | `	ph7_stream_data *pData;` |
|   11 |  374 | `	const char *zIn = zName;` |
|   11 |  375 | `	const char *zEnd = &zName[SyStrlen(zName)];` |
|   11 |  376 | `	const char *zComma = 0;` |
|   11 |  377 | `	int bBase64 = 0;` |
|    5 |  378 | `	SXUNUSED(iMode);` |
|    - |  379 | `	/* Find the comma separating the mediatype from the payload */` |
|  105 |  380 | `	while( zIn < zEnd ){` |
|  105 |  381 | `		if( zIn[0] == ',' ){` |
|   11 |  382 | `			zComma = zIn;` |
|   11 |  383 | `			break;` |
|    - |  384 | `		}` |
|   95 |  385 | `		zIn++;` |
|    1 |  386 | `	}` |
|   11 |  387 | `	if( zComma == 0 ){` |
|  ! 0 |  388 | `		return -1;` |
|    - |  389 | `	}` |
|   10 |  390 | `	if( zComma - zName >= (int)sizeof(";base64")-1` |
|   10 |  391 | `	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){` |
|    3 |  392 | `		bBase64 = 1;` |
|    1 |  393 | `	}` |
|   11 |  394 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);` |
|   11 |  395 | `	if( pData == 0 ){` |
|  ! 0 |  396 | `		return -1;` |
|    - |  397 | `	}` |
|   11 |  398 | `	pData->bReadOnly = 1;` |
|   11 |  399 | `	zIn = &zComma[1];` |
|   11 |  400 | `	if( bBase64 ){` |
|    3 |  401 | `		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){` |
|  ! 0 |  402 | `			SyBlobRelease(&pData->sMem);` |
|  ! 0 |  403 | `			SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|  ! 0 |  404 | `			return -1;` |
|    - |  405 | `		}` |
|    2 |  406 | `	}else{` |
|    - |  407 | `		/* percent-decode the payload */` |
|   71 |  408 | `		while( zIn < zEnd ){` |
|   63 |  409 | `			char c = zIn[0];` |
|   63 |  410 | `			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){` |
|    3 |  411 | `				int hi = SyHexToint(zIn[1]);` |
|    3 |  412 | `				int lo = SyHexToint(zIn[2]);` |
|    3 |  413 | `				c = (char)((hi << 4) \| lo);` |
|    3 |  414 | `				zIn += 3;` |
|    2 |  415 | `			}else{` |
|   61 |  416 | `				zIn++;` |
|    - |  417 | `			}` |
|   63 |  418 | `			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){` |
|  ! 0 |  419 | `				SyBlobRelease(&pData->sMem);` |
|  ! 0 |  420 | `				SyMemBackendFree(&pData->pVm->sAllocator,pData);` |
|  ! 0 |  421 | `				return -1;` |
|    - |  422 | `			}` |
|    1 |  423 | `		}` |
|    - |  424 | `	}` |
|   11 |  425 | `	*ppHandle = (void *)pData;` |
|   11 |  426 | `	return PH7_OK;` |
|    6 |  427 | `}` |
|    - |  428 | `/* data:// rejects writes outright */` |
|  ! 0 |  429 | `static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|  ! 0 |  430 | `{` |
|  ! 0 |  431 | `	SXUNUSED(pHandle);` |
|  ! 0 |  432 | `	SXUNUSED(pBuf);` |
|  ! 0 |  433 | `	SXUNUSED(nWrite);` |
|  ! 0 |  434 | `	return -1;` |
|  ! 0 |  435 | `}` |
|    - |  436 | `PH7_PRIVATE const ph7_io_stream sDATA_Stream = {` |
|    - |  437 | `	"data",` |
|    - |  438 | `	PH7_IO_STREAM_VERSION,` |
|    - |  439 | `	DataStreamData_Open,  /* xOpen */` |
|    - |  440 | `	0,   /* xOpenDir */` |
|    - |  441 | `	PHPStreamData_Close, /* xClose */` |
|    - |  442 | `	0,  /* xCloseDir */` |
|    - |  443 | `	PHPStreamData_Read,  /* xRead */` |
|    - |  444 | `	0,  /* xReadDir */` |
|    - |  445 | `	DataStreamData_Write, /* xWrite */` |
|    - |  446 | `	PHPStreamData_Seek,  /* xSeek */` |
|    - |  447 | `	0,  /* xLock */` |
|    - |  448 | `	0,  /* xRewindDir */` |
|    - |  449 | `	PHPStreamData_Tell,  /* xTell */` |
|    - |  450 | `	0,  /* xTrunc */` |
|    - |  451 | `	0,  /* xSync */` |
|    - |  452 | `	0   /* xStat */` |
|    - |  453 | `};` |
|    - |  454 | `/*` |
|    - |  455 | ` * Pipe stream implementation for popen/pclose.` |
|    - |  456 | ` * This stream wraps the system's popen/pclose APIs to provide` |
|    - |  457 | ` * PHP-compatible process I/O functionality.` |
|    - |  458 | ` */` |
|    - |  459 | `typedef struct pipe_private pipe_private;` |
|    - |  460 | `struct pipe_private` |
|    - |  461 | `{` |
|    - |  462 | `	FILE *pFile;    /* Pipe file handle from popen */` |
|    - |  463 | `	ph7_vm *pVm;    /* VM that owns this instance */` |
|    - |  464 | `	int iMode;      /* Open mode: 'r' for read, 'w' for write */` |
|    - |  465 | `#ifdef __WINNT__` |
|    - |  466 | `	HANDLE hProcess; /* Process handle on Windows for proper waiting */` |
|    - |  467 | `	HANDLE hPipe;    /* Pipe handle (for cleanup) */` |
|    - |  468 | `#endif` |
|    - |  469 | `};` |
|    - |  470 |  |
|    - |  471 | `#ifdef __WINNT__` |
|    - |  472 | `#include <Windows.h>` |
|    - |  473 | `#include <stdio.h>` |
|    - |  474 | `#include <io.h>` |
|    - |  475 | `#include <fcntl.h>` |
|    - |  476 | `/*` |
|    - |  477 | ` * Custom Windows popen implementation using CreateProcess.` |
|    - |  478 | ` * This allows us to properly wait for process completion.` |
|    - |  479 | ` */` |
|    - |  480 | `static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)` |
|    5 |  481 | `{` |
|    5 |  482 | `	HANDLE hReadPipe = NULL, hWritePipe = NULL;` |
|    5 |  483 | `	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;` |
|    5 |  484 | `	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;` |
|    - |  485 | `	SECURITY_ATTRIBUTES sa;` |
|    - |  486 | `	STARTUPINFOW si;` |
|    - |  487 | `	PROCESS_INFORMATION pi;` |
|    5 |  488 | `	WCHAR *zWideCmd = NULL;` |
|    5 |  489 | `	FILE *pFile = NULL;` |
|    - |  490 | `	int fd;` |
|    5 |  491 | `	BOOL bRead = (zMode[0] == 'r');` |
|    - |  492 |  |
|    - |  493 | `	/* Set up security attributes for pipe inheritance */` |
|    5 |  494 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|    5 |  495 | `	sa.bInheritHandle = TRUE;` |
|    5 |  496 | `	sa.lpSecurityDescriptor = NULL;` |
|    - |  497 |  |
|    - |  498 | `	/* Create pipes for child process I/O */` |
|    5 |  499 | `	if( bRead ){` |
|    - |  500 | `		/* Reading from child's stdout */` |
|    5 |  501 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|  ! 0 |  502 | `			return NULL;` |
|    - |  503 | `		}` |
|    - |  504 | `		/* Ensure read handle is not inherited */` |
|    5 |  505 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|    5 |  506 | `		hReadPipe = hChildStdoutRd;` |
|    5 |  507 | `		*phPipe = hChildStdoutRd;` |
|    5 |  508 | `	}else{` |
|    - |  509 | `		/* Writing to child's stdin */` |
|  ! 0 |  510 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|  ! 0 |  511 | `			return NULL;` |
|    - |  512 | `		}` |
|    - |  513 | `		/* Ensure write handle is not inherited */` |
|  ! 0 |  514 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|  ! 0 |  515 | `		hWritePipe = hChildStdinWr;` |
|  ! 0 |  516 | `		*phPipe = hChildStdinWr;` |
|    - |  517 | `	}` |
|    - |  518 |  |
|    - |  519 | `	/* Convert command to wide string */` |
|    - |  520 | `	{` |
|    5 |  521 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|    5 |  522 | `		if( nLen <= 0 ){` |
|  ! 0 |  523 | `			goto cleanup_pipes;` |
|    - |  524 | `		}` |
|    5 |  525 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|    5 |  526 | `		if( !zWideCmd ){` |
|  ! 0 |  527 | `			goto cleanup_pipes;` |
|    - |  528 | `		}` |
|    5 |  529 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|    - |  530 | `	}` |
|    - |  531 |  |
|    - |  532 | `	/* Set up process startup info */` |
|    5 |  533 | `	ZeroMemory(&si, sizeof(si));` |
|    5 |  534 | `	si.cb = sizeof(si);` |
|    5 |  535 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|    5 |  536 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|    5 |  537 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|    5 |  538 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|    5 |  539 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|    - |  540 |  |
|    5 |  541 | `	ZeroMemory(&pi, sizeof(pi));` |
|    - |  542 |  |
|    - |  543 | `	/* Create the child process */` |
|    5 |  544 | `	if( !CreateProcessW(` |
|    - |  545 | `		NULL,           /* Application name */` |
|    - |  546 | `		zWideCmd,       /* Command line */` |
|    - |  547 | `		NULL,           /* Process security attributes */` |
|    - |  548 | `		NULL,           /* Thread security attributes */` |
|    - |  549 | `		TRUE,           /* Inherit handles */` |
|    - |  550 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|    - |  551 | `		NULL,           /* Environment */` |
|    - |  552 | `		NULL,           /* Current directory */` |
|    - |  553 | `		&si,            /* Startup info */` |
|    - |  554 | `		&pi             /* Process info */` |
|    - |  555 | `	)){` |
|  ! 0 |  556 | `		goto cleanup_all;` |
|    - |  557 | `	}` |
|    - |  558 |  |
|    - |  559 | `	/* Close handles we don't need in parent */` |
|    5 |  560 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    5 |  561 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    - |  562 |  |
|    - |  563 | `	/* Close thread handle (we only need process handle) */` |
|    5 |  564 | `	CloseHandle(pi.hThread);` |
|    - |  565 |  |
|    - |  566 | `	/* Store process handle for later waiting */` |
|    5 |  567 | `	*phProcess = pi.hProcess;` |
|    - |  568 |  |
|    - |  569 | `	/* Convert OS handle to C file descriptor, then to FILE* */` |
|    5 |  570 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|    - |  571 | `	                     bRead ? _O_RDONLY \| _O_TEXT : _O_WRONLY \| _O_TEXT);` |
|    5 |  572 | `	if( fd == -1 ){` |
|  ! 0 |  573 | `		CloseHandle(pi.hProcess);` |
|  ! 0 |  574 | `		*phProcess = NULL;` |
|  ! 0 |  575 | `		goto cleanup_all;` |
|    - |  576 | `	}` |
|    - |  577 |  |
|    5 |  578 | `	pFile = _fdopen(fd, zMode);` |
|    5 |  579 | `	if( !pFile ){` |
|  ! 0 |  580 | `		_close(fd); /* This will also close the underlying handle */` |
|  ! 0 |  581 | `		CloseHandle(pi.hProcess);` |
|  ! 0 |  582 | `		*phProcess = NULL;` |
|  ! 0 |  583 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|  ! 0 |  584 | `		return NULL;` |
|    - |  585 | `	}` |
|    - |  586 |  |
|    5 |  587 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    5 |  588 | `	return pFile;` |
|    - |  589 |  |
|    - |  590 | `cleanup_all:` |
|  ! 0 |  591 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    - |  592 | `cleanup_pipes:` |
|  ! 0 |  593 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|  ! 0 |  594 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|  ! 0 |  595 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|  ! 0 |  596 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|  ! 0 |  597 | `	return NULL;` |
|    5 |  598 | `}` |
|    - |  599 |  |
|    - |  600 | `/*` |
|    - |  601 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|    - |  602 | ` */` |
|    - |  603 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|    5 |  604 | `{` |
|    5 |  605 | `	DWORD dwExitCode = 0;` |
|    - |  606 | `	int status;` |
|    - |  607 |  |
|    - |  608 | `	/* Close the FILE* (this closes the pipe) */` |
|    5 |  609 | `	fclose(pFile);` |
|    - |  610 |  |
|    5 |  611 | `	if( hProcess ){` |
|    - |  612 | `		/* Wait for the process to complete */` |
|    5 |  613 | `		WaitForSingleObject(hProcess, INFINITE);` |
|    - |  614 |  |
|    5 |  615 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|    5 |  616 | `			status = (int)dwExitCode;` |
|    5 |  617 | `		}else{` |
|  ! 0 |  618 | `			status = -1;` |
|    - |  619 | `		}` |
|    - |  620 |  |
|    - |  621 | `		/* Close process handle */` |
|    5 |  622 | `		CloseHandle(hProcess);` |
|    5 |  623 | `	}else{` |
|  ! 0 |  624 | `		status = -1;` |
|    - |  625 | `	}` |
|    - |  626 |  |
|    5 |  627 | `	return status;` |
|    5 |  628 | `}` |
|    - |  629 | `#endif /* __WINNT__ */` |
|    - |  630 | `/*` |
|    - |  631 | ` * Open a pipe to a process.` |
|    - |  632 | ` * This is called internally by popen(), not through the stream device interface.` |
|    - |  633 | ` */` |
| 3970 |  634 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|    5 |  635 | `{` |
|    - |  636 | `	pipe_private *pPipe;` |
|    - |  637 | `	FILE *pFile;` |
| 3975 |  638 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|  ! 0 |  639 | `		return 0;` |
|    - |  640 | `	}` |
|    - |  641 | `	/* Validate mode - only 'r' or 'w' allowed */` |
| 3975 |  642 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|  ! 0 |  643 | `		return 0;` |
|    - |  644 | `	}` |
|    - |  645 | `	/* Open the pipe using system popen */` |
|    - |  646 | `#ifdef __WINNT__` |
|    - |  647 | `	{` |
|    - |  648 | `		/* Build cmd.exe command wrapper */` |
|    5 |  649 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|    5 |  650 | `		const char *zShellSuffix = "\"";` |
|    5 |  651 | `		size_t nPrefix = strlen(zShellPrefix);` |
|    5 |  652 | `		size_t nSuffix = strlen(zShellSuffix);` |
|    5 |  653 | `		size_t nCmd = strlen(zCommand);` |
|    5 |  654 | `		size_t nQuotes = 0;` |
|    5 |  655 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|    5 |  656 | `			if (zCommand[i] == '"') nQuotes++;` |
|    5 |  657 | `		}` |
|    5 |  658 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|    5 |  659 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|    5 |  660 | `		if (zCmdEsc == NULL) {` |
|  ! 0 |  661 | `			return 0;` |
|    - |  662 | `		}` |
|    - |  663 | `		/* Escape quotes in command */` |
|    5 |  664 | `		size_t j = 0;` |
|    5 |  665 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|    5 |  666 | `			char ch = zCommand[i];` |
|    5 |  667 | `			if (ch == '"') {` |
|    4 |  668 | `				zCmdEsc[j++] = '^';` |
|    4 |  669 | `				zCmdEsc[j++] = '"';` |
|    4 |  670 | `			} else {` |
|    5 |  671 | `				zCmdEsc[j++] = ch;` |
|    - |  672 | `			}` |
|    5 |  673 | `		}` |
|    5 |  674 | `		zCmdEsc[j] = '\0';` |
|    5 |  675 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|    5 |  676 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|    5 |  677 | `		if (zWinCmd == NULL) {` |
|  ! 0 |  678 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|  ! 0 |  679 | `			return 0;` |
|    - |  680 | `		}` |
|    5 |  681 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|    5 |  682 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|    5 |  683 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|    5 |  684 | `		zWinCmd[nTotal - 1] = '\0';` |
|    - |  685 | `		/* Allocate pipe structure early so we can store handles */` |
|    5 |  686 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|    5 |  687 | `		if( pPipe == 0 ){` |
|  ! 0 |  688 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|  ! 0 |  689 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|  ! 0 |  690 | `			return 0;` |
|    - |  691 | `		}` |
|    - |  692 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|    5 |  693 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|    5 |  694 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    5 |  695 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    5 |  696 | `		if( pFile == 0 ){` |
|  ! 0 |  697 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|  ! 0 |  698 | `			return 0;` |
|    - |  699 | `		}` |
|    - |  700 | `		/* Initialize remaining fields */` |
|    5 |  701 | `		pPipe->pFile = pFile;` |
|    5 |  702 | `		pPipe->pVm = pVm;` |
|    5 |  703 | `		pPipe->iMode = zMode[0];` |
|    - |  704 | `	}` |
|    - |  705 | `#elif defined(__UNIXES__) /* Unix */` |
| 3970 |  706 | `	pFile = popen(zCommand, zMode);` |
| 3970 |  707 | `	if( pFile == 0 ){` |
|  ! 0 |  708 | `		return 0;` |
|    - |  709 | `	}` |
|    - |  710 | `	/* Allocate pipe private structure */` |
| 3970 |  711 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
| 3970 |  712 | `	if( pPipe == 0 ){` |
|    - |  713 | `		/* Out of memory, close the pipe */` |
|  ! 0 |  714 | `		pclose(pFile);` |
|  ! 0 |  715 | `		return 0;` |
|    - |  716 | `	}` |
|    - |  717 | `	/* Initialize the structure */` |
| 3970 |  718 | `	pPipe->pFile = pFile;` |
| 3970 |  719 | `	pPipe->pVm = pVm;` |
| 3970 |  720 | `	pPipe->iMode = zMode[0];` |
|    - |  721 | `#else /* OS_OTHER: no process pipes on this platform */` |
|    - |  722 | `	(void)pFile;` |
|    - |  723 | `	return 0;` |
|    - |  724 | `#endif` |
| 3975 |  725 | `	return pPipe;` |
| 1990 |  726 | `}` |
|    - |  727 | `/*` |
|    - |  728 | ` * Close a pipe and return the exit status of the process.` |
|    - |  729 | ` * Returns the exit status, or -1 on error.` |
|    - |  730 | ` */` |
| 3944 |  731 | `static int PipeClose(pipe_private *pPipe)` |
|    5 |  732 | `{` |
|    - |  733 | `	int status;` |
|    - |  734 | `	ph7_vm *pVm;` |
| 3949 |  735 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|  ! 0 |  736 | `		return -1;` |
|    - |  737 | `	}` |
| 3949 |  738 | `	pVm = pPipe->pVm;` |
|    - |  739 | `	/* Close the pipe and get exit status */` |
|    - |  740 | `#ifdef __WINNT__` |
|    - |  741 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|    5 |  742 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|    - |  743 | `#elif defined(__UNIXES__)` |
| 3944 |  744 | `	status = pclose(pPipe->pFile);` |
|    - |  745 | `	/* On Unix, pclose returns the status from waitpid, need to extract exit code */` |
| 3944 |  746 | `	if( status != -1 ){` |
| 3944 |  747 | `		if( WIFEXITED(status) ){` |
| 3944 |  748 | `			status = WEXITSTATUS(status);` |
| 1972 |  749 | `		}else if( WIFSIGNALED(status) ){` |
|    - |  750 | `			/* Process was killed by a signal - use shell convention: 128 + signal number */` |
|  ! 0 |  751 | `			status = 128 + WTERMSIG(status);` |
|  ! 0 |  752 | `		}else{` |
|    - |  753 | `			/* Unknown termination reason */` |
|  ! 0 |  754 | `			status = -1;` |
|    - |  755 | `		}` |
| 1972 |  756 | `	}` |
|    - |  757 | `#else /* OS_OTHER: no process pipes on this platform */` |
|    - |  758 | `	status = -1;` |
|    - |  759 | `#endif` |
|    - |  760 | `	/* Free the structure */` |
| 3949 |  761 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
| 3949 |  762 | `	return status;` |
| 1977 |  763 | `}` |
|    - |  764 | `/*` |
|    - |  765 | ` * Pipe stream xClose implementation.` |
|    - |  766 | ` * Note: This is called by fclose(), not pclose().` |
|    - |  767 | ` * It closes the pipe but does not return the exit status.` |
|    - |  768 | ` */` |
|  102 |  769 | `static void PipeStream_Close(void *pHandle)` |
|    4 |  770 | `{` |
|  106 |  771 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|  106 |  772 | `	if( pPipe ){` |
|  106 |  773 | `		PipeClose(pPipe);` |
|   51 |  774 | `	}` |
|  106 |  775 | `}` |
|    - |  776 | `/*` |
|    - |  777 | ` * Pipe stream xRead implementation.` |
|    - |  778 | ` */` |
| 5916 |  779 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|    4 |  780 | `{` |
| 5920 |  781 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    - |  782 | `	size_t nRead;` |
| 5920 |  783 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|  ! 0 |  784 | `		return -1;` |
|    - |  785 | `	}` |
| 5920 |  786 | `	if( pPipe->iMode != 'r' ){` |
|    - |  787 | `		/* Cannot read from a write-only pipe */` |
|  ! 0 |  788 | `		return -1;` |
|    - |  789 | `	}` |
| 5920 |  790 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
| 5920 |  791 | `	if( nRead == 0 ){` |
| 3978 |  792 | `		if( feof(pPipe->pFile) ){` |
| 3978 |  793 | `			return 0; /* EOF */` |
|    - |  794 | `		}` |
|  ! 0 |  795 | `		return -1; /* Error */` |
|    - |  796 | `	}` |
| 1946 |  797 | `	return (ph7_int64)nRead;` |
| 2962 |  798 | `}` |
|    - |  799 | `/*` |
|    - |  800 | ` * Pipe stream xWrite implementation.` |
|    - |  801 | ` */` |
|    4 |  802 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|  ! 0 |  803 | `{` |
|    4 |  804 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    - |  805 | `	size_t nWritten;` |
|    4 |  806 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|  ! 0 |  807 | `		return -1;` |
|    - |  808 | `	}` |
|    4 |  809 | `	if( pPipe->iMode != 'w' ){` |
|    - |  810 | `		/* Cannot write to a read-only pipe */` |
|  ! 0 |  811 | `		return -1;` |
|    - |  812 | `	}` |
|    4 |  813 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|    4 |  814 | `	if( nWritten == 0 && nWrite > 0 ){` |
|  ! 0 |  815 | `		return -1; /* Error */` |
|    - |  816 | `	}` |
|    4 |  817 | `	return (ph7_int64)nWritten;` |
|    2 |  818 | `}` |
|    - |  819 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|    - |  820 | `static const ph7_io_stream sPipe_Stream = {` |
|    - |  821 | `	"pipe",` |
|    - |  822 | `	PH7_IO_STREAM_VERSION,` |
|    - |  823 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|    - |  824 | `	0,  /* xOpenDir */` |
|    - |  825 | `	PipeStream_Close,  /* xClose */` |
|    - |  826 | `	0,  /* xCloseDir */` |
|    - |  827 | `	PipeStream_Read,   /* xRead */` |
|    - |  828 | `	0,  /* xReadDir */` |
|    - |  829 | `	PipeStream_Write,  /* xWrite */` |
|    - |  830 | `	0,  /* xSeek */` |
|    - |  831 | `	0,  /* xLock */` |
|    - |  832 | `	0,  /* xRewindDir */` |
|    - |  833 | `	0,  /* xTell */` |
|    - |  834 | `	0,  /* xTrunc */` |
|    - |  835 | `	0,  /* xSync */` |
|    - |  836 | `	0   /* xStat */` |
|    - |  837 | `};` |
|    - |  838 | `/*` |
|    - |  839 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|    - |  840 | ` * FALSE otherwise.` |
|    - |  841 | ` */` |
| 3842 |  842 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|    5 |  843 | `{` |
| 3847 |  844 | `	return pStream == &sPipe_Stream;` |
|    5 |  845 | `}` |
|    - |  846 | `/*` |
|    - |  847 | ` * resource popen(string $command, string $mode)` |
|    - |  848 | ` *  Opens process file pointer.` |
|    - |  849 | ` * Parameters` |
|    - |  850 | ` *  $command` |
|    - |  851 | ` *   The command to execute. Passed to the system shell.` |
|    - |  852 | ` *  $mode` |
|    - |  853 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|    - |  854 | ` *   'r' - Open for reading (read from the command's stdout).` |
|    - |  855 | ` *   'w' - Open for writing (write to the command's stdin).` |
|    - |  856 | ` * Return` |
|    - |  857 | ` *  Returns a file pointer on success, or FALSE on error.` |
|    - |  858 | ` */` |
|    - |  859 | `/*` |
|    - |  860 | ` * string\|false\|null shell_exec(string $command)` |
|    - |  861 | ` *  Execute a command via the shell and return the complete output as a string.` |
|    - |  862 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|    - |  863 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|    - |  864 | ` */` |
|  ! 0 |  865 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 |  866 | `{` |
|    - |  867 | `	const char *zCommand;` |
|    - |  868 | `	pipe_private *pPipe;` |
|    - |  869 | `	SyBlob sOut;` |
|    - |  870 | `	char zBuf[4096];` |
|    - |  871 | `	size_t nRead;` |
|    - |  872 | `	int nCmdLen;` |
|  ! 0 |  873 | `	if( nArg < 1 ){` |
|  ! 0 |  874 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  875 | `		return PH7_OK;` |
|    - |  876 | `	}` |
|  ! 0 |  877 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|  ! 0 |  878 | `	if( nCmdLen < 1 ){` |
|  ! 0 |  879 | `		ph7_result_null(pCtx);` |
|  ! 0 |  880 | `		return PH7_OK;` |
|    - |  881 | `	}` |
|  ! 0 |  882 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|  ! 0 |  883 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|  ! 0 |  884 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  885 | `		return PH7_OK;` |
|    - |  886 | `	}` |
|  ! 0 |  887 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  ! 0 |  888 | `	for(;;){` |
|  ! 0 |  889 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|  ! 0 |  890 | `		if( nRead < 1 ){` |
|  ! 0 |  891 | `			break;` |
|    - |  892 | `		}` |
|  ! 0 |  893 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|  ! 0 |  894 | `	}` |
|  ! 0 |  895 | `	PipeClose(pPipe);` |
|  ! 0 |  896 | `	if( SyBlobLength(&sOut) < 1 ){` |
|    - |  897 | `		/* php answers NULL, not "", when the command printed nothing */` |
|  ! 0 |  898 | `		ph7_result_null(pCtx);` |
|  ! 0 |  899 | `	}else{` |
|  ! 0 |  900 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    - |  901 | `	}` |
|  ! 0 |  902 | `	SyBlobRelease(&sOut);` |
|  ! 0 |  903 | `	return PH7_OK;` |
|  ! 0 |  904 | `}` |
| 3970 |  905 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  906 | `{` |
|    - |  907 | `	const char *zCommand, *zMode;` |
|    - |  908 | `	pipe_private *pPipe;` |
|    - |  909 | `	io_private *pDev;` |
|    - |  910 | `	int nCmdLen, nModeLen;` |
| 3975 |  911 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_string(apArg[1]) ){` |
|    - |  912 | `		/* Missing/Invalid arguments, return FALSE */` |
|  ! 0 |  913 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");` |
|  ! 0 |  914 | `		ph7_result_bool(pCtx, 0);` |
|  ! 0 |  915 | `		return PH7_OK;` |
|    - |  916 | `	}` |
|    - |  917 | `	/* Extract the command and mode */` |
| 3975 |  918 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
| 3975 |  919 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
| 3975 |  920 | `	if( nCmdLen < 1 ){` |
|  ! 0 |  921 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");` |
|  ! 0 |  922 | `		ph7_result_bool(pCtx, 0);` |
|  ! 0 |  923 | `		return PH7_OK;` |
|    - |  924 | `	}` |
| 3975 |  925 | `	if( nModeLen < 1 \|\| (zMode[0] != 'r' && zMode[0] != 'w') ){` |
|  ! 0 |  926 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");` |
|  ! 0 |  927 | `		ph7_result_bool(pCtx, 0);` |
|  ! 0 |  928 | `		return PH7_OK;` |
|    - |  929 | `	}` |
|    - |  930 | `	/* Open the pipe */` |
| 3975 |  931 | `	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);` |
| 3975 |  932 | `	if( pPipe == 0 ){` |
|    - |  933 | `		/* Failed to open pipe */` |
|  ! 0 |  934 | `		ph7_result_bool(pCtx, 0);` |
|  ! 0 |  935 | `		return PH7_OK;` |
|    - |  936 | `	}` |
|    - |  937 | `	/* Allocate an io_private instance to wrap the pipe */` |
| 3975 |  938 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
| 3975 |  939 | `	if( pDev == 0 ){` |
|  ! 0 |  940 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|  ! 0 |  941 | `		PipeClose(pPipe);` |
|  ! 0 |  942 | `		ph7_result_bool(pCtx, 0);` |
|  ! 0 |  943 | `		return PH7_OK;` |
|    - |  944 | `	}` |
|    - |  945 | `	/* Initialize the io_private structure */` |
| 3975 |  946 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
| 3975 |  947 | `	pDev->pHandle = pPipe;` |
|    - |  948 | `	/* Return the io_private instance as a resource */` |
| 3975 |  949 | `	ph7_result_resource(pCtx, pDev);` |
| 3975 |  950 | `	return PH7_OK;` |
| 1990 |  951 | `}` |
|    - |  952 | `/*` |
|    - |  953 | ` * int pclose(resource $handle)` |
|    - |  954 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|    - |  955 | ` * Parameters` |
|    - |  956 | ` *  $handle` |
|    - |  957 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|    - |  958 | ` * Return` |
|    - |  959 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|    - |  960 | ` */` |
| 3842 |  961 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 |  962 | `{` |
|    - |  963 | `	const ph7_io_stream *pStream;` |
|    - |  964 | `	pipe_private *pPipe;` |
|    - |  965 | `	io_private *pDev;` |
|    - |  966 | `	int status;` |
| 3847 |  967 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - |  968 | `		/* Missing/Invalid arguments, return -1 */` |
|  ! 0 |  969 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|  ! 0 |  970 | `		ph7_result_int(pCtx, -1);` |
|  ! 0 |  971 | `		return PH7_OK;` |
|    - |  972 | `	}` |
|    - |  973 | `	/* Extract our private data */` |
| 3847 |  974 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|    - |  975 | `	/* Make sure we are dealing with a valid io_private instance */` |
| 3847 |  976 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|  ! 0 |  977 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|  ! 0 |  978 | `		ph7_result_int(pCtx, -1);` |
|  ! 0 |  979 | `		return PH7_OK;` |
|    - |  980 | `	}` |
|    - |  981 | `	/* Point to the target IO stream device */` |
| 3847 |  982 | `	pStream = pDev->pStream;` |
| 3847 |  983 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|  ! 0 |  984 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|  ! 0 |  985 | `		ph7_result_int(pCtx, -1);` |
|  ! 0 |  986 | `		return PH7_OK;` |
|    - |  987 | `	}` |
|    - |  988 | `	/* Get the pipe handle */` |
| 3847 |  989 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|    - |  990 | `	/* Close the pipe and get exit status */` |
| 3847 |  991 | `	status = PipeClose(pPipe);` |
|    - |  992 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
| 3847 |  993 | `	MarkIOPrivateClosed(pDev);` |
|    - |  994 | `	/* Return the exit status */` |
| 3847 |  995 | `	ph7_result_int(pCtx, status);` |
| 3847 |  996 | `	return PH7_OK;` |
| 1926 |  997 | `}` |
|    - |  998 | `/*` |
|    - |  999 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|    - | 1000 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|    - | 1001 | ` *   standard descriptors (php's process-control family). The returned` |
|    - | 1002 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|    - | 1003 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|    - | 1004 | ` *   bounds and report it as a live, non-stream resource.` |
|    - | 1005 | ` */` |
|    - | 1006 | `#ifdef __UNIXES__` |
|    - | 1007 | `#define PROC_PRIVATE_MAGIC 0x9C0DE5` |
|    - | 1008 | `#define PROC_MAX_DESC 16` |
|    - | 1009 | `typedef struct proc_private proc_private;` |
|    - | 1010 | `struct proc_private` |
|    - | 1011 | `{` |
|    - | 1012 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|    - | 1013 | `	int pid;           /* child process id */` |
|    - | 1014 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|    - | 1015 | `	int exit_code;     /* cached exit status once reaped */` |
|    - | 1016 | `};` |
|    - | 1017 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|   28 | 1018 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd)` |
|    - | 1019 | `{` |
|   28 | 1020 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|   28 | 1021 | `	if( pDev == 0 ){` |
|  ! 0 | 1022 | `		return 0;` |
|    - | 1023 | `	}` |
|   28 | 1024 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|   28 | 1025 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|   28 | 1026 | `	return pDev;` |
|   14 | 1027 | `}` |
|    - | 1028 | `/* One parsed descriptor-spec entry. */` |
|    - | 1029 | `struct proc_desc` |
|    - | 1030 | `{` |
|    - | 1031 | `	int child_fd;      /* the array key: which fd the child sees */` |
|    - | 1032 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|    - | 1033 | `	/* pipe */` |
|    - | 1034 | `	int child_end;     /* fd the child must have at child_fd */` |
|    - | 1035 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|    - | 1036 | `	/* file */` |
|    - | 1037 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|    - | 1038 | `	/* redirect */` |
|    - | 1039 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|    - | 1040 | `};` |
|   10 | 1041 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1042 | `{` |
|    - | 1043 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|   10 | 1044 | `	int nDesc = 0;` |
|    - | 1045 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|    - | 1046 | `	ph7_hashmap *pSpecMap;` |
|    - | 1047 | `	ph7_hashmap_node *pNode;` |
|   10 | 1048 | `	ph7_vm *pVm = pCtx->pVm;` |
|   10 | 1049 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|   10 | 1050 | `	int nArgv = 0;` |
|   10 | 1051 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|   10 | 1052 | `	const char *zCwd = 0;` |
|   10 | 1053 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|   10 | 1054 | `	int nEnv = 0;` |
|    - | 1055 | `	proc_private *pProc;` |
|    - | 1056 | `	pid_t pid;` |
|    - | 1057 | `	int i, rc;` |
|   10 | 1058 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|  ! 0 | 1059 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|  ! 0 | 1060 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1061 | `		return PH7_OK;` |
|    - | 1062 | `	}` |
|    - | 1063 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|   10 | 1064 | `	if( ph7_value_is_array(apArg[0]) ){` |
|   10 | 1065 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   10 | 1066 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|   10 | 1067 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|   10 | 1068 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   10 | 1069 | `		pNode = pCmdMap->pFirst;` |
|   20 | 1070 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|   10 | 1071 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|    - | 1072 | `			int nLen; const char *zs;` |
|   10 | 1073 | `			PH7_MemObjInit(pVm,pv);` |
|   10 | 1074 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|   10 | 1075 | `			zs = ph7_value_to_string(pv,&nLen);` |
|   10 | 1076 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|   10 | 1077 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|   10 | 1078 | `			PH7_MemObjRelease(pv);` |
|   10 | 1079 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|   10 | 1080 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|    5 | 1081 | `		}` |
|   10 | 1082 | `		azArgv[nArgv] = 0;` |
|    5 | 1083 | `	}else{` |
|    - | 1084 | `		int nLen;` |
|  ! 0 | 1085 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|    - | 1086 | `	}` |
|    - | 1087 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|   10 | 1088 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|  ! 0 | 1089 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|  ! 0 | 1090 | `		if( nLen < 1 ){ zCwd = 0; }` |
|  ! 0 | 1091 | `	}` |
|    5 | 1092 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|  ! 0 | 1093 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|  ! 0 | 1094 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|  ! 0 | 1095 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|  ! 0 | 1096 | `		if( azEnv ){` |
|  ! 0 | 1097 | `			pNode = pEnvMap->pFirst;` |
|  ! 0 | 1098 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|    - | 1099 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|  ! 0 | 1100 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|  ! 0 | 1101 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|  ! 0 | 1102 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|  ! 0 | 1103 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|  ! 0 | 1104 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|  ! 0 | 1105 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));` |
|  ! 0 | 1106 | `				if( zPair ){` |
|  ! 0 | 1107 | `					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|  ! 0 | 1108 | `					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|  ! 0 | 1109 | `					azEnv[nEnv++] = zPair;` |
|  ! 0 | 1110 | `				}` |
|  ! 0 | 1111 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|  ! 0 | 1112 | `				pNode = pNode->pPrev;` |
|  ! 0 | 1113 | `			}` |
|  ! 0 | 1114 | `			azEnv[nEnv] = 0;` |
|  ! 0 | 1115 | `		}` |
|  ! 0 | 1116 | `	}` |
|    - | 1117 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|   10 | 1118 | `	pSpec = apArg[1];` |
|   10 | 1119 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|   10 | 1120 | `	pNode = pSpecMap->pFirst;` |
|   40 | 1121 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|   30 | 1122 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|   30 | 1123 | `		PH7_MemObjInit(pVm,&sKey);` |
|   30 | 1124 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|   30 | 1125 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|   30 | 1126 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1;` |
|   30 | 1127 | `		PH7_MemObjRelease(&sKey);` |
|   30 | 1128 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   30 | 1129 | `		PH7_MemObjInit(pVm,pEntry);` |
|   30 | 1130 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|   30 | 1131 | `		if( ph7_value_is_array(pEntry) ){` |
|    - | 1132 | `			int nLen; const char *zType;` |
|   30 | 1133 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|   30 | 1134 | `			zType = pType ? ph7_value_to_string(pType,&nLen) : "";` |
|   30 | 1135 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|    - | 1136 | `				int fds[2];` |
|   28 | 1137 | `				if( pipe(fds) == 0 ){` |
|   28 | 1138 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|    - | 1139 | `					{` |
|   28 | 1140 | `						int nMode; const char *zMode = pParam ? ph7_value_to_string(pParam,&nMode) : "r";` |
|   28 | 1141 | `						pD->kind = 0;` |
|   28 | 1142 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|    - | 1143 | `							/* child writes -> parent reads: child gets write end */` |
|   18 | 1144 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|    9 | 1145 | `						}else{` |
|    - | 1146 | `							/* child reads -> parent writes: child gets read end */` |
|   10 | 1147 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|    - | 1148 | `						}` |
|   28 | 1149 | `						nDesc++;` |
|    - | 1150 | `					}` |
|   14 | 1151 | `				}` |
|   16 | 1152 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|  ! 0 | 1153 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|  ! 0 | 1154 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|  ! 0 | 1155 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|  ! 0 | 1156 | `				zPath = pPath ? ph7_value_to_string(pPath,&nLen2) : "";` |
|  ! 0 | 1157 | `				zMode = pMode ? ph7_value_to_string(pMode,&nLen3) : "r";` |
|  ! 0 | 1158 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|  ! 0 | 1159 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|  ! 0 | 1160 | `				pD->kind = 1;` |
|  ! 0 | 1161 | `				pD->file_fd = open(zPath,oflag,0644);` |
|  ! 0 | 1162 | `				nDesc++;` |
|    2 | 1163 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|    2 | 1164 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|    2 | 1165 | `				pD->kind = 2;` |
|    2 | 1166 | `				pD->redirect_to = pParam ? ph7_value_to_int(pParam) : 1;` |
|    2 | 1167 | `				nDesc++;` |
|    1 | 1168 | `			}` |
|   15 | 1169 | `		}` |
|   30 | 1170 | `		PH7_MemObjRelease(pEntry);` |
|   30 | 1171 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|   30 | 1172 | `		pNode = pNode->pPrev;` |
|   15 | 1173 | `	}` |
|    - | 1174 | `	/* --- Fork the child --- */` |
|   10 | 1175 | `	pid = fork();` |
|   15 | 1176 | `	if( pid < 0 ){` |
|  ! 0 | 1177 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open(): fork() failed");` |
|  ! 0 | 1178 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1179 | `		return PH7_OK;` |
|    - | 1180 | `	}` |
|   20 | 1181 | `	if( pid == 0 ){` |
|    - | 1182 | `		/* Child: wire up descriptors then exec */` |
|   40 | 1183 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|   30 | 1184 | `			struct proc_desc *pD = &aDesc[i];` |
|   30 | 1185 | `			if( pD->kind == 0 ){` |
|   28 | 1186 | `				dup2(pD->child_end,pD->child_fd);` |
|   28 | 1187 | `				close(pD->parent_end);` |
|   28 | 1188 | `				close(pD->child_end);` |
|   16 | 1189 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|  ! 0 | 1190 | `				dup2(pD->file_fd,pD->child_fd);` |
|  ! 0 | 1191 | `				close(pD->file_fd);` |
|  ! 0 | 1192 | `			}` |
|   15 | 1193 | `		}` |
|    - | 1194 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|   40 | 1195 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|   30 | 1196 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|   15 | 1197 | `		}` |
|   10 | 1198 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|   10 | 1199 | `		if( azEnv ){` |
|  ! 0 | 1200 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|  ! 0 | 1201 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|  ! 0 | 1202 | `		}else{` |
|   10 | 1203 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|  ! 0 | 1204 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|    - | 1205 | `		}` |
|    5 | 1206 | `		_exit(127); /* exec failed */` |
|    - | 1207 | `	}` |
|    - | 1208 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|   10 | 1209 | `	pPipes = ph7_context_new_array(pCtx);` |
|   40 | 1210 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|   30 | 1211 | `		struct proc_desc *pD = &aDesc[i];` |
|   30 | 1212 | `		if( pD->kind == 0 ){` |
|    - | 1213 | `			io_private *pEnd;` |
|    - | 1214 | `			ph7_value *pRes;` |
|   28 | 1215 | `			close(pD->child_end);` |
|   28 | 1216 | `			pEnd = ProcWrapFd(pVm,pD->parent_end);` |
|   28 | 1217 | `			pRes = ph7_context_new_scalar(pCtx);` |
|   28 | 1218 | `			if( pEnd && pRes && pPipes ){` |
|   28 | 1219 | `				ph7_value_resource(pRes,pEnd);` |
|   28 | 1220 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|   14 | 1221 | `			}` |
|   28 | 1222 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|   16 | 1223 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|  ! 0 | 1224 | `			close(pD->file_fd);` |
|  ! 0 | 1225 | `		}` |
|   15 | 1226 | `	}` |
|   10 | 1227 | `	if( pPipes ){` |
|   10 | 1228 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|    5 | 1229 | `	}` |
|    - | 1230 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|   10 | 1231 | `	if( azArgv ){` |
|   20 | 1232 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|   10 | 1233 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|    5 | 1234 | `	}` |
|   15 | 1235 | `	if( azEnv ){` |
|  ! 0 | 1236 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|  ! 0 | 1237 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|  ! 0 | 1238 | `	}` |
|    - | 1239 | `	/* Build the process resource */` |
|   10 | 1240 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|   10 | 1241 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   10 | 1242 | `	SyZero(pProc,sizeof(proc_private));` |
|   10 | 1243 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|   10 | 1244 | `	pProc->pid = (int)pid;` |
|   10 | 1245 | `	pProc->running = 1;` |
|   10 | 1246 | `	pProc->exit_code = 0;` |
|   10 | 1247 | `	ph7_result_resource(pCtx,pProc);` |
|    5 | 1248 | `	(void)rc;` |
|   10 | 1249 | `	return PH7_OK;` |
|    5 | 1250 | `}` |
|    - | 1251 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|   10 | 1252 | `static void ProcReap(proc_private *pProc,int block)` |
|    - | 1253 | `{` |
|   10 | 1254 | `	int status = 0;` |
|    - | 1255 | `	pid_t r;` |
|   10 | 1256 | `	if( !pProc->running ){ return; }` |
|   10 | 1257 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|   10 | 1258 | `	if( r == (pid_t)pProc->pid ){` |
|   10 | 1259 | `		pProc->running = 0;` |
|   10 | 1260 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|  ! 0 | 1261 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|    5 | 1262 | `	}` |
|    5 | 1263 | `}` |
|   10 | 1264 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1265 | `{` |
|    - | 1266 | `	proc_private *pProc;` |
|   10 | 1267 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1268 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1269 | `		return PH7_OK;` |
|    - | 1270 | `	}` |
|   10 | 1271 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|   10 | 1272 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|  ! 0 | 1273 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1274 | `		return PH7_OK;` |
|    - | 1275 | `	}` |
|   10 | 1276 | `	ProcReap(pProc,1/*block until it exits*/);` |
|   10 | 1277 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|   10 | 1278 | `	return PH7_OK;` |
|    5 | 1279 | `}` |
|  ! 0 | 1280 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1281 | `{` |
|    - | 1282 | `	proc_private *pProc;` |
|  ! 0 | 1283 | `	int sig = 15; /* SIGTERM */` |
|  ! 0 | 1284 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1285 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1286 | `		return PH7_OK;` |
|    - | 1287 | `	}` |
|  ! 0 | 1288 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|  ! 0 | 1289 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|  ! 0 | 1290 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1291 | `		return PH7_OK;` |
|    - | 1292 | `	}` |
|  ! 0 | 1293 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|  ! 0 | 1294 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|  ! 0 | 1295 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 | 1296 | `	return PH7_OK;` |
|  ! 0 | 1297 | `}` |
|  ! 0 | 1298 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1299 | `{` |
|    - | 1300 | `	proc_private *pProc;` |
|    - | 1301 | `	ph7_value *pArray, *pVal;` |
|  ! 0 | 1302 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1303 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1304 | `		return PH7_OK;` |
|    - | 1305 | `	}` |
|  ! 0 | 1306 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|  ! 0 | 1307 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|  ! 0 | 1308 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1309 | `		return PH7_OK;` |
|    - | 1310 | `	}` |
|  ! 0 | 1311 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|  ! 0 | 1312 | `	pArray = ph7_context_new_array(pCtx);` |
|  ! 0 | 1313 | `	pVal = ph7_context_new_scalar(pCtx);` |
|  ! 0 | 1314 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|  ! 0 | 1315 | `	ph7_value_int(pVal,pProc->pid);` |
|  ! 0 | 1316 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|  ! 0 | 1317 | `	ph7_value_bool(pVal,pProc->running);` |
|  ! 0 | 1318 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|  ! 0 | 1319 | `	ph7_value_bool(pVal,0);` |
|  ! 0 | 1320 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|  ! 0 | 1321 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|  ! 0 | 1322 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|  ! 0 | 1323 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|  ! 0 | 1324 | `	ph7_value_int(pVal,0);` |
|  ! 0 | 1325 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|  ! 0 | 1326 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|  ! 0 | 1327 | `	ph7_context_release_value(pCtx,pVal);` |
|  ! 0 | 1328 | `	ph7_result_value(pCtx,pArray);` |
|  ! 0 | 1329 | `	return PH7_OK;` |
|  ! 0 | 1330 | `}` |
|    - | 1331 | `#else /* !__UNIXES__ */` |
|    - | 1332 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1333 | `{` |
|    - | 1334 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|  ! 0 | 1335 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|  ! 0 | 1336 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 | 1337 | `	return PH7_OK;` |
|  ! 0 | 1338 | `}` |
|    - | 1339 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1340 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|    - | 1341 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1342 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    - | 1343 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1344 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    - | 1345 | `#endif /* __UNIXES__ */` |
|    - | 1346 | `/* Export the php:// stream */` |
|    - | 1347 | `PH7_PRIVATE const ph7_io_stream sPHP_Stream = {` |
|    - | 1348 | `	"php",` |
|    - | 1349 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1350 | `	PHPStreamData_Open,  /* xOpen */` |
|    - | 1351 | `	0,   /* xOpenDir */` |
|    - | 1352 | `	PHPStreamData_Close, /* xClose */` |
|    - | 1353 | `	0,  /* xCloseDir */` |
|    - | 1354 | `	PHPStreamData_Read,  /* xRead */` |
|    - | 1355 | `	0,  /* xReadDir */` |
|    - | 1356 | `	PHPStreamData_Write, /* xWrite */` |
|    - | 1357 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|    - | 1358 | `	0,  /* xLock */` |
|    - | 1359 | `	0,  /* xRewindDir */` |
|    - | 1360 | `	PHPStreamData_Tell,  /* xTell */` |
|    - | 1361 | `	PHPStreamData_Trunc, /* xTrunc */` |
|    - | 1362 | `	0,  /* xSync */` |
|    - | 1363 | `	0   /* xStat */` |
|    - | 1364 | `};` |
|    - | 1365 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1366 | `/*` |
|    - | 1367 | ` * Return TRUE if we are dealing with the php:// stream.` |
|    - | 1368 | ` * FALSE otherwise.` |
|    - | 1369 | ` */` |
|  214 | 1370 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream)` |
|    4 | 1371 | `{` |
|    - | 1372 | `#ifndef PH7_DISABLE_DISK_IO` |
|  218 | 1373 | `	return pStream == &sPHP_Stream;` |
|    - | 1374 | `#else` |
|    - | 1375 | `	SXUNUSED(pStream); /* cc warning */` |
|    - | 1376 | `	return 0;` |
|    - | 1377 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    4 | 1378 | `}` |
|    - | 1379 | `/*` |
|    - | 1380 | ` * Return TRUE if we are dealing with the data:// stream.` |
|    - | 1381 | ` */` |
|  194 | 1382 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream)` |
|    4 | 1383 | `{` |
|    - | 1384 | `#ifndef PH7_DISABLE_DISK_IO` |
|  198 | 1385 | `	return pStream == &sDATA_Stream;` |
|    - | 1386 | `#else` |
|    - | 1387 | `	SXUNUSED(pStream); /* cc warning */` |
|    - | 1388 | `	return 0;` |
|    - | 1389 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    4 | 1390 | `}` |
|    - | 1391 | `/*` |
|    - | 1392 | ` * bool stream_isatty(resource $stream)` |
|    - | 1393 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|    - | 1394 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|    - | 1395 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|    - | 1396 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|    - | 1397 | ` */` |
|    6 | 1398 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1399 | `{` |
|    7 | 1400 | `	int bTty = 0;` |
|    7 | 1401 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1402 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1403 | `		return PH7_OK;` |
|    - | 1404 | `	}` |
|    - | 1405 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1406 | `	{` |
|    7 | 1407 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|    7 | 1408 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|    5 | 1409 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|    6 | 1410 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|    4 | 1411 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|    3 | 1412 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|    - | 1413 | `#ifdef __WINNT__` |
|    1 | 1414 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|    - | 1415 | `#else` |
|    4 | 1416 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|    - | 1417 | `#endif` |
|    2 | 1418 | `			}` |
|    2 | 1419 | `		}` |
|    - | 1420 | `	}` |
|    - | 1421 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    7 | 1422 | `	ph7_result_bool(pCtx,bTty);` |
|    7 | 1423 | `	return PH7_OK;` |
|    4 | 1424 | `}` |
|    - | 1425 |  |
|    - | 1426 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1427 | `/*` |
|    - | 1428 | ` * Export the STDIN handle.` |
|    - | 1429 | ` */` |
|    2 | 1430 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|    1 | 1431 | `{` |
|    - | 1432 | `#ifndef PH7_DISABLE_DISK_IO` |
|    3 | 1433 | `	if( pVm->pStdin == 0  ){` |
|    - | 1434 | `		io_private *pIn;` |
|    - | 1435 | `		/* Allocate an IO private instance */` |
|    3 | 1436 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    3 | 1437 | `		if( pIn == 0 ){` |
|  ! 0 | 1438 | `			return 0;` |
|    - | 1439 | `		}` |
|    3 | 1440 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|    - | 1441 | `		/* Initialize the handle */` |
|    3 | 1442 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|    - | 1443 | `		/* Install the STDIN stream */` |
|    3 | 1444 | `		pVm->pStdin = pIn;` |
|    3 | 1445 | `		return pIn;` |
|  ! 0 | 1446 | `	}else{` |
|    - | 1447 | `		/* NULL or STDIN */` |
|  ! 0 | 1448 | `		return pVm->pStdin;` |
|    - | 1449 | `	}` |
|    - | 1450 | `#else` |
|    - | 1451 | `	SXUNUSED(pVm); /* cc warning */` |
|    - | 1452 | `	return 0;` |
|    - | 1453 | `#endif` |
|    2 | 1454 | `}` |
|    - | 1455 | `/*` |
|    - | 1456 | ` * Export the STDOUT handle.` |
|    - | 1457 | ` */` |
|    8 | 1458 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|    1 | 1459 | `{` |
|    - | 1460 | `#ifndef PH7_DISABLE_DISK_IO` |
|    9 | 1461 | `	if( pVm->pStdout == 0  ){` |
|    - | 1462 | `		io_private *pOut;` |
|    - | 1463 | `		/* Allocate an IO private instance */` |
|    7 | 1464 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    7 | 1465 | `		if( pOut == 0 ){` |
|  ! 0 | 1466 | `			return 0;` |
|    - | 1467 | `		}` |
|    7 | 1468 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|    - | 1469 | `		/* Initialize the handle */` |
|    7 | 1470 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|    - | 1471 | `		/* Install the STDOUT stream */` |
|    7 | 1472 | `		pVm->pStdout = pOut;` |
|    7 | 1473 | `		return pOut;` |
|  ! 0 | 1474 | `	}else{` |
|    - | 1475 | `		/* NULL or STDOUT */` |
|    3 | 1476 | `		return pVm->pStdout;` |
|    - | 1477 | `	}` |
|    - | 1478 | `#else` |
|    - | 1479 | `	SXUNUSED(pVm); /* cc warning */` |
|    - | 1480 | `	return 0;` |
|    - | 1481 | `#endif` |
|    5 | 1482 | `}` |
|    - | 1483 | `/*` |
|    - | 1484 | ` * Export the STDERR handle.` |
|    - | 1485 | ` */` |
|   10 | 1486 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|    1 | 1487 | `{` |
|    - | 1488 | `#ifndef PH7_DISABLE_DISK_IO` |
|   11 | 1489 | `	if( pVm->pStderr == 0  ){` |
|    - | 1490 | `		io_private *pErr;` |
|    - | 1491 | `		/* Allocate an IO private instance */` |
|    9 | 1492 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    9 | 1493 | `		if( pErr == 0 ){` |
|  ! 0 | 1494 | `			return 0;` |
|    - | 1495 | `		}` |
|    9 | 1496 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|    - | 1497 | `		/* Initialize the handle */` |
|    9 | 1498 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|    - | 1499 | `		/* Install the STDERR stream */` |
|    9 | 1500 | `		pVm->pStderr = pErr;` |
|    9 | 1501 | `		return pErr;` |
|  ! 0 | 1502 | `	}else{` |
|    - | 1503 | `		/* NULL or STDERR */` |
|    3 | 1504 | `		return pVm->pStderr;` |
|    - | 1505 | `	}` |
|    - | 1506 | `#else` |
|    - | 1507 | `	SXUNUSED(pVm); /* cc warning */` |
|    - | 1508 | `	return 0;` |
|    - | 1509 | `#endif` |
|    6 | 1510 | `}` |
|    - | 1511 |  |
