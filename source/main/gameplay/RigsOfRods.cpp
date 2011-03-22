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
#include "RigsOfRods.h"

#include <Ogre.h>

#include "GameState.h"
#include "Settings.h"
#include "ContentManager.h"

using namespace Ogre;

RigsOfRods::RigsOfRods(Ogre::String name, Ogre::String hwnd, Ogre::String mainhwnd, bool embedded) : 
	stateManager(0),
	hwnd(hwnd),
	mainhwnd(mainhwnd),
	name(name),
	embedded(embedded)
{
}

RigsOfRods::~RigsOfRods()
{
	delete stateManager;
    delete OgreFramework::getSingletonPtr();

}

void RigsOfRods::go(void)
{
	// init ogre
	new OgreFramework();
	if(!OgreFramework::getSingletonPtr()->initOgre(name, hwnd, mainhwnd, embedded))
		return;

	// then the base content setup
	new ContentManager();
	ContentManager::getSingleton().init();

	// now add the game states
	stateManager = new AppStateManager();

	GameState::create(stateManager,  "GameState");

	// select the first one
	if(embedded)
	{
		stateManager->changeAppState(stateManager->findByName("GameState"));
		LOG("Rigs of Rods initialized!");
	} else
	{
		LOG("Rigs of Rods main loop starting ...");
		stateManager->start(stateManager->findByName("GameState"));
	}
}

void RigsOfRods::update(double dt)
{
	stateManager->update(dt);
}