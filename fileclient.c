/*
	T Dinesh Ram Kumar
	2014A3A70302P
	fileclient this sends request and obtains file in chunks as requested ... 
*/
#define _LARGEFILE64_SOURCE

#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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
#include <pthread.h>
#include <time.h>

#define LISTEN_BACKLOG 5
#define CURSORUP "\033[A"
#define CLEARLINE "\033[2K"
#define BEGINLINE "\r"
#define MAXPROGRESS 100
#define INITIALPROGRESS(connections) for (int i=0;i <connections; i++) printf("\n");
#define try(expr,format, ...) if ( (expr) == -1 ) { printf(format, ##__VA_ARGS__); perror(""); exit(EXIT_FAILURE); }
#define tryt(expr,format, ...) if ( (expr) != 0 ) { printf(format, ##__VA_ARGS__); exit(EXIT_FAILURE); }
struct _request {
	char type;
	char filename[PATH_MAX];
	long offset;
	long size;
};
// Note: Dont add network formatting..
struct _threadrequest {
	char type;
	char filename[PATH_MAX];
	long offset;
	long size;
	int threadindex;
	struct sockaddr_in serveraddr;
	int addrlen;
	char outfilename[PATH_MAX];
};
typedef struct _request 	request ;
typedef struct _request *	Request ;
typedef struct _threadrequest threadrequest;
typedef struct _threadrequest *Threadrequest;

pthread_mutex_t mtx_write_progress = PTHREAD_MUTEX_INITIALIZER;
int connections; 
int *progress;
void printprogress() {
	pthread_mutex_lock(&mtx_write_progress);
	printf("\033[%dA", connections); // go up many lines (cursor up)
	for ( int i=0; i< connections; i++) {
		printf( CLEARLINE BEGINLINE "Connection: %4d [", i+1); // reset line.. 
		for ( int j=1; j<= MAXPROGRESS; j++) {
			j < progress[i] ? printf("="): j == progress[i] ? printf(">"): printf(" ");
		}
		printf("] %3d/%3d\n", progress[i], MAXPROGRESS);
	}
	pthread_mutex_unlock(&mtx_write_progress);
}
void * thread_get_file_chunk( void * args ) {
	Threadrequest trequest = (Threadrequest) args;
	int filefd, sockfd, numbytes ;
	long totalbytes = 0, totalsize ;
	char buffer[BUFSIZ];
	request clientrequest;
	clientrequest.type = trequest->type;
	strncpy(clientrequest.filename, trequest->filename, PATH_MAX);
	clientrequest.offset = htonl(trequest->offset);
	clientrequest.size = htonl(trequest->size);
	//try( filefd = open(trequest->outfilename,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU), "failed to create a file." );
	try( filefd = open(trequest->outfilename,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU), "failed to create a file." );
	try ( sockfd = socket(PF_INET, SOCK_STREAM, 0), "socket failed.");
	try ( connect(sockfd, (struct sockaddr *)&(trequest->serveraddr), trequest->addrlen), "connect failed.");
	try ( write(sockfd, (void *)&clientrequest, sizeof(request)), "write failed." );
	try ( read(sockfd, (void *)&clientrequest, sizeof(request)), "write failed." );
	// printf("Thread: %d Response: type:%c filename:%s offset:%ld size:%ld \n", trequest->threadindex, clientrequest.type, clientrequest.filename, ntohl(clientrequest.offset), ntohl(clientrequest.size) );
	try ( lseek(filefd, ntohl(clientrequest.offset),SEEK_SET), "lseek failed." );
	totalsize = ntohl(clientrequest.size);
	while ( 1 ) {
		try( numbytes = read(sockfd, buffer, BUFSIZ), "read failed." ) ;
		if ( numbytes == 0 ) break ;
		totalbytes += numbytes ;
		try ( write(filefd, buffer, numbytes), "write failed." ) ;
		progress[trequest->threadindex] = (totalbytes * 100/ totalsize);
		// printf("TOTALSIZE%ld %ld\n",totalsize, totalbytes);
		printprogress();
	}
	try ( close(filefd) , "file close failed.");
	return (void *) trequest;
}
int main(int argc, char  *argv[]) {
	if ( argc != 6 ) {
		printf("FORMAT: executable ip port server-file connections out-file\n");
		exit(EXIT_FAILURE);
	}
	int sockfd, addrlen= sizeof(struct sockaddr_in); 
	connections = atoi(argv[4]); 
	if ( connections == 0 ) printf("invalid connections.\n"), exit(EXIT_FAILURE);
	progress= malloc(sizeof(int)*connections) ;
	for ( int i=0; i < connections; i++) progress[i] = 0;
	pthread_t threadid[connections] ;
	Threadrequest trequests = malloc(sizeof(threadrequest) * connections);
	Threadrequest tresults[connections]; // This points to results returned by threads...
	for ( int i=0 ;i <connections; i++) progress[i] =0;
	long offset, size;
	struct sockaddr_in serveraddr;
	request clientrequest;
	clock_t start, end;
	clientrequest.type = 'I' ;
	strncpy(clientrequest.filename, argv[3], PATH_MAX);
	
	clientrequest.offset = htonl(0);
	clientrequest.size = htonl(0);
	bzero(&serveraddr,sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(atoi(argv[2]));
	serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
	try ( sockfd = socket(PF_INET, SOCK_STREAM, 0), "socket failed.");
	try ( connect(sockfd, (struct sockaddr *)&serveraddr, addrlen), "connect failed.");
	try ( write(sockfd, (void *)&clientrequest, sizeof(request)), "write failed." );
	try ( read(sockfd, (void *)&clientrequest, sizeof(request)), "write failed." );
	// Now print the response
	offset = ntohl(clientrequest.offset);
	size = ntohl(clientrequest.size);
	printf("Response: type:%c filename:%s offset:%ld size:%ld \n", clientrequest.type, clientrequest.filename, offset, size);
	// INITIALPROGRESS(connections);
	// printprogress(connections, progress);
	try ( close(sockfd) ,"close failed.");
	for ( int i =0; i <connections; i++) {
		trequests[i].type = 'F';
		strncpy(trequests[i].filename,clientrequest.filename, PATH_MAX);
		strncpy(trequests[i].outfilename, argv[5], PATH_MAX);
		trequests[i].offset = (size/connections)*i;
		trequests[i].serveraddr = serveraddr;
		trequests[i].addrlen = addrlen;
		trequests[i].threadindex = i;
		//snprintf(trequests[i].outfilename,PATH_MAX,"GENERATED_%s", clientrequest.filename);	
	}
	for ( int i= 0; i <connections; i++){
		if ( i == (connections -1) )
			trequests[i].size = size - trequests[i].offset;
		else 
			trequests[i].size = trequests[i+1].offset - trequests[i].offset;
	}
	for ( int i=0 ; i < connections; i++) {
		printf("Offset: %ld Size: %ld Total: %ld\n", trequests[i].offset, trequests[i].size, trequests[i].offset + trequests[i].size);
	}
	start = clock();
	// Now do all the stuff...
	// Note: Main thread just creates and waits for 'connections' threads..
	for ( int i= 0; i< connections; i++) {
		tryt( pthread_create( &(threadid[i]), NULL, thread_get_file_chunk, (void *) &(trequests[i]) ) ,"pthread create [%d] failed.\n",i +1 );
	}
	for ( int i= 0; i< connections; i++) {
		tryt( pthread_join(threadid[i], (void **)&(tresults[i]) ), "join [%d] failed.\n",i+1 );
	}
	end = clock();
	printf("Total Time Taken. %lf \n", ((double) end - start)/CLOCKS_PER_SEC );
	return 0;
}
