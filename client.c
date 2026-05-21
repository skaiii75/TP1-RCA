#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
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

	/*
	 * À partir de l'étape 3, le programme est séparé en deux processus :
	 * - le parent réceptionne les messages du serveur ;
	 * - l'enfant sera utilisé plus tard pour lire le terminal.
	 */
	int pid = fork();
	if(pid == -1) {
		perror("Erreur fork");
		close(sock);
		return EXIT_FAILURE;
	}

	while(!quit) {
		if(pid == 0) {
			/*
			 * Étape 4 - Envoi de messages sur le chat général
			 *
			 * Le thread enfant lit le terminal. scanf() utilise IN_FILT pour
			 * récupérer tout ce qui est saisi jusqu'au premier '\n', donc
			 * jusqu'à l'appui sur Entrée. Le message lu est ensuite envoyé au
			 * serveur avec send().
			 */
			int nb_scan = scanf(IN_FILT, buffer);

			if(nb_scan == 1) {
				if(strcmp(buffer, CMD_QUIT) == 0) {
					/*
					 * Étape 5 - Déconnexion du serveur
					 *
					 * Après fork(), le parent et l'enfant ont chacun leur
					 * propre variable quit. Modifier quit dans l'enfant ne
					 * suffit donc pas à arrêter le parent. shutdown() ferme la
					 * connexion TCP côté noyau : le recv() bloquant du parent
					 * se débloque alors et peut quitter sa boucle.
					 */
					quit = 1;
					shutdown(sock, SHUT_RDWR);
				} else {
					int nb_send = send(sock, buffer, strlen(buffer), 0);
					if(nb_send == -1) {
						perror("Erreur envoi message");
						quit = 1;
						shutdown(sock, SHUT_RDWR);
					}
				}
			} else if(nb_scan == EOF) {
				quit = 1;
				shutdown(sock, SHUT_RDWR);
			} else {
				/* Ligne vide : scanf() ne lit rien, on consomme le '\n'. */
				getchar();
			}
		} else {
			/*
			 * Étape 3 - Réception du chat général
			 *
			 * Le thread parent écoute la socket TCP connectée au serveur.
			 * recv() est bloquant : le parent attend ici qu'un message soit
			 * diffusé par le serveur, puis l'affiche dans le terminal.
			 */
			int nb_read = recv(sock, buffer, MSG_LEN - 1, 0);

			if(nb_read <= 0) {
				printf("Déconnecté du serveur\n");
				quit = 1;
			} else {
				buffer[nb_read] = '\0';
				printf("%s\n", buffer);
			}
		}
	}

	if(pid != 0) {
		/* Le parent termine le processus enfant avant de quitter. */
		kill(pid, SIGTERM);
	}

	close(sock);

	return EXIT_SUCCESS;
}
