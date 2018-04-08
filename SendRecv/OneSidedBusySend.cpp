/*
Initiate two MPI processes
One process busy sends.
The other process busy polls.
*/

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
#define BUSY_SEND_TAG 1
#define MODE "ASYNC"
#define BATCH_SIZE 5

using namespace std;

/* ----------- SYNCHRONOUS ---------- */
void busy_send_sync_routine(int recepient){
  double sent;
  for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
    sent = MPI_Wtime();
    MPI_Send(&sent, 1, MPI_DOUBLE, recepient, BUSY_SEND_TAG, MPI_COMM_WORLD);
  }
}

void busy_recv_sync_routine(int sender){
  double recv;
  double sent[NUM_ACTUAL_MESSAGES];
  vector<double> timestamps;
  double start_timestamp  = MPI_Wtime();

  for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i += BATCH_SIZE){
    //Do the batch here
    for (int k = 0; k < BATCH_SIZE; k++){
      MPI_Recv(&sent[i + k], 1, MPI_DOUBLE, sender, BUSY_SEND_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    //Put down the timestamp
    if (i != 0){
      recv = MPI_Wtime();
      timestamps.push_back(recv - sent[i-BATCH_SIZE+1]);
    }
  }

  //Calculate the stats
  double end_timestamp = MPI_Wtime();
  Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH(timestamps, start_timestamp, end_timestamp, sizeof(double), BATCH_SIZE);
  printf("---------------------------\n");
  printf("Sync Latency Stats are: \n");
  latency_stats_in_microsecond.print();
  latency_stats_in_microsecond.write_to_csv_file("Output/OneSidedBusySend_Sync.txt");
}

int main(int argc, char** argv) {

  // Initialize the MPI environment
  MPI_Init(NULL, NULL);
  // Find out rank, size
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // We are assuming at least 2 processes for this task
  if (world_size < 2) {
    fprintf(stderr, "World size must be greater than 1 for %s\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  if (world_rank == 0){
    MPI_SEND_WARMUP(NUM_WARMUP_MESSAGES, 1);
  }else if (world_rank == 1){
    MPI_RECV_WARMUP(NUM_WARMUP_MESSAGES, 0);
  }
  MPI_Barrier(MPI_COMM_WORLD);

  if (world_rank == 0){
    busy_send_sync_routine(1);
  }else if (world_rank == 1){
    busy_recv_sync_routine(0);
  }
  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
}
