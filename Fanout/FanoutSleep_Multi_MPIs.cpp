/*
Initiate Multiple MPI processes
One process scatter the message
Other processes receive the message and put down the timestamp
*/

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>

#include "../Lib/HelperFunctions.h"
#define NUM_ACTUAL_MESSAGES 10000
#define NUM_WARMUP_MESSAGES 1000
#define BUSY_SEND_RECV_TAG 1
#define BATCH_SIZE 5

int SLEEP_BASE = 100;
int SLEEP_FLUCTUATION = 1;

using namespace std;

//Sync
void busy_scatter_sync_routine(int sender_address, int my_address, bool isVerbose, int world_size){
  double sendBuffer[NUM_ACTUAL_MESSAGES * world_size];
  double recvBuffer[NUM_ACTUAL_MESSAGES];
  vector<double> timestamps;
  double start_timestamp, end_timestamp;

  double dummy_send = 0;
  double dummy_recv = 0;

  //If I am the one who scatters the message
  if (sender_address == my_address){
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i += BATCH_SIZE){

      //Do the batch here
      for (int k = 0; k < BATCH_SIZE; k++){
        //Initiate the buffer
        double current_time = MPI_Wtime();
        for (int j = 0; j < world_size; j++){
          sendBuffer[i * world_size + j] = current_time;
        }
        //Scatter the buffer
        MPI_Scatter(&sendBuffer[i * world_size], 1, MPI_DOUBLE, recvBuffer, 1, MPI_DOUBLE, sender_address, MPI_COMM_WORLD);
      }
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }
  //If I am the one who receives the message
  }else{
    int counter = 0;
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i+=BATCH_SIZE){
      //Do the batch here
      for (int k = 0; k < BATCH_SIZE; k++){
        //Receive the scattered message
        MPI_Scatter(sendBuffer, 1, MPI_DOUBLE, &recvBuffer[counter], 1, MPI_DOUBLE, sender_address, MPI_COMM_WORLD);
        counter++;
      }
      double recv = MPI_Wtime();
      timestamps.push_back(recv - recvBuffer[counter - BATCH_SIZE + 1]);
      if (i == BATCH_SIZE){
        start_timestamp = MPI_Wtime();
      }
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }
    end_timestamp = MPI_Wtime();
  }

  if (isVerbose){
    double end_timestamp = MPI_Wtime();
    //Need to take care of two way communication
    //So we multiply the each message size by 2
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH(timestamps, start_timestamp, end_timestamp, sizeof(double), BATCH_SIZE);
    printf("-----------------------------------------\n");
    printf("Fanout Sleep Sync Latency is: \n");
    latency_stats_in_microsecond.print();
    latency_stats_in_microsecond.write_to_csv_file("Output/FanoutSleep_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");

  }
}

//Async Iscatter & Iscatter
void busy_scatter_async_routine(int sender_address, int my_address, bool isVerbose, int world_size){
  double sendBuffer[NUM_ACTUAL_MESSAGES * world_size];
  double recvBuffer[NUM_ACTUAL_MESSAGES];
  vector<double> timestamps;
  double start_timestamp, end_timestamp;

  MPI_Request sendRequests[BATCH_SIZE];
  MPI_Request recvRequests[BATCH_SIZE];

  //If I am the one who scatters the mssage
  if (sender_address == my_address){

    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i += BATCH_SIZE){
      //Do the batch here
      for (int k = 0; k < BATCH_SIZE; k++){
        double current_time = MPI_Wtime();
        //Initiate the buffer
        for (int j = 0; j < world_size; j++){
          sendBuffer[i * world_size + j] = current_time;
        }
        //Scatter the buffer
        MPI_Iscatter(&sendBuffer[i * world_size], 1, MPI_DOUBLE, recvBuffer, 1, MPI_DOUBLE, sender_address, MPI_COMM_WORLD, &sendRequests[k]);
      }
      //Sleep
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
      MPI_Waitall(BATCH_SIZE, sendRequests, MPI_STATUSES_IGNORE);
    }


  //If I am the one who receives the message
  }else{
    int counter = 0;
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i+=BATCH_SIZE){
      //Do the batch here
      for (int k = 0; k < BATCH_SIZE; k++){
        //Receive the scattered message
        MPI_Iscatter(sendBuffer, 1, MPI_DOUBLE, &recvBuffer[counter], 1, MPI_DOUBLE, sender_address, MPI_COMM_WORLD, &recvRequests[k]);
        counter++;
      }
      MPI_Waitall(BATCH_SIZE, recvRequests, MPI_STATUSES_IGNORE);
      double recv = MPI_Wtime();
      timestamps.push_back(recv - recvBuffer[counter - BATCH_SIZE + 1]);
      if (i == BATCH_SIZE){
        start_timestamp = MPI_Wtime();
      }
      //Sleep
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }
    end_timestamp = MPI_Wtime();
  }

  if (isVerbose){
    double end_timestamp = MPI_Wtime();
    //Need to take care of two way communication
    //So we multiply the each message size by 2
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH(timestamps, start_timestamp, end_timestamp, sizeof(double), BATCH_SIZE);
    printf("-----------------------------------------\n");
    printf("Fanout Asnyc Latency V2 is: \n");
    latency_stats_in_microsecond.print();
    latency_stats_in_microsecond.write_to_csv_file("Output/FanoutBusy_Multi_MPIs_Async_" + to_string(world_size) + ".txt");
  }
}


int main(int argc, char** argv) {
  //Porcess Arguments
  int opt;
  while ((opt = getopt(argc,argv,":B:F:d")) != EOF){
      switch(opt)
      {
          case 'B':
            SLEEP_BASE = stoi(optarg);
            break;
          case 'F':
            SLEEP_FLUCTUATION = stoi(optarg);
            break;
          case '?':
            fprintf(stderr, "USAGE:\n -B <BASE> -F <FLUCT> To sleep for BASE + FLUCT miscroseconds \n");
            break;
          default:
            break;
      }
  }

  // Initialize the MPI environment
  MPI_Init(NULL, NULL);
  // Find out rank, size
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // We are assuming at least 2 and even amount of processes for this task
  if (world_size < 2 ) {
    fprintf(stderr, "World size must be greater than 1 for %s\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  int verboser = GENERATE_A_RANDOM_NUMBER(1, world_size - 1);
  if (world_rank == verboser){
    printf("Verboser is %d \n", verboser);
  }

  //Warmup
  if (world_rank == 0){
    for (int i = 1; i < world_size; i++){
      MPI_SEND_WARMUP(NUM_WARMUP_MESSAGES, i);
    }
  }else{
    MPI_RECV_WARMUP(NUM_WARMUP_MESSAGES, 0);
  }
  MPI_Barrier(MPI_COMM_WORLD);

  //Sync
  busy_scatter_sync_routine(0, world_rank, verboser == world_rank, world_size);
  MPI_Barrier(MPI_COMM_WORLD);

  //Async
  busy_scatter_async_routine(0, world_rank, verboser == world_rank, world_size);
  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
}
