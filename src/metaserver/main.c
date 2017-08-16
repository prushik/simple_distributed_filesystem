//****************************************************************
//**															**
//**	PHilip RUshik. 2017										**
//**															**
//**			SDFS Metadata Server							**
//**															**
//**	Serves metadata for Simple Distributed File System.		**
//**															**
//****************************************************************

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#include "network.h"
#include "opcodes.h"

void usage()
{
	write(1, "SDFS Metadata server v0.1\nWritten by Philip Rushik\n", 51);
	write(1, "\n", 1);
	write(1, "USAGE: server [options]\n\nOptions:\n", 34);
	write(1, "\t-h\t\t---\tShow Help\n", 19);
	write(1, "\t-p <port>\t---\tUse alternate port number [default: 80]\n", 55);
	write(1, "\t-d <dir>\t---\tStart in different directory [default: ./]\n", 57);
	write(1, "\t-q\t\t---\tQuiet, don't print access log [default: disabled]\n", 59);

	return;
}

void error_(unsigned char *msg, int len)
{
	write(2, msg, len);
	exit(1);

	return;
}

int execute_and_send(int sock, unsigned char * fname)
{
	int i,j;

	if (fname[1] == '\0' || chdir(&fname[1]) == 0)
	{
		for (i=0;fname[i];i++) ;
		
		if (i < 2)
		{
			write(sock,"HTTP/1.0 301 Redirect\r\nLocation: index\r\n\r\n",42);
		} else {
			write(sock,"HTTP/1.0 301 Redirect\r\nLocation: ",33);
			write(sock,fname,i);
			write(sock,"/index\r\n\r\n",10);
		}
		close(sock); //close this, shutdown on close will only happen if all fds are closed
		exit(0);
	}

	write(sock,"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n",44);

	dup2(sock,1);

	close(sock); //close this, shutdown on close will only happen if all fds are closed

	char *argv[32];
	int argc = parse_uri(&fname[1], argv);
//	char relfname[strlen(fname)+2];
//	sprintf(relfname,"./%s",fname);

//	for (i = 0; i < argc; i++)
//	{
//		for (j = 0; argv[i][j]; j++) ;

//		write(2,argv[i],j);
//	}

	return execve(&fname[1],argv,NULL);
}

//This function blindly sends an entire file. It does not check the file size
int send_http_file(int sock, int bsize, unsigned char * fname, int quiet)
{
	unsigned char data[bsize];
	int i;
	off_t size;
	int fd;

	fd = open(&fname[1], O_RDONLY, 0);

	if (fd < 0)
	{
		if (!quiet)
		{
			for (i=0; fname[i+1]; i++) ;

			write(2,"Not Found: ",11);
			write(2,&fname[1],i);
			write(2,"\n",1);
		}
		return 0;
	}

	do
	{
		i=read(fd,data,bsize);
		write(sock,data,i);
	} while (i==bsize);

	return 1;
}

void show_addr(char *ifname,int port, char *rname)
{
//	int fd;
//	struct ifreq ifr;

//	fd = socket(AF_INET, SOCK_DGRAM, 0);
//	ifr.ifr_addr.sa_family = AF_INET;
//	strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
//	ioctl(fd, SIOCGIFADDR, &ifr);
//	close(fd);

//	printf("http://%s:%d/%s\n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),port,rname);

	return;
}

int parse_uri(char *uri, char **argv)
{
	int i;
	int argc=0;
	argv[argc] = uri;
	argc++;
	for (i = 0; uri[i]; i++)
	{
		if (uri[i] == '?' || uri[i] == '&')
		{
			uri[i] = '\0';
			argv[argc] = &uri[i+1];
			argc++;
		}
	}

	return argc;
}

int main(int argc, char **argv)
{
	int sockfd, clilen, openfd;
	unsigned char *data;
	struct sockaddr_in serv_addr = {0}, cli_addr = {0};
	int i,n,port=80,quiet=0;
	char *resp=NULL;
	char *ifname="eth0";

	if (argc==1)
	{
		usage();
		exit(0);
	}

	for (i=0;i<argc;i++)
	{
		if (argv[i][0]=='-')
		{
			if (argv[i][1]=='p')//Alternate port number
				port = atoi(argv[i+1]);
			if (argv[i][1]=='d')//Base directory - careful with this, nothing prevents ../../../../etc/shadow
				chdir(argv[i+1]);
			if (argv[i][1]=='f')//Specify response file
				resp=argv[i+1];
			if (argv[i][1]=='i')//Specify interface
				ifname=argv[i+1];
			if (argv[i][1]=='q')//Shut the hell up
				quiet=1;
			if (argv[i][1]=='h')//Display help
			{
				usage();
				exit(0);
			}
		}
	}
	if (resp==NULL && !quiet)
		show_addr(ifname,port,"");
	else
		show_addr(ifname,port,resp);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("Error opening socket\n", 21);
//	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("Error binding\n",14);
	clilen = sizeof(cli_addr);

	//Just in-case the client disconnects unexpectedly
	signal(SIGPIPE, SIG_IGN);

	//Reap children immediately
	signal(SIGCHLD, SIG_IGN);

	listen(sockfd,5);
	while (1)
	{
		openfd = accept(sockfd,(struct sockaddr *) &cli_addr,&clilen);
		if (openfd < 0)
			error("Error accepting connection\n", 27);

		int pid = fork();

		if (!pid)
		{
			// I'm using PATH_MAX here since a uri is really just a path
			unsigned char buffer[512] = {0};
			n = read(openfd,buffer,512);

			if (n <= 0)
				error("Error receiving\n", 16);

//			struct http_request req;
//			int req_len = parse_http_request(buffer,&req);

			if (!quiet)
			{
//				if ((((uint32_t*)(req.METHOD))[0] & 0x00ffffff) == 0x00544547)  //Thats "GET " in little endian
					write(2,"get\t",4);
//				if (((uint32_t*)(req.METHOD))[0] == 0x54534f50)  //Thats "POST" in little endian
					write(2,"post\t",5);
//				write(2,&req.URI[1],req_len-1);
//				write(2,"\n",1);
			}

//			execute_and_send(openfd,req.URI);
//			if (!send_http_file(1, 1024, req.URI, quiet))
				write(1,"404 Not Found\n",14);

			close(openfd); // unfortunately, this needs to be done by the child
			exit(1);
		}
		close(openfd);
	}

	return 0;
}
