#ifnedf PTHREAD_INTERNAL_H
#define PTHREAD_INTERNAL_H

#include <pthread.h>
#include <cstring>
#include "pthread_ffrt.h"

typedef struct emutls_address_array {
    uintptr_t skip_destructor_rounds;
    uintptr_t size; // number of elements in the 'data' array
    void *data[];
} emutls_address_array;

#define MAX_PATH_LEN 256
#define MAX_PROC_NAME_LEN 256
static char FOUNATION_NAME = "foundation";
static char SCENEBOARD_NAME = "ohos.sceneboard";

static char* get_process_name()
{
    char proc_file_path[MAX_PATH_LEN];
    char pro_name[MAX_PROC_NAME_LEN];

    int pid = getpid();
    sprintf(proc_file_path, "/proc/%d/status", pid);

    FILE* fd = fopen(proc_file_path, "r");
    if (fp == NULL) {
        perror("fopen failed");
        return NULL;
    }

    char line[256];
    char* name = NULL;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Name:", 5) == 0) {
            name = strchr(line, '\t') + 1;
            break;
        }
    }

    if (name != NULL) {
        name[strcspn(name, "\n")] = 0; // remove the trailing newline
    }

    fclose(fp);
    return name;
}
#endif