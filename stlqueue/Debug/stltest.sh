#! /bin/sh

g++ -O2 -Wall -c -fmessage-length=0 -lboost_system -DRAW -DLATENCY -MMD -MP -MF"src/stlqueue.d" -MT"src/stlqueue.d" -o"src/stlqueue.o" "../src/stlqueue.cpp"

g++  -o"stlqueue"  ./src/stlqueue.o   -lboost_system -lpthread -lboost_thread
./stlqueue 5 2,4,6,8,12,16 STLLatencyRaw16 STLLatencySummary16 
./stlqueue 5 24,32,48 STLLatencyRaw48 STLLatencySummary48
g++ -O2 -Wall -c -fmessage-length=0 -lboost_system -DLATENCY -MMD -MP -MF"src/stlqueue.d" -MT"src/stlqueue.d" -o"src/stlqueue.o" "../src/stlqueue.cpp"

g++  -o"stlqueue"  ./src/stlqueue.o   -lboost_system -lpthread -lboost_thread
./stlqueue 5 96,114,192,228,384 STLLatencyRaw384 STLLatencySummary384
./stlqueue 5 456,768,912,1024 STLLatencyRaw1024 STLLatencySummary1024
g++ -O2 -Wall -c -fmessage-length=0 -lboost_system -DTHROUGHPUT -MMD -MP -MF"src/stlqueue.d" -MT"src/stlqueue.d" -o"src/stlqueue.o" "../src/stlqueue.cpp"

g++  -o"stlqueue"  ./src/stlqueue.o   -lboost_system -lpthread -lboost_thread
./stlqueue 5 2,4,6,8,12,16 STLThroughputRaw16 STLThroughputSummary16
./stlqueue 5 24,32,48 STLThroughputRaw48 STLThroughputSummary48
./stlqueue 5 96,114,192,228,384 STLThroughputRaw384 STLThroughputSummary384
./stlqueue 5 456,768,912,1024 STLThroughputRaw1024 STLThroughputSummary1024
