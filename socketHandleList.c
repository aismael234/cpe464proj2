#include <string.h>
#include <stdlib.h>
#include "socketHandleList.h"

node *head = NULL;
node *current = NULL;

// add node to beginning of list
void addNode(int socket, char* handle){

    //create a link
    node *new = (node*) malloc(sizeof(struct node));
    new->socket = socket;
    // copy handle in
    memcpy(new->handle, handle, strlen(handle));
    new->next = head;

    head = new;
}

void deleteNode(int clientSocket){
    node *temp = head;
    node *prev;

    // If at beginning of list
    if (temp != NULL && temp->socket == clientSocket) {
        head = temp->next;
        return;
    }

    // If in middle of list
    while (temp != NULL && temp->socket != clientSocket) {
        prev = temp;
        temp = temp->next;
    }

    // If handle does not exist
    if (temp == NULL) 
        return;

    // Remove node
    prev->next = temp->next;
    free(temp);
}

// returns target node. If not found, return NULL
node* findNode(char* handle){

   struct node *temp = head;
   while(temp != NULL) {
      if (!strcmp(temp->handle, handle)) {
         return temp;
      }
      temp = temp->next;
   }
   return NULL;
}

