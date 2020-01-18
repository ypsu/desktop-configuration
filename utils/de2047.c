// decode rfc2047 messages (https://tools.ietf.org/html/rfc2047). detect base64
// or quoted printable tokens on stdin and decode them. example:
//
//   $ echo =?utf-8?B?c3rFkWzFkSBiYW7DoW4=?= alma =?UTF-8?Q?di=C3=B3?= | de2047
//   szőlő banán alma dió

#include <assert.h>
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
    char buf[1000];
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
    scanf("%950s", buf);
    len = strlen(buf);
    assert(len < 940);
    if (len >= 12 && memcmp(buf + len - 2, "?=", 2) == 0) {
      if (strncasecmp(buf, "?utf-8?b?", 9) == 0) {
        tokentype = ttbase64;
      } else if (strncasecmp(buf, "?utf-8?q?", 9) == 0) {
        tokentype = ttquoted;
      }
    }
    if (tokentype == ttnormal) {
      if (hadspace) {
        putchar(' ');
        hadspace = hadenc = false;
      }
      putchar('=');
      check(fputs(buf, stdout) != EOF);
      continue;
    }
    if (hadspace && !hadenc) putchar(' ');
    hadenc = true;
    hadspace = false;
    len -= 2;
    int idx = 9;
    if (tokentype == ttquoted) {
      while (idx < len) {
        if (buf[idx] == '_') {
          putchar(' ');
          idx++;
          continue;
        }
        if (buf[idx] != '=') {
          putchar(buf[idx++]);
          continue;
        }
        int v1 = hex2dec[(unsigned char)buf[idx + 1]];
        int v2 = hex2dec[(unsigned char)buf[idx + 2]];
        check(v1 >= 0 && v2 >= 0);
        putchar(v1 * 16 + v2);
        idx += 3;
      }
      continue;
    }
    assert(tokentype == ttbase64);
    while (idx < len) {
      assert(idx + 4 <= len);
      int bits = 0;
      int bytes = 3;
      if (buf[idx + 3] == '=') bytes = 2;
      if (buf[idx + 2] == '=') bytes = 1;
      for (int i = 0; i < 4; i++) {
        bits <<= 6;
        check(base64bits[(unsigned char)buf[idx + i]] >= 0);
        bits |= base64bits[(unsigned char)buf[idx + i]];
      }
      putchar(bits >> 16);
      if (bytes >= 2) putchar((bits >> 8) & 0xff);
      if (bytes >= 3) putchar(bits & 0xff);
      idx += 4;
    }
  }
  return 0;
}
