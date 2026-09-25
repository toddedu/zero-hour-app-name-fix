/*
 * Fix for C&C Generals Zero Hour (Steam, app 2732960):
 * "Failed to fetch Steam App Name!" / "Failed to initialize Steam!"
 *
 * Game.dat looks up its own name via
 *   GET store.steampowered.com/api/appdetails?appids=<appid>
 * and expects the JSON reply keyed by that appid: {"<appid>":{...}}.
 * Since Sep 2026 the store answers under a different key ("2748390"), so the
 * game can't find its name and exits.
 *
 * appname_fix_install() redirects the game's own wininet imports (its IAT
 * entries only) to the functions below. For appdetails requests only, they
 * buffer the whole reply and, if the top-level key is not the requested appid,
 * rewrite that key. Correct replies and all other requests pass through as-is.
 */
#include <windows.h>
#include <wininet.h>
#include "appname_fix.h"
#include "log.h"

typedef HINTERNET (WINAPI *HttpOpenRequestA_t)(HINTERNET, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR *, DWORD, DWORD_PTR);
typedef BOOL (WINAPI *InternetReadFile_t)(HINTERNET, LPVOID, DWORD, LPDWORD);
typedef BOOL (WINAPI *InternetQueryDataAvailable_t)(HINTERNET, LPDWORD, DWORD, DWORD_PTR);
typedef BOOL (WINAPI *InternetCloseHandle_t)(HINTERNET);

static HttpOpenRequestA_t real_HttpOpenRequestA;
static InternetReadFile_t real_InternetReadFile;
static InternetQueryDataAvailable_t real_InternetQueryDataAvailable;
static InternetCloseHandle_t real_InternetCloseHandle;

static const char APPDETAILS[] = "/api/appdetails?appids=";

#define MAX_TRACKED 8

struct tracked {
    HINTERNET handle;
    char appid[16];     /* requested appid, as sent in the URL */
    char *body;         /* buffered (possibly fixed) reply */
    DWORD len, pos;
    BOOL loaded;
};

static struct tracked tracked[MAX_TRACKED];
static CRITICAL_SECTION lock;
static HANDLE heap;
static LONG store_lookups;

static void copy_bytes(char *dst, const char *src, DWORD n)
{
    while (n--) *dst++ = *src++;
}

static struct tracked *find_tracked(HINTERNET h)
{
    int i;
    if (!h) return NULL;
    for (i = 0; i < MAX_TRACKED; i++)
        if (tracked[i].handle == h) return &tracked[i];
    return NULL;
}

/* Logs the first bytes of the reply, printable characters only. */
static void log_reply_start(const struct tracked *t)
{
    char start[48], size[12];
    DWORD i, n = t->len < sizeof(start) - 1 ? t->len : sizeof(start) - 1;

    for (i = 0; i < n; i++)
        start[i] = t->body[i] >= 32 && t->body[i] < 127 ? t->body[i] : '.';
    start[n] = 0;
    log_msg("store reply: ", fmt_uint(size, t->len), " bytes");
    log_msg("store reply starts: ", start, NULL);
}

/* If body starts with {"<digits>": and <digits> != appid, replace the key. */
static void fix_key(struct tracked *t)
{
    DWORD i = 2, keylen, applen = lstrlenA(t->appid), newlen;
    char *fixed, change[40];

    log_reply_start(t);
    if (t->len < 4 || t->body[0] != '{' || t->body[1] != '"') {
        log_msg("store reply is not in the expected format, left unchanged", NULL, NULL);
        return;
    }
    while (i < t->len && t->body[i] >= '0' && t->body[i] <= '9') i++;
    keylen = i - 2;
    if (i >= t->len || t->body[i] != '"' || keylen == 0 || keylen >= 16) {
        log_msg("store reply is not in the expected format, left unchanged", NULL, NULL);
        return;
    }
    if (keylen == applen && CompareStringA(LOCALE_INVARIANT, 0, t->body + 2, keylen, t->appid, applen) == CSTR_EQUAL) {
        log_msg("store reply key is already correct (", t->appid, "), nothing to fix - Steam may have fixed the store");
        return;
    }

    newlen = t->len - keylen + applen;
    fixed = HeapAlloc(heap, 0, newlen);
    if (!fixed) {
        log_msg("out of memory, store reply left unchanged", NULL, NULL);
        return;
    }
    copy_bytes(fixed, "{\"", 2);
    copy_bytes(fixed + 2, t->appid, applen);
    copy_bytes(fixed + 2 + applen, t->body + i, t->len - i);
    copy_bytes(change, t->body + 2, keylen);
    lstrcpynA(change + keylen, " -> ", sizeof(change) - keylen);
    lstrcpynA(change + keylen + 4, t->appid, sizeof(change) - keylen - 4);
    HeapFree(heap, 0, t->body);
    t->body = fixed;
    t->len = newlen;
    log_msg("FIXED store reply key ", change, " - OK");
}

/* Read the full reply from the real wininet into t->body. */
static BOOL load_body(struct tracked *t)
{
    DWORD cap = 16384, got;
    char *buf;

    if (t->loaded) return TRUE;
    buf = HeapAlloc(heap, 0, cap);
    if (!buf) return FALSE;
    t->len = 0;
    for (;;) {
        if (t->len == cap) {
            char *bigger = HeapReAlloc(heap, 0, buf, cap * 2);
            if (!bigger) { HeapFree(heap, 0, buf); return FALSE; }
            buf = bigger;
            cap *= 2;
        }
        if (!real_InternetReadFile(t->handle, buf + t->len, cap - t->len, &got)) {
            DWORD err = GetLastError();
            char num[12];
            log_msg("reading store reply failed, error ", fmt_uint(num, err), NULL);
            HeapFree(heap, 0, buf);
            SetLastError(err);
            return FALSE;
        }
        if (!got) break;
        t->len += got;
    }
    t->body = buf;
    t->pos = 0;
    t->loaded = TRUE;
    fix_key(t);
    return TRUE;
}

static HINTERNET WINAPI hook_HttpOpenRequestA(HINTERNET conn, LPCSTR verb, LPCSTR object, LPCSTR version,
                                       LPCSTR referrer, LPCSTR *accept, DWORD flags, DWORD_PTR ctx)
{
    HINTERNET h = real_HttpOpenRequestA(conn, verb, object, version, referrer, accept, flags, ctx);
    DWORD n = sizeof(APPDETAILS) - 1;
    int i;

    if (!h || !object || lstrlenA(object) <= (int)n ||
        CompareStringA(LOCALE_INVARIANT, 0, object, n, APPDETAILS, n) != CSTR_EQUAL)
        return h;

    EnterCriticalSection(&lock);
    for (i = 0; i < MAX_TRACKED; i++) {
        if (!tracked[i].handle) {
            tracked[i].handle = h;
            lstrcpynA(tracked[i].appid, object + n, sizeof(tracked[i].appid));
            tracked[i].body = NULL;
            tracked[i].len = tracked[i].pos = 0;
            tracked[i].loaded = FALSE;
            InterlockedIncrement(&store_lookups);
            log_msg("game asked the store for its name: ", object, NULL);
            break;
        }
    }
    LeaveCriticalSection(&lock);
    return h;
}

static BOOL WINAPI hook_InternetReadFile(HINTERNET h, LPVOID out, DWORD want, LPDWORD read)
{
    struct tracked *t;
    BOOL ok = TRUE;
    DWORD n;

    EnterCriticalSection(&lock);
    t = find_tracked(h);
    if (!t) {
        LeaveCriticalSection(&lock);
        return real_InternetReadFile(h, out, want, read);
    }
    if (load_body(t)) {
        n = t->len - t->pos;
        if (n > want) n = want;
        copy_bytes(out, t->body + t->pos, n);
        t->pos += n;
        *read = n;
    } else {
        ok = FALSE;
    }
    LeaveCriticalSection(&lock);
    return ok;
}

static BOOL WINAPI hook_InternetQueryDataAvailable(HINTERNET h, LPDWORD avail, DWORD flags, DWORD_PTR ctx)
{
    struct tracked *t;
    BOOL ok = TRUE;

    EnterCriticalSection(&lock);
    t = find_tracked(h);
    if (!t) {
        LeaveCriticalSection(&lock);
        return real_InternetQueryDataAvailable(h, avail, flags, ctx);
    }
    if (load_body(t)) {
        if (avail) *avail = t->len - t->pos;
    } else {
        ok = FALSE;
    }
    LeaveCriticalSection(&lock);
    return ok;
}

static BOOL WINAPI hook_InternetCloseHandle(HINTERNET h)
{
    struct tracked *t;

    EnterCriticalSection(&lock);
    t = find_tracked(h);
    if (t) {
        if (t->body) HeapFree(heap, 0, t->body);
        t->handle = NULL;
        t->body = NULL;
    }
    LeaveCriticalSection(&lock);
    return real_InternetCloseHandle(h);
}

typedef int (WINAPI *MessageBoxA_t)(HWND, LPCSTR, LPCSTR, UINT);
typedef int (WINAPI *MessageBoxW_t)(HWND, LPCWSTR, LPCWSTR, UINT);

static MessageBoxA_t real_MessageBoxA;
static MessageBoxW_t real_MessageBoxW;

/* Record any error box the game shows, so reports contain its exact text. */
static int WINAPI hook_MessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type)
{
    log_msg("game showed a message box: ", caption ? caption : "", " |");
    log_msg("    ", text ? text : "", NULL);
    return real_MessageBoxA(owner, text, caption, type);
}

static int WINAPI hook_MessageBoxW(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type)
{
    char c[128] = "", t[384] = "";

    if (caption) WideCharToMultiByte(CP_UTF8, 0, caption, -1, c, sizeof(c) - 1, NULL, NULL);
    if (text) WideCharToMultiByte(CP_UTF8, 0, text, -1, t, sizeof(t) - 1, NULL, NULL);
    log_msg("game showed a message box: ", c, " |");
    log_msg("    ", t, NULL);
    return real_MessageBoxW(owner, text, caption, type);
}

struct hook {
    const char *name;
    void *hook;
    void **real;
};

/* Point the exe's own IAT entries for the given DLL's hooked functions at our hooks. */
static int patch_imports(BYTE *base, const char *dll, const struct hook *hooks, int count)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt;
    IMAGE_DATA_DIRECTORY *dir;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    int patched = 0, i, k;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress) return 0;

    for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir->VirtualAddress); imp->Name; imp++) {
        IMAGE_THUNK_DATA *names, *iat;

        if (lstrcmpiA((const char *)(base + imp->Name), dll)) continue;
        iat = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        names = imp->OriginalFirstThunk ? (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk) : NULL;

        for (i = 0; iat[i].u1.Function; i++) {
            for (k = 0; k < count; k++) {
                BOOL match;
                DWORD old;

                if (names && !IMAGE_SNAP_BY_ORDINAL(names[i].u1.Ordinal)) {
                    IMAGE_IMPORT_BY_NAME *ibn = (IMAGE_IMPORT_BY_NAME *)(base + names[i].u1.AddressOfData);
                    match = !lstrcmpA((const char *)ibn->Name, hooks[k].name);
                } else {
                    match = (void *)iat[i].u1.Function == *hooks[k].real;
                }
                if (!match) continue;
                if (!VirtualProtect(&iat[i].u1.Function, sizeof(iat[i].u1.Function), PAGE_READWRITE, &old))
                    break;
                iat[i].u1.Function = (DWORD_PTR)hooks[k].hook;
                VirtualProtect(&iat[i].u1.Function, sizeof(iat[i].u1.Function), old, &old);
                patched++;
                break;
            }
        }
    }
    return patched;
}

/* Resolve the real functions, then patch the exe's imports. Returns number patched, -1 if unavailable. */
static int install(const char *dll, struct hook *hooks, int count)
{
    HMODULE module = GetModuleHandleA(dll);
    int i;

    if (!module) return -1;
    for (i = 0; i < count; i++) {
        *hooks[i].real = (void *)GetProcAddress(module, hooks[i].name);
        if (!*hooks[i].real) {
            log_msg(dll, " is missing ", hooks[i].name);
            return -1;
        }
    }
    return patch_imports((BYTE *)GetModuleHandleA(NULL), dll, hooks, count);
}

void appname_fix_install(void)
{
    struct hook net_hooks[] = {
        { "HttpOpenRequestA", (void *)hook_HttpOpenRequestA, (void **)&real_HttpOpenRequestA },
        { "InternetReadFile", (void *)hook_InternetReadFile, (void **)&real_InternetReadFile },
        { "InternetQueryDataAvailable", (void *)hook_InternetQueryDataAvailable, (void **)&real_InternetQueryDataAvailable },
        { "InternetCloseHandle", (void *)hook_InternetCloseHandle, (void **)&real_InternetCloseHandle },
    };
    struct hook box_hooks[] = {
        { "MessageBoxA", (void *)hook_MessageBoxA, (void **)&real_MessageBoxA },
        { "MessageBoxW", (void *)hook_MessageBoxW, (void **)&real_MessageBoxW },
    };
    char num[12];
    int patched;

    heap = GetProcessHeap();
    InitializeCriticalSection(&lock);

    patched = install("wininet.dll", net_hooks, 4);
    if (patched < 0)
        log_msg("PROBLEM: this program does not use wininet, nothing to fix (is this Game.dat?)", NULL, NULL);
    else
        log_msg("patched ", fmt_uint(num, patched), patched == 4 ? " of 4 web functions - OK" : " of 4 web functions - PROBLEM, expected 4");

    patched = install("user32.dll", box_hooks, 2);
    if (patched > 0) log_msg("watching for game error messages", NULL, NULL);
}

void appname_fix_summary(void)
{
    log_msg(store_lookups ? "game closed (store name lookup was seen)"
                          : "game closed (store name lookup was never seen)", NULL, NULL);
}
