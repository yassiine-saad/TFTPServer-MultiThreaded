
/*********************************************************************************************
 *                              @file main_server.c.h                                        *
 *                        @brief : Programme principal du serveur TFTP.                      *
 *                                                                                           *
 *********************************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>


#include "tftp.h"
#include "sync.h"

#define SERVER_MAIN_PORT 69

typedef int (*TFTP_HandlerFunction)(TFTP_Client *client, TFTP_Request* request);


void *handleClient(void *arg);



// Global VAR
FileList fileList;
TFTP_ClientsList clientsList;



/**
 * @brief Fonction principale du serveur TFTP.
 * @return 0 en cas de succès.
 */

int main() {
    struct sockaddr_in client_addr;
    socklen_t client_len;
    char buffer[MAX_PACKET_SIZE];

    // Initialisation du serveur TFTP
    printf("Initialisation du serveur TFTP...\n");
    int sockfd = create_Socket("0.0.0.0", SERVER_MAIN_PORT);
    printf("Serveur TFTP initialisé et en attente de connexions sur le port %d\n", SERVER_MAIN_PORT);
    client_len = sizeof(client_addr);

    initialize_fileList(&fileList);

    while (1) {
        ssize_t num_bytes_received = recvfrom(sockfd, &buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (num_bytes_received == -1) {
            perror("Erreur lors de la réception des données du client");
            continue;
        }

        if (num_bytes_received < 11){
            continue;
        }

        if (get_client(&clientsList,client_addr,buffer) != NULL ){
            printf("client déja .... ignore\n");
            continue;
        }

        char* client_ip = inet_ntoa(client_addr.sin_addr);
        int newsockfd = create_Socket(client_ip,0);

        TFTP_Client* client = init_client(client_addr,buffer);

        if (client == NULL) {
            perror("Erreur lors de l'allocation de mémoire pour les données client");
            close(newsockfd);
            continue;
        }

        client->socket_fd = newsockfd;
        client->client_addr = client_addr;
        memcpy(client->packet, buffer, num_bytes_received);

        ajouterClient(client,&clientsList);


        // Création d'un thread pour gérer le client
        pthread_t tid;
        if (pthread_create(&tid, NULL, handleClient, client) != 0) {
            perror("Erreur lors de la création du thread client");
            supprimer_client(&clientsList,client);
            continue;
        }
        
        pthread_detach(tid); // Le thread est détaché car nous n'attendons pas explicitement sa fin
    }

    return 0;
}









/**
 * @brief la fonction handleClient, qui sera exécutée par chaque thread pour gérer un client connecté 
 * @param arg Argument pointeur vers la structure TFTP_Client représentant le client.
 * @return Aucune valeur de retour.
 */
void *handleClient(void *arg) {
    TFTP_Request request;
    TFTP_HandlerFunction selectedHandler = NULL;
    Sync_Function SYNC_START = NULL;
    Sync_Function SYNC_END = NULL;


    // Récupérer les données du client
    TFTP_Client *client = (TFTP_Client *)arg;
    int sockfd = client->socket_fd;
    printf("\nNouveau client connecté, adresse IP : %s, port : %d\n", inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port));

    // Analyser le paquet reçu
    char* buffer = client->packet;
    memcpy(&request.opcode, buffer, sizeof(uint16_t));

    // Sélection du gestionnaire de demande en fonction de l'opcode
    if (ntohs(request.opcode) == TFTP_OPCODE_RRQ) {
        SYNC_START = sync_start_read;
        SYNC_END = sync_end_read;
        selectedHandler = handle_read_request;
    } else if (ntohs(request.opcode) == TFTP_OPCODE_WRQ) {
        SYNC_START = sync_start_write;
        SYNC_END = sync_end_write;
        selectedHandler = handle_write_request;
    } else {
        send_error_packet(sockfd, &client->client_addr, NotDefined, get_error_message(NotDefined),"Opcode non pris en charge");
        pthread_exit(NULL);
    }

    // Extraction du nom de fichier
    size_t filename_length = strlen(buffer + 2);
    strcpy(request.filename, buffer + 2);
    if (filename_length == 0) {
        printf("Erreur: Nom de fichier vide.\n");
        send_error_packet(sockfd, &client->client_addr, NotDefined, get_error_message(NotDefined),"Nom de fichier vide");  // Envoyer un paquet d'erreur au client
        pthread_exit(NULL);
    }

    

    // Extraction du mode de transfert
    size_t mode_offset = 2 + filename_length + 1; // Offset pour accéder au début du mode
    size_t mode_length = strlen(buffer + mode_offset);

    strcpy(request.mode, buffer + mode_offset);
    if (mode_length == 0 || (strcasecmp(request.mode, "netascii") != 0 && strcasecmp(request.mode, "octet") != 0) ) {
        printf("Erreur: Mode de transfert non reconnu.\n");
        send_error_packet(sockfd, &client->client_addr, NotDefined, get_error_message(NotDefined),"Mode de transfert non reconnu");    // Envoyer un paquet d'erreur au client
        
    }


    

    SYNC_START(request.filename,&fileList);     // début de la synchronisation pour le fichier demandé
    char* temp_file;
    int status;
    // printf("client fd %d commence\n",client->socket_fd);


    if(ntohs(request.opcode) == TFTP_OPCODE_RRQ ) { // Ouverture du fichier en lecture ou écriture en fonction de l'opération demandée
        if (strcasecmp(request.mode, "netascii") == 0) { // Ouverture du fichier demandé
            client->file = fopen(request.filename, "r");
        } else if (strcasecmp(request.mode, "octet") == 0) {
            client->file = fopen(request.filename, "rb");
        } 
    } else {

        temp_file = get_temp_file_name(request.filename);
        // printf("temp filename %s\n",temp_file);

        if (strcasecmp(request.mode, "netascii") == 0) {
            client->file = fopen(temp_file, "w");
        } else if (strcasecmp(request.mode, "octet") == 0) {
            client->file = fopen(temp_file, "wb");
        }

    }
    
    
    if (client->file == NULL) { 
        printf("Erreur !! : fichier non trouvé\n");
        send_error_packet(client->socket_fd, &client->client_addr,FileNotFound, get_error_message(FileNotFound),NULL);// Envoi d'un paquet d'erreur au client
        SYNC_END(request.filename,&fileList); 
        pthread_exit(NULL);;
    }
    
    
    status = selectedHandler(client, &request); // Gestion de la demande du client

    

    if (ntohs(request.opcode) == TFTP_OPCODE_WRQ){

        if (status == 0) {
            // Supprimer l'ancien fichier
            if (access(request.filename, F_OK) != -1) {
                // printf("Le fichier existe.\n");
                if (remove(request.filename) != 0) {
                    perror("Erreur lors de la suppression de l'ancien fichier");
                }
            }

            // Renommer le fichier temporaire en cas de succès
            if (rename(temp_file, request.filename) != 0) {
                perror("Erreur lors du renommage du fichier temporaire");
            }
        } else if (status != 0 ) {
            // Supprimer le fichier temporaire en cas d'échec
            if (remove(temp_file) != 0) {
                perror("Erreur lors de la suppression du fichier temporaire");
            }
        }

        free(temp_file);
    }
    
    

    // printf("client fd %d termine\n",client->socket_fd);
    SYNC_END(request.filename,&fileList);    // Fin de la synchronisation pour le fichier demandé
    supprimer_client(&clientsList,client);   // Suppression du client de la liste des clients connectés

    pthread_exit(NULL);

     
}



