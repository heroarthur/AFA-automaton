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


#include "err.h"



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
	int N, A, Q, U, F; //automaton header
	int startState;
	bool acceptState[122];
	State* states;
};
typedef struct automaton Automaton;
Automaton automaton; //definition of automaton

struct checked_word {
	char W[100];
	int len;
};
typedef struct checked_word Word;
//Word word;
char W[100];
int len;

struct thread_args {
	int* accept_i;
	bool* acceptArray;
	int* state;
	int* wordStart;
};
typedef struct thread_args Thread_args;


char *INPUT;
int numberOfTransitions[100][122]; //with state and sign [state][sign] 
/*
1.
-tablica tablic accept[index][state]
-funkcja ktora dla danego indeksu i state obliczy  accept[index][state]
-funkcja ktora wykorzysta poprzednia do obliczenia dla indeksu wszystkich stanow 
-testowanie czy dynamik dziala
*/
bool acceptArray[1000][100];



/*
 N A Q U F\n
q\n
[q]\n
[q a r [p]\n]

gdzie
    N to liczba linii opisu automatu;
    A to rozmiar alfabetu: alfabet to zbiór {a,...,x}, gdzie 'x'-'a' = A-1;
    Q to liczba stanów: stany to zbiór {0,...,Q-1};
    U to liczba stanów uniwersalnych: stany uniwersalne to zbiór {0, .., U-1}, stany egzystencjalne to zbiór {U, .., Q-1};
    F to liczba stanów akceptujących;
    q,r,p to pewne stany;
    a a to pewna litera alfabetu
*/


int initSynchronizationVariables() {
	if (pthread_mutex_init(&processNumberLock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return Failed;
    }	
	return Succes;
}


bool canCreateNewProcess() {
	bool canCreate = false;	
	pthread_mutex_lock(&processNumberLock);
	//canCreate = processNumber < MAX_PROCESS_LIMIT;
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
  //char* buf = INPUT;
  
  if (argc != 2)
    fatal("Usage: %s \n", argv[0]);
  
  printf("Child, trying to open |%s|\n", argv[1]);
  desc = open(argv[1], O_RDWR);
  if(desc == -1) syserr("Child, error in open:");
  
  printf("Child, reading data from descriptor %d\n\n", desc);
	char lastRead = ' ';
	while(lastRead != '#') {
		if ((buf_len += read(desc, buffer+buf_len, 1)) == -1) syserr("Error in read\n");
		lastRead = buffer[buf_len-1];
	}

	buffer[buf_len] = '\0';	
  if (buf_len == 0) fatal("Unexpected end-of-file\n");
  if(close(desc)) syserr("Child, error in close\n");
}


void saveToAutomatonHeader(Automaton* automaton, int header_index, int val) {
	if(header_index == 0) automaton->N = val;
	if(header_index == 1)	automaton->A = val;
	if(header_index == 2)	automaton->Q = val;
	if(header_index == 3)	automaton->U = val;
	if(header_index == 4)	automaton->F = val;
}


void load_automaton_header(Automaton* automaton, char* buffer) {
	int val;
	int i = 0;
	int header_index = 0;
	char last = ' ';
	char str_int[3];
	bool startNextInt = true;
	while(last != '#') {
		last = buffer[i++];		
		if(startNextInt) {
			memset(str_int, 0, 3);
			str_int[0] = last;
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
				str_int[1] = last;
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
	printf("LADUJE TRANZYCJE \n");
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
	//transisions = numberOfTransitions[][];	
	int *transisions = automaton->states[state].transisions[(int)sign];
	while(buff[*nextLine-1] != '\n') {
		*beginLine = *nextLine;
		nextNumber(&nextState, beginLine, nextLine, buff);
		transisions[j++] = nextState;
		//automaton->states[state].transisions[sign][j++] = nextState; TODO
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
		printf("uzyskano: state: %d, sign: %c, tr: %d \n", state, sign, transisions);
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




//acceptArray[i][state] oznacza ze prefiks zaczynajacy sie od litery na i-tej pozycji i stanie poczatkowym state jest akceptowane
/*
dla slowa dlugosci 5 to acceptArray[4][state] sprawdzi dla acceptArray[5][state] - czyli puste slowo i zalezy tylko od akceptacji stanu

*/


void set_acceptArray(int wordStart, int state, int transisions) {
	char sign = W[wordStart];

	if(wordStart == len) {acceptArray[wordStart][state] = automaton.acceptState[state]; return;}
	if(leaf(transisions)) {acceptArray[wordStart][state] = isUniversal(&automaton, state); return;}

	int* transisionStates = automaton.states[state].transisions[(int)sign];
	int i;
	acceptArray[wordStart][state] = false;
	if(isUniversal(&automaton, state)) { //uniwersalny
		for(i = 0; i < transisions; i++) {
			if(!acceptArray[wordStart+1][transisionStates[i]]) {
				return;
			}
		}
		acceptArray[wordStart][state] = true;
		return;
	}

	//egzystencalny
	for(i = 0; i < transisions; i++) {
		if(acceptArray[wordStart+1][transisionStates[i]]) {
			acceptArray[wordStart][state] = true;
			return;
		}
	}
}

void acceptAllStates(int wordStart, Automaton* automaton) {
	int state;
	char sign = W[wordStart];
	int transisions;
	for(state = 0; state < automaton->Q; state++) {
		transisions = numberOfTransitions[state][(int)sign];
		set_acceptArray(wordStart, state, transisions);//TODO nie przekazuj tranzycji przez parametr bo dla 0 tez to trzeba obliczyc wiec i tak odpalisz set_acceptArray		
	}
}


bool acceptWord(Automaton* automaton) {
	int wordStart;
	for(wordStart = len; wordStart >= 0; wordStart--) {
		acceptAllStates(wordStart, automaton);
	}
	return acceptArray[0][automaton->startState];
}


int main(int argc, char *argv[])
{
	//TESTOWANE SLOWO
	

	printf("teraz go alokuje \n");
//ababababababababababababababab
//abababababababababababababababababababababababababab 52
	printf("przed wczytaniem\n");
	print_automat(&automaton);
	char* slowo = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab"; 
	len = 67;
	for(int k = 0; k < len; k++) {
		W[k] = slowo[k];
	}

	//printNumberOfTransision();
	allocate_buffer(&INPUT, 2000000);
	clearNumberOfTransision();
	clearAcceptArray();
	read_automaton_to_buffer(INPUT, argc, argv);
	load_automaton(&automaton, INPUT);//TO DO! testy przesylania i wczytywania automatow
	automaton.startState = 0;
	//sprawdz szczegolne przypadki
	//napisz wczytywanie akceptujacego
	
	//zaprojektuj pthread do akceptowania slowa, prosta przejrzysta konstrukcja, rekurencja dozwolona (pozniej moze derekurencja)
	//napisz 70%


	print_automat(&automaton);
	printf("\nAUTOMAT WYPISANY\n");
	
	//int size = 1;
	//int startState = 0;

	printf("set accept start \n");
	//int index = 0;
	//int wordStart = 0;

	bool wynik = acceptWord(&automaton);

	printf("zakonczono sprawdzenie :)\n");
	if(wynik) printf("slowo AKCEPTOWANE przez automat \n");
	else printf("slowo NIE-akceptowane przez automat \n");
	
	
  return 0;
}
