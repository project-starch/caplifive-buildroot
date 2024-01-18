#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))
#define __domreentry __attribute__((domreentry))

#define CAPSTONE_DPI_CALL             0x0
#define CAPSTONE_DPI_REGION_SHARE     0x1

#define SIZE_OF_ULL 8
#define CONTENT_FIXED_SIZE 58

#define C_PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))

void* shared_region;
unsigned respose_size = 0;

void putchar_to_socket(char* socket_region_ptr, char ch) {
    socket_region_ptr[respose_size] = ch;
    respose_size += 1;
}

unsigned unsigned_to_char_reverse(unsigned num, char* num_char) {
    unsigned i = 0, d;
    while (num != 0) {
        d = num / 10;
        num_char[i] = num - 10 * d + '0';
        num = d;
        i += 1;
    }
    num_char[i] = '\0';
    return i;
}

void register_success(void) {
    char name[16];
    char* socket_region_ptr = (char *)shared_region + SIZE_OF_ULL;

    /* parsing the packet and find the name */
    unsigned i = 0;
    unsigned name_index = 0;
    while (socket_region_ptr[i] != '=') {
        i += 1;
    }
    while (socket_region_ptr[i] != '&') {
        name[name_index] = socket_region_ptr[i];
        i += 1;
        name_index += 1;
    }
    name[name_index] = '\0';

    /* get the size of the content and parse it as char* */
    unsigned content_size = name_index + CONTENT_FIXED_SIZE;
    char num_char[16];
    unsigned num_char_len = unsigned_to_char_reverse(content_size, num_char);

    /* prepare the response packet */
    // HTTP/1.1 200 OK\n
    putchar_to_socket(socket_region_ptr, 'H');
    putchar_to_socket(socket_region_ptr, 'T');
    putchar_to_socket(socket_region_ptr, 'T');
    putchar_to_socket(socket_region_ptr, 'P');
    putchar_to_socket(socket_region_ptr, '/');
    putchar_to_socket(socket_region_ptr, '1');
    putchar_to_socket(socket_region_ptr, '.');
    putchar_to_socket(socket_region_ptr, '1');
    putchar_to_socket(socket_region_ptr, ' ');
    putchar_to_socket(socket_region_ptr, '2');
    putchar_to_socket(socket_region_ptr, '0');
    putchar_to_socket(socket_region_ptr, '0');
    putchar_to_socket(socket_region_ptr, ' ');
    putchar_to_socket(socket_region_ptr, 'O');
    putchar_to_socket(socket_region_ptr, 'K');
    putchar_to_socket(socket_region_ptr, '\n');
    // Connection: close\n
    putchar_to_socket(socket_region_ptr, 'C');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'c');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'i');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, ':');
    putchar_to_socket(socket_region_ptr, ' ');
    putchar_to_socket(socket_region_ptr, 'c');
    putchar_to_socket(socket_region_ptr, 'l');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 's');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, '\n');
    // Content-length: %d\n
    putchar_to_socket(socket_region_ptr, 'C');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, '-');
    putchar_to_socket(socket_region_ptr, 'l');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, 'g');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'h');
    putchar_to_socket(socket_region_ptr, ':');
    putchar_to_socket(socket_region_ptr, ' ');

    for (unsigned j = 0; j < num_char_len; j += 1) {
        putchar_to_socket(socket_region_ptr, num_char[num_char_len - j - 1]);
    }

    putchar_to_socket(socket_region_ptr, '\n');
    // Content-type: text/html\n\n
    putchar_to_socket(socket_region_ptr, 'C');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, '-');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'y');
    putchar_to_socket(socket_region_ptr, 'p');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, ':');
    putchar_to_socket(socket_region_ptr, ' ');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'x');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, '/');
    putchar_to_socket(socket_region_ptr, 'h');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'm');
    putchar_to_socket(socket_region_ptr, 'l');
    putchar_to_socket(socket_region_ptr, '\n');
    putchar_to_socket(socket_region_ptr, '\n');
    // <html><body>Hello, %s. You are now registered.</body></html>
    putchar_to_socket(socket_region_ptr, '<');
    putchar_to_socket(socket_region_ptr, 'h');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'm');
    putchar_to_socket(socket_region_ptr, 'l');
    putchar_to_socket(socket_region_ptr, '>');
    putchar_to_socket(socket_region_ptr, '<');
    putchar_to_socket(socket_region_ptr, 'b');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 'd');
    putchar_to_socket(socket_region_ptr, 'y');
    putchar_to_socket(socket_region_ptr, '>');
    putchar_to_socket(socket_region_ptr, 'H');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'l');
    putchar_to_socket(socket_region_ptr, 'l');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, ',');
    putchar_to_socket(socket_region_ptr, ' ');

    for (unsigned j = 0; j < name_index; j += 1) {
        putchar_to_socket(socket_region_ptr, name[j]);
    }

    putchar_to_socket(socket_region_ptr, '.');
    putchar_to_socket(socket_region_ptr, ' ');
    putchar_to_socket(socket_region_ptr, 'Y');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 'u');
    putchar_to_socket(socket_region_ptr, ' ');
    putchar_to_socket(socket_region_ptr, 'a');
    putchar_to_socket(socket_region_ptr, 'r');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, ' ');
    putchar_to_socket(socket_region_ptr, 'n');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 'w');
    putchar_to_socket(socket_region_ptr, ' ');
    putchar_to_socket(socket_region_ptr, 'r');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'g');
    putchar_to_socket(socket_region_ptr, 'i');
    putchar_to_socket(socket_region_ptr, 's');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'r');
    putchar_to_socket(socket_region_ptr, 'e');
    putchar_to_socket(socket_region_ptr, 'd');
    putchar_to_socket(socket_region_ptr, '.');
    putchar_to_socket(socket_region_ptr, '<');
    putchar_to_socket(socket_region_ptr, '/');
    putchar_to_socket(socket_region_ptr, 'b');
    putchar_to_socket(socket_region_ptr, 'o');
    putchar_to_socket(socket_region_ptr, 'd');
    putchar_to_socket(socket_region_ptr, 'y');
    putchar_to_socket(socket_region_ptr, '>');
    putchar_to_socket(socket_region_ptr, '<');
    putchar_to_socket(socket_region_ptr, '/');
    putchar_to_socket(socket_region_ptr, 'h');
    putchar_to_socket(socket_region_ptr, 't');
    putchar_to_socket(socket_region_ptr, 'm');
    putchar_to_socket(socket_region_ptr, 'l');
    putchar_to_socket(socket_region_ptr, '>');

    /* set the socket packet size */
    *((unsigned *)shared_region) = respose_size;
}


void dpi_call(void) {
    register_success();
}

void dpi_share_region(void *region) {
    shared_region = region;
}

unsigned handle_dpi(unsigned func, void *arg) {
    unsigned handled = 0;

    switch(func) {
        case CAPSTONE_DPI_CALL:
            dpi_call();
            break;
        case CAPSTONE_DPI_REGION_SHARE:
            dpi_share_region(arg);
            handled = 1;
            break;
    }

    return handled;
}

__domentry __domreentry void cgi_entry(__domret void *ra, unsigned func, unsigned *buf) {
    __domret void *caller_dom = ra;
    
    int handled = handle_dpi(func, buf);
    if(!handled) {
        C_PRINT(0xdeadbeef);
        while(1);
    }
    ra = caller_dom;

    __domreturn(ra, __cgi_entry_reentry, 0);
}
