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

TARGETS = oneSidedBusySend twoSidedBusySend_Multi_MPIs twoSidedBusySend_Multi_OpenMP

all:	$(TARGETS)

# Create Binary Files

oneSidedBusySend: OneSidedBusySend.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) OneSidedBusySend.o

twoSidedBusySend_Multi_MPIs: TwoSidedBusySend_Multi_MPIs.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) TwoSidedBusySend_Multi_MPIs.o

twoSidedBusySend_Multi_OpenMP: TwoSidedBusySend_Multi_OpenMP.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) TwoSidedBusySend_Multi_OpenMP.o


# Create Object Files

OneSidedBusySend.o: SendRecv/OneSidedBusySend.cpp Lib/HelperFunctions.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) SendRecv/OneSidedBusySend.cpp Lib/HelperFunctions.h

TwoSidedBusySend_Multi_MPIs.o: SendRecv/TwoSidedBusySend_Multi_MPIs.cpp Lib/HelperFunctions.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) SendRecv/TwoSidedBusySend_Multi_MPIs.cpp Lib/HelperFunctions.h

TwoSidedBusySend_Multi_OpenMP.o: SendRecv/TwoSidedBusySend_Multi_OpenMP.cpp Lib/HelperFunctions.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) SendRecv/TwoSidedBusySend_Multi_OpenMP.cpp Lib/HelperFunctions.h


clean:
	rm -f *.o $(TARGETS) *.stdout *.error *.txt
