all: np_simple

np_simple:NPshell.cpp NPshell.h Process.cpp Process.h
	g++ np_simple.cpp NPshell.cpp Process.cpp -o np_simple

clean:
	rm -f np_simple