#include <unistd.h>    // POSIX API (read, write, execve)
#include <sys/types.h> // data types used in system calls (pid_t)
#include <sys/wait.h>  // for waiti()
#include <stdlib.h>    // for memory allocation and process control
#include <string.h>    // string handling functions (strlen, strcmp, strtok)
#include <stdio.h>     // snprintf function, which prints formatted data to a string

#define MAX_ARGS 10    // max number of args
#define MAX_INPUT 1024 // max input command length

// write a string to stdout using write()
void print(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}

int main(int argc, char *argv[], char *envp[]) {
    char command[MAX_INPUT]; // buffer for command input from user
    char *args[MAX_ARGS];    // array to store pointers to individual command arguments
    int index; // index for tracking the position in the command buffer
    ssize_t bytes_read; // number of bytes read by the read() function
    char ch; // store a single character from input

    // continuously prompt the user for input until they type "exit"
    while (1) {
        print("DRE'S SHELL> ");  
        index = 0; // reset index for the new command input

        // read user input one character at a time using read()
        while ((bytes_read = read(STDIN_FILENO, &ch, 1)) > 0) {
            if (ch == '\n') {  // newline(end of command) will trigger command execution
                break;
            }
            // append the character to the command buffer if there is space
            if (index < MAX_INPUT - 1) {
                command[index++] = ch;
            }
        }
        command[index] = '\0'; // null-terminate the command string

        // if the user just pressed Enter (empty command), continue to the next prompt
        if (index == 0) {
            continue;
        }

        // if the user types 'exit', terminate the shell.
        if (strcmp(command, "exit") == 0) {
            print("Terminating shell...\n");
            break;
        }

        // tokenize the input command into separate arguments using space as delim
        // this converts the input string into an array of strings (args)
        int arg_count = 0;
        char *token = strtok(command, " ");
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL; // null terminate the arg list

        // fork a child process to execute the command
        pid_t pid = fork();
        if (pid < 0) {
            // if fork() fails, print an error message and continue with the next iteration
            print("fork failed\n");
            continue;
        }

        if (pid == 0) { // child process
            // check if the command provided is an absolute path (i.e., starts with '/')
            if (args[0][0] == '/') {
                // execute the command directly using execve() 
                execve(args[0], args, envp);
            } else {
                // the command is not an absolute path
                // search for the command in the directories listed in the $PATH environment variable
                char *path_env = NULL;
                // loop through the environment variables to find the PATH variable
                for (int i = 0; envp[i] != NULL; i++) {
                    // check if the current env variable(at index i in envp) starts with 'PATH='
                    if (strncmp(envp[i], "PATH=", 5) == 0) { 
                        // set path_env to the value of PATH, skipping the "PATH=" part
                        path_env = envp[i] + 5; 
                        break;
                    }
                }
                // if the PATH variable is not set, print an error message and exit
                if (!path_env) {
                    print("PATH not set\n");
                    _exit(1);
                }

                // duplicate the PATH string so that strtok() doesn't modify the original
                char *path_dup = strdup(path_env);
                if (!path_dup) {
                    print("Memory allocation error\n");
                    _exit(1);
                }
                
                // tokenize the duplicated PATH string using ':' as the delimiter
                // this lets us check each directory in the PATH for the command
                char *dir = strtok(path_dup, ":");
                char executablePath[MAX_INPUT]; // buffer to construct the executable path
                int found = 0; // store whether a valid command was found in path

                // iterate over each directory in PATH
                while (dir != NULL) {
                    // make the executable path: directory + "/" + command
                    snprintf(executablePath, sizeof(executablePath), "%s/%s", dir, args[0]);
                    // check if the executable path is executable by using access() to test file permissions
                    // X_OK flag checks for execute permission
                    // if access() returns 0, it means the file has execute permissions for curr user
                    if (access(executablePath, X_OK) == 0) {
                        found = 1;
                        // execute the command at the executable path using execve()
                        execve(executablePath, args, envp);
                        break;  // if execve returns, an error occurred
                    }
                    // get the next directory in PATH
                    dir = strtok(NULL, ":");
                }
                // free the duplicated PATH string
                free(path_dup);
                // if no executable was found in any directory, print an error message
                if (!found) {
                    print("command not found\n");
                }
            }
            // exit the child process if execve fails
            _exit(1);
        } else { // parent process
            // wait for the child to terminate before printing the next prompt.
            int cp = wait(NULL);
        }
    }

    return 0;
}
