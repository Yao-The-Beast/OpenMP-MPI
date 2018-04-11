#include "../Lib/Lib.h"

#define NUM_ACTUAL_MESSAGES 10000
#define NUM_WARMUP_MESSAGES 10000
#define BUSY_SEND_RECV_TAG 1

using namespace std;

int SLEEP_BASE = 100;
int SLEEP_FLUCTUATION = 25;

MPI_Datatype dt;


/* ----------- SYNCHRONOUS ---------- */
void busy_send_recv_sync_routine(int hisAddress, int myAddress, bool isVerbose, int world_size){
    //the timestamps
    double mySent[NUM_DOUBLES];
    double hisSent[NUM_DOUBLES];

    vector<double> latencies;
    double recv;

    double start_timestamp = MPI_Wtime();
    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Some send other recieve
      if (myAddress % 2 == 1){
        mySent[0] = MPI_Wtime();
        MPI_Send(mySent, 1, dt, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD);
      }else{
        MPI_Recv(hisSent, 1, dt, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      }
      //Calculate latency here
      if (isVerbose){
        recv = MPI_Wtime();
        latencies.push_back(recv - hisSent[0]);
      }
      //usleep to simulate work here
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }

    if (isVerbose){
      double end_timestamp = MPI_Wtime();
      Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
        latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2, sizeof(double) * NUM_DOUBLES, 1, false);
      printf("-----------------------------------------\n");
      printf("Sync Send & Recv Latency Stats are: \n");
      latency_stats_in_microsecond.print();
      //latency_stats_in_microsecond.write_to_csv_file("Output/twoSidedSleepSend_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");
    }
}


/* ----------- ASYNCHRONOUS ---------- */
void busy_send_recv_async_routine(int hisAddress, int myAddress, bool isVerbose, int world_size){
    //timestamp
    double mySent[NUM_DOUBLES];
    double hisSent[NUM_DOUBLES];
    vector<double> latencies;
    double recv;
    MPI_Request sendRequest;
    MPI_Request recvRequest;

    double start_timestamp = MPI_Wtime();

    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){

        //Some send other recvs
        if (myAddress % 2 == 1){
          mySent[0] = MPI_Wtime();
          MPI_Isend(mySent, 1, dt, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, &sendRequest);
        }else{
          MPI_Irecv(hisSent, 1, dt, hisAddress, BUSY_SEND_RECV_TAG, MPI_COMM_WORLD, &recvRequest);
        }

        //usleep to simulate work here
        USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);

        //Wait for the request to finish
        if (myAddress % 2 == 1)
          MPI_Waitall(1, &sendRequest, MPI_STATUSES_IGNORE);
        else
          MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);

        //Calculate latency here
        if (isVerbose){
          recv = MPI_Wtime();
          latencies.push_back((recv - hisSent[0]));
        }
    }

    if (isVerbose){
      double end_timestamp = MPI_Wtime();
      Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
        latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2, sizeof(double) * NUM_DOUBLES, 1, false);
      printf("-----------------------------------------\n");
      printf("Async Send & Recv Latency Stats are: \n");
      latency_stats_in_microsecond.print();
      //latency_stats_in_microsecond.write_to_csv_file("Output/twoSidedSleepSend_Multi_MPIs_Async_" + to_string(world_size) + ".txt");
    }
}

int main(int argc, char** argv) {

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
  if (world_size < 2 || world_size % 2 == 1) {
    fprintf(stderr, "World size must be even and greater than 1 for %s\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  //Create our datatype here
  CREATE_CONTIGUOUS_DATATYPE(dt);

  int verboser = GENERATE_A_RANDOM_NUMBER(0, world_size - 1, 1);
  if (world_rank == verboser){
    printf("----------------------------------\nArguments are: \n");
    printf("BASE %d FLUCTUATION %d \n", SLEEP_BASE, SLEEP_FLUCTUATION);
    printf("Verboser is %d \n", verboser);
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
