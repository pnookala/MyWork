#! /bin/sh

make clean raw
./sqmain 1 2,4,6,8,12,16 SQueueLatencyRaw16 SQueueLatencySummary16
./sqmain 1 24,32,48 SQueueLatencyRaw48 SQueueLatencySummary48
make clean all
./sqmain 1 96,114,192,228,384 SQueueLatencyRaw384 SQueueLatencySummary384
./sqmain 1 456,768,912,1024 SQueueLatencyRaw1024 SQueueLatencySummary1024
make clean th
./sqmain 1 2,4,6,8,12,16 SQueueThroughputRaw16 SQueueThroughputSummary16
./sqmain 1 24,32,48 SQueueThroughputRaw48 SQueueThroughputSummary48
./sqmain 1 96,114,192,228,384 SQueueThroughputRaw384 SQueueThroughputSummary384
./sqmain 1 456,768,912,1024 SQueueThroughputRaw1024 SQueueThroughputSummary1024



