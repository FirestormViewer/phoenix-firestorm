#pragma once

// linden headers
#include "llmodel.h" // LLMeshSkinInfo

// STL headers
#include <future>


class LLFloaterLocalMesh;

enum LLLocalMeshFileLOD
{
	LOCAL_LOD_LOWEST,
	LOCAL_LOD_LOW,
	LOCAL_LOD_MEDIUM,
	LOCAL_LOD_HIGH
};


/*==========================================*/
/*  LLLocalMeshFace: aka submesh denoted by */
/*  material assignment. holds per-face     */
/*  values for indices, bounding box and    */
/*  vtx pos, normals, uv coords, weights.   */
/*==========================================*/
class LLLocalMeshFace
{
	public:
		struct LLLocalMeshSkinUnit
		{
			std::array<int, 4>   mJointIndices;
			std::array<float, 4> mJointWeights;
		};

	public:
		void setFaceBoundingBox(LLVector4 data_in, bool initial_values = false);

		int                     getNumVerts() { return mPositions.size(); }
		int						getNumIndices() { return mIndices.size(); }

		std::vector<int>&       getIndices()					{ return mIndices; };
		std::vector<LLVector4>& getPositions()					{ return mPositions; };
		std::vector<LLVector4>& getNormals()					{ return mNormals; };
		std::vector<LLVector2>& getUVs()						{ return mUVs; };
		std::vector<LLLocalMeshSkinUnit>& getSkin()				{ return mSkin; }
		std::pair<LLVector4, LLVector4>& getFaceBoundingBox()	{ return mFaceBoundingBox; }

	private:
		std::vector<int>		mIndices;
		std::vector<LLVector4>	mPositions;
		std::vector<LLVector4>	mNormals;
		std::vector<LLVector2>	mUVs;
		std::vector<LLLocalMeshSkinUnit> mSkin;
		std::pair<LLVector4, LLVector4>	 mFaceBoundingBox;
};


/*==========================================*/
/*  LLLocalMeshObject: collection of faces  */
/*  has object name, transform & skininfo,  */
/*  volumeid and volumeparams for vobj.     */
/*  when applied - fills vobj volume.       */
/*==========================================*/
class LLLocalMeshObject
{
	public:
		// life cycle management
		LLLocalMeshObject(std::string name);
		~LLLocalMeshObject();

	public:
		// translation and scale
		void computeObjectBoundingBox();
		void computeObjectTransform();
		void normalizeFaceValues(LLLocalMeshFileLOD lod_iter);

		// applying local object to viewer object
		void fillVolume(LLLocalMeshFileLOD lod);
		void attachSkinInfo();

		// getters
		std::vector<std::unique_ptr<LLLocalMeshFace>>& getFaces(LLLocalMeshFileLOD lod) { return mFaces[lod]; };
		std::pair<LLVector4, LLVector4>& getObjectBoundingBox() { return mObjectBoundingBox; };
		std::string		getObjectName()			{ return mObjectName; };
		LLVector4		getObjectTranslation()	{ return mObjectTranslation; };
		LLVector4		getObjectSize()			{ return mObjectSize; };
		LLVector4		getObjectScale()		{ return mObjectScale; };
		LLMeshSkinInfo& getObjectMeshSkinInfo() { return mMeshSkinInfo; };
		LLVolumeParams	getVolumeParams()		{ return mVolumeParams; };
		bool			getIsRiggedObject();

	private:
		// internal data keeping
		std::array<std::vector<std::unique_ptr<LLLocalMeshFace>>, 4> mFaces;
		std::pair<LLVector4, LLVector4> mObjectBoundingBox;
		std::string     mObjectName;
		LLVector4		mObjectTranslation;
		LLVector4		mObjectSize;
		LLVector4		mObjectScale;

		// vovolume
		LLMeshSkinInfo		mMeshSkinInfo;
		LLUUID				mSculptID;
		LLVolumeParams		mVolumeParams;
};


/*==========================================*/
/*  LLLocalMeshFile:  Single Unit           */
/*  owns filenames [main and lods]          */
/*  owns the loaded local mesh objects      */
/*==========================================*/
class LLLocalMeshFile
{
	// class specific types
	public:
		enum LLLocalMeshFileStatus
		{
			STATUS_NONE,
			STATUS_LOADING,
			STATUS_ACTIVE,
			STATUS_ERROR
		};

		// for future gltf support, possibly more.
		enum LLLocalMeshFileExtension
		{
			EXTEN_DAE,
			EXTEN_NONE
		};

		struct LLLocalMeshFileInfo
		{
			std::string mName;
			LLLocalMeshFileStatus mStatus;
			LLUUID mLocalID;
			std::array<bool, 4> mLODAvailability;
			std::vector<std::string> mObjectList;
		};

		struct LLLocalMeshLoaderReply
		{
			bool mChanged;
			std::vector<std::string> mLog;
			std::array<bool, 4> mStatus;
		};
		
	public:
		// life cycle management
		LLLocalMeshFile(std::string filename, bool try_lods);
		~LLLocalMeshFile();
	
		// disallowing copy
		LLLocalMeshFile(const LLLocalMeshFile& local_mesh_file) = delete;
		LLLocalMeshFile& operator=(const LLLocalMeshFile& local_mesh_file) = delete;

	public:
		// file loading
		void reloadLocalMeshObjects(bool initial_load = false);
		LLLocalMeshFileStatus reloadLocalMeshObjectsCheck();
		void reloadLocalMeshObjectsCallback();
		bool updateLastModified(LLLocalMeshFileLOD lod);
		std::vector<std::unique_ptr<LLLocalMeshObject>>& getObjectVector() { return mLoadedObjectList; };

		// info getters
		bool notifyNeedsUIUpdate();
		LLLocalMeshFileInfo getFileInfo();
		std::string getFilename(LLLocalMeshFileLOD lod) { return mFilenames[lod]; };
		LLUUID getFileID() { return mLocalMeshFileID; };
		std::vector<std::string> getFileLog() { return mLoadingLog; };

		// viewer object
		void updateVObjects();
		void applyToVObject(LLUUID viewer_object_id, int object_index, bool use_scale);

		// misc
		void pushLog(std::string who, std::string what, bool is_error = false);

	private:
		std::array<std::string, 4> mFilenames;
		std::array<LLSD, 4> mLastModified;
		std::array<bool, 4> mLoadedSuccessfully; 
		bool mTryLODFiles;
		std::string mShortName;
		std::vector<std::string> mLoadingLog;
		LLLocalMeshFileExtension mExtension;
		LLLocalMeshFileStatus mLocalMeshFileStatus;
		LLUUID mLocalMeshFileID;
		bool mLocalMeshFileNeedsUIUpdate;

		std::future<LLLocalMeshLoaderReply> mAsyncFuture;
		std::vector<std::unique_ptr<LLLocalMeshObject>> mLoadedObjectList;
		std::vector<LLUUID> mSavedObjectSculptIDs;
};


/*=============================*/
/*  LLLocalMeshSystem:         */
/*  user facing manager class. */
/*=============================*/
class LLLocalMeshSystem : public LLSingleton<LLLocalMeshSystem>
{
	
	public:
		// life cycle management
		LLSINGLETON(LLLocalMeshSystem);
		~LLLocalMeshSystem();

	public:
		// file management
		void addFile(std::string filename, bool try_lods);
		void deleteFile(LLUUID local_file_id);
		void reloadFile(LLUUID local_file_id);

		// viewer object management
		void applyVObject(LLUUID viewer_object_id, LLUUID local_file_id, int object_index, bool use_scale);
		void clearVObject(LLUUID viewer_object_id);

		// high level async support
		void triggerCheckFileAsyncStatus();
		void checkFileAsyncStatus();

		// floater two-way communication
		void registerFloaterPointer(LLFloaterLocalMesh* floater_ptr);
		void triggerFloaterRefresh();
		std::vector<LLLocalMeshFile::LLLocalMeshFileInfo> getFileInfoVector();
		std::vector<std::string> getFileLog(LLUUID local_file_id);

	private:
		std::vector<std::unique_ptr<LLLocalMeshFile>> mLoadedFileList;
		bool mFileAsyncsOngoing;
		LLFloaterLocalMesh* mFloaterPtr;
};