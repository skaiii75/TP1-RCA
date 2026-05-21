#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MSG_LEN 100
#define MSG_SCAN_LEN 99
#define IN_FILT "%" STR(MSG_SCAN_LEN) "[^\n]%*c"
#define CMD_QUIT "/q"

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	/*
	 * Étape 1 - Installation
	 *
	 * Compilation :
	 *     cc -o client client.c
	 *
	 * Exécution :
	 *     ./client
	 *
	 * Pour quitter le programme, saisir la commande /q.
	 */
	char buffer[MSG_LEN];
	int quit = 0;

	while(!quit) {
		/*
		 * IN_FILT lit le contenu saisi dans le terminal jusqu'au premier
		 * retour à la ligne. MSG_SCAN_LEN évite de dépasser la taille du
		 * buffer et garde une place pour le caractère final '\0'.
		 */
		int nb_scan = scanf(IN_FILT, buffer);

		if(nb_scan == 1) {
			if(strcmp(buffer, CMD_QUIT) == 0) {
				quit = 1;
			} else {
				printf("Message saisi : %s\n", buffer);
			}
		} else if(nb_scan == EOF) {
			quit = 1;
		} else {
			/* Cas d'une ligne vide : on consomme le '\n' resté en entrée. */
			getchar();
		}
	}

	return EXIT_SUCCESS;
}
