//============================================================================
// Name        : AirPortMusic.cpp
// Author      : Jon Dellaria
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <DLog.h>
#include <LinuxCommand.h>


#include <unistd.h>

#include "MP3Stream.h"
#include "MusicDB.h"

#include "client_http.hpp"
#include "server_http.hpp"
// Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

// Added for the default_resource example
#include <algorithm>
#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#ifdef HAVE_OPENSSL
#include "crypto.hpp"
#endif


#include "UDPServer.h"
#include "APMusic.h"

#include "configurationFile.h"
#include "ApplicationModes.h"


int main_event_handler();
int eventHandler();
int configApp();

using namespace std;
// Added for the json-example:
using namespace boost::property_tree;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

int songFD = 0;
DLog myLog;

int FDflags;


#define SERVER_PORT 5000
#define DATA_GRAM_SERVER_PORT 1234
#define HTTP_SERVER_PORT 8080

#define GET_BIGENDIAN_INT(x) (*(__u8*)(x)<<24)|(*((__u8*)(x)+1)<<16)|(*((__u8*)(x)+2)<<8)|(*((__u8*)(x)+3))

configurationFile myConfig;
ApplicationModes myAppModes;
UDPServer dataGramServer;

typedef enum data_type_t {
	AUD_TYPE_NONE = 0,
	AUD_TYPE_ALAC,
	AUD_TYPE_WAV,
	AUD_TYPE_MP3,
	AUD_TYPE_OGG,
	AUD_TYPE_AAC,
	AUD_TYPE_URLMP3,
	AUD_TYPE_PLS,
	AUD_TYPE_PCM,
	AUD_TYPE_FLAC,
} data_type_t;


playActions playMode = PLAY_ACTION_PLAY;
MP3Stream mp3Stream;

extern int playAutomatic;

int PlaySong(string audioFileName, data_type_t adt);

int main(int argc, char* const argv[])
{
	int returnValue;
	string message;
	char ibuffer [33];
	struct playQRecord pQR;

	if (argc == 2) // if there is an argument, then assume it is the configuration file
	{
		myConfig.getConfiguration(argv[1]);
	}
	else //otherwise assume there is a file in the default with the name "config.conf"
	{
		myConfig.getConfiguration("config.xml");
	}

	configApp();

	dataGramServer.Start(DATA_GRAM_SERVER_PORT);

	HttpServer server;
	server.config.port =  HTTP_SERVER_PORT;

	server.resource["^/status"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
	{
	stringstream stream;
	string json_string = "{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";
	string json_status_string;

	stream << "{";
	stream << "exitMode: " << myAppModes.getPlayMode() << ",";
	stream << "playMode: " << myAppModes.getPlayMode() << ",";
	stream << "networkMode: " << myAppModes.getNetworkMode();
	stream << "}";

//	stream << json_string;

	response->write(stream);
	};

	// GET-example for the path /info
	// Responds with request-information
	server.resource["^/info$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
	{
	stringstream stream;
	stream << "<h1>Request from " << request->remote_endpoint_address() << ":" << request->remote_endpoint_port() << "</h1>";

	stream << request->method << " " << request->path << " HTTP/" << request->http_version;

	stream << "<h2>Query Fields</h2>";
	auto query_fields = request->parse_query_string();
	for(auto &field : query_fields)
	{
		stream << field.first << ": " << field.second << "<br>";
	}

	stream << "<h2>Header Fields</h2>";
	for(auto &field : request->header)
	{
		stream << field.first << ": " << field.second << "<br>";
	}

	response->write(stream);
	};


	server.on_error = [](shared_ptr<HttpServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
	// Handle errors here
	// Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
	};
	thread server_thread([&server]() {
	// Start server
	server.start();
	});


	myAppModes.setPlayMode (PLAY_ACTION_PLAY);
	myAppModes.setNetworkMode(NETWORK_ACTION_DISCONNECT);
	message = "airportAddress is: ";
	message.append(myConfig.airportAddress + "\n");
	myLog.print(logWarning, message);

	OpenDBConnection();
	playAutomatic = 1;

	while ((myAppModes.getPlayMode() != PLAY_ACTION_QUIT))
	{
		message = __func__;
		message.append(": Wile Loop - ");
		message.append("networkMode = ");
		sprintf(ibuffer, "%d", myAppModes.getNetworkMode());
		message.append(ibuffer);
		sprintf(ibuffer, "%d", myAppModes.getPlayMode());
		message.append("playMode = ");
		message.append(ibuffer);
		myLog.print(logDebug, message);

			message = __func__;
			message.append(": playMode == PLAY_ACTION_PLAY getting next song record"); //Jon
			myLog.print(logDebug, message);
			pQR = getNextSongRecord();
			message = __func__;
			message.append(": playMode == PLAY_ACTION_PLAY returned from get song record"); //Jon
			myLog.print(logDebug, message);
			if (pQR.id != 0)
			{
				if (asciiToUtf8(pQR.location, 255) != 1)
				{
					message = __func__;
					message.append(":  UTF Conversion error FileName - ");
					message.append(pQR.location);
					myLog.print(logError, message);
				}
				message = __func__;
				message.append(": PlaySong FileName - ");
				message.append(pQR.location);
				myLog.print(logInformation, message);
				songFD = PlaySong(pQR.location,AUD_TYPE_NONE);
			}
			else
			{
				playMode = PLAY_ACTION_STOP;
				message = __func__;
				message.append(":  No song to play");
				myLog.print(logWarning, message);
				sleep(5);
			}


//		PlaySong("/RAID/Music/Album Music/The Kooks/The Best Of... So Far/1-09 Sofa Song.mp3",AUD_TYPE_MP3);

		returnValue = eventHandler();
		if ((myAppModes.getPlayMode() == PLAY_ACTION_EXIT))
			break;
	}
	message.append(": NetworkTemplate closing all connections and resting for 5 seconds.");
	myLog.print(logWarning, message);
	CloseDBConnection();
	sleep(5);


	server_thread.detach();
	server.stop();
	dataGramServer.Close();
	message = __func__;
	message.append(": AirportTalk exiting Normally");
	myLog.print(logWarning, message);
}


int PlaySong(string audioFileName, data_type_t adt)  // orig
{
	__u8 *buf;
	int size;
	int returnValue;
	string message;
	char ibuffer [33];
	string sbuffer;
	LinuxCommand myCommand;
	char errorMessageBuffer[2048];

//	songFD = mp3Stream.Open(audioFileName);
//			auds.Open(audioFileName,adt);


	const char *darg[4]={MP3PLAYER,NULL, NULL,NULL};
	darg[1] = audioFileName.c_str();

	message = __func__;
	message.append(": Playing Song: ");
	message.append(audioFileName);
	myLog.print(logInformation, message);

	myCommand.Execute(darg,errorMessageBuffer,sizeof(errorMessageBuffer)-2);
	if (strlen(errorMessageBuffer) > 0)
	{
		return (-1); // on error errorMessageBuffer has the error string
	}
	else
	{
		return (0);
	}


	message = __func__;
	message.append(": opening file:");
	message.append(audioFileName);
	myLog.print(logDebug, message);
	returnValue=0;
	playMode = PLAY_ACTION_PLAY;

	while(playMode == PLAY_ACTION_PLAY)
	{
		message = __func__;
		message.append(": playMode == PLAY_ACTION_PLAY");
		myLog.print(logDebug, message);

		eventHandler();

	}

//	message = __func__;
//	message.append(": auds.Close");
//	myLog.print(logDebug, message);
//	mp3Stream.Close();

}

#define MAIN_EVENT_TIMEOUT 3 // sec unit


int configApp()
{
	string message;

	myLog.logFileName = myConfig.logFileName;
	myLog.printFile = myConfig.logPrintFile;
	myLog.printScreen = myConfig.logPrintScreen;
	myLog.printTime = myConfig.logPrintTime;

	if (myConfig.logValue.find("logDebug")!=string::npos)
	{
		myLog.logValue = logDebug;
		message = "myLog.logValue = logDebug";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logInformation")!=string::npos)
	{
		myLog.logValue = logInformation;
		message = "myLog.logValue = logInformation";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logWarning")!=string::npos)
	{
		myLog.logValue = logWarning;
		message = "myLog.logValue = logWarning";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logError")!=string::npos)
	{
		myLog.logValue = logError;
		message = "myLog.logValue = logError";
		myLog.print(logInformation, message);
	}
	return (1);
}

int eventHandler()
{
	int n;
	int iVolume;
	char buffer[1024];
	struct sockaddr_in from;
	char *ps=NULL;
	int returnValue = PLAY_ACTION_NORMAL;
	string message;

//	playMode = PLAY_ACTION_PLAY;
	bzero(buffer,1024);

	n = dataGramServer.GetMessage( buffer);
	if (n > 0)
	{
		if(strstr(buffer,"quit") != NULL )
		{
			myAppModes.setPlayMode (PLAY_ACTION_QUIT);
			message = __func__;
			message.append(": Play Quit Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"pause") != NULL )
		{
			myAppModes.setPlayMode (PLAY_ACTION_PAUSE);
			message = __func__;
			message.append(": Play Pause Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"playautomatic") != NULL )
		{

			myAppModes.setNetworkMode (NETWORK_ACTION_CONNECT);
			myAppModes.setPlayMode (PLAY_ACTION_PLAY);
			message = __func__;
			message.append(": Play Automatic Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"playmanual") != NULL )
		{

			message = __func__;
			message.append(": Play Manual Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"stop") != NULL )
		{
			myAppModes.setPlayMode (PLAY_ACTION_STOP);
			message = __func__;
			message.append(": Play Stop Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"play") != NULL )
		{
			myAppModes.setPlayMode (PLAY_ACTION_PLAY);
			myAppModes.setNetworkMode (NETWORK_ACTION_CONNECT);

			message = __func__;
			message.append(": Play Play Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"exit") != NULL )
		{
			myAppModes.setPlayMode (PLAY_ACTION_QUIT);

			message = __func__;
			message.append(": Exit Signal Received");
			myLog.print(logWarning, message);
		}

		else if( (ps = strstr(buffer,"volume")) != NULL )
		{
			iVolume = atoi(ps+7);
			message = __func__;
			message.append(": Volume Signal Received with a value of:");
			message.append(ps+7);
			myLog.print(logWarning, message);

		}
		else if(strstr(buffer,"nextalbum") != NULL )
		{
			myAppModes.setPlayMode (PLAY_ACTION_NEXTALBUM);

			message = __func__;
			message.append(": Next Album Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"nextsong") != NULL ) // next is for going to the next song via the web site... without finishing the song.
		{
			myAppModes.setPlayMode (PLAY_ACTION_NEXTSONG);
			returnValue = PLAY_ACTION_NEXTSONG;

			message = __func__;
			message.append(": Next Song Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"next") != NULL )// next is for going to the next song naturally.
		{
			myAppModes.setPlayMode (PLAY_ACTION_NEXTSONG);
			returnValue = PLAY_ACTION_NEXTSONG;
			message = __func__;
			message.append(": Next (Song) Signal Received");
			myLog.print(logWarning, message);
		}

		else if(strstr(buffer,"update") != NULL )
		{
//			playMode = PLAY_ACTION_UPDATE;
		}
	}
	usleep(1000); // let other processes have the CPU for 1000 microseconds

	return (returnValue);
}
