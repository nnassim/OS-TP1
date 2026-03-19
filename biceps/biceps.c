/* biceps.c : Bel Interpreteur de Commandes des Etudiants de Polytech Sorbonne
   Version 0.1 - Etape 1 : programme interactif avec readline */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

int main(void)
{
    char  *buf    = NULL;
    char   host[256];
    char  *user;
    char  *prompt;
    int    len;

    /* --- Construction dynamique du prompt (malloc) --- */
    if (gethostname(host, sizeof(host)) != 0) {
        perror("gethostname");
        strcpy(host, "localhost");
    }

    user = getenv("USER");
    if (user == NULL) user = "inconnu";

    /* format : "user@machine# " (root) ou "user@machine$ " (autres) */
    /* taille = len(user) + 1(@) + len(host) + 1(symbole) + 1(espace) + 1(\0) */
    len = strlen(user) + 1 + strlen(host) + 1 + 1 + 1;
    prompt = malloc(len);
    if (prompt == NULL) {
        perror("malloc");
        return 1;
    }
    sprintf(prompt, "%s@%s%c ", user, host,
            (geteuid() == 0) ? '#' : '$');

    /* --- Boucle principale de saisie --- */
    while (1) {
        buf = readline(prompt);

        /* readline renvoie NULL sur Ctrl-D (EOF) */
        if (buf == NULL) {
            printf("\nBye !\n");
            break;
        }

        /* Affichage de la commande saisie (comportement etape 1) */
        if (strlen(buf) > 0) {
            add_history(buf);
            printf("Commande : %s\n", buf);
        }

        /* readline alloue la memoire : il faut la liberer */
        free(buf);
        buf = NULL;
    }

    free(prompt);
    return 0;
}