The program is (to my knowledge) mostly functional and implemented according to the given specifications. There are issues with two trains of equal loading time that are inserted into the queue when there are no trains crossing having arbitrary ordering between them. The included input file has one of these problems: trains 6 and 7 can attempt to cross before the other is loaded, making the order arbitrary.

The included input file took around 2.8 seconds to complete (averaged over 6 calls). Once again, a minor issue with the code makes the first few trains to cross arbitrary, but here is one output: 
00:00:00.6 Train  5 is ready to go West
00:00:00.6 Train  5 is ON the main track going West
00:00:00.6 Train  6 is ready to go West
00:00:00.6 Train  7 is ready to go East
00:00:00.7 Train  4 is ready to go West
00:00:00.6 Train  5 is OFF the main track after going West
00:00:00.7 Train  6 is ON the main track going West
00:00:00.7 Train  6 is OFF the main track after going West
00:00:00.8 Train  7 is ON the main track going East
00:00:00.8 Train  7 is OFF the main track after going East
00:00:00.9 Train  4 is ON the main track going West
00:00:01.2 Train  0 is ready to go West
00:00:01.1 Train  2 is ready to go West
00:00:01.1 Train  1 is ready to go West
00:00:01.2 Train  3 is ready to go East
00:00:00.9 Train  4 is OFF the main track after going West
00:00:01.2 Train  0 is ON the main track going West
00:00:01.2 Train  0 is OFF the main track after going West
00:00:01.9 Train  3 is ON the main track going East
00:00:01.9 Train  3 is OFF the main track after going East
00:00:02.2 Train  2 is ON the main track going West
00:00:02.2 Train  2 is OFF the main track after going West
00:00:02.3 Train  1 is ON the main track going West
00:00:02.3 Train  1 is OFF the main track after going West