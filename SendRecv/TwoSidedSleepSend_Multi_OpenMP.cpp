

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>
#include "../Lib/HelperFunctions.h"

#define NUM_ACTUAL_MESSAGES 50000
#define NUM_WARMUP_MESSAGES 50000
#define BATCH_SIZE 5
#define BUSY_SEND_RECV_TAG 2

using namespace std;

const int THREAD_LEVEL = MPI_THREAD_MULTIPLE;
int NUM_THREADS = 1;
int SLEEP_BASE = 100;
int SLEEP_FLUCTUATION = 25;

/* ----------- SYNCHRONOUS ---------- */
void busy_send_recv_sync_routine(int hisAddress, bool isVerbose, int world_size){
    //the timestamp of the msg I sent
    double mySent[NUM_ACTUAL_MESSAGES];
    //the timestamp of the msg he sent to me
    double hisSent[NUM_ACTUAL_MESSAGES];

    vector<double> latencies;
    double recv;
    double start_timestamp, end_timestamp;
    int tag = BUSY_SEND_RECV_TAG * 100 + omp_get_thread_num();

    start_timestamp = MPI_Wtime();

    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i += BATCH_SIZE){
      //Do the batch here
      for (int k = 0; k < BATCH_SIZE; k++){
        //Enforce send & recv sequence to make sure there is no deadlock
        if (hisAddress % 2 == 0){
          mySent[i + k] = MPI_Wtime();
          MPI_Send(&mySent[i + k], 1, MPI_DOUBLE, hisAddress, tag, MPI_COMM_WORLD);
          MPI_Recv(&hisSent[i + k], 1, MPI_DOUBLE, hisAddress, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }else{
          MPI_Recv(&hisSent[i + k], 1, MPI_DOUBLE, hisAddress, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          mySent[i + k] = MPI_Wtime();
          MPI_Send(&mySent[i + k], 1, MPI_DOUBLE, hisAddress, tag, MPI_COMM_WORLD);
        }
      }

      //Calculate the latency
      if (isVerbose && i != 0){
        recv = MPI_Wtime();
        latencies.push_back((recv - hisSent[i - BATCH_SIZE + 1]) / 2.0);
      }

      //usleep to simulate work here
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }

    if (isVerbose){
      end_timestamp = MPI_Wtime();
      //Need to take care of two way communication
      //So we multiply the each message size by 2
      Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
        latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0, 2 * sizeof(double), BATCH_SIZE, true);
      printf("-----------------------------------------\n");
      printf("Sync Send & Recv Latency Stats are: \n");
      latency_stats_in_microsecond.print();
      //latency_stats_in_microsecond.write_to_csv_file("Output/twoSidedSleepSend_Multi_OpenMP_Sync_" + to_string(world_size * NUM_THREADS) + ".txt");
    }
}

/* ----------- ASYNCHRONOUS ---------- */
void busy_send_recv_async_routine(int hisAddress, bool isVerbose, int world_size){
    //timestamp
    double mySent[NUM_ACTUAL_MESSAGES];
    double hisSent[NUM_ACTUAL_MESSAGES];

    MPI_Request sendRequests[BATCH_SIZE];
    MPI_Request recvRequests[BATCH_SIZE];

    vector<double> latencies;
    double recv;
    double start_timestamp, end_timestamp;
    int tag = BUSY_SEND_RECV_TAG * 100 + omp_get_thread_num();

    start_timestamp = MPI_Wtime();

    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i += BATCH_SIZE){
      //Do the batch here
      for (int k = 0; k < BATCH_SIZE; k++){
        mySent[i + k] = MPI_Wtime();
        MPI_Isend(&mySent[i + k], 1, MPI_DOUBLE, hisAddress, tag, MPI_COMM_WORLD, &sendRequests[k]);
        MPI_Irecv(&hisSent[i + k], 1, MPI_DOUBLE, hisAddress, tag, MPI_COMM_WORLD, &recvRequests[k]);
      }
      MPI_Waitall(BATCH_SIZE, sendRequests, MPI_STATUSES_IGNORE);
      MPI_Waitall(BATCH_SIZE, recvRequests, MPI_STATUSES_IGNORE);

      //Calculate the time
      if (isVerbose && i != 0){
        recv = MPI_Wtime();
        latencies.push_back((recv - hisSent[i - BATCH_SIZE + 1]) / 2.0);
      }

      //usleep to simulate work here
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }

    if (isVerbose){
      end_timestamp = MPI_Wtime();
      //Need to take care of two way communication
      //So we multiply the each message size by 2
      Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
        latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0, 2 * sizeof(double), BATCH_SIZE, true);
      printf("-----------------------------------------\n");
      printf("Async Send & Recv Latency Stats are: \n");
      latency_stats_in_microsecond.print();
      //latency_stats_in_microsecond.write_to_csv_file("Output/twoSidedSleepSend_Multi_OpenMP_Async_" + to_string(world_size * NUM_THREADS) + ".txt");
    }
}



int main(int argc, char** argv) {

  //Porcess Arguments
  int opt;
  while ((opt = getopt(argc,argv,":B:F:T:d")) != EOF){
      switch(opt)
      {
          case 'B':
            SLEEP_BASE = stoi(optarg);
            break;
          case 'F':
            SLEEP_FLUCTUATION = stoi(optarg);
            break;
          case 'T':
            NUM_THREADS = stoi(optarg);
            break;
          case '?':
            fprintf(stderr, "USAGE:\n -B <BASE> -F <FLUCT> To sleep for BASE + FLUCT miscroseconds \n");
            break;
          default:
            abort();
      }
  }

  // Initialize the MPI environment
  int provided, claimed;
  MPI_Init_thread( 0, 0, THREAD_LEVEL, &provided);
  if (provided < THREAD_LEVEL) {
     cerr << "THREAD_LEVEL IS NOT SUPPORTED" << endl;
     return 0;
  }
  // Find out rank, size
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // We are assuming at least 2 and even amount of processes for this task
  if (world_size < 2 || world_size % 2 == 1) {
    fprintf(stderr, "World size must be even and greater than 1 for %s\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  int verboser_thread = GENERATE_A_RANDOM_NUMBER(0, NUM_THREADS - 1, 0);
  int verboser_rank = GENERATE_A_RANDOM_NUMBER(0,world_size - 1);

  omp_set_num_threads(NUM_THREADS);

  #pragma omp parallel
  {
    int tid = omp_get_thread_num();
    if (tid == verboser_thread && world_rank == verboser_rank){
      printf("----------------------------------\nArguments are: \n");
      printf("BASE %d FLUCTUATION %d THREADS %d \n", SLEEP_BASE, SLEEP_FLUCTUATION, NUM_THREADS);
      cout << "Verboser is " << verboser_rank << " " << verboser_thread << endl;
      cout << "Number of threads " <<  omp_get_num_threads() << endl;
    }

    //Warmup
    if (world_rank % 2 == 0){
      OPENMP_SEND_WARMUP(NUM_WARMUP_MESSAGES, world_rank + 1, THREAD_LEVEL);
      OPENMP_RECV_WARMUP(NUM_WARMUP_MESSAGES, world_rank + 1, THREAD_LEVEL);
    }else{
      OPENMP_RECV_WARMUP(NUM_WARMUP_MESSAGES, world_rank - 1, THREAD_LEVEL);
      OPENMP_SEND_WARMUP(NUM_WARMUP_MESSAGES, world_rank - 1, THREAD_LEVEL);
    }

    //Use Send & Recv
    if (world_rank % 2 == 0){
      busy_send_recv_sync_routine(world_rank + 1, verboser_rank == world_rank && verboser_thread == tid, world_size);
    }else{
      busy_send_recv_sync_routine(world_rank - 1, verboser_rank == world_rank && verboser_thread == tid, world_size);
    }

    //Use Isend & Recv
    if (world_rank % 2 == 0){
      busy_send_recv_async_routine(world_rank + 1, verboser_rank == world_rank && verboser_thread == tid, world_size);
    }else{
      busy_send_recv_async_routine(world_rank - 1, verboser_rank == world_rank && verboser_thread == tid, world_size);
    }
  }

  MPI_Finalize();
}
