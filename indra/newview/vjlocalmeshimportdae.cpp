/**
 * @file vjlocalmeshimportdae.cpp
 * @author Vaalith Jinn
 * @brief Local Mesh dae importer source
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Local Mesh contribution source code
 * Copyright (C) 2022, Vaalith Jinn.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $/LicenseInfo$
 */

 /* precompiled headers */
#include "llviewerprecompiledheaders.h"

/* own header */
#include "vjlocalmesh.h"
#include "vjlocalmeshimportdae.h"

/* linden headers */
#include "llmodelloader.h"
#include "llvoavatarself.h"

/* dae headers*/
#if LL_MSVC
#pragma warning (disable : 4263)
#pragma warning (disable : 4264)
#endif

#include "dae.h"
#include "dom/domConstants.h"
#include "dom/domMesh.h"
#include "dom/domSkin.h"
#include "dom/domGeometry.h"
#include "dom/domInstance_controller.h"
#include "dom/domNode.h"

#if LL_MSVC
#pragma warning (default : 4263)
#pragma warning (default : 4264)
#endif


LLLocalMeshImportDAE::loadFile_return LLLocalMeshImportDAE::loadFile(LLLocalMeshFile* data, LLLocalMeshFileLOD lod)
{
	pushLog("DAE Importer", "Starting");

	// instantiate collada objects
	DAE			 collada_core;
	domCOLLADA*  collada_dom			= nullptr;
	daeDocument* collada_document		= nullptr;
	daeElement*  collada_document_root	= nullptr;
	daeDatabase* collada_db				= nullptr;
	std::string  filename				= data->getFilename(lod);
	mLod								= lod;
	mLoadingLog.clear();

	// open file and check if opened
	collada_dom = collada_core.open(filename);
	if (!collada_dom)
	{
		pushLog("DAE Importer", "Collada DOM instance could not initialize.");
		return loadFile_return(false, mLoadingLog);
	}

	collada_document = collada_core.getDoc(filename);
	if (!collada_document)
	{
		pushLog("DAE Importer", "Collada file contained no collada document.");
		return loadFile_return(false, mLoadingLog);
	}

	collada_document_root = collada_document->getDomRoot();
	if (!collada_document_root)
	{
		pushLog("DAE Importer", "Collada document contained no root.");
		return loadFile_return(false, mLoadingLog);
	}

	// fill collada db
	collada_db = collada_core.getDatabase();
	// NOTE:
	// do we need version?
	// doesn't seem like we do, but this here would be the space for it if yes.

	LLMatrix4 scene_transform_base;

	static auto always_use_meter_scale = LLUICachedControl<bool>("FSLocalMeshScaleAlwaysMeters", false);

	if(!always_use_meter_scale)
	{
		domAsset::domUnit* unit = daeSafeCast<domAsset::domUnit>(collada_document_root->getDescendant(daeElement::matchType(domAsset::domUnit::ID())));
		scene_transform_base.setIdentity();
		if (unit)
		{
			F32 meter = unit->getMeter();
			scene_transform_base.mMatrix[0][0] = meter;
			scene_transform_base.mMatrix[1][1] = meter;
			scene_transform_base.mMatrix[2][2] = meter;
		}
	}
	else
	{
		scene_transform_base.setIdentity();
	}
	//get up axis rotation
	LLMatrix4 rotation;
	
	domUpAxisType up = UPAXISTYPE_Y_UP;  // default in Collada is Y_UP
	domAsset::domUp_axis* up_axis =
	daeSafeCast<domAsset::domUp_axis>(collada_document_root->getDescendant(daeElement::matchType(domAsset::domUp_axis::ID())));
	
	if (up_axis)
	{
		up = up_axis->getValue();
		
		if (up == UPAXISTYPE_X_UP)
		{
			rotation.initRotation(0.0f, 90.0f * DEG_TO_RAD, 0.0f);
		}
		else if (up == UPAXISTYPE_Y_UP)
		{
			rotation.initRotation(90.0f * DEG_TO_RAD, 0.0f, 0.0f);
		}
		
	}
	rotation *= scene_transform_base;
	scene_transform_base = rotation;
	scene_transform_base.condition();	

	size_t mesh_amount = collada_db->getElementCount(NULL, COLLADA_TYPE_MESH);
	size_t skin_amount = collada_db->getElementCount(NULL, COLLADA_TYPE_SKIN);

	if (mesh_amount == 0)
	{
		pushLog("DAE Importer", "Collada document contained no MESH instances.");
		return loadFile_return(false, mLoadingLog);
	}

	std::vector<domMesh*> mesh_usage_tracker;

	// [ iterate over OBJECTS ]
	for (size_t mesh_index = 0; mesh_index < mesh_amount; ++mesh_index)
	{
		pushLog("DAE Importer", "Parsing object number " + std::to_string(mesh_index));

		daeElement* mesh_current_elem = nullptr;
		collada_db->getElement(&mesh_current_elem, mesh_index, NULL, COLLADA_TYPE_MESH);
		domMesh* mesh_current = daeSafeCast<domMesh>(mesh_current_elem);
		if (!mesh_current)
		{
			pushLog("DAE Importer", "Could not dereference the object, skipping.");
			continue;
		}

		// this WILL always return with some kind of a name, even a fallback one of "object_[idx]"
		std::string object_name = getElementName(mesh_current_elem, mesh_index);
		pushLog("DAE Importer", "Object name found: " + object_name);

		// we're filling up from the main file, so we're creating our objects
		if (mLod == LLLocalMeshFileLOD::LOCAL_LOD_HIGH)
		{
			std::unique_ptr<LLLocalMeshObject> current_object = std::make_unique<LLLocalMeshObject>(object_name);
			bool object_success = processObject(mesh_current, current_object.get());

			if (object_success)
			{
				pushLog("DAE Importer", "Object loaded successfully.");
				current_object->computeObjectBoundingBox();
				current_object->computeObjectTransform(scene_transform_base);
				current_object->normalizeFaceValues(mLod);
				// normalizeFaceValues is necessary for skin calculations down below,
				// but we also have to do it once per each lod so we'll call it foreach lod.

				auto& object_vector = data->getObjectVector();
				object_vector.push_back(std::move(current_object));
				mesh_usage_tracker.push_back(mesh_current);
			}

			else
			{
				pushLog("DAE Importer", "Object loading failed, skipping this one.");
				// should we error out here, or recover?
			}
		}

		// parsing a lower lod file, into objects made during LOD3 parsing
		else
		{
			auto& object_vector = data->getObjectVector();
			if (object_vector.size() <= mesh_index)
			{
				pushLog("DAE Importer", "LOD" + std::to_string(mLod) + " is requesting an object that LOD3 did not have or failed to load, skipping.");
				continue; // we try to recover, or should we just fail this?
			}

			auto current_object_ptr = object_vector[mesh_index].get();
			if (!current_object_ptr)
			{
				pushLog("DAE Importer", "Bad object reference given, skipping.");
				continue; // ditto
			}

			bool object_success = processObject(mesh_current, current_object_ptr);

			if (object_success)
			{
				pushLog("DAE Importer", "Object loaded successfully.");
				current_object_ptr->normalizeFaceValues(mLod);
			}

			else
			{
				pushLog("DAE Importer", "Object loading failed.");
			}
		}
	}

	// check if we managed to load any objects at all, if not - no point continuing.
	if (data->getObjectVector().empty())
	{
		pushLog("DAE Importer", "No objects have been successfully loaded, stopping.");
		return loadFile_return(false, mLoadingLog);
	}

	pushLog("DAE Importer", "Object and face parsing complete.");

	// [ iterate over SKINS ]
	size_t skins_loaded = 0;
	for (size_t skin_index = 0; skin_index < skin_amount; ++skin_index)
	{
		pushLog("DAE Importer", "Parsing skin number " + std::to_string(skin_index));

		daeElement* skin_current_elem = nullptr;
		collada_db->getElement(&skin_current_elem, skin_index, NULL, COLLADA_TYPE_SKIN);
		domSkin* skin_current = daeSafeCast<domSkin>(skin_current_elem);
		if (!skin_current)
		{
			pushLog("DAE Importer", "Skin pointer is null, skipping.");
			continue;
		}
		
		domGeometry* skin_current_geom = daeSafeCast<domGeometry>(skin_current->getSource().getElement());
		if (!skin_current_geom)
		{
			pushLog("DAE Importer", "Skin doesn't seem to have associated geometry, skipping.");
			continue;
		}

		domMesh* skin_current_mesh = skin_current_geom->getMesh();
		if (!skin_current_mesh)
		{
			pushLog("DAE Importer", "Skin associated geometry continer has no mesh, skipping.");
			continue;
		}

		// find the associated lllocalmeshobject linked to this mesh
		int current_object_iter = -1;
		for (size_t mesh_tracker_iterator = 0; mesh_tracker_iterator < mesh_usage_tracker.size(); ++mesh_tracker_iterator)
		{
			auto iterable_mesh = mesh_usage_tracker[mesh_tracker_iterator];
			if (skin_current_mesh == iterable_mesh)
			{
				current_object_iter = mesh_tracker_iterator;
				break;
			}
		}

		if (current_object_iter < 0)
		{
			pushLog("DAE Importer", "Skin associated mesh has no equivalent loaded object mesh, skipping.");
			continue;
		}

		auto& object_vector = data->getObjectVector();
		if (current_object_iter >= object_vector.size())
		{
			pushLog("DAE Importer", "Requested object out of bounds, skipping.");
			continue;
		}

		auto& current_object = object_vector[current_object_iter];
		bool skin_success = processSkin(collada_db, collada_document_root, skin_current_mesh, skin_current, current_object);
		if (skin_success)
		{
			++skins_loaded;
			pushLog("DAE Importer", "Skin idx " + std::to_string(skin_index) + " loading successful.");
		}
		else
		{
			pushLog("DAE Importer", "Skin idx " + std::to_string(skin_index) + " loading unsuccessful.");
		}

	}

	if (skin_amount)
	{
		if (skins_loaded)
		{
			if (skins_loaded == skin_amount)
			{
				pushLog("DAE Importer", "All available skin data has been successfully loaded..");
			}
			else
			{
				pushLog("DAE Importer", std::to_string(skin_amount) + " Skin data instances found, out of these - " + std::to_string(skins_loaded) + "loaded successfully.");
			}
		}
		else
		{
			pushLog("DAE Importer", "Skinning data found, but all of it failed to load.");
		}
	}
	else
	{
		pushLog("DAE Importer", "No skinning data found.");
	}

	/*
		this entire function silently, or maybe not so silently ignores certain failures, such as certain objects or skins not loading properly
		does not halt the function. i'm not sure if it should necesarily? 

		it is my hope that the log coupled with seeing what does load correctly - can help people diagnose their mesh better rather than
		just a flat "loading error, nothing loaded" blanket errors.
	*/
	
	return loadFile_return(true, mLoadingLog);
}


bool LLLocalMeshImportDAE::processObject(domMesh* current_mesh, LLLocalMeshObject* current_object)
{
	auto& object_faces = current_object->getFaces(mLod);
	domTriangles_Array& triangle_array = current_mesh->getTriangles_array();
	domPolylist_Array&  polylist_array = current_mesh->getPolylist_array();
//	domPolygons_Array&  polygons_array = current_mesh->getPolygons_array(); // NOTE: polygon schema

	enum submesh_type
	{
		SUBMESH_TRIANGLE,
		SUBMESH_POLYLIST,
		SUBMESH_POLYGONS
	};
	
	const size_t total_faces_found
		= triangle_array.getCount()
		+ polylist_array.getCount();
		//+ polygons_array.getCount(); // NOTE: polygon schema

	pushLog("DAE Importer", "Potentially " + std::to_string(total_faces_found) + " object faces found.");
	
	if (total_faces_found > 8)
	{
		pushLog("DAE Importer", "NOTE: object contains more than 8 faces, but ONLY up to 8 faces will be loaded.");
	}

	bool submesh_failure_found = false;
	bool stop_loading_additional_faces = false;
	auto lambda_process_submesh = [&](int idx, submesh_type array_type)
	{
		// check for face overflow
		if (object_faces.size() == 8)
		{
			pushLog("DAE Importer", "NOTE: reached the limit of 8 faces per object, ignoring the rest.");
			stop_loading_additional_faces = true;
			return;
		}

		auto current_submesh = std::make_unique<LLLocalMeshFace>();
		bool submesh_load_success = false;

		/* in true khronos fashion, there's multiple ways to represent what a mesh object is made of,
		   typically blender does polygons, max does polylists, maya & ad fbx converter do triangles. */
		switch (array_type)
		{
			case submesh_type::SUBMESH_TRIANGLE:
			{
				pushLog("DAE Importer", "Attempting to load face idx " + std::to_string(idx) + " of type TRIANGLES.");
				domTrianglesRef& current_triangle_set = triangle_array.get(idx);
				submesh_load_success = readMesh_Triangle(current_submesh.get(), current_triangle_set);
				break;
			}

			case submesh_type::SUBMESH_POLYLIST:
			{
				pushLog("DAE Importer", "Attempting to load face idx " + std::to_string(idx) + " of type POLYLIST.");
				domPolylistRef& current_polylist_set = polylist_array.get(idx);
				submesh_load_success = readMesh_Polylist(current_submesh.get(), current_polylist_set);
				break;
			}
			// NOTE: polygon schema
			case submesh_type::SUBMESH_POLYGONS:
			{
				// polygons type schema is deprecated, no modern DCC exports that.
				pushLog("DAE Importer", "Attempting to load face idx " + std::to_string(idx) + " of type POLYGONS.");
				pushLog("DAE Importer", "POLYGONS type schema is deprecated.");
				break;
			}
			default:
			{
				pushLog("DAE Importer", "Attempting to load face idx " + std::to_string(idx) + " of unknown type.");
				break;
			}
		}

		if (submesh_load_success)
		{
			pushLog("DAE Importer", "Face idx " + std::to_string(idx) + " loaded successfully.");
			object_faces.push_back(std::move(current_submesh));
		}
		else
		{
			pushLog("DAE Importer", "Face idx " + std::to_string(idx) + " failed to load.");
			submesh_failure_found = true;
		}
	};

	for (size_t iter = 0; iter < triangle_array.getCount(); ++iter)
	{
		if (stop_loading_additional_faces)
		{
			break;
		}

		lambda_process_submesh(iter, submesh_type::SUBMESH_TRIANGLE);
	}

	for (size_t iter = 0; iter < polylist_array.getCount(); ++iter)
	{
		if (stop_loading_additional_faces)
		{
			break;
		}

		lambda_process_submesh(iter, submesh_type::SUBMESH_POLYLIST);
	}

	// NOTE: polygon schema
	/* 
	for (size_t iter = 0; iter < polygons_array.getCount(); ++iter)
	{
		lambda_process_submesh(iter, submesh_type::SUBMESH_POLYGONS);
	}
	*/

	return !submesh_failure_found;
}

// this function is a mess even after refactoring, omnissiah help whichever tech priest delves into this mess.
bool LLLocalMeshImportDAE::processSkin(daeDatabase* collada_db, daeElement* collada_document_root, domMesh* current_mesh, domSkin* current_skin, 
	std::unique_ptr<LLLocalMeshObject>& current_object)
{
	//===========================================================
	// get transforms, calc bind matrix, get SL joint names list
	//===========================================================

	pushLog("DAE Importer", "Preparing transformations and bind shape matrix.");

	// grab transformations
	LLVector4 inverse_translation = current_object->getObjectTranslation() * -1.f;
	LLVector4 objct_size = current_object->getObjectSize();

	// this is basically the data_out but for skinning data
	auto& skininfo = current_object->getObjectMeshSkinInfo();

	// basically copy-pasted from linden magic
	LLMatrix4 normalized_transformation;
	LLMatrix4 mesh_scale;
	normalized_transformation.setTranslation(LLVector3(inverse_translation));
	mesh_scale.initScale(LLVector3(objct_size));
	mesh_scale *= normalized_transformation;
	normalized_transformation = mesh_scale;

	glh::matrix4f inv_mat((F32*)normalized_transformation.mMatrix);
	inv_mat = inv_mat.inverse();
	LLMatrix4 inverse_normalized_transformation(inv_mat.m);

	// bind shape matrix
	domSkin::domBind_shape_matrix* skin_current_bind_matrix = current_skin->getBind_shape_matrix();
	if (skin_current_bind_matrix)
	{
		auto& bind_matrix_value = skin_current_bind_matrix->getValue();

        LLMatrix4 mat4_proxy;
		for (size_t matrix_i = 0; matrix_i < 4; matrix_i++)
		{
			for (size_t matrix_j = 0; matrix_j < 4; matrix_j++)
			{
				mat4_proxy.mMatrix[matrix_i][matrix_j] = bind_matrix_value[matrix_i + (matrix_j * 4)];
			}
		}
		skininfo.mBindShapeMatrix.loadu(mat4_proxy);
		// matrix multiplication order matters, so this is as clean as it gets.
		LLMatrix4a transform{normalized_transformation};
		matMul(transform, skininfo.mBindShapeMatrix, skininfo.mBindShapeMatrix);
	}

	// setup joint map
	JointMap joint_map = gAgentAvatarp->getJointAliases();

	// unfortunately getSortedJointNames clears the ref vector, and we need two extra lists.
	std::vector<std::string> extra_names, more_extra_names;
	gAgentAvatarp->getSortedJointNames(1, extra_names);
	gAgentAvatarp->getSortedJointNames(2, more_extra_names);
	extra_names.reserve(more_extra_names.size());
	extra_names.insert(extra_names.end(), more_extra_names.begin(), more_extra_names.end());

	// add the extras to jointmap
	for (auto extra_name : extra_names)
	{
		joint_map[extra_name] = extra_name;
	}

	
	//=======================================================
	// find or re-create a skeleton, deal with joint offsets
	//=======================================================

	pushLog("DAE Importer", "Preparing to process skeleton[s]...");

	// try to find a regular skeleton
	size_t skeleton_count = collada_db->getElementCount(NULL, "skeleton");
	bool use_alternative_joint_search = true;
	JointTransformMap joint_transforms;
	for (size_t skeleton_iterator = 0; skeleton_iterator < skeleton_count; ++skeleton_iterator)
	{
		daeElement* current_element = nullptr;
		collada_db->getElement(&current_element, skeleton_iterator, 0, "skeleton");
		auto current_skeleton = daeSafeCast<domInstance_controller::domSkeleton>(current_element);
		if (!current_skeleton)
		{
			continue;
		}

		auto current_skeleton_root = current_skeleton->getValue().getElement();
		if (!current_skeleton_root)
		{
			continue;
		}

		// we found at least one proper sekeleton
		use_alternative_joint_search = false;
		pushLog("DAE Importer", "Skeleton data found, attempting to process..");

		auto skeleton_nodes = current_skeleton_root->getChildrenByType<domNode>();
		for (size_t skeleton_node_iterator = 0; skeleton_node_iterator < skeleton_nodes.getCount(); ++skeleton_node_iterator)
		{
			auto current_node = daeSafeCast<domNode>(skeleton_nodes[skeleton_node_iterator]);
			if (!current_node)
			{
				continue;
			}

			// work with node here
			processSkeletonJoint(current_node, joint_map, joint_transforms);
		}
	}

	// no skeletons found, recreate one from joints
	if (use_alternative_joint_search)
	{
		pushLog("DAE Importer", "No conventional skeleton data found, attempting to recreate from joints...");
		daeElement* document_scene = collada_document_root->getDescendant("visual_scene");
		if (!document_scene)
		{
			// TODO: ERROR
			// missing skeleton set?
		}
		else
		{
			auto scene_children = document_scene->getChildren();
			for (size_t scene_child_iterator = 0; scene_child_iterator < scene_children.getCount(); ++scene_child_iterator)
			{
				auto current_node = daeSafeCast<domNode>(scene_children[scene_child_iterator]);
				if (!current_node)
				{
					continue;
				}

				// work with node here
				processSkeletonJoint(current_node, joint_map, joint_transforms);
			}
		}
	}


	// jointlist processing
	
	// moved this lambda definition out of the loop below.
	auto lambda_process_joint_name = [&skininfo, &joint_map](std::string joint_name)
	{
		// looking for internal joint name, otherwise use provided name?
		// seems weird, but ok.
		if (joint_map.find(joint_name) != joint_map.end())
		{
			joint_name = joint_map[joint_name];
			skininfo.mJointNames.push_back(JointKey::construct(joint_name));
			skininfo.mJointNums.push_back(-1);
		}
	};

	auto& list_of_jointinputs = current_skin->getJoints()->getInput_array();
	for (size_t joint_input_iterator = 0; joint_input_iterator < list_of_jointinputs.getCount(); ++joint_input_iterator)
	{

		auto current_element = list_of_jointinputs[joint_input_iterator]->getSource().getElement();
		auto current_source = daeSafeCast<domSource>(current_element);
		if (!current_source)
		{
			pushLog("DAE Importer", "WARNING: Joint data number " + std::to_string(joint_input_iterator) + " could not be read, skipping.");
			continue;
		}

		std::string current_semantic = list_of_jointinputs[joint_input_iterator]->getSemantic();
		if (current_semantic.compare(COMMON_PROFILE_INPUT_JOINT) == 0)
		{
			// we got a list of the active joints this mesh uses
			// names can be either as an array of names or ids
			// define lambda for common code
			// moved lambda out of the loop, see above.

			auto name_source = current_source->getName_array();
			if (name_source)
			{
				auto list_of_names = name_source->getValue();
				for (size_t joint_name_iter = 0; joint_name_iter < list_of_names.getCount(); ++joint_name_iter)
				{
					std::string current_name = list_of_names.get(joint_name_iter);
					lambda_process_joint_name(current_name);
				}
			}

			else
			{
				auto id_source = current_source->getIDREF_array();
				if (!id_source)
				{
					pushLog("DAE Importer", "WARNING: Joint number " + std::to_string(joint_input_iterator) + " did not provide name or ID, skipping.");
					continue;
				}

				auto list_of_names = id_source->getValue();
				for (size_t joint_name_iter = 0; joint_name_iter < list_of_names.getCount(); ++joint_name_iter)
				{
					std::string current_name = list_of_names.get(joint_name_iter).getID();
					lambda_process_joint_name(current_name);
				}
			}
		}

		else if (current_semantic.compare(COMMON_PROFILE_INPUT_INV_BIND_MATRIX) == 0)
		{
			// we got an inverse bind matrix
			auto float_array = current_source->getFloat_array();
			if (!float_array)
			{
				pushLog("DAE Importer", "WARNING: Inverse bind matrix not found, attempting to skip.");
				continue;
			}

			auto& current_transform = float_array->getValue();
			for (size_t transform_matrix_iterator = 0; transform_matrix_iterator < (current_transform.getCount() / 16); ++transform_matrix_iterator)
			{
				LLMatrix4 current_matrix;
				for (size_t matrix_pos_i = 0; matrix_pos_i < 4; matrix_pos_i++)
				{
					for (size_t matrix_pos_j = 0; matrix_pos_j < 4; matrix_pos_j++)
					{
						current_matrix.mMatrix[matrix_pos_i][matrix_pos_j] = current_transform
							[(transform_matrix_iterator * 16) + matrix_pos_i + (matrix_pos_j * 4)];
					}
				}

				skininfo.mInvBindMatrix.push_back(LLMatrix4a(current_matrix));
			}
		}
	}

	int jointname_number_iter = 0;
	for (auto jointname_iterator = skininfo.mJointNames.begin(); jointname_iterator != skininfo.mJointNames.end(); ++jointname_iterator, ++jointname_number_iter)
	{
		std::string name_lookup = jointname_iterator->mName;
		if (joint_map.find(name_lookup) == joint_map.end())
		{
			pushLog("DAE Importer", "WARNING: Unknown joint named " + name_lookup + " found, skipping over it.");
			continue;
		}

		if (skininfo.mInvBindMatrix.size() <= jointname_number_iter)
		{
			// doesn't seem like a critical fail that should invalidate the entire skin, just break and move on?
			pushLog("DAE Importer", "WARNING: Requesting out of bounds joint named  " + name_lookup);
			break;
		}

		LLMatrix4 newinverse = LLMatrix4(skininfo.mInvBindMatrix[jointname_number_iter].getF32ptr());
		auto joint_translation = joint_transforms[name_lookup].getTranslation();
		newinverse.setTranslation(joint_translation);
		skininfo.mAlternateBindMatrix.push_back( LLMatrix4a(newinverse) );
	}

	size_t bind_count = skininfo.mAlternateBindMatrix.size();
	if ((bind_count > 0) && (bind_count != skininfo.mJointNames.size()))
	{
		// different number of binds vs jointnames, i hestiate to fail the entire skinmap over this
		// because it can just be a case of a few additional joints being ignored, unless i'm missing something?
		pushLog("DAE Importer", "WARNING: " + std::to_string(skininfo.mJointNames.size()) + " joints were found, but " + std::to_string(bind_count) + " binds matrices were made.");
	}

	//==============================
	// transform vtx positions
	//==============================

	// transform positions
	auto raw_vertex_array = current_mesh->getVertices();
	if (!raw_vertex_array)
	{
		pushLog("DAE Importer", "ERROR: Failed to read the vertex array.");
		return false;
	}

	std::vector<LLVector4> transformed_positions;
	auto vertex_input_array = raw_vertex_array->getInput_array();

	for (size_t vertex_input_iterator = 0; vertex_input_iterator < vertex_input_array.getCount(); ++vertex_input_iterator)
	{
		std::string current_semantic = vertex_input_array[vertex_input_iterator]->getSemantic();
		if (current_semantic.compare(COMMON_PROFILE_INPUT_POSITION) != 0)
		{
			// if what we got isn't a position array - skip.
			continue;
		}

		if (!transformed_positions.empty())
		{
			// just in case we somehow got multiple valid position arrays
			break;
		}

		auto pos_source = daeSafeCast<domSource>(vertex_input_array[vertex_input_iterator]->getSource().getElement());
		if (!pos_source)
		{
			// not a valid position array, no need to bother the user wit it though.
			continue;
		}

		auto pos_array = pos_source->getFloat_array();
		if (!pos_array)
		{
			continue;
		}

		auto& vertex_positions = pos_array->getValue();
		for (size_t vtx_position_iterator = 0; vtx_position_iterator < vertex_positions.getCount(); vtx_position_iterator += 3)
		{
			if (vertex_positions.getCount() <= (vtx_position_iterator + 2))
			{
				pushLog("DAE Importer", "ERROR: Position array request out of bound.");
				break;
			}

			LLVector3 temp_pos
			(
				vertex_positions[vtx_position_iterator],
				vertex_positions[vtx_position_iterator + 1],
				vertex_positions[vtx_position_iterator + 2]
			);

			temp_pos = temp_pos * inverse_normalized_transformation;

			LLVector4 new_vector;
			new_vector.set(temp_pos[0], temp_pos[1], temp_pos[2]);
			transformed_positions.push_back(new_vector);
		}

	}

	//==============================
	// extract weights and limit to 4
	//==============================
	auto current_weights = current_skin->getVertex_weights();
	if (!current_weights)
	{
		pushLog("DAE Importer", "ERROR: Failed to read the weights array, stopping.");
		return false;
	}

	auto weight_inputs = current_weights->getInput_array();
	domFloat_array* vertex_weights = nullptr;

	for (size_t weight_input_iter = 0; weight_input_iter < weight_inputs.getCount(); ++weight_input_iter)
	{
		std::string current_semantic = weight_inputs[weight_input_iter]->getSemantic();
		if (current_semantic.compare(COMMON_PROFILE_INPUT_WEIGHT) == 0)
		{
			auto weights_source = daeSafeCast<domSource>(weight_inputs[weight_input_iter]->getSource().getElement());
			if (!weights_source)
			{
				continue;
			}

			vertex_weights = weights_source->getFloat_array();
			break;
		}
	}

	if (!vertex_weights)
	{
		pushLog("DAE Importer", "ERROR: Failed to find valid weight data, stopping.");
		return false;
	}

	// vcount - in this case defines how many joints influence each vtx
	// v - joint indices

	auto& weight_values = vertex_weights->getValue();
	auto& joint_influence_count = current_weights->getVcount()->getValue();
	auto& joint_weight_indices = current_weights->getV()->getValue();
	std::map<LLVector4, std::vector<LLModel::JointWeight> > skinweight_data;

	size_t joint_weight_strider = 0;
	for (size_t joint_iterator = 0; joint_iterator < joint_influence_count.getCount(); ++joint_iterator)
	{
		auto influencees_count = joint_influence_count[joint_iterator];
		LLModel::weight_list full_weight_list;
		LLModel::weight_list sorted_weight_list;

		// extract all of the weights
		for (size_t influence_iter = 0; influence_iter < influencees_count; ++influence_iter)
		{
			int joint_idx = joint_weight_indices[joint_weight_strider++];
			int weight_idx = joint_weight_indices[joint_weight_strider++];

			if (joint_idx == -1)
			{
				continue;
			}

			float weight_value = weight_values[weight_idx];
			full_weight_list.push_back(LLModel::JointWeight(joint_idx, weight_value));
		}

		// sort by large-to-small
		std::sort(full_weight_list.begin(), full_weight_list.end(), LLModel::CompareWeightGreater());

		
		// limit to 4, and normalize the result
		F32 total = 0.f;

		for (U32 i = 0; i < llmin((U32)4, (U32)full_weight_list.size()); ++i)
		{ //take up to 4 most significant weights
			if (full_weight_list[i].mWeight > 0.f)
			{
				sorted_weight_list.push_back(full_weight_list[i]);
				total += full_weight_list[i].mWeight;
			}
		}

		F32 scale = 1.f / total;
		if (scale != 1.f)
		{ //normalize weights
			for (U32 i = 0; i < sorted_weight_list.size(); ++i)
			{
				sorted_weight_list[i].mWeight *= scale;
			}
		}

		skinweight_data[transformed_positions[joint_iterator]] = sorted_weight_list;
	}

	//==============================
	// map weights+joints to corresponding vertices
	//==============================

	// compare that allows for some deviation, needed below
	auto soft_compare = [](LLVector4 vec_a, LLVector4 vec_b, float epsilon) -> bool
	{
		bool result = false;

		if ( (fabs(vec_a.mV[0] - vec_b.mV[0]) < epsilon) &&
			 (fabs(vec_a.mV[1] - vec_b.mV[1]) < epsilon) &&
			 (fabs(vec_a.mV[2] - vec_b.mV[2]) < epsilon) )
		{
			result = true;
		}

		return result;
	};

	auto& faces = current_object->getFaces(mLod);
	for (auto& current_face : faces)
	{
		auto& positions = current_face->getPositions();
		auto& weights = current_face->getSkin();

		for (auto& current_position : positions)
		{	
			int found_iterator = -1;

			for (size_t internal_position_iter = 0; internal_position_iter < transformed_positions.size(); ++internal_position_iter)
			{
				auto& internal_position = transformed_positions[internal_position_iter];
				if (soft_compare(current_position, internal_position, F_ALMOST_ZERO))
				{
					found_iterator = internal_position_iter;
					break;
				}
			}

			if (found_iterator < 0)
			{
				continue;
			}

			auto cjoints = skinweight_data[transformed_positions[found_iterator]];

			LLLocalMeshFace::LLLocalMeshSkinUnit new_wght;

			// first init all joints to -1, in case below we get less than 4 influences.
			for (size_t tjidx = 0; tjidx < 4; ++tjidx)
			{
				new_wght.mJointIndices[tjidx] = -1;
			}

			/*
				NOTE: LL uses a single float value to indicate joint num and weight;
				for example: 7.55 means the vtx is affected by joint number 7, with the weight of 55.
				that of course limits max weight to 0.999f as not to actually change joint number.

				since LL uses that format, that's how i choose to store it to avoid confusion further down the pipeline.
			*/
			for (size_t jidx = 0; jidx < cjoints.size(); ++jidx)
			{
				auto cjoint = cjoints[jidx];
				
				new_wght.mJointIndices[jidx] = cjoint.mJointIdx;
				new_wght.mJointWeights[jidx] = llclamp((F32)cjoint.mWeight, 0.f, 0.999f);
			}

			weights.push_back(new_wght);
		}
	}

	return true;
}

void LLLocalMeshImportDAE::processSkeletonJoint(domNode* current_node, std::map<std::string, std::string>& joint_map, std::map<std::string, LLMatrix4>& joint_transforms)
{
	// safety checks & name check
	auto node_name = current_node->getName();
	if (!node_name)
	{
		return;
	}

	auto jointmap_iter = joint_map.find(node_name);
	if (jointmap_iter == joint_map.end())
	{
		return;
	}

	// begin actual joint work
	domTranslate* current_transformation = nullptr;

	daeSIDResolver jointResolver_translation(current_node, "./translate");
	current_transformation = daeSafeCast<domTranslate>(jointResolver_translation.getElement());

	if (!current_transformation)
	{
		daeSIDResolver jointResolver_location(current_node, "./location");
		current_transformation = daeSafeCast<domTranslate>(jointResolver_location.getElement());
	}

	if (!current_transformation)
	{
		daeElement* child_translate_element = current_node->getChild("translate");
		if (child_translate_element)
		{
			current_transformation = daeSafeCast<domTranslate>(child_translate_element);
		}
	}

	if (!current_transformation) // no queries worked
	{
		daeSIDResolver jointResolver_matrix(current_node, "./matrix");
		auto joint_transform_matrix = daeSafeCast<domMatrix>(jointResolver_matrix.getElement());
		if (joint_transform_matrix)
		{
			LLMatrix4 workingTransform;
			domFloat4x4 domArray = joint_transform_matrix->getValue();
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					workingTransform.mMatrix[i][j] = domArray[i + j * 4];
				}
			}
			// LLVector3 trans = workingTransform.getTranslation();
			joint_transforms[node_name] = workingTransform;
		}
	}

	else // previous query worked
	{
		domFloat3 joint_transform = current_transformation->getValue();
		LLVector3 singleJointTranslation(joint_transform[0], joint_transform[1], joint_transform[2]);
		LLMatrix4 workingtransform;
		workingtransform.setTranslation(singleJointTranslation);

		joint_transforms[node_name] = workingtransform;
	}
	// end actual joint work

	// get children to work on
	auto current_node_children = current_node->getChildren();
	for (size_t node_children_iter = 0; node_children_iter < current_node_children.getCount(); ++node_children_iter)
	{
		auto current_child_node = daeSafeCast<domNode>(current_node_children[node_children_iter]);
		if (!current_child_node)
		{
			continue;
		}

		processSkeletonJoint(current_child_node, joint_map, joint_transforms);
	}
}


bool LLLocalMeshImportDAE::readMesh_CommonElements(const domInputLocalOffset_Array& inputs, 
	int& offset_position, int& offset_normals, int& offset_uvmap, int& index_stride, 
	domSource*& source_position, domSource*& source_normals, domSource*& source_uvmap)
{
	index_stride = 0;

	for (size_t input_iterator = 0; input_iterator < inputs.getCount(); ++input_iterator)
	{
		index_stride = std::max((int)inputs[input_iterator]->getOffset(), index_stride);
		std::string current_semantic = inputs[input_iterator]->getSemantic();

		// vertex array
		if (current_semantic.compare(COMMON_PROFILE_INPUT_VERTEX) == 0)
		{
			daeElementRef current_element = inputs[input_iterator]->getSource().getElement();
			domVertices* input_vertices = daeSafeCast<domVertices>(current_element);
			if (!input_vertices)
			{
				pushLog("DAE Importer", "Collada file error, vertex array not found.");
				return false;
			}

			domInputLocal_Array& input_vertices_array = input_vertices->getInput_array();
			for (size_t vertex_input_iterator = 0; vertex_input_iterator < input_vertices_array.getCount(); ++vertex_input_iterator)
			{
				std::string current_vertex_semantic = input_vertices_array[vertex_input_iterator]->getSemantic();

				// vertex positions array
				if (current_vertex_semantic.compare(COMMON_PROFILE_INPUT_POSITION) == 0)
				{
					offset_position = inputs[input_iterator]->getOffset();
					daeElementRef current_element = input_vertices_array[vertex_input_iterator]->getSource().getElement();
					source_position = daeSafeCast<domSource>(current_element);
				}

				// vertex normal array
				else if (current_vertex_semantic.compare(COMMON_PROFILE_INPUT_NORMAL) == 0)
				{
					offset_normals = inputs[input_iterator]->getOffset();
					daeElementRef current_element = input_vertices_array[vertex_input_iterator]->getSource().getElement();
					source_normals = daeSafeCast<domSource>(current_element);
				}
			}

		}

		// normal array outside of vertex array
		else if (current_semantic.compare(COMMON_PROFILE_INPUT_NORMAL) == 0)
		{
			offset_normals = inputs[input_iterator]->getOffset();
			daeElementRef current_element = inputs[input_iterator]->getSource().getElement();
			source_normals = daeSafeCast<domSource>(current_element);
		}

		// uv array
		else if (current_semantic.compare(COMMON_PROFILE_INPUT_TEXCOORD) == 0)
		{
			offset_uvmap = inputs[input_iterator]->getOffset();
			daeElementRef current_element = inputs[input_iterator]->getSource().getElement();
			source_uvmap = daeSafeCast<domSource>(current_element);
		}
	}

	index_stride += 1;

	if (!source_position)
	{
		pushLog("DAE Importer", "Collada file error, position array not found.");
		return false;
	}

	if (!source_position->getFloat_array())
	{
		pushLog("DAE Importer", "Collada file error, position array is corrupt.");
		return false;
	}
	
	return true;
}

std::string LLLocalMeshImportDAE::getElementName(daeElement* element_current, int fallback_index)
{
	std::string result;

	// why lambda - we first use it on the element itself, if no result - we use it on the element's parent.
	auto lambda_ask_element = [](daeElement* element_current) -> std::string
	{
		std::string lambda_result;

		/*
			the reason we want BOTH name and id is because collada dom is rather bad
			at reading NAME from blender files, and is equally as bad at reading ID from anywhere else.
		*/

		lambda_result = element_current->getAttribute("name");
		if (element_current->getID())
		{
			// if we already have name, let's add a separator
			if (!lambda_result.empty())
			{
				lambda_result += " | ";
			}

			lambda_result += element_current->getID();
		}

		return lambda_result;
	};

	if (element_current)
	{
		// first ask the given element
		result = lambda_ask_element(element_current);
		
		// if got no name, ask parent
		if (result.empty())
		{
			auto parent = element_current->getParent();
			if (parent)
			{
				result = lambda_ask_element(parent);
			}
		}
	}

	// in case all else fails, fallback naming
	if (result.empty())
	{
		result = "object_" + std::to_string(fallback_index);
	}

	return result;
}

void LLLocalMeshImportDAE::pushLog(std::string who, std::string what)
{
	std::string log_msg = "[ " + who + " ] ";

	log_msg += what;
	mLoadingLog.push_back(log_msg);
}


bool LLLocalMeshImportDAE::readMesh_Triangle(LLLocalMeshFace* data_out, const domTrianglesRef& data_in)
{
	if (!data_out)
	{
		pushLog("DAE Importer", "LLLocalMeshFace pointer is NULL.");
		return false;
	}

	if (!data_in)
	{
		pushLog("DAE Importer", "domTrianglesRef is NULL.");
		return false;
	}

	const domInputLocalOffset_Array& inputs = data_in->getInput_array();

	int offset_position = -1;
	int offset_normals = -1;
	int offset_uvmap = -1;
	int index_stride = 0;

	domSource* source_position = nullptr;
	domSource* source_normals = nullptr;
	domSource* source_uvmap = nullptr;

	// success implies source_position is not null and has gettable float array.
	bool element_success = readMesh_CommonElements(inputs, offset_position, offset_normals, offset_uvmap,
		index_stride, source_position, source_normals, source_uvmap);

	if (!element_success)
	{
		pushLog("DAE Importer", "Collada file error, could not read array positions.");
		return false;
	}

	domListOfFloats empty;
	domListOfFloats& serialized_values_position = source_position->getFloat_array()->getValue();
	domListOfFloats& serialized_values_normals = source_normals ? source_normals->getFloat_array()->getValue() : empty;
	domListOfFloats& serialized_values_uvmap = source_uvmap ? source_uvmap->getFloat_array()->getValue() : empty;
	domListOfUInts& triangle_list = data_in->getP()->getValue();
	/* 
		we don't get/need vcount with triangles since they're, uh, limited to 3 verts.
		so P in this case is just an array of vtx indices linearly describing triangles
		like [ 0 1 2  3 4 5  4 6 7  0 9 10 ] and so on. thanks, fancy collada book.
	*/

	if (serialized_values_position.getCount() == 0)
	{
		pushLog("DAE Importer", "Collada file error, vertex position array is empty.");
		return false;
	}

	auto& list_indices = data_out->getIndices();
	auto& list_positions = data_out->getPositions();
	auto& list_normals = data_out->getNormals();
	auto& list_uvs = data_out->getUVs();

	LLVector4 initial_bbox_values;
	initial_bbox_values.set
	(
		(float)serialized_values_position[0],
		(float)serialized_values_position[1],
		(float)serialized_values_position[2]
	);
	data_out->setFaceBoundingBox(initial_bbox_values, true);

	// used to track verts of repeat pos but different uv/normal attributes.
	std::vector<LLVector4> repeat_map_position_iterable;

	struct repeat_map_data_struct
	{
		LLVector4	vtx_normal_data;
		LLVector2	vtx_uv_data;
		int			vtx_index;
	};

	std::vector<std::vector<repeat_map_data_struct>> repeat_map_data;

	// iterate per vertex
	for (size_t triangle_iterator = 0; triangle_iterator < triangle_list.getCount(); triangle_iterator += index_stride)
	{
		LLVector4 attr_position;
		LLVector4 attr_normal;
		LLVector2  attr_uv;

		// position attribute
		attr_position.set
		(
			serialized_values_position[triangle_list[triangle_iterator + offset_position] * 3 + 0],
			serialized_values_position[triangle_list[triangle_iterator + offset_position] * 3 + 1],
			serialized_values_position[triangle_list[triangle_iterator + offset_position] * 3 + 2],
			0
		);

		// normal attribute
		if (source_normals)
		{
			attr_normal.set
			(
				serialized_values_normals[triangle_list[triangle_iterator + offset_normals] * 3 + 0],
				serialized_values_normals[triangle_list[triangle_iterator + offset_normals] * 3 + 1],
				serialized_values_normals[triangle_list[triangle_iterator + offset_normals] * 3 + 2],
				0
			);
		}

		// uv attribute
		if (source_uvmap)
		{
			attr_uv.set
			(
				serialized_values_uvmap[triangle_list[triangle_iterator + offset_uvmap] * 2 + 0],
				serialized_values_uvmap[triangle_list[triangle_iterator + offset_uvmap] * 2 + 1]
			);
		}


		// check if this vertex data already exists,
		// if yes - just push in a new index that points where the original data is found.
		bool repeat_found = false;

		// check if we've already seen a vertex with this position value
		auto seeker_position = std::find(repeat_map_position_iterable.begin(), repeat_map_position_iterable.end(), attr_position);
		if (seeker_position != repeat_map_position_iterable.end())
		{
			// compare to check if you find one with matching normal and uv values
			size_t seeker_index = std::distance(repeat_map_position_iterable.begin(), seeker_position);
			for (auto repeat_vtx_data : repeat_map_data[seeker_index])
			{
				if (repeat_vtx_data.vtx_normal_data != attr_normal)
				{
					continue;
				}

				if (repeat_vtx_data.vtx_uv_data != attr_uv)
				{
					continue;
				}

				// found a vertex whose position, normals and uvs match what we already have
				int repeated_index = repeat_vtx_data.vtx_index;
				int list_indices_size = list_indices.size();
				int triangle_check = list_indices_size % 3;

				if (((triangle_check < 1) || (list_indices[list_indices_size - 1] != repeated_index))
					&& ((triangle_check < 2) || (list_indices[list_indices_size - 2] != repeated_index)))
				{
					repeat_found = true;
					list_indices.push_back(repeated_index);
				}

				break;
			}

		}

		// the vertex data is actually new, let's push it in together with it's new index
		if (!repeat_found)
		{
			// check if vertex data list + 1 would overflow 65535 size
			if ((list_positions.size() + 1) >= 65535)
			{
				pushLog("DAE Importer", "Face error, too many vertices. Do NOT exceed 65535 vertics per face.");
				return false;
			}

			// update bounding box
			data_out->setFaceBoundingBox(attr_position);

			// push back vertex data
			list_positions.push_back(attr_position);
			if (source_normals)
			{
				list_normals.push_back(attr_normal);
			}

			if (source_uvmap)
			{
				list_uvs.push_back(attr_uv);
			}

			// index is verts list - 1, push that index into indices
			int current_index = list_positions.size() - 1;
			list_indices.push_back(current_index);

			// track this position for future repeats
			repeat_map_data_struct new_trackable;
			new_trackable.vtx_normal_data = attr_normal;
			new_trackable.vtx_uv_data = attr_uv;
			new_trackable.vtx_index = current_index;

			if (seeker_position != repeat_map_position_iterable.end())
			{
				int seeker_index = std::distance(repeat_map_position_iterable.begin(), seeker_position);
				repeat_map_data[seeker_index].push_back(new_trackable);
			}
			else
			{
				repeat_map_position_iterable.push_back(attr_position);
				std::vector<repeat_map_data_struct> new_map_data_vec;
				new_map_data_vec.push_back(new_trackable);
				repeat_map_data.push_back(new_map_data_vec);
			}
		}

		if (list_positions.empty())
		{
			pushLog("DAE Importer", "Face error, no valid vertex positions found.");
			return false;
		}

	}

	return true;
}

bool LLLocalMeshImportDAE::readMesh_Polylist(LLLocalMeshFace* data_out, const domPolylistRef& data_in)
{
	if (!data_out)
	{
		pushLog("DAE Importer", "LLLocalMeshFace pointer is NULL.");
		return false;
	}

	if (!data_in)
	{
		pushLog("DAE Importer", "domPolylistRef is NULL.");
		return false;
	}

	const domInputLocalOffset_Array& inputs = data_in->getInput_array();

	int offset_position = -1;
	int offset_normals = -1;
	int offset_uvmap = -1;
	int index_stride = 0;

	domSource* source_position = nullptr;
	domSource* source_normals = nullptr;
	domSource* source_uvmap = nullptr;

	// success implies source_position is not null and has gettable float array.
	bool element_success = readMesh_CommonElements(inputs, offset_position, offset_normals, offset_uvmap,
		index_stride, source_position, source_normals, source_uvmap);

	if (!element_success)
	{
		pushLog("DAE Importer", "Collada file error, could not read array positions.");
		return false;
	}

	domListOfFloats empty;
	domListOfFloats& serialized_values_position = source_position->getFloat_array()->getValue();
	domListOfFloats& serialized_values_normals = source_normals ? source_normals->getFloat_array()->getValue() : empty;
	domListOfFloats& serialized_values_uvmap = source_uvmap ? source_uvmap->getFloat_array()->getValue() : empty;

	// vcount in <polylist> indicates the amount of vertices per prim,
	// with the added benefit of getCount() of vcount gets us the number of primitives in this polylist.
	domListOfUInts& list_primitives = data_in->getVcount()->getValue();

	// P indicates the vertex index, so a prim 0 of [vcount containing 4 verts] means 4 first indices here.
	domListOfUInts& vertex_indices = data_in->getP()->getValue();


	if (serialized_values_position.getCount() == 0)
	{
		pushLog("DAE Importer", "Collada file error, vertex position array is empty.");
		return false;
	}

	auto& list_indices = data_out->getIndices();
	auto& list_positions = data_out->getPositions();
	auto& list_normals = data_out->getNormals();
	auto& list_uvs = data_out->getUVs();

	LLVector4 initial_bbox_values;
	initial_bbox_values.set
	(
		(float)serialized_values_position[0],
		(float)serialized_values_position[1],
		(float)serialized_values_position[2]
	);
	data_out->setFaceBoundingBox(initial_bbox_values, true);

	// used to track verts of repeat pos but different uv/normal attributes.
	std::vector<LLVector4> repeat_map_position_iterable;

	struct repeat_map_data_struct
	{
		LLVector4	vtx_normal_data;
		LLVector2	vtx_uv_data;
		int			vtx_index;
	};

	std::vector<std::vector<repeat_map_data_struct>> repeat_map_data;

	// each time we sample an index vtx, this goes up by one, to keep an idx of where we are
	int global_vtx_index = 0;

	// iterate per primtiive
	for (size_t prim_iterator = 0; prim_iterator < list_primitives.getCount(); ++prim_iterator)
	{
		int first_idx = 0;
		int last_idx = 0;

		size_t num_vertices = list_primitives[prim_iterator];

		// iterate per vertex
		for (size_t vtx_iter = 0; vtx_iter < num_vertices; ++vtx_iter)
		{
			int current_vtx_index = global_vtx_index;
			global_vtx_index += index_stride;

			LLVector4 attr_position;
			LLVector4 attr_normal;
			LLVector2 attr_uv;

			// position attribute
			attr_position.set
			(
				serialized_values_position[vertex_indices[current_vtx_index + offset_position] * 3 + 0],
				serialized_values_position[vertex_indices[current_vtx_index + offset_position] * 3 + 1],
				serialized_values_position[vertex_indices[current_vtx_index + offset_position] * 3 + 2],
				0
			);

			// normal attribute
			if (source_normals)
			{
				attr_normal.set
				(
					serialized_values_normals[vertex_indices[current_vtx_index + offset_normals] * 3 + 0],
					serialized_values_normals[vertex_indices[current_vtx_index + offset_normals] * 3 + 1],
					serialized_values_normals[vertex_indices[current_vtx_index + offset_normals] * 3 + 2],
					0
				);
			}

			// uv attribute
			if (source_uvmap)
			{
				attr_uv.set
				(
					serialized_values_uvmap[vertex_indices[current_vtx_index + offset_uvmap] * 2 + 0],
					serialized_values_uvmap[vertex_indices[current_vtx_index + offset_uvmap] * 2 + 1]
				);
			}

			// check if this vertex data already exists,
			// if yes - just push in a new index that points where the original data is found.
			bool repeat_found = false;

			// check if we've already seen a vertex with this position value
			auto seeker_position = std::find(repeat_map_position_iterable.begin(), repeat_map_position_iterable.end(), attr_position);
			if (seeker_position != repeat_map_position_iterable.end())
			{
				// compare to check if you find one with matching normal and uv values
				int seeker_index = std::distance(repeat_map_position_iterable.begin(), seeker_position);
				for (auto repeat_vtx_data : repeat_map_data[seeker_index])
				{
					if (repeat_vtx_data.vtx_normal_data != attr_normal)
					{
						continue;
					}

					if (repeat_vtx_data.vtx_uv_data != attr_uv)
					{
						continue;
					}

					// found a vertex whose position, normals and uvs match what we already have
					repeat_found = true;
					int repeated_index = repeat_vtx_data.vtx_index;

					if (vtx_iter == 0)
					{
						first_idx = repeated_index;
					}
					else if (vtx_iter == 1)
					{
						last_idx = repeated_index;
					}
					else
					{
						// copied basically verbatim off of daeloader, i could sit and figure out why
						// this works, but i have to release this thing some time this century.
						list_indices.push_back(first_idx);
						list_indices.push_back(last_idx);
						list_indices.push_back(repeated_index);
						last_idx = repeated_index;
					}
					
					break;
				}
			}

			if (!repeat_found)
			{
				// check if vertex data list + 1 would overflow 65535 size
				if ((list_positions.size() + 1) >= 65535)
				{
					pushLog("DAE Importer", "Face error, too many vertices. Do NOT exceed 65535 vertics per face.");
					return false;
				}

				// update bounding box
				data_out->setFaceBoundingBox(attr_position);

				// push back vertex data
				list_positions.push_back(attr_position);
				if (source_normals)
				{
					list_normals.push_back(attr_normal);
				}

				if (source_uvmap)
				{
					list_uvs.push_back(attr_uv);
				}
							   				 
				// index is verts list - 1, push that index into indices
				int current_index = list_positions.size() - 1;

				if (vtx_iter == 0)
				{
					first_idx = current_index;
				}
				else if (vtx_iter == 1)
				{
					last_idx = current_index;
				}
				else
				{
					list_indices.push_back(first_idx);
					list_indices.push_back(last_idx);
					list_indices.push_back(current_index);
					last_idx = current_index;
				}

				// track this position for future repeats
				repeat_map_data_struct new_trackable;
				new_trackable.vtx_normal_data = attr_normal;
				new_trackable.vtx_uv_data = attr_uv;
				new_trackable.vtx_index = current_index;

				if (seeker_position != repeat_map_position_iterable.end())
				{
					int seeker_index = std::distance(repeat_map_position_iterable.begin(), seeker_position);
					repeat_map_data[seeker_index].push_back(new_trackable);
				}
				else
				{
					repeat_map_position_iterable.push_back(attr_position);
					std::vector<repeat_map_data_struct> new_map_data_vec;
					new_map_data_vec.push_back(new_trackable);
					repeat_map_data.push_back(new_map_data_vec);
				}
			}
		}

		if (list_positions.empty())
		{
			pushLog("DAE Importer", "Face error, no valid vertex positions found.");
			return false;
		}
	}

	return true;
}

//bool LLLocalMeshImportDAE::readMesh_Polygons(LLLocalMeshFace* data_out, const domPolygonsRef& data_in)
//{
	/*
		i couldn't find any collada files of this type to test on
		this type may have been deprecated?
	*/

	// ok so.. in here vcount should be a number of polys, EACH poly should have it's own P (array of vtx indices)

	// return false // gotta return a thing

//}