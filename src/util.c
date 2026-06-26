#include "miniconnect4.h"

void util_copy(char *dst, int max_len, const char *src)
{
    int i = 0;
    if (!dst || max_len <= 0)
        return;
    if (!src)
        src = "";
    while (src[i] && i < max_len - 1) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

int util_len(const char *s)
{
    int n = 0;
    if (!s)
        return 0;
    while (s[n])
        ++n;
    return n;
}

int util_starts(const char *s, const char *prefix)
{
    int i = 0;
    if (!s || !prefix)
        return 0;
    while (prefix[i]) {
        if (s[i] != prefix[i])
            return 0;
        ++i;
    }
    return 1;
}

void util_append(char *dst, int max_len, const char *src)
{
    int d;
    int s = 0;
    if (!dst || !src || max_len <= 0)
        return;
    d = util_len(dst);
    while (src[s] && d < max_len - 1) {
        dst[d++] = src[s++];
    }
    dst[d] = 0;
}

void util_num(char *dst, int max_len, LONG v)
{
    char tmp[16];
    int i = 0;
    int j = 0;
    LONG n;
    if (!dst || max_len <= 0)
        return;
    if (v == 0) {
        util_copy(dst, max_len, "0");
        return;
    }
    n = v;
    if (n < 0) {
        dst[i++] = '-';
        n = -n;
    }
    while (n > 0 && j < (int)sizeof(tmp)) {
        tmp[j++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (j > 0 && i < max_len - 1)
        dst[i++] = tmp[--j];
    dst[i] = 0;
}
