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

#include "fslslpreprocviewer.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llinventoryfunctions.h"
#include "lltrans.h"
#include "llvfile.h"
#include "llviewercontrol.h"
#include "llcompilequeue.h"

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


#define FAILDEBUG LL_INFOS("FSLSLPreprocessor") << "line:" << __LINE__ << LL_ENDL;


#define encode_start std::string("//start_unprocessed_text\n/*")
#define encode_end std::string("*/\n//end_unprocessed_text")

// Definitions to split the expressions into parts to improve readablility
// (using 'r' as namespace prefix for RE, to avoid conflicts)
// The code relies on none of these expressions having capturing groups.
#define rCMNT "//[^\\n]*+\\n|/\\*(?:(?!\\*/).)*+\\*/" // skip over single- or multi-line comments as a block
#define rSPC "[^][{}()<>@A-Za-z0-9_.,:;!~&|^\"=%/*+-]"
#define rREQ_SPC "(?:" rCMNT "|" rSPC ")++"
#define rOPT_SPC "(?:" rCMNT "|" rSPC ")*+"
#define rTYPE_ID "[a-z]++"
#define rIDENT "[A-Za-z_][A-Za-z0-9_]*+"
#define rCMNT_OR_STR rCMNT "|\"(?:[^\"\\\\]|\\\\[^\\n])*+\"" // skip over strings as a block too
#define rDOT_MATCHES_NEWLINE "(?s)"

std::string FSLSLPreprocessor::encode(const std::string& script)
{
	std::string otext = FSLSLPreprocessor::decode(script);
	
	bool mono = mono_directive(script);
	
	otext = boost::regex_replace(otext, boost::regex("([/*])(?=[/*|])", boost::regex::perl), "$1|");
	
	//otext = curl_escape(otext.c_str(), otext.size());
	
	otext = encode_start + otext + encode_end;
	
	otext += "\n//nfo_preprocessor_version 0";
	
	//otext += "\n//^ = determine what featureset is supported";
	otext += llformat("\n//program_version %s", LLAppViewer::instance()->getWindowTitle().c_str());
	
	time_t utc_time = time_corrected();
	std::string timeStr ="["+LLTrans::getString ("TimeMonth")+"]/["
				 +LLTrans::getString ("TimeDay")+"]/["
				 +LLTrans::getString ("TimeYear")+"] ["
				 +LLTrans::getString ("TimeHour")+"]:["
				 +LLTrans::getString ("TimeMin")+"]:["
				 +LLTrans::getString("TimeSec")+"]";
	LLSD substitution;
	substitution["datetime"] = (S32) utc_time;
	LLStringUtil::format (timeStr, substitution);
	otext += "\n//last_compiled " + timeStr;
	
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
		LL_DEBUGS("FSLSLPreprocessor") << "No start" << LL_ENDL;
		//if(sp != -1)trigger warningg/error?
		return script;
	}
	
	S32 end = script.find(encode_end);
	
	if (end == -1)
	{
		LL_DEBUGS("FSLSLPreprocessor") << "No end" << LL_ENDL;
		return script;
	}

	std::string data = script.substr(startpoint, end - startpoint);
	LL_DEBUGS("FSLSLPreprocessor") << "data = " << data << LL_ENDL;

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
	if (text.empty())
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
			while (cursor < S32(text.length()))
			{
				if (token == '\\' && ltoken == '\\')
				{
					token = ' ';
				}
				if (token == '"' && ltoken != '\\')
				{
					break;
				}
				ltoken = token;
				++cursor;
				token = text[cursor];
			}
		}
		else if (token == '\\' && ltoken == '\\')
		{
			token = ' ';
		}

		if (token != 0xA && token != 0x9 && (
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
	
	try
	{
		std::string bottom;
		std::set<std::string> kept_functions;
		std::map<std::string, std::string> functions;
		std::vector<std::pair<std::string, std::string> > gvars;

		{	// open new scope for local vars

			// Loop over every declaration in the script, classifying it according to type.

			boost::regex finddecls(
				rDOT_MATCHES_NEWLINE
				"(^" // Group 1: RE for a variable declaration.
					rOPT_SPC // skip (but capture) leading whitespace and comments
					// type<space or comments>identifier[<space or comments>]
					rTYPE_ID rREQ_SPC "(" rIDENT ")" rOPT_SPC // Group 2: Identifier
					"(?:=" // optionally with an assignment
						// comments or strings or characters that are not a semicolon
						"(?:" rCMNT_OR_STR "|[^;])++"
					")?;" // the whole assignment is optional, the semicolon isn't
				")"
				"|"
				"(^" // Group 3: RE for a function declaration (captures up to the identifier inclusive)
					rOPT_SPC // skip (but capture) whitespace
					// optionally: type<space or comments>, then ident
					"(?:" rTYPE_ID rREQ_SPC ")?(" rIDENT ")" // Group 4: identifier
				")"
				rOPT_SPC
				"\\(" // this opening paren is the key for it to be a function
				"|"
				"(^" // Group 5: State default, possibly preceded by syntax errors
					rOPT_SPC // skip (but capture) whitespace
					"(?:"
						rCMNT_OR_STR // skip strings and comments
						"|(?!"
							rCMNT_OR_STR
							"|(?<![A-Za-z0-9_])default(?![A-Za-z0-9_])"
						")." // any character that does not start a comment/string/'default'
					")*+" // don't backtrack
					"(?<![A-Za-z0-9_])default(?![A-Za-z0-9_])"
				")"
			);

			boost::smatch result;

			std::string top = std::string("\n") + script;

			while (boost::regex_search(std::string::const_iterator(top.begin()), std::string::const_iterator(top.end()), result, finddecls))
			{
				S32 len;

				if (result[1].matched)
				{
					// variable declaration
					gvars.push_back(std::make_pair(result[2], result[0]));
					len = const_iterator_to_pos(top.begin(), result[0].second);
				}
				else if (result[3].matched)
				{
					// function declaration
					std::string funcname = result[4];
					std::string funcb = scopeript2(top, 0);
					functions[funcname] = funcb;
					len = funcb.length();
				}
				else //if (result[5].matched) // assumed
				{
					// found end of globals or syntax error
					bottom = top;
					break;
				}

				// Delete the declaration just found
				top.erase(0, len);
			}

			if (bottom.empty())
			{
				return script; // don't optimize if there's no default state
			}
		}

		// Find function calls and add the used function declarations back.
		// Each time a function is added to the script a new pass is done
		// so that function calls inside the added functions are seen.

		bool repass;
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
					boost::regex findcalls(std::string() +
						rDOT_MATCHES_NEWLINE
						"(?<![A-Za-z0-9_])(" + funcname + ")" rOPT_SPC "\\(" // a call to the function...
						"|(?:"
							rCMNT_OR_STR // or comment or string...
							"|(?!"
								rCMNT_OR_STR
								"|(?<![A-Za-z0-9_])" + funcname + rOPT_SPC "\\("
							")." // or any other character that is not the start for a match of the above
						")"
					);

					std::string::const_iterator bstart = bottom.begin();
					std::string::const_iterator bend = bottom.end();

					while (boost::regex_search(bstart, bend, calls, findcalls, boost::match_default))
					{
						if (calls[1].matched)
						{
							std::string function = func_it->second;
							kept_functions.insert(funcname);
							bottom = function + bottom;
							repass = true;
							break;
						}
						else
						{
							bstart = calls[0].second;
						}
					}
				}
			}
		}
		while (repass);

		// Find variable invocations and add the declarations back if used.

		std::vector<std::pair<std::string, std::string> >::reverse_iterator var_it;
		for (var_it = gvars.rbegin(); var_it != gvars.rend(); var_it++)
		{

			std::string varname = var_it->first;
			boost::regex findvcalls(std::string() + rDOT_MATCHES_NEWLINE
				"(?<![a-zA-Z0-9_.])(" + varname + ")(?![a-zA-Z0-9_\"])" // invocation of the variable
				"|(?:" rCMNT_OR_STR // a comment or string...
					"|(?!"
						rCMNT_OR_STR
						"|(?<![a-zA-Z0-9_.])" + varname + "(?![a-zA-Z0-9_\"])"
					")." // or any other character that is not the start for a match of the above
				")"
			);
			boost::smatch vcalls;
			std::string::const_iterator bstart = bottom.begin();
			std::string::const_iterator bend = bottom.end();

			while (boost::regex_search(bstart, bend, vcalls, findvcalls, boost::match_default))
			{
				if (vcalls[1].matched)
				{
					bottom = var_it->second + bottom;
					break;
				}
				else
				{
					bstart = vcalls[0].second;
				}
			}
		}

		script = bottom;
	}
	catch (boost::regex_error& e)
	{
		LLStringUtil::format_map_t args;
		args["[WHAT]"] = e.what();
		std::string err = LLTrans::getString("fs_preprocessor_optimizer_regex_err", args);
		LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
		display_error(err);
		throw;
	}
	catch (std::exception& e)
	{
		LLStringUtil::format_map_t args;
		args["[WHAT]"] = e.what();
		std::string err = LLTrans::getString("fs_preprocessor_optimizer_exception", args);
		LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
		display_error(err);
		throw;
	}
	return script;
}

std::string FSLSLPreprocessor::lslcomp(std::string script)
{
	try
	{
		shredder(script);
		script = boost::regex_replace(script, boost::regex("(\\s+)", boost::regex::perl), "\n");
	}
	catch (boost::regex_error& e)
	{
		LLStringUtil::format_map_t args;
		args["[WHAT]"] = e.what();
		std::string err = LLTrans::getString("fs_preprocessor_compress_regex_err", args);
		LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
		display_error(err);
		throw;
	}
	catch (std::exception& e)
	{
		LLStringUtil::format_map_t args;
		args["[WHAT]"] = e.what();
		std::string err = LLTrans::getString("fs_preprocessor_compress_exception", args);
		LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
		display_error(err);
		throw;
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
		std::string cfilename = filename.substr(1, filename.length() - 2);
		LL_DEBUGS("FSLSLPreprocessor") << cfilename << ":found_include_directive" << LL_ENDL;
		LLUUID item_id = FSLSLPreprocessor::findInventoryByName(cfilename);
		if (item_id.notNull())
		{
			LLViewerInventoryItem* item = gInventory.getItem(item_id);
			if (item)
			{
				std::map<std::string,LLUUID>::iterator it = mProc->cached_assetids.find(cfilename);
				bool not_cached = (it == mProc->cached_assetids.end());
				bool changed = true;
				if (!not_cached)
				{
					changed = (mProc->cached_assetids[cfilename] != item->getAssetUUID());
				}
				if (not_cached || changed)
				{
					std::set<std::string>::iterator it = mProc->caching_files.find(cfilename);
					if (it == mProc->caching_files.end())
					{
						if (not_cached)
						{
							LLStringUtil::format_map_t args;
							args["[FILENAME]"] = cfilename;
							mProc->display_message(LLTrans::getString("fs_preprocessor_cache_miss", args));
						}
						else /*if(changed)*/
						{
							LLStringUtil::format_map_t args;
							args["[FILENAME]"] = cfilename;
							mProc->display_message(LLTrans::getString("fs_preprocessor_cache_invalidated", args));
						}
						//one is always true
						mProc->caching_files.insert(cfilename);
						ProcCacheInfo* info = new ProcCacheInfo;
						info->item = item;
						info->self = mProc;
						LLPermissions perm(((LLInventoryItem*)item)->getPermissions());
						gAssetStorage->getInvItemAsset(LLHost(),
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
		std::string def = llformat("%s=\"%s\"", macro.c_str(), id.c_str());
		usefulctx.add_macro_definition(def, false);

		mFileStack.push(filename);
		macro = "__SHORTFILE__";
		usefulctx.remove_macro_definition(macro, true);
		def = llformat("%s=\"%s\"", macro.c_str(), filename.c_str());
		usefulctx.add_macro_definition(def, false);
	}


	template <typename ContextT>
	void returning_from_include_file(ContextT const& ctx)
	{
		ContextT& usefulctx = const_cast<ContextT&>(ctx);
		if (mAssetStack.size() > 1)
		{
			mAssetStack.pop();
			std::string id = mAssetStack.top();
			std::string macro = "__ASSETID__";
			usefulctx.remove_macro_definition(macro, true);
			std::string def = llformat("%s=\"%s\"", macro.c_str(), id.c_str());
			usefulctx.add_macro_definition(def, false);

			mFileStack.pop();
			std::string filename = mFileStack.top();
			macro = "__SHORTFILE__";
			usefulctx.remove_macro_definition(macro, true);
			def = llformat("%s=\"%s\"", macro.c_str(), filename.c_str());
			usefulctx.add_macro_definition(def, false);
		}//else wave did something really wrong
	}

	template <typename ContextT, typename ExceptionT>
	void throw_exception(ContextT const& ctx, ExceptionT const& e)
	{
		std::string err;
		err = "warning: last line of file ends without a newline";
		if (!err.compare( e.description()))
		{
			err = "Ignoring warning: ";
			err += e.description();
			LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
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

void cache_script(std::string name, std::string content)
{
	
	content += "\n";/*hack!*/
	LL_DEBUGS("FSLSLPreprocessor") << "writing " << name << " to cache" << LL_ENDL;
	std::string path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "lslpreproc", name);
	LLAPRFile infile(path.c_str(), LL_APR_WB);
	if (infile.getFileHandle())
	{
		infile.write(content.c_str(), content.length());
	}

	infile.close();
}

void FSLSLPreprocessor::FSProcCacheCallback(LLVFS *vfs, const LLUUID& iuuid, LLAssetType::EType type, void *userdata, S32 result, LLExtStat extstat)
{
	LLUUID uuid = iuuid;
	LL_DEBUGS("FSLSLPreprocessor") << "cachecallback called" << LL_ENDL;
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

			std::string content;
			content.resize(file_length + 1, 0);
			file.read((U8*)&content[0], file_length);

			content = utf8str_removeCRLF(content);
			content = self->decode(content);
			/*content += llformat("\n#define __UP_ITEMID__ __ITEMID__\n#define __ITEMID__ %s\n",uuid.asString().c_str())+content;
			content += "\n#define __ITEMID__ __UP_ITEMID__\n";*/
			//prolly wont work and ill have to be not lazy, but worth a try

			if (boost::filesystem::native(name))
			{
				LL_DEBUGS("FSLSLPreprocessor") << "native name of " << name << LL_ENDL;
				LLStringUtil::format_map_t args;
				args["[FILENAME]"] = name;
				self->display_message(LLTrans::getString("fs_preprocessor_cache_completed", args));
				cache_script(name, content);
				std::set<std::string>::iterator loc = self->caching_files.find(name);
				if (loc != self->caching_files.end())
				{
					LL_DEBUGS("FSLSLPreprocessor") << "finalizing cache" << LL_ENDL;
					self->caching_files.erase(loc);
					//self->cached_files.insert(name);
					if(uuid.isNull())uuid.generate();
					item->setAssetUUID(uuid);
					self->cached_assetids[name] = uuid;//.insert(uuid.asString());
					self->start_process();
				}
				else
				{
					LL_DEBUGS("FSLSLPreprocessor") << "something went wrong" << LL_ENDL;
				}
			}
			else
			{
				LLStringUtil::format_map_t args;
				args["[FILENAME]"] = name;
				self->display_error(LLTrans::getString("fs_preprocessor_cache_unsafe", args));
			}
		}
		else
		{
			LLStringUtil::format_map_t args;
			args["[FILENAME]"] = name;
			self->display_error(LLTrans::getString("fs_preprocessor_caching_err", args));
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
	LLStringUtil::format_map_t args;
	display_message(LLTrans::getString("fs_preprocessor_starting"));
	
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

void FSLSLPreprocessor::preprocess_script(const LLUUID& asset_id, LLScriptQueueData* data, LLAssetType::EType type, const std::string& script_data)
{
	if(!data)
	{
		return;
	}
	
	std::string script = FSLSLPreprocessor::decode(script_data);
	
	mScript = script;
	mAssetID = asset_id;
	mData = data;
	mType = type;
	
	mDefinitionCaching = false;
	caching_files.clear();
	LLStringUtil::format_map_t args;
	display_message(LLTrans::getString("fs_preprocessor_starting"));
	
	LLFile::mkdir(gDirUtilp->getExpandedFilename(LL_PATH_CACHE,"") + gDirUtilp->getDirDelimiter() + "lslpreproc");
	
	if (mData->mItem)
	{
		mMainScriptName = mData->mItem->getName();
	}
	else
	{
		mMainScriptName = "(Unknown)";
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
	std::string ref;
	do
	{
		ref = script;
		script = boost::regex_replace(script, boost::regex(std::string(rDOT_MATCHES_NEWLINE
			rCMNT_OR_STR "|"
			"\\(" rOPT_SPC ) + retype + std::string(rOPT_SPC "\\)" rOPT_SPC
			// group 1: leading parenthesis
			"(\\()?"
			// group 2: identifier
			rOPT_SPC "([a-zA-Z_][a-zA-Z0-9_]*+)"
			rOPT_SPC "\\["
			// group 3: subindex expression
			"((?:"
				rCMNT_OR_STR
				// group 4: recursive bracketed expression
				"|(\\[(?:" rCMNT_OR_STR "|[^][]|(?4))*+\\])" // recursive bracketed expression (e.g. []!=[])
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
	while (script != ref);
}

static std::string reformat_lazy_lists(std::string script)
{
	script = boost::regex_replace(script, boost::regex(rDOT_MATCHES_NEWLINE
		rCMNT_OR_STR "|"
		// group 1: identifier
		"([a-zA-Z_][a-zA-Z0-9_]*+)" rOPT_SPC
		// group 2: expression within brackets
		"\\[((?:" rCMNT_OR_STR
		// group 3: recursive bracketed expression
		"|(\\[(?:" rCMNT_OR_STR "|[^][]|(?3))*+\\])" // recursive bracketed expression (e.g. []!=[])
		"|[^][]" // or anything else
		")+?)\\]" rOPT_SPC // non-greedy
		"=" rOPT_SPC
		// group 4: right-hand side expression
		"((?:" rCMNT_OR_STR
			// group 5: recursive parenthesized expression
			"|(\\((?:" rCMNT_OR_STR "|[^()]|(?5))*+\\))" // recursive parenthesized expression
			"|[^()]" // or anything else
		")+?)" // non-greedy
		"([;)])" // terminated only with a semicolon or a closing parenthesis
		), "?1$1=lazy_list_set\\($1,$2,[$4]\\)$6:$&", boost::format_all);

	// replace typed references followed by bracketed subindex with llList2XXXX,
	// e.g. (rotation)mylist[3] is replaced with llList2Rot(mylist, (3))
	subst_lazy_references(script, "integer", "llList2Integer");
	subst_lazy_references(script, "float", "llList2Float");
	subst_lazy_references(script, "string", "llList2String");
	subst_lazy_references(script, "key", "llList2Key");
	subst_lazy_references(script, "vector", "llList2Vector");
	subst_lazy_references(script, "(?:rotation|quaternion)", "llList2Rot");
	subst_lazy_references(script, "list", "llList2List");

	// add lazy_list_set function to top of script
	// (it can be overriden by a user function if the optimizer is active)
	script = utf8str_removeCRLF(lazy_list_set_func) + "\n" + script;

	return script;
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
			boost::regex findswitches(rDOT_MATCHES_NEWLINE
				rCMNT_OR_STR
				"|" rSPC "++" // optimization to skip over blocks of whitespace faster
				"|(?<![A-Za-z0-9_])(switch" rOPT_SPC "\\()"
				"|."
				);

			boost::smatch matches;
			std::string::const_iterator bstart = buffer.begin();

			while (boost::regex_search(bstart, std::string::const_iterator(buffer.end()), matches, findswitches, boost::match_default))
			{
				if (matches[1].matched)
				{
					S32 res = const_iterator_to_pos(buffer.begin(), matches[1].first);

					// slen excludes the "("
					S32 slen = const_iterator_to_pos(matches[1].first, matches[1].second) - 1;

					std::string arg = scopeript2(buffer, res + slen, '(', ')');

					//arg *will have* () around it
					if (arg == "begin out of bounds" || arg == "end out of bounds")
					{
						break;
					}
					LL_DEBUGS("FSLSLPreprocessor") << "arg=[" << arg << "]" << LL_ENDL;;
					std::string rstate = scopeript2(buffer, res + slen + arg.length());
					S32 cutlen = slen + arg.length() + rstate.length();

					// Call recursively to process nested switch statements (FIRE-10517)
					rstate = reformat_switch_statements(rstate);

					//rip off the scope edges
					S32 slicestart = rstate.find("{") + 1;
					rstate = rstate.substr(slicestart, (rstate.rfind("}") - slicestart) - 1);
					LL_DEBUGS("FSLSLPreprocessor") << "rstate=[" << rstate << "]" << LL_ENDL;

					boost::regex findcases(rDOT_MATCHES_NEWLINE
						rCMNT_OR_STR "|" rSPC "++|(?<![A-Za-z0-9_])(case" rREQ_SPC ")|.");

					boost::smatch statematches;

					std::map<std::string, std::string> ifs;
					std::string::const_iterator rstart = rstate.begin();

					while (boost::regex_search(rstart, std::string::const_iterator(rstate.end()), statematches, findcases, boost::match_default))
					{
						if (statematches[1].matched)
						{
							S32 case_start = const_iterator_to_pos(rstate.begin(), statematches[1].first);
							S32 next_curl = rstate.find("{", case_start + 1);
							S32 next_semi = rstate.find(":", case_start + 1);
							S32 case_end = (next_semi == -1) ? next_curl :
								(next_curl < next_semi && next_curl != -1) ? next_curl : next_semi;
							S32 caselen = const_iterator_to_pos(statematches[1].first, statematches[1].second);
							if (case_end != -1)
							{
								std::string casearg = rstate.substr(case_start + caselen, case_end - (case_start + caselen));
								LL_DEBUGS("FSLSLPreprocessor") << "casearg=[" << casearg << "]" << LL_ENDL;
								std::string label = quicklabel();
								ifs[casearg] = label;
								LL_DEBUGS("FSLSLPreprocessor") << "BEFORE[" << rstate << "]" << LL_ENDL;
								bool addcurl = (case_end == next_curl ? 1 : 0);
								label = "@" + label + ";\n";
								if (addcurl)
								{
									label += "{";
								}
								rstate.erase(case_start, (case_end - case_start) + 1);
								rstate.insert(case_start, label);
								LL_DEBUGS("FSLSLPreprocessor") << "AFTER[" << rstate << "]" << LL_ENDL;
								rstart = rstate.begin() + (case_start + label.length());
							}
							else
							{
								LL_DEBUGS("FSLSLPreprocessor") << "error in regex case_end != -1" << LL_ENDL;
								rstate.erase(case_start, caselen);
								rstate.insert(case_start, "error; cannot find { or :");
								rstart = rstate.begin() + (case_start + std::strlen("error; cannot find { or :"));
							}
						}
						else
						{
							rstart = statematches[0].second;
						}
					}

					std::string deflt = quicklabel();
					bool isdflt = false;
					std::string defstate;
					defstate = boost::regex_replace(rstate, boost::regex(rDOT_MATCHES_NEWLINE
						rCMNT_OR_STR "|" rSPC "++"
						"|(?<![A-Za-z0-9_])(default)(?:" rOPT_SPC ":|(" rOPT_SPC "\\{))"
						, boost::regex::perl), "?1@" + deflt + ";$2:$&", boost::format_all);
					if (defstate != rstate)
					{
						isdflt = true;
						rstate = defstate;
					}
					std::string argl;
					std::string jumptable = "{";

					std::map<std::string, std::string>::iterator ifs_it;
					for (ifs_it = ifs.begin(); ifs_it != ifs.end(); ifs_it++)
					{
						jumptable += "if(" + arg + " == (" + ifs_it->first + "))jump " + ifs_it->second + ";\n";
					}
					if (isdflt)
					{
						jumptable += "jump " + deflt + ";\n";
					}

					rstate = jumptable + rstate + "\n";

					std::string brk = quicklabel();
					defstate = boost::regex_replace(rstate, boost::regex(rDOT_MATCHES_NEWLINE
						rCMNT_OR_STR "|"
						"(?<![A-Za-z0-9_])break(" rOPT_SPC ";)"
						), "?1jump " + brk + "$1:$&", boost::format_all);
					if (defstate != rstate)
					{
						rstate = defstate;
						rstate += "\n@" + brk + ";\n";
					}
					rstate = rstate + "}";

					LL_DEBUGS("FSLSLPreprocessor") << "replacing[" << buffer.substr(res, cutlen) << "] with [" << rstate << "]" << LL_ENDL;
					buffer.erase(res, cutlen);
					buffer.insert(res, rstate);

					bstart = buffer.begin() + (res + rstate.length());

				}
				else /* not matches[1].matched */
				{
					// Found a token that is not "switch" - skip it
					bstart = matches[0].second;
				}
			}
			script = buffer;
		}
		catch (...)
		{
			LL_WARNS("FSLSLPreprocessor") << "unexpected exception caught; buffer=[" << buffer << "]" << LL_ENDL;
			throw;
		}
	}
	return script;
}

void FSLSLPreprocessor::start_process()
{
	if (mWaving)
	{
		LL_WARNS("FSLSLPreprocessor") << "already waving?" << LL_ENDL;
		return;
	}

	mWaving = true;
	boost::wave::util::file_position_type current_position;
	std::string input;
	if (mStandalone)
	{
		input = mScript;
	}
	else
	{
		input = mCore->mEditor->getText();
	}
	std::string rinput = input;
	bool preprocessor_enabled = true;

	// Simple check for the "do not preprocess" marker.  This logic will NOT survive a conversion into some form of sectional preprocessing as discussed in FIRE-9335, but will serve the basic use case given therein.
	{
		std::string::size_type location_index = rinput.find("//fspreprocessor off");
		
		if (location_index != std::string::npos)
		{
			std::string section_scanned = input.substr(0, location_index); // Used to compute the line number at which the marker was found.
			LLStringUtil::format_map_t args;
			args["[LINENUMBER]"] = llformat("%d", std::count(section_scanned.begin(), section_scanned.end(), '\n'));
			display_message(LLTrans::getString("fs_preprocessor_disabled_by_script_marker", args));
			preprocessor_enabled = false;
		}
	}

	// Convert multiline strings for preprocessor
	if (preprocessor_enabled)
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
					{
						state = 2;
					}
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
					{
						state = 4; // multiline comment
					}
					else if (*it == '/')
					{
						state = 6; // single-line comment
					}
					else
					{
						state = 0; // it was just a slash
					}
					break;
				case 4: // inside multiline comment, no '*' seen last
					if (*it == '*')
					{
						state = 5;
					}
					break;
				case 5: // inside multiline comment, '*' seen last
					if (*it == '/')
					{
						state = 0;
					}
					else if (*it != '*')
					{
						state = 4;
					}
					break;
				case 6: // inside single line comment ('//' style)
					if (*it == '\n')
					{
						state = 0;
					}
					break;
				default: // normal code
					if (*it == '"')
					{
						state = 1;
					}
					else if (*it == '/')
					{
						state = 3;
					}
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
	bool errored = false;
	if (preprocessor_enabled)
	{
		std::string settings;
		settings = LLTrans::getString("fs_preprocessor_settings_list_prefix") + " preproc";
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

		LL_DEBUGS("FSLSLPreprocessor") << settings << LL_ENDL;
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
			ctx.add_macro_definition(def, false);
			def = llformat("__AGENTID__=\"%s\"", gAgentID.asString().c_str());
			ctx.add_macro_definition(def, false);
			def = llformat("__AGENTIDRAW__=%s", gAgentID.asString().c_str());
			ctx.add_macro_definition(def, false);
			std::string aname = gAgentAvatarp->getFullname();
			def = llformat("__AGENTNAME__=\"%s\"", aname.c_str());
			ctx.add_macro_definition(def, false);
			def = llformat("__ASSETID__=%s", LLUUID::null.asString().c_str());
			ctx.add_macro_definition(def, false);
			def = llformat("__SHORTFILE__=\"%s\"", name.c_str());
			ctx.add_macro_definition(def, false);

			ctx.add_macro_definition("list(...)=((list)(__VA_ARGS__))", false);
			ctx.add_macro_definition("float(...)=((float)(__VA_ARGS__))", false);
			ctx.add_macro_definition("integer(...)=((integer)(__VA_ARGS__))", false);
			ctx.add_macro_definition("key(...)=((key)(__VA_ARGS__))", false);
			ctx.add_macro_definition("rotation(...)=((rotation)(__VA_ARGS__))", false);
			ctx.add_macro_definition("quaternion(...)=((quaternion)(__VA_ARGS__))", false);
			ctx.add_macro_definition("string(...)=((string)(__VA_ARGS__))", false);
			ctx.add_macro_definition("vector(...)=((vector)(__VA_ARGS__))", false);

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
				
				if (token == "#line")
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
			LLStringUtil::format_map_t args;
			args["[ERR_NAME]"] = e.file_name();
			args["[LINENUMBER]"] = llformat("%d",e.line_no()-1);
			args["[ERR_DESC]"] = e.description();
			std::string err = LLTrans::getString("fs_preprocessor_wave_exception", args);
			LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
			display_error(err);
		}
		catch(boost::wave::cpplexer::lexing_exception const& e)
		{
			errored = true;
			// lexing preprocessing error
			LLStringUtil::format_map_t args;
			args["[ERR_NAME]"] = e.file_name();
			args["[LINENUMBER]"] = llformat("%d",e.line_no()-1);
			args["[ERR_DESC]"] = e.description();
			std::string err = LLTrans::getString("fs_preprocessor_wave_exception", args);
			LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
			display_error(err);
		}
		catch(std::exception const& e)
		{
			FAILDEBUG
			errored = true;
			LLStringUtil::format_map_t args;
			args["[ERR_NAME]"] = std::string(current_position.get_file().c_str());
			args["[LINENUMBER]"] = llformat("%d", current_position.get_line());
			args["[ERR_DESC]"] = e.what();
			display_error(LLTrans::getString("fs_preprocessor_exception", args));
		}
		catch (...)
		{
			FAILDEBUG
			errored = true;
			LLStringUtil::format_map_t args;
			args["[ERR_NAME]"] = std::string(current_position.get_file().c_str());
			args["[LINENUMBER]"] = llformat("%d", current_position.get_line());
			std::string err = LLTrans::getString("fs_preprocessor_error", args);
			LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
			display_error(err);
		}
	}
	
	if (preprocessor_enabled && !errored)
	{
		FAILDEBUG
		if (lazy_lists)
		{
			try
			{
				display_message(LLTrans::getString("fs_preprocessor_lazylist_start"));
				try
				{
					output = reformat_lazy_lists(output);
				}
				catch (boost::regex_error& e)
				{
					LLStringUtil::format_map_t args;
					args["[WHAT]"] = e.what();
					std::string err = LLTrans::getString("fs_preprocessor_lazylist_regex_err", args);
					LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
					display_error(err);
					throw;
				}
				catch (std::exception& e)
				{
					LLStringUtil::format_map_t args;
					args["[WHAT]"] = e.what();
					std::string err = LLTrans::getString("fs_preprocessor_lazylist_exception", args);
					LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
					display_error(err);
					throw;
				}
			}
			catch(...)
			{
				errored = true;
				display_error(LLTrans::getString("fs_preprocessor_lazylist_unexpected_exception"));
			}
		}

		if (use_switch)
		{
			try
			{
				display_message(LLTrans::getString("fs_preprocessor_switchstatement_start"));
				try
				{
					output = reformat_switch_statements(output);
				}
				catch (boost::regex_error& e)
				{
					LLStringUtil::format_map_t args;
					args["[WHAT]"] = e.what();
					std::string err = LLTrans::getString("fs_preprocessor_switchstatement_regex_err", args);
					LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
					display_error(err);
					throw;
				}
				catch (std::exception& e)
				{
					LLStringUtil::format_map_t args;
					args["[WHAT]"] = e.what();
					std::string err = LLTrans::getString("fs_preprocessor_switchstatement_exception", args);
					LL_WARNS("FSLSLPreprocessor") << err << LL_ENDL;
					display_error(err);
					throw;
				}
			}
			catch(...)
			{
				errored = true;
				display_error(LLTrans::getString("fs_preprocessor_switchstatement_unexpected_exception"));
			}
		}
	}

	if (!mDefinitionCaching)
	{
		if (!errored)
		{
			if (preprocessor_enabled && use_optimizer)
			{
				display_message(LLTrans::getString("fs_preprocessor_optimizer_start"));
				try
				{
					output = lslopt(output);
				}
				catch(...)
				{	
					errored = true;
					display_error(LLTrans::getString("fs_preprocessor_optimizer_unexpected_exception"));
				}
			}
		}
		if (!errored)
		{
			if (preprocessor_enabled && use_compression)
			{
				display_message(LLTrans::getString("fs_preprocessor_compress_exception"));
				try
				{
					output = lslcomp(output);
				}
				catch(...)
				{
					errored = true;
					display_error(LLTrans::getString("fs_preprocessor_compress_unexpected_exception"));
				}
			}
		}
		
		if (preprocessor_enabled)
		{
			output = encode(rinput) + "\n\n" + output;
		}
		else
		{
			output = rinput;
		}

		if (mStandalone)
		{
			LLFloaterCompileQueue::scriptPreprocComplete(mAssetID, mData, mType, output);
		}
		else
		{
			FSLSLPreProcViewer* outfield = mCore->mPostEditor;
			if (outfield)
			{
				outfield->setText(LLStringExplicit(output));
			}
			mCore->mPostScript = output;
			mCore->enableSave(TRUE); // The preprocessor run forces a change. (For FIRE-10173) -Sei
			mCore->doSaveComplete((void*)mCore, mClose, mSync);
		}
	}
	mWaving = false;
}

#else

std::string FSLSLPreprocessor::encode(const std::string& script)
{
	LLStringUtil::format_map_t args;
	args["[WHERE]"] = "encode";
	display_error(LLTrans::getString("fs_preprocessor_not_supported", args));
	return script;
}

std::string FSLSLPreprocessor::decode(const std::string& script)
{
	LLStringUtil::format_map_t args;
	args["[WHERE]"] = "decode";
	display_error(LLTrans::getString("fs_preprocessor_not_supported", args));
	return script;
}

std::string FSLSLPreprocessor::lslopt(std::string script)
{
	LLStringUtil::format_map_t args;
	args["[WHERE]"] = "lslopt";
	display_error(LLTrans::getString("fs_preprocessor_not_supported", args));
	return script;
}

void FSLSLPreprocessor::FSProcCacheCallback(LLVFS *vfs, const LLUUID& uuid, LLAssetType::EType type, void *userdata, S32 result, LLExtStat extstat)
{
}

void FSLSLPreprocessor::preprocess_script(BOOL close, bool sync, bool defcache)
{
	FSLSLPreProcViewer* outfield = mCore->mPostEditor;
	if (outfield)
	{
		outfield->setText(LLStringExplicit(mCore->mEditor->getText()));
	}
	mCore->doSaveComplete((void*)mCore, close, sync);
}

void FSLSLPreprocessor::preprocess_script(const LLUUID& asset_id, LLScriptQueueData* data, LLAssetType::EType type, const std::string& script_data)
{
	LLFloaterCompileQueue::scriptPreprocComplete(asset_id, data, type, script_data);
}

#endif

void FSLSLPreprocessor::display_message(const std::string& err)
{
	if (mStandalone)
	{
		LLFloaterCompileQueue::scriptLogMessage(mData, err);
	}
	else
	{
		mCore->mErrorList->addCommentText(err);
	}
}

void FSLSLPreprocessor::display_error(const std::string& err)
{
	if (mStandalone)
	{
		LLFloaterCompileQueue::scriptLogMessage(mData, err);
	}
	else
	{
		LLSD row;
		row["columns"][0]["value"] = err;
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		mCore->mErrorList->addElement(row);
	}
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
