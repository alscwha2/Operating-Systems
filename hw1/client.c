/* Generic */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

/* Network */
#include <netdb.h>
#include <sys/socket.h>

#define BUF_SIZE 100

int clientfd;
char *host;
char *portnum;

pthread_barrier_t barrier;

// Get host information (used to establishConnection)
struct addrinfo *getHostInfo(char* host, char* port) {
  int r;
  struct addrinfo hints, *getaddrinfo_res;
  // Setup hints
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if ((r = getaddrinfo(host, port, &hints, &getaddrinfo_res))) {
    fprintf(stderr, "[getHostInfo:21:getaddrinfo] %s\n", gai_strerror(r));
    return NULL;
  }

  return getaddrinfo_res;
}

// Establish connection with host
int establishConnection(struct addrinfo *info) {
  if (info == NULL) return -1;

  int clientfd;
  for (;info != NULL; info = info->ai_next) {
    if ((clientfd = socket(info->ai_family,
                           info->ai_socktype,
                           info->ai_protocol)) < 0) {
      perror("[establishConnection:35:socket]");
      continue;
    }

    if (connect(clientfd, info->ai_addr, info->ai_addrlen) < 0) {
      close(clientfd);
      perror("[establishConnection:42:connect]");
      continue;
    }

    freeaddrinfo(info);
    return clientfd;
  }

  freeaddrinfo(info);
  return -1;
}

// Send GET request
void cGET( char *path) {
  /**** Establish connection with <hostname>:<port> ***/
  clientfd = establishConnection(getHostInfo(host, portnum));
  if (clientfd == -1) {
    fprintf(stderr,
            "[main:73] Failed to connect to: %s:%s%s \n",
            host, portnum, path);
    //return 3;
  }

  /*** SEND REQUEST ***/

  //don't send the request until everyone has made their connections 
  //  so that everyone can do it simultaneusly
  pthread_barrier_wait(&barrier);

  char req[1000] = {0};
  sprintf(req, "GET %s HTTP/1.0\r\n\r\n", path);
  send(clientfd, req, strlen(req), 0);

  /**** AWAIT RESULT ***/

  //don't accept responses until everyone has sent the get request
  pthread_barrier_wait(&barrier);

  //wait to recieve
  char buf[BUF_SIZE];
  while (recv(clientfd, buf, BUF_SIZE, 0) > 0) {
    fputs(buf, stdout);
    memset(buf, 0, BUF_SIZE);
  }

  //don't move on until everyone has recieved their responses
  pthread_barrier_wait(&barrier);
}

void * cLoop(void *filepath) {
  char *path = (char *) filepath;
  while(1) cGET(path);
  return "Success";
}

void concur(int numthreads, char *file1, char *file2) {
  pthread_barrier_init(&barrier, NULL, numthreads);
  
  printf("%d\n", numthreads);
  pthread_t thread_id[numthreads];
  for (int i = 0; i < numthreads; i++) {
    pthread_create(&thread_id[i], NULL, cLoop, file1);
  }
  for (int i = 0; i < numthreads; i++) pthread_join(thread_id[i], NULL);
  close(clientfd);
}

int fifo(int numthreads, char *file1, char *file2) {
  // Send GET request > stdout
  cGET(file1);
  cGET(file1);

  close(clientfd);
  return 0;
}



int main(int argc, char **argv) {
  int numthreads;
  char *file1;
  char *file2;

  /***** TESTING FOR VALID INPUT *****/

  //test for correct number of arguments
  if (argc != 6 && argc != 7) {
    fprintf(stderr, "USAGE: ./httpclient <hostname> <port> <number threads> <schedalg> <request path> <op: reqpath2>\n");
    return 1;
  }
  //test for valid number of threads (input)
  if (atoi(argv[3]) < 1) {
    fprintf(stderr, "ERROR: Must specify a positive number of threads\n");
    return 1;
  }
  //test input for valid scheduling algorithm
  if (strcmp(argv[4], "CONCUR") != 0 && strcmp(argv[4], "FIFO") != 0) {
    fprintf(stderr, "%s\n", "ERROR: Must specify a valid sceduling algorithms, either \"CONCUR\" or \"FIFO\"");
    return 1;
  }

  /***** STORING ARGUMENTS IN VARIABLES ****/
  host = argv[1];
  portnum = argv[2];
  numthreads = atoi(argv[3]);
  file1 = argv[5];

  //store the name of the second file in variable, NULL if no second file
  switch(argc) {
    case 6:
      file2 = NULL;
      break;
    case 7:
      file2 = argv[6];
      break;
    default:
      file2 = NULL;
  }

  /***** EXECUTE WITH SCHEDULING ALGORITHM ****/
  if (strcmp(argv[4], "FIFO") == 0) fifo(numthreads, file1, file2);
  if (strcmp(argv[4], "CONCUR") == 0) concur(numthreads, file1, file2);
  return 0;
}