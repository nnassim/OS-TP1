#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>

#define MAX_ARGS 100

int main(void)
{
    char  *buf = NULL;
    char   host[256];
    char  *user;
    char  *prompt;
    int    len;

    /* --- Construction du prompt --- */
    if (gethostname(host, sizeof(host)) != 0) {
        perror("gethostname");
        strcpy(host, "localhost");
    }

    user = getenv("USER");
    if (user == NULL) user = "inconnu";

    len = strlen(user) + 1 + strlen(host) + 1 + 1 + 1;
    prompt = malloc(len);
    if (prompt == NULL) {
        perror("malloc");
        return 1;
    }

    sprintf(prompt, "%s@%s%c ", user, host,
            (geteuid() == 0) ? '#' : '$');

    /* --- Boucle principale --- */
    while (1) {
        buf = readline(prompt);

        if (buf == NULL) {
            printf("\nBye !\n");
            break;
        }

        if (strlen(buf) > 0) {
            add_history(buf);

            /* --- Parsing des arguments --- */
            char *args[MAX_ARGS];
            int i = 0;

            char *token = strtok(buf, " ");
            while (token != NULL && i < MAX_ARGS - 1) {
                args[i++] = token;
                token = strtok(NULL, " ");
            }
            args[i] = NULL;

            /* --- Commande interne exit --- */
            if (strcmp(args[0], "exit") == 0) {
                free(buf);
                break;
            }

            /* --- fork + exec --- */
            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
            }
            else if (pid == 0) {
                /* processus fils */
                execvp(args[0], args);
                perror("execvp"); // si erreur
                exit(EXIT_FAILURE);
            }
            else {
                /* processus père */
                wait(NULL);
            }
        }

        free(buf);
        buf = NULL;
    }

    free(prompt);
    return 0;
}