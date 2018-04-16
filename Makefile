#
# Edison - NERSC
#
# Intel Compilers are loaded by default; for other compilers please check the module list
#
CC = CC
MPCC = CC
OPENMP = -openmp #Note: this is the flag for Intel compilers. Change this to -fopenmp for GNU compilers. See http://www.nersc.gov/users/computational-systems/edison/programming/using-openmp/
CFLAGS =  -std=c++11 -O3
LIBS =

TARGETS = P2P_Send_Multi_MPIs P2P_Send_Multi_OpenMP \
					Scatter_Multi_MPIs Scatter_Multi_OpenMP \
					Broadcast_Multi_MPIs Broadcast_Multi_OpenMP


all:	$(TARGETS)

# Create Binary Files

P2P_Send_Multi_MPIs: P2P_Send_Multi_MPIs.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) P2P_Send_Multi_MPIs.o

P2P_Send_Multi_OpenMP: P2P_Send_Multi_OpenMP.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) P2P_Send_Multi_OpenMP.o

Scatter_Multi_MPIs: Scatter_Multi_MPIs.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) Scatter_Multi_MPIs.o

Scatter_Multi_OpenMP: Scatter_Multi_OpenMP.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) Scatter_Multi_OpenMP.o

Broadcast_Multi_MPIs: Broadcast_Multi_MPIs.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) Broadcast_Multi_MPIs.o

Broadcast_Multi_OpenMP: Broadcast_Multi_OpenMP.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) Broadcast_Multi_OpenMP.o


# Create Object Files

P2P_Send_Multi_MPIs.o: P2P/P2P_Send_Multi_MPIs.cpp Lib/Lib.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) P2P/P2P_Send_Multi_MPIs.cpp Lib/Lib.h

P2P_Send_Multi_OpenMP.o: P2P/P2P_Send_Multi_OpenMP.cpp Lib/Lib.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) P2P/P2P_Send_Multi_OpenMP.cpp Lib/Lib.h

Scatter_Multi_MPIs.o: Scatter/Scatter_Multi_MPIs.cpp Lib/Lib.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) Scatter/Scatter_Multi_MPIs.cpp Lib/Lib.h

Scatter_Multi_OpenMP.o: Scatter/Scatter_Multi_OpenMP.cpp Lib/Lib.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) Scatter/Scatter_Multi_OpenMP.cpp Lib/Lib.h

Broadcast_Multi_MPIs.o: Broadcast/Broadcast_Multi_MPIs.cpp Lib/Lib.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) Broadcast/Broadcast_Multi_MPIs.cpp Lib/Lib.h

Broadcast_Multi_OpenMP.o: Broadcast/Broadcast_Multi_OpenMP.cpp Lib/Lib.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) Broadcast/Broadcast_Multi_OpenMP.cpp Lib/Lib.h

clean:
	rm -f *.o $(TARGETS) *.stdout *.error *.txt
