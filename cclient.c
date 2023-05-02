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

int buildConnect(uint8_t* sendBuf);
int buildUnicast(uint8_t* sendBuf, int sendLen);

void handleUnicastOrMulticast(uint8_t* dataBuffer,int messageLen);

uint8_t findSendFlag(uint8_t* sendBuf);

char clientHandle[MAXBUF];
uint8_t clientLength;

int main(int argc, char* argv[])
{ 
	//socket descriptor
	// setup client connection
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
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	
	sendLen = readFromStdin(sendBuf);

	uint8_t flag = findSendFlag(sendBuf);
	if(flag == 0) {
		printf("Valid flag not detected. Please start message with a valid flag (%%M,%%C,%%B,%%L).\n");
		return;
	}

	if(flag == UNICAST) {
		sendLen = buildUnicast(sendBuf, sendLen);
		if(sendLen < 0) {
			printf("Usage: %%M dest-handle message\n");
		}
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
	
	// now get the data from the client_socket
	if ((messageLen = recvPDU(socket, dataBuffer, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen > 0)
	{
		//printf("data received: %d\n", messageLen);
		if(dataBuffer[0] == UNICAST) {
			handleUnicastOrMulticast(dataBuffer, messageLen);
		}
		if(dataBuffer[0] == HANDLE_ERROR) {
			printf("could not find handle\n");
		}
	}
	else
	{
		printf("Connection closed by other side\n");
		exit(0);
	}
}

void awaitServerConnect(int socket) {
	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	
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

int buildUnicast(uint8_t* sendBuf, int sendLen) {

	// build pdu based on flag
	uint8_t tempBuf[MAXBUF];
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

void handleUnicastOrMulticast(uint8_t* dataBuffer,int messageLen) {
	char msg[MAXBUF];

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

	uint8_t flagBuf[4];
	memcpy(&flagBuf[0], sendBuf, 3);
	flagBuf[3] = '\0';

	if(!strcmp((char*)flagBuf, "%M ") || !strcmp((char*)flagBuf, "%m "))
		return UNICAST;
	else if(!strcmp((char*)flagBuf, "%C ") || !strcmp((char*)flagBuf, "%c "))
		return MULTICAST;
	else if(!strcmp((char*)flagBuf, "%B ") || !strcmp((char*)flagBuf, "%b "))
		return BROADCAST;
	else if(!strcmp((char*)flagBuf, "%L ") || !strcmp((char*)flagBuf, "%l "))
		return LIST_REQUEST;
	else 
		return 0;
}