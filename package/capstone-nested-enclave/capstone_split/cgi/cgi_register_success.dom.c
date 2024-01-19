#define __linear __attribute__((linear))
#define __dom __attribute__((dom))
#define __rev __attribute__((rev))
#define __domret __attribute__((domret))
#define __domasync __attribute__((domasync))
#define __domentry __attribute__((domentry))
#define __domreentry __attribute__((domreentry))

#define CAPSTONE_DPI_CALL             0x0
#define CAPSTONE_DPI_REGION_SHARE     0x1

/* Assumptions of this Capstone-CC compiled program:
 * 1. The machine is little endian;
 * 2. The size of unsigned long long in C is 8 bytes;
 * 3. The size of unsigned in Capstone-CC is 8 bytes;
 * 4. Pointer arithmetic in Capstone-CC is byte granular;
 * 5. Sign extension is always 0 extension in Capstone-CC;
 * 5. The packet in the socket is generated by clicking the "Submit Success" button in the register.html webpage;
 * 6. The name only contains 'a'-'z', 'A'-'Z', '0'-'9', ' ', '-', '_', and its length is less than 16.
*/

#define SIZE_OF_ULL 8
#define CONTENT_FIXED_SIZE 58

#define C_PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))

void* shared_region;

unsigned* socket_region_ptr;
unsigned response_size;
unsigned ptr_outer_offset;
unsigned ptr_inner_offset;

void putchar_to_socket(unsigned ch) {
    // zero 8 bytes at the beginning
    if (ptr_inner_offset == 0) {
        socket_region_ptr[ptr_outer_offset] = 0;
    }

    socket_region_ptr[ptr_outer_offset] |= (ch << (ptr_inner_offset * 8));
    ptr_inner_offset += 1;
    response_size += 1;

    if (ptr_inner_offset == 8) {
        ptr_inner_offset = 0;
        ptr_outer_offset += 1;
    }
}

unsigned unsigned_to_char_reverse(unsigned num, unsigned* num_char) {
    unsigned i = 0, d;
    while (num != 0) {
        d = num / 10;
        num_char[i] = num - 10 * d + '0';
        num = d;
        i += 1;
    }
    return i;
}

void register_success(void) {
    socket_region_ptr = (unsigned *)shared_region + SIZE_OF_ULL;

    /* parsing the packet and find the name */
    unsigned name[16];

    unsigned outer_i = 0, inner_i = 0;
    unsigned find_target = 0;
    unsigned name_index = 0;
    C_PRINT(0xdadada);
    // copy the name between '=' and '&'
    while (find_target != 2) {
        for (inner_i = 0; inner_i < 8; inner_i += 1) {
            unsigned ch = ((socket_region_ptr[outer_i] >> (inner_i * 8)) & 0x000000ff);
            if (find_target == 0) {
                if (ch == '=') {
                    find_target = 1;
                }
            }
            else {
                if (find_target == 1) {
                    if (ch == '&') {
                        find_target = 2;
                        break;
                    }
                    else {
                        name[name_index] = ch;
                        name_index += 1;
                    }
                }
            }
        }
        outer_i += 1;
    }

    /* size of the content */
    unsigned content_size = name_index + CONTENT_FIXED_SIZE;
    unsigned content_size_char[16];
    unsigned content_size_char_len = unsigned_to_char_reverse(content_size, content_size_char);

    /* prepare the response packet */
    ptr_outer_offset = 0;
    ptr_inner_offset = 0;

    // HTTP/1.1 200 OK\n
    putchar_to_socket('H');
    putchar_to_socket('T');
    putchar_to_socket('T');
    putchar_to_socket('P');
    putchar_to_socket('/');
    putchar_to_socket('1');
    putchar_to_socket('.');
    putchar_to_socket('1');
    putchar_to_socket(' ');
    putchar_to_socket('2');
    putchar_to_socket('0');
    putchar_to_socket('0');
    putchar_to_socket(' ');
    putchar_to_socket('O');
    putchar_to_socket('K');
    putchar_to_socket('\n');
    // Connection: close\n
    putchar_to_socket('C');
    putchar_to_socket('o');
    putchar_to_socket('n');
    putchar_to_socket('n');
    putchar_to_socket('e');
    putchar_to_socket('c');
    putchar_to_socket('t');
    putchar_to_socket('i');
    putchar_to_socket('o');
    putchar_to_socket('n');
    putchar_to_socket(':');
    putchar_to_socket(' ');
    putchar_to_socket('c');
    putchar_to_socket('l');
    putchar_to_socket('o');
    putchar_to_socket('s');
    putchar_to_socket('e');
    putchar_to_socket('\n');
    // Content-length: %d\n
    putchar_to_socket('C');
    putchar_to_socket('o');
    putchar_to_socket('n');
    putchar_to_socket('t');
    putchar_to_socket('e');
    putchar_to_socket('n');
    putchar_to_socket('t');
    putchar_to_socket('-');
    putchar_to_socket('l');
    putchar_to_socket('e');
    putchar_to_socket('n');
    putchar_to_socket('g');
    putchar_to_socket('t');
    putchar_to_socket('h');
    putchar_to_socket(':');
    putchar_to_socket(' ');

    for (unsigned i = 0; i < content_size_char_len; i += 1) {
        putchar_to_socket(content_size_char[content_size_char_len - i - 1]);
    }

    putchar_to_socket('\n');
    // Content-type: text/html\n\n
    putchar_to_socket('C');
    putchar_to_socket('o');
    putchar_to_socket('n');
    putchar_to_socket('t');
    putchar_to_socket('e');
    putchar_to_socket('n');
    putchar_to_socket('t');
    putchar_to_socket('-');
    putchar_to_socket('t');
    putchar_to_socket('y');
    putchar_to_socket('p');
    putchar_to_socket('e');
    putchar_to_socket(':');
    putchar_to_socket(' ');
    putchar_to_socket('t');
    putchar_to_socket('e');
    putchar_to_socket('x');
    putchar_to_socket('t');
    putchar_to_socket('/');
    putchar_to_socket('h');
    putchar_to_socket('t');
    putchar_to_socket('m');
    putchar_to_socket('l');
    putchar_to_socket('\n');
    putchar_to_socket('\n');
    // <html><body>Hello, %s. You are now registered.</body></html>
    putchar_to_socket('<');
    putchar_to_socket('h');
    putchar_to_socket('t');
    putchar_to_socket('m');
    putchar_to_socket('l');
    putchar_to_socket('>');
    putchar_to_socket('<');
    putchar_to_socket('b');
    putchar_to_socket('o');
    putchar_to_socket('d');
    putchar_to_socket('y');
    putchar_to_socket('>');
    putchar_to_socket('H');
    putchar_to_socket('e');
    putchar_to_socket('l');
    putchar_to_socket('l');
    putchar_to_socket('o');
    putchar_to_socket(',');
    putchar_to_socket(' ');

    for (unsigned j = 0; j < name_index; j += 1) {
        putchar_to_socket(name[j]);
    }

    putchar_to_socket('.');
    putchar_to_socket(' ');
    putchar_to_socket('Y');
    putchar_to_socket('o');
    putchar_to_socket('u');
    putchar_to_socket(' ');
    putchar_to_socket('a');
    putchar_to_socket('r');
    putchar_to_socket('e');
    putchar_to_socket(' ');
    putchar_to_socket('n');
    putchar_to_socket('o');
    putchar_to_socket('w');
    putchar_to_socket(' ');
    putchar_to_socket('r');
    putchar_to_socket('e');
    putchar_to_socket('g');
    putchar_to_socket('i');
    putchar_to_socket('s');
    putchar_to_socket('t');
    putchar_to_socket('e');
    putchar_to_socket('r');
    putchar_to_socket('e');
    putchar_to_socket('d');
    putchar_to_socket('.');
    putchar_to_socket('<');
    putchar_to_socket('/');
    putchar_to_socket('b');
    putchar_to_socket('o');
    putchar_to_socket('d');
    putchar_to_socket('y');
    putchar_to_socket('>');
    putchar_to_socket('<');
    putchar_to_socket('/');
    putchar_to_socket('h');
    putchar_to_socket('t');
    putchar_to_socket('m');
    putchar_to_socket('l');
    putchar_to_socket('>');

    /* set the socket packet size */
    *((unsigned *)shared_region) = response_size;
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
    
    unsigned handled = handle_dpi(func, buf);
    if(!handled) {
        C_PRINT(0xdeadbeef);
        while(1);
    }
    ra = caller_dom;

    __domreturn(ra, __cgi_entry_reentry, 0);
}
