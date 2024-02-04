#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>
#include "lib/libcapstone.h"

#define CAPSTONE_ANNOTATION_PERM_IN 0x0
#define CAPSTONE_ANNOTATION_PERM_INOUT 0x1
#define CAPSTONE_ANNOTATION_PERM_OUT 0x2
#define CAPSTONE_ANNOTATION_PERM_EXE 0x3
#define CAPSTONE_ANNOTATION_PERM_FULL 0x4

#define CAPSTONE_ANNOTATION_REV_DEFAULT 0x0
#define CAPSTONE_ANNOTATION_REV_BORROWED 0x1
#define CAPSTONE_ANNOTATION_REV_SHARED 0x2
#define CAPSTONE_ANNOTATION_REV_TRANSFERRED 0x3

/* metadata region layout:
 * status::unsigned long
 * html/path length::unsigned long
 * socket/response length::unsigned long long
 * html file path::char[path length]
*/
#define METADATA_STATUS_OFFSET 0
#define METADATA_HTML_LEN_OFFSET sizeof(unsigned long)
#define METADATA_SOCKET_LEN_OFFSET (2 * sizeof(unsigned long))
#define METADATA_HTML_PATH_OFFSET (2 * sizeof(unsigned long) + sizeof(unsigned long long))

#define QUEUE_SIZE 16
#define THREAD_SIZE 1 // 1 thread for now
#define CONNECTION_NUM 3
#define CGI_ELF_REGION_SIZE (4096 * 64)
#define print_nobuf(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

#define HTML_FD_UNDEFINED 0
#define HTML_FD_404RESPONSE 1
#define HTML_FD_200RESPONSE 2
#define HTML_FD_CGI 3

dom_id_t dom_id;
// borrowed regions
region_id_t socket_fd_region, response_region, html_fd_region;
char *socket_fd_region_base, *response_region_base, *html_fd_region_base, *metadata_region_base;

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

    char* ptr = socket_fd_region_base;
    unsigned long long read_size_socket_fd = read(fd, ptr, 4096);
    memcpy(metadata_region_base + METADATA_SOCKET_LEN_OFFSET, &read_size_socket_fd, sizeof(read_size_socket_fd));
    print_nobuf("read_size_socket_fd: %llu\n", read_size_socket_fd);

    unsigned long html_fd_status = HTML_FD_UNDEFINED;
    memcpy(metadata_region_base + METADATA_STATUS_OFFSET, &html_fd_status, sizeof(html_fd_status));
    
    shared_region_annotated(dom_id, socket_fd_region, CAPSTONE_ANNOTATION_PERM_IN, CAPSTONE_ANNOTATION_REV_BORROWED);
    shared_region_annotated(dom_id, response_region, CAPSTONE_ANNOTATION_PERM_OUT, CAPSTONE_ANNOTATION_REV_BORROWED);
    
    print_nobuf("enter backend: for preprocessing\n");
    call_dom(dom_id);
    print_nobuf("exit backend: return from preprocessing\n");

    unsigned long html_fd_len; // also server as the path length
    memcpy(&html_fd_len, metadata_region_base + METADATA_HTML_LEN_OFFSET, sizeof(html_fd_len));
    memcpy(&html_fd_status, metadata_region_base + METADATA_STATUS_OFFSET, sizeof(html_fd_status));
    
    if (html_fd_status == HTML_FD_200RESPONSE) {
        print_nobuf("Backend request for 200 response\n");
        char* file_path = malloc(html_fd_len);
        memcpy(file_path, metadata_region_base + METADATA_HTML_PATH_OFFSET, html_fd_len);
        char prefix[] = "/nested/capstone_split/www";
        char* html_path = malloc(strlen(prefix) + html_fd_len);
        strcpy(html_path, prefix);
        strcat(html_path, file_path);

        // read html file
        int html_fd = open(html_path, O_RDONLY);
        if (html_fd == -1) {
            // send 404 if file not found as well
            html_fd_status = HTML_FD_404RESPONSE;
            memcpy(metadata_region_base + METADATA_STATUS_OFFSET, &html_fd_status, sizeof(html_fd_status));
            print_nobuf("Couldn't open html file\n");
        }
        else {
            ptr = html_fd_region_base;
            unsigned long read_size_html_fd = read(html_fd, ptr, 4096);
            memcpy(metadata_region_base + METADATA_HTML_LEN_OFFSET, &read_size_html_fd, sizeof(read_size_html_fd));
            print_nobuf("read_size_html_fd: %d\n", read_size_html_fd);
            close(html_fd);

            revoke_region(socket_fd_region);
            shared_region_annotated(dom_id, html_fd_region, CAPSTONE_ANNOTATION_PERM_IN, CAPSTONE_ANNOTATION_REV_BORROWED);

            print_nobuf("enter backend: 200 response\n");
            call_dom(dom_id);
            print_nobuf("exit backend: return from 200 response\n");

            revoke_region(html_fd_region);
            revoke_region(response_region);
            
            // sync socket_fd_region to socket fd
            unsigned long long socket_fd_region_len;
            memcpy(&socket_fd_region_len, metadata_region_base + METADATA_SOCKET_LEN_OFFSET, sizeof(socket_fd_region_len));
            print_nobuf("socket_fd_region_len: %llu\n", socket_fd_region_len);
            ptr = response_region_base;
            write(fd, ptr, socket_fd_region_len);
            close(fd);
        }
    }

    // 2 cases: 404 from backend or 404 due to file not found
    if (html_fd_status == HTML_FD_404RESPONSE) {
        print_nobuf("Backend request for 404 response\n");
        char error_path[] = "/nested/capstone_split/404Response.txt";
        int error_fd = open(error_path, O_RDONLY);
        if (error_fd == -1) {
            print_nobuf("Couldn't open 404Response.txt\n");
        }
        else {
            ptr = html_fd_region_base;
            unsigned long read_size_html_fd = read(error_fd, ptr, 4096);
            memcpy(metadata_region_base + METADATA_HTML_LEN_OFFSET, &read_size_html_fd, sizeof(read_size_html_fd));
            print_nobuf("read_size_html_fd: %d\n", read_size_html_fd);
            close(error_fd);

            revoke_region(socket_fd_region);
            shared_region_annotated(dom_id, html_fd_region, CAPSTONE_ANNOTATION_PERM_IN, CAPSTONE_ANNOTATION_REV_BORROWED);

            print_nobuf("enter backend: 404 response\n");
            call_dom(dom_id);
            print_nobuf("exit backend: return from 404 response\n");

            revoke_region(html_fd_region);
            revoke_region(response_region);

            // sync socket_fd_region to socket fd
            unsigned long long socket_fd_region_len;
            memcpy(&socket_fd_region_len, metadata_region_base + METADATA_SOCKET_LEN_OFFSET, sizeof(socket_fd_region_len));
            print_nobuf("socket_fd_region_len: %llu\n", socket_fd_region_len);
            ptr = response_region_base;
            write(fd, ptr, socket_fd_region_len);
            close(fd);
        }
    }
    
    if (html_fd_status == HTML_FD_CGI) {
        print_nobuf("POST request is handled by CGI.\n");

        revoke_region(socket_fd_region);
        revoke_region(response_region);

        // sync socket_fd_region to socket fd
        unsigned long long socket_fd_region_len;
        memcpy(&socket_fd_region_len, metadata_region_base + METADATA_SOCKET_LEN_OFFSET, sizeof(socket_fd_region_len));
        print_nobuf("socket_fd_region_len: %llu\n", socket_fd_region_len);
        ptr = response_region_base;
        write(fd, ptr, socket_fd_region_len);
        close(fd);
    }

    if (html_fd_status == HTML_FD_UNDEFINED) {
        print_nobuf("Server internal error: HTML FD UNDEFINED!\n");
    }
  } // end while
  return NULL;
}

int main() {
    capstone_init();

    print_nobuf("Size of long: %zu bytes\n", sizeof(long));
    assert(sizeof(long) == 8);
    print_nobuf("Size of long long: %zu bytes\n", sizeof(long long));
    assert(sizeof(long long) == 8);

    dom_id = create_dom("/test-domains/sbi.dom", "/nested/capstone_split/miniweb_backend.smode");
    print_nobuf("SBI domain created with ID %lu\n", dom_id);

    socket_fd_region = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", socket_fd_region);

    response_region = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", response_region);
    
    html_fd_region = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", html_fd_region);

    region_id_t metadata_region = create_region(4096);
    print_nobuf("Shared region created with ID %lu\n", metadata_region);
    
    region_id_t cgi_success_region = create_region(CGI_ELF_REGION_SIZE);
    print_nobuf("Shared region created with ID %lu\n", cgi_success_region);
    
    region_id_t cgi_fail_region = create_region(CGI_ELF_REGION_SIZE);
    print_nobuf("Shared region created with ID %lu\n", cgi_fail_region);

    socket_fd_region_base = map_region(socket_fd_region, 4096);
    response_region_base = map_region(response_region, 4096);
    html_fd_region_base = map_region(html_fd_region, 4096);
    metadata_region_base = map_region(metadata_region, 4096);

    /* cgi content set up */ 
    char* cgi_success_region_base = map_region(cgi_success_region, CGI_ELF_REGION_SIZE);

    char cgi_success_path[] = "/nested/capstone_split/cgi/cgi_register_success.dom";
    int cgi_success_fd = open(cgi_success_path, O_RDONLY);
    if (cgi_success_fd == -1) {
        print_nobuf("Couldn't open cgi_register_success.dom\n");
    }
    else {
        char* ptr = cgi_success_region_base;
        unsigned long read_size_cgi_success = read(cgi_success_fd, ptr, CGI_ELF_REGION_SIZE);
        print_nobuf("read_size_cgi_success: %lu\n", read_size_cgi_success);
        close(cgi_success_fd);
    }

    char* cgi_fail_region_base = map_region(cgi_fail_region, CGI_ELF_REGION_SIZE);

    char cgi_fail_path[] = "/nested/capstone_split/cgi/cgi_register_fail.dom";
    int cgi_fail_fd = open(cgi_fail_path, O_RDONLY);
    if (cgi_fail_fd == -1) {
        print_nobuf("Couldn't open cgi_register_fail.dom\n");
    }
    else {
        char* ptr = cgi_fail_region_base;
        unsigned long read_size_cgi_fail = read(cgi_fail_fd, ptr, CGI_ELF_REGION_SIZE);
        print_nobuf("read_size_cgi_fail: %lu\n", read_size_cgi_fail);
        close(cgi_fail_fd);
    }

    /* share regions */
    shared_region_annotated(dom_id, metadata_region, CAPSTONE_ANNOTATION_PERM_INOUT, CAPSTONE_ANNOTATION_REV_SHARED);
    shared_region_annotated(dom_id, cgi_success_region, CAPSTONE_ANNOTATION_PERM_FULL, CAPSTONE_ANNOTATION_REV_TRANSFERRED);
    shared_region_annotated(dom_id, cgi_fail_region, CAPSTONE_ANNOTATION_PERM_FULL, CAPSTONE_ANNOTATION_REV_TRANSFERRED);

    /* socket setup */
    queue* q = queueCreate();
    pthread_t threads[THREAD_SIZE];

    for (int i = 0; i < THREAD_SIZE; i++) {
        pthread_create(&threads[i], NULL, workerThread, (void *) q);
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
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
    server.sin_port = htons(8888);

    if(bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
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
            enqueue(q, new_socket);
        }
    }

    capstone_cleanup();

    return 0;
}
