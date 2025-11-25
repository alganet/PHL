// SPDX-FileCopyrightText: 2020 OpenCppCoverage
// SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MINIDUMP_H
#define MINIDUMP_H

#ifdef __cplusplus
extern "C" {
#endif

void CreateMiniDumpOnUnHandledException();

#ifdef __cplusplus
}
#endif

// Implementation
#ifdef MINIDUMP_IMPLEMENTATION

#include <Windows.h>
#include <stdio.h>

#pragma warning(push)
#pragma warning(disable: 4091) // 'typedef ': ignored on left of '' when no variable is declared
#include <DbgHelp.h>
#pragma warning(pop)

// Define MiniDumpWithSystemInfo if not available (for older DbgHelp versions)
#ifndef MiniDumpWithSystemInfo
#define MiniDumpWithSystemInfo ((MINIDUMP_TYPE)0x800)
#endif

//-----------------------------------------------------------------------------
void GetTimestampedFilename(wchar_t* buffer, size_t bufferSize)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf(buffer, bufferSize / sizeof(wchar_t), L"PHL-%04d-%02d-%02d-%02d-%02d-%02d.dmp",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

//-----------------------------------------------------------------------------
MINIDUMP_TYPE GetMiniDumpDefaultType()
{
	return (MINIDUMP_TYPE)(MiniDumpWithDataSegs |
		MiniDumpWithPrivateReadWriteMemory |
		MiniDumpWithFullMemoryInfo |
		MiniDumpWithThreadInfo |
		MiniDumpWithSystemInfo);
}

//-----------------------------------------------------------------------------
void CreateMiniDump(
	MINIDUMP_EXCEPTION_INFORMATION* minidumpInfo,
	HANDLE hFile,
	const wchar_t* dmpFilename)
{
	MINIDUMP_TYPE miniDumpType = GetMiniDumpDefaultType();

	fwprintf(stderr, L"\tTrying to create memory dump...\n");
	if (MiniDumpWriteDump(
		GetCurrentProcess(),
		GetCurrentProcessId(),
		hFile,
		miniDumpType,
		minidumpInfo,
		NULL,
		NULL))
	{
		fwprintf(stderr, L"\tMemory dump created successfully: %s\n", dmpFilename);
		fwprintf(stderr, L"\tPlease create a new issue on ");
		fwprintf(stderr, L"https://github.com/alganet/PHL/issues and attach the memory dump ");
		fwprintf(stderr, L"%s\n", dmpFilename);
	}
	else
		fwprintf(stderr, L"\tFailed to create memory dump.\n");
}

//-----------------------------------------------------------------------------
LONG CALLBACK VectoredHandler(PEXCEPTION_POINTERS ExceptionInfo)
{
	if (ExceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
	{
		MINIDUMP_EXCEPTION_INFORMATION minidumpInfo;
		wchar_t dmpFilename[256];

		fwprintf(stderr, L"Unexpected error occurred.\n");

		minidumpInfo.ThreadId = GetCurrentThreadId();
		minidumpInfo.ExceptionPointers = ExceptionInfo;
		minidumpInfo.ClientPointers = FALSE;

		GetTimestampedFilename(dmpFilename, sizeof(dmpFilename));
		HANDLE hFile = CreateFileW(dmpFilename, GENERIC_WRITE, 0, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile != INVALID_HANDLE_VALUE)
		{
			CreateMiniDump(&minidumpInfo, hFile, dmpFilename);
			CloseHandle(hFile);
		}
		// Do not abort, let PHL handle it
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

//-------------------------------------------------------------------------
void CreateMiniDumpOnUnHandledException()
{
	PVOID handler = AddVectoredExceptionHandler(1, VectoredHandler);
	if (handler == NULL) {
		fwprintf(stderr, L"Warning: Failed to register vectored exception handler\n");
	}
}

#endif // MINIDUMP_IMPLEMENTATION

#endif // MINIDUMP_H
