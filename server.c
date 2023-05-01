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

#define MAXBUF 65536
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket, uint8_t* dataBuf);
int checkArgs(int argc, char *argv[]);
void serverControl(int mainSocket);
void addNewSocket(int mainSocket);
void processClient(int clientSocket);
void sendToClient(int socketNum, uint8_t* sendBuf, int sendLen);

int handleConnect(uint8_t* dataBuffer, int clientSocket);
//int handleUnicastOrMulticast(uint8_t dataBuffer);

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
		printf("socket: %d\n", socket);
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
	recvFromClient(clientSocket, dataBuffer);
	printf("Message received, Data: %s\n", dataBuffer);
		if(dataBuffer[0] == CONNECT) {

			int sendLen = handleConnect(dataBuffer, clientSocket);
			sendToClient(clientSocket, dataBuffer, sendLen);
		}
		// else if(dataBuffer[0] == UNICAST || dataBuffer[0] == MULTICAST) {
		//  // CREATE 2D CHAR* ARRAY HERE array[9][MAXBUF]
		// 	int sendLen = handleUnicastOrMulticast(dataBuffer);
		// 	sendToClient(clientSocket, dataBuffer, sendLen);
		// }

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
		printf("Connection closed by other side\n");
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
	if(findNode((char*)clientHandle) != NULL) {

		memset(dataBuffer, 0, MAXBUF);
		dataBuffer[0] = CONNECT_DENY;
		dataBuffer[1] = '\0';

		char errorMsg[MAXBUF];
		printf("Handle is Not Valid.\n");
		sprintf(errorMsg, "Handle already in use: %s", clientHandle);
		memcpy((char*)&dataBuffer[1], errorMsg, strlen(errorMsg) + 1);
		printf("error: %s\nlength: %zu\n", errorMsg, strlen(errorMsg) + 1);
		// flag + error message length + null terminator
		return 1 + strlen(errorMsg) + 1;
	}
	// handle is valid
	else {
		printf("Handle is valid\n");
		addNode(clientSocket, (char*)&dataBuffer[2]);
		printf("Handle: %s\n", (char*)&dataBuffer[2]);
		memset(dataBuffer, 0, MAXBUF);

		dataBuffer[0] = CONNECT_CONFIRM;
		return 1;
	}

}

// int handleUnicastOrMulticast(uint8_t dataBuffer) {
// 	// make an array of char* objects (2D array, array[9][MAXBUF]) in the function that calls this function?
// 	// add it as a parameter to be filled with the list of dest handles
// 	// add the # of dest handles as a parameter, use that for number of loops.
// 	// in each loop, send packet to corresponding dest handle indexed in the 2D array
// }


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

