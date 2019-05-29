#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>
#include <sys/types.h>        
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <unistd.h>  
#include <sys/wait.h>
#include "Protocol.h" 

//constant variables for labelling supported commands
static const char* LDIR = "ldir";
static const char* DIR = "dir";
static const char* LPWD = "lpwd";
static const char* PWD = "pwd";
static const char* LCD = "lcd";
static const char* CD = "cd";
static const char* GET = "get";
static const char* PUT = "put";
static const char* QUIT = "quit";

//function for changing client program's working directory
void clientCD(const char* arg)
{
	chdir(arg);
}

//creates child processes for command execution
int clientCMDnoArg(const char* cmd)
{
	pid_t pid;
	int status = 0;

	pid = fork();
	if(pid == 0)//child process
	{
		execlp(cmd,cmd,NULL);//runs command
		exit(10);
	}
	else if(pid > 0)//parent process waits till child finishes
	{
		wait(&status);
	}
	else//error when child cant be created
	{
		perror("fork");
		exit(1);
	}
	return 1;
}

//function that communicates with server to get the server's 
//working directory
void servPWD(int sd)
{
	char opcode = PWD_OPCODE;
	char rec;
	char buff[MAX_BLOCK_SIZE];
	//send opcode
	sendOpcode(sd, opcode);
	recDirPac(sd, buff);
	printf("server dir is %s\n", buff);
	memset(buff, 0, sizeof(buff));
}

//function that communicates with server to change the
//server's current working directory
void servCD(int sd, char* arg)
{	
	char opcode = CD_OPCODE;
	char numCode;
	char returnMsg;
	char buff[MAX_BLOCK_SIZE];
	sendReqPac(sd, opcode, arg);	
	recAckPac(sd, &returnMsg);
	
	if(returnMsg == '1')
	{
		printf("change dir success\n");
	}
	else if(returnMsg == '2')
	{
		printf("change dir fail\n");
	}
	memset(buff, 0, sizeof(buff));
}

//function that communicates with server to display the
//contents of the server's current working directory
void servDIR(int sd)
{
	char opcode = DIR_OPCODE;
	char buff[MAX_BLOCK_SIZE];
	//send opcode
	sendOpcode(sd, opcode);

	//get directory listing
	recDirPac(sd, buff);
	char *newString;
	newString = strtok(buff, "%");
	while (newString != NULL)
	{
		for(int i =0; i < 5; i++)
		{
			if(newString != NULL)
			{
				printf("%s\t", newString);
				newString = strtok(NULL, "%");
			}
			else
			{
				break;
			}
			
		}
		printf("\n");
		
	}
	
	memset(buff, 0, sizeof(buff));
}

//function that communicates with the server to get a file from
//the server to the client
void servGET(int sd, char* arg)
{
	char opcode = GET_OPCODE;
	char returnMsg;
	char path[MAX_BLOCK_SIZE];
	char buff[MAX_BLOCK_SIZE];
	FILE *fp;
	int filesize;
	int remain;
	int nr = 0;
	//send opcode
	sendReqPac(sd, opcode, arg);
	//get acknowledgement packet
	recAckPac(sd, &returnMsg);
	//if acknowledgement is ok, proceed, else cannot complete function
	if(returnMsg == FILE_EXISTS)
	{
		sendAckPac(sd, GET_OPCODE, READY_FOR_FILE);
		getcwd(path, sizeof(path));
		strcat(strcat(path, "/"), arg);
		fp = fopen(path, "wb");
		
		recDataFileSize(sd, &filesize);

		remain = filesize;
		getcwd(path, sizeof(path));
		strcat(strcat(path, "/"), arg);
		fp = fopen(path, "wb");

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
	else if(returnMsg == FILE_NOT_EXISTS)
	{
		printf("File does not exist.\n");
	}
	memset(path, 0, sizeof(path));
}

//function that communicates with the server to put a file
//from client's current working directory to the server
void servPUT(int sd, char* arg)
{
	char opcode = PUT_OPCODE;
	char buff[MAX_BLOCK_SIZE];
	char path[MAX_BLOCK_SIZE];
	char returnMsg;
	long filesize;
	int nw = 0;
	FILE *fp;
	//get the path to the file to be uploaded
	getcwd(path, sizeof(path));
	strcat(strcat(path, "/"), arg);
	//if the file exists, proceed
	if( access(path, F_OK) == 0)
	{
		fp = fopen(path, "rb");
		sendReqPac(sd, opcode, arg);	//send server a put request
		
		recAckPac(sd, &returnMsg);	//receive acknowledgement from serve
		
		if(returnMsg == READY_FOR_FILE)	//if the acknowledgement is positive, proceed
		{
			fseek(fp, 0, SEEK_END);
			filesize = ftell(fp);
			sendDataFileSize(sd, filesize, GET_OPCODE);
			int remain = filesize;
			int offset = 0;
			int numWBlocks = 0;
			//iterate through the file until all of it is uploaded
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
		}
		else if(returnMsg == FILE_EXISTS)
		{
			printf("Server already has file\n");
		}
		else if(returnMsg == FILE_CANNOT_ACPT_FILE)
		{
			printf("Server cannot accept this file\n");
		}
		fclose(fp);
	}
	else
	{
		printf("File with name %s does not exist or not available on the client.\n",path);
	}

	
	memset(path, 0, sizeof(path));
	memset(buff, 0, sizeof(buff));
}

//sets up client
int main (int argc, char *argv[]) 
{
	int sd, n, nr, nw, i=0;
	char host[100];
	struct sockaddr_in ser_addr; 
	struct hostent *hp;
	unsigned short port;
	if(argc == 1)
	{
		gethostname(host, sizeof(host));
		port = SERV_TCP_PORT;
	}
	else if(argc == 2)
	{
		strcpy(host, argv[1]);
		port = SERV_TCP_PORT;
	}
	else if(argc == 3)
	{
		strcpy(host, argv[1]);
          	int n = atoi(argv[2]);
          	if (n >= 1024 && n < 65536) 
		{              	
			port = n;
		}         	
		else {
              		printf("Error: server port number must be between 1024 and 65535\n");
              		exit(1);
          	}
	}
	else
	{
		printf("Usage: %s [ <server host name> [ <server listening port> ] ]\n", argv[0]); 
         	exit(1); 
	}
	

     	bzero((char *) &ser_addr, sizeof(ser_addr));
     	ser_addr.sin_family = AF_INET;
     	ser_addr.sin_port = htons(port);
     	if ((hp = gethostbyname(host)) == NULL)
	{
        	printf("host %s not found\n", host); exit(1);   
     	}
     	ser_addr.sin_addr.s_addr = * (u_long *) hp->h_addr;


     	sd = socket(PF_INET, SOCK_STREAM, 0);
     	if (connect(sd, (struct sockaddr *) &ser_addr, sizeof(ser_addr))<0) 
	{ 
     		perror("client connect"); exit(1);
     	}
    	
	int SIZE = 1000;   
	char str[SIZE];    
	const char s[3] = " \n";
	const char t[2] = "\n";
	char *command, *arg;

	//check for commands 
	while(1)
	{
		printf("$>");
		fgets(str,sizeof(str), stdin);
		
		command = strtok(str, s);
		
		if((arg = strtok(NULL, t)) == NULL)
		{
			arg = "\0";
		}
		
		if(command != NULL && arg != NULL)
		{
			printf("command: %s \t arg: %s \n", command, arg);

			if(strcmp(command, PWD) == 0)
			{
				servPWD(sd);
			}
			else if(strcmp(command, LPWD) == 0)
			{
				clientCMDnoArg(PWD);
			}
			else if(strcmp(command, DIR) == 0)
			{
				servDIR(sd);
			}
			else if(strcmp(command, LDIR) == 0)
			{
				clientCMDnoArg(DIR);
			}
			else if(strcmp(command, CD) == 0 && arg[0] != '\0')
			{
				servCD(sd, arg);
			}
			else if(strcmp(command, LCD) == 0 && arg[0] != '\0')
			{
				clientCD(arg);
			}
			else if(strcmp(command, "get") == 0 && arg[0] != '\0')
			{
				servGET(sd, arg);
			}
			else if(strcmp(command, "put") == 0 && arg[0] != '\0')
			{
				servPUT(sd, arg);
			}
			else if(strcmp(command, "quit") == 0)
			{
				exit(0);
			}
			else if(arg[0] == '\0' )
			{
				printf("Argument for command '%s' is invalid.\n", command);
			}	
			else
			{
				printf("Command '%s' not recognised.\n", command);
			}
		}
	}
	return(0);
}

