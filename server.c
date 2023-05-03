/******************************************************************************
* myServer.c
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

#include "networks.h"
#include "pdu.h"
#include "pollLib.h"
#include "flags.h"
#include "socketHandleList.h"
#include "util.h"

#define MAXBUF 65536
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket, uint8_t* dataBuf);
int checkArgs(int argc, char *argv[]);
void serverControl(int mainSocket);
void addNewSocket(int mainSocket);
void processClient(int clientSocket);
void sendToClient(int socketNum, uint8_t* sendBuf, int sendLen);

int handleConnect(uint8_t* dataBuffer, int clientSocket);
int handleExit(uint8_t* dataBuffer, int clientSocket);

void handleList(uint8_t* dataBuffer, int clientSocket);
void handleUnicastOrMulticast(uint8_t* dataBuffer, int senderSocket);
void handleBroadcast(uint8_t* dataBuffer, int senderSocket);

// remove
int readFromStdin(uint8_t * buffer);

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	serverControl(mainServerSocket);

	/* close the sockets */
	close(mainServerSocket);

	return 0;
}

void serverControl(int mainSocket) {

	setupPollSet();
	addToPollSet(mainSocket);

	int socket;
	while(1) {

		socket = pollCall(-1);
		//printf("socket: %d\n", socket);
		if(socket == mainSocket)
			addNewSocket(socket);
		else {
			processClient(socket);
		}
	}
}

void addNewSocket(int mainSocket) {

	// wait for client to connect
	int clientSocket = tcpAccept(mainSocket, DEBUG_FLAG);
	addToPollSet(clientSocket);
}

void processClient(int clientSocket) {
	uint8_t dataBuffer[MAXBUF];
	memset(dataBuffer, 0, MAXBUF);
	recvFromClient(clientSocket, dataBuffer);
	//printf("Message received, Data: %s\n", dataBuffer);
		if(dataBuffer[0] == CONNECT) {

			int sendLen = handleConnect(dataBuffer, clientSocket);
			sendToClient(clientSocket, dataBuffer, sendLen);
		}
		else if(dataBuffer[0] == EXIT) {
			int sendLen = handleExit(dataBuffer, clientSocket);
			sendToClient(clientSocket, dataBuffer, sendLen);
		}
		else if(dataBuffer[0] == UNICAST || dataBuffer[0] == MULTICAST) {
				 	
			handleUnicastOrMulticast(dataBuffer, clientSocket);
		}
		else if(dataBuffer[0] == BROADCAST) {
			handleBroadcast(dataBuffer, clientSocket);
		}
		else if(dataBuffer[0] == LIST_REQUEST) {
			handleList(dataBuffer, clientSocket);
		}

}

void recvFromClient(int clientSocket, uint8_t* dataBuf) {
	
	int messageLen = 0;
	
	// now get the data from the client_socket
	if ((messageLen = recvPDU(clientSocket, dataBuf, MAXBUF)) < 0)
	{
		perror("recv call");
		exit(-1);
	}

	if (messageLen <= 0)
	{
		// printf("Connection closed by other side\n");
		deleteNode(clientSocket);
		removeFromPollSet(clientSocket);
	}
}

void sendToClient(int socketNum, uint8_t* sendBuf, int sendLen)
{
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	
	sent = sendPDU(socketNum, sendBuf, sendLen);
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

}

// if new client sends connection PDU. returns PDU length
int handleConnect(uint8_t* dataBuffer, int clientSocket) {

	uint8_t clientHandle[MAXBUF];
	int handleLength = dataBuffer[1];
	// get handle name
	memcpy(clientHandle, &dataBuffer[2], handleLength);
	
	// handle already exists/ doesn't fulfill requirements
	if(findNodeByHandle((char*)clientHandle) != NULL) {

		memset(dataBuffer, 0, MAXBUF);
		dataBuffer[0] = CONNECT_DENY;
		dataBuffer[1] = '\0';

		char errorMsg[MAXBUF];
		//printf("Handle is Not Valid.\n");
		sprintf(errorMsg, "Handle already in use: %s", clientHandle);
		memcpy((char*)&dataBuffer[1], errorMsg, strlen(errorMsg) + 1);
		printf("error: %s\nlength: %zu\n", errorMsg, strlen(errorMsg) + 1);
		// flag + error message length + null terminator
		return 1 + strlen(errorMsg) + 1;
	}
	// handle is valid
	else {
		//printf("Handle is valid\n");
		addNode(clientSocket, (char*)&dataBuffer[2]);
		printf("Handle: %s\n", (char*)&dataBuffer[2]);
		memset(dataBuffer, 0, MAXBUF);

		dataBuffer[0] = CONNECT_CONFIRM;
		return 1;
	}

}

// if new client sends connection PDU. returns PDU length
int handleExit(uint8_t* dataBuffer, int clientSocket) {

	uint8_t clientHandle[MAXBUF];
	int handleLength = dataBuffer[1];
	// get handle name
	memcpy(clientHandle, &dataBuffer[2], handleLength);
	
	// handle does not exist
	if(findNodeBySocket(clientSocket) == NULL) {

		printf("handleExit: handle not found: %s\n", (char*)clientHandle);
		exit(-1);
	}
	// handle exists
	else {
		deleteNode(clientSocket);
		memset(dataBuffer, 0, MAXBUF);

		dataBuffer[0] = EXIT_ACK;
		return 1;
	}

}

// processs unicast and multicast packets
 void handleUnicastOrMulticast(uint8_t* dataBuffer, int senderSocket) {

	// offset for # of dest handles = flag + sendLen + sendHandle
	char handleBuffer[MAX_HANDLE_LENGTH];
	memset(handleBuffer, 0, MAX_HANDLE_LENGTH);
	uint8_t numHandles = dataBuffer[1 + 1 + dataBuffer[1]];
	//printf("sender length: %hu\n", dataBuffer[0]);
	printf("number of handles: %hu\n", numHandles);
	int i = 0;
	// offset for beginning of destination handles (starts at dest len)
	int offset = 1 + 1 + dataBuffer[1] + 1;
	// traverse through PDU and handle each dest handle accordingly
	for(i = 0; i < numHandles; i++) {
		int handleLength = dataBuffer[offset];
		offset += 1;
		memcpy(handleBuffer, &dataBuffer[offset], handleLength);
		//printf("Target handle: %s\n", handleBuffer);
		node* dest = NULL;
		// if dest handle not found, send sender handle an error PDU
		if((dest = findNodeByHandle(handleBuffer)) == NULL) {
			uint8_t errorPDU[MAXBUF];
			memset(errorPDU, 0, MAXBUF);
			// flag
			errorPDU[0] = HANDLE_ERROR;
			// dest handle length
			errorPDU[1] = strlen(handleBuffer);
			// dest handle
			memcpy((char*)&errorPDU[2], handleBuffer, strlen(handleBuffer));
			// error message
			char errorMsg[MAXBUF];
			sprintf(errorMsg, "Client with handle %s does not exist.", handleBuffer);
			errorMsg[strlen(errorMsg) + 1] = '\0';
			memcpy((char*)&errorPDU[1 + 1 + strlen(handleBuffer)], errorMsg, strlen(errorMsg) + 1);
			// size = flag + error message len + null terminator
			sendToClient(senderSocket, errorPDU, 1 + strlen(errorMsg) + 1);

		}
		// if found, send PDU to dest handle
		else {
			sendToClient(dest->socket, dataBuffer, strlen((char*)dataBuffer));
		}
		memset(handleBuffer, 0, MAX_HANDLE_LENGTH);
		offset += handleLength;
	}
}

// process unicast and multicast packets
 void handleBroadcast(uint8_t* dataBuffer, int senderSocket) {
	char handleBuffer[MAX_HANDLE_LENGTH];
	memset(handleBuffer, 0, MAX_HANDLE_LENGTH);
	memcpy(handleBuffer, &dataBuffer[2], dataBuffer[1]);
	int offset = 1 + 1 + dataBuffer[1];
	printf("broadcast pdu: %s\n", dataBuffer);

	char msg[MAXBUF];
	memcpy(msg, &dataBuffer[offset], strlen((char*)&dataBuffer[offset]));

	node* head = getAllNodes();
	node* dest = head;

	while(dest != NULL) {
		printf("%s\n", dest->handle);
		// if dest handle not found, send sender handle an error PDU
		if((dest = findNodeByHandle(handleBuffer)) == NULL) {
			uint8_t errorPDU[MAXBUF];
			memset(errorPDU, 0, MAXBUF);
			// flag
			errorPDU[0] = HANDLE_ERROR;
			// dest handle length
			errorPDU[1] = strlen(handleBuffer);
			// dest handle
			memcpy((char*)&errorPDU[2], handleBuffer, strlen(handleBuffer));
			// error message
			char errorMsg[MAXBUF];
			sprintf(errorMsg, "Client with handle %s does not exist.", handleBuffer);
			errorMsg[strlen(errorMsg) + 1] = '\0';
			memcpy((char*)&errorPDU[1 + 1 + strlen(handleBuffer)], errorMsg, strlen(errorMsg) + 1);
			// size = flag + error message len + null terminator
			sendToClient(senderSocket, errorPDU, 1 + strlen(errorMsg) + 1);

		}
		// if found, send PDU to dest handle
		else {
			sendToClient(dest->socket, dataBuffer, strlen((char*)dataBuffer));
		}
		dest = dest->next;
	}
	free(head);
 }

void handleList(uint8_t* dataBuffer, int clientSocket) {

	dataBuffer[0] = LIST_REPLY;
	int numHandles = 0;

	node* head = getAllNodes();
	printf("node: %s\n", head->handle);
	node* dest = head;
	
	while(dest != NULL) {
		numHandles += 1;
		dest = dest->next;
	}
	numHandles = htonl(numHandles);

	memcpy(&dataBuffer[1], &numHandles, 4);

	// send list begin pdu
	sendToClient(clientSocket, dataBuffer, 5);

	// begin sending each handle separately
	
	dest = head;
	while(dest != NULL) {
		memset(dataBuffer, 0, MAXBUF);
		dataBuffer[0] = HANDLE;
		dataBuffer[1] = strlen(dest->handle);
		memcpy(&dataBuffer[2], dest->handle, dataBuffer[1]);
		printf("handle: %s\n", &dataBuffer[2]);
		sendToClient(clientSocket, dataBuffer, 1 + 1 + dataBuffer[1]);
		dest = dest->next;
	}
	// send list end pdu
	memset(dataBuffer, 0, MAXBUF);
	dataBuffer[0] = LIST_END;
	sendToClient(clientSocket, dataBuffer, 1);
 }


// remove
int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0; 
	
	// Important you don't input more characters than you have space
	buffer[0] = '\0';
	printf("Enter data: ");
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

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

