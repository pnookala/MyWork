#! /bin/sh

make clean raw
./sqmain 2 2,4,6,8,12,16 CKLatencyRaw16 CKLatencySummary16
./sqmain 2 24,32,48 CKLatencyRaw48 CKLatencySummary48
make clean all
./sqmain 2 96,114,192,228,384 CKLatencyRaw384 CKLatencySummary384
./sqmain 2 456,768,912,1024 CKLatencyRaw1024 CKLatencySummary1024
make clean th
./sqmain 2 2,4,6,8,12,16 CKThroughputRaw16 CKThroughputSummary16
./sqmain 2 24,32,48 CKThroughputRaw48 CKThroughputSummary48
./sqmain 2 96,114,192,228,384 CKThroughputRaw384 CKThroughputSummary384
./sqmain 2 456,768,912,1024 CKThroughputRaw1024 CKThroughputSummary1024

