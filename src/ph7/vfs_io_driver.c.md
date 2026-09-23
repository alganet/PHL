# src/ph7/vfs_io_driver.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 829/1103 lines (75.16%)

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
|  152 |   77 | `static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)` |
|    5 |   78 | `{` |
|    - |   79 | `	ph7_stream_data *pData;` |
|  157 |   80 | `	if( pVm == 0 ){` |
|  ! 0 |   81 | `		return 0;` |
|    - |   82 | `	}` |
|    - |   83 | `	/* Allocate a new instance */` |
|  157 |   84 | `	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));` |
|  157 |   85 | `	if( pData == 0 ){` |
|  ! 0 |   86 | `		return 0;` |
|    - |   87 | `	}` |
|    - |   88 | `	/* Zero the structure */` |
|  157 |   89 | `	SyZero(pData,sizeof(ph7_stream_data));` |
|    - |   90 | `	/* Initialize fields */` |
|  157 |   91 | `	pData->iType = iType;` |
|  157 |   92 | `	SyBlobInit(&pData->sMem,&pVm->sAllocator);` |
|  157 |   93 | `	pData->nCur = 0;` |
|  157 |   94 | `	pData->bReadOnly = 0;` |
|  157 |   95 | `	if( iType == PH7_IO_STREAM_MEMORY ){` |
|    - |   96 | `		/* Nothing else to set up: the buffer is the stream */` |
|   86 |   97 | `	}else if( iType == PH7_IO_STREAM_OUTPUT ){` |
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
|  157 |  123 | `	pData->pVm = pVm;` |
|  157 |  124 | `	return pData;` |
|   81 |  125 | `}` |
|    - |  126 | `/*` |
|    - |  127 | ` * Implementation of the php:// IO streams routines` |
|    - |  128 | ` * Status:` |
|    - |  129 | ` *   Stable.` |
|    - |  130 | ` */` |
|    - |  131 | `/* int (*xOpen)(const char *,int,ph7_value *,void **) */` |
|  126 |  132 | `static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)` |
|    5 |  133 | `{` |
|    - |  134 | `	ph7_stream_data *pData;` |
|    - |  135 | `	SyString sStream;` |
|  131 |  136 | `	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));` |
|    - |  137 | `	/* Trim leading and trailing white spaces */` |
|  131 |  138 | `	SyStringFullTrim(&sStream);` |
|    - |  139 | `	/* Stream to open */` |
|  131 |  140 | `	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){` |
|  ! 0 |  141 | `		iMode = PH7_IO_STREAM_STDIN;` |
|  131 |  142 | `	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){` |
|    3 |  143 | `		iMode = PH7_IO_STREAM_OUTPUT;` |
|  130 |  144 | `	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){` |
|  ! 0 |  145 | `		iMode = PH7_IO_STREAM_STDOUT;` |
|  129 |  146 | `	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){` |
|  ! 0 |  147 | `		iMode = PH7_IO_STREAM_STDERR;` |
|  124 |  148 | `	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0` |
|   68 |  149 | `	       \|\| SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){` |
|    - |  150 | `		/* php://memory and php://temp (PHL keeps temp fully in memory —` |
|    - |  151 | `		 * php's 2MB disk spill is a memory-pressure detail, recorded) */` |
|  129 |  152 | `		iMode = PH7_IO_STREAM_MEMORY;` |
|   67 |  153 | `	}else{` |
|    - |  154 | `		/* unknown stream name */` |
|  ! 0 |  155 | `		return -1;` |
|    - |  156 | `	}` |
|    - |  157 | `	/* Create our handle */` |
|  131 |  158 | `	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);` |
|  131 |  159 | `	if( pData == 0 ){` |
|  ! 0 |  160 | `		return -1;` |
|    - |  161 | `	}` |
|    - |  162 | `	/* Make the handle public */` |
|  131 |  163 | `	*ppHandle = (void *)pData;` |
|  131 |  164 | `	return PH7_OK;` |
|   68 |  165 | `}` |
|    - |  166 | `/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */` |
|  142 |  167 | `static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)` |
|    3 |  168 | `{` |
|  145 |  169 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  145 |  170 | `	if( pData == 0 ){` |
|  ! 0 |  171 | `		return -1;` |
|    - |  172 | `	}` |
|  145 |  173 | `	if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|  145 |  174 | `		sxu32 nAvail = SyBlobLength(&pData->sMem);` |
|    - |  175 | `		sxu32 nRead;` |
|  145 |  176 | `		if( pData->nCur >= nAvail ){` |
|   53 |  177 | `			return 0; /* EOF */` |
|    - |  178 | `		}` |
|   95 |  179 | `		nRead = nAvail - pData->nCur;` |
|   95 |  180 | `		if( (ph7_int64)nRead > nDatatoRead ){` |
|   12 |  181 | `			nRead = (sxu32)nDatatoRead;` |
|    5 |  182 | `		}` |
|   95 |  183 | `		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);` |
|   95 |  184 | `		pData->nCur += nRead;` |
|   95 |  185 | `		return (ph7_int64)nRead;` |
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
|   74 |  216 | `}` |
|    - |  217 | `/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */` |
|  102 |  218 | `static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)` |
|    3 |  219 | `{` |
|  105 |  220 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|  105 |  221 | `	if( pData == 0 ){` |
|  ! 0 |  222 | `		return -1;` |
|    - |  223 | `	}` |
|  105 |  224 | `	if( pData->iType == PH7_IO_STREAM_STDIN ){` |
|    - |  225 | `		/* Forbidden */` |
|  ! 0 |  226 | `		return -1;` |
|  105 |  227 | `	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){` |
|    - |  228 | `		sxu32 nLen,nEnd;` |
|   93 |  229 | `		if( pData->bReadOnly ){` |
|  ! 0 |  230 | `			return -1;` |
|    - |  231 | `		}` |
|   93 |  232 | `		nLen = SyBlobLength(&pData->sMem);` |
|   93 |  233 | `		if( pData->nCur > nLen ){` |
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
|   93 |  245 | `		nEnd = pData->nCur + (sxu32)nWrite;` |
|   93 |  246 | `		if( pData->nCur < nLen ){` |
|    - |  247 | `			/* overwrite in place up to the current end */` |
|    8 |  248 | `			sxu32 nOver = nLen - pData->nCur;` |
|    8 |  249 | `			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }` |
|   11 |  250 | `			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);` |
|   11 |  251 | `			if( nEnd > nLen ){` |
|  ! 0 |  252 | `				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){` |
|  ! 0 |  253 | `					return -1;` |
|    - |  254 | `				}` |
|  ! 0 |  255 | `			}` |
|    5 |  256 | `		}else{` |
|   87 |  257 | `			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){` |
|  ! 0 |  258 | `				return -1;` |
|    - |  259 | `			}` |
|    - |  260 | `		}` |
|   93 |  261 | `		pData->nCur = nEnd;` |
|   93 |  262 | `		return nWrite;` |
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
|   54 |  298 | `}` |
|    - |  299 | `/* void (*xClose)(void *) */` |
|   64 |  300 | `static void PHPStreamData_Close(void *pHandle)` |
|    4 |  301 | `{` |
|   68 |  302 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    - |  303 | `	ph7_vm *pVm;` |
|   68 |  304 | `	if( pData == 0 ){` |
|  ! 0 |  305 | `		return;` |
|    - |  306 | `	}` |
|   68 |  307 | `	pVm = pData->pVm;` |
|   68 |  308 | `	SyBlobRelease(&pData->sMem);` |
|    - |  309 | `	/* Free the instance */` |
|   68 |  310 | `	SyMemBackendFree(&pVm->sAllocator,pData);` |
|   36 |  311 | `}` |
|    - |  312 | `/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */` |
|  118 |  313 | `static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)` |
|    3 |  314 | `{` |
|  121 |  315 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    - |  316 | `	ph7_int64 iNew;` |
|  121 |  317 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|  ! 0 |  318 | `		return -1;` |
|    - |  319 | `	}` |
|  121 |  320 | `	switch(whence){` |
|   13 |  321 | `	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;` |
|    5 |  322 | `	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;` |
|  107 |  323 | `	default:            iNew = iOfft; break;` |
|    - |  324 | `	}` |
|  121 |  325 | `	if( iNew < 0 ){` |
|  ! 0 |  326 | `		return -1;` |
|    - |  327 | `	}` |
|  121 |  328 | `	pData->nCur = (sxu32)iNew;` |
|  121 |  329 | `	return PH7_OK;` |
|   62 |  330 | `}` |
|    - |  331 | `/* ph7_int64 (*xTell)(void *); MEMORY type only */` |
|   34 |  332 | `static ph7_int64 PHPStreamData_Tell(void *pHandle)` |
|    2 |  333 | `{` |
|   36 |  334 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|   36 |  335 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY ){` |
|  ! 0 |  336 | `		return -1;` |
|    - |  337 | `	}` |
|   36 |  338 | `	return (ph7_int64)pData->nCur;` |
|   19 |  339 | `}` |
|    - |  340 | `/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */` |
|    2 |  341 | `static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)` |
|    1 |  342 | `{` |
|    3 |  343 | `	ph7_stream_data *pData = (ph7_stream_data *)pHandle;` |
|    3 |  344 | `	if( pData == 0 \|\| pData->iType != PH7_IO_STREAM_MEMORY \|\| pData->bReadOnly ){` |
|  ! 0 |  345 | `		return -1;` |
|    - |  346 | `	}` |
|    3 |  347 | `	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){` |
|    - |  348 | `		/* shrink in place: the blob keeps its allocation */` |
|    3 |  349 | `		pData->sMem.nByte = (sxu32)nLen;` |
|    2 |  350 | `	}else{` |
|    - |  351 | `		static const char zZero[64] = {0};` |
|  ! 0 |  352 | `		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){` |
|  ! 0 |  353 | `			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));` |
|  ! 0 |  354 | `			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }` |
|  ! 0 |  355 | `			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){` |
|  ! 0 |  356 | `				return -1;` |
|    - |  357 | `			}` |
|  ! 0 |  358 | `		}` |
|    - |  359 | `	}` |
|    3 |  360 | `	return PH7_OK;` |
|    2 |  361 | `}` |
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
|    5 |  492 | `	BOOL bBinary = (strchr(zMode,'b') != NULL);` |
|    - |  493 |  |
|    - |  494 | `	/* Set up security attributes for pipe inheritance */` |
|    5 |  495 | `	sa.nLength = sizeof(SECURITY_ATTRIBUTES);` |
|    5 |  496 | `	sa.bInheritHandle = TRUE;` |
|    5 |  497 | `	sa.lpSecurityDescriptor = NULL;` |
|    - |  498 |  |
|    - |  499 | `	/* Create pipes for child process I/O */` |
|    5 |  500 | `	if( bRead ){` |
|    - |  501 | `		/* Reading from child's stdout */` |
|    5 |  502 | `		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){` |
|  ! 0 |  503 | `			return NULL;` |
|    - |  504 | `		}` |
|    - |  505 | `		/* Ensure read handle is not inherited */` |
|    5 |  506 | `		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);` |
|    5 |  507 | `		hReadPipe = hChildStdoutRd;` |
|    5 |  508 | `		*phPipe = hChildStdoutRd;` |
|    5 |  509 | `	}else{` |
|    - |  510 | `		/* Writing to child's stdin */` |
|    1 |  511 | `		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){` |
|  ! 0 |  512 | `			return NULL;` |
|    - |  513 | `		}` |
|    - |  514 | `		/* Ensure write handle is not inherited */` |
|    1 |  515 | `		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);` |
|    1 |  516 | `		hWritePipe = hChildStdinWr;` |
|    1 |  517 | `		*phPipe = hChildStdinWr;` |
|    - |  518 | `	}` |
|    - |  519 |  |
|    - |  520 | `	/* Convert command to wide string */` |
|    - |  521 | `	{` |
|    5 |  522 | `		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);` |
|    5 |  523 | `		if( nLen <= 0 ){` |
|  ! 0 |  524 | `			goto cleanup_pipes;` |
|    - |  525 | `		}` |
|    5 |  526 | `		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));` |
|    5 |  527 | `		if( !zWideCmd ){` |
|  ! 0 |  528 | `			goto cleanup_pipes;` |
|    - |  529 | `		}` |
|    5 |  530 | `		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);` |
|    - |  531 | `	}` |
|    - |  532 |  |
|    - |  533 | `	/* Set up process startup info */` |
|    5 |  534 | `	ZeroMemory(&si, sizeof(si));` |
|    5 |  535 | `	si.cb = sizeof(si);` |
|    5 |  536 | `	si.dwFlags = STARTF_USESTDHANDLES \| STARTF_USESHOWWINDOW;` |
|    5 |  537 | `	si.wShowWindow = SW_HIDE; /* Hide console window */` |
|    5 |  538 | `	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;` |
|    5 |  539 | `	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);` |
|    5 |  540 | `	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);` |
|    - |  541 |  |
|    5 |  542 | `	ZeroMemory(&pi, sizeof(pi));` |
|    - |  543 |  |
|    - |  544 | `	/* Create the child process */` |
|    5 |  545 | `	if( !CreateProcessW(` |
|    - |  546 | `		NULL,           /* Application name */` |
|    - |  547 | `		zWideCmd,       /* Command line */` |
|    - |  548 | `		NULL,           /* Process security attributes */` |
|    - |  549 | `		NULL,           /* Thread security attributes */` |
|    - |  550 | `		TRUE,           /* Inherit handles */` |
|    - |  551 | `		CREATE_NO_WINDOW, /* Creation flags - no console window */` |
|    - |  552 | `		NULL,           /* Environment */` |
|    - |  553 | `		NULL,           /* Current directory */` |
|    - |  554 | `		&si,            /* Startup info */` |
|    - |  555 | `		&pi             /* Process info */` |
|    - |  556 | `	)){` |
|  ! 0 |  557 | `		goto cleanup_all;` |
|    - |  558 | `	}` |
|    - |  559 |  |
|    - |  560 | `	/* Close handles we don't need in parent */` |
|    5 |  561 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|    5 |  562 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|    - |  563 |  |
|    - |  564 | `	/* Close thread handle (we only need process handle) */` |
|    5 |  565 | `	CloseHandle(pi.hThread);` |
|    - |  566 |  |
|    - |  567 | `	/* Store process handle for later waiting */` |
|    5 |  568 | `	*phProcess = pi.hProcess;` |
|    - |  569 |  |
|    - |  570 | `	/* Convert OS handle to C file descriptor, then to FILE*. The TRANSLATION mode` |
|    - |  571 | `	 * is the caller's, not a constant: cmd.exe writes CRLF, and a descriptor` |
|    - |  572 | `	 * opened _O_TEXT eats every CR on the way in. php picks it per call —` |
|    - |  573 | `	 * "rb" for exec/system/passthru, so their output is byte-exact, "rt" for` |
|    - |  574 | `	 * shell_exec, and the script's own mode for popen() — and this used to` |
|    - |  575 | ``	 * hardcode _O_TEXT for all of them, so `passthru('type file.bin')` lost every`` |
|    - |  576 | `	 * 0x0D byte and system()'s output came back LF-only where php's is CRLF. */` |
|    5 |  577 | `	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),` |
|    - |  578 | `	                     (bRead ? _O_RDONLY : _O_WRONLY)` |
|    - |  579 | `	                     \| (bBinary ? _O_BINARY : _O_TEXT));` |
|    5 |  580 | `	if( fd == -1 ){` |
|  ! 0 |  581 | `		CloseHandle(pi.hProcess);` |
|  ! 0 |  582 | `		*phProcess = NULL;` |
|  ! 0 |  583 | `		goto cleanup_all;` |
|    - |  584 | `	}` |
|    - |  585 |  |
|    - |  586 | `	/* The stream's own translation has to agree with the descriptor's */` |
|    5 |  587 | `	pFile = _fdopen(fd, bBinary ? (bRead ? "rb" : "wb") : (bRead ? "rt" : "wt"));` |
|    5 |  588 | `	if( !pFile ){` |
|  ! 0 |  589 | `		_close(fd); /* This will also close the underlying handle */` |
|  ! 0 |  590 | `		CloseHandle(pi.hProcess);` |
|  ! 0 |  591 | `		*phProcess = NULL;` |
|  ! 0 |  592 | `		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|  ! 0 |  593 | `		return NULL;` |
|    - |  594 | `	}` |
|    - |  595 |  |
|    5 |  596 | `	HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    5 |  597 | `	return pFile;` |
|    - |  598 |  |
|    - |  599 | `cleanup_all:` |
|  ! 0 |  600 | `	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);` |
|    - |  601 | `cleanup_pipes:` |
|  ! 0 |  602 | `	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);` |
|  ! 0 |  603 | `	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);` |
|  ! 0 |  604 | `	if( hChildStdinRd ) CloseHandle(hChildStdinRd);` |
|  ! 0 |  605 | `	if( hChildStdinWr ) CloseHandle(hChildStdinWr);` |
|  ! 0 |  606 | `	return NULL;` |
|    5 |  607 | `}` |
|    - |  608 |  |
|    - |  609 | `/*` |
|    - |  610 | ` * Custom Windows pclose implementation that properly waits for process completion.` |
|    - |  611 | ` */` |
|    - |  612 | `static int WinPclose(FILE *pFile, HANDLE hProcess)` |
|    5 |  613 | `{` |
|    5 |  614 | `	DWORD dwExitCode = 0;` |
|    - |  615 | `	int status;` |
|    - |  616 |  |
|    - |  617 | `	/* Close the FILE* (this closes the pipe) */` |
|    5 |  618 | `	fclose(pFile);` |
|    - |  619 |  |
|    5 |  620 | `	if( hProcess ){` |
|    - |  621 | `		/* Wait for the process to complete */` |
|    5 |  622 | `		WaitForSingleObject(hProcess, INFINITE);` |
|    - |  623 |  |
|    5 |  624 | `		if( GetExitCodeProcess(hProcess, &dwExitCode) ){` |
|    5 |  625 | `			status = (int)dwExitCode;` |
|    5 |  626 | `		}else{` |
|  ! 0 |  627 | `			status = -1;` |
|    - |  628 | `		}` |
|    - |  629 |  |
|    - |  630 | `		/* Close process handle */` |
|    5 |  631 | `		CloseHandle(hProcess);` |
|    5 |  632 | `	}else{` |
|  ! 0 |  633 | `		status = -1;` |
|    - |  634 | `	}` |
|    - |  635 |  |
|    5 |  636 | `	return status;` |
|    5 |  637 | `}` |
|    - |  638 | `#endif /* __WINNT__ */` |
|    - |  639 | `/*` |
|    - |  640 | ` * Open a pipe to a process.` |
|    - |  641 | ` * This is called internally by popen(), not through the stream device interface.` |
|    - |  642 | ` */` |
| 4820 |  643 | `static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)` |
|    5 |  644 | `{` |
|    - |  645 | `	pipe_private *pPipe;` |
|    - |  646 | `	FILE *pFile;` |
| 4825 |  647 | `	if( pVm == 0 \|\| zCommand == 0 \|\| zMode == 0 ){` |
|  ! 0 |  648 | `		return 0;` |
|    - |  649 | `	}` |
|    - |  650 | `	/* Validate mode - only 'r' or 'w' allowed */` |
| 4825 |  651 | `	if( zMode[0] != 'r' && zMode[0] != 'w' ){` |
|  ! 0 |  652 | `		return 0;` |
|    - |  653 | `	}` |
|    - |  654 | `	/* Open the pipe using system popen */` |
|    - |  655 | `#ifdef __WINNT__` |
|    - |  656 | `	{` |
|    - |  657 | `		/* Build cmd.exe command wrapper */` |
|    5 |  658 | `		const char *zShellPrefix = "cmd.exe /c \"";` |
|    5 |  659 | `		const char *zShellSuffix = "\"";` |
|    5 |  660 | `		size_t nPrefix = strlen(zShellPrefix);` |
|    5 |  661 | `		size_t nSuffix = strlen(zShellSuffix);` |
|    5 |  662 | `		size_t nCmd = strlen(zCommand);` |
|    5 |  663 | `		size_t nQuotes = 0;` |
|    5 |  664 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|    5 |  665 | `			if (zCommand[i] == '"') nQuotes++;` |
|    5 |  666 | `		}` |
|    5 |  667 | `		size_t nCmdEsc = nCmd + nQuotes;` |
|    5 |  668 | `		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));` |
|    5 |  669 | `		if (zCmdEsc == NULL) {` |
|  ! 0 |  670 | `			return 0;` |
|    - |  671 | `		}` |
|    - |  672 | `		/* Escape quotes in command */` |
|    5 |  673 | `		size_t j = 0;` |
|    5 |  674 | `		for (size_t i = 0; i < nCmd; ++i) {` |
|    5 |  675 | `			char ch = zCommand[i];` |
|    5 |  676 | `			if (ch == '"') {` |
|    4 |  677 | `				zCmdEsc[j++] = '^';` |
|    4 |  678 | `				zCmdEsc[j++] = '"';` |
|    4 |  679 | `			} else {` |
|    5 |  680 | `				zCmdEsc[j++] = ch;` |
|    - |  681 | `			}` |
|    5 |  682 | `		}` |
|    5 |  683 | `		zCmdEsc[j] = '\0';` |
|    5 |  684 | `		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;` |
|    5 |  685 | `		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);` |
|    5 |  686 | `		if (zWinCmd == NULL) {` |
|  ! 0 |  687 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|  ! 0 |  688 | `			return 0;` |
|    - |  689 | `		}` |
|    5 |  690 | `		memcpy(zWinCmd, zShellPrefix, nPrefix);` |
|    5 |  691 | `		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);` |
|    5 |  692 | `		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);` |
|    5 |  693 | `		zWinCmd[nTotal - 1] = '\0';` |
|    - |  694 | `		/* Allocate pipe structure early so we can store handles */` |
|    5 |  695 | `		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
|    5 |  696 | `		if( pPipe == 0 ){` |
|  ! 0 |  697 | `			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|  ! 0 |  698 | `			SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|  ! 0 |  699 | `			return 0;` |
|    - |  700 | `		}` |
|    - |  701 | `		/* Use our custom WinPopen that properly tracks the process handle */` |
|    5 |  702 | `		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);` |
|    5 |  703 | `		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);` |
|    5 |  704 | `		SyMemBackendFree(&pVm->sAllocator, zWinCmd);` |
|    5 |  705 | `		if( pFile == 0 ){` |
|  ! 0 |  706 | `			SyMemBackendFree(&pVm->sAllocator, pPipe);` |
|  ! 0 |  707 | `			return 0;` |
|    - |  708 | `		}` |
|    - |  709 | `		/* Initialize remaining fields */` |
|    5 |  710 | `		pPipe->pFile = pFile;` |
|    5 |  711 | `		pPipe->pVm = pVm;` |
|    5 |  712 | `		pPipe->iMode = zMode[0];` |
|    - |  713 | `	}` |
|    - |  714 | `#elif defined(__UNIXES__) /* Unix */` |
|    - |  715 | `	/* The mode goes to popen(3) VERBATIM, exactly as php hands it its own: a mode` |
|    - |  716 | `	 * popen(3) refuses (anything but "r"/"w" — 'b' is a Windows translation flag` |
|    - |  717 | `	 * with nothing to translate here) is an open FAILURE, which is php's answer` |
|    - |  718 | `	 * for it too. */` |
| 4820 |  719 | `	pFile = popen(zCommand, zMode);` |
| 4820 |  720 | `	if( pFile == 0 ){` |
|    2 |  721 | `		return 0;` |
|    - |  722 | `	}` |
|    - |  723 | `	/* Allocate pipe private structure */` |
| 4818 |  724 | `	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));` |
| 4818 |  725 | `	if( pPipe == 0 ){` |
|    - |  726 | `		/* Out of memory, close the pipe */` |
|  ! 0 |  727 | `		pclose(pFile);` |
|  ! 0 |  728 | `		return 0;` |
|    - |  729 | `	}` |
|    - |  730 | `	/* Initialize the structure */` |
| 4818 |  731 | `	pPipe->pFile = pFile;` |
| 4818 |  732 | `	pPipe->pVm = pVm;` |
| 4818 |  733 | `	pPipe->iMode = zMode[0];` |
|    - |  734 | `#else /* OS_OTHER: no process pipes on this platform */` |
|    - |  735 | `	(void)pFile;` |
|    - |  736 | `	return 0;` |
|    - |  737 | `#endif` |
| 4823 |  738 | `	return pPipe;` |
| 2415 |  739 | `}` |
|    - |  740 | `/*` |
|    - |  741 | ` * Close a pipe and return the exit status of the process.` |
|    - |  742 | ` * Returns the exit status, or -1 on error.` |
|    - |  743 | ` */` |
| 4792 |  744 | `static int PipeClose(pipe_private *pPipe)` |
|    5 |  745 | `{` |
|    - |  746 | `	int status;` |
|    - |  747 | `	ph7_vm *pVm;` |
| 4797 |  748 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|  ! 0 |  749 | `		return -1;` |
|    - |  750 | `	}` |
| 4797 |  751 | `	pVm = pPipe->pVm;` |
|    - |  752 | `	/* Close the pipe and get exit status */` |
|    - |  753 | `#ifdef __WINNT__` |
|    - |  754 | `	/* Use our custom WinPclose that properly waits for process completion */` |
|    5 |  755 | `	status = WinPclose(pPipe->pFile, pPipe->hProcess);` |
|    - |  756 | `#elif defined(__UNIXES__)` |
| 4792 |  757 | `	status = pclose(pPipe->pFile);` |
|    - |  758 | `	/* pclose() answers waitpid()'s raw status. php (php_stream_pclose) translates` |
|    - |  759 | `	 * exactly ONE case of it — a normal exit becomes its exit CODE — and hands the` |
|    - |  760 | `	 * raw word back for every other, so a process killed by a signal reports the` |
|    - |  761 | ``	 * SIGNAL number: `kill -TERM $$` is 15, `kill -9 $$` is 9. PHL used to add the`` |
|    - |  762 | `	 * shell's own 128 to it (143, 137), a number the same status word can also` |
|    - |  763 | ``	 * mean as an ordinary `exit 143`, and answered -1 for a stopped child. This is`` |
|    - |  764 | `	 * pclose()'s answer and, through it, exec()/system()/passthru()'s` |
|    - |  765 | `	 * $result_code. */` |
| 4792 |  766 | `	if( status != -1 && WIFEXITED(status) ){` |
| 4788 |  767 | `		status = WEXITSTATUS(status);` |
| 2394 |  768 | `	}` |
|    - |  769 | `#else /* OS_OTHER: no process pipes on this platform */` |
|    - |  770 | `	status = -1;` |
|    - |  771 | `#endif` |
|    - |  772 | `	/* Free the structure */` |
| 4797 |  773 | `	SyMemBackendFree(&pVm->sAllocator, pPipe);` |
| 4797 |  774 | `	return status;` |
| 2401 |  775 | `}` |
|    - |  776 | `/*` |
|    - |  777 | ` * Pipe stream xClose implementation.` |
|    - |  778 | ` * Note: This is called by fclose(), not pclose().` |
|    - |  779 | ` * It closes the pipe but does not return the exit status.` |
|    - |  780 | ` */` |
|  102 |  781 | `static void PipeStream_Close(void *pHandle)` |
|    4 |  782 | `{` |
|  106 |  783 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|  106 |  784 | `	if( pPipe ){` |
|  106 |  785 | `		PipeClose(pPipe);` |
|   51 |  786 | `	}` |
|  106 |  787 | `}` |
|    - |  788 | `/*` |
|    - |  789 | ` * Pipe stream xRead implementation.` |
|    - |  790 | ` */` |
| 7288 |  791 | `static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)` |
|    4 |  792 | `{` |
| 7292 |  793 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    - |  794 | `	size_t nRead;` |
| 7292 |  795 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|  ! 0 |  796 | `		return -1;` |
|    - |  797 | `	}` |
| 7292 |  798 | `	if( pPipe->iMode != 'r' ){` |
|    - |  799 | `		/* Cannot read from a write-only pipe */` |
|  ! 0 |  800 | `		return -1;` |
|    - |  801 | `	}` |
| 7292 |  802 | `	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);` |
| 7292 |  803 | `	if( nRead == 0 ){` |
| 4802 |  804 | `		if( feof(pPipe->pFile) ){` |
| 4802 |  805 | `			return 0; /* EOF */` |
|    - |  806 | `		}` |
|  ! 0 |  807 | `		return -1; /* Error */` |
|    - |  808 | `	}` |
| 2494 |  809 | `	return (ph7_int64)nRead;` |
| 3648 |  810 | `}` |
|    - |  811 | `/*` |
|    - |  812 | ` * Pipe stream xWrite implementation.` |
|    - |  813 | ` */` |
|    4 |  814 | `static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)` |
|  ! 0 |  815 | `{` |
|    4 |  816 | `	pipe_private *pPipe = (pipe_private *)pHandle;` |
|    - |  817 | `	size_t nWritten;` |
|    4 |  818 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|  ! 0 |  819 | `		return -1;` |
|    - |  820 | `	}` |
|    4 |  821 | `	if( pPipe->iMode != 'w' ){` |
|    - |  822 | `		/* Cannot write to a read-only pipe */` |
|  ! 0 |  823 | `		return -1;` |
|    - |  824 | `	}` |
|    4 |  825 | `	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);` |
|    4 |  826 | `	if( nWritten == 0 && nWrite > 0 ){` |
|  ! 0 |  827 | `		return -1; /* Error */` |
|    - |  828 | `	}` |
|    4 |  829 | `	return (ph7_int64)nWritten;` |
|    2 |  830 | `}` |
|    - |  831 | `/* Export the pipe:// stream (used internally, not registered as a URI scheme) */` |
|    - |  832 | `static const ph7_io_stream sPipe_Stream = {` |
|    - |  833 | `	"pipe",` |
|    - |  834 | `	PH7_IO_STREAM_VERSION,` |
|    - |  835 | `	0,  /* xOpen - not used, pipes opened via PipeOpen() */` |
|    - |  836 | `	0,  /* xOpenDir */` |
|    - |  837 | `	PipeStream_Close,  /* xClose */` |
|    - |  838 | `	0,  /* xCloseDir */` |
|    - |  839 | `	PipeStream_Read,   /* xRead */` |
|    - |  840 | `	0,  /* xReadDir */` |
|    - |  841 | `	PipeStream_Write,  /* xWrite */` |
|    - |  842 | `	0,  /* xSeek */` |
|    - |  843 | `	0,  /* xLock */` |
|    - |  844 | `	0,  /* xRewindDir */` |
|    - |  845 | `	0,  /* xTell */` |
|    - |  846 | `	0,  /* xTrunc */` |
|    - |  847 | `	0,  /* xSync */` |
|    - |  848 | `	0   /* xStat */` |
|    - |  849 | `};` |
|    - |  850 | `/*` |
|    - |  851 | ` * Return TRUE if we are dealing with the pipe:// stream.` |
|    - |  852 | ` * FALSE otherwise.` |
|    - |  853 | ` */` |
| 4652 |  854 | `static int is_pipe_stream(const ph7_io_stream *pStream)` |
|    5 |  855 | `{` |
| 4657 |  856 | `	return pStream == &sPipe_Stream;` |
|    5 |  857 | `}` |
|    - |  858 | `/*` |
|    - |  859 | ` * resource popen(string $command, string $mode)` |
|    - |  860 | ` *  Opens process file pointer.` |
|    - |  861 | ` * Parameters` |
|    - |  862 | ` *  $command` |
|    - |  863 | ` *   The command to execute. Passed to the system shell.` |
|    - |  864 | ` *  $mode` |
|    - |  865 | ` *   The mode parameter specifies the type of access you require to the stream.` |
|    - |  866 | ` *   'r' - Open for reading (read from the command's stdout).` |
|    - |  867 | ` *   'w' - Open for writing (write to the command's stdin).` |
|    - |  868 | ` * Return` |
|    - |  869 | ` *  Returns a file pointer on success, or FALSE on error.` |
|    - |  870 | ` */` |
|    - |  871 | `/*` |
|    - |  872 | `` * The longest command line the platform's shell accepts — php's `cmd_max_len`,`` |
|    - |  873 | ` * which both escapers refuse to exceed. php reads it once at startup from` |
|    - |  874 | ` * sysconf(_SC_ARG_MAX) and hardcodes cmd.exe's constant on Windows.` |
|    - |  875 | ` */` |
|  110 |  876 | `static sxu32 ShellMaxCmdLen(void)` |
|    1 |  877 | `{` |
|    - |  878 | `#ifdef __WINNT__` |
|    - |  879 | `	/* An escaped command runs through cmd.exe, whose limit is a constant. */` |
|    1 |  880 | `	return 8192;` |
|    - |  881 | `#elif defined(__UNIXES__) && defined(_SC_ARG_MAX)` |
|  110 |  882 | `	long iMax = sysconf(_SC_ARG_MAX);` |
|  110 |  883 | `	if( iMax <= 0 ){` |
|  ! 0 |  884 | `		return 4096;   /* php's _POSIX_ARG_MAX fallback */` |
|    - |  885 | `	}` |
|  110 |  886 | `	return (sxu32)iMax;` |
|    - |  887 | `#else` |
|    - |  888 | `	return 4096;` |
|    - |  889 | `#endif` |
|   56 |  890 | `}` |
|    - |  891 | `/*` |
|    - |  892 | ` * How the two escapers WALK their argument, and the one thing they share.` |
|    - |  893 | ` *` |
|    - |  894 | ` * php walks it with php_mblen(), the process LC_CTYPE's multibyte reader: a` |
|    - |  895 | ` * well-formed sequence is copied through untouched (a metacharacter's byte value` |
|    - |  896 | ` * inside one is NOT a metacharacter), and a byte the encoding cannot start a` |
|    - |  897 | ` * character with is DROPPED. That reader's answer is platform-shaped, and this` |
|    - |  898 | ` * follows it on both, because it is what php answers on each:` |
|    - |  899 | ` *` |
|    - |  900 | ` *   POSIX    php picks LC_CTYPE up from the environment at startup, so the` |
|    - |  901 | ` *            everyday answer is a UTF-8 one — a well-formed sequence rides` |
|    - |  902 | ` *            through and an ill-formed byte is dropped. PHL is UTF-8-only (§10)` |
|    - |  903 | ` *            and has no setlocale, so PH7_Utf8ReadStrict IS that reader.` |
|    - |  904 | ` *            (php in the "C" locale glibc falls back to drops every byte >= 0x80` |
|    - |  905 | ` *            instead, which is why the corpus guards this half on the oracle's` |
|    - |  906 | ` *            own LC_CTYPE rather than pinning it unconditionally.)` |
|    - |  907 | ` *   Windows  php reports LC_CTYPE "C", and MSVCRT's C locale is SINGLE-BYTE, not` |
|    - |  908 | ` *            ASCII: mblen() answers 1 for every byte, so nothing is ever dropped` |
|    - |  909 | `` *            and `\xFF` reaches the escape table below. Verified against php`` |
|    - |  910 | ` *            8.5.8 on the gate VM, which does have an oracle — walking UTF-8` |
|    - |  911 | ` *            there instead deleted bytes php keeps.` |
|    - |  912 | ` *` |
|    - |  913 | ` * Neither escaper can see a NUL byte: php parses both parameters with` |
|    - |  914 | ` * Z_PARAM_PATH and the central screen (VmBuiltinPathMask) refuses one first.` |
|    - |  915 | ` */` |
|  556 |  916 | `static int ShellCharIsWellFormed(const unsigned char *zIn,sxu32 nLeft,sxu32 *pnSeq)` |
|    1 |  917 | `{` |
|    - |  918 | `#ifdef __WINNT__` |
|    - |  919 | `	SXUNUSED(zIn);` |
|    - |  920 | `	SXUNUSED(nLeft);` |
|    1 |  921 | `	*pnSeq = 1;` |
|    1 |  922 | `	return 1;` |
|    - |  923 | `#else` |
|  556 |  924 | `	return PH7_Utf8ReadStrict(zIn,nLeft,pnSeq) >= 0;` |
|    - |  925 | `#endif` |
|    1 |  926 | `}` |
|    - |  927 | `/*` |
|    - |  928 | ` * php's escapeshellarg(): wrap the whole argument in quotes the shell does not` |
|    - |  929 | ` * look inside, and neutralise the one byte that could end them.` |
|    - |  930 | ` *` |
|    - |  931 | `` * POSIX: single quotes, and a `'` becomes `'\''` — close, escape, reopen.`` |
|    - |  932 | `` * Windows: double quotes; there is no in-quote escape for `"` on cmd.exe, so php`` |
|    - |  933 | `` * REPLACES `"` (and `%`/`!`, which cmd.exe still expands inside quotes) with a`` |
|    - |  934 | ` * space, and doubles a trailing ODD run of backslashes so the last one escapes` |
|    - |  935 | ` * itself rather than the closing quote.` |
|    - |  936 | ` */` |
|   44 |  937 | `static void ShellEscapeArg(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|    1 |  938 | `{` |
|   45 |  939 | `	sxu32 i = 0;` |
|    - |  940 | `#ifdef __WINNT__` |
|    1 |  941 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|    - |  942 | `#else` |
|   44 |  943 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|    - |  944 | `#endif` |
|  221 |  945 | `	while( i < nLen ){` |
|  177 |  946 | `		sxu32 nSeq = 1;` |
|  177 |  947 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|    - |  948 | `			/* Ill-formed: php skips the byte rather than escaping it */` |
|   18 |  949 | `			i += nSeq;` |
|   22 |  950 | `			continue;` |
|    - |  951 | `		}` |
|  159 |  952 | `		if( nSeq > 1 ){` |
|    8 |  953 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|    8 |  954 | `			i += nSeq;` |
|    8 |  955 | `			continue;` |
|    - |  956 | `		}` |
|    - |  957 | `#ifdef __WINNT__` |
|    1 |  958 | `		if( zIn[i] == '"' \|\| zIn[i] == '%' \|\| zIn[i] == '!' ){` |
|    1 |  959 | `			SyBlobAppend(pOut," ",sizeof(char));` |
|    1 |  960 | `		}else{` |
|    1 |  961 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|    - |  962 | `		}` |
|    - |  963 | `#else` |
|  150 |  964 | `		if( zIn[i] == '\'' ){` |
|    8 |  965 | `			SyBlobAppend(pOut,"'\\'",sizeof("'\\'")-1);` |
|    4 |  966 | `		}` |
|  150 |  967 | `		SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|    - |  968 | `#endif` |
|  151 |  969 | `		i++;` |
|    1 |  970 | `	}` |
|    - |  971 | `#ifdef __WINNT__` |
|    - |  972 | `	{` |
|    - |  973 | `		/* A trailing run of backslashes would escape the closing quote if it is` |
|    - |  974 | `		 * odd; the opening quote at offset 0 stops the scan the way php's does. */` |
|    1 |  975 | `		const char *zCur = (const char *)SyBlobData(pOut);` |
|    1 |  976 | `		sxu32 nCur = SyBlobLength(pOut);` |
|    1 |  977 | `		sxu32 k = 0;` |
|    1 |  978 | `		while( k < nCur && zCur[nCur - 1 - k] == '\\' ){` |
|    1 |  979 | `			k++;` |
|    1 |  980 | `		}` |
|    1 |  981 | `		if( (k & 1) != 0 ){` |
|    1 |  982 | `			SyBlobAppend(pOut,"\\",sizeof(char));` |
|    - |  983 | `		}` |
|    - |  984 | `	}` |
|    1 |  985 | `	SyBlobAppend(pOut,"\"",sizeof(char));` |
|    - |  986 | `#else` |
|   44 |  987 | `	SyBlobAppend(pOut,"'",sizeof(char));` |
|    - |  988 | `#endif` |
|   45 |  989 | `}` |
|    - |  990 | `/*` |
|    - |  991 | ` * php's escapeshellcmd(): the argument is a COMMAND, so it is not quoted at all —` |
|    - |  992 | ` * every byte that could break out of one is prefixed with the shell's escape` |
|    - |  993 | `` * character instead (`\` on POSIX, `^` on cmd.exe).`` |
|    - |  994 | ` *` |
|    - |  995 | ` * The one shape that is not a straight escape is a quote on POSIX: php leaves a` |
|    - |  996 | ` * PAIR of them alone (the command may legitimately quote one of its own` |
|    - |  997 | `` * arguments) and escapes an unpaired one. `pPair` is php's own one-slot state for`` |
|    - |  998 | ` * that — it remembers the partner it found for the quote currently open, so the` |
|    - |  999 | ` * closing one is recognised and the pairing resets. cmd.exe has no such rule, so` |
|    - | 1000 | `` * both quote characters (and `%`/`!`) are ordinary escapes there.`` |
|    - | 1001 | ` */` |
|   64 | 1002 | `static void ShellEscapeCmd(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)` |
|    1 | 1003 | `{` |
|   65 | 1004 | `	sxu32 i = 0;` |
|    - | 1005 | `#ifndef __WINNT__` |
|   64 | 1006 | `	const unsigned char *pPair = 0;` |
|    - | 1007 | `	static const char zEsc[] = "\\";` |
|    - | 1008 | `#else` |
|    - | 1009 | `	static const char zEsc[] = "^";` |
|    - | 1010 | `#endif` |
|  445 | 1011 | `	while( i < nLen ){` |
|  381 | 1012 | `		sxu32 nSeq = 1;` |
|  381 | 1013 | `		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){` |
|   18 | 1014 | `			i += nSeq;` |
|   22 | 1015 | `			continue;` |
|    - | 1016 | `		}` |
|  363 | 1017 | `		if( nSeq > 1 ){` |
|    8 | 1018 | `			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);` |
|    8 | 1019 | `			i += nSeq;` |
|    8 | 1020 | `			continue;` |
|    - | 1021 | `		}` |
|  355 | 1022 | `		switch( zIn[i] ){` |
|    - | 1023 | `#ifndef __WINNT__` |
|   12 | 1024 | `		case '"':` |
|    - | 1025 | `		case '\'':` |
|   19 | 1026 | `			if( pPair == 0` |
|   14 | 1027 | `			 && (pPair = (const unsigned char *)memchr(&zIn[i+1],zIn[i],nLen - i - 1)) != 0 ){` |
|    - | 1028 | `				/* This quote opens a pair: leave both of them alone */` |
|   17 | 1029 | `			}else if( pPair != 0 && pPair[0] == zIn[i] ){` |
|    8 | 1030 | `				pPair = 0;   /* the partner: pairing satisfied */` |
|    4 | 1031 | `			}else{` |
|    8 | 1032 | `				SyBlobAppend(pOut,zEsc,sizeof(char));` |
|    - | 1033 | `			}` |
|   24 | 1034 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|   24 | 1035 | `			break;` |
|    - | 1036 | `#else` |
|    - | 1037 | `		/* cmd.exe expands %VAR% and !VAR! even inside quotes, and has no` |
|    - | 1038 | ``		 * in-quote escape, so all four are plain `^` escapes there. */`` |
|    - | 1039 | `		case '%':` |
|    - | 1040 | `		case '!':` |
|    - | 1041 | `		case '"':` |
|    - | 1042 | `		case '\'':` |
|    - | 1043 | `#endif` |
|   21 | 1044 | `		case '#':` |
|    - | 1045 | `		case '&':` |
|    - | 1046 | `		case ';':` |
|    - | 1047 | ``		case '`':`` |
|    - | 1048 | `		case '\|':` |
|    - | 1049 | `		case '*':` |
|    - | 1050 | `		case '?':` |
|    - | 1051 | `		case '~':` |
|    - | 1052 | `		case '<':` |
|    - | 1053 | `		case '>':` |
|    - | 1054 | `		case '^':` |
|    - | 1055 | `		case '(':` |
|    - | 1056 | `		case ')':` |
|    - | 1057 | `		case '[':` |
|    - | 1058 | `		case ']':` |
|    - | 1059 | `		case '{':` |
|    - | 1060 | `		case '}':` |
|    - | 1061 | `		case '$':` |
|    - | 1062 | `		case '\\':` |
|    - | 1063 | `		case 0x0A:` |
|    - | 1064 | `		/* php escapes 0xFF too, and this is the row that decides the walk above` |
|    - | 1065 | `		 * is worth getting right: it is unreachable under the UTF-8 walk (0xF5..` |
|    - | 1066 | `		 * 0xFF is never a lead byte, so the reader drops the byte first) and` |
|    - | 1067 | `		 * REACHED on Windows, where php's single-byte C locale hands it here —` |
|    - | 1068 | ``		 * `escapeshellcmd("a\xffb")` is `a^\xffb` on the oracle. */`` |
|    - | 1069 | `		case 0xFF:` |
|   43 | 1070 | `			SyBlobAppend(pOut,zEsc,sizeof(char));` |
|    - | 1071 | `			/* fall through */` |
|  165 | 1072 | `		default:` |
|  331 | 1073 | `			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));` |
|  330 | 1074 | `			break;` |
|    - | 1075 | `		}` |
|  355 | 1076 | `		i++;` |
|    1 | 1077 | `	}` |
|   65 | 1078 | `}` |
|    - | 1079 | `/*` |
|    - | 1080 | ` * string escapeshellarg(string $arg)` |
|    - | 1081 | ` *  Escape an argument so a shell passes it to the command as ONE word, whatever` |
|    - | 1082 | ` *  it contains.` |
|    - | 1083 | ` */` |
|   44 | 1084 | `PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1085 | `{` |
|   45 | 1086 | `	sxu32 nMax = ShellMaxCmdLen();` |
|    - | 1087 | `	const char *zArg;` |
|    - | 1088 | `	SyBlob sOut;` |
|    - | 1089 | `	int nLen;` |
|   22 | 1090 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|   45 | 1091 | `	zArg = ph7_value_to_string(apArg[0],&nLen);` |
|    - | 1092 | `	/* php's own bound: the command line has to hold the two quotes and a NUL */` |
|   45 | 1093 | `	if( nLen > 0 && (sxu32)nLen > nMax - 3 ){` |
|  ! 0 | 1094 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 1095 | `			"Argument exceeds the allowed length of %d bytes",(int)nMax);` |
|    - | 1096 | `	}` |
|   45 | 1097 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   45 | 1098 | `	ShellEscapeArg((const unsigned char *)zArg,(sxu32)nLen,&sOut);` |
|   45 | 1099 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|  ! 0 | 1100 | `		SyBlobRelease(&sOut);` |
|  ! 0 | 1101 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 1102 | `			"Escaped argument exceeds the allowed length of %d bytes",(int)nMax);` |
|    - | 1103 | `	}` |
|   45 | 1104 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   45 | 1105 | `	SyBlobRelease(&sOut);` |
|   45 | 1106 | `	return PH7_OK;` |
|   23 | 1107 | `}` |
|    - | 1108 | `/*` |
|    - | 1109 | ` * string escapeshellcmd(string $command)` |
|    - | 1110 | ` *  Escape every character that could break out of a shell command.` |
|    - | 1111 | ` */` |
|   66 | 1112 | `PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1113 | `{` |
|   67 | 1114 | `	sxu32 nMax = ShellMaxCmdLen();` |
|    - | 1115 | `	const char *zCmd;` |
|    - | 1116 | `	SyBlob sOut;` |
|    - | 1117 | `	int nLen;` |
|   33 | 1118 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|   67 | 1119 | `	zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|   67 | 1120 | `	if( nLen < 1 ){` |
|    - | 1121 | `		/* php answers "" without running the escaper at all */` |
|    3 | 1122 | `		ph7_result_string(pCtx,"",0);` |
|    3 | 1123 | `		return PH7_OK;` |
|    - | 1124 | `	}` |
|   65 | 1125 | `	if( (sxu32)nLen > nMax - 3 ){` |
|  ! 0 | 1126 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 1127 | `			"Command exceeds the allowed length of %d bytes",(int)nMax);` |
|    - | 1128 | `	}` |
|   65 | 1129 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   65 | 1130 | `	ShellEscapeCmd((const unsigned char *)zCmd,(sxu32)nLen,&sOut);` |
|   65 | 1131 | `	if( SyBlobLength(&sOut) > nMax + 1 ){` |
|  ! 0 | 1132 | `		SyBlobRelease(&sOut);` |
|  ! 0 | 1133 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 1134 | `			"Escaped command exceeds the allowed length of %d bytes",(int)nMax);` |
|    - | 1135 | `	}` |
|   65 | 1136 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   65 | 1137 | `	SyBlobRelease(&sOut);` |
|   65 | 1138 | `	return PH7_OK;` |
|   34 | 1139 | `}` |
|    - | 1140 | `/*` |
|    - | 1141 | ` * php refuses an EMPTY command in all four runners (exec/system/passthru/` |
|    - | 1142 | ` * shell_exec) — the shell would answer success for one, so the refusal is the` |
|    - | 1143 | ` * only way a script hears about a command string that came out empty.` |
|    - | 1144 | ` */` |
|    8 | 1145 | `static sxi32 ShellEmptyCommandError(ph7_context *pCtx)` |
|    1 | 1146 | `{` |
|   13 | 1147 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|    4 | 1148 | `		"%s(): Argument #1 ($command) must not be empty",ph7_function_name(pCtx));` |
|    1 | 1149 | `}` |
|    - | 1150 | `/*` |
|    - | 1151 | ` * string\|false\|null shell_exec(string $command)` |
|    - | 1152 | ` *  Execute a command via the shell and return the complete output as a string.` |
|    - | 1153 | ` * Returns NULL when the command produces no output, FALSE when the pipe cannot be` |
|    - | 1154 | ` * opened. This is what the backtick operator compiles to, exactly as in php.` |
|    - | 1155 | ` */` |
|    2 | 1156 | `PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1157 | `{` |
|    - | 1158 | `	const char *zCommand;` |
|    - | 1159 | `	pipe_private *pPipe;` |
|    - | 1160 | `	SyBlob sOut;` |
|    - | 1161 | `	char zBuf[4096];` |
|    - | 1162 | `	size_t nRead;` |
|    - | 1163 | `	int nCmdLen;` |
|    4 | 1164 | `	if( nArg < 1 ){` |
|  ! 0 | 1165 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1166 | `		return PH7_OK;` |
|    - | 1167 | `	}` |
|    4 | 1168 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|    4 | 1169 | `	if( nCmdLen < 1 ){` |
|    - | 1170 | `		/* php refuses an empty command rather than running the shell on it */` |
|    3 | 1171 | `		return ShellEmptyCommandError(pCtx);` |
|    - | 1172 | `	}` |
|    1 | 1173 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");` |
|    1 | 1174 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|    - | 1175 | `		/* php's own wording for this one; the three runners below say "Unable to` |
|    - | 1176 | `		 * fork [%s]" instead. Both used to be silent. */` |
|  ! 0 | 1177 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to execute '%s'",` |
|  ! 0 | 1178 | `			ph7_function_name(pCtx),zCommand);` |
|  ! 0 | 1179 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1180 | `		return PH7_OK;` |
|    - | 1181 | `	}` |
|    1 | 1182 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|  ! 0 | 1183 | `	for(;;){` |
|    1 | 1184 | `		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|    1 | 1185 | `		if( nRead < 1 ){` |
|    1 | 1186 | `			break;` |
|    - | 1187 | `		}` |
|    1 | 1188 | `		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);` |
|    1 | 1189 | `	}` |
|    1 | 1190 | `	PipeClose(pPipe);` |
|    1 | 1191 | `	if( SyBlobLength(&sOut) < 1 ){` |
|    - | 1192 | `		/* php answers NULL, not "", when the command printed nothing */` |
|  ! 0 | 1193 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1194 | `	}else{` |
|    1 | 1195 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    - | 1196 | `	}` |
|    1 | 1197 | `	SyBlobRelease(&sOut);` |
|    1 | 1198 | `	return PH7_OK;` |
|    3 | 1199 | `}` |
|    - | 1200 | `/*` |
|    - | 1201 | ` * php's three command RUNNERS are one routine (php_exec) with a mode, and the` |
|    - | 1202 | ` * mode decides two things: what happens to each LINE of the command's output,` |
|    - | 1203 | ` * and what the call answers.` |
|    - | 1204 | ` *` |
|    - | 1205 | ` *   exec($cmd)           keep nothing, answer the LAST line` |
|    - | 1206 | ` *   exec($cmd, $output)  append every line to the array, answer the last line` |
|    - | 1207 | ` *   system($cmd)         WRITE every line as it arrives, answer the last line` |
|    - | 1208 | ` *   passthru($cmd)       write the raw bytes, answer NULL` |
|    - | 1209 | ` *` |
|    - | 1210 | ` * "The last line" is php's: its trailing WHITESPACE is stripped — spaces and` |
|    - | 1211 | ` * tabs as much as the newline — and so is every element of $output's. A command` |
|    - | 1212 | ` * that printed nothing answers "" rather than false, which is php's documented` |
|    - | 1213 | ` * BC wart and not an error indication; the error indication is FALSE, and only` |
|    - | 1214 | ` * a pipe that could not be opened produces it.` |
|    - | 1215 | ` *` |
|    - | 1216 | ` * All three share the exit status, which is the pipe's close status (php's` |
|    - | 1217 | ` * $result_code out-param) and -1 when there was no process at all.` |
|    - | 1218 | ` */` |
|    - | 1219 | `#ifdef __WINNT__` |
|    - | 1220 | `# define SHELL_RUN_PIPE_MODE "rb"` |
|    - | 1221 | `#else` |
|    - | 1222 | `# define SHELL_RUN_PIPE_MODE "r"` |
|    - | 1223 | `#endif` |
|    - | 1224 | `#define SHELL_RUN_LAST     0   /* exec() with no $output array */` |
|    - | 1225 | `#define SHELL_RUN_ECHO     1   /* system() */` |
|    - | 1226 | `#define SHELL_RUN_COLLECT  2   /* exec() with one */` |
|    - | 1227 | `#define SHELL_RUN_RAW      3   /* passthru() */` |
|    - | 1228 | `/*` |
|    - | 1229 | ` * php's strip_trailing_whitespace(): answers the length that stays.` |
|    - | 1230 | ` */` |
|   72 | 1231 | `static sxu32 ShellStripTrailing(const char *zLine,sxu32 nLine)` |
|    2 | 1232 | `{` |
|  172 | 1233 | `	while( nLine > 0 && SyisSpace((unsigned char)zLine[nLine - 1]) ){` |
|  100 | 1234 | `		nLine--;` |
|    2 | 1235 | `	}` |
|   74 | 1236 | `	return nLine;` |
|    2 | 1237 | `}` |
|    - | 1238 | `/*` |
|    - | 1239 | ` * One complete line of output, dealt with the mode's way. The line still carries` |
|    - | 1240 | ` * its own newline: system() writes it (php hands the whole line to the output` |
|    - | 1241 | ` * layer, so an output buffer catches it like any echo), and the collector strips` |
|    - | 1242 | ` * it along with the rest of the trailing whitespace.` |
|    - | 1243 | ` */` |
|   50 | 1244 | `static sxi32 ShellHandleLine(ph7_context *pCtx,int iType,ph7_value *pArray,` |
|    - | 1245 | `	const char *zLine,sxu32 nLine)` |
|    2 | 1246 | `{` |
|   52 | 1247 | `	if( iType == SHELL_RUN_ECHO ){` |
|   12 | 1248 | `		return ph7_context_output(pCtx,zLine,(int)nLine);` |
|    - | 1249 | `	}` |
|   41 | 1250 | `	if( iType == SHELL_RUN_COLLECT && pArray ){` |
|   41 | 1251 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   41 | 1252 | `		if( pVal == 0 ){` |
|  ! 0 | 1253 | `			return PH7_OK;` |
|    - | 1254 | `		}` |
|   41 | 1255 | `		ph7_value_string(pVal,zLine,(int)ShellStripTrailing(zLine,nLine));` |
|   41 | 1256 | `		ph7_array_add_elem(pArray,0,pVal);` |
|   41 | 1257 | `		ph7_context_release_value(pCtx,pVal);` |
|   20 | 1258 | `	}` |
|   41 | 1259 | `	return PH7_OK;` |
|   27 | 1260 | `}` |
|    - | 1261 | `/*` |
|    - | 1262 | ` * Run $command through the shell in the given mode, fill the by-reference` |
|    - | 1263 | ` * out-params and set the call's result. The three builtins below are this` |
|    - | 1264 | ` * routine plus their own mode.` |
|    - | 1265 | ` */` |
|   44 | 1266 | `static sxi32 ShellRunCommand(ph7_context *pCtx,int iType,int nArg,ph7_value **apArg)` |
|    2 | 1267 | `{` |
|    - | 1268 | `	/* exec() carries $output before $result_code; the other two do not */` |
|   46 | 1269 | `	int iCodeArg = (iType == SHELL_RUN_LAST) ? 2 : 1;` |
|   46 | 1270 | `	ph7_value *pArray = 0, *pOwned = 0;` |
|    - | 1271 | `	const char *zCommand;` |
|    - | 1272 | `	pipe_private *pPipe;` |
|    - | 1273 | `	SyBlob sLine, sLast;` |
|    - | 1274 | `	char zBuf[4096];` |
|    - | 1275 | `	size_t nRead;` |
|   46 | 1276 | `	int nCmdLen, iStatus = -1;` |
|   46 | 1277 | `	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);` |
|   46 | 1278 | `	if( nCmdLen < 1 ){` |
|    7 | 1279 | `		return ShellEmptyCommandError(pCtx);` |
|    - | 1280 | `	}` |
|    - | 1281 | `	/* $output turns exec() into the collecting mode. php uses the array the` |
|    - | 1282 | `	 * caller already holds — the manual's "will append to the end of the array" —` |
|    - | 1283 | `	 * and replaces anything else with a fresh one, BEFORE running the command, so` |
|    - | 1284 | `	 * even a failed run leaves the variable an array. */` |
|   40 | 1285 | `	if( iType == SHELL_RUN_LAST && nArg > 1 ){` |
|   27 | 1286 | `		iType = SHELL_RUN_COLLECT;` |
|   27 | 1287 | `		if( ph7_value_is_array(apArg[1]) ){` |
|    2 | 1288 | `			PH7_HashmapCowSeparate(pCtx->pVm,apArg[1]);` |
|    2 | 1289 | `			pArray = apArg[1];` |
|    1 | 1290 | `		}else{` |
|   25 | 1291 | `			pOwned = pArray = ph7_context_new_array(pCtx);` |
|    - | 1292 | `		}` |
|   13 | 1293 | `	}` |
|    - | 1294 | `	/* php_exec's own mode, per platform: the three runners hand back what the` |
|    - | 1295 | `	 * command WROTE, so on Windows the CRs have to survive the pipe (where` |
|    - | 1296 | `	 * shell_exec() takes php's "rt" and does translate them). popen(3) refuses a` |
|    - | 1297 | `	 * 'b' it has nothing to translate, which is why this is not one string. */` |
|   40 | 1298 | `	pPipe = PipeOpen(pCtx->pVm,zCommand,SHELL_RUN_PIPE_MODE);` |
|   40 | 1299 | `	if( pPipe == 0 \|\| pPipe->pFile == 0 ){` |
|  ! 0 | 1300 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to fork [%s]",` |
|  ! 0 | 1301 | `			ph7_function_name(pCtx),zCommand);` |
|  ! 0 | 1302 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1303 | `	}else{` |
|   40 | 1304 | `		int bAbort = 0;   /* the output consumer asked to stop (PH7_ABORT) */` |
|   40 | 1305 | `		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|   40 | 1306 | `		SyBlobInit(&sLast,&pCtx->pVm->sAllocator);` |
|   35 | 1307 | `		for(;;){` |
|   78 | 1308 | `			nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);` |
|   78 | 1309 | `			if( nRead < 1 ){` |
|   40 | 1310 | `				break;` |
|    - | 1311 | `			}` |
|   40 | 1312 | `			if( iType == SHELL_RUN_RAW ){` |
|    - | 1313 | `				/* passthru() never looks for a line: php writes what it read */` |
|    8 | 1314 | `				if( ph7_context_output(pCtx,zBuf,(int)nRead) == PH7_ABORT ){` |
|  ! 0 | 1315 | `					break;` |
|    - | 1316 | `				}` |
|    8 | 1317 | `				continue;` |
|    - | 1318 | `			}` |
|    - | 1319 | `			{` |
|   34 | 1320 | `				size_t iOfft = 0;` |
|   82 | 1321 | `				while( iOfft < nRead ){` |
|   56 | 1322 | `					const char *zNl = (const char *)memchr(&zBuf[iOfft],'\n',nRead - iOfft);` |
|   56 | 1323 | `					size_t nChunk = zNl ? (size_t)(zNl - &zBuf[iOfft]) + 1 : nRead - iOfft;` |
|   56 | 1324 | `					SyBlobAppend(&sLine,&zBuf[iOfft],(sxu32)nChunk);` |
|   56 | 1325 | `					iOfft += nChunk;` |
|   56 | 1326 | `					if( zNl == 0 ){` |
|    6 | 1327 | `						break;   /* the line continues in the next read */` |
|    - | 1328 | `					}` |
|   72 | 1329 | `					if( ShellHandleLine(pCtx,iType,pArray,` |
|   74 | 1330 | `						(const char *)SyBlobData(&sLine),SyBlobLength(&sLine)) == PH7_ABORT ){` |
|  ! 0 | 1331 | `						bAbort = 1;` |
|  ! 0 | 1332 | `					}` |
|    - | 1333 | `					/* Keep it: the call answers the last line it saw */` |
|   50 | 1334 | `					SyBlobReset(&sLast);` |
|   50 | 1335 | `					SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|   50 | 1336 | `					SyBlobReset(&sLine);` |
|   50 | 1337 | `					if( bAbort ){` |
|  ! 0 | 1338 | `						break;` |
|    - | 1339 | `					}` |
|    2 | 1340 | `				}` |
|    - | 1341 | `			}` |
|   34 | 1342 | `			if( bAbort ){` |
|  ! 0 | 1343 | `				break;` |
|    - | 1344 | `			}` |
|    2 | 1345 | `		}` |
|    - | 1346 | `		/* Output that ended without a newline is still a line */` |
|   40 | 1347 | `		if( !bAbort && SyBlobLength(&sLine) > 0 ){` |
|    3 | 1348 | `			ShellHandleLine(pCtx,iType,pArray,` |
|    2 | 1349 | `				(const char *)SyBlobData(&sLine),SyBlobLength(&sLine));` |
|    2 | 1350 | `			SyBlobReset(&sLast);` |
|    2 | 1351 | `			SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));` |
|    1 | 1352 | `		}` |
|   40 | 1353 | `		iStatus = PipeClose(pPipe);` |
|   40 | 1354 | `		if( iType == SHELL_RUN_RAW ){` |
|    8 | 1355 | `			ph7_result_null(pCtx);` |
|    5 | 1356 | `		}else{` |
|   50 | 1357 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&sLast),` |
|   32 | 1358 | `				(int)ShellStripTrailing((const char *)SyBlobData(&sLast),SyBlobLength(&sLast)));` |
|    - | 1359 | `		}` |
|   40 | 1360 | `		SyBlobRelease(&sLine);` |
|   40 | 1361 | `		SyBlobRelease(&sLast);` |
|    - | 1362 | `	}` |
|   40 | 1363 | `	if( pOwned ){` |
|   25 | 1364 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOwned);` |
|   25 | 1365 | `		ph7_context_release_value(pCtx,pOwned);` |
|   12 | 1366 | `	}` |
|   36 | 1367 | `	if( nArg > iCodeArg ){` |
|    - | 1368 | `		ph7_value sVal;` |
|   20 | 1369 | `		PH7_MemObjInitFromInt(pCtx->pVm,&sVal,iStatus);` |
|   20 | 1370 | `		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iCodeArg],&sVal);` |
|   20 | 1371 | `		PH7_MemObjRelease(&sVal);` |
|    9 | 1372 | `	}` |
|   40 | 1373 | `	return PH7_OK;` |
|   24 | 1374 | `}` |
|    - | 1375 | `/*` |
|    - | 1376 | ` * string\|false exec(string $command, array &$output = null, int &$result_code = null)` |
|    - | 1377 | ` *  Run a command and answer the last line of its output.` |
|    - | 1378 | ` */` |
|   28 | 1379 | `PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1380 | `{` |
|   29 | 1381 | `	return ShellRunCommand(pCtx,SHELL_RUN_LAST,nArg,apArg);` |
|    1 | 1382 | `}` |
|    - | 1383 | `/*` |
|    - | 1384 | ` * string\|false system(string $command, int &$result_code = null)` |
|    - | 1385 | ` *  Run a command, write its output as it arrives, answer the last line.` |
|    - | 1386 | ` */` |
|    8 | 1387 | `PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1388 | `{` |
|   10 | 1389 | `	return ShellRunCommand(pCtx,SHELL_RUN_ECHO,nArg,apArg);` |
|    2 | 1390 | `}` |
|    - | 1391 | `/*` |
|    - | 1392 | ` * ?false passthru(string $command, int &$result_code = null)` |
|    - | 1393 | ` *  Run a command and write its output through, byte for byte.` |
|    - | 1394 | ` */` |
|    8 | 1395 | `PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1396 | `{` |
|   10 | 1397 | `	return ShellRunCommand(pCtx,SHELL_RUN_RAW,nArg,apArg);` |
|    2 | 1398 | `}` |
|    - | 1399 | `/*` |
|    - | 1400 | ` * bool proc_nice(int $priority)` |
|    - | 1401 | ` *  Change the priority of the running process — the last member of php's own` |
|    - | 1402 | ` *  process-execution surface, and the only one of the seven that runs no shell.` |
|    - | 1403 | ` */` |
|    6 | 1404 | `PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1405 | `{` |
|    - | 1406 | `	ph7_int64 iPri;` |
|    3 | 1407 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|    7 | 1408 | `	iPri = ph7_value_to_int64(apArg[0]);` |
|    - | 1409 | `#ifdef __WINNT__` |
|    - | 1410 | `	{` |
|    - | 1411 | `		/* php's own mapping (win32/nice.c): cmd.exe has no nice value, so the` |
|    - | 1412 | `		 * POSIX increment is bucketed into the five priority CLASSES Windows` |
|    - | 1413 | `		 * has. REALTIME is deliberately not reachable there, and neither is it` |
|    - | 1414 | `		 * here. */` |
|    1 | 1415 | `		DWORD dwFlag = NORMAL_PRIORITY_CLASS;` |
|    1 | 1416 | `		if( iPri < -9 ){` |
|  ! 0 | 1417 | `			dwFlag = HIGH_PRIORITY_CLASS;` |
|    1 | 1418 | `		}else if( iPri < -4 ){` |
|  ! 0 | 1419 | `			dwFlag = ABOVE_NORMAL_PRIORITY_CLASS;` |
|    1 | 1420 | `		}else if( iPri > 9 ){` |
|  ! 0 | 1421 | `			dwFlag = IDLE_PRIORITY_CLASS;` |
|    1 | 1422 | `		}else if( iPri > 4 ){` |
|  ! 0 | 1423 | `			dwFlag = BELOW_NORMAL_PRIORITY_CLASS;` |
|    - | 1424 | `		}` |
|    1 | 1425 | `		SetPriorityClass(GetCurrentProcess(),dwFlag);` |
|    - | 1426 | `		/* php answers TRUE whatever that returned: its nice() reports failure` |
|    - | 1427 | `		 * through its return value and leaves errno alone, and proc_nice() reads` |
|    - | 1428 | `		 * only errno. */` |
|    1 | 1429 | `		ph7_result_bool(pCtx,1);` |
|    - | 1430 | `	}` |
|    - | 1431 | `#elif defined(__UNIXES__)` |
|    - | 1432 | `	{` |
|    - | 1433 | `		int iIgnored;` |
|    - | 1434 | `		/* nice() legitimately answers -1 (it returns the NEW nice value), so` |
|    - | 1435 | `		 * errno is the only failure evidence — php clears it first for the same` |
|    - | 1436 | `		 * reason. */` |
|    6 | 1437 | `		errno = 0;` |
|    6 | 1438 | `		iIgnored = nice((int)iPri);` |
|    3 | 1439 | `		(void)iIgnored;` |
|    6 | 1440 | `		if( errno != 0 ){` |
|    3 | 1441 | `			PH7_VmThrowWarningFmt(pCtx->pVm,` |
|    - | 1442 | `				"%s(): Only a super user may attempt to increase the priority of a process",` |
|    1 | 1443 | `				ph7_function_name(pCtx));` |
|    2 | 1444 | `			ph7_result_bool(pCtx,0);` |
|    1 | 1445 | `		}else{` |
|    4 | 1446 | `			ph7_result_bool(pCtx,1);` |
|    - | 1447 | `		}` |
|    - | 1448 | `	}` |
|    - | 1449 | `#else` |
|    - | 1450 | `	ph7_result_bool(pCtx,0);` |
|    - | 1451 | `#endif` |
|    7 | 1452 | `	return PH7_OK;` |
|    1 | 1453 | `}` |
| 4792 | 1454 | `PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 | 1455 | `{` |
|    - | 1456 | `	const char *zCommand, *zMode;` |
|    - | 1457 | `	char zPosix[8];` |
|    - | 1458 | `	pipe_private *pPipe;` |
|    - | 1459 | `	io_private *pDev;` |
| 4797 | 1460 | `	int nCmdLen, nModeLen, nPosix, i, bDropped = 0;` |
| 2396 | 1461 | `	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */` |
|    - | 1462 | `	/* Extract the command and mode */` |
| 4797 | 1463 | `	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);` |
| 4797 | 1464 | `	zMode = ph7_value_to_string(apArg[1], &nModeLen);` |
|    - | 1465 | `	/*` |
|    - | 1466 | `	 * php's mode rule, and the only one it has: ONE 'b' — C's binary flag, which` |
|    - | 1467 | `	 * popen(3) itself refuses — is dropped from the mode on POSIX, and what is` |
|    - | 1468 | `	 * left must be exactly "r", "w", "rb" or "wb". PHL used to read mode[0] and` |
|    - | 1469 | `	 * hand the REST to popen(3) unexamined, which was wrong in both directions:` |
|    - | 1470 | ``	 * `popen($cmd, 'rb')`, the ordinary binary spelling, answered FALSE because`` |
|    - | 1471 | ``	 * glibc rejected the 'b', and `popen($cmd, 'rr')` opened a pipe php refuses.`` |
|    - | 1472 | `	 */` |
| 4797 | 1473 | `	nPosix = 0;` |
|    - | 1474 | `#ifdef __WINNT__` |
|    - | 1475 | `	SXUNUSED(bDropped);   /* cmd.exe keeps the 'b': _popen understands it */` |
|    - | 1476 | `#endif` |
| 9603 | 1477 | `	for( i = 0 ; i < nModeLen && nPosix < (int)sizeof(zPosix) - 1 ; ++i ){` |
|    - | 1478 | `#ifndef __WINNT__` |
| 4806 | 1479 | `		if( zMode[i] == 'b' && !bDropped ){` |
|    8 | 1480 | `			bDropped = 1;   /* php drops the FIRST one and only that one */` |
|    8 | 1481 | `			continue;` |
|    - | 1482 | `		}` |
|    - | 1483 | `#endif` |
| 4803 | 1484 | `		zPosix[nPosix++] = zMode[i];` |
| 2404 | 1485 | `	}` |
| 4797 | 1486 | `	zPosix[nPosix] = 0;` |
| 4792 | 1487 | `	if( nPosix > 2` |
| 4792 | 1488 | `	 \|\| (nPosix == 1 && zPosix[0] != 'r' && zPosix[0] != 'w')` |
| 2409 | 1489 | `	 \|\| (nPosix == 2 && SyMemcmp(zPosix,"rb",sizeof("rb")-1) != 0` |
|    7 | 1490 | `	                 && SyMemcmp(zPosix,"wb",sizeof("wb")-1) != 0) ){` |
|    9 | 1491 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - | 1492 | `			"popen(): Argument #2 ($mode) must be one of \"r\", \"rb\", \"w\", or \"wb\"");` |
|    - | 1493 | `	}` |
|    - | 1494 | `	/* Open the pipe. An EMPTY mode passes php's check above and fails HERE, in` |
|    - | 1495 | `	 * popen(3) — php reports it as an open failure and so does this, rather than` |
|    - | 1496 | `	 * letting the platform layer read mode[0] out of an empty string. */` |
| 4789 | 1497 | `	pPipe = nPosix > 0 ? PipeOpen(pCtx->pVm, zCommand, zPosix) : 0;` |
| 4789 | 1498 | `	if( pPipe == 0 ){` |
|    - | 1499 | ``		/* php names both arguments in this one: `popen(cmd,mode): message`. PHL`` |
|    - | 1500 | `		 * answered FALSE in silence, so a script had nothing to report. */` |
|    5 | 1501 | `		if( nPosix < 1 ){` |
|    3 | 1502 | `			errno = EINVAL;` |
|    1 | 1503 | `		}` |
|    7 | 1504 | `		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",` |
|    4 | 1505 | `			ph7_function_name(pCtx),zCommand,zPosix,VfsStrerror(errno));` |
|    5 | 1506 | `		ph7_result_bool(pCtx, 0);` |
|    5 | 1507 | `		return PH7_OK;` |
|    - | 1508 | `	}` |
|    - | 1509 | `	/* Allocate an io_private instance to wrap the pipe */` |
| 4785 | 1510 | `	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);` |
| 4785 | 1511 | `	if( pDev == 0 ){` |
|  ! 0 | 1512 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");` |
|  ! 0 | 1513 | `		PipeClose(pPipe);` |
|  ! 0 | 1514 | `		ph7_result_bool(pCtx, 0);` |
|  ! 0 | 1515 | `		return PH7_OK;` |
|    - | 1516 | `	}` |
|    - | 1517 | `	/* Initialize the io_private structure */` |
| 4785 | 1518 | `	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);` |
| 4785 | 1519 | `	pDev->pHandle = pPipe;` |
|    - | 1520 | `	/* Return the io_private instance as a resource */` |
| 4785 | 1521 | `	ph7_result_resource(pCtx, pDev);` |
| 4785 | 1522 | `	return PH7_OK;` |
| 2401 | 1523 | `}` |
|    - | 1524 | `/*` |
|    - | 1525 | ` * int pclose(resource $handle)` |
|    - | 1526 | ` *  Closes a process file pointer opened by popen() and returns the exit code.` |
|    - | 1527 | ` * Parameters` |
|    - | 1528 | ` *  $handle` |
|    - | 1529 | ` *   The file pointer must be valid, and must have been returned by popen().` |
|    - | 1530 | ` * Return` |
|    - | 1531 | ` *  Returns the termination status of the process that was run, or -1 on error.` |
|    - | 1532 | ` */` |
| 4652 | 1533 | `PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    5 | 1534 | `{` |
|    - | 1535 | `	const ph7_io_stream *pStream;` |
|    - | 1536 | `	pipe_private *pPipe;` |
|    - | 1537 | `	io_private *pDev;` |
|    - | 1538 | `	int status;` |
| 4657 | 1539 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|    - | 1540 | `		/* Missing/Invalid arguments, return -1 */` |
|  ! 0 | 1541 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|  ! 0 | 1542 | `		ph7_result_int(pCtx, -1);` |
|  ! 0 | 1543 | `		return PH7_OK;` |
|    - | 1544 | `	}` |
|    - | 1545 | `	/* Extract our private data */` |
| 4657 | 1546 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|    - | 1547 | `	/* Make sure we are dealing with a valid io_private instance */` |
| 4657 | 1548 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|  ! 0 | 1549 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");` |
|  ! 0 | 1550 | `		ph7_result_int(pCtx, -1);` |
|  ! 0 | 1551 | `		return PH7_OK;` |
|    - | 1552 | `	}` |
|    - | 1553 | `	/* Point to the target IO stream device */` |
| 4657 | 1554 | `	pStream = pDev->pStream;` |
| 4657 | 1555 | `	if( pStream == 0 \|\| !is_pipe_stream(pStream) ){` |
|  ! 0 | 1556 | `		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");` |
|  ! 0 | 1557 | `		ph7_result_int(pCtx, -1);` |
|  ! 0 | 1558 | `		return PH7_OK;` |
|    - | 1559 | `	}` |
|    - | 1560 | `	/* Get the pipe handle */` |
| 4657 | 1561 | `	pPipe = (pipe_private *)pDev->pHandle;` |
|    - | 1562 | `	/* Close the pipe and get exit status */` |
| 4657 | 1563 | `	status = PipeClose(pPipe);` |
|    - | 1564 | `	/* Keep the handle alive but flag it closed so shared copies see it */` |
| 4657 | 1565 | `	MarkIOPrivateClosed(pDev);` |
|    - | 1566 | `	/* Return the exit status */` |
| 4657 | 1567 | `	ph7_result_int(pCtx, status);` |
| 4657 | 1568 | `	return PH7_OK;` |
| 2331 | 1569 | `}` |
|    - | 1570 | `/*` |
|    - | 1571 | ` * proc_open() / proc_close() / proc_get_status() / proc_terminate()` |
|    - | 1572 | ` *   Run a command via fork()/exec() with fine-grained control over its` |
|    - | 1573 | ` *   standard descriptors (php's process-control family). The returned` |
|    - | 1574 | ` *   "process" resource wraps a small proc_private whose leading bytes mirror` |
|    - | 1575 | ` *   io_private (a distinct magic) so is_resource()/gettype() probes stay in` |
|    - | 1576 | ` *   bounds and report it as a live, non-stream resource.` |
|    - | 1577 | ` */` |
|    - | 1578 | `#ifdef __UNIXES__` |
|    - | 1579 | `#define PROC_PRIVATE_MAGIC 0x9C0DE5` |
|    - | 1580 | `#define PROC_MAX_DESC 16` |
|    - | 1581 | `typedef struct proc_private proc_private;` |
|    - | 1582 | `struct proc_private` |
|    - | 1583 | `{` |
|    - | 1584 | `	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */` |
|    - | 1585 | `	int pid;           /* child process id */` |
|    - | 1586 | `	int running;       /* TRUE until reaped by proc_close/proc_get_status */` |
|    - | 1587 | `	int exit_code;     /* cached exit status once reaped */` |
|    - | 1588 | `};` |
|    - | 1589 | `/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */` |
|   28 | 1590 | `static io_private * ProcWrapFd(ph7_vm *pVm,int fd)` |
|    - | 1591 | `{` |
|   28 | 1592 | `	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|   28 | 1593 | `	if( pDev == 0 ){` |
|  ! 0 | 1594 | `		return 0;` |
|    - | 1595 | `	}` |
|   28 | 1596 | `	InitIOPrivate(pVm,&sUnixFileStream,pDev);` |
|   28 | 1597 | `	pDev->pHandle = SX_INT_TO_PTR(fd);` |
|   28 | 1598 | `	return pDev;` |
|   14 | 1599 | `}` |
|    - | 1600 | `/* One parsed descriptor-spec entry. */` |
|    - | 1601 | `struct proc_desc` |
|    - | 1602 | `{` |
|    - | 1603 | `	int child_fd;      /* the array key: which fd the child sees */` |
|    - | 1604 | `	int kind;          /* 0=pipe, 1=file, 2=redirect */` |
|    - | 1605 | `	/* pipe */` |
|    - | 1606 | `	int child_end;     /* fd the child must have at child_fd */` |
|    - | 1607 | `	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */` |
|    - | 1608 | `	/* file */` |
|    - | 1609 | `	int file_fd;       /* opened fd for a ['file',path,mode] spec */` |
|    - | 1610 | `	/* redirect */` |
|    - | 1611 | `	int redirect_to;   /* target child fd for a ['redirect',N] spec */` |
|    - | 1612 | `};` |
|   10 | 1613 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1614 | `{` |
|    - | 1615 | `	struct proc_desc aDesc[PROC_MAX_DESC];` |
|   10 | 1616 | `	int nDesc = 0;` |
|    - | 1617 | `	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;` |
|    - | 1618 | `	ph7_hashmap *pSpecMap;` |
|    - | 1619 | `	ph7_hashmap_node *pNode;` |
|   10 | 1620 | `	ph7_vm *pVm = pCtx->pVm;` |
|   10 | 1621 | `	char **azArgv = 0;      /* exec argv when the command is an array */` |
|   10 | 1622 | `	int nArgv = 0;` |
|   10 | 1623 | `	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */` |
|   10 | 1624 | `	const char *zCwd = 0;` |
|   10 | 1625 | `	char **azEnv = 0;       /* constructed envp when an env array is supplied */` |
|   10 | 1626 | `	int nEnv = 0;` |
|    - | 1627 | `	proc_private *pProc;` |
|    - | 1628 | `	pid_t pid;` |
|    - | 1629 | `	int i, rc;` |
|   10 | 1630 | `	if( nArg < 3 \|\| !ph7_value_is_array(apArg[1]) ){` |
|  ! 0 | 1631 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");` |
|  ! 0 | 1632 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1633 | `		return PH7_OK;` |
|    - | 1634 | `	}` |
|    - | 1635 | `	/* --- Command: array (execvp) or string (/bin/sh -c) --- */` |
|   10 | 1636 | `	if( ph7_value_is_array(apArg[0]) ){` |
|   10 | 1637 | `		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|   10 | 1638 | `		int nCount = (int)ph7_array_count(apArg[0]);` |
|   10 | 1639 | `		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|   10 | 1640 | `		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   10 | 1641 | `		pNode = pCmdMap->pFirst;` |
|   20 | 1642 | `		for( i = 0 ; i < nCount && pNode ; ++i ){` |
|   10 | 1643 | `			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|    - | 1644 | `			int nLen; const char *zs;` |
|   10 | 1645 | `			PH7_MemObjInit(pVm,pv);` |
|   10 | 1646 | `			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);` |
|   10 | 1647 | `			zs = ph7_value_to_string(pv,&nLen);` |
|   10 | 1648 | `			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);` |
|   10 | 1649 | `			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }` |
|   10 | 1650 | `			PH7_MemObjRelease(pv);` |
|   10 | 1651 | `			SyMemBackendFree(&pVm->sAllocator,pv);` |
|   10 | 1652 | `			pNode = pNode->pPrev; /* hashmap insertion-order walk */` |
|    5 | 1653 | `		}` |
|   10 | 1654 | `		azArgv[nArgv] = 0;` |
|    5 | 1655 | `	}else{` |
|    - | 1656 | `		int nLen;` |
|  ! 0 | 1657 | `		zCmd = ph7_value_to_string(apArg[0],&nLen);` |
|    - | 1658 | `	}` |
|    - | 1659 | `	/* --- Optional cwd (arg 4) and env (arg 5) --- */` |
|   10 | 1660 | `	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){` |
|  ! 0 | 1661 | `		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);` |
|  ! 0 | 1662 | `		if( nLen < 1 ){ zCwd = 0; }` |
|  ! 0 | 1663 | `	}` |
|    5 | 1664 | `	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){` |
|  ! 0 | 1665 | `		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;` |
|  ! 0 | 1666 | `		int nCount = (int)ph7_array_count(apArg[4]);` |
|  ! 0 | 1667 | `		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));` |
|  ! 0 | 1668 | `		if( azEnv ){` |
|  ! 0 | 1669 | `			pNode = pEnvMap->pFirst;` |
|  ! 0 | 1670 | `			for( i = 0 ; i < nCount && pNode ; ++i ){` |
|    - | 1671 | `				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;` |
|  ! 0 | 1672 | `				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);` |
|  ! 0 | 1673 | `				PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|  ! 0 | 1674 | `				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);` |
|  ! 0 | 1675 | `				zk = ph7_value_to_string(&sKey,&nk);` |
|  ! 0 | 1676 | `				zv = ph7_value_to_string(&sVal,&nv);` |
|  ! 0 | 1677 | `				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));` |
|  ! 0 | 1678 | `				if( zPair ){` |
|  ! 0 | 1679 | `					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';` |
|  ! 0 | 1680 | `					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;` |
|  ! 0 | 1681 | `					azEnv[nEnv++] = zPair;` |
|  ! 0 | 1682 | `				}` |
|  ! 0 | 1683 | `				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);` |
|  ! 0 | 1684 | `				pNode = pNode->pPrev;` |
|  ! 0 | 1685 | `			}` |
|  ! 0 | 1686 | `			azEnv[nEnv] = 0;` |
|  ! 0 | 1687 | `		}` |
|  ! 0 | 1688 | `	}` |
|    - | 1689 | `	/* --- Parse the descriptor spec, creating pipes as we go --- */` |
|   10 | 1690 | `	pSpec = apArg[1];` |
|   10 | 1691 | `	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;` |
|   10 | 1692 | `	pNode = pSpecMap->pFirst;` |
|   40 | 1693 | `	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){` |
|   30 | 1694 | `		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];` |
|   30 | 1695 | `		PH7_MemObjInit(pVm,&sKey);` |
|   30 | 1696 | `		PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|   30 | 1697 | `		pD->child_fd = ph7_value_to_int(&sKey);` |
|   30 | 1698 | `		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1;` |
|   30 | 1699 | `		PH7_MemObjRelease(&sKey);` |
|   30 | 1700 | `		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));` |
|   30 | 1701 | `		PH7_MemObjInit(pVm,pEntry);` |
|   30 | 1702 | `		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);` |
|   30 | 1703 | `		if( ph7_value_is_array(pEntry) ){` |
|    - | 1704 | `			int nLen; const char *zType;` |
|   30 | 1705 | `			pType = ph7_array_fetch(pEntry,"0",1);` |
|   30 | 1706 | `			zType = pType ? ph7_value_to_string(pType,&nLen) : "";` |
|   30 | 1707 | `			if( SyStrncmp(zType,"pipe",4) == 0 ){` |
|    - | 1708 | `				int fds[2];` |
|   28 | 1709 | `				if( pipe(fds) == 0 ){` |
|   28 | 1710 | `					pParam = ph7_array_fetch(pEntry,"1",1);` |
|    - | 1711 | `					{` |
|   28 | 1712 | `						int nMode; const char *zMode = pParam ? ph7_value_to_string(pParam,&nMode) : "r";` |
|   28 | 1713 | `						pD->kind = 0;` |
|   28 | 1714 | `						if( zMode[0] == 'w' \|\| zMode[0] == 'a' ){` |
|    - | 1715 | `							/* child writes -> parent reads: child gets write end */` |
|   18 | 1716 | `							pD->child_end = fds[1]; pD->parent_end = fds[0];` |
|    9 | 1717 | `						}else{` |
|    - | 1718 | `							/* child reads -> parent writes: child gets read end */` |
|   10 | 1719 | `							pD->child_end = fds[0]; pD->parent_end = fds[1];` |
|    - | 1720 | `						}` |
|   28 | 1721 | `						nDesc++;` |
|    - | 1722 | `					}` |
|   14 | 1723 | `				}` |
|   16 | 1724 | `			}else if( SyStrncmp(zType,"file",4) == 0 ){` |
|  ! 0 | 1725 | `				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;` |
|  ! 0 | 1726 | `				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);` |
|  ! 0 | 1727 | `				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);` |
|  ! 0 | 1728 | `				zPath = pPath ? ph7_value_to_string(pPath,&nLen2) : "";` |
|  ! 0 | 1729 | `				zMode = pMode ? ph7_value_to_string(pMode,&nLen3) : "r";` |
|  ! 0 | 1730 | `				if( zMode[0] == 'w' ){ oflag = O_WRONLY\|O_CREAT\|O_TRUNC; }` |
|  ! 0 | 1731 | `				else if( zMode[0] == 'a' ){ oflag = O_WRONLY\|O_CREAT\|O_APPEND; }` |
|  ! 0 | 1732 | `				pD->kind = 1;` |
|  ! 0 | 1733 | `				pD->file_fd = open(zPath,oflag,0644);` |
|  ! 0 | 1734 | `				nDesc++;` |
|    2 | 1735 | `			}else if( SyStrncmp(zType,"redirect",8) == 0 ){` |
|    2 | 1736 | `				pParam = ph7_array_fetch(pEntry,"1",1);` |
|    2 | 1737 | `				pD->kind = 2;` |
|    2 | 1738 | `				pD->redirect_to = pParam ? ph7_value_to_int(pParam) : 1;` |
|    2 | 1739 | `				nDesc++;` |
|    1 | 1740 | `			}` |
|   15 | 1741 | `		}` |
|   30 | 1742 | `		PH7_MemObjRelease(pEntry);` |
|   30 | 1743 | `		SyMemBackendFree(&pVm->sAllocator,pEntry);` |
|   30 | 1744 | `		pNode = pNode->pPrev;` |
|   15 | 1745 | `	}` |
|    - | 1746 | `	/* --- Fork the child --- */` |
|   10 | 1747 | `	pid = fork();` |
|   15 | 1748 | `	if( pid < 0 ){` |
|  ! 0 | 1749 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"fork() failed");` |
|  ! 0 | 1750 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1751 | `		return PH7_OK;` |
|    - | 1752 | `	}` |
|   20 | 1753 | `	if( pid == 0 ){` |
|    - | 1754 | `		/* Child: wire up descriptors then exec */` |
|   40 | 1755 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|   30 | 1756 | `			struct proc_desc *pD = &aDesc[i];` |
|   30 | 1757 | `			if( pD->kind == 0 ){` |
|   28 | 1758 | `				dup2(pD->child_end,pD->child_fd);` |
|   28 | 1759 | `				close(pD->parent_end);` |
|   28 | 1760 | `				close(pD->child_end);` |
|   16 | 1761 | `			}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|  ! 0 | 1762 | `				dup2(pD->file_fd,pD->child_fd);` |
|  ! 0 | 1763 | `				close(pD->file_fd);` |
|  ! 0 | 1764 | `			}` |
|   15 | 1765 | `		}` |
|    - | 1766 | `		/* Redirects run after the pipes are in place (e.g. 2>&1) */` |
|   40 | 1767 | `		for( i = 0 ; i < nDesc ; ++i ){` |
|   30 | 1768 | `			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }` |
|   15 | 1769 | `		}` |
|   10 | 1770 | `		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }` |
|   10 | 1771 | `		if( azEnv ){` |
|  ! 0 | 1772 | `			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }` |
|  ! 0 | 1773 | `			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }` |
|  ! 0 | 1774 | `		}else{` |
|   10 | 1775 | `			if( azArgv ){ execvp(azArgv[0],azArgv); }` |
|  ! 0 | 1776 | `			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }` |
|    - | 1777 | `		}` |
|    5 | 1778 | `		_exit(127); /* exec failed */` |
|    - | 1779 | `	}` |
|    - | 1780 | `	/* Parent: close the child ends, wrap the parent ends into $pipes */` |
|   10 | 1781 | `	pPipes = ph7_context_new_array(pCtx);` |
|   40 | 1782 | `	for( i = 0 ; i < nDesc ; ++i ){` |
|   30 | 1783 | `		struct proc_desc *pD = &aDesc[i];` |
|   30 | 1784 | `		if( pD->kind == 0 ){` |
|    - | 1785 | `			io_private *pEnd;` |
|    - | 1786 | `			ph7_value *pRes;` |
|   28 | 1787 | `			close(pD->child_end);` |
|   28 | 1788 | `			pEnd = ProcWrapFd(pVm,pD->parent_end);` |
|   28 | 1789 | `			pRes = ph7_context_new_scalar(pCtx);` |
|   28 | 1790 | `			if( pEnd && pRes && pPipes ){` |
|   28 | 1791 | `				ph7_value_resource(pRes,pEnd);` |
|   28 | 1792 | `				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);` |
|   14 | 1793 | `			}` |
|   28 | 1794 | `			if( pRes ){ ph7_context_release_value(pCtx,pRes); }` |
|   16 | 1795 | `		}else if( pD->kind == 1 && pD->file_fd >= 0 ){` |
|  ! 0 | 1796 | `			close(pD->file_fd);` |
|  ! 0 | 1797 | `		}` |
|   15 | 1798 | `	}` |
|   10 | 1799 | `	if( pPipes ){` |
|   10 | 1800 | `		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);` |
|    5 | 1801 | `	}` |
|    - | 1802 | `	/* Free the exec argv/env copies now that the child owns its own image */` |
|   10 | 1803 | `	if( azArgv ){` |
|   20 | 1804 | `		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }` |
|   10 | 1805 | `		SyMemBackendFree(&pVm->sAllocator,azArgv);` |
|    5 | 1806 | `	}` |
|   15 | 1807 | `	if( azEnv ){` |
|  ! 0 | 1808 | `		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }` |
|  ! 0 | 1809 | `		SyMemBackendFree(&pVm->sAllocator,azEnv);` |
|  ! 0 | 1810 | `	}` |
|    - | 1811 | `	/* Build the process resource */` |
|   10 | 1812 | `	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));` |
|   10 | 1813 | `	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|   10 | 1814 | `	SyZero(pProc,sizeof(proc_private));` |
|   10 | 1815 | `	pProc->base.iMagic = PROC_PRIVATE_MAGIC;` |
|   10 | 1816 | `	pProc->pid = (int)pid;` |
|   10 | 1817 | `	pProc->running = 1;` |
|   10 | 1818 | `	pProc->exit_code = 0;` |
|   10 | 1819 | `	ph7_result_resource(pCtx,pProc);` |
|    5 | 1820 | `	(void)rc;` |
|   10 | 1821 | `	return PH7_OK;` |
|    5 | 1822 | `}` |
|    - | 1823 | `/* Reap the child if it has not been reaped yet, caching the exit code. */` |
|   10 | 1824 | `static void ProcReap(proc_private *pProc,int block)` |
|    - | 1825 | `{` |
|   10 | 1826 | `	int status = 0;` |
|    - | 1827 | `	pid_t r;` |
|   10 | 1828 | `	if( !pProc->running ){ return; }` |
|   10 | 1829 | `	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);` |
|   10 | 1830 | `	if( r == (pid_t)pProc->pid ){` |
|   10 | 1831 | `		pProc->running = 0;` |
|   10 | 1832 | `		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }` |
|  ! 0 | 1833 | `		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }` |
|    5 | 1834 | `	}` |
|    5 | 1835 | `}` |
|   10 | 1836 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1837 | `{` |
|    - | 1838 | `	proc_private *pProc;` |
|   10 | 1839 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1840 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1841 | `		return PH7_OK;` |
|    - | 1842 | `	}` |
|   10 | 1843 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|   10 | 1844 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|  ! 0 | 1845 | `		ph7_result_int(pCtx,-1);` |
|  ! 0 | 1846 | `		return PH7_OK;` |
|    - | 1847 | `	}` |
|   10 | 1848 | `	ProcReap(pProc,1/*block until it exits*/);` |
|   10 | 1849 | `	ph7_result_int(pCtx,pProc->exit_code);` |
|   10 | 1850 | `	return PH7_OK;` |
|    5 | 1851 | `}` |
|  ! 0 | 1852 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1853 | `{` |
|    - | 1854 | `	proc_private *pProc;` |
|  ! 0 | 1855 | `	int sig = 15; /* SIGTERM */` |
|  ! 0 | 1856 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1857 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1858 | `		return PH7_OK;` |
|    - | 1859 | `	}` |
|  ! 0 | 1860 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|  ! 0 | 1861 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|  ! 0 | 1862 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1863 | `		return PH7_OK;` |
|    - | 1864 | `	}` |
|  ! 0 | 1865 | `	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }` |
|  ! 0 | 1866 | `	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }` |
|  ! 0 | 1867 | `	ph7_result_bool(pCtx,1);` |
|  ! 0 | 1868 | `	return PH7_OK;` |
|  ! 0 | 1869 | `}` |
|  ! 0 | 1870 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - | 1871 | `{` |
|    - | 1872 | `	proc_private *pProc;` |
|    - | 1873 | `	ph7_value *pArray, *pVal;` |
|  ! 0 | 1874 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1875 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1876 | `		return PH7_OK;` |
|    - | 1877 | `	}` |
|  ! 0 | 1878 | `	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);` |
|  ! 0 | 1879 | `	if( pProc == 0 \|\| pProc->base.iMagic != PROC_PRIVATE_MAGIC ){` |
|  ! 0 | 1880 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1881 | `		return PH7_OK;` |
|    - | 1882 | `	}` |
|  ! 0 | 1883 | `	ProcReap(pProc,0/*non-blocking poll*/);` |
|  ! 0 | 1884 | `	pArray = ph7_context_new_array(pCtx);` |
|  ! 0 | 1885 | `	pVal = ph7_context_new_scalar(pCtx);` |
|  ! 0 | 1886 | `	if( pArray == 0 \|\| pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }` |
|  ! 0 | 1887 | `	ph7_value_int(pVal,pProc->pid);` |
|  ! 0 | 1888 | `	ph7_array_add_strkey_elem(pArray,"pid",pVal);` |
|  ! 0 | 1889 | `	ph7_value_bool(pVal,pProc->running);` |
|  ! 0 | 1890 | `	ph7_array_add_strkey_elem(pArray,"running",pVal);` |
|  ! 0 | 1891 | `	ph7_value_bool(pVal,0);` |
|  ! 0 | 1892 | `	ph7_array_add_strkey_elem(pArray,"signaled",pVal);` |
|  ! 0 | 1893 | `	ph7_array_add_strkey_elem(pArray,"stopped",pVal);` |
|  ! 0 | 1894 | `	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);` |
|  ! 0 | 1895 | `	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);` |
|  ! 0 | 1896 | `	ph7_value_int(pVal,0);` |
|  ! 0 | 1897 | `	ph7_array_add_strkey_elem(pArray,"termsig",pVal);` |
|  ! 0 | 1898 | `	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);` |
|  ! 0 | 1899 | `	ph7_context_release_value(pCtx,pVal);` |
|  ! 0 | 1900 | `	ph7_result_value(pCtx,pArray);` |
|  ! 0 | 1901 | `	return PH7_OK;` |
|  ! 0 | 1902 | `}` |
|    - | 1903 | `#else /* !__UNIXES__ */` |
|    - | 1904 | `PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1905 | `{` |
|    - | 1906 | `	SXUNUSED(nArg); SXUNUSED(apArg);` |
|  ! 0 | 1907 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");` |
|  ! 0 | 1908 | `	ph7_result_bool(pCtx,0);` |
|  ! 0 | 1909 | `	return PH7_OK;` |
|  ! 0 | 1910 | `}` |
|    - | 1911 | `PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1912 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }` |
|    - | 1913 | `PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1914 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    - | 1915 | `PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|  ! 0 | 1916 | `{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }` |
|    - | 1917 | `#endif /* __UNIXES__ */` |
|    - | 1918 | `/* Export the php:// stream */` |
|    - | 1919 | `PH7_PRIVATE const ph7_io_stream sPHP_Stream = {` |
|    - | 1920 | `	"php",` |
|    - | 1921 | `	PH7_IO_STREAM_VERSION,` |
|    - | 1922 | `	PHPStreamData_Open,  /* xOpen */` |
|    - | 1923 | `	0,   /* xOpenDir */` |
|    - | 1924 | `	PHPStreamData_Close, /* xClose */` |
|    - | 1925 | `	0,  /* xCloseDir */` |
|    - | 1926 | `	PHPStreamData_Read,  /* xRead */` |
|    - | 1927 | `	0,  /* xReadDir */` |
|    - | 1928 | `	PHPStreamData_Write, /* xWrite */` |
|    - | 1929 | `	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */` |
|    - | 1930 | `	0,  /* xLock */` |
|    - | 1931 | `	0,  /* xRewindDir */` |
|    - | 1932 | `	PHPStreamData_Tell,  /* xTell */` |
|    - | 1933 | `	PHPStreamData_Trunc, /* xTrunc */` |
|    - | 1934 | `	0,  /* xSync */` |
|    - | 1935 | `	0   /* xStat */` |
|    - | 1936 | `};` |
|    - | 1937 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - | 1938 | `/*` |
|    - | 1939 | ` * Return TRUE if we are dealing with the php:// stream.` |
|    - | 1940 | ` * FALSE otherwise.` |
|    - | 1941 | ` */` |
|  382 | 1942 | `PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream)` |
|    5 | 1943 | `{` |
|    - | 1944 | `#ifndef PH7_DISABLE_DISK_IO` |
|  387 | 1945 | `	return pStream == &sPHP_Stream;` |
|    - | 1946 | `#else` |
|    - | 1947 | `	SXUNUSED(pStream); /* cc warning */` |
|    - | 1948 | `	return 0;` |
|    - | 1949 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    5 | 1950 | `}` |
|    - | 1951 | `/*` |
|    - | 1952 | ` * Return TRUE if we are dealing with the data:// stream.` |
|    - | 1953 | ` */` |
|  250 | 1954 | `PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream)` |
|    4 | 1955 | `{` |
|    - | 1956 | `#ifndef PH7_DISABLE_DISK_IO` |
|  254 | 1957 | `	return pStream == &sDATA_Stream;` |
|    - | 1958 | `#else` |
|    - | 1959 | `	SXUNUSED(pStream); /* cc warning */` |
|    - | 1960 | `	return 0;` |
|    - | 1961 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    4 | 1962 | `}` |
|    - | 1963 | `/*` |
|    - | 1964 | ` * bool stream_isatty(resource $stream)` |
|    - | 1965 | ` *  TRUE when the stream is an interactive terminal. PHL answers this for the` |
|    - | 1966 | ` *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the` |
|    - | 1967 | ` *  platform isatty()/GetFileType(); file and memory streams are never a` |
|    - | 1968 | ` *  terminal, so they answer FALSE (php-exact for those).` |
|    - | 1969 | ` */` |
|    6 | 1970 | `PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1971 | `{` |
|    7 | 1972 | `	int bTty = 0;` |
|    7 | 1973 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|  ! 0 | 1974 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1975 | `		return PH7_OK;` |
|    - | 1976 | `	}` |
|    - | 1977 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - | 1978 | `	{` |
|    7 | 1979 | `		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|    7 | 1980 | `		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){` |
|    5 | 1981 | `			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;` |
|    6 | 1982 | `			if( pData && (pData->iType == PH7_IO_STREAM_STDIN` |
|    4 | 1983 | `				\|\| pData->iType == PH7_IO_STREAM_STDOUT` |
|    3 | 1984 | `				\|\| pData->iType == PH7_IO_STREAM_STDERR) ){` |
|    - | 1985 | `#ifdef __WINNT__` |
|    1 | 1986 | `				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;` |
|    - | 1987 | `#else` |
|    4 | 1988 | `				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;` |
|    - | 1989 | `#endif` |
|    2 | 1990 | `			}` |
|    2 | 1991 | `		}` |
|    - | 1992 | `	}` |
|    - | 1993 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    7 | 1994 | `	ph7_result_bool(pCtx,bTty);` |
|    7 | 1995 | `	return PH7_OK;` |
|    4 | 1996 | `}` |
|    - | 1997 |  |
|    - | 1998 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1999 | `/*` |
|    - | 2000 | ` * Export the STDIN handle.` |
|    - | 2001 | ` */` |
|    2 | 2002 | `PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)` |
|    1 | 2003 | `{` |
|    - | 2004 | `#ifndef PH7_DISABLE_DISK_IO` |
|    3 | 2005 | `	if( pVm->pStdin == 0  ){` |
|    - | 2006 | `		io_private *pIn;` |
|    - | 2007 | `		/* Allocate an IO private instance */` |
|    3 | 2008 | `		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    3 | 2009 | `		if( pIn == 0 ){` |
|  ! 0 | 2010 | `			return 0;` |
|    - | 2011 | `		}` |
|    3 | 2012 | `		InitIOPrivate(pVm,&sPHP_Stream,pIn);` |
|    - | 2013 | `		/* Initialize the handle */` |
|    3 | 2014 | `		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);` |
|    - | 2015 | `		/* Install the STDIN stream */` |
|    3 | 2016 | `		pVm->pStdin = pIn;` |
|    3 | 2017 | `		return pIn;` |
|  ! 0 | 2018 | `	}else{` |
|    - | 2019 | `		/* NULL or STDIN */` |
|  ! 0 | 2020 | `		return pVm->pStdin;` |
|    - | 2021 | `	}` |
|    - | 2022 | `#else` |
|    - | 2023 | `	SXUNUSED(pVm); /* cc warning */` |
|    - | 2024 | `	return 0;` |
|    - | 2025 | `#endif` |
|    2 | 2026 | `}` |
|    - | 2027 | `/*` |
|    - | 2028 | ` * Export the STDOUT handle.` |
|    - | 2029 | ` */` |
|    8 | 2030 | `PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)` |
|    1 | 2031 | `{` |
|    - | 2032 | `#ifndef PH7_DISABLE_DISK_IO` |
|    9 | 2033 | `	if( pVm->pStdout == 0  ){` |
|    - | 2034 | `		io_private *pOut;` |
|    - | 2035 | `		/* Allocate an IO private instance */` |
|    7 | 2036 | `		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    7 | 2037 | `		if( pOut == 0 ){` |
|  ! 0 | 2038 | `			return 0;` |
|    - | 2039 | `		}` |
|    7 | 2040 | `		InitIOPrivate(pVm,&sPHP_Stream,pOut);` |
|    - | 2041 | `		/* Initialize the handle */` |
|    7 | 2042 | `		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);` |
|    - | 2043 | `		/* Install the STDOUT stream */` |
|    7 | 2044 | `		pVm->pStdout = pOut;` |
|    7 | 2045 | `		return pOut;` |
|  ! 0 | 2046 | `	}else{` |
|    - | 2047 | `		/* NULL or STDOUT */` |
|    3 | 2048 | `		return pVm->pStdout;` |
|    - | 2049 | `	}` |
|    - | 2050 | `#else` |
|    - | 2051 | `	SXUNUSED(pVm); /* cc warning */` |
|    - | 2052 | `	return 0;` |
|    - | 2053 | `#endif` |
|    5 | 2054 | `}` |
|    - | 2055 | `/*` |
|    - | 2056 | ` * Export the STDERR handle.` |
|    - | 2057 | ` */` |
|   10 | 2058 | `PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)` |
|    1 | 2059 | `{` |
|    - | 2060 | `#ifndef PH7_DISABLE_DISK_IO` |
|   11 | 2061 | `	if( pVm->pStderr == 0  ){` |
|    - | 2062 | `		io_private *pErr;` |
|    - | 2063 | `		/* Allocate an IO private instance */` |
|    9 | 2064 | `		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));` |
|    9 | 2065 | `		if( pErr == 0 ){` |
|  ! 0 | 2066 | `			return 0;` |
|    - | 2067 | `		}` |
|    9 | 2068 | `		InitIOPrivate(pVm,&sPHP_Stream,pErr);` |
|    - | 2069 | `		/* Initialize the handle */` |
|    9 | 2070 | `		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);` |
|    - | 2071 | `		/* Install the STDERR stream */` |
|    9 | 2072 | `		pVm->pStderr = pErr;` |
|    9 | 2073 | `		return pErr;` |
|  ! 0 | 2074 | `	}else{` |
|    - | 2075 | `		/* NULL or STDERR */` |
|    3 | 2076 | `		return pVm->pStderr;` |
|    - | 2077 | `	}` |
|    - | 2078 | `#else` |
|    - | 2079 | `	SXUNUSED(pVm); /* cc warning */` |
|    - | 2080 | `	return 0;` |
|    - | 2081 | `#endif` |
|    6 | 2082 | `}` |
|    - | 2083 |  |
