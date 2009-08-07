#ifdef OPENSTEER
#include "AITraffic.h"
#include "ExampleFrameListener.h"

AITraffic::AITraffic(ScriptEngine *engine)
{
	scriptengine = engine;
	initialize();
}

AITraffic::~AITraffic()
{
	if (aimatrix) delete(aimatrix);
}

void AITraffic::load()
{}

void AITraffic::initialize()
{
	aimatrix = new AITraffic_Matrix();

	mLampTimer = 0;
	mLampTimer				= 0;
	cnt						= 0;
	num_of_intersections	= 1;
	num_of_waypoints		= 50;
	mTotalElapsedTime		= 0.0f;
/*
	Penguinville testtrack
//	123.429, 0.00173146, 24.7226	<- this is width = 5;
	31.2367, 0.0496842, 19.8068
	123.445, 0.0496726, 19.8812
	140.172, 0.0499454, 34.2714
	140.249, 0.0507317, 72.9784
	164.384, 0.0485949, 19.8671
	243.265, 0.0497771, 19.8707
*/

	// setting up waypoints
	aimatrix->trafficgrid->num_of_waypoints = 6;
	aimatrix->trafficgrid->waypoints[0].position = Ogre::Vector3(31.2367, 0.0496842, 19.8068);
	aimatrix->trafficgrid->waypoints[1].position = Ogre::Vector3(123.445, 0.0496726, 19.8812);
	aimatrix->trafficgrid->waypoints[2].position = Ogre::Vector3(140.172, 0.0499454, 34.2714);
	aimatrix->trafficgrid->waypoints[3].position = Ogre::Vector3(140.249, 0.0507317, 72.9784);
	aimatrix->trafficgrid->waypoints[4].position = Ogre::Vector3(164.384, 0.0485949, 19.8671);
	aimatrix->trafficgrid->waypoints[5].position = Ogre::Vector3(243.265, 0.0497771, 19.8707);

	// setting up segments
	aimatrix->trafficgrid->num_of_segments = 5;

	aimatrix->trafficgrid->segments[0].start			= 0;
	aimatrix->trafficgrid->segments[0].end				= 1;
	aimatrix->trafficgrid->segments[0].num_of_lanes		= 2;
	aimatrix->trafficgrid->segments[0].width			= 5;

	aimatrix->trafficgrid->segments[1].start			= 1;
	aimatrix->trafficgrid->segments[1].end				= 2;
	aimatrix->trafficgrid->segments[1].num_of_lanes		= 2;
	aimatrix->trafficgrid->segments[1].width			= 5;

	aimatrix->trafficgrid->segments[2].start			= 2;
	aimatrix->trafficgrid->segments[2].end				= 3;
	aimatrix->trafficgrid->segments[2].num_of_lanes		= 2;
	aimatrix->trafficgrid->segments[2].width			= 5;

	aimatrix->trafficgrid->segments[3].start			= 1;
	aimatrix->trafficgrid->segments[3].end				= 4;
	aimatrix->trafficgrid->segments[3].num_of_lanes		= 2;
	aimatrix->trafficgrid->segments[3].width			= 5;

	aimatrix->trafficgrid->segments[4].start			= 4;
	aimatrix->trafficgrid->segments[4].end				= 5;
	aimatrix->trafficgrid->segments[4].num_of_lanes		= 2;
	aimatrix->trafficgrid->segments[4].width			= 5;

	// connect segments together

	aimatrix->trafficgrid->waypoints[0].num_of_connections	= 1;
	aimatrix->trafficgrid->waypoints[0].nextsegs[0].segment = 0;

	aimatrix->trafficgrid->waypoints[1].num_of_connections	= 2;
	aimatrix->trafficgrid->waypoints[1].nextsegs[0].segment = 1;
	aimatrix->trafficgrid->waypoints[1].nextsegs[1].segment = 3;

	aimatrix->trafficgrid->waypoints[2].num_of_connections	= 1;
	aimatrix->trafficgrid->waypoints[2].nextsegs[0].segment = 2;

	aimatrix->trafficgrid->waypoints[3].num_of_connections	= 0;

	aimatrix->trafficgrid->waypoints[4].num_of_connections	= 1;
	aimatrix->trafficgrid->waypoints[4].nextsegs[0].segment = 4;

	aimatrix->trafficgrid->waypoints[5].num_of_connections	= 0;

	// creating virtual paths for traffic

	aimatrix->trafficgrid->num_of_paths = 2;
	aimatrix->trafficgrid->paths[0].num_of_segments = 3;
	aimatrix->trafficgrid->paths[0].path_type	= 2;		
	aimatrix->trafficgrid->paths[0].segments[0] = 0;
	aimatrix->trafficgrid->paths[0].segments[1] = 1;
	aimatrix->trafficgrid->paths[0].segments[2] = 2;


	aimatrix->trafficgrid->paths[1].num_of_segments = 3;
	aimatrix->trafficgrid->paths[1].path_type	= 2 ;		
	aimatrix->trafficgrid->paths[1].segments[0] = 0;
	aimatrix->trafficgrid->paths[1].segments[1] = 3;
	aimatrix->trafficgrid->paths[1].segments[2] = 4;

	num_of_vehicles = 3;
	for (int i=1;i<NUM_OF_TRAFFICED_CARS || i<num_of_vehicles;i++)
		{
			vehicles[i] = new AITraffic_Vehicle();
			vehicles[i]->serial = i;
			vehicles[i]->path_id= i-1;
			vehicles[i]->ps_idx = aimatrix->trafficgrid->paths[vehicles[i]->path_id].segments[0];
			vehicles[i]->aimatrix = aimatrix;
			vehicles[i]->setPosition(aimatrix->trafficgrid->trafficnodes[i].position);
			vehicles[i]->reset();
		}
}

void AITraffic::frameStep(Ogre::Real deltat)
{
	mLampTimer +=deltat;

//	deltat = deltat/1000.0f;
	float elapsedTime =  deltat;
	mTotalElapsedTime += deltat;

	aimatrix->trafficgrid->trafficnodes[0].rotation = playerrot; 
	aimatrix->trafficgrid->trafficnodes[0].position = playerpos; 

//	for (int i=1;i<num_of_vehicles;i++)
	for (int i=1;i<2;i++)
	{
			vehicles[i]->updateSimple(elapsedTime, mTotalElapsedTime);
			aimatrix->trafficgrid->trafficnodes[i].position = vehicles[i]->getPosition();
			aimatrix->trafficgrid->trafficnodes[i].rotation = vehicles[i]->getOrientation();
		
	}
return;
	cnt ++;
	if (cnt==1) 	
		{
			spawnObject("trafficlight3", "trafficlight-1-1", 5, 0, 5, 0, 0, 0, "trafficevent", true);
			spawnObject("trafficlight3", "trafficlight-1-2", 5, 0, 6, 0, 0, 0, "trafficevent", true);
			spawnObject("trafficlight3", "trafficlight-1-3", 5, 0, 7, 0, 0, 0, "trafficevent", true);
			spawnObject("trafficlight3", "trafficlight-1-4", 5, 0, 8, 0, 0, 0, "trafficevent", true);
			spawnObject("trafficlight3", "trafficlight-1-5", 5, 0, 9, 0, 0, 0, "trafficevent", true);
			spawnObject("trafficlight3", "trafficlight-1-6", 5, 0, 10, 0, 0, 0, "trafficevent", true);
			
			setSignalState("/trafficlight-1-1",0);
			setSignalState("/trafficlight-1-2",0);
			setSignalState("/trafficlight-1-3",0);
			setSignalState("/trafficlight-1-4",0);
			setSignalState("/trafficlight-1-5",0);
			setSignalState("/trafficlight-1-6",0);

			intersection[0].num_of_lamps = 6;
			intersection[0].lamps[0].name = "/trafficlight-1-1";
			intersection[0].lamps[1].name = "/trafficlight-1-2";
			intersection[0].lamps[2].name = "/trafficlight-1-3";
			intersection[0].lamps[3].name = "/trafficlight-1-4";
			intersection[0].lamps[4].name = "/trafficlight-1-5";
			intersection[0].lamps[5].name = "/trafficlight-1-6";

			intersection[0].length_of_program = 3;
			intersection[0].program[0] = 35;
			intersection[0].program[1] = 56;
			intersection[0].program[2] = 44;

			intersection[0].inchange = false;
			intersection[0].prg_idx = 0;
		}

	if (mLampTimer>1)
		{
			updateLamps();
			mLampTimer = 0.0f;
		}

}

void AITraffic::processOneCar(int idx, float delta)
{

		// is it close to the driven car?
		
		// now we simply update car offsets
//		trafficgrid[idx].x1 += 0.1f;
//		trafficgrid[idx].x2 += 0.1f;
}

/* ============================== TRAFFIC LAMP MANAGEMENT ================================= */

void AITraffic::updateLamps()
{
	for (int k=0;k<num_of_intersections;k++)
		{
			if (intersection[k].inchange)
				{
					intersection[k].inchange = false;
					for (int i=0;i<intersection[k].num_of_lamps;i++)
						{
							if (intersection[k].lamps[i].state == intersection[k].lamps[i].nextstate)
								{
									// do nothing
								}
							else
								{
									int stat = 1;
									if (intersection[k].lamps[i].state==4)			stat = 2;
									else if (intersection[k].lamps[i].state==1)	stat = 3;
									setSignalState(intersection[k].lamps[i].name, stat);
								}
						}
				}
			else  
				{
					intersection[k].inchange = true;
					intersection[k].prg_idx++;
					if (intersection[k].prg_idx>=intersection[k].length_of_program) intersection[k].prg_idx = 0;
//					intersection.prg_idx = intersection.prg_idx % intersection.length_of_program;
//					int nextidx = (intersection.prg_idx+1) % intersection.length_of_program;

					int nextidx = intersection[k].prg_idx+1;
					if (nextidx>=intersection[k].length_of_program) nextidx = 0;

					int flag = 1;
					for (int i=0;i<intersection[k].num_of_lamps;i++)
						{
							// this should be optimized the following way:
							// we calculate only nextstate and when updating that, we use the older state 
							// to disply

							int stat = 1;
							// set the current state
							if (intersection[k].program[intersection[k].prg_idx] & flag) stat = 4;
							intersection[k].lamps[i].state = stat;
	
							setSignalState(intersection[k].lamps[i].name, stat);
				
							// calculate next state
							stat = 1;
							if (intersection[k].program[nextidx] & flag) stat = 4;
							intersection[k].lamps[i].nextstate = stat;

							flag *=2;
						}
				}
		}
}


// register traffic lamp to the system
void AITraffic::registerTrafficLamp(int id, int group_id, int role, int name)
{
}

// mode: 0 - normal, 1 - blinking, 2 - out of order			
void AITraffic::setTrafficLampGroupServiceMode(int mode)
{
}


int AITraffic::setMaterialAmbient(const std::string &materialName, float red, float green, float blue)
{
	try
	{
		Ogre::MaterialPtr m = Ogre::MaterialManager::getSingleton().getByName(materialName);
		if(m.isNull()) return 0;
		m->setAmbient(red, green, blue);
	} catch(...)
	{
		return 0;
	}
	return 1;
}

int AITraffic::setMaterialDiffuse(const std::string &materialName, float red, float green, float blue, float alpha)
{
	try
	{
		Ogre::MaterialPtr m = Ogre::MaterialManager::getSingleton().getByName(materialName);
		if(m.isNull()) return 0;
		m->setDiffuse(red, green, blue, alpha);
	} catch(...)
	{
		return 0;
	}
	return 1;
}

int AITraffic::setMaterialSpecular(const std::string &materialName, float red, float green, float blue, float alpha)
{
	try
	{
		Ogre::MaterialPtr m = Ogre::MaterialManager::getSingleton().getByName(materialName);
		if(m.isNull()) return 0;
		m->setSpecular(red, green, blue, alpha);
	} catch(...)
	{
		return 0;
	}
	return 1;
}

int AITraffic::setMaterialEmissive(const std::string &materialName, float red, float green, float blue)
{
	try
	{
		Ogre::MaterialPtr m = Ogre::MaterialManager::getSingleton().getByName(materialName);
		if(m.isNull()) return 0;
		m->setSelfIllumination(red, green, blue);
	} catch(...)
	{
		return 0;
	}
	return 1;
}

void AITraffic::spawnObject(const std::string &objectName, const std::string &instanceName, float px, float py, float pz, float rx, float ry, float rz, const std::string &eventhandler, bool uniquifyMaterials)
{
/*
	asIScriptModule *mod=0;
	try
	{
		mod = mse->getEngine()->GetModule("terrainScript", asGM_ONLY_IF_EXISTS);
	}catch(...)
	{
		return;
	}
	if(!mod) return;
	int functionPtr = mod->GetFunctionIdByName(eventhandler.c_str());
*/
	// trying to create the new object
	ExampleFrameListener *mefl = (ExampleFrameListener *)scriptengine;
	SceneNode *bakeNode=mefl->getSceneMgr()->getRootSceneNode()->createChildSceneNode();
	mefl->loadObject(const_cast<char*>(objectName.c_str()), px, py, pz, rx, ry, rz, bakeNode, const_cast<char*>(instanceName.c_str()), true, 0, const_cast<char*>(objectName.c_str()), uniquifyMaterials);
}

void AITraffic::setSignalState(Ogre::String instance, int state)
{
	int red		= state & 1;
	int yellow	= state & 2;
	int green	= state & 4;
	// states:
	// 0 = red
	// 1 = yellow
	// 2 = green


	setMaterialAmbient("trafficlight3/top"+instance,	red,	0,	0);
	setMaterialDiffuse("trafficlight3/top"+instance,	red,	0,	0, 1);
	setMaterialEmissive("trafficlight3/top"+instance,	(red? 1:0),	0,	0);

	setMaterialAmbient("trafficlight3/middle"+instance, yellow, yellow, 0);
	setMaterialDiffuse("trafficlight3/middle"+instance, yellow, yellow, 0, 1);
	setMaterialEmissive("trafficlight3/middle"+instance, (yellow? 1:0), (yellow? 1:0), 0);

	setMaterialAmbient("trafficlight3/bottom"+instance, 0, green, 0);
	setMaterialDiffuse("trafficlight3/bottom"+instance, 0, green, 0, 1);
	setMaterialEmissive("trafficlight3/bottom"+instance, 0, (green? 1:0), 0);
}



#endif //OPENSTEER