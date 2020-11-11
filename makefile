all: disk.o b+tree

b+tree:
	g++ -o b+tree b+tree.cpp disk.o

disk.o:
	g++ -c disk.cpp

clean:
	rm disk.o b+tree disk
