//  ·····
//  ····· Made by: Thomas-SBE
//  ····· 
// ◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤

#include "lib.hpp"

//////////////////////////////////////////////// DATA ////////////////////////////////////////////////
int *DATA;            // - Matrice des MNT
char *DIRECTIONS;       // - Matrice des directions de flots
int *ACCUMULATION;      // - Matrice des accumulations
char *CHANGED;          // - Matrice de changement quand une cellule a une nouvelle valeure
int LINES = 0;          // - Nombre de lignes de la matrice MNT
int COLS = 0;           // - Nombre de colones de la matrice MNT
int S_LINES = 0;        // - Nombre de lignes de DATA (MNT+ghosts)
int S_COLS = 0;         // - Nombre de colones de DATA (MNT+ghosts)
int NODATA = -1;      // - Valeur de NO_DATA


bool F_BASE = false;    // - Flag d'affichage de la matrice de base
bool F_DIR = false;     // - Flag d'affichage de la matrice de directions
bool F_ACC = false;     // - Flag d'affichage de la matrice d'accumulation

//////////////////////////////////////////////// METHODS ////////////////////////////////////////////////
/*
    Description:
    Génère une matrice de directions (char), représenté dans l'énoncé de MNT.
    La fenètre DATA doit contenie des "shadows" c.a.d des bordures de NODATA.
*/
void SEQ_Directions()
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


    for(int i = 0; i < S_LINES * S_COLS; i++){
        
        int offset = 0;
        int x = ((i + offset) % S_COLS);
        int y = ((i + offset) / S_COLS) + 1;

        if(DATA[x + (y * S_COLS)] == NODATA) continue;

        int minval = __INT_MAX__;
        char mindir = 0;
        int* cells = new int[9];

        for(int e = 0; e < 9; e++){
            int lx = (e%3) - 1;
            int ly = (e/3) - 1;
            cells[e] = DATA[x+lx + ((y+ly)*S_COLS)];
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
        }
        if(minval >= cells[4]){
            // Si le minimum autour est plus grand que nous, alors aucune direction.
            mindir = 0;
        }
        DIRECTIONS[x+(y*S_COLS)] = mindir;
    }

    
    if(F_DIR) displayDirectionsWithArrows(DIRECTIONS, S_COLS * S_LINES, S_COLS, 0, 0, DIRECTION_NODATA);
}

void SEQ_AccumulateBySelf()
{
    
    int sumOfChanges = 1;
    char* lChanges = new char[S_COLS * S_LINES];

    while(sumOfChanges != 0)
    {
        sumOfChanges = 0;
        // On fait une copie des changements de l'itération précédente pour ne pas influencer notre itération actuelle.
        for(int i = 0; i < S_LINES * S_COLS; i++) { lChanges[i] = CHANGED[i]; }
        for(int i = 0; i < S_LINES * S_COLS; i++){
            // 0. Pré-requis
            int offset = 0;
            int x = ((i + offset) % S_COLS);
            int y = ((i + offset) / S_COLS) + 1;

            if(DIRECTIONS[x + (y * S_COLS)] == DIRECTION_NODATA) continue;

            // 1. Récupérer ses alentours
            char* local_changes = new char[9];
            char* local_dirs = new char[9];
            int* local_acc = new int[9];

            for(int e = 0; e < 9; e++){
                int lx = (e%3) - 1;
                int ly = (e/3) - 1;
                local_acc[e] = ACCUMULATION[x+lx + ((y+ly)*S_COLS)];
                local_changes[e] = lChanges[x+lx + ((y+ly)*S_COLS)];
                local_dirs[e] = DIRECTIONS[x+lx + ((y+ly)*S_COLS)];
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
                ACCUMULATION[x + (y * S_COLS)] = 1 + neighbour_acc;

                // 4. On désigne avoir changé de valeur !
                CHANGED[x + (y * S_COLS)] = 1;

                // 5. On compte le nombre de changements encore a faire.
                sumOfChanges += 1;
            
            }else{

                // 3.bis. Si personne autour de nous ne change, aucune raison de changer nous-même.
                CHANGED[x + (y * S_COLS)] = 0;
            }

            delete local_changes;
            delete local_dirs;
            delete local_acc;
        }
    }

    if(F_ACC) displayData(ACCUMULATION, COLS*LINES, COLS, 1, 1);
}

//////////////////////////////////////////////// MAIN ////////////////////////////////////////////////
int main(int argc, char **argv)
{

    if (argc < 3)
        exit(EXIT_FAILURE);
    else if(argc == 3 && strlen(argv[2]) <= 3){
        size_t len = strlen(argv[2]);
        for(size_t i = 0; i < len; i++){
            if(i == 0) F_BASE = argv[2][i] == '1';
            if(i == 1) F_DIR = argv[2][i] == '1';
            if(i == 2) F_ACC = argv[2][i] == '1';
        }
    }

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


    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

    SEQ_Directions();
    SEQ_AccumulateBySelf();

    std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

    printf("[Execution Time] %f for %d elements in matrix, sequentially.\n", temps(start, end), COLS*LINES);


    return EXIT_SUCCESS;
}