#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 100
#define MAX_CMDS 10
#define HISTORY_FILE ".biceps_history"

/* ------------------------------------------------------------------ */
/* Etape 5.1 : split sur | en respectant les espaces autour            */
/* ------------------------------------------------------------------ */
int split_pipe(char *line, char *cmds[]) {
    int i = 0;
    char *saveptr;
    char *token = strtok_r(line, "|", &saveptr);

    while (token && i < MAX_CMDS) {
        /* supprimer espaces en debut/fin de chaque segment */
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) *end-- = '\0';
        cmds[i++] = token;
        token = strtok_r(NULL, "|", &saveptr);
    }
    cmds[i] = NULL;
    return i;
}

/* ------------------------------------------------------------------ */
/* Etape 4.2 : split sur ; pour commandes sequentielles               */
/* ------------------------------------------------------------------ */
int split_sequential(char *line, char *cmds[]) {
    int i = 0;
    char *saveptr;
    char *token = strtok_r(line, ";", &saveptr);

    while (token && i < MAX_CMDS) {
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) *end-- = '\0';
        cmds[i++] = token;
        token = strtok_r(NULL, ";", &saveptr);
    }
    cmds[i] = NULL;
    return i;
}

/* ------------------------------------------------------------------ */
/* Parsing arguments + redirections (< > >> )                          */
/* ------------------------------------------------------------------ */
void parse_args(char *cmd, char *args[], char **in, char **out, int *append) {
    int i = 0;
    *in = NULL;
    *out = NULL;
    *append = 0;

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
            *append = 0;
        }
        else if (strcmp(token, ">>") == 0) {
            token = strtok_r(NULL, " \t", &saveptr);
            if (token) *out = token;
            *append = 1;
        }
        else {
            /* gestion guillemets doubles */
            if (token[0] == '"') {
                char *full = strdup(token);
                while (full[strlen(full)-1] != '"'
                       && (token = strtok_r(NULL, " \t", &saveptr))) {
                    full = realloc(full, strlen(full) + strlen(token) + 2);
                    strcat(full, " ");
                    strcat(full, token);
                }
                full[strlen(full)-1] = '\0';
                args[i++] = full + 1;
            } else {
                args[i++] = token;
            }
        }
        token = strtok_r(NULL, " \t", &saveptr);
    }
    args[i] = NULL;
}

/* ------------------------------------------------------------------ */
/* Etape 5.1 : execution d'une pipeline de n commandes                 */
/* ------------------------------------------------------------------ */
void exec_pipeline(char *cmds[], int n) {
    int prev_fd = -1;

    for (int i = 0; i < n; i++) {
        int pipefd[2];

        /* creer le pipe si ce n'est pas la derniere commande */
        if (i < n - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return;
            }
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return;
        }

        if (pid == 0) {
            /* --- processus fils --- */
            char *cmd_copy = strdup(cmds[i]);

            char *args[MAX_ARGS];
            char *in = NULL, *out = NULL;
            int append = 0;

            parse_args(cmd_copy, args, &in, &out, &append);

            if (!args[0]) exit(0);

            /* redirection entree fichier */
            if (in) {
                int fd = open(in, O_RDONLY);
                if (fd < 0) { perror(in); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            /* redirection sortie fichier */
            if (out) {
                int fd = append
                    ? open(out, O_CREAT | O_WRONLY | O_APPEND, 0644)
                    : open(out, O_CREAT | O_WRONLY | O_TRUNC,  0644);
                if (fd < 0) { perror(out); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            /* lire depuis le pipe precedent */
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            /* ecrire dans le pipe suivant */
            if (i < n - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            execvp(args[0], args);
            fprintf(stderr, "%s : commande inexistante !\n", args[0]);
            exit(1);
        }

        /* --- processus pere --- */

        /* fermer le fd de lecture du pipe precedent */
        if (prev_fd != -1)
            close(prev_fd);

        if (i < n - 1) {
            /* fermer l'extremite ecriture, garder lecture pour le fils suivant */
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
    }

    /* attendre tous les fils de la pipeline */
    for (int i = 0; i < n; i++)
        wait(NULL);
}

/* ------------------------------------------------------------------ */
/* Gestionnaire Ctrl-C (etape 4.3) : ne pas quitter biceps             */
/* ------------------------------------------------------------------ */
void handle_sigint(int sig) {
    (void)sig;
    /* nouvelle ligne pour ne pas coller au prompt */
    write(STDOUT_FILENO, "\n", 1);
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */
int main(void)
{
    char *buf;
    char  host[256], *user, *prompt;

    /* --- Ctrl-C : ignorer (etape 4.3) --- */
    signal(SIGINT, handle_sigint);

    /* --- prompt dynamique (etape 1) --- */
    if (gethostname(host, sizeof(host)) != 0)
        strcpy(host, "localhost");
    user = getenv("USER");
    if (!user) user = "user";

    int len = strlen(user) + strlen(host) + 4;
    prompt = malloc(len);
    if (!prompt) { perror("malloc"); return 1; }
    sprintf(prompt, "%s@%s%c ", user, host, (geteuid() == 0) ? '#' : '$');

    /* --- historique persistant (etape 4.3) --- */
    read_history(HISTORY_FILE);

    /* ---------------------------------------------------------------- */
    /* Boucle principale                                                 */
    /* ---------------------------------------------------------------- */
    while (1) {
        buf = readline(prompt);

        /* Ctrl-D : sortie propre */
        if (!buf) {
            printf("\nBye !\n");
            break;
        }

        if (strlen(buf) == 0) {
            free(buf);
            continue;
        }

        /* n'ajouter a l'historique que les lignes utiles (etape 4.3) */
        add_history(buf);
        write_history(HISTORY_FILE);

        /* copie de travail */
        char *buf_copy = strdup(buf);

        /* -------------------------------------------------------------- */
        /* Etape 4.2 : decoupage sequentiel sur ;                          */
        /* -------------------------------------------------------------- */
        char *seq_cmds[MAX_CMDS];
        int  nseq = split_sequential(buf_copy, seq_cmds);

        for (int s = 0; s < nseq; s++) {

            char *seg_copy = strdup(seq_cmds[s]);

            /* ---------------------------------------------------------- */
            /* Etape 5.1 : decoupage pipeline sur |                        */
            /* ---------------------------------------------------------- */
            char *pipe_cmds[MAX_CMDS];
            int  npipe = split_pipe(seg_copy, pipe_cmds);

            /* commandes internes (cd, exit, pwd, vers) :                 */
            /* uniquement si pipeline d'une seule commande                */
            if (npipe == 1) {
                char *cmd_copy = strdup(pipe_cmds[0]);
                char *args[MAX_ARGS];
                char *in = NULL, *out = NULL;
                int   append = 0;

                parse_args(cmd_copy, args, &in, &out, &append);

                if (args[0]) {
                    /* cd (etape 4.1) */
                    if (strcmp(args[0], "cd") == 0) {
                        if (args[1]) {
                            if (chdir(args[1]) != 0) perror("cd");
                        } else {
                            fprintf(stderr, "cd: argument manquant\n");
                        }
                        free(cmd_copy);
                        free(seg_copy);
                        continue;
                    }

                    /* exit (etape 3) */
                    if (strcmp(args[0], "exit") == 0) {
                        printf("Bye !\n");
                        free(cmd_copy); free(seg_copy); free(buf_copy); free(buf);
                        free(prompt);
                        return 0;
                    }

                    /* pwd (etape 4.1) */
                    if (strcmp(args[0], "pwd") == 0) {
                        char cwd[1024];
                        if (getcwd(cwd, sizeof(cwd)))
                            printf("%s\n", cwd);
                        else
                            perror("pwd");
                        free(cmd_copy);
                        free(seg_copy);
                        continue;
                    }

                    /* vers (etape 4.1) */
                    if (strcmp(args[0], "vers") == 0) {
                        printf("biceps version 1.1\n");
                        free(cmd_copy);
                        free(seg_copy);
                        continue;
                    }
                }
                free(cmd_copy);
            }

            /* commandes externes : exec_pipeline gere 1 ou N commandes */
            exec_pipeline(pipe_cmds, npipe);

            free(seg_copy);
        }

        free(buf_copy);
        free(buf);
    }

    /* sauvegarde historique a la sortie */
    write_history(HISTORY_FILE);
    free(prompt);
    return 0;
}