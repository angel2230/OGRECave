/*
-----------------------------------------------------------------------------
This source file is part of LEXIExporter

Copyright 2006 NDS Limited

Author(s):
Mark Folkenberg,
Bo Krohn

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

#include "LexiIntermediateAPI.h"

class COgreMaterialCompiler
{
public:

	COgreMaterialCompiler( CIntermediateMaterial* pIntermediateMaterial, Ogre::String sExtension="", bool bExportColours=false );
	virtual ~COgreMaterialCompiler();

	bool				WriteOgreMaterial( const Ogre::String& sFilename );
	Ogre::MaterialPtr	GetOgreMaterial( void );
	void				CopyTextureMaps( Ogre::String outPath, Ogre::String extension );
	void				CopyShaderSources( Ogre::String outPath );
protected:

	void	InitializeOgreComponents( void );

private:

	void	CreateOgreMaterial( void );
	void	ParseMaterialMaps( Ogre::Pass* pass );
	void	CreateTextureUnits( Ogre::Pass* pass, STextureMapInfo texInfo );

	void	SetBlendingByOpacity(Ogre::Pass* pass);
	void	SetCommonVertexProgramParameters(Ogre::Pass* pass, Ogre::GpuProgramParametersSharedPtr params );
	void	SetCommonFragmentProgramParameters(Ogre::Pass* pass, Ogre::GpuProgramParametersSharedPtr params );

	void	CreatePureBlinn( Ogre::Pass* pass);
	void	CreateDiffuse(  Ogre::Pass* pass );
	void	CreateSpecularColor(  Ogre::Pass* pass );
	void	CreateSpecularLevel(  Ogre::Pass* pass );
	void	CreateSelfIllumination(  Ogre::Pass* pass );
	void	CreateDiffuseAndOpacity( Ogre::Pass* pass );
	void	CreateDiffuseAndSpecularLevel( Ogre::Pass* pass );

	static int doFileCopy(Ogre::String inFile, Ogre::String outFile);

	Ogre::MaterialPtr m_pOgreMaterial;
	CIntermediateMaterial* m_pIMaterial;

	Ogre::String m_sExtensionOverride;

	bool 	m_bReindex;
	bool 	m_bExportNormals;
	bool 	m_bExportColours;
	bool 	m_bExportTexUVs;

	bool	m_bShadersSupported;

};
