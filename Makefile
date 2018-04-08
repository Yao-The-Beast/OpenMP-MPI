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

TARGETS =
		twoSidedSleepSend_Multi_MPIs twoSidedSleepSend_Multi_OpenMP \
		fanoutBusy_Multi_MPIs fanoutSleep_Multi_MPIs


all:	$(TARGETS)

# Create Binary Files

twoSidedSleepSend_Multi_MPIs: TwoSidedSleepSend_Multi_MPIs.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) TwoSidedSleepSend_Multi_MPIs.o

twoSidedSleepSend_Multi_OpenMP: TwoSidedSleepSend_Multi_OpenMP.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) TwoSidedSleepSend_Multi_OpenMP.o

fanoutBusy_Multi_MPIs: FanoutBusy_Multi_MPIs.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) FanoutBusy_Multi_MPIs.o

fanoutSleep_Multi_MPIs: FanoutSleep_Multi_MPIs.o
	$(MPCC) -o $@ $(LIBS) $(MPILIBS) $(OPENMP) FanoutSleep_Multi_MPIs.o


# Create Object Files

TwoSidedSleepSend_Multi_MPIs.o: SendRecv/TwoSidedSleepSend_Multi_MPIs.cpp Lib/HelperFunctions.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) SendRecv/TwoSidedSleepSend_Multi_MPIs.cpp Lib/HelperFunctions.h

TwoSidedSleepSend_Multi_OpenMP.o: SendRecv/TwoSidedSleepSend_Multi_OpenMP.cpp Lib/HelperFunctions.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) SendRecv/TwoSidedSleepSend_Multi_OpenMP.cpp Lib/HelperFunctions.h

FanoutBusy_Multi_MPIs.o: Fanout/FanoutBusy_Multi_MPIs.cpp Lib/HelperFunctions.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) Fanout/FanoutBusy_Multi_MPIs.cpp Lib/HelperFunctions.h

FanoutSleep_Multi_MPIs.o: Fanout/FanoutSleep_Multi_MPIs.cpp Lib/HelperFunctions.h
	$(MPCC) -c $(CFLAGS) $(OPENMP) Fanout/FanoutSleep_Multi_MPIs.cpp Lib/HelperFunctions.h

clean:
	rm -f *.o $(TARGETS) *.stdout *.error *.txt
