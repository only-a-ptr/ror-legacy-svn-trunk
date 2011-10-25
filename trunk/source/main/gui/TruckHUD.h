/*
This source file is part of Rigs of Rods
Copyright 2005-2011 Pierre-Michel Ricordel
Copyright 2007-2011 Thomas Fischer

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
#ifndef __TRUCKHUD_H__
#define __TRUCKHUD_H__

#include "RoRPrerequisites.h"

#define COMMANDS_VISIBLE 25

#include "Beam.h"
#include "OgreLineStreamOverlayElement.h"

class TruckHUD
{
public:
	TruckHUD();
	~TruckHUD();
	bool update(float dt, Beam *truck, Ogre::SceneManager *sm, Ogre::Camera* mCamera, Ogre::RenderWindow* mWindow, bool visible=true);
	void show(bool value);
	bool isVisible();
  
protected:
    static TruckHUD *myInstance;
	std::map<int, float> maxVelos;
	std::map<int, float> minVelos;
	std::map<int, float> avVelos;
	std::map<int, float> maxPosVerG;
	std::map<int, float> maxNegVerG;
	std::map<int, float> maxPosSagG;
	std::map<int, float> maxNegSagG;
	std::map<int, float> maxPosLatG;
	std::map<int, float> maxNegLatG;
	Ogre::Overlay *truckHUD;
	float updatetime;
	int width, border;
	void checkOverflow(Ogre::OverlayElement* e);
};


#endif
