#ifndef ZH_LOG_H
#define ZH_LOG_H

#include <windows.h>

#define ZH_FIX_VERSION "1.0"

/* Opens zh-appname-fix.log next to the DLL (or in %TEMP%) and writes a header. */
void log_open(HMODULE self);
void log_close(void);
/* Writes a+b+c as one line to the log file and to OutputDebugString. b, c may be NULL. */
void log_msg(const char *a, const char *b, const char *c);
/* Formats v as decimal / 8-digit hex into buf (at least 12 bytes). */
char *fmt_uint(char *buf, DWORD v);
char *fmt_hex(char *buf, DWORD v);

#endif
