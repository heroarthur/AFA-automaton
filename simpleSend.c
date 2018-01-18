//fifo_parent.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "err.h"

int main ()
{
  int desc;
  int wrote = 0;
  char *message = "7 2 2 1 1\n0\n0\n0 a 0 1\n0 b 0\n1 a 1\n1 b 0\n#"; //pamietaj ze trzeba bedzie dodac na koncu \n i #   
//5 3 4 0 2_0_0 1 2_0 a 1 2_2 b 0 1 2 3_#
//"5 3 4 0 4\n0\n0 1 2 3\n0 a 3 1\n2 b 3 2 1 0\n#"


//"12 2 5 0 2\n0\n2 4\n0 a 1 3\n1 a 2\n1 b 1\n2 a 1\n2 b 2\n3 a 3\n3 b 4\n4 a 3\n4 b 4\n#"
//"7 2 2 1 1\n0\n0\n0 a 0 1\n0 b 0\n1 a 1\n1 b 0\n#"


	/*
	N A Q U F\n
	q\n
	[q]\n
	[q a r [p]\n]

    N to liczba linii opisu automatu;
    A to rozmiar alfabetu: alfabet to zbiór {a,...,x}, gdzie 'x'-'a' = A-1;
    Q to liczba stanów: stany to zbiór {0,...,Q-1};
    U to liczba stanów uniwersalnych: stany uniwersalne to zbiór {0, .., U-1}, stany egzystencjalne to zbiór {U, .., Q-1};
    F to liczba stanów akceptujących;
    q,r,p to pewne stany;
    a a to pewna litera alfabetu
	*/

	

//"Hello my child! jesze troche paszkwilu \n\ntora\nTAKUMI!!\ni koniec#";
  int message_len = 88;
  char *file_name = "/tmp/fifo_tmp"; /*the path to fifo */
  
  /*crate the fifo's file*/
  //if(unlink(file_name)) syserr("Error in unlink:");     //TUTAJ UNLINK!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if(mkfifo(file_name, 0755)) syserr("Error in mkfifo:");
  
  
  switch (fork()) {                     /* fork*/
    case -1: 
      syserr("Error in fork\n");
   
    case 0:                                   /* child */
 
      printf("I am a child and my pid is %d\n", getpid());      
      
      /*give the fifo's name to the child*/
      execlp("./run", "./run", file_name, NULL); /* exec */
      syserr("Error in execlp\n");
    
    default:                                  /* parent */
      break;
  }
    /*parent*/
    printf("I am a parent and my pid is %d\n", getpid());
    
    /*open fifo*/
    desc = open(file_name, O_WRONLY);
    if(desc == -1) syserr("Error in open:"); /*what with the fifo file?*/
    
    printf("Parent: writing a message\n");

	int ilosc = 1;
    int c = 0;
	for(c = 0; c < ilosc; c++) {
	    wrote = write(desc, message, message_len);
	    if (wrote < 0) 
	      syserr("Error in write\n");
	}
   	printf("UDALO SIE WSYSTKO WYPOISAC \n");
    
    /*wait for the child and remove the fifo*/
    if (wait(0) == -1) syserr("Error in wait\n");
    
    if(close(desc)) syserr("Error in close\n");
      
    if(unlink(file_name)) syserr("Error in unlink:");
    return 0;
}
