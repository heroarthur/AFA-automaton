//DeJa Vu
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/stat.h>        /* SPITFIRE */
#include <mqueue.h>
#include <stdbool.h>

#include "err.h"
#define MAX_PID 60000
#define MAX_SIZE 1024
#define int64 long long int
#define accepted 'T'
#define rejected 'N'
#define messageFromTester 't'
#define messageFromRun 'r'
#define MAX_AUTOMATON_SIZE 2000000
#define terminatePost 2


#define NO_MORE_REQUEST '!'
#define QUEUE_NAME "/tester" 
#define MAX_QUEUE_NAME 50
#define MAX_PID_LEN 12

#define TESTER_REQUEST_SUCCES 1
#define RUN_REQUEST_SUCCES 1



struct state {
	int* transisions[122];
};
typedef struct state State;

struct automaton {
	int N, A, Q, U, F;
	int startState;
	bool acceptState[122];
	State* states;
};
typedef struct automaton Automaton;


const char* pidStart = "@";
const char* wordStart = "$";
const char* wordEnd = "#";
const char* terminateTesterMessage = "T$!#\0";


char* queueName = "/validatorReceive";
int validatorQueueDescriptor;


int queryOfProcessWithPid[MAX_PID];
int acceptedQueryOfProcessWithPid[MAX_PID];
mqd_t testerQueuesDescriptors[MAX_PID];
int runForTesterWithPid[MAX_PID];
bool haveTesterQueueDescriptor[MAX_PID];
int numberOfRun = 0;
bool AUTOMATON_RECEIVE = true;

char* allocatedAutomaton;
//int automatonSize = 50;

int currentNumberOfPendingRun = 0;

int validatorQueries = 0;
int validatorAnswer = 0;
int validatorAcceptQueries = 0;

char pendingWordBuff[MAX_SIZE];
char wordAccepted;
char buffPID[MAX_PID_LEN];
char testerQueueName[MAX_QUEUE_NAME];
char messageToTester[MAX_SIZE];
char messageHeader[MAX_SIZE];


void initializePidArrays() {
	int i;
	for(i = 0; i < MAX_PID; i++) {
		queryOfProcessWithPid[i] = 0;
		acceptedQueryOfProcessWithPid[i] = 0;
		testerQueuesDescriptors[i] = 0;
		runForTesterWithPid[i] = 0;
		haveTesterQueueDescriptor[i] = false;
	}
}



void setQueueName(const char* buffPID, char* name) {
	memset(name, 0, MAX_QUEUE_NAME);
	strcpy(name, QUEUE_NAME);
	strcat(name, buffPID);
}

void setMessageToTester(const char* wordAcceptance, const char* word, char* messageBuff) {
	//T$aaaabbac# 
	char acceptance[0]; acceptance[0] = 'F';
	if(wordAcceptance[0] == 'T') acceptance[0] = 'T';  
	memset(messageBuff, 0, MAX_SIZE);
	strcpy(messageBuff, acceptance);
	strcat(messageBuff, wordStart);
	strcat(messageBuff, word);
	strcat(messageBuff, wordEnd);
}



void findParseCharacters(int* pidStartChar, int* wordStartChar, int* wordEndChar, const char* message, const int size) {
	int i;
	for(i = 0; i < size; i++) {
		if(message[i] == wordStart[0]) *wordStartChar = i;
		if(message[i] == pidStart[0]) *pidStartChar = i;
		if(message[i] == wordEnd[0]) *wordEndChar = i;
	}
}

void copyDataFromMessage(int dataBegin, int dataLen, int maxDataSize, char* data, const char* message) {
	memset(data, 0, maxDataSize);
	strncpy(data, message+dataBegin, dataLen);
}

void strToInt(int* result, const char* strData) {
	*result = atoi(strData);
}


void uniqueFifoName(char* name) {
	char buffPid[MAX_PID_LEN];
	int pid = getpid();
	memset(buffPid, 0, MAX_PID_LEN);
	memset(name, 0, MAX_QUEUE_NAME);
	sprintf(buffPid, "%d", pid);
	strcpy(name, "/tmp/SuperEurobeat_");
	strcat(name, buffPid);
}


void printAnswerToTesterWithPid(int pid) {
	printf("PID: %d\n", pid);
	printf("Rcd: %d\n", queryOfProcessWithPid[pid]);
	printf("Acc: %d\n", acceptedQueryOfProcessWithPid[pid]);
}

void printValidatorRaport(int x, int y, int z) {
	int i;
	printf("Rcd: %d\n", x);
	printf("Snt: %d\n", y);
	printf("Acc: %d\n", z);
	for(i = 0; i < MAX_PID; i++) {
		if(queryOfProcessWithPid[i] != 0) printAnswerToTesterWithPid(i);
	}
}


void sendTerminateToTester(int pid) {
	mq_send(testerQueuesDescriptors[pid], terminateTesterMessage, strlen(terminateTesterMessage), 0);
	haveTesterQueueDescriptor[pid] = false;
	mq_close(testerQueuesDescriptors[pid]);
}


void closeQueues() {
	if (mq_close(validatorQueueDescriptor)) syserr("Error in close:");
	if (mq_unlink(queueName)) syserr("Error in unlink:");
}

void endValidatorProcess(int exit_value) {
	free(allocatedAutomaton);
	printValidatorRaport(validatorQueries, validatorAnswer, validatorAcceptQueries);
	//TODO pozamykac te kolejki!
	closeQueues();
	exit(exit_value);
}


void closeImmidiatellyAllTesters() {
	int pid;
	for(pid = 0; pid < MAX_PID; pid++) {
		if(haveTesterQueueDescriptor[pid]) {
			sendTerminateToTester(pid);			
		}
	}
	endValidatorProcess(1);	
}


int executeAndWaitForRunEnding(char* messageHeaderBuff) {
	int desc;
	int wrote = 0;
	char file_name[MAX_QUEUE_NAME];
	uniqueFifoName(file_name);
	int messageChunk = 1000;

	if(mkfifo(file_name, 0755)) syserr("Error in mkfifo:");  
	switch (fork()) {                     /* fork*/
		case -1: 
			syserr("Error in fork\n");
		case 0:                                   /* child */
			execlp("./run", "./run", file_name, NULL); /* exec */
      		syserr("Error in execlp\n");    
	default:                                  /* parent */
		break;
	}
   
    desc = open(file_name, O_WRONLY);
    if(desc == -1) syserr("Error in open:");
	
	int wrote_pos = 0;
	int headerSize = strlen(messageHeaderBuff);
	int automatonSize = (int)strlen(allocatedAutomaton);
	wrote = write(desc, messageHeaderBuff, headerSize);
	if (wrote < 0) 
			syserr("Error in write\n");
	while( (wrote = write(desc, allocatedAutomaton+wrote_pos, messageChunk) != 0) ) {
		wrote_pos += messageChunk;
		if(wrote_pos >= automatonSize || wrote == 0) break;
		if (wrote < 0) 
			syserr("Error in write\n");
	}
    if (wait(0) == -1) syserr("Error in wait\n");    
    if(close(desc)) syserr("Error in close\n");    
	exit(1);
	return 1;
}
 

int processRequestWordWithRun(char* messageHeader) {
	
	switch (fork()) {
    case -1: 
      	syserr("Error in fork\n");
   		closeImmidiatellyAllTesters();
	case 0:
		executeAndWaitForRunEnding(messageHeader);
			
	}
	return 1;
}












void setMessageHeaderToRun(const char* pid, const char* word, char* header) {
	memset(header, 0, MAX_SIZE);
	strcpy(header, pid);
	strcat(header, "$");
	strcat(header, word);
	strcat(header, "@");
}


int findSymbolPosition(const char* symbol, const char* buff) {
	int i;
	int len = strlen(buff);
	for(i = 0; i < len; i++) {
		if(symbol[0] == buff[i]) return i; 
	}
	return len;
}





void sendTerminatingMessageToTesters() {
	int pid;
	for(pid = 0; pid < MAX_PID; pid++) {
		if(haveTesterQueueDescriptor[pid] && runForTesterWithPid[pid] == 0) {
			sendTerminateToTester(pid);			
		}
	}
	if(numberOfRun == 0) {
		fflush(stdin);
		endValidatorProcess(0);
	}
}




void validatorReceivedTerminate() {
	AUTOMATON_RECEIVE = false;
	sendTerminatingMessageToTesters();
	
}



bool testerSendTerminatingMessage(const char* message) {
	int startWordChar = findSymbolPosition(wordStart, message);
	return message[startWordChar+1] == NO_MORE_REQUEST;
}


int sentAnswerToTester(const char* wordAcceptance, const char* pid, const char* word, char* queueName, char* messageBuff) {
	int testerPid;
	int ret;
	strToInt(&testerPid, pid);
	mqd_t validatorAswersQueue = testerQueuesDescriptors[testerPid];
	setMessageToTester(wordAcceptance, word, messageBuff);
	ret = mq_send(validatorAswersQueue, messageBuff, strlen(messageBuff), 1);
  	if (ret < 0) {printf("We have written message of sizeee %ld\n", strlen(messageBuff)); return 0;}
	
	if(!AUTOMATON_RECEIVE) printf("czekam juz tylko, z pidem: %d pozostalo %d \n", testerPid, runForTesterWithPid[testerPid]);
	if(!AUTOMATON_RECEIVE && runForTesterWithPid[testerPid] == 0) {
		printf("wysylam terminate z opoznieniem\n");
		sendTerminateToTester(testerPid);
	}
	return TESTER_REQUEST_SUCCES;
}



int updateTestersInformationData(const char* pid) {
	int testerPid;
	strToInt(&testerPid, pid);

	if(!haveTesterQueueDescriptor[testerPid]) {
		mqd_t validatorAswersQueue;

		char queueName[MAX_QUEUE_NAME];
		memset(queueName, 0, MAX_QUEUE_NAME);
		setQueueName(pid, queueName);
		validatorAswersQueue = mq_open(queueName, O_WRONLY);
		if(validatorAswersQueue == -1) {syserr("opening tester queue\n"); return 0;}
	
		testerQueuesDescriptors[testerPid] = validatorAswersQueue;
		haveTesterQueueDescriptor[testerPid] = true;
	}
	return 1;
}




int processFromTester(char* buffPID, char* buffWord, const char* message, const int messageSize) {
	int pidStartChar, wordStartChar, wordEndChar;
	int pidStart, wordStart;
	if(testerSendTerminatingMessage(message)) {
		validatorReceivedTerminate();
		return 1;
	}

	validatorQueries++;
	findParseCharacters(&pidStartChar, &wordStartChar, &wordEndChar, message, messageSize);
	pidStart = pidStartChar+1;
	wordStart = wordStartChar+1;
	int pidLen = wordStartChar - pidStart;
	copyDataFromMessage(pidStart, pidLen, MAX_PID_LEN, buffPID, message);
	int wordLen = wordEndChar - wordStart;
	copyDataFromMessage(wordStart, wordLen, MAX_SIZE, buffWord, message);
	int testerPid;
	strToInt(&testerPid, buffPID);
	queryOfProcessWithPid[testerPid]++;
	setMessageHeaderToRun(buffPID, buffWord, messageHeader);
	runForTesterWithPid[testerPid]++;
	numberOfRun++;
	updateTestersInformationData(buffPID);
	processRequestWordWithRun(messageHeader);
	
	return 1;
}


bool checkIfNoRun() {
	int i;
	for(i = 0; i < MAX_PID; i++) {
		if (runForTesterWithPid[i] != 0) return false;
	}
	return true;
}


//message run -> validator: r@3321$Taaaaabc#
int processFromRun(char* buffPID, char* buffWord, char* answerBuff, const char* message, const int messageSize) {
	//printf("slowo do process od run: %s \n", message);
	if(!AUTOMATON_RECEIVE) printf("otrzymalem wiadomosc od RUN gdy juz nie przyjmuje\n");
	numberOfRun--;
	validatorAnswer++;
	int pidStartChar, wordStartChar, wordEndChar;
	int pidStart, wordStart;//, wordEnd;
	char acceptance[1];
	findParseCharacters(&pidStartChar, &wordStartChar, &wordEndChar, message, messageSize);
	
	pidStart = pidStartChar+1;
	wordStart = wordStartChar+2;
	int pidLen = wordStartChar - pidStart;
	copyDataFromMessage(pidStart, pidLen, MAX_PID_LEN, buffPID, message);
	int wordLen = wordEndChar - wordStart;
	copyDataFromMessage(wordStart, wordLen, MAX_SIZE, buffWord, message);
	int testerPid;
	strToInt(&testerPid, buffPID);
	acceptance[0] = message[wordStartChar+1];
	if(acceptance[0] == 'T') {acceptedQueryOfProcessWithPid[testerPid]++; validatorAcceptQueries++;}
	runForTesterWithPid[testerPid]--;
	char testerQueueName[MAX_QUEUE_NAME];
	setQueueName(buffPID, testerQueueName);
	setMessageToTester(acceptance, buffWord, answerBuff);
	wait(0);
	sentAnswerToTester(acceptance, buffPID, buffWord, testerQueueName, answerBuff);
	if(!AUTOMATON_RECEIVE) printf("inne miejsce???\n");
	if(!AUTOMATON_RECEIVE && numberOfRun == 0) {
		printf("czemu dziala to a nie tamto?: %d \n", checkIfNoRun());
		printf("ostatni run otrzymalem \n");
		fflush(stdin);
		endValidatorProcess(0);
	}
	return 1;
}


int processReceivedMessage(char* buffPID, char* buffWord, const char* message, const int messageSize) {
	if(message[0] == messageFromTester && AUTOMATON_RECEIVE) {
		return processFromTester(buffPID, buffWord, message, messageSize);}
	if(message[0] == messageFromRun) {return processFromRun(buffPID, buffWord, messageToTester, message, messageSize);}
	return 1;
}


void allocate_buffer(char** buffer, int size) {
	*buffer = malloc(sizeof(char) * size);
	memset(*buffer, 0, size);
}


void freeAllocatedAutomaton(char** buffer) {
	free(*buffer);
}


void saveToAutomatonHeader(Automaton* automaton, int header_index, int val) {
	if(header_index == 0) automaton->N = val;
	if(header_index == 1)	automaton->A = val;
	if(header_index == 2)	automaton->Q = val;
	if(header_index == 3)	automaton->U = val;
	if(header_index == 4)	automaton->F = val;
}

void load_automaton_N(Automaton* automaton, char* buffer) {
	int val;
	int i = 0;
	int str_int_pos = 0;
	char last = ' ';
	char str_int[50];
	bool startNextInt = true;
	while(last != '\n') {
		last = buffer[i++];		
		if(startNextInt) {
			str_int_pos = 0;
			memset(str_int, 0, 50);
			str_int[str_int_pos++] = last;
			startNextInt = false;
		}
		else {
			if(last == ' ' || last == '\n') {
				val = atoi(str_int);
				automaton->N = val;
				return;
				if(last == '\n') {
					return;
				}
				startNextInt = true;
			}
			else {
				str_int[str_int_pos++] = last;
			}
		}
	}
}


void readAutomatonFromStdin(char* automatonBuffer) {
	Automaton automaton;
	char line[MAX_SIZE];
	memset(line, 0, MAX_SIZE);
	fgets(line, MAX_SIZE, stdin);
	load_automaton_N(&automaton, line);

	int linesToRead = automaton.N - 1;
	int i;
	strcpy(automatonBuffer, line);
	for(i = 0; i < linesToRead; i++) {
		memset(line, 0, MAX_SIZE);
		fgets(line, MAX_SIZE, stdin);
		strcat(automatonBuffer, line);		
	}
	automatonBuffer[strlen(automatonBuffer)] = '#';
	//printf("automaton: %s \n",automatonBuffer);
}






int main () {

	allocate_buffer(&allocatedAutomaton, MAX_AUTOMATON_SIZE);
	readAutomatonFromStdin(allocatedAutomaton); 	//TODO wczytaj automat OK/ TODO testuj wczytywanie
	//printf("DLACZGO SPITFIRE NIE LECI?!?!\n");

	mqd_t desc;
    struct mq_attr attr;
	int buff_size = MAX_SIZE + 1;
    char buff[MAX_SIZE + 1];
	int ret;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;	

    desc = mq_open(queueName, O_CREAT | O_RDONLY, 0644, &attr);
	validatorQueueDescriptor = desc;

	if(desc == (mqd_t) -1)
      syserr("Error in mq_open");

	bool ALWAYS_RECEIVE = true;
	while(ALWAYS_RECEIVE) {
		ret = mq_receive(desc, buff, buff_size, NULL);
		if (ret < 0) syserr("Error in rec: ");
		processReceivedMessage(buffPID, pendingWordBuff, buff, ret);
	}
	if (mq_close(desc)) syserr("Error in close:");
	if (mq_unlink(queueName)) syserr("Error in unlink:");	
    return 0;
}
