#include "cs537.h"
#include "request.h"

// 
// server.c: A very, very simple web server
//
// To run:
//  server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// CS537: Parse the new arguments too


// Structure for holding requests, can be scaled for xtra credit
typedef struct request{
  int cfd;
  long reqSize; // Added to hold file size for sorting
} Req;

Req **reqBuffer;

// Create mutex and CVs for threads
pthread_mutex_t tLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tFill = PTHREAD_COND_INITIALIZER;
pthread_cond_t tEmpty = PTHREAD_COND_INITIALIZER;

// Thread buffer
pthread_t **cBuffer;

// Global variables used to keep track of unserved requests
int reqNum, reqIndexFill, reqIndexServe, bs;

void getargs(int *port, int *nThreads, int *bufferSize, int argc, char *argv[])
{
  if (argc != 4) {
    fprintf(stderr, "Usage: %s <port> <threads> <buffers>\n", argv[0]);
    exit(1);
  }
  *port = atoi(argv[1]);
  
  *nThreads = atoi(argv[2]);
  if(*nThreads <= 0){
    fprintf(stderr, "<threads> must be a positive integer\n");
    exit(1);
  }

  *bufferSize = atoi(argv[3]);
  if(*nThreads <= 0){
    fprintf(stderr, "<buffers> must be a positive integer\n");
    exit(1);
  }

}


/* 
** Used for qsort() function. 
** Compares the reqSize of two requests in ReqBuffer
*/
int reqSizeComp(const void *f, const void *s){
  int comp;

  comp = ((*(Req**) f)->reqSize - (*(Req**) s)->reqSize);

  return comp;
}


/*
** Consumer function that serves the server request
*/
void *serveRequest(void *arg){
  
  while(1){
    pthread_mutex_lock(&tLock);

    while(reqNum == 0){
      pthread_cond_wait(&tFill, &tLock);
    }

    Req *sReq;

    /*    //     FIFO implementation
    sReq = reqBuffer[reqIndexServe];
    reqIndexServe = (reqIndexServe + 1) % bs;
    */

    /* Smallest First implementation - doesn't work correctly */
    // I think the probablem has to do with grabbing the file size... but idk
    // Assuming array is sorted, grab first element
    sReq = reqBuffer[0];
    
    // Replace first element with last and sort again
    reqBuffer[0] = reqBuffer[reqIndexFill - 1];
    if(reqIndexFill > 1){
      qsort(reqBuffer, reqIndexFill, sizeof(*reqBuffer), reqSizeComp);
    }
    reqIndexFill--;

    
    //requestHandle(sReq->cfd);
    //Close(sReq->cfd);

    reqNum--;
    
    pthread_cond_signal(&tEmpty);
    pthread_mutex_unlock(&tLock);
    
    requestHandle(sReq->cfd);
    Close(sReq->cfd);
  }

  return NULL;
}


int main(int argc, char *argv[])
{
  int listenfd, connfd, port, clientlen, nThreads, bufferSize;
  int i;
  struct sockaddr_in clientaddr;
    
  
  getargs(&port, &nThreads, &bufferSize, argc, argv);
  
  // Initialize other local and global variables
  reqNum = 0;
  reqIndexFill = 0;
  reqIndexServe = 0;
  bs = bufferSize;

  // Allocate a buffer to hold the maximum number of requests the
  // server can handle at one time.
  reqBuffer = (Req**) malloc(sizeof(*reqBuffer) * bufferSize);
  
  // printf("req buffer size = %ld\n", sizeof(*reqBuffer)*bufferSize); 
  

  // 
  // CS537: Create some threads...
  //
  // Allocate a buffer to hold the maximum number of worker threads
  // the server can use.
  cBuffer = (pthread_t **) malloc(sizeof(*cBuffer) * nThreads);

  // printf("cBuffer size = %ld\n", sizeof(*cBuffer) * nThreads);
  // Allocate a pool of threads to be used to process requests as
  // they arrive.
  for(i = 0; i < nThreads; i++){
    cBuffer[i] = (pthread_t*) malloc(sizeof(pthread_t));
    pthread_create(cBuffer[i], NULL, serveRequest, NULL);
  }

  listenfd = Open_listenfd(port);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
    
    // Busy wait while there is no more room in 
    pthread_mutex_lock(&tLock);
    while(reqNum == bufferSize){
      pthread_cond_wait(&tEmpty, &tLock);
    }

    // Allocate a new request
    Req *nReq = (Req*) malloc(sizeof(Req));
    nReq->cfd = connfd;
    reqNum++;

    /*
    // First in first out implementation
    reqBuffer[reqIndexFill] = nReq;
    reqIndexFill = (reqIndexFill+1) % bufferSize;
    */

    // Smallest file first implementation
    struct stat reqStat;
    fstat(connfd, &reqStat);
    nReq->cfd = connfd;
    nReq->reqSize = reqStat.st_size;
    reqBuffer[reqIndexFill] = nReq;
    reqIndexFill++;
    if(reqIndexFill > 1){
      qsort(reqBuffer, reqIndexFill, sizeof(*reqBuffer), reqSizeComp);
    }
    
    // 
    // CS537: In general, don't handle the request in the main thread.
    // Save the relevant info in a buffer and have one of the worker threads 
    // do the work.
    //
    
    
    //requestHandle(connfd);
    
    pthread_cond_signal(&tFill);
    pthread_mutex_unlock(&tLock);
    
    // Close(connfd);
  }

}


    


 
