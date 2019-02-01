/*
 * AudioStream.cpp
 *
 *  Created on: Feb 17, 2010
 *      Author: jdellaria
 */

#include "AudioStream.h"
#include "APMusic.h"
#include "MP3Stream.h"
#include <DLog.h>

extern DLog myLog;

AudioStream::AudioStream() {
	stream = 0;
	sample_rate = DEFAULT_SAMPLE_RATE;
	data_type_t data_type;
	channels = 2;
}

AudioStream::~AudioStream() {
	// TODO Auto-generated destructor stub
}
//static data_type_t get_data_type(const char *fname);


int AudioStream::Open(string audioFileName, data_type_t adt)
{
	int rval=-1;
	int err;
	string message;

//	channels=2; //default is stereo
	OpenALSADriver(2, 44100); // channes = 2 sample rate = 44100
	if(adt==AUD_TYPE_NONE)
	{
		message = "AudioStream.cpp :";
		message.append(__func__);
		message.append(": GetDataType");
		myLog.print(logDebug, message);

		data_type=GetDataType(audioFileName.c_str());
	}
	else
	{
		data_type=adt;
	}
	switch(data_type){

	case AUD_TYPE_URLMP3:
	case AUD_TYPE_MP3:
		message = "AudioStream.cpp :";
		message.append(__func__);
		message.append(": mp3Stream.Open");
		myLog.print(logDebug, message);
		rval = mp3Stream.Open(audioFileName);
		break;
	case AUD_TYPE_NONE:
		message = "AudioStream.cpp :";
		message.append(__func__);
		message.append(" unknown audio data type");
		myLog.print(logError, message);
		break;
	}
	if (rval == 0) //Error occurred Opening file
	{
		message = "AudioStream.cpp :";
		message.append(__func__);
		message.append(": error");
		myLog.print(logError, message);
		Close();
		return 0;
	}
	return rval;
}

int AudioStream::Close()
{
	string message;

	switch(data_type)
	{
		case AUD_TYPE_URLMP3:
		case AUD_TYPE_MP3:
			message = "AudioStream.cpp :";
			message.append(__func__);
			message.append(": mp3Stream.Close");
			myLog.print(logDebug, message);
			mp3Stream.Close();
			break;
		case AUD_TYPE_NONE:
			message = "AudioStream.cpp :";
			message.append(__func__);
			message.append(": ### shouldn't come here");
			myLog.print(logError, message);
			break;
	}
	CloseALSADriver();
	return 0;
}

int AudioStream::CloseALSADriver()
{
	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
	free(buff);
}

snd_pcm_t * AudioStream::OpenALSADriver(int numberOfChannels, int samplingRate)
{
	unsigned int pcm, tmp;
	int dir;
	snd_pcm_hw_params_t *params;

	string message;
	char ibuffer [33];

	sample_rate = samplingRate;
	channels = numberOfChannels;

	/* Open the PCM device in playback mode */
	if (pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0) < 0)
	{
		message = __func__;
		message.append(" Can't open \"");
		message.append(PCM_DEVICE);
		message.append("\"PCM device. ");
		message.append(snd_strerror(pcm));
		myLog.print(logError, message);
	}

	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle, params);

	/* Set parameters */
	if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
	{
		message = __func__;
		message.append(" Can't set interleaved mode. ");
		message.append(snd_strerror(pcm));
		myLog.print(logError, message);
	}

	if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE) < 0)
	{
		message = __func__;
		message.append(" Can't set format. ");
		message.append(snd_strerror(pcm));
		myLog.print(logError, message);
	}

	if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0)
	{
		message = __func__;
		message.append(" Can't set channels number.  ");
		message.append(snd_strerror(pcm));
		myLog.print(logError, message);
	}

	if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &sample_rate, &dir) < 0)
	{
		message = __func__;
		message.append(" Can't set rate. ");
		message.append(snd_strerror(pcm));
		myLog.print(logError, message);
	}

	/* Set period size to 4096 frames. */
	frames = 1024;
	snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &frames, &dir);

	/* Write parameters */
	if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
	{
		message = __func__;
		message.append(" Can't set harware parameters. ");
		message.append(snd_strerror(pcm));
		myLog.print(logError, message);
	}
	/* Resume information */
	message = __func__;
	message.append(" PCM name: ");
	message.append(snd_pcm_name(pcm_handle));
	myLog.print(logInformation, message);

	message = __func__;
	message.append(" PCM state: ");
	message.append(snd_pcm_state_name(snd_pcm_state(pcm_handle)));
	myLog.print(logInformation, message);

	snd_pcm_hw_params_get_channels(params, &tmp);

	message = __func__;
	message.append(" channels: ");
	sprintf(ibuffer, "%d", tmp);
	message.append(ibuffer);
	myLog.print(logInformation, message);

	if (tmp == 1)
	{
		message = __func__;
		message.append(" (mono)");
		myLog.print(logInformation, message);
	}

	else if (tmp == 2)
	{
		message = __func__;
		message.append(" (stereo)");
		myLog.print(logInformation, message);
	}

	snd_pcm_hw_params_get_rate(params, &tmp, &dir);

	message = __func__;
	message.append(" rate: ");
	sprintf(ibuffer, "%d", tmp);
	message.append(ibuffer);
	myLog.print(logInformation, message);

	/* Allocate buffer to hold single period */
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);

	chunk_size = frames * channels * 2 /* 2 -> sample size */;
	buff = (unsigned char *) malloc(chunk_size);

	message = __func__;
	message.append(" frames: ");
	sprintf(ibuffer, "%d", frames);
	message.append(ibuffer);
	myLog.print(logInformation, message);

	message = __func__;
	message.append(" buff_size: ");
	sprintf(ibuffer, "%d", chunk_size);
	message.append(ibuffer);
	myLog.print(logInformation, message);

	snd_pcm_hw_params_get_period_time(params, &tmp, NULL);
	message = __func__;
	message.append(" period_time: ");
	sprintf(ibuffer, "%d", tmp);
	message.append(ibuffer);
	myLog.print(logInformation, message);

	return (pcm_handle);
}


int AudioStream::GetNextSample( __u8 **data, int *size)
{
	int rval;
	string message;

	switch(data_type){
	case AUD_TYPE_URLMP3:
	case AUD_TYPE_MP3:
//		message = "AudioStream.cpp :";
//		message.append(__func__);
//		message.append(": ");
//		myLog.print(logDebug, message);
		rval=mp3Stream.GetNextSample( data, size); // returns the file handle for mp3 audio stream
		break;
	case AUD_TYPE_NONE:
		message = "AudioStream.cpp :";
		message.append(__func__);
		message.append(": ### shouldn't come here");
		myLog.print(logError, message);
		return -1;
	}
	return rval;
}

int AudioStream::WritePCM( __u8 *buffer, __u8 **data, int *size, int bsize, data_source_t *ds)
{
	int bytesRead = 0;
	bytesRead = read(ds->u.fd, buffer, (size_t) &size);
	if (bytesRead == 0) {
		printf("error.\n");
			return 0;
	}
	return (bytesRead);

	__u8 one[4];
	int count=0;
	int bpos=0;
	__u8 *bp=buffer;
	int i,nodata=0;
	__s16 *resamp=NULL, *pr=NULL;

	bits_write(&bp,1,3,&bpos); // channel=1, stereo
	bits_write(&bp,0,4,&bpos); // unknown
	bits_write(&bp,0,8,&bpos); // unknown
	bits_write(&bp,0,4,&bpos); // unknown
	if(bsize!=4096)
		bits_write(&bp,1,1,&bpos); // hassize
	else
		bits_write(&bp,0,1,&bpos); // hassize
	bits_write(&bp,0,2,&bpos); // unused
	bits_write(&bp,1,1,&bpos); // is-not-compressed
	if(bsize!=4096){
		bits_write(&bp,(bsize>>24)&0xff,8,&bpos); // size of data, integer, big endian
		bits_write(&bp,(bsize>>16)&0xff,8,&bpos);
		bits_write(&bp,(bsize>>8)&0xff,8,&bpos);
		bits_write(&bp,bsize&0xff,8,&bpos);
	}
	while(1){
		if(pr){
			if(channels==1)
				*((__s16*)one)=*pr;
			else
				*((__s16*)one)=*pr++;
			*((__s16*)one+1)=*pr++;
		}else {
			switch(ds->type){
			case DESCRIPTOR:
				if(channels==1){
					if(read(ds->u.fd, one, 2)!=2) nodata=1;
					*((__s16*)one+1)=*((__s16*)one);
				}else{
					if(read(ds->u.fd, one, 4)!=4) nodata=1;
				}
				break;
			case STREAM:
				if(channels==1){
					if(fread(one,1,2,ds->u.inf)!=2) nodata=1;
					*((__s16*)one+1)=*((__s16*)one);
				}else{
					if(fread(one,1,4,ds->u.inf)!=4) nodata=1;
				}
				break;
			case MEMORY:
				if(channels==1){
					if(ds->u.mem.size<=count*2) nodata=1;
					*((__s16*)one)=ds->u.mem.data[count];
					*((__s16*)one+1)=*((__s16*)one);
				}else{
					if(ds->u.mem.size<=count*4) nodata=1;
					*((__s16*)one)=ds->u.mem.data[count*2];
					*((__s16*)one+1)=ds->u.mem.data[count*2+1];
				}
				break;
			}
		}
		if(nodata) break;

		bits_write(&bp,one[1],8,&bpos);
		bits_write(&bp,one[0],8,&bpos);
		bits_write(&bp,one[3],8,&bpos);
		bits_write(&bp,one[2],8,&bpos);
		if(++count==bsize) break;
	}
	if(!count){
		*size=bp-buffer;
		if(bpos) *size+=1;
		*data=buffer;
		return -1; // when no data at all, it should stop playing
	}
	/* when readable size is less than bsize, fill 0 at the bottom */
	for(i=0;i<(bsize-count)*4;i++){
		bits_write(&bp,0,8,&bpos);
	}
	*size=bp-buffer;
	if(bpos) *size+=1;
	*data=buffer;
	return 0;
}

int AudioStream::ClacChunkSize(int sample_rate)
{
	int bsize=MAX_SAMPLES_IN_CHUNK;
	int ratio=DEFAULT_SAMPLE_RATE*100/sample_rate;
	// to make suer the resampled size is <= 4096
	if(ratio>100) bsize=bsize*100/ratio-1;
	return bsize;
}

data_type_t AudioStream::GetDataType(const char *fname)
{
	int i;
	for(i=strlen(fname)-1;i>=0;i--)
		if(fname[i]=='.') break;
	if(i<0) return AUD_TYPE_PCM;
	if(i>=strlen(fname)-1) return AUD_TYPE_NONE;
	if(!strcasecmp(fname+i+1,"mp3")) return AUD_TYPE_MP3;
	if(strstr(fname,"http")==fname) return AUD_TYPE_URLMP3;
	return AUD_TYPE_NONE;
}
