#include "Struct.h"

#define OUTBOUND_BIN_CAPACITY 5
#define INBOUND_BIN_CAPACITY 5

struct Datagram{
	int message_size;
	int message_num;
	vector<double> data;
	int from_rank;
	int from_tid;
	int to_rank;
	int to_tid;
	int tag;

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
};

struct DatagramMeta{
	int to_rank;
	int to_tid;
	int from_rank;
	int from_tid;
	int tag;
	uint64_t hashCode;
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

struct Bin{
	int owner_rank;
	int owner_tid;
	int capacity;
	mutex key;
  	condition_variable listener;
	map<uint64_t, queue<Datagram>> datagram_dict;

	queue<Datagram> outwardDatagrams;


	void setParam(int owner_rank, int owner_tid, int capacity){
		this->owner_rank = owner_rank;
		this->owner_tid = owner_tid;
		this->capacity = capacity;
	}

	Bin(){}

	//Put the datagram into the inbound mailbox
	void putDatagram(vector<double> data, int message_size, int message_num, 
		int from_rank, int from_tid, int to_rank, int to_tid, int tag){
		Datagram datagram(data, message_size, message_num, from_rank, from_tid, to_rank, to_tid, tag);
		DatagramMeta meta(from_rank, from_tid, to_rank, to_tid, tag);
		uint64_t hashCode = meta.getHashCode();

		cout << "adfadere " << endl;
		//Wait until the queue is not full
		unique_lock<mutex> lock(key);
		cout << "jere " << endl;
    	//listener.wait(lock, [=]{ return datagram_dict[hashCode].size() < capacity; });
    	datagram_dict[hashCode].push(datagram);
    	//notify others
    	listener.notify_one();
	}

	//Fetch the datagram from the inbound mailbox
	Datagram fetchDatagram(int from_rank, int from_tid, int to_rank, int to_tid, int tag){
		DatagramMeta meta(from_rank, from_tid, to_rank, to_tid, tag);

		uint64_t hashCode = meta.getHashCode();
		//Wait until the queue is not empty
		unique_lock<mutex> lock(key);

    	listener.wait(lock, [=]{ return (!datagram_dict[hashCode].empty()); });
    	Datagram thisDatagram = datagram_dict[hashCode].front();
    	datagram_dict[hashCode].pop();
    	//notify others
    	//listener.notify_all();

    	return thisDatagram;
	}

	//Put the datagram into the outbound datagram queue
	void bufferOutwardDatagrams(vector<double> data, int message_size, int message_num, 
		int from_rank, int from_tid, int to_rank, int to_tid, int tag){

		//Lock the queue
		unique_lock<mutex> lock(key);
    	//listener.wait(lock, [=]{ return datagram_dict[hashCode].size() < capacity; });
		Datagram datagram(data, message_size, message_num, from_rank, from_tid, to_rank, to_tid, tag);
		outwardDatagrams.push(datagram);
		//notify others
		//listener.notify_one();
	}
};

struct Bins{
	vector<Bin> outbound_bins;
	vector<Bin> inbound_bins;
	int num_bins;
	int world_rank; //which MPI rank

	Bins(){}

	Bins(int num_bins, int world_rank){
		outbound_bins = vector<Bin>(num_bins);
		inbound_bins = vector<Bin>(num_bins);
		for (int i = 0; i < num_bins; i++){
			outbound_bins[i].setParam(world_rank, i, OUTBOUND_BIN_CAPACITY);
			inbound_bins[i].setParam(world_rank, i, INBOUND_BIN_CAPACITY);
		}

		this->num_bins = num_bins;
		this->world_rank = world_rank;
	}	

	//Worker function
	void sendDatagram(vector<double> data, int message_size, int message_num, 
		int from_rank, int from_tid, int to_rank, int to_tid, int tag){

		//Make sure the request comes from the local
		assert(from_rank == world_rank);

		//I can do it as the receiver is controlled by me
		//Directly put into the receiver's bin
		if (world_rank == to_rank){
			Bin& receiverBin = inbound_bins[to_tid];
			receiverBin.putDatagram(data, message_size, message_num, 
				from_rank, from_tid,
				to_rank, to_tid,
				tag);
		//I cannot do it
		//Put into its outbound bin
		}else{
			Bin& senderBin = outbound_bins[from_tid];
			senderBin.bufferOutwardDatagrams(data, message_size, message_num, 
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


		//Sender is from the same world
		if (from_rank == world_rank){
			Bin& receiverBin = inbound_bins[to_tid];
			Datagram datagram = receiverBin.fetchDatagram(from_rank, from_tid, to_rank, to_tid, tag);
			//copy the data into the buffer
			datagram.copyData(recvBuffer, message_size, message_num);

		//Sender is from the outside world
		}else{
			Bin& receiverBin = inbound_bins[to_tid];
			Datagram datagram = receiverBin.fetchDatagram(from_rank, from_tid, to_rank, to_tid, tag);
			//copy the data into the buffer
			datagram.copyData(recvBuffer, message_size, message_num);
		}
	}

	//Master function
	void busySend(int message_size, int message_num, MPI_Datatype dt, int goal){
		int counter = 0;
		while (counter < goal){
			for (int i = 0; i < num_bins; i++){
				Bin& thisOutboundBin = outbound_bins[i];
				if (!thisOutboundBin.outwardDatagrams.empty()){
					//acquire the lock
					thisOutboundBin.key.lock();
					Datagram& thisDatagram  = thisOutboundBin.outwardDatagrams.front();
					DatagramMeta meta(
						thisDatagram.getFromInfo().first, thisDatagram.getFromInfo().second, 
						thisDatagram.getToInfo().first, thisDatagram.getToInfo().second,
						thisDatagram.getTag());
					MPI_Send(thisDatagram.getData(), message_num, dt, thisDatagram.getToInfo().first, meta.getHashCode(), MPI_COMM_WORLD);
					thisOutboundBin.outwardDatagrams.pop();
					//release the lock
					thisOutboundBin.key.unlock();

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
			DatagramMeta meta = DECOMPOSE(tag);
			
			//Sanity check
			assert(from_rank == meta.from_rank && world_rank == meta.to_rank);
			
			//put into the correct bin
			Bin& thisInboundBin = inbound_bins[meta.to_tid];
			thisInboundBin.key.lock();
			thisInboundBin.putDatagram(recvBuffer, message_size, message_num,
				meta.from_rank, meta.from_tid, meta.to_rank, meta.to_tid, meta.tag);
			thisInboundBin.key.unlock();

			counter++;
		}
	}
};













