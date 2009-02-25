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
#include "ExampleFrameListener.h"

//#include "joystick.h"
#include "ProceduralManager.h"

#include "main.h"

#ifdef LUASCRIPT
# include "luasystem.h"
#endif

#ifdef ANGELSCRIPT
#include "ScriptEngine.h"
#include "ogreconsole.h"
#endif

#include "Road.h"
#include "road2.h"
#include "editor.h"
#include "water.h"
#include "WaterOld.h"

#ifdef HYDRAX
# include "HydraxWater.h"
#endif

#include "dashboard.h"
#include "Heathaze.h"
#ifdef HAS_EDITOR
#include "truckeditor.h"
#endif
#include "network.h"
#ifdef NEWNET
#include "networknew.h"
#endif //NEWNET
#include "engine.h"
#include "turboprop.h"
#include "screwprop.h"
#include "FlexAirfoil.h"

#include "gui_manager.h"
#include "gui_loader.h"

#include "mirrors.h"
#include "TruckHUD.h"
#include "autopilot.h"
#include "ResourceBuffer.h"
#include "CacheSystem.h"

#ifdef PAGED
# include "PagedGeometry.h"
# include "ImpostorPage.h"
# include "BatchPage.h"
#endif

#include "MovableText.h"
#include "IngameEditor.h"
#include "IngameConsole.h"
#ifdef TIMING
	#include "BeamStats.h"
#endif

#ifdef PAGED
# include "TreeLoader2D.h"
# include "TreeLoader3D.h"
# include "GrassLoader.h"
#endif

#include "MapControl.h"
#include <OgreFontManager.h>
#include "language.h"


#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	#include <Windows.h>
# ifdef XFIRE
	// XFire support under windows only!
	#include "xfiregameclient.h"
# endif //XFIRE
#endif

#include "OISKeyboard.h"

//#include "OgreTerrainSceneManager.h" // = ILLEGAL to link to a plugin!

#include "writeTextToTexture.h"

#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
//#include <CFUserNotification.h>
#endif

using namespace std;

bool disableRendering=false;

SceneManager *caelumSM;
Camera *gCamera;

String debugText;

class disableRenderingListener : public RenderTargetListener
{
private:

public:
	void preRenderTargetUpdate(const RenderTargetEvent& evt)
	{
		if (disableRendering) mScene->setFindVisibleObjects(false);
	}
	void postRenderTargetUpdate(const RenderTargetEvent& evt)
	{
		mScene->setFindVisibleObjects(true);
	}

};

disableRenderingListener disableListener;

Material *terrainmaterial = 0;

char terrainoriginalmaterial[100];
bool shutdownall=false;
bool caelum_mapped=false;

class TerrainUpdater : public caelum::CaelumListener {
private:
	int state;
	char pname[100];
	char sname[10];
	ExampleFrameListener *mefl;
public:
	TerrainUpdater(ExampleFrameListener *efl)
	{
		mefl=efl;
		state=-1;
	}
	bool caelumStarted (const Ogre::FrameEvent &e, caelum::CaelumSystem *sys)
	{
		if (shutdownall) return true;

		// disable caelum in menu's, etc
		if(mefl->getLoadingState() != ALL_LOADED)
			return true;

		caelumSM->setAmbientLight(sys->getSun()->getSunColour() / 4.0);
		//			terrainmaterial->setLightingEnabled(sys->getSun()->getInclination()<Degree(0.0));
		//			terrainmaterial->setLightingEnabled(true);

		// try to update the water "ambient"
		Water *w = mefl->getWater();
		if(w)
		{
			w->setFadeColour(sys->getSun()->getSunColour() / 4.0);
			Ogre::Vector3 sunPosition = gCamera->getPosition() - 100000 * sys->getSun()->getSunDirection().normalisedCopy();
			w->setSunPosition(sunPosition);
		}

		if (!caelum_mapped)
			return true;

		// old school terrain texture loading
		if(!terrainmaterial)
			return true;
		if (state==-1)
		{
			strcpy(terrainoriginalmaterial, (terrainmaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getTextureName()).c_str());
			strcpy(pname,terrainoriginalmaterial);
			char* p=pname+strlen(pname);
			while (p!=pname && *p!='.') p--;
			*p=0;
			strcpy(sname, p+1);
		}
		float time=sys->getLocalTime()/3600.0;
		int nstate=(int)(time+0.5);
		if (nstate<7) nstate=0;
		if (nstate>17) nstate=0;
		if (state!=nstate)
		{
			state=nstate;
			char tname[100];
			sprintf(tname, "%s-%ih.%s", pname, state, sname);
			//				LogManager::getSingleton().logMessage("Caelum terrain loading: "+StringConverter::toString(tname));
			terrainmaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(tname);

		}
		return true;
	}
};


// static heightfinder
HeightFinder *ExampleFrameListener::hfinder = 0;
ExampleFrameListener *eflsingleton = 0;

//workaround for pagedgeometry
inline float getTerrainHeight(Ogre::Real x, Ogre::Real z, void *unused=0)
{
	if(!ExampleFrameListener::hfinder)
		return 1;
	return ExampleFrameListener::hfinder->getHeightAt(x, z);
}

void ExampleFrameListener::updateStats(void)
{
	static String currFps = _L("Current FPS: ");
	static String avgFps = _L("Average FPS: ");
	static String bestFps = _L("Best FPS: ");
	static String worstFps = _L("Worst FPS: ");
	static String tris = _L("Triangle Count: ");

	// update stats when necessary
	try {
		OverlayElement* guiAvg = OverlayManager::getSingleton().getOverlayElement("Core/AverageFps");
		OverlayElement* guiCurr = OverlayManager::getSingleton().getOverlayElement("Core/CurrFps");
		OverlayElement* guiBest = OverlayManager::getSingleton().getOverlayElement("Core/BestFps");
		OverlayElement* guiWorst = OverlayManager::getSingleton().getOverlayElement("Core/WorstFps");

		const RenderTarget::FrameStats& stats = mWindow->getStatistics();

		guiAvg->setCaption(avgFps + StringConverter::toString(stats.avgFPS));
		guiCurr->setCaption(currFps + StringConverter::toString(stats.lastFPS));
		guiBest->setCaption(bestFps + StringConverter::toString(stats.bestFPS)
			+" "+StringConverter::toString(stats.bestFrameTime)+" ms");
		guiWorst->setCaption(worstFps + StringConverter::toString(stats.worstFPS)
			+" "+StringConverter::toString(stats.worstFrameTime)+" ms");

		OverlayElement* guiTris = OverlayManager::getSingleton().getOverlayElement("Core/NumTris");
		String triss = tris + StringConverter::toString(stats.triangleCount);
		if(stats.triangleCount > 1000000)
			triss = tris + StringConverter::toString(stats.triangleCount/1000000.0f) + " M";
		else if(stats.triangleCount > 1000)
			triss = tris + StringConverter::toString(stats.triangleCount/1000.0f) + " k";
		guiTris->setCaption(triss);

		OverlayElement* guiDbg = OverlayManager::getSingleton().getOverlayElement("Core/DebugText");
		guiDbg->setCaption(debugText);
	}
	catch(...)
	{
		// ignore
	}
}

void ExampleFrameListener::startTimer()
{
	//LogManager::getSingleton().logMessage("LUA: startTimer()");
	raceStartTime = (int)rtime;
	OverlayManager::getSingleton().getByName("tracks/Racing")->show();
	laptimes->show();
	laptimems->show();
	laptimemin->show();
}

float ExampleFrameListener::stopTimer()
{
	//LogManager::getSingleton().logMessage("LUA: stopTimer()");
	float time=rtime - raceStartTime;
	char txt[255];
	sprintf(txt, "Last lap: %.2i'%.2i.%.2i", ((int)(time))/60,((int)(time))%60, ((int)(time*100.0))%100);
	lasttime->setCaption(txt);
	// let the display on
	//OverlayManager::getSingleton().getByName("tracks/Racing")->hide();
	laptimes->hide();
	laptimems->hide();
	laptimemin->hide();
	raceStartTime = -1;
	return time;
}

void ExampleFrameListener::updateRacingGUI()
{
	// update raceing gui if required
	float time=rtime - raceStartTime;
	char txt[10];
	sprintf(txt, "%.2i", ((int)(time*100.0))%100);
	laptimems->setCaption(txt);
	sprintf(txt, "%.2i", ((int)(time))%60);
	laptimes->setCaption(txt);
	sprintf(txt, "%.2i'", ((int)(time))/60);
	laptimemin->setCaption(txt);
}

//update engine panel
void ExampleFrameListener::updateGUI(float dt)
{
	if (current_truck==-1) return;

	//update the truck info gui (also if not displayed!)
	TRUCKHUD.update(dt, trucks[current_truck], mSceneMgr, mCamera, mWindow, mTruckInfoOn);
	CONSOLE.update(dt);

#ifdef TIMING
	BES.updateGUI(dt, current_truck, trucks);
#endif

	if (pressure_pressed)
	{
		Real angle;
		angle=135.0-trucks[current_truck]->getPressure()*2.7;
		pressuretexture->setTextureRotate(Degree(angle));
	}

	// update map
	if(bigMap)
	{
		for (int i=0; i<free_truck; i++)
		{
			MapEntity *e = bigMap->getEntityByName("Truck"+StringConverter::toString(i));
			if(!e)
				continue;
			if (trucks[i]->state != RECYCLE && !interactivemap)
			{
				e->setState(trucks[i]->state);
				e->setVisibility(true);
				e->setPosition(trucks[i]->getPosition());
				e->setRotation(-Radian(trucks[i]->getHeadingDirectionAngle()));
			}else
				e->setVisibility(false);
		}
	}

	if (trucks[current_truck]->driveable==TRUCK)
	{
		if(!trucks[current_truck]->engine)
			return;
		//TRUCK
		//special case for the editor
		if (trucks[current_truck]->editorId>=0 && editor)
		{
			OverlayManager::getSingleton().getOverlayElement("tracks/EditorPosition")->setCaption("Position: X=" +
				StringConverter::toString(trucks[current_truck]->nodes[trucks[current_truck]->editorId].AbsPosition.x)+
				"  Y="+StringConverter::toString(trucks[current_truck]->nodes[trucks[current_truck]->editorId].AbsPosition.y)+
				"  Z="+StringConverter::toString(trucks[current_truck]->nodes[trucks[current_truck]->editorId].AbsPosition.z)
				);
			OverlayManager::getSingleton().getOverlayElement("tracks/EditorAngles")->setCaption("Angles: 0.0 " +
				StringConverter::toString(editor->pturn)+
				"  "+StringConverter::toString(editor->ppitch)
				);
			char type[256];
			sprintf(type, "Object: %s", editor->curtype);
			OverlayManager::getSingleton().getOverlayElement("tracks/EditorObject")->setCaption(type);
		}
		if (trucks[current_truck]->engine->getGear()>0)
		{
			int gear = trucks[current_truck]->engine->getGear();
			int numgears = trucks[current_truck]->engine->getNumGears();
			String gearstr = StringConverter::toString(gear) + "/" + StringConverter::toString(numgears);
			guiGear->setCaption(gearstr);
			guiGear3D->setCaption(gearstr);
		}
		if (trucks[current_truck]->engine->getGear()==0)
		{
			guiGear->setCaption("N");
			guiGear3D->setCaption("N");
		}
		if (trucks[current_truck]->engine->getGear()<0)
		{
			guiGear->setCaption("R");
			guiGear3D->setCaption("R");
		}
		//autogears
		int i;
		int cg=trucks[current_truck]->engine->getAutoShift();
		for (i=0; i<5; i++)
		{
			if (i==cg)
			{
				if (i==1)
				{
					guiAuto[i]->setColourTop(ColourValue(1.0, 0.2, 0.2, 1.0));
					guiAuto[i]->setColourBottom(ColourValue(0.8, 0.1, 0.1, 1.0));
					guiAuto3D[i]->setColourTop(ColourValue(1.0, 0.2, 0.2, 1.0));
					guiAuto3D[i]->setColourBottom(ColourValue(0.8, 0.1, 0.1, 1.0));
				} else
				{
					guiAuto[i]->setColourTop(ColourValue(1.0, 1.0, 1.0, 1.0));
					guiAuto[i]->setColourBottom(ColourValue(0.8, 0.8, 0.8, 1.0));
					guiAuto3D[i]->setColourTop(ColourValue(1.0, 1.0, 1.0, 1.0));
					guiAuto3D[i]->setColourBottom(ColourValue(0.8, 0.8, 0.8, 1.0));
				}
			} else
			{
				if (i==1)
				{
					guiAuto[i]->setColourTop(ColourValue(0.4, 0.05, 0.05, 1.0));
					guiAuto[i]->setColourBottom(ColourValue(0.3, 0.02, 0.2, 1.0));
					guiAuto3D[i]->setColourTop(ColourValue(0.4, 0.05, 0.05, 1.0));
					guiAuto3D[i]->setColourBottom(ColourValue(0.3, 0.02, 0.2, 1.0));
				} else
				{
					guiAuto[i]->setColourTop(ColourValue(0.4, 0.4, 0.4, 1.0));
					guiAuto[i]->setColourBottom(ColourValue(0.3, 0.3, 0.3, 1.0));
					guiAuto3D[i]->setColourTop(ColourValue(0.4, 0.4, 0.4, 1.0));
					guiAuto3D[i]->setColourBottom(ColourValue(0.3, 0.3, 0.3, 1.0));
				}
			}

		}
		//pedals
		guipedclutch->setTop(-0.05*trucks[current_truck]->engine->getClutch()-0.01);
		guipedbrake->setTop(-0.05*(1.0-trucks[current_truck]->brake/trucks[current_truck]->brakeforce)-0.01);
		guipedacc->setTop(-0.05*(1.0-trucks[current_truck]->engine->getAcc())-0.01);

		//	speed_node->resetOrientation();

		// calculate speed
		Real guiSpeedFactor = 7.0 * (140.0 / trucks[current_truck]->speedoMax);
		Real angle = 140 - fabs(trucks[current_truck]->WheelSpeed * guiSpeedFactor);
		if (angle < -140)
			angle = -140;
		speedotexture->setTextureRotate(Degree(angle));


		//	speed_node->roll(Degree(angle));
		//	tach_node->resetOrientation();

		// calculate tach stuff
		Real tachoFactor = 0.072;
		if(trucks[current_truck]->useMaxRPMforGUI)
			tachoFactor = 0.072 * (3500 / trucks[current_truck]->engine->getMaxRPM());
		angle=126.0-fabs(trucks[current_truck]->engine->getRPM() * tachoFactor);
		if (angle<-120.0) angle=-120.0;
		if (angle>121.0) angle=121.0;
		tachotexture->setTextureRotate(Degree(angle));


		//	tach_node->roll(Degree(angle));
		//roll
		Vector3 dir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranoderoll[0]].RelPosition;
		dir.normalise();
		//	roll_node->resetOrientation();
		angle=asin(dir.dotProduct(Vector3::UNIT_Y));
		if (angle<-1) angle=-1;
		if (angle>1) angle=1;
		//float jroll=angle*1.67;
		rolltexture->setTextureRotate(Radian(angle));
		//	roll_node->roll(Radian(angle));
		//rollcorr
		if (trucks[current_truck]->free_active_shock)
		{
			//		rollcorr_node->resetOrientation();
			//		rollcorr_node->roll(Radian(-trucks[current_truck]->stabratio*5.0));
			rollcortexture->setTextureRotate(Radian(-trucks[current_truck]->stabratio*10.0));
			if (trucks[current_truck]->stabcommand)
				guiRoll->setMaterialName("tracks/rollmaskblink");
			else guiRoll->setMaterialName("tracks/rollmask");
		}
		//pitch
		dir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[0]].RelPosition;
		dir.normalise();
		//	pitch_node->resetOrientation();
		angle=asin(dir.dotProduct(Vector3::UNIT_Y));
		if (angle<-1) angle=-1;
		if (angle>1) angle=1;
		pitchtexture->setTextureRotate(Radian(angle));
		//	pitch_node->roll(Radian(angle));
		//turbo
		angle=40.0-trucks[current_truck]->engine->getTurboPSI()*3.34;
		turbotexture->setTextureRotate(Degree(angle));


		//force feedback
		/*
		// FF-code commented out when upgrading to new input system.
		if (useforce)
		{
			Vector3 udir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].Position-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[0]].Position;
			Vector3 uroll=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].Position-trucks[current_truck]->nodes[trucks[current_truck]->cameranoderoll[0]].Position;
			udir.normalise();
			uroll.normalise();
			joy->setForceFeedback(-trucks[current_truck]->ffforce.dotProduct(uroll)/10000.0,
				trucks[current_truck]->ffforce.dotProduct(udir)/10000.0);
		}
		*/
		//map update
		//((TerrainSceneManager*)mSceneMgr)->getScale().x;


		//indicators
		if (trucks[current_truck]->engine->contact ) igno->setMaterialName("tracks/ign-on");
		else igno->setMaterialName("tracks/ign-off");
		if (trucks[current_truck]->engine->contact && !trucks[current_truck]->engine->running) batto->setMaterialName("tracks/batt-on");
		else batto->setMaterialName("tracks/batt-off");
		if (trucks[current_truck]->parkingbrake) pbrakeo->setMaterialName("tracks/pbrake-on");
		else pbrakeo->setMaterialName("tracks/pbrake-off");
		if (trucks[current_truck]->locked) lockedo->setMaterialName("tracks/locked-on");
		else lockedo->setMaterialName("tracks/locked-off");
		if (trucks[current_truck]->tied)
		{
			if (trucks[current_truck]->commandkey[0].commandValue)
			{
				flipflop=!flipflop;
				if (flipflop) securedo->setMaterialName("tracks/secured-on");
				else securedo->setMaterialName("tracks/secured-off");
			}
			else securedo->setMaterialName("tracks/secured-on");
		}
		else securedo->setMaterialName("tracks/secured-off");
		if (!trucks[current_truck]->canwork) lopresso->setMaterialName("tracks/lopress-on");
		else lopresso->setMaterialName("tracks/lopress-off");

		if (fabs(trucks[current_truck]->engine->getTorque())>=100000.0) clutcho->setMaterialName("tracks/clutch-on");
		else clutcho->setMaterialName("tracks/clutch-off");

		if (trucks[current_truck]->lights) lightso->setMaterialName("tracks/lights-on");
		else lightso->setMaterialName("tracks/lights-off");
	}
	else
		if (trucks[current_truck]->driveable==AIRPLANE)
		{
			//AIRPLANE GUI
			int ftp=trucks[current_truck]->free_aeroengine;
			//throtles
			thro1->setTop(thrtop+thrheight*(1.0-trucks[current_truck]->aeroengines[0]->getThrotle())-1.0);
			if (ftp>1) thro2->setTop(thrtop+thrheight*(1.0-trucks[current_truck]->aeroengines[1]->getThrotle())-1.0);
			if (ftp>2) thro3->setTop(thrtop+thrheight*(1.0-trucks[current_truck]->aeroengines[2]->getThrotle())-1.0);
			if (ftp>3) thro4->setTop(thrtop+thrheight*(1.0-trucks[current_truck]->aeroengines[3]->getThrotle())-1.0);
			//fire
			if (trucks[current_truck]->aeroengines[0]->isFailed()) engfireo1->setMaterialName("tracks/engfire-on"); else engfireo1->setMaterialName("tracks/engfire-off");
			if (ftp>1 && trucks[current_truck]->aeroengines[1]->isFailed()) engfireo2->setMaterialName("tracks/engfire-on"); else engfireo2->setMaterialName("tracks/engfire-off");
			if (ftp>2 && trucks[current_truck]->aeroengines[2]->isFailed()) engfireo3->setMaterialName("tracks/engfire-on"); else engfireo3->setMaterialName("tracks/engfire-off");
			if (ftp>3 && trucks[current_truck]->aeroengines[3]->isFailed()) engfireo4->setMaterialName("tracks/engfire-on"); else engfireo4->setMaterialName("tracks/engfire-off");

			//airspeed
			float angle=0.0;
			float ground_speed_kt=trucks[current_truck]->nodes[0].Velocity.length()*1.9438;

			//tropospheric model valid up to 11.000m (33.000ft)
			float altitude=trucks[current_truck]->nodes[0].AbsPosition.y;
			float sea_level_temperature=273.15+15.0; //in Kelvin
			float sea_level_pressure=101325; //in Pa
			float airtemperature=sea_level_temperature-altitude*0.0065; //in Kelvin
			float airpressure=sea_level_pressure*pow(1.0-0.0065*altitude/288.15, 5.24947); //in Pa
			float airdensity=airpressure*0.0000120896;//1.225 at sea level

			float kt=ground_speed_kt*sqrt(airdensity/1.225); //KIAS

			if (kt>23.0)
				if (kt<50.0) angle=((kt-23.0)/1.111);
				else if (kt<100.0) angle=(24.0+(kt-50.0)/0.8621);
				else if (kt<300.0) angle=(82.0+(kt-100.0)/0.8065);
				else angle=329.0;
				airspeedtexture->setTextureRotate(Degree(-angle));
				//aoa
				angle=0;
				if (trucks[current_truck]->free_wing>4) angle=trucks[current_truck]->wings[4].fa->aoa;
				if (kt<10.0) angle=0;
				float absangle=angle;
				if (absangle<0) absangle=-absangle;
				ssm->modulate(current_truck, SS_MOD_AOA, absangle);
				if (absangle>18.0) ssm->trigStart(current_truck, SS_TRIG_AOA); else ssm->trigStop(current_truck, SS_TRIG_AOA);
				if (angle>25.0) angle=25.0;
				if (angle<-25.0) angle=-25.0;
				aoatexture->setTextureRotate(Degree(-angle*4.7+90.0));
				//altimeter
				angle=trucks[current_truck]->nodes[0].AbsPosition.y*1.1811;
				altimetertexture->setTextureRotate(Degree(-angle));
				char altc[10];
				sprintf(altc, "%03u", (int)(trucks[current_truck]->nodes[0].AbsPosition.y/30.48));
				alt_value_taoe->setCaption(altc);
				//adi
				//roll
				Vector3 rollv=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranoderoll[0]].RelPosition;
				rollv.normalise();
				float rollangle=asin(rollv.dotProduct(Vector3::UNIT_Y));
				//pitch
				Vector3 dirv=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[0]].RelPosition;
				dirv.normalise();
				float pitchangle=asin(dirv.dotProduct(Vector3::UNIT_Y));
				Vector3 upv=dirv.crossProduct(-rollv);
				if (upv.y<0) rollangle=3.14159-rollangle;
				adibugstexture->setTextureRotate(Radian(-rollangle));
				aditapetexture->setTextureVScroll(-pitchangle*0.25);
				aditapetexture->setTextureRotate(Radian(-rollangle));

				//hsi
				Vector3 idir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[0]].RelPosition;
				//			idir.normalise();
				float dirangle=atan2(idir.dotProduct(Vector3::UNIT_X), idir.dotProduct(-Vector3::UNIT_Z));
				hsirosetexture->setTextureRotate(Radian(dirangle));
				if (trucks[current_truck]->autopilot)
				{
					hsibugtexture->setTextureRotate(Radian(dirangle)-Degree(trucks[current_truck]->autopilot->heading));
					float vdev=0;
					float hdev=0;
					trucks[current_truck]->autopilot->getRadioFix(localizers, free_localizer, &vdev, &hdev);
					if (hdev>15) hdev=15;
					if (hdev<-15) hdev=-15;
					hsivtexture->setTextureUScroll(-hdev*0.02);
					if (vdev>15) vdev=15;
					if (vdev<-15) vdev=-15;
					hsihtexture->setTextureVScroll(-vdev*0.02);
				}

				//vvi
				float vvi=trucks[current_truck]->nodes[0].Velocity.y*196.85;
				if (vvi<1000.0 && vvi>-1000.0) angle=vvi*0.047;
				if (vvi>1000.0 && vvi<6000.0) angle=47.0+(vvi-1000.0)*0.01175;
				if (vvi>6000.0) angle=105.75;
				if (vvi<-1000.0 && vvi>-6000.0) angle=-47.0+(vvi+1000.0)*0.01175;
				if (vvi<-6000.0) angle=-105.75;
				vvitexture->setTextureRotate(Degree(-angle+90.0));
				//rpm
				float pcent=trucks[current_truck]->aeroengines[0]->getRPMpc();
				if (pcent<60.0) angle=-5.0+pcent*1.9167;
				else if (pcent<110.0) angle=110.0+(pcent-60.0)*4.075;
				else angle=314.0;
				airrpm1texture->setTextureRotate(Degree(-angle));

				if (ftp>1) pcent=trucks[current_truck]->aeroengines[1]->getRPMpc(); else pcent=0;
				if (pcent<60.0) angle=-5.0+pcent*1.9167;
				else if (pcent<110.0) angle=110.0+(pcent-60.0)*4.075;
				else angle=314.0;
				airrpm2texture->setTextureRotate(Degree(-angle));

				if (ftp>2) pcent=trucks[current_truck]->aeroengines[2]->getRPMpc(); else pcent=0;
				if (pcent<60.0) angle=-5.0+pcent*1.9167;
				else if (pcent<110.0) angle=110.0+(pcent-60.0)*4.075;
				else angle=314.0;
				airrpm3texture->setTextureRotate(Degree(-angle));

				if (ftp>3) pcent=trucks[current_truck]->aeroengines[3]->getRPMpc(); else pcent=0;
				if (pcent<60.0) angle=-5.0+pcent*1.9167;
				else if (pcent<110.0) angle=110.0+(pcent-60.0)*4.075;
				else angle=314.0;
				airrpm4texture->setTextureRotate(Degree(-angle));

				if (trucks[current_truck]->aeroengines[0]->getType()==AEROENGINE_TYPE_TURBOPROP)
				{
					Turboprop *tp=(Turboprop*)trucks[current_truck]->aeroengines[0];
					//pitch
					airpitch1texture->setTextureRotate(Degree(-tp->pitch*2.0));
					//torque
					pcent=100.0*tp->indicated_torque/tp->max_torque;
					if (pcent<60.0) angle=-5.0+pcent*1.9167;
					else if (pcent<110.0) angle=110.0+(pcent-60.0)*4.075;
					else angle=314.0;
					airtorque1texture->setTextureRotate(Degree(-angle));
				}

				if (ftp>1 && trucks[current_truck]->aeroengines[1]->getType()==AEROENGINE_TYPE_TURBOPROP)
				{
					Turboprop *tp=(Turboprop*)trucks[current_truck]->aeroengines[1];
					//pitch
					airpitch2texture->setTextureRotate(Degree(-tp->pitch*2.0));
					//torque
					pcent=100.0*tp->indicated_torque/tp->max_torque;
					if (pcent<60.0) angle=-5.0+pcent*1.9167;
					else if (pcent<110.0) angle=110.0+(pcent-60.0)*4.075;
					else angle=314.0;
					airtorque2texture->setTextureRotate(Degree(-angle));
				}

				if (ftp>2 && trucks[current_truck]->aeroengines[2]->getType()==AEROENGINE_TYPE_TURBOPROP)
				{
					Turboprop *tp=(Turboprop*)trucks[current_truck]->aeroengines[2];
					//pitch
					airpitch3texture->setTextureRotate(Degree(-tp->pitch*2.0));
					//torque
					pcent=100.0*tp->indicated_torque/tp->max_torque;
					if (pcent<60.0) angle=-5.0+pcent*1.9167;
					else if (pcent<110.0) angle=110.0+(pcent-60.0)*4.075;
					else angle=314.0;
					airtorque3texture->setTextureRotate(Degree(-angle));
				}

				if (ftp>3 && trucks[current_truck]->aeroengines[3]->getType()==AEROENGINE_TYPE_TURBOPROP)
				{
					Turboprop *tp=(Turboprop*)trucks[current_truck]->aeroengines[3];
					//pitch
					airpitch4texture->setTextureRotate(Degree(-tp->pitch*2.0));
					//torque
					pcent=100.0*tp->indicated_torque/tp->max_torque;
					if (pcent<60.0) angle=-5.0+pcent*1.9167;
					else if (pcent<110.0) angle=110.0+(pcent-60.0)*4.075;
					else angle=314.0;
					airtorque4texture->setTextureRotate(Degree(-angle));
				}

				//starters
				if (trucks[current_truck]->aeroengines[0]->getIgnition()) engstarto1->setMaterialName("tracks/engstart-on"); else engstarto1->setMaterialName("tracks/engstart-off");
				if (ftp>1 && trucks[current_truck]->aeroengines[1]->getIgnition()) engstarto2->setMaterialName("tracks/engstart-on"); else engstarto2->setMaterialName("tracks/engstart-off");
				if (ftp>2 && trucks[current_truck]->aeroengines[2]->getIgnition()) engstarto3->setMaterialName("tracks/engstart-on"); else engstarto3->setMaterialName("tracks/engstart-off");
				if (ftp>3 && trucks[current_truck]->aeroengines[3]->getIgnition()) engstarto4->setMaterialName("tracks/engstart-on"); else engstarto4->setMaterialName("tracks/engstart-off");
		}
		else
			if (trucks[current_truck]->driveable==BOAT)
			{
				//BOAT GUI
				int fsp=trucks[current_truck]->free_screwprop;
				//throtles
				bthro1->setTop(thrtop+thrheight*(0.5-trucks[current_truck]->screwprops[0]->getThrotle()/2.0)-1.0);
				if (fsp>1) bthro2->setTop(thrtop+thrheight*(0.5-trucks[current_truck]->screwprops[1]->getThrotle()/2.0)-1.0);
				//map update

				//position
				Vector3 dir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[0]].RelPosition;
				dir.normalise();
				//moveBoatMapDot(trucks[current_truck]->position.x/mapsizex, trucks[current_truck]->position.z/mapsizez);
				//position

				char tmp[50]="";
				if(trucks[current_truck]->getLowestNode() != -1)
				{
					Vector3 pos = trucks[current_truck]->nodes[trucks[current_truck]->getLowestNode()].AbsPosition;
					float height =  pos.y - hfinder->getHeightAt(pos.x, pos.z);
					if(height>0.1 && height < 99.9)
					{
						sprintf(tmp, "%2.1f", height);
						boat_depth_value_taoe->setCaption(tmp);
					}else
						boat_depth_value_taoe->setCaption("--.-");
				}

				//waterspeed
				float angle=0.0;
				Vector3 hdir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[0]].RelPosition;
				hdir.normalise();
				float kt=hdir.dotProduct(trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].Velocity)*1.9438;
				angle=kt*4.2;
				boatspeedtexture->setTextureRotate(Degree(-angle));
				boatsteertexture->setTextureRotate(Degree(trucks[current_truck]->screwprops[0]->getRudder() * 170));
			}
}

void ExampleFrameListener::resizePanel(OverlayElement *oe, RenderWindow *win)
{
	oe->setHeight(oe->getHeight()*(Real)win->getWidth()/(Real)win->getHeight());
	oe->setTop(oe->getTop()*(Real)win->getWidth()/(Real)win->getHeight());
}

void ExampleFrameListener::reposPanel(OverlayElement *oe, RenderWindow *win)
{
	oe->setTop(oe->getTop()*(Real)win->getWidth()/(Real)win->getHeight());
}

void ExampleFrameListener::placeNeedle(RenderWindow* win, SceneNode *node, float x, float y, float len)
{/**(Real)win->getHeight()/(Real)win->getWidth()*/
	node->setPosition((x-640.0)/444.0, (512-y)/444.0, -2.0);
	node->setScale(0.0025, 0.007*len, 0.007);
}

float ExampleFrameListener::gravity = DEFAULT_GRAVITY;

void ExampleFrameListener::setGravity(float value)
{
	if(!eflsingleton) return;
	// update the mass of all trucks
	gravity = value;
	for (int t=0; t<eflsingleton->free_truck; t++)
	{
		eflsingleton->trucks[t]->recalc_masses();
	}
}

// Constructor takes a RenderWindow because it uses that to determine input context
ExampleFrameListener::ExampleFrameListener(RenderWindow* win, Camera* cam, SceneManager* scm, Root* root) :  initialized(false)
{
	eflsingleton=this;
#ifdef ANGELSCRIPT
	scriptEngine = 0;
	timeLastScriptStep = 0;
	new OgreConsole;
	OgreConsole::getSingleton().init(root);
	OgreConsole::getSingleton().setVisible(false);
#endif
	externalCameraMode=0;
	gameStartTime = CACHE.getTimeStamp();
	loadedTerrain="none";
	creditsviewtime=5;
	terrainUID="";
	fogmode=-1;
	fogdensity=0;
	mtc=0;
	bigMap=0;
	envmap=0;
	debugCollisions=false;
	interactivemap=0;
	free_localizer=0;
	loading_state=NONE_LOADED;
	pressure_pressed=false;
	chatting=false;
	caelumSM=scm;
	caelum_mapped=false;
	mCaelumSystem=0;
	rtime=0;
	joyshiftlock=0;
	mScene=scm;
	persostart=Vector3(0,0,0);
	person=0;
	reload_pos=Vector3::ZERO;
	win->addListener(&disableListener);
	free_truck=0;
	//dirt=0;
	editorfd=0;
#ifdef HAS_EDITOR
	trucked=0;
#endif
	showcredits=0;
	current_truck=-1;
	w=0;
	mapsizex = 3000;
	mapsizez = 3000;
	hidegui=false;
	collisions=0;
	timeUntilUnflash=-1.0;
	editor=0;
	mirror=0;
	dashboard=0;//new Dashboard(scm,win);

	mSceneMgr=scm;
	ssm=SoundScriptManager::getSingleton();
	mRoot=root;

	// setup direction arrow
	directionOverlay = OverlayManager::getSingleton().getByName("tracks/DirectionArrow");
	directionArrowText = (TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/DirectionArrow/Text");
	directionArrowDistance = (TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/DirectionArrow/Distance");
	Entity *arrent = mSceneMgr->createEntity("dirArrowEntity", "arrow2.mesh");
	arrent->setNormaliseNormals(true);
	// Add entity to the scene node
	dirArrowNode= new SceneNode(mSceneMgr);
	dirArrowNode->attachObject(arrent);
	dirArrowNode->setVisible(false);
	dirArrowNode->setScale(0.1, 0.1, 0.1);
	dirArrowNode->setPosition(Vector3(-0.6, +0.4, -1));
	dirArrowNode->setFixedYawAxis(true, Vector3::UNIT_Y);
	dirvisible = false;
	dirArrowPointed = Vector3::ZERO;
	directionOverlay->add3D(dirArrowNode);
	raceStartTime=-1;
	truck_preload_num=0;

	// setup input
	inputGrabMode=GRAB_ALL;
	switchMouseButtons=false;
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
	// on apple, switch by default
	switchMouseButtons=true;
#endif
	if(SETTINGS.getSetting("Switch Mouse Buttons") == "Yes")
		switchMouseButtons=true;
	else if(SETTINGS.getSetting("Switch Mouse Buttons") == "No")
		switchMouseButtons=false;

	mouseGrabState=0; // 0 = not grabbed
	if(SETTINGS.getSetting("Input Grab") == "All")
		inputGrabMode = GRAB_ALL;
	else if(SETTINGS.getSetting("Input Grab") == "Dynamically")
		inputGrabMode = GRAB_DYNAMICALLY;
	else if(SETTINGS.getSetting("Input Grab") == "None")
		inputGrabMode = GRAB_NONE;

	// start input engine
	size_t hWnd = 0;
	win->getCustomAttribute("WINDOW", &hWnd);

	INPUTENGINE.setup(hWnd, true, true, inputGrabMode);

	// init GUI
	MYGUI.setup(cam, scm, win);

	UILOADER.setup(win);

	CACHE.startup();

	mouseOverlay = OverlayManager::getSingleton().getByName("tracks/MouseOverlay");
#ifdef HAS_EDITOR
	truckeditorOverlay = OverlayManager::getSingleton().getByName("tracks/TruckEditorOverlay");
#endif
	mDebugOverlay = OverlayManager::getSingleton().getByName("Core/DebugOverlay");
	mTimingDebugOverlay = OverlayManager::getSingleton().getByName("tracks/DebugBeamTiming");
	mTimingDebugOverlay->hide();


	machinedashboardOverlay = OverlayManager::getSingleton().getByName("tracks/MachineDashboardOverlay");
	airdashboardOverlay = OverlayManager::getSingleton().getByName("tracks/AirDashboardOverlay");
	airneedlesOverlay = OverlayManager::getSingleton().getByName("tracks/AirNeedlesOverlay");
	boatdashboardOverlay = OverlayManager::getSingleton().getByName("tracks/BoatDashboardOverlay");
	boatneedlesOverlay = OverlayManager::getSingleton().getByName("tracks/BoatNeedlesOverlay");
	dashboardOverlay = OverlayManager::getSingleton().getByName("tracks/DashboardOverlay");
	needlesOverlay = OverlayManager::getSingleton().getByName("tracks/NeedlesOverlay");
	needlesMaskOverlay = OverlayManager::getSingleton().getByName("tracks/NeedlesMaskOverlay");





	playerListOverlay = OverlayManager::getSingleton().getByName("tracks/MPPlayerList");

	pressureOverlay = OverlayManager::getSingleton().getByName("tracks/PressureOverlay");
	pressureNeedleOverlay = OverlayManager::getSingleton().getByName("tracks/PressureNeedleOverlay");
	editorOverlay = OverlayManager::getSingleton().getByName("tracks/EditorOverlay");

	//adjust dashboard size for screen ratio
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/pressureo"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/pressureneedle"), win);
	pressuretexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/pressureneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/speedo"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/tacho"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/anglo"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/instructions"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/machineinstructions"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/dashbar"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/dashfiller"), win	);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/helppanel"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/machinehelppanel"), win);

	resizePanel(igno=OverlayManager::getSingleton().getOverlayElement("tracks/ign"), win);
	resizePanel(batto=OverlayManager::getSingleton().getOverlayElement("tracks/batt"), win);
	resizePanel(pbrakeo=OverlayManager::getSingleton().getOverlayElement("tracks/pbrake"), win);
	resizePanel(lockedo=OverlayManager::getSingleton().getOverlayElement("tracks/locked"), win);
	resizePanel(securedo=OverlayManager::getSingleton().getOverlayElement("tracks/secured"), win);
	resizePanel(lopresso=OverlayManager::getSingleton().getOverlayElement("tracks/lopress"), win);
	resizePanel(clutcho=OverlayManager::getSingleton().getOverlayElement("tracks/clutch"), win);
	resizePanel(lightso=OverlayManager::getSingleton().getOverlayElement("tracks/lights"), win);

	mouseElement = OverlayManager::getSingleton().getOverlayElement("mouse/pointer");
	screenWidth=win->getWidth();
	screenHeight=win->getHeight();
	mouseX=screenWidth-20;
	mouseY=screenHeight-20;
	isnodegrabbed=false;

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/machinedashbar"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/machinedashfiller"), win);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airdashbar"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airdashfiller"), win);
	OverlayElement* tempoe;
	resizePanel(tempoe=OverlayManager::getSingleton().getOverlayElement("tracks/thrusttrack1"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/thrusttrack2"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/thrusttrack3"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/thrusttrack4"), win);

	//resizePanel(airmapo=OverlayManager::getSingleton().getOverlayElement("tracks/airmap"), win);
	//	resizePanel(airmapdot=OverlayManager::getSingleton().getOverlayElement("tracks/airreddot"), win);

	resizePanel(thro1=OverlayManager::getSingleton().getOverlayElement("tracks/thrust1"), win);
	resizePanel(thro2=OverlayManager::getSingleton().getOverlayElement("tracks/thrust2"), win);
	resizePanel(thro3=OverlayManager::getSingleton().getOverlayElement("tracks/thrust3"), win);
	resizePanel(thro4=OverlayManager::getSingleton().getOverlayElement("tracks/thrust4"), win);
	engfireo1=OverlayManager::getSingleton().getOverlayElement("tracks/engfire1");
	engfireo2=OverlayManager::getSingleton().getOverlayElement("tracks/engfire2");
	engfireo3=OverlayManager::getSingleton().getOverlayElement("tracks/engfire3");
	engfireo4=OverlayManager::getSingleton().getOverlayElement("tracks/engfire4");
	engstarto1=OverlayManager::getSingleton().getOverlayElement("tracks/engstart1");
	engstarto2=OverlayManager::getSingleton().getOverlayElement("tracks/engstart2");
	engstarto3=OverlayManager::getSingleton().getOverlayElement("tracks/engstart3");
	engstarto4=OverlayManager::getSingleton().getOverlayElement("tracks/engstart4");
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airrpm1"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airrpm2"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airrpm3"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airrpm4"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airpitch1"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airpitch2"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airpitch3"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airpitch4"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airtorque1"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airtorque2"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airtorque3"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airtorque4"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airspeed"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/vvi"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/altimeter"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/altimeter_val"), win);
	alt_value_taoe=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/altimeter_val");
	boat_depth_value_taoe=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/boatdepthmeter_val");
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/adi-tape"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/adi"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/adi-bugs"), win);
	adibugstexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/adi-bugs")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	aditapetexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/adi-tape")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/aoa"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/hsi"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/hsi-rose"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/hsi-bug"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/hsi-v"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/hsi-h"), win);
	hsirosetexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/hsi-rose")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	hsibugtexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/hsi-bug")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	hsivtexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/hsi-v")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	hsihtexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/hsi-h")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	//autopilot
	reposPanel(OverlayManager::getSingleton().getOverlayElement("tracks/ap_hdg_pack"), win);
	reposPanel(OverlayManager::getSingleton().getOverlayElement("tracks/ap_wlv_but"), win);
	reposPanel(OverlayManager::getSingleton().getOverlayElement("tracks/ap_nav_but"), win);
	reposPanel(OverlayManager::getSingleton().getOverlayElement("tracks/ap_alt_pack"), win);
	reposPanel(OverlayManager::getSingleton().getOverlayElement("tracks/ap_vs_pack"), win);
	reposPanel(OverlayManager::getSingleton().getOverlayElement("tracks/ap_ias_pack"), win);
	reposPanel(OverlayManager::getSingleton().getOverlayElement("tracks/ap_gpws_but"), win);
	reposPanel(OverlayManager::getSingleton().getOverlayElement("tracks/ap_brks_but"), win);


	thrtop=1.0+tempoe->getTop()+thro1->getHeight()/2.0;
	thrheight=tempoe->getHeight()-thro1->getHeight()*2.0;
	throffset=thro1->getHeight()/2.0;

	//boat
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/boatdashbar"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/boatdashfiller"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/boatthrusttrack1"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/boatthrusttrack2"), win);

	//resizePanel(boatmapo=OverlayManager::getSingleton().getOverlayElement("tracks/boatmap"), win);
	//resizePanel(boatmapdot=OverlayManager::getSingleton().getOverlayElement("tracks/boatreddot"), win);

	resizePanel(bthro1=OverlayManager::getSingleton().getOverlayElement("tracks/boatthrust1"), win);
	resizePanel(bthro2=OverlayManager::getSingleton().getOverlayElement("tracks/boatthrust2"), win);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/boatspeed"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/boatsteer"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/boatspeedneedle"), win);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/boatsteer/fg"), win);
	boatspeedtexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/boatspeedneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	boatsteertexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/boatsteer/fg_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	//adjust dashboard mask
	//pitch mask
	OverlayElement *pmask=OverlayManager::getSingleton().getOverlayElement("tracks/pitchmask");
	pmask->setHeight(pmask->getHeight()*(Real)win->getWidth()/(Real)win->getHeight());
	pmask->setTop(pmask->getTop()*(Real)win->getWidth()/(Real)win->getHeight());
	//roll mask
	OverlayElement *rmask=OverlayManager::getSingleton().getOverlayElement("tracks/rollmask");
	rmask->setHeight(rmask->getHeight()*(Real)win->getWidth()/(Real)win->getHeight());
	rmask->setTop(rmask->getTop()*(Real)win->getWidth()/(Real)win->getHeight());

	//prepare needles
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/speedoneedle"), win);
	speedotexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/speedoneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/tachoneedle"), win);
	tachotexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/tachoneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/rollneedle"), win);
	rolltexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/rollneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/pitchneedle"), win);
	pitchtexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/pitchneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/rollcorneedle"), win);
	rollcortexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/rollcorneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/turboneedle"), win);
	turbotexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/turboneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airspeedneedle"), win);
	airspeedtexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airspeedneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/altimeterneedle"), win);
	altimetertexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/altimeterneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/vvineedle"), win);
	vvitexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/vvineedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/aoaneedle"), win);
	aoatexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/aoaneedle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);


	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airrpm1needle"), win);
	airrpm1texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airrpm1needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airrpm2needle"), win);
	airrpm2texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airrpm2needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airrpm3needle"), win);
	airrpm3texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airrpm3needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airrpm4needle"), win);
	airrpm4texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airrpm4needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airpitch1needle"), win);
	airpitch1texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airpitch1needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airpitch2needle"), win);
	airpitch2texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airpitch2needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airpitch3needle"), win);
	airpitch3texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airpitch3needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airpitch4needle"), win);
	airpitch4texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airpitch4needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airtorque1needle"), win);
	airtorque1texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airtorque1needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airtorque2needle"), win);
	airtorque2texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airtorque2needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airtorque3needle"), win);
	airtorque3texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airtorque3needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	resizePanel(OverlayManager::getSingleton().getOverlayElement("tracks/airtorque4needle"), win);
	airtorque4texture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/airtorque4needle_mat")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);
	//		speedotexture=((MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/speedoneedle")))->getTechnique(0)->getPass(0)->getTextureUnitState(0);

	//        Entity *ecs=scm->createEntity("speed_needle", "needle.mesh");
	//        speed_node=scm->getRootSceneNode()->createChildSceneNode();
	//        speed_node->attachObject(ecs);
	//		needlesOverlay->add3D(speed_node);
	//		placeNeedle(win, speed_node, 1189, 936, 1.0);
	//tacho
	//        Entity *ect=scm->createEntity("tacho_needle", "needle.mesh");
	//        tach_node=scm->getRootSceneNode()->createChildSceneNode();
	//        tach_node->attachObject(ect);
	//		needlesOverlay->add3D(tach_node);
	//		placeNeedle(win, tach_node, 1011, 935, 1.0);
	//		//pitch
	//        Entity *ecp=scm->createEntity("pitch_needle", "needle.mesh");
	//		ecp->setMaterialName("tracks/whiteneedle");
	//        pitch_node=scm->getRootSceneNode()->createChildSceneNode();
	//        pitch_node->attachObject(ecp);
	//		needlesOverlay->add3D(pitch_node);
	//		placeNeedle(win, pitch_node, 876, 1014, 1.0);
	//roll
	//        Entity *ecr=scm->createEntity("roll_needle", "needle.mesh");
	//		ecr->setMaterialName("tracks/whiteneedle");
	//        roll_node=scm->getRootSceneNode()->createChildSceneNode();
	//        roll_node->attachObject(ecr);
	//		needlesOverlay->add3D(roll_node);
	//		placeNeedle(win, roll_node, 876, 924, 1.0);

	//rollcorr
	//		Entity *ecrc=scm->createEntity("rollcorr_needle", "needle.mesh");
	//		ecrc->setMaterialName("tracks/stabneedle");
	//		rollcorr_node=scm->getRootSceneNode()->createChildSceneNode();
	//		rollcorr_node->attachObject(ecrc);
	//		needlesOverlay->add3D(rollcorr_node);
	//		placeNeedle(win, rollcorr_node, 876, 924, 0.8);


	guiGear=OverlayManager::getSingleton().getOverlayElement("tracks/Gear");
	guiGear3D=OverlayManager::getSingleton().getOverlayElement("tracks/3DGear");
	guiRoll=OverlayManager::getSingleton().getOverlayElement("tracks/rollmask");

	guiAuto[0]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/AGearR");
	guiAuto[1]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/AGearN");
	guiAuto[2]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/AGearD");
	guiAuto[3]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/AGear2");
	guiAuto[4]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/AGear1");

	guiAuto3D[0]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/3DAGearR");
	guiAuto3D[1]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/3DAGearN");
	guiAuto3D[2]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/3DAGearD");
	guiAuto3D[3]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/3DAGear2");
	guiAuto3D[4]=(TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/3DAGear1");

	guipedclutch=OverlayManager::getSingleton().getOverlayElement("tracks/pedalclutch");
	guipedbrake=OverlayManager::getSingleton().getOverlayElement("tracks/pedalbrake");
	guipedacc=OverlayManager::getSingleton().getOverlayElement("tracks/pedalacc");

	mRotateSpeed = 100;
	mMoveSpeed = 50;
	//        beam=NULL;
	laptimemin = (TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/LapTimemin");
	laptimes = (TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/LapTimes");
	laptimems = (TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/LapTimems");
	lasttime = (TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/LastTime");

	flashOverlay = OverlayManager::getSingleton().getByName("tracks/FlashMessage");
	flashMessageTE = (TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement("tracks/Message");

	//set up player list
	for(int i=0; i<MAX_PLAYLIST_ENTRIES;i++)
	{
		char tmp[255]="";
		sprintf(tmp, "tracks/MPPlayerList/Player%d", i);
		playerlistOverlay[i] = (TextAreaOverlayElement*)OverlayManager::getSingleton().getOverlayElement(tmp);
		playerlistOverlay[i]->setCaption("");
	}


	/*
	//OIS CONFIGURE
	OIS::ParamList pl;
	size_t windowHnd = 0;
	std::ostringstream windowHndStr;
	win->getCustomAttribute("WINDOW", &windowHnd);
	windowHndStr << windowHnd;
	pl.insert(std::make_pair(std::string("WINDOW"), windowHndStr.str()));
	mInputManager = OIS::InputManager::createInputSystem( pl );

	mMouse = static_cast<OIS::Mouse*>(mInputManager->createInputObject(OIS::OISMouse, false));
	*/

	windowResized(win);
	Ogre::WindowEventUtilities::addWindowEventListener(win, this);


	debugCollisions = (SETTINGS.getSetting("Debug Collisions") == "Yes");
    
    xfire_enabled = (SETTINGS.getSetting("XFire") == "Yes");

	externalCameraMode = (SETTINGS.getSetting("External Camera Mode") == "Static")? 1 : 0;

#ifndef XFIRE
	xfire_enabled = false;
#endif



	// get lights mode
	flaresMode = 0; //None
	if(SETTINGS.getSetting("Lights") == "None (fastest)")
		flaresMode = 0;
	else if(SETTINGS.getSetting("Lights") == "No light sources")
		flaresMode = 1;
	else if(SETTINGS.getSetting("Lights") == "Only current vehicle, main lights")
		flaresMode = 2;
	else if(SETTINGS.getSetting("Lights") == "All vehicles, main lights")
		flaresMode = 3;
	else if(SETTINGS.getSetting("Lights") == "All vehicles, all lights")
		flaresMode = 4;


	// heathaze effect
	heathaze=0;
	if(SETTINGS.getSetting("HeatHaze") == "Yes")
	{
		heathaze=new HeatHaze(scm, win,cam);
		heathaze->setEnable(true);
	}

	// no more force feedback
	// useforce=(SETTINGS.getSetting("Controler Force Feedback")=="Enable");
	if(SETTINGS.getSetting("Screenshot Format")=="" || SETTINGS.getSetting("Screenshot Format")=="jpg (smaller, default)")
		strcpy(screenshotformat, "jpg");
	else if(SETTINGS.getSetting("Screenshot Format")=="png (bigger, no quality loss)")
		strcpy(screenshotformat, "png");
	else
		strncpy(screenshotformat, SETTINGS.getSetting("Screenshot Format").c_str(), 10);

	//Joystick
	/*
	float deadzone=0.1;
	String deadzone_string = SETTINGS.getSetting("Controler Deadzone");
	if (deadzone_string != String("")) {
		deadzone = atof(deadzone_string.c_str());
	}
	*/
	//joy=new BeamJoystick(mInputManager, deadzone, useforce, &cfg);
	//useforce=joy->hasForce();

	mCamera = cam;
	gCamera = cam;
	mWindow = win;
	mStatsOn = false;
	mTruckInfoOn = false;
	mapMode=0;
	mTimeUntilNextToggle = 0;
	mSceneDetailIndex = 0;
	mMoveScale = 0.0f;
	mRotScale = 0.0f;
	camIdealPosition = Vector3::ZERO;
	lastPosition = Vector3::ZERO;
	//lastangle=0;
	camRotX=0;
	camRotY=Degree(12);
	camDist=20;
	clutch=0;
	cameramode=0;
	road=0;

	objcounter=0;

	useIngameEditor = false;

	//network
	netmode=(SETTINGS.getSetting("Network enable")=="Yes" || SETTINGS.getSetting("join URI") != "");
	net=0;

	// preselected map or truck?
	String preselected_map = SETTINGS.getSetting("Preselected Map");
	String preselected_truck = SETTINGS.getSetting("Preselected Truck");

	if(preselected_map != "") LogManager::getSingleton().logMessage("Preselected Map: " + (preselected_map));
	if(preselected_truck != "") LogManager::getSingleton().logMessage("Preselected Truck: " + (preselected_truck));

	//LogManager::getSingleton().logMessage("huette debug 1");

	// hide console when not in netmode
	CONSOLE.setMode(this, CONSOLE_LEFT_SMALL, false);

	if(netmode)
	{
		std::string sname = SETTINGS.getSetting("Server name").c_str();
		long sport = StringConverter::parseLong(SETTINGS.getSetting("Server port"));
		if (sport==0)
		{
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
			MessageBox( NULL, _L("Bad server port").c_str(), _L("A network error occured").c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
#elif OGRE_PLATFORM == OGRE_PLATFORM_LINUX
			printf("\n\nBad server port\n\n");
#elif OGRE_PLATFORM == OGRE_PLATFORM_APPLE
			printf("\n\nBad server port\n\n");
			//CFOptionFlags flgs;
			//CFUserNotificationDisplayAlert(0, kCFUserNotificationStopAlertLevel, NULL, NULL, NULL, T("A network error occured"), T("Bad server port."), NULL, NULL, NULL, &flgs);
#endif
			exit(123);
			return;
		}
		LogManager::getSingleton().logMessage("trying to join server '" + String(sname) + "' on port " + StringConverter::toString(sport) + "'...");

		UILOADER.setProgress(UI_PROGRESSBAR_AUTOTRACK, _L("Trying to connect to server ..."));
		// important note: all new network code is written in order to allow also the old network protocol to further exist.
		// at some point you need to decide with what type of server you communicate below and choose the correct class
#ifdef NEWNET
		net = new NetworkNew(trucks, sname, sport, this);
#else
		net = new Network(trucks, sname, sport, this);
#endif
		bool connres = net->connect();
		UILOADER.setProgress(UI_PROGRESSBAR_HIDE);
		if(!connres)
		{
			LogManager::getSingleton().logMessage("connection failed. server down?");
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
			String err = _L("Unable to connect to the server. It is certainly down or you have network problems.");
			String title = _L("Unable to connect to server");
			MessageBox( NULL, err.c_str(), title.c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
#endif
			//fatal
			exit(1);
		}
		char *terrn = net->getTerrainName();
		if(terrn && !strcmp(terrn, "any"))
			// so show the terrain selection
			preselected_map = "";
		else
			preselected_map = String(terrn);

	}
	//LogManager::getSingleton().logMessage("huette debug 2");

	if(preselected_map != "")
	{
		char mapname_cstr[1024];
		sprintf(mapname_cstr, "%s.terrn", preselected_map.c_str());
		String mapname = String(mapname_cstr);
		if(!CACHE.checkResourceLoaded(mapname))
		{
			LogManager::getSingleton().logMessage("Terrain not found: " + mapname);
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
			String msg = String(_L("Terrain not found: ")) + mapname;
			MessageBox( NULL, msg.c_str(), _L("Terrain loading error").c_str(), MB_OK | MB_ICONERROR | MB_TASKMODAL);
#elif OGRE_PLATFORM == OGRE_PLATFORM_LINUX
			printf("\n\nTerrain not found: %s\n\n", mapname.c_str());
#elif OGRE_PLATFORM == OGRE_PLATFORM_APPLE
			printf("\n\nTerrain not found: %s\n\n", mapname.c_str());
#endif
			exit(123);
		}

		loadTerrain(mapname);
		//miniature map stuff
		//char ppname[1024];
		//sprintf(ppname, "%s-mini.png", preselected_map);
		//MaterialPtr tmat=(MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/Map"));

		//search if mini picture exists
		//FileInfoListPtr files= ResourceGroupManager::getSingleton().findResourceFileInfo(ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME`, ppname);
		//if ( && !files->empty())
		//{
		//	tmat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(ppname);
		//}
		//else
		//{
		//	tmat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName("unknown.png");
		//}

		//load preselected truck
		if(preselected_truck != "")
		{
			loading_state=TERRAIN_LOADED;
			initTrucks(true, preselected_truck.c_str());
		} else {
			// no trucks loaded?
			if (truck_preload_num == 0 || netmode)
			{
				// show truck selector
				UILOADER.setEnableCancel(false);
				if(w)
				{
					hideMap();
					UILOADER.show(GUI_Loader::LT_NetworkWithBoat);
				}
				else
				{
					hideMap();
					UILOADER.show(GUI_Loader::LT_Network);
				}
			} else {
				// init no trucks, as there were found some
				initTrucks(false, mapname);
			}
		}
	} else
	{
		// show terrain selector
		hideMap();
		//LogManager::getSingleton().logMessage("huette debug 3");
		UILOADER.show(GUI_Loader::LT_Terrain);
		UILOADER.setEnableCancel(false);
	}

	//LogManager::getSingleton().logMessage("ngolo1");

	// load 3d line for mouse picking
	pickLine =  mSceneMgr->createManualObject("PickLineObject");
	pickLineNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("PickLineNode");

	MaterialPtr pickLineMaterial = MaterialManager::getSingleton().create("PickLineMaterial","debugger");
	pickLineMaterial->setReceiveShadows(false);
	pickLineMaterial->getTechnique(0)->setLightingEnabled(true);
	pickLineMaterial->getTechnique(0)->getPass(0)->setDiffuse(0,0,1,0);
	pickLineMaterial->getTechnique(0)->getPass(0)->setAmbient(0,0,1);
	pickLineMaterial->getTechnique(0)->getPass(0)->setSelfIllumination(0,0,1);

	pickLine->begin("PickLineMaterial", Ogre::RenderOperation::OT_LINE_LIST);
	pickLine->position(0, 0, 0);
	pickLine->position(0, 0, 0);
	pickLine->end();

	pickLineNode->attachObject(pickLine);
	pickLineNode->setVisible(false);


	initialized=true;
	//        showDebugOverlay(true);
}

ExampleFrameListener::~ExampleFrameListener()
{
//	if (joy) delete (joy);
#ifdef PAGED
	for(std::vector<paged_geometry_t>::iterator it=pagedGeometry.begin(); it!=pagedGeometry.end(); it++)
	{
		if(it->geom)
		{
			delete(it->geom);
			it->geom=0;
		}
		if(it->loader)
		{
			delete(it->loader);
			it->loader=0;
		}
	}
#endif
	if (net) delete (net);
	//we should destroy OIS here
//we could also try to destroy SoundScriptManager, but we don't care!
}


void ExampleFrameListener::loadNetTerrain(char *preselected_map)
{
	// load preselected map
	char mapname[1024];
	sprintf(mapname, "%s.terrn", preselected_map);
	loadTerrain(mapname);
	//miniature map stuff
	MaterialPtr tmat=(MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/Map"));
	//search if mini picture exists
	char ppname[1024];
	sprintf(ppname, "%s-mini.dds", preselected_map);
	String group="";
	if(group == "")
	{
		sprintf(ppname, "%s-mini.png", preselected_map);
		try
		{
			group = ResourceGroupManager::getSingleton().findGroupContainingResource(ppname);
		}catch(...)
		{
		}
	}
	if(group == "")
		return;
	FileInfoListPtr files= ResourceGroupManager::getSingleton().findResourceFileInfo(group, ppname);
	if (!files->empty())
	{
		tmat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(ppname);
	}
	else
	{
		tmat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName("unknown.dds");
	}
}



/*	void ExampleFrameListener::loadObject(char* name, float px, float py, float pz, float rx, float ry, float rz, SceneNode * bakeNode, bool enable_collisions=true)
{
//FILE *fd;
char fname[1024];
char oname[1024];
char mesh[1024];
char line[1024];
float scx, scy, scz;
float lx, hx, ly, hy, lz, hz;
float srx, sry, srz;
Quaternion rotation;
bool isdoor=false;
rotation=Quaternion(Degree(rx), Vector3::UNIT_X)*Quaternion(Degree(ry), Vector3::UNIT_Y)*Quaternion(Degree(rz), Vector3::UNIT_Z);

sprintf(fname,"%s.odef", name);
//fd=fopen(fname, "r");
ResourceGroupManager& rgm = ResourceGroupManager::getSingleton();
DataStreamPtr ds=rgm.openResource(fname, DEFAULT_RESOURCE_GROUP_NAME);
//mesh
//fscanf(fd," %[^\n\r]",mesh);
ds->readLine(mesh, 1023);

//scale
//fscanf(fd," %[^\n\r]",line);
ds->readLine(line, 1023);
sscanf(line, "%f, %f, %f",&scx,&scy,&scz);
//race stuff
if (!strncmp(name, "chp-start", 9))
{
//a new race!
collisions->newRace();
}
//collision box(es)
if (enable_collisions)
{
//while (!feof(fd))
while (!ds->eof())
{
int resp;
char type='n';
//				fscanf(fd," %[^\n\r]",line);
size_t ll=ds->readLine(line, 1023);
if (ll==0) continue;
if (!strcmp("end",line)) break;
resp=sscanf(line, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %c",&lx,&hx,&ly, &hy,&lz, &hz, &srx, &sry, &srz,&type);
collisions->addCollisionBox(resp,px,py,pz,rx,ry,rz,lx,hx,ly,hy,lz,hz,srx,sry,srz,type, scx, scy, scz);
if (type=='d') isdoor=true;
}
}
//fclose(fd);
//ds->close();

sprintf(oname,"object%i(%s)", objcounter,name);
objcounter++;
Entity *te = mSceneMgr->createEntity(oname, mesh);
//		if (!strncmp(name, "road", 4)&&mSceneMgr->getShadowTechnique()==SHADOWTYPE_TEXTURE_MODULATIVE) te->setCastShadows(false);
SceneNode *tenode;
if (bakeNode && !isdoor)
{
//this is a road, add to a special node that will be baked
tenode=bakeNode->createChildSceneNode();
}
else
{
te->setNormaliseNormals(true);
tenode=mSceneMgr->getRootSceneNode()->createChildSceneNode();
}
tenode->attachObject(te);
tenode->setScale(scx,scy,scz);
tenode->setPosition(px,py,pz);
tenode->rotate(rotation);
tenode->pitch(Degree(-90));
//door
if (isdoor)
{
collisions->setDoor(tenode);
}

}
*/

void ExampleFrameListener::getMeshInformation(Mesh* mesh,size_t &vertex_count,Vector3* &vertices,
											  size_t &index_count, unsigned* &indices,
											  const Vector3 &position,
											  const Quaternion &orient,const Vector3 &scale)
{
	vertex_count = index_count = 0;

	bool added_shared = false;
	size_t current_offset = vertex_count;
	size_t shared_offset = vertex_count;
	size_t next_offset = vertex_count;
	size_t index_offset = index_count;
	size_t prev_vert = vertex_count;
	size_t prev_ind = index_count;

	// Calculate how many vertices and indices we're going to need
	for(int i = 0;i < mesh->getNumSubMeshes();i++)
	{
		SubMesh* submesh = mesh->getSubMesh(i);

		// We only need to add the shared vertices once
		if(submesh->useSharedVertices)
		{
			if(!added_shared)
			{
				VertexData* vertex_data = mesh->sharedVertexData;
				vertex_count += vertex_data->vertexCount;
				added_shared = true;
			}
		}
		else
		{
			VertexData* vertex_data = submesh->vertexData;
			vertex_count += vertex_data->vertexCount;
		}

		// Add the indices
		Ogre::IndexData* index_data = submesh->indexData;
		index_count += index_data->indexCount;
	}

	// Allocate space for the vertices and indices
	vertices = new Vector3[vertex_count];
	indices = new unsigned[index_count];

	added_shared = false;

	// Run through the submeshes again, adding the data into the arrays
	for(int i = 0;i < mesh->getNumSubMeshes();i++)
	{
		SubMesh* submesh = mesh->getSubMesh(i);

		Ogre::VertexData* vertex_data = submesh->useSharedVertices ? mesh->sharedVertexData : submesh->vertexData;
		if((!submesh->useSharedVertices)||(submesh->useSharedVertices && !added_shared))
		{
			if(submesh->useSharedVertices)
			{
				added_shared = true;
				shared_offset = current_offset;
			}

			const Ogre::VertexElement* posElem = vertex_data->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
			Ogre::HardwareVertexBufferSharedPtr vbuf = vertex_data->vertexBufferBinding->getBuffer(posElem->getSource());
			unsigned char* vertex = static_cast<unsigned char*>(vbuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
			Ogre::Real* pReal;

			for(size_t j = 0; j < vertex_data->vertexCount; ++j, vertex += vbuf->getVertexSize())
			{
				posElem->baseVertexPointerToElement(vertex, &pReal);

				Vector3 pt;

				pt.x = (*pReal++);
				pt.y = (*pReal++);
				pt.z = (*pReal++);

				pt = (orient * (pt * scale)) + position;

				vertices[current_offset + j].x = pt.x;
				vertices[current_offset + j].y = pt.y;
				vertices[current_offset + j].z = pt.z;
			}
			vbuf->unlock();
			next_offset += vertex_data->vertexCount;
		}

		Ogre::IndexData* index_data = submesh->indexData;

		size_t numTris = index_data->indexCount / 3;
		unsigned short* pShort;
		unsigned int* pInt;
		Ogre::HardwareIndexBufferSharedPtr ibuf = index_data->indexBuffer;
		bool use32bitindexes = (ibuf->getType() == Ogre::HardwareIndexBuffer::IT_32BIT);
		if (use32bitindexes) pInt = static_cast<unsigned int*>(ibuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));
		else pShort = static_cast<unsigned short*>(ibuf->lock(Ogre::HardwareBuffer::HBL_READ_ONLY));

		for(size_t k = 0; k < numTris; ++k)
		{
			size_t offset = (submesh->useSharedVertices)?shared_offset:current_offset;

			unsigned int vindex = use32bitindexes? *pInt++ : *pShort++;
			indices[index_offset + 0] = vindex + offset;
			vindex = use32bitindexes? *pInt++ : *pShort++;
			indices[index_offset + 1] = vindex + offset;
			vindex = use32bitindexes? *pInt++ : *pShort++;
			indices[index_offset + 2] = vindex + offset;

			index_offset += 3;
		}
		ibuf->unlock();
		current_offset = next_offset;
	}
}
void ExampleFrameListener::loadObject(char* name, float px, float py, float pz, float rx, float ry, float rz, SceneNode * bakeNode, char* instancename, bool enable_collisions, int luahandler, char *type)
{
	if(strnlen(name, 250)==0)
		return;

	//FILE *fd;
	char fname[1024];
	char oname[1024];
	char mesh[1024];
	char line[1024];
	float scx, scy, scz;
	float lx, hx, ly, hy, lz, hz;
	float srx, sry, srz;
	float drx, dry, drz;
	float fcx, fcy, fcz;
	bool forcecam=false;
	char collmesh[1024];
	Quaternion rotation;
	bool ismovable=false;
	int event_filter = EVENT_ALL;
	rotation=Quaternion(Degree(rx), Vector3::UNIT_X)*Quaternion(Degree(ry), Vector3::UNIT_Y)*Quaternion(Degree(rz), Vector3::UNIT_Z);

	// try to load with UID first!
	String odefgroup = "";
	String odefname = "";
	bool odefFound = false;
	if(terrainUID != "" && !CACHE.stringHasUID(name))
	{
		sprintf(fname,"%s-%s.odef", terrainUID.c_str(), name);
		odefname = String(fname);
		try
		{
			odefgroup = ResourceGroupManager::getSingleton().findGroupContainingResource(odefname);
			odefFound = true;
		} catch(...)
		{
			odefFound=false;
		}
	}

	if(!odefFound)
	{
		sprintf(fname,"%s.odef", name);
		odefname = String(fname);
		try
		{
			odefgroup = ResourceGroupManager::getSingleton().findGroupContainingResource(odefname);
			odefFound=true;
		} catch(...)
		{
			odefFound=false;
		}
	}
	
	//if(!CACHE.checkResourceLoaded(odefname, odefgroup))
	if(!odefFound)
	{
		LogManager::getSingleton().logMessage("Error while loading Terrain: could not find required .odef file: " + odefname + ". Ignoring entry.");
		return;
	}

	DataStreamPtr ds=ResourceGroupManager::getSingleton().openResource(odefname, odefgroup);
	//mesh
	ds->readLine(mesh, 1023);

	//scale
	ds->readLine(line, 1023);
	sscanf(line, "%f, %f, %f",&scx,&scy,&scz);
	sprintf(oname,"object%i(%s)", objcounter,name);
	objcounter++;
	Entity *te = mSceneMgr->createEntity(oname, mesh);
	//		if (!strncmp(name, "road", 4)&&mSceneMgr->getShadowTechnique()==SHADOWTYPE_TEXTURE_MODULATIVE) te->setCastShadows(false);
	SceneNode *tenode;
	if (bakeNode && !ismovable) //FIXME
	{
		//add to a special node that will be baked
		tenode=bakeNode->createChildSceneNode();
	}
	else
	{
		te->setNormaliseNormals(true);
		tenode=mSceneMgr->getRootSceneNode()->createChildSceneNode();
	}
	tenode->attachObject(te);
	tenode->setScale(scx,scy,scz);
	tenode->setPosition(px,py,pz);
	tenode->rotate(rotation);
	tenode->pitch(Degree(-90));
	//collision box(es)
	bool virt=false;
	bool rotating=false;
	bool classic_ref=true;
	ground_model_t *gm=&GROUND_CONCRETE;
	ground_model_t gmi;
	char eventname[256];
	eventname[0]=0;
	while (!ds->eof())
	{
		size_t ll=ds->readLine(line, 1023);
		char* ptline=line;
		if (ll==0 || line[0]=='/' || line[0]==';') continue;
		//trim line
		while (*ptline==' ' || *ptline=='\t') ptline++;
		if (!strcmp("end",ptline)) break;
		if (!strcmp("movable", ptline)) {ismovable=true;continue;};
		if (!strcmp("localizer-h", ptline))
		{
			localizers[free_localizer].position=Vector3(px,py,pz);
			localizers[free_localizer].rotation=rotation;
			localizers[free_localizer].type=LOCALIZER_HORIZONTAL;
			free_localizer++;
			continue;
		}
		if (!strcmp("localizer-v", ptline))
		{
			localizers[free_localizer].position=Vector3(px,py,pz);
			localizers[free_localizer].rotation=rotation;
			localizers[free_localizer].type=LOCALIZER_VERTICAL;
			free_localizer++;
			continue;
		}
		if (!strcmp("localizer-ndb", ptline))
		{
			localizers[free_localizer].position=Vector3(px,py,pz);
			localizers[free_localizer].rotation=rotation;
			localizers[free_localizer].type=LOCALIZER_NDB;
			free_localizer++;
			continue;
		}
		if (!strcmp("localizer-vor", ptline))
		{
			localizers[free_localizer].position=Vector3(px,py,pz);
			localizers[free_localizer].rotation=rotation;
			localizers[free_localizer].type=LOCALIZER_VOR;
			free_localizer++;
			continue;
		}
		if (!strcmp("standard", ptline)) {classic_ref=false;tenode->pitch(Degree(90));continue;};
		if (!strcmp("beginbox", ptline) || !strcmp("beginmesh", ptline))
		{
			drx=dry=drz=0.0;
			rotating=false;
			virt=false;
			forcecam=false;
			event_filter=EVENT_ALL;
			eventname[0]=0;
			collmesh[0]=0;
			gm=&GROUND_CONCRETE;
			continue;
		};
		if (!strncmp("boxcoords", ptline, 9))
		{
			sscanf(ptline, "boxcoords %f, %f, %f, %f, %f, %f",&lx,&hx,&ly, &hy,&lz, &hz);
			continue;
		}
		if (!strncmp("mesh", ptline, 4))
		{
			sscanf(ptline, "mesh %s",collmesh);
			continue;
		}
		if (!strncmp("rotate", ptline, 6))
		{
			sscanf(ptline, "rotate %f, %f, %f",&srx, &sry, &srz);
			rotating=true;
			continue;
		}
		if (!strncmp("forcecamera", ptline, 11))
		{
			sscanf(ptline, "forcecamera %f, %f, %f",&fcx, &fcy, &fcz);
			forcecam=true;
			continue;
		}
		if (!strncmp("direction", ptline, 9))
		{
			sscanf(ptline, "direction %f, %f, %f",&drx, &dry, &drz);
			continue;
		}
		if (!strncmp("friction", ptline, 8))
		{
			collisions->parseGroundModel(&gmi, ptline+9, "custom");
//				sscanf(ptline, "friction %f, %f, %f, %f, %f",&gmi.ms, &gmi.mc, &gmi.t2, &gmi.vs, &gmi.alpha);
			gm=&gmi;
			continue;
		}
		if (!strncmp("stdfriction", ptline, 11))
		{
			char stdf[256];
			sscanf(ptline, "stdfriction %s",stdf);
			if (!strcmp("concrete", stdf)) gm=&GROUND_CONCRETE;
			if (!strcmp("asphalt", stdf)) gm=&GROUND_ASPHALT;
			if (!strcmp("gravel", stdf)) gm=&GROUND_GRAVEL;
			if (!strcmp("rock", stdf)) gm=&GROUND_ROCK;
			if (!strcmp("ice", stdf)) gm=&GROUND_ICE;
			if (!strcmp("snow", stdf)) gm=&GROUND_SNOW;
			if (!strcmp("metal", stdf)) gm=&GROUND_METAL;
			if (!strcmp("grass", stdf)) gm=&GROUND_GRASS;
			if (!strcmp("sand", stdf)) gm=&GROUND_SAND;
			continue;
		}
		if (!strcmp("virtual", ptline)) {virt=true;continue;};
		if (!strncmp("event", ptline, 5))
		{
			char ts[256];
			ts[0]=0;
			sscanf(ptline, "event %s %s",eventname, ts);
			if(!strncmp(ts, "avatar", 6))
				event_filter=EVENT_AVATAR;
			else if(!strncmp(ts, "truck", 5))
				event_filter=EVENT_TRUCK;
			else if(!strncmp(ts, "airplane", 8))
				event_filter=EVENT_AIRPLANE;
			continue;
		}
		//resp=sscanf(ptline, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %c",&lx,&hx,&ly, &hy,&lz, &hz, &srx, &sry, &srz,&type);
		if (!strcmp("endbox", ptline))
		{
			if (enable_collisions) collisions->addCollisionBox(tenode, rotating, virt,px,py,pz,rx,ry,rz,lx,hx,ly,hy,lz,hz,srx,sry,srz,eventname, instancename, forcecam, Vector3(fcx, fcy, fcz), scx, scy, scz, drx, dry, drz, event_filter, luahandler);
			continue;
		}
		if (!strcmp("endmesh", ptline))
		{
			//oookay
			static int colmeshcounter = 0;
			colmeshcounter++;
			char colmeshname[255]="";
			sprintf(colmeshname, "collision_mesh_%d", colmeshcounter);
			Entity *ent = mSceneMgr->createEntity(colmeshname, collmesh);
			ent->setMaterialName("tracks/transred");

			size_t vertex_count,index_count;
			Vector3* vertices;
			unsigned* indices;

			getMeshInformation(ent->getMesh().getPointer(),vertex_count,vertices,index_count,indices, Vector3(px,py,pz), tenode->getOrientation(), Vector3(scx, scy, scz));

			//LogManager::getSingleton().logMessage(LML_NORMAL,"Vertices in mesh: %u",vertex_count);
			//LogManager::getSingleton().logMessage(LML_NORMAL,"Triangles in mesh: %u",index_count / 3);
			for (int i=0; i<(int)index_count/3; i++)
			{
				if (enable_collisions) collisions->addCollisionTri(vertices[indices[i*3]], vertices[indices[i*3+1]], vertices[indices[i*3+2]], gm);
			}

			delete[] vertices;
			delete[] indices;
			if(!debugCollisions)
			{
				mSceneMgr->destroyEntity(colmeshname);
			} else
			{
				SceneNode *colbakenode=mSceneMgr->getRootSceneNode()->createChildSceneNode();
				colbakenode->attachObject(ent);
				colbakenode->setPosition(Vector3(px,py,pz));
				colbakenode->setOrientation(tenode->getOrientation());
			}

			continue;
		}
		if (!strncmp("setMeshMaterial", ptline, 15))
		{
			char mat[255]="";
			sscanf(ptline, "setMeshMaterial %s", mat);
			if(te && strnlen(mat,250)>0)
			{
				te->setMaterialName(String(mat));
				// load it
				//MaterialManager::getSingleton().load(String(mat), ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
			}
			continue;
		}
		if (!strncmp("playanimation", ptline, 13))
		{
			char animname[255]="";
			float speedfactorMin = 0, speedfactorMax = 0;
			sscanf(ptline, "playanimation %f, %f, %s", &speedfactorMin, &speedfactorMax, animname);
			if(tenode && te && strnlen(animname,250)>0)
			{
				AnimationStateSet *s = te->getAllAnimationStates();
				if(!s->hasAnimationState(String(animname)))
				{
					LogManager::getSingleton().logMessage("ODEF: animation '" + String(animname) + "' for mesh: '" + String(mesh) + "' in odef file '" + String(name) + ".odef' not found!");
					continue;
				}
				animated_object_t ao;
				ao.node = tenode;
				ao.ent = te;
				ao.speedfactor = speedfactorMin;
				if(speedfactorMin != speedfactorMax)
					ao.speedfactor = Math::RangeRandom(speedfactorMin, speedfactorMax);
				ao.anim = 0;
				try
				{
					ao.anim = te->getAnimationState(String(animname));
				} catch (...)
				{
					ao.anim = 0;
				}
				if(!ao.anim)
				{
					LogManager::getSingleton().logMessage("ODEF: animation '" + String(animname) + "' for mesh: '" + String(mesh) + "' in odef file '" + String(name) + ".odef' not found!");
					continue;
				}
				ao.anim->setEnabled(true);
				animatedObjects.push_back(ao);
			}
			continue;
		}
		if (!strncmp("drawTextOnMeshTexture", ptline, 21))
		{
			if(!te)
				continue;
			String matName = te->getSubEntity(0)->getMaterialName();
			MaterialPtr m = MaterialManager::getSingleton().getByName(matName);
			if(m.getPointer() == 0)
			{
				LogManager::getSingleton().logMessage("ODEF: problem with drawTextOnMeshTexture command: mesh material not found: "+String(fname)+" : "+String(ptline));
				continue;
			}
			String texName = m->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getTextureName();
			Texture* background = (Texture *)TextureManager::getSingleton().getByName(texName).getPointer();
			if(!background)
			{
				LogManager::getSingleton().logMessage("ODEF: problem with drawTextOnMeshTexture command: mesh texture not found: "+String(fname)+" : "+String(ptline));
				continue;
			}

			static int textureNumber = 0;
			textureNumber++;
			char tmpTextName[255]="", tmpMatName[255]="";
			sprintf(tmpTextName, "TextOnTexture_%d_Texture", textureNumber);
			sprintf(tmpMatName, "TextOnTexture_%d_Material", textureNumber);			// Make sure the texture is not WRITE_ONLY, we need to read the buffer to do the blending with the font (get the alpha for example)
			TexturePtr texture = TextureManager::getSingleton().createManual(tmpTextName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, TEX_TYPE_2D, (Ogre::uint)background->getWidth(), (Ogre::uint)background->getHeight(), MIP_UNLIMITED , PF_X8R8G8B8, Ogre::TU_STATIC|Ogre::TU_AUTOMIPMAP, new ResourceBuffer());
			if(texture.getPointer() == 0)
			{
				LogManager::getSingleton().logMessage("ODEF: problem with drawTextOnMeshTexture command: could not create texture: "+String(fname)+" : "+String(ptline));
				continue;
			}

			float x=0, y=0, w=0, h=0;
			float a=0, r=0, g=0, b=0;
			char fontname[255]="";
			char text[255]="";
			char option='l';
			int res = sscanf(ptline, "drawTextOnMeshTexture %f, %f, %f, %f, %f, %f, %f, %f, %c, %s %s", &x, &y, &w, &h, &r, &g, &b, &a, &option, fontname, text);
			if(res < 11)
			{
				LogManager::getSingleton().logMessage("ODEF: problem with drawTextOnMeshTexture command: "+String(fname)+" : "+String(ptline));
				continue;
			}

			// cehck if we got a template argument
			if(!strncmp(text, "{{argument1}}", 13))
				strncpy(text, instancename, 250);

			// replace '_' with ' '
			char *text_pointer = text;
			while (*text_pointer!=0) {if (*text_pointer=='_') *text_pointer=' ';text_pointer++;};

			Font* font = (Font *)FontManager::getSingleton().getByName(String(fontname)).getPointer();
			if(!font)
			{
				LogManager::getSingleton().logMessage("ODEF: problem with drawTextOnMeshTexture command: font not found: "+String(fname)+" : "+String(ptline));
				continue;
			}


			//Draw the background to the new texture
			texture->getBuffer()->blit(background->getBuffer());

			x = background->getWidth() * x;
			y = background->getHeight() * y;
			w = background->getWidth() * w;
			h = background->getHeight() * h;

      Image::Box box = Image::Box((size_t)x, (size_t)y, (size_t)(x+w), (size_t)(y+h));
      WriteToTexture(String(text), texture, box, font, ColourValue(r, g, b, a), option);

			// we can save it to disc for debug purposes:
			//SaveImage(texture, "test.png");

			m->clone(tmpMatName);
			MaterialPtr mNew = MaterialManager::getSingleton().getByName(tmpMatName);
			mNew->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(tmpTextName);

			te->setMaterialName(String(tmpMatName));
			continue;
		}

		LogManager::getSingleton().logMessage("ODEF: unknown command in "+String(fname)+" : "+String(ptline));
	}

	// ds closes automatically, so do not close it explicitly here:
	//ds->close();

	//add icons if type is set
	String typestr = "";
	if(type && bigMap)
	{
		typestr = String(type);
		// hack for raceways
		if (!strcmp(name, "chp-checkpoint"))
			typestr = "checkpoint";
		if (!strcmp(name, "chp-start"))
			typestr = "racestart";
		if (!strncmp(name, "road", 4))
			typestr = "road";

		if(typestr != String("") && typestr != String("road") && typestr != String("sign"))
		{
			MapEntity *e = bigMap->createMapEntity(typestr);
			e->setVisibility(true);
			e->setPosition(px, pz);
			e->setRotation(ry);
			if(typestr == String("road")) //how can this be possible ?!?
			{
				e->setScale(0.5);
				e->setMinSize(5);
			} else
			{
				e->setScale(1);
				e->setMinSize(10);
			}
			if(String(name) != String(""))
				e->setDescription(String(instancename));
		}
	}
}

void ExampleFrameListener::repairTruck(char* inst, char* box)
{
	//find the right truck (the one in the box)
	int t;
	int rtruck=-1;
	for (t=0; t<free_truck; t++)
	{
		if (collisions->isInside(trucks[t]->nodes[0].AbsPosition, inst, box))
		{
			//we found one
			if (rtruck==-1) rtruck=t;
			else rtruck=-2; //too much trucks!
		}
	}
	if (rtruck>=0)
	{
		//take a position reference
		ssm->trigOnce(rtruck, SS_TRIG_REPAIR);
		Vector3 ipos=trucks[rtruck]->nodes[0].AbsPosition;
		trucks[rtruck]->reset();
		trucks[rtruck]->resetPosition(ipos.x, ipos.z, false);
		trucks[rtruck]->updateVisual();
	}
}

bool ExampleFrameListener::updateEvents(float dt)
{
	if(!INPUTENGINE.getInputsChanged())
		return true;
	bool dirty=false;
	if (dt==0.0) return true;
	//update joystick readings
//	joy->UpdateInputState();
	//stick shift general uglyness
	/*
	// no more stickshift, commented out when upgrading to the inputengine
	if (loading_state==ALL_LOADED && current_truck!=-1 && trucks[current_truck]->driveable==TRUCK && trucks[current_truck]->engine->getAutoMode()==MANUAL)
	{
		int gb;
		gb=joy->updateStickShift(true, trucks[current_truck]->engine->getClutch());
		// TODO: FIXME
		//if (gb!=-1) trucks[current_truck]->engine->setGear(gb);
	}
	else joy->updateStickShift(false, 0.0);
	*/
	//flashing message
	if (timeUntilUnflash>0)
	{
		timeUntilUnflash-=dt;
	}
	else
	{
		if (flashOverlay->isVisible()) flashOverlay->hide();
	}


	if (CONSOLE.getVisible() && INPUTENGINE.getEventBoolValue("COMMON_ENTER_CHAT") && mTimeUntilNextToggle <= 0 && !hidegui)
	{
		if (chatting)
		{
			processConsoleInput();
			CONSOLE.setEnterText("", false);
			mTimeUntilNextToggle = 0.5;
			chatting=false;
			INPUTENGINE.setRecordInput(false);
			INPUTENGINE.resetKeyLine();
		}
		else
		{
			INPUTENGINE.resetKeyLine();
			INPUTENGINE.setRecordInput(true);
			CONSOLE.setEnterText("", true, true);
			chatting=true;
			mTimeUntilNextToggle = 0.5;
		}
	}

	// nno evnet handling during chatting!
	if(chatting)
		return true;

	if (INPUTENGINE.getEventBoolValue("COMMON_SHOWTRUCKTOOL") && mTimeUntilNextToggle <= 0 && current_truck != -1)
	{
		//if(truckToolGUI)
		//	truckToolGUI->show();
		mTimeUntilNextToggle = 0.5;
	}

	if (INPUTENGINE.getEventBoolValue("COMMON_RELOAD_ROADS") && mTimeUntilNextToggle <= 0)
	{
		if(proceduralManager)
		{
			//proceduralManager->deleteAllObjects();
			proceduralManager->updateAllObjects();
		}
		mTimeUntilNextToggle = 0.5;
	}

	if (INPUTENGINE.getEventBoolValue("COMMON_SCREENSHOT") && mTimeUntilNextToggle <= 0)
	{
		int mNumScreenShots=0;
		String tmp = SETTINGS.getSetting("User Path") + String("screenshot_") + StringConverter::toString(++mNumScreenShots) + String(".") + String(screenshotformat);
		while(fileExists(const_cast<char*>(tmp.c_str())))
			tmp = SETTINGS.getSetting("User Path") + String("screenshot_") + StringConverter::toString(++mNumScreenShots) + String(".") + String(screenshotformat);

		LogManager::getSingleton().logMessage("Written screenshot : " + tmp);
		// hide any flash message
		flashMessage(0);
		mWindow->update();

		mWindow->writeContentsToFile(tmp);
		mTimeUntilNextToggle = 0.5;
		char tmp1[255];
		String ssmsg = _L("wrote screenshot:");
		sprintf(tmp1, "%s %d", ssmsg.c_str(), mNumScreenShots);
		flashMessage(tmp1);
	}

	if (INPUTENGINE.getEventBoolValue("COMMON_FULLSCREEN_TOGGLE") && mTimeUntilNextToggle <= 0)
	{
		static int org_width = -1, org_height = -1;
		int width = mWindow->getWidth();
		int height = mWindow->getHeight();
		if(org_width == -1)
			org_width = width;
		if(org_height == -1)
			org_height = height;
		bool mode = mWindow->isFullScreen();
		if(!mode)
		{
			mWindow->setFullscreen(true, org_width, org_height);
			LogManager::getSingleton().logMessage(" ** switched to fullscreen: "+StringConverter::toString(width)+"x"+StringConverter::toString(height));
		} else
		{
			mWindow->setFullscreen(false, 640, 480);
			mWindow->setFullscreen(false, org_width, org_height);
			LogManager::getSingleton().logMessage(" ** switched to windowed mode: ");
		}
		mTimeUntilNextToggle = 2;
	}

	if (INPUTENGINE.getEventBoolValue("CAMERA_FREE_MODE_FIX") && mTimeUntilNextToggle <= 0)
	{
		if(cameramode == CAMERA_FREE)
		{
			// change to fixed free camera: that is working like fixed cam
			cameramode = CAMERA_FREE_FIXED;
			LogManager::getSingleton().logMessage("switching to fixed free camera mode");
			flashMessage(_L("fixed free camera"));
		} else if(cameramode == CAMERA_FREE_FIXED)
		{
			cameramode = CAMERA_FREE;
			LogManager::getSingleton().logMessage("switching to free camera mode from fixed mode");
			flashMessage(_L("free camera").c_str());
		}
		mTimeUntilNextToggle = 0.2;
	}
	if (INPUTENGINE.getEventBoolValue("CAMERA_FREE_MODE") && mTimeUntilNextToggle <= 0)
	{
		static int storedcameramode = -1;
		if(cameramode == CAMERA_FREE || cameramode == CAMERA_FREE_FIXED)
		{
			// change back to normal camera
			cameramode = storedcameramode;
			LogManager::getSingleton().logMessage("exiting free camera mode");
			flashMessage(_L("normal camera").c_str());
		} else if(cameramode != CAMERA_FREE && cameramode != CAMERA_FREE_FIXED )
		{
			// enter free camera mode
			storedcameramode = cameramode;
			cameramode = CAMERA_FREE;
			LogManager::getSingleton().logMessage("entering free camera mode");
			flashMessage(_L("free camera").c_str());
		}
		mTimeUntilNextToggle = 0.2;
	}

	if (loading_state==ALL_LOADED)
	{
		if (INPUTENGINE.getEventBoolValue("INGAMEEDITOR_SHOW") && mTimeUntilNextToggle <= 0 && !netmode)
		{
			useIngameEditor = ! useIngameEditor;
			if(useIngameEditor)
			{
				if(current_truck != -1)
				{
					if(!INGAMEEDITOR.wasSetup())
						INGAMEEDITOR.setup(this);
					INGAMEEDITOR.show(true);
					loading_state = EDITOR_PAUSE;
					flashMessage(_L("Press Y again to exit the editor!\nPress I for help.").c_str(), 3);
				} else {
					useIngameEditor = false;
				}
			} else {
				loading_state = ALL_LOADED;
				INGAMEEDITOR.show(false);
			}
			mTimeUntilNextToggle = 1;
		}
		// if the ingame editor is used, no normal event handling will occur
		bool enablegrab = true;
		if (!useIngameEditor && !(cameramode==CAMERA_FREE))
		{
			MYGUI.setCursorPosition(mouseX, mouseY);
			if (current_truck==-1)
			{
				if(person)
					person->update(dt);
				//camera mode
				if (INPUTENGINE.getEventBoolValue("CAMERA_CHANGE") && mTimeUntilNextToggle <= 0)
				{
					if (cameramode==CAMERA_INT)
					{
						//end of internal cam
						camRotX=pushcamRotX;
						camRotY=pushcamRotY;
					}
					cameramode++;
					if (cameramode==CAMERA_INT)
					{
						//start of internal cam
						pushcamRotX=camRotX;
						pushcamRotY=camRotY;
						camRotX=0;
						camRotY=DEFAULT_INTERNAL_CAM_PITCH;
					}
					if (cameramode==CAMERA_END) cameramode=0;
					mTimeUntilNextToggle = 0.2;
				}
			}
			else //we are in a vehicle
			{
				//the mouse stuff
				const OIS::MouseState mstate = INPUTENGINE.getMouseState();
				static bool buttonsPressed = false;
				int click=-1;
				// click = -1 = nothing clicked
				// click = 0 left click (windows)
				// click = 1 right click (linux)

				// do not move the mouse while rotating the view
				static int oldstate = 0;
				// oldstate = 0 : dragging view to rotate
				// oldstate = 1 : moving mouse
				// oldstate = 2 : picking node
				//LogManager::getSingleton().logMessage("oldstate="+StringConverter::toString(oldstate));

				// this is a workaround to bea able to release mouse buttons in the dynamic mode
				buttonsPressed = (mstate.buttons > 0);

				if(inputGrabMode == GRAB_DYNAMICALLY)
				{
					// we do not use this anymore, as we can set the mouse's position instead of grabbing it
					//INPUTENGINE.grabMouse(buttonsPressed);

					// Hide the mouse if we press any mouse buttons of if we are in mouse mode
					INPUTENGINE.hideMouse((buttonsPressed||(oldstate==1)));

					// if we are rotating, fix the mouse cursor
					if(oldstate == 0)
						INPUTENGINE.setMousePosition(mouseX, mouseY);
				}

				//if(inputGrabMode == GRAB_ALL || (inputGrabMode == GRAB_DYNAMICALLY &&  (mstate.buttons != 0 || buttonsPressed)))

				if(!mstate.buttonDown((switchMouseButtons?(OIS::MB_Left):(OIS::MB_Right))))
				{
					if(oldstate != 1)
					{
						mouseOverlay->getChild("mouse/pointer")->setMaterialName("mouse");
						oldstate = 1;
					}

					if(inputGrabMode == GRAB_DYNAMICALLY)
					{
						// using absolute positions!
						mouseX=mstate.X.abs;
						mouseY=mstate.Y.abs;
					} else
					{
						mouseX+=mstate.X.rel;
						mouseY+=mstate.Y.rel;
					}
					if (mouseX<0) mouseX=0;
					if (mouseX>screenWidth-1) mouseX=screenWidth-1;
					if (mouseY<0) mouseY=0;
					if (mouseY>screenHeight-1) mouseY=screenHeight-1;

					// set the final position of the ingame cursor
					mouseElement->setPosition(mouseX, mouseY);

					//action
					if(switchMouseButtons)
					{
						if (mstate.buttonDown(OIS::MB_Right)) click=0;
						if (mstate.buttonDown(OIS::MB_Left)) click=1;
					} else
					{
						if (mstate.buttonDown(OIS::MB_Left)) click=0;
						if (mstate.buttonDown(OIS::MB_Right)) click=1;
					}

				}else {
					if(oldstate != 0)
					{
						mouseOverlay->getChild("mouse/pointer")->setMaterialName("mouse-rotate");
						oldstate = 0;
					}
				}


				// get commands
				int i;
				for (i=1; i<=MAX_COMMANDS; i++)
				{
					trucks[current_truck]->commandkey[i].commandValue=0;
					// prepend zero's
					String zero = i<10?"0":"";
					float tmp = INPUTENGINE.getEventValue("COMMANDS_" + zero + StringConverter::toString(i));
					if(tmp > 0.0)
						trucks[current_truck]->commandkey[i].commandValue = tmp;
				}

				if (trucks[current_truck]->driveable==TRUCK)
				{
					//road construction stuff
					if (INPUTENGINE.getEventBoolValue("TERRAINEDITOR_SELECTROAD") && trucks[current_truck]->editorId>=0 && mTimeUntilNextToggle <= 0  && !trucks[current_truck]->replaymode)
					{
						if (road)
						{
							road->reset(trucks[current_truck]->nodes[trucks[current_truck]->editorId].AbsPosition);
						}
						else
							road=new Road(mSceneMgr, trucks[current_truck]->nodes[trucks[current_truck]->editorId].AbsPosition);
						mTimeUntilNextToggle = 0.5;
					}
					//editor stuff
					if (INPUTENGINE.getEventBoolValue("TERRAINEDITOR_TOGGLEOBJECT") && trucks[current_truck]->editorId>=0 && mTimeUntilNextToggle <= 0  && !trucks[current_truck]->replaymode)
					{
						if (editor)
						{
							editor->toggleType();
						}
						else
							editor=new Editor(mSceneMgr, this);
						mTimeUntilNextToggle = 0.2;
					}
					//this should not be there
					if (editor && trucks[current_truck]->editorId>=0) editor->setPos(trucks[current_truck]->nodes[trucks[current_truck]->editorId].AbsPosition);

					if (INPUTENGINE.getEventBoolValue("TRUCK_LEFT_MIRROR_LEFT") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
						trucks[current_truck]->leftMirrorAngle-=0.001;

					if (INPUTENGINE.getEventBoolValue("TRUCK_LEFT_MIRROR_RIGHT") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
						trucks[current_truck]->leftMirrorAngle+=0.001;

					if (INPUTENGINE.getEventBoolValue("TRUCK_RIGHT_MIRROR_LEFT") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
						trucks[current_truck]->rightMirrorAngle-=0.001;

					if (INPUTENGINE.getEventBoolValue("TRUCK_RIGHT_MIRROR_RIGHT") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
						trucks[current_truck]->rightMirrorAngle+=0.001;

					if (INPUTENGINE.getEventBoolValue("TERRAINEDITOR_ROTATELEFT") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
					{
						float value = 0.5;
						if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
							value = 4;
						if (road) {road->dturn(+value);mTimeUntilNextToggle = 0.1;}
						else if (editor) {editor->dturn(+1);mTimeUntilNextToggle = 0.1;}
					}
					if (INPUTENGINE.getEventBoolValue("TERRAINEDITOR_ROTATERIGHT") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
					{
						float value = 0.5;
						if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
							value = 4;
						if (road) {road->dturn(-value);mTimeUntilNextToggle = 0.1;}
						else if (editor) {editor->dturn(-1);mTimeUntilNextToggle = 0.1;}
					}
					if (INPUTENGINE.getEventBoolValue("TERRAINEDITOR_PITCHBACKWARD") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
					{
						float value = 0.5;
						if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
							value = 4;
						if (road) {road->dpitch(-value);mTimeUntilNextToggle = 0.1;}
						else if (editor) {editor->dpitch(-1);mTimeUntilNextToggle = 0.1;}
					}
					if (INPUTENGINE.getEventBoolValue("TERRAINEDITOR_PITCHFOREWARD") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
					{
						float value = 0.5;
						if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
							value = 4;
						if (road) {road-> dpitch(value);mTimeUntilNextToggle = 0.1;}
						else if (editor) {editor->dpitch(1);mTimeUntilNextToggle = 0.1;}
					}
					if (INPUTENGINE.getEventBoolValue("TERRAINEDITOR_TOGGLEROADTYPE") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
					{
						if (road)
						{
							road->toggleType();
							mTimeUntilNextToggle = 0.5;
						}
					}
					if (INPUTENGINE.getEventBoolValue("TERRAINEDITOR_BUILT") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
					{
						if (road)
						{
							if (!editorfd) 
							{
								String editorfn = SETTINGS.getSetting("Log Path") + "editor_out.txt";
								editorfd = fopen(editorfn.c_str(), "a");
								fprintf(editorfd, " ==== new session\n");
							}
							road->append();
							fprintf(editorfd, "%f, %f, %f, %f, %f, %f, %s\n", road->rpos.x, road->rpos.y, road->rpos.z, road->rrot.x, road->rrot.y, road->rrot.z, road->curtype);
							LogManager::getSingleton().logMessage(StringConverter::toString(road->rpos.x)+", "+
								StringConverter::toString(road->rpos.y)+", "+
								StringConverter::toString(road->rpos.z)+", "+
								StringConverter::toString(road->rrot.x)+", "+
								StringConverter::toString(road->rrot.y)+", "+
								StringConverter::toString(road->rrot.z)+", "+road->curtype);

							loadObject(road->curtype, road->rpos.x, road->rpos.y, road->rpos.z, road->rrot.x, road->rrot.y, road->rrot.z, 0, "generic");
							mTimeUntilNextToggle = 0.5;
						}
						if (editor)
						{
							if (!editorfd) 
							{
								String editorfn = SETTINGS.getSetting("Log Path") + "editor_out.txt";
								editorfd = fopen(editorfn.c_str(), "a");
								fprintf(editorfd, " ==== new session\n");
							}
							fprintf(editorfd, "%f, %f, %f, %f, %f, %f, %s\n", editor->ppos.x, editor->ppos.y, editor->ppos.z, 0.0, editor->pturn, editor->ppitch, editor->curtype);
							LogManager::getSingleton().logMessage(StringConverter::toString(editor->ppos.x)+", "+
								StringConverter::toString(editor->ppos.y)+", "+
								StringConverter::toString(editor->ppos.z)+", "+
								StringConverter::toString(0)+", "+
								StringConverter::toString(editor->pturn)+", "+
								StringConverter::toString(editor->ppitch)+", "+editor->curtype);
							loadObject(editor->curtype, editor->ppos.x, editor->ppos.y, editor->ppos.z, 0, editor->pturn, editor->ppitch, 0, "generic", false);
							mTimeUntilNextToggle = 0.5;
						}
					}


					// replay mode
					if (trucks[current_truck]->replaymode)
					{
						if (INPUTENGINE.getEventBoolValue("COMMON_REPLAY_FORWARD") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->replaypos<=0)
						{
							trucks[current_truck]->replaypos++;
							mTimeUntilNextToggle = 0.1;
						}
						if (INPUTENGINE.getEventBoolValue("COMMON_REPLAY_BACKWARD") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->replaypos > -trucks[current_truck]->replaylen)
						{
							trucks[current_truck]->replaypos--;
							mTimeUntilNextToggle = 0.1;
						}
						if (INPUTENGINE.getEventBoolValue("COMMON_REPLAY_FAST_FORWARD") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->replaypos+10<=0)
						{
							trucks[current_truck]->replaypos+=10;
							mTimeUntilNextToggle = 0.1;
						}
						if (INPUTENGINE.getEventBoolValue("COMMON_REPLAY_FAST_BACKWARD") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->replaypos-10 > -trucks[current_truck]->replaylen)
						{
							trucks[current_truck]->replaypos-=10;
							mTimeUntilNextToggle = 0.1;
						}
					} else
					{
						float tmp_left = INPUTENGINE.getEventValue("TRUCK_STEER_LEFT");
						float tmp_right = INPUTENGINE.getEventValue("TRUCK_STEER_RIGHT");
						float sum = -tmp_left + tmp_right;
						if(sum < -1) sum = -1;
						if(sum > 1) sum = 1;
						//LogManager::getSingleton().logMessage("steer: "+StringConverter::toString(sum));
						trucks[current_truck]->hydrodircommand = sum;
					}


					//accelerate
					if (!trucks[current_truck]->replaymode)
					{
						float accval = INPUTENGINE.getEventValue("TRUCK_ACCELERATE");
						if(trucks[current_truck]->engine)
							trucks[current_truck]->engine->autoSetAcc(accval);
					}

					//brake
					if (!trucks[current_truck]->replaymode && !trucks[current_truck]->parkingbrake)
					{
						float brake = INPUTENGINE.getEventValue("TRUCK_BRAKE");
						trucks[current_truck]->brake = brake*trucks[current_truck]->brakeforce/(1.0+fabs(trucks[current_truck]->WheelSpeed)/10.0);
						if (trucks[current_truck]->brake > trucks[current_truck]->brakeforce/6.0)
							ssm->trigStart(current_truck, SS_TRIG_BRAKE);
						else
							ssm->trigStop(current_truck, SS_TRIG_BRAKE);
					};

					if (INPUTENGINE.getEventBoolValue("TRUCK_AUTOSHIFT_UP") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
					{
						if(trucks[current_truck]->engine)
							trucks[current_truck]->engine->autoShiftUp();
						mTimeUntilNextToggle = 0.2;
					}

					if (INPUTENGINE.getEventBoolValue("TRUCK_AUTOSHIFT_DOWN") && mTimeUntilNextToggle <= 0 && !trucks[current_truck]->replaymode)
					{
						if(trucks[current_truck]->engine)
							trucks[current_truck]->engine->autoShiftDown();
						mTimeUntilNextToggle = 0.2;
					}

					if (INPUTENGINE.getEventBoolValue("TRUCK_TOGGLE_CONTACT") && !trucks[current_truck]->replaymode && mTimeUntilNextToggle <= 0)
					{
						// contact
						if(trucks[current_truck]->engine)
							trucks[current_truck]->engine->toggleContact();
						mTimeUntilNextToggle = 0.3;
					}

					if(trucks[current_truck]->engine)
					{
						if (INPUTENGINE.getEventBoolValue("TRUCK_STARTER") && trucks[current_truck]->engine->contact && !trucks[current_truck]->replaymode)
						{
							//starter
							trucks[current_truck]->engine->setstarter(1);
							ssm->trigStart(current_truck, SS_TRIG_STARTER);
						} else {
							trucks[current_truck]->engine->setstarter(0);
							ssm->trigStop(current_truck, SS_TRIG_STARTER);
						}
					}

					if (INPUTENGINE.getEventBoolValue("TRUCK_SWITCH_SHIFT_MODES") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->engine)
					{
						//Toggle Auto shift
						trucks[current_truck]->engine->toggleAutoMode();
						if (trucks[current_truck]->engine->getAutoMode()==AUTOMATIC) flashMessage(_L("Automatic shift"));
						if (trucks[current_truck]->engine->getAutoMode()==SEMIAUTO) flashMessage(_L("Manual shift - Auto clutch"));
						if (trucks[current_truck]->engine->getAutoMode()==MANUAL) flashMessage(_L("Fully Manual shift"));
						mTimeUntilNextToggle = 0.2;
					}

					//joy clutch
					float cval = INPUTENGINE.getEventValue("TRUCK_MANUAL_CLUTCH");
					if(trucks[current_truck]->engine)
						trucks[current_truck]->engine->setManualClutch(cval);

					if (INPUTENGINE.getEventBoolValue("TRUCK_SHIFT_UP") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->engine)
					{
						//Shift up
						trucks[current_truck]->engine->shift(1);
						mTimeUntilNextToggle = 0.2;
					}

					if (INPUTENGINE.getEventBoolValue("TRUCK_SHIFT_DOWN") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->engine)
					{
						//Shift down
						trucks[current_truck]->engine->shift(-1);
						mTimeUntilNextToggle = 0.2;
					}

					if (INPUTENGINE.getEventBoolValue("TRUCK_SHIFT_NEUTRAL") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->engine)
					{
						//Shift down
						trucks[current_truck]->engine->shiftTo(0);
						mTimeUntilNextToggle = 0.2;
					}

					if (trucks[current_truck]->ispolice)
					{
						if (INPUTENGINE.getEventBoolValue("TRUCK_HORN") && mTimeUntilNextToggle <= 0)
						{
							ssm->trigToggle(current_truck, SS_TRIG_HORN);
							mTimeUntilNextToggle = 0.2;
						}
					}
					else
					{
						if (INPUTENGINE.getEventBoolValue("TRUCK_HORN") && !trucks[current_truck]->replaymode)
						{
							ssm->trigStart(current_truck, SS_TRIG_HORN);
						} else {ssm->trigStop(current_truck, SS_TRIG_HORN);};
					}

					if (INPUTENGINE.getEventBoolValue("TRUCK_PARKING_BRAKE") && mTimeUntilNextToggle <= 0)
					{
						trucks[current_truck]->parkingbrakeToggle();
						mTimeUntilNextToggle = 0.2;
					}

				}
				if (trucks[current_truck]->driveable==AIRPLANE)
				{
					//autopilot
					if (trucks[current_truck]->autopilot && trucks[current_truck]->autopilot->wantsdisconnect)
					{
						trucks[current_truck]->disconnectAutopilot();
					}
					//left mouse click
					if (click==0)
					{
						OverlayElement *element=airneedlesOverlay->findElementAt((float)mouseX/(float)screenWidth,(float)mouseY/(float)screenHeight);
						if (element)
						{
							char name[256];
							strcpy(name,element->getName().c_str());
							if (!strncmp(name, "tracks/thrust1", 14)) trucks[current_truck]->aeroengines[0]->setThrotle(1.0f-((((float)mouseY/(float)screenHeight)-thrtop-throffset)/thrheight));
							if (!strncmp(name, "tracks/thrust2", 14) && trucks[current_truck]->free_aeroengine>1) trucks[current_truck]->aeroengines[1]->setThrotle(1.0f-((((float)mouseY/(float)screenHeight)-thrtop-throffset)/thrheight));
							if (!strncmp(name, "tracks/thrust3", 14) && trucks[current_truck]->free_aeroengine>2) trucks[current_truck]->aeroengines[2]->setThrotle(1.0f-((((float)mouseY/(float)screenHeight)-thrtop-throffset)/thrheight));
							if (!strncmp(name, "tracks/thrust4", 14) && trucks[current_truck]->free_aeroengine>3) trucks[current_truck]->aeroengines[3]->setThrotle(1.0f-((((float)mouseY/(float)screenHeight)-thrtop-throffset)/thrheight));
							enablegrab=false;
						}
						//also for main dashboard
						OverlayElement *element2=airdashboardOverlay->findElementAt((float)mouseX/(float)screenWidth,(float)mouseY/(float)screenHeight);
						if (element2)
						{
							enablegrab=false;
							char name[256];
							strcpy(name,element2->getName().c_str());
							//LogManager::getSingleton().logMessage("element "+element2->getName());
							if (!strncmp(name, "tracks/engstart1", 16)) trucks[current_truck]->aeroengines[0]->flipStart();
							if (!strncmp(name, "tracks/engstart2", 16) && trucks[current_truck]->free_aeroengine>1) trucks[current_truck]->aeroengines[1]->flipStart();
							if (!strncmp(name, "tracks/engstart3", 16) && trucks[current_truck]->free_aeroengine>2) trucks[current_truck]->aeroengines[2]->flipStart();
							if (!strncmp(name, "tracks/engstart4", 16) && trucks[current_truck]->free_aeroengine>3) trucks[current_truck]->aeroengines[3]->flipStart();
							//heading group
							if (!strcmp(name, "tracks/ap_hdg_but") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.2;
								if(trucks[current_truck]->autopilot->toggleHeading(HEADING_FIXED)==HEADING_FIXED)
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_hdg_but")->setMaterialName("tracks/hdg-on");
								else
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_hdg_but")->setMaterialName("tracks/hdg-off");
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_wlv_but")->setMaterialName("tracks/wlv-off");
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_nav_but")->setMaterialName("tracks/nav-off");
							}
							if (!strcmp(name, "tracks/ap_wlv_but") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.2;
								if(trucks[current_truck]->autopilot->toggleHeading(HEADING_WLV)==HEADING_WLV)
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_wlv_but")->setMaterialName("tracks/wlv-on");
								else
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_wlv_but")->setMaterialName("tracks/wlv-off");
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_hdg_but")->setMaterialName("tracks/hdg-off");
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_nav_but")->setMaterialName("tracks/nav-off");
							}
							if (!strcmp(name, "tracks/ap_nav_but") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.2;
								if(trucks[current_truck]->autopilot->toggleHeading(HEADING_NAV)==HEADING_NAV)
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_nav_but")->setMaterialName("tracks/nav-on");
								else
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_nav_but")->setMaterialName("tracks/nav-off");
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_wlv_but")->setMaterialName("tracks/wlv-off");
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_hdg_but")->setMaterialName("tracks/hdg-off");
							}
							//altitude group
							if (!strcmp(name, "tracks/ap_alt_but") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.2;
								if(trucks[current_truck]->autopilot->toggleAlt(ALT_FIXED)==ALT_FIXED)
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_alt_but")->setMaterialName("tracks/hold-on");
								else
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_alt_but")->setMaterialName("tracks/hold-off");
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_vs_but")->setMaterialName("tracks/vs-off");
							}
							if (!strcmp(name, "tracks/ap_vs_but") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.2;
								if(trucks[current_truck]->autopilot->toggleAlt(ALT_VS)==ALT_VS)
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_vs_but")->setMaterialName("tracks/vs-on");
								else
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_vs_but")->setMaterialName("tracks/vs-off");
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_alt_but")->setMaterialName("tracks/hold-off");
							}
							//IAS
							if (!strcmp(name, "tracks/ap_ias_but") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.2;
								if(trucks[current_truck]->autopilot->toggleIAS())
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_ias_but")->setMaterialName("tracks/athr-on");
								else
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_ias_but")->setMaterialName("tracks/athr-off");
							}
							//GPWS
							if (!strcmp(name, "tracks/ap_gpws_but") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.2;
								if(trucks[current_truck]->autopilot->toggleGPWS())
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_gpws_but")->setMaterialName("tracks/gpws-on");
								else
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_gpws_but")->setMaterialName("tracks/gpws-off");
							}
							//BRKS
							if (!strcmp(name, "tracks/ap_brks_but") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								trucks[current_truck]->parkingbrakeToggle();
								if(trucks[current_truck]->parkingbrake)
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_brks_but")->setMaterialName("tracks/brks-on");
								else
									OverlayManager::getSingleton().getOverlayElement("tracks/ap_brks_but")->setMaterialName("tracks/brks-off");
								mTimeUntilNextToggle = 0.2;
							}
							//trims
							if (!strcmp(name, "tracks/ap_hdg_up") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.1;
								int val=trucks[current_truck]->autopilot->adjHDG(1);
								char str[10];
								sprintf(str, "%.3u", val);
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_hdg_val")->setCaption(str);
							}
							if (!strcmp(name, "tracks/ap_hdg_dn") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.1;
								int val=trucks[current_truck]->autopilot->adjHDG(-1);
								char str[10];
								sprintf(str, "%.3u", val);
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_hdg_val")->setCaption(str);
							}
							if (!strcmp(name, "tracks/ap_alt_up") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.1;
								int val=trucks[current_truck]->autopilot->adjALT(100);
								char str[10];
								sprintf(str, "%i00", val/100);
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_alt_val")->setCaption(str);
							}
							if (!strcmp(name, "tracks/ap_alt_dn") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.1;
								int val=trucks[current_truck]->autopilot->adjALT(-100);
								char str[10];
								sprintf(str, "%i00", val/100);
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_alt_val")->setCaption(str);
							}
							if (!strcmp(name, "tracks/ap_vs_up") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.1;
								int val=trucks[current_truck]->autopilot->adjVS(100);
								char str[10];
								if (val<0)
									sprintf(str, "%i00", val/100);
								else if (val==0) strcpy(str, "000");
								else sprintf(str, "+%i00", val/100);
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_vs_val")->setCaption(str);
							}
							if (!strcmp(name, "tracks/ap_vs_dn") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.1;
								int val=trucks[current_truck]->autopilot->adjVS(-100);
								char str[10];
								if (val<0)
									sprintf(str, "%i00", val/100);
								else if (val==0) strcpy(str, "000");
								else sprintf(str, "+%i00", val/100);
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_vs_val")->setCaption(str);
							}
							if (!strcmp(name, "tracks/ap_ias_up") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.1;
								int val=trucks[current_truck]->autopilot->adjIAS(1);
								char str[10];
								sprintf(str, "%.3u", val);
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_ias_val")->setCaption(str);
							}
							if (!strcmp(name, "tracks/ap_ias_dn") && trucks[current_truck]->autopilot && mTimeUntilNextToggle <= 0)
							{
								mTimeUntilNextToggle = 0.1;
								int val=trucks[current_truck]->autopilot->adjIAS(-1);
								char str[10];
								sprintf(str, "%.3u", val);
								OverlayManager::getSingleton().getOverlayElement("tracks/ap_ias_val")->setCaption(str);
							}
						}

					}
					//AIRPLANE KEYS
					float commandrate=4.0;
					//float dt=evt.timeSinceLastFrame;
					//turning
					if (trucks[current_truck]->replaymode)
					{
						if (INPUTENGINE.getEventBoolValue("COMMON_REPLAY_FORWARD") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->replaypos<=0)
						{
							trucks[current_truck]->replaypos++;
							mTimeUntilNextToggle = 0.1;
						}
						if (INPUTENGINE.getEventBoolValue("COMMON_REPLAY_BACKWARD") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->replaypos > -trucks[current_truck]->replaylen)
						{
							trucks[current_truck]->replaypos--;
							mTimeUntilNextToggle = 0.1;
						}
						if (INPUTENGINE.getEventBoolValue("COMMON_REPLAY_FAST_FORWARD") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->replaypos+10<=0)
						{
							trucks[current_truck]->replaypos+=10;
							mTimeUntilNextToggle = 0.1;
						}
						if (INPUTENGINE.getEventBoolValue("COMMON_REPLAY_FAST_BACKWARD") && mTimeUntilNextToggle <= 0 && trucks[current_truck]->replaypos-10 > -trucks[current_truck]->replaylen)
						{
							trucks[current_truck]->replaypos-=10;
							mTimeUntilNextToggle = 0.1;
						}
					} else
					{
						float tmp_left = INPUTENGINE.getEventValue("AIRPLANE_STEER_LEFT");
						float tmp_right = INPUTENGINE.getEventValue("AIRPLANE_STEER_RIGHT");
						float sum_steer = -tmp_left + tmp_right;
						INPUTENGINE.smoothValue(trucks[current_truck]->aileron, sum_steer, dt*commandrate);
						trucks[current_truck]->hydrodircommand = trucks[current_truck]->aileron;
					}

					//pitch
					float tmp_pitch_up = INPUTENGINE.getEventValue("AIRPLANE_ELEVATOR_UP");
					float tmp_pitch_down = INPUTENGINE.getEventValue("AIRPLANE_ELEVATOR_DOWN");
					float sum_pitch = tmp_pitch_down - tmp_pitch_up;
					INPUTENGINE.smoothValue(trucks[current_truck]->elevator, sum_pitch, dt*commandrate);

					//rudder
					float tmp_rudder_left = INPUTENGINE.getEventValue("AIRPLANE_RUDDER_LEFT");
					float tmp_rudder_right = INPUTENGINE.getEventValue("AIRPLANE_RUDDER_RIGHT");
					float sum_rudder = tmp_rudder_left - tmp_rudder_right;
					INPUTENGINE.smoothValue(trucks[current_truck]->rudder, sum_rudder, dt*commandrate);

					//brake
					if (!trucks[current_truck]->replaymode && !trucks[current_truck]->parkingbrake)
					{
						trucks[current_truck]->brake=0.0;
						if (INPUTENGINE.getEventBoolValue("AIRPLANE_BRAKE"))
						{
							trucks[current_truck]->brake=trucks[current_truck]->brakeforce*0.66;
						}
					};
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_PARKING_BRAKE") && mTimeUntilNextToggle <= 0)
					{
						trucks[current_truck]->parkingbrakeToggle();
						if(trucks[current_truck]->parkingbrake)
							OverlayManager::getSingleton().getOverlayElement("tracks/ap_brks_but")->setMaterialName("tracks/brks-on");
						else
							OverlayManager::getSingleton().getOverlayElement("tracks/ap_brks_but")->setMaterialName("tracks/brks-off");
						mTimeUntilNextToggle = 0.2;
					}
					//reverse
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_REVERSE") && mTimeUntilNextToggle <= 0)
					{
						int i;
						for (i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->toggleReverse();
						mTimeUntilNextToggle = 0.2;
					}

					// toggle engines
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_TOGGLE_ENGINES") && mTimeUntilNextToggle <= 0)
					{
						int i;
						for (i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->flipStart();
					}

					//flaps
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_FLAPS_NONE") && mTimeUntilNextToggle <= 0)
					{
						if (trucks[current_truck]->flap>0)
							trucks[current_truck]->flap=0;
						mTimeUntilNextToggle = 0.2;
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_FLAPS_FULL") && mTimeUntilNextToggle <= 0)
					{
						if (trucks[current_truck]->flap<5)
							trucks[current_truck]->flap=5;
						mTimeUntilNextToggle = 0.2;
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_FLAPS_LESS") && mTimeUntilNextToggle <= 0)
					{
						if (trucks[current_truck]->flap>0)
							trucks[current_truck]->flap=(trucks[current_truck]->flap)-1;
						mTimeUntilNextToggle = 0.2;
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_FLAPS_MORE") && mTimeUntilNextToggle <= 0)
					{
						if (trucks[current_truck]->flap<5)
							trucks[current_truck]->flap=(trucks[current_truck]->flap)+1;
						mTimeUntilNextToggle = 0.2;
					}

					//airbrakes
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_AIRBRAKES_NONE") && mTimeUntilNextToggle <= 0)
					{
						if (trucks[current_truck]->airbrakeval>0)
							trucks[current_truck]->airbrakeval=0;
						mTimeUntilNextToggle = 0.2;
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_AIRBRAKES_FULL") && mTimeUntilNextToggle <= 0)
					{
						if (trucks[current_truck]->airbrakeval<5)
							trucks[current_truck]->airbrakeval=5;
						mTimeUntilNextToggle = 0.2;
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_AIRBRAKES_LESS") && mTimeUntilNextToggle <= 0)
					{
						if (trucks[current_truck]->airbrakeval>0)
							trucks[current_truck]->airbrakeval=(trucks[current_truck]->airbrakeval)-1;
						mTimeUntilNextToggle = 0.2;
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_AIRBRAKES_MORE") && mTimeUntilNextToggle <= 0)
					{
						if (trucks[current_truck]->airbrakeval<5)
							trucks[current_truck]->airbrakeval=(trucks[current_truck]->airbrakeval)+1;
						mTimeUntilNextToggle = 0.2;
					}

					//throttle
					float tmp_throttle = INPUTENGINE.getEventBoolValue("AIRPLANE_THROTTLE");
					if(tmp_throttle > 0)
						for (int i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->setThrotle(tmp_throttle);

					if(INPUTENGINE.isEventDefined("AIRPLANE_THROTTLE_AXIS"))
					{
						float f = INPUTENGINE.getEventValue("AIRPLANE_THROTTLE_AXIS");
						for (int i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->setThrotle(f);
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_THROTTLE_DOWN") && mTimeUntilNextToggle <= 0)
					{
						//throtle down
						int i;
						for (i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->setThrotle(trucks[current_truck]->aeroengines[i]->getThrotle()-0.05);
						mTimeUntilNextToggle = 0.1;
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_THROTTLE_UP") && mTimeUntilNextToggle <= 0)
					{
						//throtle up
						int i;
						for (i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->setThrotle(trucks[current_truck]->aeroengines[i]->getThrotle()+0.05);
						mTimeUntilNextToggle = 0.1;
					}

					if (INPUTENGINE.getEventBoolValue("AIRPLANE_THROTTLE_NO") && mTimeUntilNextToggle <= 0)
					{
						// no throtle
						int i;
						for (i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->setThrotle(0);
						mTimeUntilNextToggle = 0.1;
					}
					if (INPUTENGINE.getEventBoolValue("AIRPLANE_THROTTLE_FULL") && mTimeUntilNextToggle <= 0)
					{
						// full throtle
						int i;
						for (i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->setThrotle(1);
						mTimeUntilNextToggle = 0.1;
					}
					if (trucks[current_truck]->autopilot)
					{
						for (i=0; i<trucks[current_truck]->free_aeroengine; i++)
							trucks[current_truck]->aeroengines[i]->setThrotle(trucks[current_truck]->autopilot->getThrotle(trucks[current_truck]->aeroengines[i]->getThrotle(), dt));
					}


				}
				if (trucks[current_truck]->driveable==BOAT)
				{
					//BOAT SPECIFICS

					//throttle

					if(INPUTENGINE.isEventDefined("BOAT_THROTTLE_AXIS"))
					{
						float f = INPUTENGINE.getEventValue("BOAT_THROTTLE_AXIS");
						// use negative values also!
						f = f * 2 - 1;
						for (int i=0; i<trucks[current_truck]->free_screwprop; i++)
							trucks[current_truck]->screwprops[i]->setThrotle(-f);
					}
					if (INPUTENGINE.getEventBoolValue("BOAT_THROTTLE_DOWN") && mTimeUntilNextToggle <= 0)
					{
						//throtle down
						int i;
						for (i=0; i<trucks[current_truck]->free_screwprop; i++)
							trucks[current_truck]->screwprops[i]->setThrotle(trucks[current_truck]->screwprops[i]->getThrotle()-0.05);
						mTimeUntilNextToggle = 0.1;
					}
					if (INPUTENGINE.getEventBoolValue("BOAT_THROTTLE_UP") && mTimeUntilNextToggle <= 0)
					{
						//throtle up
						int i;
						for (i=0; i<trucks[current_truck]->free_screwprop; i++)
							trucks[current_truck]->screwprops[i]->setThrotle(trucks[current_truck]->screwprops[i]->getThrotle()+0.05);
						mTimeUntilNextToggle = 0.1;
					}


					// steer
					float tmp_steer_left = INPUTENGINE.getEventValue("BOAT_STEER_LEFT");
					float tmp_steer_right = INPUTENGINE.getEventValue("BOAT_STEER_RIGHT");
					float sum_steer = (tmp_steer_left - tmp_steer_right) * 0.06;
					// do not center the rudder!
					if(fabs(sum_steer)>0 && mTimeUntilNextToggle <= 0)
					{
						for (int i=0; i<trucks[current_truck]->free_screwprop; i++)
							trucks[current_truck]->screwprops[i]->setRudder(trucks[current_truck]->screwprops[i]->getRudder() + sum_steer);
						mTimeUntilNextToggle = 0.1;
					}
					if(INPUTENGINE.isEventDefined("BOAT_STEER_LEFT_AXIS") && INPUTENGINE.isEventDefined("BOAT_STEER_RIGHT_AXIS"))
					{
						float tmp_steer_left = INPUTENGINE.getEventValue("BOAT_STEER_LEFT_AXIS");
						float tmp_steer_right = INPUTENGINE.getEventValue("BOAT_STEER_RIGHT_AXIS");
						float sum_steer = (tmp_steer_left - tmp_steer_right);
						for (int i=0; i<trucks[current_truck]->free_screwprop; i++)
							trucks[current_truck]->screwprops[i]->setRudder(sum_steer);
					}
					if (INPUTENGINE.getEventBoolValue("BOAT_CENTER_RUDDER") && mTimeUntilNextToggle <= 0)
					{
						int i;
						for (i=0; i<trucks[current_truck]->free_screwprop; i++)
							trucks[current_truck]->screwprops[i]->setRudder(0);
						mTimeUntilNextToggle = 0.1;
					}

					if (INPUTENGINE.getEventBoolValue("BOAT_REVERSE") && mTimeUntilNextToggle <= 0)
					{
						int i;
						for (i=0; i<trucks[current_truck]->free_screwprop; i++)
							trucks[current_truck]->screwprops[i]->toggleReverse();
						mTimeUntilNextToggle = 0.2;
					}
				}
				//COMMON KEYS


				if (INPUTENGINE.getEventBoolValue("COMMON_LOCK") && mTimeUntilNextToggle <= 0)
				{
					trucks[current_truck]->lockToggle(trucks, free_truck);
					mTimeUntilNextToggle = 0.2;
				}
				//strap
				if (INPUTENGINE.getEventBoolValue("COMMON_SECURE_LOAD") && mTimeUntilNextToggle <= 0)
				{
					trucks[current_truck]->tieToggle(trucks, free_truck);
					mTimeUntilNextToggle = 0.2;
				}
				if (INPUTENGINE.getEventBoolValue("COMMON_RESET_TRUCK") && !trucks[current_truck]->replaymode)
				{
					// init
					trucks[current_truck]->reset();
				}
				//replay mode
				if (INPUTENGINE.getEventBoolValue("COMMON_TOGGLE_REPLAY_MODE")  && mTimeUntilNextToggle <= 0)
				{
					trucks[current_truck]->setReplayMode(!trucks[current_truck]->replaymode);
					mTimeUntilNextToggle = 0.2;
				}

				if (INPUTENGINE.getEventBoolValue("COMMON_TOGGLE_CUSTOM_PARTICLES")  && mTimeUntilNextToggle <= 0)
				{
					trucks[current_truck]->toggleCustomParticles();
					mTimeUntilNextToggle = 0.2;
				}


				if (INPUTENGINE.getEventBoolValue("COMMON_SHOW_SKELETON") && mTimeUntilNextToggle <=0)
				{
					if (trucks[current_truck]->skeleton)
					{
						trucks[current_truck]->hideSkeleton(true);
					}
					else
						trucks[current_truck]->showSkeleton(true, true);
					trucks[current_truck]->updateVisual();
					mTimeUntilNextToggle = 0.2;
				}

				if (INPUTENGINE.getEventBoolValue("COMMON_TOGGLE_TRUCK_LIGHTS") && mTimeUntilNextToggle <= 0)
				{
					trucks[current_truck]->lightsToggle(trucks, free_truck);
					mTimeUntilNextToggle = 0.2;
				}

				if (INPUTENGINE.getEventBoolValue("COMMON_TOGGLE_TRUCK_BEACONS") && mTimeUntilNextToggle <= 0)
				{
					trucks[current_truck]->beaconsToggle();
					mTimeUntilNextToggle = 0.2;
				}
				//camera mode
				if (INPUTENGINE.getEventBoolValue("CAMERA_CHANGE") && mTimeUntilNextToggle <= 0)
				{
					if (cameramode==CAMERA_INT && trucks[current_truck]->currentcamera<trucks[current_truck]->freecinecamera-1)
					{
						trucks[current_truck]->currentcamera++;
					}
					else
					{
						trucks[current_truck]->currentcamera=0;
						if (cameramode==CAMERA_INT)
						{
							//end of internal cam
							camRotX=pushcamRotX;
							camRotY=pushcamRotY;
							trucks[current_truck]->prepareInside(false);
							showDashboardOverlays(true, trucks[current_truck]->driveable);
							if(bigMap) bigMap->setVisibility(true);
						}
						cameramode++;
						if (cameramode==CAMERA_INT)
						{
							//start of internal cam
							pushcamRotX=camRotX;
							pushcamRotY=camRotY;
							camRotX=0;
							camRotY=DEFAULT_INTERNAL_CAM_PITCH;
							trucks[current_truck]->prepareInside(true);
							if(bigMap) bigMap->setVisibility(false);
							// airplane dashboard in the plane visible
							if(trucks[current_truck]->driveable == AIRPLANE)
								showDashboardOverlays(true, trucks[current_truck]->driveable);
							else
								showDashboardOverlays(false, 0);
						}
						if (cameramode==CAMERA_END) cameramode=0;
					}
					mTimeUntilNextToggle = 0.2;
				}
				//camera mode
				if (INPUTENGINE.getEventBoolValue("COMMON_PRESSURE_LESS") && current_truck!=-1)
				{
					showPressureOverlay(true);
					ssm->trigStart(current_truck, SS_TRIG_AIR);
					trucks[current_truck]->addPressure(-dt*10.0);
					pressure_pressed=true;
				}
				else if (INPUTENGINE.getEventBoolValue("COMMON_PRESSURE_MORE"))
				{
					showPressureOverlay(true);
					ssm->trigStart(current_truck, SS_TRIG_AIR);
					trucks[current_truck]->addPressure(dt*10.0);
					pressure_pressed=true;
				} else if (pressure_pressed)
				{
					ssm->trigStop(current_truck, SS_TRIG_AIR);
					pressure_pressed=false;
					showPressureOverlay(false);
				}

				if(enablegrab)
				{
					//node grabbing
					bool ctrldown = INPUTENGINE.isKeyDown(OIS::KC_LCONTROL) || INPUTENGINE.isKeyDown(OIS::KC_RCONTROL);
					//node grabbing
					if (isnodegrabbed)
					{
						if(oldstate != 2)
						{
							if(ctrldown)
								mouseOverlay->getChild("mouse/pointer")->setMaterialName("mouse-locked-heavy");
							else
								mouseOverlay->getChild("mouse/pointer")->setMaterialName("mouse-locked");
							pickLineNode->setVisible(true);
							oldstate = 2;
						}

						if (click==0)
						{
							//exert forces
							//find pointed position
							Ray mouseRay=mCamera->getCameraToViewportRay((float)mouseX/(float)screenWidth, (float)mouseY/(float)screenHeight);
							Vector3 pos=mouseRay.getPoint(distgrabbed);

							// update pickline
							pickLine->beginUpdate(0);
							pickLine->position(trucks[truckgrabbed]->nodes[nodegrabbed].AbsPosition);
							pickLine->position(pos);
							pickLine->end();

							// add forces
							if (ctrldown)
								trucks[truckgrabbed]->mouseMove(nodegrabbed,pos, 0);
							else
								trucks[truckgrabbed]->mouseMove(nodegrabbed,pos, 1);
						}
						else
						{
							pickLineNode->setVisible(false);
							isnodegrabbed=false;
							trucks[truckgrabbed]->mouseMove(-1,Vector3::ZERO, 0);
						}
					}
					else
					{
						if (click==0)
						{
							//try to grab something
							Ray mouseRay=mCamera->getCameraToViewportRay((float)mouseX/(float)screenWidth, (float)mouseY/(float)screenHeight);
							int t;
							int mindist=30000;
							int mintruck=-1;
							int minnode=-1;
							for (t=0; t<free_truck; t++)
							{
								int i;
								for (i=0; i<trucks[t]->free_node; i++)
								{
									std::pair<bool,Real> pair=mouseRay.intersects(Sphere(trucks[t]->nodes[i].AbsPosition, 0.1));
									if (pair.first)
									{
										if (pair.second<mindist)
										{
											mindist=(int)pair.second;
											mintruck=t;
											minnode=i;
										}
									}
								}
							}
							//okay see if we got a match
							if (mintruck!=-1)
							{
								truckgrabbed=mintruck;
								nodegrabbed=minnode;
								distgrabbed=mindist;
								isnodegrabbed=true;
							}
						}
					}
				}
			}//end of truck!=-1
		}


		static unsigned char brushNum=0;

		const OIS::MouseState mstate = INPUTENGINE.getMouseState();
		static bool buttonsPressed = false;
		if(inputGrabMode == GRAB_ALL || (inputGrabMode == GRAB_DYNAMICALLY &&  (mstate.buttons != 0 || buttonsPressed)))
		{
			if ((cameramode==CAMERA_INT || cameramode==CAMERA_EXT || cameramode==CAMERA_FIX || cameramode==CAMERA_FREE_FIXED) && current_truck == -1)
			{
				bool btnview = mstate.buttonDown((switchMouseButtons?OIS::MB_Left:OIS::MB_Right));
				if(inputGrabMode == GRAB_DYNAMICALLY)
				{
					if(!buttonsPressed && btnview)
					{
						// set mouse position on the first click
						mouseX = mstate.X.abs;
						mouseY = mstate.Y.abs;
						// display cursor
						mouseElement->setPosition(mouseX, mouseY);
						mouseOverlay->getChild("mouse/pointer")->setMaterialName("mouse-rotate");
						mouseOverlay->show();
						INPUTENGINE.hideMouse(true);
					} else if (buttonsPressed && !btnview)
					{
						// release event
						mouseOverlay->getChild("mouse/pointer")->setMaterialName("mouse");
						mouseOverlay->hide();
						INPUTENGINE.hideMouse(false);
					}
					if(btnview)
						INPUTENGINE.setMousePosition(mouseX, mouseY);

				}
				if(inputGrabMode == GRAB_ALL || (inputGrabMode == GRAB_DYNAMICALLY &&  btnview))
				{
					// 'First person' mode :)
					// x rotation = automatically by rotating the character
					if (mstate.Y.rel != 0)
						camRotY += Degree(-mstate.Y.rel / 10.0);
					if (mstate.Z.rel != 0)
						if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
							camDist += -mstate.Z.rel / 12.0;
						else
							camDist += -mstate.Z.rel / 120.0;

					// rather hacked workaround :|
					float angle = person->getAngle() + (float)(mstate.X.rel) / 120.0;
					person->getSceneNode()->resetOrientation();
					person->getSceneNode()->yaw(-Radian(angle));
					person->setAngle(angle);
					person->updateMapIcon();
				}
			} else
			{
				if(mstate.buttonDown((switchMouseButtons?OIS::MB_Left:OIS::MB_Right)))
				{
					// fix mouse at the current place :)
					if(inputGrabMode == GRAB_DYNAMICALLY)
						INPUTENGINE.setMousePosition(mouseX, mouseY);
					if (mstate.X.rel != 0)
						camRotX += Degree(mstate.X.rel / 10.0);
					if (mstate.Y.rel != 0)
						camRotY += Degree(-mstate.Y.rel / 10.0);
					if (mstate.Z.rel != 0)
						if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
							camDist += -mstate.Z.rel / 12.0;
						else
							camDist += -mstate.Z.rel / 120.0;
				}
			}
			buttonsPressed = (mstate.buttons > 0);
		}

		if (INPUTENGINE.getEventBoolValue("CAMERA_LOOKBACK") && mTimeUntilNextToggle <=0)
		{
			if(camRotX > Degree(0))
				camRotX=Degree(0);
			else
				camRotX=Degree(180);
			mTimeUntilNextToggle = 0.2;
		}
		if (INPUTENGINE.getEventBoolValue("CAMERA_ROTATE_LEFT"))
		{
			// Move camera left
			camRotX-=mRotScale;
		}

		if (INPUTENGINE.getEventBoolValue("CAMERA_ROTATE_RIGHT"))
		{
			// Move camera RIGHT
			camRotX+=mRotScale;
		}

		if ((INPUTENGINE.getEventBoolValue("CAMERA_ROTATE_UP")) && camRotY<Degree(88))
		{
			// Move camera up
			camRotY+=mRotScale;
		}

		if ((INPUTENGINE.getEventBoolValue("CAMERA_ROTATE_DOWN")) && camRotY>Degree(-80))
		{
			// Move camera down
			camRotY-=mRotScale;
		}

		if (INPUTENGINE.getEventBoolValue("CAMERA_ZOOM_IN") && camDist>1)
		{
			// Move camera near
			camDist-=mMoveScale;
		}
		if (INPUTENGINE.getEventBoolValue("CAMERA_ZOOM_IN_FAST") && camDist>1)
		{
			// Move camera near
			camDist-=mMoveScale * 10;
		}
		if (INPUTENGINE.getEventBoolValue("CAMERA_ZOOM_OUT"))
		{
			// Move camera far
			camDist+=mMoveScale;
		}
		if (INPUTENGINE.getEventBoolValue("CAMERA_ZOOM_OUT_FAST"))
		{
			// Move camera far
			camDist+=mMoveScale * 10;
		}
		if (INPUTENGINE.getEventBoolValue("CAMERA_RESET"))
		{
			camRotX=0;
			if (cameramode!=CAMERA_INT) camRotY=Degree(12);
			else camRotY=DEFAULT_INTERNAL_CAM_PITCH;
			camDist=20;
		}
		if (INPUTENGINE.getEventBoolValue("CAELUM_INCREASE_TIME") && mCaelumSystem)
		{
			int val=(int)(mCaelumSystem->getLocalTime()+60);
			//				int val=(mCaelumSystem->getLocalTime()+3600);
			if (val>86400) val-=86400;
			mCaelumSystem->setLocalTime(val);
			if (envmap) envmap->forceUpdate(Vector3(terrainxsize/2.0, hfinder->getHeightAt(terrainxsize/2.0, terrainzsize/2.0)+50.0, terrainzsize/2.0));
		}
		if (INPUTENGINE.getEventBoolValue("CAELUM_INCREASE_TIME_FAST") && mCaelumSystem)
		{
			int val=(int)(mCaelumSystem->getLocalTime()+400);
			//				int val=(mCaelumSystem->getLocalTime()+3600);
			if (val>86400) val-=86400;
			mCaelumSystem->setLocalTime(val);
			if (envmap) envmap->forceUpdate(Vector3(terrainxsize/2.0, hfinder->getHeightAt(terrainxsize/2.0, terrainzsize/2.0)+50.0, terrainzsize/2.0));
		}
		if (INPUTENGINE.getEventBoolValue("CAELUM_DECREASE_TIME") && mCaelumSystem)
		{
			int val=(int)(mCaelumSystem->getLocalTime()-60);
			if (val<0) val+=86400;
			mCaelumSystem->setLocalTime(val);
			if (envmap) envmap->forceUpdate(Vector3(terrainxsize/2.0, hfinder->getHeightAt(terrainxsize/2.0, terrainzsize/2.0)+50.0, terrainzsize/2.0));
		}
		if (INPUTENGINE.getEventBoolValue("CAELUM_DECREASE_TIME_FAST") && mCaelumSystem)
		{
			int val=(int)(mCaelumSystem->getLocalTime()-400);
			if (val<0) val+=86400;
			mCaelumSystem->setLocalTime(val);
			if (envmap) envmap->forceUpdate(Vector3(terrainxsize/2.0, hfinder->getHeightAt(terrainxsize/2.0, terrainzsize/2.0)+50.0, terrainzsize/2.0));
		}

		if (INPUTENGINE.getEventBoolValue("COMMON_TOGGLE_RENDER_MODE") && mTimeUntilNextToggle <=0)
		{
			mSceneDetailIndex = (mSceneDetailIndex+1)%3 ;
			switch(mSceneDetailIndex) {
				case 0 : mCamera->setPolygonMode(PM_SOLID) ; break ;
				case 1 : mCamera->setPolygonMode(PM_WIREFRAME) ; break ;
				case 2 : mCamera->setPolygonMode(PM_POINTS) ; break ;
			}
			if(mtc && interactivemap)
			{
				switch(mSceneDetailIndex) {
					case 0 : mtc->setCameraMode(PM_SOLID) ; break ;
					case 1 : mtc->setCameraMode(PM_WIREFRAME) ; break ;
					case 2 : mtc->setCameraMode(PM_POINTS) ; break ;
				}
				mtc->update();

			}
			mTimeUntilNextToggle = 0.5;
		}

		if (INPUTENGINE.getEventBoolValue("COMMON_VIEW_MAP") && mTimeUntilNextToggle <= 0)
		{
			if(bigMap)
			{
				mapMode++;
				if(mapMode>1)
					mapMode=0;

				if(mapMode==0)
				{
					if(cameramode!=CAMERA_INT)
					{
						bigMap->setVisibility(true);
						if(mtc) mtc->update();
						//make it small again
						bigMap->updateRenderMetrics(mWindow);
						bigMap->setPosition(0, 0.81, 0.14, 0.19, mWindow);
					} else
						bigMap->setVisibility(false);

					if(net) playerListOverlay->hide();
				} else if(mapMode==1)
				{
					bigMap->setVisibility(true);
					if(mtc) mtc->update();
					// make it big
					bigMap->updateRenderMetrics(mWindow);
					bigMap->setPosition(0.2, 0, 0.8, 0.8, mWindow);
					if(net)
						playerListOverlay->show();
				} else
				{
					bigMap->setVisibility(false);
					if(net)
						playerListOverlay->hide();
				}
			}

			mTimeUntilNextToggle = 0.2;
		}
		if (INPUTENGINE.getEventBoolValue("COMMON_MAP_ALPHA") && mTimeUntilNextToggle <= 0)
		{
			if(bigMap)
			{
				if(fabs(1-bigMap->getAlpha()) < 0.001)
				{
					bigMap->setAlpha(0.5);
					if(mtc) mtc->setTranlucency(0.5);
				}
				else if(fabs(0.5-bigMap->getAlpha()) < 0.001)
				{
					bigMap->setAlpha(0.2);
					if(mtc) mtc->setTranlucency(0.2);
				}
				else if(fabs(0.2-bigMap->getAlpha()) < 0.001)
				{
					bigMap->setAlpha(1);
					if(mtc) mtc->setTranlucency(1);
				}
			}
			mTimeUntilNextToggle = 0.2;
		}
		if (INPUTENGINE.getEventBoolValue("COMMON_RESCUE_TRUCK") && mTimeUntilNextToggle <=0 && !netmode && !useIngameEditor)
		{
			//rescue!
			//if (current_truck!=-1) setCurrentTruck(-1);
			int rtruck=-1;
			// search a rescue truck
			for (int i=0; i<free_truck; i++) 
				if (trucks[i]->rescuer) 
					rtruck=i;
			if(rtruck == -1)
			{			
				flashMessage("No rescue truck found!", 3);
			} else
			{
				setCurrentTruck(rtruck);
			}
			mTimeUntilNextToggle = 0.5;
		}

		if (INPUTENGINE.getEventBoolValue("TRUCK_BLINK_LEFT") && mTimeUntilNextToggle <=0 && current_truck>=0)
		{
			if (trucks[current_truck]->getBlinkType() == BLINK_LEFT)
				trucks[current_truck]->setBlinkType(BLINK_NONE);
			else
				trucks[current_truck]->setBlinkType(BLINK_LEFT);
			mTimeUntilNextToggle = 0.2;
		}

		if (INPUTENGINE.getEventBoolValue("TRUCK_BLINK_RIGHT") && mTimeUntilNextToggle <=0 && current_truck>=0)
		{
			if (trucks[current_truck]->getBlinkType() == BLINK_RIGHT)
				trucks[current_truck]->setBlinkType(BLINK_NONE);
			else
				trucks[current_truck]->setBlinkType(BLINK_RIGHT);
			mTimeUntilNextToggle = 0.2;
		}

		if (INPUTENGINE.getEventBoolValue("TRUCK_BLINK_WARN") && mTimeUntilNextToggle <=0 && current_truck>=0)
		{
			if (trucks[current_truck]->getBlinkType() == BLINK_WARN)
				trucks[current_truck]->setBlinkType(BLINK_NONE);
			else
				trucks[current_truck]->setBlinkType(BLINK_WARN);
			mTimeUntilNextToggle = 0.2;
		}

		if (INPUTENGINE.getEventBoolValue("COMMON_CONSOLEDISPLAY") && mTimeUntilNextToggle <=0)
		{
			CONSOLE.toggleVisible(this);
			mTimeUntilNextToggle = 0.2;
		}

		if (INPUTENGINE.getEventBoolValue("COMMON_CONSOLEMODE") && mTimeUntilNextToggle <=0)
		{
			CONSOLE.toggleMode(this);
			mTimeUntilNextToggle = 0.2;
		}

		if (INPUTENGINE.getEventBoolValue("COMMON_ENTER_OR_EXIT_TRUCK") && mTimeUntilNextToggle <= 0 && !netmode && !useIngameEditor)
		{
			//perso in/out
			mTimeUntilNextToggle = 0.5;
			if (current_truck==-1)
			{
				//find the nearest truck
				int i;
				float mindist=1000.0;
				int minindex=-1;
				for (i=0; i<free_truck; i++)
				{
					if (!trucks[i]->driveable)

						continue;
					if (trucks[i]->cinecameranodepos[0]==-1)
					{
						LogManager::getSingleton().logMessage("cinecam missing, cannot enter truck!");
						continue;
					}
					float len=(trucks[i]->nodes[trucks[i]->cinecameranodepos[0]].AbsPosition-(person->getPosition()+Vector3(0.0, 2.0, 0.0))).length();
					if (len<mindist)
					{
						mindist=len;
						minindex=i;
					}
				}
				if (mindist<20.0) setCurrentTruck(minindex);
			}
			else if (trucks[current_truck]->nodes[trucks[current_truck]->cinecameranodepos[0]].Velocity.length()<1)
			{
				setCurrentTruck(-1);
			} else {
				trucks[current_truck]->brake=trucks[current_truck]->brakeforce*0.66;
				mTimeUntilNextToggle = 0.0;
			}
		}

	} else
	{
		//no terrain or truck loaded

		//uiloader->updateEvents(dt);


		if (UILOADER.isFinishedSelecting())
		{
			if (loading_state==NONE_LOADED)
			{
				Cache_Entry *sel = UILOADER.getSelection();
				if(sel)
				{
					terrainUID = sel->uniqueid;
					loadTerrain(sel->fname);

					//load truck list
					//OverlayManager::getSingleton().getOverlayElement("tracks/Title")->setCaption("Choose your vehicle:");
					//					if (collisions->hasbuildbox && !netmode) FIXME

					// no trucks loaded?
					if (truck_preload_num == 0 || netmode)
					{
						// show truck selector
						if(w)
						{
							hideMap();
							UILOADER.show(GUI_Loader::LT_NetworkWithBoat);
						}
						else
						{
							hideMap();
							UILOADER.show(GUI_Loader::LT_Network);
						}
					} else {
						// init no trucks, as there were found some
						initTrucks(false, sel->fname);
					}
				}
			} else if (loading_state==TERRAIN_LOADED) {
				Cache_Entry *selt = UILOADER.getSelection();
				std::vector<Ogre::String> config = UILOADER.getTruckConfig();
				std::vector<Ogre::String> *configptr = &config;
				if(config.size() == 0) configptr = 0;
				if(selt)
					initTrucks(true, selt->fname, selt->fext, configptr);
				// show console in netmode!
				if(netmode)
					CONSOLE.setMode(this, CONSOLE_LEFT_SMALL, true);

			} else if (loading_state==RELOADING) {
				Cache_Entry *selt = UILOADER.getSelection();
				if(selt)
				{
					//we load an extra truck
					char *selected = const_cast<char *>(selt->fname.c_str());
					std::vector<Ogre::String> config = UILOADER.getTruckConfig();
					std::vector<Ogre::String> *configptr = &config;
					if(config.size() == 0) configptr = 0;
					trucks[free_truck] = new Beam(free_truck, mSceneMgr, mSceneMgr->getRootSceneNode(), mWindow, &mapsizex, &mapsizez, reload_pos.x, reload_pos.y, reload_pos.z, reload_dir, selected, collisions, dustp, clumpp, sparksp, dripp, splashp, ripplep, hfinder, w, mCamera, mirror, true, false, false, reload_box, false, flaresMode, configptr);
				}

				if(bigMap)
				{
					MapEntity *e = bigMap->createNamedMapEntity("Truck"+StringConverter::toString(free_truck), MapControl::getTypeByDriveable(trucks[free_truck]->driveable));
					e->setState(DESACTIVATED);
					e->setVisibility(true);
					e->setPosition(reload_pos);
					e->setRotation(-Radian(trucks[free_truck]->getHeadingDirectionAngle()));
					// create a map icon
					//createNamedMapEntity();
				}

				UILOADER.hide();
				loading_state=ALL_LOADED;
				if(trucks[free_truck]->driveable)
				{
					//we are supposed to be in this truck, if it is a truck
					if (trucks[free_truck]->engine)
						trucks[free_truck]->engine->start();
					setCurrentTruck(free_truck);
				} else {
					// if it is a load or trailer, than stay in person mode
					// but relocate to the new position, so we dont spawn the dialog again
					//personode->setPosition(reload_pos);
					person->move(Vector3(3.0, 0.2, 0.0)); //bad, but better
					//setCurrentTruck(-1);
				}
				free_truck++;
			}

		}
	}

#ifdef HAS_EDITOR
	/*
	// not supported anymore
	if (loading_state==EDITING)
	{
		if (!trucked->processInput(mMouse, mKeyboard))
		{
			//exit clicked
			dirty=true;
			showTruckEditorOverlay(false);
			loading_state=ALL_LOADED;
		}
	}
	*/
#endif

	if(INPUTENGINE.getEventBoolValue("COMMON_QUIT_GAME") && mTimeUntilNextToggle <= 0)
	{
		if(!showcredits)
			shutdown_pre();
		else
			shutdown_final();
		mTimeUntilNextToggle = 0.5;
	}

	if(INPUTENGINE.getEventBoolValue("COMMON_TRUCK_INFO") && !showcredits && mTimeUntilNextToggle <= 0 && current_truck != -1)
	{
		mTruckInfoOn = ! mTruckInfoOn;
		dirty=true;
		TRUCKHUD.show(mTruckInfoOn);
		mTimeUntilNextToggle = 0.2;
	}

	if(INPUTENGINE.getEventBoolValue("COMMON_HIDE_GUI") && !showcredits && mTimeUntilNextToggle <= 0)
	{
		hidegui = !hidegui;
		if(hidegui)
		{
			mouseOverlay->hide();
			if (netmode && CONSOLE.getVisible())
				CONSOLE.toggleVisible(this);

			showDashboardOverlays(false,0);
			showEditorOverlay(false);
			TRUCKHUD.show(false);
			if(bigMap) bigMap->setVisibility(false);
		}
		else
		{
			if (netmode && !CONSOLE.getVisible())
				CONSOLE.toggleVisible(this);
			if(current_truck != -1 && cameramode!=CAMERA_INT)
			{
				mouseOverlay->show();
				showDashboardOverlays(true, trucks[current_truck]->driveable);
				if(bigMap) bigMap->setVisibility(true);
			}
		}
		dirty=true;
		mTimeUntilNextToggle = 0.2;
	}

#ifdef HAS_EDITOR
	if (INPUTENGINE.getEventBoolValue("COMMON_START_TRUCK_EDITOR") && (loading_state==ALL_LOADED) && mTimeUntilNextToggle <= 0 && !useIngameEditor)
	{
		if (!trucked) trucked=new TruckEditor(truckeditorOverlay, mWindow->getWidth(), mWindow->getHeight());
		showTruckEditorOverlay(true);
		loading_state=EDITING;

		mTimeUntilNextToggle = 0.2;
	}
#endif
	if (INPUTENGINE.getEventBoolValue("COMMON_TOGGLE_STATS") && mTimeUntilNextToggle <= 0)
	{
		dirty=true;
		mStatsOn = !mStatsOn;
		showDebugOverlay(mStatsOn);

		mTimeUntilNextToggle = 0.2;
	}

	if (INPUTENGINE.getEventBoolValue("COMMON_TOGGLE_MAT_DEBUG") && mTimeUntilNextToggle <= 0)
	{
		static bool showMatDebug = false;
		dirty=true;
		showMatDebug = !showMatDebug;
		showDebugOverlay(true, showMatDebug);

		mTimeUntilNextToggle = 0.2;
	}

	// grass beta - do not use in release
#ifdef PAGED
	if (INPUTENGINE.getEventBoolValue("GRASS_MORE") && mTimeUntilNextToggle <= 0)
	{
		Vector3 pos = Vector3::ZERO;
		if(current_truck == -1)
			pos = person->getPosition();
		else
			pos = trucks[current_truck]->getPosition();
		setGrassDensity(pos.x, pos.z, 20, true);
		mTimeUntilNextToggle = 0.2;
	}
#endif
	if (INPUTENGINE.getEventBoolValue("MAP_INTERACTIVE_TOGGLE") && mTimeUntilNextToggle <= 0 && mtc)
	{
		if(mtc && bigMap)
		{
			if(interactivemap)
			{
				interactivemap=0;
				mtc->setCamZoom(((mapsizex+mapsizez)/2)*0.5); // zoom that fits 1:1 to the map
				mtc->setCamPosition(Vector3(mapsizex/2, hfinder->getHeightAt(mapsizex/2, mapsizez/2) , mapsizez/2), Quaternion(Degree(0), Vector3::UNIT_X));
				mtc->update();
				bigMap->setEntityVisibility(true);
				LogManager::getSingleton().logMessage("disabled interactive Map");
			} else
			{
				mtc->setCamZoom(30); // zoom very near
				bigMap->setEntityVisibility(false);
				interactivemap=1;
				LogManager::getSingleton().logMessage("enabled interactive Map");
			}
		}
		mTimeUntilNextToggle = 0.5;
	}

	if (INPUTENGINE.getEventBoolValue("MAP_IN") && mTimeUntilNextToggle <= 0 && interactivemap && mtc)
	{
		//LogManager::getSingleton().logMessage("zoom in");
		if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
			mtc->setCamZoomRel(4);
		else
			mtc->setCamZoomRel(1);
		mtc->update();
	}
	if (INPUTENGINE.getEventBoolValue("MAP_OUT") && mTimeUntilNextToggle <= 0 && interactivemap && mtc)
	{
		//LogManager::getSingleton().logMessage("zoom out");
		if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
			mtc->setCamZoomRel(-4);
		else
			mtc->setCamZoomRel(-1);
		mtc->update();
	}
#ifdef PAGED
//TODO: fix code below
#if 0
	if (INPUTENGINE.getEventBoolValue("GRASS_LESS") && mTimeUntilNextToggle <= 0)
	{
		if(grass)
		{
			Vector3 pos = Vector3::ZERO;
			if(current_truck == -1)
				pos = person->getPosition();
			else
				pos = trucks[current_truck]->getPosition();
			setGrassDensity(pos.x, pos.z, -20, true);
			mTimeUntilNextToggle = 0.2;
		}
	}

	if (INPUTENGINE.getEventBoolValue("GRASS_SAVE") && mTimeUntilNextToggle <= 0)
	{
		if(grass)
		{
			saveGrassDensity();
			mTimeUntilNextToggle = 1;
		}
	}

	if (INPUTENGINE.getEventBoolValue("GRASS_MOST") && mTimeUntilNextToggle <= 0)
	{
		if(grass)
		{
			Vector3 pos = Vector3::ZERO;
			if(current_truck == -1)
				pos = person->getPosition();
			else
				pos = trucks[current_truck]->getPosition();
			setGrassDensity(pos.x, pos.z, 255);
			mTimeUntilNextToggle = 0.2;
		}
	}

	if (INPUTENGINE.getEventBoolValue("GRASS_NONE") && mTimeUntilNextToggle <= 0)
	{
		if(grass)
		{
			Vector3 pos = Vector3::ZERO;
			if(current_truck == -1)
				pos = person->getPosition();
			else
				pos = trucks[current_truck]->getPosition();
			setGrassDensity(pos.x, pos.z, 0);
			mTimeUntilNextToggle = 0.2;
		}
	}
#endif //0
#endif //paged
	if (INPUTENGINE.getEventBoolValue("COMMON_OUTPUT_POSITION") && mTimeUntilNextToggle <= 0 && loading_state == ALL_LOADED)
	{
		Vector3 pos = Vector3::ZERO;
		float rotz = 0;
		if(current_truck == -1)
		{
			pos = person->getPosition();
			rotz = person->getOrientation().getYaw().valueDegrees()+180;
		}
		else
		{
			pos = trucks[current_truck]->getPosition();
			Vector3 idir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[0]].RelPosition;
			rotz = atan2(idir.dotProduct(Vector3::UNIT_X), idir.dotProduct(-Vector3::UNIT_Z));
			rotz = -Radian(rotz).valueDegrees();
		}
		LogManager::getSingleton().logMessage("position " + StringConverter::toString(pos.x) + ", "+ StringConverter::toString(pos.y) + ", " + StringConverter::toString(pos.z) + ", 0, " + StringConverter::toString(rotz)+", 0");

		mTimeUntilNextToggle = 0.2;
	}

	//update window
	if (!mWindow->isAutoUpdated())
	{
#ifdef HAS_EDITOR
		if ((trucked && trucked->dirty)||dirty) {trucked->dirty=false;mWindow->update();}
#else
		if (dirty) {mWindow->update();}
#endif
		else
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
			Sleep(10);
#endif
#if OGRE_PLATFORM == OGRE_PLATFORM_LINUX
		usleep(10000);
#endif
#if OGRE_PLATFORM == OGRE_PLATFORM_APPLE
		usleep(10000);
#endif
	}
	// Return true to continue rendering
	return true;
}

int ExampleFrameListener::addTruck(char *fname, Vector3 pos)
{
	trucks[free_truck] = new Beam(free_truck, mSceneMgr, mSceneMgr->getRootSceneNode(), mWindow, &mapsizex, &mapsizez, pos.x, pos.y, pos.z, Quaternion::ZERO, fname, collisions, dustp, clumpp, sparksp, dripp, splashp, ripplep, hfinder, w, mCamera, mirror, true,false,false,0,false,flaresMode);

	if(bigMap)
	{
		MapEntity *e = bigMap->createNamedMapEntity("Truck"+StringConverter::toString(free_truck), MapControl::getTypeByDriveable(trucks[free_truck]->driveable));
		e->setState(DESACTIVATED);
		e->setVisibility(true);
		e->setPosition(reload_pos);
		e->setRotation(-Radian(trucks[free_truck]->getHeadingDirectionAngle()));
	}

	int num = free_truck;
	free_truck++;
	return num;
}

void ExampleFrameListener::shutdown_final()
{
	LogManager::getSingleton().logMessage(" ** Shutdown final");
	if (editorfd) fclose(editorfd);
	if (w) w->prepareShutdown();
	if (dashboard) dashboard->prepareShutdown();
	if (heathaze) heathaze->prepareShutdown();
	if (current_truck!=-1) trucks[current_truck]->prepareShutdown();
	INPUTENGINE.prepareShutdown();

	// destroy input things properly
	//mInputManager->destroyInputObject(mMouse); mMouse = 0;
	//mInputManager->destroyInputObject(mKeyboard); mKeyboard = 0;
	//OIS::InputManager::destroyInputSystem(mInputManager); mInputManager = 0;

	shutdownall=true;
	//terrainmaterial->getBestTechnique()->getPass(0)->getTextureUnitState(0)->setTextureName(terrainoriginalmaterial);

	//delete mCaelumSystem;mCaelumSystem = 0;
}

void ExampleFrameListener::shutdown_pre()
{
	LogManager::getSingleton().logMessage(" ** Shutdown preparation");
	MYGUI.shutdown();
	if (net) net->disconnect();
	showcredits=1;
	loading_state=EXITING;
	OverlayManager::getSingleton().getByName(_L("tracks/CreditsOverlay"))->show();
	ssm->soundEnable(false);
}

void ExampleFrameListener::hideMap()
{
	if(bigMap)
		bigMap->setVisibility(false);
}

void ExampleFrameListener::processConsoleInput()
{
	String chatline = INPUTENGINE.getKeyLine();

	if (chatline.size()==0) return;
	if(!CONSOLE.getScriptMode())
	{
		// normal chat mode

#ifdef ANGELSCRIPT
		// switch to script mode?
		if(chatline == "/console")
		{
			CONSOLE.addText(_L("^8 Player: ^7") + chatline);
			CONSOLE.setScriptMode(true);

		} else 
		{
#endif //ANGELSCRIPT

			if(netmode)
			{
				CONSOLE.addText("^8" + ColoredTextAreaOverlayElement::StripColors(String(net->getNickname())) + ": ^7" + ColoredTextAreaOverlayElement::StripColors(chatline), false);
				net->sendChat(const_cast<char *>(chatline.c_str()));
			} else
				CONSOLE.addText(_L("^8 Player: ^7") + chatline);

#ifdef ANGELSCRIPT
		}
#endif //ANGELSCRIPT

	} else
	{
		// script mode
#ifdef ANGELSCRIPT
		//execute string
		CONSOLE.addText(chatline);
		if(scriptEngine) scriptEngine->executeString(chatline);
#else
		CONSOLE.addText(_L("No scripting support compiled in."));
		CONSOLE.setScriptMode(false);
#endif //ANGELSCRIPT
	}
	CONSOLE.setEnterText("", false);
	//CONSOLE.noScroll();
}

void ExampleFrameListener::loadTerrain(String terrainfile)
{
	loadedTerrain = terrainfile;
#ifdef XFIRE
	updateXFire();
#endif

	UILOADER.setProgress(60, _L("Loading Terrain"), true);
	bool disableMap = (SETTINGS.getSetting("disableOverViewMap") == "Yes");

	// map must be loaded before lua!
	// init the map
	if(!disableMap)
	{
		bigMap = new MapControl((int)mapsizex, (int)mapsizez);
		// important: update first!
		bigMap->updateRenderMetrics(mWindow);
		bigMap->setVisibility(true);
		if(mtc) mtc->update();
		//make it small again
		bigMap->updateRenderMetrics(mWindow);
		bigMap->setPosition(0, 0.81, 0.14, 0.19, mWindow);
		if(net)
			playerListOverlay->hide();
		//bigMap->setPosition(0, 0, 1, 1);
		//bigMap->resizeToScreenRatio(win);
	}

#ifdef ANGELSCRIPT
	LogManager::getSingleton().logMessage("Loading Angelscript Script engine." );
	scriptEngine = new ScriptEngine(this);
	scriptEngine->loadTerrainScript(terrainfile+".sa");
#endif


#ifdef LUASCRIPT
	//setup lua
	LogManager::getSingleton().logMessage("Loading LUA Script engine." );
	lua=new LuaSystem(this);
	//setup collision system
	collisions=new Collisions(lua, this, debugCollisions);

	lua->loadTerrain(terrainfile);
#else
  collisions=new Collisions(this, debugCollisions);
#endif

	//we load terrain
	//FILE *fd;
	char geom[1024];
	char sandstormcubemap[255];
	sandstormcubemap[0]=0;
	char line[1024];
	float r,g,b;
	float cx,cy,cz;
	String group="";
	try
	{
		group = ResourceGroupManager::getSingleton().findGroupContainingResource(terrainfile);
	}catch(...)
	{
	}
	if(group == "")
	{
		// we need to do a bit more here, since this can also happen on joining a MP server, in that case the user should get a better error message ...
		LogManager::getSingleton().logMessage("Terrain not found: " + String(terrainfile));
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
		String msg = _L("Terrain not found: ") + terrainfile;
		MessageBox( NULL, msg.c_str(), _L("Terrain loading error").c_str(), MB_OK | MB_ICONERROR | MB_TASKMODAL);
#elif OGRE_PLATFORM == OGRE_PLATFORM_LINUX
		printf("\n\nTerrain not found: %s\n\n", terrainfile.c_str());
#elif OGRE_PLATFORM == OGRE_PLATFORM_APPLE
		printf("\n\nTerrain not found: %s\n\n", terrainfile.c_str());
#endif
		exit(123);
	}

	DataStreamPtr ds=ResourceGroupManager::getSingleton().openResource(terrainfile, group);
	ds->readLine(line, 1023);
	//geometry
	ds->readLine(geom, 1023);
	//colour
	ds->readLine(line, 1023);
	//water stuff
	float waterline=-9999;
	if (line[0]=='w')
	{
		sscanf(line+1, "%f", &waterline);
		//fscanf(fd," %[^\n\r]",line);
		ds->readLine(line, 1023);
	};
	//caelum maps
	if (!strncmp(line,"caelum", 6))
	{
		caelum_mapped=true;
		//fscanf(fd," %[^\n\r]",line);
		ds->readLine(line, 1023);
	};

	sscanf(line,"%f, %f, %f",&r,&g,&b);
	//coordinates
	//fscanf(fd," %[^\n\r]",line);
	ds->readLine(line, 1023);
	sscanf(line, "%f, %f, %f, %f, %f, %f, %f, %f, %f",&truckx,&trucky,&truckz,&cx,&cy,&cz, &persostart.x, &persostart.y, &persostart.z);
	spawn_location_t spl;
	memset(&spl, 0, sizeof(spl));
	spl.pos = Vector3(truckx, trucky, truckz);
	spl.rot = Quaternion::ZERO;
	netSpawnPos["truck"] = spl;
	netSpawnPos["airplane"] = spl;
	netSpawnPos["car"] = spl;

	//shadows
	if (SETTINGS.getSetting("Shadow technique")=="Stencil shadows (best looking)")
	{
		float scoef=0.2;
		mSceneMgr->setShadowColour(ColourValue(0.563+scoef, 0.578+scoef, 0.625+scoef));

		mSceneMgr->setShadowTechnique(SHADOWTYPE_STENCIL_MODULATIVE); //projects on ground
		//        mSceneMgr->setShadowTechnique(SHADOWTYPE_STENCIL_ADDITIVE); //does not project on ground
		//		mSceneMgr->setShadowIndexBufferSize(2000000);
		mSceneMgr->setShadowDirectionalLightExtrusionDistance(100);
		mSceneMgr->setShadowFarDistance(550);

		//important optimization
		mSceneMgr->getRenderQueue()->getQueueGroup(RENDER_QUEUE_WORLD_GEOMETRY_1)->setShadowsEnabled(false);

		//		mSceneMgr->setUseCullCamera(false);
		//		mSceneMgr->setShowBoxes(true);
		//		mSceneMgr->showBoundingBoxes(true);
		//		mSceneMgr->setShowDebugShadows(true);
	}
	if (SETTINGS.getSetting("Shadow technique")=="Texture shadows")
	{
		float scoef=0.2;
		mSceneMgr->setShadowColour(ColourValue(0.563+scoef, 0.578+scoef, 0.625+scoef));

		mSceneMgr->setShadowTechnique(SHADOWTYPE_TEXTURE_MODULATIVE);
		//		LiSPSMShadowCameraSetup* m_LiSPSMSetup = new Ogre::LiSPSMShadowCameraSetup();
		//      m_LiSPSMSetup->setOptimalAdjustFactor( 1 );
		//      ShadowCameraSetupPtr m_currentShadowCameraSetup = Ogre::ShadowCameraSetupPtr(m_LiSPSMSetup);
		//		mSceneMgr->setShadowCameraSetup(m_currentShadowCameraSetup);

		//		FocusedShadowCameraSetup* m_FocusedSetup = new Ogre::FocusedShadowCameraSetup();
		//      ShadowCameraSetupPtr m_currentShadowCameraSetup = Ogre::ShadowCameraSetupPtr(m_FocusedSetup);
		//		mSceneMgr->setShadowCameraSetup(m_currentShadowCameraSetup);

		mSceneMgr->setShadowFarDistance(50);
		mSceneMgr->setShadowTextureSettings(1024,1);
	}
	ColourValue fadeColour(r,g,b);

	bool fogEnable = true;
	if (SETTINGS.getSetting("Fog") == "No")
	{
		fogEnable = false;
		fogmode=0;
	}

	float farclipPercent = 0.3;
	if (SETTINGS.getSetting("FarClip Percent") != "")
		farclipPercent = StringConverter::parseInt(SETTINGS.getSetting("FarClip Percent"));

	float farclip = 1000;
	terrainxsize=1000;
	terrainzsize=1000;
	{
		//compute farclip from terrain size
	    ConfigFile config;
		ResourceGroupManager& rgm = ResourceGroupManager::getSingleton();
		String group="";
		try
		{
			group = ResourceGroupManager::getSingleton().findGroupContainingResource(geom);
		}catch(...)
		{
		}
		if(group == "")
			return;
		DataStreamPtr stream=rgm.openResource(geom, group);
		config.load( stream );
	    String val;
		float fval;
		val = config.getSetting("PageWorldX");
		if ( !val.empty() )
			fval = atof( val.c_str() );
		farclip=fval;
		terrainxsize=fval;
		val = config.getSetting("PageWorldZ");
		if ( !val.empty() )
			fval = atof( val.c_str() );
		if (fval>farclip) farclip=fval;
		terrainzsize=fval;
		//we take farclip=1/3rd the terrain size (default)
		farclip = farclip * farclipPercent / 100.0f;
		if (farclip<1000.0)
			//cap for small terrains
			farclip=1000.0;
	}

	String fcos = SETTINGS.getSetting("Farclip");
	if(fcos != "")
		farclip = atof(fcos.c_str());

	LogManager::getSingleton().logMessage("Farclip computed:" + StringConverter::toString(farclip));

	float fogstart = 0;

	//caelum skies
	if (SETTINGS.getSetting("Sky effects")=="Caelum (best looking, slower)")
	{
		//mCamera->setNearClipDistance (0.01);
		mCamera->setFarClipDistance( farclip*1.733 );

		// Initialise Caelum
		ResourceGroupManager::getSingleton ().createResourceGroup ("Caelum");
		mCaelumSystem = new caelum::CaelumSystem (mRoot, mSceneMgr);
		mCaelumSystem->getSun ()->setInclination (Degree (13));

		// Create and configure the sky colours model to use
		mCaelumModel = new caelum::StoredImageSkyColourModel ();
		mCaelumSystem->setSkyColourModel (mCaelumModel);	// Call this before changing the gradients image!!
		static_cast<caelum::StoredImageSkyColourModel *>(mCaelumModel)->setSkyGradientsImage ("EarthClearSky.png");


//		Real fogdensity = 0.005;
		fogdensity = 5.0/farclip;
//		if (SETTINGS.getSetting("Caelum Fog Density") != "")
//			fogdensity = StringConverter::parseReal(SETTINGS.getSetting("Caelum Fog Density"));

		if(fogEnable)
		{
			fogmode=1;
			mCaelumSystem->setManageFog(true);
			static_cast<caelum::StoredImageSkyColourModel *>(mCaelumModel)->setFogColoursImage ("EarthClearSkyFog.png");
			static_cast<caelum::StoredImageSkyColourModel *>(mCaelumModel)->setFogDensity (fogdensity);
		}
		else
		{
			fogmode=2;
			mCaelumSystem->setManageFog(true);
			static_cast<caelum::StoredImageSkyColourModel *>(mCaelumModel)->setFogColoursImage ("EarthClearSkyFog.png");
			static_cast<caelum::StoredImageSkyColourModel *>(mCaelumModel)->setFogDensity(0);
		}

		// Create a sky dome
		caelum::SkyDome *dome = mCaelumSystem->createSkyDome ();
		dome->setSize(farclip);

		// Create a starfield
		//mWindow->addListener (mCaelumSystem->createStarfield ("Starfield.jpg"));
		//mCaelumSystem->getStarfield ()->setInclination (Degree (13));

		//add a listener to change terrain visual
		mCaelumSystem->addListener (new TerrainUpdater(this));

		// Register all to the render window
		mCaelumSystem->registerAllToTarget (mWindow);

		// Set some time parameters
		time_t t = time (&t);

		// t2 is unused
		//struct tm *t2 = localtime (&t);

		//			mCaelumSystem->setTimeScale (10.0);
		mCaelumSystem->setTimeScale (1.0);
		//			mCaelumSystem->setLocalTime (3600 * t2->tm_hour + 60 * t2->tm_min + t2->tm_sec);
		mCaelumSystem->setLocalTime (3600 * 15 );
		mCaelumSystem->setUpdateRate (1.0 / (24.0*60.0));
		//			mCaelumSystem->setUpdateRate (1.0/60.0);
	}
	else
	{
		fogmode=3;
		fogstart = farclip * 0.8;
//		if (SETTINGS.getSetting("Sandstorm Fog Start") != "")
//			fogstart = StringConverter::parseLong(SETTINGS.getSetting("Sandstorm Fog Start"));

		// Create a light
		Light* l = mSceneMgr->createLight("MainLight");
		//directional light for shadow
		l->setType(Light::LT_DIRECTIONAL);
		l->setDirection(0.785, -0.423, 0.453);

		l->setDiffuseColour(fadeColour);
		l->setSpecularColour(fadeColour);

		mCamera->setFarClipDistance( farclip*1.733 );

		// Fog
		// NB it's VERY important to set this before calling setWorldGeometry
		// because the vertex program picked will be different
		if(fogEnable)
		{
			fogdensity = 0.001;
			mSceneMgr->setFog(FOG_LINEAR, fadeColour, fogdensity, fogstart, farclip);
		} else
			mSceneMgr->setFog(FOG_LINEAR, fadeColour, 0, 999998, 999999);

		//mSceneMgr->setSkyBox(true, sandstormcubemap, farclip);
		//mSceneMgr->setSkyDome(true, "Examples/CloudySky", 5, 8);

		mCamera->getViewport()->setBackgroundColour(fadeColour);
	}

//	MaterialPtr terMat = ((TerrainSceneManager*)mSceneMgr)->getTerrainMaterial(); //can't do that
	MaterialPtr terMat = (MaterialPtr)(MaterialManager::getSingleton().getByName("TerrainSceneManager/Terrain"));
	{
		// load configuration from STM (sizes)
		ConfigFile cfg;
		String group="";
		try
		{
			group = ResourceGroupManager::getSingleton().findGroupContainingResource(String(geom));
		}catch(...)
		{
		}
		if(group == "")
			return;
		DataStreamPtr ds_config = ResourceGroupManager::getSingleton().openResource(String(geom), group);
		cfg.load(ds_config, "\t:=", false);

		// X and Z scale
		String tmpSize = cfg.getSetting("PageWorldX");
		if (tmpSize != String(""))
			mapsizex = atof(tmpSize.c_str());

		tmpSize = cfg.getSetting("PageWorldZ");
		if (tmpSize != String(""))
			mapsizez = atof(tmpSize.c_str());

		if(!disableMap && bigMap)
			bigMap->setWorldSize(mapsizex, mapsizez);

		mSceneMgr -> setWorldGeometry(geom);
	}



	// get vegetation mode
	int pagedMode = 0; //None
	float pagedDetailFactor = 0;
	if(SETTINGS.getSetting("Vegetation") == "None (fastest)")
	{
		pagedMode = 0;
		pagedDetailFactor = 0.001;
	}
	else if(SETTINGS.getSetting("Vegetation") == "20%")
	{
		pagedMode = 1;
		pagedDetailFactor = 0.2;
	}
	else if(SETTINGS.getSetting("Vegetation") == "50%")
	{
		pagedMode = 2;
		pagedDetailFactor = 0.5;
	}
	else if(SETTINGS.getSetting("Vegetation") == "Full (best looking, slower)")
	{
		pagedMode = 3;
		pagedDetailFactor = 1;
	}

	// Define the required skyplane
	Plane plane;
	// 1000 world units from the camera
	plane.d = 200;
	// Above the camera, facing down
	plane.normal = -Vector3::UNIT_Y;


	if(!terMat.isNull())
		terrainmaterial = terMat.get();
	if(terrainmaterial)
		LogManager::getSingleton().logMessage("using Terrain Material '"+terrainmaterial->getName()+"'");

	//create sky material
	//			MaterialPtr skmat=(MaterialPtr)(MaterialManager::getSingleton().create("Skycol", "Standard"));
	//			Technique* sktechnique = skmat->getTechnique(0);
	//			Pass* skpass = sktechnique->getPass(0);
	//			skpass->setDepthWriteEnabled(false);
	//			skpass->setLightingEnabled(false);
	//			TextureUnitState* sktunit=skpass->createTextureUnitState();
	//			sktunit->setColourOperationEx(LBX_MODULATE, LBS_MANUAL, LBS_CURRENT, fadeColour);

	//		mSceneMgr->setSkyPlane(true, plane, "tracks/skyplanecol", 1000, 1, true, 0.5, 100, 100);
	//		mSceneMgr->setSkyBox(true, "tracks/skycol", 1000);

	//bloom effect
	if (SETTINGS.getSetting("Bloom")=="Yes")
	{
		CompositorManager::getSingleton().addCompositor(mCamera->getViewport(),"Bloom");
		CompositorManager::getSingleton().setCompositorEnabled(mCamera->getViewport(), "Bloom", true);
	}
	// Motion blur stuff :)
	if (SETTINGS.getSetting("Motion blur")=="Yes")
	{
		/// Motion blur effect
		CompositorPtr comp3 = CompositorManager::getSingleton().create(
			"MotionBlur", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME
			);
		{
			CompositionTechnique *t = comp3->createTechnique();
			{
				CompositionTechnique::TextureDefinition *def = t->createTextureDefinition("scene");
				def->width = 0;
				def->height = 0;
				def->format = PF_R8G8B8;
			}
			{
				CompositionTechnique::TextureDefinition *def = t->createTextureDefinition("sum");
				def->width = 0;
				def->height = 0;
				def->format = PF_R8G8B8;
			}
			{
				CompositionTechnique::TextureDefinition *def = t->createTextureDefinition("temp");
				def->width = 0;
				def->height = 0;
				def->format = PF_R8G8B8;
			}
			/// Render scene
			{
				CompositionTargetPass *tp = t->createTargetPass();
				tp->setInputMode(CompositionTargetPass::IM_PREVIOUS);
				tp->setOutputName("scene");
			}
			/// Initialisation pass for sum texture
			{
				CompositionTargetPass *tp = t->createTargetPass();
				tp->setInputMode(CompositionTargetPass::IM_PREVIOUS);
				tp->setOutputName("sum");
				tp->setOnlyInitial(true);
			}
			/// Do the motion blur
			{
				CompositionTargetPass *tp = t->createTargetPass();
				tp->setInputMode(CompositionTargetPass::IM_NONE);
				tp->setOutputName("temp");
				{ CompositionPass *pass = tp->createPass();
				pass->setType(CompositionPass::PT_RENDERQUAD);
				pass->setMaterialName("Ogre/Compositor/Combine");
				pass->setInput(0, "scene");
				pass->setInput(1, "sum");
				}
			}
			/// Copy back sum texture
			{
				CompositionTargetPass *tp = t->createTargetPass();
				tp->setInputMode(CompositionTargetPass::IM_NONE);
				tp->setOutputName("sum");
				{ CompositionPass *pass = tp->createPass();
				pass->setType(CompositionPass::PT_RENDERQUAD);
				pass->setMaterialName("Ogre/Compositor/Copyback");
				pass->setInput(0, "temp");
				}
			}
			/// Display result
			{
				CompositionTargetPass *tp = t->getOutputTargetPass();
				tp->setInputMode(CompositionTargetPass::IM_NONE);
				{ CompositionPass *pass = tp->createPass();
				pass->setType(CompositionPass::PT_RENDERQUAD);
				pass->setMaterialName("Ogre/Compositor/MotionBlur");
				pass->setInput(0, "sum");
				}
			}
		}
		CompositorManager::getSingleton().addCompositor(mCamera->getViewport(),"MotionBlur");
		CompositorManager::getSingleton().setCompositorEnabled(mCamera->getViewport(), "MotionBlur", true);
		/*						RenderTexture *motionBlurTexture = mRoot->getRenderSystem ()->createRenderTexture ("MotionBlurTexture", 1280, 1024);
		motionBlurTexture->addViewport (mCamera);
		motionBlurTexture->getViewport(0)->setBackgroundColour(fadeColour);
		//motionBlurTexture->getViewport(0)->setOverlaysEnabled (false);

		Material *motionBlurMaterial;
		MaterialPtr tmp;
		tmp=(MaterialPtr)MaterialManager::getSingleton().create("MotionBlurMaterial", "Main");
		motionBlurMaterial = &(*tmp);
		//Material *motionBlurMaterial = mSceneMgr->createMaterial ("MotionBlurMaterial");

		TextureUnitState *tUnit = motionBlurMaterial->getTechnique (0)->getPass (0)->createTextureUnitState ();
		tUnit->setTextureName ("MotionBlurTexture");
		tUnit->setAlphaOperation (LBX_BLEND_MANUAL, LBS_MANUAL, LBS_CURRENT, 0.5, 1, 1);
		motionBlurMaterial->setSceneBlending (SBT_TRANSPARENT_ALPHA);
		motionBlurMaterial->setDepthCheckEnabled (false);
		motionBlurMaterial->setDepthWriteEnabled (false);

		//        Overlay *motionBlurOverlay = mSceneMgr->createOverlay ("MotionBlurOverlay", 10);
		Overlay *motionBlurOverlay = OverlayManager::getSingleton().create ("MotionBlurOverlay");
		motionBlurOverlay->setZOrder(10);

		OverlayElement *motionBlurPanel = OverlayManager::getSingleton ().createOverlayElement ("Panel", "MotionBlurPanel", false);
		//		GuiElement *motionBlurPanel = GuiManager::getSingleton ().createGuiElement ("Panel", "MotionBlurPanel", false);
		motionBlurPanel->setMaterialName ("MotionBlurMaterial");
		motionBlurOverlay->add2D ((OverlayContainer *)motionBlurPanel);
		motionBlurOverlay->show ();*/
	}
	// End of motion blur :(

	//SUNBURN
	if (SETTINGS.getSetting("Sunburn")=="Yes")
	{
		CompositorManager::getSingleton().addCompositor(mCamera->getViewport(),"Sunburn");
		CompositorManager::getSingleton().setCompositorEnabled(mCamera->getViewport(), "Sunburn", true);
	}



	//hack
	if (CompositorManager::getSingleton().hasCompositorChain(mCamera->getViewport()))
	{
		//CompositorManager::getSingleton().getCompositorChain(mCamera->getViewport())->getCompositor(0)->getTechnique()->getOutputTargetPass()->getPass(0)->setClearColour(fadeColour);
		CompositorManager::getSingleton().getCompositorChain(mCamera->getViewport())->_getOriginalSceneCompositor()->getTechnique()->getOutputTargetPass()->getPass(0)->setClearColour(fadeColour);
	}


	// not such ugly anymore: read .raw heightmap filename out of the terrain .cfg file
	{
		ConfigFile config;
		ResourceGroupManager& rgm = ResourceGroupManager::getSingleton();
		String group="";
		try
		{
			group = ResourceGroupManager::getSingleton().findGroupContainingResource(String(geom));
		}catch(...)
		{
		}
		if(group == "")
			return;
		DataStreamPtr stream=rgm.openResource(geom, group);
		config.load(stream);
		String val = config.getSetting("Heightmap.image");
		if (!val.empty())
		{
			strcpy(terrainmap, val.c_str());
		} else
		{
			// bad ugly hack
			strcpy(terrainmap, geom);
			terrainmap[strlen(terrainmap)-3]='r';
			terrainmap[strlen(terrainmap)-2]='a';
			terrainmap[strlen(terrainmap)-1]='w';
		}
		//ensure file gets closed again using this braces
	}

	//set terrain map
	//MaterialPtr mat=(MaterialPtr)(MaterialManager::getSingleton().getByName("tracks/BigMap"));
	if(terrainmaterial && bigMap)
	//mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName(terrainmaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getTextureName());
		bigMap->setBackground(terrainmaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0)->getTextureName());

	mCamera->setPosition(Vector3(cx,cy,cz));

	//water!
	bool useHydrax = (SETTINGS.getSetting("Hydrax") == "Yes");
	String hydraxConfig = "hydrax_default.hdx";

	if (waterline!=-9999)
	{
		bool usewaves=(SETTINGS.getSetting("Waves")=="Yes");

		// disable waves in multiplayer
		if(net)
			usewaves=false;

#ifndef HYDRAX
	useHydrax=false;
#endif

    if(useHydrax)
    {
#ifdef HYDRAX
      w = new HydraxWater(WATER_BASIC, mCamera, mSceneMgr, mWindow, waterline, &mapsizex, &mapsizez, usewaves);
#endif
    }
    else if(!useHydrax)
		{
			if (SETTINGS.getSetting("Water effects")=="None")
				w=0;
			else if (SETTINGS.getSetting("Water effects")=="Basic (fastest)")
				w=new WaterOld(WATER_BASIC, mCamera, mSceneMgr, mWindow, waterline, &mapsizex, &mapsizez, usewaves);
			else if (SETTINGS.getSetting("Water effects")=="Reflection")
				w=new WaterOld(WATER_REFLECT, mCamera, mSceneMgr, mWindow, waterline, &mapsizex, &mapsizez, usewaves);
			else if (SETTINGS.getSetting("Water effects")=="Reflection + refraction (speed optimized)")
				w=new WaterOld(WATER_FULL_SPEED, mCamera, mSceneMgr, mWindow, waterline, &mapsizex, &mapsizez, usewaves);
			else if (SETTINGS.getSetting("Water effects")=="Reflection + refraction (quality optimized)")
				w=new WaterOld(WATER_FULL_QUALITY, mCamera, mSceneMgr, mWindow, waterline, &mapsizex, &mapsizez, usewaves);
		}
	}

	//environment map
	//envmap is always created!
	if (SETTINGS.getSetting("Envmapdisable")!="Yes")
	{
		envmap=new Envmap(mSceneMgr, mWindow, mCamera, SETTINGS.getSetting("Envmap")=="Yes");
	}

	//mirrors!
	if(SETTINGS.getSetting("Mirrors")=="Yes")
	{
		mirror = new Mirrors(mSceneMgr, mWindow, mCamera);
	}
	//dashboard
	if(SETTINGS.getSetting("Dashboard")=="Yes")
	{
		dashboard = new Dashboard(mSceneMgr,mWindow);
	}
	else
	{
		//we must do something to fix the texture mapping
		MaterialPtr mat = MaterialManager::getSingleton().getByName("renderdash");
		mat->getTechnique(0)->getPass(0)->getTextureUnitState(0)->setTextureName("virtualdashboard.dds");

	}


	//setup heightfinder
	float wheight=0.0;
	if (w) wheight=w->getHeight()-30.0;


	hfinder = new TSMHeightFinder(geom, terrainmap, wheight);
	collisions->setHfinder(hfinder);

	if(bigMap)
	{
		mtc = new MapTextureCreator(mScene, mCamera, this);
		//mtc->setCamClip(mapsizex*1.2);
		mtc->setCamZoom(((mapsizex+mapsizez)/2)*0.5);
		mtc->setCamPosition(Vector3(mapsizex/2, hfinder->getHeightAt(mapsizex/2, mapsizez/2) , mapsizez/2), Quaternion(Degree(0), Vector3::UNIT_X));
		mtc->setAutoUpdated(false); // important!
		bigMap->setVisibility(false);
		bigMap->setBackgroundMaterial(mtc->getMaterialName());
	}

	// fix the person starting position
	if(persostart.isZeroLength() && !spl.pos.isZeroLength())
		if(hfinder)
			persostart = Vector3(spl.pos.x, hfinder->getHeightAt(spl.pos.x, spl.pos.z), spl.pos.z);
		else
			persostart = spl.pos;

	//prepare road2
	proceduralManager = new ProceduralManager(mSceneMgr, hfinder, collisions);

	Vector3 r2lastpos=Vector3::ZERO;
	Quaternion r2lastrot=Quaternion::IDENTITY;
	int r2counter=0;

	//prepare for baking
	SceneNode *bakeNode=mSceneMgr->getRootSceneNode()->createChildSceneNode();
	//load objects
	//while (!feof(fd))
	bool proroad=false;
#ifdef PAGED
	int treemode=0;
	treeLoader = 0;
	Entity *curTree = 0;
	int treecounter=0;
	float treescale_min=1,treescale_max=1;
	String treename="";
#endif

	ProceduralObject po;
	po.loadingState = -1;
	int r2oldmode=0;

	while (!ds->eof())
	{
		char oname[1024];
		char type[256];
		char name[256];
		int r;
		float ox, oy, oz;
		float rx, ry, rz;
		//fscanf(fd," %[^\n\r]",line);
		size_t ll=ds->readLine(line, 1023);
		if (line[0]=='/' || ll==0) continue; //comments
		if (!strcmp("end",line)) break;
		//sandstorm cube texture
		if (!strncmp(line,"sandstormcubemap", 16))
		{
			sscanf(line, "sandstormcubemap %s", sandstormcubemap);
		};
		if (!strncmp("ground-model", line, 12))
		{
			collisions->setupLandUse(line+13);
			continue;
		}

#ifdef HYDRAX
		if (!strncmp("hydraxconfig", line, 12))
		{
			char tmp[255]="";
			int res = sscanf(line, "hydraxconfig %s", tmp);
			if(res < 1)
			{
				LogManager::getSingleton().logMessage("error reading hydraxconfig command!");
				continue;
			}
			hydraxConfig=String(tmp);
		}
#endif //HYDRAX
		if (!strncmp("mpspawn", line, 7))
		{
			spawn_location_t spl;
			memset(&spl, 0, sizeof(spawn_location_t));

			char tmp[255]="";
			float x=0,y=0,z=0, rx=0, ry=0, rz=0;
			int res = sscanf(line, "mpspawn %s %f %f %f %f %f %f", tmp, &x, &y, &z, &rx, &ry, &rz);
			if(res < 7)
			{
				LogManager::getSingleton().logMessage("error reading mpspawn command!");
				continue;
			}
			spl.pos = Vector3(x, y, z);
			spl.rot = Quaternion(Degree(rx), Vector3::UNIT_X)*Quaternion(Degree(ry), Vector3::UNIT_Y)*Quaternion(Degree(rz), Vector3::UNIT_Z);
			netSpawnPos[String(tmp)] = spl;
			continue;
		}
		if (!strncmp("gravity", line, 7))
		{
			int res = sscanf(line, "gravity %f", &gravity);
			if(res < 1)
			{
				LogManager::getSingleton().logMessage("error reading gravity command!");
			}
			continue;
		}
		//ugly stuff to parse map size
		if (!strncmp("mapsize", line, 7))
		{
			// this is deprecated!!! (replaced by direct .cfg reads)
			//sscanf(line, "mapsize %f, %f", &mapsizex, &mapsizez);
			continue;
		}
#ifdef PAGED
		//ugly stuff to parse trees :)
		if(!strncmp("trees", line, 5))
		{
			if(pagedMode==0)
				continue;
			char ColorMap[255]="";
			char DensityMap[255]="";
			char treemesh[255]="";
			float yawfrom=0, yawto=0, scalefrom=0, scaleto=0, highdens=1;
			int minDist=90, maxDist=700;
			sscanf(line, "trees %f, %f, %f, %f, %f, %d, %d, %s %s %s", &yawfrom, &yawto, &scalefrom, &scaleto, &highdens, &minDist, &maxDist, treemesh, ColorMap, DensityMap);
			if(strnlen(ColorMap, 3) == 0)
			{
				LogManager::getSingleton().logMessage("tree ColorMap map zero!");
				continue;
			}
			if(strnlen(DensityMap, 3) == 0)
			{
				LogManager::getSingleton().logMessage("tree DensityMap zero!");
				continue;
			}
			Forests::DensityMap *densityMap = Forests::DensityMap::load(DensityMap, CHANNEL_COLOR);
			if(!densityMap)
			{
				LogManager::getSingleton().logMessage("could not load densityMap: "+String(DensityMap));
				continue;
			}
			densityMap->setFilter(Forests::MAPFILTER_BILINEAR);
			//densityMap->setMapBounds(TRect(0, 0, mapsizex, mapsizez));
			
			paged_geometry_t paged;
			paged.geom = new PagedGeometry();
			//paged.geom->setTempDir(SETTINGS.getSetting("User Path") + "cache" + SETTINGS.getSetting("dirsep"));
			paged.geom->setCamera(mCamera);
			paged.geom->setPageSize(50);
			paged.geom->setInfinite();
			Ogre::TRect<Ogre::Real> bounds = TBounds(0, 0, mapsizex, mapsizez);
			//trees->setBounds(bounds);

			//Set up LODs
			//trees->addDetailLevel<EntityPage>(50);
			float min = minDist * pagedDetailFactor;
			if(min<10) min = 10;
			paged.geom->addDetailLevel<BatchPage>(min, min/2);
			float max = maxDist * pagedDetailFactor;
			if(max<10) max = 10;
			paged.geom->addDetailLevel<ImpostorPage>(max, max/10);
			TreeLoader2D *treeLoader = new TreeLoader2D(paged.geom, TBounds(0, 0, mapsizex, mapsizez));
			paged.geom->setPageLoader(treeLoader);
			treeLoader->setHeightFunction(&getTerrainHeight);
			if(String(ColorMap) != "none")
				treeLoader->setColorMap(ColorMap);			

			curTree = mSceneMgr->createEntity(String("paged_")+treemesh+StringConverter::toString(pagedGeometry.size()), treemesh);

			float density = 0, yaw=0, scale=0;
			int numTreesToPlace=0;
			int gridsize = 10;
			for(int x=0;x<mapsizex;x+=gridsize)
			{
				for(int z=0;z<mapsizez;z+=gridsize)
				{
					density = densityMap->_getDensityAt_Unfiltered(x, z, bounds);
					numTreesToPlace = (int)((float)(highdens) * density * pagedDetailFactor);
					float nx=0, nz=0;
					while(numTreesToPlace-->0)
					{
						nx = Math::RangeRandom(x, x + gridsize);
						nz = Math::RangeRandom(z, z + gridsize);
						yaw = Math::RangeRandom(yawfrom, yawto);
						scale = Math::RangeRandom(scalefrom, scaleto);
						treeLoader->addTree(curTree, Vector3(nx, 0, nz), Degree(yaw), (Ogre::Real)scale);
					}
				}
			}
			paged.loader = (void*)treeLoader;
			pagedGeometry.push_back(paged);
		}
		//ugly stuff to parse grass :)
		if (!strncmp("grass", line, 5) || !strncmp("grass2", line, 6))
		{
			// is paged geometry disabled by configuration?
			if(pagedMode==0)
				continue;
			int range=80;
			float SwaySpeed=0.5, SwayLength=0.05, SwayDistribution=10.0, minx=0.2, miny=0.2, maxx=1, maxy=0.6, Density=0.6, minH=-9999, maxH=9999;
			char grassmat[255]="";
			char ColorMap[255]="";
			char DensityMap[255]="";
			int growtechnique = 0;
			int techn = GRASSTECH_CROSSQUADS;
			if(!strncmp("grass2", line, 6))
				sscanf(line, "grass2 %d, %f, %f, %f, %f, %f, %f, %f, %f, %d, %f, %f, %d, %s %s %s", &range, &SwaySpeed, &SwayLength, &SwayDistribution, &Density, &minx, &miny, &maxx, &maxy, &growtechnique, &minH, &maxH, &techn, grassmat, ColorMap, DensityMap);
			else if(!strncmp("grass", line, 5))
				sscanf(line, "grass %d, %f, %f, %f, %f, %f, %f, %f, %f, %d, %f, %f, %s %s %s", &range, &SwaySpeed, &SwayLength, &SwayDistribution, &Density, &minx, &miny, &maxx, &maxy, &growtechnique, &minH, &maxH, grassmat, ColorMap, DensityMap);

			//Initialize the PagedGeometry engine
			try
			{
				paged_geometry_t paged;
				PagedGeometry *grass = new PagedGeometry(mCamera, 30);
				//Set up LODs

				grass->addDetailLevel<GrassPage>(range * pagedDetailFactor); // original value: 80

				//Set up a GrassLoader for easy use
				GrassLoader *grassLoader = new GrassLoader(grass);
				grass->setPageLoader(grassLoader);
				grassLoader->setHeightFunction(&getTerrainHeight);

				// render grass at first
				grassLoader->setRenderQueueGroup(RENDER_QUEUE_MAIN);

				GrassLayer* grassLayer = grassLoader->addLayer(grassmat);
				grassLayer->setHeightRange(minH, maxH);

				grassLayer->setAnimationEnabled((SwaySpeed>0));
				grassLayer->setSwaySpeed(SwaySpeed);
				grassLayer->setSwayLength(SwayLength);
				grassLayer->setSwayDistribution(SwayDistribution);

				grassdensityTextureFilename = String(DensityMap);

				grassLayer->setDensity(Density * pagedDetailFactor);
				if(techn>10)
					grassLayer->setRenderTechnique(static_cast<GrassTechnique>(techn-10), true);
				else
					grassLayer->setRenderTechnique(static_cast<GrassTechnique>(techn), false);

				grassLayer->setMapBounds(TBounds(0, 0, mapsizex, mapsizez));

				if(strcmp(ColorMap,"none") != 0)
				{
					grassLayer->setColorMap(ColorMap);
					grassLayer->setColorMapFilter(MAPFILTER_BILINEAR);
				}

				if(strcmp(DensityMap,"none") != 0)
				{
					grassLayer->setDensityMap(DensityMap);
					grassLayer->setDensityMapFilter(MAPFILTER_BILINEAR);
				}

				//grassLayer->setMinimumSize(0.5,0.5);
				//grassLayer->setMaximumSize(1.0, 1.0);

				grassLayer->setMinimumSize(minx, miny);
				grassLayer->setMaximumSize(maxx, maxy);

				// growtechnique
				if(growtechnique == 0)
					grassLayer->setFadeTechnique(FADETECH_GROW);
				else if(growtechnique == 1)
					grassLayer->setFadeTechnique(FADETECH_ALPHAGROW);
				else if(growtechnique == 2)
					grassLayer->setFadeTechnique(FADETECH_ALPHA);
				paged.geom = grass;
				paged.loader = (void*)grassLoader;
				pagedGeometry.push_back(paged);
			} catch(...)
			{
				LogManager::getSingleton().logMessage("error loading grass!");
			}

			continue;
		}
#endif //PAGED
		//ugly stuff to parse procedural roads
		if (!strncmp("begin_procedural_roads", line, 22))
		{
			proroad=true;
			po = ProceduralObject();
			po.loadingState = 1;
			r2oldmode=1;
			continue;
		}
		if (!strncmp("end_procedural_roads", line, 20))
		{
			proroad=false;
			if(r2oldmode)
			{
				if(proceduralManager)
					proceduralManager->addObject(po);
				po = ProceduralObject();
			}
			continue;
		}
		if (proroad)
		{

			float rwidth, bwidth, bheight;
			//position x,y,z rotation rx,ry,rz, width, border width, border height, type
			r=sscanf(line, "%f, %f, %f, %f, %f, %f, %f, %f, %f, %s",&ox,&oy,&oz, &rx, &ry, &rz, &rwidth, &bwidth, &bheight, oname);
			Vector3 pos=Vector3(ox, oy, oz);
			Quaternion rotation = Quaternion(Degree(rx), Vector3::UNIT_X)*Quaternion(Degree(ry), Vector3::UNIT_Y)*Quaternion(Degree(rz), Vector3::UNIT_Z);
			int roadtype=ROAD_AUTOMATIC;
			int pillartype = 0;
			if (!strcmp(oname, "flat")) roadtype=ROAD_FLAT;
			if (!strcmp(oname, "left")) roadtype=ROAD_LEFT;
			if (!strcmp(oname, "right")) roadtype=ROAD_RIGHT;
			if (!strcmp(oname, "both")) roadtype=ROAD_BOTH;
			if (!strcmp(oname, "bridge")) {roadtype=ROAD_BRIDGE;pillartype=1;}
			if (!strcmp(oname, "monorail")) {roadtype=ROAD_MONORAIL;pillartype=2;}
			if (!strcmp(oname, "monorail2")) {roadtype=ROAD_MONORAIL;pillartype=0;}
			if (!strcmp(oname, "bridge_no_pillars")) {roadtype=ROAD_BRIDGE;pillartype=0;}

			if(r2oldmode)
			{
				//fill object
				ProceduralPoint pp;
				pp.bheight = bheight;
				pp.bwidth = bwidth;
				pp.pillartype = pillartype;
				pp.position = pos;
				pp.rotation = rotation;
				pp.type = roadtype;
				pp.width = rwidth;

				po.points.push_back(pp);
			}
			continue;
		}
		//end of the ugly (somewhat)
		strcpy(name, "generic");
		memset(oname, 0, 255);
		memset(type, 0, 255);
		memset(name, 0, 255);
		r=sscanf(line, "%f, %f, %f, %f, %f, %f, %s %s %s",&ox,&oy,&oz, &rx, &ry, &rz, oname, type, name);
		if(r<6)
			continue;
		if((!strcmp(oname, "truck")) || (!strcmp(oname, "load") || (!strcmp(oname, "machine")) || (!strcmp(oname, "boat")) ))
		{
			if(!strcmp(oname, "boat") && !w)
				// no water so do not load boats!
				continue;
			String group="";
			String truckname=String(type);
			if(!CACHE.checkResourceLoaded(truckname, group))
			{
				LogManager::getSingleton().logMessage("error while loading terrain: truck " + String(type) + " not found. ignoring.");
				continue;
			}
			//this is a truck or load declaration
			truck_preload[truck_preload_num].px=ox;
			truck_preload[truck_preload_num].py=oy;
			truck_preload[truck_preload_num].pz=oz;
			truck_preload[truck_preload_num].ismachine=(!strcmp(oname, "machine"));
			truck_preload[truck_preload_num].rotation=Quaternion(Degree(rx), Vector3::UNIT_X)*Quaternion(Degree(ry), Vector3::UNIT_Y)*Quaternion(Degree(rz), Vector3::UNIT_Z);
			//truck_preload[truck_preload_num].ry=ry;
			strcpy(truck_preload[truck_preload_num].name, truckname.c_str());
			truck_preload_num++;
			continue;
		}
		if (   !strcmp(oname, "road")
			|| !strcmp(oname, "roadborderleft")
			|| !strcmp(oname, "roadborderright")
			|| !strcmp(oname, "roadborderboth")
			|| !strcmp(oname, "roadbridgenopillar")
			|| !strcmp(oname, "roadbridge"))
		{
			int pillartype = !(strcmp(oname, "roadbridgenopillar") == 0);
			//okay, this is a job for roads2
			int roadtype=ROAD_AUTOMATIC;
			if (!strcmp(oname, "road")) roadtype=ROAD_FLAT;
			Vector3 pos=Vector3(ox, oy, oz);
			Quaternion rotation;
			rotation=Quaternion(Degree(rx), Vector3::UNIT_X)*Quaternion(Degree(ry), Vector3::UNIT_Y)*Quaternion(Degree(rz), Vector3::UNIT_Z);
			if ((pos-r2lastpos).length()>20.0)
			{
				//break the road
				if (r2oldmode!=0)
				{
					//fill object
					ProceduralPoint pp;
					pp.bheight = 0.2;
					pp.bwidth = 1.4;
					pp.pillartype = pillartype;
					pp.position = r2lastpos+r2lastrot*Vector3(10.0,0,0);
					pp.rotation = r2lastrot;
					pp.type = roadtype;
					pp.width = 8;
					po.points.push_back(pp);

					// finish it and start new object
					if(proceduralManager)
						proceduralManager->addObject(po);
					po = ProceduralObject();
					r2oldmode=1;
				}
				r2oldmode=1;
				// beginning of new
				ProceduralPoint pp;
				pp.bheight = 0.2;
				pp.bwidth = 1.4;
				pp.pillartype = pillartype;
				pp.position = pos;
				pp.rotation = rotation;
				pp.type = roadtype;
				pp.width = 8;
				po.points.push_back(pp);
			}
			else
			{
				//fill object
				ProceduralPoint pp;
				pp.bheight = 0.2;
				pp.bwidth = 1.4;
				pp.pillartype = pillartype;
				pp.position = pos;
				pp.rotation = rotation;
				pp.type = roadtype;
				pp.width = 8;
				po.points.push_back(pp);
			}
			r2lastpos=pos;
			r2lastrot=rotation;


			continue;
		}
		loadObject(oname, ox, oy, oz, rx, ry, rz, bakeNode, name, true, -1, type);
	}
	//fclose(fd);

	// ds closes automatically, so do not close it explicitly here:
	//ds->close();

	//finish the last road
	if (r2oldmode!=0)
	{
		//fill object
		ProceduralPoint pp;
		pp.bheight = 0.2;
		pp.bwidth = 1.4;
		pp.pillartype = 1;
		pp.position = r2lastpos+r2lastrot*Vector3(10.0,0,0);
		pp.rotation = r2lastrot;
		pp.type = ROAD_AUTOMATIC;
		pp.width = 8;
		po.points.push_back(pp);

		// finish it and start new object
		if(proceduralManager)
			proceduralManager->addObject(po);
	}


	//okay now bake everything
	bakesg=mSceneMgr->createStaticGeometry("bakeSG");
	bakesg->addSceneNode(bakeNode);
	bakesg->setRegionDimensions(Vector3(farclip/2.0, 10000.0, farclip/2.0));
	bakesg->setRenderingDistance(farclip);
	try
	{
		bakesg->build();
		bakeNode->detachAllObjects();
		bakeNode->removeAndDestroyAllChildren();
	}catch(...)
	{
		LogManager::getSingleton().logMessage("error while baking roads. ignoring.");

	}

	// tell this the map, so it can change the drawing distance !
	if(mtc)
		mtc->setStaticGeometry(bakesg);
	collisions->printStats();
	loading_state=TERRAIN_LOADED;

	// we set the sky this late, so the user can configure it ...
	if (SETTINGS.getSetting("Sky effects")!="Caelum (best looking, slower)" && strlen(sandstormcubemap)>0)
	{
		mSceneMgr->setSkyBox(true, sandstormcubemap, farclip);
	}

#ifdef HYDRAX
	if(w && useHydrax)
		((HydraxWater*)w)->loadConfig(hydraxConfig);
#endif // HYDRAX

	if(debugCollisions)
		collisions->createCollisionDebugVisualization();

	UILOADER.setProgress(UI_PROGRESSBAR_HIDE);

	//okay, taking a picture of the scene for the envmap
	// SAY CHEESE!
	//no, not yet, caelum is not ready!
	//if (envmap) envmap->update(Vector3(terrainxsize/2.0, hfinder->getHeightAt(terrainxsize/2.0, terrainzsize/2.0)+50.0, terrainzsize/2.0));
}

void ExampleFrameListener::updateXFire()
{
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
    if(!xfire_enabled)
        // user mus explicitly enable it
        return;
	// ----
#ifdef XFIRE
	char terrainname[100]="";
	strncpy(terrainname, const_cast<char*>(loadedTerrain.c_str()), 99);

	// ----
	String gameState = "unkown";
	switch(loading_state)
	{
		case NONE_LOADED: gameState = "in menu"; break;
		case TERRAIN_LOADED: gameState = "selecting vehicle"; break;
		case ALL_LOADED: gameState = "playing"; break;
		case EXITING: gameState = "exiting the game"; break;
		case EDITING: gameState = "editing"; break;
		case RELOADING: gameState = "selecting additional vehicle"; break;
		case EDITOR_PAUSE: gameState = "pause mode"; break;
	}

	// ----
	String vehicleName = "unkown";
	if(loading_state == ALL_LOADED)
	{
		if(current_truck == -1)
			vehicleName = "person";
		else if(current_truck != -1 && current_truck <= free_truck)
				vehicleName = trucks[current_truck]->getTruckName();
	}

	// ----
	char playingTime[100]="";
	int minutes = (CACHE.getTimeStamp() - gameStartTime) / 60;
	int hours = minutes / 60;
	if (minutes > 60)
		sprintf(playingTime, "%d hour(s), %d minutes", hours, minutes-60*hours);
	else
		sprintf(playingTime, "%d minute(s)", minutes);

	// ----
	char gameType[100]="";
	if (!netmode)
		strcpy(gameType, "Single Player");
	else
		strcpy(gameType, "Multi Player");

	// ----
	static String servername = "unkown";
	static int serverport=0;
	if(netmode && servername == "unkown" && serverport == 0)
	{
		servername = SETTINGS.getSetting("Server name").c_str();
		serverport=StringConverter::parseLong(SETTINGS.getSetting("Server port"));
	}
	int players = 0;
	if(net) players = net->getConnectedClientCount();

	char serverString[100]="";
	if(netmode)
		sprintf(serverString, "%s:%d (%d players)", servername.c_str(), serverport, players);
	else
		strcpy(serverString, "Playing Offline");

	// ----
	String gameVersion = "RoR: "+String(ROR_VERSION_STRING) + " / protocol: " + String(RORNET_VERSION);

	// ----
	const char *key[] = {
		"Game State",
		"Terrain",
		"Used Vehicle",
		"Playing since",
		"Game Type",
		"Server",
		"Game Version",
	};
	const char *value[] = {
		const_cast<char*>(gameState.c_str()),
		(const char *)terrainname,
		const_cast<char*>(vehicleName.c_str()),
		(const char *)playingTime,
		(const char *)gameType,
		(const char *)serverString,
		const_cast<char*>(gameVersion.c_str()),
	};
	if(XfireIsLoaded() == 1)
	{
		int res = XfireSetCustomGameData(7, key, value);
		LogManager::getSingleton().logMessage("XFire GameData updated, result code: "+StringConverter::toString(res));
	}

#endif // XFIRE
#endif // WIN32
}

void ExampleFrameListener::setGrassDensity(float x, float y, int density, bool relative)
{
#ifdef PAGED
// todo to fix
#if 0
	if(!grassLoader)
		return;
	GrassLayer *grassLayer = grassLoader->getLayerList().front();
	PixelBox *b = grassLayer->getDensityMapData();
	if(!b || ! grassLayer)
		return;
	int gwidth = (int)b->getWidth();
	int gheight = (int)b->getHeight();
	int nx = (int)(x / mapsizex * gwidth);
	int ny = (int)(y / mapsizez * gwidth);
	unsigned char *data = ((unsigned char*)b->data) + nx + ny * gwidth;
	int value=0, valuechange=0;
	if(relative)
	{
		value = changeGrassBuffer(data, density);
		valuechange = density/2;
	}
	else
	{
		value = density;
		valuechange = (value - *data);
	}
	// surrounding value of the pixel (brush like)
	if(valuechange != 0)
		valuechange /= 2;
	// so set some data, like a brush, to make everything smoother :)
	/* pixels:
	1 2 3
	4 5 6
	7 8 9
	*/
	if(ny > 1 && nx > 1)
	{
		// set 1,2,3
		changeGrassBuffer(data-1-gwidth, valuechange);
		changeGrassBuffer(data-gwidth, valuechange);
		changeGrassBuffer(data+1-gwidth, valuechange);
		// set 4,5,6
		changeGrassBuffer(data-1, valuechange);
		changeGrassBuffer(data, valuechange);
		changeGrassBuffer(data+1, valuechange);
		// set 7,8,9
		changeGrassBuffer(data-1+gwidth, valuechange);
		changeGrassBuffer(data+gwidth, valuechange);
		changeGrassBuffer(data+1+gwidth, valuechange);
	}

	LogManager::getSingleton().logMessage("setting grass: "+ \
		StringConverter::toString(gwidth)+"x"+StringConverter::toString(gheight)+", "+ \
		"("+StringConverter::toString(nx)+"x"+StringConverter::toString(ny)+") = " + \
		StringConverter::toString(*data)+" / " + StringConverter::toString(value)+", "+StringConverter::toString(valuechange));
	grass->reloadGeometryPage(Vector3(x, 0, y));
#endif
#endif //PAGED
}

void ExampleFrameListener::updateGrass(Vector3 pos)
{
#if 0
// todo: fix
	if(!grassLoader || !grass)
		return;

	grass->reloadGeometryPage(pos);
#endif
}

int ExampleFrameListener::changeGrassBuffer(unsigned char *data, int relchange)
{
#ifdef PAGED
	int newdata = *data + relchange;
	if(newdata < 0)
		newdata = 0;
	if(newdata > 255)
		newdata = 255;
	*data = newdata;
	return newdata;
#else
	return 0;
#endif
}


void ExampleFrameListener::saveGrassDensity()
{
#ifdef PAGED
#if 0
	if(!grassLoader)
		return;
	GrassLayer *grassLayer = grassLoader->getLayerList().front();
	PixelBox *b = grassLayer->getDensityMapData();
	if(!b || !grassLayer)
		return;
	int gwidth = (int)b->getWidth();
	int gheight = (int)b->getHeight();
	grass->reloadGeometry();

	Image img;
	img = img.loadDynamicImage ((uchar *)b->data, gwidth, gheight, PF_BYTE_L);

	String filename = String("data/terrains/") + grassdensityTextureFilename;
	img.save(filename.c_str());
	LogManager::getSingleton().logMessage("saving grass to "+filename);
#endif
#endif //PAGED
}

void ExampleFrameListener::initTrucks(bool loadmanual, Ogre::String selected, Ogre::String selectedExtension, std::vector<Ogre::String> *truckconfig)
{
	//we load truck

	//we create dust
	dustp=0;
	dripp=0;
	splashp=0;
	ripplep=0;
	sparksp=0;
	clumpp=0;
	if (SETTINGS.getSetting("Dust")=="Yes")
	{
		dustp=new DustPool("tracks/Dust", 20, mSceneMgr->getRootSceneNode(), mSceneMgr, w);
		if (w) w->registerDust(dustp);
		clumpp=new DustPool("tracks/Clump", 20, mSceneMgr->getRootSceneNode(), mSceneMgr, w);
		if (w) w->registerDust(clumpp);
		sparksp=new DustPool("tracks/Sparks", 10, mSceneMgr->getRootSceneNode(), mSceneMgr, w);
		if (w) w->registerDust(sparksp);
	}
	if (SETTINGS.getSetting("Spray")=="Yes")
	{
		dripp=new DustPool("tracks/Drip", 50, mSceneMgr->getRootSceneNode(), mSceneMgr, w);
		if (w) w->registerDust(dripp);
		splashp=new DustPool("tracks/Splash", 20, mSceneMgr->getRootSceneNode(), mSceneMgr, w);
		if (w) w->registerDust(splashp);
		ripplep=new DustPool("tracks/Ripple", 20, mSceneMgr->getRootSceneNode(), mSceneMgr, w);
		if (w) w->registerDust(ripplep);
	}

	char *selectedchr = const_cast<char *>(selected.c_str());
	if (loadmanual)
	{
		if(net)
		{
			Vector3 spawnpos = Vector3(truckx, trucky, truckz);
			Quaternion spawnrot = Quaternion::ZERO;
			if(selectedExtension.size() > 0)
			{
				try
				{
					spawnpos = netSpawnPos[selectedExtension].pos;
					spawnrot = netSpawnPos[selectedExtension].rot;
				} catch(...)
				{
					spawnpos = Vector3(truckx, trucky, truckz);
					spawnrot = Quaternion::ZERO;
				}
			}
			trucks[free_truck]=new Beam(free_truck, mSceneMgr, mSceneMgr->getRootSceneNode(), mWindow, &mapsizex, &mapsizez, spawnpos.x, spawnpos.y, spawnpos.z, spawnrot, selectedchr, collisions, dustp, clumpp, sparksp, dripp, splashp, ripplep, hfinder, w, mCamera, mirror, false, false, netmode,0,false,flaresMode, truckconfig);
		} else
		{
			trucks[free_truck]=new Beam(free_truck, mSceneMgr, mSceneMgr->getRootSceneNode(), mWindow, &mapsizex, &mapsizez, truckx, trucky, truckz, Quaternion::ZERO, selectedchr, collisions, dustp, clumpp, sparksp, dripp, splashp, ripplep, hfinder, w, mCamera, mirror, false, false, netmode,0,false,flaresMode, truckconfig);
		}

		if(bigMap)
		{
			MapEntity *e = bigMap->createNamedMapEntity("Truck"+StringConverter::toString(free_truck), MapControl::getTypeByDriveable(trucks[free_truck]->driveable));
			e->setState(DESACTIVATED);
			e->setVisibility(true);
			e->setPosition(truckx, truckz);
			e->setRotation(-Radian(trucks[free_truck]->getHeadingDirectionAngle()));
		}

		if (trucks[free_truck]->engine) trucks[free_truck]->engine->start();
		if (netmode && net)
			net->sendVehicleType(selectedchr, trucks[free_truck]->netbuffersize);
		free_truck++;
	}
	//load the rest
	if (!netmode)
	{
		int i;
		for (i=0; i<truck_preload_num; i++)
		{
			trucks[free_truck]=new Beam(free_truck, mSceneMgr, mSceneMgr->getRootSceneNode(), mWindow,
				&mapsizex, &mapsizez, truck_preload[i].px, truck_preload[i].py, truck_preload[i].pz, truck_preload[i].rotation, truck_preload[i].name, collisions, dustp, clumpp, sparksp, dripp, splashp, ripplep, hfinder, w, mCamera, mirror,false,false,false,0,truck_preload[i].ismachine,flaresMode, truckconfig);
			if(bigMap)
			{
				MapEntity *e = bigMap->createNamedMapEntity("Truck"+StringConverter::toString(free_truck), MapControl::getTypeByDriveable(trucks[free_truck]->driveable));
				e->setState(DESACTIVATED);
				e->setVisibility(true);
				e->setPosition(truck_preload[free_truck].px, truck_preload[i].pz);
				e->setRotation(-Radian(trucks[free_truck]->getHeadingDirectionAngle()));
			}

			free_truck++;
			//trucks[free_truck-1]->setNetworkName(truck_preload[i].name);
		}
	}
	LogManager::getSingleton().logMessage("EFL: beam instanciated");

 	person=new Character(collisions, hfinder, w, bigMap, mSceneMgr);

	setCurrentTruck(-1);
	if (netmode)
	{
		setCurrentTruck(0);
	}
	//force perso start
	if (persostart!=Vector3(0,0,0)) person->setPosition(persostart);
	//bigMap->getEntityByName("person")->onTop();

	/*cameramode=CAMERA_INT;
	pushcamRotX=camRotX;
	pushcamRotY=camRotY;
	camRotX=0;
	camRotY=DEFAULT_INTERNAL_CAM_PITCH;
	*/
	loading_state=ALL_LOADED;
	//uiloader->hide();
	showcredits=0;
	//					showDashboardOverlays(true);
	LogManager::getSingleton().logMessage("EFL: overlays ok");
	//showDebugOverlay(true);

	if(mtc)
		mtc->update();
}

void ExampleFrameListener::setCurrentTruck(int v)
{
	if (cameramode==CAMERA_FREE) return;
	if (v==current_truck) return;
	int previous_truck=current_truck;
	if ( current_truck!=-1 && current_truck<free_truck) trucks[current_truck]->desactivate();
	current_truck=v;

	if (current_truck==-1)
	{
		if(bigMap) bigMap->setVisibility(false);

		// hide truckhud
		TRUCKHUD.show(false);

		// return to normal mapmode
		if(mtc && interactivemap > 1)
		{
			// 2 = disabled normally, enabled for car
			interactivemap = 0;
			bigMap->setEntityVisibility(true);
		}

		//getting outside
		mouseOverlay->hide();
		trucks[previous_truck]->prepareInside(false);

		Vector3 position;
		// this workaround enables trucks to spawn that have no cinecam. required for cmdline options
		if(trucks[previous_truck]->cinecameranodepos[0] != -1)
		{
			// truck has a cinecam
			position=trucks[previous_truck]->nodes[trucks[previous_truck]->cinecameranodepos[0]].AbsPosition;
			position+=-2.0*((trucks[previous_truck]->nodes[trucks[previous_truck]->cameranodepos[0]].RelPosition-trucks[previous_truck]->nodes[trucks[previous_truck]->cameranoderoll[0]].RelPosition).normalisedCopy());
			position+=Vector3(0.0, -1.0, 0.0);
		}
		else
		{
			// truck has no cinecam
			position=trucks[previous_truck]->nodes[0].AbsPosition;
		}
		//			position.y=hfinder->getHeightAt(position.x,position.z);
		person->setPosition(position);
		person->setVisible(true);
		showDashboardOverlays(false,0);
		showEditorOverlay(false);
		ssm->trigStop(previous_truck, SS_TRIG_AIR);
		ssm->trigStop(previous_truck, SS_TRIG_PUMP);
		int t;
		for (t=0; t<free_truck; t++) {trucks[t]->sleepcount=9;} //make trucks synchronous
		//lastangle=0;
		camRotX=0;
		camRotY=Degree(12);
		camDist=20;
	}
	else
	{
		//getting inside
		mouseOverlay->show();
		person->setVisible(false);
		if(!hidegui)
		{
			showDashboardOverlays(true, trucks[current_truck]->driveable);
			showEditorOverlay(trucks[current_truck]->editorId>=0);
		}

		// mapmode change?
		if(mtc && trucks[current_truck]->dynamicMapMode > 0)
		{
			// > 1 = disabled normally, enabled for car
			if(interactivemap == 0)
				interactivemap = 1 + trucks[current_truck]->dynamicMapMode;
			else if(interactivemap == 1 && trucks[current_truck]->dynamicMapMode == 2)
				interactivemap = 3;
			mtc->setCamZoom(30);
			bigMap->setEntityVisibility(false);
		}

		// show minimap and put it into lower left corner
		if(bigMap) 
		{
			bigMap->setVisibility(true);
			bigMap->setPosition(0, 0.81, 0.14, 0.19, mWindow);
		}

		trucks[current_truck]->activate();
		//if (trucks[current_truck]->engine->running) trucks[current_truck]->audio->playStart();
		//hide unused items
		if (trucks[current_truck]->free_active_shock==0) (OverlayManager::getSingleton().getOverlayElement("tracks/rollcorneedle"))->hide();
		//					rollcorr_node->setVisible((trucks[current_truck]->free_active_shock>0));
		//help panel

		try
		{
			// we wont crash for help panels ...
			if (trucks[current_truck]->hashelp)
			{
				OverlayManager::getSingleton().getOverlayElement("tracks/helppanel")->setMaterialName(trucks[current_truck]->helpmat);
				OverlayManager::getSingleton().getOverlayElement("tracks/machinehelppanel")->setMaterialName(trucks[current_truck]->helpmat);
			}
			else
			{
				OverlayManager::getSingleton().getOverlayElement("tracks/helppanel")->setMaterialName("tracks/black");
				OverlayManager::getSingleton().getOverlayElement("tracks/machinehelppanel")->setMaterialName("tracks/black");
			}
		} catch(...)
		{
		}
		// enable gui mods
		if (trucks[current_truck]->speedomat != String(""))
			OverlayManager::getSingleton().getOverlayElement("tracks/speedo")->setMaterialName(trucks[current_truck]->speedomat);
		else
			OverlayManager::getSingleton().getOverlayElement("tracks/speedo")->setMaterialName("tracks/Speedo");

		if (trucks[current_truck]->tachomat != String(""))
			OverlayManager::getSingleton().getOverlayElement("tracks/tacho")->setMaterialName(trucks[current_truck]->tachomat);
		else
			OverlayManager::getSingleton().getOverlayElement("tracks/tacho")->setMaterialName("tracks/Tacho");

		//lastangle=0;
		camRotX=0;
		camRotY=Degree(12);
		camDist=20;
		if (cameramode==CAMERA_INT)
		{
			trucks[current_truck]->prepareInside(true);
			showDashboardOverlays(false, 0);
			camRotY=DEFAULT_INTERNAL_CAM_PITCH;
			if(bigMap) bigMap->setVisibility(false);
		}
	}
#ifdef XFIRE
	updateXFire();
#endif
}

#if 0
bool ExampleFrameListener::processUnbufferedMouseInput(const FrameEvent& evt)
{
	/* Rotation factors, may not be used if the second mouse button is pressed. */

	/* If the second mouse button is pressed, then the mouse movement results in
	sliding the camera, otherwise we rotate. */
	/*        if( mInputDevice->getMouseButton( 1 ) )
	{
	mTranslateVector.x += mInputDevice->getMouseRelativeX() * 0.13;
	mTranslateVector.y -= mInputDevice->getMouseRelativeY() * 0.13;
	}
	else
	{
	mRotX = Degree(-mInputDevice->getMouseRelativeX() * 0.13);
	mRotY = Degree(-mInputDevice->getMouseRelativeY() * 0.13);
	}
	*/

	//if (!mWindow->isAutoUpdated()) mWindow->update();
	return true;
}
#endif

void ExampleFrameListener::moveCamera(float dt)
{
	if(!hfinder)
		return;
	if (loading_state!=ALL_LOADED && loading_state != EDITOR_PAUSE) return;

	if (isnodegrabbed) return; //freeze camera

	if(useIngameEditor) return; // no movement in editor mode

	if (cameramode==CAMERA_FREE)
	{
		// this is a workaround for the free camera mode :)
		Real mMoveScale = 0.1;
		Ogre::Degree mRotScale(0.1f);
		Ogre::Degree mRotX(0);
		Ogre::Degree mRotY(0);
		Vector3 mTranslateVector = Vector3::ZERO;

		if(INPUTENGINE.isKeyDown(OIS::KC_LSHIFT) || INPUTENGINE.isKeyDown(OIS::KC_RSHIFT))
		{
			mRotScale *= 3;
			mMoveScale *= 3;
		}

		if(INPUTENGINE.isKeyDown(OIS::KC_LCONTROL))
		{
			mRotScale *= 30;
			mMoveScale *= 30;
		}

		if(INPUTENGINE.isKeyDown(OIS::KC_LMENU))
		{
			mRotScale *= 0.05;
			mMoveScale *= 0.05;
		}

		const OIS::MouseState &ms = INPUTENGINE.getMouseState();
		if( ms.buttonDown(OIS::MB_Right) )
		{
			mTranslateVector.x += ms.X.rel * 0.13;
			mTranslateVector.y -= ms.Y.rel * 0.13;
		}
		else
		{
			mRotX = Degree(-ms.X.rel * 0.13);
			mRotY = Degree(-ms.Y.rel * 0.13);
		}

		if(INPUTENGINE.getEventBoolValue("CHARACTER_SIDESTEP_LEFT"))
			mTranslateVector.x = -mMoveScale;	// Move camera left

		if(INPUTENGINE.getEventBoolValue("CHARACTER_SIDESTEP_RIGHT"))
			mTranslateVector.x = mMoveScale;	// Move camera RIGHT

		if(INPUTENGINE.getEventBoolValue("CHARACTER_FORWARD"))
			mTranslateVector.z = -mMoveScale;	// Move camera forward

		if(INPUTENGINE.getEventBoolValue("CHARACTER_BACKWARDS"))
			mTranslateVector.z = mMoveScale;	// Move camera backward

		if(INPUTENGINE.getEventBoolValue("CHARACTER_ROT_UP"))
			mRotY += mRotScale;

		if(INPUTENGINE.getEventBoolValue("CHARACTER_ROT_DOWN"))
			mRotY += -mRotScale;

		if(INPUTENGINE.getEventBoolValue("CHARACTER_UP"))
			mTranslateVector.y = mMoveScale;	// Move camera up

		if(INPUTENGINE.getEventBoolValue("CHARACTER_DOWN"))
			mTranslateVector.y = -mMoveScale;	// Move camera down

		if(INPUTENGINE.getEventBoolValue("CHARACTER_RIGHT"))
			mRotX += -mRotScale;

		if(INPUTENGINE.getEventBoolValue("CHARACTER_LEFT"))
			mRotX += mRotScale;

		mCamera->yaw(mRotX);
		mCamera->pitch(mRotY);
		mCamera->moveRelative(mTranslateVector);
	}
	if (current_truck==-1)
	{
		if(mtc && interactivemap)
		{
			mtc->setCamPosition(person->getPosition(), mCamera->getOrientation());
			mtc->update();
		}
		//perso mode
		if (cameramode==CAMERA_EXT)
		{
			if (collisions->forcecam)
			{
				mCamera->setPosition(collisions->forcecampos);
				mCamera->lookAt(person->getPosition()+Vector3(0.0,1.0,0.0));
				mCamera->setFOVy(Degree(80));
				collisions->forcecam=false;
			}
			else
			{
				// Make all the changes to the camera
				//Vector3 delta=lastPosition-personode->getPosition();
				//delta.y=0.0;
				float angle=-person->getAngle()-(3.14159/2.0);
				//			float angle2;
				//if (delta.length()>0.01) angle=atan2(delta.x,delta.z); else angle=lastangle;
				camIdealPosition=camDist/2.0*Vector3(sin(angle+camRotX.valueRadians())*cos(camRotY.valueRadians()),sin(camRotY.valueRadians()),cos(angle+camRotX.valueRadians())*cos(camRotY.valueRadians()));


				camIdealPosition=camIdealPosition+person->getPosition();
				Vector3 newposition=(camIdealPosition+10.0*mCamera->getPosition())/11.0;
				Real h=hfinder->getHeightAt(newposition.x,newposition.z);

				if (w && !w->allowUnderWater() && w->getHeightWaves(newposition) > h)
					h=w->getHeightWaves(newposition);

				h+=1.0;
				if (newposition.y<h) newposition.y=h;
				mCamera->setPosition(newposition);
				mCamera->lookAt(person->getPosition()+Vector3(0.0,1.0,0.0));
				mCamera->setFOVy(Degree(60));

				lastPosition=person->getPosition();
				//lastangle=angle;
			}
		}
		else if (cameramode==CAMERA_FIX)
		{
			float px, pz;
			px=((int)(person->getPosition().x)/100)*100;
			pz=((int)(person->getPosition().z)/100)*100;
			Real h=hfinder->getHeightAt(px+50.0,pz+50.0);
			Real random = Ogre::Math::RangeRandom(0.0f, 1.0f);
			if(w && random > 0.3f && !w->allowUnderWater())
			{
				// chance of 30% to get an underwater view?
				if (w && w->getHeightWaves(Vector3(px+50.0,0,pz+50.0)) > h)
					h=w->getHeightWaves(Vector3(px+50.0,0,pz+50.0));
			} else if (w && w->allowUnderWater())
			{
				h = w->getHeightWaves(Vector3(px+50.0,0,pz+50.0) - 10.0f);
			}
			mCamera->setPosition(Vector3(px+50.0, h+1.7, pz+50.0));
			mCamera->lookAt(person->getPosition());
			mCamera->setFOVy(Radian(atan2(20.0f,(mCamera->getPosition()-person->getPosition()).length())));
		}
		else if (cameramode==CAMERA_INT)
		{
			mCamera->setFOVy(Degree(75));
			mCamera->setPosition(person->getPosition()+Vector3(0,1.7,0));
			Vector3 dir=Vector3(cos(person->getAngle()), 0.0, sin(person->getAngle()));
			mCamera->lookAt(mCamera->getPosition()+dir);
			mCamera->yaw(-camRotX);
			mCamera->pitch(camRotY);
			if (w && lastPosition.y<w->getHeightWaves(lastPosition))
			{
				cameramode=CAMERA_EXT;
				camRotX=pushcamRotX;
				camRotY=pushcamRotY;
			}
		}
	}
	else
	{
		if(mtc && interactivemap)
		{
			// update the interactive map
			// to improve: make the map, so it "looks forward", means the truck is at the bottom
			// to improve: use average vehicle speed, not the current one
			if(interactivemap == 3) // auto - zoom
			{
				mtc->setCamZoom(30 + trucks[current_truck]->WheelSpeed * 0.5);
			}
			mtc->setCamPosition(trucks[current_truck]->getPosition(), mCamera->getOrientation());
			mtc->update();
		}
		if (cameramode==CAMERA_EXT)
		{
			if (collisions->forcecam)
			{
				mCamera->setPosition(collisions->forcecampos);
				mCamera->lookAt(trucks[current_truck]->getPosition());
				mCamera->setFOVy(Degree(80));
				collisions->forcecam=false;
			}
			else
			{
				// Make all the changes to the camera
				//Vector3 delta=lastPosition-trucks[current_truck]->position;
				//delta.y=0.0;
				float angle;
				//			float angle2;
				//				if (delta.length()>0.05) angle=atan2(delta.x,delta.z); else angle=lastangle;
				Vector3 dir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].smoothpos-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[0]].smoothpos;
				dir.normalise();
				angle=-atan2(dir.dotProduct(Vector3::UNIT_X), dir.dotProduct(-Vector3::UNIT_Z));
				if(externalCameraMode==0)
				{
					float pitch=-asin(dir.dotProduct(Vector3::UNIT_Y));
					camIdealPosition=camDist*Vector3(sin(angle+camRotX.valueRadians())*cos(pitch+camRotY.valueRadians()),sin(pitch+camRotY.valueRadians()),cos(angle+camRotX.valueRadians())*cos(pitch+camRotY.valueRadians()));
				} else if(externalCameraMode==1)
				{
					camIdealPosition=camDist*Vector3(sin(angle+camRotX.valueRadians())*cos(camRotY.valueRadians()),sin(camRotY.valueRadians()),cos(angle+camRotX.valueRadians())*cos(camRotY.valueRadians()));
				}

				camIdealPosition=camIdealPosition+trucks[current_truck]->getPosition();
				Vector3 oldposition=mCamera->getPosition()+trucks[current_truck]->nodes[0].Velocity*trucks[current_truck]->ttdt;
				float ratio=1.0/(trucks[current_truck]->tdt*4.0);
				//float ratio=0.001;
				//Vector3 newposition=(camIdealPosition+ratio*mCamera->getPosition())/(ratio+1.0);
				//Vector3 newposition=camIdealPosition;
				Vector3 newposition=(1/(ratio+1.0))*camIdealPosition+(ratio/(ratio+1.0))*oldposition;

				Real h=hfinder->getHeightAt(newposition.x,newposition.z);
				if (w && !w->allowUnderWater() && w->getHeightWaves(newposition)>h)
					h=w->getHeightWaves(newposition);
				h+=1.0;
				if (newposition.y<h) newposition.y=h;
				mCamera->setPosition(newposition);
				mCamera->lookAt(trucks[current_truck]->getPosition());
				mCamera->setFOVy(Degree(60));

				lastPosition=trucks[current_truck]->getPosition();
				//lastangle=angle;
			}
		}
		if (cameramode==CAMERA_FIX)
		{
			if (trucks[current_truck]->driveable==AIRPLANE)
			{
				if ((mCamera->getPosition()-trucks[current_truck]->getPosition()).length()>500.0)
				{
					Vector3 newposition=trucks[current_truck]->getPosition();
					Vector3 dir=trucks[current_truck]->nodes[0].Velocity;
					dir.normalise();
					newposition=newposition+dir*450.0+Vector3(5.0, 0.0, 5.0);
					Real h=hfinder->getHeightAt(newposition.x,newposition.z);
					Real random = Ogre::Math::RangeRandom(0.0f, 1.0f);
					if(w && random > 0.3f && !w->allowUnderWater())
					{
						// chance of 30% to get an underwater view?
						if (w && w->getHeightWaves(newposition)>h)
							h=w->getHeightWaves(newposition);
					} else if (w && w->allowUnderWater())
					{
						h=w->getHeightWaves(newposition) - 10.0f;
					}

					if (newposition.y<h+2.0) newposition.y=h+2.0;
					mCamera->setPosition(newposition);
				}
				mCamera->lookAt(trucks[current_truck]->getPosition());
				mCamera->setFOVy(Radian(atan2(100.0f,(mCamera->getPosition()-trucks[current_truck]->getPosition()).length())));
			}
			else
			{
				float px, pz;
				px=((int)(trucks[current_truck]->getPosition().x)/100)*100;
				pz=((int)(trucks[current_truck]->getPosition().z)/100)*100;
				Real h=hfinder->getHeightAt(px+50.0,pz+50.0);
				if (w && !w->allowUnderWater() && w->getHeightWaves(Vector3(px+50.0,0,pz+50.0))>h)
					h=w->getHeightWaves(Vector3(px+50.0,0,pz+50.0));
				mCamera->setPosition(Vector3(px+50.0, h+1.7, pz+50.0));
				mCamera->lookAt(trucks[current_truck]->getPosition());
				mCamera->setFOVy(Radian(atan2(20.0f,(mCamera->getPosition()-trucks[current_truck]->getPosition()).length())));
			}
		}
		if (cameramode==CAMERA_INT)
		{
			int currentcamera=trucks[current_truck]->currentcamera;
			mCamera->setFOVy(Degree(90));
			if (trucks[current_truck]->cinecameranodepos>=0) lastPosition=trucks[current_truck]->nodes[trucks[current_truck]->cinecameranodepos[currentcamera]].smoothpos;
			else lastPosition=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[currentcamera]].smoothpos;
			mCamera->setPosition(lastPosition);
			if(trucks[current_truck]->cablightNode)
				trucks[current_truck]->cablightNode->setPosition(lastPosition);
			//old direction code
			/*
			Vector3 dir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[currentcamera]].smoothpos-trucks[current_truck]->nodes[trucks[current_truck]->cameranoderoll[currentcamera]].smoothpos;
			dir.normalise();
			mCamera->lookAt(lastPosition+trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[currentcamera]].smoothpos-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[currentcamera]].smoothpos);
			mCamera->roll(Radian(asin(dir.dotProduct(Vector3::UNIT_Y))));
			mCamera->yaw(-camRotX);
			mCamera->pitch(camRotY);
			*/
			Vector3 dir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[currentcamera]].smoothpos-trucks[current_truck]->nodes[trucks[current_truck]->cameranodedir[currentcamera]].smoothpos;
			dir.normalise();
			Vector3 side=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[currentcamera]].smoothpos-trucks[current_truck]->nodes[trucks[current_truck]->cameranoderoll[currentcamera]].smoothpos;
			side.normalise();
			if (trucks[current_truck]->revroll[currentcamera]) side=-side; //to fix broken vehicles
			Vector3 up=dir.crossProduct(side);
			//we recompute the side vector to be sure we make an orthonormal system
			side=up.crossProduct(dir);
			Quaternion cdir=Quaternion(camRotX, up)*Quaternion(Degree(180.0)+camRotY, side)*Quaternion(side, up, dir);
			mCamera->setOrientation(cdir);

			if (w && lastPosition.y<w->getHeightWaves(lastPosition))
			{
				cameramode=CAMERA_EXT;
				camRotX=pushcamRotX;
				camRotY=pushcamRotY;
				trucks[current_truck]->prepareInside(false);
				showDashboardOverlays(true, trucks[current_truck]->driveable);
			}
		}
	}

	//set LOD per truck
	int i;
	for (i=0; i<free_truck; i++)
	{
		trucks[i]->setDetailLevel((mCamera->getPosition()-trucks[i]->getPosition()).length()>trucks[i]->fadeDist);
	}
	//envmap
	if (envmap)
	{
		if (!envmap->inited) envmap->forceUpdate(Vector3(terrainxsize/2.0, hfinder->getHeightAt(terrainxsize/2.0, terrainzsize/2.0)+50.0, terrainzsize/2.0));
		if (current_truck!=-1) envmap->update(trucks[current_truck]->getPosition(), trucks[current_truck]);
	}

	//position audio listener
	Vector3 cpos=mCamera->getPosition();
	Vector3 cdir=mCamera->getDirection();
	Vector3 cup=mCamera->getUp();
	Vector3 cspeed=(cpos-cdoppler)/dt;
	cdoppler=cpos;
	// XXX maybe thats the source of sound destorsion: runtime order???
	ssm->setCamera(cpos, cdir, cup, cspeed);
	//if (cspeed.length()>50.0) {cspeed.normalise(); cspeed=50.0*cspeed;};
	//if (audioManager) audioManager->setListenerPosition(cpos.x, cpos.y, cpos.z, cspeed.x, cspeed.y, cspeed.z, cdir.x, cdir.y, cdir.z, cup.x, cup.y, cup.z);
	//water
	if (w)
	{
		if (current_truck!=-1)
			w->moveTo(mCamera, w->getHeightWaves(trucks[current_truck]->getPosition()));
		else
			w->moveTo(mCamera, w->getHeight());
	}
}


void ExampleFrameListener::showDebugOverlay(bool show, bool showmat)
{
	if (mDebugOverlay)
	{
		if (show)
		{
			mDebugOverlay->show();
			if(showmat)
				mTimingDebugOverlay->show();
			else
				mTimingDebugOverlay->hide();
		}
		else
		{
			mDebugOverlay->hide();
			mTimingDebugOverlay->hide();
		}
	}
}

#ifdef HAS_EDITOR
void ExampleFrameListener::showTruckEditorOverlay(bool show)
{
	if (truckeditorOverlay && mouseOverlay)
	{
		if (show)
		{
			if (audioManager) audioManager->soundEnable(false);
			truckeditorOverlay->show();
			mouseOverlay->show();
			if (dashboard) dashboard->setEnable(false);
			disableRendering=true;
			mWindow->setAutoUpdated(false);
		}
		else
		{
			truckeditorOverlay->hide();
			//mouseOverlay->hide();
			if (dashboard) dashboard->setEnable(true);
			disableRendering=false;
			mWindow->setAutoUpdated(true);
			if (audioManager) audioManager->soundEnable(true);
		}
	}
}
#endif

/*
void ExampleFrameListener::showBigMapOverlay(bool show)
{
	if (bigMapOverlay)
	{
		if (show)
		{
			bigMapOverlay->show();
			if(netmode)
				playerListOverlay->show();
		}
		else
		{
			bigMapOverlay->hide();
			if(netmode)
				playerListOverlay->hide();
		}
	}
}
*/

void ExampleFrameListener::showPressureOverlay(bool show)
{
	if (pressureOverlay)
	{
		if (show)
		{
			pressureOverlay->show();
			pressureNeedleOverlay->show();
		}
		else
		{
			pressureOverlay->hide();
			pressureNeedleOverlay->hide();
		}
	}
}

void ExampleFrameListener::showEditorOverlay(bool show)
{
	if (editorOverlay)
	{
		if (show)
		{
			editorOverlay->show();
		}
		else
		{
			editorOverlay->hide();
		}
	}
}

void ExampleFrameListener::showDashboardOverlays(bool show, int mode)
{
	if (needlesOverlay && dashboardOverlay)
	{
		if (show)
		{
			if (mode==TRUCK)
			{
				needlesMaskOverlay->show();
				needlesOverlay->show();
				dashboardOverlay->show();
			};
			if (mode==AIRPLANE)
			{
				airneedlesOverlay->show();
				airdashboardOverlay->show();
				//mouseOverlay->show();
			};
			if (mode==BOAT)
			{
				boatneedlesOverlay->show();
				boatdashboardOverlay->show();
			};
			if (mode==BOAT)
			{
				boatneedlesOverlay->show();
				boatdashboardOverlay->show();
			};
			if (mode==MACHINE)
			{
				machinedashboardOverlay->show();
			};
		}
		else
		{
			machinedashboardOverlay->hide();
			needlesMaskOverlay->hide();
			needlesOverlay->hide();
			dashboardOverlay->hide();
			//for the airplane
			airneedlesOverlay->hide();
			airdashboardOverlay->hide();
			//mouseOverlay->hide();
			boatneedlesOverlay->hide();
			boatdashboardOverlay->hide();
		}
	}
}

#define isnan(x) (x!=x)


bool ExampleFrameListener::updateAnimatedObjects(float dt)
{
	if(animatedObjects.size() == 0)
		return true;
	std::vector<animated_object_t>::iterator it;
	for(it=animatedObjects.begin(); it!=animatedObjects.end(); it++)
	{
		if(it->anim && it->speedfactor != 0)
		{
			Real time = dt * it->speedfactor;
			it->anim->addTime(time);
		}
	}
	return true;
}


bool ExampleFrameListener::updateTruckMirrors(float dt)
{
	//mirror
	if (mirror && current_truck!=-1)
	{
		//searching the best mirror=the nearest in the fov
		int i,j;
		float minlen=10000.0;
		SceneNode *mirrornode=0;
		int mirrortype=0;
		//simplify
		//for (i=0; i<free_truck; i++)
		i=current_truck;
		{
			for (j=0; j<trucks[i]->free_prop; j++)
			{
				if (trucks[i]->props[j].mirror)
				{
					Vector3 dist=trucks[i]->props[j].snode->getPosition()-mCamera->getPosition();
					//is it in the fov?
					//						if (dist.directionEquals(mCamera->getDirection(), mCamera->getFOVy()*mCamera->getAspectRatio()/2.0))
					//(enlarged FOV)
					if (dist.directionEquals(mCamera->getDirection(), mCamera->getFOVy()*mCamera->getAspectRatio()/1.2)) // 1.6 -> 1.2
					{
						//yes maam
						float fdist=dist.length();
						if (fdist<minlen)
						{
							minlen=fdist;
							mirrornode=trucks[i]->props[j].snode;
							mirrortype=trucks[i]->props[j].mirror;
						};
					}
				}
			}

		}
		if (mirrornode)
		{
			/*
			static SceneNode *sn=0;
			static Entity *mda=0;
			if (sn == 0)
			{
				mda = mSceneMgr->createEntity("MirrorAxesDebug", "axes.mesh");
				sn = mSceneMgr->getRootSceneNode()->createChildSceneNode();
				sn->attachObject(mda);
				sn->setScale(0.1, 0.1, 0.1);
			}
			*/

			//woohee, we have found it
			//mirrornode->showBoundingBox(!mirrornode->getShowBoundingBox());
			//				mirror->update(Plane(mirrornode->getOrientation()*Vector3(0.947,0.316,0), mirrornode->getPosition()+mirrornode->getOrientation()*Vector3(0.07,-0.22,0)));
			//				mirror->update(Plane(mirrornode->getOrientation()*Vector3(1,0,0), mirrornode->getPosition()));
			//				mirror->update(Plane(Vector3(1,0,0), mirrornode->getPosition()));
			//				mirror->update(mirrornode->getOrientation()*Vector3(1,0,0), mirrornode->getPosition());

			//				if (mirrortype==1) mirror->update(mirrornode->getOrientation()*Vector3(0.866,0.5,0), mirrornode->getPosition()+mirrornode->getOrientation()*Vector3(0.07,-0.22,0));
			//				if (mirrortype==-1) mirror->update(mirrornode->getOrientation()*Vector3(0.866,-0.5,0), mirrornode->getPosition()+mirrornode->getOrientation()*Vector3(0.07,0.22,0));
			Vector3 updateNormal = Vector3::ZERO, updateCenter = Vector3::ZERO;

			// calculate truck roll
			Vector3 dir=trucks[current_truck]->nodes[trucks[current_truck]->cameranodepos[0]].RelPosition-trucks[current_truck]->nodes[trucks[current_truck]->cameranoderoll[0]].RelPosition;
			dir.normalise();
			Radian rollangle = Degree(360) - Radian(asin(dir.dotProduct(Vector3::UNIT_Y)));

			// two mirror types: left and right
			if (mirrortype==1)
			{
				updateNormal = mirrornode->getOrientation() * Vector3(cos(trucks[current_truck]->leftMirrorAngle),sin(trucks[current_truck]->leftMirrorAngle),0);
				updateCenter = mirrornode->getPosition()+mirrornode->getOrientation()*Vector3(0.07,-0.22,0);
			}
			if (mirrortype==-1)
			{
				updateNormal = mirrornode->getOrientation()*Vector3(cos(trucks[current_truck]->rightMirrorAngle),sin(trucks[current_truck]->rightMirrorAngle),0);
				updateCenter = mirrornode->getPosition()+mirrornode->getOrientation()*Vector3(0.07,0.22,0);
			}
			if(updateNormal != Vector3::ZERO && updateCenter != Vector3::ZERO)
			{
				//sn->setPosition(updateCenter);
				//sn->setDirection(updateNormal,Node::TS_WORLD);
				mirror->update(updateNormal, updateCenter, rollangle);
			}

		}
	}
	return true;
}

// Override frameStarted event to process that (don't care about frameEnded)
bool ExampleFrameListener::frameStarted(const FrameEvent& evt)
{
	float dt=evt.timeSinceLastFrame;
	if (dt==0) return true;
	if (dt>1.0/20.0) dt=1.0/20.0;
	rtime+=dt; //real time
	if(mWindow->isClosed())
		return false;

	// update GUI
	INPUTENGINE.Capture();
	MYGUI.frameStarted(evt);

	//if(collisions) 	printf("> ground model used: %s\n", collisions->last_used_ground_model->name);

	// exit frame started method when just displaying the GUI
	if (UILOADER.getFrameForced())
		return true;

	if(showcredits && creditsviewtime > 0)
		creditsviewtime-= dt;
	if(showcredits && creditsviewtime < 0 && !shutdownall)
	{
		shutdown_final();
		return true;
	}
	else if(showcredits && creditsviewtime < 0 && shutdownall)
		return false;

	if(shutdownall) // shortcut: press ESC in credits
		return false;

	// update animated objects
	updateAnimatedObjects(dt);

	// updating mirrors fixes its shaking!
	if (cameramode==CAMERA_INT)
		updateTruckMirrors(dt);

	moveCamera(dt); //antishaking

	// update water after the camera!
	if(loading_state==ALL_LOADED && w) w->framestep(dt);

	//update visual - antishaking
	int t;
	if (loading_state==ALL_LOADED)
	{
		if (current_truck!=-1 && isnan(trucks[current_truck]->getPosition().x))
		{
			//physics is contaminated with QNaNs!
			trucks[current_truck]->reset();
			setCurrentTruck(-1);
		}
		for (t=0; t<free_truck; t++)
		{
			if (trucks[t]->state!=SLEEPING && trucks[t]->loading_finished) trucks[t]->updateVisual(dt);
			//trucks[t]->updateFlares();
		}
	}

	if(!updateEvents(dt))
	{
		LogManager::getSingleton().logMessage("exiting...");
		return false;
	}

	// update gui 3d arrow
	if(dirvisible && loading_state==ALL_LOADED)
	{
		dirArrowNode->lookAt(dirArrowPointed, Node::TS_WORLD,Vector3::UNIT_Y);
		char tmp[255];
		Real distance = 0;
		if(current_truck != -1 && trucks[current_truck]->state == ACTIVATED)
			distance = trucks[current_truck]->getPosition().distance(dirArrowPointed);
		else
			distance = person->getPosition().distance(dirArrowPointed);
		sprintf(tmp,"%0.1f meter", distance);
		directionArrowDistance->setCaption(tmp);
	}

	if(useIngameEditor)
		INGAMEEDITOR.frameStep(evt);


	// one of the input modes is immediate, so setup what is needed for immediate mouse/key movement
	if (mTimeUntilNextToggle >= 0)
		mTimeUntilNextToggle -= evt.timeSinceLastFrame;

	// If this is the first frame, pick a speed
	if (evt.timeSinceLastFrame == 0)
	{
		mMoveScale = 1;
		mRotScale = 0.1;
	}
	// Otherwise scale movement units by time passed since last frame
	else
	{
		// Move about 100 units per second,
		mMoveScale = mMoveSpeed * evt.timeSinceLastFrame;
		// Take about 10 seconds for full rotation
		mRotScale = mRotateSpeed * evt.timeSinceLastFrame;
	}

	// one of the input modes is immediate, so update the movement vector
	if (loading_state==ALL_LOADED)
	{
#ifdef PAGED
		// paged geometry
		for(std::vector<paged_geometry_t>::iterator it=pagedGeometry.begin();it!=pagedGeometry.end();it++)
		{
			if(it->geom) it->geom->update();
		}
#endif //PAGED

		//airplane chatter
		//chatter disabled for the moment
		//if (current_truck!=-1 && trucks[current_truck]->driveable==AIRPLANE && trucks[current_truck]->audio) trucks[current_truck]->audio->playChatter(dt);

		//the dirt
		//if (dirt) dirt->update(dt);
		if (current_truck!=-1)
		{
			trucks[current_truck]->disableDrag=false;
			recursiveActivation(current_truck);
			//if its grabbed, its moving
			if (isnodegrabbed && trucks[truckgrabbed]->state==SLEEPING) trucks[truckgrabbed]->desactivate();
			//put to sleep
			for (int t=0; t<free_truck; t++)
			{
				if (trucks[t]->state==MAYSLEEP)
				{
					bool sleepyList[MAX_TRUCKS];
					for (int i=0; i<MAX_TRUCKS; i++) sleepyList[i]=false;
					if (!checkForActive(t, sleepyList))
					{
						//no active truck in the set, put everybody to sleep
						for (int i=0; i<free_truck; i++) if (sleepyList[i]) trucks[i]->state=GOSLEEP;
					}
				}
			}
		}

		//special stuff for rollable gear
		int t;
		bool rollmode=false;
		for (t=0; t<free_truck; t++)
			trucks[t]->updateFlares(dt, (t==current_truck) );
		for (t=0; t<free_truck; t++) if (trucks[t]->state!=SLEEPING) rollmode=rollmode || trucks[t]->wheel_contact_requested;
		for (t=0; t<free_truck; t++) trucks[t]->requires_wheel_contact=rollmode;// && !trucks[t]->wheel_contact_requested;
		//we simulate one truck, it will take care of the others (except networked ones)
		//this is the big "shaker"
		if (current_truck!=-1) trucks[current_truck]->frameStep(evt.timeSinceLastFrame, trucks, free_truck);
		//things always on
		for (t=0; t<free_truck; t++)
		{
			//networked trucks must be taken care of
			if (trucks[t]->state==NETWORKED) trucks[t]->calcNetwork();
			//the flares are always on
			//trucks[t]->updateFlares(dt);

			//let the engines run
			if (t!=current_truck && trucks[t]->state!=NETWORKED && trucks[t]->state!=RECYCLE)
			{
				if (trucks[t]->engine) trucks[t]->engine->update(dt, 1);
			}
		}

	}


	if (loading_state==ALL_LOADED)
	{
#ifdef LUASCRIPT
		lua->framestep();
#endif
		updateGUI(dt);
		if (raceStartTime > 0)
			updateRacingGUI();
		if (netmode && net)
		{
			//send player's truck data
			net->sendData(trucks[0]);
			//also take care of this
			trucks[0]->expireNetForce();
			//check for chat
			char name[256];
			//check for new truck to spawn
			unsigned int uid;
			unsigned int label;
			if (net->vehicle_to_spawn(name, &uid, &label))
			{
				//check first if we can recycle a truck
				bool recy=false;
				for (int t=0; t<free_truck; t++)
				{
					if (trucks[t]->state==RECYCLE && !strcmp(name, trucks[t]->realtruckfilename))
					{
						recy=true;
						trucks[t]->state=NETWORKED;
						trucks[t]->label=label;
						trucks[t]->reset();
						client_t cinfo = net->vehicle_spawned(uid, t);
						trucks[t]->setNetworkInfo(cinfo);
						break;
					}
				}
				if (!recy)
				{
					std::vector<Ogre::String> truckconfig;
					truckconfig.push_back("networked");

					trucks[free_truck]=new Beam(free_truck, mSceneMgr, mSceneMgr->getRootSceneNode(), mWindow,
						&mapsizex, &mapsizez, truckx, trucky, truckz, Quaternion::ZERO, name, collisions, dustp, clumpp, sparksp, dripp, splashp, ripplep, hfinder, w, mCamera, mirror, true, true,false,0,false,flaresMode, &truckconfig);
					trucks[free_truck]->label=label;

					if(bigMap)
					{
						MapEntity *e = bigMap->createNamedMapEntity("Truck"+StringConverter::toString(free_truck), MapControl::getTypeByDriveable(trucks[free_truck]->driveable));
						e->setState(DESACTIVATED);
						e->setVisibility(true);
						e->setPosition(truckx, truckz);
						e->setRotation(-Radian(trucks[free_truck]->getHeadingDirectionAngle()));
					}

					free_truck++;
					client_t cinfo = net->vehicle_spawned(uid, free_truck-1);
					trucks[free_truck-1]->setNetworkInfo(cinfo);

				}

				//add text
				/*
				char poename[256];
				sprintf(poename,"maptext-%s-%s", nickname, "netname");
				TextAreaOverlayElement *taoe=new TextAreaOverlayElement(poename);
				taoe->setColourTop(ColourValue(0.1, 0.1, 0.1, 1.0));
				taoe->setColourBottom(ColourValue(0.0, 0.0, 0.0, 1.0));
				taoe->setFontName("BlueHighway");
				taoe->setCharHeight(0.02);
				taoe->setCaption(nickname);
				taoe->setLeft(bigMapo->getWidth()*(truckx/mapsizex)+0.01);
				taoe->setTop(bigMapo->getHeight()*(trucky/mapsizez)-0.01);
				((OverlayContainer*)bigMapo)->addChild(taoe);
				*/
			}
		}
	}


#ifdef ANGELSCRIPT
	// only call the script step every 200ms, not more often!
	timeLastScriptStep += dt;
	// TODO: let the script choose the sleep time itself
	if(scriptEngine && timeLastScriptStep > 0.2)
	{
		scriptEngine->framestep(timeLastScriptStep);
		timeLastScriptStep = 0;
	}

#endif

	return true;
}

void ExampleFrameListener::recursiveActivation(int j)
{
	int i;
	for (i=0; i<free_truck; i++)
	{
		if ((trucks[i]->state==SLEEPING || trucks[i]->state==MAYSLEEP || trucks[i]->state==GOSLEEP ||(trucks[i]->state==DESACTIVATED && trucks[i]->sleepcount>=5)) &&
			((trucks[j]->minx<trucks[i]->minx && trucks[i]->minx<trucks[j]->maxx) || (trucks[j]->minx<trucks[i]->maxx && trucks[i]->maxx<trucks[j]->maxx) || (trucks[i]->minx<trucks[j]->maxx && trucks[j]->maxx<trucks[i]->maxx)) &&
			((trucks[j]->miny<trucks[i]->miny && trucks[i]->miny<trucks[j]->maxy) || (trucks[j]->miny<trucks[i]->maxy && trucks[i]->maxy<trucks[j]->maxy) || (trucks[i]->miny<trucks[j]->maxy && trucks[j]->maxy<trucks[i]->maxy)) &&
			((trucks[j]->minz<trucks[i]->minz && trucks[i]->minz<trucks[j]->maxz) || (trucks[j]->minz<trucks[i]->maxz && trucks[i]->maxz<trucks[j]->maxz) || (trucks[i]->minz<trucks[j]->maxz && trucks[j]->maxz<trucks[i]->maxz))
			)
		{
			trucks[i]->desactivate();//paradoxically, this activates the truck!
			trucks[i]->disableDrag=trucks[current_truck]->driveable==AIRPLANE;
			recursiveActivation(i);
		};
	}
}

//j is the index of a MAYSLEEP truck, returns true if one active was found in the set
bool ExampleFrameListener::checkForActive(int j, bool *sleepyList)
{
	int i;
	sleepyList[j]=true;
	for (i=0; i<free_truck; i++)
	{
		if ( !sleepyList[i] &&
			((trucks[j]->minx<trucks[i]->minx && trucks[i]->minx<trucks[j]->maxx) || (trucks[j]->minx<trucks[i]->maxx && trucks[i]->maxx<trucks[j]->maxx) || (trucks[i]->minx<trucks[j]->maxx && trucks[j]->maxx<trucks[i]->maxx)) &&
			((trucks[j]->miny<trucks[i]->miny && trucks[i]->miny<trucks[j]->maxy) || (trucks[j]->miny<trucks[i]->maxy && trucks[i]->maxy<trucks[j]->maxy) || (trucks[i]->miny<trucks[j]->maxy && trucks[j]->maxy<trucks[i]->maxy)) &&
			((trucks[j]->minz<trucks[i]->minz && trucks[i]->minz<trucks[j]->maxz) || (trucks[j]->minz<trucks[i]->maxz && trucks[i]->maxz<trucks[j]->maxz) || (trucks[i]->minz<trucks[j]->maxz && trucks[j]->maxz<trucks[i]->maxz))
			)
		{
			if (trucks[i]->state==MAYSLEEP || (trucks[i]->state==DESACTIVATED && trucks[i]->sleepcount>=5))
			{
				if (checkForActive(i, sleepyList)) return true;
			}
			else return true;
		};
	}
	return false;
}

void ExampleFrameListener::flashMessage(Ogre::String txt, float time, float charHeight)
{
	flashMessage(const_cast<char*>(txt.c_str()), time, charHeight);
}

void ExampleFrameListener::flashMessage(char* txt, float time, float charHeight)
{
	if(!txt || time < 0)
	{
		// hide
		flashOverlay->hide();
		timeUntilUnflash=0;
		return;
	}
	try
	{
		// catch any non UTF chars
		flashMessageTE->setCaption(txt);
	} catch(...)
	{
	}

	if(charHeight != -1)
		flashMessageTE->setCharHeight(charHeight);
	flashOverlay->show();
	timeUntilUnflash=time;
}

void ExampleFrameListener::flashMessage()
{
	flashOverlay->show();
	timeUntilUnflash=1;
}


bool ExampleFrameListener::frameEnded(const FrameEvent& evt)
{
	updateStats();

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
		/*
		// this *could* improve the event handling under windows ...
		MSG msg;
		if (PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE )>0)
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		*/
#endif

	//		moveCamera();

	// workaround to be able to show a single waiting sign before working on the files
	//if(uiloader && uiloader->hasWork())
	//	uiloader->dowork();

	if(heathaze)
		heathaze->update();
	return true;
}


void ExampleFrameListener::showLoad(int type, char* instance, char* box)
{
	// check for water
	/*
	if ((SETTINGS.getSetting("Water effects")=="None") && type == LOADER_BOAT)
	{
		flashMessage("Closed (No water)", 4);
		return;
	}
	*/

	//first, test if the place if clear
	collision_box_t *spawnbox=collisions->getBox(instance, box);
	for (int t=0; t<free_truck; t++)
	{
		for (int i=0; i<trucks[t]->free_node; i++)
		{
			if (collisions->isInside(trucks[t]->nodes[i].AbsPosition, spawnbox))
			{
				//boy, thats bad
				flashMessage(_L("Please clear the place first").c_str(), 4);
				return;
			}
		}
	}
	reload_pos=collisions->getPosition(instance, box);
	reload_dir=collisions->getDirection(instance, box);
	reload_box=collisions->getBox(instance, box);
	loading_state=RELOADING;
	hideMap();
	UILOADER.show(type);
}

bool ExampleFrameListener::fileExists(char* filename)
{
	FILE* f = fopen(filename, "rb");
	if(f != NULL) {
		fclose(f);
		return true;
	}
	return false;
}

void ExampleFrameListener::setDirectionArrow(char *text, Vector3 position)
{
	if(!text)
	{
		dirArrowNode->setVisible(false);
		dirvisible = false;
		dirArrowPointed = Vector3::ZERO;
		directionOverlay->hide();
	}
	else
	{
		directionOverlay->show();
		directionArrowText->setCaption(String(text));
		float w = directionArrowText->getWidth();
		//LogManager::getSingleton().logMessage("*** new pointed position: " + StringConverter::toString(position));
		directionArrowDistance->setCaption("");
		dirvisible = true;
		dirArrowPointed = position;
		dirArrowNode->setVisible(true);
	}

}

void ExampleFrameListener::netDisconnectTruck(int number)
{
	if(bigMap)
	{
		MapEntity *e = bigMap->getEntityByName("Truck"+StringConverter::toString(number));
		if(e)
			e->setVisibility(false);
	}
}


/* --- Window Events ------------------------------------------ */
void ExampleFrameListener::windowResized(RenderWindow* rw)
{
	if(!rw)
		return;
	LogManager::getSingleton().logMessage("*** windowResized");

	if(initialized && mCamera)
	{
		Viewport *vp = mCamera->getViewport();
		if(vp)
			mCamera->setAspectRatio(Real(vp->getActualWidth()) / Real(vp->getActualHeight()));
	}

	// update GUI area
	MYGUI.windowResized(rw);

	//update mouse area
	INPUTENGINE.windowResized(rw);
}

//Unattach OIS before window shutdown (very important under Linux)
void ExampleFrameListener::windowClosed(RenderWindow* rw)
{
	LogManager::getSingleton().logMessage("*** windowClosed");
}

void ExampleFrameListener::windowMoved(RenderWindow* rw)
{
	LogManager::getSingleton().logMessage("*** windowMoved");
}

void ExampleFrameListener::windowFocusChange(RenderWindow* rw)
{
	LogManager::getSingleton().logMessage("*** windowFocusChange");
	INPUTENGINE.resetKeys();
}

Ogre::Ray ExampleFrameListener::getMouseRay()
{
	return mCamera->getCameraToViewportRay((float)mouseX/(float)screenWidth, (float)mouseY/(float)screenHeight);
}

void ExampleFrameListener::pauseSim(bool value)
{
	// TODO: implement this (how to do so?)
	static int savedmode = -1;
	if(value)
	{
		savedmode = loading_state;
		loading_state = EDITOR_PAUSE;
		LogManager::getSingleton().logMessage("** pausing game");
	}else if (!value && savedmode != -1)
	{
		loading_state = savedmode;
		LogManager::getSingleton().logMessage("** unpausing game");
	}
}

