#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>

#include "../Lib/HelperFunctions.h"
#include "../Lib/HelperFunctions2.h"
using namespace std;

#define NUM_ACTUAL_MESSAGES 10000
#define NUM_WARMUP_MESSAGES 1000
#define BUSY_SEND_RECV_TAG 1
#define MASTER_RANK 0
#define MASTER_TID 0

const int THREAD_LEVEL = MPI_THREAD_MULTIPLE;
int NUM_THREADS = 2;
int SLEEP_BASE = 100;
int SLEEP_FLUCTUATION = 25;
int WORLD_SIZE = 1;

MPI_Datatype dt;
MailRoom mailRoom;



//Async using Isend
void scatter_async_regular_routine(int my_rank, int my_tid, bool isVerbose){

  vector<double> sendBuffer(NUM_DOUBLES * WORLD_SIZE * NUM_THREADS, 0);
  vector<double> recvBuffer(NUM_DOUBLES, 0);
  vector<double> latencies;

  MPI_Request sendRequests[WORLD_SIZE * NUM_THREADS];
  MPI_Request recvRequest;

  double start_timestamp = MPI_Wtime();
  //I am the Master who scatters the messages
  if (my_rank == MASTER_RANK && my_tid == MASTER_TID){
    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Prepare the data
      for (int j = 0; j < WORLD_SIZE * NUM_THREADS; j++){
        sendBuffer[j * NUM_DOUBLES + 10] = MPI_Wtime();
      }
      //Send to everyone
      int counter = 0;
      for (int w = 0; w < WORLD_SIZE; w++){
        for (int t = 0; t < NUM_THREADS; t++){
          MPI_Isend(&sendBuffer[counter * NUM_DOUBLES], 1, dt, w, t, MPI_COMM_WORLD, &sendRequests[counter++]);
        }
      }
      MPI_Irecv(&recvBuffer[0], 1, dt, MASTER_RANK, my_tid, MPI_COMM_WORLD, &recvRequest);
      //usleep to simulate work here
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
      //Wait for the requests
      MPI_Waitall(counter, sendRequests, MPI_STATUSES_IGNORE);
      MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);
    }
  //I receive messages from the master
  }else{
    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Receive the scattered message
      MPI_Irecv(&recvBuffer[0], 1, dt, MASTER_RANK, my_tid, MPI_COMM_WORLD, &recvRequest);
      //usleep to simulate work here
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
      //Wait for the requests
      MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);
      //Calculate the latency
      if (isVerbose){
        double recv = MPI_Wtime();
        latencies.push_back((recv - recvBuffer[10]));
      }
    }
  }

  if (isVerbose){
    double end_timestamp = MPI_Wtime();
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
      latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0, sizeof(double) * NUM_DOUBLES, 1, false);
    printf("-----------------------------------------\n");
    printf("Scatter Using Isend Latency is: \n");
    latency_stats_in_microsecond.print();
    //latency_stats_in_microsecond.write_to_csv_file("Output/FanoutSleep_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");
  }
}

//Async using shared data structure MailRoom
void scatter_async_self_invented_routine(int my_rank, int my_tid, bool isVerbose){
  const int num_messages_per_thread = 1;
  const int message_size = NUM_DOUBLES;

  vector<double> sendBuffer(num_messages_per_thread * message_size * WORLD_SIZE * NUM_THREADS, 0);
  vector<double> recvBuffer(num_messages_per_thread * message_size, 0);
  vector<double> latencies;
  int postman_tid = 1;
  if (WORLD_SIZE == 0)
    postman_tid = 0;

  double start_timestamp = MPI_Wtime();

  for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
    //I am the Master who scatters the messages
    if (my_rank == MASTER_RANK && my_tid == MASTER_TID){
      //Prepare the data
      for (int j = 0; j < WORLD_SIZE * NUM_THREADS; j++){
        sendBuffer[j * message_size * num_messages_per_thread + 10] = MPI_Wtime();
      }
    }
    //Call the scatter function, option set to send
    SELF_DEFINED_SCATTER(mailRoom,
      &sendBuffer[0], &recvBuffer[0],
      num_messages_per_thread, message_size, dt,
      my_rank, my_tid,
      MASTER_RANK, MASTER_TID,
      postman_tid,
      WORLD_SIZE, NUM_THREADS,
      -1
    );

    //Sleep
    USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);

    //Call the scatter function, option set to receive
    SELF_DEFINED_SCATTER(mailRoom,
      &sendBuffer[0], &recvBuffer[0],
      num_messages_per_thread, message_size, dt,
      my_rank, my_tid,
      MASTER_RANK, MASTER_TID,
      postman_tid,
      WORLD_SIZE, NUM_THREADS,
      1
    );

    //Calculate the latency
    if (isVerbose){
      double recv = MPI_Wtime();
      latencies.push_back((recv - recvBuffer[10]));
    }
  }

  if (isVerbose){
    double end_timestamp = MPI_Wtime();
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
      latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0, sizeof(double) * NUM_DOUBLES, 1, false);
    printf("-----------------------------------------\n");
    printf("Scatter Using Isend Latency is: \n");
    latency_stats_in_microsecond.print();
    //latency_stats_in_microsecond.write_to_csv_file("Output/FanoutSleep_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");
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
              NUM_THREADS =  stoi(optarg);
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
    WORLD_SIZE = world_size;

    //Create dt datatype
    CREATE_CONTIGUOUS_DATATYPE(dt);
    //Create shared data structure
    mailRoom = MailRoom(NUM_THREADS);


    int verboser_thread ;
    if (NUM_THREADS == 1){
      verboser_thread = 0;
    }else{
      verboser_thread = GENERATE_A_RANDOM_NUMBER(1, NUM_THREADS - 1, 0);
    }

    int verboser_rank;
    if (world_size == 1)
      verboser_rank = 0;
    else
      verboser_rank = GENERATE_A_RANDOM_NUMBER(0,world_size - 1);

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
      scatter_async_regular_routine(world_rank, tid, tid == verboser_thread && world_rank == verboser_rank);

      scatter_async_self_invented_routine(world_rank, tid, tid == verboser_thread && world_rank == verboser_rank);
    }

    MPI_Finalize();
}
