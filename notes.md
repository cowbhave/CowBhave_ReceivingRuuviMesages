Raspberry IP
ifconfig

Run the data collection program
sudo ./RuuviDataCollection/RuuviScanner

Buil the data collection program
g++ RuuviScanner.cpp -L/usr/local/lib -lble++ -lzmq -o RuuviScanner

Run the tag MAC address presenting program
sudo ./RuuviDataCollection/RuuviScannerTagMACDefine

Buil the tag MAC address presenting program
g++ RuuviScanner.cpp -L/usr/local/lib -lble++ -lzmq -o RuuviScannerTagMACDefine

Manual date update
sudo date -s "21 June 2019 21:47:45"

Processor temperature
/opt/vc/bin/vcgencmd measure_temp
