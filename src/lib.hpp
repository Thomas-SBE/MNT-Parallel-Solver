//  ·····
//  ····· Made by: Thomas-SBE
//  ····· 
// ◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤

#ifndef TSBE_LIB_CPP
#define TSBE_LIB_CPP
#define LOGGED(x) if(VERBOSE)(x);

#include <iostream>
#include <chrono>
#include <ctime>
#include <cstring>

//////////////////////////////////////////////// DATA ////////////////////////////////////////////////

// Enables debug mod and gives more informations
#define VERBOSE false
// Valeur de NODATA pour les directions, ne peut être NODATA car DIRECTIONS est de type char (max. 255)
#define DIRECTION_NODATA 64

/*
    Description:
    Résentation en Unicode des flèches pour les directions telles que:
    0   = Caractère "espace",
    1-8 = Caractère "flèche",
    9   = Caractère "croix" pour DIRECTION_NODATA
*/
int ARROWS[10] = {0x20, 0x2196, 0x2191, 0x2197, 0x2192, 0x2198, 0x2193, 0x2199, 0x2190, 0x2717};


//////////////////////////////////////////////// METHODS ////////////////////////////////////////////////
/*
    Description:
    Lit le fichier MNT qui est donné.
*/
void readFileData(const char* filepath, int* outSizeX, int* outSizeY, float* outNoData, float** inoutData)
{
    LOGGED(std::cout << "Opening file " << filepath << std::endl)
    
    FILE *fp = fopen(filepath, "r");
    if (fp == NULL)
    {
        std::cerr << "File could not be opened !" << std::endl;
        exit(0);
    }
    int _;
    fscanf(fp, "%d", outSizeX);
    fscanf(fp, "%d", outSizeY);
    fscanf(fp, "%d", &_);
    fscanf(fp, "%d", &_);
    fscanf(fp, "%d", &_);
    fscanf(fp, "%f", outNoData);

    LOGGED(std::cout << "SIZE_X:" << *outSizeX << " SIZE_Y:" << *outSizeY << " NODATA:" << *outNoData << std::endl)

    int width = (*outSizeX);
    int height = (*outSizeY);

    *inoutData = new float[(width+2) * (height+2)];
    LOGGED(std::cout << "Malloced " << (width*height) << " bytes to inoutData " << inoutData << std::endl)

    float cDepth;

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            fscanf(fp, "%f ", &cDepth);
            (*inoutData)[(j+1) + ((i+1)*(width+2))] = cDepth;
        }
    }
    
    fclose(fp);

    for (int i = 0; i < std::max(width+2, height+2); i++){
        (*inoutData)[(i < width+2 ? i : width+2)] = *outNoData;
        (*inoutData)[(i < width+2 ? i : width+2) + (height+1) * (width+2)] = *outNoData;
        (*inoutData)[(i < height+2 ? i : height+2) * (width+2)] = *outNoData;
        (*inoutData)[(i < height+2 ? i : height+2) * (width+2) + (width+1)] = *outNoData;
    }

    LOGGED(std::cout << "Filled data from file, creating shadows" << std::endl)
    for (int i = 0; i < std::max(width+2, height+2); i++){
        
    }
    LOGGED(std::cout << "File read end." << std::endl)
}

/*
    Description:
    Affiche une matrice donnée formatée.
*/
void displayData(float* inData, int inDataSize, int inColumnNumber){
    LOGGED(std::cout << "Displaying " << inDataSize << " values from Data " << inData << std::endl)
    for(int i = 0; i < inDataSize; i++){
        std::cout << inData[i] << " ";
        if((i+1) % inColumnNumber == 0) std::cout << std::endl;
    }
}
/*
    Description:
    Affiche une matrice donnée formatée.
*/
void displayData(char* inData, int inDataSize, int inColumnNumber){
    LOGGED(std::cout << "Displaying " << inDataSize << " values from Data " << inData << std::endl)
    for(int i = 0; i < inDataSize; i++){
        printf("%d ", inData[i]);
        if((i+1) % inColumnNumber == 0) std::cout << std::endl;
    }
}
/*
    Description:
    Affiche une matrice donnée formatée.
*/
void displayData(int* inData, int inDataSize, int inColumnNumber){
    LOGGED(std::cout << "Displaying " << inDataSize << " values from Data " << inData << std::endl)
    for(int i = 0; i < inDataSize; i++){
        printf("%d ", inData[i]);
        if((i+1) % inColumnNumber == 0) std::cout << std::endl;
    }
}
/*
    Description:
    Affiche une matrice donnée formatée.
*/
void displayData(int* inData, int inDataSize, int inColumnNumber, int offsetX, int offsetY){
    LOGGED(std::cout << "Displaying " << inDataSize << " values from Data " << inData << std::endl)
    for(int i = 0; i < inDataSize; i++){
        int x = (i % inColumnNumber)+offsetX;
        int y = (i / inColumnNumber)+offsetY;
        printf("%d ", inData[x+y*(inColumnNumber+(2*offsetX))]);
        if((i+1) % inColumnNumber == 0) std::cout << std::endl;
    }
}
/*
    Description:
    Affiche une matrice donnée avec correspondance des valeurs en flèches grace a la table ARROWS.
*/
void displayDirectionsWithArrows(char* inData, int inDataSize, int inColumnNumber, int offsetX, int offsetY, int nodataValue){
    setlocale(LC_ALL, "");
    for(int i = 0; i < inDataSize; i++){
        int index = offsetX + (offsetY*inColumnNumber) + i;
        if(inData[index] == nodataValue) printf("%lc ", ARROWS[9]);
        else if(inData[index] < 0 || inData[index] > 8) printf("E ");
        else printf("%lc ", ARROWS[inData[index]]);
        if((i+1) % inColumnNumber == 0) std::cout << std::endl;
    }
}
/*
    Description:
    Permet de calculer l'espace de temps (en secondes) entre deux chronos donnés.
*/
double temps(std::chrono::time_point<std::chrono::system_clock> debut, std::chrono::time_point<std::chrono::system_clock> fin){
  std::chrono::duration<double> tps=fin-debut;
  return tps.count();
}



#endif