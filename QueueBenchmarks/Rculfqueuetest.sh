#! /bin/sh

make clean rawwithtitle
./sqmain 5 2 RCULatencyRaw2 RCULatencySummary
make clean raw
./sqmain 5 4 RCULatencyRaw4 RCULatencySummary
./sqmain 5 6 RCULatencyRaw6 RCULatencySummary
./sqmain 5 8 RCULatencyRaw8 RCULatencySummary
./sqmain 5 12 RCULatencyRaw12 RCULatencySummary
./sqmain 5 16 RCULatencyRaw16 RCULatencySummary
./sqmain 5 24 RCULatencyRaw24 RCULatencySummary
./sqmain 5 32 RCULatencyRaw32 RCULatencySummary
./sqmain 5 48 RCULatencyRaw48 RCULatencySummary

make clean all
./sqmain 5 96,114,192,228,384 RCULatencyRaw2 RCULatencySummary
./sqmain 5 456,768,912,1024 RCULatencyRaw2 RCULatencySummary

make clean thwithtitle
./sqmain 5 2 RCULatencyRaw2 RCUThroughputSummary
make clean th
./sqmain 5 4,6,8,12,16 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 24,32,48 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 96,114,192,228,384 RCULatencyRaw2 RCUThroughputSummary
./sqmain 5 456,768,912,1024 RCULatencyRaw2 RCUThroughputSummary



