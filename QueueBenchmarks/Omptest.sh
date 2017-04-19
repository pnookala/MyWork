#! /bin/sh

./sqmain_rawwithtitle 4 2 MSQLatencyRaw2 MSQLatencySummary
./sqmain_raw 4 4 MSQLatencyRaw4 MSQLatencySummary
./sqmain_raw 4 6 MSQLatencyRaw6 MSQLatencySummary
./sqmain_raw 4 8 MSQLatencyRaw8 MSQLatencySummary
./sqmain_raw 4 12 MSQLatencyRaw12 MSQLatencySummary
./sqmain_raw 4 16 MSQLatencyRaw16 MSQLatencySummary
./sqmain_raw 4 24 MSQLatencyRaw24 MSQLatencySummary
./sqmain_raw 4 32 MSQLatencyRaw32 MSQLatencySummary
./sqmain_raw 4 48 MSQLatencyRaw48 MSQLatencySummary
./sqmain_all 4 96,114,192,228,384 MSQLatencyRaw2 MSQLatencySumma
./sqmain_all 4 456,768,912,1024 MSQLatencyRaw2 MSQLatencySummary

./sqmain_thwithtitle 4 2 MSQLatencyRaw2 MSQThroughputSummary
./sqmain_th 4 4,6,8,12,16 MSQLatencyRaw2 MSQThroughputSummary
./sqmain_th 4 24,32,48 MSQLatencyRaw2 MSQThroughputSummary
./sqmain_th 4 96,114,192,228,384 MSQLatencyRaw2 MSQThroughputSummary
./sqmain_th 4 456,768,912,1024 MSQLatencyRaw2 MSQThroughputSummary
