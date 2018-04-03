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
  double start_timestamp;
  for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
    if (i == 1){
      start_timestamp = MPI_Wtime();
    }
    MPI_Recv(&sent[i], 1, MPI_DOUBLE, sender, BUSY_SEND_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (i % BATCH_SIZE == 0 && i != 0){
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

/* ----------- ASYNCHRONOUS V1---------- */
void busy_send_async_routine_v1(int recepient){
  double sent[NUM_ACTUAL_MESSAGES];
  MPI_Request requests[NUM_ACTUAL_MESSAGES];
  for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
    sent[i] = MPI_Wtime();
    MPI_Isend(&sent[i], 1, MPI_DOUBLE, recepient, BUSY_SEND_TAG, MPI_COMM_WORLD, &requests[i]);
  }
  MPI_Waitall(NUM_ACTUAL_MESSAGES, requests, MPI_STATUSES_IGNORE);
}

void busy_recv_async_routine_v1(int sender){
  double sent[NUM_ACTUAL_MESSAGES];
  double recv;
  MPI_Request requests[NUM_ACTUAL_MESSAGES];
  vector<double> timestamps;
  double start_timestamp = MPI_Wtime();
  for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
    MPI_Irecv(&sent[i], 1, MPI_DOUBLE, sender, BUSY_SEND_TAG, MPI_COMM_WORLD, &requests[i]);
  }
  MPI_Waitall(NUM_ACTUAL_MESSAGES, requests, MPI_STATUSES_IGNORE);

  //Calcuate the stats
  double end_timestamp = MPI_Wtime();
  recv = MPI_Wtime();
  double average_time = (recv - sent[0]) / NUM_ACTUAL_MESSAGES * 1000000;
  double throughput = CALCULATE_THROUGHPUT_BATCH(sent[0], end_timestamp, NUM_ACTUAL_MESSAGES, sizeof(double), 1);
  printf("---------------------------\n");
  printf("Async V1 Average Latency is: %f ms \n", average_time);
  printf("Async Throughput is %f MB/s \n", throughput);
}

/* ----------- ASYNCHRONOUS V2---------- */
void busy_send_async_routine_v2(int recepient){
  double sent[NUM_ACTUAL_MESSAGES];
  MPI_Request requests[NUM_ACTUAL_MESSAGES];
  for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
    sent[i] = MPI_Wtime();
    MPI_Isend(&sent[i], 1, MPI_DOUBLE, recepient, BUSY_SEND_TAG, MPI_COMM_WORLD, &requests[i]);
  }
  MPI_Waitall(NUM_ACTUAL_MESSAGES, requests, MPI_STATUSES_IGNORE);
}

void busy_recv_async_routine_v2(int sender){
  double sent[NUM_ACTUAL_MESSAGES];
  double recv;
  vector<double> timestamps;
  MPI_Request request;
  double start_timestamp = MPI_Wtime();
  for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
    MPI_Recv(&sent[i], 1, MPI_DOUBLE, sender, BUSY_SEND_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (i % BATCH_SIZE == 0 && i != 0){
      recv = MPI_Wtime();
      timestamps.push_back(recv - sent[i-BATCH_SIZE+1]);
    }
  }

  //Calcuate the Stats
  double end_timestamp = MPI_Wtime();
  Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH(timestamps, start_timestamp, end_timestamp, sizeof(double), BATCH_SIZE);
  printf("-----------------------------\n");
  printf("Async V2 Latency Stats are: \n");
  latency_stats_in_microsecond.print();
  latency_stats_in_microsecond.write_to_csv_file("Output/OneSidedBusySend_Async_V2.txt");
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

  if (world_rank == 0){
    busy_send_async_routine_v1(1);
  }else if (world_rank == 1){
    busy_recv_async_routine_v1(0);
  }
  MPI_Barrier(MPI_COMM_WORLD);

  if (world_rank == 0){
    busy_send_async_routine_v2(1);
  }else if (world_rank == 1){
    busy_recv_async_routine_v2(0);
  }
  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
}
