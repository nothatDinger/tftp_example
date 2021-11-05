#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>
enum {
        UPDATE_SUCCESS = 0,
        RECV_ERR = 30000,
        CKSUM_ERR,
        DECRYPT_ERR,
        FORMAT_ERR,
        SAVE_FILES_ERR,
        NOT_ENOUGH_MEMORY, //30005
        FILE_NOT_EXIST,
        TOO_LONG_FILENAME,
        ACCESS_VIOLATE,
        TIMEOUT,
        OTHER_ERR,
        UPDATING,
        NETWORK_UNREACHABLE
};

static const char *tftp_error_msg[] = {
	"Undefined error",
	"File not found",
	"Access violation",
	"Disk full or allocation error",
	"Illegal TFTP operation",
	"Unknown transfer ID",
	"File already exists",
	"No such user"
};

const int tftp_cmd_get = 1;
const int tftp_cmd_put = 2;

unsigned long malloc_size = 1024000 * 4;
unsigned long restsize;

void renew_checksum(void);
int Is_updating();

char *mbuf, *head_buf, *wp;
/*
void signal_handle()
{
	signal(SIGINT, catch_signal);
	signal(SIGABRT, catch_signal);
	signal(SIGHUP, catch_signal);
	signal(SIGKILL, catch_signal);
	signal(SIGSTOP, catch_signal);
	signal(SIGTERM, catch_signal);
}
*/
static inline int tftp(const int cmd, const struct hostent *host, const char *serverfile, const int port, int tftp_bufsize)
{
	const int cmd_get = cmd & tftp_cmd_get;
	const int cmd_put = cmd & tftp_cmd_put;
	const int bb_tftp_num_retries = 3;
	struct sockaddr_in sa;
	struct sockaddr_in from;
	struct timeval tv;
	socklen_t fromlen;
	fd_set rfds;
	char *cp;
	unsigned short tmp;
	int socketfd;
	int len;
	int opcode = 0;
	int finished = 0;
	int timeout = bb_tftp_num_retries;
	int block_nr = 1;
	char buf[tftp_bufsize + 4];

	//RESERVE_BB_BUFFER(buf, tftp_bufsize + 4); // Why 4 ?
	wp = mbuf = head_buf = NULL;
	wp = mbuf = head_buf = (char *) malloc(malloc_size);

	if (mbuf == NULL) {
		printf("malloc  failed.\n");
		printf("Malloc memory failed!");

		return -NOT_ENOUGH_MEMORY;
	}
	restsize = malloc_size;

	tftp_bufsize += 4;

	if ((socketfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("socket  failed.\n");
		printf("Create UDP socket failed!");
		return -OTHER_ERR;
	}
	//bind local port
	len = sizeof(sa);
	bzero(&sa, sizeof(sa));
	if (bind(socketfd, (struct sockaddr *) &sa, len) < 0) {
		printf("Bind local port failed!\n");
		printf("Bind local port failed!\n");
		return -OTHER_ERR;
	}
	//get local port and open it in PF
	bzero(&sa, sizeof(sa));
	if (getsockname(socketfd, (struct sockaddr *) &sa, (socklen_t *)&len) == -1) {
		printf("Get local port failed (%s)\n", strerror(errno));
		return -OTHER_ERR;
	}

	//set server address
	bzero(&sa, sizeof(sa));
	sa.sin_family = host->h_addrtype;
	sa.sin_port = htons(port);
	memcpy(&sa.sin_addr, (struct in_addr *) host->h_addr, sizeof(sa.sin_addr));

	/* build opcode */
	if (cmd_get) {
		opcode = 1;	// read request = 1
	}
	if (cmd_put) {
		opcode = 2;	// write request = 2
	}

	while (1) {
		/* build packet of type "opcode" */
		cp = buf;
		*((unsigned short *) cp) = htons(opcode);
		cp += 2;

		/* add filename and mode */
		if ((cmd_get && (opcode == 1)) ||	// read request = 1
		    (cmd_put && (opcode == 2))) {	// write request = 2
			/* what is this trying to do ? */
			while (cp != &buf[tftp_bufsize - 1]) {
				if ((*cp = *serverfile++) == '\0')
					break;
				cp++;
			}
			/* and this ? */
			if ((*cp != '\0') || (&buf[tftp_bufsize - 1] - cp) < 7) {
				printf("too long server-filename.\n");
				printf("Too long server-filename!");
				return -TOO_LONG_FILENAME;
			}

			memcpy(cp + 1, "octet", 6);
			cp += 7;
		}

		/* add ack and data */
		if ((cmd_get && (opcode == 4)) ||	// acknowledgement = 4
		    (cmd_put && (opcode == 3))) {	// data packet == 3
			*((unsigned short *) cp) = htons(block_nr);
			cp += 2;
			block_nr++;

			if (cmd_put && (opcode == 3)) {	// data packet == 3
			} else if (finished) {
				return 0;
			}
		}

		/* send packet */
		do {
			len = cp - buf;
#if 0
			printf("sending %u bytes\n", len);
			for (cp = buf; cp < &buf[len]; cp++)
				DBG("%02x ", *cp);
			DBG("\n");
#endif

	for (int i = 0; i < 17; i++) {
		//cout << data[i];
		printf("%02x ", buf[i]);
	}

			if (sendto(socketfd, buf, len, 0, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
				printf("send  failed.\n");
				return -NETWORK_UNREACHABLE;
				//len = -1;
				//break;
			}

			/* receive packet */
			memset(&from, 0, sizeof(from));
			fromlen = sizeof(from);

			tv.tv_sec = 2;	// BB_TFPT_TIMEOUT = 5
			tv.tv_usec = 0;

			FD_ZERO(&rfds);
			FD_SET(socketfd, &rfds);

			switch (select(FD_SETSIZE, &rfds, NULL, NULL, &tv)) {
			case 1:
				len = recvfrom(socketfd, buf, tftp_bufsize, 0, (struct sockaddr *) &from, &fromlen);

				if (len < 0) {
					printf("recvfrom  failed.\n");
					printf("Receive update package failed!");
					return -RECV_ERR;
				}
				timeout = 0;
				if (sa.sin_port == htons(port)) {
					sa.sin_port = from.sin_port;
				}
				if (sa.sin_port == from.sin_port) {
					break;
				}
				/* fall-through for bad packets! */
				/* discard the packet - treat as timeout */
				timeout = bb_tftp_num_retries;
			case 0:
				printf("Timeout...\n");
				if (timeout == 0) {
					len = -1;
					printf("last timeout.\n");
					printf("Receive timeout!");
					return -TIMEOUT;
				} else {
					timeout--;
				}
				break;
			default:
				printf("select  failed.\n");
				len = -1;
			}
		} while (timeout && (len >= 0));

		if (len < 0) {
			break;
		}

		/* process received packet */

		opcode = ntohs(*((unsigned short *) buf));
		tmp = ntohs(*((unsigned short *) &buf[2]));
		//printf("received %d bytes: %04x %04x\n", len, opcode, tmp);

		if (cmd_get && (opcode == 3)) {	// data packet == 3
			if (tmp == block_nr) {
				if (restsize < (len - 4)) {
					malloc_size *= 2;
					printf("malloc_size = %d\n", malloc_size);
					head_buf = mbuf = (char*)realloc(mbuf, malloc_size);
					if (mbuf == NULL) {
						printf("realloc failed.\n");
						printf("Realloc memory failed!");
						return -NOT_ENOUGH_MEMORY;
					}
					wp = mbuf + (malloc_size >> 1) - restsize;
					restsize = restsize + (malloc_size >> 1);
				}

				memcpy(wp, buf + 4, len - 4);

				if (len < 0) {
					printf("write  failed.\n");
					break;
				}

				if (len - 4 != (tftp_bufsize - 4)) {
					finished++;
				}

				wp = wp + len - 4;
				restsize = restsize - len + 4;

				opcode = 4;	// acknowledgement = 4
				continue;
			}
		}

		if (cmd_put && (opcode == 4)) {	// acknowledgement = 4
			if (tmp == (block_nr - 1)) {
				if (finished) {
					break;
				}
				opcode = 3;	// data packet == 3
				continue;
			}
		}

		if (opcode == 5) {	// error code == 5
			char *msg = NULL;

			if (buf[4] != '\0') {
				msg = &buf[4];
				buf[tftp_bufsize - 1] = '\0';
			} else if (tmp < (sizeof(tftp_error_msg) / sizeof(char *))) {
				msg = (char *) tftp_error_msg[tmp];
			}

			if (msg) {
				printf("server says: %s\n", msg);
				printf("Receive update package message \"%s\"", msg);
				if (tmp == 1)
					return -FILE_NOT_EXIST;
				if (tmp == 2)
					return -ACCESS_VIOLATE;
				return -OTHER_ERR;
			}

			break;
		}
	}

	close(socketfd);


	return finished ? 0 : -OTHER_ERR;
}

int main(int argc, char **argv)
{

	char *serverip="162.14.65.50";
	char *filename="test.txt";
	
	tftp(tftp_cmd_get, gethostbyname(serverip), filename, 69, 512);
}
