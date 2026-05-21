#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MSG_LEN 100         // taille max des messages autorisés
#define IN_FILT "%[^\n]%*c" // filtre le prompt : récupère tout jusqu'au premier retour à la ligne
#define CMD_QUIT "/q"       // commande du prompt pour quitter
#define SERVER_IP "169.254.61.46"
#define SERVER_PORT 4242

int main(int argc , char *argv[]) {
	(void)argc;
	(void)argv;

	/* Ce buffer recevra les messages à transmettre */
	char buffer[MSG_LEN];

	/* Connexion au serveur du chat général */
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) {
		perror("Erreur création socket");
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

	/* Fork: création d'un thread enfant */
	int pid = fork();
	if(pid == -1) {
		perror("Erreur fork");
		close(sock);
		return EXIT_FAILURE;
	}

	/* Flag pour arrêter le programme */
	int quit = 0;
	while(!quit) {
		if(pid==0) { // thread enfant
			/* L'envoi des messages sera ajouté à l'étape 4 */
			sleep(1);
		} else { // thread parent
			/* Réception des messages transmis par le serveur */
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
	if(pid != 0) { // thread parent
		/* Termine le thread enfant */
		kill(pid, SIGTERM);
	}
	close(sock);

	return 0;
}
