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
#ifndef __RoRFrameListener_H_
#define __RoRFrameListener_H_

#include "RoRPrerequisites.h"

#include "BeamData.h" // for localizer_t
#include "Ogre.h"

#include <pthread.h>

// Forward declarations
class Character;
class Envmap;
class ForceFeedback;

namespace MOC
{
	class CollisionTools;
}

namespace Ogre
{
	class TerrainGroup;
}

#ifdef USE_PAGED
namespace Forests
{
	class PagedGeometry;
	class TreeLoader2D;
}
#endif //USE_PAGED

class RoRFrameListener: public Ogre::FrameListener, public Ogre::WindowEventListener
{
public:
	// Constructor takes a RenderWindow because it uses that to determine input context
	RoRFrameListener(AppState *parent, Ogre::RenderWindow* win, Ogre::Camera* cam, Ogre::SceneManager* scm, Ogre::Root* root, bool isEmbedded=false, Ogre::String inputhwnd=0);
	virtual ~RoRFrameListener();


	Character *person;
	ChatSystem *netChat;

	bool freeTruckPosition;

	float netcheckGUITimer;

	int loading_state;

	enum LoadingStatuses { NONE_LOADED, TERRAIN_LOADED, ALL_LOADED, EXITING, EDITING, RELOADING, PAUSE, TERRAIN_EDITOR };
	enum SurveyMapTypes { SURVEY_MAP_NONE, SURVEY_MAP_SMALL, SURVEY_MAP_BIG, SURVEY_MAP_END};

protected:

	typedef struct
	{
		float px;
		float py;
		float pz;
		//float ry;
		Ogre::Quaternion rotation;
		char name[256];
		bool ismachine;
		bool freePosition;
	} truck_prepare_t;

	typedef struct
	{
		Ogre::Vector3 pos;
		Ogre::Quaternion rot;
	} spawn_location_t;


#ifdef USE_PAGED
	typedef struct
	{
		Forests::PagedGeometry *geom;
		void *loader;
	} paged_geometry_t;

	std::vector<paged_geometry_t> pagedGeometry;
	Forests::TreeLoader2D *treeLoader;
#endif //USE_PAGED

#ifdef HAS_EDITOR
	TruckEditor *trucked;
#endif //HAS_EDITOR

#ifdef USE_MPLATFORM
	MPlatform_Base *mplatform;
#endif //USE_MPLATFORM

#ifdef USE_OIS_G27
	OIS::Win32LogitechLEDs *leds;
#endif //USE_OIS_G27

	AppState *parentState;
	Dashboard *dashboard;
	DOFManager *dof;
	Editor *editor;
	FILE *editorfd;
	ForceFeedback *forcefeedback;
	HeatHaze *heathaze;
	MapControl *surveyMap;
	MapTextureCreator *mtc;
	Network *net;

	Ogre::Camera* mCamera;

	Ogre::Quaternion reload_dir;
	Ogre::Real distgrabbed;
	Ogre::Real mTimeUntilNextToggle; // just to stop toggles flipping too fast
	Ogre::RenderWindow* mWindow;
	Ogre::RenderWindow* renderwin;
	Ogre::Root *mRoot;
	Ogre::SceneManager *mSceneMgr;
	Ogre::SceneNode *dirArrowNode;
	Ogre::SceneNode *pointerDestination;
	Ogre::String grassdensityTextureFilename;
	Ogre::String inputhwnd;
	Ogre::String terrainUID;

	Ogre::Vector3 dirArrowPointed;
	Ogre::Vector3 persostart;

	OverlayWrapper *ow;
	bool benchmarking;
	bool chatlock;
	bool debugCollisions;
	bool dirvisible;
	bool enablePosStor;
	bool flipflop;
	bool hidegui;
	bool initialized;
	bool isEmbedded;
	bool mTruckInfoOn;
	bool netmode;
	bool pressure_pressed;

	char screenshotformat[256];
	char terrainmap[1024];
	
	TerrainManager *terrainManager;

	collision_box_t *reload_box;
	double rtime;

	float clutch;
	float farclip;
	float mouseGrabForce;
	float terrainxsize;
	float terrainzsize;
	float truckx, trucky, truckz;

	int flaresMode;
	int free_localizer;
	int gameStartTime;
	int inputGrabMode;
	int joyshiftlock;
	int mStatsOn;
	int surveyMapMode;
	int mouseGrabState;
	int netPointToUID;
	int nodegrabbed;
	int objcounter;
	int objectCounter;
	int raceStartTime;
	int screenHeight;
	int screenWidth;
	int shaderSchemeMode;
	int truck_preload_num;
	int truckgrabbed;
	

	truck_prepare_t truck_preload[100];

	unsigned int mNumScreenShots;
	
	bool updateTruckMirrors(float dt);

	int setupBenchmark();

	void gridScreenshots(Ogre::RenderWindow* pRenderWindow, Ogre::Camera* pCamera, const int& pGridSize, const Ogre::String& path, const Ogre::String& pFileName, const Ogre::String& pFileExtention, const bool& pStitchGridImages);

	void initSoftShadows();
	void initializeCompontents();
	void updateGUI(float dt); // update engine panel
	void updateIO(float dt);
	void updateStats(void);

	// WindowEventListener
	void windowMoved(Ogre::RenderWindow* rw);
	void windowClosed(Ogre::RenderWindow* rw);
	void windowFocusChange(Ogre::RenderWindow* rw);

private:

	int net_quality;
	bool net_quality_changed;

public: // public methods

	Network *getNetwork() { return net; };

	Ogre::RenderWindow *getRenderWindow() { return mWindow; };
	Ogre::SceneManager *getSceneMgr() { return mSceneMgr; };
	Ogre::Camera *getCamera() { return mCamera; };

	OverlayWrapper *getOverlayWrapper() { return ow; };

	bool RTSSgenerateShadersForMaterial(Ogre::String curMaterialName, Ogre::String normalTextureName);
	bool frameEnded(const Ogre::FrameEvent& evt);
	bool frameStarted(const Ogre::FrameEvent& evt); // Override frameStarted event to process that (don't care about frameEnded)

	bool updateEvents(float dt);
	double getTime() { return rtime; };
	float stopTimer();

	int getLoadingState() { return loading_state; };
	int getNetPointToUID() { return netPointToUID; };

	void changedCurrentTruck(Beam *previousTruck, Beam *currentTruck);
	void checkRemoteStreamResultsChanged();
	void hideGUI(bool visible);
	void hideMap();
	void initTrucks(bool loadmanual, Ogre::String selected, Ogre::String selectedExtension = Ogre::String(), std::vector<Ogre::String> *truckconfig=0, bool enterTruck=false, Skin *skin=NULL);
	
	void loadTerrain(Ogre::String terrainfile);

	void netDisconnectTruck(int number);
	void pauseSim(bool value);
	void reloadCurrentTruck();
	void removeBeam(Beam *);
	void RTSSgenerateShaders(Ogre::Entity *entity, Ogre::String normalTextureName);
	void setDirectionArrow(char *text, Ogre::Vector3 position);
	void setLoadingState(int value);
	void setNetPointToUID(int uid);
	void showLoad(int type, char* instance, char* box);
	void showspray(bool s);
	void shutdown_final();
	void startTimer();
	void updateCruiseControl(Beam* curr_truck, float dt);
	void checkSpeedlimit(Beam* curr_truck, float dt);
	void updateRacingGUI();
	void windowResized(Ogre::RenderWindow* rw); // this needs to be public so we can call it manually in embedded mode

	// mutex'ed data
	void setNetQuality(int q);
	int getNetQuality(bool ack=false);
	bool getNetQualityChanged();
	pthread_mutex_t mutex_data;
};

#endif // __RoRFrameListener_H_
