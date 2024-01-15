#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include "miniweb_backend.smode.h"

static char stack[4096];
static char *socket_fd_region_base;
static char *html_fd_region_base;
static char *cgi_success_region_base;
// static char *cgi_fail_region_base;

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

static void request_handle_cgi(unsigned long register_success) {
	unsigned long html_fd_status = HTML_FD_CGI;

	if (register_success == 1) {
		// TODO: domain creation and call
	}
	else if (register_success == 0) {
		// TODO: domain creation and call
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

	socket_fd_region_ptr = socket_fd_region_base + sizeof(unsigned long);
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
	memcpy(socket_fd_region_base, html_fd_region_base + sizeof(unsigned long), sizeof(html_fd_len) + html_fd_len);
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
	unsigned long socket_fd_len = buffer_size + html_fd_len;

	memcpy(socket_fd_region_base, &socket_fd_len, sizeof(socket_fd_len));
	memcpy(socket_fd_region_base + sizeof(socket_fd_len), buffer, buffer_size);
	memcpy(socket_fd_region_base + sizeof(socket_fd_len) + buffer_size, html_fd_region_base + sizeof(unsigned long) + sizeof(html_fd_len), html_fd_len);
}

static void main(void) {
	/* set up region id and pointers */
	struct sbiret sbi_res = sbi_ecall(SBI_EXT_CAPSTONE, SBI_EXT_CAPSTONE_REGION_COUNT,
		0, 0, 0, 0, 0, 0);
	const region_id_t region_n = sbi_res.value;
	// region_id_t cgi_fail_region = region_n - 1;
	// region_id_t cgi_success_region = cgi_fail_region - 1;
	region_id_t cgi_success_region = region_n - 1;
	region_id_t html_fd_region = cgi_success_region - 1;
	region_id_t socket_fd_region = html_fd_region - 1;

	socket_fd_region_base = region_id_to_base(socket_fd_region);
	html_fd_region_base = region_id_to_base(html_fd_region);
	cgi_success_region_base = region_id_to_base(cgi_success_region);
	// cgi_fail_region_base = region_id_to_base(cgi_fail_region);

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
