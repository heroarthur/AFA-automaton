#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <unistd.h>
#include <stdbool.h>


#include "err.h"

#define MAX_PID 32768
#define MAX_SIZE 1024
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

//x to liczba wysłanych zapytań;
//y to liczba otrzymanych odpowiedzi;
//a z to liczba poprawnych słów;
int numberOfSendRequest; //x
int numberOfReceivedAnswer; //y
int numberOfAcceptedWord; //z

int testerQueueDesc;


void setQueueName(char* name) {
	int PID = getpid();
	char strPID[MAX_PID_LEN];
	memset(name, 0, MAX_QUEUE_NAME);
	memset(strPID, 0, MAX_PID_LEN);
	sprintf(strPID, "%d", PID);
	strcpy(name, QUEUE_NAME);
	strcat(name, strPID);
	printf("wartosc name: %s \n", name);
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
		printf("slowo: %s \n", t->word); 
		t = t->next;
	} while(t_next != NULL);
}


void closeTesterQueue(int testerQueueDesc) {
	char testerQueueName[MAX_QUEUE_NAME];
	setQueueName(testerQueueName);
	if (mq_close(testerQueueDesc)) syserr("Error in close:");
	printf("kolejka tester: %s \n", testerQueueName);
  	if (mq_unlink(testerQueueName)) syserr("Error in unlink:");
	//sleep(3);
	printf("pomyslnie zamknieto kolejki\n");
}


void freeAllocatedResources(Queue* testerQueue) {
	freeQueueMemory(testerQueue);
	printf("kolejka czysta\n");
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




/*
void setQueueName(char* name) {
	int PID = getpid();
	char strPID[MAX_PID_LEN];
	memset(name, 0, MAX_QUEUE_NAME);
	memset(strPID, 0, MAX_PID_LEN);
	sprintf(strPID, "%d", PID);
	strcpy(name, QUEUE_NAME);
	strcat(name, strPID);
	printf("wartosc name: %s \n", name);
}
*/

void endTesterProcess(Queue* testerQueue) {
	printf("NUMBER ONE!!!!\n");
	printReport(numberOfSendRequest, numberOfReceivedAnswer, numberOfAcceptedWord, testerQueue);
	printf("baby, take me higher!!, SPITFIRE!!!\n");
	freeAllocatedResources(testerQueue);	
	exit(0);
}

//message validator -> tester: T$aaaabbac#   T$!#
//message tester -> validator: t@3321$aaaabbc# albo T$!#
//message validator -> run 3321$aaaaabbc@automat#
//message run -> validator: r@3321$Taaaaabc#



/*
PID: pid\n
[[a-z] A|N\n]
Snt: x\n
Rcd: y\n
Acc: z\n
gdzie
pid to pid procesu testera;
a-z to litery alfabetu;
x to liczba wysłanych zapytań;
y to liczba otrzymanych odpowiedzi;
a z to liczba poprawnych słów;


PID: pid 
a A
aa N
aabbaba N
ab A
Snt: 4
Rcd: 4
Acc: 2
*/

int findSymbolPosition(const char* symbol, const char* buff) {
	int i;
	int len = strlen(buff);
	for(i = 0; i < len; i++) {
		if(symbol[0] == buff[i]) return i; 
	}
	return len;
}

//message validator -> tester: T$aaaabbac# 
void processMessageFromValidator(char* wordBuffer, Queue* testerQueue, const char* buffer) {
	if(buffer[validatorTerminatedChar] == validatorTerminated) {endTesterProcess(testerQueue);}
	//char recvAcceptance[1];
	//recvAcceptance[0] = wordBuffer[0];
	numberOfReceivedAnswer++;
	int wordEndChar = findSymbolPosition(wordEnd, buffer);
	int wordStart = 2;
	int wordLen = wordEndChar - wordStart;
	memset(wordBuffer, 0, MAX_SIZE);
	//strcpy(wordBuffer, buffer);
	strncpy(wordBuffer, buffer+wordStart, wordLen);
	printf("zapisuje slowo: %s \n", wordBuffer);
	bool acceptance = false;
	if(buffer[0] == 'T') {
		acceptance = true; 
		numberOfAcceptedWord++;
	}
	addWordToQueue(testerQueue, acceptance, wordBuffer);
}


/*
int numberOfSendRequest; //x
int numberOfReceivedAnswer; //y
int numberOfAcceptedWord;
*/


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
	//printf("wiadomosc do wyslania:  %s  \n", message);
}


bool isMessageTerminating(char* message) {
	int startWordChar = findSymbolPosition(wordStart, message);
	int terminatingCharPos = startWordChar + 1;
	return message[terminatingCharPos] == validatorTerminated;
}


int main(int argc, char **argv)
{
	mqd_t validatorQueue;
	char buffer[MAX_SIZE];
	char wordBuffer[MAX_SIZE];
	char testerMessage[MAX_SIZE];
	char* validatorQueueName = "/validatorReceive";
	//int testerPID = getpid();

    /* open the mail queue */
    validatorQueue = mq_open(validatorQueueName, O_WRONLY);
    //CHECK((mqd_t)-1 != mq);
	Queue checkedWordQueue;	
	checkedWordQueue.head = checkedWordQueue.tail = NULL;


	//kolejka testera
	mqd_t testerQueueDesc;
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;	
	char testerQueueName[MAX_QUEUE_NAME];
	setQueueName(testerQueueName);
    testerQueueDesc = mq_open(testerQueueName, O_CREAT | O_RDONLY, 0644, &attr);
	int ret;
	
	/*

7 2 2 0 1
0
1
0 a 1
0 b 0
1 a 0
1 b 1
*/


    //printf("Send to server (enter \"exit\" to stop it):\n");

	//TODO zrob wczytywanie z stdin i wysylaj do validatora, nie czekajac jescez na odpowiedz
    while (fgets(buffer, MAX_SIZE, stdin)) {
		buffer[strlen(buffer)-1] = '\0';   
		//printf("odczytana linia: %s\n", buffer);
        /* send the message */
		setMessage(buffer, testerMessage);
		if(!isMessageTerminating(testerMessage)) numberOfSendRequest++;
        mq_send(validatorQueue, testerMessage, MAX_SIZE, 0);
		


		memset(buffer, 0, MAX_SIZE);	
		ret = mq_receive(testerQueueDesc, buffer, MAX_SIZE, NULL);
		printf("received message: %s \n", buffer);
		if(ret < 0) printf("tester receive error \n");
		processMessageFromValidator(wordBuffer, &checkedWordQueue, buffer);
		memset(buffer, 0, MAX_SIZE);
    }
	//EOF
	//TODO ZNACZY ZE MAMY EOF WIEC TEZ RAPORT!

    /* cleanup */
    ///CHECK((mqd_t)-1 != mq_close(mq));

    return 0;
}
