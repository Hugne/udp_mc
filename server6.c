#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/ip6.h>
#include <errno.h>
#include <stdlib.h>
#include <search.h>
#include <string.h>

#include <time.h>

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

void recvloop(uint32_t s)
{
	#define MSIZE 10000
	#define NSIZE 1000
	#define LSIZE 32
	int i;
        struct sockaddr_storage sender;
	char *buf = (char *)malloc(MSIZE);
	char *message;
	char name[NSIZE];
        char *key;
        char *viewId;
	struct entry {
		char key[64];
		char value[64];
	};
	struct entry dumblist[LSIZE];
	memset(dumblist,0,sizeof(struct entry)*LSIZE);
	int nextfree = 0;
	struct entry *e = NULL;

	printf ("msg pointer is 0x%p\n",message);
	while(1) {
		memset(&sender,0,sizeof(struct sockaddr_storage));
        	socklen_t sendsize = sizeof(sender);
		printf("param recvfrom (%d, %p, %d, %d, %p, %p)\n",
			s, message, MSIZE, 0, &sender, &sendsize);
		int recvSize = recvfrom(s, buf, MSIZE, 0, (struct sockaddr *)&sender, &sendsize);
		if (recvSize <= 0){
			perror("recvfrom\n");
			_exit(-1);
		}
		inet_ntop(AF_INET6, &sender, name, 1000);
		message = buf;
		#define STRLIM 16
		printf("From: %s msg: %.*s\n", name,STRLIM, message);
                for (i=0; i < 2; i++) {
                    if (i == 0) {
                       key = strsep(&message, ":");
                    } else if (i == 1) {
                       viewId = strsep(&message, ":");
                    }
                }
		e = NULL;
		for(i=0;i<nextfree;i++){
			if(strcmp(dumblist[i].key, key) == 0){
				e = &dumblist[i];
				break;
			}
		}
		if (e != NULL) {
			
                    printf("Key: %s Value %s\n", e->key, e->value); 

                    double current_val = strtod(viewId, NULL);
                    double previous_val = strtod(e->value, NULL);

                    if ((current_val - previous_val) > 1 ) {
			printf("ERROR - missed packet from: %s - current %f previous %f \n",
				e->key, current_val, previous_val);
			exit(1);
                    } else {
			printf(" Update Key:%s old Value=%s, new Value=%s\n",e->key, e->value, viewId);
			strcpy(e->value, viewId);
			continue;
			}
		}else {
		printf("add new entry with key %s to list\n", key);
		strncpy(dumblist[nextfree].key,key,63);
		nextfree++;
		}
	}

}

int main(int argc, char *argv[])
{
	struct sockaddr_in6 si_other;
	struct sockaddr_in6 si_local;
	uint32_t s, i, slen=sizeof(si_local);
	uint32_t on = 1;
	uint32_t acclen = 0;

	int c;
	char *mc_addr = NULL;
	char *bind_addr = NULL;
	char *etherdev = NULL;
	char *port = NULL;

	while ((c = getopt (argc, argv, "m:s:e:p:")) != -1)
	{
		switch (c) {
		case 'm':
			mc_addr = optarg;
		break;
		case 's':
			bind_addr = optarg;
		break;
		case 'e':
			etherdev = optarg;
		break;
                case 'p':
			port = optarg;
		}
	}
	if (bind_addr == NULL)
		diep("server address reqd\n");

	printf("Server bind address: %s\n",bind_addr);
	printf("Multicast group: %s\n", mc_addr);


	if (!bind_addr)
		diep("server address reqd\n");
	
	if ((s=socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("Socket");

	memset((char *) &si_other, 0, sizeof(si_other));
	
	si_local.sin6_family = AF_INET6;

	int myport = PORT;
	if (port)
		myport = atoi(port);

	si_local.sin6_port = htons(myport);
	if (inet_pton(AF_INET6, bind_addr, &si_local.sin6_addr) == 0) {
	fprintf(stderr, "inet_aton() failed\n");
		diep("aton");
	}
	if (etherdev)
		si_local.sin6_scope_id = if_nametoindex(etherdev);
	else
		printf("no etherdevice specified, scope will be 0\n");

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

	int set_option_on = 1;
	// it is important to do "reuse address" before bind, not after
	int res = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*) &set_option_on, sizeof(set_option_on));

	if (bind(s, (struct sockaddr *) &si_local, sizeof(si_local)) != 0)
		diep("bind failed");


	recvloop(s);	
	close(s);
	return 0;
}
