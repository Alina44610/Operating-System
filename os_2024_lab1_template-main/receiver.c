#include "receiver.h"

#define SHM_SIZE 1024
#define MAX_TEXT 512
#define SEM_NAME "/sem_example"


void receive(message_t *message_ptr, mailbox_t *mailbox_ptr) {
    if (mailbox_ptr->flag == 1) {
        msgrcv(mailbox_ptr->storage.msqid, message_ptr, sizeof(message_ptr->mtext), 1, 0);
    } else if (mailbox_ptr->flag == 2) {
        strcpy(message_ptr->mtext, mailbox_ptr->storage.shm_addr);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <mechanism: 1 for Message Passing, 2 for Shared Memory>\n", argv[0]);
        return 1;
    }

    int mechanism = atoi(argv[1]);
    mailbox_t mailbox;
    message_t message;

    mailbox.flag = mechanism;
    int shmid;
    if (mechanism == 1) {
        key_t key = ftok("msgqueue", 65);
        mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);
    } else if (mechanism == 2) {
        key_t key = ftok("shmfile", 65);
        shmid = shmget(key, SHM_SIZE, 0666 | IPC_CREAT);
        mailbox.storage.shm_addr = (char *)shmat(shmid, NULL, 0);
    }

    
    sem_t* receive_sem = sem_open("/receive_sem", O_CREAT, 0644, 0);
    sem_t* send_sem = sem_open("/send_sem", O_CREAT, 0644, 0);
    double elapsed_time;



    struct timespec start, end;
    
    printf("Message Passing\n");
    while (1) {
        if(sem_wait(receive_sem)   == 0){
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        }
        if (strcmp(message.mtext, "exit") == 0) {
            printf("Sender exit!\n");
            break;
        }
        elapsed_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        printf("Receiving message: %s\n", message.mtext);
        sem_post(send_sem);
    }
        printf("Total time taken in recieving msg: %f s\n", elapsed_time);
    
    


    // 清理資源
    if (mechanism == 1) {
        msgctl(mailbox.storage.msqid, IPC_RMID, NULL); 
    } else if (mechanism == 2) {
        shmdt(mailbox.storage.shm_addr); 
        shmctl(shmid, IPC_RMID, NULL);   
    }

    
    sem_close(receive_sem);
    sem_close(send_sem);

    return 0;
}
