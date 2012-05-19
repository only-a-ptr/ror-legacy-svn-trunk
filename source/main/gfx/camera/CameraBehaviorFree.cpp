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
#include "heightfinder.h"
#include "InputEngine.h"
#include "language.h"
#include "mygui/BaseLayout.h"
#include "Ogre.h"

using namespace Ogre;

void CameraBehaviorFree::update(const CameraManager::cameraContext_t &ctx)
{
	Degree mRotX(0.0f);
	Degree mRotY(0.0f);
	Degree mRotScale(ctx.mRotScale * 0.5f);
	Vector3 mTrans(Vector3::ZERO);
	Real mTransScale(ctx.mTransScale * 0.5f);

	if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
	{
		mRotScale   *= 3.0f;
		mTransScale *= 3.0f;
	}
	if(INPUTENGINE.isKeyDown(OIS::KC_LCONTROL))
	{
		mRotScale   *= 30.0f;
		mTransScale *= 30.0f;
	}
	if(INPUTENGINE.isKeyDown(OIS::KC_LMENU))
	{
		mRotScale   *= 0.05f;
		mTransScale *= 0.05f;
	}

	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_SIDESTEP_LEFT) )
	{
		mTrans.x -= mTransScale;
	}
	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_SIDESTEP_RIGHT) )
	{
		mTrans.x += mTransScale;
	}
	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_FORWARD) )
	{
		mTrans.z -= mTransScale;
	}
	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_BACKWARDS) )
	{
		mTrans.z += mTransScale;
	}
	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_UP) )
	{
		mTrans.y += mTransScale;
	}
	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_DOWN) )
	{
		mTrans.y -= mTransScale;
	}

	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_RIGHT) )
	{
		mRotX -= mRotScale;
	}
	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_LEFT) )
	{
		mRotX += mRotScale;
	}
	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_ROT_UP) )
	{
		mRotY += mRotScale;
	}
	if ( INPUTENGINE.getEventBoolValue(EV_CHARACTER_ROT_DOWN) )
	{
		mRotY -= mRotScale;
	}

	ctx.mCamera->yaw(mRotX);
	ctx.mCamera->pitch(mRotY);

	Vector3 camPosition = ctx.mCamera->getPosition() + ctx.mCamera->getOrientation() * mTrans;

	if ( ctx.mHfinder )
	{
		float h = ctx.mHfinder->getHeightAt(camPosition.x, camPosition.z) + 1.0f;

		camPosition.y = std::max(h, camPosition.y);
	}

	ctx.mCamera->setPosition(camPosition);
}

bool CameraBehaviorFree::mouseMoved(const CameraManager::cameraContext_t &ctx, const OIS::MouseEvent& _arg)
{
	const OIS::MouseState ms = _arg.state;

	ctx.mCamera->yaw(Degree(-ms.X.rel * 0.13f));
	ctx.mCamera->pitch(Degree(-ms.Y.rel * 0.13f));

#ifdef USE_MYGUI
	MyGUI::PointerManager::getInstance().setVisible(false);
#endif // USE_MYGUI

	return true;
}

void CameraBehaviorFree::activate(const CameraManager::cameraContext_t &ctx, bool reset /* = true */)
{
#ifdef USE_MYGUI
	Console::getSingleton().putMessage(Console::CONSOLE_MSGTYPE_INFO, Console::CONSOLE_SYSTEM_NOTICE, _L("free camera"), "camera_go.png", 3000);
#endif // USE_MYGUI
}
