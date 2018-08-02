/**
 * @file daeexport.h
 * @brief A system which allows saving in-world objects to Collada .DAE files for offline texturizing/shading.
 * @authors Latif Khalifa
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

#ifndef DAEEXPORT_H_
#define DAEEXPORT_H_

#include "llfloater.h"
#include "lltextureentry.h"
#include "lltexturecache.h"
#include <dom/domElements.h>

class LLViewerObject;
class LLObjectSelection;

class DAESaver
{
public:
	class MaterialInfo
	{
	public:
		LLUUID textureID;
		LLColor4 color;
		std::string name;

		bool matches(LLTextureEntry* te)
		{
			return (textureID == te->getID()) && (color == te->getColor());
		}

		bool operator== (const MaterialInfo& rhs)
		{
			return (textureID == rhs.textureID) && (color == rhs.color) && (name == rhs.name);
		}

		bool operator!= (const MaterialInfo& rhs)
		{
			return !(*this == rhs);
		}

		MaterialInfo() {}

		MaterialInfo(const MaterialInfo& rhs)
		{
			textureID = rhs.textureID;
			color = rhs.color;
			name = rhs.name;
		}

		MaterialInfo& operator= (const MaterialInfo& rhs)
		{
			textureID = rhs.textureID;
			color = rhs.color;
			name = rhs.name;
			return *this;
		}

	};

	typedef std::vector<std::pair<LLViewerObject*,std::string> > obj_info_t;
	typedef std::vector<LLUUID> id_list_t;
	typedef std::vector<std::string> string_list_t;
	typedef std::vector<S32> int_list_t;
	typedef std::vector<MaterialInfo> material_list_t;

	material_list_t mAllMaterials;
	id_list_t mTextures;
	string_list_t mTextureNames;
	obj_info_t mObjects;
	LLVector3 mOffset;
	std::string mImageFormat;
	S32 mTotalNumMaterials;

	DAESaver() {};
	void updateTextureInfo();
	void add(const LLViewerObject* prim, const std::string name);
	bool saveDAE(std::string filename);

private:
	void transformTexCoord(S32 num_vert, LLVector2* coord, LLVector3* positions, LLVector3* normals, LLTextureEntry* te, LLVector3 scale);
	void addSource(daeElement* mesh, const char* src_id, std::string params, const std::vector<F32> &vals);
	void addPolygons(daeElement* mesh, const char* geomID, const char* materialID, LLViewerObject* obj, int_list_t* faces_to_include);
	bool skipFace(LLTextureEntry *te);
	MaterialInfo getMaterial(LLTextureEntry* te);
	void getMaterials(LLViewerObject* obj, material_list_t* ret);
	void getFacesWithMaterial(LLViewerObject* obj, MaterialInfo& mat, int_list_t* ret);
	void generateEffects(daeElement *effects);
	void generateImagesSection(daeElement* images);
};

class ColladaExportFloater : public LLFloater
{
public:
	ColladaExportFloater(const LLSD& key);
	BOOL postBuild();
	void updateSelection();
	
protected:
	void onTexturesSaved();
	
	LLSafeHandle<LLObjectSelection> mObjectSelection;
	LLTimer mTimer;
	typedef std::map<LLUUID, std::string> texture_list_t;
	texture_list_t mTexturesToSave;
	std::string mFilename;
	
private:
	virtual ~ColladaExportFloater();
	/* virtual */ void draw();
	/* virtual */ void onOpen(const LLSD& key);
	void refresh();
	void dirty();
	void onClickExport();
	void onExportFileSelected(const std::vector<std::string>& filenames);
	void onTextureExportCheck();
	void onCommitTextureType();
	void saveTextures();
	void addSelectedObjects();
	void addTexturePreview();
	void updateTitleProgress();
	void updateUI();
	S32 getNumExportableTextures();
	
	DAESaver mSaver;
	S32 mTotal;
	S32 mIncluded;
	S32 mNumTextures;
	S32 mNumExportableTextures;
	std::string mObjectName;
	LLUIString mTitleProgress;
	LLPanel* mTexturePanel;
	LLUUID mCurrentObjectID;
	bool mDirty;
	
	class CacheReadResponder : public LLTextureCache::ReadResponder
	{
		friend class ColladaExportFloater;
	private:
		LLPointer<LLImageFormatted> mFormattedImage;
		LLUUID mID;
		std::string mName;
		S32 mImageType;
		
	public:
		CacheReadResponder(const LLUUID& id, LLImageFormatted* image, std::string name, S32 img_type);
		
		void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat, BOOL imagelocal);
		virtual void completed(bool success);
		static void saveTexturesWorker(void* data);
	};
};

#endif // DAEEXPORT_H_
