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

// created by Thomas Fischer thomas{AT}thomasfischer{DOT}biz, 7th of August 2009

#include "NetworkStreamManager.h"
#include "Streamable.h"

#include "Ogre.h"
#include "network.h"
#include "utils.h"
#include "sha1.h"

using namespace Ogre;

NetworkStreamManager::NetworkStreamManager()
{
	streamid=10;
	pthread_mutex_init(&send_work_mutex, NULL);
	pthread_cond_init(&send_work_cv, NULL);
}

NetworkStreamManager::~NetworkStreamManager()
{
}

template<> NetworkStreamManager * Singleton< NetworkStreamManager >::ms_Singleton = 0;
NetworkStreamManager* NetworkStreamManager::getSingletonPtr(void)
{
	return ms_Singleton;
}
NetworkStreamManager& NetworkStreamManager::getSingleton(void)
{
	assert( ms_Singleton );  return ( *ms_Singleton );
}

void NetworkStreamManager::addStream(Streamable *stream, int osource, int ostreamid)
{
	if(ostreamid==-1)
	{
		// for own streams: count stream id up ...
		osource = net->getUserID();
		stream->streamid = streamid;
		if(streams.find(osource) == streams.end())
			streams[osource] = std::map < unsigned int, Streamable *>();
		streams[osource][streamid] = stream;
		LogManager::getSingleton().logMessage("adding local stream: " + StringConverter::toString(osource) + ":"+ StringConverter::toString(streamid));
		streamid++;
	} else
	{
		stream->streamid = ostreamid;
		streams[osource][ostreamid] = stream;
		LogManager::getSingleton().logMessage("adding remote stream: " + StringConverter::toString(osource) + ":"+ StringConverter::toString(ostreamid));
	}
}

void NetworkStreamManager::removeStream(Streamable *stream)
{
}

void NetworkStreamManager::pauseStream(Streamable *stream)
{
}

void NetworkStreamManager::resumeStream(Streamable *stream)
{
}

void NetworkStreamManager::pushReceivedStreamMessage(unsigned int &type, int &source, unsigned int &streamid, unsigned int &wrotelen, char *buffer)
{
	if(streams.find(source) == streams.end())
		// no such stream?!
		return;
	if(streams.find(source)->second.find(streamid) == streams.find(source)->second.end())
		// no such stream?!
		return;

	streams[source][streamid]->receiveStreamData(type, source, streamid, buffer, wrotelen);
}

void NetworkStreamManager::triggerSend()
{
	pthread_mutex_lock(&send_work_mutex);
	pthread_cond_broadcast(&send_work_cv);
	pthread_mutex_unlock(&send_work_mutex);
}

void NetworkStreamManager::sendStreams(Network *net, SWInetSocket *socket)
{
	pthread_mutex_lock(&send_work_mutex);
	pthread_cond_wait(&send_work_cv, &send_work_mutex);
	pthread_mutex_unlock(&send_work_mutex);

	char *buffer = 0;
	int bufferSize=0;

	std::map < int, std::map < unsigned int, Streamable *> >::iterator it;
	for(it=streams.begin(); it!=streams.end(); it++)
	{
		std::map<unsigned int,Streamable *>::iterator it2;
		for(it2=it->second.begin(); it2!=it->second.end(); it2++)
		{
			std::queue <Streamable::bufferedPacket_t> *packets = it2->second->getPacketQueue();

			if(packets->empty())
				continue;

			// remove oldest packet in queue
			Streamable::bufferedPacket_t packet = packets->front();

			// handle always one packet
			int etype = net->sendMessageRaw(socket, packet.packetBuffer, packet.size);
		
			packets->pop();

			if (etype)
			{
				char emsg[256];
				sprintf(emsg, "Error %i while sending data packet", etype);
				net->netFatalError(emsg);
				return;
			}
		}
	}
}
