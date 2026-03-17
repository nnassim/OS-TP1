#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <readline/readline.h>
#include <readline/history.h>

int main(int argc, char *argv[]) {
    char *buf = NULL;
    char host[64];
    char *user;
    char *prompt;

    /* 1.1 Nom de la machine */
    gethostname(host, sizeof(host));

    /* 1.2 Nom de l'utilisateur */
    user = getenv("USER");
    if (user == NULL) user = "inconnu";

    /* 1.3 Construction du prompt */
    prompt = malloc(128);
    /* geteuid() retourne l'UID effectif, plus correct que getuid() pour un shell */
    char symbole = (geteuid() == 0) ? '#' : '$';
    sprintf(prompt, "%s@%s%c ", user, host, symbole);

    printf("Demarrage du shell biceps...\n");

    /* 1.4 Boucle de lecture */
    while (1) {
        buf = readline(prompt);

        /* Gestion du Ctrl-D (EOF) */
        if (buf == NULL) {
            printf("\nBye!\n");
            break;
        }

        /* Si la ligne n'est pas vide, on l'ajoute a l'historique */
        if (strlen(buf) > 0) {
            add_history(buf);
            /* Pour l'etape 1, on affiche juste la saisie */
            printf("Saisie : %s\n", buf);
        }

        /* Tres important : readline alloue de la memoire qu'il faut liberer */
        free(buf);
    }

    free(prompt);
    return 0;
}