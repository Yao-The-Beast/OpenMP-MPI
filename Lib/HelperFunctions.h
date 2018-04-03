#include <mpi.h>
#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <functional>
#include <numeric>
#include <time.h>
#include <fstream>
#include "omp.h"

using namespace std;

#define WARMUP_TAG 100

//Stats Structure to store the data and the relevant measurements
struct Stats{
  double average;
  double median;
  vector<double> data;
  double throughput;
  Stats(vector<double> data, double average, double median, double throughput){
    this->data = data;
    this->average = average;
    this->median = median;
    this->throughput = throughput;
  }

  void print(){
    printf("Average: %f ms, Median: %f ms, Throughput: %f MB/s \n", average, median, throughput);
  }

  void write_to_csv_file(string path){
    ofstream myfile;
    myfile.open(path);
    //write the throughput first
    myfile << throughput;
    myfile << "\n";
    //write rest of the latency data
    for (auto it = data.begin(); it != data.end(); it++){
      myfile << *it;
      myfile << "\n";
    }
    myfile.close();
  }
};

//Get current time in microseconds
uint64_t NOW_IN_MICROSECOND(){
  return std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
}

//return the throughput in MB/S
double CALCULATE_THROUGHPUT_BATCH(double start_timestamp, double end_timestamp, int num_messages, int message_size, int batchSize){
  double time_elapsed = end_timestamp - start_timestamp;
  double data_size_MB = num_messages * message_size * batchSize / 1024.0 / 1024.0;
  double throughput = data_size_MB / time_elapsed;
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
  Stats stats(timestamps, average, median, throughput);
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

//Return a random number [lower, upper]
int GENERATE_A_RANDOM_NUMBER(int lower, int upper){
  srand (time(NULL));
  return rand() % (upper - lower + 1) + lower;
}
