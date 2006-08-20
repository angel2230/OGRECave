//////////////////////////////////////////////////////////////////////////
//																		//
//	File: ogretest.cpp													//
//																		//
//  Description: simple application demonstrating plane optimal			//
//	shadow mapping.  This means for specified planes of interest,		//
//	we can set up the shadow map such that samples on the geometry		//
//	are perfectly sampled (one shadow map sample per pixel on plane).	//
//	The power of this idea is demonstrated with a "dueling frusta"		//
//	setup.																//
//  The example is based on the CustomShadows.cpp demo.					//
//	For simplicity and for demonstrating lack of pronounced				//
//	artifacts, shaders do not do percentage closest filtering			//
//	(although copying the shaders from CustomShadowsShaders will		//
//	enable it).															//
//  This application also shows how to create an application not		//
//  using ExampleApplication.h											//
//	_________________________________________________________________	//
//																		//
//	Author: Hamilton Chong (hamilton.chong@gmail.com)					//
//	Based off mini Ogre Application by Mythma (mythma@126.com).			//
//																		//
//////////////////////////////////////////////////////////////////////////


// I N C L U D E S ///////////////////////////////////////////////////////

#include "Ogre.h" 
#include "OgreFrameListener.h" 
#include "OgreEventListeners.h" 
#include "OgreKeyEvent.h" 

using namespace Ogre; 


// G L O B A L S /////////////////////////////////////////////////////////

static SceneManager*	sceneMgr		= 0;	// scene manager
static Camera*			camera			= 0;	// view camera (eye)
static Viewport*		viewPort		= 0;	// viewport
static Light *			spotlight		= 0;	// spot light
static Matrix4			camRBT;					// camera rigid body transform
static Matrix4			lightRBT;				// light rigid body transform


// C L A S S E S /////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////// 
// class CSimpleFrameListener 
////////////////////////////////////////////////////////////////////////// 
class CSimpleFrameListener : public FrameListener 
{ 
public: 

	CSimpleFrameListener(InputReader* inputReader) 
	{ 
		mInputReader = inputReader; 
	} 
	
	bool frameStarted(const FrameEvent& evt) 
	{ 
		mInputReader->capture(); 

		//exit if key KC_ESCAPE pressed 
		if(mInputReader->isKeyDown(KC_ESCAPE)) 
			return false; 

		// == TODO: Insert application's code here!!! ===================


		// ==============================================================

		return true; 
	} 
	

	bool frameEnded(const FrameEvent& evt) 
	{ 
		return true; 
	} 
private: 
	InputReader* mInputReader; 
}; 

////////////////////////////////////////////////////////////////////////// 
// class CSimpleKeyListener 
////////////////////////////////////////////////////////////////////////// 
class CSimpleKeyListener : public KeyListener 
{ 
public: 
	void keyClicked(KeyEvent* e){} // key up and down

	void keyPressed(KeyEvent* e) // key down
	{
		switch(e->getKey())
		{
		default:
			// example interaction
			camera->moveRelative(Vector3(50.0, 0.0, 0.0));
			break;
		}
	}

	void keyReleased(KeyEvent* e) {} // key up
}; 

////////////////////////////////////////////////////////////////////////// 
// class CSimpleMouseListener 
////////////////////////////////////////////////////////////////////////// 
class CSimpleMouseListener : public MouseListener, public MouseMotionListener
{ 
private:
	bool	mLeftDown;
	bool	mRightDown;

public: 

	void mousePressed(Ogre::MouseEvent *e) 
	{
		int buttonID = e->getButtonID();
		if ( buttonID & InputEvent::BUTTON0_MASK )
		{
			// left button
			mLeftDown = true;
		}
		if ( buttonID & MouseEvent::BUTTON1_MASK )
		{
			// right button
			mRightDown = true;
		}
	}

	void mouseReleased(Ogre::MouseEvent *e) 
	{
		int buttonID = e->getButtonID();
		if ( buttonID & InputEvent::BUTTON0_MASK )
		{
			// left button
			mLeftDown = false;
		}
		if ( buttonID & MouseEvent::BUTTON1_MASK )
		{
			// right button
			mRightDown = false;
		}
	}


	// MouseMotionListener

	void mouseMoved(Ogre::MouseEvent *e) {}

	void mouseDragged(Ogre::MouseEvent *e) 
	{
		// Let A = matrix for auxillary frame with respect to which we perform an action
		// Let O = matrix for Object's frame (object upon which we act)
		// Let O' = matrix for updated object's frame
		// Let Q = matrix representing action we wish to perform
		// O' = A Q A^-1 O
		// TODO: one could choose Q such that we get an arcball interface, but those are
		//  details that distract from our main purpose here.

		static const Real MOUSE_SPEED = 30.0;
		if (mLeftDown)
		{
			Real dragx = e->getRelX();
			Real dragy = e->getRelY();
			
			Vector3 disp(dragx, dragy, 0.0);
			Vector3 axis = Vector3(0,0,1).crossProduct(disp);
			Real degrees = axis.normalise();

			Quaternion rot(Radian(degrees * 3.14159 / 180.0 * MOUSE_SPEED), axis);
			
			Matrix4 rotMat;
			Matrix4 auxFrame;
			auxFrame.makeTransform(Vector3(0,0,0), Vector3(1,1,1), camRBT.extractQuaternion());
			Matrix4 auxInverse = auxFrame.inverse();
			rotMat.makeTransform(Vector3(0,0,0), Vector3(1,1,1), rot);
			camRBT = auxFrame * rotMat * auxInverse * camRBT;
			Vector4 camPt = camRBT * Vector4(0,0,0,1);
			camera->setPosition(camPt.x, camPt.y, camPt.z);
			camera->lookAt(0.0, 0.0, 0.0);
			camRBT.makeTransform(Vector3(camPt.x, camPt.y, camPt.z), Vector3(1,1,1), camera->getOrientation());
		}

		if (mRightDown)
		{
			Real dragx = e->getRelX();
			Real dragy = e->getRelY();
			
			Vector3 disp(dragx, dragy, 0.0);
			Vector3 axis = Vector3(0,0,1).crossProduct(disp);
			Real degrees = axis.normalise();

			Quaternion rot(Radian(degrees * 3.14159 / 180.0 * MOUSE_SPEED), axis);
			
			Matrix4 rotMat;
			Matrix4 auxFrame;
			auxFrame.makeTransform(Vector3(0,0,0), Vector3(1,1,1), camRBT.extractQuaternion());
			Matrix4 auxInverse = auxFrame.inverse();
			rotMat.makeTransform(Vector3(0,0,0), Vector3(1,1,1), rot);
			lightRBT = auxFrame * rotMat * auxInverse * lightRBT;
			Vector4 lightPt = lightRBT * Vector4(0,0,0,1);
			Vector3 dir(-lightPt.x, -lightPt.y, -lightPt.z);
			dir.normalise();
			spotlight->setPosition(lightPt.x, lightPt.y, lightPt.z);
			spotlight->setDirection(dir.x, dir.y, dir.z);
		}
	}


	// Legacy/Defunct functions; leave empty
	void mouseClicked(Ogre::MouseEvent*) {}
	void mouseEntered(Ogre::MouseEvent*) {}
	void mouseExited(Ogre::MouseEvent*) {}
}; 


// M A I N ///////////////////////////////////////////////////////////////

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32 
#define WIN32_LEAN_AND_MEAN 
#include "windows.h" 
INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR strCmdLine, INT) 
#else 
int main(int argc, char **argv) 
#endif 

{ 
	//----------------------------------------------------- 
	// 1 enter ogre 
	//----------------------------------------------------- 
	Root* root = new Root; 

	//----------------------------------------------------- 
	// 2 configure resource paths
	//----------------------------------------------------- 
        // Load resource paths from config file

        // File format is:
        //  [ResourceGroupName]
        //  ArchiveType=Path
        //  .. repeat
        // For example:
        //  [General]
        //  FileSystem=media/
        //  Zip=packages/level1.zip

        ConfigFile cf;
        cf.load("resources.cfg");

        // Go through all sections & settings in the file
        ConfigFile::SectionIterator seci = cf.getSectionIterator();

        String secName, typeName, archName;
        while (seci.hasMoreElements())
        {
            secName = seci.peekNextKey();
            ConfigFile::SettingsMultiMap *settings = seci.getNext();
            ConfigFile::SettingsMultiMap::iterator i;
            for (i = settings->begin(); i != settings->end(); ++i)
            {
                typeName = i->first;
                archName = i->second;
                ResourceGroupManager::getSingleton().addResourceLocation(
                    archName, typeName, secName);
            }
        }
	//----------------------------------------------------- 
	// 3 Configures the application 
	//----------------------------------------------------- 
	if(!root->showConfigDialog())
            return false;                                 // Exit the application on cancel
	root->initialise(true, "Plane Optimal Shadows Demo Application"); 

    ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

	//----------------------------------------------------- 
	// 4 Create the SceneManager
	// 
	//		ST_GENERIC = octree
	//		ST_EXTERIOR_CLOSE = simple terrain
	//		ST_EXTERIOR_FAR = nature terrain (depreciated)
	//		ST_EXTERIOR_REAL_FAR = paging landscape
	//		ST_INTERIOR = Quake3 BSP
	//----------------------------------------------------- 
	sceneMgr = root->createSceneManager(ST_GENERIC); 

	//-----------------------------------------------------
	// 5 Create the scene
	//-----------------------------------------------------
	// create the lights; most of the properties are superfluous since our custom shaders
	// will be determining the eventual look
	sceneMgr->setAmbientLight(ColourValue(0.0,0.0,0.0));
	lightRBT.makeTransform(Vector3(0,300,0), Vector3(1,1,1), Quaternion(Radian(0.0),Vector3(1,0,0)));
	Vector4 lightPt = lightRBT * Vector4(0,0,0,1);
	Vector3 dir(-lightPt.x, -lightPt.y, -lightPt.z);
	dir.normalise();
	spotlight = sceneMgr->createLight( "SpotLight" );
	spotlight->setType( Light::LT_SPOTLIGHT );
	spotlight->setDiffuseColour( 0.75, 0.8, 1.0 );
	spotlight->setSpecularColour( 0.75, 0.8, 1.0 );
	spotlight->setPosition(lightPt.x, lightPt.y, lightPt.z);
	spotlight->setDirection(dir.x, dir.y, dir.z);
	spotlight->setSpotlightRange( Degree(30), Degree(40) );

	// create ogre head
	Entity *ogreHead = sceneMgr->createEntity("OgreHead", "ogrehead.mesh");
	SceneNode *ogreNode = sceneMgr->getRootSceneNode()->createChildSceneNode("OgreNode");
	ogreNode->attachObject(ogreHead);
	
	// create ground plane
	Plane plane( Vector3::UNIT_Y, -80.0 );
	MeshManager::getSingleton().createPlane("plane.mesh",
           ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, plane,
           1000,1000,200,200,true,1,5,5,Vector3::UNIT_Z);
	Entity *ground = sceneMgr->createEntity( "Ground", "plane.mesh" );
	SceneNode *groundNode = sceneMgr->getRootSceneNode()->createChildSceneNode("GroundNode");
	groundNode->attachObject(ground);
	ground->setMaterialName("Examples/Rockwall");
    ground->setCastShadows(false);

	// create a dummy node; could unify with ground plane or keep separate to allow for optimizing
	// other planes of interest.
	MovablePlane *dummyPlane = new MovablePlane("DummyPlane");
	dummyPlane->normal = plane.normal;
	dummyPlane->d = plane.d;
	PlaneOptimalShadowCameraSetup *planeOptShadowCamera = new PlaneOptimalShadowCameraSetup(dummyPlane);
	Entity *dummyPlaneEntity = sceneMgr->createEntity( "DummyPlane", "plane.mesh" );
	dummyPlaneEntity->setVisible(false);
	dummyPlaneEntity->setCastShadows(false);
	SceneNode *dummyPlaneNode = sceneMgr->getRootSceneNode()->createChildSceneNode("DummyPlaneNode");
	dummyPlaneNode->attachObject(dummyPlaneEntity);
	SharedPtr<ShadowCameraSetup> planeOptPtr(planeOptShadowCamera);
	spotlight->setCustomShadowCameraSetup(planeOptPtr);
	
	// Use Ogre's custom shadow mapping ability
	sceneMgr->setShadowTexturePixelFormat(PF_FLOAT32_R);
	sceneMgr->setShadowTechnique( SHADOWTYPE_TEXTURE_ADDITIVE );
	sceneMgr->setShadowTextureCasterMaterial("PlaneOptimalShadows/ShadowCaster");
	sceneMgr->setShadowTextureReceiverMaterial("PlaneOptimalShadows/ShadowReceiver");
	sceneMgr->setShadowTextureSelfShadow(true);	// NOTE: need to perform depth biasing for this to work
	// Ideally, we'd use a rectangular texture exactly the size of the screen so we don't have to waste
	// samples by rounding dimensions up to the max power of two.  Exact size match between shadow map
	// and screen also guarantees that samples are matched exactly 1-1 (no aliasing at all).
	// Tweaks to try: can change the resolution here to see that for lower resolutions, aliasing is spread
	// around uniformly in pixel space, even for dueling frusta.  Don't forget to change the texture size
	// variables in the material script (defines input values for shaders).
	sceneMgr->setShadowTextureSize(1024);


	//----------------------------------------------------- 
	// 6 Create the camera 
	//----------------------------------------------------- 
	camera = sceneMgr->createCamera("SimpleCamera");
	camRBT.makeTransform(Vector3(0,0,500), Vector3(1,1,1), Quaternion(Radian(0),Vector3(0,1,0)));
	Vector4 camPt = camRBT * Vector4(0,0,0,1);
	camera->setPosition(camPt.x, camPt.y, camPt.z);
	camera->lookAt(0.0, 0.0, 0.0);
	camera->setNearClipDistance(5);
	camera->setFarClipDistance(5000.0);
	camera->setAutoAspectRatio(true);

	//----------------------------------------------------- 
	// 7 Create one viewport, entire window 
	//----------------------------------------------------- 
	viewPort = root->getAutoCreatedWindow()->addViewport(camera);
	viewPort->setBackgroundColour(ColourValue(0.0, 0.3, 0.8));

	//---------------------------------------------------- 
	// 8 add event processor 
	//----------------------------------------------------
	EventProcessor* eventProcessor = new EventProcessor(); 
	eventProcessor->initialise(root->getAutoCreatedWindow()); 
	eventProcessor->startProcessingEvents(); 
	// event engine is running, now we listen for keys and frames (replaces while loops)
	CSimpleKeyListener* keyListener = new CSimpleKeyListener(); 
	eventProcessor->addKeyListener(keyListener); 
	CSimpleFrameListener* frameListener = new CSimpleFrameListener(
		eventProcessor->getInputReader());
	// you can have many frameListeners
	root->addFrameListener(frameListener); 
	CSimpleMouseListener *mouseListener = new CSimpleMouseListener();
	eventProcessor->addMouseListener(mouseListener);	
    eventProcessor->addMouseMotionListener(mouseListener);

	//----------------------------------------------------
	// 9 start rendering 
	//----------------------------------------------------
	root->startRendering(); // blocks until a frame listener returns false. eg from pressing escape in this example

	//----------------------------------------------------
	// 10 clean 
	//----------------------------------------------------
	delete dummyPlane;
	delete frameListener; 
	delete keyListener; 
	delete eventProcessor; 
	delete root;    

	return 0; 
}