#include <windows.h>
#include <cstdlib>
#include <cstdio>

#if defined(_M_X64)

#if defined(WOWCLIENT_CRASH_LOG)
void CrashLogLine(const char* text);
void CrashLogStack(CONTEXT* context);
#endif

static DWORD s_mainThreadId;
static HANDLE s_mainThread;
static void* s_freezeTrap;
static volatile LONG s_freezeTrapHit;

static void FreezeLog(const char* text) {
#if defined(WOWCLIENT_CRASH_LOG)
    CrashLogLine(text);
#else
    OutputDebugStringA(text);
#endif
}

static LONG CALLBACK FreezeTrapHandler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionAddress == s_freezeTrap) {
        InterlockedExchange(&s_freezeTrapHit, 1);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static BOOL CALLBACK FindVisibleWindow(HWND hwnd, LPARAM param) {
    if (IsWindowVisible(hwnd)) {
        *reinterpret_cast<HWND*>(param) = hwnd;
        return FALSE;
    }

    return TRUE;
}

static bool PushReturnAddress(CONTEXT* context) {
    __try {
        *reinterpret_cast<volatile DWORD64*>(context->Rsp - 8) = context->Rip;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    context->Rsp -= 8;
    return true;
}

static DWORD WINAPI FreezeWatchdog(void*) {
    SetThreadDescription(GetCurrentThread(), L"FreezeWatchdog");

    DWORD limit = 10;
    char value[16];

    if (GetEnvironmentVariableA("WOWCLIENT_FREEZE_SECONDS", value, sizeof(value))) {
        limit = strtoul(value, nullptr, 10);
    }

    if (!limit) {
        return 0;
    }

    // IsHungAppWindow flags the idle client as hung, so probe the pump directly. Each timed out probe takes a second.
    DWORD hungSeconds = 0;

    while (hungSeconds < limit) {
        HWND hwnd = nullptr;
        EnumThreadWindows(s_mainThreadId, FindVisibleWindow, reinterpret_cast<LPARAM>(&hwnd));

        DWORD_PTR result;

        if (hwnd && !IsDebuggerPresent() && !SendMessageTimeoutA(hwnd, WM_NULL, 0, 0, SMTO_NORMAL, 1000, &result) && GetLastError() == ERROR_TIMEOUT) {
            hungSeconds++;
        } else {
            hungSeconds = 0;
            Sleep(1000);
        }
    }

    char line[256];
    _snprintf_s(line, sizeof(line), _TRUNCATE, "FREEZE main thread has not pumped messages for about %lu seconds, forcing a crash on it\n", hungSeconds);
    FreezeLog(line);

    if (SuspendThread(s_mainThread) == static_cast<DWORD>(-1)) {
        return 0;
    }

    CONTEXT frozen = {};
    frozen.ContextFlags = CONTEXT_FULL;

    if (!GetThreadContext(s_mainThread, &frozen)) {
        ResumeThread(s_mainThread);
        return 0;
    }

    // Fake a call into an unmapped page so the fault keeps the frozen frame as its caller.
    CONTEXT redirect = frozen;
    redirect.ContextFlags = CONTEXT_CONTROL;

    if (PushReturnAddress(&redirect)) {
        redirect.Rip = reinterpret_cast<DWORD64>(s_freezeTrap);
        SetThreadContext(s_mainThread, &redirect);
    }

    ResumeThread(s_mainThread);

    for (int i = 0; i < 50 && !s_freezeTrapHit; i++) {
        Sleep(100);
    }

    if (s_freezeTrapHit) {
        return 0;
    }

    FreezeLog("FREEZE main thread never reached the trap, it is likely blocked in a kernel wait\n");

#if defined(WOWCLIENT_CRASH_LOG)
    CrashLogLine("FREEZE main thread stack at detection:\n");
    CrashLogStack(&frozen);
#endif

    reinterpret_cast<void (*)()>(s_freezeTrap)();
    return 0;
}

static struct FreezeDetectorInit {
    FreezeDetectorInit() {
        s_mainThreadId = GetCurrentThreadId();
        s_mainThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, s_mainThreadId);
        s_freezeTrap = VirtualAlloc(nullptr, 0x1000, MEM_RESERVE, PAGE_NOACCESS);

        if (!s_mainThread || !s_freezeTrap) {
            return;
        }

        AddVectoredExceptionHandler(1, FreezeTrapHandler);

        HANDLE watchdog = CreateThread(nullptr, 0, FreezeWatchdog, nullptr, 0, nullptr);

        if (watchdog) {
            CloseHandle(watchdog);
        }
    }
} s_freezeDetectorInit;

#endif
