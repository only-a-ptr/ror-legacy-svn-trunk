/*
This source file is part of Rigs of Rods
Copyright 2005,2006,2007,2008,2009 Pierre-Michel Ricordel
Copyright 2007,2008,2009 Thomas Fischer

For more information, see http://www.rigsofrods.com/

Rigs of Rods is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License version 3, as 
published by the Free Software Foundation.

Rigs of Rods is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Rigs of Rods.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __Network_H__
#define __Network_H__
#include "Ogre.h"
using namespace Ogre;

#include "SocketW.h"
#include "rornet.h"
#include "pthread.h"
#include "Beam.h"
#include "engine.h"
#include "SoundScriptManager.h"
#include "networkinfo.h"

class ExampleFrameListener;

class Network
{
private:
	SWInetSocket socket;
	unsigned int myuid;
	int myauthlevel;
	pthread_t sendthread;
	pthread_t receivethread;
	pthread_t downloadthread;
	static Timer timer;
	int last_time;
	int speed_time;
	int speed_bytes_sent, speed_bytes_sent_tmp, speed_bytes_recv, speed_bytes_recv_tmp;
	char* send_buffer;
	int send_buffer_len;
	oob_t send_oob;
	pthread_mutex_t dl_data_mutex;
	pthread_mutex_t send_work_mutex;
	pthread_cond_t send_work_cv;
	client_t clients[MAX_PEERS];
	pthread_mutex_t clients_mutex;
	pthread_mutex_t chat_mutex;
	Beam** trucks;
	netlock_t netlock;
	std::string mySname;
	long mySport;
	char sendthreadstart_buffer[MAX_MESSAGE_LENGTH];
	pthread_mutex_t msgsend_mutex;
	ExampleFrameListener *mefl;
	char terrainName[255];
	bool requestTerrainName();
	Ogre::String nickname;
	int rconauthed;
	bool shutdown;
	client_info_on_join userdata;
	SoundScriptManager* ssm;
	Ogre::String getUserChatName(client_t *c);
	void calcSpeed();
	std::map<int, float> lagDataClients;
	std::map<Ogre::String, Ogre::String> downloadingMods;
	void updatePlayerList();
public:

	Network(Beam **btrucks, std::string servername, long sport, ExampleFrameListener *efl);
	~Network();

	// messaging functions
	int sendMessageRaw(SWInetSocket *socket, char *content, unsigned int msgsize);
	int sendmessage(SWInetSocket *socket, int type, unsigned int streamid, unsigned int len, char* content);
	int receivemessage(SWInetSocket *socket, header_t *header, char* content, unsigned int bufferlen);

	// methods
	bool connect();
	void disconnect();
	void netFatalError(String error, bool exit=true);

	void sendthreadstart();
	void receivethreadstart();

	char *getTerrainName() { return terrainName; };
	Ogre::String getNickname(bool colour=false);
	unsigned int getUserID() { return myuid; };
	static unsigned long getNetTime();
	client_t *getClientInfo(unsigned int uid);

	int getSpeedUp();
	int getSpeedDown();
};


#endif
