#! /bin/sh

make clean all
./sqmain 1 2 SQueueLatencyRaw2 SQueueLatencySummary
make clean all
./sqmain 1 4 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 6 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 8 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 12 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 16 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 24 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 32 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 48 SQueueLatencyRaw2 SQueueLatencySummary

make clean all
./sqmain 1 96,114,192,228,384 SQueueLatencyRaw2 SQueueLatencySummary
./sqmain 1 456,768,912,1024 SQueueLatencyRaw2 SQueueLatencySummary

make clean all
./sqmain 3 2 BQLatencyRaw2 BQLatencySummary
make clean all
./sqmain 3 4 BQLatencyRaw2 BQLatencySummary
./sqmain 3 6 BQLatencyRaw2 BQLatencySummary
./sqmain 3 8 BQLatencyRaw2 BQLatencySummary
./sqmain 3 12 BQLatencyRaw2 BQLatencySummary
./sqmain 3 16 BQLatencyRaw2 BQLatencySummary
./sqmain 3 24 BQLatencyRaw2 BQLatencySummary
./sqmain 3 32 BQLatencyRaw2 BQLatencySummary
./sqmain 3 48 BQLatencyRaw2 BQLatencySummary

make clean all
./sqmain 3 96,114,192,228,384 BQLatencyRaw2 BQLatencySummary
./sqmain 3 456,768,912,1024 BQLatencyRaw2 BQLatencySummary

make clean all
./sqmain 1 2 RCULatencyRaw2 RCULatencySummary
make clean all
./sqmain 1 4 RCULatencyRaw2 RCULatencySummary
./sqmain 1 6 RCULatencyRaw2 RCULatencySummary
./sqmain 1 8 RCULatencyRaw2 RCULatencySummary
./sqmain 1 12 RCULatencyRaw2 RCULatencySummary
./sqmain 1 16 RCULatencyRaw2 RCULatencySummary
./sqmain 1 24 RCULatencyRaw2 RCULatencySummary
./sqmain 1 32 RCULatencyRaw2 RCULatencySummary
./sqmain 1 48 RCULatencyRaw2 RCULatencySummary

make clean all
./sqmain 1 96,114,192,228,384 RCULatencyRaw2 RCULatencySummary
./sqmain 1 456,768,912,1024 RCULatencyRaw2 RCULatencySummary

make clean all
./sqmain 4 2 MSQLatencyRaw2 MSQLatencySummary
make clean all
./sqmain 4 4 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 6 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 8 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 12 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 16 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 24 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 32 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 48 MSQLatencyRaw2 MSQLatencySummary

make clean all
./sqmain 4 96,114,192,228,384 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 456,768,912,1024 MSQLatencyRaw2 MSQLatencySummary


