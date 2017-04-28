#! /bin/sh

make clean all
./sqmain 1 2 SQueueLatencyRaw2 SQueueLatencySummary
make clean all
./sqmain 1 4 SQueueLatencyRaw4 SQueueLatencySummary
./sqmain 1 6 SQueueLatencyRaw6 SQueueLatencySummary
./sqmain 1 8 SQueueLatencyRaw8 SQueueLatencySummary
./sqmain 1 12 SQueueLatencyRaw12 SQueueLatencySummary
./sqmain 1 16 SQueueLatencyRaw16 SQueueLatencySummary
./sqmain 1 24 SQueueLatencyRaw24 SQueueLatencySummary
./sqmain 1 32 SQueueLatencyRaw32 SQueueLatencySummary
./sqmain 1 48 SQueueLatencyRaw48 SQueueLatencySummary

make clean all
./sqmain 1 96,114,192,228,384 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 456,768,912,1024 SQueueLatencyRaw2 SQueueLatencySummary

#make clean thwithtitle
#./sqmain 1 2 SQueueLatencyRaw2 SQueueThroughputSummary
#make clean th
#./sqmain 1 4,6,8,12,16 SQueueLatencyRaw2 SQueueThroughputSummary
#./sqmain 1 24,32,48 SQueueLatencyRaw2 SQueueThroughputSummary
#./sqmain 1 96,114,192,228,384 SQueueLatencyRaw2 SQueueThroughputSummary
#./sqmain 1 456,768,912,1024 SQueueLatencyRaw2 SQueueThroughputSummary



