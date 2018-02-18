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
void GET(int clientfd, char *path) {
  char req[1000] = {0};
  sprintf(req, "GET %s HTTP/1.0\r\n\r\n", path);
  send(clientfd, req, strlen(req), 0);
}

void await_request(int clientfd) {
  char buf[BUF_SIZE];
  while (recv(clientfd, buf, BUF_SIZE, 0) > 0) {
    fputs(buf, stdout);
    memset(buf, 0, BUF_SIZE);
  }
}

void concur(int clientfd, int numthreads, char *file1, char *file2) {
  // Send GET request > stdout
  GET(clientfd, file1);
  //wait for the response for the GET request
  await_request(clientfd);

  close(clientfd);
}

void fifo(int clientfd, int numthreads, char *file1, char *file2) {
  // Send GET request > stdout
  GET(clientfd, file1);
  //wait for the response for the GET request
  await_request(clientfd);

  close(clientfd);
}



int main(int argc, char **argv) {
  int clientfd;
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

  // Establish connection with <hostname>:<port>
  clientfd = establishConnection(getHostInfo(argv[1], argv[2]));
  if (clientfd == -1) {
    fprintf(stderr,
            "[main:73] Failed to connect to: %s:%s%s \n",
            argv[1], argv[2], argv[5]);
    return 3;
  }

  /***** STORING ARGUMENTS IN VARIABLES ****/
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
  if (strcmp(argv[4], "CONCUR") == 0) concur(clientfd, numthreads, file1, file2);
  if (strcmp(argv[4], "FIFO") == 0) fifo(clientfd, numthreads, file1, file2);


  return 0;
}