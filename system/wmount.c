#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void remount(const char *path, const char *mode) {
  char cmd[256];
  assert(strlen(path) < 60 && strlen(mode) == 2);
  sprintf(cmd, "mount -o remount,%s %s", mode, path);
  if (system(cmd) != 0) {
    sprintf(cmd, "fuser -vMm %s 2>&1 | grep F", path);
    puts(cmd);
    system(cmd);
    sprintf(cmd, "fuser -vMm %s 2>&1 | grep F", "/home");
    puts(cmd);
    system(cmd);
    exit(1);
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    puts("usage 1: wmount buf - mount /homebuf as rw, / as ro.");
    puts("usage 2: wmount disk - mount /homebuf as ro, / as rw.");
    puts("usage 3: wmount all - mount /homebuf as rw, / as rw.");
    return 0;
  }
  if (setuid(0) != 0) {
    puts("error: can't setuid to root.");
    return 1;
  }
  if (strcmp(argv[1], "buf") == 0) {
    remount("/", "ro");
    remount("/boot", "ro");
    remount("/homebuf", "rw");
  } else if (strcmp(argv[1], "disk") == 0) {
    remount("/homebuf", "ro");
    remount("/homebufrw", "rw");
    remount("/", "rw");
    remount("/boot", "rw");
  } else if (strcmp(argv[1], "all") == 0) {
    remount("/", "rw");
    remount("/boot", "rw");
    remount("/homebuf", "rw");
  } else {
    puts("error: unknown argument.");
    return 1;
  }
  return 0;
}
