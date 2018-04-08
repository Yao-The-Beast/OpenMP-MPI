/*
Initiate Multiple MPI processes
Each process busy sends and busy polls.
Process id 2i talks to Process id 2i+1
No synchronization is enforced.
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
#define BUSY_SEND_RECV_TAG 1
#define BATCH_SIZE 5

using namespace std;

/* ----------- SYNCHRONOUS ---------- */
void busy_send_recv_sync_routine(int hisAddress, int myAddress, bool isVerbose, int world_size){
    //the timestamp of the msg I sent
    double mySent[NUM_ACTUAL_MESSAGES];
    //the timestamp of the msg he sent to me
    double hisSent[NUM_ACTUAL_MESSAGES];

    vector<double> timestamps;
    double recv;

    double start_timestamp = MPI_Wtime();
    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i+=BATCH_SIZE){
      //Make sure one process sends and the other receives. Otherwise there will be a block
      if (myAddress % 2 == 1){
        //Do the batch here
        for (int k = 0; k < BATCH_SIZE; k++){
          mySent[i + k] = MPI_Wtime();
          MPI_Send(&mySent[i + k], 1, MPI_DOUBLE, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD);
          MPI_Recv(&hisSent[i + k], 1, MPI_DOUBLE, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
      }else{
        //Do the batch here
        for (int k = 0; k < BATCH_SIZE; k++){
          MPI_Recv(&hisSent[i + k], 1, MPI_DOUBLE, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
          mySent[i + k] = MPI_Wtime();
          MPI_Send(&mySent[i + k], 1, MPI_DOUBLE, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD);
        }
      }
      //Put down the timestamp here
      if (isVerbose && i != 0){
        recv = MPI_Wtime();
        timestamps.push_back((recv - hisSent[i - BATCH_SIZE + 1]) / 2.0);
      }
    }

    if (isVerbose){
      double end_timestamp = MPI_Wtime();
      //Need to take care of two way communication
      //So we multiply the each message size by 2
      Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH(timestamps, start_timestamp, end_timestamp, 2 * sizeof(double), BATCH_SIZE);
      printf("-----------------------------------------\n");
      printf("Sync Send & Recv Latency Stats are: \n");
      latency_stats_in_microsecond.print();
      latency_stats_in_microsecond.write_to_csv_file("Output/twoSidedBusySend_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");
    }
}

/* ----------- ASYNCHRONOUS ---------- */
void busy_send_recv_async_routine(int hisAddress, int myAddress, bool isVerbose, int world_size){
    //the timestamp of the msg I sent
    double mySent[NUM_ACTUAL_MESSAGES];
    //the timestamp of the msg he sent to me
    double hisSent[NUM_ACTUAL_MESSAGES];

    //Requests
    MPI_Request sendRequests[BATCH_SIZE];
    MPI_Request recvRequests[BATCH_SIZE];

    vector<double> timestamps;
    double recv;

    double start_timestamp = MPI_Wtime();
    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i+=BATCH_SIZE){
      //Make sure one process sends and the other receives. Otherwise there will be a block
      if (myAddress % 2 == 1){
        //Do the batch here
        for (int k = 0; k < BATCH_SIZE; k++){
          mySent[i + k] = MPI_Wtime();
          MPI_Isend(&mySent[i + k], 1, MPI_DOUBLE, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, &sendRequests[k]);
          MPI_Irecv(&hisSent[i + k], 1, MPI_DOUBLE, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, &recvRequests[k]);
        }
      }else{
        //Do the batch here
        for (int k = 0; k < BATCH_SIZE; k++){
          MPI_Irecv(&hisSent[i + k], 1, MPI_DOUBLE, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, &recvRequests[k]);
          mySent[i + k] = MPI_Wtime();
          MPI_Isend(&mySent[i + k], 1, MPI_DOUBLE, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, &sendRequests[k]);
        }
      }
      //Wait for the non-blocking calls to complete
      MPI_Waitall(BATCH_SIZE, sendRequests, MPI_STATUSES_IGNORE);
      MPI_Waitall(BATCH_SIZE, recvRequests, MPI_STATUSES_IGNORE);
      //Put down the timestamp here
      if (isVerbose && i != 0){
        recv = MPI_Wtime();
        timestamps.push_back((recv - hisSent[i - BATCH_SIZE + 1]) / 2.0);
      }
    }

    if (isVerbose){
      double end_timestamp = MPI_Wtime();
      //Need to take care of two way communication
      //So we multiply the each message size by 2
      Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH(timestamps, start_timestamp, end_timestamp, 2 * sizeof(double), BATCH_SIZE);
      printf("-----------------------------------------\n");
      printf("Async Send & Recv Latency Stats are: \n");
      latency_stats_in_microsecond.print();
      latency_stats_in_microsecond.write_to_csv_file("Output/twoSidedBusySend_Multi_MPIs_Async_" + to_string(world_size) + ".txt");
    }
}


int main(int argc, char** argv) {

  // Initialize the MPI environment
  MPI_Init(NULL, NULL);
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

  //Choose an odd id verboser
  int verboser = GENERATE_A_RANDOM_NUMBER(0, world_size - 1);
  if (world_rank == verboser){
    printf("Verboser is %d \n", verboser);
  }

  if (world_rank % 2 == 0){
    MPI_SEND_WARMUP(NUM_WARMUP_MESSAGES, world_rank + 1);
    MPI_RECV_WARMUP(NUM_WARMUP_MESSAGES, world_rank + 1);
  }else{
    MPI_RECV_WARMUP(NUM_WARMUP_MESSAGES, world_rank - 1);
    MPI_SEND_WARMUP(NUM_WARMUP_MESSAGES, world_rank - 1);
  }


  //Benchmark Sync
  MPI_Barrier(MPI_COMM_WORLD);
  if (world_rank % 2 == 0){
    busy_send_recv_sync_routine(world_rank + 1, world_rank, verboser == world_rank, world_size);
  }else{
    busy_send_recv_sync_routine(world_rank - 1, world_rank, verboser == world_rank, world_size);
  }

  //Benchmark Async
  MPI_Barrier(MPI_COMM_WORLD);
  if (world_rank % 2 == 0){
    busy_send_recv_async_routine(world_rank + 1, world_rank, verboser == world_rank, world_size);
  }else{
    busy_send_recv_async_routine(world_rank - 1, world_rank, verboser == world_rank, world_size);
  }

  MPI_Finalize();
}
