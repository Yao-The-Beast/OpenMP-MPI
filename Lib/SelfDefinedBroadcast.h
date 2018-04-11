#include "Struct.h"

using namespace std;

/*
  Our broadcast function (highly similar to the scatter)
  1. Master sends all the messages that are to be sent to that world to the postman.
  2. After postman receives the messages, it will put the mail into the all the mailBoxes in the mailRoom
  3. All the threads in all the worlds will try to fetch mails from their own mailBoxes
*/
void SELF_DEFINED_BROADCAST(
  MailRoom& mailRoom,
  double* sendBuffer, double* recvBuffer,
  int num_messages, int message_size, MPI_Datatype dt,
  int my_rank, int my_tid,
  int sender_rank, int sender_tid,
  int postman_tid,
  int world_size, int num_threads,
  int option){

    /* ---------- Receive ----------*/
    if (option == 1){
      //Step 3
      pair<int, int> sender_info = mailRoom.fetchMail(my_tid, recvBuffer, "SCATTER");
      return;
    }

    /*---------- Send ----------*/
    int SELF_DEFINED_BROADCAST_TAG = 98;
    MPI_Request sendRequests[world_size];
    MPI_Request recvRequest;
    //Sender rountine
    if (my_rank == sender_rank && my_tid == sender_tid){
      for (int w = 0; w < world_size; w++){
        //Step 1
        MPI_Isend(
          sendBuffer + w * num_messages * message_size * num_threads,
          num_messages,
          dt, w,
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
      vector<double> recvBuffer(message_size * num_messages);
      MPI_Irecv(
        &recvBuffer[0],
        num_messages,
        dt, sender_rank,
        SELF_DEFINED_SCATTER_TAG, MPI_COMM_WORLD, &recvRequest);
      MPI_Waitall(1, &recvRequest, MPI_STATUSES_IGNORE);
      //If postman is master, also wait on send request
      if (my_rank == sender_rank && my_tid == sender_tid){
        MPI_Waitall(world_size, sendRequests, MPI_STATUSES_IGNORE);
      }

      //Step 2 part 2, Put the message into ech mailbox
      for (int t = 0; t < num_threads; t++){
        mailRoom.putMail(recvBuffer, t, sender_rank, sender_tid, "SCATTER");
      }
    }
}
