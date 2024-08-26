/********************************************************
 *                   @file tftp.c                      *
 *                    @brief  :                        *
 *  Ce fichier contient les implémentations des        *
 *  fonctions nécessaires pour le fonctionnement du    *
 *  protocole TFTP (Trivial File Transfer Protocol).   *
 ********************************************************/


#include "tftp.h"




const char* TFTPErrorMessages[NUM_TFTP_ERRORS] = {
    "Not defined, see error message (if any)",
    "File not found",
    "Access violation",
    "Disk full or allocation exceeded",
    "Illegal TFTP operation",
    "Unknown transfer ID",
    "File already exists",
    "No such user"
};



/*********************************************************************************************************
 *                                                SECTION 1                                              *
 *                                    GESTION DES REQUÊTES TFTP (WRQ/RRQ)                                 *
 *********************************************************************************************************/




/**
 * Fonction : handle_read_request
 * @brief : Cette fonction est responsable de la gestion d'une demande de lecture (RRQ) provenant d'un client TFTP.
 * Elle lit les données du fichier demandé par le client et les envoie en paquets de données, tout en attendant les ACK correspondants.
 * En cas d'erreur, elle envoie un paquet d'erreur approprié au client.
 * @param client : Un pointeur vers la structure représentant le client TFTP.
 * @param request : Un pointeur vers la structure représentant la demande de lecture.
 * @return : 0 sucess     -1 ERR
 */
int handle_read_request(TFTP_Client *client, TFTP_Request *request) {

    struct timeval tv;
    tv.tv_sec = TIMEOUT_SECONDS;
    tv.tv_usec = 0;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    TFTP_DataPacket data_packet;
    TFTP_AckPacket ack_packet;
    size_t num_bytes_read;
    int retryCount = 0;
    int block_num = 1;

    printf("[RRQ] @IP %s:%d, file: %s, Mode: %s\n", inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port), request->filename, request->mode);

    do {
        num_bytes_read = fread(data_packet.data, 1, sizeof(data_packet.data), client->file);
        if (num_bytes_read == (long unsigned int) -1) {
            perror("Erreur lors de la lecture du fichier");
            send_error_packet(client->socket_fd,&client->client_addr,FileNotFound, get_error_message(FileNotFound),NULL);// Envoi d'un paquet d'erreur au client
            return -1;
        }

        data_packet.opcode = htons(TFTP_OPCODE_DATA);
        data_packet.block_num = htons(block_num);

        if (sendto(client->socket_fd, &data_packet, num_bytes_read + 4, 0, (struct sockaddr*)&client->client_addr, sizeof(client->client_addr)) == -1) {
            send_error_packet(client->socket_fd, &client->client_addr,FileNotFound, get_error_message(NotDefined), NULL);// Envoi d'un paquet d'erreur au client
            perror("Erreur lors de l'envoi du paquet de données");
            return -1;
        }

        // printf("[DATA] Packet : %d (%zd Bytes) -> @IP %s:%d\n", block_num,num_bytes_read+4,inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
        

        retryCount = 0;
        while (1) {
            ssize_t recvlen = recvfrom(client->socket_fd, &ack_packet, sizeof(ack_packet), 0, NULL, NULL);
            if (recvlen > 0 && ack_packet.opcode == htons(TFTP_OPCODE_ACK) && ack_packet.block_num == htons(block_num)) {
                // printf("[ACK]  Packet : %d <- @IP %s:%d\n", ntohs(ack_packet.block_num),inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
                break; // ACK reçu
            } else if (recvlen == -1) {
                // Timeout, retransmission
                if (retryCount < MAX_RETRIES) {
                    printf("Client[fd %d] Time Out !, retransmission du DATA %d\n",client->socket_fd ,block_num);
                    sendto(client->socket_fd, &data_packet, num_bytes_read + TFTP_HEADER_SIZE, 0, (struct sockaddr*)&client->client_addr, sizeof(client->client_addr));
                    retryCount++;
                } else {
                    printf("Client[fd %d] |-_-| Nombre maximum de tentatives atteint, abandon de la transmission.\n",client->socket_fd);
                    send_error_packet(client->socket_fd, &client->client_addr,NotDefined, get_error_message(NotDefined), NULL);
                    return -1;
                }
            }
        }

        block_num++;
        //  usleep(50000); //50ms
    } while (num_bytes_read == sizeof(data_packet.data));

    printf("Client[fd %d] |^_^| Transmission terminée avec succès. | file : %s (%zu Bytes)\n",client->socket_fd,request->filename, ftell(client->file));
    return 0;
}












/**
 * Fonction : handle_write_request
 * @brief : Cette fonction est responsable de la gestion d'une demande d'écriture (WRQ) provenant d'un client TFTP.
 * Elle écrit les données reçues du client dans un fichier, tout en envoyant les ACK correspondants après chaque paquet de données reçu.
 * En cas d'erreur, elle envoie un paquet d'erreur approprié au client.
 * @param client : Un pointeur vers la structure représentant le client TFTP.
 * @param request : Un pointeur vers la structure représentant la demande d'écriture.
 * @return : 0 sucess     -1 ERR
 */
int handle_write_request(TFTP_Client *client, TFTP_Request *request) {

    printf("[WRQ] @IP %s:%d, file: %s, Mode: %s\n", inet_ntoa(client->client_addr.sin_addr), ntohs(client->client_addr.sin_port), request->filename, request->mode);
  
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SECONDS;
    tv.tv_usec = 0;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    int retryCount = 0;


    // Envoi du premier ACK
    TFTP_AckPacket ackPacket;
    ackPacket.opcode = htons(TFTP_OPCODE_ACK);
    ackPacket.block_num = htons(0);
    sendto(client->socket_fd, &ackPacket, sizeof(ackPacket), 0, (struct sockaddr*)&client->client_addr, sizeof(client->client_addr));


    int blockNumber = 1;

    while (1) {
        TFTP_DataPacket dataPacket;
        ssize_t recvlen = recvfrom(client->socket_fd, &dataPacket, sizeof(dataPacket), 0, NULL, NULL);

        if (recvlen == -1) {
            // Timeout, retransmission de l'ACK précédent
            printf("Client[fd %d] Time Out !, retransmission de l'ACK %d\n",client->socket_fd,ntohs(ackPacket.block_num));
            if (retryCount < MAX_RETRIES) {
                sendto(client->socket_fd, &ackPacket, sizeof(ackPacket), 0, (struct sockaddr*)&client->client_addr, sizeof(client->client_addr));
                retryCount++;
                continue;
            } else {
                printf("Client[fd %d] |-_-| Nombre maximum de tentatives atteint, abandon de la transmission.\n",client->socket_fd);
                return -1;
            }
        }

        retryCount = 0; // reset

        if (dataPacket.opcode == htons(TFTP_OPCODE_DATA) && ntohs(dataPacket.block_num) == blockNumber-1) {
            sendto(client->socket_fd, &ackPacket, sizeof(ackPacket), 0, (struct sockaddr*)&client->client_addr, sizeof(client->client_addr));
            continue;
        }

        if (dataPacket.opcode == htons(TFTP_OPCODE_DATA) && ntohs(dataPacket.block_num) == blockNumber) {
            size_t bytesWritten = fwrite(dataPacket.data, 1, recvlen - 4, client->file);
            if ((int) bytesWritten < recvlen - 4) {
                printf("Erreur lors de l'écriture dans le fichier\n");
                send_error_packet(client->socket_fd, &client->client_addr, DiskFullOrAllocationExceeded, get_error_message(DiskFullOrAllocationExceeded),NULL);
                return -1;
            }

            // Envoi de l'ACK
            ackPacket.block_num = dataPacket.block_num;
            sendto(client->socket_fd, &ackPacket, sizeof(ackPacket), 0, (struct sockaddr*)&client->client_addr, sizeof(client->client_addr));

            if (recvlen < MAX_PACKET_SIZE) {
                // Dernier paquet reçu, fin de la transmission
                printf("Client[fd %d] |^_^| Réception terminée avec succès. | file : %s (%zu):\n",client->socket_fd, request->filename, ftell(client->file));
                break;
            }

            blockNumber++;
        } else if (ntohs(dataPacket.opcode) == TFTP_OPCODE_ERR) {
            printf("Erreur reçue du serveur : %s\n",dataPacket.data);
            return -1;
        } else {
            send_error_packet(client->socket_fd, &client->client_addr, NotDefined, get_error_message(NotDefined),NULL);
            return -1;
        }
        // usleep(50000); //50ms
    }

    // Fin fonction
    return 0;
}









/**
 * @brief : get_error_message
 * Description : Cette fonction retourne le message d'erreur correspondant à un code d'erreur TFTP spécifié.
 * @param error_code : Le code d'erreur TFTP pour lequel obtenir le message d'erreur.
 * @return : Le message d'erreur correspondant au code d'erreur spécifié.
 */
const char* get_error_message(int error_code) {
    if (error_code >= 0 && error_code < NUM_TFTP_ERRORS) {
        return TFTPErrorMessages[error_code];
    } else {
        return "Unknown error";
    }
}





/**
 * Fonction : send_error_packet
 * @brief : Cette fonction envoie un paquet d'erreur à un client TFTP avec le code d'erreur spécifié et un message d'erreur.
 * @param sockfd : Le descripteur de fichier du socket.
 * @param client_addr : Un pointeur vers la structure représentant l'adresse du client.
 * @param errorCode : Le code d'erreur TFTP à inclure dans le paquet d'erreur.
 * @param error_message : Le message d'erreur à inclure dans le paquet d'erreur.
 * @param additional_message : Un message supplémentaire à concaténer avec le message d'erreur (optionnel).
 * @return : Aucun
 */
void send_error_packet(int sockfd, struct sockaddr_in* client_addr, uint16_t errorCode, const char* error_message, const char* additional_message) {
    TFTP_ErrorPacket errPacket;
    errPacket.opcode = htons(TFTP_OPCODE_ERR);
    errPacket.err_code = htons(errorCode);

    // Copier le message d'erreur dans le paquet d'erreur
    strncpy(errPacket.err_msg, error_message, MAX_ERROR_MSG_LEN);

    // Si additional_message n'est pas NULL et qu'il reste de la place dans le buffer, le concaténer
    if (additional_message != NULL && strlen(errPacket.err_msg) + strlen(additional_message) + 1 < MAX_ERROR_MSG_LEN) {
        strncat(errPacket.err_msg, ": ", MAX_ERROR_MSG_LEN - strlen(errPacket.err_msg) - 1);
        strncat(errPacket.err_msg, additional_message, MAX_ERROR_MSG_LEN - strlen(errPacket.err_msg) - 1);
    }
    // Envoyer le paquet d'erreur
    sendto(sockfd, &errPacket, sizeof(errPacket), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
}








/**
 * Fonction : create_Socket
 * @brief : Cette fonction crée un socket UDP et le lie à une adresse IP et un port spécifiés.
 * @param ipAddress : L'adresse IP à laquelle lier le socket (ou NULL pour lier à toutes les interfaces).
 * @param port : Le numéro de port à utiliser pour le socket.
 * @return : Le descripteur de fichier du socket créé.
 */
int create_Socket(const char *ipAddress, int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);// Création du socket
    if (sockfd == -1) {
        perror("Erreur lors de la création du socket");
        exit(EXIT_FAILURE);
    }

    // Préparation de l'adresse pour le bind
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (ipAddress)
        server_addr.sin_addr.s_addr = inet_addr(ipAddress);
    else
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Lier le socket à l'adresse IP et au port spécifiés
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Erreur lors du bind du socket");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}



/**
 * Cette fonction génère un nom de fichier temporaire en ajoutant l'extension ".tmp" au nom du fichier original.
 * @param nom_fichier Le nom du fichier original.
 * @return Un pointeur vers une nouvelle chaîne de caractères contenant le nom du fichier temporaire.
 *         Il est de la responsabilité de l'appelant de libérer la mémoire allouée après usage.
 */
char* get_temp_file_name(const char* nom_fichier) {
    char* nouveau_nom = malloc(strlen(nom_fichier) + 5); // +5 pour ".tmp\0"

    // Vérifier si l'allocation de mémoire a réussi
    if (nouveau_nom == NULL) {
        perror("Erreur lors de l'allocation de mémoire");
        return NULL;
    }
    // Copier le nom du fichier original dans le nouveau nom
    strcpy(nouveau_nom, nom_fichier);
    strcat(nouveau_nom, ".tmp");// Ajouter l'extension ".tmp"
    return nouveau_nom;

}

















/*****************************************************************************************************************
 *                                                     SECTION 2                                                 *
 *                                              GESTION DES CLIENTS TFTP                                         *
 *****************************************************************************************************************/





/**
 * Fonction : initialiser_ListeClients
 * @brief : Cette fonction initialise une liste de clients TFTP en allouant la mémoire nécessaire et en initialisant les autres champs.
 * @param list_clients : Un pointeur vers la structure représentant la liste des clients TFTP.
 * @return : 0 en cas de succès, -1 en cas d'échec.
 */

int initialiser_ListeClients(TFTP_ClientsList* list_clients) {
    // Allouer de la mémoire pour la structure TFTP_ClientsList
    if (list_clients == NULL) {
        fprintf(stderr, "Erreur : Allocation de mémoire échouée\n");
        return -1;
    }
    list_clients->clients = NULL; // Initialiser le tableau de clients à NULL
    list_clients->nbClients = 0; // Initialiser le nombre de clients à 0
    pthread_mutex_init(&list_clients->mutex, NULL); // Initialiser le mutex
    return 0;
}







/**
 * Fonction : init_client
 * @brief : Cette fonction initialise un client TFTP en allouant la mémoire nécessaire et en copiant les informations de l'adresse IP et de la demande du client.
 * @param client_addr : La structure représentant l'adresse IP du client.
 * @param request : La demande du client TFTP.
 * @return : Un pointeur vers la structure représentant le client initialisé.
 */
TFTP_Client *init_client(struct sockaddr_in client_addr, const char *request) {
    // Allouer de la mémoire pour la structure TFTP_Client
    TFTP_Client *client = (TFTP_Client *)malloc(sizeof(TFTP_Client));
    if (client == NULL) {
        fprintf(stderr, "Erreur : Allocation de mémoire échouée\n");
        return NULL;
    }
    memcpy(&client->client_addr, &client_addr, sizeof(client_addr)); // Copier les informations de l'adresse IP et du port du client
    strcpy(client->packet, request);// Copier la demande du client
    
    return client;
}








/**
 * Fonction : ajouterClient
 * @brief : Cette fonction ajoute un client à la liste des clients TFTP.
 * @param nouveauClient : Un pointeur vers la structure représentant le nouveau client à ajouter.
 * @param clients_list : Un pointeur vers la structure représentant la liste des clients TFTP.
 * @return : Aucun
 */
void ajouterClient(TFTP_Client *nouveauClient, TFTP_ClientsList *clients_list) {

    pthread_mutex_lock(&clients_list->mutex);
    // Allouer ou réallouer de la mémoire pour la liste des clients
    TFTP_Client **nouveauTableau;
    if (clients_list->nbClients == 0) {
        nouveauTableau = (TFTP_Client **)malloc(sizeof(TFTP_Client *));
    } else {
        nouveauTableau = (TFTP_Client **)realloc(clients_list->clients, (clients_list->nbClients + 1) * sizeof(TFTP_Client *));
    }
    
    // Vérifier si l'allocation de mémoire a réussi
    if (nouveauTableau == NULL) {
        fprintf(stderr, "Erreur : Allocation de mémoire échouée\n");
        pthread_mutex_unlock(&clients_list->mutex);// Déverrouiller le mutex
        return;
    }
    
    // Trouver un emplacement vide pour le nouveau client
    int emplacementVide = -1;
    for (int i = 0; i < clients_list->nbClients; i++) {
        if (nouveauTableau[i] == NULL) {
            emplacementVide = i;
            break;
        }
    }
    
    // Si aucun emplacement vide n'a été trouvé, ajouter à la fin
    if (emplacementVide == -1) {
        emplacementVide = clients_list->nbClients;
    }
    
    // Ajouter le nouveau client à l'emplacement trouvé
    nouveauTableau[emplacementVide] = nouveauClient;
    // printf("client ajouté %d\n",emplacementVide);
    
    // Mettre à jour la liste des clients et le nombre de clients
    clients_list->clients = nouveauTableau;
    clients_list->nbClients++;

    
    pthread_mutex_unlock(&clients_list->mutex);// Déverrouiller le mutex
}








/**
 * Fonction : get_client
 * @brief : Cette fonction recherche un client dans la liste des clients TFTP en fonction de l'adresse IP et de la demande spécifiées.
 * @param listeClients : Un pointeur vers la structure représentant la liste des clients TFTP.
 * @param client_addr : La structure représentant l'adresse IP du client à rechercher.
 * @param request : La demande du client TFTP à rechercher.
 * @return : Un pointeur vers la structure représentant le client trouvé (ou NULL si non trouvé).
 */
TFTP_Client *get_client(TFTP_ClientsList *listeClients, struct sockaddr_in client_addr, const char *request) {
    pthread_mutex_lock(&listeClients->mutex);
    // Parcourir la liste des clients pour trouver celui correspondant à l'adresse IP et au port
    for (int i = 0; i < listeClients->nbClients; i++) {
        TFTP_Client *client = listeClients->clients[i];
        if (memcmp(&client->client_addr, &client_addr, sizeof(struct sockaddr_in)) == 0 && strcmp(client->packet, request) == 0) {
            pthread_mutex_unlock(&listeClients->mutex);
            return client;
        }
    }
    pthread_mutex_unlock(&listeClients->mutex);// Déverrouiller le mutex
    return NULL;// Aucun client trouvé avec l'adresse IP et le port correspondants
}








/**
 * Fonction : supprimer_client
 * @brief : Cette fonction supprime un client de la liste des clients TFTP.
 * @param listeClients : Un pointeur vers la structure représentant la liste des clients TFTP.
 * @param client : Un pointeur vers la structure représentant le client à supprimer.
 * @return : Aucun
 */
void supprimer_client(TFTP_ClientsList *listeClients, TFTP_Client *client) {

    pthread_mutex_lock(&listeClients->mutex);   // Verrouiller le mutex pour garantir l'accès exclusif à la liste
    for (int i = 0; i < listeClients->nbClients; i++) {
        if (listeClients->clients[i]->socket_fd == client->socket_fd) {
            // Libérer la mémoire allouée pour le client
            close(client->socket_fd);
            if (client->file != NULL){
                fclose(client->file);
            }
            listeClients->clients[i] = NULL;
            // printf("client %d supprimé\n",i);
            free(client);
            // Déplacer les clients suivants pour remplir l'espace
            for (int j = i; j < listeClients->nbClients - 1; j++) {
                listeClients->clients[j] = listeClients->clients[j + 1];
            }
            
            // Réduire la taille du tableau
            TFTP_Client **nouveauTableau = (TFTP_Client **)realloc(listeClients->clients, (listeClients->nbClients - 1) * sizeof(TFTP_Client *));
            if (nouveauTableau == NULL && listeClients->nbClients > 1) {
                fprintf(stderr, "Erreur : Réallocation de mémoire échouée\n");
                pthread_mutex_unlock(&listeClients->mutex);
                return;
            }
            // Mettre à jour la liste des clients et le nombre de clients
            listeClients->clients = nouveauTableau;
            listeClients->nbClients--;
            pthread_mutex_unlock(&listeClients->mutex);
            return;
        }
    }

    pthread_mutex_unlock(&listeClients->mutex); // Déverrouiller le mutex si le client n'est pas trouvé
}


