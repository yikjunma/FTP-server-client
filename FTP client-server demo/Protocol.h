//constant variables to represent the protocol's supported
//opcodes
static const char PWD_OPCODE = 'P';	//opcode for pwd command
static const char DIR_OPCODE = 'D';	//opcode for dir command
static const char CD_OPCODE = 'C';	//opcode for cd command
static const char GET_OPCODE = 'G';	//opcode for get command
static const char PUT_OPCODE = 'U';	//opcode for put command

//constant variables to represent the return values of an
//ackowledgement packet.
static const char READY_FOR_FILE = '0';		
static const char DIR_SUCCESS = '1';
static const char DIR_FAIL = '2';
static const char FILE_NOT_EXISTS = '3';
static const char FILE_CANNOT_ACPT_FILE = '4';
static const char FILE_EXISTS = '5';

#define MAX_BLOCK_SIZE 1024
#define SERV_TCP_PORT 4032

//the protocol's function prototypes
int sendOpcode(int fd, char opcode);
int receiveOpcode(int fd, char *opcode);
int sendDirPac(int fd, char opcode, char *buff, int nr);
int recDirPac(int fd, char *dirCont);
int sendReqPac(int fd, char opcode, char* arg);
int recReqPac(int sd, char opcode, char* reqMSG);
int sendAckPac(int fd, char opcode, char MSG);
int recAckPac(int fd, char *buff);
int sendDataPac(int fd, char *buff, int buffsize);
int recDataPac(int fd, char *buff, int buffsize);
int sendDataFileSize(int fd,  int filesize, char opcode);
int recDataFileSize(int fd, int *filesize);
