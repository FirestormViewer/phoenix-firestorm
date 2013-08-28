/**
 * @file daeexport.cpp
 * @brief A system which allows saving in-world objects to Collada .DAE files for offline texturizing/shading.
 * @authors Latif Khalifa, Cinder Biscuits
 *
 * $LicenseInfo:firstyear=2013&license=LGPLV2.1$
 * Copyright (C) 2013 Latif Khalifa
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */
#include "llviewerprecompiledheaders.h"

#include "daeexport.h"

//colladadom includes
#if LL_MSVC
#pragma warning (disable : 4018)
#pragma warning (push)
#pragma warning (disable : 4068)
#pragma warning (disable : 4263)
#pragma warning (disable : 4264)
#endif
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include "dae.h"
//#include "dom.h"
#include "dom/domAsset.h"
#include "dom/domBind_material.h"
#include "dom/domCOLLADA.h"
#include "dom/domConstants.h"
#include "dom/domController.h"
#include "dom/domEffect.h"
#include "dom/domGeometry.h"
#include "dom/domInstance_geometry.h"
#include "dom/domInstance_material.h"
#include "dom/domInstance_node.h"
#include "dom/domInstance_effect.h"
#include "dom/domMaterial.h"
#include "dom/domMatrix.h"
#include "dom/domNode.h"
#include "dom/domProfile_COMMON.h"
#include "dom/domRotate.h"
#include "dom/domScale.h"
#include "dom/domTranslate.h"
#include "dom/domVisual_scene.h"
#if LL_MSVC
#pragma warning (pop)
#endif

// newview includes
#include "llagent.h"
#include "llfilepicker.h"
#include "llmeshrepository.h"
#include "llnotificationsutil.h"
#include "llselectmgr.h"
#include "lltrans.h"
#include "llversioninfo.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
#include "llvovolume.h"

#define ANY_FACE -1

namespace DAEExportUtil
{
	bool can_export_node(LLSelectNode* node)
	{
		bool exportable = false;
		LLViewerObject* object = node->getObject();
		if ((LLGridManager::getInstance()->isInSecondLife())
			&& object->permYouOwner()
			&& (gAgentID == node->mPermissions->getCreator()))
		{
			LLVOVolume *volobjp = NULL;
			if (object->getPCode() == LL_PCODE_VOLUME)
			{
				volobjp = (LLVOVolume *)object;
			}
			if (volobjp && volobjp->isSculpted())
			{
				const LLSculptParams *sculpt_params = (const LLSculptParams *)object->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
				if(volobjp->isMesh())
				{
					LLSD mesh_header = gMeshRepo.getMeshHeader(sculpt_params->getSculptTexture());
					exportable = mesh_header["creator"].asUUID() == gAgentID;
				}
				else if (sculpt_params)
				{
					LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(sculpt_params->getSculptTexture());
					exportable = (imagep->mComment.find("a") != imagep->mComment.end()
								  && LLUUID(imagep->mComment["a"]) == gAgentID);
					if (!exportable)
						LL_INFOS("export") << "Sculpt map has failed permissions check." << LL_ENDL;
				}
				
			}
			else
			{
				exportable = true;
			}

		}
#if OPENSIM
		else if (LLGridManager::getInstance()->isInOpenSim())
		{
			LLViewerRegion* region = gAgent.getRegion();
			if (region && region->regionSupportsExport() == LLViewerRegion::EXPORT_ALLOWED)
			{
				exportable = node->mPermissions->allowExportBy(gAgent.getID());
			}
			else if (region && region->regionSupportsExport() == LLViewerRegion::EXPORT_DENIED)
			{
				// Only your own creations if this is explicitly set
				exportable = (object->permYouOwner()
							  && gAgentID == node->mPermissions->getCreator());
			}
			/// TODO: Once enough grids adopt a version supporting the exports cap, get consensus
			/// on whether we should allow full perm exports anymore.
			else	// LLViewerRegion::EXPORT_UNDEFINED
			{
				exportable = (object->permYouOwner()
							  && object->permModify()
							  && object->permCopy()
							  && object->permTransfer());
			}
		}
#endif // OPENSIM
		return exportable;
	}
	
	void export_selection()
	{
		LLObjectSelectionHandle selection = LLSelectMgr::instance().getSelection();
		if (selection)
		{
			DAESaver *daesaver = new DAESaver;
			daesaver->mOffset = -selection->getFirstRootObject()->getRenderPosition();
		
			LLObjectSelection::valid_root_iterator root_it = selection->valid_root_begin();
			LLSelectNode *node = *root_it;
			std::string filename = node->mName;
		
			S32 total = 0;
			S32 included = 0;
		
			for (LLObjectSelection::iterator iter = selection->begin(); iter != selection->end(); iter++)
			{
				node = *iter;
				total++;
			
				if (!can_export_node(node) || !node->getObject()->getVolume())
				{
					continue;
				}
			
				included++;
				daesaver->add(node->getObject(), node->mName);
			}
		
			if (daesaver->mObjects.empty())
			{
				LLNotificationsUtil::add("ExportColladaFailed");
				return;
			}
		
			LLFilePicker &file_picker = LLFilePicker::instance();
		
			if (!file_picker.getSaveFile(LLFilePicker::FFSAVE_COLLADA, LLDir::getScrubbedFileName(filename + ".dae")))
			{
				LL_INFOS("export") << "User closed the filepicker; aborting export." << LL_ENDL;
				return;
			}
		
			daesaver->saveDAE(file_picker.getFirstFile());
		
			LLSD args;
			args["TOTAL"] = total;
			args["FAILED"] = total - included;
		
			LLNotificationsUtil::add(args["FAILED"].asInteger() ? "ExportColladaPartial" : "ExportColladaSuccess", args);
		}
		else
		{
			LL_INFOS("export") << "Nothing selected; Bailing." << LL_ENDL;
		}
	}
}


void DAESaver::add(const LLViewerObject* prim, const std::string name)
{
	mObjects.push_back(std::pair<LLViewerObject*,std::string>((LLViewerObject*)prim, name));
}

class v4adapt
{
private:
	LLStrider<LLVector4a> mV4aStrider;
public:
	v4adapt(LLVector4a* vp){ mV4aStrider = vp; }
	inline LLVector3 operator[] (const unsigned int i)
	{
		return LLVector3((F32*)&mV4aStrider[i]);
	}
};

void DAESaver::addSource(daeElement* mesh, const char* src_id, std::string params, const std::vector<F32> &vals)
{
	daeElement* source = mesh->add("source");
	source->setAttribute("id", src_id);
	daeElement* src_array = source->add("float_array");

	src_array->setAttribute("id", llformat("%s-%s", src_id, "array").c_str());
	src_array->setAttribute("count", llformat("%d", vals.size()).c_str());

	for (S32 i = 0; i < vals.size(); i++)
	{
		((domFloat_array*)src_array)->getValue().append(vals[i]);
	}

	domAccessor* acc = daeSafeCast<domAccessor>(source->add("technique_common accessor"));
	acc->setSource(llformat("#%s-%s", src_id, "array").c_str());
	acc->setCount(vals.size() / params.size());
	acc->setStride(params.size());

	for (std::string::iterator p_iter = params.begin(); p_iter != params.end(); ++p_iter)
	{
		domElement* pX = acc->add("param");
		pX->setAttribute("name", llformat("%c", *p_iter).c_str());
		pX->setAttribute("type", "float");
	}
}

void DAESaver::addPolygons(daeElement* mesh, const char* geomID, const char* materialID, LLViewerObject* obj, int face_to_include)
{
	domPolylist* polylist = daeSafeCast<domPolylist>(mesh->add("polylist"));
	polylist->setMaterial(materialID);

	// Vertices semantic
	{
		domInputLocalOffset* input = daeSafeCast<domInputLocalOffset>(polylist->add("input"));
		input->setSemantic("VERTEX");
		input->setOffset(0);
		input->setSource(llformat("#%s-%s", geomID, "vertices").c_str());
	}

	// Normals semantic
	{
		domInputLocalOffset* input = daeSafeCast<domInputLocalOffset>(polylist->add("input"));
		input->setSemantic("NORMAL");
		input->setOffset(0);
		input->setSource(llformat("#%s-%s", geomID, "normals").c_str());
	}

	// UV semantic
	{
		domInputLocalOffset* input = daeSafeCast<domInputLocalOffset>(polylist->add("input"));
		input->setSemantic("TEXCOORD");
		input->setOffset(0);
		input->setSource(llformat("#%s-%s", geomID, "map0").c_str());
	}

	// Save indices
	domP* p = daeSafeCast<domP>(polylist->add("p"));
	domPolylist::domVcount *vcount = daeSafeCast<domPolylist::domVcount>(polylist->add("vcount"));
	S32 index_offset = 0;
	S32 num_tris = 0;
	for (S32 face_num = 0; face_num < obj->getVolume()->getNumVolumeFaces(); face_num++)
	{
		const LLVolumeFace* face = (LLVolumeFace*)&obj->getVolume()->getVolumeFace(face_num);

		if (face_to_include == ANY_FACE || face_to_include == face_num)
		{
			for (S32 i = 0; i < face->mNumIndices; i++)
			{
				U16 index = index_offset + face->mIndices[i];
				(p->getValue()).append(index);
				if (i % 3 == 0)
				{
					(vcount->getValue()).append(3);
					num_tris++;
				}
			}
		}
		index_offset += face->mNumVertices;
	}
	polylist->setCount(num_tris);
}

bool DAESaver::saveDAE(std::string filename)
{
	DAE dae;
	// First set the filename to save
	daeElement* root = dae.add(filename);

	// Obligatory elements in header
	daeElement* asset = root->add("asset");
	// Get ISO format time
	char buff[8];
	time_t now = time(NULL);
	strftime(buff, 8, "%Y%m%d", localtime(&now));
	std::string date(buff, 8);
	
	daeElement* created = asset->add("created");
	created->setCharData(date);
	daeElement* modified = asset->add("modified");
	modified->setCharData(date);
	daeElement* unit = asset->add("unit");
	unit->setAttribute("name", "meter");
	unit->setAttribute("value", "1");
	daeElement* up_axis = asset->add("up_axis");
	up_axis->setCharData("Z_UP");

	// File creator
	std::string author = "Unknown";
	if (gCacheName)
		gCacheName->getFullName(gAgentID, author);
	
	daeElement* contributor = asset->add("contributor");
	contributor->add("author")->setCharData(author);
	contributor->add("authoring_tool")->setCharData(LLVersionInfo::getChannelAndVersion());

	daeElement* geomLib = root->add("library_geometries");
	daeElement* effects = root->add("library_effects");
	daeElement* materials = root->add("library_materials");
	daeElement* scene = root->add("library_visual_scenes visual_scene");
	scene->setAttribute("id", "Scene");
	scene->setAttribute("name", "Scene");

	S32 prim_nr = 0;

	for (obj_info_t::iterator obj_iter = mObjects.begin(); obj_iter != mObjects.end(); ++obj_iter)
	{
		LLViewerObject* obj = obj_iter->first;
		S32 total_num_vertices = 0;

		std::string name = "";
		if (name.empty()) name = llformat("prim%d", prim_nr++);

		const char* geomID = name.c_str();

		daeElement* geom = geomLib->add("geometry");
		geom->setAttribute("id", llformat("%s-%s", geomID, "mesh").c_str());
		daeElement* mesh = geom->add("mesh");

		std::vector<F32> position_data;
		std::vector<F32> normal_data;
		std::vector<F32> uv_data;

		S32 num_faces = obj->getVolume()->getNumVolumeFaces();

		for (S32 face_num = 0; face_num < num_faces; face_num++)
		{
			const LLVolumeFace* face = (LLVolumeFace*)&obj->getVolume()->getVolumeFace(face_num);
			total_num_vertices += face->mNumVertices;

			v4adapt verts(face->mPositions);
			v4adapt norms(face->mNormals);
			for (S32 i=0; i < face->mNumVertices; i++)
			{
				const LLVector3 v = verts[i];
				position_data.push_back(v.mV[VX]);
				position_data.push_back(v.mV[VY]);
				position_data.push_back(v.mV[VZ]);

				const LLVector3 n = norms[i];
				normal_data.push_back(n.mV[VX]);
				normal_data.push_back(n.mV[VY]);
				normal_data.push_back(n.mV[VZ]);

				const LLVector2 uv = face->mTexCoords[i];
				uv_data.push_back(uv.mV[VX]);
				uv_data.push_back(uv.mV[VY]);
			}
		}
		
		
		addSource(mesh, llformat("%s-%s", geomID, "positions").c_str(), "XYZ", position_data);
		addSource(mesh, llformat("%s-%s", geomID, "normals").c_str(), "XYZ", normal_data);
		addSource(mesh, llformat("%s-%s", geomID, "map0").c_str(), "ST", uv_data);

		// Add the <vertices> element
		{
			daeElement*	verticesNode = mesh->add("vertices");
			verticesNode->setAttribute("id", llformat("%s-%s", geomID, "vertices").c_str());
			daeElement* verticesInput = verticesNode->add("input");
			verticesInput->setAttribute("semantic", "POSITION");
			verticesInput->setAttribute("source", llformat("#%s-%s", geomID, "positions").c_str());
		}

		// Add triangles
		for (S32 face_num = 0; face_num < num_faces; face_num++)
		{
			addPolygons(mesh, geomID, llformat("%s-f%d-%s", geomID, face_num, "material").c_str(), obj, face_num);
		}

		// Effects (face color, alpha)
		for (S32 face_num = 0; face_num < num_faces; face_num++)
		{
			LLTextureEntry* te = obj->getTE(face_num);
			LLColor4 color = te->getColor();
			domEffect* effect = (domEffect*)effects->add("effect");
			effect->setId(llformat("%s-f%d-%s", geomID, face_num, "fx").c_str());
			daeElement* t = effect->add("profile_COMMON technique");
			t->setAttribute("sid", "common");
			domElement* phong = t->add("phong");
			phong->add("diffuse color")->setCharData(llformat("%f %f %f %f", color.mV[0], color.mV[1], color.mV[2], color.mV[3]).c_str());
			phong->add("transparency float")->setCharData(llformat("%f", color.mV[3]).c_str());
		}

		// Materials
		for (S32 face_num = 0; face_num < num_faces; face_num++)
		{
			domMaterial* mat = (domMaterial*)materials->add("material");
			mat->setId(llformat("%s-f%d-%s", geomID, face_num, "material").c_str());
			domElement* matEffect = mat->add("instance_effect");
			matEffect->setAttribute("url", llformat("#%s-f%d-%s", geomID, face_num, "fx").c_str());
		}

		daeElement* node = scene->add("node");
		node->setAttribute("type", "NODE");
		node->setAttribute("id", geomID);
		node->setAttribute("name", geomID);

		// Set tranform matrix (node position, rotation and scale)
		domMatrix* matrix = (domMatrix*)node->add("matrix");
		LLXform srt;
		srt.setScale(obj->getScale());
		srt.setPosition(obj->getRenderPosition() + mOffset);
		srt.setRotation(obj->getRenderRotation());
		LLMatrix4 m4;
		srt.getLocalMat4(m4);
		for (int i=0; i<4; i++)
			for (int j=0; j<4; j++)
				(matrix->getValue()).append(m4.mMatrix[j][i]);

		// Geometry of the node
		daeElement* nodeGeometry = node->add("instance_geometry");

		// Bind materials
		daeElement* tq = nodeGeometry->add("bind_material technique_common");
		for (S32 face_num = 0; face_num < num_faces; face_num++)
		{
			daeElement* instanceMaterial = tq->add("instance_material");
			instanceMaterial->setAttribute("symbol", llformat("%s-f%d-%s", geomID, face_num, "material").c_str());
			instanceMaterial->setAttribute("target", llformat("#%s-f%d-%s", geomID, face_num, "material").c_str());
		}

		nodeGeometry->setAttribute("url", llformat("#%s-%s", geomID, "mesh").c_str());

	}
	root->add("scene instance_visual_scene")->setAttribute("url", "#Scene");

	return dae.writeAll();
}

DAESaver::DAESaver()
{}
