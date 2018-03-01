// Aishabibi Adambek - HW3

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define BUFFLEN 1024
#define ARGNUM 100

// To check the existence/permissions of the file
void check_permissions(char *filename){
	if(access(filename, F_OK ) != 0){
		printf("Specified file does not exist. Check your input.\n");
		exit(-1);
	}
	if(access(filename, R_OK) != 0 || access(filename, W_OK) != 0){
		printf("Read or/and Write permissions denied. Check your input.\n");
		exit(-1);
	}
}

// A structure to hold the string of arguments for different sides of all <=10 pipes
struct command{
	char **argv;
};

/*	The function to handle the individual pair of command structs 
	- one as in for pipe, the other as out for pipe.
	Pass as in the input from which cmd should read 
	AND pass as out the output to which cmd should write.
*/
int proc (int in, int out, struct command *cmd) {
  int pid = fork(); // fork to make each cmd struct run as child process AND to control its in and out
  if (pid == 0){
    if (in != 0) {
        dup2(in, 0); // redirect input to be accepted from in
        close(in);
   	}
	if (out != 1) {
	    dup2(out, 1); // redirect output to be sent to out
	    close(out);
   	}
    return execvp(cmd->argv[0], (char * const *)cmd->argv); // execute the cmd struct with new in AND out
  }

  return pid;
}

/*	Spawn all process in a row using helper function proc [see above]
	EXCEPT for the last one, whose output should go to the stdout and provided backup file.
	Accepts pip[2] as an argument to allow it to communicate with parent,
			pnum to know the number of pipes to handle,
			struct cmd to know the command sets to handle.
*/
int fkps(int pip[2], int pnum, struct command *cmd){
	int pid, in, k;

	in = 0; // For the first pipe, the input is stndin

	for (k = 0; k < pnum - 1; k++) {
		if(pipe(pip) < 0) {
			printf("Error in pipe.\n");
			exit(-1);
		}
		proc(in, pip[1], cmd + k); // Make the cmd + K struct read from in, write to pip[1]
		close(pip[1]);
		in = pip[0]; // For the next cmd struct
	}

	// Handle the last cmd struct
	if(in != 0) dup2(in, 0);
	dup2(pip[1], 1);
	return execvp(cmd[k].argv[0], (char * const *)cmd[k].argv); // for the last cmd group
}


int main(int argc, char *argv[]) {
	int pid, status, i, j, fdin, in, out, apn, point, pcnt, count;
	int fd[2], pip[2];						// pip - for cases with |, fd - for others
	char buffer[1024]; 						// to get command line
	char *args[ARGNUM]; 					// to store up to 100 arguments
	int background, pipesNum;
	char *str1 = "Program begins.\n";
	char *str2 = "\n\nPlease enter a command:   ";
	char *input, *output, *appnd;		// for I/O redirection's 3 cases - input, output, append
	struct command *cmd; 				// to store structs of commands that come with pipes |
	char **bf, **bf1;					// to handle pipes |

	printf( "%s", str1 );

	if(argc == 2) {
		check_permissions(argv[1]);
		fdin = open(argv[1], O_RDWR | O_APPEND);
	} else {
		fdin = open("output.txt", O_RDWR | O_APPEND | O_CREAT, 0770); // open with RWX permissions
	}
	write(fdin, str1, strlen(str1));
	
	for (;;) {
		in = 0, apn = 0, out = 0, background = 0, pipesNum = 0, point = 0, count = 0, pcnt = 0;
		printf( "%s", str2 );
		if(strcmp(fgets( buffer, BUFFLEN, stdin ), "\n") == 0) {
			printf("Empty input not allowed. \n");
			continue;
		}
		buffer[strlen(buffer) - 1] = '\0';
		if(strcmp(buffer, "quit") == 0) break;

		write(fdin, str2, strlen(str2));
		write(fdin, buffer, strlen(buffer));
		write(fdin, "\n", strlen("\n"));

	    char *token;
	    token = strtok(buffer, " ");
	    int i = 0;
	    while(token != NULL){ 
	        args[i] = token; 
	        token = strtok(NULL, " ");  
	        i++;   
	    }
	    args[i] = NULL; 										// For execvp

    	if(strcmp(args[i - 1], "&") == 0) {						// For background commands
			args[i - 1] = NULL;
			background = 1;
		} 

		/* 	As we null out the entries w >,<,>> and their corresponding filenames,
			we need to keep track of the number of elements we 'erase'
			from the original array of commands
		*/
		int r = 0;

		for(int j = 0; j < i - 1; j++){

			if(strcmp(args[j], "<") == 0) {
				input = args[j + 1];
				if((in = open(input, O_RDONLY)) < 0) {
					printf("Check your inputs. \n");
					exit(-1);
				}
				args[j] = NULL;
				args[j + 1] = NULL;
				j += 2;
				r += 2;
			}

			if(j >= i) break;

			if(strcmp(args[j], ">") == 0) {
				output = args[j + 1];
				if((out = open(output, O_RDWR)) < 0) {
					out = open(output, O_RDWR | O_CREAT, 0770);
				}
				args[j] = NULL;
				args[j + 1] = NULL;
				j += 2;
				r += 2;
			}

			if(j >= i) break;

			if(strcmp(args[j], ">>") == 0) {
				appnd = args[j + 1];
				if((apn = open(appnd, O_RDWR | O_APPEND)) < 0) {
					apn = open(appnd, O_RDWR | O_APPEND | O_CREAT, 0770);
				}
				args[j] = NULL;
				args[j + 1] = NULL;
				j += 2;
				r += 2;
			}

			if(j >= i) break;

			if(strcmp(args[j], "|") == 0) {
				pipesNum++;
			}

		}

		// To handle pipes
		if(pipesNum > 0){
			cmd = (struct command *) malloc((pipesNum + 1) * sizeof(struct command));
			for(int j = 0; j < i; j++){
				if(strcmp(args[j], "|") == 0) {
					bf = (char **) malloc(100 * sizeof(char *));
					for(int a = j, y = 0; a > point; a--){
						if(strcmp(args[j - a + point], "|") != 0) {
							bf[y] = args[j - a + point];
							y++;
						}
					}
					point = j;
					struct command x = { .argv = bf };
					cmd[count] = x;

					count++;
					pcnt++;

					if(pcnt == pipesNum){
						bf1 = (char **) malloc(100 * sizeof(char *));
						for(int y = 0; j < i; j++){
							if(strcmp(args[j], "|") != 0) {
								bf1[y] = args[j];
								y++;
							}
						}
						struct command x = { .argv = bf1 };
						cmd[count] = x;

						count++;
						break;
					}
				}
			}
		}

		i = i - r;	// Decrease the number of arguments by the number of enries we nulled out

		if(pipe(fd) < 0 || pipe(pip) < 0) {
			printf("Pipe failed.\n");
			exit(-1);
		}
		pid = fork();
		if ( pid < 0 ) {
			printf( "Error in fork.\n" );
			exit(-1);
		}
		if ( pid != 0 ) {
			if(background == 1) {									// For background commands
				printf("Process started: [%d]\n\n", pid);
				write(fdin, "Process started.\n", strlen("Process started.\n"));
				continue;
			} else {
				waitpid( pid, &status, 0 );
				char ch;
				close(fd[1]);

				if(pipesNum > 0){ // (3) To manage the output of the last cmd struct
					close(pip[1]);
					while(read(pip[0], &ch, 1) > 0){
						write(fdin, &ch, 1);
						write(1, &ch, 1);
					}
				} else if(out > 0){
					while(read(fd[0], &ch, 1) > 0){
						write(out, &ch, 1);
					}
					close(out);
				}else if(apn > 0){
					while(read(fd[0], &ch, 1) > 0){
						write(apn, &ch, 1);
					}
					close(apn);
				}else {
					while(read(fd[0], &ch, 1) > 0){
						write(fdin, &ch, 1);
						write(1, &ch, 1);
					}
				}
			}
		} else {
			if(pipesNum == 0){
			
				close(fd[0]);
				dup2(fd[1], 1);
				close(fd[1]);
				
				if(in > 0){ // Get the input from the input file if any
					dup2(in, 0);
				}

				if(i == 1) {
					if(execlp(args[0], args[0], NULL) < 0) {
						printf("Invalid command. Try again.\n");
						exit(-1);
					}
				} else {
					if(execvp(args[0], args) < 0) {
						printf("Invalid command. Try again.\n");
						exit(-1);
					}
				}
			} else {
				close(pip[0]);
				dup2(pip[1], 1);
				close(pip[1]);
				return fkps(pip, pipesNum + 1, cmd);
			}
		}
		
		if(in > 0) close(in);
		if(pipesNum > 0) {
			free(cmd);
			free(bf);
			free(bf1);
		}

	}
	return 0;
}
