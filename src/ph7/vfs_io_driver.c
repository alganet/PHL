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
		if( nRd < 0 ){
			return -1;
		}
		/* ZERO is end of file, not an error — the contract every other device
		 * here keeps. Collapsing the two meant nothing could ever latch EOF on
		 * php://stdin, so `while (!feof(STDIN))` never ended. */
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
	BOOL bBinary = (strchr(zMode,'b') != NULL);

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

	/* Convert OS handle to C file descriptor, then to FILE*. The TRANSLATION mode
	 * is the caller's, not a constant: cmd.exe writes CRLF, and a descriptor
	 * opened _O_TEXT eats every CR on the way in. php picks it per call —
	 * "rb" for exec/system/passthru, so their output is byte-exact, "rt" for
	 * shell_exec, and the script's own mode for popen() — and this used to
	 * hardcode _O_TEXT for all of them, so `passthru('type file.bin')` lost every
	 * 0x0D byte and system()'s output came back LF-only where php's is CRLF. */
	fd = _open_osfhandle((intptr_t)(bRead ? hReadPipe : hWritePipe),
	                     (bRead ? _O_RDONLY : _O_WRONLY)
	                     | (bBinary ? _O_BINARY : _O_TEXT));
	if( fd == -1 ){
		CloseHandle(pi.hProcess);
		*phProcess = NULL;
		goto cleanup_all;
	}

	/* The stream's own translation has to agree with the descriptor's */
	pFile = _fdopen(fd, bBinary ? (bRead ? "rb" : "wb") : (bRead ? "rt" : "wt"));
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
	/* The mode goes to popen(3) VERBATIM, exactly as php hands it its own: a mode
	 * popen(3) refuses (anything but "r"/"w" — 'b' is a Windows translation flag
	 * with nothing to translate here) is an open FAILURE, which is php's answer
	 * for it too. */
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
	/* pclose() answers waitpid()'s raw status. php (php_stream_pclose) translates
	 * exactly ONE case of it — a normal exit becomes its exit CODE — and hands the
	 * raw word back for every other, so a process killed by a signal reports the
	 * SIGNAL number: `kill -TERM $$` is 15, `kill -9 $$` is 9. PHL used to add the
	 * shell's own 128 to it (143, 137), a number the same status word can also
	 * mean as an ordinary `exit 143`, and answered -1 for a stopped child. This is
	 * pclose()'s answer and, through it, exec()/system()/passthru()'s
	 * $result_code. */
	if( status != -1 && WIFEXITED(status) ){
		status = WEXITSTATUS(status);
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
 * The longest command line the platform's shell accepts — php's `cmd_max_len`,
 * which both escapers refuse to exceed. php reads it once at startup from
 * sysconf(_SC_ARG_MAX) and hardcodes cmd.exe's constant on Windows.
 */
static sxu32 ShellMaxCmdLen(void)
{
#ifdef __WINNT__
	/* An escaped command runs through cmd.exe, whose limit is a constant. */
	return 8192;
#elif defined(__UNIXES__) && defined(_SC_ARG_MAX)
	long iMax = sysconf(_SC_ARG_MAX);
	if( iMax <= 0 ){
		return 4096;   /* php's _POSIX_ARG_MAX fallback */
	}
	return (sxu32)iMax;
#else
	return 4096;
#endif
}
/*
 * How the two escapers WALK their argument, and the one thing they share.
 *
 * php walks it with php_mblen(), the process LC_CTYPE's multibyte reader: a
 * well-formed sequence is copied through untouched (a metacharacter's byte value
 * inside one is NOT a metacharacter), and a byte the encoding cannot start a
 * character with is DROPPED. That reader's answer is platform-shaped, and this
 * follows it on both, because it is what php answers on each:
 *
 *   POSIX    php picks LC_CTYPE up from the environment at startup, so the
 *            everyday answer is a UTF-8 one — a well-formed sequence rides
 *            through and an ill-formed byte is dropped. PHL is UTF-8-only (§10)
 *            and has no setlocale, so PH7_Utf8ReadStrict IS that reader.
 *            (php in the "C" locale glibc falls back to drops every byte >= 0x80
 *            instead, which is why the corpus guards this half on the oracle's
 *            own LC_CTYPE rather than pinning it unconditionally.)
 *   Windows  php reports LC_CTYPE "C", and MSVCRT's C locale is SINGLE-BYTE, not
 *            ASCII: mblen() answers 1 for every byte, so nothing is ever dropped
 *            and `\xFF` reaches the escape table below. Verified against php
 *            8.5.8 on the gate VM, which does have an oracle — walking UTF-8
 *            there instead deleted bytes php keeps.
 *
 * Neither escaper can see a NUL byte: php parses both parameters with
 * Z_PARAM_PATH and the central screen (VmBuiltinPathMask) refuses one first.
 */
static int ShellCharIsWellFormed(const unsigned char *zIn,sxu32 nLeft,sxu32 *pnSeq)
{
#ifdef __WINNT__
	SXUNUSED(zIn);
	SXUNUSED(nLeft);
	*pnSeq = 1;
	return 1;
#else
	return PH7_Utf8ReadStrict(zIn,nLeft,pnSeq) >= 0;
#endif
}
/*
 * php's escapeshellarg(): wrap the whole argument in quotes the shell does not
 * look inside, and neutralise the one byte that could end them.
 *
 * POSIX: single quotes, and a `'` becomes `'\''` — close, escape, reopen.
 * Windows: double quotes; there is no in-quote escape for `"` on cmd.exe, so php
 * REPLACES `"` (and `%`/`!`, which cmd.exe still expands inside quotes) with a
 * space, and doubles a trailing ODD run of backslashes so the last one escapes
 * itself rather than the closing quote.
 */
static void ShellEscapeArg(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)
{
	sxu32 i = 0;
#ifdef __WINNT__
	SyBlobAppend(pOut,"\"",sizeof(char));
#else
	SyBlobAppend(pOut,"'",sizeof(char));
#endif
	while( i < nLen ){
		sxu32 nSeq = 1;
		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){
			/* Ill-formed: php skips the byte rather than escaping it */
			i += nSeq;
			continue;
		}
		if( nSeq > 1 ){
			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);
			i += nSeq;
			continue;
		}
#ifdef __WINNT__
		if( zIn[i] == '"' || zIn[i] == '%' || zIn[i] == '!' ){
			SyBlobAppend(pOut," ",sizeof(char));
		}else{
			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));
		}
#else
		if( zIn[i] == '\'' ){
			SyBlobAppend(pOut,"'\\'",sizeof("'\\'")-1);
		}
		SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));
#endif
		i++;
	}
#ifdef __WINNT__
	{
		/* A trailing run of backslashes would escape the closing quote if it is
		 * odd; the opening quote at offset 0 stops the scan the way php's does. */
		const char *zCur = (const char *)SyBlobData(pOut);
		sxu32 nCur = SyBlobLength(pOut);
		sxu32 k = 0;
		while( k < nCur && zCur[nCur - 1 - k] == '\\' ){
			k++;
		}
		if( (k & 1) != 0 ){
			SyBlobAppend(pOut,"\\",sizeof(char));
		}
	}
	SyBlobAppend(pOut,"\"",sizeof(char));
#else
	SyBlobAppend(pOut,"'",sizeof(char));
#endif
}
/*
 * php's escapeshellcmd(): the argument is a COMMAND, so it is not quoted at all —
 * every byte that could break out of one is prefixed with the shell's escape
 * character instead (`\` on POSIX, `^` on cmd.exe).
 *
 * The one shape that is not a straight escape is a quote on POSIX: php leaves a
 * PAIR of them alone (the command may legitimately quote one of its own
 * arguments) and escapes an unpaired one. `pPair` is php's own one-slot state for
 * that — it remembers the partner it found for the quote currently open, so the
 * closing one is recognised and the pairing resets. cmd.exe has no such rule, so
 * both quote characters (and `%`/`!`) are ordinary escapes there.
 */
static void ShellEscapeCmd(const unsigned char *zIn,sxu32 nLen,SyBlob *pOut)
{
	sxu32 i = 0;
#ifndef __WINNT__
	const unsigned char *pPair = 0;
	static const char zEsc[] = "\\";
#else
	static const char zEsc[] = "^";
#endif
	while( i < nLen ){
		sxu32 nSeq = 1;
		if( !ShellCharIsWellFormed(&zIn[i],nLen - i,&nSeq) ){
			i += nSeq;
			continue;
		}
		if( nSeq > 1 ){
			SyBlobAppend(pOut,(const char *)&zIn[i],nSeq);
			i += nSeq;
			continue;
		}
		switch( zIn[i] ){
#ifndef __WINNT__
		case '"':
		case '\'':
			if( pPair == 0
			 && (pPair = (const unsigned char *)memchr(&zIn[i+1],zIn[i],nLen - i - 1)) != 0 ){
				/* This quote opens a pair: leave both of them alone */
			}else if( pPair != 0 && pPair[0] == zIn[i] ){
				pPair = 0;   /* the partner: pairing satisfied */
			}else{
				SyBlobAppend(pOut,zEsc,sizeof(char));
			}
			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));
			break;
#else
		/* cmd.exe expands %VAR% and !VAR! even inside quotes, and has no
		 * in-quote escape, so all four are plain `^` escapes there. */
		case '%':
		case '!':
		case '"':
		case '\'':
#endif
		case '#':
		case '&':
		case ';':
		case '`':
		case '|':
		case '*':
		case '?':
		case '~':
		case '<':
		case '>':
		case '^':
		case '(':
		case ')':
		case '[':
		case ']':
		case '{':
		case '}':
		case '$':
		case '\\':
		case 0x0A:
		/* php escapes 0xFF too, and this is the row that decides the walk above
		 * is worth getting right: it is unreachable under the UTF-8 walk (0xF5..
		 * 0xFF is never a lead byte, so the reader drops the byte first) and
		 * REACHED on Windows, where php's single-byte C locale hands it here —
		 * `escapeshellcmd("a\xffb")` is `a^\xffb` on the oracle. */
		case 0xFF:
			SyBlobAppend(pOut,zEsc,sizeof(char));
			/* fall through */
		default:
			SyBlobAppend(pOut,(const char *)&zIn[i],sizeof(char));
			break;
		}
		i++;
	}
}
/*
 * string escapeshellarg(string $arg)
 *  Escape an argument so a shell passes it to the command as ONE word, whatever
 *  it contains.
 */
PH7_PRIVATE int PH7_builtin_escapeshellarg(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxu32 nMax = ShellMaxCmdLen();
	const char *zArg;
	SyBlob sOut;
	int nLen;
	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */
	zArg = ph7_value_to_string(apArg[0],&nLen);
	/* php's own bound: the command line has to hold the two quotes and a NUL */
	if( nLen > 0 && (sxu32)nLen > nMax - 3 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Argument exceeds the allowed length of %d bytes",(int)nMax);
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	ShellEscapeArg((const unsigned char *)zArg,(sxu32)nLen,&sOut);
	if( SyBlobLength(&sOut) > nMax + 1 ){
		SyBlobRelease(&sOut);
		return PH7_VmThrowException(pCtx,"ValueError",
			"Escaped argument exceeds the allowed length of %d bytes",(int)nMax);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * string escapeshellcmd(string $command)
 *  Escape every character that could break out of a shell command.
 */
PH7_PRIVATE int PH7_builtin_escapeshellcmd(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxu32 nMax = ShellMaxCmdLen();
	const char *zCmd;
	SyBlob sOut;
	int nLen;
	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */
	zCmd = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php answers "" without running the escaper at all */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( (sxu32)nLen > nMax - 3 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Command exceeds the allowed length of %d bytes",(int)nMax);
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	ShellEscapeCmd((const unsigned char *)zCmd,(sxu32)nLen,&sOut);
	if( SyBlobLength(&sOut) > nMax + 1 ){
		SyBlobRelease(&sOut);
		return PH7_VmThrowException(pCtx,"ValueError",
			"Escaped command exceeds the allowed length of %d bytes",(int)nMax);
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * php refuses an EMPTY command in all four runners (exec/system/passthru/
 * shell_exec) — the shell would answer success for one, so the refusal is the
 * only way a script hears about a command string that came out empty.
 */
static sxi32 ShellEmptyCommandError(ph7_context *pCtx)
{
	return PH7_VmThrowException(pCtx,"ValueError",
		"%s(): Argument #1 ($command) must not be empty",ph7_function_name(pCtx));
}
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
		/* php refuses an empty command rather than running the shell on it */
		return ShellEmptyCommandError(pCtx);
	}
	pPipe = PipeOpen(pCtx->pVm,zCommand,"r");
	if( pPipe == 0 || pPipe->pFile == 0 ){
		/* php's own wording for this one; the three runners below say "Unable to
		 * fork [%s]" instead. Both used to be silent. */
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to execute '%s'",
			ph7_function_name(pCtx),zCommand);
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
/*
 * php's three command RUNNERS are one routine (php_exec) with a mode, and the
 * mode decides two things: what happens to each LINE of the command's output,
 * and what the call answers.
 *
 *   exec($cmd)           keep nothing, answer the LAST line
 *   exec($cmd, $output)  append every line to the array, answer the last line
 *   system($cmd)         WRITE every line as it arrives, answer the last line
 *   passthru($cmd)       write the raw bytes, answer NULL
 *
 * "The last line" is php's: its trailing WHITESPACE is stripped — spaces and
 * tabs as much as the newline — and so is every element of $output's. A command
 * that printed nothing answers "" rather than false, which is php's documented
 * BC wart and not an error indication; the error indication is FALSE, and only
 * a pipe that could not be opened produces it.
 *
 * All three share the exit status, which is the pipe's close status (php's
 * $result_code out-param) and -1 when there was no process at all.
 */
#ifdef __WINNT__
# define SHELL_RUN_PIPE_MODE "rb"
#else
# define SHELL_RUN_PIPE_MODE "r"
#endif
#define SHELL_RUN_LAST     0   /* exec() with no $output array */
#define SHELL_RUN_ECHO     1   /* system() */
#define SHELL_RUN_COLLECT  2   /* exec() with one */
#define SHELL_RUN_RAW      3   /* passthru() */
/*
 * php's strip_trailing_whitespace(): answers the length that stays.
 */
static sxu32 ShellStripTrailing(const char *zLine,sxu32 nLine)
{
	while( nLine > 0 && SyisSpace((unsigned char)zLine[nLine - 1]) ){
		nLine--;
	}
	return nLine;
}
/*
 * One complete line of output, dealt with the mode's way. The line still carries
 * its own newline: system() writes it (php hands the whole line to the output
 * layer, so an output buffer catches it like any echo), and the collector strips
 * it along with the rest of the trailing whitespace.
 */
static sxi32 ShellHandleLine(ph7_context *pCtx,int iType,ph7_value *pArray,
	const char *zLine,sxu32 nLine)
{
	if( iType == SHELL_RUN_ECHO ){
		return ph7_context_output(pCtx,zLine,(int)nLine);
	}
	if( iType == SHELL_RUN_COLLECT && pArray ){
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pVal == 0 ){
			return PH7_OK;
		}
		ph7_value_string(pVal,zLine,(int)ShellStripTrailing(zLine,nLine));
		ph7_array_add_elem(pArray,0,pVal);
		ph7_context_release_value(pCtx,pVal);
	}
	return PH7_OK;
}
/*
 * Run $command through the shell in the given mode, fill the by-reference
 * out-params and set the call's result. The three builtins below are this
 * routine plus their own mode.
 */
static sxi32 ShellRunCommand(ph7_context *pCtx,int iType,int nArg,ph7_value **apArg)
{
	/* exec() carries $output before $result_code; the other two do not */
	int iCodeArg = (iType == SHELL_RUN_LAST) ? 2 : 1;
	ph7_value *pArray = 0, *pOwned = 0;
	const char *zCommand;
	pipe_private *pPipe;
	SyBlob sLine, sLast;
	char zBuf[4096];
	size_t nRead;
	int nCmdLen, iStatus = -1;
	zCommand = ph7_value_to_string(apArg[0],&nCmdLen);
	if( nCmdLen < 1 ){
		return ShellEmptyCommandError(pCtx);
	}
	/* $output turns exec() into the collecting mode. php uses the array the
	 * caller already holds — the manual's "will append to the end of the array" —
	 * and replaces anything else with a fresh one, BEFORE running the command, so
	 * even a failed run leaves the variable an array. */
	if( iType == SHELL_RUN_LAST && nArg > 1 ){
		iType = SHELL_RUN_COLLECT;
		if( ph7_value_is_array(apArg[1]) ){
			PH7_HashmapCowSeparate(pCtx->pVm,apArg[1]);
			pArray = apArg[1];
		}else{
			pOwned = pArray = ph7_context_new_array(pCtx);
		}
	}
	/* php_exec's own mode, per platform: the three runners hand back what the
	 * command WROTE, so on Windows the CRs have to survive the pipe (where
	 * shell_exec() takes php's "rt" and does translate them). popen(3) refuses a
	 * 'b' it has nothing to translate, which is why this is not one string. */
	pPipe = PipeOpen(pCtx->pVm,zCommand,SHELL_RUN_PIPE_MODE);
	if( pPipe == 0 || pPipe->pFile == 0 ){
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(): Unable to fork [%s]",
			ph7_function_name(pCtx),zCommand);
		ph7_result_bool(pCtx,0);
	}else{
		int bAbort = 0;   /* the output consumer asked to stop (PH7_ABORT) */
		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);
		SyBlobInit(&sLast,&pCtx->pVm->sAllocator);
		for(;;){
			nRead = fread(zBuf,1,sizeof(zBuf),pPipe->pFile);
			if( nRead < 1 ){
				break;
			}
			if( iType == SHELL_RUN_RAW ){
				/* passthru() never looks for a line: php writes what it read */
				if( ph7_context_output(pCtx,zBuf,(int)nRead) == PH7_ABORT ){
					break;
				}
				continue;
			}
			{
				size_t iOfft = 0;
				while( iOfft < nRead ){
					const char *zNl = (const char *)memchr(&zBuf[iOfft],'\n',nRead - iOfft);
					size_t nChunk = zNl ? (size_t)(zNl - &zBuf[iOfft]) + 1 : nRead - iOfft;
					SyBlobAppend(&sLine,&zBuf[iOfft],(sxu32)nChunk);
					iOfft += nChunk;
					if( zNl == 0 ){
						break;   /* the line continues in the next read */
					}
					if( ShellHandleLine(pCtx,iType,pArray,
						(const char *)SyBlobData(&sLine),SyBlobLength(&sLine)) == PH7_ABORT ){
						bAbort = 1;
					}
					/* Keep it: the call answers the last line it saw */
					SyBlobReset(&sLast);
					SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));
					SyBlobReset(&sLine);
					if( bAbort ){
						break;
					}
				}
			}
			if( bAbort ){
				break;
			}
		}
		/* Output that ended without a newline is still a line */
		if( !bAbort && SyBlobLength(&sLine) > 0 ){
			ShellHandleLine(pCtx,iType,pArray,
				(const char *)SyBlobData(&sLine),SyBlobLength(&sLine));
			SyBlobReset(&sLast);
			SyBlobAppend(&sLast,SyBlobData(&sLine),SyBlobLength(&sLine));
		}
		iStatus = PipeClose(pPipe);
		if( iType == SHELL_RUN_RAW ){
			ph7_result_null(pCtx);
		}else{
			ph7_result_string(pCtx,(const char *)SyBlobData(&sLast),
				(int)ShellStripTrailing((const char *)SyBlobData(&sLast),SyBlobLength(&sLast)));
		}
		SyBlobRelease(&sLine);
		SyBlobRelease(&sLast);
	}
	if( pOwned ){
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pOwned);
		ph7_context_release_value(pCtx,pOwned);
	}
	if( nArg > iCodeArg ){
		ph7_value sVal;
		PH7_MemObjInitFromInt(pCtx->pVm,&sVal,iStatus);
		PH7_VmStoreArgByRef(pCtx->pVm,apArg[iCodeArg],&sVal);
		PH7_MemObjRelease(&sVal);
	}
	return PH7_OK;
}
/*
 * string|false exec(string $command, array &$output = null, int &$result_code = null)
 *  Run a command and answer the last line of its output.
 */
PH7_PRIVATE int PH7_builtin_exec(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ShellRunCommand(pCtx,SHELL_RUN_LAST,nArg,apArg);
}
/*
 * string|false system(string $command, int &$result_code = null)
 *  Run a command, write its output as it arrives, answer the last line.
 */
PH7_PRIVATE int PH7_builtin_system(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ShellRunCommand(pCtx,SHELL_RUN_ECHO,nArg,apArg);
}
/*
 * ?false passthru(string $command, int &$result_code = null)
 *  Run a command and write its output through, byte for byte.
 */
PH7_PRIVATE int PH7_builtin_passthru(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return ShellRunCommand(pCtx,SHELL_RUN_RAW,nArg,apArg);
}
/*
 * bool proc_nice(int $priority)
 *  Change the priority of the running process — the last member of php's own
 *  process-execution surface, and the only one of the seven that runs no shell.
 */
PH7_PRIVATE int PH7_builtin_proc_nice(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 iPri;
	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */
	iPri = ph7_value_to_int64(apArg[0]);
#ifdef __WINNT__
	{
		/* php's own mapping (win32/nice.c): cmd.exe has no nice value, so the
		 * POSIX increment is bucketed into the five priority CLASSES Windows
		 * has. REALTIME is deliberately not reachable there, and neither is it
		 * here. */
		DWORD dwFlag = NORMAL_PRIORITY_CLASS;
		if( iPri < -9 ){
			dwFlag = HIGH_PRIORITY_CLASS;
		}else if( iPri < -4 ){
			dwFlag = ABOVE_NORMAL_PRIORITY_CLASS;
		}else if( iPri > 9 ){
			dwFlag = IDLE_PRIORITY_CLASS;
		}else if( iPri > 4 ){
			dwFlag = BELOW_NORMAL_PRIORITY_CLASS;
		}
		SetPriorityClass(GetCurrentProcess(),dwFlag);
		/* php answers TRUE whatever that returned: its nice() reports failure
		 * through its return value and leaves errno alone, and proc_nice() reads
		 * only errno. */
		ph7_result_bool(pCtx,1);
	}
#elif defined(__UNIXES__)
	{
		int iIgnored;
		/* nice() legitimately answers -1 (it returns the NEW nice value), so
		 * errno is the only failure evidence — php clears it first for the same
		 * reason. */
		errno = 0;
		iIgnored = nice((int)iPri);
		(void)iIgnored;
		if( errno != 0 ){
			PH7_VmThrowWarningFmt(pCtx->pVm,
				"%s(): Only a super user may attempt to increase the priority of a process",
				ph7_function_name(pCtx));
			ph7_result_bool(pCtx,0);
		}else{
			ph7_result_bool(pCtx,1);
		}
	}
#else
	ph7_result_bool(pCtx,0);
#endif
	return PH7_OK;
}
PH7_PRIVATE int PH7_builtin_popen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCommand, *zMode;
	char zPosix[8];
	pipe_private *pPipe;
	io_private *pDev;
	int nCmdLen, nModeLen, nPosix, i, bDropped = 0;
	SXUNUSED(nArg);   /* Arity is enforced from aBuiltinSig[] before the call */
	/* Extract the command and mode */
	zCommand = ph7_value_to_string(apArg[0], &nCmdLen);
	zMode = ph7_value_to_string(apArg[1], &nModeLen);
	/*
	 * php's mode rule, and the only one it has: ONE 'b' — C's binary flag, which
	 * popen(3) itself refuses — is dropped from the mode on POSIX, and what is
	 * left must be exactly "r", "w", "rb" or "wb". PHL used to read mode[0] and
	 * hand the REST to popen(3) unexamined, which was wrong in both directions:
	 * `popen($cmd, 'rb')`, the ordinary binary spelling, answered FALSE because
	 * glibc rejected the 'b', and `popen($cmd, 'rr')` opened a pipe php refuses.
	 */
	nPosix = 0;
#ifdef __WINNT__
	SXUNUSED(bDropped);   /* cmd.exe keeps the 'b': _popen understands it */
#endif
	for( i = 0 ; i < nModeLen && nPosix < (int)sizeof(zPosix) - 1 ; ++i ){
#ifndef __WINNT__
		if( zMode[i] == 'b' && !bDropped ){
			bDropped = 1;   /* php drops the FIRST one and only that one */
			continue;
		}
#endif
		zPosix[nPosix++] = zMode[i];
	}
	zPosix[nPosix] = 0;
	if( nPosix > 2
	 || (nPosix == 1 && zPosix[0] != 'r' && zPosix[0] != 'w')
	 || (nPosix == 2 && SyMemcmp(zPosix,"rb",sizeof("rb")-1) != 0
	                 && SyMemcmp(zPosix,"wb",sizeof("wb")-1) != 0) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"popen(): Argument #2 ($mode) must be one of \"r\", \"rb\", \"w\", or \"wb\"");
	}
	/* Open the pipe. An EMPTY mode passes php's check above and fails HERE, in
	 * popen(3) — php reports it as an open failure and so does this, rather than
	 * letting the platform layer read mode[0] out of an empty string. */
	pPipe = nPosix > 0 ? PipeOpen(pCtx->pVm, zCommand, zPosix) : 0;
	if( pPipe == 0 ){
		/* php names both arguments in this one: `popen(cmd,mode): message`. PHL
		 * answered FALSE in silence, so a script had nothing to report. */
		if( nPosix < 1 ){
			errno = EINVAL;
		}
		PH7_VmThrowWarningFmt(pCtx->pVm,"%s(%s,%s): %s",
			ph7_function_name(pCtx),zCommand,zPosix,VfsStrerror(errno));
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
	/* A pipe has no wrapper and no path, so php's meta reports the MODE and
	 * neither `wrapper_type` nor `uri`: an empty URI is what leaves them out. */
	SetIOPrivateOpenedAs(pDev,0,0,zPosix,nPosix);
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
	/* A write chain gets its closing call while the pipe is still open. */
	PH7_StreamFilterReleaseChains(pDev);
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
static io_private * ProcWrapFd(ph7_vm *pVm,int fd,int bParentReads)
{
	io_private *pDev = (io_private *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(io_private));
	if( pDev == 0 ){
		return 0;
	}
	InitIOPrivate(pVm,&sUnixFileStream,pDev);
	/* Same shape as popen()'s end: a proc_open() pipe carries no path, and its
	 * mode is the direction the PARENT holds — the opposite of the child's. */
	SetIOPrivateOpenedAs(pDev,0,0,bParentReads ? "r" : "w",1);
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
	int parent_reads;  /* the parent's end is the READ end (the child writes) */
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
		pD->parent_end = -1; pD->file_fd = -1; pD->redirect_to = -1; pD->parent_reads = 0;
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
						pD->parent_reads = 1;
						}else{
							/* child reads -> parent writes: child gets read end */
							pD->child_end = fds[0]; pD->parent_end = fds[1];
						pD->parent_reads = 0;
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
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"fork() failed");
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
			pEnd = ProcWrapFd(pVm,pD->parent_end,pD->parent_reads);
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
 * Is this a handle php's plain-files device would own -- a file, a pipe, a
 * standard stream? php sets the blocking mode on those with O_NONBLOCK, which
 * Windows does not have, so there stream_set_blocking() answers FALSE for them.
 */
PH7_PRIVATE int PH7_StreamIsPlainDevice(io_private *pDev)
{
#ifndef PH7_DISABLE_DISK_IO
	if( pDev == 0 || pDev->pHandle == 0 || pDev->pStream == 0 || pDev->bDir ){
		return 0;
	}
#ifdef __WINNT__
	if( pDev->pStream == &sWinFileStream ){
		return 1;
	}
#elif defined(__UNIXES__)
	if( pDev->pStream == &sUnixFileStream ){
		return 1;
	}
#endif
	if( pDev->pStream == &sPipe_Stream ){
		return 1;
	}
	if( is_php_stream(pDev->pStream) ){
		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;
		return pData->iType == PH7_IO_STREAM_STDIN || pData->iType == PH7_IO_STREAM_STDOUT
		 || pData->iType == PH7_IO_STREAM_STDERR;
	}
	return 0;
#else
	SXUNUSED(pDev); /* cc warning */
	return 0;
#endif
}
/*
 * The POSIX descriptor behind an open handle, or -1 when there is none.
 * php applies blocking mode and timeouts AT the descriptor, so a stream that
 * has no fd — a memory buffer, a data:// payload, a userland wrapper, and
 * every file on Windows, where the device carries a HANDLE — is exactly the
 * set php answers "unsupported" for.
 */
PH7_PRIVATE int PH7_StreamPosixFd(io_private *pDev)
{
#if !defined(__WINNT__) && !defined(PH7_DISABLE_DISK_IO)
	if( pDev == 0 || pDev->pHandle == 0 || pDev->pStream == 0 || pDev->bDir ){
		/* A DIRECTORY handle rides the same ops table as a file and stores a
		 * DIR* where a file stores its descriptor, so reading one as the other
		 * hands fcntl()/lseek() a truncated heap pointer — an arbitrary fd
		 * number belonging to something else in this process. */
		return -1;
	}
	if( pDev->pStream == &sUnixFileStream ){
		return SX_PTR_TO_INT(pDev->pHandle);
	}
	if( pDev->pStream == &sPipe_Stream ){
		pipe_private *pPipe = (pipe_private *)pDev->pHandle;
		return pPipe->pFile ? fileno(pPipe->pFile) : -1;
	}
	if( is_php_stream(pDev->pStream) ){
		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;
		if( pData->iType == PH7_IO_STREAM_STDIN || pData->iType == PH7_IO_STREAM_STDOUT
		 || pData->iType == PH7_IO_STREAM_STDERR ){
			return SX_PTR_TO_INT(pData->x.pHandle);
		}
	}
	return -1;
#else
	SXUNUSED(pDev); /* cc warning */
	return -1;
#endif
}
/*
 * Can this handle report a POSITION? php's `seekable` is a fact about what the
 * handle sits on, not about what the device could do — php://stdout is seekable
 * into a file and not down a pipe — and the descriptor is the only thing that
 * knows. Answers 1 (yes), 0 (no) or -1 (nothing here can tell; the caller falls
 * back on the device's own xTell).
 */
PH7_PRIVATE int PH7_StreamHandleCanSeek(io_private *pDev)
{
#ifndef PH7_DISABLE_DISK_IO
#ifdef __WINNT__
	/* The file devices carry a HANDLE rather than a descriptor here, and only
	 * the php:// standard streams hold one this can ask. */
	if( pDev && pDev->pHandle && is_php_stream(pDev->pStream) ){
		ph7_stream_data *pData = (ph7_stream_data *)pDev->pHandle;
		if( pData->iType == PH7_IO_STREAM_STDIN || pData->iType == PH7_IO_STREAM_STDOUT
		 || pData->iType == PH7_IO_STREAM_STDERR ){
			LARGE_INTEGER zero,pos;
			zero.QuadPart = 0;
			return SetFilePointerEx((HANDLE)pData->x.pHandle,zero,&pos,FILE_CURRENT) ? 1 : 0;
		}
	}
	return -1;
#else
	int fd = PH7_StreamPosixFd(pDev);
	if( fd < 0 ){
		return -1;
	}
	return lseek(fd,0,SEEK_CUR) == (off_t)-1 ? 0 : 1;
#endif
#else
	SXUNUSED(pDev); /* cc warning */
	return -1;
#endif /* PH7_DISABLE_DISK_IO */
}
/*
 * Which php:// sub-stream a handle opened. stream_get_meta_data() has to tell
 * MEMORY, TEMP and STDIO apart and only the device's own private state knows;
 * everything else answers 0.
 */
PH7_PRIVATE int PH7_PhpStreamKind(void *pHandle)
{
#ifndef PH7_DISABLE_DISK_IO
	ph7_stream_data *pData = (ph7_stream_data *)pHandle;
	return pData ? pData->iType : 0;
#else
	SXUNUSED(pHandle); /* cc warning */
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
		SetIOPrivateOpenedAs(pIn,"php://stdin",(int)sizeof("php://stdin")-1,"rb",2);
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
		SetIOPrivateOpenedAs(pOut,"php://stdout",(int)sizeof("php://stdout")-1,"wb",2);
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
		SetIOPrivateOpenedAs(pErr,"php://stderr",(int)sizeof("php://stderr")-1,"wb",2);
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
