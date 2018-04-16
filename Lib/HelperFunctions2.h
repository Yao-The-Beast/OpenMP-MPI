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
#include <cstring>

using namespace std;

#define NUM_DOUBLES 2048

void CREATE_CONTIGUOUS_DATATYPE(MPI_Datatype& newType, int num_data = NUM_DOUBLES, MPI_Datatype oldType = MPI_DOUBLE){
   MPI_Type_contiguous(num_data, oldType, &newType);
   MPI_Type_commit(&newType);
}

/*
  Our scatter function
  1. Master sends all the messages that are to be sent to that world to the postman.
  2. After postman receives the messages, it will put the messages into the correpsonding mailBoxes in the mailRoom
  3. All the threads in all the worlds will try to fetch mails from their own mailBoxes
*/
void SELF_DEFINED_SCATTER(
  MailRoom& mailRoom,
  double* sendBuffer, double* recvBuffer,
  int num_messages_per_thread, int message_size, MPI_Datatype dt,
  int my_rank, int my_tid,
  int sender_rank, int sender_tid,
  int postman_tid,
  int world_size, int num_threads,
  int option){

    /* ---------- Receive ----------*/
    if (option == 1){
      //Step 3
      pair<int, int> sender_info = mailRoom.fetchMail(my_tid, recvBuffer);
      return;
    }

    /*---------- Send ----------*/
    int SELF_DEFINED_SCATTER_TAG = 99;
    MPI_Request sendRequests[world_size];
    MPI_Request recvRequest;
    //Sender rountine
    if (my_rank == sender_rank && my_tid == sender_tid){
      for (int w = 0; w < world_size; w++){
        //Step 1
        MPI_Isend(sendBuffer + w * num_messages_per_thread * message_size * num_threads, num_threads, dt, w,
          SELF_DEFINED_SCATTER_TAG, MPI_COMM_WORLD, &sendRequests[w]);
      }
      //prevent deadlock when the master is both the postman and the master
      if (my_tid != postman_tid){
        MPI_Waitall(world_size, sendRequests, MPI_STATUSES_IGNORE);
      }
    }

    //Postman routine
    if (my_tid == postman_tid){
      //Step 2 part 1, Receive the message
      vector<double> recvBuffer(num_threads * message_size * num_messages_per_thread);
      MPI_Irecv(&recvBuffer[0], num_threads * num_messages_per_thread, dt, sender_rank, SELF_DEFINED_SCATTER_TAG, MPI_COMM_WORLD, &recvRequest);
      MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);
      //If postman is master, also wait on send request
      if (my_rank == sender_rank && my_tid == sender_tid){
        MPI_Waitall(world_size, sendRequests, MPI_STATUSES_IGNORE);
      }

      //Step 2 part 2, Split the message and put the message into ech mailbox
      for (int t = 0; t < num_threads; t++){
        vector<double> thisMail(recvBuffer.begin() + message_size * num_messages_per_thread * t,
          recvBuffer.begin() + message_size * num_messages_per_thread * (t + 1));
        mailRoom.putMail(thisMail, t, sender_rank, sender_tid);
      }
    }
}
