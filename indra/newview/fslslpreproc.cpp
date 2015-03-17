/**
 * @file fslslpreproc.cpp
 * Copyright (c) 2010
 *
 * Modular Systems All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"

#include "fslslpreproc.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llscrolllistctrl.h"
#include "llviewertexteditor.h"
#include "llinventorymodel.h"
#include "llviewercontrol.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// NaCl - missing LSL Preprocessor includes
#include "llinventoryfunctions.h"
#include "llviewerinventory.h"
#include "llvfile.h"
#include "llvoavatarself.h"
#include "llscripteditor.h"
// NaCl End

#include "fscommon.h"

#ifdef __GNUC__
// There is a sprintf( ... "%d", size_t_value) buried inside boost::wave. In order to not mess with system header, I rather disable that warning here.
#pragma GCC diagnostic ignored "-Wformat"
#endif

class ScriptMatches : public LLInventoryCollectFunctor
{
public:
	ScriptMatches(const std::string& name)
	{
		mName = name;
	}

	virtual ~ScriptMatches() {}

	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		return (item && item->getName() == mName && item->getType() == LLAssetType::AT_LSL_TEXT);
	}

private:
	std::string mName;
};

LLUUID FSLSLPreprocessor::findInventoryByName(std::string name)
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	ScriptMatches namematches(name);
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(), cats, items, FALSE, namematches);

	if (!items.empty())
	{
		return items.front()->getUUID();
	}
	return LLUUID::null;
}


std::map<std::string,LLUUID> FSLSLPreprocessor::cached_assetids;

#if !defined(LL_DARWIN) || defined(DARWINPREPROC)

//apparently LL #defined this function which happens to precisely match
//a boost::wave function name, destroying the internet, silly grey furries
#undef equivalent

// Work around stupid Microsoft STL warning
#ifdef LL_WINDOWS
#pragma warning (disable : 4702) // warning C4702: unreachable code
#endif
//#define BOOST_SPIRIT_THREADSAFE

#include <boost/assert.hpp>
#include <boost/wave.hpp>
#include <boost/wave/cpplexer/cpp_lex_token.hpp>    // token class
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp> // lexer class
#include <boost/wave/preprocessing_hooks.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

using namespace boost::regex_constants;


#define FAILDEBUG LL_INFOS() << "line:" << __LINE__ << LL_ENDL;


#define encode_start std::string("//start_unprocessed_text\n/*")
#define encode_end std::string("*/\n//end_unprocessed_text")

// Definitions to split the expressions into parts to improve readablility
// (using 'r' as namespace prefix for RE, to avoid conflicts)
// The code relies on none of these expressions having capturing groups.
#define rCMNT "//.*?\\n|/\\*.*?\\*/" // skip over single- or multi-line comments as a block
#define rREQ_SPC "(?:" rCMNT "|\\s)+"
#define rOPT_SPC "(?:" rCMNT "|\\s)*"
#define rTYPE_ID "[a-z]+"
#define rIDENT "[A-Za-z_][A-Za-z0-9_]*"
#define rCMNT_OR_STR rCMNT "|\"(?:[^\"\\\\]|\\\\[^\\n])*\"" // skip over strings as a block too
#define rDOT_MATCHES_NEWLINE "(?s)"

std::string FSLSLPreprocessor::encode(const std::string& script)
{
	std::string otext = FSLSLPreprocessor::decode(script);
	
	bool mono = mono_directive(script);
	
	otext = boost::regex_replace(otext, boost::regex("([/*])(?=[/*|])",boost::regex::perl), "$1|");
	
	//otext = curl_escape(otext.c_str(), otext.size());
	
	otext = encode_start+otext+encode_end;
	
	otext += "\n//nfo_preprocessor_version 0";
	
	//otext += "\n//^ = determine what featureset is supported";
	otext += llformat("\n//program_version %s", LLAppViewer::instance()->getWindowTitle().c_str());
	
	otext += "\n";
	
	if (mono)
	{
		otext += "//mono\n";
	}
	else
	{
		otext += "//lsl2\n";
	}

	return otext;
}

std::string FSLSLPreprocessor::decode(const std::string& script)
{
	static S32 startpoint = encode_start.length();
	
	std::string tip = script.substr(0, startpoint);
	
	if (tip != encode_start)
	{
		LL_DEBUGS() << "No start" << LL_ENDL;
		//if(sp != -1)trigger warningg/error?
		return script;
	}
	
	S32 end = script.find(encode_end);
	
	if (end == -1)
	{
		LL_DEBUGS() << "No end" << LL_ENDL;
		return script;
	}

	std::string data = script.substr(startpoint, end - startpoint);
	LL_DEBUGS() << "data = " << data << LL_ENDL;

	std::string otext = data;

	otext = boost::regex_replace(otext, boost::regex("([/*])\\|", boost::regex::perl), "$1");

	//otext = curl_unescape(otext.c_str(),otext.length());

	return otext;
}


static std::string scopeript2(std::string& top, S32 fstart, char left = '{', char right = '}')
{
	if (fstart >= S32(top.length()))
	{
		return "begin out of bounds";
	}
	
	S32 cursor = fstart;
	bool noscoped = true;
	bool in_literal = false;
	S32 count = 0;
	char ltoken = ' ';
	
	do
	{
		char token = top.at(cursor);
		if (token == '"' && ltoken != '\\')
		{
			in_literal = !in_literal;
		}
		else if (token == '\\' && ltoken == '\\')
		{
			token = ' ';
		}
		else if (!in_literal)
		{
			if (token == left)
			{
				count += 1;
				noscoped = false;
			}
			else if (token == right)
			{
				count -= 1;
				noscoped = false;
			}
		}
		ltoken = token;
		cursor++;
	}
	while ((count > 0 || noscoped) && cursor < S32(top.length()));

	S32 end = (cursor - fstart);
	if (end > S32(top.length()))
	{
		return "end out of bounds";
	}
	
	return top.substr(fstart,(cursor-fstart));
}

static inline S32 const_iterator_to_pos(std::string::const_iterator begin, std::string::const_iterator cursor)
{
	return std::distance(begin, cursor);
}

static void shredder(std::string& text)
{
	S32 cursor = 0;
	if (text.length() == 0)
	{
		text = "No text to shredder.";
		return;
	}

	char ltoken = ' ';
	do
	{
		char token = text[cursor];
		if (token == '"' && ltoken != '\\')
		{
			ltoken = token;
			token = text[++cursor];
			while(cursor < S32(text.length()))
			{
				if(token == '\\' && ltoken == '\\') token = ' ';
				if(token == '"' && ltoken != '\\')
					break;
				ltoken = token;
				++cursor;
				token = text[cursor];
			}
		}
		else if (token == '\\' && ltoken == '\\')
		{
			token = ' ';
		}

		if(token != 0xA && token != 0x9 && (
		   token < 0x20 ||
		   token == '#' || 
		   token == '$' || 
		   token == '\\' || 
		   token == '\'' || 
		   token == '?' ||
		   token >= 0x7F))
		{
			text[cursor] = ' ';
		}
		ltoken = token;
		++cursor;
	}
	while (cursor < S32(text.length()));
}

std::string FSLSLPreprocessor::lslopt(std::string script)
{
	
	script = " \n" + script;//HACK//this should prevent regex fail for functions starting on line 0, column 0
	//added more to prevent split fail on scripts with no global data
	//this should be fun

	//Removes invalid characters from the script.
	shredder(script);
	
	try
	{
		boost::smatch result;
		if (boost::regex_match(script, result, boost::regex(rDOT_MATCHES_NEWLINE
			"((?:" rCMNT_OR_STR "|.)*?)" // any tokens (non-greedy)
			"(" // capture states (include any whitespace and comments preceding 'default')
				rOPT_SPC "default" rOPT_SPC "\\{.*"
			")"
		)))
		{
			
			std::string top = result[1];
			std::string bottom = result[2];

			boost::regex findfuncts(
				rDOT_MATCHES_NEWLINE
				"^(?:" // Skip variable declarations. RE for a variable declaration follows.
					// skip leading whitespace and comments
					rOPT_SPC
					// type<space or comments>identifier[<space or comments>]
					rTYPE_ID rREQ_SPC rIDENT rOPT_SPC
					"(?:=" // optionally with an assignment
						// comments or strings or characters that are not a semicolon
						"(?:" rCMNT_OR_STR "|[^;])+"
					")?;" // the whole assignment is optional, the semicolon isn't
				")*" // (zero or more variable declarations skipped)
				"(" // start capturing group 1 here (to identify the starting point of the fn def)
					rOPT_SPC // skip whitespace
					// optionally: type<space or comments>
					"(?:" rTYPE_ID rREQ_SPC ")?"
					// identifier (captured as group 2)
					"(" rIDENT ")"
				")" // end of group 1
				rOPT_SPC
				"\\(" // this opening paren is the key for it to be a function
			);
			
			boost::smatch TOPfmatch;
			std::set<std::string> kept_functions;
			std::map<std::string, std::string> functions;
			
			while (boost::regex_search(std::string::const_iterator(top.begin()), std::string::const_iterator(top.end()), TOPfmatch, findfuncts, boost::match_default))
			{
				
				//std::string type = TOPfmatch[1];
				std::string funcname = TOPfmatch[2];

				// Grab starting position of group 1
				S32 pos = TOPfmatch.position(boost::match_results<std::string::const_iterator>::size_type(1));
				std::string funcb = scopeript2(top, pos);
				functions[funcname] = funcb;
				LL_DEBUGS() << "func " << funcname << " added to list[" << funcb << "]" << LL_ENDL;
				top.erase(pos,funcb.size());
			}
			
			bool repass = false;
			do
			{
				
				repass = false;
				std::map<std::string, std::string>::iterator func_it;
				for (func_it = functions.begin(); func_it != functions.end(); func_it++)
				{
					
					std::string funcname = func_it->first;
					
					if (kept_functions.find(funcname) == kept_functions.end())
					{
						
						boost::smatch calls;
												//funcname has to be [a-zA-Z0-9_]+, so we know it's safe
						boost::regex findcalls(std::string(rDOT_MATCHES_NEWLINE
							"(?:" rCMNT_OR_STR "|(?<![a-zA-Z0-9_])(") + funcname
							+ std::string(")" rOPT_SPC "\\(|.)*"));
						
						std::string::const_iterator bstart = bottom.begin();
						std::string::const_iterator bend = bottom.end();

						if (boost::regex_match(bstart, bend, calls, findcalls, boost::match_default))
						{
							if (calls[1].matched)
							{
								std::string function = func_it->second;
								kept_functions.insert(funcname);
								bottom = function + bottom;
								repass = true;
							}
						}
					}
				}
			}
			while (repass);

			std::vector<std::pair<std::string, std::string> > gvars;
			boost::regex findvars(rDOT_MATCHES_NEWLINE
				"^" // must be at start of script (we're deleting them as they are found)
				rOPT_SPC // skip whitespace or comments
				// type<space or comments>identifier (captured as group 1)
				rTYPE_ID rREQ_SPC "(" rIDENT ")" rOPT_SPC
				// optional assignment, non-optional semicolon
				"(?:=(?:" rCMNT_OR_STR "|[^;])+)?;"
			);
			boost::smatch TOPvmatch;
			
			while(boost::regex_search(std::string::const_iterator(top.begin()), std::string::const_iterator(top.end()), TOPvmatch, findvars, boost::match_default))
			{
				
				std::string varname = TOPvmatch[1];
				std::string fullref = TOPvmatch[0];

				gvars.insert(gvars.begin(), std::make_pair(varname, fullref));
				S32 start = const_iterator_to_pos(std::string::const_iterator(top.begin()), TOPvmatch[0].first);
				top.erase(start, fullref.length());
			}

			// top must be empty now, after removing all var and function declarations
			// (if it isn't, it means it has a syntax error such an extra semicolon etc.)
			boost::smatch discarded;
			// should always match but we guard it in an if() just in case
			if (boost::regex_match(top, discarded, boost::regex(rDOT_MATCHES_NEWLINE rOPT_SPC "(.*?)" rOPT_SPC)))
			{
				if (discarded[1].matched)
				{
					top = discarded[1];
					if (!top.empty())
					{
						LL_DEBUGS() << "syntax error in globals section: \"" << top << "\"" << LL_ENDL;
					}
				}
			}
			
			std::vector<std::pair<std::string, std::string> >::iterator var_it;
			for (var_it = gvars.begin(); var_it != gvars.end(); var_it++)
			{
				
				std::string varname = var_it->first;
				boost::regex findvcalls(std::string(rDOT_MATCHES_NEWLINE
					"(?:" rCMNT_OR_STR "|(?<![a-zA-Z0-9_.])(") + varname + std::string(")(?![a-zA-Z0-9_\"])|.)*"));
				boost::smatch vcalls;
				std::string::const_iterator bstart = bottom.begin();
				std::string::const_iterator bend = bottom.end();
				
				if (boost::regex_match(bstart, bend, vcalls, findvcalls, boost::match_default))
				{
					if (vcalls[1].matched)
					{
						bottom = var_it->second + bottom;
					}
				}
			}

			// Don't hide syntax errors - append the unmatched part of the top to the script too
			script = top + bottom;
		}
	}
	catch (boost::regex_error& e)
	{
		std::string err = "not a valid regular expression: \"";
		err += e.what();
		err += "\"; optimization skipped";
		LL_WARNS() << err << LL_ENDL;
	}
	catch (...)
	{
		LL_WARNS() << "unexpected exception caught; optimization skipped" << LL_ENDL;
	}
	return script;
}

std::string FSLSLPreprocessor::lslcomp(std::string script)
{
	try
	{
		shredder(script);
		script = boost::regex_replace(script, boost::regex("(\\s+)",boost::regex::perl), "\n");
	}
	catch (boost::regex_error& e)
	{
		std::string err = "not a valid regular expression: \"";
		err += e.what();
		err += "\"; compression skipped";
		LL_WARNS() << err << LL_ENDL;
	}
	catch (...)
	{
		LL_WARNS() << "unexpected exception caught; compression skipped" << LL_ENDL;
	}
	return script;
}

struct ProcCacheInfo
{
	LLViewerInventoryItem* item;
	FSLSLPreprocessor* self;
};

static inline std::string shortfile(std::string in)
{
	return boost::filesystem::path(std::string(in)).filename().string();
}


class trace_include_files : public boost::wave::context_policies::default_preprocessing_hooks
{
public:
	trace_include_files(FSLSLPreprocessor* proc)
	:   mProc(proc) 
	{
		mAssetStack.push(LLUUID::null.asString());
		mFileStack.push(proc->mMainScriptName);
	}


	template <typename ContextT>
	bool found_include_directive(ContextT const& ctx, std::string const &filename, bool include_next)
	{
		std::string cfilename = filename.substr(1,filename.length()-2);
		LL_DEBUGS() << cfilename << ":found_include_directive" << LL_ENDL;
		LLUUID item_id = FSLSLPreprocessor::findInventoryByName(cfilename);
		if(item_id.notNull())
		{
			LLViewerInventoryItem* item = gInventory.getItem(item_id);
			if(item)
			{
				std::map<std::string,LLUUID>::iterator it = mProc->cached_assetids.find(cfilename);
				bool not_cached = (it == mProc->cached_assetids.end());
				bool changed = true;
				if(!not_cached)
				{
					changed = (mProc->cached_assetids[cfilename] != item->getAssetUUID());
				}
				if (not_cached || changed)
				{
					std::set<std::string>::iterator it = mProc->caching_files.find(cfilename);
					if (it == mProc->caching_files.end())
					{
						if(not_cached)mProc->display_message(std::string("Caching ")+cfilename);
						else /*if(changed)*/mProc->display_message(cfilename+std::string(" has changed, recaching..."));
						//one is always true
						mProc->caching_files.insert(cfilename);
						ProcCacheInfo* info = new ProcCacheInfo;
						info->item = item;
						info->self = mProc;
						LLPermissions perm(((LLInventoryItem*)item)->getPermissions());
						gAssetStorage->getInvItemAsset(LLHost::invalid,
						gAgent.getID(),
						gAgent.getSessionID(),
						perm.getOwner(),
						LLUUID::null,
						item->getUUID(),
						LLUUID::null,
						item->getType(),
						&FSLSLPreprocessor::FSProcCacheCallback,
						info,
						TRUE);
						return true;
					}
				}
			}
		}
		else
		{
			//todo check on HDD in user defined dir for file in question
		}
		//++include_depth;
		return false;
	}

	template <typename ContextT>
	void opened_include_file(ContextT const& ctx, 
		std::string const &relname, std::string const& absname,
		bool is_system_include)
	{
		
		ContextT& usefulctx = const_cast<ContextT&>(ctx);
		std::string id;
		std::string filename = shortfile(relname);//boost::filesystem::path(std::string(relname)).filename();
		std::map<std::string,LLUUID>::iterator it = mProc->cached_assetids.find(filename);
		if (it != mProc->cached_assetids.end())
		{
			id = mProc->cached_assetids[filename].asString();
		}
		else
		{
			id = "NOT_IN_WORLD";//I guess, still need to add external includes atm
		}
		mAssetStack.push(id);
		std::string macro = "__ASSETID__";
		usefulctx.remove_macro_definition(macro, true);
		std::string def = llformat("%s=\"%s\"",macro.c_str(),id.c_str());
		usefulctx.add_macro_definition(def,false);

		mFileStack.push(filename);
		macro = "__SHORTFILE__";
		usefulctx.remove_macro_definition(macro, true);
		def = llformat("%s=\"%s\"",macro.c_str(),filename.c_str());
		usefulctx.add_macro_definition(def,false);
	}


	template <typename ContextT>
	void returning_from_include_file(ContextT const& ctx)
	{
		ContextT& usefulctx = const_cast<ContextT&>(ctx);
		if(mAssetStack.size() > 1)
		{
			mAssetStack.pop();
			std::string id = mAssetStack.top();
			std::string macro = "__ASSETID__";
			usefulctx.remove_macro_definition(macro, true);
			std::string def = llformat("%s=\"%s\"",macro.c_str(),id.c_str());
			usefulctx.add_macro_definition(def,false);

			mFileStack.pop();
			std::string filename = mFileStack.top();
			macro = "__SHORTFILE__";
			usefulctx.remove_macro_definition(macro, true);
			def = llformat("%s=\"%s\"",macro.c_str(),filename.c_str());
			usefulctx.add_macro_definition(def,false);
		}//else wave did something really wrong
	}

	template <typename ContextT, typename ExceptionT>
	void throw_exception(ContextT const& ctx, ExceptionT const& e)
	{
		std::string err;
		err = "warning: last line of file ends without a newline";
		if( !err.compare( e.description()))
		{
			err = "Ignoring warning: ";
			err += e.description();
			LL_WARNS() << err << LL_ENDL;
		}
		else
		{
			boost::throw_exception(e);
		}
	}

private:
	FSLSLPreprocessor* mProc;
	std::stack<std::string> mAssetStack;
	std::stack<std::string> mFileStack;
};


/* unused:
static std::string cachepath(std::string name)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "lslpreproc", name);
}
*/

void cache_script(std::string name, std::string content)
{
	
	content += "\n";/*hack!*/
	LL_DEBUGS() << "writing " << name << " to cache" << LL_ENDL;
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"lslpreproc",name);
	LLAPRFile infile(path.c_str(), LL_APR_WB);
	if( infile.getFileHandle() )
		infile.write(content.c_str(), content.length());

	infile.close();
}

void FSLSLPreprocessor::FSProcCacheCallback(LLVFS *vfs, const LLUUID& iuuid, LLAssetType::EType type, void *userdata, S32 result, LLExtStat extstat)
{
	LLUUID uuid = iuuid;
	LL_DEBUGS() << "cachecallback called" << LL_ENDL;
	ProcCacheInfo* info = (ProcCacheInfo*)userdata;
	LLViewerInventoryItem* item = info->item;
	FSLSLPreprocessor* self = info->self;
	if (item && self)
	{
		std::string name = item->getName();
		if (result == LL_ERR_NOERR)
		{
			LLVFile file(vfs, uuid, type);
			S32 file_length = file.getSize();
			char* buffer = new char[file_length+1];
			file.read((U8*)buffer, file_length);
			// put a EOS at the end
			buffer[file_length] = 0;

			std::string content(buffer);
			content = utf8str_removeCRLF(content);
			content = self->decode(content);
			/*content += llformat("\n#define __UP_ITEMID__ __ITEMID__\n#define __ITEMID__ %s\n",uuid.asString().c_str())+content;
			content += "\n#define __ITEMID__ __UP_ITEMID__\n";*/
			//prolly wont work and ill have to be not lazy, but worth a try
			delete buffer;
			if (boost::filesystem::native(name))
			{
				LL_DEBUGS() << "native name of " << name << LL_ENDL;
				self->display_message("Cached " + name);
				cache_script(name, content);
				std::set<std::string>::iterator loc = self->caching_files.find(name);
				if (loc != self->caching_files.end())
				{
					LL_DEBUGS() << "finalizing cache" << LL_ENDL;
					self->caching_files.erase(loc);
					//self->cached_files.insert(name);
					if(uuid.isNull())uuid.generate();
					item->setAssetUUID(uuid);
					self->cached_assetids[name] = uuid;//.insert(uuid.asString());
					self->start_process();
				}
				else
				{
					LL_DEBUGS() << "something went wrong" << LL_ENDL;
				}
			}
			else self->display_error(std::string("Error: script named '") + name + "' isn't safe to copy to the filesystem. This include will fail.");
		}
		else
		{
			self->display_error(std::string("Error caching "+name));
		}
	}

	if (info)
	{
		delete info;
	}
}

void FSLSLPreprocessor::preprocess_script(BOOL close, bool sync, bool defcache)
{
	mClose = close;
	mSync = sync;
	mDefinitionCaching = defcache;
	caching_files.clear();
	display_message("PreProc Starting...");
	
	LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"") + gDirUtilp->getDirDelimiter() + "lslpreproc");
	std::string script = mCore->mEditor->getText();
	if (mMainScriptName.empty())//more sanity
	{
		const LLInventoryItem* item = NULL;
		LLPreview* preview = (LLPreview*)mCore->mUserdata;
		if (preview)
		{
			item = preview->getItem();
		}

		if (item)
		{
			mMainScriptName = item->getName();
		}
		else
		{
			mMainScriptName = "(Unknown)";
		}
	}
	std::string name = mMainScriptName;
	cached_assetids[name] = LLUUID::null;
	cache_script(name, script);
	//start the party
	start_process();
}

const std::string lazy_list_set_func("\
list lazy_list_set(list L, integer i, list v)\n\
{\n\
    while (llGetListLength(L) < i)\n\
        L = L + 0;\n\
    return llListReplaceList(L, v, i, i);\n\
}\n\
");

static void subst_lazy_references(std::string& script, std::string retype, std::string fn)
{
	script = boost::regex_replace(script, boost::regex(std::string(rDOT_MATCHES_NEWLINE
		rCMNT_OR_STR "|"
		"\\(" rOPT_SPC ) + retype + std::string(rOPT_SPC "\\)" rOPT_SPC
		// group 1: leading parenthesis
		"(\\()?"
		// group 2: identifier
		rOPT_SPC "([a-zA-Z_][a-zA-Z0-9_]*)"
		rOPT_SPC "\\["
		// group 3: subindex expression
		"((?:"
			rCMNT_OR_STR
			// group 4: recursive bracketed expression
			"|(\\[(?:" rCMNT_OR_STR "|[^][]|(?4))\\])" // recursive bracketed expression (e.g. []!=[])
			"|[^][]" // or anything else
		")+?)" // (non-greedy)
		"\\]"
		// group 5: trailing parenthesis
		"(\\))?"
		)),
		// Boost supports conditions in format strings used in regex_replace.
		// ?nX:Y means output X if group n matched, else output Y. $n means output group n.
		// $& means output the whole match, which is used here to not alter the string.
		// Parentheses are used for grouping; they have to be prefixed with \ to output them.
		std::string("?2" // if group 2 matched, we have a (type)variable[index] to substitute
		// if first paren is present, require the second (output the original text if not present)
		"(?1(?5$1") + fn + std::string("\\($2,$3\\)$5:$&):")
		// if first parent is not present, copy $5 verbatim (matched or not)
		+ fn + std::string("\\($2,$3\\)$5):"
		// if $2 didn't match, output whatever matched (string or comment)
		"$&"), boost::format_all); // format_all enables these features
}

static std::string reformat_lazy_lists(std::string script)
{
	bool add_set = false;
	std::string nscript = script;
	nscript = boost::regex_replace(nscript, boost::regex(rDOT_MATCHES_NEWLINE
		rCMNT_OR_STR "|"
		// group 1: identifier
		"([a-zA-Z_][a-zA-Z0-9_]*)" rOPT_SPC
		// group 2: expression within brackets
		"\\[((?:" rCMNT_OR_STR
		// group 3: recursive bracketed expression
		"|(\\[(?:" rCMNT_OR_STR "|[^][]|(?3))\\])" // recursive bracketed expression (e.g. []!=[])
		"|[^][]" // or anything else
		")+?)\\]" rOPT_SPC // non-greedy
		"=" rOPT_SPC
		// group 4: right-hand side expression
		"((?:" rCMNT_OR_STR
			// group 5: recursive parenthesized expression
			"|(\\((?:" rCMNT_OR_STR "|[^()]|(?5))\\))" // recursive parenthesized expression
			"|[^()]" // or anything else
		")+?)" // non-greedy
		"([;)])" // terminated only with a semicolon or a closing parenthesis
		), "?1$1=lazy_list_set\\($1,$2,[$4]\\)$6:$&", boost::format_all);

	if (nscript != script)
	{
		// the function is only necessary if an assignment was made and it's not already defined
		add_set = boost::regex_search(nscript, boost::regex(
			// Find if the function is already defined.
			rDOT_MATCHES_NEWLINE
			"^(?:"
				// Skip variable or function declarations.
				// RE for a variable declaration follows.
				rOPT_SPC // skip spaces and comments
				// type<space or comments>identifier[<space or comments>]
				rTYPE_ID rREQ_SPC rIDENT rOPT_SPC
				"(?:=" // optionally with an assignment
					// comments or strings or characters that are not a semicolon
					"(?:" rCMNT_OR_STR "|[^;])+"
				")?;" // the whole assignment is optional, the semicolon isn't
				"|"
				// RE for function declarations follows.
				rOPT_SPC // skip spaces and comments
				// [type<space or comments>] identifier
				"(?:" rTYPE_ID rREQ_SPC ")?" rIDENT rOPT_SPC
				// open parenthesis, comments or other stuff, close parenthesis
				// (strings can't appear in the parameter list so don't bother to skip them)
				"\\((?:" rCMNT "|[^()])*\\)" rOPT_SPC
				// capturing group 1 used for nested curly braces
				"(\\{(?:" rCMNT_OR_STR "|(?1)|[^{}])\\})" // recursively skip braces
			")*?" // (zero or more variable/function declarations skipped)
			rOPT_SPC "(?:" rTYPE_ID rREQ_SPC ")?lazy_list_set" rOPT_SPC "\\("
		)) ? false : true;
	}

	// replace typed references followed by bracketed subindex with llList2XXXX,
	// e.g. (rotation)mylist[3] is replaced with llList2Rot(mylist, (3))
	subst_lazy_references(nscript, "integer", "llList2Integer");
	subst_lazy_references(nscript, "float", "llList2Float");
	subst_lazy_references(nscript, "string", "llList2String");
	subst_lazy_references(nscript, "key", "llList2Key");
	subst_lazy_references(nscript, "vector", "llList2Vector");
	subst_lazy_references(nscript, "(?:rotation|quaternion)", "llList2Rot");
	subst_lazy_references(nscript, "list", "llList2List");

	if (add_set)
	{
		//add lazy_list_set function to top of script, as it is used
		nscript = utf8str_removeCRLF(lazy_list_set_func) + "\n" + nscript;
	}
	return nscript;
}


static inline std::string randstr(S32 len, std::string chars)
{
	S32 clen = S32(chars.length());
	S32 built = 0;
	std::string ret;
	while (built < len)
	{
		S32 r = std::rand() / ( RAND_MAX / clen );
		r = r % clen;//sanity
		ret += chars.at(r);
		built += 1;
	}
	return ret;
}

static inline std::string quicklabel()
{
	return std::string("c") + randstr(5, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
}

/* unused:
static std::string minimalize_whitespace(std::string in)
{
	return boost::regex_replace(in, boost::regex("\\s*",boost::regex::perl), "\n");
}
*/

static std::string reformat_switch_statements(std::string script)
{
	std::string buffer = script;
	{
		try
		{
			// This expression, combined with regex_match rather than regex_search, matches
			// the last instance of a switch statement. This is important for nested
			// switches (FIRE-10517). If they are scanned in forward order, case/default/break
			// inside nested switches are replaced as if they were part of the same switch.
			boost::regex findswitches(rDOT_MATCHES_NEWLINE
				"(?:" rCMNT_OR_STR
				"|(?<![A-Za-z0-9_])(switch" rOPT_SPC "\\()"
				"|.)*"
				);

			boost::smatch matches;

			S32 escape = 100;

			while(boost::regex_match(std::string::const_iterator(buffer.begin()), std::string::const_iterator(buffer.end()), matches, findswitches, boost::match_default) && matches[1].matched && escape > 1)
			{
				S32 res = matches.position(boost::match_results<std::string::const_iterator>::size_type(1));
				
				static S32 slen = const_iterator_to_pos(matches[1].first, matches[1].second);

				std::string arg = scopeript2(buffer, res+slen-1,'(',')');

				//arg *will have* () around it
				if(arg == "begin out of bounds" || arg == "end out of bounds")
				{
					//reportToNearbyChat(arg);
					break;
				}
				LL_DEBUGS() << "arg=[" << arg << "]" << LL_ENDL;;
				std::string rstate = scopeript2(buffer, res+slen+arg.length()-1);

				S32 cutlen = slen;
				cutlen -= 1;
				cutlen += arg.length();
				cutlen += rstate.length();
				//slen is for switch( and arg has () so we need to - 1 ( to get the right length
				//then add arg len and state len to get section to excise

				//rip off the scope edges
				S32 slicestart = rstate.find("{")+1;
				rstate = rstate.substr(slicestart,(rstate.rfind("}")-slicestart)-1);
				LL_DEBUGS() << "rstate=[" << rstate << "]" << LL_ENDL;

				boost::regex findcases(rDOT_MATCHES_NEWLINE
					"(?:" rCMNT_OR_STR "|(?<![A-Za-z0-9_])(case" rREQ_SPC ")|.)*");

				boost::smatch statematches;

				std::map<std::string,std::string> ifs;

				while(boost::regex_match(std::string::const_iterator(rstate.begin()), std::string::const_iterator(rstate.end()), statematches, findcases, boost::match_default) && statematches[1].matched && escape > 1)
				{
					//if(statematches[0].matched)
					{
						S32 case_start = statematches.position(boost::match_results<std::string::const_iterator>::size_type(1));
						S32 next_curl = rstate.find("{",case_start+1);
						S32 next_semi = rstate.find(":",case_start+1);
						S32 case_end = (next_semi == -1) ? next_curl :
							(next_curl < next_semi && next_curl != -1) ? next_curl : next_semi;
						S32 caselen = const_iterator_to_pos(statematches[1].first, statematches[1].second);
						if(case_end != -1)
						{
							std::string casearg = rstate.substr(case_start+caselen,case_end-(case_start+caselen));
							LL_DEBUGS() << "casearg=[" << casearg << "]" << LL_ENDL;
							std::string label = quicklabel();
							ifs[casearg] = label;
							LL_DEBUGS() << "BEFORE[" << rstate << "]" << LL_ENDL;
							bool addcurl = (case_end == next_curl ? 1 : 0);
							label = "@"+label+";\n";
							if(addcurl)
							{
								label += "{";
							}
							rstate.erase(case_start,(case_end-case_start) + 1);
							rstate.insert(case_start,label);
							LL_DEBUGS() << "AFTER[" << rstate << "]" << LL_ENDL;
						}
						else
						{
							LL_DEBUGS() << "error in regex case_end != -1" << LL_ENDL;
							rstate.erase(case_start,caselen);
							rstate.insert(case_start,"error; cannot find { or :");
						}
					}
					escape -= 1;
				}

				std::string deflt = quicklabel();
				bool isdflt = false;
				std::string defstate;
				defstate = boost::regex_replace(rstate, boost::regex(rDOT_MATCHES_NEWLINE
					rCMNT_OR_STR "|"
					"(?<![A-Za-z0-9_])(default)(?:" rOPT_SPC ":|(" rOPT_SPC "\\{))"
					,boost::regex::perl), "?1@"+deflt+";$2:$&", boost::format_all);
				if(defstate != rstate)
				{
					isdflt = true;
					rstate = defstate;
				}
				std::string argl;
				std::string jumptable = "{";
				/*std::string type = gettype(buffer, arg, res);
				if(type != "void" && type != "-1")
				{
					std::string argl = quicklabel();
					jumptable += type+" "+argl+" = "+arg+";\n";
					arg = argl;
				}else
				{
					reportToNearbyChat("type="+type);
				}*/

				std::map<std::string, std::string>::iterator ifs_it;
				for(ifs_it = ifs.begin(); ifs_it != ifs.end(); ifs_it++)
				{
					jumptable += "if("+arg+" == ("+ifs_it->first+"))jump "+ifs_it->second+";\n";
				}
				if(isdflt)jumptable += "jump "+deflt+";\n";

				rstate = jumptable + rstate + "\n";
			
				std::string brk = quicklabel();
				defstate = boost::regex_replace(rstate, boost::regex(rDOT_MATCHES_NEWLINE
					rCMNT_OR_STR "|"
					"(?<![A-Za-z0-9_])break(" rOPT_SPC ";)"
					), "?1jump "+brk+"$1:$&", boost::format_all);
				if(defstate != rstate)
				{
					rstate = defstate;
					rstate += "\n@"+brk+";\n";
				}
				rstate = rstate + "}";

				LL_DEBUGS() << "replacing[" << buffer.substr(res,cutlen) << "] with [" << rstate << "]" << LL_ENDL;
				buffer.erase(res,cutlen);
				buffer.insert(res,rstate);

				//start = buffer.begin();
				//end = buffer.end();

				escape -= 1;

				//res = buffer.find(switchstr);
			}
			script = buffer;
		}
		catch (boost::regex_error& e)
		{
			std::string err = "not a valid regular expression: \"";
			err += e.what();
			err += "\"; switch statements skipped";
			LL_WARNS() << err << LL_ENDL;
		}
		catch (...)
		{
			LL_WARNS() << "unexpected exception caught; buffer=[" << buffer << "]" << LL_ENDL;
		}
	}
	return script;
}

void FSLSLPreprocessor::start_process()
{
	if (mWaving)
	{
		LL_WARNS() << "already waving?" << LL_ENDL;
		return;
	}

	mWaving = true;
	boost::wave::util::file_position_type current_position;
	std::string input = mCore->mEditor->getText();
	std::string rinput = input;

	// Convert multiline strings for preprocessor
	{
		std::ostringstream oaux;
		// Simple DFA to parse the code for strings and comments,
		// replacing newlines with '\n' inside strings so that the
		// C preprocessor can interpret it correctly.
		// states: 0=normal, 1=seen '"', 2=seen '"...\',
		// 3=seen '/', 4=seen '/*', 5=seen '/*...*', 6=seen '//'
		int state = 0;
		int nlines = 0;
		for (std::string::iterator it = input.begin(); it != input.end(); it++)
		{
			switch (state)
			{
				case 1: // inside string, no '\' seen last
					if (*it == '\n')
					{
						// we're inside a string and detected a newline;
						// replace with "\\n"
						oaux << "\\n";
						nlines++;
						continue; // don't store the newline itself
					}
					else if (*it == '\\')
						state = 2;
					else if (*it == '"')
					{
						oaux << '"';
						// add as many newlines as the string had,
						// to respect original line numbers
						while (nlines)
						{
							oaux << '\n';
							nlines--;
						}
						state = 0;
						continue;
					}
					break;
				case 2: // inside string, '\' seen last
					// just eat the escaped character
					state = 1;
					break;
				case 3: // in code, '/' seen last
					if (*it == '*')
						state = 4; // multiline comment
					else if (*it == '/')
						state = 6; // single-line comment
					else
						state = 0; // it was just a slash
					break;
				case 4: // inside multiline comment, no '*' seen last
					if (*it == '*')
						state = 5;
					break;
				case 5: // inside multiline comment, '*' seen last
					if (*it == '/')
						state = 0;
					else if (*it != '*')
						state = 4;
					break;
				case 6: // inside single line comment ('//' style)
					if (*it == '\n')
						state = 0;
					break;
				default: // normal code
					if (*it == '"')
						state = 1;
					else if (*it == '/')
						state = 3;
			}
			oaux << *it;
		}
		input = oaux.str();
	}
	
	//Make sure wave does not complain about missing newline at end of script.
	input += "\n";
	std::string output;
	
	std::string name = mMainScriptName;
	bool lazy_lists = gSavedSettings.getBOOL("_NACL_PreProcLSLLazyLists");
	bool use_switch = gSavedSettings.getBOOL("_NACL_PreProcLSLSwitch");
	bool use_optimizer = gSavedSettings.getBOOL("_NACL_PreProcLSLOptimizer");
	bool enable_hdd_include = gSavedSettings.getBOOL("_NACL_PreProcEnableHDDInclude");
	bool use_compression = gSavedSettings.getBOOL("_NACL_PreProcLSLTextCompress");
	std::string settings;
	settings = "Settings: preproc";
	if (lazy_lists)
	{
		settings = settings + " LazyLists";
	}
	if (use_switch)
	{
		settings = settings + " Switches";
	}
	if (use_optimizer)
	{
		settings = settings + " Optimize";
	}
	if (enable_hdd_include)
	{
		settings = settings + " HDDInclude";
	}
	if (use_compression)
	{
		settings = settings + " Compress";
	}
	//display the settings
	display_message(settings);

	LL_DEBUGS() << settings << LL_ENDL;
	bool errored = false;
	std::string err;
	try
	{
		trace_include_files tracer(this);
		typedef boost::wave::cpplexer::lex_token<> token_type;
		typedef boost::wave::cpplexer::lex_iterator<token_type> lex_iterator_type;
		typedef boost::wave::context<std::string::iterator, lex_iterator_type, boost::wave::iteration_context_policies::load_file_to_string, trace_include_files >
				context_type;

		context_type ctx(input.begin(), input.end(), name.c_str(), tracer);
		ctx.set_language(boost::wave::enable_long_long(ctx.get_language()));
		ctx.set_language(boost::wave::enable_prefer_pp_numbers(ctx.get_language()));
		ctx.set_language(boost::wave::enable_variadics(ctx.get_language()));
		
		std::string path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"") + gDirUtilp->getDirDelimiter() + "lslpreproc" + gDirUtilp->getDirDelimiter();
		ctx.add_include_path(path.c_str());
		if (enable_hdd_include)
		{
			std::string hddpath = gSavedSettings.getString("_NACL_PreProcHDDIncludeLocation");
			if (!hddpath.empty())
			{
				ctx.add_include_path(hddpath.c_str());
				ctx.add_sysinclude_path(hddpath.c_str());
			}
		}
		std::string def = llformat("__AGENTKEY__=\"%s\"", gAgentID.asString().c_str());//legacy because I used it earlier
		ctx.add_macro_definition(def,false);
		def = llformat("__AGENTID__=\"%s\"", gAgentID.asString().c_str());
		ctx.add_macro_definition(def,false);
		def = llformat("__AGENTIDRAW__=%s", gAgentID.asString().c_str());
		ctx.add_macro_definition(def,false);
		std::string aname = gAgentAvatarp->getFullname();
		def = llformat("__AGENTNAME__=\"%s\"", aname.c_str());
		ctx.add_macro_definition(def,false);
		def = llformat("__ASSETID__=%s", LLUUID::null.asString().c_str());
		ctx.add_macro_definition(def,false);
		def = llformat("__SHORTFILE__=\"%s\"", name.c_str());
		ctx.add_macro_definition(def,false);

		ctx.add_macro_definition("list(...)=((list)(__VA_ARGS__))",false);
		ctx.add_macro_definition("float(...)=((float)(__VA_ARGS__))",false);
		ctx.add_macro_definition("integer(...)=((integer)(__VA_ARGS__))",false);
		ctx.add_macro_definition("key(...)=((key)(__VA_ARGS__))",false);
		ctx.add_macro_definition("rotation(...)=((rotation)(__VA_ARGS__))",false);
		ctx.add_macro_definition("quaternion(...)=((quaternion)(__VA_ARGS__))",false);
		ctx.add_macro_definition("string(...)=((string)(__VA_ARGS__))",false);
		ctx.add_macro_definition("vector(...)=((vector)(__VA_ARGS__))",false);

		context_type::iterator_type first = ctx.begin();
		context_type::iterator_type last = ctx.end();

		while (first != last)
		{
			if (caching_files.size() != 0)
			{
				mWaving = false;
				return;
			}
			current_position = (*first).get_position();
			
			std::string token = std::string((*first).get_value().c_str());//stupid boost bitching even though we know its a std::string
			
			if(token == "#line")
			{
				token = "//#line";
			}

			output += token;
			
			if (!lazy_lists)
			{
				lazy_lists = ctx.is_defined_macro(std::string("USE_LAZY_LISTS"));
			}
			
			if (!use_switch)
			{
				use_switch = ctx.is_defined_macro(std::string("USE_SWITCHES"));
			}
			++first;
		}
	}
	catch(boost::wave::cpp_exception const& e)
	{
		errored = true;
		// some preprocessing error
		err = name + "(" + llformat("%d",e.line_no()-1) + "): " + e.description();
		LL_WARNS() << err << LL_ENDL;
		display_error(err);
	}
	catch(std::exception const& e)
	{
		FAILDEBUG
		errored = true;
		err = std::string(current_position.get_file().c_str()) + "(" + llformat("%d", current_position.get_line()) + "): ";
		err += std::string("exception caught: ") + e.what();
		display_error(err);
	}
	catch (...)
	{
		FAILDEBUG
		errored = true;
		err = std::string(current_position.get_file().c_str()) + llformat("%d", current_position.get_line());
		err += std::string("): unexpected exception caught.");
		LL_WARNS() << err << LL_ENDL;
		display_error(err);
	}

	if (!errored)
	{
		FAILDEBUG
		if (lazy_lists)
		{
			try
			{
				display_message("Applying lazy list set transform");
				output = reformat_lazy_lists(output);
			}
			catch(...)
			{
				errored = true;
				err = "unexpected exception in lazy list converter.";
				display_error(err);
			}

		}
		if (use_switch)
		{
			try
			{
				display_message("Applying switch statement transform");
				output = reformat_switch_statements(output);
			}
			catch(...)
			{
				errored = true;
				err = "unexpected exception in switch statement converter.";
				display_error(err);
			}
		}
	}

	if (!mDefinitionCaching)
	{
		if (!errored)
		{
			if (use_optimizer)
			{
				display_message("Optimizing out unreferenced user-defined functions and global variables");
				try
				{
					output = lslopt(output);
				}
				catch(...)
				{	
					errored = true;
					err = "unexpected exception in lsl optimizer";
					display_error(err);
				}
			}
		}
		if (!errored)
		{
			if (use_compression)
			{
				display_message("Compressing lsltext by removing unnecessary space");
				try
				{
					output = lslcomp(output);
				}
				catch(...)
				{
					errored = true;
					err = "unexpected exception in lsl compressor";
					display_error(err);
				}
			}
		}
		output = encode(rinput) + "\n\n" + output;


		LLTextEditor* outfield = mCore->mPostEditor;
		if (outfield)
		{
			outfield->setText(LLStringExplicit(output));
		}
		mCore->mPostScript = output;
		mCore->doSaveComplete((void*)mCore, mClose, mSync);
	}
	mWaving = false;
}

#else

std::string FSLSLPreprocessor::encode(std::string script)
{
	display_error("(encode) Warning: Preprocessor not supported in this build.");
	return script;
}

std::string FSLSLPreprocessor::decode(std::string script)
{
	display_error("(decode) Warning: Preprocessor not supported in this build.");
	return script;
}

std::string FSLSLPreprocessor::lslopt(std::string script)
{
	display_error("(lslopt) Warning: Preprocessor not supported in this build.");
	return script;
}

void FSLSLPreprocessor::FSProcCacheCallback(LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type, void *userdata, S32 result, LLExtStat extstat)
{
}

void FSLSLPreprocessor::preprocess_script(BOOL close, bool sync, bool defcache)
{
	LLTextEditor* outfield = mCore->mPostEditor;
	if(outfield)
	{
		outfield->setText(LLStringExplicit(mCore->mEditor->getText()));
	}
	mCore->doSaveComplete((void*)mCore,close);
}

#endif

void FSLSLPreprocessor::display_message(const std::string& err)
{
	mCore->mErrorList->addCommentText(err);
}

void FSLSLPreprocessor::display_error(const std::string& err)
{
	LLSD row;
	row["columns"][0]["value"] = err;
	row["columns"][0]["font"] = "SANSSERIF_SMALL";
	mCore->mErrorList->addElement(row);
}


bool FSLSLPreprocessor::mono_directive(std::string const& text, bool agent_inv)
{
	bool domono = agent_inv;
	
	if (text.find("//mono\n") != -1)
	{
		domono = true;
	}
	else if (text.find("//lsl2\n") != -1)
	{
		domono = false;
	}
	return domono;
}
