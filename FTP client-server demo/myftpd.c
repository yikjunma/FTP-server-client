#include <string.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <syslog.h>
#include <netinet/in.h>
#include "Protocol.h"
#include <dirent.h>

void ClaimChildren()
{
	pid_t pid = 1;

	while(pid > 1)
	{	//claim as many zombies as we can
		pid = waitpid(0, (int *)0, WNOHANG);
	}
}

void DaemonInit(char * dir)
{
	pid_t pid;
	struct sigaction act;

	if( (pid = fork() ) < 0) 
	{
		perror("fork");
		exit(1);
	}
	else if(pid > 0)
	{
		printf("server pid = %d\n", pid);
		exit(0);	//parent terminates
	}

	//child process continues
	setsid();	//become session leader
	chdir(dir);	//change working directory
	umask(0); 	//clear file mode creation mask

	//catch SIGCHILD to remove zombies from system
	act.sa_handler = ClaimChildren;		//use reliable signal
	sigemptyset(&act.sa_mask);		//not to block other signals
	act.sa_flags = SA_NOCLDSTOP;		//not catch stopped children
	sigaction(SIGCHLD, (struct sigaction *)&act, (struct sigaction *)0);
}

//Manages the clients connected to the server
void ServeAClient(int sd, FILE* log)
{
	fputs("A client has connected. \n", log);
	fflush(log);
	
	int nr = 0, nw = 0;
	char buff[MAX_BLOCK_SIZE];
	char filename[256];
	char path[MAX_BLOCK_SIZE];

	char opcode, returnMsg, error = 'E';
	FILE *fp;
	long filesize;
	int fs;
	int received;
	DIR *dir;
	struct dirent *entry;
	fputs("Prcocess client request begin. \n", log);
	fflush(log);
	while (1)	//checks for opcodes to perform commands
	{
		nr = received = receiveOpcode(sd, &opcode);
		if(nr > 0)
		{
			fprintf(log, "Server received opcode %c \n" ,opcode);
			fflush(log);	
			if(opcode == PWD_OPCODE)
			{
				fprintf(log, "Server performing pwd command.\n");
				fflush(log);
				getcwd(path, sizeof(path));
				nr = strlen(path);
				fprintf(log, "Server current directory path %s. \n", path);
				fflush(log);
				sendDirPac(sd, PWD_OPCODE, path, nr);
				fprintf(log, "Server has sent path\n");
				fflush(log);
				nr = 0;
			}
			else if(opcode == DIR_OPCODE)
			{
				memset(buff, 0, sizeof(buff));
				fprintf(log, "Server performing dir command.\n");
				fflush(log);
				getcwd(path, sizeof(path));
				fprintf(log, "Server current directory path %s. \n", path);
				fflush(log);
				if((dir = opendir(path)) == NULL)
				{
					perror("opendir() error");
				}
				else
				{
					while((entry = readdir(dir)) != NULL)
					{
						strcat(buff, entry->d_name);
						strcat(buff, "%");
					}

					closedir(dir);
					nr = strlen(buff);
					//buff[nr] = '\0';
					fprintf(log, "Server files in path %s. \n", buff);
					fflush(log);
					sendDirPac(sd, DIR_OPCODE, buff, nr);

					fprintf(log, "Server sent current working directory files \n");
					fflush(log);
					nr = 0;
				}
				
			}
			else if(opcode == CD_OPCODE)
			{
				fprintf(log, "Server performing cd command.\n");
				fflush(log);
				recReqPac(sd, opcode, buff);
				fprintf(log, "Server changing dir to %s.\n", buff );
				fflush(log);	
				if(chdir(buff) == 0)
				{
					fprintf(log, "Server changed dir successfully message sent to client.\n" );
					fflush(log);	
					sendAckPac(sd, opcode, DIR_SUCCESS);
				}
				else if(chdir(buff) < 0)
				{
					fprintf(log, "Server changed dir unsuccessfully message sent to client.\n" );
					fflush(log);	
					sendAckPac(sd, opcode, DIR_FAIL);
				}
				
			}
			else if(opcode == GET_OPCODE)
			{
				fprintf(log, "Server performing get command\n.");
				fflush(log);
				//receive request for download
				recReqPac(sd, opcode, buff);
				fprintf(log, "Server received request to send %s file. \n", buff);
				fflush(log);	
				//get path for current working directory
				getcwd(path, sizeof(path));
				//concatenate filename to the current working directory
				strcat(strcat(path, "/"), buff);	//note: do we add '\0'?
				

				if( access(path, F_OK) == 0)	//cwdir is now path to file
				{
					fprintf(log, "File %s exists in server. \n", buff);
					fflush(log);	
					//file can be accessed
					sendAckPac(sd, opcode, FILE_EXISTS);
					//receive acknowledgement to continue download
					recAckPac(sd, &returnMsg);
					if(returnMsg == '0')
					{
						if((fp = fopen(path, "rb")) == NULL)
						{
							fprintf(log, "error opening file %s\n", path);
							fflush(log);
						} 
						else
						{
							//sendDataPac(sd, fp, opcode, log);
							fseek(fp, 0, SEEK_END);
							filesize = ftell(fp);
							sendDataFileSize(sd, filesize, GET_OPCODE);
							int remain = filesize;
							int offset = 0;
							int numWBlocks = 0;
							while(remain > 0)
							{
								if(remain >= MAX_BLOCK_SIZE)
								{
									fseek(fp, offset, SEEK_SET);
									fread(buff, MAX_BLOCK_SIZE, 1, fp);
									nw = sendDataPac(sd, buff, MAX_BLOCK_SIZE);
									remain = remain - nw;
									offset = offset + nw;
									numWBlocks++;
								}
								else if(remain < MAX_BLOCK_SIZE)
									break;

							}
							

							fseek(fp, MAX_BLOCK_SIZE * numWBlocks, SEEK_SET);
							char remBuff[remain];
							fread(remBuff, remain, 1, fp);
							sendDataPac(sd, remBuff, remain);
							memset(remBuff, 0, sizeof(remBuff));
							
							fclose(fp);
							fprintf(log, "File sent to client. \n");
							fflush(log);	
						}
					}				
				}
				else
				{
					fprintf(log, "File %s does not exists in server. \n", path);
					fflush(log);	
					sendAckPac(sd, opcode, FILE_NOT_EXISTS);
				}
				
				
			}
			else if(opcode == PUT_OPCODE)
			{
				fprintf(log, "Server performing put command.\n");
				fflush(log);
				recReqPac(sd, opcode, buff);
				fprintf(log, "Client request to upload file %s.\n" , buff);
				fflush(log);
				getcwd(path, sizeof(path));
				//concatenate filename to the current working directory
				strcat(strcat(path, "/"), buff);
				
				if(access(path, F_OK) != 0)
				{
					fp = fopen(path, "wb");
					sendAckPac(sd, PUT_OPCODE, READY_FOR_FILE);
					fprintf(log, "server sending ack packet that server is ready to recieve file.\n");
					fp = fopen(path, "wb");
					recDataFileSize(sd, &fs);

					int remain;

					remain = fs;
					

					while(remain > 0)
					{

						if(remain >= MAX_BLOCK_SIZE)
						{
							nr = recDataPac(sd, buff, MAX_BLOCK_SIZE);
							fwrite(buff, nr, 1, fp);
							memset(buff, 0, sizeof(buff));
							remain = remain - nr;
						}
						else if(remain < MAX_BLOCK_SIZE)
							break;
					}
			
					char remBuff[remain];
					nr = recDataPac(sd, remBuff, remain);
					fwrite(remBuff, nr, 1, fp);
					memset(remBuff, 0, sizeof(remBuff));

					fclose(fp);
				}
				else
				{
					sendAckPac(sd, opcode, FILE_EXISTS);
					fprintf(log, "server sending ack packet that server already has file.\n");
					fflush(log);
				}
			}
			else
			{
				sendOpcode(sd, error);
				fprintf(log, "error sent to client.\n");
				fflush(log);
			}
		}
		
		memset(buff, 0, sizeof(buff));
		memset(filename, 0, sizeof(filename));
		memset(path, 0, sizeof(path));
	}	
}

//sets up server
int main(int argc, char *argv[])
{
	char curDir[1000];
	FILE *log;
	int sd, nsd, n;
	pid_t pid;
	unsigned short port;	//server listening port
	socklen_t cliAddrLen;
	struct sockaddr_in serAddr, cliAddr;

	//create a log file
	log = fopen("server.txt", "wa");
	if(!log)
	{
		fprintf(log, "Error: could not create log file\n");
		fflush(log);
		exit(1);
	}
	fprintf(log, "%s \n", argv[1]);
	fflush(log);
	if(argc == 2)
	{
		if(chdir(argv[1]) < 0)
		{
			fprintf(log, "Cannot change to dir %s", argv[1]);
			fflush(log);
			exit(1);
		}
	}
	else
	{
		char* directory = "/";
		chdir(directory);
	}
	
	getcwd(curDir, sizeof(curDir));
	fprintf(log, "%s \n", curDir);
	fflush(log);
	
	//get port number

	port = SERV_TCP_PORT;


	//turn process into a daemon
	DaemonInit(curDir);

	//log daemon pid
	pid = getpid();
	fprintf(log, "My pid is %d\n", pid);
	fflush(log);
	
	//set up listening socket sd
	if((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("server:socket");
		exit(1);
	}
	
	//build server Internet socket address
	bzero((char *)&serAddr, sizeof(serAddr));
	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(port);
	serAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//accept client request sent to any one of the network
	//interface(s) on this host.

	//bind server address to socket sd
	if(bind(sd, (struct sockaddr *) &serAddr, sizeof(serAddr)) < 0)
	{
		perror("server bind");
		exit(1);
	}
	
	//become a listening socket
	listen(sd, 5);
	
	//do stuff
	while(1)
	{
		//wait to accept a client request for connection
		cliAddrLen = sizeof(cliAddr);
		nsd = accept(sd, (struct sockaddr *) &cliAddr, &cliAddrLen);
		if(nsd < 0)
		{
			if(errno == EINTR)	//if interrupted by SIGCHILD
			{
				continue;
			}
			perror("Server:accept"); 
			exit(1);
		}
		
		//create a child process to handle this client
		if((pid = fork()) < 0)
		{
			perror("fork");
			exit(1);
		}
		else if(pid > 0)
		{
			close(nsd);
			continue;	//parent to wait for next client
		}

		//now in child, serve the current client
		close(sd);
		ServeAClient(nsd, log);
	}
	fclose(log);
	exit(0);
}





