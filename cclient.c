/******************************************************************************
* myClient.c
*
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>
#include <ctype.h>

#include "networks.h"
#include "pdu.h"
#include "pollLib.h"
#include "flags.h"
#include "util.h"

#define MAXBUF 1024
#define DEBUG_FLAG 1

int readFromStdin(uint8_t * buffer);
int setup(int argc, char* argv[]);
void checkArgs(int argc, char * argv[]);

void clientControl(int clientSocket);

void processMsgFromServer(int socket);
void sendToServer(int socketNum);
void recvFromServer(int socket);
void awaitServerConnect(int socket);

// build PDU depending on message type
int buildConnect(uint8_t* sendBuf);
int buildExit(uint8_t* sendBuf);
int buildList(uint8_t* sendBuf);
int buildUnicast(uint8_t* sendBuf, int sendLen);
int buildBroadcast(uint8_t* sendBuf, int sendLen);
int buildMulticast(uint8_t* sendBuf, int sendLen);

void handleUnicastOrMulticast(uint8_t* dataBuffer,int messageLen);
void handleBroadcast(uint8_t* dataBuffer,int messageLen);
// CLIENT PROCESSING LIST
// DONT POLL FOR STDIN WHILE RECEIVING FLAG = 12 PACKETS
void handleList(uint8_t* dataBuffer,int messageLen);

uint8_t findSendFlag(uint8_t* sendBuf);
int isNumber(char* str);

char clientHandle[MAXBUF];
uint8_t clientLength;

int main(int argc, char* argv[])
{ 
	// setup client connection
	// socket descriptor
	int clientSocket = setup(argc, argv);        

	clientControl(clientSocket);

	close(clientSocket);
	return 0;
}

int setup(int argc, char* argv[]) {
	checkArgs(argc, argv);

	// client handle and length
	strcpy(clientHandle, argv[1]);
	clientLength = strlen(clientHandle);
	if(clientLength <= 0 || clientLength > 100) {
		printf("usage: %s handle(1-100 characters) host-name port-number \n", argv[0]);
		exit(-1);
	}
	if(!isalpha(clientHandle[0])) {
		printf("Handle must start with a letter and contain only alphanumeric characters (a-z, A-Z, 0-9).\n");
	}
	int i;
	for(i = 0; i < clientLength; i++) {
		if(!isalnum(clientHandle[i])) {
			printf("Handle must start with a letter and contain only alphanumeric characters (a-z, A-Z, 0-9).\n");
			exit(-1);
		}
	}

	/* set up the TCP Client socket  */
	int clientSocket = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

	// send connection initialization PDU
	uint8_t sendBuf[MAXBUF];
	int sendLen = buildConnect(sendBuf);
	sendPDU(clientSocket, sendBuf, sendLen);
	// wait for server connect reply
	awaitServerConnect(clientSocket);
	return clientSocket;
}

void clientControl(int clientSocket) {

	setupPollSet();
	addToPollSet(STDIN_FILENO);
	addToPollSet(clientSocket);

	int fd;
	while(1) {
		printf("$:");
		fflush(stdout);
		fd = pollCall(-1);
		if(fd == STDIN_FILENO)
			sendToServer(clientSocket);
		else if(fd == clientSocket) {
			printf("\n");
			processMsgFromServer(clientSocket);
		}
	}
}

void sendToServer(int socketNum)
{
	uint8_t sendBuf[MAXBUF];   //data buffer
	memset(sendBuf, 0, MAXBUF);
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	
	sendLen = readFromStdin(sendBuf);

	uint8_t flag = findSendFlag(sendBuf);
	if(flag == 0) {
		printf("Valid flag not detected. Please start message with a valid flag (%%M,%%C,%%B,%%L,%%E).\n");
		return;
	}

	if(flag == UNICAST) {
		sendLen = buildUnicast(sendBuf, sendLen);
		if(sendLen < 0) {
			printf("Usage: %%M dest-handle [message]\n");
			return;
		}
	}
	if(flag == MULTICAST) {
		sendLen = buildMulticast(sendBuf, sendLen);
		if(sendLen < 0) {
			printf("Usage: %%C num-handles(2-9) dest-handle dest-handle [dest-handle]... [message]\n");
			return;
		}
	}
	if(flag == BROADCAST) {
		sendLen = buildBroadcast(sendBuf, sendLen);
		if(sendLen < 0) {
			printf("Usage: %%B [message]\n");
			return;
		}
	}
	if(flag == EXIT) {
		sendLen = buildExit(sendBuf);
	}

	//printf("read: %s string len: %d (including null). flag: %d\n", sendBuf, sendLen, flag);
	//printf("PDU sent: %s, length: %zu\n", sendBuf, strlen((char*)sendBuf));
	//printf("sending data: %d\n", sendLen);
	sent = sendPDU(socketNum, sendBuf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	// printf("Amount of data sent is: %d\n", sent);
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0; 
	
	// Important you don't input more characters than you have space
	buffer[0] = '\0';

	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle(1-100 characters, start w/ letter) host-name port-number \n", argv[0]);
		exit(1);
	}
}

void processMsgFromServer(int socket) {
	recvFromServer(socket);
}

void recvFromServer(int socket) {
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	memset(dataBuffer, 0, MAXBUF);
	// now get the data from the client_socket
	if ((messageLen = recvPDU(socket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		//printf("flag received: %s\n", dataBuffer;
		if(dataBuffer[0] == UNICAST || dataBuffer[0] == MULTICAST) {
			handleUnicastOrMulticast(dataBuffer, messageLen);
		}
		if(dataBuffer[0] == HANDLE_ERROR) {
			int handleLength = dataBuffer[1];
			char handleBuffer[MAX_HANDLE_LENGTH];
			memcpy(handleBuffer, (char*)&dataBuffer[2], handleLength);
			printf("Client with handle %s does not exist.\n", handleBuffer);
		}
		if(dataBuffer[0] == EXIT_ACK) {
		exit(0);
		}
	}
	else
	{
		printf("Server Terminated\n");
		exit(0);
	}
}

void awaitServerConnect(int socket) {
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	memset(dataBuffer, 0, MAXBUF);
	
	// now get the data from the client_socket
	if ((messageLen = recvPDU(socket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		if(dataBuffer[0] == CONNECT_CONFIRM) {
			return;
		}
		if(dataBuffer[0] == CONNECT_DENY) {
			printf("%s\n", &dataBuffer[1]);
			exit(-1);
		}
	}
}

int buildConnect(uint8_t* sendBuf) {

	// build pdu based on flag
	uint8_t tempBuf[MAXBUF];
	memset(tempBuf, 0, MAXBUF);
	tempBuf[0] = (uint8_t)CONNECT;

	// sender handle length
	tempBuf[1] = clientLength;
	// sender handle
	memcpy(&tempBuf[2], clientHandle, clientLength);
	
	memset(sendBuf, 0, MAXBUF);
	memcpy(sendBuf, tempBuf, MAXBUF);
	
	// return PDU length
	// flag + send name + send len
	return 1 + clientLength + 1;
}

int buildExit(uint8_t* sendBuf) {

	// build pdu based on flag
	uint8_t tempBuf[MAXBUF];
	memset(tempBuf, 0, MAXBUF);
	tempBuf[0] = (uint8_t)EXIT;

	// sender handle length
	tempBuf[1] = clientLength;
	// sender handle
	memcpy(&tempBuf[2], clientHandle, clientLength);
	
	memset(sendBuf, 0, MAXBUF);
	memcpy(sendBuf, tempBuf, MAXBUF);
	
	// return PDU length
	// flag + send name + send len
	return 1 + clientLength + 1;
}

int buildList(uint8_t* sendBuf) {

	// build pdu based on flag
	uint8_t tempBuf[MAXBUF];
	memset(tempBuf, 0, MAXBUF);
	tempBuf[0] = (uint8_t)LIST_REQUEST;

	// sender handle length
	tempBuf[1] = clientLength;
	// sender handle
	memcpy(&tempBuf[2], clientHandle, clientLength);
	
	memset(sendBuf, 0, MAXBUF);
	memcpy(sendBuf, tempBuf, MAXBUF);
	
	// return PDU length
	// flag + send name + send len
	return 1 + clientLength + 1;
}

int buildUnicast(uint8_t* sendBuf, int sendLen) {

	// build pdu based on flag
	uint8_t tempBuf[MAXBUF];
	memset(tempBuf, 0, MAXBUF);
	tempBuf[0] = (uint8_t)UNICAST;

	char s[MAXBUF];
	strcpy(s, (char*)sendBuf);
	// tokenize to extract destination handle
	char* token = strtok(s, " ");
	token = strtok(NULL, " ");
	uint8_t tokenLength = strlen(token); 
	if(token == NULL) {
		printf("Usage: %%M dest-handle message\n");
		return -1;
	}
	// sender handle length
	tempBuf[1] = clientLength;
	// sender handle
	memcpy(&tempBuf[2], clientHandle, clientLength);
	// # of destinations
	tempBuf[2 + clientLength] = 1;
	// destination handle length
	tempBuf[2 + clientLength + 1] = tokenLength;
	// destination handle
	memcpy(&tempBuf[2 + clientLength + 1 + 1], token, tokenLength);
	// message
	memcpy(&tempBuf[2 + clientLength + 1 + 1 + tokenLength], &sendBuf[3 + tokenLength + 1], sendLen);
	
	memset(sendBuf, 0, MAXBUF);
	memcpy(sendBuf, tempBuf, MAXBUF);
	// correct message length(subtract "%M [handle] " portion)
	sendLen -= (3 + tokenLength + 1);
	// return PDU length
	// message length + sender name + dest name + (flag + send len + dest len + # of dest)
	return sendLen + clientLength + tokenLength + 4;
}

int buildMulticast(uint8_t* sendBuf, int sendLen) {
	// build pdu based on flag
	uint8_t tempBuf[MAXBUF];
	memset(tempBuf, 0, MAXBUF);
	tempBuf[0] = (uint8_t)MULTICAST;

	// sender handle length
	tempBuf[1] = clientLength;
	// sender handle
	memcpy(&tempBuf[2], clientHandle, clientLength);

	// get all dest handles
	char s[MAXBUF];
	strcpy(s, (char*)sendBuf);
	// tokenize to extract destination handle
	char* token = strtok(s, " ");
	token = strtok(NULL, " ");
	// num-handles
	char* numHandles = token;
	// validate num-handles (must be 2-9)
	if(!isNumber(numHandles) || strlen(numHandles) != 1) {
		//printf("fail 1\n");
		return -1;
	}
	if(numHandles[0] < 2) {
		//printf("fail 2\n");
		return -1;
	}
	// finally add num-handles
	tempBuf[2 + clientLength] = (uint8_t)atoi(numHandles);
	
	// add all destinations
	int i = 0;
	int offset = 2 + clientLength + 1;
	for(i = 0; i < atoi(numHandles); i++) {
		token = strtok(NULL, " ");
		if(token == NULL) {
		return -1;
		}
		uint8_t tokenLength = strlen(token); 

		// destination handle length
		tempBuf[offset] = tokenLength;
		offset += 1;
		// destination handle
		memcpy(&tempBuf[offset], token, tokenLength);
		offset += tokenLength;
	}
	// null terminator in case of empty message
	int emptyMsgOffset = offset;
	tempBuf[emptyMsgOffset] = '\0';
	// add message
	while((token = strtok(NULL, " ")) != NULL) {

		uint8_t tokenLength = strlen(token);
		memcpy(&tempBuf[offset], token, tokenLength);
		offset += tokenLength;
		tempBuf[offset] = ' ';
		offset += 1;
	}
	// if not empty message
	if(offset > emptyMsgOffset){
		tempBuf[offset - 1] = '\0';
	}
	
	memset(sendBuf, 0, MAXBUF);
	memcpy(sendBuf, tempBuf, MAXBUF);

	// return PDU length
	return offset;
}

int buildBroadcast(uint8_t* sendBuf, int sendLen) {
	// build pdu based on flag
	uint8_t tempBuf[MAXBUF];
	memset(tempBuf, 0, MAXBUF);
	tempBuf[0] = (uint8_t)BROADCAST;

	// sender handle length
	tempBuf[1] = clientLength;
	// sender handle
	memcpy(&tempBuf[2], clientHandle, clientLength);

	int offset = 2;
	while(sendBuf[offset] == ' ')
		offset += 1;

	// message
	memcpy((char*)&tempBuf[2 + clientLength], (char*)&sendBuf[offset], strlen((char*)&sendBuf[offset]));
	
	memset(sendBuf, 0, MAXBUF);
	memcpy(sendBuf, tempBuf, MAXBUF);
	// return PDU length
	return 1 + 1 + clientLength + strlen((char*)&sendBuf[offset]);
}

void handleBroadcast(uint8_t* dataBuffer,int messageLen) {
	char msg[MAXBUF];
	memset(msg, 0, MAXBUF);
	uint8_t senderLength = dataBuffer[1];
	memcpy(msg, &dataBuffer[2], senderLength);

	msg[senderLength] = ':';
	msg[senderLength + 1] = ' ';
	//printf("sender handle length: %d, offset: %d, message length: %d\n", senderLength, offset, messageLen);
	memcpy(&msg[senderLength + 2], (char*)&dataBuffer[1 + 1 + senderLength], strlen((char*)&dataBuffer[1 + 1 + senderLength]));
	printf("%s\n", msg);
}

void handleUnicastOrMulticast(uint8_t* dataBuffer,int messageLen) {
	char msg[MAXBUF];
	memset(msg, 0, MAXBUF);

	uint8_t senderLength = dataBuffer[1];
	memcpy(msg, &dataBuffer[2], senderLength);
	// # of destinations
	int numHandles = dataBuffer[2 + senderLength];
	//printf("number of handles: %d\n", numHandles);
	int offset = 2 + senderLength + 1;
	//printf("offset: %d\n", offset);
	int i;
	// skip through all handles until you reach data
	for(i = 0; i < numHandles; i++) {
		offset += dataBuffer[offset] + 1;
		//printf("new offset: %d\n", offset);
	}
	//printf("%s\n", &dataBuffer[offset]);
	msg[senderLength] = ':';
	msg[senderLength + 1] = ' ';
	//printf("sender handle length: %d, offset: %d, message length: %d\n", senderLength, offset, messageLen);
	memcpy(&msg[senderLength + 2], &dataBuffer[offset], messageLen - offset);
	printf("%s\n", msg);
	
}

// return valid flag number if found. Otherwise, return 0.
uint8_t findSendFlag(uint8_t* sendBuf) {

	// uint8_t flagBuf[4];
	// memcpy(&flagBuf[0], sendBuf, 3);
	// flagBuf[3] = '\0';

	char s[MAXBUF];
	strcpy(s, (char*)sendBuf);
	// tokenize to extract destination handle
	char* token = strtok(s, " ");
	if(token == NULL) {
		printf("Valid flag not detected. Please start message with a valid flag (%%M,%%C,%%B,%%L).\n");
		return -1;
	}

	if(!strcmp(token, "%M") || !strcmp(token, "%m"))
		return UNICAST;
	else if(!strcmp(token, "%C") || !strcmp(token, "%c"))
		return MULTICAST;
	else if(!strcmp(token, "%B") || !strcmp(token, "%b"))
		return BROADCAST;
	else if(!strcmp(token, "%L") || !strcmp(token, "%l"))
		return LIST_REQUEST;
	else if(!strcmp(token, "%E") || !strcmp(token, "%e"))
		return EXIT;
	else 
		return 0;
}

int isNumber(char* str){
	int i;
    for (i = 0; str[i]!= '\0'; i++)
    {
        if (isdigit(str[i]) == 0)
              return 0;
    }
    return 1;
}