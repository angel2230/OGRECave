/*
-----------------------------------------------------------------------------
This source file is part of OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2005 The OGRE Team
Also see acknowledgements in Readme.html

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/
#ifndef __CompositorManager_H__
#define __CompositorManager_H__

#include "OgrePrerequisites.h"
#include "OgreResourceManager.h"
#include "OgreCompositor.h"
#include "OgreRectangle2D.h"
namespace Ogre {
    /** Class for managing Compositor settings for Ogre. Compositors provide the means 
        to flexibly "composite" the final rendering result from multiple scene renders
        and intermediate operations like rendering fullscreen quads. This makes 
        it possible to apply postfilter effects, HDRI postprocessing, and shadow 
        effects to a Viewport.
        @par
            When loaded from a script, a Compositor is in an 'unloaded' state and only stores the settings
            required. It does not at that stage load any textures. This is because the material settings may be
            loaded 'en masse' from bulk material script files, but only a subset will actually be required.
        @par
            Because this is a subclass of ResourceManager, any files loaded will be searched for in any path or
            archive added to the resource paths/archives. See ResourceManager for details.
    */
    class _OgreExport CompositorManager : public ResourceManager, public Singleton<CompositorManager>
    {
    public:
        CompositorManager();
        virtual ~CompositorManager();
        
        /// Overridden from ResourceManager
        Resource* createImpl(const String& name, ResourceHandle handle, 
            const String& group, bool isManual, ManualResourceLoader* loader,
            const NameValuePairList* params);
            
        /** Intialises the Compositor manager, which also triggers it to 
            parse all available .compositor scripts. */
        void initialise(void);
        
        /** @see ScriptLoader::parseScript
        */
        void parseScript(DataStreamPtr& stream, const String& groupName);

        /** Get the compositor chain for a Viewport. If there is none yet, a new 
			compositor chain is registered.
			XXX We need a _notifyViewportRemoved to find out when this viewport disappears,
			so we can destroy its chain as well.
        */
        CompositorChain *getCompositorChain(Viewport *vp);

		/** Add a compositor to a viewport. By default, it is added to end of the chain,
			after the other compositors.
			@param vp			Viewport to modify
			@param compositor	The name of the compositor to apply
			@param addPosition	At which position to add, defaults to the end (-1).
			@returns pointer to instance, or 0 if it failed.
		*/
		CompositorInstance *addCompositor(Viewport *vp, const String &compositor, int addPosition=-1);

		/** Remove a compositor from a viewport
		*/
		void removeCompositor(Viewport *vp, const String &compositor);

		/** Set the state of a compositor on a viewport to enabled or disabled. 
			Disabling a compositor stops it from rendering but does not free any resources. 
			This can be more efficient than using removeCompositor and addCompositor in cases
			the filter is switched on and off a lot.
		*/
		void setCompositorEnabled(Viewport *vp, const String &compositor, bool value);

		/** Get a textured fullscreen 2D rectangle, for internal use.
		*/
		Renderable *_getTexturedRectangle2D();
    private:
        typedef std::map<Viewport*, CompositorChain*> Chains;
        Chains mChains;
        
        /** Clear composition chains for all viewports
         */
        void freeChains();

		Rectangle2D *mRectangle;
    };
}

#endif