#include <unistd.h>    // POSIX API (read, write, execve)
#include <sys/types.h> // data types used in system calls (pid_t)
#include <sys/wait.h>  // for waiti()
#include <stdlib.h>    // for memory allocation and process control
#include <string.h>    // string handling functions (strlen, strcmp, strtok)
#include <stdio.h>     // snprintf function, which prints formatted data to a string
#include <fcntl.h>

#define MAX_ARGS 10    // max number of args
#define MAX_INPUT 1024 // max input command length

// TO DO: 
    // - explain the difference between using exit() and _exit(), explain why exit() terminates ur shell after running python program and why _exit() doesnt terminate ur shell











// TESTING COMMANDS:
    // python demos/p1-fork.py
    // python demos/p1-fork.py > demos/test.txt
    // wc < demos/test.txt
    // grep I demos/hi.txt | sort



// write a string to stdout using write()
void print(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}


void executeCommand(char *args[MAX_ARGS], char *envp[]){
    // check if the command provided is an absolute path (starts with '/')
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
            print("PATH env variable not found\n");
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
}


// closes stdout and sets fd 1 to users inputted file then executes command with executeCommand()
void handleRedirection(char *args[MAX_ARGS], char *envp[]){

    // store the index of redirection operator if found, set to -1 for default value, meaning not found
    int redirectionIndex = -1;

    int redirectionFlag = -1; // flag: 0 for '>', 1 for '<'

    // iterate through args array to find redirection operator, '>'
    for(int i = 0; args[i]!= NULL; i++){
        if(strcmp(args[i], ">") == 0){
            redirectionIndex = i;
            redirectionFlag = 0;
            break;
        }
        else if(strcmp(args[i], "<") == 0){
            redirectionIndex = i;
            redirectionFlag = 1;
            break;
        }
    }
    char *file = NULL; // store file path that were reading from or writing to
    if(redirectionIndex >= 0){ //redirection operator was found

        // the next arg after redirection operator should be the destination file
        file = args[redirectionIndex + 1];

        // remove the redirection operator and the file from the arguments
        args[redirectionIndex] = NULL; // null terminate the args array 
        if (file == NULL) {
            print("No file provided for redirection\n");
            _exit(1);
        }
        
        if(redirectionFlag==0){ // redirection operator is '>'
            close(1); // close stdout (fd 1)

            // open the file with write permissions, create it if necessary, and overwrite if necessary
            // Since fd 1 is closed, open() should assign the lowest available fd (which will be 1)
            int fd = open(file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
            if (fd < 0) {
                print("Failed to open destination file for redirection\n");
                _exit(1);
            }
        }
        else{ // redirection flag is '<'
            close(0); // close stdin (fd 0)
            int fd = open(file, O_RDONLY); // open the file for reading
            if (fd < 0) {
                print("Failed to open file for input redirection\n");
                _exit(1);
            }
        }

        // execute the command after setting up redirection
        executeCommand(args, envp);
    }
}

void handlePipe(char *args[MAX_ARGS], char *envp[]){
    // store the index of pipe operator if found, set to -1 for default value, meaning not found
    int pipeIndex = -1;
    // iterate through args array to find pipe operator, '|'
    for(int i = 0; args[i]!= NULL; i++){
        if(strcmp(args[i], "|") == 0){
            pipeIndex = i;
            break;
        }
    }
    // no pipe operator found, execute normally
    if (pipeIndex == -1) {
        executeCommand(args, envp);
        return;
    }
    char *rightCommand[MAX_ARGS]; // right side of pipe
    char *leftCommand[MAX_ARGS]; // left side of pipe

    // copy tokens for the left command (everything before '|')
    int i; // i will be used to iterate through args
    for (i = 0; i < pipeIndex; i++){
        leftCommand[i] = args[i];
    }
    leftCommand[i] = NULL; // null terminate the left command array

    // copy tokens for the right command (everything after '|')
    int j = 0; // index to iterate through indexes of right cmd
    for(int i = pipeIndex + 1; args[i]!= NULL; i++){ // i will iterate through args
        rightCommand[j++] = args[i];
    }
    rightCommand[j]=NULL; // null terminate the right command array
    

    
    int *pipeFds;
    pipeFds = (int *) calloc(2, sizeof(int));
    if (pipe(pipeFds) == -1) {
        print("Pipe failed\n");
        _exit(1);
    }

    int leftCmdPid;
    leftCmdPid = fork();
    if (leftCmdPid < 0) {
        print("Fork failed for left command\n");
        _exit(1);
    }

    if(leftCmdPid == 0){ // child process for left command
        close(1); // close stdout fd
        dup(pipeFds[1]); // dup our write fd from pipefds and assign it to fd #1
        close(pipeFds[0]); close(pipeFds[1]); // close pipefds
        // output will be ridirected to write fd in pipefds
        executeCommand(leftCommand, envp); // execute left command
        exit(2);
    }
    

    // fork the second child for the right command
    int rightCmdPid;
    rightCmdPid = fork();
    if (rightCmdPid < 0) {
        print("Fork failed for right command\n");
        _exit(1);
    }
    
    if(rightCmdPid == 0){ // child process for left command
        close(0); // close stdin
        dup(pipeFds[0]); // dup our read fd from pipefds and assign it to fd #0
        close(pipeFds[0]); close(pipeFds[1]); // close pipefds
        // command will now take input from read fd in pipefds
        executeCommand(rightCommand, envp); // execute left command, output will be ridirected
        exit(2);
    }
   
    close(pipeFds[0]); close(pipeFds[1]); // close pipefds

    // wait for both children
    waitpid(leftCmdPid, NULL, 0);
    waitpid(rightCmdPid, NULL, 0);
}




int main(int argc, char *argv[], char *envp[]) {
    char command[MAX_INPUT]; // buffer for command input from user
    char *args[MAX_ARGS];    // array to store pointers to individual command arguments
    int index; // index for tracking the position in the command buffer
    ssize_t bytes_read; // number of bytes read by the read() function
    char ch; // store a single character from input

    // continuously prompt the user for input until they type "exit"
    while (1) {
        print("ANDRE'S SHELL: ");  
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

        // if the first argument is a cd command, we'll call chdir() in parent process
        // we want to stay in the parent process because forking a child would only affect the childs directory
        if (strcmp(args[0], "cd") == 0) { 
            char *home = NULL; // string to store the current home directory
            for (int i = 0; envp[i] != NULL; i++) { // iterate through envp to find "HOME="
                if (strncmp(envp[i], "HOME=", 5) == 0) {
                    home = envp[i] + 5; // skip the "HOME=" part
                    break;
                }
            }
            // if no directory argument is provided, default to HOME from envp
            if (args[1] == NULL) {
                if (home == NULL) {
                    print("cd: HOME not set\n");
                } else if (chdir(home) != 0) {
                    print("cd error\n");
                }
            } else {
                // change directory to the path specified by user
                if (chdir(args[1]) != 0) {
                    print("cd error, directory does not exist\n");
                }
            }
            // break out of loop and continue to next prompt without forking a child process
            continue; // skip the fork in code below
        }



        // fork a child process to execute the command
        pid_t pid = fork();
        if (pid < 0) {
            // if fork() fails, print an error message and continue with the next iteration
            print("fork failed\n");
            continue;
        }

        if (pid == 0) { // child process
            int hasPipe = 0; // store if pipe operator was found
            // traverse chars in command and look for pipe operator
            for (int i = 0; args[i] != NULL; i++){
                if (strcmp(args[i], "|") == 0) {
                    hasPipe = 1;
                    break;
                }
            }
            // if a pipe was found, call handlePipe()
            if(hasPipe) {
                handlePipe(args, envp);
            }
            else{ // if no pipe was found, look for redirection
                // check if the command includes redirection
                int hasRedirection = 0;
                for (int i = 0; args[i] != NULL; i++) {
                    if (strcmp(args[i], ">") == 0 || strcmp(args[i], "<") == 0) {
                        hasRedirection = 1;
                        break;
                    }
                }
                if (hasRedirection) {
                    // handle redirection if found
                    handleRedirection(args, envp);
                }
                else{
                    // no redirection, execute command normally
                    executeCommand(args, envp);
                }
                // executeCommand(args, envp); // execute command
            }   
        } else { // parent process
    
            int waitVal, waitStatus; //store waitpid() returned pid and status

            // wait for the child to terminate before printing the next prompt
            waitVal = waitpid(pid, &waitStatus, 0); 
            if(waitVal == pid){
                int exitCode = WEXITSTATUS(waitStatus);
                if (exitCode != 0) {
                    char buffer[128];
                    snprintf(buffer, sizeof(buffer), "Program terminated with exit code %d.\n", exitCode);
                    print(buffer);
                }
            }
        }
    }
    return 0;
}
