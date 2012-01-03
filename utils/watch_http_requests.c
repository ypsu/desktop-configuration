#define _GNU_SOURCE
#include <pcap/pcap.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

char error_str[PCAP_ERRBUF_SIZE];

void handler(u_char *user, const struct pcap_pkthdr *h, const u_char *bytes)
{
	(void) user;
	int sz = h->caplen;
	//printf("captured a %d long packet!\n", sz);
	char b[sz];
	memcpy(b, bytes, sz);
	for (int i = 0; i < sz; ++i) {
		if (b[i] < 32 && b[i] != '\n')
			b[i] = '.';
	}
	b[sz] = 0;
	char *req = strstr(b, "GET /");
	if (req == NULL) return;
	req += 4;
	char *req_end = strchr(req, ' ');
	if (req_end == NULL) return;
	*req_end = 0;

	char *host = strstr(req_end+1, "Host: ");
	if (host == NULL) return;
	host += 6;
	char *host_end = strchr(host, '\n');
	if (host_end == NULL) return;
	while (host_end > host && *host_end != '.') {
		*host_end = 0;
		host_end -= 1;
	}
	if (*host_end == '.') *host_end = 0;

	printf("%s%s\n", host, req);
}

int main(void)
{
	setlinebuf(stdout);

	pcap_t *dev = pcap_open_live("any", 65536, 0, 1000, error_str);
	if (dev == NULL) {
		printf("Error opening device: %s\n", error_str);
		return 1;
	}

	while (true) {
		/*int packets =*/ pcap_dispatch(dev, -1, handler, NULL);
		//printf("%d packets processed\n", packets);
	}

	pcap_close(dev);
	return 0;
}
