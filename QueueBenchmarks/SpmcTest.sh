#! /bin/sh

make clean all
./sqmain 8 1 SPMCLatencyRaw1 SPMCLatencySummary
make clean all
./sqmain 8 2 SPMCLatencyRaw2 SPMCLatencySummary
./sqmain 8 4 SPMCLatencyRaw4 SPMCLatencySummary
./sqmain 8 6 SPMCLatencyRaw6 SPMCLatencySummary
./sqmain 8 8 SPMCLatencyRaw8 SPMCLatencySummary
./sqmain 8 12 SPMCLatencyRaw12 SPMCLatencySummary
./sqmain 8 16 SPMCLatencyRaw16 SPMCLatencySummary
./sqmain 8 24 SPMCLatencyRaw24 SPMCLatencySummary
./sqmain 8 32 SPMCLatencyRaw32 SPMCLatencySummary
./sqmain 8 48 SPMCLatencyRaw48 SPMCLatencySummary

make clean all
./sqmain 8 96,114,192,228,384 SPMCLatencyRaw2 SPMCLatencySummary
./sqmain 8 456,768,912,1024 SPMCLatencyRaw2 SPMCLatencySummary

make clean thwithtitle
./sqmain 8 1 SPMCLatencyRaw2 SPMCThroughputSummary
make clean th
./sqmain 8 2,4,6,8,12 SPMCLatencyRaw2 SPMCThroughputSummary
./sqmain 8 16,24,32,48 SPMCLatencyRaw2 SPMCThroughputSummary
./sqmain 8 96,114,192,228,384 SPMCLatencyRaw2 SPMCThroughputSummary
./sqmain 8 456,768,912,1024 SPMCLatencyRaw2 SPMCThroughputSummary




