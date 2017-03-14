#! /bin/sh

make clean raw
./sqmain 4 2,4,6,8,12,16 MSQLatencyRaw16 MSQLatencySummary16 2.4
./sqmain 4 24,32,48 MSQLatencyRaw48 MSQLatencySummary48 2.4
make clean all
./sqmain 4 96,114,192,228,384 MSQLatencyRaw384 MSQLatencySummary384 2.4
./sqmain 4 456,768,912,1024 MSQLatencyRaw1024 MSQLatencySummary1024 2.4
make clean th
./sqmain 4 2,4,6,8,12,16 MSQThroughputRaw16 MSQThroughputSummary16 2.4
./sqmain 4 24,32,48 MSQThroughputRaw48 MSQThroughputSummary48 2.4
./sqmain 4 96,114,192,228,384 MSQThroughputRaw384 MSQThroughputSummary384 2.4
./sqmain 4 456,768,912,1024 MSQThroughputRaw1024 MSQThroughputSummary1024 2.4
