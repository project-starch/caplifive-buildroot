#include "miniweb_backend.smode.h"

#define DEBUG_COUNTER_SHARED 10
#define DEBUG_COUNTER_SHARED_TIMES 11
#define DEBUG_COUNTER_BORROWED 12
#define DEBUG_COUNTER_BORROWED_TIMES 13
#define DEBUG_COUNTER_DOUBLE_TRANSFERRED 14
#define DEBUG_COUNTER_DOUBLE_TRANSFERRED_TIMES 15
#define DEBUG_COUNTER_BORROWED_TRANSFERRED 16
#define DEBUG_COUNTER_BORROWED_TRANSFERRED_TIMES 17
#define DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED 18
#define DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED_TIMES 19
#define debug_counter_inc(counter_no, delta) __asm__ volatile(".insn r 0x5b, 0x1, 0x45, x0, %0, %1" :: "r"(counter_no), "r"(delta))
#define debug_shared_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_SHARED, delta); debug_counter_inc(DEBUG_COUNTER_SHARED_TIMES, 1)
#define debug_borrowed_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_BORROWED, delta); debug_counter_inc(DEBUG_COUNTER_BORROWED_TIMES, 1)
#define debug_double_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_DOUBLE_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_DOUBLE_TRANSFERRED_TIMES, 1)
#define debug_borrowed_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_BORROWED_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_BORROWED_TRANSFERRED_TIMES, 1)
#define debug_mutable_borrowed_transferred_counter_inc(delta) debug_counter_inc(DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED, delta); debug_counter_inc(DEBUG_COUNTER_MUTABLE_BORROWED_TRANSFERRED_TIMES, 1)

#define C_PRINT(v) __asm__ volatile(".insn r 0x5b, 0x1, 0x43, x0, %0, x0" :: "r"(v))
#define STACK_SIZE (4096 * 2)

#define HTML_REGION_POP_NUM 3
#define CGI_REGION_POP_NUM 2

static char stack[STACK_SIZE];

static dom_id_t cgi_success_dom_id;
static dom_id_t cgi_fail_dom_id;

static region_id_t metadata_region;
static region_id_t socket_fd_region;
static region_id_t response_region;

static char *metadata_region_base;
static char *socket_fd_region_base;
static char *response_region_base;

static char* region_id_to_base(region_id_t region_id) {
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_QUERY,
		/* region_id = */ region_id,
		/* field = */ CAPSTONE_REGION_FIELD_BASE,
		0, 0, 0, 0);
	return (char *)sbi_res.value;
}

static char *socket_fd_region_ptr;
static void read_line_from_socket(char *buf, int buf_size) {
	int i = 0;
	while (i < buf_size) {
		buf[i] = socket_fd_region_ptr[i];
		if (buf[i] == '\n') {
			buf[i] = '\0';
			i++;
			break;
		}
		i++;
	}
	socket_fd_region_ptr += i;
}

static dom_id_t create_dom_from_region(char* elf_region_base) {
	Elf64_Ehdr *elf_header = (Elf64_Ehdr *)elf_region_base;

	if (elf_header->e_machine != EM_RISCV) {
		C_PRINT(0xdeadbeefULL);
	}

	Elf64_Phdr *phdrs = (Elf64_Phdr*)((void*)elf_header + elf_header->e_phoff);
    Elf64_Half phnum = elf_header->e_phnum;

	int ph_idx;
    for (ph_idx = 0; ph_idx < phnum; ph_idx++) {
        if (phdrs[ph_idx].p_type == PT_LOAD && (phdrs[ph_idx].p_flags & PF_X)) {
            break;
        }
    }

	unsigned long code_start = (unsigned long)elf_header + phdrs[ph_idx].p_offset;
	unsigned long code_len = phdrs[ph_idx].p_filesz;
	unsigned long entry_offset = elf_header->e_entry - phdrs[ph_idx].p_vaddr;
	unsigned long tot_len = code_len + C_DOMAIN_DATA_SIZE;
	tot_len = (((tot_len - 1) >> 4) + 1) << 4; // align to 16 bytes

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CREATE,
		/* base paddr = */ code_start,
		/* code size = */ code_len,
		/* tot size = */ tot_len,
		/* entry offset = */ entry_offset,
		0, 0);
	
	return (dom_id_t)sbi_res.value;
}

static int simple_strcmp(const char *str1, const char *str2) {
    while (*str1 != '\0' && *str2 != '\0') {
        if (*str1 != *str2) {
            return (*str1 - *str2);
        }
        str1++;
        str2++;
    }

    return (*str1 - *str2);
}

static int simple_strncmp(const char *str1, const char *str2, unsigned long n) {
    while (n > 0) {
        if (*str1 == '\0' || *str2 == '\0') {
            return (*str1 - *str2);
        }

        if (*str1 != *str2) {
            return (*str1 - *str2);
        }

        str1++;
        str2++;
        n--;
    }

    return 0;
}

static void simple_memcpy(void *dest, const void *src, unsigned long n) {
    char *dest_ptr = (char *)dest;
    const char *src_ptr = (const char *)src;

    while (n > 0) {
        *dest_ptr = *src_ptr;
        dest_ptr++;
        src_ptr++;
        n--;
    }
}

static void request_handle_cgi(unsigned long register_success) {
	unsigned long html_fd_status = HTML_FD_CGI;
	simple_memcpy(metadata_region_base + METADATA_STATUS_OFFSET, &html_fd_status, sizeof(html_fd_status));
	debug_shared_counter_inc(sizeof(unsigned long));

	if (register_success == 1) {
		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
			cgi_success_dom_id, socket_fd_region, CAPSTONE_ANNOTATION_PERM_IN, CAPSTONE_ANNOTATION_REV_TRANSFERRED, 0, 0);
		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
			cgi_success_dom_id, response_region, CAPSTONE_ANNOTATION_PERM_OUT, CAPSTONE_ANNOTATION_REV_TRANSFERRED, 0, 0);

		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL,
			cgi_success_dom_id, 0, 0, 0, 0, 0);
	}
	else {
		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
			cgi_fail_dom_id, socket_fd_region, CAPSTONE_ANNOTATION_PERM_IN, CAPSTONE_ANNOTATION_REV_TRANSFERRED, 0, 0);
		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
			cgi_fail_dom_id, response_region, CAPSTONE_ANNOTATION_PERM_OUT, CAPSTONE_ANNOTATION_REV_TRANSFERRED, 0, 0);
		
		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL,
			cgi_fail_dom_id, 0, 0, 0, 0, 0);
	}

	// pop regions
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_POP,
			CGI_REGION_POP_NUM, 0, 0, 0, 0, 0);
}

static void handle_404_preprocessing(void) {
	unsigned long html_fd_status = HTML_FD_404RESPONSE;
	simple_memcpy(metadata_region_base + METADATA_STATUS_OFFSET, &html_fd_status, sizeof(html_fd_status));
	debug_shared_counter_inc(sizeof(unsigned long));
}

static void request_reprocessing(void) {
	char lineBuffer[256];

	socket_fd_region_ptr = socket_fd_region_base;
	read_line_from_socket(lineBuffer, 256);
	char method[16];
	char url[128];
	
	// get method and url from lineBuffer
	// POST /cgi/register HTTP/1.1
	int i = 0;
	int j = 0;
	while (lineBuffer[i] != ' ' && i < 16) {
		method[i] = lineBuffer[i];
		i++;
	}
	method[i] = '\0';
	i++;
	while (lineBuffer[i] != ' ' && j < 128) {
		url[j] = lineBuffer[i];
		i++;
		j++;
	}
	url[j] = '\0';
	unsigned long url_length = j - 1;

	if (simple_strcmp(method, "POST") == 0) {
		if (simple_strncmp(url, "/cgi/", 5) == 0) {
			if (simple_strncmp(url, "/cgi/cgi_register_success.dom", 29) == 0) {
				request_handle_cgi(1);
				return;
			}
			else if (simple_strncmp(url, "/cgi/cgi_register_fail.dom", 26) == 0){
				request_handle_cgi(0);
				return;
			}
			else {
				handle_404_preprocessing();
				return;
			}
		}
		else {
			handle_404_preprocessing();
			return;
		}
	}
	else {
		unsigned long html_fd_status = HTML_FD_200RESPONSE;
		simple_memcpy(metadata_region_base + METADATA_STATUS_OFFSET, &html_fd_status, sizeof(html_fd_status));
		debug_shared_counter_inc(sizeof(unsigned long));
		unsigned long html_fd_len = url_length + 1;
		simple_memcpy(metadata_region_base + METADATA_HTML_LEN_OFFSET, &html_fd_len, sizeof(html_fd_len));
		debug_shared_counter_inc(sizeof(unsigned long));
		simple_memcpy(metadata_region_base + METADATA_HTML_PATH_OFFSET, url, html_fd_len);
		debug_shared_counter_inc(html_fd_len);
		return;
	}
}

static void request_handle_404(void) {
	unsigned long html_fd_len;
	simple_memcpy(&html_fd_len, metadata_region_base + METADATA_HTML_LEN_OFFSET, sizeof(html_fd_len));
	debug_shared_counter_inc(sizeof(unsigned long));

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	region_id_t html_fd_region = region_n - 1;
	char *html_fd_region_base = region_id_to_base(html_fd_region);

	unsigned long long socket_fd_len = (unsigned long long)html_fd_len;

	simple_memcpy(metadata_region_base + METADATA_SOCKET_LEN_OFFSET, &socket_fd_len, sizeof(socket_fd_len));
	debug_shared_counter_inc(sizeof(unsigned long long));
	simple_memcpy(response_region_base, html_fd_region_base, html_fd_len);
	debug_borrowed_counter_inc(html_fd_len);

	// delin response, so that the frontend can read it
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_DE_LINEAR,
			response_region, 0, 0, 0, 0, 0);

	// pop regions
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_POP,
			HTML_REGION_POP_NUM, 0, 0, 0, 0, 0);
}

void ulong_to_str(char *dest, unsigned long num) {
    if (num == 0) {
        *dest = '0';
        dest++;
        *dest = '\0';
        return;
    }

    char temp[21];

    int i = 0;
    while (num > 0) {
        temp[i] = '0' + num % 10;
        num /= 10;
        i++;
    }

    for (int j = i - 1; j >= 0; j--) {
        *dest = temp[j];
        dest++;
    }

    *dest = '\0';
}

unsigned long strcat_four_strings(char *dest, const char *str1, const char *str2, const char *str3, const char *str4) {
    unsigned long length = 0;
	
	while (*str1 != '\0') {
        *dest = *str1;
        dest++;
        str1++;
		length++;
    }

    while (*str2 != '\0') {
        *dest = *str2;
        dest++;
        str2++;
		length++;
    }

    while (*str3 != '\0') {
        *dest = *str3;
        dest++;
        str3++;
		length++;
    }

	while (*str4 != '\0') {
        *dest = *str4;
        dest++;
        str4++;
		length++;
    }

	// padding with \n\n
	*dest = '\n';
	*(dest + 1) = '\n';
	length += 2;

    return length;
}

static void request_handle_200(void) {
	const char* responseStatus = "HTTP/1.1 200 OK\n";
	const char* responseOther = "Connection: close\nContent-Type: text/html\n";
	const char* responseLen = "Content-Length: ";

	unsigned long html_fd_len;
	simple_memcpy(&html_fd_len, metadata_region_base + METADATA_HTML_LEN_OFFSET, sizeof(html_fd_len));
	debug_shared_counter_inc(sizeof(unsigned long));

	char buffer[256];
	char num_char[21];
	ulong_to_str(num_char, html_fd_len);
	unsigned long buffer_size = strcat_four_strings(buffer, responseStatus, responseOther, responseLen, num_char);
	unsigned long long socket_fd_len = buffer_size + html_fd_len;

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	region_id_t html_fd_region = region_n - 1;
	char *html_fd_region_base = region_id_to_base(html_fd_region);

	simple_memcpy(metadata_region_base + METADATA_SOCKET_LEN_OFFSET, &socket_fd_len, sizeof(socket_fd_len));
	debug_shared_counter_inc(sizeof(unsigned long long));
	simple_memcpy(response_region_base, buffer, buffer_size);
	simple_memcpy(response_region_base + buffer_size, html_fd_region_base, html_fd_len);
	debug_borrowed_counter_inc(buffer_size + html_fd_len);

	// delin response, so that the frontend can read it
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_DE_LINEAR,
			response_region, 0, 0, 0, 0, 0);

	// pop regions
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_POP,
			HTML_REGION_POP_NUM, 0, 0, 0, 0, 0);
}

static void main(void) {
	/* set up region id and pointers */
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	response_region = region_n - 1;
	socket_fd_region = response_region - 1;
	region_id_t cgi_fail_region = socket_fd_region - 1;
	region_id_t cgi_success_region = cgi_fail_region - 1;
	metadata_region = cgi_success_region - 1;

	metadata_region_base = region_id_to_base(metadata_region);
	char *cgi_success_region_base = region_id_to_base(cgi_success_region);
	char *cgi_fail_region_base = region_id_to_base(cgi_fail_region);
	socket_fd_region_base = region_id_to_base(socket_fd_region);
	response_region_base = region_id_to_base(response_region);

	/* CGI domains */
	cgi_success_dom_id = create_dom_from_region(cgi_success_region_base);
	cgi_fail_dom_id = create_dom_from_region(cgi_fail_region_base);

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
		cgi_success_dom_id, metadata_region, CAPSTONE_ANNOTATION_PERM_INOUT, CAPSTONE_ANNOTATION_REV_SHARED, 0, 0);
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE_ANNOTATED,
		cgi_fail_dom_id, metadata_region, CAPSTONE_ANNOTATION_PERM_INOUT, CAPSTONE_ANNOTATION_REV_SHARED, 0, 0);

	while (1) {
		unsigned long rv = 0;

		unsigned long html_fd_status;
		simple_memcpy(&html_fd_status, metadata_region_base + METADATA_STATUS_OFFSET, sizeof(html_fd_status));
		debug_shared_counter_inc(sizeof(unsigned long));

		switch (html_fd_status) {
			case HTML_FD_UNDEFINED:
				request_reprocessing();
				break;
			case HTML_FD_404RESPONSE:
				request_handle_404();
				break;
			case HTML_FD_200RESPONSE:
				request_handle_200();
				break;
			default:
				rv = -1;
				break;
		}

		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_RETURN,
			rv, 0, 0, 0, 0, 0);
	}
}

__attribute__((naked)) void _start() {
    __asm__ volatile ("mv sp, %0" :: "r"(stack + STACK_SIZE));
	__asm__ volatile ("j main");
}
