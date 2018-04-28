#include "../Lib/Lib.h"

#define NUM_ACTUAL_MESSAGES 5000
using namespace std;

const int THREAD_LEVEL = MPI_THREAD_MULTIPLE;
int NUM_MESSAGE_PER_OPERATION = NUM_MESSAGE_PER_RECEIVER;
const int MESSAGE_SIZE = NUM_DOUBLES;

int NUM_THREADS = 1;
int SLEEP_BASE = 0;
int SLEEP_FLUCTUATION = 0;
MPI_Datatype dt;


Bins* bins;

void master_send_rountine(){
  bins->busySend(MESSAGE_SIZE, NUM_MESSAGE_PER_OPERATION, dt, NUM_ACTUAL_MESSAGES * (NUM_THREADS-2));
}

void master_recv_rountine(){
  bins->busyRecv(MESSAGE_SIZE, NUM_MESSAGE_PER_OPERATION, dt, NUM_ACTUAL_MESSAGES * (NUM_THREADS-2));
}

void send_routine(int my_rank, int world_size, bool isVerbose){

  int my_tid = omp_get_thread_num();

  vector<double> sendBuffer(NUM_MESSAGE_PER_OPERATION * MESSAGE_SIZE, 0);
  int tag = 1; 
  int to_rank = -1; int to_tid = -1;

  for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){

    sendBuffer[i % MESSAGE_SIZE] = MPI_Wtime();

    to_rank = my_rank+1;
    to_tid = my_tid;

    bins->sendDatagram(&sendBuffer, MESSAGE_SIZE, NUM_MESSAGE_PER_OPERATION, 
        my_rank, my_tid, to_rank, to_tid, tag);
  }
}



void recv_routine(int my_rank, int world_size, bool isVerbose){
  
  int my_tid = omp_get_thread_num();
  vector<double> latencies;
  double start_timestamp = MPI_Wtime();

  vector<double> recvBuffer(NUM_MESSAGE_PER_OPERATION * MESSAGE_SIZE, 0);
  int tag = 1;
  int from_rank = -1; int from_tid = -1;

  for (int i = 0; i < NUM_ACTUAL_MESSAGES; i++){

    from_rank = my_rank-1;
    from_tid = my_tid;

    bins->recvDatagram(&recvBuffer[0], MESSAGE_SIZE, NUM_MESSAGE_PER_OPERATION,
      from_rank, from_tid, my_rank, my_tid, tag);
    if (isVerbose){
      latencies.push_back(MPI_Wtime() - recvBuffer[i % MESSAGE_SIZE]);
    }
  }


  if (isVerbose){
    double end_timestamp = MPI_Wtime();
    Stats latency_stats_in_microsecond = CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(
      latencies, start_timestamp, end_timestamp, SLEEP_BASE + SLEEP_FLUCTUATION / 2.0,
      sizeof(double) * MESSAGE_SIZE * NUM_MESSAGE_PER_OPERATION, 1, false);
    latency_stats_in_microsecond.description = "SuperSendRemote";
    latency_stats_in_microsecond.print();
  }

}

int main(int argc, char** argv) {

  //Porcess Arguments
  int opt;
  while ((opt = getopt(argc,argv,":B:F:T:N:d")) != EOF){
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
          case 'N':
            NUM_MESSAGE_PER_OPERATION = stoi(optarg);
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

  int verboser_thread = GENERATE_A_RANDOM_NUMBER(2, NUM_THREADS - 1);
  int verboser_rank = GENERATE_A_RANDOM_NUMBER(0,world_size - 1, 0);
  verboser_rank = 1;
  verboser_thread = 3;

  //initialize bin here
  bins = new Bins(NUM_THREADS, world_rank);

  omp_set_num_threads(NUM_THREADS);

  int inum, err, cpu;
  cpu_set_t cpu_mask;        

  #pragma omp parallel shared(bins) private(inum, cpu_mask, err, cpu)
  {
    inum = omp_get_thread_num() + NUM_THREADS * world_rank;
    CPU_ZERO(     &cpu_mask);           
    CPU_SET(inum, &cpu_mask);           
    err = sched_setaffinity((pid_t)0, sizeof(cpu_mask), &cpu_mask );
    cpu = sched_getcpu();     
              
    int tid = omp_get_thread_num();

    #pragma omp barrier

    if (world_rank % 2 == 0){
      if (tid == 0){
        master_send_rountine();
      }else if (tid == 1){
      }else{
        send_routine(world_rank, world_size, world_rank == verboser_rank && tid == verboser_thread);
      }
    }else{
      if (tid == 0){
      }else if (tid == 1){
        master_recv_rountine();
      }else{
        recv_routine(world_rank, world_size, world_rank == verboser_rank && tid == verboser_thread);
      }
    }
  }

  delete(bins);

  MPI_Finalize();
}
