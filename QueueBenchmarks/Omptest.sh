#! /bin/sh

make clean rawwithtitle
./sqmain 6 2 OmpSQueueLatencyRaw2 OmpSQueueLatencySummary
make clean raw
./sqmain 6 4 OmpSQueueLatencyRaw4 OmpSQueueLatencySummary
./sqmain 6 6 OmpSQueueLatencyRaw6 OmpSQueueLatencySummary
./sqmain 6 8 OmpSQueueLatencyRaw8 OmpSQueueLatencySummary
./sqmain 6 12 OmpSQueueLatencyRaw12 OmpSQueueLatencySummary
./sqmain 6 16 OmpSQueueLatencyRaw16 OmpSQueueLatencySummary
./sqmain 6 24 OmpSQueueLatencyRaw24 OmpSQueueLatencySummary
./sqmain 6 32 OmpSQueueLatencyRaw32 OmpSQueueLatencySummary
./sqmain 6 48 OmpSQueueLatencyRaw48 OmpSQueueLatencySummary

make clean all
./sqmain 6 96,114,192,228,384 OmpSQueueLatencyRaw2 OmpSQueueLatencySummary
./sqmain 6 456,768,912,1024 OmpSQueueLatencyRaw2 OmpSQueueLatencySummary

make clean thwithtitle
./sqmain 6 2 OmpSQueueLatencyRaw2 OmpSQueueThroughputSummary
make clean th
./sqmain 6 4,6,8,12,16 OmpSQueueLatencyRaw2 OmpSQueueThroughputSummary
./sqmain 6 24,32,48 OmpSQueueLatencyRaw2 OmpSQueueThroughputSummary
./sqmain 6 96,114,192,228,384 OmpSQueueLatencyRaw2 OmpSQueueThroughputSummary
./sqmain 6 456,768,912,1024 OmpSQueueLatencyRaw2 OmpSQueueThroughputSummary




