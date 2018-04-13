#include "../Lib/Lib.h"

#define NUM_ACTUAL_MESSAGES 10000
#define NUM_WARMUP_MESSAGES 10000
#define BUSY_SEND_RECV_TAG 1
#define MASTER 0

int SLEEP_BASE = 100;
int SLEEP_FLUCTUATION = 1;
MPI_Datatype dt;
const int NUM_MESSAGE_PER_OPERATION = NUM_MESSAGE_PER_RECEIVER;
const int MESSAGE_SIZE = NUM_DOUBLES;

using namespace std;


//Sync using MPI_Bcast
void broadcast_sync_routine(int my_address, bool isVerbose, int world_size){
  vector<double> buffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 0);
  vector<double> latencies;
  double start_timestamp, end_timestamp;

  start_timestamp = MPI_Wtime();

  //If I am the one who scatters the message
  if (my_address == MASTER){
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //initialize send buffer here
      buffer[10] = MPI_Wtime();
      //Send the messages
      MPI_Bcast(&buffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, MPI_COMM_WORLD);
      //Simulate here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }
  //If I am the one who receives the message
  }else{
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Send the messages
      MPI_Bcast(&buffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, MPI_COMM_WORLD);
      //Put down the timestamp
      if (isVerbose){
        double recv = MPI_Wtime();
        latencies.push_back(recv - buffer[10]);
      }
      //Simulate work here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }
  }

  if (isVerbose){
    end_timestamp = MPI_Wtime();
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
      latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0,
      sizeof(double) * MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 1, false);
    printf("-----------------------------------------\n");
    printf("Broadcasting Sync Latency is: \n");
    latency_stats_in_microsecond.print();
  }
}

//Async using MPI_Ibcast
void broadcast_async_routine(int my_address, bool isVerbose, int world_size){
  vector<double> buffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 0);
  vector<double> latencies;
  double start_timestamp, end_timestamp;

  start_timestamp = MPI_Wtime();

  MPI_Request recvRequest, sendRequest;

  //If I am the one who broadcasts the message
  if (my_address == MASTER){
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //initialize send buffer here
      buffer[10] = MPI_Wtime();
      //Send the messages
      MPI_Ibcast(&buffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, MPI_COMM_WORLD, &sendRequest);
      //Simulate here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
      //Wait for the request
      MPI_Waitall(1, &sendRequest, MPI_STATUSES_IGNORE);
    }
  //If I am the one who receives the message
  }else{
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Send the messages
      MPI_Ibcast(&buffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, MPI_COMM_WORLD, &recvRequest);
      //Simulate work here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
      //Wait for the request
      MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);
      //Put down the timestamp
      if (isVerbose){
        double recv = MPI_Wtime();
        latencies.push_back(recv - buffer[10]);
      }
    }
  }

  if (isVerbose){
    end_timestamp = MPI_Wtime();
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
      latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0,
      sizeof(double) * MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 1, false);
    printf("-----------------------------------------\n");
    printf("Broadcasting Async Latency is: \n");
    latency_stats_in_microsecond.print();
  }
}

//Async using Isend
void broadcast_async_isend_routine(int my_world_rank, bool isVerbose, int world_size){

  vector<double> sendBuffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 0);
  vector<double> recvBuffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 0);
  vector<double> latencies;

  MPI_Request sendRequests[world_size];
  MPI_Request recvRequest;

  double start_timestamp = MPI_Wtime();
  //I am the Master who scatters the messages
  if (my_world_rank == MASTER){
    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Prepare the data
      sendBuffer[10] = MPI_Wtime();
      //Send to everyone
      for (int w = 0; w < world_size; w++){
        int tag = w;
        MPI_Isend(&sendBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, w, tag, MPI_COMM_WORLD, &sendRequests[w]);
      }
      MPI_Irecv(&recvBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, 0, MPI_COMM_WORLD, &recvRequest);
      ////USLEEP to simulate work here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
      //Wait for the requests
      MPI_Waitall(world_size, sendRequests, MPI_STATUSES_IGNORE);
      MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);
    }
  //I receive messages from the master
  }else{
    int tag = my_world_rank;
    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Receive the scattered message
      MPI_Irecv(&recvBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, tag, MPI_COMM_WORLD, &recvRequest);
      ////USLEEP to simulate work here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
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
      latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0,
      sizeof(double) * NUM_MESSAGE_PER_OPERATION * MESSAGE_SIZE, 1, false);
    printf("-----------------------------------------\n");
    printf("Broadcasting Using Isend Latency is: \n");
    latency_stats_in_microsecond.print();
    //latency_stats_in_microsecond.write_to_csv_file("Output/FanoutSleep_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");
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

  //Create dt datatype
  CREATE_CONTIGUOUS_DATATYPE(dt);

  int verboser = GENERATE_A_RANDOM_NUMBER(1, world_size - 1, 0);
  if (world_rank == verboser){
    printf("Verboser is %d \n", verboser);
    printf("SLEEP_BASES %d SLEEP_FLUCTUATION %d \n", SLEEP_BASE, SLEEP_FLUCTUATION);
  }

  MPI_Barrier(MPI_COMM_WORLD);

  //Sync
  broadcast_sync_routine(world_rank, verboser == world_rank, world_size);
  MPI_Barrier(MPI_COMM_WORLD);

  //Async
  broadcast_async_routine(world_rank, verboser == world_rank, world_size);
  MPI_Barrier(MPI_COMM_WORLD);

  //Use Isend instead of Iscatter
  broadcast_async_isend_routine(world_rank, verboser == world_rank, world_size);
  MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
}
