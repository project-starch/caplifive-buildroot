#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
  char* lengthStr;
  if ((lengthStr = getenv("CONTENT_LENGTH")) != NULL) {
    // Get the content length
    int length;
    sscanf(lengthStr,"%d",&length);
    // Read the query from stdin
    char buffer[256];
    read(STDIN_FILENO,buffer,length);
    buffer[length] = '\0';
    // Isolate the name from the query string.
    char* p = strchr(buffer, '&');
    *p = '\0';
    char* name = buffer+5;
    p = strchr(name,'+');
    if(p != NULL)
    *p = ' ';

    // Make the body of the response
    char content[1024];
    sprintf(content,"<!DOCTYPE html>\r\n");
    sprintf(content,"%s<head>\r\n",content);
    sprintf(content,"%s<title>Registration successful</title>\r\n",content);
    sprintf(content,"%s</head>\r\n",content);
    sprintf(content,"%s<body>\r\n",content);
    sprintf(content,"%s<p>Hello, %s. You are now registered.<\p>\r\n",content,name);
    sprintf(content,"%s</body>",content);

    // Send the response
    printf("HTTP/1.1 200 OK\r\n");
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    fflush(stdout);
  } else {
    // Send back an error response
    printf("HTTP/1.1 500 Internal Server Error\r\n");
    printf("Connection: close\r\n");
    printf("Content-length: 21\r\n");
    printf("Content-type: text/plain\r\n\r\n");
    printf("Something went wrong.");
    fflush(stdout);
  }
  return 0;
}
