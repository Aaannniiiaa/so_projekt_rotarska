#ifndef IPC_H
#define IPC_H

#include <sys/types.h>
#include <sys/ipc.h>

key_t ipc_key(char proj);

#endif