#! /bin/sh

make clean rawwithtitle
./sqmain 1 2 RCULatencyRaw2 RCULatencySummary
make clean raw
./sqmain 1 4 RCULatencyRaw4 RCULatencySummary
./sqmain 1 6 RCULatencyRaw6 RCULatencySummary
./sqmain 1 8 RCULatencyRaw8 RCULatencySummary
./sqmain 1 12 RCULatencyRaw12 RCULatencySummary
./sqmain 1 16 RCULatencyRaw16 RCULatencySummary
./sqmain 1 24 RCULatencyRaw24 RCULatencySummary
./sqmain 1 32 RCULatencyRaw32 RCULatencySummary
./sqmain 1 48 RCULatencyRaw48 RCULatencySummary

make clean all
./sqmain 1 96,114,192,228,384 RCULatencyRaw2 RCULatencySummary
./sqmain 1 456,768,912,1024 RCULatencyRaw2 RCULatencySummary

make clean thwithtitle
./sqmain 1 2 RCULatencyRaw2 RCUThroughputSummary
make clean th
./sqmain 1 4,6,8,12,16 RCULatencyRaw2 RCUThroughputSummary
./sqmain 1 24,32,48 RCULatencyRaw2 RCUThroughputSummary
./sqmain 1 96,114,192,228,384 RCULatencyRaw2 RCUThroughputSummary
./sqmain 1 456,768,912,1024 RCULatencyRaw2 RCUThroughputSummary



