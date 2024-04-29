/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PTHREAD_INTERNAL_H
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
static char FOUNDATION_NAME[] = "foundation";
static char SCENEBOARD_NAME[] = "ohos.sceneboard";

static char* get_process_name()
{
    char proc_file_path[MAX_PATH_LEN];
    char proc_name[MAX_PROC_NAME_LEN];

    int pid = getpid();
    sprintf(proc_file_path, "/proc/%d/status", pid);

    FILE* fp = fopen(proc_file_path, "r");
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