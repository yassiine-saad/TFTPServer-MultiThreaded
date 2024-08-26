
/**
 * @file sync.h
 * @brief Définition des fonctions et des structures pour la synchronisation des accès aux fichiers.
 */


#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef SYNC_H
#define SYNC_H

#define MAX_CLIENTS 500


/**
 * @struct FileEntry
 * @brief Structure représentant un fichier avec son mutex et sa variable de condition.
 */
typedef struct FileEntry {
    char filename[256]; 
    pthread_mutex_t rw_mutex; /** Mutex pour synchronisation */ 
    pthread_cond_t cond; /** Variable de condition */
    int actif_readers;  /* les lecteurs actifs */
    int num_readers;    /* Nombre de lecteurs actuels */
    int num_writers;    /* Nombre d'écrivains actuels */
}FileEntry;




/**
 * @struct FileList
 * @brief Structure représentant une liste de fichiers avec leur mutex.
 */
typedef struct FileList{
    FileEntry* files[MAX_CLIENTS]; // Liste des fichiers
    int num_files; // Nombre de fichiers dans la liste
    pthread_mutex_t files_mutex;
}FileList;




typedef void (*Sync_Function)(char *filename, FileList* file_list);

void initialize_fileList(FileList* file_list);
FileEntry* get_or_create_fileEntry(const char* filename, FileList* file_list);
FileEntry* get_fileEntry(const char* filename, FileList* file_list);
FileEntry* create_fileEntry(const char* filename, FileList* file_list);
int add_fileEntry(FileEntry* new_entry, FileList* file_list);
int delete_fileEntry(const char* filename, FileList* fileList);

void sync_start_read(char *filename, FileList* file_list);
void sync_end_read(char *filename, FileList* file_list);
void sync_start_write(char *filename, FileList* file_list);
void sync_end_write(char *filename, FileList* file_list);

#endif