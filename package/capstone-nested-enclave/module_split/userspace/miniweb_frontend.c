#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>

#define QUEUE_SIZE 16
#define THREAD_SIZE 1
#define CONNECTION_NUM 3
#define print_nobuf(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

#define HTML_FD_UNDEFINED 0
#define HTML_FD_404RESPONSE 1
#define HTML_FD_200RESPONSE 2
#define HTML_FD_CGI 3

#define DEVICE_PATH "/dev/nested"
#define SHARED_MEMORY_SIZE (4096 * 2)
#define HTML_FD_OFFSET 4096

int dev_fd;
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
    // socket_fd_region: (unsigned long long) socket_fd_len, (char*) content (socket file content)
    unsigned long long read_size_socket_fd;
    char* ptr = socket_fd_region_base + sizeof(read_size_socket_fd);
    read_size_socket_fd = read(fd, ptr, 4096 - sizeof(read_size_socket_fd));
    memcpy(socket_fd_region_base, &read_size_socket_fd, sizeof(read_size_socket_fd));
    print_nobuf("read_size_socket_fd: %llu\n", read_size_socket_fd);

    // provide html file accordingly
    // html_fd_region: (unsigned long) html_fd_status, (unsigned long) html_fd_len, (char*) content (path or file content)
    unsigned long html_fd_status = HTML_FD_UNDEFINED;
    memcpy(html_fd_region_base, &html_fd_status, sizeof(html_fd_status));

    print_nobuf("enter backend: for preprocessing\n");
    ioctl(dev_fd, 0, 0);
    print_nobuf("exit backend: return from preprocessing\n");

    
    unsigned long html_fd_len; // also server as the path length
    memcpy(&html_fd_len, html_fd_region_base + sizeof(html_fd_status), sizeof(html_fd_len));
    memcpy(&html_fd_status, html_fd_region_base, sizeof(html_fd_status));
    
    if (html_fd_status == HTML_FD_200RESPONSE) {
        print_nobuf("Backend request for 200 response\n");
        char* file_path = malloc(html_fd_len);
        memcpy(file_path, html_fd_region_base + sizeof(html_fd_status) + sizeof(html_fd_len), html_fd_len);
        char prefix[] = "/nested/capstone_split/www";
        char* html_path = malloc(strlen(prefix) + html_fd_len);
        strcpy(html_path, prefix);
        strcat(html_path, file_path);

        // read html file
        int html_fd = open(html_path, O_RDONLY);
        if (html_fd == -1) {
            // send 404 if file not found as well
            html_fd_status = HTML_FD_404RESPONSE;
            memcpy(html_fd_region_base, &html_fd_status, sizeof(html_fd_status));
            print_nobuf("Couldn't open html file\n");
        }
        else {
            ptr = html_fd_region_base + sizeof(html_fd_status) + sizeof(html_fd_len);
            unsigned long read_size_html_fd = read(html_fd, ptr, 4096 - sizeof(html_fd_status) - sizeof(html_fd_len));
            memcpy(html_fd_region_base + sizeof(html_fd_status), &read_size_html_fd, sizeof(read_size_html_fd));
            print_nobuf("read_size_html_fd: %d\n", read_size_html_fd);
            close(html_fd);

            print_nobuf("enter backend: 200 response\n");
            ioctl(dev_fd, 0, 0);
            print_nobuf("exit backend: return from 200 response\n");
            // sync socket_fd_region to socket fd
            unsigned long long socket_fd_region_len;
            memcpy(&socket_fd_region_len, socket_fd_region_base, sizeof(socket_fd_region_len));
            print_nobuf("socket_fd_region_len: %llu\n", socket_fd_region_len);
            ptr = socket_fd_region_base + sizeof(socket_fd_region_len);
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
            ptr = html_fd_region_base + sizeof(html_fd_status);
            unsigned long read_size_html_fd = read(error_fd, ptr, 4096 - sizeof(html_fd_status));
            memcpy(html_fd_region_base + sizeof(html_fd_status), &read_size_html_fd, sizeof(read_size_html_fd));
            print_nobuf("read_size_html_fd: %d\n", read_size_html_fd);
            close(error_fd);

            print_nobuf("enter backend: 404 response\n");
            ioctl(dev_fd, 0, 0);
            print_nobuf("exit backend: return from 404 response\n");
            // sync socket_fd_region to socket fd
            unsigned long long socket_fd_region_len;
            memcpy(&socket_fd_region_len, socket_fd_region_base, sizeof(socket_fd_region_len));
            print_nobuf("socket_fd_region_len: %llu\n", socket_fd_region_len);
            ptr = socket_fd_region_base + sizeof(socket_fd_region_len);
            write(fd, ptr, socket_fd_region_len);
            close(fd);
        }
    }
    
    if (html_fd_status == HTML_FD_CGI) {
        print_nobuf("POST request is handled by CGI.\n");

        // sync socket_fd_region to socket fd
        unsigned long long socket_fd_region_len;
        memcpy(&socket_fd_region_len, socket_fd_region_base, sizeof(socket_fd_region_len));
        print_nobuf("socket_fd_region_len: %llu\n", socket_fd_region_len);
        ptr = socket_fd_region_base + sizeof(socket_fd_region_len);
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
    dev_fd = open(DEVICE_PATH, O_RDWR);
    if (dev_fd < 0) {
        perror("Failed to open the device");
        return -1;
    }

    char *shared_kernel_memory = mmap(NULL, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, 0);
    if (shared_kernel_memory == MAP_FAILED) {
        perror("Failed to map memory");
        close(dev_fd);
        return -1;
    }

    socket_fd_region_base = shared_kernel_memory;
    html_fd_region_base = shared_kernel_memory + HTML_FD_OFFSET;

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

    munmap(shared_kernel_memory, SHARED_MEMORY_SIZE);
    close(dev_fd);

    return 0;
}
