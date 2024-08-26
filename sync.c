/**
 * @file sync.c
 * @brief Implémentation des fonctions de synchronisation des accès aux fichiers.
 */


#include "sync.h"



/**
 * Fonction : initialize_fileList
 * Description : Cette fonction initialise la liste des fichiers en allouant de la mémoire pour la liste et en initialisant les mutex et pointeurs de fichiers.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : Aucune valeur de retour
 */
void initialize_fileList(FileList* file_list) {
    if (file_list != NULL) {
        file_list->num_files = 0;
        pthread_mutex_init(&(file_list->files_mutex), NULL);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            file_list->files[i] = NULL;
        }
    }
}




/**
 * Fonction : get_or_create_fileEntry
 * Description : Cette fonction recherche un fichier dans la liste des fichiers et le crée s'il n'existe pas encore.
 * @param filename : Le nom du fichier à rechercher ou à créer.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : Un pointeur vers la structure représentant l'entrée du fichier recherché ou créé.
 */
FileEntry* get_or_create_fileEntry(const char* filename, FileList* file_list) {

    FileEntry* file = get_fileEntry(filename,file_list);

    if (file == NULL){
        file = create_fileEntry(filename,file_list);
        if (add_fileEntry(file,file_list) < 0) {
            file = NULL;
            free(file);
        }
    }
    
    return file;
}



/**
 * Fonction : get_fileEntry
 * Description : Cette fonction recherche un fichier dans la liste des fichiers en fonction de son nom.
 * @param filename : Le nom du fichier à rechercher.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : Un pointeur vers la structure représentant l'entrée du fichier recherché, ou NULL s'il n'est pas trouvé.
 */
FileEntry* get_fileEntry(const char* filename, FileList* file_list) {

    for (int i = 0; i < file_list->num_files; ++i) {
        if (strcmp(file_list->files[i]->filename, filename) == 0) {
            return file_list->files[i]; // Fichier trouvé
        }
    }
    
    return NULL;// Si le fichier n'est pas trouvé
}





/**
 * Fonction : create_fileEntry
 * Description : Cette fonction crée une nouvelle entrée de fichier avec le nom spécifié.
 * @param filename : Le nom du fichier à créer.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : Un pointeur vers la structure représentant la nouvelle entrée de fichier, ou NULL en cas d'erreur.
 */
FileEntry* create_fileEntry(const char* filename, FileList* file_list) {
    if (filename == NULL || file_list == NULL) {
        return NULL;
    }

    // Vérifier si le nombre maximum de fichiers est atteint
    if (file_list->num_files >= MAX_CLIENTS) {
        return NULL; // Dépassement de capacité
    }

    FileEntry* new_entry = (FileEntry*)malloc(sizeof(FileEntry));
    if (new_entry != NULL) {
        // Initialisation de la nouvelle entrée de fichier
        strcpy(new_entry->filename, filename);
        pthread_mutex_init(&(new_entry->rw_mutex), NULL);
        pthread_cond_init(&(new_entry->cond), NULL);
        new_entry->actif_readers = 0;
        new_entry->num_readers = 0;
        new_entry->num_writers = 0;
    }

    return new_entry;
}





/**
 * Fonction : add_fileEntry
 * Description : Cette fonction ajoute une nouvelle entrée de fichier à la liste des fichiers.
 * @param new_entry : Un pointeur vers la nouvelle entrée de fichier à ajouter.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : 0 en cas de succès, -1 en cas d'échec.
 */
int add_fileEntry(FileEntry* new_entry, FileList* file_list) {
    if (new_entry == NULL || file_list == NULL) {
        return -1;
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) {  // Recherche d'un emplacement vide dans le tableau files
        if (file_list->files[i] == NULL) {
            file_list->files[i] = new_entry;  // Ajout de la nouvelle entrée à l'emplacement vide trouvé
            file_list->num_files++;
            // printf("fichier Ajouté %d\n",i);
            return 0;
        }
    }
    return -1; // Aucun emplacement vide trouvé
}





/**
 * Fonction : delete_fileEntry
 * Description : Cette fonction supprime une entrée de fichier de la liste des fichiers.
 * @param filename : Le nom du fichier à supprimer.
 * @param fileList : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : 0 en cas de succès, -1 en cas d'échec.
 */
int delete_fileEntry(const char* filename, FileList* fileList) {
    pthread_mutex_lock(&(fileList->files_mutex)); // Verrouillage du mutex
    
    FileEntry* entry = get_fileEntry(filename, fileList);// Recherche de l'entrée du fichier dans la liste
    if (entry == NULL) {
        pthread_mutex_unlock(&(fileList->files_mutex));
        return -1; // Fichier non trouvé
    }
    
    if (entry->num_readers == 0 && entry->num_writers == 0 && pthread_mutex_trylock(&(entry->rw_mutex)) == 0) { // Vérification si aucun lecteur ni écriture en cours
    // if (entry->num_readers == 0 && entry->num_writers == 0) {
        // Suppression de l'entrée du fichier
        for (int i = 0; i < fileList->num_files; ++i) {
            if (fileList->files[i] == entry) {
                free(fileList->files[i]);
                fileList->files[i] = NULL;
                // printf("fichier supprimé %d\n",i);
                break;
            }
        }
        
        
        fileList->num_files--; // Décrémentation du nombre de fichiers dans la liste
        pthread_mutex_unlock(&(fileList->files_mutex)); // Déverrouillage du mutex
        // pthread_cond_broadcast(&file->cond);
        return 0; // Suppression réussie
    }
    
    pthread_mutex_unlock(&(fileList->files_mutex)); // Déverrouillage du mutex
    // pthread_cond_broadcast(&file->cond);
    return -1; // Impossible de supprimer le fichier car des opérations en cours
}










/**
 * Fonction : sync_start_read
 * Description : Cette fonction signale le début d'une opération de lecture sur un fichier.
 * @param filename : Le nom du fichier sur lequel l'opération de lecture démarre.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : Aucun
 */
void sync_start_read(char *filename, FileList* file_list){
    pthread_mutex_lock(&(file_list->files_mutex)); // Verrouillage du mutex de la list
    FileEntry* file = get_or_create_fileEntry(filename,file_list);
    if (file == NULL){
        return;
    }

    file->num_readers++; // ++ Nb lecteurs qui veulent effectuer un lecture
    while (pthread_mutex_trylock(&(file->rw_mutex)) != 0 && file->actif_readers == 0) {
        // Si le verrouillage du mutex échoue
        printf("file %s in use(Writing) ! please wait -_-\n",filename);
        pthread_cond_wait(&file->cond, &file_list->files_mutex);
        if (file->num_readers > 1){
            break;
        }
    }
    file->actif_readers++;
    pthread_mutex_unlock(&(file_list->files_mutex));
    
}




/**
 * Fonction : sync_end_read
 * Description : Cette fonction signale la fin d'une opération de lecture sur un fichier.
 * @param filename : Le nom du fichier sur lequel l'opération de lecture se termine.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : Aucun
 */
void sync_end_read(char *filename, FileList* file_list){
    pthread_mutex_lock(&(file_list->files_mutex)); // Verrouillage du mutex de la list
    FileEntry* file = get_fileEntry(filename,file_list);
    if (file == NULL){
        return;
    }

    file->num_readers--;
    file->actif_readers--;
    if (file->num_readers == 0) {
        pthread_mutex_unlock(&file->rw_mutex); // Réveiller un éventuel thread en attente d'écriture
        pthread_cond_broadcast(&file->cond); // Réveiller un éventuel thread en attente d'écriture
    }
    pthread_mutex_unlock(&(file_list->files_mutex)); // Déverrouiller l'accès à la liste des fichiers
    delete_fileEntry(filename,file_list);
}



/**
 * Fonction : sync_start_write
 * Description : Cette fonction signale le début d'une opération d'écriture sur un fichier.
 * @param filename : Le nom du fichier sur lequel l'opération d'écriture démarre.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : Aucun
 */
void sync_start_write(char *filename, FileList* file_list){
    pthread_mutex_lock(&(file_list->files_mutex)); // Verrouiller le mutex de la liste des fichiers

    FileEntry* file = get_or_create_fileEntry(filename,file_list);  // Récupérer ou créer une entrée de fichier pour le fichier spécifié
    if (file == NULL){
        return;
    }

    file->num_writers++;
    while (pthread_mutex_trylock(&file->rw_mutex) != 0 || file->actif_readers > 0) {
        // Si le verrouillage du mutex échoue
        printf("file %s in use ! please wait -_-\n",filename);
        pthread_cond_wait(&file->cond, &file_list->files_mutex);
    }
    pthread_mutex_unlock(&(file_list->files_mutex)); // Déverrouiller l'accès à la liste des fichiers
}





/**
 * Fonction : sync_end_write
 * Description : Cette fonction signale la fin d'une opération d'écriture sur un fichier.
 * @param filename : Le nom du fichier sur lequel l'opération d'écriture se termine.
 * @param file_list : Un pointeur vers la structure représentant la liste des fichiers.
 * @return : Aucun
 */
void sync_end_write(char *filename, FileList* file_list){
    pthread_mutex_lock(&(file_list->files_mutex)); // Verrouillage du mutex de la list
    FileEntry* file = get_fileEntry(filename,file_list);    // Récupérer l'entrée de fichier pour le fichier spécifié

    if (file == NULL){
        return;
    }

    file->num_writers--;

    pthread_mutex_unlock(&file->rw_mutex);  // Déverrouiller le verrou de lecture/écriture
    pthread_mutex_unlock(&(file_list->files_mutex));    // Déverrouiller le mutex de la liste des fichiers
    pthread_cond_broadcast(&file->cond);    // Réveiller tous les threads en attente
    delete_fileEntry(filename,file_list);   // Supprimer l'entrée de fichier de la liste des fichiers
     
}





