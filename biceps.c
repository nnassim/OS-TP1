#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 100
#define MAX_CMDS 10

/* --- split commandes par | --- */
int split_commands(char *line, char *cmds[]) {
    int i = 0;
    char *saveptr;
    char *token = strtok_r(line, "|", &saveptr);

    while (token && i < MAX_CMDS) {
        cmds[i++] = token;
        token = strtok_r(NULL, "|", &saveptr);
    }
    cmds[i] = NULL;
    return i;
}

/* --- parsing arguments + redirections --- */
void parse_args(char *cmd, char *args[], char **in, char **out) {
    int i = 0;
    *in = NULL;
    *out = NULL;

    char *saveptr;
    char *token = strtok_r(cmd, " \t", &saveptr);

    while (token) {
        if (strcmp(token, "<") == 0) {
            token = strtok_r(NULL, " \t", &saveptr);
            if (token) *in = token;
        }
        else if (strcmp(token, ">") == 0) {
            token = strtok_r(NULL, " \t", &saveptr);
            if (token) *out = token;
        }
        else {
            args[i++] = token;
        }
        token = strtok_r(NULL, " \t", &saveptr);
    }

    args[i] = NULL;
}

int main(void)
{
    char *buf;
    char host[256], *user, *prompt;
    int len;

    /* --- prompt --- */
    gethostname(host, sizeof(host));
    user = getenv("USER");
    if (!user) user = "user";

    len = strlen(user) + strlen(host) + 4;
    prompt = malloc(len);
    if (!prompt) {
        perror("malloc");
        exit(1);
    }

    sprintf(prompt, "%s@%s$ ", user, host);

    while (1) {
        buf = readline(prompt);

        /* Ctrl+D */
        if (!buf) {
            printf("\nBye !\n");
            break;
        }

        if (strlen(buf) == 0) {
            free(buf);
            continue;
        }

        add_history(buf);

        /* ⚠️ copie pour éviter corruption strtok */
        char *buf_copy = strdup(buf);
        if (!buf_copy) {
            perror("strdup");
            free(buf);
            continue;
        }

        char *cmds[MAX_CMDS];
        int n = split_commands(buf_copy, cmds);

        /* --- cd + exit --- */
        if (n == 1) {
            char *cmd_copy = strdup(cmds[0]);
            if (!cmd_copy) {
                perror("strdup");
                free(buf_copy);
                free(buf);
                continue;
            }

            char *args[MAX_ARGS];
            char *in = NULL, *out = NULL;

            parse_args(cmd_copy, args, &in, &out);

            if (args[0]) {
                /* cd */
                if (strcmp(args[0], "cd") == 0) {
                    if (args[1]) {
                        if (chdir(args[1]) != 0)
                            perror("cd");
                    } else {
                        fprintf(stderr, "cd: argument manquant\n");
                    }
                    free(cmd_copy);
                    free(buf_copy);
                    free(buf);
                    continue;
                }

                /* exit */
                if (strcmp(args[0], "exit") == 0) {
                    printf("Bye !\n");
                    free(cmd_copy);
                    free(buf_copy);
                    free(buf);
                    break;
                }
            }

            free(cmd_copy);
        }

        int prev_fd = -1;

        for (int i = 0; i < n; i++) {
            int pipefd[2];

            if (i < n - 1) {
                if (pipe(pipefd) < 0) {
                    perror("pipe");
                    exit(1);
                }
            }

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                exit(1);
            }

            if (pid == 0) {
                char *cmd_copy = strdup(cmds[i]);
                if (!cmd_copy) {
                    perror("strdup");
                    exit(1);
                }

                char *args[MAX_ARGS];
                char *in = NULL, *out = NULL;

                parse_args(cmd_copy, args, &in, &out);

                if (args[0] == NULL)
                    exit(0);

                /* redirection entrée */
                if (in) {
                    int fd = open(in, O_RDONLY);
                    if (fd < 0) {
                        perror("open input");
                        exit(1);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                /* redirection sortie */
                if (out) {
                    int fd = open(out, O_CREAT | O_WRONLY | O_TRUNC, 0644);
                    if (fd < 0) {
                        perror("open output");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                /* pipe entrée */
                if (prev_fd != -1) {
                    dup2(prev_fd, STDIN_FILENO);
                    close(prev_fd);
                }

                /* pipe sortie */
                if (i < n - 1) {
                    close(pipefd[0]);
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                execvp(args[0], args);
                perror("execvp");
                exit(1);
            }

            /* parent */
            if (prev_fd != -1)
                close(prev_fd);

            if (i < n - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        }

        /* attendre tous les fils */
        for (int i = 0; i < n; i++)
            wait(NULL);

        free(buf_copy);
        free(buf);
    }

    free(prompt);
    return 0;
}