#ifndef MESSAGE_H
#define MESSAGE_H

#include "Struct.h"

#define OUTBOUND_BIN_CAPACITY 5
#define INBOUND_BIN_CAPACITY 3

struct DatagramMeta{
	int to_rank;
	int to_tid;
	int from_rank;
	int from_tid;
	int tag;
	uint64_t hashCode;
	DatagramMeta(){}

	DatagramMeta(int from_rank, int from_tid, int to_rank, int to_tid, int tag){
		this->to_rank = to_rank;
		this->to_tid = to_tid;
		this->from_rank = from_rank;
		this->from_tid = from_tid;
		this->tag = tag;
		hashCode = to_rank * 1000 * 1000 * 1000 * 1000 + to_tid * 1000 * 1000 +
		 from_rank * 1000 * 1000 + from_tid * 1000 + 
		 tag;
	}

	uint64_t getHashCode(){
		return hashCode;
	}
};

struct Datagram{
	int message_size;
	int message_num;
	vector<double> data;
	int from_rank;
	int from_tid;
	int to_rank;
	int to_tid;
	int tag;

	DatagramMeta meta;

	Datagram(){};

	Datagram(vector<double> data, int message_size, int message_num, 
		int from_rank, int from_tid, int to_rank, int to_tid, int tag){
		this->data = data;
		this->message_size = message_size;
		this->message_num = message_num;
		this->from_rank = from_rank;
		this->to_rank = to_rank;
		this->from_tid = from_tid;
		this->to_tid = to_tid;
		this->tag = tag;
		meta = DatagramMeta(from_rank, from_tid, to_rank, to_tid, tag);
	}

	double* getData(){
		return &data[0];
	}

	int getTag(){
		return tag;
	}

	pair<int, int> getFromInfo(){
		return make_pair(from_rank, from_tid);
	}

	pair<int, int> getToInfo(){
		return make_pair(to_rank, to_tid);
	}

	pair<int, int> getDataInfo(){
		return make_pair(message_size, message_num);
	}

	void copyData(double* buffer, int message_size_, int message_num_){
		assert(message_size == message_size_ && message_num == message_num_);
		memcpy(buffer, &data[0], data.size() * sizeof(double));
	}

	uint64_t getHashCode(){
		return meta.getHashCode();
	}
};



DatagramMeta DECOMPOSE(uint64_t hashCode){
	int tag = hashCode % 1000;
	hashCode /= 1000;
	int from_tid = hashCode % 1000;
	hashCode /= 1000;
	int from_rank = hashCode % 1000;
	hashCode /= 1000;
	int to_tid = hashCode % 1000;
	hashCode /= 1000;
	int to_rank = hashCode % 1000;
	return DatagramMeta(from_rank, from_tid, to_rank, to_tid, tag);
}

int COMPOSE_COMPRESSED_TAG(int from_tid, int to_tid, int tag){
	return from_tid * 100 * 100 + to_tid * 100 + tag;
}

int EXTRACT_FROM_TID(int hashCode){
	return hashCode / 100 / 100;
}

int EXTRACT_TO_TID(int hashCode){
	return (hashCode / 100) % 100;
}

int EXTRACT_TAG(int hashCode){
	return hashCode % 100;
}

struct Bin{
	int owner_rank;
	int owner_tid;
	int capacity;
	mutex key;
  	condition_variable listener;
	list<Datagram*> message_queue;


	Bin(int owner_rank, int owner_tid, int capacity){
		this->owner_rank = owner_rank;
		this->owner_tid = owner_tid;
		this->capacity = capacity;
	}


	//Put the datagram into the inbound mailbox
	void putDatagram(vector<double>* data, int message_size, int message_num, 
		int from_rank, int from_tid, int to_rank, int to_tid, int tag){
		Datagram* datagram = new Datagram(*data, message_size, message_num, from_rank, from_tid, to_rank, to_tid, tag);
		uint64_t hashCode = datagram->getHashCode();
		unique_lock<mutex> lock(key);
    	message_queue.push_back(datagram);
    	listener.notify_one();
	}

	//Fetch the datagram from the inbound mailbox
	list<Datagram*>::iterator fetchDatagram(int from_rank, int from_tid, int to_rank, int to_tid, int tag){
		DatagramMeta meta(from_rank, from_tid, to_rank, to_tid, tag);
		uint64_t hashCode = meta.getHashCode();
		unique_lock<mutex> lock(key);
		list<Datagram*>::iterator thisIterator;

    	listener.wait(lock, [&](){ 
    		for (auto it = message_queue.begin(); it != message_queue.end(); it++){
    			if ((*it)->getHashCode() == hashCode){
    				thisIterator = it;
    				return true;
    			}
    		}
    		return false;
    	});
    	return thisIterator;
	}

	//Put the datagram into the outbound datagram queue
	void bufferOutwardDatagrams(vector<double>* data, int message_size, int message_num, 
		int from_rank, int from_tid, int to_rank, int to_tid, int tag){

		Datagram* datagram = new Datagram(*data, message_size, message_num, from_rank, from_tid, to_rank, to_tid, tag);
		//Lock the queue
		unique_lock<mutex> lock(key);
		listener.wait(lock, [=](){
			if (message_queue.size() == capacity){
				return false;
			}else{
				return true;
			}
		});
		message_queue.push_back(datagram);
	}

	void eraseIterator(list<Datagram*>::iterator it){
		unique_lock<mutex> lock(key);
		delete(*it);
		message_queue.erase(it);
	}
};

struct Bins{
	vector<Bin*> outbound_bins;
	vector<Bin*> inbound_bins;
	int num_bins;
	int world_rank; //which MPI rank

	Bins(int num_bins, int world_rank){
		for (int i = 0; i < num_bins; i++){
			Bin* thisBin = new Bin(world_rank, i, OUTBOUND_BIN_CAPACITY);
			outbound_bins.push_back(thisBin);
			Bin* thisBin2 = new Bin(world_rank, i, INBOUND_BIN_CAPACITY);
			inbound_bins.push_back(thisBin2);
		}

		this->num_bins = num_bins;
		this->world_rank = world_rank;
	}	

	~Bins(){
		for (int i = 0; i < num_bins; i++){
			delete(outbound_bins[i]);
			delete(inbound_bins[i]);
		}
	}

	//Worker function
	void sendDatagram(vector<double>* data, int message_size, int message_num, 
		int from_rank, int from_tid, int to_rank, int to_tid, int tag){

		//Make sure the request comes from the local
		assert(from_rank == world_rank);

		//I can do it as the receiver is controlled by me
		//Directly put into the receiver's bin
		if (world_rank == to_rank){
			Bin* receiverBin = inbound_bins[to_tid];
			receiverBin->putDatagram(data, message_size, message_num, 
				from_rank, from_tid,
				to_rank, to_tid,
				tag);
		//I cannot do it
		//Put into its outbound bin
		}else{
			Bin* senderBin = outbound_bins[from_tid];
			senderBin->bufferOutwardDatagrams(data, message_size, message_num, 
				from_rank, from_tid,
				to_rank, to_tid,
				tag);
		}
	}

	//Worker function
	void recvDatagram(double* recvBuffer, int message_size, int message_num, 
		int from_rank, int from_tid, int to_rank, int to_tid, int tag){

		//Make sure the request comes from the local
		assert(to_rank == world_rank);

		Bin* receiverBin = inbound_bins[to_tid];
		auto it = receiverBin->fetchDatagram(from_rank, from_tid, to_rank, to_tid, tag);
		//copy the data into the buffer
		(*it)->copyData(recvBuffer, message_size, message_num);
		receiverBin->eraseIterator(it);
		
	}

	//Master function
	void busySend(int message_size, int message_num, MPI_Datatype dt, int goal){
		int counter = 0;
		while (counter < goal){ 
			for (int i = 0; i < num_bins; i++){
				Bin* thisOutboundBin = outbound_bins[i];
				
				if (thisOutboundBin->message_queue.size() != 0){

					thisOutboundBin->key.lock();
					Datagram* thisDatagram  = thisOutboundBin->message_queue.front();
					thisOutboundBin->message_queue.pop_front();
					//release the lock
					thisOutboundBin->key.unlock();
					thisOutboundBin->listener.notify_one();
					
					int compressed_tag = COMPOSE_COMPRESSED_TAG(
						thisDatagram->getFromInfo().second,
						thisDatagram->getToInfo().second, 
						thisDatagram->getTag());
					//New to define a new tag as we cannot exceed the tag limit
					MPI_Send(thisDatagram->getData(), message_num, dt, thisDatagram->getToInfo().first, compressed_tag, MPI_COMM_WORLD);
					delete(thisDatagram);
					counter++;
				}
			}
		}
	}

	//Master function
	void busyRecv(int message_size, int message_num, MPI_Datatype dt, int goal){
		
		int counter = 0;
		vector<double> recvBuffer(message_size * message_num, 0);
		MPI_Status status;
		while (counter < goal){
			MPI_Recv(&recvBuffer[0], message_num, dt, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			int tag = status.MPI_TAG;
			int from_rank = status.MPI_SOURCE;
			DatagramMeta meta(from_rank, EXTRACT_FROM_TID(tag), world_rank, EXTRACT_TO_TID(tag), EXTRACT_TAG(tag));			
			//put into the correct bin
			Bin* thisInboundBin = inbound_bins[meta.to_tid];
			thisInboundBin->putDatagram(&recvBuffer, message_size, message_num,
				meta.from_rank, meta.from_tid, meta.to_rank, meta.to_tid, meta.tag);

			counter++;
		}
 	}
};


#endif










