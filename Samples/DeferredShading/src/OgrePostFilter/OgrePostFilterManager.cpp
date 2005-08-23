// OgrePostFilterManager
// Manuel Bua
//
// $HeadURL: svn://localhost/OgreDev/testBed/OgrePostFilter/OgrePostFilterManager.cpp $
// $Id$

#include "OgrePostFilterManager.h"

OgrePostFilterManager::OgrePostFilterManager( Root* anOgreRoot,
											  RenderTarget* aRenderWindow,
											  SceneManager* aSceneManager,
											  SceneNode* aMainSceneNode,
											  Camera* aCamera )
											  : iRenderer(0)
{

	iShared = new OgrePostFilterShared();

	// partially setup shared data
	iShared->iRoot = anOgreRoot;
	iShared->iWindow = aRenderWindow;
	iShared->iCamera = aCamera;
	iShared->iSceneManager = aSceneManager;
	iShared->iMainSceneNode = aMainSceneNode;

	iRenderer = new OgrePostFilterRenderer( iShared );

}

OgrePostFilterManager::~OgrePostFilterManager() {

	delete iRenderer;
	delete iShared;

}

void OgrePostFilterManager::setPostFilter( OgrePostFilter* aPostFilter ) {

	iRenderer->setPostFilter( aPostFilter );

}