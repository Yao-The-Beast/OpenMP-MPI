#include "Struct.h"

using namespace std;

/*
  Our scatter function
  1. Master sends all the messages that are to be sent to that world to the postman.
  2. After postman receives the messages, it will put the messages into the correpsonding mailBoxes in the mailRoom
  3. All the threads in all the worlds will try to fetch mails from their own mailBoxes
*/
void SELF_DEFINED_SCATTER(
  MasterBuffer* masterBuffer, double* recvBuffer,
  int num_messages_per_thread, int message_size, MPI_Datatype dt,
  int my_rank, int my_tid,
  int sender_rank, int sender_tid,
  int world_size, int num_threads,
  int option){

    /* ---------- Receive ----------*/
    if (option == 1){
      masterBuffer->fetchDataScatter(recvBuffer, my_tid);
      return;
    }

    /*---------- Send ----------*/
    int SELF_DEFINED_SCATTER_TAG = 99;
    MPI_Request sendRequests[world_size-1];
    MPI_Request recvRequest;
    //Sender rountine
    int counter = 0;
    if (my_rank == sender_rank && my_tid == sender_tid){

      //Wait for all threads to finish polling data
      masterBuffer->is_safe_to_replace_buffer();
      //reset the counters
      masterBuffer->reset();

      // //Send the data to others
      double* sendBuffer = masterBuffer->getData();
      for (int w = 0; w < world_size; w++){
        if (w == sender_rank){
          continue;
        }
        //Step 1
        MPI_Isend(
          sendBuffer + w * num_messages_per_thread * message_size * num_threads,
          num_messages_per_thread * num_threads,
          dt, w,
          SELF_DEFINED_SCATTER_TAG, MPI_COMM_WORLD, &sendRequests[counter++]);
      }
      MPI_Waitall(world_size-1, sendRequests, MPI_STATUSES_IGNORE);
      return;
    }

    //Postman routine
    if (my_tid == sender_tid && my_rank != sender_rank){
      //Step 2 part 1, Receive the message
      masterBuffer->is_safe_to_replace_buffer();
      MPI_Recv(masterBuffer->getData(), num_threads * num_messages_per_thread, dt, sender_rank, SELF_DEFINED_SCATTER_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      masterBuffer->reset();
    }
}