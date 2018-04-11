#include "Struct.h"

using namespace std;

#define WARMUP_TAG 100

//Create MPI_Datatype dt to transmit a vector of double
void CREATE_CONTIGUOUS_DATATYPE(MPI_Datatype& newType, int num_data = NUM_DOUBLES, MPI_Datatype oldType = MPI_DOUBLE){
   MPI_Type_contiguous(num_data, oldType, &newType);
   MPI_Type_commit(&newType);
}

//Return a random number [lower, upper]
//option: 0 dont really care, -1 odd, 1 even
int GENERATE_A_RANDOM_NUMBER(int lower, int upper, int option = -1){
  srand (time(NULL));
  int candidate = rand() % (upper - lower + 1) + lower;
  if (option == -1){
    candidate = max(1, candidate / 2 * 2 - 1);
  }else if (option == 1){
    candidate = candidate / 2 * 2;
  }
  return candidate;
}

//Sleep for (base + random(fluctuation)) microseconds
void USLEEP(int base, int fluctuation){
  unsigned int totalSleepTime = GENERATE_A_RANDOM_NUMBER(0, fluctuation) + base;
  usleep(totalSleepTime);
}

//return the throughput in MB/S
double CALCULATE_THROUGHPUT_BATCH(double start_timestamp, double end_timestamp, int num_messages, int message_size, int batchSize){
  double time_elapsed = end_timestamp - start_timestamp;
  double data_size_MB = num_messages * (message_size  / 1024.0 / 1024.0) * batchSize ;
  double throughput = data_size_MB / time_elapsed;
  return throughput;
}

//return the normalized throughput in MB/S
double TRANSFORM_INTO_NORMALIZED_THROUGHPUT_BATCH(int interval_ms, double start_timestamp, double end_timestamp, int num_messages, int message_size, int batchSize){
  double time_elapsed = end_timestamp - start_timestamp - interval_ms / 1000.0 * num_messages / 1000.0;
  double data_size_MB = num_messages * (message_size  / 1024.0 / 1024.0) * batchSize ;
  double throughput = data_size_MB / time_elapsed;
  return throughput;
}

//return the theoretical throughput in MB/S
double CALCULATE_THEORETIC_THROUGHPUT_BATCH(int interval_ms, int message_size, int batchSize, bool isRoundtrip){
  double multi = 1.0;
  if (isRoundtrip)
    multi = 2.0;
  double throughput = message_size * batchSize * multi * 1.0 / interval_ms * 1000.0 / 1024.0 / 1024.0 * 1000.0;
  return throughput;
}

//Return the stats based on the input vector<double>
Stats CALCULATE_TIMESTAMP_STATS_BATCH(vector<double> timestamps, double start_timestamp, double end_timestamp,
    int message_size, int batchSize){
  for (auto it = timestamps.begin(); it != timestamps.end(); it++){
    (*it) = (*it) / batchSize;
  }
  sort(timestamps.begin(), timestamps.end());
  double average = accumulate(timestamps.begin(), timestamps.end(), 0.0) * 1.0 / timestamps.size() * 1000000;
  double median = timestamps[timestamps.size() / 2] * 1000000 ;
  double throughput = CALCULATE_THROUGHPUT_BATCH(start_timestamp, end_timestamp, timestamps.size(), message_size, batchSize);
  Stats stats(timestamps, average, median, throughput, throughput);
  return stats;
}

//Return the stats based on the input vector<double>
Stats CALCULATE_TIMESTAMP_STATS_BATCH_WITH_SLEEP(vector<double> timestamps, double start_timestamp, double end_timestamp,
    int interval_ms, int message_size, int batchSize, bool isRoundtrip = false){
  for (auto it = timestamps.begin(); it != timestamps.end(); it++){
    (*it) = (*it) / batchSize;
  }
  sort(timestamps.begin(), timestamps.end());
  double average = accumulate(timestamps.begin(), timestamps.end(), 0.0) * 1.0 / timestamps.size() * 1000000;
  double median = timestamps[timestamps.size() / 2] * 1000000 ;
  double throughput = CALCULATE_THROUGHPUT_BATCH(start_timestamp, end_timestamp, timestamps.size(), message_size, batchSize);
  //double theoretical_throughput = CALCULATE_THEORETIC_THROUGHPUT_BATCH(interval_ms, message_size, batchSize, isRoundtrip);
  double normalized_throughput = TRANSFORM_INTO_NORMALIZED_THROUGHPUT_BATCH(interval_ms, start_timestamp, end_timestamp, timestamps.size(), message_size, batchSize);
  Stats stats(timestamps, average, median, throughput, normalized_throughput);
  stats.normalized_throughput = normalized_throughput;
  return stats;
}


//Warmup MPI before doing the actual benchmarking
void MPI_RECV_WARMUP(const int NUM_WARMUP_MESSAGES, int recepient){
  double sent;
  for (uint64_t i = 0; i < NUM_WARMUP_MESSAGES; i++){
    sent = MPI_Wtime();
    MPI_Send(&sent, 1, MPI_DOUBLE, recepient, WARMUP_TAG, MPI_COMM_WORLD);
  }
}
//Warmup MPI before doing the actual benchmarking
void MPI_SEND_WARMUP(const int NUM_WARMUP_MESSAGES, int sender){
  double sent;
  for (uint64_t i = 0; i < NUM_WARMUP_MESSAGES; i++){
    MPI_Recv(&sent, 1, MPI_DOUBLE, sender, WARMUP_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }
}

//Warmup OpenMP before doing the actual benchmarking
void OPENMP_RECV_WARMUP(const int NUM_WARMUP_MESSAGES, int recepient, int THREAD_LEVEL){
  double sent;
  if (THREAD_LEVEL == MPI_THREAD_MULTIPLE){
    int myTag = omp_get_thread_num() + WARMUP_TAG;
    for (uint64_t i = 0; i < NUM_WARMUP_MESSAGES; i++){
      sent = MPI_Wtime();
      MPI_Send(&sent, 1, MPI_DOUBLE, recepient, myTag, MPI_COMM_WORLD);
    }
  }else{
    cerr << "THREAD LEVEL UNKOWN " << THREAD_LEVEL << endl;
  }
}
//Warmup MPI before doing the actual benchmarking
void OPENMP_SEND_WARMUP(const int NUM_WARMUP_MESSAGES, int sender, int THREAD_LEVEL){
  double sent;
  if (THREAD_LEVEL == MPI_THREAD_MULTIPLE){
    int myTag = omp_get_thread_num() + WARMUP_TAG;
    for (uint64_t i = 0; i < NUM_WARMUP_MESSAGES; i++){
      MPI_Recv(&sent, 1, MPI_DOUBLE, sender, myTag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
  }else{
    cerr << "THREAD LEVEL UNKOWN " << THREAD_LEVEL << endl;
  }
}

//Get current time in microseconds
uint64_t NOW_IN_MICROSECOND(){
  return std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
}
