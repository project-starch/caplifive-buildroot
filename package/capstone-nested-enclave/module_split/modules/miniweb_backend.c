#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>

#define HTML_FD_UNDEFINED 0
#define HTML_FD_404RESPONSE 1
#define HTML_FD_200RESPONSE 2
#define HTML_FD_CGI 3

#define MODULE_NAME "nested"
#define SHARED_MEMORY_SIZE (4096 * 2)
#define HTML_FD_OFFSET 4096

static char *shared_kernel_memory;

static char *socket_fd_region_base;
static char *html_fd_region_base;

extern void cgi_register_success(char *region_base);
extern void cgi_register_fail(char *region_base);

static int nested_open(struct inode *inode, struct file *file) {
    return 0;
}

static int nested_mmap(struct file *file, struct vm_area_struct *vma) {
    unsigned long pfn = vmalloc_to_pfn(shared_kernel_memory);

    if (remap_pfn_range(vma, vma->vm_start, pfn, SHARED_MEMORY_SIZE, vma->vm_page_prot)) {
        pr_err("Failed to remap memory\n");
        return -EIO;
    }

    pr_info("mmap successful\n");
    return 0;
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
		cgi_register_success(socket_fd_region_base);
	}
	else if (register_success == 0) {
		cgi_register_fail(socket_fd_region_base);
	}
	else {
		html_fd_status = HTML_FD_404RESPONSE;
	}
	*((unsigned long *)html_fd_region_base) = html_fd_status;
}

static void handle_404_preprocessing(void) {
	unsigned long html_fd_status = HTML_FD_404RESPONSE;
	*((unsigned long *)html_fd_region_base) = html_fd_status;
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
	unsigned long url_length = j - 1;

	if (strcmp(method, "POST") == 0) {
		if (strncmp(url, "/cgi/", 5) == 0) {
			if (strncmp(url, "/cgi/cgi_register_success", 25) == 0) {
				request_handle_cgi(1);
				return;
			}
			else if (strncmp(url, "/cgi/cgi_register_fail", 22) == 0){
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
		unsigned long html_fd_len = url_length + 1;
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

static void backend_entry(void) {
    unsigned long html_fd_status = *((unsigned long *)html_fd_region_base);

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
            break;
    }
}

static long nested_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
    switch (ioctl_num) {
        case 0:
            backend_entry();
            return 0;
        default:
            return -EINVAL;
    }
}

static struct file_operations nested_fops = {
    .open = nested_open,
    .mmap = nested_mmap,
    .unlocked_ioctl = nested_ioctl,
};

static struct miscdevice nested_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODULE_NAME,
	.fops = &nested_fops,
	.mode = 0666,
};

static int __init nested_init(void) {
	shared_kernel_memory = kmalloc(SHARED_MEMORY_SIZE, GFP_DMA);
	if (!shared_kernel_memory) {
		pr_err("Failed to allocate kernel buffer.\n");
		return -ENOMEM;
	}

    socket_fd_region_base = shared_kernel_memory;
    html_fd_region_base = socket_fd_region_base + HTML_FD_OFFSET;

	if (misc_register(&nested_dev)) {
		pr_alert("Failed to register device\n");
        vfree(shared_kernel_memory);
		return -EIO;
	}

    pr_info("NESTED: Module loaded.\n");
    return 0;
}

static void __exit nested_exit(void) {
    unregister_chrdev(0, MODULE_NAME);
    vfree(shared_kernel_memory);

    pr_info("NESTED: Module unloaded.\n");
}

module_init(nested_init);
module_exit(nested_exit);

MODULE_LICENSE("GPL");
