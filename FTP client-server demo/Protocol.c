#include <sys/types.h>
#include <netinet/in.h> 
#include <unistd.h>
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>
#include <dirent.h>
#include "Protocol.h"

//function for sending opcodes between server and client
int sendOpcode(int fd, char opcode)
{
	write(fd, &opcode, 1);

	return 1;
}

//function used for receiving opcodes sent between server
//and client
int receiveOpcode(int fd, char *opcode)
{
	char code;
	int nr;
	nr = read(fd, &code, 1);
	*opcode = code;
	return nr;
}

//function for sending DIR packets
int sendDirPac(int fd, char opcode, char *buff, int nr)
{

	sendOpcode(fd, opcode);
	write(fd, buff, nr);
	
	return 1;
}

//function for receiving DIIR packets
int recDirPac(int fd, char *dirCont)
{
	char rec;
	char buff[MAX_BLOCK_SIZE];
	
	receiveOpcode(fd, &rec);

	if(rec == DIR_OPCODE || rec == PWD_OPCODE)
	{
		read(fd, dirCont, MAX_BLOCK_SIZE);
		memset(buff, 0, sizeof(buff));
		return 0;
	}
	else
	{
		memset(buff, 0, sizeof(buff));
		return -1;
	}
	
}

//function for sending request packets
int sendReqPac(int fd, char opcode, char* arg)
{
	char buff[MAX_BLOCK_SIZE];
	int nr;
	strcpy(buff, arg);
	nr = strlen(buff);
	buff[nr] = '\0';
	if(opcode == CD_OPCODE || opcode == PUT_OPCODE || opcode == GET_OPCODE)
	{
		sendOpcode(fd, opcode);
		write(fd, buff, nr);
	}
	memset(buff, 0, sizeof(buff));	
	return 1;
}

//function for receiving request packets
int recReqPac(int fd, char opcode, char* reqMSG)
{	
	if(opcode == CD_OPCODE || opcode == PUT_OPCODE || opcode == GET_OPCODE)
	{
		read(fd, reqMSG, MAX_BLOCK_SIZE);		
	}
	return 1;
}

//function for sending acknowledgement packets
int sendAckPac(int fd, char opcode, char MSG)
{
	if(opcode == CD_OPCODE || opcode == PUT_OPCODE || opcode == GET_OPCODE)
	{
		sendOpcode(fd, opcode);
		write(fd, &MSG, 1);
	}

	return 1;
}

//function for receiving ackowledgement packets
int recAckPac(int fd, char *buff)
{	
	char rec;
	char msg;
	receiveOpcode(fd, &rec);
	if(rec == CD_OPCODE || rec == PUT_OPCODE || rec == GET_OPCODE)
	{
		read(fd, &msg, sizeof(msg));
		*buff = msg;
		return 0;
	}
	else
		return -1;

	
}

//function for sending data packets
int sendDataPac(int fd, char *buff, int buffsize)
{
	return write(fd, buff, buffsize);
}

//function for receiving data packets
int recDataPac(int fd, char *buff, int buffsize)
{
	return read(fd,buff, buffsize);
}

//function for sending the size of a file
int sendDataFileSize(int fd,  int filesize, char opcode)
{
	int tmp = filesize;
	tmp = htonl((uint32_t)tmp);
	sendOpcode(fd, opcode);
	write(fd, &tmp, sizeof(tmp));
}

//function for receiving the size of a file
int recDataFileSize(int fd, int *filesize)
{
	int tmp;
	char rec;
	receiveOpcode(fd, &rec);
	if(rec == PUT_OPCODE || rec == GET_OPCODE)
	{
		read(fd, &tmp, sizeof(tmp));
		tmp = ntohl(tmp);
		*filesize = tmp;
		return 0;
	}
	else
		return -1;
}
