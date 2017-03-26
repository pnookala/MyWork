#! /bin/sh

make clean rawwithtitle
./sqmain 2 2 CKLatencyRaw2 CKLatencySummary
make clean raw
./sqmain 2 4 CKLatencyRaw4 CKLatencySummary
./sqmain 2 6 CKLatencyRaw6 CKLatencySummary
./sqmain 2 8 CKLatencyRaw8 CKLatencySummary
./sqmain 2 12 CKLatencyRaw12 CKLatencySummary
./sqmain 2 16 CKLatencyRaw16 CKLatencySummary
./sqmain 2 24 CKLatencyRaw24 CKLatencySummary
./sqmain 2 32 CKLatencyRaw32 CKLatencySummary
./sqmain 2 48 CKLatencyRaw48 CKLatencySummary

make clean all
./sqmain 2 96,114,192,228,384 CKLatencyRaw2 CKLatencySummary
./sqmain 2 456,768,912,1024 CKLatencyRaw2 CKLatencySummary

make clean thwithtitle
./sqmain 2 2,4,6,8,12,16 CKLatencyRaw2 CKThroughputSummary
make clean th
./sqmain 2 24,32,48 CKLatencyRaw2 CKThroughputSummary
./sqmain 2 96,114,192,228,384 CKLatencyRaw2 CKThroughputSummary
./sqmain 2 456,768,912,1024 CKLatencyRaw2 CKThroughputSummary



