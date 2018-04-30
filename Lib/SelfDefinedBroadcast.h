#include "Struct.h"

using namespace std;

/*
  Our broadcast function (highly similar to the scatter)
  1. Master sends all the messages that are to be sent to that world to the postman.
  2. After postman receives the messages, it will put the mail into the all the mailBoxes in the mailRoom
  3. All the threads in all the worlds will try to fetch mails from their own mailBoxes
*/
void SELF_DEFINED_BROADCAST(
  MasterBuffer* masterBuffer,
  vector<double>* buffer,
  int num_messages, int message_size, MPI_Datatype dt,
  int my_rank, int my_tid,
  int sender_rank, int sender_tid,
  int world_size, int num_threads,
  int option){

    /* ---------- Receive ----------*/
    if (option == 1){
      masterBuffer->fetchDataBroadcast(&(*buffer)[0], my_tid);
      return;
    }

    //Sender rountine
    if (my_rank == sender_rank && my_tid == sender_tid){
      double* sendBuffer = masterBuffer->getData();
      //Wait for all threads to finish polling data
      masterBuffer->is_safe_to_replace_buffer();
      //reset the counters
      masterBuffer->reset();
      //Broadcast the message
      MPI_Bcast(sendBuffer,  num_messages, dt, sender_rank, MPI_COMM_WORLD);
    }

    //Postman routine
    if (my_tid == sender_tid && my_rank != sender_rank){
      masterBuffer->is_safe_to_replace_buffer();
      MPI_Bcast(masterBuffer->getData(), num_messages, dt, sender_rank, MPI_COMM_WORLD);
      masterBuffer->reset();
    }
}