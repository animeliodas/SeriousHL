/* SeriousHL - a crash's report. See CrashLog.h. */
#include "StdH.h"
#include "CrashLog.h"

namespace crashlog {

static char _strPath[MAX_PATH] = "";
static char _strVersion[32] = "";
static LPTOP_LEVEL_EXCEPTION_FILTER _pPrevFilter = NULL;
static PVOID _hVectored = NULL;
static DWORD _dwMainThread = 0;
static const char *volatile _strPhase = NULL;
static const char *volatile _strLastPhase = "-";
static volatile LONG _bWritten = 0;

void Phase(const char *strPhase) {
  _strPhase = strPhase;
  if (strPhase != NULL) _strLastPhase = strPhase;
};

// (no CRT, no heap: the process may be in a bad way)
static void Write(HANDLE hFile, const char *str) {
  DWORD dw = 0;
  WriteFile(hFile, str, lstrlenA(str), &dw, NULL);
};

static BOOL ModuleOf(DWORD dwAddress, char *strName, DWORD &dwOffset) {
  HMODULE hm = NULL;
  strName[0] = 0;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)dwAddress, &hm)) {
    return FALSE;
  }
  char strPath[MAX_PATH];
  if (GetModuleFileNameA(hm, strPath, MAX_PATH) == 0) return FALSE;
  const char *pch = strPath;
  for (const char *p = strPath; *p != 0; p++) if (*p == '\\' || *p == '/') pch = p + 1;
  lstrcpynA(strName, pch, 64);
  dwOffset = dwAddress - (DWORD)hm;
  return TRUE;
};

static void Report(EXCEPTION_POINTERS *pep, const char *strHow) {
  if (InterlockedExchange(&_bWritten, 1) != 0) return; // (once)
  HANDLE hFile = CreateFileA(_strPath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) return;
  char str[512], strModule[64];
  DWORD dwOffset = 0;
  SYSTEMTIME st;
  GetLocalTime(&st);
  const EXCEPTION_RECORD *per = pep->ExceptionRecord;
  const CONTEXT *pc = pep->ContextRecord;
  const DWORD dwAt = (DWORD)per->ExceptionAddress;
  wsprintfA(str, "=== SeriousHL %s crash report %04d-%02d-%02d %02d:%02d:%02d (%s)\r\n", _strVersion, st.wYear, st.wMonth, st.wDay,
    st.wHour, st.wMinute, st.wSecond, strHow);
  Write(hFile, str);
  if (ModuleOf(dwAt, strModule, dwOffset)) {
    wsprintfA(str, "exception %08X at %08X = %s+%X\r\n", per->ExceptionCode, dwAt, strModule, dwOffset);
  } else {
    wsprintfA(str, "exception %08X at %08X (in no module)\r\n", per->ExceptionCode, dwAt);
  }
  Write(hFile, str);
  if (per->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && per->NumberParameters >= 2) {
    wsprintfA(str, "  %s %08X\r\n", per->ExceptionInformation[0] == 8 ? "executing" : per->ExceptionInformation[0] == 1 ? "writing" : "reading",
      (DWORD)per->ExceptionInformation[1]);
    Write(hFile, str);
  }
  const DWORD dwThread = GetCurrentThreadId();
  wsprintfA(str, "thread %u (%s); the plugin %s %s\r\n", dwThread, dwThread == _dwMainThread ? "the game's main one" : "another",
    _strPhase != NULL ? "in" : "out of, last in", _strPhase != NULL ? _strPhase : _strLastPhase);
  Write(hFile, str);
  wsprintfA(str, "EIP %08X ESP %08X EBP %08X EAX %08X EBX %08X ECX %08X EDX %08X ESI %08X EDI %08X\r\n", pc->Eip, pc->Esp, pc->Ebp,
    pc->Eax, pc->Ebx, pc->Ecx, pc->Edx, pc->Esi, pc->Edi);
  Write(hFile, str);
  // the stack: what points into a module (return addresses, mostly), up from where it was
  const NT_TIB *ptib = (const NT_TIB *)NtCurrentTeb();
  const DWORD *pdwFrom = (const DWORD *)pc->Esp, *pdwTo = (const DWORD *)ptib->StackBase;
  INDEX ct = 0;
  for (const DWORD *p = pdwFrom; p >= (const DWORD *)ptib->StackLimit && p < pdwTo && p < pdwFrom + 4096 && ct < 48; p++) {
    if (!ModuleOf(*p, strModule, dwOffset)) continue;
    wsprintfA(str, "  [ESP+%04X] %s+%X\r\n", (DWORD)((const char *)p - (const char *)pdwFrom), strModule, dwOffset);
    Write(hFile, str);
    ct++;
  }
  Write(hFile, "\r\n");
  CloseHandle(hFile);
};

static LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS *pep) {
  Report(pep, "unhandled");
  return _pPrevFilter != NULL ? _pPrevFilter(pep) : EXCEPTION_CONTINUE_SEARCH;
};

static LONG WINAPI Vectored(EXCEPTION_POINTERS *pep) {
  const EXCEPTION_RECORD *per = pep->ExceptionRecord;
  if (per->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
    char strModule[64];
    DWORD dwOffset;
    if (!ModuleOf((DWORD)per->ExceptionAddress, strModule, dwOffset)) Report(pep, "code run in no module");
  }
  return EXCEPTION_CONTINUE_SEARCH;
};

void Init(const char *strVersion) {
  lstrcpynA(_strVersion, strVersion, sizeof(_strVersion));
  // next to the game's own log: the game's folder
  CTString strDir = _fnmApplicationPath;
  lstrcpynA(_strPath, (strDir + "SeriousHL_crash.txt").str_String, MAX_PATH);
  _dwMainThread = GetCurrentThreadId();
  _pPrevFilter = SetUnhandledExceptionFilter(&UnhandledFilter);
  _hVectored = AddVectoredExceptionHandler(0, &Vectored);
};

void Shutdown(void) {
  if (_hVectored != NULL) RemoveVectoredExceptionHandler(_hVectored);
  _hVectored = NULL;
  SetUnhandledExceptionFilter(_pPrevFilter);
};

}; // namespace
