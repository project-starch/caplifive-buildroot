#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/elf.h>
#include "miniweb_backend.smode.h"

static char stack[4096];

static region_id_t socket_fd_region;
static dom_id_t cgi_success_dom_id;
static dom_id_t cgi_fail_dom_id;

static char *socket_fd_region_base;
static char *html_fd_region_base;
static char *cgi_success_region_base;
static char *cgi_fail_region_base;

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

	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CREATE,
		/* base paddr = */ code_start,
		/* code size = */ code_len,
		/* tot size = */ tot_len,
		/* entry offset = */ entry_offset,
		0, 0);
	
	return (dom_id_t)sbi_res.value;
}

static void request_handle_cgi(unsigned long register_success) {
	unsigned long html_fd_status = HTML_FD_CGI;

	if (register_success == 1) {
		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL,
			cgi_success_dom_id, 0, 0, 0, 0, 0);
	}
	else if (register_success == 0) {
		sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_DOM_CALL,
			cgi_fail_dom_id, 0, 0, 0, 0, 0);
	}
	else {
		html_fd_status = HTML_FD_404RESPONSE;
	}
	memcpy(html_fd_region_base, &html_fd_status, sizeof(html_fd_status));
}

static void handle_404_preprocessing(void) {
	unsigned long html_fd_status = HTML_FD_404RESPONSE;
	memcpy(html_fd_region_base, &html_fd_status, sizeof(html_fd_status));
}

static void request_reprocessing(void) {
	char lineBuffer[256];

	socket_fd_region_ptr = socket_fd_region_base + sizeof(unsigned long long);
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

	if (strcmp(method, "POST") == 0) {
		if (strncmp(url, "/cgi/", 5) == 0) {
			if (strncmp(url, "/cgi/cgi_register_success.dom", 29) == 0) {
				request_handle_cgi(1);
				return;
			}
			else if (strncmp(url, "/cgi/cgi_register_fail.dom", 26) == 0){
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
		memcpy(html_fd_region_base, &html_fd_status, sizeof(html_fd_status));
		unsigned long html_fd_len = strlen(url) + 1;
		memcpy(html_fd_region_base + sizeof(html_fd_status), &html_fd_len, sizeof(html_fd_len));
		memcpy(html_fd_region_base + sizeof(html_fd_status) + sizeof(html_fd_len), url, html_fd_len);
		return;
	}
}

static void request_handle_404(void) {
	unsigned long html_fd_len;
	memcpy(&html_fd_len, html_fd_region_base + sizeof(unsigned long), sizeof(html_fd_len));
	unsigned long long socket_fd_len = (unsigned long long)html_fd_len;
	memcpy(socket_fd_region_base, &socket_fd_len, sizeof(socket_fd_len));
	memcpy(socket_fd_region_base + sizeof(socket_fd_len), html_fd_region_base + 2 * sizeof(unsigned long), html_fd_len);
}

static void request_handle_200(void) {
	const char* responseStatus = "HTTP/1.1 200 OK\n";
	const char* responseOther = "Connection: close\nContent-Type: text/html\n";
	const char* responseLen = "Content-Length: ";
	unsigned long html_fd_len;
	memcpy(&html_fd_len, html_fd_region_base + sizeof(unsigned long), sizeof(html_fd_len));

	char buffer[256];
	snprintf(buffer, 256, "%s%s%s%lu\n\n", responseStatus, responseOther, responseLen, html_fd_len);
	unsigned long buffer_size = strlen(buffer);
	unsigned long long socket_fd_len = buffer_size + html_fd_len;

	memcpy(socket_fd_region_base, &socket_fd_len, sizeof(socket_fd_len));
	memcpy(socket_fd_region_base + sizeof(socket_fd_len), buffer, buffer_size);
	memcpy(socket_fd_region_base + sizeof(socket_fd_len) + buffer_size, html_fd_region_base + sizeof(unsigned long) + sizeof(html_fd_len), html_fd_len);
}

static void main(void) {
	/* set up region id and pointers */
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	region_id_t cgi_fail_region = region_n - 1;
	region_id_t cgi_success_region = cgi_fail_region - 1;
	region_id_t html_fd_region = cgi_success_region - 1;
	socket_fd_region = html_fd_region - 1;

	socket_fd_region_base = region_id_to_base(socket_fd_region);
	html_fd_region_base = region_id_to_base(html_fd_region);
	cgi_success_region_base = region_id_to_base(cgi_success_region);
	cgi_fail_region_base = region_id_to_base(cgi_fail_region);

	/* CGI domains */
	// note that this part of code can only be executed once in multiple domain re-entry 
	dom_id_t cgi_success_dom_id = create_dom_from_region(cgi_success_region_base);
	dom_id_t cgi_fail_dom_id = create_dom_from_region(cgi_fail_region_base);

	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE,
			cgi_success_dom_id, socket_fd_region, 0, 0, 0, 0);
	sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_SHARE,
			cgi_fail_dom_id, socket_fd_region, 0, 0, 0, 0);

	while (1) {
		unsigned long rv = 0;

		unsigned long html_fd_status;
		memcpy(&html_fd_status, html_fd_region_base, sizeof(html_fd_status));

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

static __attribute__((naked)) int __init miniweb_backend_init(void)
{
	__asm__ volatile ("mv sp, %0" :: "r"(stack + 4096));
	main();

	return 0;
}

static void __exit miniweb_backend_exit(void)
{
	return;
}

module_init(miniweb_backend_init);
module_exit(miniweb_backend_exit);

MODULE_LICENSE("GPL");
