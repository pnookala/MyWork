#! /bin/sh

make clean all
./sqmain 5 2 RCULatencyRaw2 RCULatencySummary
make clean all
./sqmain 5 4 RCULatencyRaw4 RCULatencySummary
./sqmain 5 6 RCULatencyRaw6 RCULatencySummary
./sqmain 5 8 RCULatencyRaw8 RCULatencySummary
./sqmain 5 12 RCULatencyRaw12 RCULatencySummary
./sqmain 5 16 RCULatencyRaw16 RCULatencySummary
./sqmain 5 24 RCULatencyRaw24 RCULatencySummary
./sqmain 5 32 RCULatencyRaw32 RCULatencySummary
./sqmain 5 48 RCULatencyRaw48 RCULatencySummary

make clean all
./sqmain 5 96 RCULatencyRaw2 RCULatencySummary
./sqmain 5 114 RCULatencyRaw2 RCULatencySummary
./sqmain 5 192 RCULatencyRaw2 RCULatencySummary
./sqmain 5 228 RCULatencyRaw2 RCULatencySummary
./sqmain 5 384 RCULatencyRaw2 RCULatencySummary
./sqmain 5 456 RCULatencyRaw2 RCULatencySummary
./sqmain 5 768 RCULatencyRaw2 RCULatencySummary
./sqmain 5 912 RCULatencyRaw2 RCULatencySummary
./sqmain 5 1024 RCULatencyRaw2 RCULatencySummary

make clean thwithtitle
./sqmain 5 2 RCULatencyRaw2 RCUThroughputSummary
make clean th
./sqmain 5 4 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 6 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 8 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 12 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 16 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 24 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 32 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 48 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 96 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 114 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 192 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 228 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 384 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 456 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 768 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 912 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 1024 RCULatencyRaw2 RCUThroughputSummary



