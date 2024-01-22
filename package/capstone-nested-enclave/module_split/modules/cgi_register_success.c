#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/kernel.h>

#define SIZE_OF_ULL 8

void cgi_register_success(char *socket_fd_region_base) {
	char name[16];
    char* socket_region_ptr = socket_fd_region_base + SIZE_OF_ULL;

    /* parsing the packet and find the name */
    int i = 0;
    int name_index = 0;
    while (socket_region_ptr[i] != '=') {
        i += 1;
    }
    while (socket_region_ptr[i] != '&') {
        name[name_index] = socket_region_ptr[i];
        i += 1;
        name_index += 1;
    }
    name[name_index] = '\0';

	/* prepare the packet */
	char content[1024];
    sprintf(content,"<!DOCTYPE html>\n");
    sprintf(content,"%s<head>\n",content);
    sprintf(content,"%s<title>Registration successful</title>\n",content);
    sprintf(content,"%s</head>\n",content);
    sprintf(content,"%s<body>\n",content);
    sprintf(content,"%s<p>Hello, %s. You are now registered.<\\p>\n",content,name);
    sprintf(content,"%s</body>",content);

    sprintf(socket_region_ptr, "HTTP/1.1 200 OK\n");
    sprintf(socket_region_ptr, "%sConnection: close\n", socket_region_ptr);
    sprintf(socket_region_ptr, "%sContent-length: %d\n", socket_region_ptr, (int)strlen(content));
    sprintf(socket_region_ptr, "%sContent-type: text/html\n\n", socket_region_ptr);
    sprintf(socket_region_ptr, "%s%s", socket_region_ptr, content);

	*(unsigned long long *)socket_fd_region_base = strlen(socket_region_ptr);
}
EXPORT_SYMBOL(cgi_register_success);

static int __init cgi_register_success_init(void)
{
	printk(KERN_INFO "Entering CGI Register Success module.");

	return 0;
}

static void __exit cgi_register_success_exit(void)
{
	printk(KERN_INFO "Exiting CGI Register Success module.");
}

module_init(cgi_register_success_init);
module_exit(cgi_register_success_exit);

MODULE_LICENSE("GPL");
