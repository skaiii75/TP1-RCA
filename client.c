#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "parse_command.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MSG_LEN 100
#define MSG_SCAN_LEN 99
#define IN_FILT "%" STR(MSG_SCAN_LEN) "[^\n]%*c"
#define CMD_QUIT "/q"
#define SERVER_IP "169.254.61.46"
#define SERVER_PORT 4242
#define PRIVATE_PORT 5555

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
	char private_ip[MSG_LEN];
	char private_msg[MSG_LEN];
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
	 * Étape 6 - Socket d'écoute UDP pour les messages privés
	 *
	 * Les messages privés ne passeront pas par le serveur. On prépare donc
	 * une socket UDP locale, attachée à toutes les interfaces réseau avec
	 * INADDR_ANY et au port PRIVATE_PORT. Cette socket sera lue plus tard
	 * par un thread dédié.
	 */
	int private_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(private_sock == -1) {
		perror("Erreur création socket UDP privée");
		close(sock);
		return EXIT_FAILURE;
	}

	struct sockaddr_in private_addr;
	memset(&private_addr, 0, sizeof(private_addr));
	private_addr.sin_family = AF_INET;
	private_addr.sin_port = htons(PRIVATE_PORT);
	private_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(private_sock, (struct sockaddr *)&private_addr, sizeof(private_addr)) == -1) {
		perror("Erreur bind socket UDP privée");
		close(private_sock);
		close(sock);
		return EXIT_FAILURE;
	}

	/*
	 * Étape 9 - Création d'un thread supplémentaire
	 *
	 * pid[0] correspond au premier enfant, chargé de lire le terminal et
	 * d'envoyer les messages. Le deuxième fork est lancé uniquement par le
	 * parent, afin de créer pid[1], qui servira à recevoir les messages
	 * privés UDP à l'étape 10.
	 *
	 * Valeurs obtenues :
	 * - parent : pid[0] > 0 et pid[1] > 0 ;
	 * - premier enfant : pid[0] == 0 et pid[1] == 0 ;
	 * - deuxième enfant : pid[0] > 0 et pid[1] == 0.
	 */
	int pid[2] = {0, 0};
	pid[0] = fork();
	if(pid[0] == -1) {
		perror("Erreur premier fork");
		close(sock);
		close(private_sock);
		return EXIT_FAILURE;
	}

	if(pid[0] != 0) {
		pid[1] = fork();
		if(pid[1] == -1) {
			perror("Erreur deuxième fork");
			kill(pid[0], SIGTERM);
			close(sock);
			close(private_sock);
			return EXIT_FAILURE;
		}
	}

	while(!quit) {
		if(pid[0] == 0) {
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
					/*
					 * Étape 7 - Commande de message privé
					 *
					 * parse_mp() détecte la forme :
					 *     /mp [adresse_ip] [message]
					 *
					 * Si la commande est reconnue, l'étape 8 enverra le
					 * contenu extrait avec UDP.
					 */
					memset(private_ip, 0, sizeof(private_ip));
					memset(private_msg, 0, sizeof(private_msg));
					int mp_status = parse_mp(buffer, private_ip, private_msg);

					if(mp_status == -1) {
						fprintf(stderr, "Erreur analyse commande /mp\n");
						quit = 1;
						shutdown(sock, SHUT_RDWR);
					} else if(mp_status == 1) {
						/*
						 * Étape 8 - Envoi des messages privés
						 *
						 * La commande /mp ne passe pas par le serveur TCP. On
						 * crée une socket UDP temporaire, on construit l'adresse
						 * du destinataire, puis on envoie directement le message
						 * sur son port privé.
						 */
						int send_private_sock = socket(AF_INET, SOCK_DGRAM, 0);
						if(send_private_sock == -1) {
							perror("Erreur création socket UDP d'envoi");
						} else {
							struct sockaddr_in dest_addr;
							memset(&dest_addr, 0, sizeof(dest_addr));
							dest_addr.sin_family = AF_INET;
							dest_addr.sin_port = htons(PRIVATE_PORT);

							if(inet_pton(AF_INET, private_ip, &dest_addr.sin_addr) != 1) {
								fprintf(stderr, "Adresse IP privée invalide : %s\n", private_ip);
							} else {
								int nb_send = sendto(
									send_private_sock,
									private_msg,
									strlen(private_msg),
									0,
									(struct sockaddr *)&dest_addr,
									sizeof(dest_addr)
								);

								if(nb_send == -1) {
									perror("Erreur envoi message privé");
								}
							}

							close(send_private_sock);
						}
					} else {
						int nb_send = send(sock, buffer, strlen(buffer), 0);
						if(nb_send == -1) {
							perror("Erreur envoi message");
							quit = 1;
							shutdown(sock, SHUT_RDWR);
						}
					}
				}
			} else if(nb_scan == EOF) {
				quit = 1;
				shutdown(sock, SHUT_RDWR);
			} else {
				/* Ligne vide : scanf() ne lit rien, on consomme le '\n'. */
				getchar();
			}
		} else if(pid[1] == 0) {
			/*
			 * Étape 10 - Réception des messages privés
			 *
			 * Le deuxième enfant scrute la socket UDP attachée au port privé.
			 * recvfrom() récupère à la fois le contenu du message et l'adresse
			 * de l'expéditeur, ce qui permet d'afficher clairement l'origine
			 * du message privé.
			 */
			struct sockaddr_in sender_addr;
			socklen_t sender_len = sizeof(sender_addr);

			int nb_read = recvfrom(
				private_sock,
				buffer,
				MSG_LEN - 1,
				0,
				(struct sockaddr *)&sender_addr,
				&sender_len
			);

			if(nb_read == -1) {
				perror("Erreur réception message privé");
				quit = 1;
			} else {
				char sender_ip[INET_ADDRSTRLEN];
				buffer[nb_read] = '\0';

				if(inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip)) == NULL) {
					perror("Erreur conversion IP expéditeur");
					snprintf(sender_ip, sizeof(sender_ip), "inconnue");
				}

				printf("[MP %s:%d] %s\n", sender_ip, ntohs(sender_addr.sin_port), buffer);
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

	if(pid[0] != 0 && pid[1] != 0) {
		/* Le parent termine les deux processus enfants avant de quitter. */
		kill(pid[0], SIGTERM);
		kill(pid[1], SIGTERM);
	}

	close(sock);
	close(private_sock);

	return EXIT_SUCCESS;
}
