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

#include "RoREditor.h"

#include "Settings.h"
#include "errorutils.h"
#include "RigsOfRods.h"
#include "errorutils.h"

#include "AdvancedOgreFramework.h"

RoREditor::RoREditor(string _meshPath)
{
	initialized=false;
	timeSinceLastFrame = 1;
	startTime = 0;
}

RoREditor::~RoREditor(void)
{
}

bool RoREditor::Initialize(std::string hwndStr, std::string mainhwndStr)
{
	if(!SETTINGS.setupPaths())
		return false;
	
	SETTINGS.setSetting("Preselected Truck", "agoras.truck");
	SETTINGS.setSetting("Preselected Map",   "simple.terrn");

	//printf("#>>>>>>> %s # %s\n", hwndStr.c_str(), mainhwndStr.c_str());
	app = new RigsOfRods("RoREditor", hwndStr, mainhwndStr);

	app->go();

	return true;
}

void RoREditor::Deinitialize(void)
{
	if (!initialized) return;

	initialized = false;
}

void RoREditor::Update()
{
	startTime = OgreFramework::getSingletonPtr()->m_pTimer->getMillisecondsCPU();
	
	if(app)
		app->update(timeSinceLastFrame);

	timeSinceLastFrame = OgreFramework::getSingletonPtr()->m_pTimer->getMillisecondsCPU() - startTime;
}
