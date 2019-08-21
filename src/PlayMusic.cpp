//============================================================================
// Name        : PlayMusic.cpp
// Author      : Jon Dellaria
// Version     :
// Copyright   : Your copyright notice

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

#include "AudioStream.h"

#define USE_SOUND_CARD

#ifdef USE_SOUND_CARD
#include <alsa/asoundlib.h>

#define PCM_DEVICE "default"
snd_pcm_t *pcm_handle;
snd_pcm_uframes_t frames;
__u8 *buff;
unsigned int pcm;
int buff_size, loops;
int rate, channels, seconds;
unsigned int tmp, dir;
snd_pcm_hw_params_t *params;
#endif


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

//MP3Stream mp3Stream;
AudioStream auds;

int PlaySong(string audioFileName, data_type_t adt);
int PlaySongS(string audioFileName, data_type_t adt);
snd_pcm_t * OpenSound(unsigned char * buff, snd_pcm_uframes_t frames);


int main(int argc, char* const argv[])
{
	int returnValue;
	string message;
	char ibuffer [33];
	struct playQRecord pQR;

	pQR.id = 0;

	if (argc == 2) // if there is an argument, then assume it is the configuration file
	{
		myConfig.getConfiguration(argv[1]);
	}
	else //otherwise assume there is a file in the default with the name "config.conf"
	{
		myConfig.getConfiguration("config.xml");
	}
	sleep (5); // wait for 5 seconds to make sure database and network connections are up and stable.

	configApp();

	dataGramServer.Start(DATA_GRAM_SERVER_PORT);

	HttpServer server;
	server.config.port =  HTTP_SERVER_PORT;

	server.resource["^/status"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request)
	{
	stringstream stream;
//	string json_string = "{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";
	string json_status_string;
	struct playQRecord pQRCurrentSong;
	pQRCurrentSong = getCurrentSongInPlayQ();

	stream << "{\n";
	stream << "exitMode: " << myAppModes.getPlayMode() << ",\n";
	stream << "playMode: " << myAppModes.getPlayMode() << ",\n";
	stream << "networkMode: " << myAppModes.getNetworkMode() << ",\n";
	if (pQRCurrentSong.id == 0)
	{
		stream << "Current Song: No Song Currently Playing\n";
	}
	else
	{
		stream << "Current Track: " << pQRCurrentSong.tracknumber << ",\n";
		stream << "Current Song: " << pQRCurrentSong.name << ",\n";
		stream << "Current Artist: " << pQRCurrentSong.artist << ",\n";
		stream << "Current Album: " << pQRCurrentSong.album << ",\n";
		stream << "Current Song Year: " << pQRCurrentSong.songyear << "\n";
	}

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
//	myAppModes.setContinuous();
//	myAppModes.setManual();
//	myAppModes.setNetworkMode(NETWORK_ACTION_DISCONNECT);
	message = "airportAddress is: ";
	message.append(myConfig.airportAddress + "\n");
	myLog.print(logWarning, message);

	OpenDBConnection();
//	playAutomatic = 1;

int tries = 0;

	while ((myAppModes.getPlayMode() != PLAY_ACTION_QUIT))
	{

		message = "PlayMusic.cpp :";
		message.append(__func__);
		message.append(": Wile Loop - ");
//		message.append("networkMode = ");
//		sprintf(ibuffer, "%d", myAppModes.getNetworkMode());
//		message.append(ibuffer);
		sprintf(ibuffer, "%d", myAppModes.getPlayMode());
		message.append("playMode = ");
		message.append(ibuffer);
		myLog.print(logDebug, message);
		message = "PlayMusic.cpp :";
		message.append(__func__);
		message.append(": playMode == PLAY_ACTION_PLAY getting next song record"); //Jon
		myLog.print(logDebug, message);
		if (myAppModes.getApplicationMode() != APPLICATION_ACTION_CONTINUOUS)
		{
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": play mode is manual wait a few seconds"); //Jon
			myLog.print(logDebug, message);
			sleep (5);
		}
		pQR = getNextSongRecord();
		if (pQR.id != 0)
		{
			if (asciiToUtf8(pQR.location, 255) != 1)
			{
				message = "PlayMusic.cpp :";
				message.append(__func__);
				message.append(":  UTF Conversion error FileName - ");
				message.append(pQR.location);
				myLog.print(logError, message);
			}
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": PlaySong FileName - ");
			message.append(pQR.location);
			myLog.print(logInformation, message);
			songFD = PlaySong(pQR.location, AUD_TYPE_NONE);
			if (songFD == 0)
			{
				tries++;
			}
			else
			{
				tries = 0;
			}
			if (tries > 15)
			{
				myLog << "PlayMusic.cpp :" << __func__ << " Exiting becuase of fatal error" << DLogError;
				myAppModes.setPlayMode(PLAY_ACTION_QUIT);
			}
		}
		else
		{
			myAppModes.setPlayMode (PLAY_ACTION_STOP);
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(":  No song to play");
			myLog.print(logDebug, message);
		}
		returnValue = eventHandler();
		if ((myAppModes.getPlayMode() == PLAY_ACTION_EXIT))
			break;
	}
	message.append(": Closing all connections and waiting for 5 seconds.");
	myLog.print(logWarning, message);
	CloseDBConnection();
	sleep(5);


	server_thread.detach();
	server.stop();
	dataGramServer.Close();
	message = "PlayMusic.cpp :";
	message.append(__func__);
	message.append(": AirportTalk exiting Normally");
	myLog.print(logWarning, message);
}

int PlaySong(string audioFileName, data_type_t adt)
{
	string message;
	int bytesRead = 0;
	int returnValue;

	myAppModes.setPlayMode (PLAY_ACTION_PLAY);

	songFD = auds.Open(audioFileName,adt);
	message = "PlayMusic.cpp :";
	message.append(__func__);
	message.append(": opening file:");
	message.append(audioFileName);
	myLog.print(logDebug, message);
	if (songFD == 0) // error occurred
	{
		message = "PlayMusic.cpp :";
		message.append(__func__);
		message.append(": opening file:");
		message.append(audioFileName);
		myLog.print(logError, message);
		return (0);
	}
	while(myAppModes.getPlayMode() == PLAY_ACTION_PLAY )
	{
		message = "PlayMusic.cpp :";
		message.append(__func__);
		message.append(": playMode == PLAY_ACTION_PLAY");
//		myLog.print(logDebug, message);
		bytesRead = auds.GetNextSample(); // this should get PCM data and put all the info in the right place to send to ALSA Driver
		if ( bytesRead == 0)

		{
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(" end of Audio file. No bytes read.");
			myLog.print(logDebug, message);
			break;
		}
		else
		{
			auds.SendPCMToALSADriver();
		}
		returnValue = eventHandler();
	}
	auds.Close();
	return 0;
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
	if (myConfig.playContinuous == true)
	{
		myAppModes.setContinuous();
		message = "Continuous Play Mode";
		myLog.print(logInformation, message);
	}
	else
	{
		myAppModes.setManual();
		message = "Manual Play Mode";
		myLog.print(logInformation, message);
	}
	return (1);
}

int eventHandler()
{
	int n;
	long iVolume;
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
			returnValue = PLAY_ACTION_QUIT;
			myAppModes.setPlayMode (PLAY_ACTION_QUIT);
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Play Quit Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"pause") != NULL )
		{
			returnValue = PLAY_ACTION_PAUSE;
			myAppModes.setPlayMode (PLAY_ACTION_PAUSE);
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Play Pause Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"playautomatic") != NULL )
		{

//			myAppModes.setNetworkMode (NETWORK_ACTION_CONNECT);
			myAppModes.setContinuous();
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Play Automatic Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"playmanual") != NULL )
		{
			myAppModes.setManual();
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Play Manual Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"stop") != NULL )
		{
			returnValue = PLAY_ACTION_STOP;
			myAppModes.setPlayMode (PLAY_ACTION_STOP);
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Play Stop Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"play") != NULL )
		{
			returnValue = PLAY_ACTION_PLAY;
			myAppModes.setPlayMode (PLAY_ACTION_PLAY);
//			myAppModes.setNetworkMode (NETWORK_ACTION_CONNECT);
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Play Play Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"exit") != NULL )
		{
			returnValue = PLAY_ACTION_QUIT;
			myAppModes.setPlayMode (PLAY_ACTION_QUIT);
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Exit Signal Received");
			myLog.print(logWarning, message);
		}

		else if( (ps = strstr(buffer,"volume")) != NULL )
		{
			iVolume = atoi(ps+7);
			auds.SetAlsaMasterVolume(iVolume); //between 0 and 100
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Volume Signal Received with a value of:");
			message.append(ps+7);
			myLog.print(logWarning, message);

		}
		else if(strstr(buffer,"nextalbum") != NULL )
		{
			returnValue = PLAY_ACTION_NEXTALBUM;
			myAppModes.setPlayMode (PLAY_ACTION_NEXTALBUM);
			skipToNextAlbum();
			auds.mp3Stream.StopDecoder();
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Next Album Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"nextsong") != NULL ) // next is for going to the next song via the web site... without finishing the song.
		{
			returnValue = PLAY_ACTION_NEXTSONG;
			myAppModes.setPlayMode (PLAY_ACTION_NEXTSONG);
			auds.mp3Stream.StopDecoder();
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Next Song Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"next") != NULL )// next is for going to the next song naturally.
		{
			returnValue = PLAY_ACTION_NEXTSONG;
			myAppModes.setPlayMode (PLAY_ACTION_NEXTSONG);
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(": Next (Song) Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"debug") != NULL )// next is for going to the next song naturally.
		{
			myLog.logValue = logDebug;
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(" myLog.logValue = logDebug");
			myLog.print(logInformation, message);
		}
		else if(strstr(buffer,"info") != NULL )// next is for going to the next song naturally.
		{
			myLog.logValue = logInformation;
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(" myLog.logValue = logInformation");
			myLog.print(logInformation, message);
		}
		else if(strstr(buffer,"warn") != NULL )// next is for going to the next song naturally.
		{
			myLog.logValue = logWarning;
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(" myLog.logValue = logWarning");
			myLog.print(logInformation, message);
		}
		else if(strstr(buffer,"warn") != NULL )// next is for going to the next song naturally.
		{
			myLog.logValue = logError;
			message = "PlayMusic.cpp :";
			message.append(__func__);
			message.append(" myLog.logValue = logError");
			myLog.print(logInformation, message);
		}
		else if(strstr(buffer,"update") != NULL )
		{
//			playMode = PLAY_ACTION_UPDATE;
		}
	}
	usleep(100); // let other processes have the CPU for 100 microseconds

	return (returnValue);
}
