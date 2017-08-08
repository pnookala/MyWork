#! /bin/sh

make clean all
./sqmain 6 2 OMPLatencyRaw2 OMPLatencySummary
./sqmain 6 4 OMPLatencyRaw4 OMPLatencySummary
./sqmain 6 6 OMPLatencyRaw6 OMPLatencySummary
./sqmain 6 8 OMPLatencyRaw8 OMPLatencySummary
./sqmain 6 12 OMPLatencyRaw12 OMPLatencySummary
./sqmain 6 16 OMPLatencyRaw16 OMPLatencySummary
./sqmain 6 24 OMPLatencyRaw24 OMPLatencySummary
./sqmain 6 32 OMPLatencyRaw32 OMPLatencySummary
./sqmain 6 48 OMPLatencyRaw48 OMPLatencySummary
./sqmain 6 96,114,192,228,384 OMPLatencyRaw2 OMPLatencySumma
./sqmain 6 456,768,912,1024 OMPLatencyRaw2 OMPLatencySummary

make clean th
./sqmain 6 2 OMPLatencyRaw2 OMPThroughputSummary
./sqmain 6 4,6,8,12,16 OMPLatencyRaw2 OMPThroughputSummary
./sqmain 6 24,32,48 OMPLatencyRaw2 OMPThroughputSummary
./sqmain 6 96,114,192,228,384 OMPLatencyRaw2 OMPThroughputSummary
./sqmain 6 456,768,912,1024 OMPLatencyRaw2 OMPThroughputSummary

#./sqmain_all 4 2 OMPLatencyRaw2 OMPLatencySummary
#./sqmain_all 4 4 OMPLatencyRaw4 OMPLatencySummary
#./sqmain_all 4 6 OMPLatencyRaw6 OMPLatencySummary
#./sqmain_all 4 8 OMPLatencyRaw8 OMPLatencySummary
#./sqmain_all 4 12 OMPLatencyRaw12 OMPLatencySummary
#./sqmain_all 4 16 OMPLatencyRaw16 OMPLatencySummary
#./sqmain_all 4 24 OMPLatencyRaw24 OMPLatencySummary
#./sqmain_all 4 32 OMPLatencyRaw32 OMPLatencySummary
#./sqmain_all 4 48 OMPLatencyRaw48 OMPLatencySummary
#./sqmain_all 4 96,114,192,228,384 OMPLatencyRaw2 OMPLatencySumma
#./sqmain_all 4 456,768,912,1024 OMPLatencyRaw2 OMPLatencySummary
#
#./sqmain_thwithtitle 4 2 OMPLatencyRaw2 OMPThroughputSummary
#./sqmain_th 4 4,6,8,12,16 OMPLatencyRaw2 OMPThroughputSummary
#./sqmain_th 4 24,32,48 OMPLatencyRaw2 OMPThroughputSummary
#./sqmain_th 4 96,114,192,228,384 OMPLatencyRaw2 OMPThroughputSummary
#./sqmain_th 4 456,768,912,1024 OMPLatencyRaw2 OMPThroughputSummary
