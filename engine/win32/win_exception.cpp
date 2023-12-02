/*
===========================================================================
Copyright (C) 2017-2019 Gian 'myT' Schellenbaum

This file is part of Challenge Quake 3 (CNQ3).

Challenge Quake 3 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Challenge Quake 3 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Challenge Quake 3. If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/
// exception and exit handlers (crash reports, minidumps), error mode changes, etc

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/crash.h"
#ifndef DEDICATED
#include "../client/client.h"
#endif
#include "win_local.h"
#include "glw_win.h"
#include <DbgHelp.h>
#include <strsafe.h>


#if !id386 && !idx64
#	error "This architecture is not supported."
#endif


typedef void (WINAPI *FptrGeneric)();
typedef BOOL (WINAPI *FptrSymInitialize)(HANDLE, PCSTR, BOOL);
typedef PVOID (WINAPI *FptrSymFunctionTableAccess64)(HANDLE, DWORD64);
typedef DWORD64 (WINAPI *FptrSymGetModuleBase64)(HANDLE, DWORD64);
typedef BOOL (WINAPI *FptrStackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
typedef BOOL (WINAPI *FptrSymGetSymFromAddr64)(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64);
typedef BOOL (WINAPI *FptrMiniDumpWriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, CONST PMINIDUMP_EXCEPTION_INFORMATION, CONST PMINIDUMP_USER_STREAM_INFORMATION, CONST PMINIDUMP_CALLBACK_INFORMATION);


typedef struct {
	HMODULE libraryHandle;
	FptrSymInitialize SymInitialize;
	FptrSymFunctionTableAccess64 SymFunctionTableAccess64;
	FptrSymGetModuleBase64 SymGetModuleBase64;
	FptrStackWalk64 StackWalk64;
	FptrSymGetSymFromAddr64 SymGetSymFromAddr64;
	FptrMiniDumpWriteDump MiniDumpWriteDump;
} debug_help_t;


static void WIN_CloseDebugHelp( debug_help_t* debugHelp )
{
	if (debugHelp->libraryHandle == NULL)
		return;

	FreeLibrary(debugHelp->libraryHandle);
	debugHelp->libraryHandle = NULL;
}

static qboolean WIN_OpenDebugHelp( debug_help_t* debugHelp )
{
	debugHelp->libraryHandle = LoadLibraryA("dbghelp.dll");
	if (debugHelp->libraryHandle == NULL)
		return qfalse;

#define GET_FUNCTION(func) \
	debugHelp->func = (Fptr##func)GetProcAddress(debugHelp->libraryHandle, #func); \
	if (debugHelp->func == NULL) { \
		WIN_CloseDebugHelp(debugHelp); \
		return qfalse; \
	}

	GET_FUNCTION(SymInitialize)
	GET_FUNCTION(SymFunctionTableAccess64)
	GET_FUNCTION(SymGetModuleBase64)
	GET_FUNCTION(StackWalk64)
	GET_FUNCTION(SymGetSymFromAddr64)
	GET_FUNCTION(MiniDumpWriteDump)

#undef GET_FUNCTION

	return qtrue;
}

static void WIN_DumpStackTrace( debug_help_t* debugHelp )
{
	enum {
		BUFFER_SIZE = 1024,
		MAX_LEVELS = 256
	};

	if (!debugHelp->SymInitialize(GetCurrentProcess(), NULL, TRUE))
		return;

	CONTEXT context;
#if id386
	ZeroMemory(&context, sizeof(CONTEXT));
	context.ContextFlags = CONTEXT_CONTROL;
	__asm {
	Label:
		mov[context.Ebp], ebp;
		mov[context.Esp], esp;
		mov eax, [Label];
		mov[context.Eip], eax;
	}
#else // idx64
	RtlCaptureContext(&context);
#endif

	// Init the stack frame for this function
	STACKFRAME64 stackFrame;
	ZeroMemory(&stackFrame, sizeof(stackFrame));
#if id386
	const DWORD machineType = IMAGE_FILE_MACHINE_I386;
	stackFrame.AddrPC.Offset = context.Eip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Ebp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Esp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
#else // idx64
	const DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
	stackFrame.AddrPC.Offset = context.Rip;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Rsp;
	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Rsp;
	stackFrame.AddrStack.Mode = AddrModeFlat;
#endif

	const HANDLE processHandle = GetCurrentProcess();
	const HANDLE threadHandle = GetCurrentThread();

	JSONW_BeginNamedArray("stack_trace");
	
	unsigned char buffer[sizeof(IMAGEHLP_SYMBOL64) + BUFFER_SIZE];
	IMAGEHLP_SYMBOL64* const symbol = (IMAGEHLP_SYMBOL64*)buffer;

	int level = 1;
	while (level++ < (MAX_LEVELS + 1)) {
		BOOL result = debugHelp->StackWalk64(
			machineType, processHandle, threadHandle, &stackFrame, &context,
			NULL, debugHelp->SymFunctionTableAccess64, debugHelp->SymGetModuleBase64, NULL);
		if (!result || stackFrame.AddrPC.Offset == 0)
			break;

		ZeroMemory(symbol, sizeof(IMAGEHLP_SYMBOL64) + BUFFER_SIZE);
		symbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
		symbol->MaxNameLength = BUFFER_SIZE;

		JSONW_BeginObject();
		JSONW_HexValue("program_counter", stackFrame.AddrPC.Offset);
		JSONW_HexValue("stack_pointer", stackFrame.AddrStack.Offset);
		JSONW_HexValue("frame_pointer", stackFrame.AddrFrame.Offset);
		JSONW_HexValue("return_address", stackFrame.AddrReturn.Offset);

		DWORD64 displacement;
		result = debugHelp->SymGetSymFromAddr64(processHandle, stackFrame.AddrPC.Offset, &displacement, symbol);
		if (result)
			JSONW_StringValue("name", symbol->Name);

		const DWORD64 moduleBase = debugHelp->SymGetModuleBase64(processHandle, stackFrame.AddrPC.Offset);
		char moduleName[MAX_PATH];
		if (moduleBase && GetModuleFileNameA((HMODULE)moduleBase, moduleName, sizeof(moduleName)))
			JSONW_StringValue("module_name", moduleName);

		JSONW_EndObject();
	}

	JSONW_EndArray();
}

static BOOL WINAPI WIN_MiniDumpCallback(
	IN PVOID CallbackParam,
	IN CONST PMINIDUMP_CALLBACK_INPUT CallbackInput,
	IN OUT PMINIDUMP_CALLBACK_OUTPUT CallbackOutput )
{
	// Keep everything except...
	if (CallbackInput->CallbackType != ModuleCallback)
		return TRUE;

	// ...modules unreferenced by memory.
	if ((CallbackOutput->ModuleWriteFlags & ModuleReferencedByMemory) == 0) {
		CallbackOutput->ModuleWriteFlags &= ~ModuleWriteModule;
		return TRUE;
	}

	return TRUE;
}

static qboolean WIN_CreateDirectoryIfNeeded( const char* path )
{
	const BOOL success = CreateDirectoryA(path, NULL);
	if (success)
		return qtrue;

	return GetLastError() == ERROR_ALREADY_EXISTS;
}

static char exc_reportFolderPath[MAX_PATH];

static void WIN_CreateDumpFilePath( char* buffer, const char* fileName, SYSTEMTIME* time )
{
	// First try to create "%temp%/cnq3_crash", then ".cnq3_crash".
	// If both fail, just use the current directory.
	char* const tempPath = getenv("TEMP");
	const char* reportPath = va("%s\\cnq3_crash", tempPath);
	if (tempPath == NULL || !WIN_CreateDirectoryIfNeeded(reportPath)) {
		reportPath = ".cnq3_crash";
		if (!WIN_CreateDirectoryIfNeeded(reportPath)) {
			reportPath = ".";
		}
	}

	Q_strncpyz(exc_reportFolderPath, reportPath, sizeof(exc_reportFolderPath));
	StringCchPrintfA(
		buffer, MAX_PATH, "%s\\%s_%04d.%02d.%02d_%02d.%02d.%02d",
		exc_reportFolderPath, fileName, time->wYear, time->wMonth, time->wDay,
		time->wHour, time->wMinute, time->wSecond);
}

static qboolean WIN_GetOSVersion( int* major, int* minor, int* revision )
{
	enum { FILE_INFO_SIZE = 4096 };
	const DWORD fileInfoSize = min(FILE_INFO_SIZE, GetFileVersionInfoSizeA("kernel32.dll", NULL));
	if (fileInfoSize == 0)
		return qfalse;
		
	char fileInfo[FILE_INFO_SIZE];
	if (!GetFileVersionInfoA("kernel32.dll", 0, fileInfoSize, fileInfo))
		return qfalse;

	LPVOID osInfo = NULL;
	UINT osInfoSize = 0;
	if (!VerQueryValueA(&fileInfo[0], "\\", &osInfo, &osInfoSize) ||
		osInfoSize < sizeof(VS_FIXEDFILEINFO))
		return qfalse;

	const VS_FIXEDFILEINFO* const versionInfo = (const VS_FIXEDFILEINFO*)osInfo;
	*major = HIWORD(versionInfo->dwProductVersionMS);
	*minor = LOWORD(versionInfo->dwProductVersionMS);
	*revision = HIWORD(versionInfo->dwProductVersionLS);

	return qtrue;
}

static const char* WIN_GetExceptionCodeString( DWORD exceptionCode )
{
	switch (exceptionCode) {
		case EXCEPTION_ACCESS_VIOLATION: return "The thread tried to read from or write to a virtual address for which it does not have the appropriate access.";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "The thread tried to access an array element that is out of bounds and the underlying hardware supports bounds checking.";
		case EXCEPTION_BREAKPOINT: return "A breakpoint was encountered.";
		case EXCEPTION_DATATYPE_MISALIGNMENT: return "The thread tried to read or write data that is misaligned on hardware that does not provide alignment.";
		case EXCEPTION_FLT_DENORMAL_OPERAND: return "One of the operands in a floating-point operation is denormal.";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "The thread tried to divide a floating-point value by a floating-point divisor of zero.";
		case EXCEPTION_FLT_INEXACT_RESULT: return "The result of a floating-point operation cannot be represented exactly as a decimal fraction.";
		case EXCEPTION_FLT_INVALID_OPERATION: return "This exception represents any floating-point exception not described by other codes.";
		case EXCEPTION_FLT_OVERFLOW: return "The exponent of a floating-point operation is greater than the magnitude allowed by the corresponding type.";
		case EXCEPTION_FLT_STACK_CHECK: return "The stack overflowed or underflowed as the result of a floating-point operation.";
		case EXCEPTION_FLT_UNDERFLOW: return "The exponent of a floating-point operation is less than the magnitude allowed by the corresponding type.";
		case EXCEPTION_ILLEGAL_INSTRUCTION: return "The thread tried to execute an invalid instruction.";
		case EXCEPTION_IN_PAGE_ERROR: return "The thread tried to access a page that was not present and the system was unable to load the page.";
		case EXCEPTION_INT_DIVIDE_BY_ZERO: return "The thread tried to divide an integer value by an integer divisor of zero.";
		case EXCEPTION_INT_OVERFLOW: return "The result of an integer operation caused a carry out of the most significant bit of the result.";
		case EXCEPTION_INVALID_DISPOSITION: return "An exception handler returned an invalid disposition to the exception dispatcher.";
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "The thread tried to continue execution after a noncontinuable exception occurred.";
		case EXCEPTION_PRIV_INSTRUCTION: return "The thread tried to execute an instruction whose operation is not allowed in the current machine mode.";
		case EXCEPTION_SINGLE_STEP: return "A trace trap or other single-instruction mechanism signaled that one instruction has been executed.";
		case EXCEPTION_STACK_OVERFLOW: return "The thread used up its stack.";
		default: return "Unknown exception code";
	}
}

static const char* WIN_GetAccessViolationCodeString( DWORD avCode )
{
	switch (avCode) {
		case 0: return "Read access violation";
		case 1: return "Write access violation";
		case 8: return "User-mode data execution prevention (DEP) violation";
		default: return "Unknown violation";
	}
}

#ifndef DEDICATED
// We save those separately because the handler might change related state before writing the report.
static qbool wasDevModeValid = qfalse;
static qbool wasMinimized = qfalse;
#endif

static qbool WIN_WriteTextData( const char* filePath, debug_help_t* debugHelp, EXCEPTION_RECORD* pExceptionRecord )
{
	FILE* const file = fopen(filePath, "w");
	if (file == NULL)
		return qfalse;

	JSONW_BeginFile(file);
	
	WIN_DumpStackTrace(debugHelp);
	JSONW_HexValue("exception_code", pExceptionRecord->ExceptionCode);
	JSONW_HexValue("exception_flags", pExceptionRecord->ExceptionFlags);
	JSONW_StringValue("exception_description", WIN_GetExceptionCodeString(pExceptionRecord->ExceptionCode));

	if (pExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && 
		pExceptionRecord->NumberParameters >= 2) {
		JSONW_StringValue("exception_details", "%s at address %s",
			WIN_GetAccessViolationCodeString(pExceptionRecord->ExceptionInformation[0]),
			Q_itohex(pExceptionRecord->ExceptionInformation[1], qtrue, qtrue));
	}

	int osVersion[3];
	if (WIN_GetOSVersion(osVersion, osVersion + 1, osVersion + 2)) {
		JSONW_StringValue("windows_version", "%d.%d.%d", osVersion[0], osVersion[1], osVersion[2]);
	}

#ifndef DEDICATED
	JSONW_BooleanValue("device_mode_changed", wasDevModeValid);
	JSONW_BooleanValue("minimized", wasMinimized);
#endif

	Crash_PrintToFile(__argv[0]);

	JSONW_EndFile();

	fclose(file);

	return qtrue;
}

static qbool WIN_WriteMiniDump( const char* filePath, debug_help_t* debugHelp, EXCEPTION_POINTERS* pExceptionPointers )
{
	const HANDLE dumpFile = CreateFileA(
		filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
		0, CREATE_ALWAYS, 0, 0);

	if (dumpFile == INVALID_HANDLE_VALUE)
		return qfalse;

	MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
	ZeroMemory(&exceptionInfo, sizeof(exceptionInfo));
	exceptionInfo.ThreadId = GetCurrentThreadId();
	exceptionInfo.ExceptionPointers = pExceptionPointers;
	exceptionInfo.ClientPointers = TRUE;

	MINIDUMP_CALLBACK_INFORMATION callbackInfo;
	ZeroMemory(&callbackInfo, sizeof(callbackInfo));
	callbackInfo.CallbackRoutine = &WIN_MiniDumpCallback;
	callbackInfo.CallbackParam = NULL;

	debugHelp->MiniDumpWriteDump(
		GetCurrentProcess(), GetCurrentProcessId(), dumpFile,
		(MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
		&exceptionInfo, NULL, &callbackInfo);

	CloseHandle(dumpFile);

	return qtrue;
}

static const char* WIN_GetFileName( const char* path )
{
	const char* name = strrchr(path, '\\');
	if (name != path)
		return name + 1;

	name = strrchr(path, '/');
	if (name != path)
		return name + 1;

	return path;
}

// We consider the report written if at least 1 file was successfully written to.
static qbool exc_reportWritten = qfalse;

static void WIN_WriteExceptionFilesImpl( EXCEPTION_POINTERS* pExceptionPointers )
{
	debug_help_t debugHelp;
	if (!WIN_OpenDebugHelp(&debugHelp))
		return;

	SYSTEMTIME time;
	GetSystemTime(&time);

	char modulePath[MAX_PATH];
	GetModuleFileNameA(GetModuleHandle(NULL), modulePath, sizeof(modulePath));

	char dumpFilePath[MAX_PATH];
	WIN_CreateDumpFilePath(dumpFilePath, WIN_GetFileName(modulePath), &time);

	exc_reportWritten |= WIN_WriteTextData(va("%s.json", dumpFilePath), &debugHelp, pExceptionPointers->ExceptionRecord);
	exc_reportWritten |= WIN_WriteMiniDump(va("%s.dmp", dumpFilePath), &debugHelp, pExceptionPointers);

	WIN_CloseDebugHelp(&debugHelp);
}

static int WINAPI WIN_WriteExceptionFiles( EXCEPTION_POINTERS* pExceptionPointers )
{
	// No exception info?
	if (!pExceptionPointers) {
		__try {
			// Generate an exception to get a proper context.
			RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
		} __except (WIN_WriteExceptionFiles(GetExceptionInformation()), EXCEPTION_CONTINUE_EXECUTION) {}

		return EXCEPTION_EXECUTE_HANDLER;
	}

	// We have exception information now, so let's proceed.
	WIN_WriteExceptionFilesImpl(pExceptionPointers);

	return EXCEPTION_EXECUTE_HANDLER;
}

// Returns qtrue when execution should stop immediately and a crash report should be written.
// Obviously, this piece of code must be *very* careful with what exception codes are considered fatal.
static qbool WIN_IsCrashCode( DWORD exceptionCode )
{
	switch (exceptionCode) {
		// The following should always invoke our handler.
		case EXCEPTION_ACCESS_VIOLATION:
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		case EXCEPTION_DATATYPE_MISALIGNMENT:
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		case EXCEPTION_FLT_INVALID_OPERATION:
		case EXCEPTION_FLT_STACK_CHECK:
		case EXCEPTION_IN_PAGE_ERROR:
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		case EXCEPTION_PRIV_INSTRUCTION:
		case EXCEPTION_STACK_OVERFLOW:
		case STATUS_HEAP_CORRUPTION:
		case STATUS_STACK_BUFFER_OVERRUN:
		// The debugger has first-chance access.
		// Therefore, if we get these, we should stop too.
		case EXCEPTION_BREAKPOINT:
		case EXCEPTION_SINGLE_STEP:
			return qtrue;

		// We don't handle the rest.
		// Please leave the commented lines so we know what's being allowed.
		//case DBG_PRINTEXCEPTION_C:			// used by OutputDebugStringA/W
		//case EXCEPTION_FLT_INEXACT_RESULT:
		//case EXCEPTION_FLT_DENORMAL_OPERAND:
		//case EXCEPTION_FLT_OVERFLOW:
		//case EXCEPTION_FLT_UNDERFLOW:
		//case EXCEPTION_INT_OVERFLOW:
		//case EXCEPTION_INVALID_DISPOSITION:	// should not happen
		//case EXCEPTION_POSSIBLE_DEADLOCK:		// STATUS_POSSIBLE_DEADLOCK is not defined
		//case EXCEPTION_GUARD_PAGE:			// we hit the stack guard page (used for growing the stack)
		//case EXCEPTION_INVALID_HANDLE:		// invalid kernel object (may have been closed)
		default:
			return qfalse;
	}
}

// Debugging a full-screen app with a single screen is a horrendous experience.
// With our custom handler, we can handle crashes by restoring the desktop settings
// and hiding the main window before letting the debugger take over.
// This will only help for "crash" exceptions though, other exceptions and breakpoints will still be fucked up.
// Work-around for breakpoints: use __debugbreak(); and don't launch the app through the debugger.
static qbool WIN_WouldDebuggingBeOkay()
{
	if (g_wv.monitorCount >= 2 || g_wv.hWnd == NULL)
		return qtrue;

	const qbool fullScreen = (GetWindowLongA(g_wv.hWnd, GWL_STYLE) & WS_POPUP) != 0;
	if (!fullScreen)
		return qtrue;

	return qfalse;
}

//
// WIN_HandleCrash' tasks are to:
// - reset system settings that won't get reset
//   as part of the normal process clean-up by the OS
// - write a crash report if it was called because of a crash
//
// It can't do any memory allocation or use any synchronization objects.
// Ideally, we want it to be called before every abrupt application exit
// and right after any legitimate crash.
//
// There are 2 cases where the function won't be called:
//
// 1. Termination through the debugger.
//    Our atexit handler never gets called.
//
//    Work-around: Quit normally.
//
// 2. Breakpoints. The debugger has first-chance access and handles them.
//    Our exception handler doesn't get called.
//
//    Work-around: None for debugging. Quit normally.
//

static qbool exc_exitCalled = qfalse;
static qbool exc_quietMode = qfalse;
static int exc_quietModeDlgRes = IDYES;

static LONG WIN_HandleCrash( EXCEPTION_POINTERS* ep )
{
	__try {
		WIN_EndTimePeriod(); // system timer resolution
	} __except (EXCEPTION_EXECUTE_HANDLER) {}

#ifndef DEDICATED
	__try {
		CL_MapDownload_CrashCleanUp();
	} __except(EXCEPTION_EXECUTE_HANDLER) {}
#endif

	if (IsDebuggerPresent() && WIN_WouldDebuggingBeOkay())
		return EXCEPTION_CONTINUE_SEARCH;

#ifndef DEDICATED
	__try {
		wasDevModeValid = glw_state.cdsDevModeValid;
		if (glw_state.cdsDevModeValid)
			WIN_SetDesktopDisplaySettings();
	} __except (EXCEPTION_EXECUTE_HANDLER) {}

	if (g_wv.hWnd != NULL) {
		__try {
			wasMinimized = (qbool)!!IsIconic(g_wv.hWnd);
		} __except(EXCEPTION_EXECUTE_HANDLER) {}

		__try {
			ShowWindow(g_wv.hWnd, SW_MINIMIZE);
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
#endif

	if (exc_exitCalled || IsDebuggerPresent())
		return EXCEPTION_CONTINUE_SEARCH;

	static const char* mbTitle = "CNQ3 Crash";
	static const char* mbMsg = "CNQ3 crashed!\n\nYes to generate a crash report\nNo to continue after attaching a debugger\nCancel to quit";
	const int result = exc_quietMode ? exc_quietModeDlgRes : MessageBoxA(NULL, mbMsg, mbTitle, MB_YESNOCANCEL | MB_ICONERROR | MB_TOPMOST);
	if (result == IDYES) {
		WIN_WriteExceptionFiles(ep);
		if (!exc_quietMode) {
			if (exc_reportWritten)
				ShellExecute(NULL, "open", exc_reportFolderPath, NULL, NULL, SW_SHOW);
			else
				MessageBoxA(NULL, "CNQ3's crash report generation failed!\nExiting now", mbTitle, MB_OK | MB_ICONERROR);
		}		
	} else if (result == IDNO && IsDebuggerPresent()) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	ExitProcess(666);
}

// Always called.
static LONG CALLBACK WIN_FirstExceptionHandler( EXCEPTION_POINTERS* ep )
{
#if defined(CNQ3_DEV)
	MessageBeep(MB_OK);
	Sleep(1000);
#endif

	if (ep != NULL && ep->ExceptionRecord != NULL) {
		if (WIN_IsCrashCode(ep->ExceptionRecord->ExceptionCode))
			return WIN_HandleCrash(ep);
	}

	return EXCEPTION_CONTINUE_SEARCH;
}

// Only called when no other handler processed the exception.
// This is never called when a debugger is attached.
// If a non-fatal exception happens while debugging,
// we won't get the chance to minimize the window etc. :-(
static LONG CALLBACK WIN_LastExceptionHandler( EXCEPTION_POINTERS* ep )
{
#if defined(CNQ3_DEV)
	MessageBeep(MB_ICONERROR);
	Sleep(1000);
#endif

	return WIN_HandleCrash(ep);
}

static void WIN_ExitHandler( void )
{
	exc_exitCalled = qtrue;
	WIN_HandleCrash(NULL);
}

void WIN_InstallExceptionHandlers()
{
	// Register the vectored exception handler for all threads present and future in this process.
	// The handler is always called in the context of the thread raising the exception.
	// 1 means we're inserting the handler at the front of the queue.
	// The debugger does still get first-chance access though.
	AddVectoredExceptionHandler(1, &WIN_FirstExceptionHandler);

	// Register the top-level exception handler for all threads present and future in this process.
	// The handler is always called in the context of the thread raising the exception.
	// The handler will never get called when a debugger is attached.
	// We're giving others the chance to handle exceptions we don't recognize as fatal.
	// If they don't get handled, we'll shut down.
	SetUnhandledExceptionFilter(&WIN_LastExceptionHandler);

	// Make sure we reset system settings even when someone calls exit.
	atexit(&WIN_ExitHandler);

	// SetErrorMode(0) gets the current flags
	// SEM_FAILCRITICALERRORS -> no abort/retry/fail errors
	// SEM_NOGPFAULTERRORBOX  -> the Windows Error Reporting dialog will not be shown
	SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

	for (int i = 1; i < __argc; ++i) {
		if (!Q_stricmp(__argv[i], "/crashreport:yes")) {
			exc_quietMode = qtrue;
			exc_quietModeDlgRes = IDYES;
			break;
		} else if (!Q_stricmp(__argv[i], "/crashreport:no")) {
			exc_quietMode = qtrue;
			exc_quietModeDlgRes = IDCANCEL;
			break;
		}	
	}
}

#if defined(CNQ3_DEV)

static void WIN_RaiseException_f()
{
	const int argc = Cmd_Argc();
	unsigned int exception;
	if ((argc != 2 && argc != 3) ||
		sscanf(Cmd_Argv(1), "%X", &exception) != 1) {
		Com_Printf("Usage: %s <code_hex> [flags_hex]\n", Cmd_Argv(0));
		return;
	}

	unsigned int flags = 0;
	if (argc == 3)
		sscanf(Cmd_Argv(2), "%X", &flags);

	RaiseException((DWORD)exception, (DWORD)flags, 0, NULL);
}

void WIN_RegisterExceptionCommands()
{
	Cmd_AddCommand("raise", &WIN_RaiseException_f);
	Cmd_SetHelp("raise", "calls the WinAPI function RaiseException");
}

#else

void WIN_RegisterExceptionCommands()
{
}

#endif
