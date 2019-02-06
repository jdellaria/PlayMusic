/*
 * MP3Stream.cpp
 *
 *  Created on: Feb 24, 2010
 *      Author: jdellaria
 */

//#include "MP3Stream.h"
#include "AudioStream.h"
#include "APMusic.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <DLog.h>

extern DLog myLog;
extern AudioStream auds;

MP3Stream::MP3Stream() {
	// TODO Auto-generated constructor stub

}

MP3Stream::~MP3Stream() {
	// TODO Auto-generated destructor stub
}


//int MP3Stream::Open(AudioStream *auds, string audioFileName)
int MP3Stream::Open(string audioFileName)

{
	int returnValue;
	string message;

	message = "MP3Stream.cpp :";
	message.append(__func__);
	message.append(": startPlay");
	myLog.print(logDebug, message);

	mp3FileName=audioFileName;
	auds.sample_rate=DEFAULT_SAMPLE_RATE;
	auds.data_type = AUD_TYPE_MP3;
	returnValue = startPlay(audioFileName);
	if (returnValue == 0)
	{
		Close();
		return 0;
	}
	auds.chunk_size=auds.ClacChunkSize(auds.sample_rate);
	return returnValue;
}

int MP3Stream::Close()
{
	string message;

	message = "MP3Stream.cpp :";
	message.append(__func__);
	message.append(": StopDecoder");
	myLog.print(logDebug, message);

//	printf("MP3Stream::Close Decoder programID = %d\n",programID);
	if(dfd>=0) close(dfd);
	StopDecoder();
	return 0;
}

int MP3Stream::GetNextSample( __u8 *data, int size)
{
	data_source_t ds;
	int bytesRead;
	ds.type = DESCRIPTOR;
	ds.u.fd = dfd = fdin;

	return auds.WritePCM(data, size, &ds);

}

int MP3Stream::startPlay(string fileName)
{
	string message;
	char ibuffer [33];
	char *darg[7]={MP3PLAYER,"-s","-r", "44100", "--stereo", NULL, NULL}; // this forces play at a rate of 44100 and in stereo
//	char *darg[7]={MP3PLAYER, NULL, NULL}; // this forces play at a rate of 44100 and in stereo

	char* name;

	dpid = 0 ;
	fdin = 0;
	name = new char[fileName.length() + 1]; //converting string to char*
	strcpy(name, fileName.c_str());

//	darg[1] = name;
	darg[5] = name;

	message = __func__;
	message.append(": Playing Song: ");
	message.append(fileName);
	myLog.print(logInformation, message);

	sleep(2);

	dpid=ExecuteDecoder(darg,&fdin,NULL,NULL);
	if (dpid == 0) // ExecuteDecoder could not execte the commange and we have an error
	{
		message = __func__;
		message.append(": ExecuteDecoder returned an error. errno = ");
		sprintf(ibuffer, "%d", errno);
		message.append(ibuffer);
		myLog.print(logError, message);
		return(0);
	}

	dfd = fdin; //JON

	message = __func__;
	message.append(": file descriptor = ");
	sprintf(ibuffer, "%d", dpid);
	message.append(ibuffer);
	myLog.print(logDebug, message);
	return(fdin);
}

int MP3Stream::ExecuteDecoder(char* const argv[], int *infd, int *outfd, int *errfd)
{
	/*main invoke a new child process
	 argv is an array exactly like the array passed to the main routine.
	 This should contain the following in order:

	 1.command to execute at the command line. Example, MPG123
	 2.any parameters. (-s)
	 3.name of music file
	 */
	string message;
	char ibuffer [33];
	int infds[2];
	int errfds[2];
	int outfds[2];
	int pid;
	pipe(infds);
	pipe(outfds);
	pipe(errfds);
	pid=fork();
	if(pid==0)
	{
		// this is a child
		if(infd){
			dup2(infds[1],1);// copy output pipe to standard output
		}else{
			close(infds[1]);
		}
		close(infds[0]);

		if(errfd){
			dup2(errfds[1],2);// copy output pipe to standard error
		}else{
			close(errfds[1]);
		}
		close(infds[0]);

		if(outfd){
			dup2(outfds[0],0);// copy input pipe to standard input
		}else{
			close(outfds[0]);
		}
		close(outfds[1]);

		execvp(argv[0], argv); // If this call returns, then there was an error
		message = __func__;
		message.append(": execvp returned an error. errno = ");
		sprintf(ibuffer, "%d", errno);
		message.append(ibuffer);
		myLog.print(logError, message);
		return (0);
	}

	if(infd)
	{
		*infd=infds[0];
		inputFileDescriptor = infds[0];
//		CurrentlyPlaying <<
	}
	else
		close(infds[0]);
	close(infds[1]);//close output pipe

	if(errfd)
	{
		*errfd=errfds[0];
		errorFileDescriptor = errfds[0];
	}
	else
		close(errfds[0]);
	close(errfds[1]);//close output pipe

	if(outfd)
	{
		*outfd=outfds[1];
		outputFileDescriptor=outfds[1];
	}
	else
		close(outfds[1]);
	close(outfds[0]);// close read pipe
	programID = pid;
//	printf("MP3Stream::ExecuteDecoder Decoder programID = %d\n",programID);

	readFlag = 1;
	return pid;
}

int MP3Stream::StopDecoder()
{
	int status;

//	printf("MP3Stream::StopDecoder Decoder programID = %d\n",programID);
	if(!programID) return 0;
	kill(programID,SIGTERM);
	usleep(1000);
	WaitForDecoderToExit();

	return 0;
}

int MP3Stream::WaitForDecoderToExit()
{
	int status;
	string message;

	if(!programID) return 0;
	waitpid (programID, &status, 0); // performing a wait allows the system to release the resources associated with the child; if a wait is not performed, then terminated the child remains in a "zombie" state
	programID = 0;
	if (WIFEXITED(status) )
	{
//		printf("Audio::StopDecoder Decoder exited normally\n");
	}
	else
	{
		message = "MP3Stream.cpp ";
		message.append(__func__);
		message.append(": Decoder did NOT exited normally");
		myLog.print(logWarning, message);
	}
	if (WIFSIGNALED(status) )
	{
		message = "MP3Stream.cpp ";
		message.append(__func__);
		message.append(": Decoder exited by an external Signal");
		myLog.print(logWarning, message);
	}
	return 0;
}
