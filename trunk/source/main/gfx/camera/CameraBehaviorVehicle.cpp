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
#include "CameraBehaviorVehicle.h"

#include "Beam.h"
#include "Settings.h"

using namespace Ogre;

CameraBehaviorVehicle::CameraBehaviorVehicle() :
	  CameraBehavior()
	, camPitching(true)
{
	if ( SSETTING("External Camera Mode", "Pitching") == "Static" )
	{
		camPitching = false;
	}
}

void CameraBehaviorVehicle::update(const CameraManager::cameraContext_t &ctx)
{
	Vector3 dir = (currTruck->nodes[currTruck->cameranodepos[0]].smoothpos
				 - currTruck->nodes[currTruck->cameranodedir[0]].smoothpos).normalisedCopy();

	targetDirection = -atan2(dir.dotProduct(Vector3::UNIT_X), dir.dotProduct(-Vector3::UNIT_Z));
	targetPitch     = 0.0f;

	if ( camPitching )
	{
		targetPitch = -asin(dir.dotProduct(Vector3::UNIT_Y));
	}

	camLookAt = currTruck->getPosition();

	CameraBehavior::update(ctx);
}

void CameraBehaviorVehicle::activate(const CameraManager::cameraContext_t &ctx)
{
	currTruck = ctx.mCurrTruck;
	camDistMin = currTruck->getMinimalCameraRadius() * 2.0f;
	reset(ctx);
}

void CameraBehaviorVehicle::reset(const CameraManager::cameraContext_t &ctx)
{
	camRotX = 0.0f;
	camRotY = 0.5f;
	camDist = camDistMin * 1.5f;
}
