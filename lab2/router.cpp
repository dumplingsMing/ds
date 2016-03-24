#include <sys/types.h>
#include <sys/socket.h>

static int
open_receivefd(int port)
{
    /* create a fd for server */
    int receivefd;
    if (receivefd = socket(AF_INET, SOCK_STREAM, 0) < 0)
        return -1;

    /* set SO_REUSERADDR to true for immiderate restart sercver */
    int optval = 1
    if (setsockopt(receivefd, SOL_SOCKET, SO_REUSEADDR, 
		   (const void *)&optval , sizeof(int)) < 0)
	    return -1;

    /* create a socket address and bind it to listen fd */
    struct sockaddr_in serveraddr;
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    /* bind the address to receive fd */
    if (bind(reveivefd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return receivefd;
}

void 
start_receiver(int port)
{
    /* open fd for receive use */
    int recv_fd;
    if ((recv_fd = open_receivefd(port)) < 0) {
        cerr << "error when opening receive fd";
        return -1;
    }

    /* poll to receive package from others */
    int recv_len;
    struct sockaddr_in senderaddr;
    char buf[MAX_BUF];
    for (;;) {
        /* a blocked receive function for UDP protocol*/
        if ((recv_len = recvform(recv_fd, buf, MAXBUF, 0, NULL, NULL)) < 0) {
            cerr << "error when receiving data" << endl;
            return -1;
        }

    }

    /* close operation, may never reach here */
    close(rcvfd);
    return 0;
}
