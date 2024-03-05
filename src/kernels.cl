//  ·····
//  ····· Made by: Thomas-SBE
//  ····· 
// ◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤

__kernel
void directions(__global int* bDATA, __global char* bDIRECTIONS, const int NODATA, const char DIRECTION_NODATA)
{
    int x = get_global_id(0) + 1;
    int y = get_global_id(1) + 1;
    int width = get_global_size(0) + 2;
    int cells[9];

    if(bDATA[x+y*width] == NODATA)
    {
        bDIRECTIONS[x+y*width] = DIRECTION_NODATA;
    }
    else
    {
        for(int e = 0; e < 9; e++){
            int lx = (e%3) - 1;
            int ly = (e/3) - 1;
            cells[e] = bDATA[(x+lx)+(y+ly)*width];
        }

        int minval = INT_MAX;
        char mindir;

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
        bDIRECTIONS[x+y*width] = mindir;

    }
}

__kernel
void accumulations(__global char* bDIRECTIONS, __global int* bACCUMULATIONS, __global char* bCHANGES, const int NODATA, const char DIRECTION_NODATA, __global int* sumOfChanges)
{
    int x = get_global_id(0) + 1;
    int y = get_global_id(1) + 1;
    int width = get_global_size(0) + 2;

    char local_changes[9];
    char local_dirs[9];
    char local_acc[9];
    int sum = 0;

    if(bDIRECTIONS[x+y*width] != DIRECTION_NODATA)
    {
        for(int e = 0; e < 9; e++){
            int lx = (e%3) - 1;
            int ly = (e/3) - 1;
            local_acc[e] = bACCUMULATIONS[x+lx + ((y+ly)*width)];
            local_changes[e] = bCHANGES[x+lx + ((y+ly)*width)];
            local_dirs[e] = bDIRECTIONS[x+lx + ((y+ly)*width)];
        }
    }
    
    barrier(CLK_GLOBAL_MEM_FENCE);

    if(bDIRECTIONS[x+y*width] == DIRECTION_NODATA)
    {
        bACCUMULATIONS[x+y*width] = NODATA;
        bCHANGES[x+y*width] = 0;
    }
    else
    {
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
            bACCUMULATIONS[x + (y * width)] = 1 + neighbour_acc;
            bCHANGES[x + (y * width)] = 1;
        }else{
            bCHANGES[x + (y * width)] = 0;
        }

    }

    if(bCHANGES[x + (y * width)] == 1){
        atomic_add(sumOfChanges, 1);
    }
    
}