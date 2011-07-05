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
#include "PreviewRenderer.h"
#include "Ogre.h"
#include "Settings.h"

#include "RoRFrameListener.h"
#include "AdvancedOgreFramework.h"

#include "SkyManager.h"

#include "Beam.h"
#include "BeamFactory.h"
#include "engine.h"

PreviewRenderer::PreviewRenderer()
{
	fn = SSETTING("OPT_IMGPATH");
	if(fn.empty()) return;
	LOG("previewRenderer initialized");
}

PreviewRenderer::~PreviewRenderer()
{
}

void PreviewRenderer::render()
{
	LOG("starting previewRenderer...");
	Beam *truck = BeamFactory::getSingleton().getCurrentTruck();
	Ogre::SceneManager *sceneMgr = RoRFrameListener::eflsingleton->getSceneMgr();
	Ogre::Viewport *vp = RoRFrameListener::eflsingleton->getRenderWindow()->getViewport(0);

	// disable skybox
	//sceneMgr->setSkyBox(false, "");
	// disable fog
	//sceneMgr->setFog(FOG_NONE);
	// disable shadows
	//vp->setShadowsEnabled(false);
	// white background
	//vp->setBackgroundColour(Ogre::ColourValue::White);
	vp->setClearEveryFrame(true);

	// better mipmapping
	MaterialManager::getSingleton().setDefaultAnisotropy(8);
	MaterialManager::getSingleton().setDefaultTextureFiltering(TFO_ANISOTROPIC);

	// now switch on headlights and stuff
	BeamFactory::getSingleton().setCurrentTruck(truck->trucknum);
	truck->reset();
	truck->lightsToggle();
	truck->beaconsToggle();
	truck->setBlinkType(BLINK_WARN);
	truck->toggleCustomParticles();
	// accelerate
	if(truck->engine)
		truck->engine->autoSetAcc(0.2f);

	// steer a bit
	truck->hydrodircommand = -0.3f;

	// run the beam engine for ten seconds
	LOG("simulating truck for ten seconds ...");
	float time=0;
	Real dt = 0.0333; // 0.0333 seconds / frame = 30 FPS
	while(time < 10)
	{
		// run the engine for ten virtual seconds
		BeamFactory::getSingleton().calcPhysics(dt);
		time += dt;
	}
	BeamFactory::getSingleton().updateVisual(dt);
	LOG("simulation done");

	// calculate min camera radius for truck and the trucks center

	// first: average pos
	//truck->updateTruckPosition();

	// then camera radius:
	float minCameraRadius = 0;
	for (int i=0; i < truck->free_node; i++)
	{
		Real dist = truck->nodes[i].AbsPosition.distance(truck->position);
		if(dist > minCameraRadius)
			minCameraRadius = dist;
	}
	minCameraRadius *= 2.1f;

	SceneNode *camNode = sceneMgr->getRootSceneNode()->createChildSceneNode();

	Camera *cam = RoRFrameListener::eflsingleton->getCamera();
	cam->setLodBias(1000.0f);
	cam->setAspectRatio(1.0f);

	// calculate the camera-distance
	float fov = 60;
	Ogre::Vector3 maxVector = Vector3(truck->maxx, truck->maxy, truck->maxz);
	Ogre::Vector3 minVector = Vector3(truck->minx, truck->miny, truck->minz);
	int z1 = (maxVector.z-minVector.z)/2 + (((maxVector.x-minVector.x)/2) / tan(fov / 2));
	int z2 = (maxVector.z-minVector.z)/2 + (((maxVector.y-minVector.y)/2) / tan(fov / 2));
	camNode->setPosition(truck->position);
	camNode->attachObject(cam);

	cam->setFOVy(Ogre::Angle(fov));
	cam->setNearClipDistance(1.0f);
	cam->setPosition(Ogre::Vector3(0,12,std::max(z1, z2)+1));

	Ogre::Real radius = cam->getPosition().length();
	cam->setPosition(0.0,0.0,0.0);
	cam->setOrientation(Ogre::Quaternion::IDENTITY);
	cam->yaw(Ogre::Degree(0));
	cam->pitch(Ogre::Degree(-45));
	
	cam->moveRelative(Ogre::Vector3(0.0,0.0,radius));


	// only render our object
	//sceneMgr->clearSpecialCaseRenderQueues(); 
	//sceneMgr->addSpecialCaseRenderQueue(RENDER_QUEUE_6); 
	//sceneMgr->setSpecialCaseRenderQueueMode(SceneManager::SCRQM_INCLUDE); 

	//render2dviews(truck, cam, minCameraRadius);
	render3dpreview(truck, cam, minCameraRadius, camNode);
}

void PreviewRenderer::render2dviews(Beam *truck, Camera *cam, float minCameraRadius)
{
	float ominCameraRadius = minCameraRadius;

	for(int o=0;o<2;o++)
	{
		String oext = "ortho.";
		if     (o == 0)
		{
			cam->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
			minCameraRadius = ominCameraRadius * 2.1f;
		}
		else if(o == 1)
		{
			oext = "3d.";
			cam->setProjectionType(Ogre::PT_PERSPECTIVE);
			minCameraRadius = ominCameraRadius * 2.4f;
		}

		for(int i=0;i<2;i++)
		{
			String ext = "normal.";
			if(i == 0)
			{
				truck->hideSkeleton(true);
			} else if(i == 1)
			{
				ext = "skeleton.";
				// now show the skeleton
				truck->showSkeleton(true, true);
				truck->updateSimpleSkeleton();
			}
			ext = oext + ext;

			// 3d first :)
			cam->setPosition(Vector3(truck->position.x - minCameraRadius, truck->position.y * 1.5f, truck->position.z - minCameraRadius));
			cam->lookAt(truck->position);
			render(ext + "perspective");

			// right
			cam->setPosition(Vector3(truck->position.x, truck->position.y, truck->position.z - minCameraRadius));
			cam->lookAt(truck->position);
			render(ext + "right");

			// left
			cam->setPosition(Vector3(truck->position.x, truck->position.y, truck->position.z + minCameraRadius));
			cam->lookAt(truck->position);
			render(ext + "left");

			// front
			cam->setPosition(Vector3(truck->position.x - minCameraRadius, truck->position.y, truck->position.z));
			cam->lookAt(truck->position);
			render(ext + "front");

			// back
			cam->setPosition(Vector3(truck->position.x + minCameraRadius, truck->position.y, truck->position.z));
			cam->lookAt(truck->position);
			render(ext + "back");

			// top
			cam->setPosition(Vector3(truck->position.x, truck->position.y + minCameraRadius, truck->position.z + 0.01f));
			cam->lookAt(truck->position);
			render(ext + "top");

			// bottom
			cam->setPosition(Vector3(truck->position.x, truck->position.y - minCameraRadius, truck->position.z + 0.01f));
			cam->lookAt(truck->position);
			render(ext + "bottom");

		}
	}
}

void PreviewRenderer::render3dpreview(Beam *truck, Camera *renderCamera, float minCameraRadius, SceneNode *camNode)
{
	int yaw_angles = 1;
	int pitch_angles = 30;
	TexturePtr renderTexture;
	uint32 textureSize = 1024;
	//renderTexture = TextureManager::getSingleton().createManual("3dpreview1", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, TEX_TYPE_2D, textureSize * pitch_angles, textureSize * yaw_angles, 0, PF_A8R8G8B8, TU_RENDERTARGET);
	renderTexture = TextureManager::getSingleton().createManual("3dpreview1", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, TEX_TYPE_2D, textureSize, textureSize, 32, 0, PF_A8R8G8B8, TU_RENDERTARGET, 0, false, 8, "Quality");
	
	RenderTexture *renderTarget = renderTexture->getBuffer()->getRenderTarget();
	renderTarget->setAutoUpdated(false);
	Viewport *renderViewport = renderTarget->addViewport(renderCamera);
	renderViewport->setOverlaysEnabled(false);
	renderViewport->setClearEveryFrame(true);
	//renderViewport->setShadowsEnabled(false);
	//renderViewport->setBackgroundColour(ColourValue(1, 1, 1, 0));

	SkyManager::getSingleton().notifyCameraChanged(renderCamera);
	SkyManager::getSingleton().forceUpdate(0.01f);


	String skelmode = "normal";

	const float xDivFactor = 1.0f / pitch_angles;
	const float yDivFactor = 1.0f / yaw_angles;
	for (int s=0;s<2; s++)
	{
		if(s == 0)
		{
			truck->hideSkeleton(true);
			skelmode = "normal";
		} else if(s == 1)
		{
			skelmode = "skeleton";
			// now show the skeleton
			truck->showSkeleton(true, true);
			truck->updateSimpleSkeleton();
		}

		for (int o = 0; o < pitch_angles; ++o)
		{
			//4 pitch angle renders
			Radian pitch = Degree((360.0f * o) * xDivFactor);
			for (int i = 0; i < yaw_angles; ++i)
			{
				Radian yaw = Degree(-10); //Degree((20.0f * i) * yDivFactor - 10); //0, 45, 90, 135, 180, 225, 270, 315

				Ogre::Real radius = minCameraRadius; //renderCamera->getPosition().length();
				renderCamera->setPosition(Vector3::ZERO);
				renderCamera->setOrientation(Ogre::Quaternion::IDENTITY);
				renderCamera->yaw(Ogre::Degree(pitch));
				renderCamera->pitch(Ogre::Degree(yaw));
				renderCamera->moveRelative(Ogre::Vector3(0.0,0.0,radius));

				//Render into the texture
				// only when rendering all images into one texture
				//renderViewport->setDimensions((float)(o) * xDivFactor, (float)(i) * yDivFactor, xDivFactor, yDivFactor);

				renderTarget->update();
				//SkyManager::getSingleton().forceUpdate(0.01f);

				char tmp[56];
				sprintf(tmp, "%03d_%03d.jpg", i, o); // use .png for transparancy 
				renderTarget->writeContentsToFile(fn + skelmode + SSETTING("dirsep") + String(tmp));
				
				LOG("rendered " + fn + skelmode + SSETTING("dirsep") + String(tmp));
			}
		}
	}

	//Save RTT to file with respecting the temp dir
	//renderTarget->writeContentsToFile(fn + "all.png");

}


void PreviewRenderer::render(Ogre::String ext)
{
	// create some screenshot
	RoRWindowEventUtilities::messagePump();
	Ogre::Root::getSingleton().renderOneFrame();
	OgreFramework::getSingletonPtr()->m_pRenderWnd->update();
	OgreFramework::getSingletonPtr()->m_pRenderWnd->writeContentsToFile(fn+"."+ext+".jpg");
}
