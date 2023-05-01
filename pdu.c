#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "pdu.h"
#include "safeUtil.h"

int sendPDU(int clientSocket, uint8_t* dataBuffer, int lengthOfData){
    
    // add 2 for pduLength
    uint8_t* pduBuffer = (uint8_t*)malloc(sizeof(uint8_t) * lengthOfData + 2);
    // get pdu length and insert into packet in network byte order
    // add 2 to include length of pduLength field
    uint16_t pduLength = (uint16_t)lengthOfData + 2;
    pduLength = htons(pduLength);
    memcpy(&pduBuffer[0], &pduLength, 2);
    memcpy(&pduBuffer[2], dataBuffer, lengthOfData);

    int ret;
    if((ret = safeSend(clientSocket, pduBuffer, lengthOfData + 2, 0)) == 0) {
        return ret;
    }

    free(pduBuffer);
    return lengthOfData;
}

int recvPDU(int socketNumber, uint8_t* dataBuffer, int bufferSize){

    uint8_t pduLengthBuffer[2];

    // receive first 2 bytes to get pduLength
    int ret;
    ret = safeRecv(socketNumber, pduLengthBuffer, 2, MSG_WAITALL);
    // return if 0 bytes received
    if(ret == 0) {
        return ret;
    }

    uint16_t pduLength;
    memcpy(&pduLength, &pduLengthBuffer[0], 2);
    // remove pduLength field size
    pduLength = ntohs(pduLength) - 2;

    

    // confirm input buffer size is large enough
    if(bufferSize < pduLength){
        fprintf(stderr, "bufferSize too small. pduLength: %hu, bufferSize: %hu\n", pduLength, (uint16_t) bufferSize);
        exit(-1);
    }

    // receive actual data
    ret = safeRecv(socketNumber, dataBuffer, pduLength, MSG_WAITALL);
    return ret;

}