/****************************************************************************
 *
 * Proxy lab
 * Min Xu
 * andrewID: minxu
 * 
 * This simple proxy provides connection between clients and servers.
 * It has mutiple threads and can concurrently handle requests from 
 * different clients and forward to different servers, and forward response
 * of servers back to clients. All the threads share the same 1 MB LRU cache. 
 * Contents from server can be stored in cache if its size does exceed 
 * 1 KB. URL of each request is used to mark individual web contents If exceeds
 * 1 MB cache size, the least recently used contents will be replaced. Writing 
 * in cache will only be accessed by one thread, while reading in cache can be 
 * concurrent. 
 * 
 * Robustness and error handling:
 * Made the following changes in csapp.c:
 *   -for all styles error functions: removed exit(0) for application in 
 *    proxyLab this is for specifically handling the errors in thread using 
 *    thread_exit instead of directly calling exit
 *   -Rio_writen: change the return type from void to ssize_t for better
 *    error handling
 *   -rio_read & rio_readn: retry on ECONNRESET and EINTR errors
 *   -rio_writen: retry on EPIPE and EINTR errors
 * All the error handlings are processed in proxy.c, either exit in main() or
 * thread_exit in one of the threads
 * 
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including these long lines in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

/* Global cache pointer */
queue *cacheQueue;

/* function prototypes */
void *thread(void *clientfdp);
inline static void serverToClient(rio_t *toServerrp, char *url, int clientfd, \
																int serverfd);
inline static void packToServer(char *headers, char *path, char *toServerReq);
void parReq(char *url, char *hostname, char *portp, char *path);
inline static void toServerhdr(char *hostname, rio_t *reqrp, char *headers, \
												int serverfd, int clientfd);
int main(int argc, char **argv)
{
	Signal(SIGPIPE, SIG_IGN); //handle SIGPIPE
	int listenfd, *clientfdp;
	char *portp;
	struct sockaddr_in clientaddr;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	pthread_t tid;

	//if arg count is not 2, report error
	if(argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	portp = argv[1];

	if((listenfd = Open_listenfd(portp)) < 0) { //listen to input port
		exit(0);
	}

	cacheQueue = initCache(); //initialize cache here

	//connect to client and handle request in a newly created thread
	while(1) { 
		//use calloc to prevent race condition for client
		clientfdp = (int *)Calloc(1, sizeof(int)); 
		*clientfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		/* if accept error, free this file descriptor and continue */
		if(*clientfdp < 0) { 
			Free(clientfdp);
			continue;
		}
		Pthread_create(&tid, NULL, thread, clientfdp);
	}
}

void *thread(void *clientfdp) {
	char req[MAXLINE];
	char method[MAXLINE], hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
	char headers[MAXLINE], url[MAXLINE], end[MAXLINE];

	strcpy(port, "80"); //default port number

	rio_t reqRead;
	
	//store the input client file discriptor
	int clientfd = *((int *)clientfdp);

	Pthread_detach(pthread_self()); //detach it self
	
	Free(clientfdp); //free the previous allocated pointer
	
	/* the first line of client request will hold info on 
	 * method, url and http version */
	Rio_readinitb(&reqRead, clientfd);
	/* if readlineb error, exit the thread */
	if(Rio_readlineb(&reqRead, req, MAXLINE) < 0) {
		Close(clientfd);
		return NULL;
	}
	
	sscanf(req, "%s %s %s", method, url, end); //end will be ignored
	
	//parse url strings to get hostname, port and path
	parReq(url, hostname, port, path);

	//method is not GET, simply return
	if(strcmp(method, "GET")) {
		Close(clientfd);
		return NULL;
	}
	
	/* if found the path in cache, write the data to client and return */
	object *dataFromCache;
	if((dataFromCache = searchCache(url, cacheQueue)) != NULL) {
		Rio_writen(clientfd, dataFromCache->data, dataFromCache->dsize);
		Close(clientfd);
		return NULL;
	} 

	/* get server fd, write the request package from client to server */
	int serverfd;
	rio_t toServerRead;
	char toServerReq[MAXLINE];
	size_t reqSize;

	/* on error, close clientfd and return this thread */
	if((serverfd = Open_clientfd(hostname, port)) < 0) {
		Close(clientfd);
		return NULL;
	}

	/* prepare for request package to be sent to server, store all the
	 * info into arrray toServerReq */
	toServerhdr(hostname, &reqRead, headers, serverfd, clientfd);
	packToServer(headers, path, toServerReq);
	reqSize = strlen(toServerReq);

	/* write the request package to server */
	Rio_readinitb(&toServerRead, serverfd);
	/* if writen error, exit the thread */
	if(Rio_writen(serverfd, toServerReq, reqSize) != reqSize) {
		Close(serverfd);
		Close(clientfd);
		return NULL;
	}
	
	/* return the server's reponse to client */
	serverToClient(&toServerRead, url, clientfd, serverfd);

	Close(serverfd);
	Close(clientfd);
	
	return NULL;
}


/* go through each line of data sent back from server, write it to client
 * store in dataToCache if size does not exceeds MAX_OBJECT_SIZE */
inline static void serverToClient(rio_t *toServerrp, char *url, int clientfd, \
																int serverfd) {

	char clientLine[MAXLINE]; //data read from each line
	char dataToCache[MAX_OBJECT_SIZE]; //all the data to be cached
	char *tempPtr = dataToCache; //temp pointer tracks the end of cached data
	size_t dataSize = 0; //total data size
	size_t cycleSize; //size of content read from each cycle
	size_t urlSize = strlen(url)+1; //string size of path

	/* read MAXLINE each cycle and write it back to client, store to the
	 * buffer dataToCache if size does not  */
	while((cycleSize = Rio_readnb(toServerrp, clientLine, MAXLINE)) > 0) {
		/* if writen error, exit the thread */
		if(Rio_writen(clientfd, clientLine, cycleSize) != cycleSize) {
			Close(serverfd);
			Close(clientfd);
			Pthread_exit(NULL);
		}
		dataSize = dataSize + cycleSize;
		/* if size is fine, append to dataToCache */
		if(dataSize <= MAX_OBJECT_SIZE) {
			memcpy(tempPtr, clientLine, cycleSize);
			tempPtr = tempPtr + cycleSize;
		} 
	}

	if(cycleSize < 0) { //if readnb error, exit the thread
		Close(serverfd);
		Close(clientfd);
		Pthread_exit(NULL);
	}
	
	/*if does not exceeds MAX_OBJECT_SIZE, push in cache */
	if(dataSize <= MAX_OBJECT_SIZE) {
		pushCache(dataToCache, dataSize, url, urlSize, cacheQueue);
	} 
}


/* packToServer - put together "GET path" and all headers */
inline static void packToServer(char *headers, char *path, char *toServerReq) {
	char pathBuf[MAXLINE];
	sprintf(pathBuf, "GET %s HTTP/1.0\r\n", path);
	sprintf(toServerReq, "%s%s\r\n", pathBuf, headers);
}

/* toServerhdr - go through inputs of client, look for existence of each header
 * if existing, replace the original default header. look for other
 * headers, put them together */
inline static void toServerhdr(char *hostname, rio_t *reqrp, char *headers, \
								int serverfd, int clientfd) 				{
	char hosthdr[MAXLINE]; //host header string
	char hdrLine[MAXLINE]; //string read in one line
	ssize_t rc;
	/* buffers store the rest of headers */
	char userhdr[MAXLINE];
	char accepthdr[MAXLINE];
	char acceptEncodinghdr[MAXLINE];
	char connectionhdr[MAXLINE];
	char proxyConnectionhdr[MAXLINE];
	char morehdrs[MAXLINE];
	
	/* copy default headers to buffers */
	sprintf(hosthdr, "Host: %s\r\n", hostname);
	strcpy(userhdr, user_agent_hdr);
	strcpy(accepthdr, accept_hdr);
	strcpy(acceptEncodinghdr, accept_encoding_hdr);
	strcpy(connectionhdr, connection_hdr);
	strcpy(proxyConnectionhdr, proxy_connection_hdr);
	
	/* read through each line from client, if found corresponding head
	 * replace the original header to the specified one */
	while((rc = Rio_readlineb(reqrp, hdrLine, MAXLINE)) > 0) {

		/* if cannot find header termination, just break */
		if(!strcmp(hdrLine, "\r\n")) break; 
		/* if Host header specified, use the specified one */
		if(strstr(hdrLine, "Host:") != NULL) {
			strcpy(hosthdr, hdrLine);
		}
		/* do nothing to the other specified headers, use default */
		else if(strstr(hdrLine, "User-Agent:") != NULL) {
			continue;
		}
		else if(strstr(hdrLine, "Accept:") != NULL) {
			continue;
		}
		else if(strstr(hdrLine, "Accept-Encoding:") != NULL) {
			continue;
		}
		else if(strstr(hdrLine, "Connection:") != NULL) {
			continue;
		}
		else if(strstr(hdrLine, "Proxy-Connection:") != NULL) {
			continue;
		}
		/* Anything other than those headers, strcat them */
		else {
			strcat(morehdrs, hdrLine);
		}
	}
	
	if(rc < 0) { //if readlineb error, exit this thread
		Close(serverfd);
		Close(clientfd);
		Pthread_exit(NULL);
	}
	
	//connect all headers together
	sprintf(headers, "%s%s%s%s%s%s%s", hosthdr, userhdr, accepthdr,\
	        acceptEncodinghdr, connectionhdr, proxyConnectionhdr, morehdrs);
}


/* parReq - for given url, parse the hostname, port number and path, if no path
 * specified, just add "/" to make sure it is not "\0", if no port number
 * specified, just use default well know port number 80 */
void parReq(char *url, char *hostname, char *portp, char *path) {
	char *curr;
	char ports[MAXLINE];
	
	//skip possible "http://"
	if((curr = strstr(url, "://")) == NULL) {
		curr = url;
	}
	else {
		curr += 3;
	}
	
	//hostname before the first '/' or ':'
	while(*curr != '/' && *curr != ':' && *curr != '\0') {
		*hostname = *curr;
		hostname++;
		curr++;
	}
	*hostname = '\0'; //terminate hostname string

	if(*curr ==  '\0') { //if terminated after hostname, path is "/"
		strcpy(path, "/");
		return;
	}

	if(*curr == ':') { //if find ':', set the port number
		curr++;
		char *temp = ports;
		while(*curr != '/' && *curr !=  '\0') {
			*temp = *curr;
			temp++;
			curr++;
		}
		*temp = '\0'; //terminate port string
		strcpy(portp, ports);
		if(*curr ==  '\0') { //if terminated after port, path is "/"
			strcpy(path, "/");
			return;
		}
	}
	else {	//no ':', use default 80
		strcpy(portp, "80");
	}

	sprintf(path, "%s", curr); //put the rest string as path
}
