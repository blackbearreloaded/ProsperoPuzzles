// ProsperoPuzzles - Process-level runtime shims for the OpenGL runtime.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Adapted from ps5-opengl native-app/runtime_shims.c: the app log receipt,
// the never-return main policy, and libc entry points the clean-room libc
// shim does not provide but the statically linked Mesa runtime references.

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

extern int sceKernelUsleep(uint32_t microseconds);

#define PPZ_DATA_DIR "/data/prosperopuzzles"
#define PPZ_LOG_PATH PPZ_DATA_DIR "/app.log"

static char ppz_log_buffer[1024u * 1024u];
static char ppz_error_buffer[64u * 1024u];

int ppz_open_log(void)
{
    chmod(PPZ_DATA_DIR, 0755); /* earlier builds created it private */
    /* Keep the previous launch's log for post-close inspection. */
    rename(PPZ_LOG_PATH, PPZ_DATA_DIR "/app.prev.log");
    FILE *stream = freopen(PPZ_LOG_PATH, "w", stdout);
    /* Start a fresh receipt, then keep stdout buffered: ps5-opengl telemetry is
     * chatty and synchronous writes to /data can stall rendering and audio. */
    if (stream != NULL)
        stream = freopen(PPZ_LOG_PATH, "a", stdout);
    if (stream != NULL)
        setvbuf(stream, ppz_log_buffer, _IOFBF, sizeof(ppz_log_buffer));
    stream = freopen(PPZ_LOG_PATH, "a", stderr);
    if (stream != NULL)
        setvbuf(stream, ppz_error_buffer, _IOFBF, sizeof(ppz_error_buffer));
    return stream != NULL;
}

/* Returning from main or calling exit() crashes a native title; stay alive
 * until the shell closes the title. */
__attribute__((noreturn)) void catchReturnFromMain(int status)
{
    printf("[PPZ] main returned status=%d\n", status);
    fflush(NULL);
    for (;;)
        sceKernelUsleep(100000);
}

void ppz_glapi_tls_context_init(void) __asm__("_ZTH23_mesa_glapi_tls_Context");

void ppz_glapi_tls_context_init(void)
{
}

__attribute__((noreturn)) void __assert(const char *function, const char *file, int line,
                                        const char *expression)
{
    fprintf(stderr, "[PPZ] assertion failed: %s (%s:%d, %s)\n", expression, file, line, function);
    abort();
}

int mkstemps(char *template_name, int suffix_length)
{
    (void)template_name;
    (void)suffix_length;
    errno = ENOSYS;
    return -1;
}

void openlog(const char *identifier, int option, int facility)
{
    (void)identifier;
    (void)option;
    (void)facility;
}

FILE *popen(const char *command, const char *mode)
{
    (void)command;
    (void)mode;
    errno = ENOSYS;
    return NULL;
}

int pclose(FILE *stream)
{
    (void)stream;
    errno = ENOSYS;
    return -1;
}
