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

#include <string>

using namespace std;
using namespace BLEPP;

void catch_function(int)
{
	cerr << "\nInterrupdted!\n";
}

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
	cout << "Ruuvi" << endl;
	char MACString[17];

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

					ad.address.copy(MACString, 17); MACString[17] = '\0';
					cout << MACString << endl;
				}
			}
		}
	}
}

