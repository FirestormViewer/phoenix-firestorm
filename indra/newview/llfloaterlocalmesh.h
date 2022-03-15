#pragma once

#include "llfloater.h"
class LLObjectSelection;


class LLFloaterLocalMesh : public LLFloater
{
	public:
		LLFloaterLocalMesh(const LLSD& key);
		virtual ~LLFloaterLocalMesh(void);

	public:
		void onOpen(const LLSD& key);
		void onClose(bool app_quitting);
		void onSelectionChangedCallback();
		virtual BOOL postBuild();
		virtual void draw();

		/* add    - loads a new file, adds it to the list and reads it.
		   reload - re-loads a selected file, reapplies it to viewer objects.
		   remove - clears all affected viewer objects and unloads selected file
		   apply  - applies selected file onto a selected viewer object
		   clear  - reverts a selected viewer object to it's normal state
		   show log/show list - toggles between loaded file list, and log.
		*/
		static void onBtnAdd(void* userdata);
		void onBtnAddCallback(std::string filename);

		static void onBtnReload(void* userdata);
		static void onBtnRemove(void* userdata);
		static void onBtnApply(void* userdata);
		static void onBtnClear(void* userdata);
		static void onBtnShowLog(void* userdata);

		void reloadFileList(bool keep_selection);
		void onFileListCommitCallback();
		void reloadLowerUI();	
		void toggleSelectTool(bool toggle);
		LLUUID getCurrentSelectionIfValid();

	private:
		// llsafehandle is deprecated.
		LLPointer<LLObjectSelection> mObjectSelection;

		// since we use this to check if selection changed,
		// and since uuid seems like a safer way to check rather than
		// comparing llvovolumes, we might as well refer to this
		// when querying what is actually selected.
		LLUUID mLastSelectedObject;

};