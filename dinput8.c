/*
 * dinput8.dll proxy: Game.dat loads dinput8.dll from its own folder first, so
 * this file gets loaded into the game. Every dinput8 export is forwarded to the
 * real system dinput8.dll; on load it installs the app-name fix.
 */
#include <windows.h>
#include "appname_fix.h"
#include "log.h"

typedef HRESULT (WINAPI *DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
typedef HRESULT (WINAPI *DllCanUnloadNow_t)(void);
typedef HRESULT (WINAPI *DllGetClassObject_t)(REFCLSID, REFIID, LPVOID *);
typedef HRESULT (WINAPI *DllRegisterServer_t)(void);
typedef const void *(WINAPI *GetdfDIJoystick_t)(void);

static DirectInput8Create_t real_DirectInput8Create;
static DllCanUnloadNow_t real_DllCanUnloadNow;
static DllGetClassObject_t real_DllGetClassObject;
static DllRegisterServer_t real_DllRegisterServer;
static DllRegisterServer_t real_DllUnregisterServer;
static GetdfDIJoystick_t real_GetdfDIJoystick;

HRESULT WINAPI proxy_DirectInput8Create(HINSTANCE inst, DWORD version, REFIID riid, LPVOID *out, LPUNKNOWN outer)
{
    static LONG logged;
    HRESULT hr;
    char num[12];

    if (!real_DirectInput8Create) {
        log_msg("PROBLEM: game asked for mouse/keyboard input, but the real dinput8 is not available", NULL, NULL);
        return E_FAIL;
    }
    hr = real_DirectInput8Create(inst, version, riid, out, outer);
    if (!InterlockedExchange(&logged, 1))
        log_msg("mouse/keyboard input setup: ", fmt_hex(num, hr), SUCCEEDED(hr) ? " - OK" : " - PROBLEM");
    return hr;
}

HRESULT WINAPI proxy_DllCanUnloadNow(void)
{
    return real_DllCanUnloadNow ? real_DllCanUnloadNow() : S_FALSE;
}

HRESULT WINAPI proxy_DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID *out)
{
    if (!real_DllGetClassObject) return CLASS_E_CLASSNOTAVAILABLE;
    return real_DllGetClassObject(clsid, riid, out);
}

HRESULT WINAPI proxy_DllRegisterServer(void)
{
    return real_DllRegisterServer ? real_DllRegisterServer() : E_FAIL;
}

HRESULT WINAPI proxy_DllUnregisterServer(void)
{
    return real_DllUnregisterServer ? real_DllUnregisterServer() : E_FAIL;
}

const void *WINAPI proxy_GetdfDIJoystick(void)
{
    return real_GetdfDIJoystick ? real_GetdfDIJoystick() : NULL;
}

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved)
{
    char path[MAX_PATH];
    HMODULE real;
    UINT n;

    (void)reserved;
    if (reason == DLL_PROCESS_DETACH) {
        appname_fix_summary();
        log_close();
        return TRUE;
    }
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);
    log_open(self);

    n = GetSystemDirectoryA(path, MAX_PATH - 16);
    if (!n || n >= MAX_PATH - 16) return FALSE;
    lstrcpynA(path + n, "\\dinput8.dll", MAX_PATH - n);
    real = LoadLibraryA(path);
    if (!real || real == self) {
        log_msg("PROBLEM: could not load the real dinput8 from ", path, NULL);
        log_close();
        return FALSE;
    }
    real_DirectInput8Create = (DirectInput8Create_t)GetProcAddress(real, "DirectInput8Create");
    real_DllCanUnloadNow = (DllCanUnloadNow_t)GetProcAddress(real, "DllCanUnloadNow");
    real_DllGetClassObject = (DllGetClassObject_t)GetProcAddress(real, "DllGetClassObject");
    real_DllRegisterServer = (DllRegisterServer_t)GetProcAddress(real, "DllRegisterServer");
    real_DllUnregisterServer = (DllRegisterServer_t)GetProcAddress(real, "DllUnregisterServer");
    real_GetdfDIJoystick = (GetdfDIJoystick_t)GetProcAddress(real, "GetdfDIJoystick");
    log_msg("real dinput8 loaded from ", path, " - OK");

    appname_fix_install();
    return TRUE;
}
