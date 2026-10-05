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
#include <string.h>
#include <sys/stat.h>

extern int sceKernelUsleep(uint32_t microseconds);

static char ppz_log_buffer[1024u * 1024u];
static char ppz_error_buffer[64u * 1024u];

/* The app log receipt, in the app's data folder. main opens it once filesystem
 * access has decided where that is (platform/ps5/storage.hpp). */
void ppz_open_log(const char *directory)
{
    char path[160];
    char previous[160];
    if (strlen(directory) > sizeof(path) - 16)
        return;
    mkdir(directory, 0777);
    chmod(directory, 0777); /* earlier builds created it private */
    snprintf(path, sizeof(path), "%s/app.log", directory);
    snprintf(previous, sizeof(previous), "%s/app.prev.log", directory);
    /* Keep the previous launch's log for post-close inspection. */
    rename(path, previous);
    FILE *stream = freopen(path, "w", stdout);
    /* Start a fresh receipt, then keep stdout buffered: ps5-opengl telemetry is
     * chatty and synchronous writes to /data can stall rendering and audio.
     * sys::flush_log() writes it out at the moments that matter. */
    if (stream != NULL)
        stream = freopen(path, "a", stdout);
    if (stream != NULL)
        setvbuf(stream, ppz_log_buffer, _IOFBF, sizeof(ppz_log_buffer));
    stream = freopen(path, "a", stderr);
    if (stream != NULL)
        setvbuf(stream, ppz_error_buffer, _IOFBF, sizeof(ppz_error_buffer));
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

/* openlog, popen and pclose, which Mesa also references, come from
 * update/console_curl.c (libcurl asks for them too). */
