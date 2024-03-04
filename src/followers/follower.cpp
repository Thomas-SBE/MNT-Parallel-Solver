//  ·····
//  ····· Made by: Thomas-SBE
//  ····· 
// ◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤

#include "mpi.h"
#include "../lib.hpp"

#define CHANGED 1
#define UNCHANGED 0

//////////////////////////////////////////////// DATA ////////////////////////////////////////////////
int LINES;                      // - Nombre de lignes de la matrice MNT
int COLS;                       // - Nombre de colonnes de la matrice MNT
float NODATA;                   // - Valeur de NODATA dans MNT
int S_LINES;                    // - Nombre de lignes de la matrice MNT avec shadows
int S_COLS;                     // - Nombre de colonnes de la matrice MNT avec shadows
int nFollower, PID;             // - Nombre de processus follower + PID du processus actuel
MPI_Comm baseComm;              // - Communication temporaire remplacée par intercom
MPI_Comm intercom;              // - Communication entre les processus leader / follower
MPI_Win DATA;                   // - Fenètre vers les données de MNT
MPI_Win DIRECTIONS;             // - Fenètre vers les données de la matrice de direction
MPI_Win ACCUMULATION;           // - Fenètre vers les données de la matrice d'accumulation
MPI_Win CHANGES;                // - Genètre vers les données de la matrice de changements

//////////////////////////////////////////////// METHODS ////////////////////////////////////////////////
/*
    Description:
    Génère une matrice de directions (char), représenté dans l'énoncé de MNT.
    La fenètre DATA doit contenie des "shadows" c.a.d des bordures de NODATA.
*/
void FollowerDirections()
{
    int nOperations = 0;

    int linesToCopy = (LINES / nFollower) + (LINES % nFollower == 0 ? 0 : 1); // On prends une ligne en plus si on déborde.
    float* lData = new float[S_COLS*(linesToCopy+2)];
    char* dDirections = new char[S_COLS * (linesToCopy+2)];
    for(int i = 0; i < S_COLS * ((LINES/nFollower)+2); i++) dDirections[i] = DIRECTION_NODATA;

    int elementsToPlace = (LINES*S_COLS) / nFollower;
    int processDataOffset = (PID-1) * elementsToPlace + S_COLS;
    int currentLine = processDataOffset / S_COLS;

    MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, DATA);
    MPI_Get(lData, S_COLS*(linesToCopy+2), MPI_FLOAT, 0, (currentLine-1) * S_COLS, S_COLS*(linesToCopy+2), MPI_FLOAT, DATA);
    MPI_Win_unlock(0, DATA);

    for(int i = 0; i < elementsToPlace; i++){
        
        int offset = processDataOffset % S_COLS;
        int x = ((i + offset) % S_COLS);
        int y = ((i + offset) / S_COLS) + 1;

        if(lData[x + (y * S_COLS)] == NODATA) continue;

        int minval = __INT_MAX__;
        char mindir = 0;
        float* cells = new float[9];

        for(int e = 0; e < 9; e++){
            int lx = (e%3) - 1;
            int ly = (e/3) - 1;
            cells[e] = lData[x+lx + ((y+ly)*S_COLS)];
        }

        for(int i = 0; i < 9; i++){
            if(i == 4) continue; // Ne pas se compter soi-même
            if(cells[i] == NODATA) continue; // Ne pas prendre en compte les nodata
            if(cells[i] < minval){
                minval = cells[i];
                switch(i){
                    case 0: mindir = 1; break;
                    case 1: mindir = 2; break;
                    case 2: mindir = 3; break;
                    case 3: mindir = 8; break;
                    case 5: mindir = 4; break;
                    case 6: mindir = 7; break;
                    case 7: mindir = 6; break;
                    case 8: mindir = 5; break;
                    default: mindir = 0;
                }
            }
            if(minval >= cells[4]){
                // Si le minimum autour est plus grand que nous, alors aucune direction.
                mindir = 0;
            }
        }
        nOperations++;
        dDirections[x+(y*S_COLS)] = mindir;
    }

    // Restitution des connaissances.
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, DIRECTIONS);
    MPI_Put(dDirections+S_COLS+(processDataOffset%S_COLS), elementsToPlace, MPI_CHAR, 0, processDataOffset, elementsToPlace, MPI_CHAR, DIRECTIONS);
    MPI_Win_unlock(0, DIRECTIONS);

    MPI_Win_fence(0, DIRECTIONS);
}

/*
    Description:
    Génère sa liste locale des accumulations en partant d'un principe de
    simulation. Lorsqu'un process est sur une case, il va suivre le chemin
    de cette case vers ce "trou" (valeur 0). Etant donné qu'en théorie toute
    cellule doit finir dans un trou, nous pouvont simuler cet écoulement.
    Pour terminer, cette liste est accumulée sur le PID 0 pour avoir
    l'accumulation totale.

    Problème:
    Cette méthode requiert beaucoup de transactions mémoire pour un modèle
    sans copie (peu efficace sur GPU par exemple).

    Note importante: cette variante est gourmande en mémoire:
    `nFollower * COLS * LINES * sizeof(int)` octets.
*/
void FollowerSimulated()
{
    int* lACC = new int[COLS*LINES];

    for(int i = 0; i < COLS*LINES; i++) lACC[i] = 0;

    char* lDIR = new char[S_COLS*S_LINES];

    MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, DIRECTIONS);
    MPI_Get(lDIR, S_COLS*S_LINES, MPI_CHAR, 0, 0, S_LINES*S_COLS, MPI_CHAR, DIRECTIONS);
    MPI_Win_unlock(0, DIRECTIONS);


    for(int i = 0; i < (COLS*LINES) / nFollower; i++){
        
        int localPID = PID-1;
        int localOffset = localPID * ((LINES*COLS)/nFollower) + i;
        int x = (localOffset % COLS)+1; // On evite les shadows / ghosts
        int y = (localOffset / COLS)+1; // On evite les shadows / ghosts
        char D = lDIR[x+(y*S_COLS)];
        

        while(D != 0){
            lACC[(y-1)*COLS+(x-1)] += 1;
            // Déplacement en fonction de la direction
            switch(D){
                case 1: y--; x--; break;
                case 2: y--; break;
                case 3: y--; x++; break;
                case 4: x++; break;
                case 5: y++; x++; break;
                case 6: y++; break;
                case 7: y++; x--; break;
                case 8: x--; break;
                default:
                    printf("INVALID_VALUE_FOR_DIRECTION - [%d,%d] - (%d)\n", x, y, D);
                    return;
            }
            //if(PID == 4) printf("(%d, %d)\n", x, y);
            D = lDIR[x+(y*S_COLS)];
        }
        lACC[(y-1)*COLS+(x-1)] += 1; // On accumule aussi sur la cellule finale
    }

    MPI_Accumulate(lACC, COLS*LINES, MPI_INT, 0, 0, COLS*LINES, MPI_INT, MPI_SUM, ACCUMULATION);
    MPI_Win_fence(0, ACCUMULATION);
}

/*
    Description:
    Calcule son accumulation en regardant qui pointe vers lui, doit réitérer
    plusieurs fois son calcul et se synchroniser jusqu'a ce que la somme des 
    trous soit égal a la taille de la matrice e.g: 6*6 = 36.
    Cela part de la théorique qu'il ne peut pas y avoir de cycle, infini.
    Et que chaque cellule s'écoule dans un trou.
*/
void FollowerSelfAccumulated()
{

    MPI_Win_fence(0, CHANGES);

    int linesToCopy = (LINES / nFollower) + (LINES % nFollower == 0 ? 0 : 1); // On prends une ligne en plus si on déborde.
    int elementsToPlace = (LINES*S_COLS) / nFollower;
    int processDataOffset = (PID-1) * elementsToPlace + S_COLS;
    int currentLine = processDataOffset / S_COLS;

    int sum = COLS * LINES;
    char* lChanges = new char[S_COLS * (linesToCopy+2)];
    char* rChanges = new char[S_COLS * (linesToCopy+2)];
    int* lACC = new int[S_COLS * (linesToCopy+2)];
    char* lDirr = new char[S_COLS * (linesToCopy+2)];

    MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, DIRECTIONS);
    MPI_Get(lDirr, S_COLS*(linesToCopy+2), MPI_CHAR, 0, (currentLine-1) * S_COLS, S_COLS*(linesToCopy+2), MPI_CHAR, DIRECTIONS);
    MPI_Win_unlock(0, DIRECTIONS);

    while (sum != 0){
        sum = 0;

        // Récupérons les données
        MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, ACCUMULATION);
        MPI_Get(lACC, S_COLS*(linesToCopy+2), MPI_INT, 0, (currentLine-1) * S_COLS, S_COLS*(linesToCopy+2), MPI_INT, ACCUMULATION);
        MPI_Win_unlock(0, ACCUMULATION);
        MPI_Win_lock(MPI_LOCK_SHARED, 0, 0, CHANGES);
        MPI_Get(lChanges, S_COLS*(linesToCopy+2), MPI_CHAR, 0, (currentLine-1) * S_COLS, S_COLS*(linesToCopy+2), MPI_CHAR, CHANGES);
        MPI_Win_unlock(0, CHANGES);

        // Faire une copie de lChanges dans rChanges pour ne pas fausser les résultats.
        memcpy(rChanges, lChanges, S_COLS * (linesToCopy+2) * sizeof(char));

        for(int i = 0; i < elementsToPlace; i++){
            // 0. Pré-requis
            int offset = processDataOffset % S_COLS;
            int x = ((i + offset) % S_COLS);
            int y = ((i + offset) / S_COLS) + 1;

            if(lDirr[x + (y * S_COLS)] == DIRECTION_NODATA) continue;

            // 1. Récupérer ses alentours
            char* local_changes = new char[9];
            char* local_dirs = new char[9];
            int* local_acc = new int[9];

            for(int e = 0; e < 9; e++){
                int lx = (e%3) - 1;
                int ly = (e/3) - 1;
                local_acc[e] = lACC[x+lx + ((y+ly)*S_COLS)];
                local_changes[e] = lChanges[x+lx + ((y+ly)*S_COLS)];
                local_dirs[e] = lDirr[x+lx + ((y+ly)*S_COLS)];
            }

            // 2. Regarder autour de soi
            // Directions: NO:1 N:2 NE:3 E:4 SE:5 S:6 SO:7 O:8 Aucune:0
            bool somebodyHasChanged = false;
            int neighbour_acc = 0;
            for(int e = 0; e < 9; e++){
                if(e == 0 && local_dirs[e] == 5) { somebodyHasChanged = (local_changes[e] == 1 ? true : somebodyHasChanged); neighbour_acc += local_acc[e]; }
                if(e == 1 && local_dirs[e] == 6) { somebodyHasChanged = (local_changes[e] == 1 ? true : somebodyHasChanged); neighbour_acc += local_acc[e]; }
                if(e == 2 && local_dirs[e] == 7) { somebodyHasChanged = (local_changes[e] == 1 ? true : somebodyHasChanged); neighbour_acc += local_acc[e]; }
                if(e == 3 && local_dirs[e] == 4) { somebodyHasChanged = (local_changes[e] == 1 ? true : somebodyHasChanged); neighbour_acc += local_acc[e]; }
                if(e == 4) continue; // On ne se regarde pas soi-même
                if(e == 5 && local_dirs[e] == 8) { somebodyHasChanged = (local_changes[e] == 1 ? true : somebodyHasChanged); neighbour_acc += local_acc[e]; }
                if(e == 6 && local_dirs[e] == 3) { somebodyHasChanged = (local_changes[e] == 1 ? true : somebodyHasChanged); neighbour_acc += local_acc[e]; }
                if(e == 7 && local_dirs[e] == 2) { somebodyHasChanged = (local_changes[e] == 1 ? true : somebodyHasChanged); neighbour_acc += local_acc[e]; }
                if(e == 8 && local_dirs[e] == 1) { somebodyHasChanged = (local_changes[e] == 1 ? true : somebodyHasChanged); neighbour_acc += local_acc[e]; }
            }

            if(somebodyHasChanged)
            {
            
                // 3. Si quelqu'un a changé, on recalcule notre valeur
                lACC[x + (y * S_COLS)] = 1 + neighbour_acc;

                // 4. On désigne avoir changé de valeur !
                rChanges[x + (y * S_COLS)] = CHANGED;

                // 5. On compte le nombre de changements encore a faire.
                sum += 1;
            
            }else{

                // 3.bis. Si personne autour de nous ne change, aucune raison de changer nous-même.
                rChanges[x + (y * S_COLS)] = UNCHANGED;
            
            }
        }

        

        // 6. Tous les éléments sont terminés, partageons les résultats.
        int result;
        MPI_Allreduce(&sum, &result, 1, MPI_INT, MPI_SUM, intercom);
        sum = result;
        
        // Restitution des connaissances.
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, ACCUMULATION);
        MPI_Put(lACC+S_COLS+(processDataOffset%S_COLS), elementsToPlace, MPI_INT, 0, processDataOffset, elementsToPlace, MPI_INT, ACCUMULATION);
        MPI_Win_unlock(0, ACCUMULATION);
        MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, CHANGES);
        MPI_Put(rChanges+S_COLS+(processDataOffset%S_COLS), elementsToPlace, MPI_CHAR, 0, processDataOffset, elementsToPlace, MPI_CHAR, CHANGES);
        MPI_Win_unlock(0, CHANGES);

        MPI_Win_fence(0, CHANGES);

    }

}


//////////////////////////////////////////////// MAIN ////////////////////////////////////////////////
int main(int argc, char** argv) {
    

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nFollower);
    MPI_Comm_get_parent(&baseComm);

    MPI_Intercomm_merge(baseComm, 5, &intercom);
    MPI_Comm_rank(intercom, &PID);


    MPI_Bcast(&LINES, 1, MPI_INT, 0, intercom);
    MPI_Bcast(&COLS, 1, MPI_INT, 0, intercom);
    MPI_Bcast(&NODATA, 1, MPI_FLOAT, 0, intercom);

    // Taille de matrice avec shadows
    S_LINES = LINES + 2;
    S_COLS = COLS + 2;
    
    MPI_Win_create(NULL, 0, sizeof(char), MPI_INFO_NULL, intercom, &DATA);
    MPI_Win_fence(MPI_MODE_NOPRECEDE, DATA);
    MPI_Win_create(NULL, 0, sizeof(char), MPI_INFO_NULL, intercom, &DIRECTIONS);
    MPI_Win_fence(0, DIRECTIONS);
    MPI_Win_create(NULL, 0, sizeof(char), MPI_INFO_NULL, intercom, &ACCUMULATION);
    MPI_Win_fence(0, ACCUMULATION);
    MPI_Win_create(NULL, 0, sizeof(char), MPI_INFO_NULL, intercom, &CHANGES);
    MPI_Win_fence(0, CHANGES);

    // ==== PROCESSES ====
    FollowerDirections();
    //FollowerSimulated();
    FollowerSelfAccumulated();
    // ==== ========= ====

    MPI_Win_fence(0, CHANGES);
    MPI_Win_fence(0, ACCUMULATION);
    MPI_Win_fence(0, DIRECTIONS);
    MPI_Win_fence(MPI_MODE_NOSUCCEED, DATA);
    MPI_Win_free(&CHANGES);
    MPI_Win_free(&ACCUMULATION);
    MPI_Win_free(&DIRECTIONS);
    MPI_Win_free(&DATA);
    MPI_Comm_free(&intercom);
    MPI_Finalize();

    return EXIT_SUCCESS;

}