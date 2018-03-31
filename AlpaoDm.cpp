// Header: All types and class are in the ACS namespace
/****************************************************
* (c) 2018, Zuzeng Lin
*           zuzeng@kth.se
* Copyright(c) Alpao SDK 
****************************************************
*/
#include "asdkDM.h"
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1  
#define _CRT_SECURE_NO_WARNINGS 1  
using namespace acs;
#pragma comment(lib, "ws2_32.lib") 
#include <winsock2.h>
#include <thread>
#include <time.h>
// System Headers
#include <iostream>
#include <windows.h>


//UDP PORT
WSADATA wsaData;
WORD sockVersion;
SOCKET serSocket;
sockaddr_in serAddr;
sockaddr_in remoteAddr;
int port;
bool terminating = false;
std::thread udploop;
#define PERROR(...) printf( "[ERROR] " );printf( __VA_ARGS__ );printf("\n" );_gettch();
#define PINFO(...) printf( __VA_ARGS__);printf( "\n" );
#define MAX_AMPLITUDE 1
#define MIN_AMPLITUDE 0
//globals
UInt nbAct;

// Example using C++ API
int dmExample(DM dm)
{
	sockVersion = MAKEWORD(2, 2);
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		return 0;
	}

	serSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serSocket == INVALID_SOCKET)
	{
		return 0;
	}

	serAddr.sin_family = AF_INET;
	serAddr.sin_port = htons(port);
	serAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(serSocket, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR)
	{
		closesocket(serSocket);
		return 0;
	}
	int nAddrLen = sizeof(remoteAddr);
	char recvData[512];
	Scalar *voltage = new Scalar[nbAct];


	while (true) {
		int nAddrLen = sizeof(remoteAddr);
		char recvData[512];
		int ret = recvfrom(serSocket, recvData, 512, 0, (sockaddr *)&remoteAddr, &nAddrLen);
		if (ret > 0)
		{

			recvData[ret] = 0x00;
			//printf("%s \r\n", inet_ntoa(remoteAddr.sin_addr));	
			bool set = true;
			int offset;
			char *data = recvData;

			auto reci_case = 0;
			if (sscanf(data, "%d%n", &reci_case, &offset) == 1) {
				data += offset;
				switch (reci_case) {
				case 2: {
					//reset
					PINFO("[ERROR] Reset is not possible on this mirror!");
					break;
				}
				case 1: {
					//write
					for (int i = 0; i < nbAct; i++) {
						double recitmp;
						if (sscanf(data, "%lf%n", &recitmp, &offset) == 1) {
							data += offset;
							if (recitmp <= MAX_AMPLITUDE) {
								voltage[i] = recitmp;
							}
							else {
								voltage[i] = MAX_AMPLITUDE;
								PINFO("[ERROR] MAX_AMPLITUDE exceeded at ch %d, setting to max.", i);
								PINFO("%s", recvData);
								break;
							}
						}
						else {
							PINFO("[ERROR] partial data in a packet.");
							set = false;
							break;
						}

					}
					if (set) {
						if (!dm.Check())
						{
							return -1;
						}
						dm.Send(voltage);
						
					}
					char sendData[512];
					sprintf(sendData, "set point okay\n");
					sendto(serSocket, sendData, strlen(sendData), 0, (sockaddr *)&remoteAddr, nAddrLen);
					break;
				}
				case 3:
				{
					//read
					char sendData[512];
					sendData[0] = 0x00;
					for (int i = 0; i < nbAct; i++)
						sprintf(sendData + strlen(sendData), "%d ", voltage[i]);
					sprintf(sendData + strlen(sendData), "\n");
					sendto(serSocket, sendData, strlen(sendData), 0, (sockaddr *)&remoteAddr, nAddrLen);
					break;
				}
				default:
				{
					terminating = true;
					break;
				}
				}
			}
		}
		if (terminating) break;
	}
	// Release memory
	delete[] voltage;
	closesocket(serSocket);
	WSACleanup();

   


    return 0;
}

// Main program
int main( int argc, char ** argv )
{
	String serialName;
	PINFO("-----------------------------------------------------------\n");
	PINFO(" Alpao Deformable Mirror instrument driver \n");
	PINFO(" zuzeng@kth.se \n");
	PINFO("-----------------------------------------------------------\n\n");
	if (argc <= 1)
		port = 5555;
	else
		sscanf(argv[1], "%d", &port);
	std::cout << "Please enter the S/N within the following format: BXXYYY (see DM backside)" << std::endl;
	std::cin >> serialName;
	std::cin.ignore(10, '\n');
	PINFO("Receiving command from UDP port: %d", port)
	// Get serialName number
	
	// Load configuration file
	DM dm(serialName.c_str());
	// Get the number of actuators
	nbAct = (UInt)dm.Get("NbOfActuator");

	// Check errors
	if (!dm.Check())
	{
		return -1;
	}

	std::cout << "Number of actuators: " << nbAct << std::endl;
    int ret = dmExample(dm);
    
    // Print last errors if any
    while ( !DM::Check() ) DM::PrintLastError();

    return ret;
}