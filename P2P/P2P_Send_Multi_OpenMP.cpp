#include "../Lib/Lib.h"

#define NUM_ACTUAL_MESSAGES 10000
#define NUM_WARMUP_MESSAGES 10000
#define BUSY_SEND_RECV_TAG 2

using namespace std;

const int THREAD_LEVEL = MPI_THREAD_MULTIPLE;
int NUM_THREADS = 1;
int SLEEP_BASE = 100;
int SLEEP_FLUCTUATION = 25;
MPI_Datatype dt;

/* ----------- SYNCHRONOUS ---------- */
void busy_send_recv_sync_routine(int hisAddress, int myAddress, bool isVerbose, int world_size){

    double mySent[NUM_DOUBLES];
    double hisSent[NUM_DOUBLES];

    vector<double> latencies;
    double recv;
    double start_timestamp, end_timestamp;
    int tag = BUSY_SEND_RECV_TAG * 100 + omp_get_thread_num();

    start_timestamp = MPI_Wtime();

    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Enforce send & recv sequence to make sure there is no deadlock
      if (myAddress % 2 == 1){
        mySent[10] = MPI_Wtime();
        MPI_Send(mySent, 1, dt, hisAddress, tag, MPI_COMM_WORLD);
      }else{
        MPI_Recv(hisSent, 1, dt, hisAddress, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      }

      //Calculate the latency
      if (isVerbose){
        recv = MPI_Wtime();
        latencies.push_back((recv - hisSent[10]));
      }

      //usleep to simulate work here
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }

    if (isVerbose){
      end_timestamp = MPI_Wtime();
      Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
        latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0, sizeof(double) * NUM_DOUBLES, 1, false);
      printf("-----------------------------------------\n");
      printf("Sync Send & Recv Latency Stats are: \n");
      latency_stats_in_microsecond.print();
      //latency_stats_in_microsecond.write_to_csv_file("Output/twoSidedSleepSend_Multi_OpenMP_Sync_" + to_string(world_size * NUM_THREADS) + ".txt");
    }
}

/* ----------- ASYNCHRONOUS ---------- */
void busy_send_recv_async_routine(int hisAddress, int myAddress, bool isVerbose, int world_size){
    //timestamp
    double mySent[NUM_DOUBLES];
    double hisSent[NUM_DOUBLES];

    MPI_Request sendRequest;
    MPI_Request recvRequest;

    vector<double> latencies;
    double recv;
    double start_timestamp, end_timestamp;
    int tag = BUSY_SEND_RECV_TAG * 100 + omp_get_thread_num();

    start_timestamp = MPI_Wtime();

    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){

      if (myAddress % 2 == 1){
        mySent[10] = MPI_Wtime();
        MPI_Isend(&mySent, 1, dt, hisAddress, tag, MPI_COMM_WORLD, &sendRequest);
      }else{
        MPI_Irecv(&hisSent, 1, dt, hisAddress, tag, MPI_COMM_WORLD, &recvRequest);
      }

      //usleep to simulate work here
      USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);

      //Wait for the request to finish
      if (myAddress % 2 == 1){
        MPI_Waitall(1, &sendRequest, MPI_STATUSES_IGNORE);
      }else{
        MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);
      }
      //Calculate the time
      if (isVerbose){
        recv = MPI_Wtime();
        latencies.push_back((recv - hisSent[10]));
      }
    }

    if (isVerbose){
      end_timestamp = MPI_Wtime();
      //Need to take care of two way communication
      //So we multiply the each message size by 2
      Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
        latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0, sizeof(double) * NUM_DOUBLES, 1, false);
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

  // Create datatype
  CREATE_CONTIGUOUS_DATATYPE(dt);

  // We are assuming at least 2 and even amount of processes for this task
  if (world_size < 2 || world_size % 2 == 1) {
    fprintf(stderr, "World size must be even and greater than 1 for %s\n", argv[0]);
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  int verboser_thread = GENERATE_A_RANDOM_NUMBER(0, NUM_THREADS - 1, 0);
  int verboser_rank = GENERATE_A_RANDOM_NUMBER(0,world_size - 1, 1);

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

    //Use Send & Recv
    if (world_rank % 2 == 0){
      busy_send_recv_sync_routine(world_rank + 1, world_rank, verboser_rank == world_rank && verboser_thread == tid, world_size);
    }else{
      busy_send_recv_sync_routine(world_rank - 1, world_rank, verboser_rank == world_rank && verboser_thread == tid, world_size);
    }

    //Use Isend & Irecv
    if (world_rank % 2 == 0){
      busy_send_recv_async_routine(world_rank + 1, world_rank, verboser_rank == world_rank && verboser_thread == tid, world_size);
    }else{
      busy_send_recv_async_routine(world_rank - 1, world_rank, verboser_rank == world_rank && verboser_thread == tid, world_size);
    }
  }

  MPI_Finalize();
}
