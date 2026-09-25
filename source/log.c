/*
 * Small log for reporting problems: zh-appname-fix.log next to the DLL, rewritten on
 * each launch. Only records the program name, system version and what the fix
 * did, never full paths (they can contain the user's name).
 */
#include <windows.h>
#include "log.h"

static HANDLE log_file = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION log_lock;

char *fmt_uint(char *buf, DWORD v)
{
    char tmp[12];
    int n = 0, i = 0;
    do { tmp[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) buf[i++] = tmp[--n];
    buf[i] = 0;
    return buf;
}

char *fmt_hex(char *buf, DWORD v)
{
    static const char digits[] = "0123456789abcdef";
    int i;
    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; i++) buf[2 + i] = digits[(v >> (28 - 4 * i)) & 15];
    buf[10] = 0;
    return buf;
}

static void append(char *line, int size, const char *s)
{
    int n = lstrlenA(line);
    if (s) lstrcpynA(line + n, s, size - n);
}

static void pad2(char *line, int size, WORD v)
{
    char b[3] = { '0' + v / 10 % 10, '0' + v % 10, 0 };
    append(line, size, b);
}

static void write_line(const char *text)
{
    char line[600];
    SYSTEMTIME t;
    DWORD written;
    char ms[12];

    if (log_file == INVALID_HANDLE_VALUE) return;
    GetLocalTime(&t);
    line[0] = 0;
    pad2(line, sizeof(line), t.wHour);
    append(line, sizeof(line), ":");
    pad2(line, sizeof(line), t.wMinute);
    append(line, sizeof(line), ":");
    pad2(line, sizeof(line), t.wSecond);
    append(line, sizeof(line), ".");
    fmt_uint(ms, 1000 + t.wMilliseconds);
    append(line, sizeof(line), ms + 1);
    append(line, sizeof(line), "  ");
    append(line, sizeof(line), text);
    append(line, sizeof(line), "\r\n");
    WriteFile(log_file, line, lstrlenA(line), &written, NULL);
}

void log_msg(const char *a, const char *b, const char *c)
{
    char text[512], dbg[540];

    text[0] = 0;
    append(text, sizeof(text), a);
    append(text, sizeof(text), b);
    append(text, sizeof(text), c);

    lstrcpynA(dbg, "[zh-appname-fix] ", sizeof(dbg));
    append(dbg, sizeof(dbg), text);
    append(dbg, sizeof(dbg), "\n");
    OutputDebugStringA(dbg);

    if (log_file == INVALID_HANDLE_VALUE) return;
    EnterCriticalSection(&log_lock);
    write_line(text);
    LeaveCriticalSection(&log_lock);
}

static const char *base_name(const char *path)
{
    const char *p = path, *name = path;
    for (; *p; p++)
        if (*p == '\\' || *p == '/') name = p + 1;
    return name;
}

static HANDLE open_in_dir(const char *dir_of)
{
    char path[MAX_PATH];
    char *name;

    lstrcpynA(path, dir_of, MAX_PATH);
    name = (char *)base_name(path);
    if (name - path + 20 >= MAX_PATH) return INVALID_HANDLE_VALUE;
    lstrcpynA(name, "zh-appname-fix.log", MAX_PATH - (int)(name - path));
    return CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void log_system(void)
{
    typedef LONG (WINAPI *RtlGetVersion_t)(OSVERSIONINFOW *);
    typedef const char *(CDECL *wine_get_version_t)(void);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    RtlGetVersion_t get_version = ntdll ? (RtlGetVersion_t)GetProcAddress(ntdll, "RtlGetVersion") : NULL;
    wine_get_version_t wine_version = ntdll ? (wine_get_version_t)GetProcAddress(ntdll, "wine_get_version") : NULL;
    OSVERSIONINFOW v = { sizeof(v) };
    char num[12], line[80], exe[MAX_PATH];

    if (get_version && !get_version(&v)) {
        line[0] = 0;
        append(line, sizeof(line), fmt_uint(num, v.dwMajorVersion));
        append(line, sizeof(line), ".");
        append(line, sizeof(line), fmt_uint(num, v.dwMinorVersion));
        append(line, sizeof(line), " build ");
        append(line, sizeof(line), fmt_uint(num, v.dwBuildNumber));
        log_msg("system: Windows ", line,
                v.dwMajorVersion == 10 && v.dwBuildNumber >= 22000 ? " (Windows 11)" : NULL);
    }
    if (wine_version) log_msg("running under Wine/Proton ", wine_version(), NULL);
    else log_msg("running on real Windows (not Wine)", NULL, NULL);

    if (GetModuleFileNameA(NULL, exe, MAX_PATH)) log_msg("program: ", base_name(exe), NULL);
}

void log_open(HMODULE self)
{
    char path[MAX_PATH];
    SYSTEMTIME t;
    char y[12], line[64];

    InitializeCriticalSection(&log_lock);
    if (GetModuleFileNameA(self, path, MAX_PATH)) log_file = open_in_dir(path);
    if (log_file == INVALID_HANDLE_VALUE && GetTempPathA(MAX_PATH - 1, path)) {
        lstrcpynA(path + lstrlenA(path), "x", MAX_PATH - lstrlenA(path));
        log_file = open_in_dir(path);
    }

    GetLocalTime(&t);
    fmt_uint(y, t.wYear);
    line[0] = 0;
    append(line, sizeof(line), y);
    append(line, sizeof(line), "-");
    pad2(line, sizeof(line), t.wMonth);
    append(line, sizeof(line), "-");
    pad2(line, sizeof(line), t.wDay);
    log_msg("zh-appname-fix " ZH_FIX_VERSION " started, date ", line, NULL);
    log_system();
}

void log_close(void)
{
    if (log_file == INVALID_HANDLE_VALUE) return;
    FlushFileBuffers(log_file);
    CloseHandle(log_file);
    log_file = INVALID_HANDLE_VALUE;
}
