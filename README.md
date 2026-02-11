The program is (to my knowledge) mostly functional and implemented according to the assignment specifications. There are issues with two trains of equal loading time that are inserted into the queue when there are no trains crossing having arbitrary ordering between them. The included input file has one of these problems: trains 6 and 7 can attempt to cross before the other is loaded, making the order arbitrary. 

To run the included input file, make the file and run ./mts input.txt 
