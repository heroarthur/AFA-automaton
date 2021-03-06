#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <sys/prctl.h>

#include "err.h"


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

#define MAX_AUTOMATON_SIZE 1000000

//message validator -> tester: T$aaaabbac#  
//message tester -> validator: t@3321$aaaabbc# albo T$!#
//message validator -> run 3321$aaaaabbc@automat#
//message run -> validator: r@3321$Taaaaabc#


const char* wordStart = "$";
const char* automatonStartChar = "@";
const char* wordEnd = "#";


const char* wordAccepted = "T";
const char* wordRejected = "F";


int queueDescriptor;

enum Validation {Succes = 0, Failed = 1};

const int MAX_PROCESS_LIMIT = 16;
const int MAX_NUMBER_OF_STATES = 100;
const int MAX_SIZE_OF_AUTOMATON_ALPHABET = 26;

int processNumber = 1;
pthread_mutex_t processNumberLock;


struct state {
	int* transisions[122];
};
typedef struct state State;



const int Z_ASCII = 122;
struct automaton {
	int N, A, Q, U, F; 
	int startState;
	bool acceptState[122];
	State* states;
};
typedef struct automaton Automaton;
Automaton automaton; 

struct checked_word {
	char W[100];
	int len;
};
typedef struct checked_word Word;



char W[MAX_SIZE];
int len;

struct thread_args {
	int wordStart;
	int state;
};
typedef struct thread_args Thread_args;
Thread_args thread_arguments[100];

char *INPUT;
int numberOfTransitions[100][122]; //with state and sign [state][sign] 
pthread_t threads[100];



bool acceptArray[1000][100];






int initSynchronizationVariables() {
	if (pthread_mutex_init(&processNumberLock, NULL) != 0)
    {
        return Failed;
    }	
	return Succes;
}


bool canCreateNewProcess() {
	bool canCreate = false;	
	pthread_mutex_lock(&processNumberLock);
	canCreate = false;
	if(canCreate) processNumber++;
    pthread_mutex_unlock(&processNumberLock);
	return canCreate;
}


void endProcess() {
	pthread_mutex_lock(&processNumberLock);
	processNumber--;
    	pthread_mutex_unlock(&processNumberLock);
}


void allocate_buffer(char** buffer, int size) {
	*buffer = malloc(sizeof(char) * size);
}


void read_automaton_to_buffer(char* buffer, int argc, char *argv[]) {
	int desc, buf_len = 0;
	char readBuffer[MAX_SIZE];  

  if (argc != 2)
    fatal("Usage: %s \n", argv[0]);
  

    queueDescriptor = desc = open(argv[1], O_RDWR);
  if(desc == -1) syserr("Child, error in open:");
  
	//char lastRead = ' ';


	memset(buffer, 0, MAX_AUTOMATON_SIZE);
	memset(readBuffer, 0, MAX_SIZE);
	int numberOfLastRead = 1;
	while(numberOfLastRead != 0) {	
		if ((numberOfLastRead = read(desc, readBuffer, 1000)) == -1) {
			syserr("Error in read\n");
		}
		strcat(buffer, readBuffer);
		if(readBuffer[strlen(readBuffer)-1] == '#') break;
		memset(readBuffer, 0, MAX_SIZE);

		buf_len += numberOfLastRead;
		//lastRead = buffer[buf_len-1];
	}	


  if (buf_len == 0) fatal("Unexpected end-of-file\n");
  if(close(desc)) syserr("Child, error in close\n");
	if(unlink(argv[1])) syserr("Error in unlink:");
}


void saveToAutomatonHeader(Automaton* automaton, int header_index, int val) {
	if(header_index == 0) automaton->N = val;
	if(header_index == 1)	automaton->A = val;
	if(header_index == 2)	automaton->Q = val;
	if(header_index == 3)	automaton->U = val;
	if(header_index == 4)	automaton->F = val;
}


void load_automaton_header(Automaton* automaton, char* buffer) {
	int str_int_pos = 0;
	char str_int[50];
	int val;
	int i = 0;
	int header_index = 0;
	char last = ' ';
	bool startNextInt = true;
	while(last != '#') {
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
				saveToAutomatonHeader(automaton, header_index++, val);
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


void clearNumberOfTransision() {
	int state, sign_offset;
	for(state = 0; state < MAX_NUMBER_OF_STATES; state++) {
		for(sign_offset = 0; sign_offset < MAX_SIZE_OF_AUTOMATON_ALPHABET; sign_offset++) {
			numberOfTransitions[state]['a'+sign_offset] = 0;
		}
	}
}


void clearAcceptArray() {
	int index, state;
	for(index = 0; index < 1000; index++) {
		for(state = 0; state < MAX_NUMBER_OF_STATES; state++) {
			acceptArray[index][state] = false;
		}
	}
}


void printNumberOfTransision() {
	int state, sign_offset, t;
	for(state = 0; state < MAX_NUMBER_OF_STATES; state++) {
		for(sign_offset = 0; sign_offset < MAX_SIZE_OF_AUTOMATON_ALPHABET; sign_offset++) {
			t = numberOfTransitions[state]['a'+sign_offset];
			printf("| %d ", t);
		}
	}
}


int findNthLine(int n, char* buff) {
	int i = 0;
	int current_line = 1;
	char last = ' ';
	if(n == 1) return i;	
	while(last != '#') {
		last = buff[i++];
		if(last == '\n') {
			current_line++;
			if(n == current_line) return i;
		}
	}
	return i;
}


int findFirstLineWithTransisions(Automaton* automaton, char* buff) {
	int line = 4;
	return line;
}


bool alphabetSign(char s) {
	return (int)'a' <= (int)s && (int)s <= 'z'; 
}


void getNumber(int* val, int* numberBegin, char* buff) {
	char str_int[3];
	memset(str_int, 0, 3);
	str_int[0] = buff[(*numberBegin)+0];
	char next = buff[(*numberBegin)+1];
	if(next != ' ' && next != '\n') str_int[1] = next;
	*val = atoi(str_int);
}


void nextNumber(int* val, int* numberBegin, int* numberEnd, char* buff) {
	char str_int[3];
	memset(str_int, 0, 3);

	*numberEnd = (*numberBegin)+2;
	str_int[0] = buff[(*numberBegin)+0];
	char next = buff[(*numberBegin)+1];
	if(next != ' ' && next != '\n') {str_int[1] = next; (*numberEnd)++;}
	*val = atoi(str_int);
}


void giveTransisionsInLine(int* lineBegin, int* nextLine, int* state, char* sign, int* transisions, char* buff) {
	int i = *lineBegin;
	char last = '@';
	int spaces = 0;

	getNumber(state, lineBegin, buff);
	while(last != '#') {
		last = buff[i++];
		if(alphabetSign(last)) {*sign = last; continue;}
		if(last == ' ')	spaces++;	
		if(last == '\n')	{*transisions=spaces-1; *nextLine=i; return;} 
	}
}


void load_transision_line(int* beginLine, int* nextLine, Automaton* automaton, char* buff) {
	int state;
	int nextState;
	char sign;
	int j = 0;
	nextNumber(&state, beginLine, nextLine, buff);
	sign = buff[(*nextLine)];
	(*nextLine) += 2;
	int *transisions = automaton->states[state].transisions[(int)sign];
	while(buff[*nextLine-1] != '\n') {
		*beginLine = *nextLine;
		nextNumber(&nextState, beginLine, nextLine, buff);
		transisions[j++] = nextState;
	}
}


void updateNumberOfTransisions(Automaton* automaton, char* buff) {
	int firstTransision = findFirstLineWithTransisions(automaton, buff);
	int begin = findNthLine(firstTransision, buff);
	int next;
	int state;
	char sign;
	int transisions;
	while(buff[begin] != '#') {
		giveTransisionsInLine(&begin, &next, &state, &sign, &transisions, buff);
		numberOfTransitions[state][(int)sign] = transisions;
		begin = next;
	}	
}


void allocate_automaton(int Q, Automaton* automaton) {
	automaton->states = malloc(Q*sizeof(State));
	int state, sign_offset;
	int transisions;
	char sign;
	for(state = 0; state < Q; state++) {
		for(sign_offset = 0; sign_offset < MAX_SIZE_OF_AUTOMATON_ALPHABET; sign_offset++) {
			sign = 'a'+sign_offset;

			transisions = numberOfTransitions[state][(int)sign];
			if(transisions != 0) {
				automaton->states[state].transisions[(int)sign] = malloc(transisions*sizeof(int));
			}
		}	
	}
}


void load_start_state(Automaton* automaton, char* buff) {
	int start_state;
	int startLineBegin = findNthLine(2, buff);
	int nextLine;
	nextNumber(&start_state, &startLineBegin, &nextLine, buff);
	automaton->startState = start_state;
}


void load_accept_states(Automaton* automaton, char* buff) {
	int acceptLineBegin = findNthLine(3, buff);
	int nextLine;
	int state;
	int i;
	int acceptStates = automaton->F;
	for(i = 0; i < acceptStates; i++) {
		nextNumber(&state, &acceptLineBegin, &nextLine, buff);
		automaton->acceptState[state] = true;
		acceptLineBegin = nextLine;
	}
}


void load_transisions(Automaton* automaton, char* buff) {
	int line = findFirstLineWithTransisions(automaton, buff);
	int transisionBegin = findNthLine(line, buff);
	int transisionLines = automaton->N - 3;


	int begin, next;
	begin = transisionBegin;

	for(int i = 0; i < transisionLines; i++) {
			load_transision_line(&begin, &next, automaton, buff);
			begin = next;
	}
}


void load_automaton(Automaton* automaton, char* buff) {//medium	
	load_automaton_header(automaton, buff);
	load_start_state(automaton, buff); //TODO!
	load_accept_states(automaton, buff);
	updateNumberOfTransisions(automaton, buff);
	allocate_automaton(automaton->Q, automaton);
 	load_transisions(automaton, buff);	
}


bool isUniversal(Automaton* automaton, int state) {//TO DO! inline 
	return state <= automaton->U - 1;
}


void allocateArray(bool** array, int size) {
	*array = malloc(sizeof(bool) * size);
}


bool leaf(int transisions) {return transisions == 0;}


void print_automat(Automaton* automaton) {
	printf("\n\n\nAutomat header: \n");
	printf("| N %d | A %d | Q %d | U %d | F %d |\n\n",automaton->N, automaton->A, automaton->Q, automaton->U, automaton->F);
	printf("Start State: %d \n", automaton->startState);
	printf("Accept States:\n");
	int i;
	bool t;
	for(i = 0; i < 100; i++) {
		t = automaton->acceptState[i];
		if(t) printf("%d |", i);
							
	}
	printf("\n\nTRANSISIONS: \n");
	int state, sign_offset;
	int transisions;
	int val;
	char sign;
	for(state = 0; state < MAX_NUMBER_OF_STATES; state++) {
		for(sign_offset = 0; sign_offset < MAX_SIZE_OF_AUTOMATON_ALPHABET; sign_offset++) {
			sign = 'a'+sign_offset;			
			transisions = numberOfTransitions[state][(int)sign];
			if(transisions != 0) printf("STATE %d, SIGN %c: ", state, sign);
			for(i = 0; i < transisions; i++) {
				printf("i VALUE: %d \n", i);
				val = automaton->states[state].transisions[(int)sign][i];				
				printf("%d | ", val);
			}
			if(transisions != 0) printf("\n");
		}	
	}

	printf("\n\n");
}


void* set_acceptArray(void* argPtr) {	
	Thread_args* args = (Thread_args*) argPtr;
	char sign = W[args->wordStart];
	int transisions = numberOfTransitions[args->state][(int)sign];

	if(args->wordStart == len) {acceptArray[args->wordStart][args->state] = automaton.acceptState[args->state]; return NULL;}
	if(leaf(transisions)) {acceptArray[args->wordStart][args->state] = isUniversal(&automaton, args->state); return NULL;}

	int* transisionStates = automaton.states[args->state].transisions[(int)sign];
	int i;
	acceptArray[args->wordStart][args->state] = false;
	if(isUniversal(&automaton, args->state)) { //uniwersalny
		for(i = 0; i < transisions; i++) {
			if(!acceptArray[args->wordStart+1][transisionStates[i]]) {
				return NULL;
			}
		}
		acceptArray[args->wordStart][args->state] = true;
		return NULL;
	}

	//egzystencalny
	for(i = 0; i < transisions; i++) {
		if(acceptArray[args->wordStart+1][transisionStates[i]]) {
			acceptArray[args->wordStart][args->state] = true;
			return NULL;
		}
	}
	return NULL;
}


void joinThreads(int count) {
	int i;	
	for(i = 0; i < count; i++) {
		pthread_join(threads[i], NULL); 
	}
}


void acceptAllStates(int wordStart, Automaton* automaton) {
	int state;
	int iret;
	for(state = 0; state < automaton->Q; state++) {
		thread_arguments[state].wordStart = wordStart;
		thread_arguments[state].state = state;
		
		bool tryPthread = true;
		while(tryPthread) {
			iret = pthread_create(&threads[state], NULL, set_acceptArray, (void*) &thread_arguments[state]);
			if(iret)
			{
				continue;
				//fprintf(stderr,"Error - pthread_create() return code: %d\n",iret);
				//exit(EXIT_FAILURE);
			}
			else {break;}
		}
	}
	
	joinThreads(automaton->Q);
}


bool acceptWord(Automaton* automaton) {
	int wordStart;
	for(wordStart = len; wordStart >= 0; wordStart--) {
		acceptAllStates(wordStart, automaton);
	}
	return acceptArray[0][automaton->startState];
}


int findSymbolPosition(const char* symbol, const char* buff) {
	int i;
	int len = strlen(buff);
	for(i = 0; i < len; i++) {
		if(symbol[0] == buff[i]) return i; 
	}
	return len;
}


void copyDataFromMessage(int dataBegin, int dataLen, int maxDataSize, char* data, const char* message) {
	memset(data, 0, maxDataSize);
	strncpy(data, message+dataBegin, dataLen);
}


void strToInt(int* result, char* strData) {
	*result = atoi(strData);
}


void readPID(char* pid, const char* buff) {
	int pidBegin = 0;
	memset(pid, 0, MAX_PID_LEN);
	int pidLen = findSymbolPosition(wordStart, buff);
	copyDataFromMessage(pidBegin, pidLen, MAX_PID_LEN, pid, buff);
}



void readWordToProcess(char* word, const char* buff) {
	int wordStartChar = findSymbolPosition(wordStart, buff);
	int automatonStart = findSymbolPosition(automatonStartChar, buff);
	memset(word, 0, MAX_SIZE);
	int wordLen = automatonStart - wordStartChar -1;
	len = wordLen;
	copyDataFromMessage(wordStartChar+1, wordLen, MAX_SIZE, word, buff);
}



void setAnswerToValidator(bool wordAcceptance, char* pid, char* word, char* answerBuff) {
	char accept[1];
	accept[0] = 'F';
	if(wordAcceptance) accept[0] = 'T';
	memset(answerBuff, 0, MAX_SIZE);
	strcpy(answerBuff, "r");
	strcat(answerBuff, automatonStartChar);
	strcat(answerBuff, pid);
	strcat(answerBuff, wordStart);
	strcat(answerBuff, accept);
	strcat(answerBuff, word);
	strcat(answerBuff, wordEnd);
}


int sendAnswerToValidator(bool wordAcceptance, char* pid, char* word, char* answerBuff) {
	setAnswerToValidator(wordAcceptance, pid, word, answerBuff);	
	mqd_t validatorQueue;
	char* validatorQueueName = "/validatorReceive";
	int ret;
    validatorQueue = mq_open(validatorQueueName, O_WRONLY);
	if(validatorQueue < 0) return 0;
	ret = mq_send(validatorQueue, answerBuff, MAX_SIZE, 0);
	if(ret < 0) return 0;
	return 1;
}



/*void printToFileAutomaton(char* buffer) {
	FILE *f = fopen("run_automaton.txt", "w");
	if (f == NULL)
	{
	    printf("Error opening file!\n");
	    exit(1);
	}
	fprintf(f, "%s", buffer);
}*/

int main(int argc, char *argv[])
{
	prctl(PR_SET_PDEATHSIG, SIGUSR1);
	char buffPid[MAX_PID_LEN];
	char answerToValidator[MAX_SIZE];
	allocate_buffer(&INPUT, MAX_AUTOMATON_SIZE);
	clearNumberOfTransision();
	clearAcceptArray();
	read_automaton_to_buffer(INPUT, argc, argv);
	//printf("AUTOMAT:\n\n %s",INPUT);
	//printToFileAutomaton(INPUT);
	

	readPID(buffPid, INPUT);
	readWordToProcess(W, INPUT);
	int automatonStart = findSymbolPosition(automatonStartChar, INPUT)+1;
	load_automaton(&automaton, INPUT+automatonStart);
	
	//print_automat(&automaton);
	
	bool wynik = acceptWord(&automaton);
	if(!sendAnswerToValidator(wynik, buffPid, W, answerToValidator)) printf("send answer to validator ERROR\n");	
	
  return 0;
}
