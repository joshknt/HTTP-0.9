/*-----------------------------------------------------------------------------
Author:		Josh Kent

Course-Section:	CS360

Assignment:	Program #4

Collaborators:	James Jerkins | exitKey()

Resources:	https://linux.die.net/man/2/send
		https://linux.die.net/man/3/recv
		https://linux.die.net/man/2/select
		https://linux.die.net/man/2/fcntl
		https://linux.die.net/man/3/fread
		https://linux.die.net/man/7/signal
		https://linux.die.net/man/3/strtok
		https://linux.die.net/man/3/strcpy
		https://linux.die.net/man/3/strcmp
		https://linux.die.net/man/1/gdb

Description: 	A webserver that implements HTTTP 0.9 protocol. It supports
		html, css, js, and png files. This implementation sends a
		header to be compatible with HTTP 1.x., and it does not
		support folder indexing. The web server ignores ^C to close.
		To close the server, the user must press "ENTER".

Testing:	This implementation has only been tested in chrome.

Arguments:	Server requires a single argument for the file path in which
		pages will be served. The error pages are in the local
		file path with the executable. (ex. /home/usr/web/)
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>	/*For malloc(), exit(), and free()*/
#include <unistd.h>	/*For close*/
#include <string.h>	/*For srtlcpy(), strlcat(), bzero(), strtok(),
				strlen(), and strlen()*/
#include <signal.h>	/*Needed to ignore ^C*/
#include <fcntl.h>	/*Needed to make input non-blocking*/
#include <sys/time.h>	/*Needed for select*/
#include <ctype.h>	/*For tolower()*/
#include <sys/stat.h>	/*For fstat()*/


#define SERVER_PORT 5416
#define MAX_LINE 65536
#define MAX_PENDING 5
#define MAX_HEADER_LINE 80
#define MAX_FILE_SIZE 16777216 /*2MB max file size*/
#define FILE_INDEX "index.html"
#define FILE_404 "404error.html"
#define FILE_414 "414error.html"
#define FILE_501 "501error.html"


/*Define HTTP header*/
struct httpHeader
{
	char status[MAX_HEADER_LINE];
	char connection[MAX_HEADER_LINE];
	char contentType[MAX_HEADER_LINE];
	char contentLength[MAX_HEADER_LINE];
	char server[MAX_HEADER_LINE];
};

/*********************************************************************
Prototypes*/
int exitKey();
void sigintIgnore();
int getFileType(char *b);
int fhandler(int new_s, FILE *fd, char *buff, struct httpHeader *header,
		char *ftype, int sc);
/*********************************************************************/


int main(int argc, char *argv[])
{
	if(argc != 2)
	{
		printf("Path to serve files must be passed as an argument\n");
		return 0;
	}

	/*Ignore ^C termination*/
	signal(SIGINT, sigintIgnore);

	/*Declare variables*/
	struct sockaddr_in sin;
	socklen_t new_len;
	int s;
	int new_s;

	int retval;
	int shutdown = 0;
	fd_set rfds;
	struct timeval tv;

	char buf[MAX_LINE];
	char temp[MAX_LINE];
	char *fileName;
	char directory[MAX_HEADER_LINE];
	char *ftype;
	char *request;
	int i;
	FILE *fd;
	char *buff = malloc(MAX_FILE_SIZE);
	int served;
	float totalServed = 0;


	/*Initialize HTTP header*/
	struct httpHeader *header = malloc(sizeof(struct httpHeader));
	strlcpy(header->connection, "Connection: close\r\n",
		sizeof(header->connection));
	strlcpy(header->server, "Server: cs360httpd/0.1 (Unix)",
		sizeof(header->server));
	strlcat(header->server + (MAX_HEADER_LINE -4), "\r\n\r\n",
		sizeof(header->server));


	/*Build address data structure*/
	bzero((char *) &sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(SERVER_PORT);

	/*Setup passive open*/
	if((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket failed to create.\n");
		exit(1);
	}
	if((bind(s, (struct sockaddr *) &sin, sizeof(sin))) < 0)
	{
		printf("Socket failed to bind.\n");
		exit(1);
	}

	/*Listen on socket for a connection*/
	listen(s, MAX_PENDING);

	/*Main driver*/
	while(!shutdown)
	{
		/*Time struct for select*/
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		/*Check if socket has an incoming message*/
		retval = select(s + 1, &rfds, NULL, NULL, &tv);
		if(retval > 0)
		{
			if((new_s = accept(s, (struct sockaddr*) &sin, &new_len)) < 0)
				printf("Accept call failed.\n");

			/*Clear buffer, get request, and copy buffer into a temp buffer*/
			bzero(buf, sizeof(buf));
			recv(new_s, buf, sizeof(buf), 0);
			memcpy(temp, buf, sizeof(temp));

			/*Get HTTP request type*/
			request = strtok (buf, " ");

			/*Get file name, directory and file type*/
			strlcpy(directory, argv[1], sizeof(directory));
			fileName = strtok(NULL, " /");
			ftype = strtok(temp, ".");
			ftype = strtok(NULL, " ");

			/*Check for appropriate directory size*/
			if(strlen(directory) + strlen(fileName) > MAX_HEADER_LINE)
			{
				fd = fopen(FILE_414, "r");
				served = fhandler(new_s, fd, buff, header, ftype, 3);
			}

			/*Check for GET Request*/
			if(strcmp(request, "get") == 0 || strcmp(request, "GET") == 0){

				/*Check for index file*/
				if(strcmp(fileName, "/") == 0 || strcmp(fileName, "HTTP")==0)
					strlcpy(fileName, FILE_INDEX, 16);

				/*Open file, if not found send 404*/
				strlcat(directory, fileName, sizeof(directory));
				fd = fopen(directory, "r");
				printf("dir: %s\n", directory);

				if(fd != NULL)
				{
					served = fhandler(new_s, fd, buff, header, ftype, 0);
				}
				else	/*File not found*/
				{
					fd = fopen(FILE_404, "r");
					served = fhandler(new_s, fd, buff, header, ftype, 2);
				}
			}
			else
			{
				fd = fopen(FILE_501, "r");
				served = fhandler(new_s, fd, buff, header, ftype, 4);
			}

			totalServed = totalServed + served;
		}/*End if(retval > 0)*/

		/*Check if ENTER was pressed*/
		if(exitKey())
		{
			shutdown = 1;
			close(new_s);
			close(s);
		}
	}/*End while*/

	/*Print the total megabytes the server handled while up*/
	printf("Total served: %.2f MB\n", totalServed / 1000000.0);

	/*Free memory*/
	free(header);
	free(buff);

	return 0;
}


/*
*********************************************************************
Author: 	Josh Kent

Purpose:	Handle requests and file types

Input:		new_s - file descriptor containing the socker
		fd - File descriptor of file needing to be read
		buff - Buffer for the file contents to read into
		header - Struct for containing the HTTP header
		ftype - The file type (i.e. .html, .png, .css, etc.)
		sc - The status code for the proper response type

Return:		An integer containing the size of the file
*/
int fhandler(int new_s, FILE *fd, char *buff, struct httpHeader *header,
		char *ftype, int sc)
{
	int fileSize;
	struct stat st;
	int fp;
	char *statusCode[5] =
	{
		"200 OK",
		"400 Bad Request",
		"404 Not Found",
		"414 Request-URI Too Long",
		"501 Not Implemented"
	};
	char *mimeType[4] =
	{
		"text/html",
		"text/css",
		"application/js",
		"image/png"
	};

	/*Get file size and read file into buffer*/
	fp = fileno(fd);	/*Get file descriptor for fstat()*/
	fstat(fp, &st);
	fileSize = st.st_size;
	int r;
	bzero(buff, sizeof(buff));
	r = fread(buff, 1, fileSize, fd);

	/*Set status code*/
	snprintf(header->status, sizeof(header->status),
			"HTTP/1.0 %s\r\n", statusCode[sc]);

	/*Set content type*/
	snprintf(header->contentType, sizeof(header->contentType),
			"Content-Type: %s\r\n", mimeType[getFileType(ftype)]);

	/*Set content size*/
	snprintf(header->contentLength, sizeof(header->contentLength),
			"Content-Length: %d\r\n", fileSize);

	printf("Serving: %.2f KB\n\n", (fileSize / 1000.0));
	send(new_s, header, sizeof(struct httpHeader), 0);
	send(new_s, buff, fileSize, 0);
	close(new_s);

	return fileSize;
}

/*
*********************************************************************
Author:		Josh Kent
Purpose:	Get file type
Return:		An integer containing the predertimed file type
*/
int getFileType(char *ft)
{
	if(strcmp(ft, "html") == 0)
		return 0;
	else if(strcmp(ft, "css") == 0)
		return 1;
	else if(strcmp(ft, "js") == 0)
		return 2;
	else if(strcmp(ft, "png") == 0)
		return 3;
	else
		return 0;
}

/*
*********************************************************************
Author:		Josh Kent
Purpose:	Handler to ignore ^C
*/
void sigintIgnore()
{
	signal(SIGINT, sigintIgnore);
	printf("**To Exit: Press \"Enter\"**\n");
	fflush(stdout); /*Flushes the buffer to output above message*/
}

/*
*********************************************************************
Author:		James Jerkins
Modified_By:	Josh Kent
Purpose:	Check stdin if the user presses enter
Return:		-1(true) if enter was pressed by the user
		-0(false) if enter was not detected in stdin
*/
int exitKey()
{
	fd_set rfds;
	struct timeval tv;
	int retval;
	char buff[1];

	/*Flags to set fread() to nonblocking*/
	int flags = fcntl(0, F_GETFL, 0);
	fcntl(0, F_SETFL, flags | O_NONBLOCK);

	/*Watch stdin (fd 0) to see if it has input*/
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);

	/*Wait for a short amount of time*/
	tv.tv_sec = 0;
	tv.tv_usec = 10000;

	/*Checks every file descriptor in set that are waiting to be read*/
	retval = select(1, &rfds, NULL, NULL, &tv);

	while(fread(buff, 1, 1, stdin) > 0)
	{
		if(buff[0] == '\n')
		return 1;
	}

	return 0;
}
