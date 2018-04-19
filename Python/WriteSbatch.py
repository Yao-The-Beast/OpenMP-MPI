import os;
import argparse


def constructSrun(num_nodes_N, num_ranks_per_node_n, num_cores_per_mpi_c ,binary_binary, which_model, sleep_base_B, sleep_fluc_F, thread_num_T, msg_size_N):
	if (which_model == "MPI"):
		return ('srun -N {} -n {} -c {} --cpu_bind=cores ./{} -B {} -F {} -N {}'.format(
			str(num_nodes_N), 
			str(num_ranks_per_node_n * num_nodes_N),
			str(num_cores_per_mpi_c),
			binary_binary + "_" + which_model,
			str(sleep_base_B),
			str(sleep_fluc_F),
			str(msg_size_N)));
	else:
		return ('srun -N {} -n {} -c {} --cpu_bind=cores ./{} -B {} -F {} -T {} -N {}'.format(
			str(num_nodes_N), 
			str(num_ranks_per_node_n * num_nodes_N),
			str(num_cores_per_mpi_c),
			binary_binary + "_" + which_model,
			str(sleep_base_B),
			str(sleep_fluc_F),
			str(thread_num_T),
			str(msg_size_N)));

def main():
	# set parameters
	ap = argparse.ArgumentParser()
	ap.add_argument("-b", "--binary", required=True, help="eg: P2P, Scatter, Broadcast or Gather");
	ap.add_argument("-p", "--processor", required=True, help="eg: haswell or knl");
	ap.add_argument("-r", "--max_ranks_per_node", required=True, help="max amount of mpis per node, eg: 64");
	ap.add_argument("-n", "--max_msg_size", required=True, help="the size of each message in KB, eg: 64");
	args = vars(ap.parse_args());
	binary = args["binary"];
	processor = args["processor"];
	max_ranks_per_node = int(args["max_ranks_per_node"]);
	max_msg_size = int(args["max_msg_size"]);

	num_cores = 0;
	if (processor == "haswell"):
		num_cores = 64;
	else:
		num_cores = 256;
	num_nodes = 2;

	
	# open script
	file = open("../" + binary + "-" + processor + "-SBATCH", "w+");


	file.write("#!/bin/bash -l \n");
	file.write("#SBATCH -C " + processor + "\n");
	file.write("#SBATCH -p debug \n");
	file.write("#SBATCH -N 2 \n");
	file.write("#SBATCH -t 00:30:00 \n");
	file.write("#SBATCH -J " + binary + "-" + processor + "\n");
	file.write("#SBATCH -o " + binary + "-" + processor + ".%j.stdout \n");
	file.write("#SBATCH -e " + binary + "-" + processor + ".%j.error \n");
	if (processor == "knl"):
		file.write("#SBATCH -S 4 \n");			


	# write environmental variables					  
	file.write("export OMP_PROC_BIND=true \n");
	file.write("export OMP_PLACES=threads \n");
	file.write("export MPICH_MAX_THREAD_SAFETY=MPI_THREAD_MULTIPLE \n");
	file.write("export OMP_NUM_THREADS=" + str(num_cores) + " \n");
	file.write("\n");

	########### write sruns ###########

	# Baseline test
	file.write("echo \"" + binary + " baseline, ranks/node:" + str(max_ranks_per_node) + ", max msg size:" + str(max_msg_size) + "KB \" \n");
	msg_size = 1; 
	while msg_size <= max_msg_size:
		file.write(constructSrun(num_nodes, 
			max_ranks_per_node, 
			num_cores / max_ranks_per_node, 
			binary, "MPI", 
			0, 
			0, 
			1, 
			msg_size) + "\n");
		msg_size *= 2;
	file.write("echo \" \" \n\n");

	# Hybrid test
	rank_per_node = 1;
	while rank_per_node <= max_ranks_per_node:
		file.write("echo \"" + binary + " Hybrid, ranks/node:" + str(rank_per_node) + " number of threads: " + str(max_ranks_per_node / rank_per_node)+ ", max msg size:" + str(max_msg_size) + "KB \" \n");

		msg_size = 1; 
		while msg_size <= max_msg_size:
			file.write(constructSrun(num_nodes, 
				rank_per_node, 
				num_cores / rank_per_node, 
				binary, "OpenMP", 
				0, 
				0, 
				max_ranks_per_node / rank_per_node, 
				msg_size) + "\n");
			msg_size *= 2;

		file.write("echo \" \" \n\n");
		rank_per_node *= 2;

if __name__ == "__main__":
    main()



