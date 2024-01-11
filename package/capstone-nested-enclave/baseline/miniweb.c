#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "queue.h"

extern char **environ;

void readLine(int fd,char* buffer,int maxBytes) {
  char* ptr = buffer;
  int bytesRead = 0;
  while(bytesRead < maxBytes) {
    read(fd,ptr,1);
    if(*ptr == '\n')
      break;
    ptr++;
  }
  *(++ptr) = '\0';
}

void handle404(int fd) {
  char buffer[256];
  // The user has a requested a file that we don't have.
  // Send them back the canned 404 error response.
  int f404 = open("404Response.txt",O_RDONLY);
  int readSize = read(f404,buffer,255);
  close(f404);
  write(fd,buffer,readSize);
}

void serveRequest(int fd) {
  char lineBuffer[256];

  // Read the first line of the request
  readLine(fd,lineBuffer,255);
  // Grab the method and URL
  char method[16];
  char url[128];
  sscanf(lineBuffer,"%s %s",method,url);

  if(strcmp(method,"POST") == 0) {
    if(strncmp(url,"/cgi/",5) == 0) {
			// Read everything up to the blank line.
			// Grab the content length as you go.
			char contentLength[16];
			while(1) {
			  readLine(fd,lineBuffer,255);
			  if(strncmp(lineBuffer,"Content-Length:",15) == 0) {
			    strcpy(contentLength,lineBuffer+16);
			  } else if(lineBuffer[0] == '\r')
			    break;
			}
      pid_t pid = fork();
			if (pid == 0) {
			  char* emptylist[] = { NULL };
			  setenv("CONTENT_LENGTH", contentLength, 1);
			  dup2(fd, STDIN_FILENO);
			  dup2(fd, STDOUT_FILENO);
			  execve(url+1, emptylist, environ);
			}
			waitpid(pid,NULL,0);
    } else {
      handle404(fd);
    }
  } else {
    // Try to get the file the user wants
    char fileName[128];
    strcpy(fileName,"www");
    strcat(fileName,url);
    int filed = open(fileName,O_RDONLY);
    if(filed == -1) {
      handle404(fd);
    } else {
      const char* responseStatus = "HTTP/1.1 200 OK\n";
      const char* responseOther = "Connection: close\nContent-Type: text/html\n";
      // Get the size of the file
      char len[64];
      struct stat st;
      fstat(filed,&st);
      sprintf(len,"Content-Length: %d\n\n",(int) st.st_size);
      // Send the headers
      write(fd,responseStatus,strlen(responseStatus));
      write(fd,responseOther,strlen(responseOther));
      write(fd,len,strlen(len));
      // Send the file
      char buffer[1024];
      int bytesRead;
      while(bytesRead = read(filed,buffer,1023)) {
        write(fd,buffer,bytesRead);
      }
      close(filed);
    }
  }
  close(fd);
}

void* workerThread(void *arg) {
  queue* q = (queue*) arg;
  while(1) {
    int fd = dequeue(q);
    serveRequest(fd);
  }
  return NULL;
}

int main() {
  // Set up the queue
  queue* q = queueCreate();

  // Set up the worker threads
  pthread_t w1,w2;
  pthread_create(&w1,NULL,workerThread,q);
  pthread_create(&w2,NULL,workerThread,q);

  // Create the socket
  int server_socket = socket(AF_INET , SOCK_STREAM , 0);
  if (server_socket == -1) {
    printf("Could not create socket.\n");
    return 1;
  }

  // Set the 'reuse address' socket option
  int on = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  //Prepare the sockaddr_in structure
  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons( 8888 );

  // Bind to the port we want to use
  if(bind(server_socket,(struct sockaddr *)&server , sizeof(server)) < 0) {
    printf("Bind failed\n");
    return 1;
  }
  printf("Bind done\n");

  // Mark the socket as a passive socket
  listen(server_socket , 3);

  // Accept incoming connections
  printf("Waiting for incoming connections...\n");
  while(1) {
    struct sockaddr_in client;
    int new_socket , c = sizeof(struct sockaddr_in);
    new_socket = accept(server_socket, (struct sockaddr *) &client, (socklen_t*)&c);
    if(new_socket != -1) {
      enqueue(q,new_socket);
    }
  }

  return 0;
}
