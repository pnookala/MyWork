#! /bin/sh

make clean all
./sqmain 8 1 SPMCLatencyRaw2 SPMCLatencySummary
make clean all
./sqmain 8 3 SPMCLatencyRaw2 SPMCLatencySummary
./sqmain 8 5 LatencyRaw4 SPMCLatencySummary
./sqmain 8 7 SPMCLatencyRaw6 SPMCLatencySummary
./sqmain 8 11 SPMCLatencyRaw8 SPMCLatencySummary
./sqmain 8 15 SPMCLatencyRaw12 SPMCLatencySummary
./sqmain 8 23 SPMCLatencyRaw16 SPMCLatencySummary
./sqmain 8 31 SPMCLatencyRaw24 SPMCLatencySummary
./sqmain 8 47 SPMCLatencyRaw32 SPMCLatencySummary

make clean all
./sqmain 8 95,113,191,227,383 SPMCLatencyRaw2 SPMCLatencySummary
./sqmain 8 455,767,911,1023 SPMCLatencyRaw2 SPMCLatencySummary

make clean thwithtitle
./sqmain 8 1 SPMCLatencyRaw2 SPMCThroughputSummary
make clean th
./sqmain 8 3,5,7,11,15 SPMCLatencyRaw2 SPMCThroughputSummary
./sqmain 8 23,31,47 SPMCLatencyRaw2 SPMCThroughputSummary
./sqmain 8 95,113,191,227,383 SPMCLatencyRaw2 SPMCThroughputSummary
./sqmain 8 455,767,911,1023 SPMCLatencyRaw2 SPMCThroughputSummary




