#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include "lab.h"
#include <bits/posix1_lim.h>


// Set the shell prompt
char *get_prompt(const char *env){
    const char *env_prompt = getenv(env);
    const char *prompt = env_prompt ? env_prompt: "shell>";

    //Allocate memory for the prompt + null terminator
    char *result = malloc(strlen(prompt) +1);
    if(result == NULL){return NULL;}

    strcpy(result, prompt);
    return result;
}


//Changes the current working directory of the shell
int change_dir(char **dir){
    char *target = dir[1];

    //If no directory is given, use the home directory
    if(dir[1] == NULL){
        target = getenv("HOME");
        printf("DEBUG: HOME directory detected as: %s\n", target);

        if(!target){
            struct passwd *pw = getpwuid(getuid());
            if(pw){
                target = pw->pw_dir;
            }else{
                fprintf(stderr, "cd: Cannot find home directory\n");
                return -1;
            }
        }
    }

    //Change directory
    if(chdir(target) != 0){
        perror("cd");
        return -1;
    }
    return 0;
}


//Convert line read from the user into to format that will work with execvp
char **cmd_parse(char const *line){
    if(!line) return NULL;

    // Get the max argument limit
    long arg_max = sysconf(_SC_ARG_MAX);
    if(arg_max <=0) arg_max = _POSIX_ARG_MAX;

    //Allowcate memory
    char **args = malloc(sizeof(char *) * arg_max);
    if(!args){
        perror("maloc");
        return NULL;
    }

    char *line_copy = strdup(line);
    if(!line_copy){
        perror("strdup");
        free(args);
        return NULL;
    }

    int count =0;
    char *token = strtok(line_copy, " ");
    while (token && count <arg_max -1){
        args[count++] = strdup(token);
        token = strtok(NULL, " ");
    }
    args[count] = NULL; //null-terminate

    free(line_copy);
    return args;
}



//Free the line that was constructed with parse_cmd
void cmd_free(char ** line){
    if(!line) return;

    for(int i =0; line[i] != NULL; i++){
        free(line[i]);
    }
    free(line); //it's time to say goodbye the array itself
}



//Remove the white space
char *trim_white(char *line) {
    if (!line) return NULL;

    // Traverse to the first non-whites space character
    while (isspace((unsigned char)*line)){
        line++;
    }
    if (*line == '\0') return line;
    
    // Set pointer to the end of the string.
    char *end = line + strlen(line) - 1;

    // Trim trailing whitespace.
    while (end > line && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return line;
}

//Takes an argument list and checks if the first argument is a built in command
bool do_builtin(struct shell *sh, char **argv){
    if( !argv || argv[0] == NULL) return false;

    if(strcmp(argv[0], "exit") == 0){ //buil-in 'exit' command
        sh_destroy(sh);
        exit(0);
    }else if(strcmp(argv[0], "cd") ==0){ //buil-in 'cd' command
        if(change_dir(argv) != 0){
            fprintf(stderr, "cd: Change directory was failed\n");
        }
        return true;
    }else if (strcmp(argv[0], "history") == 0) {
        HIST_ENTRY **hist_list = history_list();
        if (hist_list) {
            for (int i = 0; hist_list[i]; i++) {
                printf("%d: %s\n", i + history_base, hist_list[i]->line);
            }
        }
    }
    return false;
}


//Initialize the shell for use.
void sh_init(struct shell *sh) {
    if (!sh) {
        fprintf(stderr, "sh_init: shell pointer is NULL\n");
        exit(EXIT_FAILURE);
    }

    sh->shell_terminal = STDIN_FILENO;
    // Check if running interactively.
    sh->shell_is_interactive = isatty(sh->shell_terminal);
    if (!sh->shell_is_interactive) {
        fprintf(stderr, "sh_init: Not running interactively.\n");
        exit(EXIT_FAILURE);
    }

    sh->prompt = get_prompt("MY_PROMPT");

    // Put the shell in its own process group.
    sh->shell_pgid = getpid();
    if (setpgid(sh->shell_pgid, sh->shell_pgid) < 0) {
        perror("sh_init: setpgid failed");
        exit(EXIT_FAILURE);
    }

    // Grab control of the terminal by setting the foreground process group.
    if (tcsetpgrp(sh->shell_terminal, sh->shell_pgid) < 0) {
        perror("sh_init: tcsetpgrp failed");
        exit(EXIT_FAILURE);
    }

    // Save current terminal attributes.
    if (tcgetattr(sh->shell_terminal, &sh->shell_tmodes) < 0) {
        perror("sh_init: tcgetattr failed");
        exit(EXIT_FAILURE);
    }

    // Ignore signals that the shell should not handle.
    signal(SIGINT,  SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    printf("Shell initialized: PGID=%d, Terminal FD=%d\n", sh->shell_pgid, sh->shell_terminal);
}


//Destroy shell. Free any allocated memory and resources and exit
void sh_destroy(struct shell *sh){
    if(!sh) return;

    if(sh->prompt){
        free(sh->prompt);
        sh->prompt = NULL;
    }
    exit(0);
}


//Parse command line args from the user when the shell was launched
void parse_args(int argc, char **argv){
    int opt;

    while((opt = getopt(argc, argv, "v")) != -1){
        switch (opt){
            case 'v' :
                printf("Shell Version: %d.%d\n", lab_VERSION_MAJOR, lab_VERSION_MINOR);
                break;
            default:
                fprintf(stderr, "Usage: %s [-v]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
}
