#include "commons.h"

static int dev_fd;
static dom_id_t dom_id;

static struct client_domain *client_domains = NULL;

struct client_domain *
get_client_domains(void)
{
    return client_domains;
}

void
set_client_domains(struct client_domain *__client_domains)
{
    client_domains = __client_domains;
}

int
get_dev_fd(void)
{
    return dev_fd;
}

void
set_dev_fd(int __fd)
{
    dev_fd = __fd;
}

void
set_domain_id(dom_id_t __id)
{
    dom_id = __id;
}

dom_id_t
get_domain_id(void)
{
    return dom_id;
}
