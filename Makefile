all: np_simple np_single_proc

np_simple: np_simple_dir/* shared_lib/*
	g++ np_simple_dir/np_simple.cpp np_simple_dir/NPshell.cpp np_simple_dir/Process.cpp -o np_simple

np_single_proc: np_single_proc_dir/* shared_lib/*
	g++ np_single_proc_dir/np_single_proc.cpp np_single_proc_dir/NPshell.cpp np_single_proc_dir/Process.cpp -o np_single_proc

clean:
	rm -f np_simple
	rm -f np_single_proc