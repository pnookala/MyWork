#! /bin/sh

make clean rawwithtitle
./sqmain 3 2 BQLatencyRaw2 BQLatencySummary
make clean raw
./sqmain 3 4 BQLatencyRaw4 BQLatencySummary
./sqmain 3 6 BQLatencyRaw6 BQLatencySummary
./sqmain 3 8 BQLatencyRaw8 BQLatencySummary
./sqmain 3 12 BQLatencyRaw12 BQLatencySummary
./sqmain 3 16 BQLatencyRaw16 BQLatencySummary
./sqmain 3 24 BQLatencyRaw24 BQLatencySummary
./sqmain 3 32 BQLatencyRaw32 BQLatencySummary
./sqmain 3 48 BQLatencyRaw48 BQLatencySummary

make clean all
./sqmain 3 96,114,192,228,384 BQLatencyRaw2 BQLatencySummary
./sqmain 3 456,768,912,1024 BQLatencyRaw2 BQLatencySummary

make clean thwithtitle
./sqmain 3 2 BQLatencyRaw2 BQThroughputSummary
make clean th
./sqmain 3 4,6,8,12,16 BQLatencyRaw2 BQThroughputSummary
./sqmain 3 24,32,48 BQLatencyRaw2 BQThroughputSummary
./sqmain 3 96,114,192,228,384 BQLatencyRaw2 BQThroughputSummary
./sqmain 3 456,768,912,1024 BQLatencyRaw2 BQThroughputSummary



