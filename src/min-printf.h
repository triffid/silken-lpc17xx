#ifndef _MIN_PRINTF_H
#define _MIN_PRINTF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <sys/reent.h>

int vfprintf(int fd, const char *format, va_list args);
int fprintf(int fd, const char *format, ...) __attribute__ ((__format__ (__printf__, 2, 3)));
int printf(const char *format, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
int puts(const char*);

// #define puts(s) _puts(0, s)

#ifdef __cplusplus
}
#endif

#endif /* _MIN_PRINTF_H */
