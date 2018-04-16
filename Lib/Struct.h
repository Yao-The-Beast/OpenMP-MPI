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

using namespace std;

#define NUM_DOUBLES 1024
#define NUM_MESSAGE_PER_RECEIVER 1;

//Stats Structure to store the data and the relevant measurements
struct Stats{
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
    printf("Average: %f ms, Median: %f ms \nReal Throughput: %f MB/s, Normalized Throughput: %f MB/s\n",
      average, median, throughput, normalized_throughput);
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

//A componenet of MailRoom data structure
struct MailBox{
  //< data, <sender_rank, sender_tid> >
  queue<pair<vector<double>, pair<int, int>>> mails;
  mutex key;
  condition_variable listener;

  MailBox(){}

  void putMail(vector<double> mail, int sender_rank, int sender_tid){
    unique_lock<mutex> lock(key);
    mails.push(make_pair(mail, make_pair(sender_rank, sender_tid)));
    listener.notify_one();
  }

  //Get the first mail
  //Blocked until the mailbox is not empty
  pair<int, int> fetchMail(double* recvBuffer){
    unique_lock<mutex> lock(key);
    listener.wait(lock, [=]{ return !mails.empty(); });
    auto mail = mails.front();
    mails.pop();

    //cpy the data into the recvBuffer
    vector<double>& tempMail = mail.first;
    memcpy(recvBuffer, &tempMail[0], tempMail.size());

    return mail.second;
  }
};

//The shared data structure we use
struct MailRoom{
  map<string, vector<MailBox>> categories;

  MailRoom(){};

  MailRoom(int numMailBoxes){
    categories["BROADCAST"] = vector<MailBox>(numMailBoxes);
    categories["SCATTER"] = vector<MailBox>(numMailBoxes);
    categories["GATHER"] = vector<MailBox>(numMailBoxes);
  }

  void putMail(vector<double>* mail, int whichMailBox, int sender_rank, int sender_tid, string operation){
    categories[operation][whichMailBox].putMail(*mail, sender_rank, sender_tid);
  }

  //Return <sender_rank, sender_tid>
  pair<int, int> fetchMail(int whichMailBox, double* recvBuffer, string operation){
    return categories[operation][whichMailBox].fetchMail(recvBuffer);
  }
};

#endif
