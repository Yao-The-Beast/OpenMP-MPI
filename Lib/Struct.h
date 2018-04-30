#ifndef STRUCT_H
#define STRUCT_H

#include <mpi.h>
#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <algorithm>
#include <functional>
#include <numeric>
#include <time.h>
#include <fstream>
#include <unistd.h>
#include "omp.h"
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <cstring>
#include <sched.h>
#include <assert.h>
#include <atomic>
#include <list>

using namespace std;

#define NUM_DOUBLES 1024
#define NUM_MESSAGE_PER_RECEIVER 1;

//Stats Structure to store the data and the relevant measurements
struct Stats{
  string description;
  double average;
  double median;
  vector<double> data;
  double throughput;
  double normalized_throughput;

  Stats(vector<double> data, double average, double median, double throughput, double normalized_throughput){
    this->data = data;
    this->average = average;
    this->median = median;
    this->throughput = throughput;
    this->normalized_throughput = normalized_throughput;
  }

  void print(){
    printf("%s, %f, %f\n", description.c_str(), median, normalized_throughput);
  }

  void write_to_csv_file(string path){
    ofstream myfile;
    myfile.open(path);
    //write the throughput first
    myfile << normalized_throughput;
    myfile << "\n";
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

struct MasterBuffer{
  MasterBuffer(){};

  vector<double> data;
  vector<bool> canPull;
  atomic<int> counter = 0;

  int message_size;
  int num_messages_per_operation;
  int num_workers;

  mutex key;
  condition_variable listener;
  condition_variable counterListener;

  MasterBuffer(int message_size, int num_messages_per_operation, int num_workers){
    this->message_size = message_size;
    this->num_messages_per_operation = num_messages_per_operation;
    this->num_workers = num_workers;
    canPull = vector<bool>(num_workers, false);
  }

  double* getData(){
    return &data[0];
  }

  void setBuffer(int message_size, int num_messages_per_operation, int num_workers, int num_ranks){
    data = std::vector<double>(message_size * num_messages_per_operation * num_workers * num_ranks, 0);
  }

  void is_safe_to_replace_buffer(){
    unique_lock<mutex> lock(key);
    while (counter.load(std::memory_order_relaxed) != 0){
      counterListener.wait(lock);
    }
  }
  
  void reset(){
    unique_lock<mutex> lock(key);
    counter.store(num_workers, std::memory_order_relaxed);
    canPull = std::vector<bool>(num_workers, true);
    listener.notify_all();
  }

  void canFetch(int tid){
    unique_lock<mutex> lock(key);
    listener.wait(lock, [=]{ return canPull[tid]; });
  }

  void fetchDataScatter(double* recvBuffer, int tid){
    canFetch(tid);
    memcpy(recvBuffer, &data[0] + tid * message_size * num_messages_per_operation, message_size * num_messages_per_operation * sizeof(double));
    
    unique_lock<mutex> lock(key);
    canPull[tid] = false;
    if (1 == counter.fetch_sub(1, memory_order_relaxed)){
      counterListener.notify_one();
    }
  }

  void fetchDataBroadcast(double* recvBuffer, int tid){
    canFetch(tid);
    memcpy(recvBuffer, &data[0] , message_size * num_messages_per_operation * sizeof(double));
    
    unique_lock<mutex> lock(key);
    canPull[tid] = false;
    if (1 == counter.fetch_sub(1, memory_order_relaxed)){
      counterListener.notify_one();
    }
  }

};

#endif