#! /bin/sh

make clean rawwithtitle
./sqmain 4 2 MSQLatencyRaw2 MSQLatencySummary
make clean raw
./sqmain 4 4 MSQLatencyRaw4 MSQLatencySummary
./sqmain 4 6 MSQLatencyRaw6 MSQLatencySummary
./sqmain 4 8 MSQLatencyRaw8 MSQLatencySummary
./sqmain 4 12 MSQLatencyRaw12 MSQLatencySummary
./sqmain 4 16 MSQLatencyRaw16 MSQLatencySummary
./sqmain 4 24 MSQLatencyRaw24 MSQLatencySummary
./sqmain 4 32 MSQLatencyRaw32 MSQLatencySummary
./sqmain 4 48 MSQLatencyRaw48 MSQLatencySummary

make clean all
./sqmain 4 96,114,192,228,384 MSQLatencyRaw2 MSQLatencySummary
./sqmain 4 456,768,912,1024 MSQLatencyRaw2 MSQLatencySummary

make clean thwithtitle
./sqmain 4 2,4,6,8,12,16 MSQLatencyRaw2 MSQThroughputSummary
make clean th
./sqmain 4 24,32,48 MSQLatencyRaw2 MSQThroughputSummary
./sqmain 4 96,114,192,228,384 MSQLatencyRaw2 MSQThroughputSummary
./sqmain 4 456,768,912,1024 MSQLatencyRaw2 MSQThroughputSummary



