/** 
 * @file kvfloaterflickrupload.h
 * @brief Flickr upload floater
 * @copyright Copyright (c) 2011 Katharine Berry
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef KV_KVFLOATERFLICKRUPLOAD_H
#define KV_KVFLOATERFLICKRUPLOAD_H

#include "llfloater.h"
#include "llcheckboxctrl.h"

#include "llpointer.h"

class LLViewerTexture;
class LLImageFormatted;

class KVFloaterFlickrUpload : public LLFloater
{
public:
	KVFloaterFlickrUpload(const LLSD& key);
	~KVFloaterFlickrUpload();

	BOOL postBuild();
	void draw();
	void saveSettings();
	void uploadSnapshot();

	void confirmToken(bool success, const LLSD &response);
	void authCallback(bool authorised);
	void imageUploaded(bool success, const LLSD& response);

	static void onClickCancel(void* data);
	static void onClickUpload(void* data);
	static KVFloaterFlickrUpload* showFromSnapshot(LLImageFormatted *compressed, LLViewerTexture *img, const LLVector2& img_scale, const LLVector3d& pos_taken_global);

private:
	LLPointer<LLImageFormatted> mCompressedImage;
	LLPointer<LLViewerTexture> mViewerImage;
	LLVector2 mImageScale;
	LLVector3d mPosTakenGlobal;
	std::string mTitle; // Used in the confirmation announcement.
};


#endif // KV_KVFLOATERFLICKRUPLOAD_H
