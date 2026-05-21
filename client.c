#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MSG_LEN 100
#define MSG_SCAN_LEN 99
#define IN_FILT "%" STR(MSG_SCAN_LEN) "[^\n]%*c"
#define CMD_QUIT "/q"
#define SERVER_IP "169.254.61.46"
#define SERVER_PORT 4242

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

	/*
	 * Étape 2 - Connexion au serveur
	 *
	 * Le serveur du chat général écoute en TCP sur SERVER_IP:SERVER_PORT.
	 * La connexion doit être établie avant la boucle principale afin que
	 * le client dispose ensuite d'une socket prête pour envoyer et recevoir.
	 */
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) {
		perror("Erreur création socket TCP");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_PORT);

	if(inet_pton(AF_INET, SERVER_IP, &server.sin_addr) != 1) {
		perror("Adresse IP serveur invalide");
		close(sock);
		return EXIT_FAILURE;
	}

	if(connect(sock, (struct sockaddr *)&server, sizeof(server)) == -1) {
		perror("Erreur connexion serveur");
		close(sock);
		return EXIT_FAILURE;
	}

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

	close(sock);

	return EXIT_SUCCESS;
}
