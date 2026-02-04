#ifndef KASA_IPC_H
#define KASA_IPC_H

int  kasaq_create_clean(void);   /* kasa queue (czyści stare) */
void kasaq_remove(int msgid);

#endif