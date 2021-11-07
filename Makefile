all: np_simple np_single_proc np_multi_proc

np_multi_proc: $(shell find np_multi_proc_dir) $(shell find shared_lib)
	g++ np_multi_proc_dir/np_multi_proc.cpp np_multi_proc_dir/NPshell.cpp np_multi_proc_dir/Process.cpp np_multi_proc_dir/global.cpp -o np_multi_proc -lrt 

np_simple: $(shell find np_simple_dir) $(shell find shared_lib)
	g++ np_simple_dir/np_simple.cpp np_simple_dir/NPshell.cpp np_simple_dir/Process.cpp -o np_simple

np_single_proc: $(shell find np_single_proc_dir) $(shell find shared_lib)
	g++ np_single_proc_dir/np_single_proc.cpp np_single_proc_dir/NPshell.cpp np_single_proc_dir/Process.cpp -o np_single_proc

clean:
	rm -f np_simple
	rm -f np_single_proc
	rm -f np_multi_proc