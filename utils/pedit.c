#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(cond)                               \
  do {                                            \
    if (!(cond)) {                                \
      check(#cond, __FILE__, __func__, __LINE__); \
    }                                             \
  } while (0)
static void check(const char *expr, const char *file, const char *func,
                  int line) {
  const char *fmt;
  fmt = "check \"%s\" in %s at %s:%d failed, errno = %d (%m)\n";
  printf(fmt, expr, func, file, line, errno);
  abort();
}

int main(void) {
  const char *pwd = getpass("Password: ");
  CHECK(strlen(pwd) < 100);
  int child;
  siginfo_t rv;
  const char cipherfile[] = "/home/rlblaster/.d/cfg/.contacts.txt.gpg";
  const char tmpfile[] = "/dev/shm/data";

  // Decrypt.
  child = fork();
  CHECK(child != -1);
  if (child == 0) {
    execl("/usr/bin/gpg2", "/usr/bin/gpg2", "--no-autostart", "--batch",
          "--decrypt", "--passphrase", pwd, "--output", tmpfile, cipherfile,
          NULL, NULL);
    CHECK(false);
  }
  CHECK(waitid(P_PID, child, &rv, WEXITED) == 0);
  if (rv.si_code != CLD_EXITED || rv.si_status != 0) {
    puts("Error.");
    unlink(tmpfile);
    exit(1);
  }

  // Edit.
  struct stat origstat, newstat;
  CHECK(stat(tmpfile, &origstat) == 0);
  child = fork();
  CHECK(child != -1);
  if (child == 0) {
    execl("/usr/bin/vim", "/usr/bin/vim", "-u", "NONE", "-i", "NONE", "-N",
          "-c", "set backspace=indent,eol,start", tmpfile, NULL, NULL);
    CHECK(false);
  }
  CHECK(waitid(P_PID, child, &rv, WEXITED) == 0);
  if (rv.si_code != CLD_EXITED || rv.si_status != 0) {
    puts("Error.");
    unlink(tmpfile);
    exit(1);
  }
  CHECK(stat(tmpfile, &newstat) == 0);
  if (origstat.st_mtime == newstat.st_mtime) {
    puts("No change detected, bailing out.");
    unlink(tmpfile);
    exit(1);
  }

  // Encrypt.
  unlink(cipherfile);
  child = fork();
  CHECK(child != -1);
  if (child == 0) {
    execl("/usr/bin/gpg2", "/usr/bin/gpg2", "--no-autostart", "--batch",
          "--symmetric", "--cipher-algo=AES256", "--passphrase", pwd,
          "--output", cipherfile, tmpfile, NULL, NULL);
    CHECK(false);
  }
  CHECK(waitid(P_PID, child, &rv, WEXITED) == 0);
  if (rv.si_code == CLD_EXITED && rv.si_status == 2) {
    // Agent failure. WTH do we need this stupid agent? Anyways, do
    // nothing with it. Hopefully things didn't go too wrong. :(
  } else if (rv.si_code != CLD_EXITED || rv.si_status != 0) {
    puts("Error.");
    unlink(tmpfile);
    exit(1);
  }

  // Clean up.
  CHECK(unlink(tmpfile) == 0);
  puts("Done. Don't forget to commit!");
  return 0;
}
