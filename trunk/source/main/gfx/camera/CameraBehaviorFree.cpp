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
#include "CameraBehaviorFree.h"

#include "Console.h"
#include "InputEngine.h"
#include "language.h"
#include "Ogre.h"

using namespace Ogre;

void CameraBehaviorFree::update(const CameraManager::cameraContext_t &ctx)
{
	Vector3 mTranslateVector = Vector3::ZERO;
	Degree mRotX(0.0f);
	Degree mRotY(0.0f);

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_SIDESTEP_LEFT))
		mTranslateVector.x -= ctx.mTransScale;	// Move camera left

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_SIDESTEP_RIGHT))
		mTranslateVector.x += ctx.mTransScale;	// Move camera RIGHT

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_FORWARD))
		mTranslateVector.z -= ctx.mTransScale;	// Move camera forward

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_BACKWARDS))
		mTranslateVector.z += ctx.mTransScale;	// Move camera backward

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_ROT_UP))
		mRotY += ctx.mRotScale;

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_ROT_DOWN))
		mRotY -= ctx.mRotScale;

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_UP))
		mTranslateVector.y += ctx.mTransScale;	// Move camera up

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_DOWN))
		mTranslateVector.y -= ctx.mTransScale;	// Move camera down

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_RIGHT))
		mRotX -= ctx.mRotScale;

	if(INPUTENGINE.getEventBoolValue(EV_CHARACTER_LEFT))
		mRotX += ctx.mRotScale;

	ctx.mCamera->yaw(mRotX);
	ctx.mCamera->pitch(mRotY);

	Vector3 trans = ctx.mCamera->getOrientation() * mTranslateVector;
	ctx.mCamera->setPosition(ctx.mCamera->getPosition() + trans);
}

void CameraBehaviorFree::activate(const CameraManager::cameraContext_t &ctx)
{
	ctx.mCamera->setFixedYawAxis(true, Vector3::UNIT_Y);

	CONSOLE_PUTMESSAGE(Console::CONSOLE_MSGTYPE_INFO, Console::CONSOLE_SYSTEM_NOTICE, _L("free camera"), "camera_go.png", 3000, false);

#ifdef USE_MYGUI
	MyGUI::PointerManager::getInstance().setVisible(false);
#endif // USE_MYGUI
}

void CameraBehaviorFree::deactivate(const CameraManager::cameraContext_t &ctx)
{
	ctx.mCamera->setFixedYawAxis(false);

	CONSOLE_PUTMESSAGE(Console::CONSOLE_MSGTYPE_INFO, Console::CONSOLE_SYSTEM_NOTICE, _L("normal camera"), "camera.png", 3000, false);

#ifdef USE_MYGUI
	MyGUI::PointerManager::getInstance().setVisible(true);
#endif // USE_MYGUI
}

bool CameraBehaviorFree::mouseMoved(const CameraManager::cameraContext_t &ctx, const OIS::MouseEvent& _arg)
{
	const OIS::MouseState ms = _arg.state;

	ctx.mCamera->yaw(Degree(-(float)ms.X.rel * 0.13f));
	ctx.mCamera->pitch(Degree(-(float)ms.Y.rel * 0.13f));

	return true;
}
