#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include "lib/libcapstone.h"

#define QUEUE_SIZE 16
#define THREAD_SIZE 1
#define CONNECTION_NUM 3
#define print_nobuf(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

#define HTML_FD_UNDEFINED 0
#define HTML_FD_404RESPONSE -1
#define HTML_FD_DEFINED 1

dom_id_t dom_id;
char *socket_fd_region_base, *html_fd_region_base;

/* queue operations*/
typedef struct {
    int d[QUEUE_SIZE];
    int front;
    int back;
    sem_t mutex; 
    sem_t slots; 
    sem_t items; 
} queue;

queue* queueCreate() {
    queue *q = (queue*) malloc(sizeof(queue));
    q->front = 0;
    q->back = 0;
    sem_init(&q->mutex,0,1);
    sem_init(&q->slots,0,QUEUE_SIZE);
    sem_init(&q->items,0,0);
    return q;
}

void enqueue(queue* q,int fd) {
    sem_wait(&q->slots);
    sem_wait(&q->mutex);
    q->d[q->back] = fd;
    q->back = (q->back+1)%QUEUE_SIZE;
    sem_post(&q->mutex);
    sem_post(&q->items);
}

int dequeue(queue* q) {
    int fd;
    sem_wait(&q->items);
    sem_wait(&q->mutex);
    fd = q->d[q->front];
    q->front = (q->front+1)%QUEUE_SIZE;
    sem_post(&q->mutex);
    sem_post(&q->slots);
    return fd;
}

void* workerThread(void *arg) {
  queue* q = (queue*) arg;
  while(1) {
    int fd = dequeue(q);

    // read from socket
    char* ptr = socket_fd_region_base;
    int read_size_sockert_fd = read(fd, ptr, 4096);
    print_nobuf("read_size_sockert_fd: %d\n", read_size_sockert_fd);

    // provide html file accordingly
    int html_fd_status = HTML_FD_UNDEFINED;
    memcpy(html_fd_region_base, &html_fd_status, sizeof(html_fd_status));
    call_dom(dom_id);
    unsigned long html_fd_len; // also server as the path length
    memcpy(&html_fd_len, html_fd_region_base + sizeof(html_fd_status), sizeof(html_fd_len));
    if (html_fd_len != HTML_FD_DEFINED) {
        char* file_path = malloc(html_fd_len + 1);
        memcpy(file_path, html_fd_region_base + sizeof(html_fd_status) + sizeof(html_fd_len), html_fd_len + 1);
        char prefix[] = "/nested/capstone_split/www/";
        char* html_path = malloc(strlen(prefix) + html_fd_len + 1);
        strcpy(html_path, prefix);
        strcat(html_path, file_path);

        // read html file
        int html_fd = open(html_path, O_RDONLY);
        if (html_fd == -1) {
            print_nobuf("Couldn't open html file\n");
        }
        else {
            ptr = html_fd_region_base + sizeof(html_fd_status) + sizeof(html_fd_len);
            int read_size_html_fd = read(html_fd, ptr, 4096);
            memcpy(html_fd_region_base + sizeof(html_fd_status), &read_size_html_fd, sizeof(read_size_html_fd));
            print_nobuf("read_size_html_fd: %d\n", read_size_html_fd);
            call_dom(dom_id);
        }
    }
    else {
        char error_path[] = "/nested/capstone_split/404Response.txt";
        int error_fd = open(error_path, O_RDONLY);
        if (error_fd == -1) {
            print_nobuf("Couldn't open 404Response.txt\n");
        }
        else {
            ptr = html_fd_region_base + sizeof(html_fd_status);
            int read_size_html_fd = read(error_fd, ptr, 4096);
            memcpy(html_fd_region_base + sizeof(html_fd_status), &read_size_html_fd, sizeof(read_size_html_fd));
            print_nobuf("read_size_html_fd: %d\n", read_size_html_fd);
            call_dom(dom_id);
        }
    }
  } // end while
  return NULL;
}

int main() {
    capstone_init();

    /* domain creation, shared regions setup */
    dom_id = create_dom_ko("/test-domains/sbi.dom", "/nested/capstone_split/miniweb_backend.smode.ko");
    print_nobuf("SBI domain created with ID %lu\n", dom_id);

    // this region is shared among miniweb_frontend.user, miniweb_backend.smode.ko and cgis
    region_id_t socket_fd_region = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", socket_fd_region);
    // html_fd_region is used for both data and control
    // data: the html file content, passed from miniweb_frontend.user to miniweb_backend.smode.ko
    // control: path of the html file, used between miniweb_frontend.user and miniweb_backend.smode.ko
    region_id_t html_fd_region = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", html_fd_region);
    // used between miniweb_backend.smode.ko and cgis, no need for mapping
    region_id_t environ_region = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", environ_region);

    socket_fd_region_base = map_region(socket_fd_region, 4096);
    html_fd_region_base = map_region(html_fd_region, 4096);

    share_region(dom_id, socket_fd_region);
    share_region(dom_id, html_fd_region);
    share_region(dom_id, environ_region);

    /* socket setup */
    queue* q = queueCreate();
    pthread_t threads[THREAD_SIZE];

    for (int i = 0; i < THREAD_SIZE; i++) {
        pthread_create(&threads[i], NULL, workerThread, (void *) q);
    }

    int server_socket = socket(AF_INET , SOCK_STREAM , 0);
    if (server_socket == -1) {
        print_nobuf("Could not create socket.\n");
        return 1;
    }
    print_nobuf("Socket created.\n");

    int on = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 8888 );

    if(bind(server_socket,(struct sockaddr *)&server , sizeof(server)) < 0) {
        print_nobuf("Bind failed.\n");
        return 1;
    }
    print_nobuf("Bind done.\n");

    listen(server_socket, CONNECTION_NUM);
    print_nobuf("Waiting for incoming connections...\n");

    while(1) {
        struct sockaddr_in client;
        int c = sizeof(struct sockaddr_in);
        int new_socket = accept(server_socket, (struct sockaddr *) &client, (socklen_t*)&c);
        if(new_socket != -1) {
            enqueue(q,new_socket);
        }
    }

    capstone_cleanup();

    return 0;
}
