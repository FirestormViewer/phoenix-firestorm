/* Copyright (c) 2010
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
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
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
#include "llcurl.h"
#include "llscrolllistctrl.h"
#include "llviewertexteditor.h"
#include "llinventorymodel.h"
#include "llviewercontrol.h"
#include "llversionviewer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

// NaCl - missing LSL Preprocessor includes
#include "llinventoryfunctions.h"
#include "llviewerinventory.h"
#include "llvfile.h"
#include "llvoavatarself.h"
// NaCl End

#include "fscommon.h"

#ifdef __GNUC__
// There is a sprintf( ... "%d", size_t_value) buried inside boost::wave. In order to not mess with system header, I rather disable that warning here.
#pragma GCC diagnostic ignored "-Wformat"
#endif

class ScriptMatches : public LLInventoryCollectFunctor
{
public:
	ScriptMatches(std::string name)
	{
		sName = name;
	}
	virtual ~ScriptMatches() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		if(item)
		{
			//LLViewerInventoryCategory* folderp = gInventory.getCategory((item->getParentUUID());
			if(item->getName() == sName)
			{
				if(item->getType() == LLAssetType::AT_LSL_TEXT)return true;
			}
			//return (item->getName() == sName);// && cat->getName() == "#v");
		}
		return false;
	}
private:
	std::string sName;
};

LLUUID FSLSLPreprocessor::findInventoryByName(std::string name)
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	ScriptMatches namematches(name);
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(),cats,items,FALSE,namematches);

	if (items.count())
	{
		LLInventoryModel::item_array_t::iterator it = items.begin();
		it = items.begin();
		LLViewerInventoryItem* foo=it->get();
		return foo->getUUID();
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


#define FAILDEBUG llinfos << "line:" << __LINE__ << llendl;


#define encode_start std::string("//start_unprocessed_text\n/*")
#define encode_end std::string("*/\n//end_unprocessed_text")

std::string FSLSLPreprocessor::encode(std::string script)
{
	
	std::string otext = FSLSLPreprocessor::decode(script);
	
	BOOL mono = mono_directive(script);
	
	otext = boost::regex_replace(otext, boost::regex("([/*])(?=[/*|])",boost::regex::perl), "$1|");
	
	//otext = curl_escape(otext.c_str(), otext.size());
	
	otext = encode_start+otext+encode_end;
	
	otext += "\n//nfo_preprocessor_version 0";
	
	//otext += "\n//^ = determine what featureset is supported";
	otext += llformat("\n//program_version %s", LLAppViewer::instance()->getWindowTitle().c_str());
	
	otext += "\n";
	
	if(mono)otext += "//mono\n";
	else otext += "//lsl2\n";
	

	return otext;
}

std::string FSLSLPreprocessor::decode(std::string script)
{
	
	static S32 startpoint = encode_start.length();
	
	std::string tip = script.substr(0,startpoint);
	
	if(tip != encode_start)
	{
		
		//reportToNearbyChat("no start");
		//if(sp != -1)trigger warningg/error?
		return script;
	}
	
	S32 end = script.find(encode_end);
	
	if(end == -1)
	{
		
		//reportToNearbyChat("no end");
		return script;
	}
	

	std::string data = script.substr(startpoint,end-startpoint);
	//reportToNearbyChat("data="+data);
	

	std::string otext = data;

	

	otext = boost::regex_replace(otext, boost::regex("([/*])\\|",boost::regex::perl), "$1");

	
	//otext = curl_unescape(otext.c_str(),otext.length());

	return otext;
}


std::string scopeript2(std::string& top, S32 fstart, char left = '{', char right = '}')
{
	
	if(fstart >= int(top.length()))return "begin out of bounds";
	
	int cursor = fstart;
	bool noscoped = true;
	bool in_literal = false;
	int count = 0;
	char ltoken = ' ';
	
	do
	{
		char token = top.at(cursor);
		if(token == '"' && ltoken != '\\')in_literal = !in_literal;
		else if(token == '\\' && ltoken == '\\')token = ' ';
		else if(!in_literal)
		{
			if(token == left)
			{
				count += 1;
				noscoped = false;
			}else if(token == right)
			{
				count -= 1;
				noscoped = false;
			}
		}
		ltoken = token;
		cursor += 1;
	}while((count > 0 || noscoped) && cursor < int(top.length()));
	int end = (cursor-fstart);
	if(end > int(top.length()))
	{
		return "end out of bounds";
	}
	
	return top.substr(fstart,(cursor-fstart));
}

inline int const_iterator_to_pos(std::string::const_iterator begin, std::string::const_iterator cursor)
{
	return std::distance(begin, cursor);
}

void shredder(std::string& text)
{
	int cursor = 0;
	if(int(text.length()) == 0)
	{
		text = "y u do dis?";
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
			while(cursor < int(text.length()))
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
	}while(cursor < int(text.length()));
}

std::string FSLSLPreprocessor::lslopt(std::string script)
{
	
	script = " \n"+script;//HACK//this should prevent regex fail for functions starting on line 0, column 0
	//added more to prevent split fail on scripts with no global data
	//this should be fun

	//Removes invalid characters from the script.
	shredder(script);
	
	try
	{
		boost::smatch result;
		if (boost::regex_search(script, result, boost::regex("([\\S\\s]*?)(\\s*default\\s*\\{)([\\S\\s]*)")))
		{
			
			std::string top = result[1];
			std::string bottom = result[2];
			bottom += result[3];

			boost::regex findfuncts("(integer|float|string|key|vector|rotation|list){0,1}[\\}\\s]+([a-zA-Z0-9_]+)\\(");
			//there is a minor problem with this regex, it will 
			//grab extra wnhitespace/newlines in front of functions that do not return a value
			//however this seems unimportant as it is only a couple chars and 
			//never grabs code that would actually break compilation
			
			boost::smatch TOPfmatch;
			std::set<std::string> kept_functions;
			std::map<std::string, std::string> functions;
			
			while(boost::regex_search(std::string::const_iterator(top.begin()), std::string::const_iterator(top.end()), TOPfmatch, findfuncts, boost::match_default))
			{
				
				//std::string type = TOPfmatch[1];
				std::string funcname = TOPfmatch[2];

				int pos = TOPfmatch.position(boost::match_results<std::string::const_iterator>::size_type(0));
				std::string funcb = scopeript2(top, pos);
				functions[funcname] = funcb;
				//reportToNearbyChat("func "+funcname+" added to list["+funcb+"]");
				top.erase(pos,funcb.size());
			}
			
			bool repass = false;
			do
			{
				
				repass = false;
				std::map<std::string, std::string>::iterator func_it;
				for(func_it = functions.begin(); func_it != functions.end(); func_it++)
				{
					
					std::string funcname = func_it->first;
					
					if(kept_functions.find(funcname) == kept_functions.end())
					{
						
						boost::smatch calls;
												//funcname has to be [a-zA-Z0-9_]+, so we know its safe
						boost::regex findcalls(std::string("[^a-zA-Z0-9_]")+funcname+std::string("\\("));
						
						std::string::const_iterator bstart = bottom.begin();
						std::string::const_iterator bend = bottom.end();

						if(boost::regex_search(bstart, bend, calls, findcalls, boost::match_default))
						{
							
							std::string function = func_it->second;
							kept_functions.insert(funcname);
							bottom = function+"\n"+bottom;
							repass = true;
						}
					}
				}
			}while(repass);

			std::map<std::string, std::string> gvars;
			boost::regex findvars("(integer|float|string|key|vector|rotation|list)\\s+([a-zA-Z0-9_]+)([^\\(\\);]*;)");
			boost::smatch TOPvmatch;
			
			while(boost::regex_search(std::string::const_iterator(top.begin()), std::string::const_iterator(top.end()), TOPvmatch, findvars, boost::match_default))
			{
				
				std::string varname = TOPvmatch[2];
				std::string fullref = TOPvmatch[1] + " " + varname+TOPvmatch[3];

				gvars[varname] = fullref;
				int start = const_iterator_to_pos(std::string::const_iterator(top.begin()), TOPvmatch[1].first);
				top.erase(start,fullref.length());
			}
			
			std::map<std::string, std::string>::iterator var_it;
			for(var_it = gvars.begin(); var_it != gvars.end(); var_it++)
			{
				
				std::string varname = var_it->first;
				boost::regex findvcalls(std::string("[^a-zA-Z0-9_]+")+varname+std::string("[^a-zA-Z0-9_]+"));
				boost::smatch vcalls;
				std::string::const_iterator bstart = bottom.begin();
				std::string::const_iterator bend = bottom.end();
				
				if(boost::regex_search(bstart, bend, vcalls, findvcalls, boost::match_default))
				{
					bottom = var_it->second + "\n" + bottom;
				}
			}
			
			script = bottom;
		}
	}
	catch (boost::regex_error& e)
	{
		std::string err = "not a valid regular expression: \"";
		err += e.what();
		err += "\"; optimization skipped";
		////reportToNearbyChat(err);
	}
	catch (...)
	{
		
		////reportToNearbyChat("unexpected exception caught; optimization skipped");
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
		////reportToNearbyChat(err);
	}
	catch (...)
	{
		
		////reportToNearbyChat("unexpected exception caught; compression skipped");
	}
	return script;
}

struct ProcCacheInfo
{
	LLViewerInventoryItem* item;
	FSLSLPreprocessor* self;
};

inline std::string shortfile(std::string in)
{
#if BOOST_FILESYSTEM_VERSION == 3
	return boost::filesystem::path(std::string(in)).filename().string();
#else
	return boost::filesystem::path(std::string(in)).filename();
#endif
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
    bool found_include_directive(ContextT const& ctx, 
        std::string const &filename, bool include_next)
	{
		std::string cfilename = filename.substr(1,filename.length()-2);
		//reportToNearbyChat(cfilename+":found_include_directive");
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
						if(not_cached)mProc->display_error(std::string("Caching ")+cfilename);
						else /*if(changed)*/mProc->display_error(cfilename+std::string(" has changed, recaching..."));
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
        }else
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
		if(it != mProc->cached_assetids.end())
		{
			id = mProc->cached_assetids[filename].asString();
		}else id = "NOT_IN_WORLD";//I guess, still need to add external includes atm
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
		}//else wave did something really fucked up
	}
private:
	FSLSLPreprocessor* mProc;
	std::stack<std::string> mAssetStack;
	std::stack<std::string> mFileStack;
};


std::string cachepath(std::string name)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"lslpreproc",name);
}

void cache_script(std::string name, std::string content)
{
	
	content += "\n";/*hack!*/
	//reportToNearbyChat("writing "+name+" to cache");
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"lslpreproc",name);
	LLAPRFile infile(path.c_str(), LL_APR_WB);
	apr_file_t *fp = infile.getFileHandle();
	if(fp)infile.write(content.c_str(), content.length());
	infile.close();
}

void FSLSLPreprocessor::FSProcCacheCallback(LLVFS *vfs, const LLUUID& iuuid, LLAssetType::EType type, void *userdata, S32 result, LLExtStat extstat)
{
	
	LLUUID uuid = iuuid;
	//reportToNearbyChat("cachecallback called");
	ProcCacheInfo* info =(ProcCacheInfo*)userdata;
	LLViewerInventoryItem* item = info->item;
	FSLSLPreprocessor* self = info->self;
	if(item && self)
	{
		std::string name = item->getName();
		if(result == LL_ERR_NOERR)
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
			if(boost::filesystem::native(name))
			{
				//reportToNearbyChat("native name of "+name);
				self->mCore->mErrorList->setCommentText(std::string("Cached ")+name);
				cache_script(name, content);
				std::set<std::string>::iterator loc = self->caching_files.find(name);
				if(loc != self->caching_files.end())
				{
					//reportToNearbyChat("finalizing cache");
					self->caching_files.erase(loc);
					//self->cached_files.insert(name);
					if(uuid.isNull())uuid.generate();
					item->setAssetUUID(uuid);
					self->cached_assetids[name] = uuid;//.insert(uuid.asString());
					self->start_process();
				}else
				{
					////reportToNearbyChat("something fucked");
				}
			}else self->mCore->mErrorList->setCommentText(std::string("Error: script named '")+name+"' isn't safe to copy to the filesystem. This include will fail.");
		}else
		{
			self->mCore->mErrorList->setCommentText(std::string("Error caching "+name));
		}
	}

	if(info)
	{
		delete info;
	}
}

void FSLSLPreprocessor::preprocess_script(BOOL close, BOOL defcache)
{
	mClose = close;
	mDefinitionCaching = defcache;
	caching_files.clear();
	//this->display_error("PreProc Starting...");
	mCore->mErrorList->setCommentText(std::string("PreProc Starting..."));
	
	LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"")+gDirUtilp->getDirDelimiter()+"lslpreproc");
	std::string script = mCore->mEditor->getText();
	if(mMainScriptName == "")//more sanity
	{
		const LLInventoryItem* item = NULL;
		LLPreview* preview = (LLPreview*)mCore->mUserdata;
		if(preview)
		{
			item = preview->getItem();
		}
		if(item)
		{
			mMainScriptName = item->getName();
		}else
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
list lazy_list_set(list target, integer pos, list newval)\n\
{\n\
    integer end = llGetListLength(target);\n\
    if(end > pos)\n\
    {\n\
        target = llListReplaceList(target,newval,pos,pos);\n\
    }else if(end == pos)\n\
    {\n\
        target += newval;\n\
    }else\n\
    {\n\
        do\n\
        {\n\
            target += [0];\n\
            end += 1;\n\
        }while(end < pos);\n\
        target += newval;\n\
    }\n\
    return target;\n\
}\n\
");

std::string reformat_lazy_lists(std::string script)
{
	BOOL add_set = FALSE;
	std::string nscript = script;
	nscript = boost::regex_replace(nscript, boost::regex("([a-zA-Z0-9_]+)\\[([a-zA-Z0-9_()\"]+)]\\s*=\\s*([a-zA-Z0-9_()\"\\+\\-\\*/]+)([;)])",boost::regex::perl), "$1=lazy_list_set($1,$2,[$3])$4");
	if(nscript != script)
	{
		add_set = TRUE;
	}

	if(add_set == TRUE)
	{
		//add lazy_list_set function to top of script, as it is used
		nscript = utf8str_removeCRLF(lazy_list_set_func) + "\n" + nscript;
	}
	return nscript;
}


inline std::string randstr(int len, std::string chars)
{
	int clen = int(chars.length());
	int built = 0;
	std::string ret;
	while(built < len)
	{
		int r = std::rand() / ( RAND_MAX / clen );
		r = r % clen;//sanity
		ret += chars.at(r);
		built += 1;
	}
	return ret;
}

inline std::string quicklabel()
{
	return std::string("c")+randstr(5,"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
}

std::string minimalize_whitespace(std::string in)
{
	return boost::regex_replace(in, boost::regex("\\s*",boost::regex::perl), "\n");		
}

std::string reformat_switch_statements(std::string script)
{
	std::string buffer = script;
	{
		try
		{
			boost::regex findswitches("\\sswitch\\(");//nasty

			boost::smatch matches;

			static std::string switchstr = "switch(";

			int escape = 100;

			while(boost::regex_search(std::string::const_iterator(buffer.begin()), std::string::const_iterator(buffer.end()), matches, findswitches, boost::match_default) && escape > 1)
			{
				int res = matches.position(boost::match_results<std::string::const_iterator>::size_type(0))+1;
				
				static int slen = switchstr.length();

				std::string arg = scopeript2(buffer, res+slen-1,'(',')');

				//arg *will have* () around it
				if(arg == "begin out of bounds" || arg == "end out of bounds")
				{
					////reportToNearbyChat(arg);
					break;
				}
				//reportToNearbyChat("arg=["+arg+"]");
				std::string rstate = scopeript2(buffer, res+slen+arg.length()-1);

				int cutlen = slen;
				cutlen -= 1;
				cutlen += arg.length();
				cutlen += rstate.length();
				//slen is for switch( and arg has () so we need to - 1 ( to get the right length
				//then add arg len and state len to get section to excise

				//rip off the scope edges
				int slicestart = rstate.find("{")+1;
				rstate = rstate.substr(slicestart,(rstate.rfind("}")-slicestart)-1);
				//reportToNearbyChat("rstate=["+rstate+"]");



				boost::regex findcases("\\scase\\s");

				boost::smatch statematches;

				std::map<std::string,std::string> ifs;

				while(boost::regex_search(std::string::const_iterator(rstate.begin()), std::string::const_iterator(rstate.end()), statematches, findcases, boost::match_default) && escape > 1)
				{
					//if(statematches[0].matched)
					{
						int case_start = statematches.position(boost::match_results<std::string::const_iterator>::size_type(0))+1;//const_iterator2pos(statematches[0].first+1, std::string::const_iterator(rstate.begin()))-1;
						int next_curl = rstate.find("{",case_start+1);
						int next_semi = rstate.find(":",case_start+1);
						int case_end = (next_curl < next_semi && next_curl != -1) ? next_curl : next_semi;
						static int caselen = std::string("case").length();
						if(case_end != -1)
						{
							std::string casearg = rstate.substr(case_start+caselen,case_end-(case_start+caselen));
							//reportToNearbyChat("casearg=["+casearg+"]");
							std::string label = quicklabel();
							ifs[casearg] = label;
							//reportToNearbyChat("BEFORE["+rstate+"]");
							bool addcurl = (case_end == next_curl ? 1 : 0);
							label = "@"+label+";\n";
							if(addcurl)label += "{";
							rstate.erase(case_start,(case_end-case_start) + 1);
							rstate.insert(case_start,label);
							//reportToNearbyChat("AFTER["+rstate+"]");
						}else
						{
							////reportToNearbyChat("error in regex case_end != -1");
							rstate.erase(case_start,caselen);
							rstate.insert(case_start,"error; cannot find { or :");
						}
					}
					escape -= 1;
				}

				std::string deflt = quicklabel();
				bool isdflt = false;
				std::string defstate;
				defstate = boost::regex_replace(rstate, boost::regex("(\\s)(default\\s*?):",boost::regex::perl), "$1\\@"+deflt+";");
				defstate = boost::regex_replace(defstate, boost::regex("(\\s)(default\\s*?)\\{",boost::regex::perl), "$1\\@"+deflt+"; \\{");
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
				defstate = boost::regex_replace(rstate, boost::regex("(\\s)break\\s*;",boost::regex::perl), "$1jump "+brk+";");
				if(defstate != rstate)
				{
					rstate = defstate;
					rstate += "\n@"+brk+";\n";
				}
				rstate = rstate + "}";

				//reportToNearbyChat("replacing["+buffer.substr(res,cutlen)+"] with ["+rstate+"]");
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
			////reportToNearbyChat(err);
		}
		catch (...)
		{
			////reportToNearbyChat("unexpected exception caught; buffer=["+buffer+"]");
		}
	}
	return script;
}

void FSLSLPreprocessor::start_process()
{
	if(waving)
	{
		////reportToNearbyChat("already waving?");
		return;
	}
	waving = TRUE;
	boost::wave::util::file_position_type current_position;
	std::string input = mCore->mEditor->getText();
	std::string rinput = input;
	//Make sure wave does not complain about missing newline at end of script.
	input += "\n";
	std::string output;
	
	std::string name = mMainScriptName;
	BOOL lazy_lists = gSavedSettings.getBOOL("_NACL_PreProcLSLLazyLists");
	BOOL use_switch = gSavedSettings.getBOOL("_NACL_PreProcLSLSwitch");
	std::string settings;
	settings = "Settings: preproc ";
	if (lazy_lists == TRUE)
	{
	  settings = settings + " Lazy Lists";
	} 
	if (use_switch == TRUE)
	{
	  settings = settings + " switches";
	}
	if(gSavedSettings.getBOOL("_NACL_PreProcLSLOptimizer")  == TRUE)
	{
		  settings = settings + " Optimize";
	}
	if(gSavedSettings.getBOOL("_NACL_PreProcEnableHDDInclude") == TRUE)
	{
		   settings = settings + " HDDInclude";
	}
	if(gSavedSettings.getBOOL("_NACL_PreProcLSLTextCompress")== TRUE)
	{
			settings = settings + " Compress";
	}
	//display the settings
	 mCore->mErrorList->setCommentText(std::string(settings));
	 
	 ////reportToNearbyChat(settings);
	BOOL errored = FALSE;
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
		
		std::string path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"")+gDirUtilp->getDirDelimiter()+"lslpreproc"+gDirUtilp->getDirDelimiter();
		ctx.add_include_path(path.c_str());
		if(gSavedSettings.getBOOL("_NACL_PreProcEnableHDDInclude"))
		{
			std::string hddpath = gSavedSettings.getString("_NACL_PreProcHDDIncludeLocation");
			if(hddpath != "")
			{
				ctx.add_include_path(hddpath.c_str());
				ctx.add_sysinclude_path(hddpath.c_str());
			}
		}
		std::string def = llformat("__AGENTKEY__=\"%s\"",gAgent.getID().asString().c_str());//legacy because I used it earlier
		ctx.add_macro_definition(def,false);
		def = llformat("__AGENTID__=\"%s\"",gAgent.getID().asString().c_str());
		ctx.add_macro_definition(def,false);
		def = llformat("__AGENTIDRAW__=%s",gAgent.getID().asString().c_str());
		ctx.add_macro_definition(def,false);
		std::string aname = gAgentAvatarp->getFullname();
		def = llformat("__AGENTNAME__=\"%s\"",aname.c_str());
		ctx.add_macro_definition(def,false);
		def = llformat("__ASSETID__=%s",LLUUID::null.asString().c_str());
		ctx.add_macro_definition(def,false);
		def = llformat("__SHORTFILE__=\"%s\"",name.c_str());
		ctx.add_macro_definition(def,false);

		ctx.add_macro_definition("list(input)=((list)(input))",false);
		ctx.add_macro_definition("float(input)=((float)(input))",false);
		ctx.add_macro_definition("integer(input)=((integer)(input))",false);
		ctx.add_macro_definition("key(input)=((key)(input))",false);
		ctx.add_macro_definition("list(input)=((list)(input))",false);
		ctx.add_macro_definition("rotation(input)=((rotation)(input))",false);
		ctx.add_macro_definition("quaternion(input)=((quaternion)(input))",false);
		ctx.add_macro_definition("string(input)=((string)(input))",false);
		ctx.add_macro_definition("vector(input)=((vector)(input))",false);

		context_type::iterator_type first = ctx.begin();
		context_type::iterator_type last = ctx.end();
	        
        while (first != last)
		{
			if(caching_files.size() != 0)
			{
				waving = FALSE;
				return;
			}
            current_position = (*first).get_position();
			
			std::string token = std::string((*first).get_value().c_str());//stupid boost bitching even though we know its a std::string
			
			if(token == "#line")
			{
				token = "//#line";
			}

			output += token;
			
			if(lazy_lists == FALSE)
			{
				lazy_lists = ctx.is_defined_macro(std::string("USE_LAZY_LISTS"));
			}
			
			if(use_switch == FALSE)
			{
				use_switch = ctx.is_defined_macro(std::string("USE_SWITCHES"));
			}
            ++first;
        }
	}
	catch(boost::wave::cpp_exception const& e)
	{
		errored = TRUE;
		// some preprocessing error
		err = name + "(" + llformat("%d",e.line_no()) + "): " + e.description();
		////reportToNearbyChat(err);
		mCore->mErrorList->setCommentText(err);
	}
	catch(std::exception const& e)
	{
		FAILDEBUG
		errored = TRUE;
		err = std::string(current_position.get_file().c_str()) + "(" + llformat("%d",current_position.get_line()) + "): ";
		err += std::string("exception caught: ") + e.what();
		////reportToNearbyChat(err);
		mCore->mErrorList->setCommentText(err);
	}
	catch (...)
	{
		FAILDEBUG
		errored = TRUE;
		err = std::string(current_position.get_file().c_str()) + llformat("%d",current_position.get_line());
		err += std::string("): unexpected exception caught.");
		////reportToNearbyChat(err);
		mCore->mErrorList->setCommentText(err);
	}

	if(!errored)
	{
		FAILDEBUG
		if(lazy_lists == TRUE)
		{
			try
			{
				mCore->mErrorList->setCommentText("Applying lazy list set transform");
				output = reformat_lazy_lists(output);
			}catch(...)
			{	
				errored = TRUE;
				err = "unexpected exception in lazy list converter.";
				mCore->mErrorList->setCommentText(err);
			}

		}
		if(use_switch == TRUE)
		{
			try
			{
				mCore->mErrorList->setCommentText("Applying switch statement transform");
				output = reformat_switch_statements(output);
			}catch(...)
			{	
				errored = TRUE;
				err = "unexpected exception in switch statement converter.";
				mCore->mErrorList->setCommentText(err);
			}
		}
	}

	if(!mDefinitionCaching)
	{
		if(!errored)
		{
			if(gSavedSettings.getBOOL("_NACL_PreProcLSLOptimizer"))
			{
				mCore->mErrorList->setCommentText("Optimizing out unreferenced user-defined functions and global variables");
				try
				{
					output = lslopt(output);
				}catch(...)
				{	
					errored = TRUE;
					err = "unexpected exception in lsl optimizer";
					mCore->mErrorList->setCommentText(err);
				}
			}
		}
		if(!errored)
		{
			if(gSavedSettings.getBOOL("_NACL_PreProcLSLTextCompress"))
			{
				mCore->mErrorList->setCommentText("Compressing lsltext by removing unnecessary space");
				try
				{
					output = lslcomp(output);
				}catch(...)
				{	
					errored = TRUE;
					err = "unexpected exception in lsl compressor";
					mCore->mErrorList->setCommentText(err);
				}
			}
		}
		output = encode(rinput)+"\n\n"+output;


		LLTextEditor* outfield = mCore->mPostEditor;//getChild<LLViewerTextEditor>("post_process");
		if(outfield)
		{
			outfield->setText(LLStringExplicit(output));
		}
		mCore->mPostScript = output;
		mCore->doSaveComplete((void*)mCore,mClose);
	}
	waving = FALSE;
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

void FSLSLPreprocessor::preprocess_script(BOOL close, BOOL defcache)
{
	LLTextEditor* outfield = mCore->mPostEditor;
	if(outfield)
	{
		outfield->setText(LLStringExplicit(mCore->mEditor->getText()));
	}
	mCore->doSaveComplete((void*)mCore,close);
}

#endif

void FSLSLPreprocessor::display_error(std::string err)
{
	mCore->mErrorList->setCommentText(err);
}


bool FSLSLPreprocessor::mono_directive(std::string const& text, bool agent_inv)
{
	bool domono = agent_inv;
	
	if(text.find("//mono\n") != -1)
	{
		domono = TRUE;
	}
	else if(text.find("//lsl2\n") != -1)
	{
		domono = FALSE;
	}
	return domono;
}
