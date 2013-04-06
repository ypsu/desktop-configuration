#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <resolv.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

void unexpected_exit(void)
{
	close(open("/dev/shm/R", O_RDWR | O_CREAT, 0777));
}

#define HANDLE_CASE(x) do {                          \
		if (x) {                             \
			puts(#x);                    \
			printf("errno: %m\n");       \
			ERR_print_errors_fp(stderr); \
			unexpected_exit();           \
			raise(SIGABRT);              \
		}                                    \
	} while (false)

enum { BUFSZ = 4096 };

SSL_CTX *ctx;
BIO *bio;

void init_ssl(void)
{
	CRYPTO_malloc_init();
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
	ctx = SSL_CTX_new(SSLv23_client_method());
	HANDLE_CASE(ctx == NULL);
	//HANDLE_CASE(SSL_CTX_load_verify_locations(ctx, "/tmp/a/server.crt", NULL) == 0);
	HANDLE_CASE(SSL_CTX_load_verify_locations(ctx, NULL, "/etc/ssl/certs/") == 0);
}

bool init_connection(void)
{
	SSL *ssl;

	bio = BIO_new_ssl_connect(ctx);
	if (bio == NULL)
		return false;
	BIO_get_ssl(bio, &ssl);
	SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
	//BIO_set_conn_hostname(bio, "localhost:9994");
	res_init();
	BIO_set_conn_hostname(bio, "imap.gmail.com:993");
	if (BIO_do_connect(bio) <= 0)
		return false;
	if (SSL_get_verify_result(ssl) != X509_V_OK)
		return false;
	if (BIO_do_handshake(bio) <= 0)
		return false;
	return true;
}

void destroy_connection(void)
{
	if (bio != NULL) {
		BIO_ssl_shutdown(bio);
		bio = NULL;
	}
}

void noop_sighandler(int sig)
{
	(void) sig;
}

char buf[BUFSZ];

void handle_response(const char *resp)
{
	if (strcmp(resp, "* SEARCH\r\n") == 0) {
		unlink("/dev/shm/B");
	} else if (strncmp(resp, "* SEARCH ", 9) == 0) {
		close(open("/dev/shm/B", O_RDWR | O_CREAT, 0777));
	}
}

// Returns true on success
// Appends the line to buf
bool read_line(void)
{
	char ch;
	int len = strlen(buf);
	while (len < BUFSZ-4) {
		int by = BIO_read(bio, &ch, 1);
		if (by == 0) {
			buf[len] = 0;
			errno = EIO;
			return false;
		}
		if (by == -1) {
			buf[len] = 0;
			return false;
		}
		assert(by == 1);
		buf[len++] = ch;
		if (ch == '\n')
			break;
	}
	buf[len] = 0;
	return true;
}

bool send_command(const char *cmd)
{
	int by = BIO_puts(bio, cmd);
	if (by != (int) strlen(cmd))
		return false;
	buf[0] = 0;

	while (true) {
		if (!read_line()) {
			if (errno == EINTR)
				continue;
			else
				return false;
		}
		//printf("got: %s", buf);
		if (strncmp(buf, "xxx OK", 6) == 0)
			break;
		if (strncmp(buf, "xxx ", 4) == 0)
			return false;
		handle_response(buf);
		buf[0] = 0;
	}
	return true;
}

void check_email(void)
{
	char username[64], password[64];
	FILE *f = fopen("/home/rlblaster/personal/.gmail_pwd", "r");
	if (f == NULL)
		return;
	if (fscanf(f, "%60s %60s", username, password) != 2)
		return;
	if (fclose(f) != 0)
		return;
	char cmdbuf[256];
	sprintf(cmdbuf, "xxx LOGIN %s %s\r\n", username, password);
	send_command(cmdbuf);

	send_command("xxx EXAMINE INBOX\r\n");

	while (true) {
		send_command("xxx SEARCH UNSEEN\r\n");

		int by = BIO_puts(bio, "xxx IDLE\r\n");
		if (by != (int) strlen("xxx IDLE\r\n"))
			return;
		buf[0] = 0;

		// first read_line should return "+ idling"
		// we wait for the second one which returns after a new event or a local signal
		if (read_line())
			read_line();

		send_command("DONE\r\n");
	}
}

int main(void)
{
	HANDLE_CASE(atexit(unexpected_exit) != 0);

	// We use this interface for signal so that SA_RESTART is not set
	struct sigaction act;
	memset(&act, 0, sizeof act);
	act.sa_handler = noop_sighandler;
	HANDLE_CASE(sigaction(SIGUSR1, &act, NULL) == -1);
	HANDLE_CASE(sigaction(SIGALRM, &act, NULL) == -1);
	alarm(600);

	init_ssl();

	while (true) {
		if (init_connection()) {
			unlink("/dev/shm/R");
			check_email();
			destroy_connection();
			continue;
		}
		destroy_connection();
		unlink("/dev/shm/B");
		close(open("/dev/shm/R", O_RDWR | O_CREAT, 0777));
		sleep(30);
	}

	return 0;
}
