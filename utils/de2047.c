// decode rfc2047 messages (https://tools.ietf.org/html/rfc2047). detect base64
// or quoted printable tokens on stdin and decode them. example:
//
//   $ echo =?utf-8?B?c3rFkWzFkSBiYW7DoW4=?= alma =?UTF-8?Q?di=C3=B3?= | de2047
//   szőlő banán alma dió

#include <assert.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define check assert

enum tokentype {
  ttnormal,
  ttbase64,
  ttquoted,
};

signed char hex2dec[256];
char base64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
signed char base64bits[256];

int main(void) {
  memset(hex2dec, -1, sizeof(hex2dec));
  for (int i = 0; i < 10; i++) hex2dec['0' + i] = i;
  for (int i = 0; i < 6; i++) {
    hex2dec['a' + i] = hex2dec['A' + i] = 10 + i;
  }
  memset(base64bits, -1, sizeof(base64bits));
  base64bits['='] = 0;
  for (int i = 0; base64[i] != 0; i++) {
    base64bits[(unsigned char)base64[i]] = i;
  }

  int ch;
  // rfc2047 wants to ignore spaces between encoded tokens. these two bools
  // represent the state required to implement that.
  bool hadenc = false, hadspace = false;
  while ((ch = getchar()) != EOF) {
    char token[1000];
    int len;
    enum tokentype tokentype = ttnormal;
    if (ch == ' ') {
      if (hadspace) {
        putchar(' ');
        hadenc = false;
      }
      hadspace = true;
      continue;
    }
    if (ch != '=') {
      if (hadspace) {
        putchar(' ');
        hadspace = hadenc = false;
      }
      check(putchar(ch) != EOF);
      continue;
    }
    scanf("%950s", token);
    len = strlen(token);
    check(len < 940);
    iconv_t iconvd;
    // inptr - input buffer pointer.
    char *inptr = NULL;
    char tmp[1000];
    if (len >= 12 && memcmp(token + len - 2, "?=", 2) == 0) {
      strcpy(tmp, token);
      do {
        char *fromcode = strtok(tmp, "?");
        if (fromcode == NULL) break;
        char *type = strtok(NULL, "?");
        if (type == NULL) break;
        inptr = strtok(NULL, "?");
        if (inptr == NULL) break;
        char *rem = strtok(NULL, "?");
        if (strcmp(rem, "=") != 0) break;
        char *end = strtok(NULL, "?");
        if (end != NULL) break;
        if (strcasecmp(type, "b") == 0) {
          tokentype = ttbase64;
        } else if (strcasecmp(type, "q") == 0) {
          tokentype = ttquoted;
        } else {
          break;
        }
        iconvd = iconv_open("utf-8", fromcode);
        check(iconvd != (iconv_t)-1);
      } while (false);
    }
    if (tokentype == ttnormal) {
      if (hadspace) {
        putchar(' ');
        hadspace = hadenc = false;
      }
      putchar('=');
      check(fputs(token, stdout) != EOF);
      continue;
    }
    if (hadspace && !hadenc) putchar(' ');
    hadenc = true;
    hadspace = false;
    // a buffer for the encoded string.
    char enctmp[1000];
    char *encptr = enctmp;
    if (tokentype == ttquoted) {
      while (*inptr != 0) {
        if (*inptr == '_') {
          *encptr++ = ' ';
          inptr++;
          continue;
        }
        if (*inptr != '=') {
          *encptr++ = *inptr++;
          continue;
        }
        inptr++;
        check(*inptr != 0 && *(inptr + 1) != 0);
        int v1 = hex2dec[(unsigned char)*inptr++];
        int v2 = hex2dec[(unsigned char)*inptr++];
        check(v1 >= 0 && v2 >= 0);
        *encptr++ = v1 * 16 + v2;
      }
    } else {
      assert(tokentype == ttbase64);
      while (*inptr != 0) {
        check(inptr[1] != 0);
        check(inptr[2] != 0);
        check(inptr[3] != 0);
        int bits = 0;
        int bytes = 3;
        if (inptr[3] == '=') bytes = 2;
        if (inptr[2] == '=') bytes = 1;
        for (int i = 0; i < 4; i++) {
          bits <<= 6;
          check(base64bits[(unsigned char)inptr[i]] >= 0);
          bits |= base64bits[(unsigned char)inptr[i]];
        }
        *encptr++ = bits >> 16;
        if (bytes >= 2) *encptr++ = (bits >> 8) & 0xff;
        if (bytes >= 3) *encptr++ = bits & 0xff;
        inptr += 4;
      }
    }
    *encptr = 0;
    size_t isz = encptr - enctmp;
    encptr = enctmp;
    char outbuf[3010];
    char *outptr = outbuf;
    size_t osz = sizeof(outbuf) - 1;
    while ((int)iconv(iconvd, &encptr, &isz, &outptr, &osz) > 0)
      ;
    check(isz == 0);
    *outptr = 0;
    fputs(outbuf, stdout);
    check(iconv_close(iconvd) == 0);
  }
  return 0;
}
