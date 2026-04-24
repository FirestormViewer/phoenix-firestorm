/**
 * @file fschecktexturegltf.cpp
 * @brief Save selected object mesh+textures as a .gltf + external PNG files without permission checks.
 *
 * Coordinate conversion: SL is Z-up right-hand.  glTF is Y-up right-hand.
 *   glTF.x =  SL.x
 *   glTF.y =  SL.z
 *   glTF.z = -SL.y
 * The same remapping is applied to quaternion components (derived by conjugating
 * with the -90° X-axis rotation): q' = (qx, qz, -qy, qw).
 * Scale axes are reordered identically: (sx, sz, sy).
 */

#include "llviewerprecompiledheaders.h"
#include "fschecktexturegltf.h"

#include "llimagej2c.h"
#include "llimagepng.h"

#include "llappviewer.h"
#include "llcallbacklist.h"
#include "llfile.h"
#include "llfilepicker.h"
#include "llviewermenufile.h"
#include "llselectmgr.h"
#include "lltexturecache.h"
#include "llviewertexturelist.h"
#include "lltinygltfhelper.h"   // pulls in tinygltf/tiny_gltf.h
#include "llgltfmaterial.h"
#include "lltextureentry.h"
#include "llversioninfo.h"
#include "llviewerobject.h"
#include "llvolume.h"
#include "llvovolume.h"

#include "llavatarappearance.h"
#include "llavatarappearancedefines.h"
#include "lllocaltextureobject.h"
#include "llvoavatarself.h"
#include "llwearable.h"
#include "llwearabledata.h"

// tinygltf constants (from tiny_gltf.h via lltinygltfhelper.h)
#ifndef TINYGLTF_MODE_TRIANGLES
#define TINYGLTF_MODE_TRIANGLES (4)
#endif
#ifndef TINYGLTF_COMPONENT_TYPE_FLOAT
#define TINYGLTF_COMPONENT_TYPE_FLOAT (5126)
#endif
#ifndef TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT (5123)
#endif
#ifndef TINYGLTF_TYPE_VEC2
#define TINYGLTF_TYPE_VEC2 (2)
#endif
#ifndef TINYGLTF_TYPE_VEC3
#define TINYGLTF_TYPE_VEC3 (3)
#endif
#ifndef TINYGLTF_TYPE_SCALAR
#define TINYGLTF_TYPE_SCALAR (64 + 1)
#endif
#ifndef TINYGLTF_TARGET_ARRAY_BUFFER
#define TINYGLTF_TARGET_ARRAY_BUFFER (34962)
#endif
#ifndef TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER
#define TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER (34963)
#endif
#ifndef TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
#define TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE (5121)
#endif
#ifndef TINYGLTF_TYPE_VEC4
#define TINYGLTF_TYPE_VEC4 (4)
#endif

static constexpr F32 CT_GLTF_TEXTURE_TIMEOUT = 120.f;

static std::string bakedIndexToChannelName(LLAvatarAppearanceDefines::EBakedTextureIndex idx)
{
    using namespace LLAvatarAppearanceDefines;
    switch (idx)
    {
    case BAKED_HEAD:     return "BAKED_HEAD";
    case BAKED_UPPER:    return "BAKED_UPPER";
    case BAKED_LOWER:    return "BAKED_LOWER";
    case BAKED_EYES:     return "BAKED_EYES";
    case BAKED_HAIR:     return "BAKED_HAIR";
    case BAKED_SKIRT:    return "BAKED_SKIRT";
    case BAKED_LEFT_ARM: return "BAKED_LEFT_ARM";
    case BAKED_LEFT_LEG: return "BAKED_LEFT_LEG";
    case BAKED_AUX1:     return "BAKED_AUX1";
    case BAKED_AUX2:     return "BAKED_AUX2";
    case BAKED_AUX3:     return "BAKED_AUX3";
    default:             return "BAKED_UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

// Replace every non-ASCII byte with '_'.  Keeps JSON strings unambiguously
// valid UTF-8 even when SL passes Latin-1 or otherwise broken multibyte data.
static std::string sanitizeAscii(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s)
        out += (c < 128) ? static_cast<char>(c) : '_';
    return out;
}

// Append raw bytes to a buffer with 4-byte alignment padding.
static size_t appendToBuffer(std::vector<unsigned char>& buf,
                              const void* data, size_t len)
{
    size_t offset = buf.size();
    const auto* src = static_cast<const unsigned char*>(data);
    buf.insert(buf.end(), src, src + len);
    while (buf.size() % 4 != 0)
        buf.push_back(0);
    return offset;
}

static int addVec3Accessor(tinygltf::Model& model,
                            const std::vector<float>& data,
                            bool computeMinMax)
{
    auto& buf = model.buffers[0].data;
    size_t offset = appendToBuffer(buf, data.data(), data.size() * sizeof(float));

    tinygltf::BufferView bv;
    bv.buffer     = 0;
    bv.byteOffset = offset;
    bv.byteLength = data.size() * sizeof(float);
    bv.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
    int bvIdx = (int)model.bufferViews.size();
    model.bufferViews.push_back(bv);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.type          = TINYGLTF_TYPE_VEC3;
    acc.count         = data.size() / 3;

    if (computeMinMax && acc.count > 0)
    {
        double minX = data[0], minY = data[1], minZ = data[2];
        double maxX = data[0], maxY = data[1], maxZ = data[2];
        for (size_t i = 3; i + 2 < data.size(); i += 3)
        {
            minX = std::min(minX, (double)data[i]);
            minY = std::min(minY, (double)data[i + 1]);
            minZ = std::min(minZ, (double)data[i + 2]);
            maxX = std::max(maxX, (double)data[i]);
            maxY = std::max(maxY, (double)data[i + 1]);
            maxZ = std::max(maxZ, (double)data[i + 2]);
        }
        acc.minValues = {minX, minY, minZ};
        acc.maxValues = {maxX, maxY, maxZ};
    }

    int accIdx = (int)model.accessors.size();
    model.accessors.push_back(acc);
    return accIdx;
}

static int addVec2Accessor(tinygltf::Model& model,
                            const std::vector<float>& data)
{
    auto& buf = model.buffers[0].data;
    size_t offset = appendToBuffer(buf, data.data(), data.size() * sizeof(float));

    tinygltf::BufferView bv;
    bv.buffer     = 0;
    bv.byteOffset = offset;
    bv.byteLength = data.size() * sizeof(float);
    bv.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
    int bvIdx = (int)model.bufferViews.size();
    model.bufferViews.push_back(bv);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.type          = TINYGLTF_TYPE_VEC2;
    acc.count         = data.size() / 2;

    int accIdx = (int)model.accessors.size();
    model.accessors.push_back(acc);
    return accIdx;
}

static int addU16Accessor(tinygltf::Model& model,
                           const std::vector<uint16_t>& data)
{
    auto& buf = model.buffers[0].data;
    size_t offset = appendToBuffer(buf, data.data(), data.size() * sizeof(uint16_t));

    tinygltf::BufferView bv;
    bv.buffer     = 0;
    bv.byteOffset = offset;
    bv.byteLength = data.size() * sizeof(uint16_t);
    bv.target     = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
    int bvIdx = (int)model.bufferViews.size();
    model.bufferViews.push_back(bv);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    acc.type          = TINYGLTF_TYPE_SCALAR;
    acc.count         = data.size();

    int accIdx = (int)model.accessors.size();
    model.accessors.push_back(acc);
    return accIdx;
}

static int addU8Vec4Accessor(tinygltf::Model& model, const std::vector<uint8_t>& data)
{
    auto& buf = model.buffers[0].data;
    size_t offset = appendToBuffer(buf, data.data(), data.size());

    tinygltf::BufferView bv;
    bv.buffer     = 0;
    bv.byteOffset = offset;
    bv.byteLength = data.size();
    bv.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
    int bvIdx = (int)model.bufferViews.size();
    model.bufferViews.push_back(bv);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    acc.type          = TINYGLTF_TYPE_VEC4;
    acc.count         = (int)data.size() / 4;
    acc.normalized    = false;

    int accIdx = (int)model.accessors.size();
    model.accessors.push_back(acc);
    return accIdx;
}

static int addVec4Accessor(tinygltf::Model& model, const std::vector<float>& data)
{
    auto& buf = model.buffers[0].data;
    size_t offset = appendToBuffer(buf, data.data(), data.size() * sizeof(float));

    tinygltf::BufferView bv;
    bv.buffer     = 0;
    bv.byteOffset = offset;
    bv.byteLength = data.size() * sizeof(float);
    bv.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
    int bvIdx = (int)model.bufferViews.size();
    model.bufferViews.push_back(bv);

    tinygltf::Accessor acc;
    acc.bufferView    = bvIdx;
    acc.byteOffset    = 0;
    acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
    acc.type          = TINYGLTF_TYPE_VEC4;
    acc.count         = (int)data.size() / 4;

    int accIdx = (int)model.accessors.size();
    model.accessors.push_back(acc);
    return accIdx;
}


// ---------------------------------------------------------------------------
// FSCheckTextureGLTF
// ---------------------------------------------------------------------------

FSCheckTextureGLTF::FSCheckTextureGLTF()
{
}

FSCheckTextureGLTF::~FSCheckTextureGLTF()
{
    if (gIdleCallbacks.containsFunction(CacheReadResponder::fetchWorker, this))
        gIdleCallbacks.deleteFunction(CacheReadResponder::fetchWorker, this);
}

void FSCheckTextureGLTF::exportSelection()
{
    LLObjectSelectionHandle sel = LLSelectMgr::getInstance()->getSelection();
    if (!sel->getPrimaryObject())
    {
        LL_WARNS() << "FSCheckTextureGLTF: no object selected" << LL_ENDL;
        return;
    }

    addSelectedObjects();
    if (mObjects.empty())
    {
        LL_WARNS() << "FSCheckTextureGLTF: selection has no geometry" << LL_ENDL;
        return;
    }

    // mObjectName may contain non-ASCII (Japanese etc.) or be empty if the
    // server hasn't responded yet.  Produce a pure-ASCII filename so the file
    // picker shows a meaningful default on all platforms.
    std::string proposed = sanitizeAscii(mObjectName);
    if (proposed.empty() || proposed.find_first_not_of('_') == std::string::npos)
        proposed = mObjects.front().obj->getID().asString();

    LLFilePickerReplyThread::startPicker(
        boost::bind(&FSCheckTextureGLTF::onFileSelected, this, _1),
        LLFilePicker::FFSAVE_GLTF,
        LLDir::getScrubbedFileName(proposed + ".gltf"));
}

void FSCheckTextureGLTF::addSelectedObjects()
{
    mObjects.clear();
    mTexturesToFetch.clear();
    mTexturesPending.clear();
    mTexturePNG.clear();
    mTextureLayerDir.clear();

    LLObjectSelectionHandle sel = LLSelectMgr::getInstance()->getEditSelection();
    LLSelectNode* rootNode = sel->getFirstRootNode();
    if (!rootNode) return;

    mObjectName = rootNode->mName;
    if (mObjectName.empty())
        mObjectName = rootNode->getObject()->getID().asString();

    mOffset = -sel->getFirstRootObject()->getRenderPosition();

    for (LLObjectSelection::iterator it = sel->begin(); it != sel->end(); ++it)
    {
        LLSelectNode* node = *it;
        LLViewerObject* obj = node->getObject();
        if (!obj || !obj->getVolume()) continue;

        PrimEntry e;
        e.obj  = obj;
        e.name = node->mName.empty() ? obj->getID().asString() : node->mName;
        mObjects.push_back(e);
    }

    // Collect all texture UUIDs (normal + PBR)
    auto enqueue = [&](const LLUUID& id)
    {
        if (id.isNull()) return;
        if (mTexturesToFetch.count(id) || mTexturePNG.count(id)) return;
        mTexturesToFetch[id] = id.asString();
        LLViewerFetchedTexture* tex = LLViewerTextureManager::getFetchedTexture(id);
        if (tex)
        {
            tex->setBoostLevel(LLGLTexture::BOOST_MAX_LEVEL);
            tex->setMinDiscardLevel(0);
        }
    };

    // Like enqueue but records the UUID as a layer texture (not referenced in glTF).
    auto enqueueLayer = [&](const LLUUID& id, const std::string& channelName)
    {
        if (id.isNull()) return;
        mTextureLayerDir.emplace(id, channelName);
        enqueue(id);
    };

    // Per-export set so each bake channel is collected at most once.
    std::set<LLAvatarAppearanceDefines::EBakedTextureIndex> processedChannels;

    for (auto& entry : mObjects)
    {
        LLViewerObject* obj = entry.obj;
        S32 nfaces = obj->getVolume()->getNumVolumeFaces();
        bool isSelfAttach = isAgentAvatarValid() &&
                            obj->getAvatar() != nullptr &&
                            obj->getAvatar()->isSelf();

        for (S32 f = 0; f < nfaces; ++f)
        {
            LLTextureEntry* te = obj->getTE(f);
            if (!te) continue;

            LLGLTFMaterial* pbr = te->getGLTFRenderMaterial();
            if (pbr)
            {
                for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
                    enqueue(pbr->mTextureId[i]);
            }
            else
            {
                using namespace LLAvatarAppearanceDefines;
                LLUUID texId;

                if (LLAvatarAppearanceDictionary::isBakedImageId(te->getID()))
                {
                    // Resolve Magic ID → UUID of the texture actually rendered
                    LLViewerTexture* rendered = obj->getTEImage(f);
                    texId = (rendered && rendered->getID().notNull())
                                ? rendered->getID() : LLUUID::null;

                    // For self-attachments collect each bake channel's layer textures
                    if (isSelfAttach)
                    {
                        EBakedTextureIndex baked_idx =
                            LLAvatarAppearanceDictionary::assetIdToBakedTextureIndex(te->getID());
                        if (baked_idx != BAKED_NUM_INDICES &&
                            !processedChannels.count(baked_idx))
                        {
                            processedChannels.insert(baked_idx);
                            std::string channelName = bakedIndexToChannelName(baked_idx);
                            const auto* dict = LLAvatarAppearance::getDictionary();
                            const auto* baked_dict = dict
                                ? dict->getBakedTexture(baked_idx) : nullptr;
                            if (baked_dict)
                            {
                                LLWearableData* wdata = gAgentAvatarp->getWearableData();
                                for (LLWearableType::EType wtype : baked_dict->mWearables)
                                {
                                    U32 cnt = wdata->getWearableCount(wtype);
                                    for (U32 j = 0; j < cnt; ++j)
                                    {
                                        LLWearable* w = wdata->getWearable(wtype, j);
                                        if (!w) continue;
                                        for (ETextureIndex tidx : baked_dict->mLocalTextures)
                                        {
                                            LLLocalTextureObject* lto =
                                                w->getLocalTextureObject((S32)tidx);
                                            if (lto)
                                                enqueueLayer(lto->getID(), channelName);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    texId = te->getID();
                }
                enqueue(texId);
            }
        }
    }
}

void FSCheckTextureGLTF::onFileSelected(const std::vector<std::string>& filenames)
{
    if (filenames.empty()) return;
    mFilename = filenames[0];

    // Ensure .gltf extension
    if (gDirUtilp->getExtension(mFilename) != "gltf")
        mFilename += ".gltf";

    startTextureFetch();
}

void FSCheckTextureGLTF::startTextureFetch()
{
    if (mTexturesToFetch.empty())
    {
        onTexturesFetched();
        return;
    }
    mTimer.start();
    mTimer.setTimerExpirySec(CT_GLTF_TEXTURE_TIMEOUT);
    gIdleCallbacks.addFunction(CacheReadResponder::fetchWorker, this);
}

void FSCheckTextureGLTF::onTexturesFetched()
{
    buildAndSaveGLTF();
    delete this;
}

bool FSCheckTextureGLTF::buildAndSaveGLTF()
{
    tinygltf::Model model;
    model.asset.version   = "2.0";
    model.asset.generator = sanitizeAscii(LLVersionInfo::getInstance()->getChannelAndVersion());

    // Single geometry buffer
    model.buffers.emplace_back();

    // Map: texture UUID → index in model.textures
    std::map<LLUUID, int> texIndexMap;

    // Write textures as external PNG files.
    // Regular textures go to {exportDir}/{uuid}.png (flat, same dir as .gltf)
    // and are referenced in the glTF by just "{uuid}.png".
    // tinygltf's default WriteImageData strips directory from URIs, so keeping
    // them flat avoids broken references when Blender imports the file.
    // Layer textures (bake-on-mesh source layers, self-only) go to
    // layers/{BAKED_XXX}/{uuid}.png and are NOT referenced in the glTF.
    std::string baseDir  = gDirUtilp->getDirName(mFilename);
    std::string sep      = gDirUtilp->getDirDelimiter();
    std::string layersDir = baseDir + sep + "layers";

    for (auto& kv : mTexturePNG)
    {
        if (kv.second.empty()) continue;

        std::string uuidStr = kv.first.asString();   // always ASCII

        auto layerIt = mTextureLayerDir.find(kv.first);
        if (layerIt != mTextureLayerDir.end())
        {
            // Layer texture — save to layers/{channel}/{uuid}.png, skip glTF model
            std::string channelDir = layersDir + sep + layerIt->second;
            LLFile::mkdir(layersDir);
            LLFile::mkdir(channelDir);
            std::string pngPath = channelDir + sep + uuidStr + ".png";
            LLFILE* fp = LLFile::fopen(pngPath, "wb");   /*Flawfinder: ignore*/
            if (fp) { fwrite(kv.second.data(), 1, kv.second.size(), fp); LLFile::close(fp); }
            else { LL_WARNS() << "FSCheckTextureGLTF: failed to write layer PNG " << pngPath << LL_ENDL; }
            continue;
        }

        // Regular texture — save flat next to the .gltf file as {uuid}.png.
        // tinygltf strips the directory from img.uri when writing JSON, so the
        // file and the URI must both use just the bare filename.
        std::string pngPath = baseDir + sep + uuidStr + ".png";
        LLFILE* fp = LLFile::fopen(pngPath, "wb");   /*Flawfinder: ignore*/
        if (fp)
        {
            fwrite(kv.second.data(), 1, kv.second.size(), fp);
            LLFile::close(fp);
        }
        else
        {
            LL_WARNS() << "FSCheckTextureGLTF: failed to write PNG " << pngPath << LL_ENDL;
        }

        tinygltf::Image img;
        img.name     = uuidStr;
        img.uri      = uuidStr + ".png";   // bare filename — tinygltf strips dirs anyway
        img.mimeType = "image/png";

        int imgIdx = (int)model.images.size();
        model.images.push_back(img);

        tinygltf::Texture tex;
        tex.source = imgIdx;
        tex.name   = img.name;
        texIndexMap[kv.first] = (int)model.textures.size();
        model.textures.push_back(tex);
    }

    // Helper: get or create deduplicated material
    std::map<std::string, int> matIndexMap;

    auto getOrCreateMaterial = [&](const LLUUID& baseColor,
                                    const LLUUID& normal,
                                    const LLUUID& orm,
                                    const LLUUID& emissive) -> int
    {
        std::string key = baseColor.asString() + "|" + normal.asString()
                        + "|" + orm.asString()  + "|" + emissive.asString();
        auto it = matIndexMap.find(key);
        if (it != matIndexMap.end()) return it->second;

        tinygltf::Material mat;
        mat.name = baseColor.asString();

        auto setTex = [&](const LLUUID& id) -> int {
            auto jt = texIndexMap.find(id);
            return (jt != texIndexMap.end()) ? jt->second : -1;
        };

        int bcIdx = setTex(baseColor);
        if (bcIdx >= 0)
            mat.pbrMetallicRoughness.baseColorTexture.index = bcIdx;

        int nIdx = setTex(normal);
        if (nIdx >= 0)
            mat.normalTexture.index = nIdx;

        int ormIdx = setTex(orm);
        if (ormIdx >= 0)
            mat.pbrMetallicRoughness.metallicRoughnessTexture.index = ormIdx;

        int emIdx = setTex(emissive);
        if (emIdx >= 0)
            mat.emissiveTexture.index = emIdx;

        // Non-PBR faces: set sensible metallic/roughness defaults
        if (normal.isNull() && orm.isNull() && emissive.isNull())
        {
            mat.pbrMetallicRoughness.metallicFactor  = 0.0;
            mat.pbrMetallicRoughness.roughnessFactor = 1.0;
        }

        int idx = (int)model.materials.size();
        model.materials.push_back(mat);
        matIndexMap[key] = idx;
        return idx;
    };

    tinygltf::Scene scene;
    scene.name = sanitizeAscii(mObjectName);

    // Skinning state: deduplicate skins and joint nodes across all objects
    std::map<const LLMeshSkinInfo*, int> skinCache;
    std::map<std::string, int> jointNodeMap;

    auto getOrCreateSkin = [&](const LLMeshSkinInfo* si) -> int
    {
        auto it = skinCache.find(si);
        if (it != skinCache.end()) return it->second;

        int numJoints = (int)si->mJointNames.size();
        if (numJoints == 0) return -1;

        tinygltf::Skin skin;
        skin.name = "Armature";
        // inverseBindMatrices omitted (defaults to -1 = all IBMs treated as identity)

        for (int j = 0; j < numJoints; ++j)
        {
            const std::string& jname = si->mJointNames[j];

            // Joint node — create once per unique joint name, at identity transform.
            // With IBM omitted and joint nodes at identity, jointMatrix(j) = I for
            // all joints, so the mesh is exported in its bind pose without distortion.
            int jNodeIdx;
            auto jit = jointNodeMap.find(jname);
            if (jit != jointNodeMap.end())
            {
                jNodeIdx = jit->second;
            }
            else
            {
                tinygltf::Node jNode;
                jNode.name = jname;
                jNodeIdx = (int)model.nodes.size();
                model.nodes.push_back(jNode);
                jointNodeMap[jname] = jNodeIdx;
            }
            skin.joints.push_back(jNodeIdx);
        }

        int skinIdx = (int)model.skins.size();
        model.skins.push_back(skin);
        skinCache[si] = skinIdx;
        return skinIdx;
    };

    for (auto& entry : mObjects)
    {
        LLViewerObject* obj = entry.obj;
        if (!obj || !obj->getVolume()) continue;

        LLVOVolume* vol = dynamic_cast<LLVOVolume*>(obj);
        const LLMeshSkinInfo* skinInfo = vol ? vol->getSkinInfo() : nullptr;
        int skinIdx = (skinInfo && !skinInfo->mJointNames.empty())
                      ? getOrCreateSkin(skinInfo) : -1;

        tinygltf::Mesh mesh;
        mesh.name = sanitizeAscii(entry.name);

        S32 nfaces = obj->getVolume()->getNumVolumeFaces();
        for (S32 f = 0; f < nfaces; ++f)
        {
            const LLVolumeFace& face = obj->getVolume()->getVolumeFace(f);
            if (face.mNumVertices == 0 || face.mNumIndices == 0) continue;
            if (!face.mPositions || !face.mNormals || !face.mTexCoords) continue;

            // Build vertex arrays with SL Z-up → glTF Y-up conversion
            std::vector<float> positions, normals, uvs;
            positions.reserve(face.mNumVertices * 3);
            normals.reserve(face.mNumVertices * 3);
            uvs.reserve(face.mNumVertices * 2);

            LLTextureEntry* te = obj->getTE(f);

            // For rigged meshes, bake getScale() into vertex positions.
            // face.mPositions is normalized to [-0.5, 0.5] per axis; node.scale is
            // cancelled by glTF's inverse(nodeWorld) in the joint matrix formula,
            // so scale must be applied directly to the vertex data.
            // Non-rigged meshes rely on node.scale instead (set later).
            const LLVector3 objScale = obj->getScale();
            const bool bakeScale = (skinIdx >= 0);

            for (S32 i = 0; i < face.mNumVertices; ++i)
            {
                // LLVector4a → LLVector3 (same pattern as daeexport.cpp's v4adapt)
                LLVector3 p((F32*)&face.mPositions[i]);
                if (bakeScale)
                {
                    positions.push_back(p.mV[VX] * objScale.mV[VX]);
                    positions.push_back(p.mV[VZ] * objScale.mV[VZ]);   //  SL.z → glTF.y
                    positions.push_back(-p.mV[VY] * objScale.mV[VY]);  // -SL.y → glTF.z
                }
                else
                {
                    positions.push_back(p.mV[VX]);
                    positions.push_back(p.mV[VZ]);    //  SL.z → glTF.y
                    positions.push_back(-p.mV[VY]);   // -SL.y → glTF.z
                }

                LLVector3 n((F32*)&face.mNormals[i]);
                normals.push_back(n.mV[VX]);
                normals.push_back(n.mV[VZ]);
                normals.push_back(-n.mV[VY]);

                uvs.push_back(face.mTexCoords[i].mV[VX]);
                uvs.push_back(1.0f - face.mTexCoords[i].mV[VY]);
            }

            // Mesh objects have artist-authored UV atlases; applying SL texture
            // scale/offset would distort them.  Only transform procedural prims.
            if (te && !obj->isMesh())
            {
                F32 cosA = cosf(te->getRotation());
                F32 sinA = sinf(te->getRotation());
                F32 repeatU, repeatV, offsetU, offsetV;
                te->getScale(&repeatU, &repeatV);
                te->getOffset(&offsetU, &offsetV);
                bool planar = (LLTextureEntry::TEX_GEN_PLANAR == te->getTexGen());

                for (S32 i = 0; i < face.mNumVertices; ++i)
                {
                    F32 u = uvs[i * 2];
                    F32 v = uvs[i * 2 + 1];

                    if (planar)
                    {
                        LLVector3 norm((F32*)&face.mNormals[i]);
                        LLVector3 pos((F32*)&face.mPositions[i]);
                        LLVector3 binormal;
                        F32 d = norm * LLVector3::x_axis;
                        if (d >= 0.5f || d <= -0.5f)
                        {
                            binormal = LLVector3::y_axis;
                            if (norm.mV[0] < 0) binormal *= -1.f;
                        }
                        else
                        {
                            binormal = LLVector3::x_axis;
                            if (norm.mV[1] > 0) binormal *= -1.f;
                        }
                        LLVector3 tangent = binormal % norm;
                        LLVector3 scaledPos = pos.scaledVec(objScale);
                        u = 1.f + (binormal * scaledPos) * 2.f - 0.5f;
                        v = -((tangent * scaledPos) * 2.f - 0.5f);
                    }

                    F32 tX = u - 0.5f;
                    F32 tY = v - 0.5f;
                    uvs[i * 2]     = (tX * cosA + tY * sinA)  * repeatU + offsetU + 0.5f;
                    uvs[i * 2 + 1] = (-tX * sinA + tY * cosA) * repeatV + offsetV + 0.5f;
                }
            }

            std::vector<uint16_t> indices;
            indices.reserve(face.mNumIndices);
            for (S32 i = 0; i < face.mNumIndices; ++i)
                indices.push_back(static_cast<uint16_t>(face.mIndices[i]));

            int posAcc  = addVec3Accessor(model, positions, /*computeMinMax=*/true);
            int normAcc = addVec3Accessor(model, normals,   /*computeMinMax=*/false);
            int uvAcc   = addVec2Accessor(model, uvs);
            int idxAcc  = addU16Accessor(model, indices);

            // Determine textures for this face
            LLUUID baseColor, normal_id, orm_id, emissive_id;
            if (te)
            {
                LLGLTFMaterial* pbr = te->getGLTFRenderMaterial();
                if (pbr)
                {
                    baseColor   = pbr->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR];
                    normal_id   = pbr->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL];
                    orm_id      = pbr->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS];
                    emissive_id = pbr->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE];
                }
                else
                {
                    baseColor = te->getID();
                }
            }

            tinygltf::Primitive prim;
            prim.attributes["POSITION"]   = posAcc;
            prim.attributes["NORMAL"]     = normAcc;
            prim.attributes["TEXCOORD_0"] = uvAcc;
            prim.indices  = idxAcc;
            prim.mode     = TINYGLTF_MODE_TRIANGLES;
            prim.material = getOrCreateMaterial(baseColor, normal_id, orm_id, emissive_id);

            if (skinIdx >= 0 && face.mWeights)
            {
                int numJ = (int)skinInfo->mJointNames.size();
                std::vector<uint8_t> joints4;
                std::vector<float>   weights4;
                joints4.reserve(face.mNumVertices * 4);
                weights4.reserve(face.mNumVertices * 4);

                for (S32 i = 0; i < face.mNumVertices; ++i)
                {
                    const LLVector4a& wv = face.mWeights[i];
                    float wSum = 0.f;
                    uint8_t ji[4] = {0, 0, 0, 0};
                    float   wf[4] = {0.f, 0.f, 0.f, 0.f};
                    for (int k = 0; k < 4; ++k)
                    {
                        float raw = wv.getF32ptr()[k];
                        int idx = llclamp((int)floorf(raw), 0, numJ - 1);
                        float w  = raw - (float)idx;
                        ji[k] = (uint8_t)idx;
                        wf[k] = w;
                        wSum += w;
                    }
                    if (wSum > 1e-6f)
                        for (int k = 0; k < 4; ++k)
                            wf[k] /= wSum;
                    for (int k = 0; k < 4; ++k)
                    {
                        joints4.push_back(ji[k]);
                        weights4.push_back(wf[k]);
                    }
                }

                prim.attributes["JOINTS_0"]  = addU8Vec4Accessor(model, joints4);
                prim.attributes["WEIGHTS_0"] = addVec4Accessor(model, weights4);
            }

            mesh.primitives.push_back(prim);
        }

        if (mesh.primitives.empty()) continue;

        int meshIdx = (int)model.meshes.size();
        model.meshes.push_back(mesh);

        // Node transform: SL Z-up → glTF Y-up
        tinygltf::Node node;
        node.name = sanitizeAscii(entry.name);
        node.mesh = meshIdx;

        if (skinIdx < 0)
        {
            // Non-rigged: place node at object's world position.
            LLVector3 pos = obj->getRenderPosition() + mOffset;
            node.translation = {(double)pos.mV[VX],
                                 (double)pos.mV[VZ],
                                -(double)pos.mV[VY]};

            // Quaternion: (qx, qy, qz, qw) → (qx, qz, -qy, qw)
            // tinygltf Node::rotation is [x, y, z, w]
            LLQuaternion rot = obj->getRenderRotation();
            node.rotation = {(double)rot.mQ[VX],
                              (double)rot.mQ[VZ],
                             -(double)rot.mQ[VY],
                              (double)rot.mQ[3]};   // mQ[3] = w

            // Scale: axes remapped same as coordinates (no negation for scale)
            LLVector3 scale = obj->getScale();
            node.scale = {(double)scale.mV[VX],
                          (double)scale.mV[VZ],
                          (double)scale.mV[VY]};
        }
        else
        {
            // Rigged: leave node transform as identity.
            // glTF skinning applies inverse(nodeWorld) to joint matrices, so a
            // non-identity mesh node would corrupt the skin deformation.
            // The bind-pose positions are encoded in the joint nodes and IBMs.
            node.skin = skinIdx;
        }

        int nodeIdx = (int)model.nodes.size();
        model.nodes.push_back(node);
        scene.nodes.push_back(nodeIdx);
    }

    // Add joint nodes as scene root nodes (required by glTF spec)
    for (auto& kv : jointNodeMap)
        scene.nodes.push_back(kv.second);

    if (scene.nodes.empty())
    {
        LL_WARNS() << "FSCheckTextureGLTF: no geometry to export" << LL_ENDL;
        return false;
    }

    model.scenes.push_back(scene);
    model.defaultScene = 0;

    bool ok = LLTinyGLTFHelper::saveModel(mFilename, model);
    if (ok)
        LL_INFOS() << "FSCheckTextureGLTF: saved " << mFilename << LL_ENDL;
    else
        LL_WARNS() << "FSCheckTextureGLTF: save failed for " << mFilename << LL_ENDL;
    return ok;
}

// ---------------------------------------------------------------------------
// FSCheckTextureGLTF::CacheReadResponder
// ---------------------------------------------------------------------------

FSCheckTextureGLTF::CacheReadResponder::CacheReadResponder(
    const LLUUID& id, LLImageFormatted* image, FSCheckTextureGLTF* owner)
    : mFormattedImage(image), mID(id), mOwner(owner)
{
    setImage(image);
}

void FSCheckTextureGLTF::CacheReadResponder::setData(
    U8* data, S32 datasize, S32 imagesize, S32 imageformat, bool imagelocal)
{
    if (imageformat == IMG_CODEC_TGA && mFormattedImage->getCodec() == IMG_CODEC_J2C)
    {
        LL_WARNS() << "FSCheckTextureGLTF: texture " << mID << " is TGA, skipping" << LL_ENDL;
        mFormattedImage = nullptr;
        mImageSize = 0;
        return;
    }
    if (mFormattedImage.notNull())
    {
        if (mFormattedImage->getCodec() == imageformat)
            mFormattedImage->appendData(data, datasize);
        else
        {
            LL_WARNS() << "FSCheckTextureGLTF: texture " << mID << " wrong format" << LL_ENDL;
            mFormattedImage = nullptr;
            mImageSize = 0;
            return;
        }
    }
    else
    {
        mFormattedImage = LLImageFormatted::createFromType(imageformat);
        mFormattedImage->setData(data, datasize);
    }
    mImageSize = imagesize;
    mImageLocal = imagelocal;
}

void FSCheckTextureGLTF::CacheReadResponder::completed(bool success)
{
    // Always remove from pending regardless of outcome
    mOwner->mTexturesPending.erase(mID);

    if (!success || mFormattedImage.isNull() || mImageSize == 0)
    {
        LL_WARNS() << "FSCheckTextureGLTF: cache read failed for " << mID << LL_ENDL;
        mOwner->mTexturePNG[mID] = {};
        return;
    }

    if (!mFormattedImage->updateData() ||
        mFormattedImage->getWidth() * mFormattedImage->getHeight() *
            mFormattedImage->getComponents() == 0)
    {
        mOwner->mTexturePNG[mID] = {};
        return;
    }

    mFormattedImage->setDiscardLevel(0);

    LLPointer<LLImageRaw> raw = new LLImageRaw;
    raw->resize(mFormattedImage->getWidth(),
                mFormattedImage->getHeight(),
                mFormattedImage->getComponents());

    if (!mFormattedImage->decode(raw, 0))
    {
        LL_WARNS() << "FSCheckTextureGLTF: decode failed for " << mID << LL_ENDL;
        mOwner->mTexturePNG[mID] = {};
        return;
    }

    LLPointer<LLImagePNG> png = new LLImagePNG;
    if (!png->encode(raw, 0))
    {
        LL_WARNS() << "FSCheckTextureGLTF: PNG encode failed for " << mID << LL_ENDL;
        mOwner->mTexturePNG[mID] = {};
        return;
    }

    S32 len = png->getDataSize();
    const U8* bytes = png->getData();
    mOwner->mTexturePNG[mID].assign(bytes, bytes + len);
}

// static
void FSCheckTextureGLTF::CacheReadResponder::fetchWorker(void* data)
{
    FSCheckTextureGLTF* self = static_cast<FSCheckTextureGLTF*>(data);

    // Both queues empty → all textures are in mTexturePNG → build the GLB.
    // This is safe: mTexturesPending is only non-empty while completed()
    // callbacks are in flight, and those remove themselves before returning.
    if (self->mTexturesToFetch.empty() && self->mTexturesPending.empty())
    {
        gIdleCallbacks.deleteFunction(fetchWorker, self);
        self->mTimer.stop();
        self->onTexturesFetched();  // calls delete this — must be last
        return;
    }

    // If only pending in-flight reads remain, check for overall timeout
    if (self->mTexturesToFetch.empty())
    {
        if (self->mTimer.hasExpired())
        {
            LL_WARNS() << "FSCheckTextureGLTF: timed out waiting for cache reads" << LL_ENDL;
            for (const LLUUID& id : self->mTexturesPending)
                self->mTexturePNG[id] = {};
            self->mTexturesPending.clear();
            // Next tick: both empty → onTexturesFetched()
        }
        return;
    }

    // Process the next texture that is waiting for download
    LLUUID id = self->mTexturesToFetch.begin()->first;
    LLViewerTexture* imagep =
        LLViewerTextureManager::findFetchedTexture(id, TEX_LIST_STANDARD);

    if (!imagep)
    {
        // Not in memory at all; skip it
        self->mTexturesToFetch.erase(id);
        self->mTexturePNG[id] = {};
        self->mTimer.reset();
        self->mTimer.setTimerExpirySec(CT_GLTF_TEXTURE_TIMEOUT);
        return;
    }

    S32 fullW = imagep->getFullWidth();
    S32 fullH = imagep->getFullHeight();
    bool fullyLoaded = (imagep->getDiscardLevel() == 0) &&
                       (fullW > 0) && (fullH > 0) &&
                       (imagep->getWidth()  >= fullW) &&
                       (imagep->getHeight() >= fullH);

    if (fullyLoaded)
    {
        LLImageFormatted* img = new LLImageJ2C;
        S32 sz = LLImageJ2C::calcDataSizeJ2C(
            fullW, fullH,
            imagep->getComponents(), 0);

        CacheReadResponder* responder = new CacheReadResponder(id, img, self);
        LLAppViewer::getTextureCache()->readFromCache(id, 0, sz, responder);

        self->mTexturesPending.insert(id);
        self->mTexturesToFetch.erase(id);
        self->mTimer.reset();
        self->mTimer.setTimerExpirySec(CT_GLTF_TEXTURE_TIMEOUT);
    }
    else if (self->mTimer.hasExpired())
    {
        LL_WARNS() << "FSCheckTextureGLTF: timed out waiting for texture " << id << LL_ENDL;
        self->mTexturesToFetch.erase(id);
        self->mTexturePNG[id] = {};
        self->mTimer.reset();
        self->mTimer.setTimerExpirySec(CT_GLTF_TEXTURE_TIMEOUT);
    }
}
