#include "../Lib/Lib.h"

#define NUM_ACTUAL_MESSAGES 5000
#define NUM_WARMUP_MESSAGES 10000
#define BUSY_SEND_RECV_TAG 1
#define MASTER 0

//Number of messages to be sent to one receive per operation
int NUM_MESSAGE_PER_OPERATION = NUM_MESSAGE_PER_RECEIVER;
const int MESSAGE_SIZE = NUM_DOUBLES;

int SLEEP_BASE = 0;
int SLEEP_FLUCTUATION = 0;
MPI_Datatype dt;

using namespace std;

//Sync
void busy_scatter_sync_routine(int my_address, bool isVerbose, int world_size){
  vector<double> sendBuffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION * world_size, 0);
  vector<double> recvBuffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 0);
  vector<double> latencies;
  double start_timestamp, end_timestamp;

  start_timestamp = MPI_Wtime();

  //If I am the one who scatters the message
  if (my_address == MASTER){
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //initialize send buffer here
      for (int j = 0; j < world_size; j++){
        sendBuffer[j * MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION + 10] = MPI_Wtime();
      }
      //Send the messages
      MPI_Scatter(&sendBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, &recvBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, MPI_COMM_WORLD);
      //Simulate here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }
  //If I am the one who receives the message
  }else{
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Receive the message
      MPI_Scatter(&sendBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, &recvBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, MPI_COMM_WORLD);
      //Put down the timestamp
      if (isVerbose){
        double recv = MPI_Wtime();
        latencies.push_back(recv - recvBuffer[10]);
      }
      //Simulate work here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
    }
  }

  if (isVerbose){
    end_timestamp = MPI_Wtime();
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
      latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0,
      sizeof(double) * NUM_MESSAGE_PER_OPERATION * MESSAGE_SIZE, 1, false);
    latency_stats_in_microsecond.description = "scatter";
    latency_stats_in_microsecond.print();
    //latency_stats_in_microsecond.write_to_csv_file("Output/FanoutSleep_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");
  }
}

//Async: Iscatter & Iscatter
void busy_scatter_async_routine(int my_address, bool isVerbose, int world_size){
  vector<double> sendBuffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION * world_size, 0);
  vector<double> recvBuffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 0);
  vector<double> latencies;
  double start_timestamp, end_timestamp;

  start_timestamp = MPI_Wtime();

  MPI_Request sendRequest;
  MPI_Request recvRequest;

  //If I am the one who scatters the message
  if (my_address == MASTER){
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //initialize send buffer here
      for (int j = 0; j < world_size; j++){
        sendBuffer[j * MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION + 10] = MPI_Wtime();
      }
      //Send the messages
      MPI_Iscatter(&sendBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, &recvBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, MPI_COMM_WORLD, &sendRequest);
      //Simulate here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
      //Wait for the request to finish
      MPI_Waitall(1, &sendRequest, MPI_STATUSES_IGNORE);
    }
  //If I am the one who receives the message
  }else{
    for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Receive the message
      MPI_Iscatter(&sendBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, &recvBuffer[0], NUM_MESSAGE_PER_OPERATION, dt, MASTER, MPI_COMM_WORLD, &recvRequest);
      //Simulate work here
      //USLEEP(SLEEP_BASE, SLEEP_FLUCTUATION);
      //Wait for the request to finish
      MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);
      //Put down the timestamp
      if (isVerbose){
        double recv = MPI_Wtime();
        latencies.push_back(recv - recvBuffer[10]);
      }
    }
  }

  if (isVerbose){
    end_timestamp = MPI_Wtime();
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
      latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0,
      sizeof(double) * MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 1, false);
    latency_stats_in_microsecond.description = "iscatter";
    latency_stats_in_microsecond.print();
    //latency_stats_in_microsecond.write_to_csv_file("Output/FanoutSleep_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");
  }
}

//Async using Isend
void busy_scatter_async_isend_routine(int my_world_rank, bool isVerbose, int world_size){

  vector<double> sendBuffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION * world_size, 0);
  vector<double> recvBuffer(MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 0);
  vector<double> latencies;

  MPI_Request sendRequests[world_size];
  MPI_Request recvRequest;

  double start_timestamp = MPI_Wtime();
  //I am the Master who scatters the messages
  if (my_world_rank == MASTER){
    for (uint64_t i = 0; i < NUM_ACTUAL_MESSAGES; i++){
      //Prepare the data
      for (int j = 0; j < world_size; j++){
        sendBuffer[j * MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION + 10] = MPI_Wtime();
      }
      //Send to everyone
      for (int w = 0; w < world_size; w++){
        int tag = w;
        MPI_Isend(&sendBuffer[w * MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION], NUM_MESSAGE_PER_OPERATION,
          dt, w, tag, MPI_COMM_WORLD, &sendRequests[w]);
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
    latency_stats_in_microsecond.description = "isend";
    latency_stats_in_microsecond.print();
    //latency_stats_in_microsecond.write_to_csv_file("Output/FanoutSleep_Multi_MPIs_Sync_" + to_string(world_size) + ".txt");
  }
}

int main(int argc, char** argv) {
  //Porcess Arguments
  int opt;
  while ((opt = getopt(argc,argv,":B:F:N:d")) != EOF){
      switch(opt)
      {
          case 'B':
            SLEEP_BASE = stoi(optarg);
            break;
          case 'F':
            SLEEP_FLUCTUATION = stoi(optarg);
            break;
          case 'N':
            NUM_MESSAGE_PER_OPERATION = stoi(optarg);
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

  //Create dt datatype
  CREATE_CONTIGUOUS_DATATYPE(dt);

  //int verboser = GENERATE_A_RANDOM_NUMBER(1, world_size - 1, 1);
  int verboser = 1;

  MPI_Barrier(MPI_COMM_WORLD);

  //Sync
  busy_scatter_sync_routine(world_rank, verboser == world_rank, world_size);
  MPI_Barrier(MPI_COMM_WORLD);

  // //Use Isend instead of Iscatter
  // busy_scatter_async_isend_routine(world_rank, verboser == world_rank, world_size);
  // MPI_Barrier(MPI_COMM_WORLD);

  MPI_Finalize();
}
