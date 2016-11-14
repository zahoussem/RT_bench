//#ifndef __USE_GNU
//#define __USE_GNU
//#endif
//#define _GNU_SOURCE
#include <sched.h>
#include <iostream>
#include <time.h> //used to call clock_gettime
#include <math.h> // used for the floor operations
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
using namespace std;




/********************************************************
*   Written by: Houssam Eddine ZAHAF                  	*
*  contact:  Houssam-eddine.zahaf@univ-lille1.fr       	*
*            zahaf.houssem@edu.univ-oran1.fr	       	*		
*   This code allow to repete a task with real time     *
*   Priorities usage : [executableName] -p [priority] -i*
*   [NumberOfrepetinion] [applicationSpecificParameters]*   							*
********************************************************/
struct repetitive {
  int index; 
  int priority;
  int proc;
  int itSize;
  string label;
};

//add compare time spec
int comp_spec(struct timespec a, struct timespec b){
  if (a.tv_sec > b.tv_sec) return 1;
  else if (a.tv_sec < b.tv_sec) return 0;
  else if (a.tv_nsec > b.tv_nsec) return 1;
  else if (a.tv_nsec == b.tv_nsec) return 1;
  else return 0; 
}

// add to spectimes
struct timespec add_spec(struct timespec End, struct timespec Begin)
{
  long a_nsec = End.tv_nsec + Begin.tv_nsec;
  long a_sec  = End.tv_sec  + Begin.tv_sec;
  if (a_nsec > 1000000000 ) {
    a_sec = a_sec  + 1 ; 
    a_nsec= a_nsec - 1000000000  ;
  }
  struct timespec add; 
  add.tv_sec = a_sec; 
  add.tv_nsec = a_nsec;
  return (add);
}
// differance between two spectimes
struct timespec diff_spec(struct timespec End, struct timespec Begin)
{
  long d_nsec = End.tv_nsec - Begin.tv_nsec;
  long d_sec  = End.tv_sec  - Begin.tv_sec;
  if (d_nsec < 0 ) {
    d_sec = d_sec  - 1 ; 
    d_nsec= 1000000000 + d_nsec;
  }
  struct timespec diff; 
  diff.tv_sec = d_sec; 
  diff.tv_nsec = d_nsec;
  return (diff);
}

// convert spectime to time in second
double toSec(struct timespec t){
  return (double) (t.tv_sec) + (t.tv_nsec)/(double) 1000000000;
} 

// convert time in second to spectime
struct timespec toSpec (double s){
  long s_sec = (long)(floor(s));
  long s_nsec = (s  - floor(s)) * 1000000000;
  struct timespec  returned;
  returned.tv_sec = s_sec;
  returned.tv_nsec= s_nsec;
  return returned;
}
#define NUM_NODES                          100
#define NONE                               9999

struct _NODE
{
  int iDist;
  int iPrev;
};
typedef struct _NODE NODE;

struct _QITEM
{
  int iNode;
  int iDist;
  int iPrev;
  struct _QITEM *qNext;
};
typedef struct _QITEM QITEM;

QITEM *qHead = NULL;

             
             
             
int AdjMatrix[NUM_NODES][NUM_NODES];

int g_qCount = 0;
NODE rgnNodes[NUM_NODES];
int ch;
int iPrev, iNode;
int i, iCost, iDist;


void print_path (NODE *rgnNodes, int chNode)
{
  if (rgnNodes[chNode].iPrev != NONE)
    {
      print_path(rgnNodes, rgnNodes[chNode].iPrev);
    }
  printf (" %d", chNode);
  fflush(stdout);
}


void enqueue (int iNode, int iDist, int iPrev)
{
  QITEM *qNew = (QITEM *) malloc(sizeof(QITEM));
  QITEM *qLast = qHead;
  
  if (!qNew) 
    {
      fprintf(stderr, "Out of memory.\n");
      exit(1);
    }
  qNew->iNode = iNode;
  qNew->iDist = iDist;
  qNew->iPrev = iPrev;
  qNew->qNext = NULL;
  
  if (!qLast) 
    {
      qHead = qNew;
    }
  else
    {
      while (qLast->qNext) qLast = qLast->qNext;
      qLast->qNext = qNew;
    }
  g_qCount++;
  //              ASSERT(g_qCount);
}


void dequeue (int *piNode, int *piDist, int *piPrev)
{
  QITEM *qKill = qHead;
  
  if (qHead)
    {
      //                 ASSERT(g_qCount);
      *piNode = qHead->iNode;
      *piDist = qHead->iDist;
      *piPrev = qHead->iPrev;
      qHead = qHead->qNext;
      free(qKill);
      g_qCount--;
    }
}


int qcount (void)
{
  return(g_qCount);
}

int dijkstra(int chStart, int chEnd) 
{
  for (ch = 0; ch < NUM_NODES; ch++)
    {
      rgnNodes[ch].iDist = NONE;
      rgnNodes[ch].iPrev = NONE;
    }
  
  if (chStart == chEnd) 
    {
      printf("Shortest path is 0 in cost. Just stay where you are.\n");
    }
  else
    {
      rgnNodes[chStart].iDist = 0;
      rgnNodes[chStart].iPrev = NONE;
      enqueue (chStart, 0, NONE);
      while (qcount() > 0)
	{
	  dequeue (&iNode, &iDist, &iPrev);
	  for (i = 0; i < NUM_NODES; i++)
	    {
	      if ((iCost = AdjMatrix[iNode][i]) != NONE)
		{
		  if ((NONE == rgnNodes[i].iDist) || 
		      (rgnNodes[i].iDist > (iCost + iDist)))
		    {
		      rgnNodes[i].iDist = iDist + iCost;
		      rgnNodes[i].iPrev = iNode;
		      enqueue (i, iDist + iCost, iNode);
		    }
		}
	    }
	}
    }
}

void shortest() {
  int i,j,k;
  /* finds 10 shortest paths between nodes */
  for (i=0,j=NUM_NODES/2;i<100;i++,j++) {
    j=j%NUM_NODES;
    dijkstra(i,j);
  }
}


void *rtRepCode(void *arg)
{
  struct repetitive *params = (struct repetitive *) arg;                                   	
  int index = params->index;	
  int priority = params -> priority;
  int cpu = params->proc;
  int itSize = params->itSize;
  string label = params->label;
  cpu_set_t cpuset; 
  CPU_ZERO(&cpuset);     
  CPU_SET( cpu , &cpuset); 
  //pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  sched_setaffinity(0, sizeof(cpuset), &cpuset);
  int it=0;
  struct timespec b,e;
  for (it =0 ; it< itSize ; it++ ){
    clock_gettime(CLOCK_REALTIME, &b);
    //call my rt function here
    shortest();
    clock_gettime(CLOCK_REALTIME, &e);
    cout<<label<<"\t"<<toSec(diff_spec(e,b))<<endl;
  }
  return NULL;
}




int main(int argc, char ** argv)
{
  if(argc < 9){
    cout<<"usage : [executableName] -p [priority] -i [NumberOfrepetinion]  -proc [processorID] -s [processingLabel] [applicationSpecificParameters]"<<endl; 
    exit(1);
  }
  int i,j,k;
  FILE *fp;
  /* open the adjacency matrix file */
  fp = fopen ("input.dat","r");
  /* make a fully connected matrix */
  for (i=0;i<NUM_NODES;i++) {
    for (j=0;j<NUM_NODES;j++) {
      /* make it more sparce */
      fscanf(fp,"%d",&k);
      AdjMatrix[i][j]= k;
    }
  }
  struct repetitive rep;
  rep.index = 0;
  rep.priority=atoi(argv[2]);
  rep.proc=atoi(argv[6]);
  rep.itSize=atoi(argv[4]);
  rep.label=argv[8];
  pthread_t th;
  pthread_attr_t myattr;
  struct sched_param myparam;
  pthread_attr_init(&myattr);  
  pthread_attr_setschedpolicy(&myattr, SCHED_FIFO);
  myparam.sched_priority = rep.priority;
  pthread_attr_setschedparam(&myattr, &myparam);   
  pthread_create(&th, &myattr, rtRepCode, &rep); 
  if(pthread_join(th, NULL)) {
    cout<<"Error joining thread"<<endl;
    return 2;
  }
  return (0);
}
