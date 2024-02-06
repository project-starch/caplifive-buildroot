#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lib/libcapstone.h"

#define print_nobuf(...) do { printf(__VA_ARGS__); fflush(stdout); } while(0)

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Please provide the domain file name!\n");
        return 1;
    }

    char* file_name = argv[1];
    
    capstone_init();

    dom_id_t dom_id = create_dom(file_name, NULL);
    print_nobuf("SBI domain created with ID %lu\n", dom_id);

    call_dom(dom_id);

    capstone_cleanup();

    return 0;
}
