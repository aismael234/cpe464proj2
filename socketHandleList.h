#include "util.h"

struct node {
   int socket;
   char handle[MAX_HANDLE_LENGTH];
   struct node *next;
} typedef node;

void addNode(int socket, char* handle);
void deleteNode(int clientSocket);
node* findNodeByHandle(char* handle);
node* findNodeBySocket(int socket);
node* getAllNodes();