#define _GNU_SOURCE
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <maxminddb.h>

#define IP2C_PORT 16886
#define BUFLEN 2048 //Must be more than enough

int main(int argc, char **argv) {
	struct sockaddr_in si_me, si_other;
	int i;
	socklen_t slen = sizeof(si_other);
	ssize_t recv_len;
	int proxy_connection;
	char buf[BUFLEN + 1];
	struct pollfd fds[1];
	static MMDB_s geoip_database;
	int geoip_database_need_close = 0;
	int gai_error, mmdb_error;
	MMDB_lookup_result_s result;
	MMDB_entry_data_s data;
	int status;
	const char *database_path;
	const char *extresp_query_string = "\377\377\377\377extResponse ip2cq ";
	unsigned int extresp_query_string_len = strlen(extresp_query_string);
	char address[128];
	const char *other_addr;
	char code[3];
	int port = IP2C_PORT;
	if (argc < 2) {
		printf("Usage: %s <mmdb database> [port]\n", argv[0]);
		goto finish;
	}
	database_path = argv[1];
	if (argc == 3) 
		port = atol(argv[2]);

	if ((fds[0].fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		goto finish;

	memset(&si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fds[0].fd, (struct sockaddr*)&si_me, sizeof(si_me)) == -1)
		goto finish;

	if ((status = MMDB_open(database_path, 0, &geoip_database)) != MMDB_SUCCESS) {
		printf("Can't open database %s: %s\n", database_path, MMDB_strerror(status));
		goto finish;
	}
	geoip_database_need_close = 1;
	for(;;)
	{
		fds[0].revents = 0;
		fds[0].events = POLLIN;
		if (poll(fds, 1, -1) < 0)
			goto finish;

		if (fds[0].revents & POLLIN)
		{
			if ((recv_len = recvfrom(fds[0].fd, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) < 0)
				goto finish;

			buf[recv_len] = '\0';
			printf("%s\n", buf);
			if (memcmp(buf, extresp_query_string, extresp_query_string_len) != 0) {
				printf("Unknown request: %s\n", buf);
				continue; //ignore unknown packets
			}
			if (strcmp(address, "local") == 0) {
				other_addr = inet_ntoa(si_other.sin_addr);
				if (other_addr)
					strncpy(address, other_addr, sizeof(address));
			} else
				strncpy(address, &buf[extresp_query_string_len], sizeof(address));

			address[sizeof(address) - 1] = '\0';
			result = MMDB_lookup_string(&geoip_database, address, &gai_error, &mmdb_error);
			if (gai_error || mmdb_error || !result.found_entry) {
				printf("Lookup failed\n");
				continue;
			}
			if (MMDB_get_value(&result.entry, &data, "country", "iso_code", NULL) != MMDB_SUCCESS) {
				printf("Result failed\n");
				continue;
			}
			strncpy(code, data.utf8_string, 2);
			code[2] = '\0';
			snprintf(buf, BUFLEN, "\377\377\377\377extResponse ip2cr %s %s", address, code);
			printf("%s\n", buf);
			//si_other.sin_port = htons(26000);
			sendto(fds[0].fd, buf, strlen(buf), 0, &si_other, slen);
		}
	}
finish:
	if (errno)
		perror("ip2c_server");

	if (fds[0].fd >= 0)
		close(fds[0].fd);

	if (geoip_database_need_close)
		MMDB_close(&geoip_database);
 
	return 0;
}
