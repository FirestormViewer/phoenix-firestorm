/**
 *
 * Copyright (c) 2009-2016, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

#ifndef RLV_HELPER_H
#define RLV_HELPER_H

#include "lleventtimer.h"
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "llwearabletype.h"

#include "rlvdefines.h"
#include "rlvcommon.h"

// ============================================================================
// Forward declarations
//

class RlvBehaviourModifier;
struct RlvBehaviourModifierComp;

// ============================================================================
// RlvBehaviourInfo class - Generic behaviour descriptor (used by restrictions, reply and force commands)
//

class RlvBehaviourInfo
{
    typedef std::function<ERlvCmdRet(const LLUUID& idRlvObj, const boost::optional<RlvBehaviourModifierValue>)> modifier_handler_func_t;
public:
    typedef std::map<std::string, std::tuple<ERlvLocalBhvrModifier, std::type_index, modifier_handler_func_t>> modifier_lookup_t;

    enum EBehaviourFlags
    {
        // General behaviour flags
        BHVR_STRICT       = 0x0001,             // Behaviour has a "_sec" version
        BHVR_SYNONYM      = 0x0002,             // Behaviour is a synonym of another
        BHVR_EXTENDED     = 0x0004,             // Behaviour is part of the RLVa extended command set
        BHVR_EXPERIMENTAL = 0x0008,             // Behaviour is part of the RLVa experimental command set
        BHVR_BLOCKED      = 0x0010,             // Behaviour is blocked
        BHVR_DEPRECATED   = 0x0020,             // Behaviour is deprecated
        BHVR_GENERAL_MASK = 0x0FFF,

        // Force-wear specific flags
        FORCEWEAR_WEAR_REPLACE   = 0x0001 << 16,
        FORCEWEAR_WEAR_ADD       = 0x0002 << 16,
        FORCEWEAR_WEAR_REMOVE    = 0x0004 << 16,
        FORCEWEAR_NODE           = 0x0010 << 16,
        FORCEWEAR_SUBTREE        = 0x0020 << 16,
        FORCEWEAR_CONTEXT_NONE   = 0x0100 << 16,
        FORCEWEAR_CONTEXT_OBJECT = 0x0200 << 16,
        FORCEWEAR_MASK           = 0xFFFF << 16
    };

    RlvBehaviourInfo(std::string strBhvr, ERlvBehaviour eBhvr, U32 maskParamType, U32 nBhvrFlags = 0)
        : m_strBhvr(strBhvr), m_eBhvr(eBhvr), m_maskParamType(maskParamType), m_nBhvrFlags(nBhvrFlags) {}
    virtual ~RlvBehaviourInfo() {}

    void                  addModifier(ERlvLocalBhvrModifier eBhvrMod, const std::type_info& valueType, const std::string& strBhvrMod, modifier_handler_func_t fnHandler = nullptr);
    const std::string&    getBehaviour() const      { return m_strBhvr; }
    ERlvBehaviour         getBehaviourType() const  { return m_eBhvr; }
    U32                   getBehaviourFlags() const { return m_nBhvrFlags; }
    U32                   getParamTypeMask() const  { return m_maskParamType; }
    const modifier_lookup_t& getModifiers() const   { return m_BhvrModifiers;  }
    bool                  hasStrict() const         { return m_nBhvrFlags & BHVR_STRICT; }
    bool                  isBlocked() const         { return m_nBhvrFlags & BHVR_BLOCKED; }
    bool                  isExperimental() const    { return m_nBhvrFlags & BHVR_EXPERIMENTAL; }
    bool                  isExtended() const        { return m_nBhvrFlags & BHVR_EXTENDED; }
    bool                  isSynonym() const         { return m_nBhvrFlags & BHVR_SYNONYM; }
    ERlvLocalBhvrModifier lookupBehaviourModifier(const std::string& strBhvrMod) const;
    void                  toggleBehaviourFlag(EBehaviourFlags eBhvrFlag, bool fEnable);

    virtual ERlvCmdRet  processCommand(const RlvCommand& rlvCmd) const { return RLV_RET_NO_PROCESSOR; }
    virtual ERlvCmdRet  processModifier(const RlvCommand& rlvCmd) const;

protected:
    std::string   m_strBhvr;
    ERlvBehaviour m_eBhvr;
    U32           m_nBhvrFlags;
    U32           m_maskParamType;
    modifier_lookup_t m_BhvrModifiers;
};

// ============================================================================
// RlvBehaviourDictionary and related classes
//

class RlvBehaviourDictionary : public LLSingleton<RlvBehaviourDictionary>
{
    friend class RlvFloaterBehaviours;

    LLSINGLETON(RlvBehaviourDictionary);
    ~RlvBehaviourDictionary();
public:
    void addEntry(const RlvBehaviourInfo* pBhvrEntry);
    void addModifier(ERlvBehaviour eBhvr, ERlvBehaviourModifier eModifier, RlvBehaviourModifier* pModifierEntry);
    void addModifier(const RlvBehaviourInfo* pBhvrEntry, ERlvBehaviourModifier eModifier, RlvBehaviourModifier* pModifierEntry);

    /*
     * General helper functions
     */
public:
    void                    clearModifiers(const LLUUID& idRlvObj);
    ERlvBehaviour           getBehaviourFromString(const std::string& strBhvr, ERlvParamType eParamType, bool* pfStrict = NULL) const;
    const RlvBehaviourInfo* getBehaviourInfo(ERlvBehaviour eBhvr, ERlvParamType eParamType) const;
    const RlvBehaviourInfo* getBehaviourInfo(const std::string& strBhvr, ERlvParamType eParamType, bool* pfStrict = nullptr, ERlvLocalBhvrModifier* peBhvrModifier = nullptr) const;
    bool                    getCommands(const std::string& strMatch, ERlvParamType eParamType, std::list<std::string>& cmdList) const;
    bool                    getHasStrict(ERlvBehaviour eBhvr) const;
    RlvBehaviourModifier*   getModifier(ERlvBehaviourModifier eBhvrMod) const { return (eBhvrMod < RLV_MODIFIER_COUNT) ? m_BehaviourModifiers[eBhvrMod] : nullptr; }
    RlvBehaviourModifier*   getModifierFromBehaviour(ERlvBehaviour eBhvr) const;
    void                    toggleBehaviourFlag(const std::string& strBhvr, ERlvParamType eParamType, RlvBehaviourInfo::EBehaviourFlags eBvhrFlag, bool fEnable);

    /*
     * Member variables
     */
protected:
    typedef std::list<const RlvBehaviourInfo*> rlv_bhvrinfo_list_t;
    typedef std::map<std::pair<std::string, ERlvParamType>, const RlvBehaviourInfo*> rlv_string2info_map_t;
    typedef std::multimap<ERlvBehaviour, const RlvBehaviourInfo*> rlv_bhvr2info_map_t;
    typedef std::map<ERlvBehaviour, ERlvBehaviourModifier> rlv_bhvr2mod_map_t;

    rlv_bhvrinfo_list_t   m_BhvrInfoList;
    rlv_string2info_map_t m_String2InfoMap;
    rlv_bhvr2info_map_t   m_Bhvr2InfoMap;
    rlv_bhvr2mod_map_t    m_Bhvr2ModifierMap;
    RlvBehaviourModifier* m_BehaviourModifiers[RLV_MODIFIER_COUNT];
};

// ============================================================================
// RlvCommandHandler and related classes
//

typedef ERlvCmdRet(RlvBhvrHandlerFunc)(const RlvCommand&, bool&);
typedef void(RlvBhvrToggleHandlerFunc)(ERlvBehaviour, bool);
typedef ERlvCmdRet(RlvForceHandlerFunc)(const RlvCommand&);
typedef ERlvCmdRet(RlvReplyHandlerFunc)(const RlvCommand&, std::string&);

//
// RlvCommandHandlerBaseImpl - Base implementation for each command type (the old process(AddRem|Force|Reply)Command functions)
//
template<ERlvParamType paramType> struct RlvCommandHandlerBaseImpl;
template<> struct RlvCommandHandlerBaseImpl<RLV_TYPE_ADDREM> { static ERlvCmdRet processCommand(const RlvCommand&, RlvBhvrHandlerFunc*, RlvBhvrToggleHandlerFunc* = nullptr); };
template<> struct RlvCommandHandlerBaseImpl<RLV_TYPE_FORCE>  { static ERlvCmdRet processCommand(const RlvCommand&, RlvForceHandlerFunc*); };
template<> struct RlvCommandHandlerBaseImpl<RLV_TYPE_REPLY>  { static ERlvCmdRet processCommand(const RlvCommand&, RlvReplyHandlerFunc*);
};

//
// RlvCommandHandler - The actual command handler (Note that a handler is more general than a processor; a handler can - for instance - be used by multiple processors)
//
#if LL_WINDOWS
    #define RLV_TEMPL_FIX(x) template<x>
#else
    #define RLV_TEMPL_FIX(x) template<typename Placeholder = int>
#endif // LL_WINDOWS

template <ERlvParamType templParamType, ERlvBehaviour templBhvr>
struct RlvCommandHandler
{
    RLV_TEMPL_FIX(typename = typename std::enable_if<templParamType == RLV_TYPE_ADDREM>::type) static ERlvCmdRet onCommand(const RlvCommand&, bool&);
    RLV_TEMPL_FIX(typename = typename std::enable_if<templParamType == RLV_TYPE_ADDREM>::type) static void onCommandToggle(ERlvBehaviour, bool);
    RLV_TEMPL_FIX(typename = typename std::enable_if<templParamType == RLV_TYPE_FORCE>::type)  static ERlvCmdRet onCommand(const RlvCommand&);
    RLV_TEMPL_FIX(typename = typename std::enable_if<templParamType == RLV_TYPE_REPLY>::type)  static ERlvCmdRet onCommand(const RlvCommand&, std::string&);
};

// Aliases to improve readability in definitions
template<ERlvBehaviour templBhvr> using RlvBehaviourHandler = RlvCommandHandler<RLV_TYPE_ADDREM, templBhvr>;
template<ERlvBehaviour templBhvr> using RlvBehaviourToggleHandler = RlvBehaviourHandler<templBhvr>;
template<ERlvBehaviour templBhvr> using RlvForceHandler = RlvCommandHandler<RLV_TYPE_FORCE, templBhvr>;
template<ERlvBehaviour templBhvr> using RlvReplyHandler = RlvCommandHandler<RLV_TYPE_REPLY, templBhvr>;

// List of shared handlers
typedef RlvBehaviourToggleHandler<RLV_BHVR_SETCAM_EYEOFFSET> RlvBehaviourCamEyeFocusOffsetHandler;  // Shared between @setcam_eyeoffset, @setcam_eyeoffsetscale and @setcam_focusoffset
typedef RlvBehaviourHandler<RLV_BHVR_REMATTACH> RlvBehaviourAddRemAttachHandler;                    // Shared between @addattach and @remattach
typedef RlvBehaviourHandler<RLV_BHVR_SENDCHANNEL> RlvBehaviourSendChannelHandler;                   // Shared between @sendchannel and @sendchannel_except
typedef RlvBehaviourHandler<RLV_BHVR_SENDIM> RlvBehaviourRecvSendStartIMHandler;                    // Shared between @recvim, @sendim and @startim
typedef RlvBehaviourHandler<RLV_BHVR_SETCAM_FOVMIN> RlvBehaviourSetCamFovHandler;                   // Shared between @setcam_fovmin and @setcam_fovmax
typedef RlvBehaviourToggleHandler<RLV_BHVR_SHOWSELF> RlvBehaviourShowSelfToggleHandler;             // Shared between @showself and @showselfhead
typedef RlvBehaviourHandler<RLV_BHVR_CAMZOOMMIN> RlvBehaviourCamZoomMinMaxHandler;                  // Shared between @camzoommin and @camzoommax (deprecated)
typedef RlvReplyHandler<RLV_BHVR_GETCAM_AVDISTMIN> RlvReplyCamMinMaxModifierHandler;                // Shared between @getcam_avdistmin and @getcam_avdistmax
typedef RlvForceHandler<RLV_BHVR_REMATTACH> RlvForceRemAttachHandler;                               // Shared between @remattach and @detach
typedef RlvForceHandler<RLV_BHVR_SETCAM_EYEOFFSET> RlvForceCamEyeFocusOffsetHandler;                // Shared between @setcam_eyeoffset, @setcam_eyeoffsetscale and @setcam_focusoffset

//
// RlvCommandProcessor - Templated glue class that brings RlvBehaviourInfo, RlvCommandHandlerBaseImpl and RlvCommandHandler together
//
template <ERlvParamType templParamType, ERlvBehaviour templBhvr, typename handlerImpl = RlvCommandHandler<templParamType, templBhvr>, typename baseImpl = RlvCommandHandlerBaseImpl<templParamType>>
class RlvCommandProcessor : public RlvBehaviourInfo
{
public:
    // Default constructor used by behaviour specializations
    RLV_TEMPL_FIX(typename = typename std::enable_if<templBhvr != RLV_BHVR_UNKNOWN>::type)
    RlvCommandProcessor(const std::string& strBhvr, U32 nBhvrFlags = 0) : RlvBehaviourInfo(strBhvr, templBhvr, templParamType, nBhvrFlags) {}

    // Constructor used when we don't want to specialize on behaviour (see RlvBehaviourGenericProcessor)
    RLV_TEMPL_FIX(typename = typename std::enable_if<templBhvr == RLV_BHVR_UNKNOWN>::type)
    RlvCommandProcessor(const std::string& strBhvr, ERlvBehaviour eBhvr, U32 nBhvrFlags = 0) : RlvBehaviourInfo(strBhvr, eBhvr, templParamType, nBhvrFlags) {}

    ERlvCmdRet processCommand(const RlvCommand& rlvCmd) const override { return baseImpl::processCommand(rlvCmd, &handlerImpl::onCommand); }
};

// Aliases to improve readability in definitions
template<ERlvBehaviour templBhvr, typename handlerImpl = RlvCommandHandler<RLV_TYPE_ADDREM, templBhvr>> using RlvBehaviourProcessor = RlvCommandProcessor<RLV_TYPE_ADDREM, templBhvr, handlerImpl>;
template<ERlvBehaviour templBhvr, typename handlerImpl = RlvCommandHandler<RLV_TYPE_FORCE, templBhvr>> using RlvForceProcessor = RlvCommandProcessor<RLV_TYPE_FORCE, templBhvr, handlerImpl>;
template<ERlvBehaviour templBhvr, typename handlerImpl = RlvCommandHandler<RLV_TYPE_REPLY, templBhvr>> using RlvReplyProcessor = RlvCommandProcessor<RLV_TYPE_REPLY, templBhvr, handlerImpl>;

// Provides pre-defined generic implementations of basic behaviours (template voodoo - see original commit for something that still made sense)
template<ERlvBehaviourOptionType templOptionType> struct RlvBehaviourGenericHandler { static ERlvCmdRet onCommand(const RlvCommand& rlvCmd, bool& fRefCount); };
template<ERlvBehaviourOptionType templOptionType> using RlvBehaviourGenericProcessor = RlvBehaviourProcessor<RLV_BHVR_UNKNOWN, RlvBehaviourGenericHandler<templOptionType>>;
template<ERlvBehaviourOptionType templOptionType> struct RlvForceGenericHandler { static ERlvCmdRet onCommand(const RlvCommand& rlvCmd); };
template<ERlvBehaviourOptionType templOptionType> using RlvForceGenericProcessor = RlvForceProcessor<RLV_BHVR_UNKNOWN, RlvForceGenericHandler<templOptionType>>;

// ============================================================================
// RlvBehaviourProcessor and related classes - Handles add/rem comamnds aka "restrictions)
//

template <ERlvBehaviour eBhvr, typename handlerImpl = RlvBehaviourHandler<eBhvr>, typename toggleHandlerImpl = RlvBehaviourToggleHandler<eBhvr>>
class RlvBehaviourToggleProcessor : public RlvBehaviourInfo
{
public:
    RlvBehaviourToggleProcessor(const std::string& strBhvr, U32 nBhvrFlags = 0) : RlvBehaviourInfo(strBhvr, eBhvr, RLV_TYPE_ADDREM, nBhvrFlags) {}
    ERlvCmdRet processCommand(const RlvCommand& rlvCmd) const override { return RlvCommandHandlerBaseImpl<RLV_TYPE_ADDREM>::processCommand(rlvCmd, &handlerImpl::onCommand, &toggleHandlerImpl::onCommandToggle); }
};
template <ERlvBehaviour eBhvr, ERlvBehaviourOptionType optionType, typename toggleHandlerImpl = RlvBehaviourToggleHandler<eBhvr>> using RlvBehaviourGenericToggleProcessor = RlvBehaviourToggleProcessor<eBhvr, RlvBehaviourGenericHandler<optionType>, toggleHandlerImpl>;

// ============================================================================
// RlvBehaviourModifier - stores behaviour modifiers in an - optionally - sorted list and returns the first element (or default value if there are no modifiers)
//

typedef std::tuple<RlvBehaviourModifierValue, LLUUID, ERlvBehaviour> RlvBehaviourModifierValueTuple;

class RlvBehaviourModifier
{
public:
    RlvBehaviourModifier(const std::string strName, const RlvBehaviourModifierValue& defaultValue, bool fAddDefaultOnEmpty, RlvBehaviourModifierComp* pValueComparator = nullptr);
    virtual ~RlvBehaviourModifier();

    /*
     * Member functions
     */
protected:
    virtual void onValueChange() const {}
public:
    bool                             addValue(const RlvBehaviourModifierValue& modValue, const LLUUID& idRlvObj, ERlvBehaviour eBhvr = RLV_BHVR_UNKNOWN);
    static bool                      convertOptionValue(const std::string& optionValue, const std::type_index& modType, RlvBehaviourModifierValue& modValue);
    void                             clearValues(const LLUUID& idRlvObj);
    bool                             getAddDefault() const { return m_fAddDefaultOnEmpty; }
    const RlvBehaviourModifierValue& getDefaultValue() const { return m_DefaultValue; }
    const LLUUID&                    getPrimaryObject() const;
    const std::string&               getName() const { return m_strName; }
    const std::type_info&            getType() const { return m_DefaultValue.type(); }
    const RlvBehaviourModifierValue& getValue() const { return (hasValue()) ? std::get<0>(m_Values.front()) : m_DefaultValue; }
    template<typename T> const T&    getValue() const { return boost::get<T>(getValue()); }
    bool                             hasValue() const;
    bool                             hasValue(const LLUUID& idRlvObj) const;
    void                             removeValue(const RlvBehaviourModifierValue& modValue, const LLUUID& idRlvObj, ERlvBehaviour eBhvr = RLV_BHVR_UNKNOWN);
    void                             setValue(const RlvBehaviourModifierValue& modValue, const LLUUID& idRlvObj);
    void                             setPrimaryObject(const LLUUID& idPrimaryObject);

    typedef boost::signals2::signal<void(const RlvBehaviourModifierValue& newValue)> change_signal_t;
    change_signal_t& getSignal() { return m_ChangeSignal; }

    /*
     * Member variables
     */
protected:
    std::string                          m_strName;
    RlvBehaviourModifierValue            m_DefaultValue;
    bool                                 m_fAddDefaultOnEmpty;
    std::list<RlvBehaviourModifierValueTuple> m_Values;
    change_signal_t                      m_ChangeSignal;
    RlvBehaviourModifierComp*           m_pValueComparator;
};

// ============================================================================
// RlvCommand
//

class RlvCommand
{
public:
    explicit RlvCommand(const LLUUID& idObj, const std::string& strCommand);
    RlvCommand(const RlvCommand& rlvCmd, ERlvParamType eParamType = RLV_TYPE_UNKNOWN);

    /*
     * Member functions
     */
public:
    std::string        asString() const;
    const std::string& getBehaviour() const     { return m_strBehaviour; }
    const RlvBehaviourInfo* getBehaviourInfo() const { return m_pBhvrInfo; }
    ERlvBehaviour      getBehaviourType() const { return (m_pBhvrInfo) ? m_pBhvrInfo->getBehaviourType() : RLV_BHVR_UNKNOWN; }
    U32                getBehaviourFlags() const{ return (m_pBhvrInfo) ? m_pBhvrInfo->getBehaviourFlags() : 0; }
    ERlvLocalBhvrModifier   getBehaviourModifier() const { return m_eBhvrModifier; }
    const LLUUID&      getObjectID() const      { return m_idObj; }
    const std::string& getOption() const        { return m_strOption; }
    const std::string& getParam() const         { return m_strParam; }
    ERlvParamType      getParamType() const     { return m_eParamType; }
    bool               hasOption() const        { return !m_strOption.empty(); }
    bool               isBlocked() const        { return (m_pBhvrInfo) ? m_pBhvrInfo->isBlocked() : false; }
    bool               isModifier() const       { return ERlvLocalBhvrModifier::Unknown != m_eBhvrModifier; }
    bool               isRefCounted() const     { return m_fRefCounted; }
    bool               isStrict() const         { return m_fStrict; }
    bool               isValid() const          { return m_fValid; }
    ERlvCmdRet         processCommand() const   { return (m_pBhvrInfo) ? m_pBhvrInfo->processCommand(*this) : RLV_RET_NO_PROCESSOR; }

protected:
    static bool parseCommand(const std::string& strCommand, std::string& strBehaviour, std::string& strOption,  std::string& strParam);
    bool               markRefCounted() const   { return m_fRefCounted = true; }

    /*
     * Operators
     */
public:
    bool operator ==(const RlvCommand&) const;

    /*
     * Member variables
     */
protected:
    bool                    m_fValid = false;
    LLUUID                  m_idObj;
    std::string             m_strBehaviour;
    const RlvBehaviourInfo* m_pBhvrInfo = nullptr;
    ERlvParamType           m_eParamType = RLV_TYPE_UNKNOWN;
    ERlvLocalBhvrModifier   m_eBhvrModifier = ERlvLocalBhvrModifier::Unknown;
    bool                    m_fStrict = false;
    std::string             m_strOption;
    std::string             m_strParam;
    mutable bool            m_fRefCounted = false;

    friend class RlvHandler;
    friend class RlvObject;
    template<ERlvParamType> friend struct RlvCommandHandlerBaseImpl;
};

// ============================================================================
// Command option parsing utility classes
//

class RlvCommandOptionHelper
{
public:
    // NOTE: this function is destructive (reference value may still change on parsing failure)
    template<typename T> static bool parseOption(const std::string& strOption, T& valueOption);
    template<typename T> static T parseOption(const std::string& strOption)
    {
        T value;
        parseOption<T>(strOption, value);
        return value;
    }
    static bool parseStringList(const std::string& strOption, std::vector<std::string>& optionList, const std::string& strSeparator = std::string(RLV_OPTION_SEPARATOR));
};

struct RlvCommandOptionGeneric
{
    bool isAttachmentPoint() const      { return (!isEmpty()) && (typeid(LLViewerJointAttachment*) == m_varOption.type()); }
    bool isAttachmentPointGroup() const { return (!isEmpty()) && (typeid(ERlvAttachGroupType) == m_varOption.type()); }
    bool isEmpty() const                { return typeid(boost::blank) == m_varOption.type(); }
    bool isNumber() const               { return (!isEmpty()) && (typeid(float) == m_varOption.type()); }
    bool isSharedFolder() const         { return (!isEmpty()) && (typeid(LLViewerInventoryCategory*) == m_varOption.type()); }
    bool isString() const               { return (!isEmpty()) && (typeid(std::string) == m_varOption.type()); }
    bool isUUID() const                 { return (!isEmpty()) && (typeid(LLUUID) == m_varOption.type()); }
    bool isVector() const               { return (!isEmpty()) && (typeid(LLVector3d) == m_varOption.type()); }
    bool isWearableType() const         { return (!isEmpty()) && (typeid(LLWearableType::EType) == m_varOption.type()); }

    LLViewerJointAttachment*   getAttachmentPoint() const { return (isAttachmentPoint()) ? boost::get<LLViewerJointAttachment*>(m_varOption) : NULL; }
    ERlvAttachGroupType        getAttachmentPointGroup() const { return (isAttachmentPointGroup()) ? boost::get<ERlvAttachGroupType>(m_varOption) : RLV_ATTACHGROUP_INVALID; }
    LLViewerInventoryCategory* getSharedFolder() const { return (isSharedFolder()) ? boost::get<LLViewerInventoryCategory*>(m_varOption) : NULL; }
    float                      getNumber() const { return (isNumber()) ? boost::get<float>(m_varOption) : 0.0f; }
    const std::string&         getString() const { return (isString()) ? boost::get<std::string>(m_varOption) : LLStringUtil::null; }
    const LLUUID&              getUUID() const { return (isUUID()) ? boost::get<LLUUID>(m_varOption) : LLUUID::null; }
    const LLVector3d&          getVector() const { return (isVector()) ? boost::get<LLVector3d>(m_varOption) : LLVector3d::zero; }
    LLWearableType::EType      getWearableType() const { return (isWearableType()) ? boost::get<LLWearableType::EType>(m_varOption) : LLWearableType::WT_INVALID; }

    typedef boost::variant<boost::blank, LLViewerJointAttachment*, ERlvAttachGroupType, LLViewerInventoryCategory*, std::string, LLUUID, LLWearableType::EType, LLVector3d, float> rlv_option_generic_t;
    void operator=(const rlv_option_generic_t& optionValue) { m_varOption = optionValue; }
protected:
    rlv_option_generic_t m_varOption;
};

// ============================================================================
// Command option parsing utility classes (these still need refactoring to fit the new methodology)
//

struct RlvCommandOption
{
protected:
    RlvCommandOption() : m_fValid(false) {}
public:
    virtual ~RlvCommandOption() {}

public:
    virtual bool isEmpty() const { return false; }
    virtual bool isValid() const { return m_fValid; }
protected:
    bool m_fValid;
};

struct RlvCommandOptionGetPath : public RlvCommandOption
{
    typedef boost::function<void(const uuid_vec_t&)> getpath_callback_t;
    RlvCommandOptionGetPath(const RlvCommand& rlvCmd, getpath_callback_t cb = NULL);

    bool              isCallback() const { return m_fCallback; }
    /*virtual*/ bool  isEmpty() const    { return m_idItems.empty(); }
    const uuid_vec_t& getItemIDs() const { return m_idItems; }

    // NOTE: Both functions are COF-based rather than items gathered from mAttachedObjects or gAgentWearables
    static bool getItemIDs(const LLViewerJointAttachment* pAttachPt, uuid_vec_t& idItems);
    static bool getItemIDs(LLWearableType::EType wtType, uuid_vec_t& idItems);

protected:
    bool       m_fCallback; // true if a callback is schedueled
    uuid_vec_t m_idItems;
};

struct RlvCommandOptionAdjustHeight : public RlvCommandOption
{
    RlvCommandOptionAdjustHeight(const RlvCommand& rlvCmd);

    F32 m_nPelvisToFoot;
    F32 m_nPelvisToFootDeltaMult;
    F32 m_nPelvisToFootOffset;
};

// ============================================================================
// RlvObject
//

class RlvObject
{
public:
    RlvObject(const LLUUID& idObj);

    /*
     * Member functions
     */
public:
    const RlvCommand& addCommand(const RlvCommand& rlvCmd, bool& fAdded);
    bool              removeCommand(const RlvCommand& rlvCmd);

    std::string getStatusString(const std::string& strFilter, const std::string& strSeparator) const;
    bool        hasBehaviour(ERlvBehaviour eBehaviour, bool fStrictOnly) const;
    bool        hasBehaviour(ERlvBehaviour eBehaviour, const std::string& strOption, bool fStrictOnly) const;


    /*
     * Accessors
     */
public:
    S32           getAttachPt() const   { return m_idxAttachPt; }
    const LLUUID& getObjectID() const   { return m_idObj; }
    const LLUUID& getRootID() const     { return m_idRoot; }
    bool          hasLookup() const     { return m_fLookup; }
    const rlv_command_list_t& getCommandList() const { return m_Commands; }

    /*
     * Local-scope modifiers
     */
public:
    void                      clearModifiers(ERlvBehaviour eBhvr);
    void                      clearModifierValue(ERlvLocalBhvrModifier eBhvrMod);
    template<typename T> bool getModifierValue(ERlvLocalBhvrModifier eBhvrModifier, T& value) const;
    void                      setModifierValue(ERlvLocalBhvrModifier eBhvrMod, const RlvBehaviourModifierValue& modValue);

    /*
     * Member variables
     */
protected:
    S32                m_idxAttachPt;       // The object's attachment point (or 0 if it's not an attachment)
    LLUUID             m_idObj;             // The object's UUID
    LLUUID             m_idRoot;            // The UUID of the object's root (may or may not be different from m_idObj)
    bool               m_fLookup;           // true if the object existed in gObjectList at one point in time
    S16                m_nLookupMisses;     // Count of unsuccessful lookups in gObjectList by the GC
    rlv_command_list_t m_Commands;          // List of behaviours held by this object (in the order they were received)
    typedef std::map<ERlvLocalBhvrModifier, RlvBehaviourModifierValue> bhvr_modifier_map_t;
    bhvr_modifier_map_t m_Modifiers;        // List of (local scope) modifiers set on this object

    friend class RlvHandler;
};

// ============================================================================
// RlvForceWear
//

class RlvForceWear : public LLSingleton<RlvForceWear>
{
    LLSINGLETON_EMPTY_CTOR(RlvForceWear);

public:
    // Folders
    enum EWearAction { ACTION_WEAR_REPLACE, ACTION_WEAR_ADD, ACTION_REMOVE };
    enum EWearFlags { FLAG_NONE = 0x00, FLAG_MATCHALL = 0x01, FLAG_DEFAULT = FLAG_NONE };
    void forceFolder(const LLViewerInventoryCategory* pFolder, EWearAction eAction, EWearFlags eFlags);

    // Generic
    static bool isWearAction(EWearAction eAction) { return (ACTION_WEAR_REPLACE == eAction) || (ACTION_WEAR_ADD == eAction); }
    static bool isWearableItem(const LLInventoryItem* pItem);
    static bool isWearingItem(const LLInventoryItem* pItem);

    // Nostrip
    static bool isStrippable(const LLUUID& idItem) { return isStrippable(gInventory.getItem(idItem)); }
    static bool isStrippable(const LLInventoryItem* pItem);

    // Attachments
    static bool isForceDetachable(const LLViewerObject* pAttachObj, bool fCheckComposite = true, const LLUUID& idExcept = LLUUID::null);
    static bool isForceDetachable(const LLViewerJointAttachment* pAttachPt, bool fCheckComposite = true, const LLUUID& idExcept = LLUUID::null);
    void forceDetach(const LLViewerObject* pAttachObj);
    void forceDetach(const LLViewerJointAttachment* ptAttachPt);

    // Wearables
    static bool isForceRemovable(const LLViewerWearable* pWearable, bool fCheckComposite = true, const LLUUID& idExcept = LLUUID::null);
    static bool isForceRemovable(LLWearableType::EType wtType, bool fCheckComposite = true, const LLUUID& idExcept = LLUUID::null);
    void forceRemove(const LLViewerWearable* pWearable);
    void forceRemove(LLWearableType::EType wtType);

public:
    void done();
protected:
    void addAttachment(const LLViewerInventoryItem* pItem, EWearAction eAction);
    void remAttachment(const LLViewerObject* pAttachObj);
    void addWearable(const LLViewerInventoryItem* pItem, EWearAction eAction);
    void remWearable(const LLViewerWearable* pWearable);

    // Convenience (prevents long lines that run off the screen elsewhere)
    bool isAddAttachment(const LLViewerInventoryItem* pItem) const
    {
        bool fFound = false;
        for (addattachments_map_t::const_iterator itAddAttachments = m_addAttachments.begin();
                (!fFound) && (itAddAttachments != m_addAttachments.end()); ++itAddAttachments)
        {
            const LLInventoryModel::item_array_t& wearItems = itAddAttachments->second;
            fFound = (std::find_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)) != wearItems.end());
        }
        return fFound;
    }
    bool isRemAttachment(const LLViewerObject* pAttachObj) const
    {
        return std::find(m_remAttachments.begin(), m_remAttachments.end(), pAttachObj) != m_remAttachments.end();
    }
    bool isAddWearable(const LLViewerInventoryItem* pItem) const
    {
        bool fFound = false;
        for (addwearables_map_t::const_iterator itAddWearables = m_addWearables.begin();
                (!fFound) && (itAddWearables != m_addWearables.end()); ++itAddWearables)
        {
            const LLInventoryModel::item_array_t& wearItems = itAddWearables->second;
            fFound = (std::find_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)) != wearItems.end());
        }
        return fFound;
    }
    bool isRemWearable(const LLViewerWearable* pWearable) const
    {
        return std::find(m_remWearables.begin(), m_remWearables.end(), pWearable) != m_remWearables.end();
    }

    /*
     * Pending attachments
     */
public:
    static void updatePendingAttachments();
protected:
    void addPendingAttachment(const LLUUID& idItem, U8 idxPoint);
    void remPendingAttachment(const LLUUID& idItem);

protected:
    typedef std::pair<LLWearableType::EType, LLInventoryModel::item_array_t> addwearable_pair_t;
    typedef std::map<LLWearableType::EType, LLInventoryModel::item_array_t> addwearables_map_t;
    addwearables_map_t               m_addWearables;
    typedef std::pair<S32, LLInventoryModel::item_array_t> addattachment_pair_t;
    typedef std::map<S32, LLInventoryModel::item_array_t> addattachments_map_t;
    addattachments_map_t             m_addAttachments;
    LLInventoryModel::item_array_t   m_addGestures;
    std::vector<LLViewerObject*>     m_remAttachments;  // This should match the definition of LLAgentWearables::llvo_vec_t
    std::list<const LLViewerWearable*> m_remWearables;
    LLInventoryModel::item_array_t   m_remGestures;

    typedef std::map<LLUUID, U8> pendingattachments_map_t;
    pendingattachments_map_t         m_pendingAttachments;
};

// ============================================================================
// RlvBehaviourNotifyObserver
//

class RlvBehaviourNotifyHandler : public LLSingleton<RlvBehaviourNotifyHandler>
{
    LLSINGLETON(RlvBehaviourNotifyHandler);
    virtual ~RlvBehaviourNotifyHandler() { if (m_ConnCommand.connected()) m_ConnCommand.disconnect(); }

public:
    void addNotify(const LLUUID& idObj, S32 nChannel, const std::string& strFilter)
    {
        m_Notifications.insert(std::pair<LLUUID, notifyData>(idObj, notifyData(nChannel, strFilter)));
    }
    void removeNotify(const LLUUID& idObj, S32 nChannel, const std::string& strFilter)
    {
        for (std::multimap<LLUUID, notifyData>::iterator itNotify = m_Notifications.lower_bound(idObj),
                endNotify = m_Notifications.upper_bound(idObj); itNotify != endNotify; ++itNotify)
        {
            if ( (itNotify->second.nChannel == nChannel) && (itNotify->second.strFilter == strFilter) )
            {
                m_Notifications.erase(itNotify);
                break;
            }
        }
        if (m_Notifications.empty())
            delete this;    // Delete ourself if we have nothing to do
    }
    static void sendNotification(const std::string& strText, const std::string& strSuffix = LLStringUtil::null);

    /*
     * Event handlers
     */
public:
    static void onWear(LLWearableType::EType eType, bool fAllowed);
    static void onTakeOff(LLWearableType::EType eType, bool fAllowed);
    static void onAttach(const LLViewerJointAttachment* pAttachPt, bool fAllowed);
    static void onDetach(const LLViewerJointAttachment* pAttachPt, bool fAllowed);
    static void onReattach(const LLViewerJointAttachment* pAttachPt, bool fAllowed);
           void onSit(const LLUUID& idObj, bool fAllowed);
           void onStand(const LLUUID& idObj, bool fAllowed);
protected:
    void        onCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet, bool fInternal);

protected:
    struct notifyData
    {
        S32         nChannel;
        std::string strFilter;
        notifyData(S32 channel, const std::string& filter) : nChannel(channel), strFilter(filter) {}
    };
    std::multimap<LLUUID, notifyData> m_Notifications;
    boost::signals2::connection m_ConnCommand;
};


// ============================================================================
// Various helper classes/timers/functors
//

class RlvGCTimer : public LLEventTimer
{
public:
    RlvGCTimer() : LLEventTimer(30.0) {}
    virtual bool tick();
};

// NOTE: Unused as of SL-3.7.2
class RlvCallbackTimerOnce : public LLEventTimer
{
public:
    typedef boost::function<void ()> nullary_func_t;
public:
    RlvCallbackTimerOnce(F32 nPeriod, nullary_func_t cb) : LLEventTimer(nPeriod), m_Callback(cb) {}
    /*virtual*/ bool tick()
    {
        m_Callback();
        return true;
    }
protected:
    nullary_func_t m_Callback;
};

// NOTE: Unused as of SL-3.7.2
inline void rlvCallbackTimerOnce(F32 nPeriod, RlvCallbackTimerOnce::nullary_func_t cb)
{
    // Timer will automatically delete itself after the callback
    new RlvCallbackTimerOnce(nPeriod, cb);
}

// ============================================================================
// Various helper functions
//

ERlvAttachGroupType rlvAttachGroupFromIndex(S32 idxGroup);
ERlvAttachGroupType rlvAttachGroupFromString(const std::string& strGroup);

std::string rlvGetFirstParenthesisedText(const std::string& strText, std::string::size_type* pidxMatch = NULL);
std::string rlvGetLastParenthesisedText(const std::string& strText, std::string::size_type* pidxStart = NULL);

namespace Rlv
{
    void forceAtmosphericShadersIfAvailable();

    int           getObjectLinkNumber(const LLUUID& idObj);
    const LLUUID& getObjectRootId(const LLUUID& idObj);
}

// ============================================================================
// Inlined class member functions
//

inline void RlvBehaviourInfo::addModifier(ERlvLocalBhvrModifier eBhvrMod, const std::type_info& valueType, const std::string& strBhvrMod, modifier_handler_func_t fnHandler)
{
    RLV_ASSERT_DBG(m_BhvrModifiers.find(strBhvrMod) == m_BhvrModifiers.end());

    m_BhvrModifiers.insert(std::make_pair(strBhvrMod, std::make_tuple(eBhvrMod, std::type_index(valueType), fnHandler)));
}

inline ERlvLocalBhvrModifier RlvBehaviourInfo::lookupBehaviourModifier(const std::string& strBhvrMod) const
{
    auto itBhvrModifier = m_BhvrModifiers.find(strBhvrMod);
    return (m_BhvrModifiers.end() != itBhvrModifier) ? std::get<0>(itBhvrModifier->second) : ERlvLocalBhvrModifier::Unknown;
}

inline void RlvBehaviourInfo::toggleBehaviourFlag(EBehaviourFlags eBhvrFlag, bool fEnable)
{
    if (fEnable)
        m_nBhvrFlags |= eBhvrFlag;
    else
        m_nBhvrFlags &= ~eBhvrFlag;
}

// Checked: 2009-09-19 (RLVa-1.0.3d)
inline std::string RlvCommand::asString() const
{
    // NOTE: @clear=<param> should be represented as clear:<param>
    return (m_eParamType != RLV_TYPE_CLEAR)
        ? (!m_strOption.empty()) ? (std::string(getBehaviour())).append(":").append(m_strOption) : (std::string(getBehaviour()))
        : (!m_strParam.empty())  ? (std::string(getBehaviour())).append(":").append(m_strParam)  : (std::string(getBehaviour()));
}

inline bool RlvCommand::operator ==(const RlvCommand& rhs) const
{
    // The specification notes that "@detach=n" is semantically identical to "@detach=add" (same for "y" and "rem"
    return (getBehaviour() == rhs.getBehaviour()) && (m_strOption == rhs.m_strOption) &&
        ( (RLV_TYPE_UNKNOWN != m_eParamType) ? (m_eParamType == rhs.m_eParamType) : (m_strParam == rhs.m_strParam) );
}

template <typename T>
inline bool RlvObject::getModifierValue(ERlvLocalBhvrModifier eBhvrModifier, T& value) const
{
    auto itBhvrModifierValue = m_Modifiers.find(eBhvrModifier);
    if (m_Modifiers.end() != itBhvrModifierValue)
    {
        value = boost::get<T>(itBhvrModifierValue->second);
        return true;
    }
    return false;
}

// Checked: 2010-04-05 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
inline bool RlvForceWear::isWearableItem(const LLInventoryItem* pItem)
{
    LLAssetType::EType assetType = (pItem) ? pItem->getType() : LLAssetType::AT_NONE;
    return
        (LLAssetType::AT_BODYPART == assetType) || (LLAssetType::AT_CLOTHING == assetType) ||
        (LLAssetType::AT_OBJECT == assetType) || (LLAssetType::AT_GESTURE == assetType);
}

// ============================================================================

#endif // RLV_HELPER_H
