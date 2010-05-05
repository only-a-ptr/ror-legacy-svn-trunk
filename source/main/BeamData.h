/*
This source file is part of Rigs of Rods
Copyright 2005,2006,2007,2008,2009,2010 Pierre-Michel Ricordel
Copyright 2007,2008,2009,2010 Thomas Fischer

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

// created on 30th of April 2010 by Thomas Fischer

#ifndef BEAMDATA_H__
#define BEAMDATA_H__

/*

 Rigs of Rods Data Structure (WIP)

 +--------------------------------------------------------+
 | Physics             | Visuals                          |
 +---------------------+----------------------------------+
 | rig_phys_t          | rig_vis_t                        |
 |  node_phys_t        | n/a                              |
 |  beam_phys_t        | n/a                              |
 +---------------------+----------------------------------+

 A word of warning:
 RoR's performance is very sensitive to the ordering of the parameters in this
 structure (due to cache reasons). You can easily destroy RoR's performance if you put
 something in the wrong place. Unless you know what you are doing (do you come armed
 with a cache usage tracker?), add what you wish to the bottom of the structure.
 
 the order of the structs in here is important as well.
*/


// The Ogre required includes
#include "OgrePrerequisites.h"
#include "OgreVector3.h"
#include "OgreColourValue.h"

// The RoR required includes
#include "RoRPrerequisites.h"

// some tool to define the bitmasks. We use this, as it it far better readable (prevents errors)
#define BITMASK(x) (1 << (x-1)) 
// BITMASK(1) = 0x00000001 = 0b00....0001
// BITMASK(2) = 0x00000002 = 0b00....0010

/* maximum limits */
static const int   MAX_TRUCKS                 = 64;              //!< maximum number of trucks for the engine

static const int   MAX_NODES                  = 1000;            //!< maximum number of nodes per truck
static const int   MAX_BEAMS                  = 5000;            //!< maximum number of beams per truck
static const int   MAX_ROTATORS               = 20;              //!< maximum number of rotators per truck
static const int   MAX_CONTACTERS             = 2000;            //!< maximum number of contacters per truck
static const int   MAX_HYDROS                 = 1000;            //!< maximum number of hydros per truck
static const int   MAX_WHEELS                 = 64;              //!< maximum number of wheels per truck
static const int   MAX_SUBMESHES              = 500;             //!< maximum number of submeshes per truck
static const int   MAX_TEXCOORDS              = 3000;            //!< maximum number of texture coordinates per truck
static const int   MAX_CABS                   = 3000;            //!< maximum number of cabs per truck
static const int   MAX_SHOCKS                 = MAX_BEAMS;       //!< maximum number of shocks per truck
static const int   MAX_ROPES                  = 64;              //!< maximum number of ropes per truck
static const int   MAX_ROPABLES               = 64;              //!< maximum number of ropables per truck
static const int   MAX_TIES                   = 64;              //!< maximum number of ties per truck
static const int   MAX_PROPS                  = 200;             //!< maximum number of props per truck
static const int   MAX_COMMANDS               = 48;              //!< maximum number of commands per truck
static const int   MAX_CAMERAS                = 10;              //!< maximum number of cameras per truck
static const int   MAX_RIGIDIFIERS            = 100;             //!< maximum number of rigifiers per truck
static const int   MAX_FLEXBODIES             = 64;              //!< maximum number of flexbodies per truck
static const int   MAX_AEROENGINES            = 8;               //!< maximum number of aero engines per truck
static const int   MAX_SCREWPROPS             = 8;               //!< maximum number of boat screws per truck
static const int   MAX_AIRBRAKES              = 20;              //!< maximum number of airbrakes per truck
static const int   MAX_SOUNDSCRIPTS_PER_TRUCK = 128;             //!< maximum number of soundsscripts per truck
static const int   MAX_WINGS                  = 40;              //!< maximum number of wings per truck
static const int   MAX_CPARTICLES             = 10;              //!< maximum number of custom particles per truck
static const int   MAX_PRESSURE_BEAMS         = 4000;            //!< maximum number of pressure beams per truck

/* other global static definitions */
static const int   TRUCKFILEFORMATVERSION     = 3;               //!< truck file format version number


/* physics defaults */
static const float DEFAULT_RIGIDIFIER_SPRING    = 1000000.0f;
static const float DEFAULT_RIGIDIFIER_DAMP      = 50000.0f;
static const float DEFAULT_SPRING               = 9000000.0f;
static const float DEFAULT_DAMP                 = 12000.0f;
static const float DEFAULT_GRAVITY              = -9.8f;         //!< earth gravity
static const float DEFAULT_DRAG                 = 0.05f;
static const float DEFAULT_BEAM_DIAMETER        = 0.05f;         //!< 5 centimeters default beam width
static const float DEFAULT_COLLISION_RANGE      = 0.02f;
static const float MIN_BEAM_LENGTH              = 0.1f;          //!< minimum beam lenght is 10 centimeters
static const float INVERTED_MIN_BEAM_LENGTH     = 1.0f / MIN_BEAM_LENGTH;
static const float BEAM_SKELETON_DIAMETER       = 0.01f;
static const float DEFAULT_WATERDRAG            = 10.0f;
static const float IRON_DENSITY                 = 7874.0f;
static const float BEAM_BREAK                   = 1000000.0f;
static const float BEAM_DEFORM                  = 400000.0f;
static const float BEAM_CREAK_DEFAULT           = 100000.0f;
static const float WHEEL_FRICTION_COEF          = 2.0f;
static const float CHASSIS_FRICTION_COEF        = 0.5f; //!< Chassis has 1/4 the friction of wheels.
static const float SPEED_STOP                   = 0.2f;
static const float STAB_RATE                    = 0.1f;
static const float NODE_FRICTION_COEF_DEFAULT   = 1.0f;
static const float NODE_VOLUME_COEF_DEFAULT     = 1.0f;
static const float NODE_SURFACE_COEF_DEFAULT    = 1.0f;
static const float NODE_LOADWEIGHT_DEFAULT      = -1.0f;

/* Enumerations */
enum {
	THREAD_MONO,    //!< mono threading mode
	THREAD_HT,      //!< dual thread mode
	THREAD_HT2      //!< multi-core threading mode
};
enum {
	BEAM_NORMAL,
	BEAM_HYDRO,
	BEAM_VIRTUAL,
	BEAM_MARKED,
	BEAM_INVISIBLE,
	BEAM_INVISIBLE_HYDRO
};
enum {
	NODE_NORMAL,
	NODE_LOADED
};

enum {
	ACTIVATED,      //!< leading truck
	DESACTIVATED,   //!< not leading but active 
	MAYSLEEP,       //!< active but wanting to sleep
	GOSLEEP,        //!< active but ordered to sleep ASAP (synchronously)
	SLEEPING,       //!< not active, sleeping
	NETWORKED,      //!< not calculated, gets remote data
	RECYCLE,        //!< waiting for reusage
	DELETED,        //!< special used when truck pointer is 0
};

enum {
	UNLOCKED,       //!< lock not locked
	PRELOCK,        //!< prelocking, attraction forces in action
	LOCKED          //!< lock locked.
};
enum {
	NOT_DRIVEABLE,  //!< not drivable at all
	TRUCK,          //!< its a truck
	AIRPLANE,       //!< its an airplane
	BOAT,           //!< its a boat
	MACHINE         //!< its a machine
};
enum {
	DRY,            //!< node is dry
	DRIPPING,       //!< node is dripping
	WET             //!< node is wet
};
enum {
	NOSHOCK,        //!< not a shock
	SHOCK1,         //!< shock1
	SHOCK2,         //!< shock2
	SUPPORTBEAM,    //!< 
	ROPE            //!< 
};
enum blinktype {
	BLINK_NONE,     //!< 
	BLINK_LEFT,     //!< 
	BLINK_RIGHT,    //!< 
	BLINK_WARN      //!< 
};

enum { 
	HYDRO_FLAG_SPEED        = BITMASK(1),
	HYDRO_FLAG_DIR          = BITMASK(2),
	HYDRO_FLAG_AILERON      = BITMASK(3),
	HYDRO_FLAG_RUDDER       = BITMASK(4),
	HYDRO_FLAG_ELEVATOR     = BITMASK(5),
	HYDRO_FLAG_REV_AILERON  = BITMASK(6),
	HYDRO_FLAG_REV_RUDDER   = BITMASK(7),
	HYDRO_FLAG_REV_ELEVATOR = BITMASK(8),
};

enum { 
	ANIM_FLAG_AIRSPEED      = BITMASK(1),
	ANIM_FLAG_VVI           = BITMASK(2),
	ANIM_FLAG_ALTIMETER     = BITMASK(3),
	ANIM_FLAG_AOA           = BITMASK(4),
	ANIM_FLAG_FLAP          = BITMASK(5),
	ANIM_FLAG_AIRBRAKE      = BITMASK(6),
	ANIM_FLAG_ROLL          = BITMASK(7),
	ANIM_FLAG_PITCH         = BITMASK(8),
	ANIM_FLAG_THROTTLE      = BITMASK(9),
	ANIM_FLAG_RPM           = BITMASK(10),
	ANIM_FLAG_ACCEL         = BITMASK(11),
	ANIM_FLAG_BRAKE         = BITMASK(12),
	ANIM_FLAG_CLUTCH        = BITMASK(13),
	ANIM_FLAG_TACHO         = BITMASK(14),
	ANIM_FLAG_SPEEDO        = BITMASK(15),
	ANIM_FLAG_PBRAKE        = BITMASK(16),
	ANIM_FLAG_TURBO         = BITMASK(17),
	ANIM_FLAG_SHIFTER       = BITMASK(18),
	ANIM_FLAG_AETORQUE      = BITMASK(19),
	ANIM_FLAG_AEPITCH       = BITMASK(20),
	ANIM_FLAG_AESTATUS      = BITMASK(21),
	ANIM_FLAG_TORQUE        = BITMASK(22),
	ANIM_FLAG_HEADING       = BITMASK(23),
	ANIM_FLAG_DIFFLOCK      = BITMASK(24),
	ANIM_FLAG_STEERING      = BITMASK(25),
	ANIM_FLAG_EVENT         = BITMASK(26),
	ANIM_FLAG_AILERONS      = BITMASK(27),
	ANIM_FLAG_ARUDDER       = BITMASK(28),
	ANIM_FLAG_BRUDDER       = BITMASK(29),
	ANIM_FLAG_BTHROTTLE     = BITMASK(30),
	ANIM_FLAG_PERMANENT     = BITMASK(31),
	ANIM_FLAG_ELEVATORS     = BITMASK(32),
};

enum { 
	ANIM_MODE_ROTA_X        = BITMASK(1),
	ANIM_MODE_ROTA_Y        = BITMASK(2),
	ANIM_MODE_ROTA_Z        = BITMASK(3),
	ANIM_MODE_OFFSET_X      = BITMASK(4),
	ANIM_MODE_OFFSET_Y      = BITMASK(5),
	ANIM_MODE_OFFSET_Z      = BITMASK(6),
	ANIM_MODE_AUTOANIMATE   = BITMASK(7),
	ANIM_MODE_NOFLIP        = BITMASK(8),
	ANIM_MODE_BOUNCE        = BITMASK(9),
};

enum { 
	SHOCK_FLAG_NORMAL       = BITMASK(1),
	SHOCK_FLAG_INVISIBLE    = BITMASK(2),
	SHOCK_FLAG_LACTIVE      = BITMASK(3),
	SHOCK_FLAG_RACTIVE      = BITMASK(4),
	SHOCK_FLAG_ISSHOCK2     = BITMASK(5),
	SHOCK_FLAG_SOFTBUMP     = BITMASK(6),
};

/* some info holding arrays */
static const float flapangles[6] = {0.0, -0.07, -0.17, -0.33, -0.67, -1.0};

/* basic structures */
struct node
{
	Ogre::Vector3 RelPosition; //!< relative to the local physics origin (one origin per truck) (shaky)
	Ogre::Vector3 AbsPosition; //!< absolute position in the world (shaky)
	Ogre::Vector3 Velocity;
	Ogre::Vector3 Forces;
	Ogre::Real inverted_mass;
	Ogre::Real mass;
	Ogre::Vector3 lastNormal;
	int locked;
	int iswheel; //!< 0=no, 1, 2=wheel1  3,4=wheel2, etc...
	int wheelid;
	int masstype;
	int wetstate;
	int contactless;
	int lockednode;
	Ogre::Vector3 lockedPosition; //!< absolute
	Ogre::Vector3 lockedForces;
	Ogre::Vector3 lockedVelocity;
	int contacted;
	Ogre::Real friction_coef;
	Ogre::Real buoyancy;
	Ogre::Real volume_coef;
	Ogre::Real surface_coef;
	Ogre::Vector3 lastdrag;
	Ogre::Vector3 gravimass;
	float wettime;
	bool isHot;
	bool overrideMass;
	bool disable_particles;
	Ogre::Vector3 buoyanceForce;
	int id;
	float colltesttimer;
	Ogre::Vector3 iPosition; //!< initial position, absolute
	Ogre::Real    iDistance; //!< initial distance from node0 during loading - used to check for loose parts
	Ogre::Vector3 smoothpos; //!< absolute, per-frame smooth, must be used for visual effects only
	bool iIsSkin;
	bool isSkin;
	bool contacter;
	int pos;
};

struct shock
{
	int beamid;
	int flags;
	float lastpos;
	float springin;
	float dampin;
	float sprogin;
	float dprogin;
	float springout;
	float dampout;
	float sprogout;
	float dprogout;

};

struct collcab_rate
{
	int rate;
	int distance;
};

struct beam
{
	node_t *p1;
	node_t *p2;
	Beam *p2truck; //!< in case p2 is on another truck
	bool disabled;
	Ogre::Real k; //!< tensile spring
	Ogre::Real d; //!< damping factor
	Ogre::Real L; //!< length
	Ogre::Real minmaxposnegstress;
	int type;
	Ogre::Real maxposstress;
	Ogre::Real maxnegstress;
	Ogre::Real shortbound;
	Ogre::Real longbound;
	Ogre::Real strength;
	Ogre::Real stress;
	int bounded;
	bool broken;
	Ogre::Real plastic_coef;
	Ogre::Real refL; //!< reference length
	Ogre::Real Lhydro;//!< hydro reference len
	Ogre::Real hydroRatio;//!< hydro rotation ratio
	int hydroFlags;
	int animFlags;
	float animOption;
	Ogre::Real commandRatioLong;
	Ogre::Real commandRatioShort;
	Ogre::Real commandShort;
	Ogre::Real commandLong;
	Ogre::Real maxtiestress;
	Ogre::Real diameter;
	Ogre::Vector3 lastforce;
	bool iscentering;
	int isOnePressMode;
	bool isforcerestricted;
	float iStrength; //!< initial strength
	Ogre::Real default_deform;
	Ogre::Real default_plastic_coef;
	int autoMovingMode;
	bool autoMoveLock;
	bool pressedCenterMode;
	float centerLength;
	float minendmass;
	float scale;
	shock_t *shock;
	Ogre::SceneNode *mSceneNode; //!< visual
	Ogre::Entity *mEntity; //!< visual
};

struct soundsource
{
	SoundScriptInstance* ssi;
	int nodenum;
};

struct contacter
{
	int nodeid;
	int contacted;
	int opticontact;
};

struct rigidifier
{
	node_t* a;
	node_t* b;
	node_t* c;
	float k;
	float d;
	float alpha;
	float lastalpha;
	beam_t *beama;
	beam_t *beamc;
};

struct wheel
{
	int nbnodes;
	node_t* nodes[50];
	/**
	 * Defines the braking characteristics of a wheel. Wheels are braked by three mechanisms:
	 * A footbrake, a handbrake/parkingbrake, and directional brakes used for skidsteer steering.
	 * - 0 = no  footbrake, no  handbrake, no  direction control -- wheel is unbraked
	 * - 1 = yes footbrake, yes handbrake, no  direction control
	 * - 2 = yes footbrake, yes handbrake, yes direction control (braked when truck steers to the left)
	 * - 3 = yes footbrake, yes handbrake, yes direction control (braked when truck steers to the right)
	 * - 4 = yes footbrake, no  handbrake, no  direction control -- wheel has footbrake only, such as with the front wheels of a normal car
	 **/
	int braked;
	node_t* arm;
	node_t* near_attach;
	node_t* refnode0;
	node_t* refnode1;
	int propulsed;
	Ogre::Real radius;
	Ogre::Real speed;
	Ogre::Real delta_rotation; //!<  difference in wheel position
	float rp;
	float rp1;
	float rp2;
	float rp3;
	float width;

	// for skidmarks
	Ogre::Vector3 lastContactInner;
	Ogre::Vector3 lastContactOuter;
	float lastSlip;
	int lastContactType;
	ground_model_t *lastGroundModel;
};

struct vwheel
{
	node_t *p1;
	node_t *p2;
	Flexable *fm;
	Ogre::SceneNode *cnode;
	bool meshwheel;
};

struct hook
{
	int locked;
	int group;
	bool lockNodes;
	node_t *hookNode;
	node_t *lockNode;
	Beam *lockTruck;
};

struct ropable
{
	node_t *node;
	int group;
	bool multilock;
	int used;
};

struct rope
{
	int locked;
	int group;
	beam_t *beam;
	node_t *lockedto;
	ropable_t *lockedto_ropable;
	Beam *lockedtruck;
};


struct tie
{
	beam_t *beam;
	ropable_t *lockedto;
	int group;
	bool tied;
	bool tying;
	float commandValue;
};


struct wing
{
	FlexAirfoil *fa;
	Ogre::SceneNode *cnode;
};

struct command
{
	float commandValue;
	std::vector<int> beams;
	std::vector<int> rotators;
	Ogre::String description;
};

struct rotator
{
	int nodes1[4];
	int nodes2[4];
	int axis1; //rot axis
	int axis2;
	float angle;
	float rate;
};

struct flare
{
	int noderef;
	int nodex;
	int nodey;
	float offsetx;
	float offsety;
	float offsetz;
	Ogre::SceneNode *snode;
	Ogre::BillboardSet *bbs;
	Ogre::Light *light;
	char type;
	int controlnumber;
	bool controltoggle_status;
	float blinkdelay;
	float blinkdelay_curr;
	bool blinkdelay_state;
	float size;
	bool isVisible;
};

struct prop
{
	int noderef;
	int nodex;
	int nodey;
	float offsetx;
	float offsety;
	float offsetz;
	float rotaX;
	float rotaY;
	float rotaZ;
	float orgoffsetX;
	float orgoffsetY;
	float orgoffsetZ;
	Ogre::Quaternion rot;
	Ogre::SceneNode *snode;
	Ogre::SceneNode *wheel;
	Ogre::Vector3 wheelpos;
	int mirror;
	char beacontype;
	Ogre::BillboardSet *bbs[4];
	Ogre::SceneNode *bbsnode[4];
	Ogre::Light *light[4];
	float brate[4];
	float bpos[4];
	int pale;
	int spinner;
	bool animated;
	float anim_x_Rot;
	float anim_y_Rot;
	float anim_z_Rot;
	float anim_x_Off;
	float anim_y_Off;
	float anim_z_Off;
	float animratio[10];
	int animFlags[10];
	int animMode[10];
	float animOpt1[10];
	float animOpt2[10];
	float animOpt3[10];
	float animOpt4[10];
	float animOpt5[10];
	int animKey[10];
	int animKeyState[10];
	int lastanimKS[10];
	Ogre::Real wheelrotdegree;
};

struct exhaust
{
	int emitterNode;
	int directionNode;
	char material[255];
	float factor;
	bool isOldFormat;
	Ogre::SceneNode *smokeNode;
	Ogre::ParticleSystem* smoker;
};


struct cparticle
{
	int emitterNode;
	int directionNode;
	bool active;
	Ogre::SceneNode *snode;
	Ogre::ParticleSystem* psys;
};


struct debugtext
{
	int id;
	Ogre::MovableText *txt;
	Ogre::SceneNode *node;
};

struct rig
{
	node_t nodes[MAX_NODES];
	int free_node;

	beam_t beams[MAX_BEAMS];
	int free_beam;

	contacter_t contacters[MAX_CONTACTERS];
	int free_contacter;

	rigidifier_t rigidifiers[MAX_RIGIDIFIERS];
	int free_rigidifier;

	wheel_t wheels[MAX_WHEELS];
	vwheel_t vwheels[MAX_WHEELS];
	int free_wheel;

	std::vector <rope_t> ropes;
	std::vector <ropable_t> ropables;
	std::vector <tie_t> ties;
	std::vector <hook_t> hooks;

	wing_t wings[MAX_WINGS];
	int free_wing;
		
	command_t commandkey[MAX_COMMANDS + 1];
	
	rotator_t rotators[MAX_ROTATORS];
	int free_rotator;

	std::vector<flare_t> flares;
	int free_flare;

	prop_t props[MAX_PROPS];
	prop_t *driverSeat;
	int free_prop;
	
	shock_t shocks[MAX_SHOCKS];
	int free_shock;
	int free_active_shock; // this has no array associated with it. its just to determine if there are active shocks!

	std::vector < exhaust_t > exhausts;

	cparticle_t cparticles[MAX_CPARTICLES];
	int free_cparticle;
	
	std::vector<debugtext_t>nodes_debug, beams_debug;
	
	soundsource_t soundsources[MAX_SOUNDSCRIPTS_PER_TRUCK];
	int free_soundsource;
	
	int pressure_beams[MAX_PRESSURE_BEAMS];
	int free_pressure_beam;

	int free_aeroengine;
	AeroEngine *aeroengines[MAX_AEROENGINES];

	int free_screwprop;
	Screwprop *screwprops[MAX_SCREWPROPS];

	int cabs[MAX_CABS*3];
	int subisback[MAX_SUBMESHES];
	int free_cab;

	int hydro[MAX_HYDROS];
	int free_hydro;

	Ogre::Vector3 texcoords[MAX_TEXCOORDS];
	int free_texcoord;

	int subtexcoords[MAX_SUBMESHES];
	int subcabs[MAX_SUBMESHES];
	int free_sub;

	int collcabs[MAX_CABS];
	int collcabstype[MAX_CABS];
	collcab_rate_t collcabrate[MAX_CABS];
	int free_collcab;

	int buoycabs[MAX_CABS];
	int buoycabtypes[MAX_CABS];
	int free_buoycab;

	int free_airbrake;
	Airbrake *airbrakes[MAX_AIRBRAKES];

	Skidmark *skidtrails[MAX_WHEELS*2];
	bool useSkidmarks;

	FlexBody *flexbodies[MAX_FLEXBODIES];
	int free_flexbody;

};

// some non-beam structs

struct collision_box
{
	//absolute collision box
	float lo_x;
	float hi_x;
	float lo_y;
	float hi_y;
	float lo_z;
	float hi_z;
	bool refined;
	//rotation
	Ogre::Quaternion rot;
	Ogre::Quaternion unrot;
	//center of rotation
	Ogre::Vector3 center;
	//relative collision box
	float relo_x;
	float rehi_x;
	float relo_y;
	float rehi_y;
	float relo_z;
	float rehi_z;
	//self rotation
	bool selfrotated;
	Ogre::Vector3 selfcenter;
	Ogre::Quaternion selfrot;
	Ogre::Quaternion selfunrot;
	int eventsourcenum;
	bool virt;
	bool camforced;
	Ogre::Vector3 campos;
	int event_filter;
};

struct ground_model
{
	float va;                       //!< adhesion velocity
	float ms;                       //!< static friction coefficient
	float mc;                       //!< sliding friction coefficient
	float t2;                       //!< hydrodynamic friction (s/m)
	float vs;                       //!< stribeck velocity (m/s)
	float alpha;                    //!< steady-steady
	float strength;                 //!< ground strength

	float fluid_density;            //!< Density of liquid
	float flow_consistency_index;   //!< general drag coefficient

	//! if flow_behavior_index<1 then liquid is Pseudoplastic (ketchup, whipped cream, paint)
	//! if =1 then liquid is Newtoni'an fluid
	//! if >1 then liquid is Dilatant fluid (less common)
	float flow_behavior_index;

	
	float solid_ground_level;       //!< how deep the solid ground is
	float drag_anisotropy;          //!< Upwards/Downwards drag anisotropy

	int fx_type;
	Ogre::ColourValue fx_colour;
	char name[255];
	char basename[255];
	char particle_name[255];

	int fx_particle_amount;         //!< amount of particles

	float fx_particle_min_velo;     //!< minimum velocity to display sparks
	float fx_particle_max_velo;     //!< maximum velocity to display sparks
	float fx_particle_fade;         //!< fade coefficient
	float fx_particle_timedelta;    //!< delta for particle animation
	float fx_particle_velo_factor;  //!< velocity factor
	float fx_particle_ttl;
};

struct client
{
	bool          used;                 //!< if this slot is used already
	int           trucknum;             //!< the truck's number of the truck bound to this slot
	int           slotnum;              //!< server slot number
	bool          loaded;               //!< if the truck bound to this slot is loaded
	bool          invisible;            //!< if the truck is invisible for the user (since he dont have the required resource)

	char          client_version[10];   //!< clients version, i.e. RoR-0.36.1
	char          protocol_version[10]; //!< protocol version, i.e. RoRNet-3.0
	char          truck_name[255];      //!< the truck filename, i.e. agoras.truck
	unsigned int  truck_size;           //!< the size truck buffer needed
	char          user_language[10];    //!< the users language
	char          user_name[20];        //!< the users nickname
	unsigned int  user_id;              //!< the users id, set by the server
	unsigned int  user_authlevel;       //!< the users authorization level.
	int  colournum;                     //!< the users authorization colour
};

struct authorinfo
{
	int id;
	char type[255];
	char name[255];
	char email[255];
};
#endif //BEAMDATA_H__
