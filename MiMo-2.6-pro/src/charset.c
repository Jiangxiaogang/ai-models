#define WIN32_LEAN_AND_MEAN

#include <stdlib.h>
#include <windows.h>

#include "charset.h"

static int convert_codepage(UINT from, UINT to, const char *in, char *out, size_t outsz) {
    int wlen = MultiByteToWideChar(from, 0, in, -1, NULL, 0);
    if (wlen <= 0) return -1;
    wchar_t *w = (wchar_t *)malloc((size_t)wlen * sizeof *w);
    if (!w) return -1;
    int ok = MultiByteToWideChar(from, 0, in, -1, w, wlen) > 0;
    int n = 0;
    if (ok) n = WideCharToMultiByte(to, 0, w, -1, out, (int)outsz, NULL, NULL);
    free(w);
    return n > 0 ? 0 : -1;
}

int ansi_to_utf8(const char *in, char *out, size_t outsz) {
    return convert_codepage(CP_ACP, CP_UTF8, in, out, outsz);
}

int utf8_to_ansi(const char *in, char *out, size_t outsz) {
    return convert_codepage(CP_UTF8, CP_ACP, in, out, outsz);
}
