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
#include "llviewercontrol.h" // for gSavedSettings
#include <llmodelloader.h>
#include "llvoavatarself.h"
#include <lldaeloader.h> // for preProcessDAE
#include <llerror.h>

/* dae headers*/
#include <dae.h>
#include <dom/domConstants.h>
#include <dom/domMesh.h>
#include <dom/domSkin.h>
#include <dom/domGeometry.h>
#include <dom/domInstance_controller.h>
#include <dom/domNode.h>

LLLocalMeshImportDAE::loadFile_return LLLocalMeshImportDAE::loadFile(LLLocalMeshFile* data, LLLocalMeshFileLOD lod)
{
    pushLog("DAE Importer", "Starting");
    LL_DEBUGS("LocalMesh") << "DAE Importer: Starting" << LL_ENDL;

    // instantiate collada objects
    DAE          collada_core;
    domCOLLADA*  collada_dom            = nullptr;
    daeDocument* collada_document       = nullptr;
    daeElement*  collada_document_root  = nullptr;
    daeDatabase* collada_db             = nullptr;
    std::string  filename               = data->getFilename(lod);
    mLod                                = lod;
    mLoadingLog.clear();

    // open file and check if opened
    if (gSavedSettings.getBOOL("ImporterPreprocessDAE"))
    {
        LL_DEBUGS("LocalMesh") << "Performing dae preprocessing" << LL_ENDL;
        collada_dom = collada_core.openFromMemory(filename, LLDAELoader::preprocessDAE(filename).c_str());
    }
    else
    {
        LL_INFOS() << "Skipping dae preprocessing" << LL_ENDL;
        collada_dom = collada_core.open(filename);
    }

    if (!collada_dom)
    {
        pushLog("DAE Importer", "Collada DOM instance could not initialize.", true);
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

    scene_transform_base.setIdentity();
    if(!always_use_meter_scale)
    {
        domAsset::domUnit* unit = daeSafeCast<domAsset::domUnit>(collada_document_root->getDescendant(daeElement::matchType(domAsset::domUnit::ID())));
        if (unit)
        {
            F32 meter = (F32)unit->getMeter();
            scene_transform_base.mMatrix[0][0] = meter;
            scene_transform_base.mMatrix[1][1] = meter;
            scene_transform_base.mMatrix[2][2] = meter;
        }
    }

    //get up axis rotation
    LLMatrix4 rotation;

    domUpAxisType up = UPAXISTYPE_Y_UP;  // default in Collada is Y_UP
    domAsset::domUp_axis* up_axis =
        daeSafeCast<domAsset::domUp_axis>(collada_document_root->getDescendant(daeElement::matchType(domAsset::domUp_axis::ID())));

    if (up_axis)
    {
        up = up_axis->getValue();
    }

    if (up == UPAXISTYPE_X_UP)
    {
        LL_DEBUGS("LocalMesh") << "Up axis is X_UP, setting up axis to Z" << LL_ENDL;
        rotation.initRotation(0.0f, 90.0f * DEG_TO_RAD, 0.0f);
    }
    else if (up == UPAXISTYPE_Y_UP)
    {
        LL_DEBUGS("LocalMesh") << "Up axis is Y_UP, setting up axis to Z" << LL_ENDL;
        rotation.initRotation(90.0f * DEG_TO_RAD, 0.0f, 0.0f);
    }
    else if (up == UPAXISTYPE_Z_UP)
    {
        LL_DEBUGS("LocalMesh") << "Up axis is Z_UP, not changed" << LL_ENDL;
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
    else
    {
        LL_DEBUGS("LocalMesh") << "Collada document contained " << mesh_amount << " MESH instances." << LL_ENDL;
    }
    LL_DEBUGS("LocalMesh") << "Collada document contained " << skin_amount << " SKIN instances." << LL_ENDL;

    std::vector<domMesh*> mesh_usage_tracker;

    // [ iterate over OBJECTS ]
    for (size_t mesh_index = 0; mesh_index < mesh_amount; ++mesh_index)
    {
        pushLog("DAE Importer", "Parsing object number " + std::to_string(mesh_index));

        daeElement* mesh_current_elem = nullptr;
        collada_db->getElement(&mesh_current_elem, static_cast<daeInt>(mesh_index), NULL, COLLADA_TYPE_MESH);
        domMesh* mesh_current = daeSafeCast<domMesh>(mesh_current_elem);
        if (!mesh_current)
        {
            pushLog("DAE Importer", "Could not dereference the object, skipping.");
            continue;
        }

        // this WILL always return with some kind of a name, even a fallback one of "object_[idx]"
        std::string object_name = getElementName(mesh_current_elem, static_cast<int>(mesh_index));
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
        collada_db->getElement(&skin_current_elem, static_cast<daeInt>(skin_index), NULL, COLLADA_TYPE_SKIN);
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
            pushLog("DAE Importer", "Skin associated geometry container has no mesh, skipping.");
            continue;
        }

        // find the associated lllocalmeshobject linked to this mesh
        int current_object_iter = -1;
        for (size_t mesh_tracker_iterator = 0; mesh_tracker_iterator < mesh_usage_tracker.size(); ++mesh_tracker_iterator)
        {
            auto iterable_mesh = mesh_usage_tracker[mesh_tracker_iterator];
            if (skin_current_mesh == iterable_mesh)
            {
                current_object_iter = static_cast<int>(mesh_tracker_iterator);
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
        current_object->logObjectInfo();
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
//  domPolygons_Array&  polygons_array = current_mesh->getPolygons_array(); // NOTE: polygon schema

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
    auto lambda_process_submesh = [&](size_t idx, submesh_type array_type)
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

// Function to load the JointMap
JointMap loadJointMap()
{
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

    return joint_map;
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
    LLPointer<LLMeshSkinInfo> skininfop = current_object->getObjectMeshSkinInfo();
    if (skininfop == nullptr)
    {
        LL_DEBUGS("LocalMesh") << "Object mesh skin info is nullptr. allocate a new skininfo." << LL_ENDL;
        try
        {
            skininfop = new LLMeshSkinInfo();
        }
        catch (const std::bad_alloc& ex)
        {
            LL_WARNS() << "Failed to allocate skin info with exception: " << ex.what()  << LL_ENDL;
            return false;
        }
    }
    // basically copy-pasted from linden magic
    LLMatrix4 normalized_transformation;
    LLMatrix4 mesh_scale;
    normalized_transformation.setTranslation(LLVector3(inverse_translation));
    mesh_scale.initScale(LLVector3(objct_size));
    mesh_scale *= normalized_transformation;
    normalized_transformation = mesh_scale;

    glm::mat4 inv_mat = glm::make_mat4((F32*)normalized_transformation.mMatrix);
    inv_mat = glm::inverse(inv_mat);
    LLMatrix4 inverse_normalized_transformation(glm::value_ptr(inv_mat));

    // bind shape matrix
    domSkin::domBind_shape_matrix* skin_current_bind_matrix = current_skin->getBind_shape_matrix();
    if (skin_current_bind_matrix)
    {
        LL_DEBUGS("LocalMesh") << "Bind shape matrix found. Applying..." << LL_ENDL;
        auto& bind_matrix_value = skin_current_bind_matrix->getValue();

        LLMatrix4 mat4_proxy;
        for (size_t matrix_i = 0; matrix_i < 4; matrix_i++)
        {
            for (size_t matrix_j = 0; matrix_j < 4; matrix_j++)
            {
                mat4_proxy.mMatrix[matrix_i][matrix_j] = (F32)bind_matrix_value[matrix_i + (matrix_j * 4)];
            }
        }
        skininfop->mBindShapeMatrix.loadu(mat4_proxy);
        // matrix multiplication order matters, so this is as clean as it gets.
        LLMatrix4a transform{normalized_transformation};
        matMul(transform, skininfop->mBindShapeMatrix, skininfop->mBindShapeMatrix);
    }

    LL_DEBUGS("LocalMesh") << "Loading Joint Map." << LL_ENDL;
    // setup joint map
    JointMap joint_map = loadJointMap();

    //=======================================================
    // find or re-create a skeleton, deal with joint offsets
    //=======================================================

    pushLog("DAE Importer", "Preparing to process skeleton[s]...");

    // try to find a regular skeleton
    size_t skeleton_count = collada_db->getElementCount(NULL, "skeleton");
    std::vector<domInstance_controller::domSkeleton*> skeletons;

    JointTransformMap joint_transforms;
    for (size_t skeleton_index = 0; skeleton_index < skeleton_count; ++skeleton_index)
    {
        daeElement* current_element = nullptr;
        daeElement* current_skeleton_root = nullptr;
        collada_db->getElement(&current_element, static_cast<daeInt>(skeleton_index), 0, "skeleton");

        auto current_skeleton = daeSafeCast<domInstance_controller::domSkeleton>(current_element);
        if (current_skeleton)
        {
            current_skeleton_root = current_skeleton->getValue().getElement();
        }

        if (current_skeleton && current_skeleton_root)
        {
            skeletons.push_back(current_skeleton);
        }

        // we found at least one proper sekeleton
    }

    if (skeletons.empty())
    {
        pushLog("DAE Importer", "No conventional skeleton data found, attempting to recreate from joints...");

        daeElement* document_scene = collada_document_root->getDescendant("visual_scene");
        if (!document_scene)
        {
            LL_WARNS() << "Could not find visual_scene element in DAE document. Skipping skinning" << LL_ENDL;
            return false;
        }
        else
        {
            auto scene_children = document_scene->getChildren();
            auto childCount = scene_children.getCount();

            // iterate through all the visual_scene and recursively through children and process any that are joints
            auto all_ok = false;
            for (size_t scene_child_index = 0; scene_child_index < childCount; ++scene_child_index)
            {
                auto current_node = daeSafeCast<domNode>(scene_children[scene_child_index]);
                if (current_node)
                {
                    all_ok = processSkeletonJoint(current_node, joint_map, joint_transforms, true);
                    if(!all_ok)
                    {
                        LL_DEBUGS("LocalMesh") << "failed to process joint: " << current_node->getName() << LL_ENDL;
                        continue;
                    }
                }
            }
            LL_DEBUGS("LocalMesh") << "All joints processed, all_ok = " << all_ok << LL_ENDL;
        }
    }
    else{
        pushLog("DAE Importer", "Found " + std::to_string(skeletons.size()) + " skeletons.");

        for( domInstance_controller::domSkeleton* current_skeleton : skeletons)
        {
            bool workingSkeleton = false;
            daeElement* current_skeleton_root = current_skeleton->getValue().getElement();
            if( current_skeleton_root )
            {
                std::stringstream ss;
                //Build a joint for the resolver to work with
                for (auto jointIt = joint_map.begin(); jointIt != joint_map.end(); ++jointIt)
                {
                    // Using stringstream to concatenate strings
                    ss << "./" << jointIt->first;
                    //Setup the resolver
                    daeSIDResolver resolver( current_skeleton_root, ss.str().c_str() );

                    //Look for the joint
                    domNode* current_joint = daeSafeCast<domNode>( resolver.getElement() );
                    if ( current_joint )
                    {
                        workingSkeleton = processSkeletonJoint(current_joint, joint_map, joint_transforms);
                        if( !workingSkeleton )
                        {
                            break;
                        }
                    }
                    ss.str("");
                }
            }
            //If anything failed in regards to extracting the skeleton, joints or translation id,
            //mention it
            if ( !workingSkeleton )
            {
                LL_WARNS("LocalMesh")<< "Partial jointmap found in asset - did you mean to just have a partial map?" << LL_ENDL;
            }
        }
    }


    // jointlist processing

    // moved this lambda definition out of the loop below.
    auto lambda_process_joint_name = [&skininfo = *skininfop, &joint_map](std::string joint_name)
    {
        // looking for internal joint name, otherwise use provided name?
        // seems weird, but ok.
        if (joint_map.find(joint_name) != joint_map.end())
        {
            LL_DEBUGS("LocalMesh") << "Found internal joint name: " << joint_name << LL_ENDL;
            joint_name = joint_map[joint_name];
            skininfo.mJointNames.push_back(JointKey::construct(joint_name));
            skininfo.mJointNums.push_back(-1);
        }
    };

    auto& list_of_jointinputs = current_skin->getJoints()->getInput_array();
    for (size_t joint_input_loop_idx = 0; joint_input_loop_idx < list_of_jointinputs.getCount(); ++joint_input_loop_idx)
    {

        auto current_element = list_of_jointinputs[joint_input_loop_idx]->getSource().getElement();
        auto current_source = daeSafeCast<domSource>(current_element);
        if (!current_source)
        {
            pushLog("DAE Importer", "WARNING: Joint data number " + std::to_string(joint_input_loop_idx) + " could not be read, skipping.");
            continue;
        }

        std::string current_semantic = list_of_jointinputs[joint_input_loop_idx]->getSemantic();
        if (current_semantic.compare(COMMON_PROFILE_INPUT_JOINT) == 0)
        {
            // we got a list of the active joints this mesh uses
            // names can be either as an array of names or ids
            // define lambda for common code
            // moved lambda out of the loop, see above.

            auto name_source = current_source->getName_array();
            if (name_source)
            {
                const auto& list_of_names = name_source->getValue();
                for (size_t joint_name_loop_index = 0; joint_name_loop_index < list_of_names.getCount(); ++joint_name_loop_index)
                {
                    std::string current_name = list_of_names.get(joint_name_loop_index);
                    lambda_process_joint_name(current_name);
                }
            }

            else
            {
                auto id_source = current_source->getIDREF_array();
                if (!id_source)
                {
                    pushLog("DAE Importer", "WARNING: Joint number " + std::to_string(joint_input_loop_idx) + " did not provide name or ID, skipping.");
                    continue;
                }

                const auto& list_of_names = id_source->getValue();
                for (size_t joint_name_loop_index = 0; joint_name_loop_index < list_of_names.getCount(); ++joint_name_loop_index)
                {
                    std::string current_name = list_of_names.get(joint_name_loop_index).getID();
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
            for (size_t matrix_index = 0; matrix_index < (current_transform.getCount() / 16); ++matrix_index)
            {
                LLMatrix4 mat;
                for (size_t i = 0; i < 4; i++)
                {
                    for (size_t j = 0; j < 4; j++)
                    {
                        mat.mMatrix[i][j] = (F32)current_transform[(matrix_index * 16) + i + (j * 4)];
                    }
                }

                skininfop->mInvBindMatrix.push_back(LLMatrix4a(mat));
            }
        }
    }

    int jointname_idx = 0;
    for (auto jointname_iterator = skininfop->mJointNames.begin(); jointname_iterator != skininfop->mJointNames.end(); ++jointname_iterator, ++jointname_idx)
    {
        std::string name_lookup = jointname_iterator->mName;
        if (joint_map.find(name_lookup) == joint_map.end())
        {
            pushLog("DAE Importer", "WARNING: Unknown joint named " + name_lookup + " found, skipping over it.");
            continue;
        }
        else
        {
            LL_DEBUGS("LocalMesh") << "Calc invBindMat for joint name: " << name_lookup << LL_ENDL;
        }

        if (skininfop->mInvBindMatrix.size() <= jointname_idx)
        {
            // doesn't seem like a critical fail that should invalidate the entire skin, just break and move on?
            pushLog("DAE Importer", "WARNING: Requesting out of bounds joint named  " + name_lookup);
            break;
        }

        LLMatrix4 newinverse = LLMatrix4(skininfop->mInvBindMatrix[jointname_idx].getF32ptr());
        const auto& joint_translation = joint_transforms[name_lookup].getTranslation();
        newinverse.setTranslation(joint_translation);
        skininfop->mAlternateBindMatrix.push_back( LLMatrix4a(newinverse) );
    }

    size_t bind_count = skininfop->mAlternateBindMatrix.size();
    if ((bind_count > 0) && (bind_count != skininfop->mJointNames.size()))
    {
        // different number of binds vs jointnames, i hestiate to fail the entire skinmap over this
        // because it can just be a case of a few additional joints being ignored, unless i'm missing something?
        pushLog("DAE Importer", "WARNING: " + std::to_string(skininfop->mJointNames.size()) + " joints were found, but " + std::to_string(bind_count) + " binds matrices were made.");
    }
    else
    {
        LL_DEBUGS("LocalMesh") << "Found " << bind_count << " bind matrices" << LL_ENDL;
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

    std::vector<LLVector4> transformed_positions; // equates to the model->mPosition vector in full loader
    const auto& vertex_input_array = raw_vertex_array->getInput_array();

    for (size_t vertex_input_index = 0; vertex_input_index < vertex_input_array.getCount(); ++vertex_input_index)
    {
        std::string current_semantic = vertex_input_array[vertex_input_index]->getSemantic();
        if (current_semantic.compare(COMMON_PROFILE_INPUT_POSITION) != 0)
        {
            // if what we got isn't a position array - skip.
            continue;
        }

        // if (!transformed_positions.empty())
        // {
        //  // just in case we somehow got multiple valid position arrays
        //  break;
        // }

        auto pos_source = daeSafeCast<domSource>(vertex_input_array[vertex_input_index]->getSource().getElement());
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

        auto& vtx_pos_list = pos_array->getValue();
        auto vtx_pos_count = vtx_pos_list.getCount();
        for (size_t vtx_pos_index = 0; vtx_pos_index < vtx_pos_count; vtx_pos_index += 3)
        {
            if (vtx_pos_count <= (vtx_pos_index + 2))
            {
                pushLog("DAE Importer", "ERROR: Position array request out of bound.");
                break;
            }

            LLVector3 pos_vec(
                (F32)vtx_pos_list[vtx_pos_index],
                (F32)vtx_pos_list[vtx_pos_index + 1],
                (F32)vtx_pos_list[vtx_pos_index + 2] );

            pos_vec = pos_vec * inverse_normalized_transformation;

            LLVector4 new_vector;
            new_vector.set(pos_vec[0], pos_vec[1], pos_vec[2]);
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

    const auto& weight_inputs = current_weights->getInput_array();
    domFloat_array* vertex_weights = nullptr;
    auto num_weight_inputs = weight_inputs.getCount();
    for (size_t weight_input_idx = 0; weight_input_idx < num_weight_inputs; ++weight_input_idx)
    {
        std::string current_semantic = weight_inputs[weight_input_idx]->getSemantic();
        if (current_semantic.compare(COMMON_PROFILE_INPUT_WEIGHT) == 0)
        {
            auto weights_source = daeSafeCast<domSource>(weight_inputs[weight_input_idx]->getSource().getElement());
            if (weights_source)
            {
                vertex_weights = weights_source->getFloat_array();
                break; // Stop on the first valid weight input.
            }
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
    auto& vtx_influence_count = current_weights->getVcount()->getValue();
    auto& joint_weight_indices = current_weights->getV()->getValue();

    std::map<LLVector4, std::vector<LLModel::JointWeight> > skinweight_data;

    size_t joint_weight_strider = 0;
    if (vtx_influence_count.getCount() > transformed_positions.size())
    {
        pushLog("DAE Importer", "WARNING: More weight entries (" 
            + std::to_string(vtx_influence_count.getCount()) 
            + ") than positions (" 
            + std::to_string(transformed_positions.size()) 
            + ").");
        LL_WARNS("LocalMesh") << "More weight entries(" << vtx_influence_count.getCount() << ") than positions("<< transformed_positions.size() << ")." << LL_ENDL;
    }
    for (size_t joint_idx = 0; joint_idx < vtx_influence_count.getCount(); ++joint_idx)
    {
        auto influences_count = vtx_influence_count[joint_idx];
        LLModel::weight_list weight_list;
        LLModel::weight_list sorted_weight_list;

        // extract all of the weights
        for (size_t influence_idx = 0; influence_idx < influences_count; ++influence_idx)
        {
            int vtx_idx = (int)joint_weight_indices[joint_weight_strider++];
            int this_weight_idx = (int)joint_weight_indices[joint_weight_strider++];

            if (vtx_idx == -1)
            {
                continue;
            }

            float weight_value = (float)weight_values[this_weight_idx];
            weight_list.push_back(LLModel::JointWeight(vtx_idx, weight_value));
        }

        // sort by large-to-small
        std::sort(weight_list.begin(), weight_list.end(), LLModel::CompareWeightGreater());

        // limit to 4, and normalize the result
        F32 total = 0.f;

        for (U32 i = 0; i < llmin((U32)4, (U32)weight_list.size()); ++i)
        { //take up to 4 most significant weights
            if (weight_list[i].mWeight > 0.f)
            {
                sorted_weight_list.push_back(weight_list[i]);
                total += weight_list[i].mWeight;
            }
        }
        LL_DEBUGS("LocalMesh") << "Vertex " << joint_idx
                            << " has total weight of " << total << LL_ENDL;
        F32 scale = 1.f / total;
        if (scale != 1.f)
        { //normalize weights
            for (U32 i = 0; i < sorted_weight_list.size(); ++i)
            {
                sorted_weight_list[i].mWeight *= scale;
            }
        }
        // Log the sorted weight influences for this vertex
        LL_DEBUGS("LocalMesh") << "Vertex " << joint_idx
                            << " has " << sorted_weight_list.size() << " weight(s): ";

        for (size_t i = 0; i < sorted_weight_list.size() && i < 4; ++i)
        {
            // Print a comma before subsequent influences
            if (i > 0)
            {
                LL_CONT << ", ";
            }

            LL_CONT << sorted_weight_list[i].mJointIdx
                    << "=" << sorted_weight_list[i].mWeight;
        }

        LL_CONT << LL_ENDL;

        skinweight_data[transformed_positions[joint_idx]] = sorted_weight_list;
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
    for (const auto& current_face : faces)
    {
        const auto& positions = current_face->getPositions();
        auto& weights = current_face->getSkin();

        for (const auto& current_position : positions)
        {
            int found_idx = -1;

            for (size_t internal_position_idx = 0; internal_position_idx < transformed_positions.size(); ++internal_position_idx)
            {
                const auto& internal_position = transformed_positions[internal_position_idx];
                if (soft_compare(current_position, internal_position, F_ALMOST_ZERO))
                {
                    found_idx = static_cast<int>(internal_position_idx);
                    break;
                }
            }

            if (found_idx < 0)
            {
                LL_DEBUGS("LocalMesh") << "Failed to find position " << current_position << " in transformed positions" << LL_ENDL;
                continue;
            }

            const auto& cjoints = skinweight_data[transformed_positions[found_idx]];

            LLLocalMeshFace::LLLocalMeshSkinUnit new_wght{};

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
                const auto& cjoint = cjoints[jidx];

                new_wght.mJointIndices[jidx] = cjoint.mJointIdx;
                new_wght.mJointWeights[jidx] = llclamp((F32)cjoint.mWeight, 0.f, 0.999f);
            }

            weights.emplace_back(new_wght);
        }
    }

    // combine mBindShapeMatrix and mInvBindMatrix into mBindPoseMatrix
    skininfop->mBindPoseMatrix.resize(skininfop->mInvBindMatrix.size());
    for (U32 i = 0; i < skininfop->mInvBindMatrix.size(); ++i)
    {
        matMul(skininfop->mBindShapeMatrix, skininfop->mInvBindMatrix[i], skininfop->mBindPoseMatrix[i]);
    }

    skininfop->updateHash();
    LL_DEBUGS("LocalMesh") << "hash: " << skininfop->mHash << LL_ENDL;
    current_object->setObjectMeshSkinInfo(skininfop);
    return true;
}

bool LLLocalMeshImportDAE::processSkeletonJoint(domNode* current_node, std::map<std::string, std::string>& joint_map, std::map<std::string, LLMatrix4>& joint_transforms, bool recurse_children)
{
    // safety checks & name check
    const auto node_name = current_node->getName();
    
    if (!node_name)
    {
        LL_WARNS("LocalMesh") << "nameless node, can't process" << LL_ENDL;
        return false;
    }

    if ( current_node->getType() != NODETYPE_JOINT)
    {
        LL_DEBUGS("LocalMesh") << "non-joint node: " << node_name << LL_ENDL;
        return false;
    }

    if (auto jointmap_iter = joint_map.find(node_name); jointmap_iter != joint_map.end())
    {
        LL_DEBUGS("LocalMesh") << "processing joint: " << node_name << LL_ENDL;

        // begin actual joint work
        daeSIDResolver jointResolver_translation(current_node, "./translate");
        domTranslate* current_transformation = daeSafeCast<domTranslate>(jointResolver_translation.getElement());
        if (current_transformation)
        {
            LL_DEBUGS("LocalMesh") << "Using ./translate for " << node_name << LL_ENDL;
        }
        else
        {
            daeSIDResolver jointResolver_location(current_node, "./location");
            current_transformation = daeSafeCast<domTranslate>(jointResolver_location.getElement());
            if (current_transformation)
            {
                LL_DEBUGS("LocalMesh") << "Using ./location for " << node_name << LL_ENDL;
            }
        }

        if (!current_transformation)
        {
            if (daeElement* child_translate_element = current_node->getChild("translate"); child_translate_element)
            {
                if (current_transformation = daeSafeCast<domTranslate>(child_translate_element); current_transformation)
                {
                    LL_DEBUGS("LocalMesh") << "Using translate child for " << node_name << LL_ENDL;
                }
            }
            else
            {
                LL_DEBUGS("Mesh")<< "Could not find a child [\"translate\"] for the element: \"" << node_name << "\"" << LL_ENDL;
            }
        }

        if (!current_transformation) // no queries worked
        {
            daeSIDResolver jointResolver_transform(current_node, "./transform");

            auto joint_transform_matrix = daeSafeCast<domMatrix>(jointResolver_transform.getElement());
            if (joint_transform_matrix)
            {
                LLMatrix4 workingTransform;
                domFloat4x4 domArray = joint_transform_matrix->getValue();
                for (int i = 0; i < 4; i++)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        workingTransform.mMatrix[i][j] = (F32)domArray[i + j * 4];
                    }
                }
                joint_transforms[node_name].setTranslation(workingTransform.getTranslation());
                LL_DEBUGS("LocalMesh") << "Using ./transform matrix for " << node_name << " = "<< joint_transforms[node_name] << LL_ENDL;
            }
            else
            {
                daeSIDResolver jointResolver_matrix(current_node, "./matrix");
                joint_transform_matrix = daeSafeCast<domMatrix>(jointResolver_matrix.getElement());
                if (joint_transform_matrix)
                {
                    LL_DEBUGS("LocalMesh") << "Using ./matrix matrix for " << node_name << LL_ENDL;
                    LLMatrix4 workingTransform;
                    domFloat4x4 domArray = joint_transform_matrix->getValue();
                    for (int i = 0; i < 4; i++)
                    {
                        for (int j = 0; j < 4; j++)
                        {
                            workingTransform.mMatrix[i][j] = (F32)domArray[i + j * 4];
                        }
                    }
                    joint_transforms[node_name] = workingTransform;
                    LL_DEBUGS("LocalMesh") << "Using ./transform matrix for " << node_name << " = "<< workingTransform << LL_ENDL;
                }
                else
                {
                    LL_WARNS() << "The found element for " << node_name << " is not a translate or matrix node - most likely a corrupt export!" << LL_ENDL;
                    // return false; // Beq - an unweighted bone? returning failure here can trigger problems with otherwise good mesh.
                }
            }
        }
        else // previous query worked
        {
            domFloat3 joint_transform = current_transformation->getValue();
            LLVector3 singleJointTranslation((F32)joint_transform[0], (F32)joint_transform[1], (F32)joint_transform[2]);
            LLMatrix4 workingTransform;
            workingTransform.setTranslation(singleJointTranslation);

            joint_transforms[node_name] = workingTransform;
            LL_DEBUGS("LocalMesh") << "Applying translation from detected transform for " << node_name << " = "<< workingTransform << LL_ENDL;
        }
        // end actual joint work
    }
    else
    {
        LL_DEBUGS("LocalMesh") << "unknown/unexpected joint: " << node_name << LL_ENDL;
        // return false; // still need to check the children
    }

    if (recurse_children)
    {
        // get children to work on
        auto current_node_children = current_node->getChildren();
        auto child_count = current_node_children.getCount();
        LL_DEBUGS("LocalMesh") << "Processing " << child_count << " children of " << current_node->getName() << LL_ENDL;
        for (size_t node_children_iter = 0; node_children_iter < child_count; ++node_children_iter)
        {
            auto current_child_node = daeSafeCast<domNode>(current_node_children[node_children_iter]);
            if (current_child_node)
            {
                if( !processSkeletonJoint(current_child_node, joint_map, joint_transforms, recurse_children) )
                {
                    LL_DEBUGS("LocalMesh") << "failed to process joint (bad child): " << current_child_node->getName() << LL_ENDL;
                    continue;
                };
            }
        }
    }

    return true;
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
                    offset_position = (int)inputs[input_iterator]->getOffset();
                    daeElementRef current_element = input_vertices_array[vertex_input_iterator]->getSource().getElement();
                    source_position = daeSafeCast<domSource>(current_element);
                }

                // vertex normal array
                else if (current_vertex_semantic.compare(COMMON_PROFILE_INPUT_NORMAL) == 0)
                {
                    offset_normals = (int)inputs[input_iterator]->getOffset();
                    daeElementRef current_element = input_vertices_array[vertex_input_iterator]->getSource().getElement();
                    source_normals = daeSafeCast<domSource>(current_element);
                }
            }

        }

        // normal array outside of vertex array
        else if (current_semantic.compare(COMMON_PROFILE_INPUT_NORMAL) == 0)
        {
            offset_normals = (int)inputs[input_iterator]->getOffset();
            daeElementRef current_element = inputs[input_iterator]->getSource().getElement();
            source_normals = daeSafeCast<domSource>(current_element);
        }

        // uv array
        else if (current_semantic.compare(COMMON_PROFILE_INPUT_TEXCOORD) == 0)
        {
            offset_uvmap = (int)inputs[input_iterator]->getOffset();
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
        LLVector4   vtx_normal_data;
        LLVector2   vtx_uv_data;
        int         vtx_index;
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
            (F32)serialized_values_position[triangle_list[triangle_iterator + offset_position] * 3 + 0],
            (F32)serialized_values_position[triangle_list[triangle_iterator + offset_position] * 3 + 1],
            (F32)serialized_values_position[triangle_list[triangle_iterator + offset_position] * 3 + 2],
            0.f
        );

        // normal attribute
        if (source_normals)
        {
            attr_normal.set
            (
                (F32)serialized_values_normals[triangle_list[triangle_iterator + offset_normals] * 3 + 0],
                (F32)serialized_values_normals[triangle_list[triangle_iterator + offset_normals] * 3 + 1],
                (F32)serialized_values_normals[triangle_list[triangle_iterator + offset_normals] * 3 + 2],
                0.f
            );
        }

        // uv attribute
        if (source_uvmap)
        {
            attr_uv.set
            (
                (F32)serialized_values_uvmap[triangle_list[triangle_iterator + offset_uvmap] * 2 + 0],
                (F32)serialized_values_uvmap[triangle_list[triangle_iterator + offset_uvmap] * 2 + 1]
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
            for (const auto& repeat_vtx_data : repeat_map_data[seeker_index])
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
                size_t list_indices_size = list_indices.size();
                size_t triangle_check = list_indices_size % 3;

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
            list_positions.emplace_back(attr_position);
            if (source_normals)
            {
                list_normals.emplace_back(attr_normal);
            }

            if (source_uvmap)
            {
                list_uvs.emplace_back(attr_uv);
            }

            // index is verts list - 1, push that index into indices
            int current_index = static_cast<int>(list_positions.size()) - 1;
            list_indices.emplace_back(current_index);

            // track this position for future repeats
            repeat_map_data_struct new_trackable;
            new_trackable.vtx_normal_data = attr_normal;
            new_trackable.vtx_uv_data = attr_uv;
            new_trackable.vtx_index = current_index;

            if (seeker_position != repeat_map_position_iterable.end())
            {
                int seeker_index = (int)std::distance(repeat_map_position_iterable.begin(), seeker_position);
                repeat_map_data[seeker_index].emplace_back(new_trackable);
            }
            else
            {
                repeat_map_position_iterable.emplace_back(attr_position);
                std::vector<repeat_map_data_struct> new_map_data_vec;
                new_map_data_vec.emplace_back(new_trackable);
                repeat_map_data.emplace_back(new_map_data_vec);
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
        LLVector4   vtx_normal_data;
        LLVector2   vtx_uv_data;
        int         vtx_index;
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
                (F32)serialized_values_position[vertex_indices[current_vtx_index + offset_position] * 3 + 0],
                (F32)serialized_values_position[vertex_indices[current_vtx_index + offset_position] * 3 + 1],
                (F32)serialized_values_position[vertex_indices[current_vtx_index + offset_position] * 3 + 2],
                0.f
            );

            // normal attribute
            if (source_normals)
            {
                attr_normal.set
                (
                    (F32)serialized_values_normals[vertex_indices[current_vtx_index + offset_normals] * 3 + 0],
                    (F32)serialized_values_normals[vertex_indices[current_vtx_index + offset_normals] * 3 + 1],
                    (F32)serialized_values_normals[vertex_indices[current_vtx_index + offset_normals] * 3 + 2],
                    0.f
                );
            }

            // uv attribute
            if (source_uvmap)
            {
                attr_uv.set
                (
                    (F32)serialized_values_uvmap[vertex_indices[current_vtx_index + offset_uvmap] * 2 + 0],
                    (F32)serialized_values_uvmap[vertex_indices[current_vtx_index + offset_uvmap] * 2 + 1]
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
                int seeker_index = (int)std::distance(repeat_map_position_iterable.begin(), seeker_position);
                for (const auto& repeat_vtx_data : repeat_map_data[seeker_index])
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
                int current_index = static_cast<int>(list_positions.size()) - 1;

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
                    list_indices.emplace_back(first_idx);
                    list_indices.emplace_back(last_idx);
                    list_indices.emplace_back(current_index);
                    last_idx = current_index;
                }

                // track this position for future repeats
                repeat_map_data_struct new_trackable;
                new_trackable.vtx_normal_data = attr_normal;
                new_trackable.vtx_uv_data = attr_uv;
                new_trackable.vtx_index = current_index;

                if (seeker_position != repeat_map_position_iterable.end())
                {
                    int seeker_index = (int)std::distance(repeat_map_position_iterable.begin(), seeker_position);
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

void LLLocalMeshImportDAE::pushLog(const std::string& who, const std::string& what, bool is_error)
{
    std::string log_msg = "[ " + who + " ] ";
    if (is_error)
    {
        log_msg += "[ ERROR ] ";
    }

    log_msg += what;
    mLoadingLog.push_back(log_msg);
    LL_INFOS("LocalMesh") << log_msg << LL_ENDL;
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
