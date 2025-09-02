#include "sender.h"

#define SHM_SIZE 1024
#define MAX_TEXT 512
#define SEM_NAME "/sem_example"

void send(message_t message, mailbox_t *mailbox_ptr) {
    if (mailbox_ptr->flag == 1) {
        msgsnd(mailbox_ptr->storage.msqid, &message, sizeof(message.mtext), 0);
    } else if (mailbox_ptr->flag == 2) {
        strcpy(mailbox_ptr->storage.shm_addr, message.mtext);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <mechanism: 1 for Message Passing, 2 for Shared Memory> <input_file>\n", argv[0]);
        return 1;
    }

    int mechanism = atoi(argv[1]);
    char *input_file = argv[2];

    mailbox_t mailbox;
    message_t message;

    mailbox.flag = mechanism;

    if (mechanism == 1) {
        key_t key = ftok("msgqueue", 65);
        mailbox.storage.msqid = msgget(key, 0666 | IPC_CREAT);
    } else if (mechanism == 2) {
        key_t key = ftok("shmfile", 65);
        int shmid = shmget(key, SHM_SIZE, 0666 | IPC_CREAT);
        mailbox.storage.shm_addr = (char *)shmat(shmid, NULL, 0);
    }

    
    sem_t* receive_sem = sem_open("/receive_sem", O_CREAT, 0644, 0);
    sem_t* send_sem = sem_open("/send_sem", O_CREAT, 0644, 0);


    struct timespec start, end;
    

    FILE *file = fopen(input_file, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }
    printf("Message Passing\n");
    double elapsed_time;

    while (fgets(message.mtext, sizeof(message.mtext), file) != NULL) {
        message.mtype = 1; // set message type
        send(message, &mailbox);
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed_time += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
        
        printf("Sending message: %s\n", message.mtext);
        sem_post(receive_sem);
        
        
        sem_wait(send_sem);
    }

    
    strcpy(message.mtext, "exit");
    printf("End of input file! exit!\n");
    send(message, &mailbox);
    sem_post(receive_sem);

    fclose(file);

    printf("Total time taken in sending msg: %f s\n", elapsed_time);

   
    sem_close(receive_sem);
    sem_close(send_sem);

    return 0;
}
