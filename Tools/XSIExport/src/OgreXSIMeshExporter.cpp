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
#include "OgreXSIMeshExporter.h"
#include <xsi_model.h>
#include <xsi_primitive.h>
#include <xsi_polygonnode.h>
#include <xsi_material.h>
#include <xsi_vertex.h>
#include <xsi_trianglevertex.h>
#include <xsi_cluster.h>
#include <xsi_kinematics.h>
#include <xsi_kinematicstate.h>
#include <xsi_selection.h>

#include "OgreException.h"
#include "OgreXSIHelper.h"
#include "OgreLogManager.h"
#include "OgreMeshManager.h"
#include "OgreMesh.h"
#include "OgreSubMesh.h"
#include "OgreMeshManager.h"
#include "OgreDefaultHardwareBufferManager.h"
#include "OgreMeshSerializer.h"

using namespace XSI;


namespace Ogre {
    //-----------------------------------------------------------------------
    XsiMeshExporter::UniqueVertex::UniqueVertex()
        : position(Vector3::ZERO), normal(Vector3::ZERO), colour(0), nextIndex(0)
    {
        for (int i = 0; i < OGRE_MAX_TEXTURE_COORD_SETS; ++i)
            uv[i] = Vector3::ZERO;
    }
    //-----------------------------------------------------------------------
    bool XsiMeshExporter::UniqueVertex::operator==(const UniqueVertex& rhs) const
    {
        bool ret = position == rhs.position && 
            normal == rhs.normal && 
            colour == rhs.colour;
        if (!ret) return ret;

        for (int i = 0; i < OGRE_MAX_TEXTURE_COORD_SETS && ret; ++i)
        {
            ret = ret && (uv[i] == rhs.uv[i]);
        }

        return ret;
        

    }
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    XsiMeshExporter::XsiMeshExporter()
    {
    }
    //-----------------------------------------------------------------------
    XsiMeshExporter::~XsiMeshExporter()
    {
    }
    //-----------------------------------------------------------------------
	void XsiMeshExporter::exportMesh(const XSI::CString& fileName, 
		bool mergeSubMeshes, bool exportChildren, 
		bool edgeLists, bool tangents)
    {
        LogManager logMgr;
		logMgr.createLog("OgreXSIExporter.log", true);
		ResourceGroupManager rgm;
        MeshManager *meshMgr = new MeshManager();
        DefaultHardwareBufferManager *hardwareBufMgr = new DefaultHardwareBufferManager();

        logMgr.createLog("OgreXSIExport.log", true);

        // Derive the scene root
        X3DObject sceneRoot(mXsiApp.GetActiveSceneRoot());

        // Construct mesh
        MeshPtr pMesh = MeshManager::getSingleton().createManual("XSIExport", 
			ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

		// Find all PolygonMesh objects
		buildPolygonMeshList(exportChildren);

		// write the data into a mesh
		buildMesh(pMesh.getPointer(), mergeSubMeshes);



        if(edgeLists)
        {
            mXsiApp.LogMessage(L"Calculating edge lists");
            pMesh->buildEdgeList();
        }

        if(tangents)
        {
            mXsiApp.LogMessage(L"Calculating tangents");
            unsigned short src, dest;
            if (pMesh->suggestTangentVectorBuildParams(src, dest))
            {
                pMesh->buildTangentVectors(src, dest);
            }
            else
            {
                mXsiApp.LogMessage(L"Could not derive tangents parameters");
            }

        }

        MeshSerializer serializer;
        serializer.exportMesh(pMesh.getPointer(), XSItoOgre(fileName));

		cleanupPolygonMeshList();
		delete meshMgr;
        delete hardwareBufMgr;
    }
	//-----------------------------------------------------------------------
	void XsiMeshExporter::buildMesh(Mesh* pMesh, bool mergeSubmeshes)
	{
		/* Iterate over the list of polygon meshes that we've already located.
			For each one:
				If we're not merging submeshes, bake any protosubmeshes built
				  into the mesh and clear the protosubmesh list
			    Scan the clusters for 'poly' clusters (which can contain material
				  discrepancies). Any that use a different material should be
				  noted in the polycluster list
			    Build ProtoSubMeshes by iterating over the triangles in the mesh, 
				  building the list of UniqueVertices against the material as we 
				  go. We check each triangle to see if the polygon index is in 
				  the list of polyclusters & if so it goes into the other lists
		    
			Finally, we bake any remaining protosubmeshes into submeshes.
		*/
		for (PolygonMeshList::iterator pm = mXsiPolygonMeshList.begin();
			pm != mXsiPolygonMeshList.end(); ++pm)
		{
			// build contents of this polymesh into ProtoSubMesh(es)
			processPolygonMesh(pMesh, *pm);

			if (!mergeSubmeshes)
			{
				// export out at the end of every PolygonMesh
				exportProtoSubMeshes(pMesh);
			}
		}
		if (mergeSubmeshes)
		{
			// export out the combined result
			exportProtoSubMeshes(pMesh);
		}
	}
	//-----------------------------------------------------------------------
	XsiMeshExporter::ProtoSubMesh* XsiMeshExporter::createOrRetrieveProtoSubMesh(
		const String& materialName, const String& name, 
		TextureCoordDimensionList& texCoordDims, bool hasVertexColours)
	{
		bool createNew = false;
		ProtoSubMesh* ret = 0;
		
		ProtoSubMeshList::iterator pi = mProtoSubmeshList.find(materialName);
		if (pi == mProtoSubmeshList.end())
		{
			createNew = true;
		}
		else
		{
			// Check format is compatible
			bool compat = true;
			if (pi->second->textureCoordDimensions.size() != texCoordDims.size())
			{
				compat = false;
			}
			if (pi->second->hasVertexColours != hasVertexColours)
			{
				compat = false;
			}
			std::vector<ushort>::iterator t = texCoordDims.begin();
			std::vector<ushort>::iterator u = pi->second->textureCoordDimensions.begin(); 
			for (;t != texCoordDims.end(); ++t,++u)
			{
				if (*t != *u)
				{
					compat = false;
					break;
				}
			}
			
			if (compat)
			{
				ret = pi->second;
			}
			else
			{
				// Can't merge these - create new
				createNew = true;
			}
		}

		if (createNew)
		{
			ret = new ProtoSubMesh();
			mProtoSubmeshList[materialName] = ret;
			ret->materialName = materialName;
			ret->name = name;
			ret->textureCoordDimensions = texCoordDims;
			ret->hasVertexColours = hasVertexColours;
		}

		return ret;
		
	}
	//-----------------------------------------------------------------------
	void XsiMeshExporter::processPolygonMesh(Mesh* pMesh, PolygonMeshEntry* xsiMesh)
	{
		// Pre-process the mesh
		if (!preprocessPolygonMesh(xsiMesh))
			return;
		
        // Retrieve all the XSI relevant summary info
        CPointRefArray pointArray(xsiMesh->mesh.GetPoints());
        MATH::CVector3Array srcPosArray = pointArray.GetPositionArray();
        CPolygonNodeRefArray nodeArray(xsiMesh->mesh.GetNodes());
        MATH::CVector3Array srcNormArray = nodeArray.GetNormalArray();
        CTriangleRefArray triArray = xsiMesh->mesh.GetTriangles();

#if _DEBUG
		StringUtil::StrStreamType msg;
		msg << "Points: " << pointArray.GetCount() << std::endl;
		msg << "Triangles: " << triArray.GetCount() << std::endl;
		msg << "Normals: " << srcNormArray.GetCount() << std::endl;
		msg << "Num UVs: " << mCurrentTextureCoordDimensions.size() << std::endl;
		String str = msg.str();
		mXsiApp.LogMessage(OgretoXSI(str));
#endif
		
		// Save transforms
		MATH::CTransformation xsiTransform = xsiMesh->obj.GetKinematics().GetGlobal().GetTransform();
		MATH::CTransformation rotTrans;
		rotTrans.SetRotation(xsiTransform.GetRotation());
		// Bounds calculation
        Real squaredRadius = 0.0f;
        Vector3 min, max;
        bool first = true;

        // Iterate through all the triangles
        // There will often be less poisitions than normals and UVs
        // But TrianglePoint
        for (long t = 0; t < triArray.GetCount(); ++t)
        {
            Triangle tri(triArray[t]);
			// derive sampler indices for triangle
			size_t samplerIndices[3];
			CPolygonFaceRefArray polys(xsiMesh->mesh.GetPolygons());
			deriveSamplerIndices(tri, polys[tri.GetPolygonIndex()], samplerIndices);

			// Decide which ProtoSubMesh we're to add this to (assume main)
			// If we find this triangle relates to a polygon which is in 
			// a cluster which has a different material, we change
			ProtoSubMesh* currentProto = mMainProtoMesh;
			PolygonToProtoSubMeshList::iterator polyi = 
				mPolygonToProtoSubMeshList.find(tri.GetPolygonIndex());
			if (polyi != mPolygonToProtoSubMeshList.end())
			{
				currentProto = polyi->second;
			}
			
            CTriangleVertexRefArray points = tri.GetPoints();
            for (long p = 0; p < 3; ++p)
            {
                TriangleVertex point(points[p]);
                long posIndex = point.GetIndex(); // unique position index
                UniqueVertex vertex;
				// Get position
				MATH::CVector3 xsipos = point.GetPosition();
				// Apply global SRT
				xsipos.MulByTransformationInPlace(xsiTransform);
                vertex.position = XSItoOgre(xsipos);
				// Get normal
				MATH::CVector3 xsinorm = point.GetNormal();
				// Apply global rotation
				xsinorm *= rotTrans;
                vertex.normal = XSItoOgre(xsinorm);

				for (size_t i = 0; i < mCurrentTextureCoordDimensions.size(); ++i)
				{
					// sampler indices can correctly dereference to sampler-order
					// uv sets we built earlier
					vertex.uv[i] = (mCurrentSamplerSets[i])[samplerIndices[p]];
				}
                
                if (mCurrentHasVertexColours)
                    vertex.colour = XSItoOgre(point.GetColor());

                size_t index = createOrRetrieveUniqueVertex(
									currentProto, posIndex, true, vertex);
                currentProto->indices.push_back(index);

				// bounds
				if (first)
				{
					squaredRadius = vertex.position.squaredLength();
					min = max = vertex.position;
					first = false;
				}
				else
				{
					squaredRadius = 
						std::max(squaredRadius, vertex.position.squaredLength());
					min.makeFloor(vertex.position);
					max.makeCeil(vertex.position);
				}
            }
        }

		// Merge bounds
		AxisAlignedBox box;
        box.setExtents(min, max);
        box.merge(pMesh->getBounds());
        pMesh->_setBounds(box);
        pMesh->_setBoundingSphereRadius(
			std::max(
				pMesh->getBoundingSphereRadius(), 
				Math::Sqrt(squaredRadius)));


		// Post-process the mesh
		postprocessPolygonMesh(xsiMesh);

	}
	//-----------------------------------------------------------------------
	bool XsiMeshExporter::preprocessPolygonMesh(PolygonMeshEntry* xsiMesh)
	{
        // derive number of UVs
        int numUVs = 0;
        CRefArray clusterRefArray;
        // Filter to 'sample' types
        xsiMesh->mesh.GetClusters().Filter(
			siSampledPointCluster,CStringArray(),L"",clusterRefArray);

        Cluster samplePointClusterUV;
        CRefArray uvClusterPropertiesRefArray;
		int i;
        
        for(i = 0; i < clusterRefArray.GetCount(); ++i)
        {
            Cluster cluster(clusterRefArray[i]);		
            // Now filter all the 'uvspace' children
            // there is one of these per UV set
            if(cluster.GetProperties().Filter(
                siClsUVSpaceTxtType, CStringArray(), L"", 
                uvClusterPropertiesRefArray) == CStatus::OK)
            {
                samplePointClusterUV = cluster;			
                break;
            }
        }

        // Ok, we now have our array of UV sets
        numUVs = uvClusterPropertiesRefArray.GetCount();
		size_t numSamplerPoints = xsiMesh->mesh.GetNodes().GetCount();
		// list of UVs stored in order of sampler points (use 3D coords by default)
		mCurrentSamplerSets.reserve(numUVs);
		mCurrentTextureCoordDimensions.reserve(numUVs);
        for(i = 0; i < numUVs; ++i)
        {
			// init sampler points
			Vector3* samplerUVs = new Vector3[numSamplerPoints];
			mCurrentSamplerSets.push_back(samplerUVs);

			// Detect the dimensions by figuring out if any are all 0
			bool hasU, hasV, hasW;
			hasU = hasV = hasW = false;

			// Pull out all the UV data for this set and reorder it based on 
			// samples, we'll need this for reference later
			// get Elements from uvspace Property
            ClusterProperty uvProp(uvClusterPropertiesRefArray[i]);
			CClusterPropertyElementArray uvElements = uvProp.GetElements();

			// Now, each Element here is actually a CDoubleArray of the u,v,w values
			// However it's not in order of samplers, we need to use the Array
			// linked to from the Elements collection under the cluster (not the 
			// cluster property, confusing I know) to figure out what sampler 
			// index it is, i.e.
			// samplerUVs[Array[j]] = Element[j]
			// In all casesmCurrentSamplerSets in XSI element properties are referenced back to 
			// their original poly index via this array, ie all cluster properties
			// in the same cluster are dereferenced in the same way. 
			CLongArray derefArray = samplePointClusterUV.GetElements().GetArray();

			for (int j = 0; j < uvElements.GetCount(); ++j)
			{
				CDoubleArray curUVW(uvElements.GetItem(j));
				size_t samplerIndex = derefArray[j];
				samplerUVs[samplerIndex].x = curUVW[0];//u
				samplerUVs[samplerIndex].y = curUVW[1];//v
				samplerUVs[samplerIndex].z = curUVW[2];//w

				if (!hasU && curUVW[0] > 0)
					hasU = true;
				if (!hasV && curUVW[1] > 0)
					hasV = true;
				if (!hasW && curUVW[2] > 0)
					hasW = true;

			}
			
			// save dimensions
			mCurrentTextureCoordDimensions.push_back(
				(hasU?1:0) + (hasV?1:0) + (hasW?1:0));

        }


        // do we have vertex colours?
        ClusterProperty vertexColourClusterProperty = xsiMesh->mesh.GetCurrentVertexColor();
        if (vertexColourClusterProperty.IsValid())
            mCurrentHasVertexColours = true;
		else
			mCurrentHasVertexColours = false;

		
		/* Create any ProtoSubMeshes which don't exist yet for the 
		 * materials in question, and define the PolygonCluster map
		 */
		// Main material (will never exist if not merging submeshes)
		mMainProtoMesh = createOrRetrieveProtoSubMesh(
			XSItoOgre(xsiMesh->obj.GetMaterial().GetName()), 
			XSItoOgre(xsiMesh->obj.GetName()),
			mCurrentTextureCoordDimensions, 
			mCurrentHasVertexColours);
		
		// For each polygon cluster
        CRefArray polygonClusters;
        // Filter to 'poly' types
        xsiMesh->mesh.GetClusters().Filter(siPolygonCluster, CStringArray(), L"", 
			polygonClusters);
		mPolygonToProtoSubMeshList.clear();
        for(i = 0; i < polygonClusters.GetCount(); ++i)
        {
			Cluster cluster(polygonClusters[i]);	
			// Is the material different for this poly cluster?
			if (cluster.GetMaterial() != xsiMesh->obj.GetMaterial())
			{
				ProtoSubMesh* ps = createOrRetrieveProtoSubMesh(
					XSItoOgre(cluster.GetMaterial().GetName()),
					XSItoOgre(cluster.GetName()),
					mCurrentTextureCoordDimensions, 
					mCurrentHasVertexColours);
				// Each element is a polygon index 
				CLongArray elems = cluster.GetElements().GetArray();
				for (int p = 0; p < elems.GetCount(); ++p)
				{
					mPolygonToProtoSubMeshList[elems[p]] = ps;
				}
			}
		}

		return true;


	}
	//-----------------------------------------------------------------------
	void XsiMeshExporter::postprocessPolygonMesh(PolygonMeshEntry* xsiMesh)
	{
		// clear all position index remaps, incase merged
		for (ProtoSubMeshList::iterator p = mProtoSubmeshList.begin();
			p != mProtoSubmeshList.end(); ++p)
		{
			ProtoSubMesh* ps = p->second;
			ps->posIndexRemap.clear();
		}

		// free temp UV data now
		for(SamplerSetList::iterator s = mCurrentSamplerSets.begin();
			s != mCurrentSamplerSets.end(); ++s)
		{
			// init sampler points
			delete [] *s;
		}
		mCurrentSamplerSets.clear();
		mCurrentTextureCoordDimensions.clear();
		
	}
	//-----------------------------------------------------------------------
	void XsiMeshExporter::exportProtoSubMeshes(Mesh* pMesh)
	{
		// Take the list of ProtoSubMesh instances and bake a SubMesh per
		// instance, then clear the list

		for (ProtoSubMeshList::iterator i = mProtoSubmeshList.begin();
			i != mProtoSubmeshList.end(); ++i)
		{
			// export each one
			exportProtoSubMesh(pMesh, i->second);

			// free it
			delete i->second;
		}
		mProtoSubmeshList.clear();
	}
	//-----------------------------------------------------------------------
	void XsiMeshExporter::exportProtoSubMesh(Mesh* pMesh, ProtoSubMesh* proto)
	{

        SubMesh* sm = 0;
        if (proto->name.empty())
        {
            // anonymous submesh
            sm = pMesh->createSubMesh();
        }
        else
        {
            // named submesh
            sm = pMesh->createSubMesh(proto->name);
        }

        // Set material
        sm->setMaterialName(proto->materialName);
        // never use shared geometry
        sm->useSharedVertices = false;
        sm->vertexData = new VertexData();
        // always do triangle list
        sm->indexData->indexCount = proto->indices.size();
		
		sm->vertexData->vertexCount = proto->uniqueVertices.size();
        // Determine index size
        bool use32BitIndexes = false;
        if (proto->uniqueVertices.size() > 65536)
        {
            use32BitIndexes = true;
        }

        sm->indexData->indexBuffer = 
            HardwareBufferManager::getSingleton().createIndexBuffer(
            use32BitIndexes ? HardwareIndexBuffer::IT_32BIT : HardwareIndexBuffer::IT_16BIT,
            sm->indexData->indexCount,
            HardwareBuffer::HBU_STATIC_WRITE_ONLY);
        if (use32BitIndexes)
        {
            uint32* pIdx = static_cast<uint32*>(
                sm->indexData->indexBuffer->lock(HardwareBuffer::HBL_DISCARD));
            writeIndexes(pIdx, proto->indices);
            sm->indexData->indexBuffer->unlock();
        }
        else
        {
            uint16* pIdx = static_cast<uint16*>(
                sm->indexData->indexBuffer->lock(HardwareBuffer::HBL_DISCARD));
            writeIndexes(pIdx, proto->indices);
            sm->indexData->indexBuffer->unlock();
        }


        // define vertex declaration
        unsigned buf = 0;
        size_t offset = 0;
		// always add position and normal
        sm->vertexData->vertexDeclaration->addElement(buf, offset, VET_FLOAT3, VES_POSITION);
        offset += VertexElement::getTypeSize(VET_FLOAT3);
        sm->vertexData->vertexDeclaration->addElement(buf, offset, VET_FLOAT3, VES_NORMAL);
        offset += VertexElement::getTypeSize(VET_FLOAT3);
        // TODO - split vertex data here if animated
        /*
        if (pMesh->hasSkeleton())
        {
            buf = 0;
            offset = 0;
        }
        */
		// Optional vertex colour
        if(proto->hasVertexColours)
        {
            sm->vertexData->vertexDeclaration->addElement(buf, offset, VET_COLOUR, VES_DIFFUSE);
            offset += VertexElement::getTypeSize(VET_COLOUR);
        }
        // Define UVs
        for (unsigned short uvi = 0; uvi < proto->textureCoordDimensions.size(); ++uvi)
        {
			VertexElementType uvType = 
				VertexElement::multiplyTypeCount(
					VET_FLOAT1, proto->textureCoordDimensions[uvi]);
            sm->vertexData->vertexDeclaration->addElement(
				buf, offset, uvType, VES_TEXTURE_COORDINATES, uvi);
            offset += VertexElement::getTypeSize(uvType);
        }

        // create & fill buffer(s)
        for (unsigned short b = 0; b <= sm->vertexData->vertexDeclaration->getMaxSource(); ++b)
        {
            createVertexBuffer(sm->vertexData, b, proto->uniqueVertices);
        }
		
	}
	//-----------------------------------------------------------------------
	void XsiMeshExporter::buildPolygonMeshList(bool includeChildren)
	{
		Selection sel(mXsiApp.GetSelection());
		if (sel.GetCount() == 0)
		{
			// Whole scene
			// Derive the scene root
			X3DObject sceneRoot(mXsiApp.GetActiveSceneRoot());
			findPolygonMeshes(sceneRoot, true);
		}
		else
		{
			// iterate over selection
			for (int i = 0; i < sel.GetCount(); ++i)
			{
				X3DObject obj(sel[i]);
				findPolygonMeshes(obj,includeChildren);
			}
		}
	}
	//-----------------------------------------------------------------------
	void XsiMeshExporter::findPolygonMeshes(X3DObject& x3dObj, bool recurse)
	{
		// Check validity of current object
		if (!x3dObj.IsValid())
		{
			OGRE_EXCEPT(Exception::ERR_INTERNAL_ERROR, 
				"Invalid X3DObject found",
				"XsiMeshExporter::exportX3DObject");
		}
#ifdef _DEBUG
		// Log a message in script window
		CString name = x3dObj.GetName() ;
		mXsiApp.LogMessage(L"-- Traversing " +  name) ;
#endif


		// locate any geometry
		if (!x3dObj.IsA(siCameraID) && 
			!x3dObj.IsA(siLightID) && 
			!x3dObj.IsA(siNullID) && 
			!x3dObj.IsA(siModelID))
		{
			Primitive prim(x3dObj.GetActivePrimitive());
			if (prim.IsValid())
			{
				Geometry geom(prim.GetGeometry());
				if (geom.GetRef().GetClassID() == siPolygonMeshID)
				{
					// add it to the list 
					mXsiPolygonMeshList.insert(
						new PolygonMeshEntry(PolygonMesh(geom), x3dObj));
#ifdef _DEBUG
					mXsiApp.LogMessage(L"-- Queueing " +  name) ;
#endif
				}
			}

		}

		// Cascade into children
		if (recurse)
		{
			CRefArray children = x3dObj.GetChildren();

			for(long i = 0; i < children.GetCount(); i++)
			{
				X3DObject childObj = children[i];
				findPolygonMeshes(childObj, recurse);
			}
		}

	}
    //-----------------------------------------------------------------------
	void XsiMeshExporter::cleanupPolygonMeshList(void)
	{
		for (PolygonMeshList::iterator pm = mXsiPolygonMeshList.begin();
			pm != mXsiPolygonMeshList.end(); ++pm)
		{
			delete *pm;
		}
		mXsiPolygonMeshList.clear();
	}
	//-----------------------------------------------------------------------
	void XsiMeshExporter::deriveSamplerIndices(const Triangle& tri, 
		const PolygonFace& face, size_t* samplerIndices)
	{
		//We want to find what is the SampleIndex associated with 3 
		//vertex in a Triangle
		CPointRefArray facePoints = face.GetPoints();

		//Get the position of the 3 vertex in the triangle
		MATH::CVector3Array triPos = tri.GetPositionArray();

		//Get the position of the N Points in the polygon
		MATH::CVector3Array facePos = facePoints.GetPositionArray();

		//To know if the 3 vertex have a point in the same position
		bool found[3];
		found[0] = false;
		found[1] = false;
		found[2] = false;

		int p,t;
		//For the 3 triangle vertices
		for(t=0; t<3 ; t++)
		{       //for each polygon point
			for(p=0; p<facePos.GetCount() && !found[t]; p++)
			{
				//Check if the position is the same
				if(triPos[t] == facePos[p])
				{
					//if so, we know the PolygonPointIndex of the TriangleVertex
					//then, we must find what is the sample index associated 
					//with this Point
					samplerIndices[t] = 
						getSamplerIndex(Facet(face), facePoints[p]);
					found[t] = true;
				}
			}

		}

		if (!found[0] || !found[1] || !found[2] )
		{
			// Problem!
			mXsiApp.LogMessage(L"!! Couldn't find a matching UV point!");
		}

	}
	//-----------------------------------------------------------------------
	size_t XsiMeshExporter::getSamplerIndex(const Facet &f, const Point &p)
	{
		//This function check if a Sample is shared by a Facet and a Point
		//just by using the operator=
		//Only one Sample can be shared.

		Sample curFacetSample;
		CSampleRefArray facetSamples( f.GetSamples() );
		CSampleRefArray pointSamples( p.GetSamples() );

		for(int i = 0; i < facetSamples.GetCount(); i++ )
		{

			curFacetSample = Sample( facetSamples[i] );

			for(int j = 0; j < pointSamples.GetCount(); j++)
			{
				if(curFacetSample == Sample(pointSamples[j]))
				{
					return curFacetSample.GetIndex();
				}
			}
		}
		// Problem!
		mXsiApp.LogMessage(L"!! Couldn't find a matching sample point!");
		return 0;
	}
    //-----------------------------------------------------------------------
    template <typename T> 
    void XsiMeshExporter::writeIndexes(T* buf, IndexList& indexes)
    {
        IndexList::const_iterator i, iend;
        iend = indexes.end();
        for (i = indexes.begin(); i != iend; ++i)
        {
            *buf++ = static_cast<T>(*i);
        }
    }
    //-----------------------------------------------------------------------
    void XsiMeshExporter::createVertexBuffer(VertexData* vd, 
		unsigned short bufIdx, UniqueVertexList& uniqueVertexList)
    {
        HardwareVertexBufferSharedPtr vbuf = 
			HardwareBufferManager::getSingleton().createVertexBuffer(
            	vd->vertexDeclaration->getVertexSize(bufIdx),
            	vd->vertexCount, 
            	HardwareBuffer::HBU_STATIC_WRITE_ONLY);
        vd->vertexBufferBinding->setBinding(bufIdx, vbuf);
        size_t vertexSize = vd->vertexDeclaration->getVertexSize(bufIdx);

        char* pBase = static_cast<char*>(
				vbuf->lock(HardwareBuffer::HBL_DISCARD));

        VertexDeclaration::VertexElementList elems = 
			vd->vertexDeclaration->findElementsBySource(bufIdx);
        VertexDeclaration::VertexElementList::iterator ei, eiend;
        eiend = elems.end();
        float* pFloat;
        RGBA* pRGBA;

        UniqueVertexList::iterator srci = uniqueVertexList.begin();

        for (size_t v = 0; v < vd->vertexCount; ++v, ++srci)
        {
            for (ei = elems.begin(); ei != eiend; ++ei)
            {
                VertexElement& elem = *ei;
                switch(elem.getSemantic())
                {
                case VES_POSITION:
                    elem.baseVertexPointerToElement(pBase, &pFloat);
                    *pFloat++ = srci->position.x;
                    *pFloat++ = srci->position.y;
                    *pFloat++ = srci->position.z;
                    break;
                case VES_NORMAL:
                    elem.baseVertexPointerToElement(pBase, &pFloat);
                    *pFloat++ = srci->normal.x;
                    *pFloat++ = srci->normal.y;
                    *pFloat++ = srci->normal.z;
                    break;
                case VES_DIFFUSE:
                    elem.baseVertexPointerToElement(pBase, &pRGBA);
                    *pRGBA = srci->colour;
                    break;
                case VES_TEXTURE_COORDINATES:
                    elem.baseVertexPointerToElement(pBase, &pFloat);
					for (int t = 0; t < VertexElement::getTypeCount(elem.getType()); ++t)
					{
						Real val = srci->uv[elem.getIndex()][t];
						*pFloat++ = val;
					}
                    break;
                }
            }
            pBase += vertexSize;
        }
        vbuf->unlock();

    }
    //-----------------------------------------------------------------------
    size_t XsiMeshExporter::createOrRetrieveUniqueVertex(
		ProtoSubMesh* proto, size_t positionIndex, 
		bool positionIndexIsOriginal, const UniqueVertex& vertex)
    {
		size_t lookupIndex;
		if (positionIndexIsOriginal)
		{
			// look up the original index
			IndexRemap::iterator remapi = 
				proto->posIndexRemap.find(positionIndex);
			if (remapi == proto->posIndexRemap.end())
			{
				// not found, add
				size_t realIndex = proto->uniqueVertices.size();
				// add remap entry so we can find this again
				proto->posIndexRemap[positionIndex] = realIndex;
				proto->uniqueVertices.push_back(vertex);
				return realIndex;
			}
			else
			{
				// Found existing mapping
				lookupIndex = remapi->second;
			}
		}
		else
		{
			// Not an original index, index is real
			lookupIndex = positionIndex;
		}

		// If we get here, either the position isn't an original index (ie
		// we've already found that it doesn't match, and have cascaded)
		// or there is an existing entry
		// Get existing
	    UniqueVertex& orig = proto->uniqueVertices[lookupIndex];
		// Compare, do we have the same details?
		if (orig == vertex)
		{
			// ok, they match
			return lookupIndex;
		}
		else
		{
    	    // no match, go to next or create new
	        if (orig.nextIndex)
        	{
            	// cascade to the next candidate (which is a real index, not an original)
                return createOrRetrieveUniqueVertex(
						proto, orig.nextIndex, false, vertex);
            }
			else
			{
				// No more cascades to check, must be a new one
	            // get new index
    	        size_t realIndex = proto->uniqueVertices.size();
        	    orig.nextIndex = realIndex;
            	// create new (NB invalidates 'orig' reference)
                proto->uniqueVertices.push_back(vertex);
				// note, don't add to remap, that's only for finding the
				// first entry, nextIndex is used to chain to the others

    	        return realIndex;
			}
		}
    }
}
