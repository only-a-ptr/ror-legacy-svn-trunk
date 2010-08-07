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
#include "Landusemap.h"
#include "collisions.h"
#include <Ogre.h>
#include <OgreConfigFile.h>
#include <OgreLogManager.h>
#include <OgreStringConverter.h>
#include "language.h"
#include "errorutils.h"

#ifdef USE_PAGED
#include "PropertyMaps.h"
#include "PagedGeometry.h"
#endif

using namespace Ogre;

// this is the Height-Finder for the standart ogre Terrain Manager

Landusemap::Landusemap(String configFilename, Collisions *c, int _mapsizex, int _mapsizez) :
	data(0), coll(c), mapsizex(_mapsizex), mapsizez(_mapsizez)
{
	loadConfig(configFilename);
#ifndef USE_PAGED
	LogManager::getSingleton().logMessage("RoR was not compiled with PagedGeometry support. You cannot use Landuse maps with it.");
#endif //USE_PAGED
}

Landusemap::~Landusemap()
{
	delete data;
}

ground_model_t *Landusemap::getGroundModelAt(int x, int z)
{
	if(!data) return 0;
#ifdef USE_PAGED
	// we return the default ground model if we are not anymore in this map
	if (x < 0 || x >= mapsizex || z < 0 || z >= mapsizez)
		return default_ground_model;

	return data[x + z * mapsizex];
#else
	return 0;
#endif // USE_PAGED
}


int Landusemap::loadConfig(Ogre::String filename)
{
	std::map<unsigned int, String> usemap;
	int version = 1;
	String textureFilename = "";

	LogManager::getSingleton().logMessage("Parsing landuse config: '"+filename+"'");

	String group = "";
	try
	{
		group = ResourceGroupManager::getSingleton().findGroupContainingResource(filename);
	}catch(...)
	{
		// we wont catch anything, since the path could be absolute as well, then the group is not found
	}

	Ogre::ConfigFile cfg;
	try
	{
		// try to load directly otherwise via resource group
		if(group == "")
			cfg.load(filename);
		else
			cfg.load(filename, group, "\x09:=", true);
	} catch(Ogre::Exception& e)
	{
		showError("Error while loading landuse config", e.getFullDescription());
		return 1;
	}

	Ogre::ConfigFile::SectionIterator seci = cfg.getSectionIterator();
	Ogre::String secName, kname, kvalue;
	while (seci.hasMoreElements())
	{
		secName = seci.peekNextKey();
		Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
		Ogre::ConfigFile::SettingsMultiMap::iterator i;
		for (i = settings->begin(); i != settings->end(); ++i)
		{
			kname = i->first;
			kvalue = i->second;
			// we got all the data available now, processing now
			if(secName == "general" || secName == "config")
			{
				// set some class properties accoring to the information in this section
				if(kname == "texture")
					textureFilename = kvalue;
				else if(kname == "frictionconfig" || kname == "loadGroundModelsConfig")
					coll->loadGroundModelsConfigFile(kvalue);
				else if(kname == "defaultuse")
					default_ground_model = coll->getGroundModelByString(kvalue);
				else if(kname == "version")
					version = StringConverter::parseInt(kvalue);

			} else if(secName == "use-map")
			{
				if(kname.size() != 10)
				{
					LogManager::getSingleton().logMessage("invalid color in landuse line in " + filename);
					continue;
				}
				char *ptr; //not used
				unsigned int color = strtoul(kname.c_str(), &ptr, 16);
				usemap[color] = kvalue;
			}
		}
	}

#ifdef USE_PAGED
	// process the config data and load the buffers finally
	Forests::ColorMap *colourMap = Forests::ColorMap::load(textureFilename, Forests::CHANNEL_COLOR);
	colourMap->setFilter(Forests::MAPFILTER_NONE);
	Ogre::TRect<Ogre::Real> bounds = Forests::TBounds(0, 0, mapsizex, mapsizez);

	/*
	// debug things below
	printf("found ground use definitions:\n");
	for(std::map < uint32, String >::iterator it=usemap.begin(); it!=usemap.end(); it++)
	{
		printf(" 0x%Lx : %s\n", it->first, it->second.c_str());
	}
	*/

	bool bgr = colourMap->getPixelBox().format == PF_A8B8G8R8;

	// now allocate the data buffer to hold pointers to ground models
	data = new ground_model_t*[mapsizex * mapsizez];
	ground_model_t **ptr = data;
	//std::map < String, int > counters;
	for(int z=0; z<mapsizez; z++)
	{
		for(int x=0; x<mapsizex; x++)
		{
			unsigned int col = colourMap->getColorAt(x, z, bounds);
			if (bgr)
			{
				// Swap red and blue values
				unsigned int cols = col & 0xFF00FF00;
				cols |= (col & 0xFF) << 16;
				cols |= (col & 0xFF0000) >> 16;
				col = cols;
			}
			String use = usemap[col];
			//if(use!="")
			//	counters[use]++;

			// store the pointer to the ground model in the data slot
			*ptr = coll->getGroundModelByString(use);
			ptr++;
		}
	}
#endif // USE_PAGED
	return 0;
}
