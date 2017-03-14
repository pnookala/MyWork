#! /bin/sh

make clean raw
./sqmain 3 2,4,6,8,12,16 BQLatencyRaw16 BQLatencySummary16
./sqmain 3 24,32,48 BQLatencyRaw48 BQLatencySummary48
make clean all
./sqmain 3 96,114,192,228,384 BQLatencyRaw384 BQLatencySummary384
./sqmain 3 456,768,912,1024 BQLatencyRaw1024 BQLatencySummary1024
make clean th
./sqmain 3 2,4,6,8,12,16 BQThroughputRaw16 BQThroughputSummary16
./sqmain 3 24,32,48 BQThroughputRaw48 BQThroughputSummary48
./sqmain 3 96,114,192,228,384 BQThroughputRaw384 BQThroughputSummary384
./sqmain 3 456,768,912,1024 BQThroughputRaw1024 BQThroughputSummary1024

