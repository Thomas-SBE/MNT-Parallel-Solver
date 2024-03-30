#  ·····
#  ····· Made by: Thomas-SBE
#  ····· 
# ◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤

CC = g++
MPCC = mpicxx.mpich
CFLAGS = -O3 -lOpenCL -w
MPCFLAGS = 
BUILD_DIR = build

all: mpi ocl seq

mpi: $(BUILD_DIR)
	${MPCC} src/mpi_mnt.cpp src/lib.hpp -o build/mpimnt
	${MPCC} src/followers/follower.cpp src/lib.hpp -o build/follower

ocl: $(BUILD_DIR)
	${CC} src/ocl_mnt.cpp src/lib.hpp ${CFLAGS} -o build/oclmnt
	cp src/*.cl build/

seq: $(BUILD_DIR)
	${CC} src/seq_mnt.cpp src/lib.hpp ${CFLAGS} -o build/seqmnt
	cp src/*.cl build/

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


clean:
	rm -Rf $(BUILD_DIR)