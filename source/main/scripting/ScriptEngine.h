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
// created on 24th of February 2009 by Thomas Fischer

#ifndef SCRIPTENGINE_H__
#define SCRIPTENGINE_H__

#include "RoRPrerequisites.h"

#include <string>
#include <angelscript.h>
#include <Ogre.h>
#include <OgreLogManager.h>


#include "scriptdictionary/scriptdictionary.h"
#include "scriptbuilder/scriptbuilder.h"

#include "collisions.h"

#define AS_INTERFACE_VERSION "0.2.0" //!< versioning for the scripting interface

#define SLOG(x) ScriptEngine::getSingleton().scriptLog->logMessage(x);

/**
 * @file ScriptEngine.h
 * @version 0.1.0
 * @brief AngelScript interface to the game
 * @authors Thomas Fischer (thomas{AT}rigsofrods{DOT}com)
 */

class GameScript;

/**
 *  @brief This class represents the angelscript scripting interface. It can load and execute scripts.
 */
class ScriptEngine : public Ogre::Singleton<ScriptEngine>, public Ogre::LogListener
{
	friend class GameScript;
public:
	ScriptEngine(RoRFrameListener *efl, Collisions *_coll);
	~ScriptEngine();

	void setCollisions(Collisions *_coll) { coll=_coll; };

	/**
	 * Loads a script
	 * @param scriptname filename to load
	 * @return 0 on success, everything else on error
	 */
	int loadScript(Ogre::String scriptname);

	/**
	 * Calls the script's framestep function to be able to use timed things inside the script
	 * @param dt time passed since the last call to this function in seconds
	 * @return 0 on success, everything else on error
	 */
	int framestep(Ogre::Real dt);

	unsigned int eventMask;                              //!< filter mask for script events
	
	/**
	 * triggers an event. Not to be used by the end-user
	 * @param eventValue \see enum scriptEvents
	 */
	void triggerEvent(int scriptEvents, int value=0);

	/**
	 * executes a string (useful for the console)
	 * @param command string to execute
	 */
	int executeString(Ogre::String command);

	int envokeCallback(int functionPtr, eventsource_t *source, node_t *node=0, int type=0);

	AngelScript::asIScriptEngine *getEngine() { return engine; };

	Ogre::String getTerrainName() { return terrainScriptName; };
	Ogre::String getTerrainScriptHash() { return terrainScriptHash; };

	// method from Ogre::LogListener
	virtual void messageLogged( const Ogre::String& message, Ogre::LogMessageLevel lml, bool maskDebug, const Ogre::String &logName );

	void exploreScripts();


	Ogre::Log *scriptLog;

protected:
    RoRFrameListener *mefl;             //!< local RoRFrameListener instance, used as proxy for many functions
	Collisions *coll;
    AngelScript::asIScriptEngine *engine;                //!< instance of the scripting engine
	AngelScript::asIScriptContext *context;              //!< context in which all scripting happens
	int frameStepFunctionPtr;               //!< script function pointer to the frameStep function
	int wheelEventFunctionPtr;               //!< script function pointer
	int eventCallbackFunctionPtr;           //!< script function pointer to the event callback function
	int defaultEventCallbackFunctionPtr;    //!< script function pointer for spawner events
	Ogre::String terrainScriptName, terrainScriptHash;
	std::map <std::string , std::vector<int> > callbacks;
	
	static char *moduleName;


	/**
	 * This function initialzies the engine and registeres all types
	 */
    void init();

	/**
	 * This is the callback function that gets called when script error occur.
	 * When the script crashes, this function will provide you with more detail
	 * @param msg arguments that contain details about the crash
	 * @param param unkown?
	 */
    void msgCallback(const AngelScript::asSMessageInfo *msg);

	/**
	 * This function reads a file into the provided string.
	 * @param filename filename of the file that should be loaded into the script string
	 * @param script reference to a string where the contents of the file is written to
	 * @param hash reference to a string where the hash of the contents is written to
	 * @return 0 on success, everything else on error
	 */
	int loadScriptFile(const char *fileName, std::string &script, std::string &hash);

	// undocumented debugging functions below, not working.
	void ExceptionCallback(AngelScript::asIScriptContext *ctx, void *param);
	void PrintVariables(AngelScript::asIScriptContext *ctx, int stackLevel);
	void LineCallback(AngelScript::asIScriptContext *ctx, unsigned long *timeOut);
};


#endif //SCRIPTENGINE_H__