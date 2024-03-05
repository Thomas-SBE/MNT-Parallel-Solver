//  ·····
//  ····· Made by: Thomas-SBE
//  ····· 
// ◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤

#define __CL_ENABLE_EXCEPTIONS

#include "lib.hpp"
#include "CL/opencl.hpp"
#include <vector>
#include <fstream>
#include <cstdlib>

#define KERNEL_SOURCE_FILE "kernels.cl"

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


/////////////////////////////////////////// OPEN CL DATA ///////////////////////////////////////////
std::vector<cl::Platform> clPLATFORMS;
std::vector<cl::Device> clDEVICES;
cl::Context* clCONTEXT;
cl::Program* clPROGRAM;
cl::CommandQueue* clQUEUE;

cl::Buffer bDATA;
cl::Buffer bDIRECTIONS;
cl::Buffer bCHANGES;
cl::Buffer bACCUMULATIONS;

int WORKGROUP_SIZEX = 128;
int WORKGROUP_SIZEY = 128;
int GPU_OPTIMAL_SUBDIVISION = 32;

//////////////////////////////////////////////// METHODS ////////////////////////////////////////////////
void OCL_Init()
{
    ///// ----- INITIALISATION OPENCL ---------
    cl::Platform::get(&clPLATFORMS);
    clPLATFORMS[0].getDevices(CL_DEVICE_TYPE_ALL, &clDEVICES);
    // - Creation du programme
    std::ifstream source (KERNEL_SOURCE_FILE);
    std::string sourceCode(std::istreambuf_iterator <char>(source),(std::istreambuf_iterator < char >()));
    clCONTEXT = new cl::Context(clDEVICES);
    clPROGRAM = new cl::Program(*clCONTEXT, sourceCode, true);
    try {
        (*clPROGRAM).build(clDEVICES);
    }catch (...){
        cl_int err = CL_SUCCESS;
        auto buildInfo = (*clPROGRAM).getBuildInfo<CL_PROGRAM_BUILD_LOG>(clDEVICES[0],&err);
        std::cerr << buildInfo << std::endl << std::endl;
        exit(0);
    }
    clQUEUE = new cl::CommandQueue(*clCONTEXT, clDEVICES[0]);
    GPU_OPTIMAL_SUBDIVISION = clDEVICES[0].getInfo<CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>();
    std::cout << "[Preflight] Prefered work-group subdivision for (" << clDEVICES[0].getInfo<CL_DEVICE_NAME>() << ") is " << GPU_OPTIMAL_SUBDIVISION << "." << std::endl;
}

void OCL_Directions()
{
    bDATA = cl::Buffer(*clCONTEXT, CL_MEM_READ_ONLY, S_LINES*S_COLS*sizeof(int));
    bDIRECTIONS = cl::Buffer(*clCONTEXT, CL_MEM_WRITE_ONLY, S_LINES*S_COLS*sizeof(char));

    (*clQUEUE).enqueueWriteBuffer(bDATA, CL_TRUE, 0, S_COLS*S_LINES*sizeof(int), DATA);

    cl::Kernel kDirections(*clPROGRAM, "directions");
    kDirections.setArg(0, bDATA);
    kDirections.setArg(1, bDIRECTIONS);
    kDirections.setArg(2, NODATA);
    kDirections.setArg(3, (char)DIRECTION_NODATA);

    cl::NDRange kGlobal(COLS, LINES);
    cl::NDRange kLocal(WORKGROUP_SIZEX, WORKGROUP_SIZEY);

    (*clQUEUE).enqueueNDRangeKernel(kDirections, cl::NullRange, kGlobal, kLocal);

    (*clQUEUE).enqueueReadBuffer(bDIRECTIONS, CL_TRUE, 0, S_COLS*S_LINES*sizeof(char), DIRECTIONS);

    if(F_DIR) displayDirectionsWithArrows(DIRECTIONS, S_COLS * S_LINES, S_COLS, 0, 0, DIRECTION_NODATA);
}

void OCL_Accumulations()
{
    bDIRECTIONS = cl::Buffer(*clCONTEXT, CL_MEM_READ_ONLY, S_LINES*S_COLS*sizeof(char));
    bACCUMULATIONS = cl::Buffer(*clCONTEXT, CL_MEM_READ_WRITE, S_LINES*S_COLS*sizeof(int));
    bCHANGES = cl::Buffer(*clCONTEXT, CL_MEM_READ_WRITE, S_LINES*S_COLS*sizeof(char));

    int localSumOfChanges[1] = {125};
    cl::Buffer bSumOfChanges = cl::Buffer(*clCONTEXT, CL_MEM_READ_WRITE, sizeof(int));

    (*clQUEUE).enqueueWriteBuffer(bDIRECTIONS, CL_TRUE, 0, S_COLS*S_LINES*sizeof(char), DIRECTIONS);
    (*clQUEUE).enqueueWriteBuffer(bACCUMULATIONS, CL_TRUE, 0, S_COLS*S_LINES*sizeof(int), ACCUMULATION);
    (*clQUEUE).enqueueWriteBuffer(bCHANGES, CL_TRUE, 0, S_COLS*S_LINES*sizeof(char), CHANGED);
    
    while (localSumOfChanges[0] != 0)
    {
        localSumOfChanges[0] = 0;
        (*clQUEUE).enqueueWriteBuffer(bSumOfChanges, CL_TRUE, 0, sizeof(int), localSumOfChanges);
        cl::Kernel kDirections(*clPROGRAM, "accumulations");
        kDirections.setArg(0, bDIRECTIONS);
        kDirections.setArg(1, bACCUMULATIONS);
        kDirections.setArg(2, bCHANGES);
        kDirections.setArg(3, NODATA);
        kDirections.setArg(4, (char)DIRECTION_NODATA);
        kDirections.setArg(5, bSumOfChanges);

        cl::NDRange kGlobal(COLS, LINES);
        cl::NDRange kLocal(WORKGROUP_SIZEX, WORKGROUP_SIZEY);

        (*clQUEUE).enqueueNDRangeKernel(kDirections, cl::NullRange, kGlobal, kLocal);
        (*clQUEUE).finish();
        (*clQUEUE).enqueueReadBuffer(bSumOfChanges, CL_TRUE, 0, sizeof(int), localSumOfChanges);
    }

    (*clQUEUE).enqueueReadBuffer(bACCUMULATIONS, CL_TRUE, 0, S_COLS*S_LINES*sizeof(int), ACCUMULATION);

    if(F_ACC) displayData(ACCUMULATION, COLS * LINES, COLS, 1, 1);
}

int LIB_LargestDivisorPower(int value){
    // - La carte graphique permet d'avoir seulement un certain nombre d'items par groupe,
    // - on prends une subdivision optimale de ce groupe (carré) pour trouver un multiple correspondant.
    for(int i = GPU_OPTIMAL_SUBDIVISION; i >= 1; i = i/2) if( value % i == 0 && i % 2 == 0 && i <= value ) return i;
}


//////////////////////////////////////////////// MAIN ////////////////////////////////////////////////
int main(int argc, char** argv)
{
    
    if (argc < 2)
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

    try{
        OCL_Init();
    }catch(cl::Error err) {
        std::cout << "Exception\n";
        std::cerr << "ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
        return EXIT_FAILURE;
    }

    WORKGROUP_SIZEX = LIB_LargestDivisorPower(COLS);
    WORKGROUP_SIZEY = LIB_LargestDivisorPower(LINES);
    printf("[Preflight] Using work-groups of size (%d,%d) to solve a matrix of size (%d,%d).\n", WORKGROUP_SIZEX, WORKGROUP_SIZEY, COLS, LINES);

    try{
        
        std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
        OCL_Directions();
        OCL_Accumulations();
        std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

        printf("[Execution Time] %f for %d elements in matrix, with %d * %d work items per group.\n", temps(start, end), COLS*LINES, WORKGROUP_SIZEX, WORKGROUP_SIZEY);

    }catch(cl::Error err) {
        std::cout << "Exception\n";
        std::cerr << "ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
        return EXIT_FAILURE;
    }

    delete clCONTEXT;
    delete clPROGRAM;
    delete clQUEUE;

    return EXIT_SUCCESS;

}