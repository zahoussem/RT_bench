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

void *rtRepCode(void *arg)
{
  struct repetitive *params = (struct repetitive *) arg;                                   	
  int index = params->index;	
  int priority = params -> priority;
  int cpu = params->priority;
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
