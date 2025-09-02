#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p) {
    //input redirection
    if (p->in_file != NULL) {
        int in_fd = open(p->in_file, O_RDONLY);
        if (in_fd < 0) {
            perror("lsh: input file error");
            exit(EXIT_FAILURE);
        }
        if (dup2(in_fd, STDIN_FILENO) < 0) {
            perror("lsh: dup2 error on input");
            close(in_fd);
            exit(EXIT_FAILURE);
        }
        close(in_fd); 
    }

    //output redirection
    if (p->out_file != NULL) {
        int out_fd = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
        if (out_fd < 0) {
            perror("lsh: output file error");
            exit(EXIT_FAILURE);
        }
        if (dup2(out_fd, STDOUT_FILENO) < 0) {
            perror("lsh: dup2 error on output");
            close(out_fd);
            exit(EXIT_FAILURE);
        }
        close(out_fd);
    }
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
    pid_t pid = fork(); // Fork the process

    if (pid < 0) {
        // Fork failed
        perror("lsh: fork failed");
        return -1;
    }
    else if (pid == 0) {
        // In child process
        
        if (p->in_file != NULL) {
            FILE *in_file = fopen(p->in_file, "r");
            if (in_file == NULL) {
                perror("lsh: input file error");
                exit(EXIT_FAILURE);
            }
            dup2(fileno(in_file), STDIN_FILENO); // Redirect input
            fclose(in_file);
        }

        if (p->out_file != NULL) {
            FILE *out_file = fopen(p->out_file, "w");
            if (out_file == NULL) {
                perror("lsh: output file error");
                exit(EXIT_FAILURE);
            }
            dup2(fileno(out_file), STDOUT_FILENO); // Redirect output
            fclose(out_file);
        }

       
        char **args = p->args; 

        if (execvp(args[0], args) == -1) {
            perror("lsh"); 
            exit(EXIT_FAILURE); 
        }
    }
    else {
        // In parent process
        int status;
        waitpid(pid, &status, 0); // Wait
        return WEXITSTATUS(status);
    }

    return 1;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd) {
    int in = 0;  
    int fd[2];   
    struct cmd_node *current = cmd->head;
    int status = 0;

    while (current != NULL) {
        if (current->next != NULL) {
            if (pipe(fd) == -1) {
                perror("pipe");
                return -1;
            }
            current->out = fd[1];
            current->next->in = fd[0];
        } else {
            current->out = STDOUT_FILENO;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("lsh: fork failed");
            return -1;
        } else if (pid == 0) {
            // Child process
            if (current->in != STDIN_FILENO) {
                if (dup2(current->in, STDIN_FILENO) < 0) {
                    perror("lsh: dup2 input failed");
                    exit(EXIT_FAILURE);
                }
                close(current->in);
            }
            if (current->out != STDOUT_FILENO) {
                if (dup2(current->out, STDOUT_FILENO) < 0) {
                    perror("lsh: dup2 output failed");
                    exit(EXIT_FAILURE);
                }
                close(current->out);
            }

            
            char **args = current->args;
            if (execvp(args[0], args) == -1) {
                perror("lsh: command execution failed");
                exit(EXIT_FAILURE);
            }
        }

        // Parent process
        if (current->in != STDIN_FILENO) close(current->in);
        if (current->out != STDOUT_FILENO) close(current->out);

        
        current = current->next;
    }

    while (wait(&status) > 0);  // Waits

    return status;
}
//=============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
