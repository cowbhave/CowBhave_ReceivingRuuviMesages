#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <cerrno>
#include <array>
#include <iomanip>
#include <vector>
//#include <boost/optional.hpp>
//#include <boost/date_time/posix_time/posix_time.hpp>
//#include <boost/date_time/gregorian/gregorian.hpp>
#include <iostream>
#include <fstream>
#include <sys/time.h>

#include <signal.h>
#include <chrono>

#include <stdexcept>

#include <blepp/logging.h>
#include <blepp/pretty_printers.h>
#include <blepp/blestatemachine.h> //for UUID. FIXME mofo
#include <blepp/lescan.h>

#include <zmq.hpp>
#include <string>

using namespace std;
//using namespace std::chrono;
using namespace BLEPP;
//using namespace boost::posix_time;
//using namespace boost::gregorian;

std::string getTime()
{
    timeval curTime;
    time_t now;

    time(&now);
    gettimeofday(&curTime, NULL);

    int milli = curTime.tv_usec / 1000;
    char buf[sizeof "2011-10-08T07:07:09"];
    strftime(buf, sizeof buf, "%FT%T", localtime(&now));
    sprintf(buf, "%s.%d", buf, milli);

    return buf;
}


void catch_function(int)
{
	cerr << "\nInterrupdted!\n";
}

typedef union{
	int16_t i16bit[3];
	uint8_t u8bit[6];
} axis3bit16_t;

///Parameters definition
bool WriteInFile = false;
int ZMQBufferN = 3000;
char StationNo[] = { '1','3','\0' };
string DataSaveFileName = "/home/pi/RuuviDataCollection/data/RuuviData";
///

int main(int argc, char** argv)
{
	string nm;
	std::vector<uint8_t> ruuvidata;
	//high_resolution_clock::time_point t1;
	//high_resolution_clock::time_point t2;
	//t1 = high_resolution_clock::now();

	HCIScanner::ScanType type = HCIScanner::ScanType::Active;
	HCIScanner::FilterDuplicates filter = HCIScanner::FilterDuplicates::Software;

	filter = HCIScanner::FilterDuplicates::Off;

	log_level = LogLevels::Warning;
	HCIScanner scanner(true, filter, type);

	//Catch the interrupt signal. If the scanner is not 
	//cleaned up properly, then it doesn't reset the HCI state.
	signal(SIGINT, catch_function);
	//cout << "Ruuvi" << endl;
	string line;
	char TagNoFit[105][4];
	char MACFit[105][18];
	char TagNo[18];
	int TagNoMACFitN = 0, i, j;
	
	ifstream Tfile("/home/pi/RuuviDataCollection/TagNumberMACFit.csv", ios::in);
	while (getline(Tfile, line))//Tfile >> line)
	{
		i = 0;
		while (line[i] != ',')
		{
			TagNoFit[TagNoMACFitN][i] = line[i];
			//cout << TagNoFit[TagNoMACFitN][i];
			i++;
		}
		TagNoFit[TagNoMACFitN][i] = '\0';
		//cout << ' ';
		i++; j = 0;
		while (line[i] != 0)
		{
			MACFit[TagNoMACFitN][j] = line[i];
			//cout << MACFit[TagNoMACFitN][j];
			i++;
			j++;
		}
		MACFit[TagNoMACFitN][j] = '\0';
		//cout << '\n';
		TagNoMACFitN++;
	}
	Tfile.close();
	
	for(i=0;i<TagNoMACFitN;i++)
	{
		j=0;
		while(j<4 && TagNoFit[i][j]!=0)
		{
			cout << TagNoFit[i][j];
			j++;
		}
		cout << " ";
		for(j=0;j<17;j++)
			cout << MACFit[i][j];
		cout << endl;
	}

	cout << "Ruuvi data collection started" << endl;
	string Date = getTime().substr(0, 10);
	//string TagNo;

	int MaxMessageSize = 200;
	char MessageString[MaxMessageSize];
	int MSN = 0;
	zmq::context_t context(1);
	zmq::socket_t publisher(context, ZMQ_PUB);
	publisher.bind("tcp://*:5556");
	std::cout << "Data is being transfered by ZMQ" << std::endl;

	char MessageStringBuffer[MaxMessageSize*ZMQBufferN];
	int ZMQBuffer_i = 0;
	int MSBN = 0;

	int CurrentStatePresentationInd = 0;
	bool CurrentStatePresentationTagNo[TagNoMACFitN];
	for (i = 0; i < TagNoMACFitN; i++) CurrentStatePresentationTagNo[i] = false;
	char MACString[17], timeChar[24];

	while (1)
	{
		//Check to see if there's anything to read from the HCI
		//and wait if there's not.
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 300000;

		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(scanner.get_fd(), &fds);
		int err = select(scanner.get_fd() + 1, &fds, NULL, NULL, &timeout);

		//Interrupted, so quit and clean up properly.
		if (err < 0 && errno == EINTR)
			break;

		if (FD_ISSET(scanner.get_fd(), &fds))
		{
			//Only read id there's something to read
			vector<AdvertisingResponse> ads = scanner.get_advertisements();

			for (const auto& ad : ads)
			{
				if (ad.manufacturer_specific_data.size() > 0 &&
					ad.manufacturer_specific_data[0][0] == 0x59 &&
					ad.manufacturer_specific_data[0][1] == 0x00)
				{

					uint8_t ruuvidata[24] = { 0 };
					memcpy(&ruuvidata, &ad.manufacturer_specific_data[0].data()[2], 24);

					axis3bit16_t acc;
					uint16_t msg_count = uint16_t(ruuvidata[21] << 8) | ruuvidata[22];
					uint32_t MSBmsg_count = 0;

					string timeString = getTime();
					timeString.copy(timeChar, 24); timeChar[23] = '\0';
					ad.address.copy(MACString, 17); MACString[17] = '\0';
					//find tag no		
					i = -1; bool f = true;
					while (i < TagNoMACFitN && f)
					{
						i++;
						if (MACFit[i][0] == MACString[0] && MACFit[i][1] == MACString[1] &&
							MACFit[i][3] == MACString[3] && MACFit[i][4] == MACString[4])
							f = false;						
					}
					if(f)
					{
						strncpy(TagNo,MACString,17);
						//cout << MACString << endl;
					}
					else
					{
						strncpy(TagNo,TagNoFit[i],4);
						CurrentStatePresentationTagNo[i] = true;
					}

					MSN = snprintf(MessageString, MaxMessageSize, "%s,%s,%s,%d", StationNo, timeChar, TagNo, (int)ad.rssi);
					//cout << ad.ch_index << endl;
					//read MSBmsg_count
					for (int ii = 4; ii >= 0; ii--)
					{
						MSBmsg_count <<= 2;
						MSBmsg_count |= ruuvidata[ii * 4 + 3] & 0b00000011;
					}
					MSBmsg_count <<= 16;
					MSBmsg_count += msg_count;
					MSN += snprintf(MessageString + MSN, MaxMessageSize, ",%d", MSBmsg_count);

					for (int ii = 0; ii < 5; ii++)
					{
						acc.u8bit[1] = ruuvidata[ii * 4];
						acc.u8bit[3] = ruuvidata[ii * 4 + 1];
						acc.u8bit[5] = ruuvidata[ii * 4 + 2];
						acc.u8bit[0] = ruuvidata[ii * 4 + 3] & 0b11000000;
						acc.u8bit[2] = (ruuvidata[ii * 4 + 3] & 0b00110000) << 2;
						acc.u8bit[4] = (ruuvidata[ii * 4 + 3] & 0b00001100) << 4;
						MSN += snprintf(MessageString + MSN, MaxMessageSize, ", %d,%d,%d", acc.i16bit[0] >> 3, acc.i16bit[1] >> 3, acc.i16bit[2] >> 3);
					}

					snprintf(MessageString + MSN, MaxMessageSize, ",%s", "END");
					//collect in the buffer					
					MSBN += snprintf(MessageStringBuffer + MSBN, MaxMessageSize, "%s\r\n", MessageString);
					ZMQBuffer_i++;
					//send by zmq						
					if (ZMQBuffer_i == ZMQBufferN)
					{
						snprintf(MessageStringBuffer + MSBN, MaxMessageSize, "%s", "ENDMES");
						zmq::message_t message(MaxMessageSize*ZMQBufferN);
						snprintf((char *)message.data(), MaxMessageSize*ZMQBufferN, "%s", MessageStringBuffer);

						publisher.send(message,zmq::send_flags::none);
						MSBN = 0;
						ZMQBuffer_i = 0;
						//cout << MessageStringBuffer << endl;
					}

					//save to file
					if (WriteInFile)
					{
						if (Date != timeString.substr(0, 10))
						{
							Date = timeString.substr(0, 10);
						}
						ofstream dfile(DataSaveFileName + "_Tag" + TagNoFit[i] + "_St" + StationNo + "_" + Date + ".csv", ios::app);

						dfile << MessageString << endl;
						dfile.close();
					}
					//cout << MessageString << endl;	
				}
			}
		}
		CurrentStatePresentationInd++;
		if (CurrentStatePresentationInd == 1000*TagNoMACFitN)
		{
			cout << timeChar << ", active tags:" << endl;
			for (i = 0; i < TagNoMACFitN; i++)
			{
				if (CurrentStatePresentationTagNo[i])
					cout << TagNoFit[i] << ", ";
				//CurrentStatePresentationTagNo[i] = false;
			}
			cout << endl;
			cout << "Inactive tags:" << endl;
			for (i = 0; i < TagNoMACFitN; i++)
			{
				if (!CurrentStatePresentationTagNo[i])
					cout << TagNoFit[i] << ", ";
				CurrentStatePresentationTagNo[i] = false;
			}
			cout << endl;
			CurrentStatePresentationInd = 0;
		}
	}
}

