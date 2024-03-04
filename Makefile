#  ·····
#  ····· Made by: Thomas-SBE
#  ····· 
# ◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤◢◤

CC = g++
MPCC = mpicxx.mpich
CFLAGS = -O3 -lOpenCL
MPCFLAGS = 
BUILD_DIR = build
OPENCL_KERNELS = $(wildcard src/*.cl)

all: mpi ocl

mpi: $(BUILD_DIR)
	${MPCC} src/mpi_mnt.cpp src/lib.hpp -o build/mpimnt
	${MPCC} src/followers/follower.cpp src/lib.hpp -o build/follower

ocl: $(BUILD_DIR) $(OPENCL_KERNELS)
	${CC} ${CFLAGS} src/ocl_mnt.cpp src/lib.hpp -o build/oclmnt

%.cl: $(BUILD_DIR)
	cp $@ $(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)


clean:
	rm -Rf $(BUILD_DIR)