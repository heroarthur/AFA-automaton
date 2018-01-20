#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <mqueue.h>
#include <unistd.h>

#define MAX_PID 32768
#define MAX_SIZE 1024
#define int64 long long int
#define accepted 'T'
#define rejected 'N'



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


//implementacja kolejki gdzie lokowane sa odebrane od validatora elementy
typedef struct node {
    int val;
    struct node* next;
} node_t;



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

int main(int argc, char **argv)
{
	mqd_t validatorQueue;
	char buffer[MAX_SIZE];
	char testerMessage[MAX_SIZE];
	char* validatorQueueName = "/validatorReceive";
	//int testerPID = getpid();

    /* open the mail queue */
    validatorQueue = mq_open(validatorQueueName, O_WRONLY);
    //CHECK((mqd_t)-1 != mq);



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
	
	


    //printf("Send to server (enter \"exit\" to stop it):\n");

	//TODO zrob wczytywanie z stdin i wysylaj do validatora, nie czekajac jescez na odpowiedz
    while (fgets(buffer, MAX_SIZE, stdin)) {
		buffer[strlen(buffer)-1] = '\0';   
		//printf("odczytana linia: %s\n", buffer);
        /* send the message */
		setMessage(buffer, testerMessage);
        mq_send(validatorQueue, testerMessage, MAX_SIZE, 0);
		


		memset(buffer, 0, MAX_SIZE);	
		ret = mq_receive(testerQueueDesc, buffer, MAX_SIZE, NULL);
		if(ret < 0) printf("tester receive error \n");
		printf("tester odebral: %s \n", buffer);
		memset(buffer, 0, MAX_SIZE);
    }
	//EOF

    /* cleanup */
    ///CHECK((mqd_t)-1 != mq_close(mq));

    return 0;
}
