#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/ip6.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define CMSG_BUFLEN 64
#define BUFLEN 512
#define NPACK 10
#define PORT 9930
#define SRV_IP "::1"

#include "sockopt.h"


void diep(char *s)
{
        perror(s);
        exit(1);
}


/*Create a simple msghdr with one iov, and buffer for ancillary data */
void msghdr_create(struct msghdr* m, struct iovec* iov,
                   void* buffer, int buflen,
                   void* control_buf, int control_len,
                   struct sockaddr_in6* saddr)
{
        memset(m,0,sizeof(struct msghdr));
        memset(iov,0,sizeof(struct iovec));
        iov[0].iov_base = buffer;
        iov[0].iov_len = buflen;
        m->msg_iov = iov;
        m->msg_iovlen = 1;
        m->msg_name = saddr;
        m->msg_namelen = sizeof(struct sockaddr_in6);
        m->msg_control = control_buf;
        m->msg_controllen = control_len;
}


int main(int argc, char *argv[])
{
	struct sockaddr_in6 si_other;
	uint32_t s, i, slen=sizeof(si_other);
	struct msghdr msg;
	struct iovec iov;
	uint8_t buf[BUFLEN];
	uint8_t cmsgbuf[CMSG_BUFLEN];
	uint32_t on = 1;
	uint8_t join_mcgrp;
	uint32_t acclen = 0;

	int c;
	char *etherdev = NULL;
	char *dest = NULL;
	char *mc_addr = NULL;
	char *port = NULL;

	while ((c = getopt (argc, argv, "d:e:m:p:")) != -1)
	{
		switch (c) {
		case 'd':
			dest = optarg;
		break;
		case 'e':
                        etherdev = optarg;
                break;
		case 'm':
			mc_addr = optarg;
		break;
		case 'p':
			port = optarg;
		break;
		}
	}
	if (dest == NULL)
		diep("destination address reqd\n");

	printf("send packets to %s\n", dest);

	if ((s=socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("Socket");

	memset(cmsgbuf,0,CMSG_BUFLEN);
	memset((char *) &si_other, 0, sizeof(si_other));

	if (etherdev)
                si_other.sin6_scope_id = if_nametoindex(etherdev);
        else
                printf("no etherdevice specified, scope will be 0\n");

	
	si_other.sin6_family = AF_INET6;
	int myport = PORT;
	if(port)
	    myport = atoi(port);
	si_other.sin6_port = htons(myport);
	if (inet_pton(AF_INET6, dest, &si_other.sin6_addr) == 0) {
		fprintf(stderr, "inet_aton() failed\n");
		diep("aton");
	}

        if (mc_addr) {
                struct ipv6_mreq mreq;
                if (inet_pton(AF_INET6, mc_addr, &mreq.ipv6mr_multiaddr) == 0) {
                        diep("aton/mcast");
                }

                mreq.ipv6mr_interface = 0; //any if

                printf("join mc group %s\n", mc_addr);

                if (setsockopt(s, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) != 0)
                        diep("IPV6_ADD_MEMBERSHIP");
        }
	#define DATASZ 5000
        char *message = (char *) malloc(10000);
	char *data = (char *) malloc(DATASZ);
	memset(data, 'A', DATASZ);

	char hostname[128];
	gethostname(hostname, sizeof hostname);

        double count = 0;
	while(1) {
		printf("Sending packet: %lf\n", count);
		snprintf(message, DATASZ, "%s:%lf:%s ", hostname, count, data);

		if(sendto(s, message, DATASZ, 0, (struct sockaddr*) &si_other, slen) == -1)
			diep("sendto");

		usleep(100000);

		count++;
	}
	
	close(s);
	return 0;
}


