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
#include <unistd.h>
#include "omp.h"
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>

using namespace std;

#define NUM_DOUBLES 20480

struct MailBox{
  queue<vector<double>> mails;
  mutex key;
  condition_variable listener;

  void putMail(vector<double> thisMail){
    unique_lock<mutex> lock(key);
    mails.push(thisMail);
    listener.notify_one();
  }

  //Get the first mail
  //Blocked until the mailbox is not empty
  vector<double> fetchMail(){
    unique_lock<mutex> lock(key);
    listener.wait(lock, [=]{ return !mails.empty(); });
    vector<double> output = mails.front();
    mails.pop();
    return output;
  }
};

void CREATE_CONTIGUOUS_DATATYPE(MPI_Datatype& newType, int num_data = NUM_DOUBLES, MPI_Datatype oldType = MPI_DOUBLE){
   MPI_Type_contiguous(num_data, oldType, &newType);
   MPI_Type_commit(&newType);
}
