/*
This source file is part of Rigs of Rods
Copyright 2005-2012 Pierre-Michel Ricordel
Copyright 2007-2012 Thomas Fischer

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

#pragma once

#ifdef USE_OPENAL
#ifndef __Sound_H_
#define __Sound_H_

#include "RoRPrerequisites.h"
#include "Ogre.h"
#include <AL/al.h>


class Sound
{
	friend class SoundManager;

public:
	Sound(ALuint buffer, SoundManager* sound_mgr, int source_index);

	void setPitch(float pitch);
	void setGain(float gain);
	void setPosition(Ogre::Vector3 pos);
	void setVelocity(Ogre::Vector3 vel);
	void setLoop(bool loop);
	void setEnabled(bool e);
	void play();
	void stop();

	bool getEnabled();
	bool isPlaying();

	enum { REASON_PLAY, REASON_STOP, REASON_GAIN, REASON_LOOP, REASON_PTCH, REASON_POSN, REASON_VLCT }; 

private:
	void computeAudibility(Ogre::Vector3 pos);

	float audibility;
	float gain;
	float pitch;
	bool loop;
	bool enabled;
	bool should_play;

	// This value is changed dynamically, depending on whether the input is played or not.
	int hardware_index;
	ALuint buffer;
	
	Ogre::Vector3 position;
	Ogre::Vector3 velocity;

	SoundManager* sound_mgr;
	// Must not be changed during the lifetime of this object
	int source_index;
};

#endif // __Sound_H_
#endif // USE_OPENAL
