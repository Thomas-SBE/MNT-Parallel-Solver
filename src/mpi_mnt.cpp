//  ·····
//  ····· Made by: Thomas-SBE
//  ····· 
// ◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤

#include "lib.hpp"
#include "mpi.h"


#define FOLLOWER_EXECNAME "./follower"


//////////////////////////////////////////////// DATA ////////////////////////////////////////////////
float *DATA;            // - Matrice des MNT
char *DIRECTIONS;       // - Matrice des directions de flots
int *ACCUMULATION;      // - Matrice des accumulations
char *CHANGED;          // - Matrice de changement quand une cellule a une nouvelle valeure
int LINES = 0;          // - Nombre de lignes de la matrice MNT
int COLS = 0;           // - Nombre de colones de la matrice MNT
int S_LINES = 0;        // - Nombre de lignes de DATA (MNT+ghosts)
int S_COLS = 0;         // - Nombre de colones de DATA (MNT+ghosts)
float NODATA = -1;      // - Valeur de NO_DATA
int PID, nLeader;       // - PID du processus courant + nombre de leaders (ici normalement max. 1)
int nFollowers;         // - Nombre de followers par leader
MPI_Comm intercom;      // - Intercommunicateur leader - follower
MPI_Win wDATA;          // - Fenètre de données vers la matrice DATA
MPI_Win wDIRECTIONS;    // - Fenètre de données vers la matrice DIRECTIONS
MPI_Win wACCUMULATION;  // - Fenètre de données vers la matrice ACCUMULATION
MPI_Win wCHANGED;       // - Fenètre de données vers la matrice CHANGED


bool F_BASE = false;    // - Flag d'affichage de la matrice de base
bool F_DIR = false;     // - Flag d'affichage de la matrice de directions
bool F_ACC = false;     // - Flag d'affichage de la matrice d'accumulation

//////////////////////////////////////////////// METHODS ////////////////////////////////////////////////
/*
    Description:
    Cette fonction doit être appelées en premier dans le script elle permet de transférer les données
    essentielles pour le bon fonctionnement du programme telles que: LINES, COLS, NODATA...
    Elle créer aussi l'environnement de travail, fenètres, communicateurs.
*/
void MPI_Initialisation(int argc, char** argv)
{
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nLeader);
    MPI_Comm_rank(MPI_COMM_WORLD, &PID);

    MPI_Comm _tmp;
    MPI_Comm_spawn(
        FOLLOWER_EXECNAME,
        MPI_ARGV_NULL,
        nFollowers,
        MPI_INFO_NULL,
        0,
        MPI_COMM_WORLD,
        &_tmp,
        MPI_ERRCODES_IGNORE);
    MPI_Intercomm_merge(_tmp, 0, &intercom);

    MPI_Bcast(&LINES, 1, MPI_INT, 0, intercom);
    MPI_Bcast(&COLS, 1, MPI_INT, 0, intercom);
    MPI_Bcast(&NODATA, 1, MPI_FLOAT, 0, intercom);

    MPI_Win_create(DATA, S_COLS * S_LINES * sizeof(float), sizeof(float), MPI_INFO_NULL, intercom, &wDATA);
    MPI_Win_fence(MPI_MODE_NOPRECEDE, wDATA);
    MPI_Win_create(DIRECTIONS, S_COLS * S_LINES * sizeof(char), sizeof(char), MPI_INFO_NULL, intercom, &wDIRECTIONS);
    MPI_Win_fence(0, wDIRECTIONS);
    MPI_Win_create(ACCUMULATION, S_LINES * S_COLS * sizeof(int), sizeof(int), MPI_INFO_NULL, intercom, &wACCUMULATION);
    MPI_Win_fence(0, wACCUMULATION);
    MPI_Win_create(CHANGED, S_LINES * S_COLS * sizeof(char), sizeof(char), MPI_INFO_NULL, intercom, &wCHANGED);
    MPI_Win_fence(0, wCHANGED);
}

/*
    Description:
    Cette fonction doit être la dernière appelée, aucune communication MPI ne sera possible a l'issue
    de cette fonction. Permet le nettoyage des données MPI telles que les fenètres, communicateurs...
*/
void MPI_Cleanup()
{
    MPI_Win_fence(0, wCHANGED);
    MPI_Win_fence(0, wACCUMULATION);
    MPI_Win_fence(0, wDIRECTIONS);
    MPI_Win_fence(MPI_MODE_NOSUCCEED, wDATA);
    MPI_Win_free(&wCHANGED);
    MPI_Win_free(&wACCUMULATION);
    MPI_Win_free(&wDIRECTIONS);
    MPI_Win_free(&wDATA);
    MPI_Comm_free(&intercom);
    MPI_Finalize();
}

/*
    Description:
    Génère une matrice de directions (char), représenté dans l'énoncé de MNT.
    La fenètre DATA doit contenie des "shadows" c.a.d des bordures de NODATA.
*/
void MPI_Directions()
{
    /*
        Le processus root prépare les shadows pour les accumulations (plus tard),
        mais les processus follower peuvent commencer leurs calculs, ils ne dépendent
        pas des shadows de DIRECTIONS, et aucune de ces cellules n'est censé être touchée.
    */
    for (int i = 0; i < std::max(S_COLS, S_LINES); i++){
        DIRECTIONS[(i < S_COLS ? i : S_COLS)] = DIRECTION_NODATA;
        DIRECTIONS[(i < S_COLS ? i : S_COLS) + (S_LINES-1) * (S_COLS)] = DIRECTION_NODATA;
        DIRECTIONS[(i < S_LINES ? i : S_LINES) * (S_COLS)] = DIRECTION_NODATA;
        DIRECTIONS[(i < S_LINES ? i : S_LINES) * (S_COLS) + (S_COLS-1)] = DIRECTION_NODATA;
    }
    /*
        Le processus démarre le calcul des directions dès qu'il est lancé,
        Il suffit alors d'attendre que toutes les instances de "direction"
        aient finies.
    */
    MPI_Win_fence(0, wDIRECTIONS);
    if(F_DIR) displayDirectionsWithArrows(DIRECTIONS, S_COLS * S_LINES, S_COLS, 0, 0, DIRECTION_NODATA);
}


void MPI_MemHungryProcess()
{
    /*
        Le processus démarre le calcul des accumulations dès que direction est fini,
        Il suffit alors d'attendre que toutes les instances de "accumulation"
        aient finies.
    */
    MPI_Win_fence(0, wACCUMULATION);
    if(F_ACC) displayData(ACCUMULATION, COLS*LINES, COLS);
}

void MPI_AccumulateBySelf()
{
    /*
        Le processus démarre le calcul des accumulations dès que direction est fini,
        Il suffit alors d'attendre que toutes les instances de "accumulation"
        aient finies.
    */
    MPI_Win_fence(0, wCHANGED); // REsync
    int sum = LINES * COLS;
    while(sum != 0){
        sum = 0;
        int result;
        MPI_Allreduce(&sum, &result, 1, MPI_INT, MPI_SUM, intercom);
        sum = result;
        MPI_Win_fence(0, wCHANGED);
    }
    if(F_ACC) displayData(ACCUMULATION, COLS*LINES, COLS, 1, 1);
}

//////////////////////////////////////////////// MAIN ////////////////////////////////////////////////
int main(int argc, char **argv)
{

    if (argc < 3)
        exit(EXIT_FAILURE);
    else if(argc == 4 && strlen(argv[3]) <= 3){
        size_t len = strlen(argv[3]);
        for(size_t i = 0; i < len; i++){
            if(i == 0) F_BASE = argv[3][i] == '1';
            if(i == 1) F_DIR = argv[3][i] == '1';
            if(i == 2) F_ACC = argv[3][i] == '1';
        }
    }

    nFollowers = atoi(argv[2]);

    readFileData(argv[1], &COLS, &LINES, &NODATA, &DATA);
    // Ajouter shadow regions a la taille de la matrice
    S_LINES = LINES + 2;
    S_COLS = COLS + 2;

    if(F_BASE) displayData(DATA, S_LINES * S_COLS, S_COLS);

    DIRECTIONS = new char[S_LINES * S_COLS];
    ACCUMULATION = new int[S_COLS * S_LINES];
    CHANGED = new char[S_LINES * S_COLS];
    for (int i = 0; i < S_COLS * S_LINES; i++)
    {
        DIRECTIONS[i] = 0;
        ACCUMULATION[i] = 1; // DOIT ETRE A 1 POUR LA METHODE AccumulateBySelf ET 0 POUR MemHungry
        CHANGED[i] = 1;
    }

    for (int i = 0; i < std::max(S_COLS, S_LINES); i++){
        CHANGED[(i < S_COLS ? i : S_COLS)] = 0;
        CHANGED[(i < S_COLS ? i : S_COLS) + (S_LINES-1) * (S_COLS)] = 0;
        CHANGED[(i < S_LINES ? i : S_LINES) * (S_COLS)] = 0;
        CHANGED[(i < S_LINES ? i : S_LINES) * (S_COLS) + (S_COLS-1)] = 0;
        ACCUMULATION[(i < S_COLS ? i : S_COLS)] = 0;
        ACCUMULATION[(i < S_COLS ? i : S_COLS) + (S_LINES-1) * (S_COLS)] = 0;
        ACCUMULATION[(i < S_LINES ? i : S_LINES) * (S_COLS)] = 0;
        ACCUMULATION[(i < S_LINES ? i : S_LINES) * (S_COLS) + (S_COLS-1)] = 0;
    }

    MPI_Initialisation(argc, argv);

    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

    MPI_Directions();
    //MPI_MemHungryProcess();
    MPI_AccumulateBySelf();

    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

    printf("[Execution Time] %f for %d elements in matrix, with %d process.\n", temps(start, end), COLS*LINES, nFollowers);

    MPI_Cleanup();

    return EXIT_SUCCESS;
}