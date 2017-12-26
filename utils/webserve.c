// webserve is a very basic static web server to serve very small files. it has
// the bare minimum set of features to be able concurrently serve a set of
// static files. start the webserver in the directory you want to serve the
// files from and webserve will read everything into the memory and then serves
// those from memory. it doesn't traverse or list directories. use ctrl-c to
// refresh its contents cache without a restart.
//
// it works by avoiding storing any per connection data. the listen sockets have
// the TCP_DEFER_ACCEPT setting on so the request line is immediately known. it
// also assumes that the responses fit into the kernel's socket buffer.
// therefore webserve handles each accept by immediately calling read and write
// on the connection. obviously only use to serve non-critical data.
//
// the landing page, aka root request ("/") goes to the file index. just symlink
// it to whatever file you wanted to serve on the landing page.

#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define nil ((void *)0)

// check is like an assert but always enabled.
#define check(cond) checkfunc(cond, #cond, __FILE__, __LINE__)
void checkfunc(bool ok, const char *s, const char *file, int line);

// state is a succinct string representation of what the application is
// currently doing. it's never logged. it's only helpful to quickly identify why
// something crashed because checkfail prints its contents.
enum { statelen = 80 };
char state[statelen + 1];
__attribute__((format(printf, 1, 2)))
void setstatestr(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(state, statelen, fmt, ap);
  va_end(ap);
}

// log usage: log("started server on port %d", port);
#define log loginfo(__FILE__, __LINE__), logfunc
void loginfo(const char *file, int line) {
  struct timeval tv;
  struct tm *tm;
  char buf[30];
  gettimeofday(&tv, nil);
  tm = gmtime(&tv.tv_sec);
  check(strftime(buf, 30, "%F %H:%M:%S", tm) > 0);
  printf("%s.%03d %s:%d ", buf, (int)tv.tv_usec / 1000, file, line);
}
__attribute__((format(printf, 1, 2)))
void logfunc(const char *fmt, ...) {
  char buf[90];
  va_list ap;
  int len;
  va_start(ap, fmt);
  len = vsnprintf(buf, 90, fmt, ap);
  va_end(ap);
  if (len > 80) {
    buf[77] = '.';
    buf[78] = '.';
    buf[79] = '.';
    buf[80] = 0;
    len = 80;
  }
  for (int i = 0; i < len; i++) {
    if (32 <= buf[i] && buf[i] <= 127) continue;
    buf[i] = '.';
  }
  puts(buf);
}

void checkfunc(bool ok, const char *s, const char *file, int line) {
  if (ok) return;
  loginfo(file, line);
  logfunc("checkfail %s", s);
  log("state: %s", state);
  if (errno != 0) log("errno: %m");
  exit(1);
}

// maxnamelength is the maximum length a filename can have.
enum { maxnamelength = 31 };

// maxfiles is the number of different files the server can handle.
enum { maxfiles = 100 };

// buffersize is used to size the two temporary helper buffers. it's big enough
// to handle all the various usecases.
enum { buffersize = 1024 * 1024 };

// filedata contains the full http response for each file.
struct filedata {
  int length;
  // dataoffset contains the offset at which the raw data begins. handy for the
  // gopher responses which don't have any headers.
  int dataoffset;
  // data is a malloc'd pointer. this structure owns it.
  char *data;
  char name[maxnamelength + 1];
};

int filedatacmp(const void *a, const void *b) {
  const struct filedata *x = a;
  const struct filedata *y = b;
  return strcmp(x->name, y->name);
}

struct {
  // if reloadfiles is true, webserve will reload all files from disk during the
  // next iteration of the main loop.
  bool reloadfiles;
  int filescount;
  struct filedata files[maxfiles];

  // helper buffers.
  char buf1[buffersize], buf2[buffersize];

  // marks the fact that the user pressed ctrl-c.
  bool interrupted;
} s;

void siginthandler(int sig) {
  if (sig == SIGINT) {
    s.interrupted = true;
  }
}

const char httpnotfound[] =
  "HTTP/1.1 404 Not Found\r\n"
  "Content-Type: text/plain; charset=utf-8\r\n"
  "Content-Length: 14\r\n"
  "\r\n"
  "404 not found\n";
const char httpnotimpl[] =
  "HTTP/1.1 501 Not Implemented\r\n"
  "Content-Type: text/plain; charset=utf-8\r\n"
  "Content-Length: 20\r\n"
  "\r\n"
  "501 not implemented\n";

int main(int argc, char **argv) {
  // variables for single purpose.
  int httpfd, gopherfd, epollfd;
  int maxfilesize = 100 * 1000;

  // helper variables with many different uses.
  int i, r, port, opt, fd, len;
  int lo, hi, mid;
  long long totalbytes;
  char *pbuf;
  const char *type;
  struct sockaddr_in addr;
  struct epoll_event ev;
  DIR *dir;
  struct dirent *dirent;
  struct filedata *fdp;
  time_t curtime, lastinterrupt;

  // initialize data.
  setstatestr("initializing");
  httpfd = -1;
  gopherfd = -1;
  s.reloadfiles = true;
  lastinterrupt = 0;
  check(signal(SIGINT, siginthandler) != SIG_ERR);

  // parse cmdline arguments.
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  while ((opt = getopt(argc, argv, "g:hp:l:")) != -1) {
    switch (opt) {
    case 'g':
      port = atoi(optarg);
      check(1 <= port && port <= 65535);
      check((gopherfd = socket(AF_INET, SOCK_STREAM, 0)) != -1);
      i = 1;
      check(setsockopt(gopherfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) == 0);
      i = 10;
      r = setsockopt(gopherfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &i, sizeof(i));
      check(r == 0);
      addr.sin_port = htons(port);
      check(bind(gopherfd, &addr, sizeof(addr)) == 0);
      break;
    case 'p':
      port = atoi(optarg);
      check(1 <= port && port <= 65535);
      check((httpfd = socket(AF_INET, SOCK_STREAM, 0)) != -1);
      i = 1;
      check(setsockopt(httpfd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) == 0);
      i = 10;
      r = setsockopt(httpfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &i, sizeof(i));
      addr.sin_port = htons(port);
      check(bind(httpfd, &addr, sizeof(addr)) == 0);
      break;
    case 'l':
      maxfilesize = atoi(optarg);
      check(1 <= maxfilesize && maxfilesize < buffersize);
      break;
    case 'h':
    default:
      puts("serves the small files from the current directory.");
      puts("usage: webserve [args]");
      puts("");
      puts("flags:");
      puts("  -g port: start gopher server on port");
      puts("  -l size: file size limit. default is 100 kB. make sure the");
      puts("           kernel can buffer this amount of data.");
      puts("  -p port: start http server on port");
      puts("  -h: this help message");
      exit(1);
    }
  }
  if (httpfd == -1 && gopherfd == -1) {
    puts("please specify the port number to listen on.");
    exit(1);
  }

  // set up the server sockets.
  check((epollfd = epoll_create1(0)) != -1);
  ev.events = EPOLLIN;
  if (httpfd != -1) {
    check(listen(httpfd, 100) == 0);
    ev.data.fd = httpfd;
    check(epoll_ctl(epollfd, EPOLL_CTL_ADD, httpfd, &ev) == 0);
    log("http server started");
  }
  if (gopherfd != -1) {
    check(listen(gopherfd, 100) == 0);
    ev.data.fd = gopherfd;
    check(epoll_ctl(epollfd, EPOLL_CTL_ADD, gopherfd, &ev) == 0);
    log("gopher server started");
  }

  // run the event loop.
  while (true) {
    // reload files if necessary.
    if (s.reloadfiles) {
      setstatestr("reloading files");
      s.reloadfiles = false;
      for (i = 0; i < maxfiles; i++) {
        free(s.files[i].data);
      }
      totalbytes = 0;
      s.filescount = 0;
      memset(s.files, 0, sizeof(s.files));
      check((dir = opendir(".")) != nil);
      while ((dirent = readdir(dir)) != nil) {
        if (s.filescount == maxfiles) {
          log("too many files in directory, ignoring the rest");
          break;
        }
        if (dirent->d_type == DT_DIR) {
          continue;
        }
        len = strlen(dirent->d_name);
        if (len > maxnamelength) {
          log("skipped the long named %s", dirent->d_name);
          continue;
        }
        setstatestr("reloading %s", dirent->d_name);
        fd = open(dirent->d_name, O_RDONLY);
        check(fd != -1);
        len = read(fd, s.buf1, maxfilesize + 1);
        check(len != -1);
        if (len <= maxfilesize) {
          check(read(fd, s.buf1, 1) == 0);
        }
        check(close(fd) == 0);
        if (len > maxfilesize) {
          log("skipped oversized %s", dirent->d_name);
          continue;
        }
        pbuf = s.buf2;
        pbuf += sprintf(pbuf, "HTTP/1.1 200 OK\r\n");
        pbuf += sprintf(pbuf, "Content-Type: ");
        if (memcmp(s.buf1, "<!DOCTYPE html", 14) == 0) {
          pbuf += sprintf(pbuf, "text/html");
        } else if (memcmp(s.buf1, "\x89PNG", 4) == 0) {
          pbuf += sprintf(pbuf, "image/png");
        } else if (memcmp(s.buf1, "\xff\xd8\xff", 3) == 0) {
          pbuf += sprintf(pbuf, "image/jpeg");
        } else {
          pbuf += sprintf(pbuf, "text/plain");
        }
        pbuf += sprintf(pbuf, "; charset=utf-8\r\n");
        pbuf += sprintf(pbuf, "Cache-Control: max-age=3600\r\n");
        pbuf += sprintf(pbuf, "Content-Length: %d\r\n", len);
        pbuf += sprintf(pbuf, "\r\n");
        fdp = &s.files[s.filescount++];
        fdp->length = len + pbuf - s.buf2;
        fdp->dataoffset = pbuf - s.buf2;
        check((fdp->data = malloc(fdp->length)) != nil);
        memcpy(fdp->data, s.buf2, fdp->dataoffset);
        memcpy(fdp->data + fdp->dataoffset, s.buf1, len);
        strcpy(fdp->name, dirent->d_name);
        totalbytes += fdp->length;
      }
      check(closedir(dir) == 0);
      log("loaded %d files, cache is %lld bytes", s.filescount, totalbytes);
      qsort(s.files, s.filescount, sizeof(s.files[0]), filedatacmp);
    }

    // process a socket event.
    setstatestr("waiting for events");
    r = epoll_wait(epollfd, &ev, 1, -1);
    if (r == -1 && errno == EINTR) {
      errno = 0;
      check(s.interrupted);
      s.interrupted = false;
      s.reloadfiles = true;
      curtime = time(nil);
      if (curtime - lastinterrupt <= 2) {
        log("quitting");
        exit(0);
      }
      lastinterrupt = curtime;
      log("interrupt received, press again to quickly to quit");
      continue;
    }
    check(r == 1);
    // accept and read the request into s.buf2. pbuf will point at the response
    // eventually and its length will be len.
    setstatestr("accepting a request");
    fd = accept4(ev.data.fd, nil, nil, SOCK_NONBLOCK);
    i = maxfilesize + 1024;
    check(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &i, sizeof(i)) == 0);
    len = read(fd, s.buf1, buffersize - 1);
    if (len <= 0) {
      log("responded 501 because read returned %d (errno: %m)", len);
      goto notimplementederror;
    }
    s.buf1[len] = 0;
    pbuf = s.buf1;
    if (ev.data.fd == httpfd) {
      type = "http";
      if (memcmp(pbuf, "GET /", 5) != 0) {
        log("responded 501 because of unimplemented http request: %s", pbuf);
        goto notimplementederror;
      }
      pbuf += 5;
    } else if (ev.data.fd == gopherfd) {
      type = "gopher";
      if (*pbuf != '/') {
        log("responded 501 because of bad gopher request: %s", pbuf);
        goto notimplementederror;
      }
      pbuf++;
    } else {
      check(false);
    }
    if (isspace(*pbuf)) {
      strcpy(s.buf2, "index");
    } else {
      check(sscanf(pbuf, "%s%n", s.buf2, &len) == 1);
      pbuf += len;
      if (!isspace(*pbuf)) {
        log("responded 501 because of unfinished %s request: %s", type, s.buf1);
        goto notimplementederror;
      }
    }
    setstatestr("responding to %s /%s", type, s.buf2);
    // find the entry via binary search and respond.
    lo = 0, hi = s.filescount - 1;
    while (lo <= hi) {
      mid = (lo + hi) / 2;
      fdp = &s.files[mid];
      r = strcmp(fdp->name, s.buf2);
      if (r == 0) {
        pbuf = fdp->data;
        len = fdp->length;
        if (ev.data.fd == gopherfd) {
          pbuf += fdp->dataoffset;
          len -= fdp->dataoffset;
        }
        log("responded success to %s /%s", type, s.buf2);
        goto respond;
      } else if (r < 0) {
        lo = mid + 1;
      } else {
        hi = mid - 1;
      }
    }
    log("responded 404 to %s /%s", type, s.buf2);
    goto notfounderror;
notfounderror:
    if (ev.data.fd == httpfd) {
      pbuf = (char *)httpnotfound;
      len = sizeof(httpnotfound) - 1;
    } else {
      len = sprintf(s.buf1, "404 not found\n");
      pbuf = s.buf1;
    }
    goto respond;
notimplementederror:
    if (ev.data.fd == httpfd) {
      pbuf = (char *)httpnotimpl;
      len = sizeof(httpnotimpl) - 1;
    } else {
      len = sprintf(s.buf1, "501 not implemented\n");
      pbuf = s.buf1;
    }
    goto respond;
respond:
    r = write(fd, pbuf, len);
    if (r != len) {
      log("the kernel can't buffer %d worth of bytes, only %d", len, r);
      check(false);
    }
    check(r == len);
    check(close(fd) == 0);
  }

  return 0;
}
