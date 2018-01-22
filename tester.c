#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include "err.h"

#define MAX_PID 32768
#define MAX_SIZE 1024
#define MAX_MULTIPLE_WORD_SIZE 30024
#define int64 long long int
#define accepted 'T'
#define rejected 'N'
#define validatorTerminatedChar 2
#define validatorTerminated '!'


#define NO_MORE_REQUEST '!'
#define QUEUE_NAME "/tester" 
#define MAX_QUEUE_NAME 50
#define MAX_PID_LEN 12

#define TESTER_REQUEST_SUCCES 1
#define RUN_REQUEST_SUCCES 1


char* pidStart = "@";
char* wordStart = "$";
char* wordEnd = "#";
char* messageFromTester = "t";


int numberOfSendRequest;
int numberOfReceivedAnswer;
int numberOfAcceptedWord;

int testerQueueDesc;


void setQueueName(char* name) {
	int PID = getpid();
	char strPID[MAX_PID_LEN];
	memset(name, 0, MAX_QUEUE_NAME);
	memset(strPID, 0, MAX_PID_LEN);
	sprintf(strPID, "%d", PID);
	strcpy(name, QUEUE_NAME);
	strcat(name, strPID);
}


typedef struct node {
    char word[MAX_SIZE];
	bool acceptance;
    struct node* next;
} node_t;
typedef struct node Node;


typedef struct queue {
    Node* head;
	Node* tail;
} queu_t;
typedef struct queue Queue;


bool queuEmpty(Queue* q) {
	return q->head == NULL;
}


void addWordToQueue(Queue* q, bool acceptance, char* word) {
	Node* new_node = malloc(sizeof(Node));
	new_node->next = NULL;
	new_node->acceptance = acceptance;
	memset(new_node->word, 0, MAX_SIZE);
	strcpy(new_node->word, word);
	if(queuEmpty(q)) {q->head = q->tail = new_node; return;}
	q->tail->next = new_node;
	q->tail = new_node;	
}


void printQueueToReport(Queue* q) {
	char acceptance[2];
	if(queuEmpty(q)) {return;}
	Node* t = q->head;
	Node* t_next;
	do {
		t_next = t->next;
		memset(acceptance, 0, 2);
		acceptance[0] = 'N';
		if(t->acceptance) acceptance[0] = 'T';
		printf("%s %s\n", t->word, acceptance); 
		t = t->next;
	} while(t_next != NULL);
}


void freeQueueMemory(Queue* q) {
	if(queuEmpty(q)) return;
	Node* t = q->head;
	Node* t_next;
	do {
		t_next = t->next;
		free(t);
		t = t->next;
	} while(t_next != NULL);
}

void printQueue(Queue* q) {
	if(queuEmpty(q)) return;
	Node* t = q->head;
	Node* t_next;

	do {
		t_next = t->next;
		//printf("slowo: %s \n", t->word); 
		t = t->next;
	} while(t_next != NULL);
}


void closeTesterQueue(int testerQueueDesc) {
	char testerQueueName[MAX_QUEUE_NAME];
	setQueueName(testerQueueName);
	if (mq_close(testerQueueDesc)) syserr("Error in close:");
  	if (mq_unlink(testerQueueName)) syserr("Error in unlink:");
}


void freeAllocatedResources(Queue* testerQueue) {
	freeQueueMemory(testerQueue);
	closeTesterQueue(testerQueueDesc);
}


void printReport(int x, int y, int z, Queue* testerQueue) {
	int pid = getpid();
	printf("PID: %d\n", pid);
	printQueueToReport(testerQueue);
	printf("Snt: %d\n", x);
	printf("Rcd: %d\n", y);
	printf("Acc: %d\n", z);	
}


void endTesterProcess(Queue* testerQueue) {
	printReport(numberOfSendRequest, numberOfReceivedAnswer, numberOfAcceptedWord, testerQueue);
	freeAllocatedResources(testerQueue);	
	exit(0);
}


int findSymbolPosition(const char* symbol, const char* buff) {
	int i;
	int len = strlen(buff);
	for(i = 0; i < len; i++) {
		if(symbol[0] == buff[i]) return i; 
	}
	return len;
}


void processMessageFromValidator(char* wordBuffer, Queue* testerQueue, const char* buffer) {
	if(buffer[validatorTerminatedChar] == validatorTerminated) {endTesterProcess(testerQueue);}
	numberOfReceivedAnswer++;
	int wordEndChar = findSymbolPosition(wordEnd, buffer);
	int wordStart = 2;
	int wordLen = wordEndChar - wordStart;
	memset(wordBuffer, 0, MAX_SIZE);
	strncpy(wordBuffer, buffer+wordStart, wordLen);
	bool acceptance = false;
	if(buffer[0] == 'T') {
		acceptance = true; 
		numberOfAcceptedWord++;
	}
	addWordToQueue(testerQueue, acceptance, wordBuffer);
}


//t@3321$aaaabbc#
void setMessage(const char* word, char* message) {
	int PID = getpid();
	char strPID[MAX_PID_LEN];
	memset(strPID, 0, MAX_PID_LEN);
	sprintf(strPID, "%d", PID);

	memset(message, 0, MAX_SIZE);
	strcpy(message, messageFromTester);
	strcat(message, pidStart);
	strcat(message, strPID);
	strcat(message, wordStart);
	strcat(message, word);
	strcat(message, wordEnd);
}


bool isMessageTerminating(char* message) {
	int startWordChar = findSymbolPosition(wordStart, message);
	int terminatingCharPos = startWordChar + 1;
	return message[terminatingCharPos] == validatorTerminated;
}


bool inputAvailable()  
{
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO+1, &fds, NULL, NULL, &tv); //  select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
  return (FD_ISSET(0, &fds));
}



int main(int argc, char **argv)
{
	mqd_t validatorQueue;
	char buffer[MAX_MULTIPLE_WORD_SIZE];
	char wordBuffer[MAX_SIZE];
	char testerMessage[MAX_SIZE];
	char* validatorQueueName = "/validatorReceive";

    validatorQueue = mq_open(validatorQueueName, O_WRONLY);
	Queue checkedWordQueue;	
	checkedWordQueue.head = checkedWordQueue.tail = NULL;

	mqd_t testerQueueDesc;
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;	
	char testerQueueName[MAX_QUEUE_NAME];
	setQueueName(testerQueueName);
    testerQueueDesc = mq_open(testerQueueName, O_CREAT | O_RDONLY | O_NONBLOCK, 0644, &attr);
	int ret;

	memset(buffer, 0, MAX_MULTIPLE_WORD_SIZE);
	bool getEOF = false;
	bool messageReceived;
	while(!getEOF) {
		while (!inputAvailable()) {
			ret = mq_receive(testerQueueDesc, buffer, MAX_SIZE, NULL);
			if(errno != EAGAIN) messageReceived = true;
			if (ret < 0) {
				 if (errno == EINTR || errno == EAGAIN) continue;
				 if(ret < 0) printf("tester receive error \n");
				 exit(0);
			} else {
				processMessageFromValidator(wordBuffer, &checkedWordQueue, buffer);
				memset(buffer, 0, MAX_MULTIPLE_WORD_SIZE);
			}
    	 }
		//scanf ("%s",buffer);
		fgets (buffer, MAX_SIZE, stdin);
		buffer[strlen(buffer)-1] = '\0';   
		setMessage(buffer, testerMessage);
		if(!isMessageTerminating(testerMessage)) numberOfSendRequest++;
        mq_send(validatorQueue, testerMessage, MAX_SIZE, 0);
		
		memset(buffer, 0, MAX_MULTIPLE_WORD_SIZE);
		messageReceived = false;	
		while(!messageReceived) {
			ret = mq_receive(testerQueueDesc, buffer, MAX_SIZE, NULL);
			if(errno != EAGAIN) messageReceived = true;			
			if (ret < 0) {
				 if (errno == EINTR || errno == EAGAIN) continue; // try again
				 if(ret < 0) printf("tester receive error \n");
				 break;
			} else {
				processMessageFromValidator(wordBuffer, &checkedWordQueue, buffer);
				memset(buffer, 0, MAX_MULTIPLE_WORD_SIZE);
				break;
			}
		}
	}

    return 0;
}
