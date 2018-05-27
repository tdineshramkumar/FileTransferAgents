/*
	T Dinesh Ram Kumar
	2014A3A70302P
	fileserver this sends request file in chunks requested ... 
*/

#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/sendfile.h>

#define _LARGEFILE64_SOURCE
#define LISTEN_BACKLOG 5
#define try(expr,format, ...) if ( (expr) == -1 ) { printf(format, ##__VA_ARGS__); perror(""); exit(EXIT_FAILURE); }
struct _request {
	char type;
	char filename[PATH_MAX];
	long offset;
	long size;
};
typedef struct _request 	request ;
typedef struct _request *	Request ;
#define ERRORMSG "Invalid request."
int main(int argc, char  *argv[]) {
	if ( argc != 3 ) {
		printf("FORMAT: executable ip port\n");
		exit(EXIT_FAILURE);
	}
	int sockfd, connfd, psfd, filefd, addrlen= sizeof(struct sockaddr_in), options=1;
	struct sockaddr_in serveraddr, clientaddr;
	bzero(&serveraddr,sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(argv[2]));
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	try ( sockfd = socket(PF_INET, SOCK_STREAM, 0), "socket failed.");
	try ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &options, sizeof(options)), "setsockopt failed." );
	try ( bind(sockfd, (struct sockaddr *) &serveraddr, addrlen), "bind failed." );
	try ( listen(sockfd, LISTEN_BACKLOG), "listen failed." );
	while (1) {
		try ( connfd = accept(sockfd,(struct sockaddr *) &clientaddr, &addrlen) , "accept failed.");
		try ( psfd = fork(), "fork failed.");
		if ( psfd == 0 ){
			request clientrequest; 
			long offset, size, sentbytes;
			char filename[PATH_MAX];
			struct stat fileinfo;
			close(sockfd);
			printf("New client: %s:%d \n", inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
			try ( read(connfd, (void *)&clientrequest, sizeof(request)), "read failed." );
			strcpy(filename, clientrequest.filename);
			offset = ntohl(clientrequest.offset);
			size = ntohl(clientrequest.size);
			switch( clientrequest.type ){
				case 'I': // request information ..
					if ( stat(filename, &fileinfo) == -1 ) {
						clientrequest.type = 'E'; // Indicate error
						strncpy(clientrequest.filename, strerror(errno), PATH_MAX);
						clientrequest.size = htonl(0);
						clientrequest.offset = htonl(0); 
						write(connfd, (void *)&clientrequest, sizeof(request));
						printf("requested file: %s doesnot exist client: %s:%d \n", filename, inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
					}
					else {
						clientrequest.type = 'A'; // indicate answer
						clientrequest.size = htonl(fileinfo.st_size); // set the size..
						clientrequest.offset = htonl(0); 
						write(connfd, (void *)&clientrequest, sizeof(request));
						printf("sent info about file:%s size:%ld client: %s:%d \n",filename, fileinfo.st_size, inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
					}
				break;
				case 'F': // get file fragment ..
					//if ( (filefd = open(filename, __O_LARGEFILE| O_RDONLY)) == -1 ) {
					if ( (filefd = open(filename, O_RDONLY)) == -1 ) {
						clientrequest.type = 'E'; // Indicate error
						strncpy(clientrequest.filename, strerror(errno), PATH_MAX);
						clientrequest.size = htonl(0);
						clientrequest.offset = htonl(0); 
						write(connfd, (void *)&clientrequest, sizeof(request));
						printf("file:%s open failed (%s) client: %s:%d \n",filename,strerror(errno),inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
					}
					else {
						clientrequest.type = 'A'; // indicate answer
						clientrequest.size = htonl(size); // set the size..
						clientrequest.offset = htonl(offset); 
						write(connfd, (void *)&clientrequest, sizeof(request));
						// Now send the file ...
						try ( sentbytes = sendfile(connfd, filefd, &offset, size), "sendfile failed.");
						printf("sent file:%s size:%ld offset:%ld sentbytes:%ld client: %s:%d \n", 
							filename, size, offset, sentbytes,
							inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
					}
				break;
				default: // Invalid option
					clientrequest.type = 'E'; // Indicate error
					strncpy(clientrequest.filename, ERRORMSG, PATH_MAX);
					clientrequest.size = htonl(0);
					clientrequest.offset = htonl(0); 
					write(connfd, (void *)&clientrequest, sizeof(request));
					printf("invalid option:%c client: %s:%d \n",clientrequest.type, inet_ntoa(clientaddr.sin_addr), ntohs(clientaddr.sin_port));
			}
			exit(EXIT_SUCCESS);
		}
		close(connfd);
	}
	return 0;
}