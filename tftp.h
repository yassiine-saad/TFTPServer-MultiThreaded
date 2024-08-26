/*********************************************************************************************
 *                                   @file : tftp.h                                        *
 *                                   Description :                                           *
 * Ce fichier contient les déclarations des structures, constantes et fonctions nécessaires  *
 * pour le fonctionnement du protocole TFTP (Trivial File Transfer Protocol).                *
 *********************************************************************************************/




#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>

#include <strings.h>

#include "sync.h"

#ifndef TFTP_H
#define TFTP_H


#define TFTP_OPCODE_RRQ 1
#define TFTP_OPCODE_WRQ 2
#define TFTP_OPCODE_DATA 3
#define TFTP_OPCODE_ACK 4
#define TFTP_OPCODE_ERR 5



#define TIMEOUT_SECONDS 5
#define MAX_RETRIES 4


#define MAX_PACKET_SIZE 516
#define MAX_DATA_SIZE 512
#define TFTP_HEADER_SIZE 4
#define MAX_ERROR_MSG_LEN 512






typedef struct {
    uint16_t opcode;
    char filename[512];
    char mode[10]; // octet | netascci
} TFTP_Request; // Structure représentant une demande TFTP

typedef struct {
    uint16_t opcode;
    uint16_t err_code;
    char err_msg[512];
} TFTP_ErrorPacket; // Structure représentant un paquet d'erreur TFTP


typedef struct {
    uint16_t opcode;
    uint16_t block_num;
    char data[512];
} TFTP_DataPacket;  // Structure représentant un paquet de données TFTP

typedef struct {
    uint16_t opcode;
    uint16_t block_num;
} TFTP_AckPacket;   // Structure représentant un paquet ACK TFTP



// Enumération des codes d'erreur TFTP
enum TFTPError {
    NotDefined = 0,
    FileNotFound = 1,
    AccessViolation = 2,
    DiskFullOrAllocationExceeded = 3,
    IllegalOperation = 4,
    UnknownTransferID = 5,
    FileAlreadyExists = 6,
    NoSuchUser = 7,
    NUM_TFTP_ERRORS 
};


// Structure représentant un client TFTP
typedef struct {
    int socket_fd;                  
    struct sockaddr_in client_addr; 
    socklen_t addr_len;             
    char filename[504];
    char packet[MAX_PACKET_SIZE];
    FILE* file;
} TFTP_Client;


typedef struct {
    TFTP_Client** clients;
    int nbClients;
    pthread_mutex_t mutex;
} TFTP_ClientsList;





/*********************************************************************************************************
 *                                                SECTION 1                                              *
 *                                    GESTION DES REQUÊTES TFTP (WRQ/RRQ)                                 *
 *********************************************************************************************************/

int handle_read_request(TFTP_Client *client, TFTP_Request *request); // Gère une demande de lecture
int handle_write_request(TFTP_Client *client, TFTP_Request *request);  // Gère une demande d'écriture
int create_Socket(const char *ipAddress, int port); // Crée un socket (avec Bind)
const char* get_error_message(int error_code);  // Obtient le message d'erreur correspondant à un code
void send_error_packet(int sockfd, struct sockaddr_in* client_addr, uint16_t errorCode, const char* error_message, const char* additional_message); // Envoie un paquet d'erreur
char* get_temp_file_name(const char* nom_fichier);

/*****************************************************************************************************************
 *                                                     SECTION 2                                                 *
 *                                              GESTION DES CLIENTS TFTP                                         *
 ******************************************************************************************************************/

int initialiser_ListeClients(TFTP_ClientsList* list_clients);   // Initialise une liste de clients
TFTP_Client *init_client(struct sockaddr_in client_addr, const char *request);  // Initialise un client
void ajouterClient(TFTP_Client *nouveauClient, TFTP_ClientsList *clients_list); // Ajoute un client à la liste
TFTP_Client *get_client(TFTP_ClientsList *listeClients, struct sockaddr_in client_addr, const char *request);   // Recherche un client dans la liste
void supprimer_client(TFTP_ClientsList *listeClients, TFTP_Client *client); // Supprime un client de la liste


#endif