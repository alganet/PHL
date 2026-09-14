/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef __UNIXES__
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#endif
/*
 * Section:
 *    Built-in IO stream drivers: php://, data://, pipe (popen) and the
 *    standard stream exporters. The file:// driver lives in
 *    vfs_unix.c/vfs_win.c; vfs.c owns registration.
 * Status:
 *    Stable.
 */
#if !defined(PH7_DISABLE_BUILTIN_FUNC) || !defined(PH7_DISABLE_DISK_IO)
#ifndef PH7_DISABLE_DISK_IO
/*
 * The following defines are mostly used by the UNIX built and have
 * no particular meaning on windows.
 */
#ifndef STDIN_FILENO
#define STDIN_FILENO	0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO	1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO	2
#endif
/*
 * php:// Accessing various I/O streams
 * According to the PHP langage reference manual
 * PHP provides a number of miscellaneous I/O streams that allow access to PHP's own input
 * and output streams, the standard input, output and error file descriptors.
 * php://stdin, php://stdout and php://stderr:
 *  Allow direct access to the corresponding input or output stream of the PHP process.
 *  The stream references a duplicate file descriptor, so if you open php://stdin and later
 *  close it, you close only your copy of the descriptor-the actual stream referenced by STDIN is unaffected.
 *  php://stdin is read-only, whereas php://stdout and php://stderr are write-only.
 * php://output
 *  php://output is a write-only stream that allows you to write to the output buffer
 *  mechanism in the same way as print and echo.
 */
typedef struct ph7_stream_data ph7_stream_data;
/* Supported IO streams */
#define PH7_IO_STREAM_STDIN  1 /* php://stdin */
#define PH7_IO_STREAM_STDOUT 2 /* php://stdout */
#define PH7_IO_STREAM_STDERR 3 /* php://stderr */
#define PH7_IO_STREAM_OUTPUT 4 /* php://output */
#define PH7_IO_STREAM_MEMORY 5 /* php://memory, php://temp, and data:// payloads */
 /* The following structure is the private data associated with the php:// stream */
struct ph7_stream_data
{
	ph7_vm *pVm; /* VM that own this instance */
	int iType;   /* Stream type */
	union{
		void *pHandle; /* Stream handle */
		ph7_output_consumer sConsumer; /* VM output consumer */
	}x;
	SyBlob sMem;     /* MEMORY type: backing buffer */
	sxu32 nCur;      /* MEMORY type: read/write cursor */
	int bReadOnly;   /* MEMORY type: TRUE for data:// payloads */
};
/*
 * Allocate a new instance of the ph7_stream_data structure.
 */
static ph7_stream_data * PHPStreamDataInit(ph7_vm *pVm,int iType)
{
	ph7_stream_data *pData;
	if( pVm == 0 ){
		return 0;
	}
	/* Allocate a new instance */
	pData = (ph7_stream_data *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_stream_data));
	if( pData == 0 ){
		return 0;
	}
	/* Zero the structure */
	SyZero(pData,sizeof(ph7_stream_data));
	/* Initialize fields */
	pData->iType = iType;
	SyBlobInit(&pData->sMem,&pVm->sAllocator);
	pData->nCur = 0;
	pData->bReadOnly = 0;
	if( iType == PH7_IO_STREAM_MEMORY ){
		/* Nothing else to set up: the buffer is the stream */
	}else if( iType == PH7_IO_STREAM_OUTPUT ){
		/* Point to the default VM consumer routine. */
		pData->x.sConsumer = pVm->sVmConsumer;
	}else{
#ifdef __WINNT__
		DWORD nChannel;
		switch(iType){
		case PH7_IO_STREAM_STDOUT:	nChannel = STD_OUTPUT_HANDLE; break;
		case PH7_IO_STREAM_STDERR:  nChannel = STD_ERROR_HANDLE; break;
		default:
			nChannel = STD_INPUT_HANDLE;
			break;
		}
		pData->x.pHandle = GetStdHandle(nChannel);
#else
		/* Assume an UNIX system */
		int ifd = STDIN_FILENO;
		switch(iType){
		case PH7_IO_STREAM_STDOUT:  ifd = STDOUT_FILENO; break;
		case PH7_IO_STREAM_STDERR:  ifd = STDERR_FILENO; break;
		default:
			break;
		}
		pData->x.pHandle = SX_INT_TO_PTR(ifd);
#endif
	}
	pData->pVm = pVm;
	return pData;
}
/*
 * Implementation of the php:// IO streams routines
 * Status:
 *   Stable.
 */
/* int (*xOpen)(const char *,int,ph7_value *,void **) */
static int PHPStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)
{
	ph7_stream_data *pData;
	SyString sStream;
	SyStringInitFromBuf(&sStream,zName,SyStrlen(zName));
	/* Trim leading and trailing white spaces */
	SyStringFullTrim(&sStream);
	/* Stream to open */
	if( SyStrnicmp(sStream.zString,"stdin",sizeof("stdin")-1) == 0 ){
		iMode = PH7_IO_STREAM_STDIN;
	}else if( SyStrnicmp(sStream.zString,"output",sizeof("output")-1) == 0 ){
		iMode = PH7_IO_STREAM_OUTPUT;
	}else if( SyStrnicmp(sStream.zString,"stdout",sizeof("stdout")-1) == 0 ){
		iMode = PH7_IO_STREAM_STDOUT;
	}else if( SyStrnicmp(sStream.zString,"stderr",sizeof("stderr")-1) == 0 ){
		iMode = PH7_IO_STREAM_STDERR;
	}else if( SyStrnicmp(sStream.zString,"memory",sizeof("memory")-1) == 0
	       || SyStrnicmp(sStream.zString,"temp",sizeof("temp")-1) == 0 ){
		/* php://memory and php://temp (PHL keeps temp fully in memory —
		 * php's 2MB disk spill is a memory-pressure detail, recorded) */
		iMode = PH7_IO_STREAM_MEMORY;
	}else{
		/* unknown stream name */
		return -1;
	}
	/* Create our handle */
	pData = PHPStreamDataInit(pResource?pResource->pVm:0,iMode);
	if( pData == 0 ){
		return -1;
	}
	/* Make the handle public */
	*ppHandle = (void *)pData;
	return PH7_OK;
}
/* ph7_int64 (*xRead)(void *,void *,ph7_int64) */
static ph7_int64 PHPStreamData_Read(void *pHandle,void *pBuffer,ph7_int64 nDatatoRead)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	if( pData == 0 ){
		return -1;
	}
	if( pData->iType == PH7_IO_STREAM_MEMORY ){
		sxu32 nAvail = SyBlobLength(&pData->sMem);
		sxu32 nRead;
		if( pData->nCur >= nAvail ){
			return 0; /* EOF */
		}
		nRead = nAvail - pData->nCur;
		if( (ph7_int64)nRead > nDatatoRead ){
			nRead = (sxu32)nDatatoRead;
		}
		SyMemcpy((const char *)SyBlobData(&pData->sMem) + pData->nCur,pBuffer,nRead);
		pData->nCur += nRead;
		return (ph7_int64)nRead;
	}
	if( pData->iType != PH7_IO_STREAM_STDIN ){
		/* Forbidden */
		return -1;
	}
#ifdef __WINNT__
	{
		DWORD nRd;
		BOOL rc;
		rc = ReadFile(pData->x.pHandle,pBuffer,(DWORD)nDatatoRead,&nRd,0);
		if( !rc ){
			/* IO error */
			return -1;
		}
		return (ph7_int64)nRd;
	}
#elif defined(__UNIXES__)
	{
		ssize_t nRd;
		int fd;
		fd = SX_PTR_TO_INT(pData->x.pHandle);
		nRd = read(fd,pBuffer,(size_t)nDatatoRead);
		if( nRd < 1 ){
			return -1;
		}
		return (ph7_int64)nRd;
	}
#else
	return -1;
#endif
}
/* ph7_int64 (*xWrite)(void *,const void *,ph7_int64) */
static ph7_int64 PHPStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	if( pData == 0 ){
		return -1;
	}
	if( pData->iType == PH7_IO_STREAM_STDIN ){
		/* Forbidden */
		return -1;
	}else if( pData->iType == PH7_IO_STREAM_MEMORY ){
		sxu32 nLen,nEnd;
		if( pData->bReadOnly ){
			return -1;
		}
		nLen = SyBlobLength(&pData->sMem);
		if( pData->nCur > nLen ){
			/* seek past end: php zero-fills the gap */
			static const char zZero[64] = {0};
			while( SyBlobLength(&pData->sMem) < pData->nCur ){
				sxu32 nPad = pData->nCur - SyBlobLength(&pData->sMem);
				if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }
				if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){
					return -1;
				}
			}
			nLen = SyBlobLength(&pData->sMem);
		}
		nEnd = pData->nCur + (sxu32)nWrite;
		if( pData->nCur < nLen ){
			/* overwrite in place up to the current end */
			sxu32 nOver = nLen - pData->nCur;
			if( nOver > (sxu32)nWrite ){ nOver = (sxu32)nWrite; }
			SyMemcpy(pBuf,(char *)SyBlobData(&pData->sMem) + pData->nCur,nOver);
			if( nEnd > nLen ){
				if( SyBlobAppend(&pData->sMem,(const char *)pBuf + nOver,nEnd - nLen) != SXRET_OK ){
					return -1;
				}
			}
		}else{
			if( SyBlobAppend(&pData->sMem,pBuf,(sxu32)nWrite) != SXRET_OK ){
				return -1;
			}
		}
		pData->nCur = nEnd;
		return nWrite;
	}else if( pData->iType == PH7_IO_STREAM_OUTPUT ){
		ph7_output_consumer *pCons = &pData->x.sConsumer;
		int rc;
		/* Call the vm output consumer */
		rc = pCons->xConsumer(pBuf,(unsigned int)nWrite,pCons->pUserData);
		if( rc == PH7_ABORT ){
			return -1;
		}
		return nWrite;
	}
#ifdef __WINNT__
	{
		DWORD nWr;
		BOOL rc;
		rc = WriteFile(pData->x.pHandle,pBuf,(DWORD)nWrite,&nWr,0);
		if( !rc ){
			/* IO error */
			return -1;
		}
		return (ph7_int64)nWr;
	}
#elif defined(__UNIXES__)
	{
		ssize_t nWr;
		int fd;
		fd = SX_PTR_TO_INT(pData->x.pHandle);
		nWr = write(fd,pBuf,(size_t)nWrite);
		if( nWr < 1 ){
			return -1;
		}
		return (ph7_int64)nWr;
	}
#else
	return -1;
#endif
}
/* void (*xClose)(void *) */
static void PHPStreamData_Close(void *pHandle)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	ph7_vm *pVm;
	if( pData == 0 ){
		return;
	}
	pVm = pData->pVm;
	SyBlobRelease(&pData->sMem);
	/* Free the instance */
	SyMemBackendFree(&pVm->sAllocator,pData);
}
/* int (*xSeek)(void *,ph7_int64,int); MEMORY type only */
static int PHPStreamData_Seek(void *pHandle,ph7_int64 iOfft,int whence)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	ph7_int64 iNew;
	if( pData == 0 || pData->iType != PH7_IO_STREAM_MEMORY ){
		return -1;
	}
	switch(whence){
	case 1/*SEEK_CUR*/: iNew = (ph7_int64)pData->nCur + iOfft; break;
	case 2/*SEEK_END*/: iNew = (ph7_int64)SyBlobLength(&pData->sMem) + iOfft; break;
	default:            iNew = iOfft; break;
	}
	if( iNew < 0 ){
		return -1;
	}
	pData->nCur = (sxu32)iNew;
	return PH7_OK;
}
/* ph7_int64 (*xTell)(void *); MEMORY type only */
static ph7_int64 PHPStreamData_Tell(void *pHandle)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	if( pData == 0 || pData->iType != PH7_IO_STREAM_MEMORY ){
		return -1;
	}
	return (ph7_int64)pData->nCur;
}
/* int (*xTrunc)(void *,ph7_int64); MEMORY type only */
static int PHPStreamData_Trunc(void *pHandle,ph7_int64 nLen)
{
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	if( pData == 0 || pData->iType != PH7_IO_STREAM_MEMORY || pData->bReadOnly ){
		return -1;
	}
	if( nLen < (ph7_int64)SyBlobLength(&pData->sMem) ){
		/* shrink in place: the blob keeps its allocation */
		pData->sMem.nByte = (sxu32)nLen;
	}else{
		static const char zZero[64] = {0};
		while( (ph7_int64)SyBlobLength(&pData->sMem) < nLen ){
			sxu32 nPad = (sxu32)(nLen - SyBlobLength(&pData->sMem));
			if( nPad > sizeof(zZero) ){ nPad = sizeof(zZero); }
			if( SyBlobAppend(&pData->sMem,zZero,nPad) != SXRET_OK ){
				return -1;
			}
		}
	}
	return PH7_OK;
}
/*
 * data:// stream: read-only in-memory payloads parsed from RFC 2397 URIs
 * (data://[mediatype][;base64],payload — the payload percent-decodes unless
 * base64). Shares the MEMORY machinery above.
 */
static sxi32 DataStreamB64Consumer(const void *pData,unsigned int nLen,void *pUserData)
{
	return SyBlobAppend((SyBlob *)pUserData,pData,nLen);
}
static int DataStreamData_Open(const char *zName,int iMode,ph7_value *pResource,void ** ppHandle)
{
	ph7_stream_data *pData;
	const char *zIn = zName;
	const char *zEnd = &zName[SyStrlen(zName)];
	const char *zComma = 0;
	int bBase64 = 0;
	SXUNUSED(iMode);
	/* Find the comma separating the mediatype from the payload */
	while( zIn < zEnd ){
		if( zIn[0] == ',' ){
			zComma = zIn;
			break;
		}
		zIn++;
	}
	if( zComma == 0 ){
		return -1;
	}
	if( zComma - zName >= (int)sizeof(";base64")-1
	 && SyStrnicmp(&zComma[-((int)sizeof(";base64")-1)],";base64",sizeof(";base64")-1) == 0 ){
		bBase64 = 1;
	}
	pData = PHPStreamDataInit(pResource?pResource->pVm:0,PH7_IO_STREAM_MEMORY);
	if( pData == 0 ){
		return -1;
	}
	pData->bReadOnly = 1;
	zIn = &zComma[1];
	if( bBase64 ){
		if( SyBase64Decode(zIn,(sxu32)(zEnd - zIn),DataStreamB64Consumer,&pData->sMem) != SXRET_OK ){
			SyBlobRelease(&pData->sMem);
			SyMemBackendFree(&pData->pVm->sAllocator,pData);
			return -1;
		}
	}else{
		/* percent-decode the payload */
		while( zIn < zEnd ){
			char c = zIn[0];
			if( c == '%' && zIn + 2 < zEnd && SyisHex(zIn[1]) && SyisHex(zIn[2]) ){
				int hi = SyHexToint(zIn[1]);
				int lo = SyHexToint(zIn[2]);
				c = (char)((hi << 4) | lo);
				zIn += 3;
			}else{
				zIn++;
			}
			if( SyBlobAppend(&pData->sMem,&c,1) != SXRET_OK ){
				SyBlobRelease(&pData->sMem);
				SyMemBackendFree(&pData->pVm->sAllocator,pData);
				return -1;
			}
		}
	}
	*ppHandle = (void *)pData;
	return PH7_OK;
}
/* data:// rejects writes outright */
static ph7_int64 DataStreamData_Write(void *pHandle,const void *pBuf,ph7_int64 nWrite)
{
	SXUNUSED(pHandle);
	SXUNUSED(pBuf);
	SXUNUSED(nWrite);
	return -1;
}
PH7_PRIVATE const ph7_io_stream sDATA_Stream = {
	"data",
	PH7_IO_STREAM_VERSION,
	DataStreamData_Open,  /* xOpen */
	0,   /* xOpenDir */
	PHPStreamData_Close, /* xClose */
	0,  /* xCloseDir */
	PHPStreamData_Read,  /* xRead */
	0,  /* xReadDir */
	DataStreamData_Write, /* xWrite */
	PHPStreamData_Seek,  /* xSeek */
	0,  /* xLock */
	0,  /* xRewindDir */
	PHPStreamData_Tell,  /* xTell */
	0,  /* xTrunc */
	0,  /* xSync */
	0   /* xStat */
};
/*
 * Pipe stream implementation for popen/pclose.
 * This stream wraps the system's popen/pclose APIs to provide
 * PHP-compatible process I/O functionality.
 */
typedef struct pipe_private pipe_private;
struct pipe_private
{
	FILE *pFile;    /* Pipe file handle from popen */
	ph7_vm *pVm;    /* VM that owns this instance */
	int iMode;      /* Open mode: 'r' for read, 'w' for write */
#ifdef __WINNT__
	HANDLE hProcess; /* Process handle on Windows for proper waiting */
	HANDLE hPipe;    /* Pipe handle (for cleanup) */
#endif
};

#ifdef __WINNT__
#include <Windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
/*
 * Custom Windows popen implementation using CreateProcess.
 * This allows us to properly wait for process completion.
 */
static FILE* WinPopen(const char *zCommand, const char *zMode, HANDLE *phProcess, HANDLE *phPipe)
{
	HANDLE hReadPipe = NULL, hWritePipe = NULL;
	HANDLE hChildStdoutRd = NULL, hChildStdoutWr = NULL;
	HANDLE hChildStdinRd = NULL, hChildStdinWr = NULL;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	WCHAR *zWideCmd = NULL;
	FILE *pFile = NULL;
	int fd;
	BOOL bRead = (zMode[0] == 'r');

	/* Set up security attributes for pipe inheritance */
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	/* Create pipes for child process I/O */
	if( bRead ){
		/* Reading from child's stdout */
		if( !CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sa, 0) ){
			return NULL;
		}
		/* Ensure read handle is not inherited */
		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);
		hReadPipe = hChildStdoutRd;
		*phPipe = hChildStdoutRd;
	}else{
		/* Writing to child's stdin */
		if( !CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0) ){
			return NULL;
		}
		/* Ensure write handle is not inherited */
		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0);
		hWritePipe = hChildStdinWr;
		*phPipe = hChildStdinWr;
	}

	/* Convert command to wide string */
	{
		int nLen = MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, NULL, 0);
		if( nLen <= 0 ){
			goto cleanup_pipes;
		}
		zWideCmd = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, nLen * sizeof(WCHAR));
		if( !zWideCmd ){
			goto cleanup_pipes;
		}
		MultiByteToWideChar(CP_UTF8, 0, zCommand, -1, zWideCmd, nLen);
	}

	/* Set up process startup info */
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE; /* Hide console window */
	si.hStdInput = bRead ? GetStdHandle(STD_INPUT_HANDLE) : hChildStdinRd;
	si.hStdOutput = bRead ? hChildStdoutWr : GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	ZeroMemory(&pi, sizeof(pi));

	/* Create the child process */
	if( !CreateProcessW(
		NULL,           /* Application name */
		zWideCmd,       /* Command line */
		NULL,           /* Process security attributes */
		NULL,           /* Thread security attributes */
		TRUE,           /* Inherit handles */
		CREATE_NO_WINDOW, /* Creation flags - no console window */
		NULL,           /* Environment */
		NULL,           /* Current directory */
		&si,            /* Startup info */
		&pi             /* Process info */
	)){
		goto cleanup_all;
	}

	/* Close handles we don't need in parent */
	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);
	if( hChildStdinRd ) CloseHandle(hChildStdinRd);

	/* Close thread handle (we only need process handle) */
	CloseHandle(pi.hThread);

	/* Store process handle for later waiting */
	*phProcess = pi.hProcess;

	/* Convert OS handle to C file descriptor, then to FILE* */
	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),
	                     bRead ? _O_RDONLY | _O_TEXT : _O_WRONLY | _O_TEXT);
	if( fd == -1 ){
		CloseHandle(pi.hProcess);
		*phProcess = NULL;
		goto cleanup_all;
	}

	pFile = _fdopen(fd, zMode);
	if( !pFile ){
		_close(fd); /* This will also close the underlying handle */
		CloseHandle(pi.hProcess);
		*phProcess = NULL;
		if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);
		return NULL;
	}

	HeapFree(GetProcessHeap(), 0, zWideCmd);
	return pFile;

cleanup_all:
	if( zWideCmd ) HeapFree(GetProcessHeap(), 0, zWideCmd);
cleanup_pipes:
	if( hChildStdoutRd ) CloseHandle(hChildStdoutRd);
	if( hChildStdoutWr ) CloseHandle(hChildStdoutWr);
	if( hChildStdinRd ) CloseHandle(hChildStdinRd);
	if( hChildStdinWr ) CloseHandle(hChildStdinWr);
	return NULL;
}

/*
 * Custom Windows pclose implementation that properly waits for process completion.
 */
static int WinPclose(FILE *pFile, HANDLE hProcess)
{
	DWORD dwExitCode = 0;
	int status;

	/* Close the FILE* (this closes the pipe) */
	fclose(pFile);

	if( hProcess ){
		/* Wait for the process to complete */
		WaitForSingleObject(hProcess, INFINITE);

		if( GetExitCodeProcess(hProcess, &dwExitCode) ){
			status = (int)dwExitCode;
		}else{
			status = -1;
		}

		/* Close process handle */
		CloseHandle(hProcess);
	}else{
		status = -1;
	}

	return status;
}
#endif /* __WINNT__ */
/*
 * Open a pipe to a process.
 * This is called internally by popen(), not through the stream device interface.
 */
static pipe_private * PipeOpen(ph7_vm *pVm, const char *zCommand, const char *zMode)
{
	pipe_private *pPipe;
	FILE *pFile;
	if( pVm == 0 || zCommand == 0 || zMode == 0 ){
		return 0;
	}
	/* Validate mode - only 'r' or 'w' allowed */
	if( zMode[0] != 'r' && zMode[0] != 'w' ){
		return 0;
	}
	/* Open the pipe using system popen */
#ifdef __WINNT__
	{
		/* Build cmd.exe command wrapper */
		const char *zShellPrefix = "cmd.exe /c \"";
		const char *zShellSuffix = "\"";
		size_t nPrefix = strlen(zShellPrefix);
		size_t nSuffix = strlen(zShellSuffix);
		size_t nCmd = strlen(zCommand);
		size_t nQuotes = 0;
		for (size_t i = 0; i < nCmd; ++i) {
			if (zCommand[i] == '"') nQuotes++;
		}
		size_t nCmdEsc = nCmd + nQuotes;
		char *zCmdEsc = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)(nCmdEsc + 1));
		if (zCmdEsc == NULL) {
			return 0;
		}
		/* Escape quotes in command */
		size_t j = 0;
		for (size_t i = 0; i < nCmd; ++i) {
			char ch = zCommand[i];
			if (ch == '"') {
				zCmdEsc[j++] = '^';
				zCmdEsc[j++] = '"';
			} else {
				zCmdEsc[j++] = ch;
			}
		}
		zCmdEsc[j] = '\0';
		size_t nTotal = nPrefix + nCmdEsc + nSuffix + 1;
		char *zWinCmd = (char *)SyMemBackendAlloc(&pVm->sAllocator, (sxu32)nTotal);
		if (zWinCmd == NULL) {
			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);
			return 0;
		}
		memcpy(zWinCmd, zShellPrefix, nPrefix);
		memcpy(zWinCmd + nPrefix, zCmdEsc, nCmdEsc);
		memcpy(zWinCmd + nPrefix + nCmdEsc, zShellSuffix, nSuffix);
		zWinCmd[nTotal - 1] = '\0';
		/* Allocate pipe structure early so we can store handles */
		pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));
		if( pPipe == 0 ){
			SyMemBackendFree(&pVm->sAllocator, zCmdEsc);
			SyMemBackendFree(&pVm->sAllocator, zWinCmd);
			return 0;
		}
		/* Use our custom WinPopen that properly tracks the process handle */
		pFile = WinPopen(zWinCmd, zMode, &pPipe->hProcess, &pPipe->hPipe);
		SyMemBackendFree(&pVm->sAllocator, zCmdEsc);
		SyMemBackendFree(&pVm->sAllocator, zWinCmd);
		if( pFile == 0 ){
			SyMemBackendFree(&pVm->sAllocator, pPipe);
			return 0;
		}
		/* Initialize remaining fields */
		pPipe->pFile = pFile;
		pPipe->pVm = pVm;
		pPipe->iMode = zMode[0];
	}
#elif defined(__UNIXES__) /* Unix */
	pFile = popen(zCommand, zMode);
	if( pFile == 0 ){
		return 0;
	}
	/* Allocate pipe private structure */
	pPipe = (pipe_private *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(pipe_private));
	if( pPipe == 0 ){
		/* Out of memory, close the pipe */
		pclose(pFile);
		return 0;
	}
	/* Initialize the structure */
	pPipe->pFile = pFile;
	pPipe->pVm = pVm;
	pPipe->iMode = zMode[0];
#else /* OS_OTHER: no process pipes on this platform */
	(void)pFile;
	return 0;
#endif
	return pPipe;
}
/*
 * Close a pipe and return the exit status of the process.
 * Returns the exit status, or -1 on error.
 */
static int PipeClose(pipe_private *pPipe)
{
	int status;
	ph7_vm *pVm;
	if( pPipe == 0 || pPipe->pFile == 0 ){
		return -1;
	}
	pVm = pPipe->pVm;
	/* Close the pipe and get exit status */
#ifdef __WINNT__
	/* Use our custom WinPclose that properly waits for process completion */
	status = WinPclose(pPipe->pFile, pPipe->hProcess);
#elif defined(__UNIXES__)
	status = pclose(pPipe->pFile);
	/* On Unix, pclose returns the status from waitpid, need to extract exit code */
	if( status != -1 ){
		if( WIFEXITED(status) ){
			status = WEXITSTATUS(status);
		}else if( WIFSIGNALED(status) ){
			/* Process was killed by a signal - use shell convention: 128 + signal number */
			status = 128 + WTERMSIG(status);
		}else{
			/* Unknown termination reason */
			status = -1;
		}
	}
#else /* OS_OTHER: no process pipes on this platform */
	status = -1;
#endif
	/* Free the structure */
	SyMemBackendFree(&pVm->sAllocator, pPipe);
	return status;
}
/*
 * Pipe stream xClose implementation.
 * Note: This is called by fclose(), not pclose().
 * It closes the pipe but does not return the exit status.
 */
static void PipeStream_Close(void *pHandle)
{
	pipe_private *pPipe = (pipe_private *)pHandle;
	if( pPipe ){
		PipeClose(pPipe);
	}
}
/*
 * Pipe stream xRead implementation.
 */
static ph7_int64 PipeStream_Read(void *pHandle, void *pBuffer, ph7_int64 nDatatoRead)
{
	pipe_private *pPipe = (pipe_private *)pHandle;
	size_t nRead;
	if( pPipe == 0 || pPipe->pFile == 0 ){
		return -1;
	}
	if( pPipe->iMode != 'r' ){
		/* Cannot read from a write-only pipe */
		return -1;
	}
	nRead = fread(pBuffer, 1, (size_t)nDatatoRead, pPipe->pFile);
	if( nRead == 0 ){
		if( feof(pPipe->pFile) ){
			return 0; /* EOF */
		}
		return -1; /* Error */
	}
	return (ph7_int64)nRead;
}
/*
 * Pipe stream xWrite implementation.
 */
static ph7_int64 PipeStream_Write(void *pHandle, const void *pBuf, ph7_int64 nWrite)
{
	pipe_private *pPipe = (pipe_private *)pHandle;
	size_t nWritten;
	if( pPipe == 0 || pPipe->pFile == 0 ){
		return -1;
	}
	if( pPipe->iMode != 'w' ){
		/* Cannot write to a read-only pipe */
		return -1;
	}
	nWritten = fwrite(pBuf, 1, (size_t)nWrite, pPipe->pFile);
	if( nWritten == 0 && nWrite > 0 ){
		return -1; /* Error */
	}
	return (ph7_int64)nWritten;
}
/* Export the pipe:// stream (used internally, not registered as a URI scheme) */
static const ph7_io_stream sPipe_Stream = {
	"pipe",
	PH7_IO_STREAM_VERSION,
	0,  /* xOpen - not used, pipes opened via PipeOpen() */
	0,  /* xOpenDir */
	PipeStream_Close,  /* xClose */
	0,  /* xCloseDir */
	PipeStream_Read,   /* xRead */
	0,  /* xReadDir */
	PipeStream_Write,  /* xWrite */
	0,  /* xSeek */
	0,  /* xLock */
	0,  /* xRewindDir */
	0,  /* xTell */
	0,  /* xTrunc */
	0,  /* xSync */
	0   /* xStat */
};
/*
 * Return TRUE if we are dealing with the pipe:// stream.
 * FALSE otherwise.
 */
static int is_pipe_stream(const ph7_io_stream *pStream)
{
	return pStream == &sPipe_Stream;
}
/*
 * resource popen(string $command, string $mode)
 *  Opens process file pointer.
 * Parameters
 *  $command
 *   The command to execute. Passed to the system shell.
 *  $mode
 *   The mode parameter specifies the type of access you require to the stream.
 *   'r' - Open for reading (read from the command's stdout).
 *   'w' - Open for writing (write to the command's stdin).
 * Return
 *  Returns a file pointer on success, or FALSE on error.
 */
/*
 * string|false|null shell_exec(string $command)
 *  Execute a command via the shell and return the complete output as a string.
 * Returns NULL when the command produces no output, FALSE when the pipe cannot be
 * opened. This is what the backtick operator compiles to, exactly as in php.
 */
PH7_PRIVATE int PH7_builtin_shell_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCommand;
	pipe_private *pPipe;
	SyBlob sOut;
	char zBuf[4096];
	size_t nRead;
	int nCmdLen;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);
	if( nCmdLen < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");
	if( pPipe == 0 || pPipe->pFile == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	for(;;){
		nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);
		if( nRead < 1 ){
			break;
		}
		SyBlobAppend(&sOut,zBuf,(sxu32)nRead);
	}
	PipeClose(pPipe);
	if( SyBlobLength(&sOut) < 1 ){
		/* php answers NULL, not "", when the command printed nothing */
		ph7_result_null(pCtx);
	}else{
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCommand, *zMode;
	pipe_private *pPipe;
	io_private *pDev;
	int nCmdLen, nModeLen;
	if( nArg < 2 || !ph7_value_is_string(apArg[0]) || !ph7_value_is_string(apArg[1]) ){
		/* Missing/Invalid arguments, return FALSE */
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a command string and mode");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* Extract the command and mode */
	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);
	zMode = ph7_value_to_string(apArg[1], &nModeLen);
	if( nCmdLen < 1 ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Empty command");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	if( nModeLen < 1 || (zMode[0] != 'r' && zMode[0] != 'w') ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Invalid mode, expected 'r' or 'w'");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* Open the pipe */
	pPipe = PipeOpen(pCtx->pVm, zCommand, zMode);
	if( pPipe == 0 ){
		/* Failed to open pipe */
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* Allocate an io_private instance to wrap the pipe */
	pDev = (io_private *)ph7_context_alloc_chunk(pCtx, sizeof(io_private), TRUE, FALSE);
	if( pDev == 0 ){
		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "PH7 is running out of memory");
		PipeClose(pPipe);
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	/* Initialize the io_private structure */
	InitIOPrivate(pCtx->pVm, &sPipe_Stream, pDev);
	pDev->pHandle = pPipe;
	/* Return the io_private instance as a resource */
	ph7_result_resource(pCtx, pDev);
	return PH7_OK;
}
/*
 * int pclose(resource $handle)
 *  Closes a process file pointer opened by popen() and returns the exit code.
 * Parameters
 *  $handle
 *   The file pointer must be valid, and must have been returned by popen().
 * Return
 *  Returns the termination status of the process that was run, or -1 on error.
 */
PH7_PRIVATE int PH7_builtin_pclose(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const ph7_io_stream *pStream;
	pipe_private *pPipe;
	io_private *pDev;
	int status;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		/* Missing/Invalid arguments, return -1 */
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");
		ph7_result_int(pCtx, -1);
		return PH7_OK;
	}
	/* Extract our private data */
	pDev = (io_private *)ph7_value_to_resource(apArg[0]);
	/* Make sure we are dealing with a valid io_private instance */
	if( IO_PRIVATE_INVALID(pDev) ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting an IO handle");
		ph7_result_int(pCtx, -1);
		return PH7_OK;
	}
	/* Point to the target IO stream device */
	pStream = pDev->pStream;
	if( pStream == 0 || !is_pipe_stream(pStream) ){
		ph7_context_throw_error(pCtx, PH7_CTX_WARNING, "Expecting a pipe handle from popen()");
		ph7_result_int(pCtx, -1);
		return PH7_OK;
	}
	/* Get the pipe handle */
	pPipe = (pipe_private *)pDev->pHandle;
	/* Close the pipe and get exit status */
	status = PipeClose(pPipe);
	/* Keep the handle alive but flag it closed so shared copies see it */
	MarkIOPrivateClosed(pDev);
	/* Return the exit status */
	ph7_result_int(pCtx, status);
	return PH7_OK;
}
/*
 * proc_open() / proc_close() / proc_get_status() / proc_terminate()
 *   Run a command via fork()/exec() with fine-grained control over its
 *   standard descriptors (php's process-control family). The returned
 *   "process" resource wraps a small proc_private whose leading bytes mirror
 *   io_private (a distinct magic) so is_resource()/gettype() probes stay in
 *   bounds and report it as a live, non-stream resource.
 */
#ifdef __UNIXES__
#define PROC_PRIVATE_MAGIC 0x9C0DE5
#define PROC_MAX_DESC 16
typedef struct proc_private proc_private;
struct proc_private
{
	io_private base;   /* io_private-compatible header (base.iMagic == PROC_PRIVATE_MAGIC) */
	int pid;           /* child process id */
	int running;       /* TRUE until reaped by proc_close/proc_get_status */
	int exit_code;     /* cached exit status once reaped */
};
/* Wrap a raw fd as an fopen-style stream resource (a php pipe end). */
static io_private * ProcWrapFd(ph7_vm *pVm,int fd)
{
	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
	if( pDev == 0 ){
		return 0;
	}
	InitIOPrivate(pVm,&sUnixFileStream,pDev);
	pDev->pHandle = SX_INT_TO_PTR(fd);
	return pDev;
}
/* One parsed descriptor-spec entry. */
struct proc_desc
{
	int child_fd;      /* the array key: which fd the child sees */
	int kind;          /* 0=pipe, 1=file, 2=redirect */
	/* pipe */
	int child_end;     /* fd the child must have at child_fd */
	int parent_end;    /* fd the parent keeps (wrapped into $pipes), -1 if none */
	/* file */
	int file_fd;       /* opened fd for a ['file',path,mode] spec */
	/* redirect */
	int redirect_to;   /* target child fd for a ['redirect',N] spec */
};
PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct proc_desc aDesc[PROC_MAX_DESC];
	int nDesc = 0;
	ph7_value *pSpec, *pPipes, *pEntry, *pType, *pParam;
	ph7_hashmap *pSpecMap;
	ph7_hashmap_node *pNode;
	ph7_vm *pVm = pCtx->pVm;
	char **azArgv = 0;      /* exec argv when the command is an array */
	int nArgv = 0;
	const char *zCmd = 0;   /* exec command when it is a string (via /bin/sh -c) */
	const char *zCwd = 0;
	char **azEnv = 0;       /* constructed envp when an env array is supplied */
	int nEnv = 0;
	proc_private *pProc;
	pid_t pid;
	int i, rc;
	if( nArg < 3 || !ph7_value_is_array(apArg[1]) ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() expects a command and a descriptor spec");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* --- Command: array (execvp) or string (/bin/sh -c) --- */
	if( ph7_value_is_array(apArg[0]) ){
		ph7_hashmap *pCmdMap = (ph7_hashmap *)apArg[0]->x.pOther;
		int nCount = (int)ph7_array_count(apArg[0]);
		azArgv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));
		if( azArgv == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }
		pNode = pCmdMap->pFirst;
		for( i = 0 ; i < nCount && pNode ; ++i ){
			ph7_value *pv = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));
			int nLen; const char *zs;
			PH7_MemObjInit(pVm,pv);
			PH7_HashmapExtractNodeValue(pNode,pv,FALSE);
			zs = ph7_value_to_string(pv,&nLen);
			azArgv[nArgv] = (char *)SyMemBackendDup(&pVm->sAllocator,zs,(sxu32)nLen+1);
			if( azArgv[nArgv] ){ azArgv[nArgv][nLen] = 0; nArgv++; }
			PH7_MemObjRelease(pv);
			SyMemBackendFree(&pVm->sAllocator,pv);
			pNode = pNode->pPrev; /* hashmap insertion-order walk */
		}
		azArgv[nArgv] = 0;
	}else{
		int nLen;
		zCmd = ph7_value_to_string(apArg[0],&nLen);
	}
	/* --- Optional cwd (arg 4) and env (arg 5) --- */
	if( nArg > 3 && ph7_value_is_string(apArg[3]) ){
		int nLen; zCwd = ph7_value_to_string(apArg[3],&nLen);
		if( nLen < 1 ){ zCwd = 0; }
	}
	if( nArg > 4 && ph7_value_is_array(apArg[4]) ){
		ph7_hashmap *pEnvMap = (ph7_hashmap *)apArg[4]->x.pOther;
		int nCount = (int)ph7_array_count(apArg[4]);
		azEnv = (char **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(char *)*(nCount+1));
		if( azEnv ){
			pNode = pEnvMap->pFirst;
			for( i = 0 ; i < nCount && pNode ; ++i ){
				ph7_value sKey, sVal; int nk, nv; const char *zk, *zv; char *zPair;
				PH7_MemObjInit(pVm,&sKey); PH7_MemObjInit(pVm,&sVal);
				PH7_HashmapExtractNodeKey(pNode,&sKey);
				PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
				zk = ph7_value_to_string(&sKey,&nk);
				zv = ph7_value_to_string(&sVal,&nv);
				zPair = (char *)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nk+nv+2));
				if( zPair ){
					SyMemcpy(zk,zPair,(sxu32)nk); zPair[nk] = '=';
					SyMemcpy(zv,&zPair[nk+1],(sxu32)nv); zPair[nk+1+nv] = 0;
					azEnv[nEnv++] = zPair;
				}
				PH7_MemObjRelease(&sKey); PH7_MemObjRelease(&sVal);
				pNode = pNode->pPrev;
			}
			azEnv[nEnv] = 0;
		}
	}
	/* --- Parse the descriptor spec, creating pipes as we go --- */
	pSpec = apArg[1];
	pSpecMap = (ph7_hashmap *)pSpec->x.pOther;
	pNode = pSpecMap->pFirst;
	for( i = 0 ; i < (int)pSpecMap->nEntry && pNode && nDesc < PROC_MAX_DESC ; ++i ){
		ph7_value sKey; struct proc_desc *pD = &aDesc[nDesc];
		PH7_MemObjInit(pVm,&sKey);
		PH7_HashmapExtractNodeKey(pNode,&sKey);
		pD->child_fd = ph7_value_to_int(&sKey);
		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1;
		PH7_MemObjRelease(&sKey);
		pEntry = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_value));
		PH7_MemObjInit(pVm,pEntry);
		PH7_HashmapExtractNodeValue(pNode,pEntry,FALSE);
		if( ph7_value_is_array(pEntry) ){
			int nLen; const char *zType;
			pType = ph7_array_fetch(pEntry,"0",1);
			zType = pType ? ph7_value_to_string(pType,&nLen) : "";
			if( SyStrncmp(zType,"pipe",4) == 0 ){
				int fds[2];
				if( pipe(fds) == 0 ){
					pParam = ph7_array_fetch(pEntry,"1",1);
					{
						int nMode; const char *zMode = pParam ? ph7_value_to_string(pParam,&nMode) : "r";
						pD->kind = 0;
						if( zMode[0] == 'w' || zMode[0] == 'a' ){
							/* child writes -> parent reads: child gets write end */
							pD->child_end = fds[1]; pD->parent_end = fds[0];
						}else{
							/* child reads -> parent writes: child gets read end */
							pD->child_end = fds[0]; pD->parent_end = fds[1];
						}
						nDesc++;
					}
				}
			}else if( SyStrncmp(zType,"file",4) == 0 ){
				int nLen2, nLen3; const char *zPath, *zMode; int oflag = O_RDONLY;
				ph7_value *pPath = ph7_array_fetch(pEntry,"1",1);
				ph7_value *pMode = ph7_array_fetch(pEntry,"2",1);
				zPath = pPath ? ph7_value_to_string(pPath,&nLen2) : "";
				zMode = pMode ? ph7_value_to_string(pMode,&nLen3) : "r";
				if( zMode[0] == 'w' ){ oflag = O_WRONLY|O_CREAT|O_TRUNC; }
				else if( zMode[0] == 'a' ){ oflag = O_WRONLY|O_CREAT|O_APPEND; }
				pD->kind = 1;
				pD->file_fd = open(zPath,oflag,0644);
				nDesc++;
			}else if( SyStrncmp(zType,"redirect",8) == 0 ){
				pParam = ph7_array_fetch(pEntry,"1",1);
				pD->kind = 2;
				pD->redirect_to = pParam ? ph7_value_to_int(pParam) : 1;
				nDesc++;
			}
		}
		PH7_MemObjRelease(pEntry);
		SyMemBackendFree(&pVm->sAllocator,pEntry);
		pNode = pNode->pPrev;
	}
	/* --- Fork the child --- */
	pid = fork();
	if( pid < 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open(): fork() failed");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pid == 0 ){
		/* Child: wire up descriptors then exec */
		for( i = 0 ; i < nDesc ; ++i ){
			struct proc_desc *pD = &aDesc[i];
			if( pD->kind == 0 ){
				dup2(pD->child_end,pD->child_fd);
				close(pD->parent_end);
				close(pD->child_end);
			}else if( pD->kind == 1 && pD->file_fd >= 0 ){
				dup2(pD->file_fd,pD->child_fd);
				close(pD->file_fd);
			}
		}
		/* Redirects run after the pipes are in place (e.g. 2>&1) */
		for( i = 0 ; i < nDesc ; ++i ){
			if( aDesc[i].kind == 2 ){ dup2(aDesc[i].redirect_to,aDesc[i].child_fd); }
		}
		if( zCwd ){ if( chdir(zCwd) != 0 ){ _exit(127); } }
		if( azEnv ){
			if( azArgv ){ execve(azArgv[0],azArgv,azEnv); }
			else{ char *av[4]; av[0]=(char*)"sh"; av[1]=(char*)"-c"; av[2]=(char*)zCmd; av[3]=0; execve("/bin/sh",av,azEnv); }
		}else{
			if( azArgv ){ execvp(azArgv[0],azArgv); }
			else{ execl("/bin/sh","sh","-c",zCmd,(char *)0); }
		}
		_exit(127); /* exec failed */
	}
	/* Parent: close the child ends, wrap the parent ends into $pipes */
	pPipes = ph7_context_new_array(pCtx);
	for( i = 0 ; i < nDesc ; ++i ){
		struct proc_desc *pD = &aDesc[i];
		if( pD->kind == 0 ){
			io_private *pEnd;
			ph7_value *pRes;
			close(pD->child_end);
			pEnd = ProcWrapFd(pVm,pD->parent_end);
			pRes = ph7_context_new_scalar(pCtx);
			if( pEnd && pRes && pPipes ){
				ph7_value_resource(pRes,pEnd);
				ph7_array_add_intkey_elem(pPipes,pD->child_fd,pRes);
			}
			if( pRes ){ ph7_context_release_value(pCtx,pRes); }
		}else if( pD->kind == 1 && pD->file_fd >= 0 ){
			close(pD->file_fd);
		}
	}
	if( pPipes ){
		PH7_VmStoreArgByRef(pVm,apArg[2],pPipes);
	}
	/* Free the exec argv/env copies now that the child owns its own image */
	if( azArgv ){
		for( i = 0 ; i < nArgv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azArgv[i]); }
		SyMemBackendFree(&pVm->sAllocator,azArgv);
	}
	if( azEnv ){
		for( i = 0 ; i < nEnv ; ++i ){ SyMemBackendFree(&pVm->sAllocator,azEnv[i]); }
		SyMemBackendFree(&pVm->sAllocator,azEnv);
	}
	/* Build the process resource */
	pProc = (proc_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(proc_private));
	if( pProc == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }
	SyZero(pProc,sizeof(proc_private));
	pProc->base.iMagic = PROC_PRIVATE_MAGIC;
	pProc->pid = (int)pid;
	pProc->running = 1;
	pProc->exit_code = 0;
	ph7_result_resource(pCtx,pProc);
	(void)rc;
	return PH7_OK;
}
/* Reap the child if it has not been reaped yet, caching the exit code. */
static void ProcReap(proc_private *pProc,int block)
{
	int status = 0;
	pid_t r;
	if( !pProc->running ){ return; }
	r = waitpid((pid_t)pProc->pid,&status,block ? 0 : WNOHANG);
	if( r == (pid_t)pProc->pid ){
		pProc->running = 0;
		if( WIFEXITED(status) ){ pProc->exit_code = WEXITSTATUS(status); }
		else if( WIFSIGNALED(status) ){ pProc->exit_code = 128 + WTERMSIG(status); }
	}
}
PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	proc_private *pProc;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);
	if( pProc == 0 || pProc->base.iMagic != PROC_PRIVATE_MAGIC ){
		ph7_result_int(pCtx,-1);
		return PH7_OK;
	}
	ProcReap(pProc,1/*block until it exits*/);
	ph7_result_int(pCtx,pProc->exit_code);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	proc_private *pProc;
	int sig = 15; /* SIGTERM */
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);
	if( pProc == 0 || pProc->base.iMagic != PROC_PRIVATE_MAGIC ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 ){ sig = ph7_value_to_int(apArg[1]); }
	if( pProc->running ){ kill((pid_t)pProc->pid,sig); }
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	proc_private *pProc;
	ph7_value *pArray, *pVal;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pProc = (proc_private *)ph7_value_to_resource(apArg[0]);
	if( pProc == 0 || pProc->base.iMagic != PROC_PRIVATE_MAGIC ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ProcReap(pProc,0/*non-blocking poll*/);
	pArray = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pVal == 0 ){ ph7_result_bool(pCtx,0); return PH7_OK; }
	ph7_value_int(pVal,pProc->pid);
	ph7_array_add_strkey_elem(pArray,"pid",pVal);
	ph7_value_bool(pVal,pProc->running);
	ph7_array_add_strkey_elem(pArray,"running",pVal);
	ph7_value_bool(pVal,0);
	ph7_array_add_strkey_elem(pArray,"signaled",pVal);
	ph7_array_add_strkey_elem(pArray,"stopped",pVal);
	ph7_value_int(pVal,pProc->running ? -1 : pProc->exit_code);
	ph7_array_add_strkey_elem(pArray,"exitcode",pVal);
	ph7_value_int(pVal,0);
	ph7_array_add_strkey_elem(pArray,"termsig",pVal);
	ph7_array_add_strkey_elem(pArray,"stopsig",pVal);
	ph7_context_release_value(pCtx,pVal);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
#else /* !__UNIXES__ */
PH7_PRIVATE int PH7_builtin_proc_open(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SXUNUSED(nArg); SXUNUSED(apArg);
	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"proc_open() is not available on this platform");
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_proc_close(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_int(pCtx,-1); return PH7_OK; }
PH7_PRIVATE int PH7_builtin_proc_terminate(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }
PH7_PRIVATE int PH7_builtin_proc_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)
{ SXUNUSED(nArg); SXUNUSED(apArg); ph7_result_bool(pCtx,0); return PH7_OK; }
#endif /* __UNIXES__ */
/* Export the php:// stream */
PH7_PRIVATE const ph7_io_stream sPHP_Stream = {
	"php",
	PH7_IO_STREAM_VERSION,
	PHPStreamData_Open,  /* xOpen */
	0,   /* xOpenDir */
	PHPStreamData_Close, /* xClose */
	0,  /* xCloseDir */
	PHPStreamData_Read,  /* xRead */
	0,  /* xReadDir */
	PHPStreamData_Write, /* xWrite */
	PHPStreamData_Seek,  /* xSeek (php://memory & php://temp) */
	0,  /* xLock */
	0,  /* xRewindDir */
	PHPStreamData_Tell,  /* xTell */
	PHPStreamData_Trunc, /* xTrunc */
	0,  /* xSync */
	0   /* xStat */
};
#endif /* PH7_DISABLE_DISK_IO */
/*
 * Return TRUE if we are dealing with the php:// stream.
 * FALSE otherwise.
 */
PH7_PRIVATE int is_php_stream(const ph7_io_stream *pStream)
{
#ifndef PH7_DISABLE_DISK_IO
	return pStream == &sPHP_Stream;
#else
	SXUNUSED(pStream); /* cc warning */
	return 0;
#endif /* PH7_DISABLE_DISK_IO */
}
/*
 * Return TRUE if we are dealing with the data:// stream.
 */
PH7_PRIVATE int is_data_stream(const ph7_io_stream *pStream)
{
#ifndef PH7_DISABLE_DISK_IO
	return pStream == &sDATA_Stream;
#else
	SXUNUSED(pStream); /* cc warning */
	return 0;
#endif /* PH7_DISABLE_DISK_IO */
}
/*
 * bool stream_isatty(resource $stream)
 *  TRUE when the stream is an interactive terminal. PHL answers this for the
 *  php:// standard streams (STDIN/STDOUT/STDERR carry their fd) via the
 *  platform isatty()/GetFileType(); file and memory streams are never a
 *  terminal, so they answer FALSE (php-exact for those).
 */
PH7_PRIVATE int PH7_builtin_stream_isatty(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bTty = 0;
	if( nArg < 1 || !ph7_value_is_resource(apArg[0]) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
#ifndef PH7_DISABLE_DISK_IO
	{
		io_private *pDev = (io_private *)ph7_value_to_resource(apArg[0]);
		if( !IO_PRIVATE_INVALID(pDev) && is_php_stream(pDev->pStream) ){
			ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;
			if( pData && (pData->iType == PH7_IO_STREAM_STDIN
				|| pData->iType == PH7_IO_STREAM_STDOUT
				|| pData->iType == PH7_IO_STREAM_STDERR) ){
#ifdef __WINNT__
				bTty = (GetFileType((HANDLE)pData->x.pHandle) == FILE_TYPE_CHAR) ? 1 : 0;
#else
				bTty = isatty(SX_PTR_TO_INT(pData->x.pHandle)) ? 1 : 0;
#endif
			}
		}
	}
#endif /* PH7_DISABLE_DISK_IO */
	ph7_result_bool(pCtx,bTty);
	return PH7_OK;
}

#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * Export the STDIN handle.
 */
PH7_PRIVATE void * PH7_ExportStdin(ph7_vm *pVm)
{
#ifndef PH7_DISABLE_DISK_IO
	if( pVm->pStdin == 0  ){
		io_private *pIn;
		/* Allocate an IO private instance */
		pIn = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
		if( pIn == 0 ){
			return 0;
		}
		InitIOPrivate(pVm,&sPHP_Stream,pIn);
		/* Initialize the handle */
		pIn->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDIN);
		/* Install the STDIN stream */
		pVm->pStdin = pIn;
		return pIn;
	}else{
		/* NULL or STDIN */
		return pVm->pStdin;
	}
#else
	SXUNUSED(pVm); /* cc warning */
	return 0;
#endif
}
/*
 * Export the STDOUT handle.
 */
PH7_PRIVATE void * PH7_ExportStdout(ph7_vm *pVm)
{
#ifndef PH7_DISABLE_DISK_IO
	if( pVm->pStdout == 0  ){
		io_private *pOut;
		/* Allocate an IO private instance */
		pOut = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
		if( pOut == 0 ){
			return 0;
		}
		InitIOPrivate(pVm,&sPHP_Stream,pOut);
		/* Initialize the handle */
		pOut->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDOUT);
		/* Install the STDOUT stream */
		pVm->pStdout = pOut;
		return pOut;
	}else{
		/* NULL or STDOUT */
		return pVm->pStdout;
	}
#else
	SXUNUSED(pVm); /* cc warning */
	return 0;
#endif
}
/*
 * Export the STDERR handle.
 */
PH7_PRIVATE void * PH7_ExportStderr(ph7_vm *pVm)
{
#ifndef PH7_DISABLE_DISK_IO
	if( pVm->pStderr == 0  ){
		io_private *pErr;
		/* Allocate an IO private instance */
		pErr = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
		if( pErr == 0 ){
			return 0;
		}
		InitIOPrivate(pVm,&sPHP_Stream,pErr);
		/* Initialize the handle */
		pErr->pHandle = PHPStreamDataInit(pVm,PH7_IO_STREAM_STDERR);
		/* Install the STDERR stream */
		pVm->pStderr = pErr;
		return pErr;
	}else{
		/* NULL or STDERR */
		return pVm->pStderr;
	}
#else
	SXUNUSED(pVm); /* cc warning */
	return 0;
#endif
}
