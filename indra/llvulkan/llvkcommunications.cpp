#include "llvkviewerui.h"
#include "llstring.h"
#include <algorithm>

std::vector<LLVKFloater*> LLVKViewerUi::communicationFloaters() const
{
    return {mConversationsFloater.get(),mPeopleFloater.get(),mResidentPicker.get()};
}

bool LLVKViewerUi::openCommunicationFloater(LLVKFloater& floater,const std::optional<LLVKWidgetTree::Rect>& rectangle,std::string& error)
{
    const bool opening=!floater.visible();
    if (!floater.open(error)) return false;
    return !opening || !rectangle || mTree.setShape(floater.id(),*rectangle,error);
}

bool LLVKViewerUi::showConversations(std::string& error)
{
    error.clear();
    if (!mConnectedView || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected)
    { error="Conversations requires a connected session"; return false; }
    if (!refreshCommunications(error) || !mCommunicationContext) return false;
    if (!mConversationsFloater || !openCommunicationFloater(*mConversationsFloater,mConversationsRectangle,error)) return false;
    mActiveFloater=mConversationsFloater.get();
    return true;
}

bool LLVKViewerUi::hideConversations(std::string& error)
{
    error.clear();
    return !mConversationsFloater || mConversationsFloater->close(error);
}

bool LLVKViewerUi::showContacts(const std::string& tab,std::string& error)
{
    const std::string panel=tab=="friends" ? "friends_panel" : tab=="groups" ? "groups_panel" :
        tab=="contact_sets" ? "contact_sets_panel" : "";
    if (panel.empty()) { error="Unknown Contacts tab"; return false; }
    return showConversations(error) &&
        mTree.selectTabPanel(find("im_box_tab_container",mCommunicationPanel),find("imcontacts",mCommunicationPanel),error) &&
        mTree.selectTabPanel(find("friends_and_groups",mCommunicationPanel),find(panel,mCommunicationPanel),error) &&
        mTree.requestControlFocus(find(panel,mCommunicationPanel),true,error);
}

bool LLVKViewerUi::showNearbyChat(std::string& error)
{
    return showConversations(error) &&
        mTree.selectTabPanel(find("im_box_tab_container",mCommunicationPanel),find("nearby_chat",mCommunicationPanel),error) &&
        mTree.requestControlFocus(find("local_composer",mCommunicationPanel),true,error);
}

bool LLVKViewerUi::showPeople(std::string& error,const std::string& tab)
{
    if (tab!="nearby_panel" && tab!="friends_panel" && tab!="groups_panel" && tab!="recent_panel" && tab!="contact_sets_panel")
    { error="Unsupported People tab"; return false; }
    if (!mConnectedView || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected)
    { error="People requires a connected session"; return false; }
    if (!refreshCommunications(error) || !mCommunicationContext) return false;
    if (!mPeopleFloater)
    {
        mPeopleFloater=LLVKFloater::createXml(mTree,*mDialogFactory,mRoot,R"(<floater name="floater_people" title="People" positioning="cascading" can_close="true" can_resize="true" height="570" min_height="200" min_width="365" width="400" layout="topleft" save_rect="true" save_visibility="true" reuse_instance="true">
<tab_container name="tabs" left="3" right="-5" top="0" bottom="-10" follows="all" tab_min_width="70" tab_height="20" tab_position="top" halign="center">
<panel name="nearby_panel" label="Nearby" follows="all" width="392" height="540">
<text name="nearby_unavailable" left="6" right="-6" top="8" height="80" follows="left|right|top" font="SansSerifSmall" word_wrap="true">Nearby People is unavailable: no native nearby-resident snapshot is connected.</text>
</panel>
<panel name="friends_panel" label="Friends" follows="all" width="392" height="540">
<text name="friends_unavailable" left="6" right="-6" top="8" height="80" follows="left|right|top" font="SansSerifSmall" word_wrap="true">Friends and presence are unavailable: no native friends service is connected.</text>
</panel>
<panel name="groups_panel" label="Groups" follows="all" width="392" height="540">
<combo_box name="group_list" left="6" right="-6" top="4" height="24" follows="left|right|top"/>
<button name="group_chat" label="Chat" left="6" bottom="-8" width="68" height="24" follows="left|bottom"/>
<text name="groups_unavailable" left="6" right="-6" top="36" height="80" follows="left|right|top" font="SansSerifSmall" word_wrap="true"/>
</panel>
<panel name="recent_panel" label="Recent" follows="all" width="392" height="540">
<text name="recent_unavailable" left="6" right="-6" top="8" height="80" follows="left|right|top" font="SansSerifSmall" word_wrap="true">Recent People is unavailable: no native recent-resident snapshot is connected.</text>
</panel>
<panel name="contact_sets_panel" label="Contact Sets" follows="all" width="392" height="540">
<text name="contact_sets_unavailable" left="6" right="-6" top="8" height="80" follows="left|right|top" font="SansSerifSmall" word_wrap="true">Contact Sets are unavailable: no native contact sets service is connected.</text>
</panel>
</tab_container></floater>)",error);
        if (!mPeopleFloater) return false;
        const auto page=mPeopleFloater->id();
        const auto tag=mCommunicationContext->tag;
        LLVKControl::Callback select;
        select.function=[this,tag,page](auto,const LLSD&)
        {
            if (mCommunicationContext && mCommunicationContext->tag==tag && mPeopleFloater && mPeopleFloater->id()==page)
                refreshPeople(mDialogError);
        };
        mTree.setControlCommit(find("group_list",page),std::move(select));
        LLVKControl::Callback chat;
        chat.function=[this,tag,page](auto,const LLSD&)
        {
            if (mCommunicationContext && mCommunicationContext->tag==tag && mPeopleFloater && mPeopleFloater->id()==page)
                showGroupConversation(LLUUID(mTree.value(find("group_list",page)).asString()),mDialogError);
        };
        mTree.setControlCommit(find("group_chat",page),std::move(chat));
        mPeopleFloater->onClose([this]
        {
            mPeopleRectangle=mTree.get(mPeopleFloater->id())->params.rect;
            if (mActiveFloater==mPeopleFloater.get()) mActiveFloater=nullptr;
        });
    }
    if (!refreshPeople(error) || !openCommunicationFloater(*mPeopleFloater,mPeopleRectangle,error)) return false;
    mActiveFloater=mPeopleFloater.get();
    const auto page=mPeopleFloater->id();
    return mTree.selectTabPanel(find("tabs",page),find(tab,page),error) && mTree.requestControlFocus(find(tab,page),true,error);
}

bool LLVKViewerUi::hidePeople(std::string& error)
{
    error.clear();
    return !mPeopleFloater || mPeopleFloater->close(error);
}

bool LLVKViewerUi::showResidentSearch(std::string& error)
{
    if (!mConnectedView || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected)
    { error="Resident search requires a connected session"; return false; }
    if (!refreshCommunications(error) || !mCommunicationContext || !mResidentPicker ||
        !openCommunicationFloater(*mResidentPicker,mResidentPickerRectangle,error)) return false;
    mActiveFloater=mResidentPicker.get();
    return mTree.requestControlFocus(find("recipient_id",mResidentPicker->id()),true,error);
}

bool LLVKViewerUi::refreshPeople(std::string& error)
{
    if (!mPeopleFloater) return true;
    const auto page=mPeopleFloater->id();
    const bool ready=mCommunicationContext && mSessionOwner && mSessionOwner->snapshot().state==LLVKSessionOwner::State::Connected &&
        mSessionOwner->snapshot().tag==mCommunicationContext->tag;
    const auto groups=ready && mCommunications.groups ? mCommunications.groups(mCommunicationContext->tag) : std::vector<LLVKChatProtocol::Group>{};
    const auto list=find("group_list",page);
    const auto selected=mTree.value(list);
    std::vector<LLVKWidgetTree::ComboItem> rows;
    for (const auto& group : groups) rows.push_back({group.name,LLSD(group.id.asString()),true});
    const auto& previous=mTree.get(list)->combo->items;
    if (rows.size()!=previous.size() || !std::equal(rows.begin(),rows.end(),previous.begin(),
        [](const auto& first,const auto& second) { return first.label==second.label && first.value.asString()==second.value.asString(); }))
        if (!mTree.replaceComboItems(list,std::move(rows),error) || !mTree.setComboValue(list,selected,error)) return false;
    const LLUUID id(mTree.value(list).asString());
    const auto found=std::find_if(groups.begin(),groups.end(),[&](const auto& group) { return group.id==id; });
    mTree.setEnabled(list,ready && !groups.empty());
    mTree.setEnabled(find("group_chat",page),found!=groups.end() && (found->powers&(std::uint64_t(1)<<16)) &&
        (found->state==LLVKChatProtocol::Group::State::Joined || bool(mCommunications.joinGroup)));
    mTree.setValue(find("groups_unavailable",page),LLSD(!ready ? "Not Connected" : !mCommunications.groups ?
        "Groups are unavailable: no native groups service is connected." : groups.empty() ? "No group memberships received." : ""));
    return true;
}

void LLVKViewerUi::setCommunicationApplicationFocused(bool focused)
{
    mCommunicationApplicationFocused=focused;
    if (!focused) stopCommunicationTyping();
}

bool LLVKViewerUi::communicationFocused(LLVKWidgetTree::Id page) const
{
    if (!page || !mCommunicationApplicationFocused || mNoticePanel || mLifecycleScreen ||
        !mConversationsFloater || !mConversationsFloater->visible() || mConversationsFloater->minimized()) return false;
    bool containsFocus=false;
    for (auto current=mTree.keyboardFocus(); mTree.get(current); current=mTree.get(current)->parent)
        if (current==page) { containsFocus=true; break; }
    if (!containsFocus) return false;
    for (auto current=page; mTree.get(current); current=mTree.get(current)->parent)
        if (!mTree.get(current)->params.visible || !mTree.get(current)->params.enabled) return false;
    return true;
}

bool LLVKViewerUi::initializeCommunications(std::string& error)
{
    if (mCommunicationPanel)
        return mTree.selectTabPanel(find("im_box_tab_container",mCommunicationPanel),find("nearby_chat",mCommunicationPanel),error);
    mConversationsFloater=LLVKFloater::createXml(mTree,*mDialogFactory,mRoot,R"(<floater name="native_communications" title="Conversations" width="396" height="390" layout="topleft" can_close="true" can_minimize="true" can_resize="true" min_width="396" min_height="300" reuse_instance="true" save_rect="true" save_visibility="true">
<text name="connection_status" visible="false" left="12" top="8" right="-130" height="22" font="SansSerif"/>
<button name="disconnect" visible="false" label="Disconnect" right="-12" top="6" width="106" height="24"/>
<tab_container name="im_box_tab_container" left="1" width="394" top="18" height="372" follows="all" tab_position="left" tab_width="115" tab_max_width="115" tab_height="20" halign="left">
<first_tab tab_bottom_image_unselected="Toolbar_Left_IM_Unselected"/>
<middle_tab tab_bottom_image_unselected="Toolbar_Middle_IM_Unselected"/>
<last_tab tab_bottom_image_unselected="Toolbar_Right_IM_Unselected"/>
<panel name="imcontacts" label="Contacts" width="396" height="390" follows="all">
<tab_container name="friends_and_groups" left="0" width="393" top="1" height="390" follows="all" tab_position="top" tab_width="90" tab_max_width="90" tab_height="20" halign="left">
<first_tab tab_bottom_image_flash="Toolbar_Left_Flash"/>
<middle_tab tab_bottom_image_flash="Toolbar_Middle_Flash"/>
<last_tab tab_bottom_image_flash="Toolbar_Right_Flash"/>
<panel name="friends_panel" label="Friends" width="393" height="370" follows="all">
<text name="friends_unavailable" left="4" right="-4" top="8" height="60" follows="left|right|top" font="SansSerifSmall" word_wrap="true">Friends and presence are unavailable: no native friends service is connected.</text>
</panel>
<panel name="groups_panel" label="Groups" width="393" height="370" follows="all">
<combo_box name="group_list" left="4" right="-4" top="4" height="24" follows="left|right|top"/>
<button name="group_join" label="Chat" left="4" top="36" width="68" height="24" follows="left|top"/>
<text name="groups_unavailable" left="4" right="-4" top="68" height="60" follows="left|right|top" font="SansSerifSmall" word_wrap="true"/>
</panel>
<panel name="contact_sets_panel" label="Contact Sets" width="393" height="370" follows="all">
<text name="contact_sets_unavailable" left="4" right="-4" top="8" height="60" follows="left|right|top" font="SansSerifSmall" word_wrap="true">Contact Sets are unavailable: no native contact sets service is connected.</text>
</panel>
</tab_container>
</panel>
<panel name="nearby_chat" label="Nearby Chat" width="394" height="390" follows="all">
<text name="local_title" left="4" top="0" height="24" right="-4" follows="left|right|top" font="SansSerif">Nearby Chat</text>
<text_editor name="local_transcript" left="4" right="-4" top="28" bottom="-42" follows="all" read_only="true" word_wrap="true" max_length="131072" parse_urls="false" font="SansSerifSmall"/>
<line_editor name="local_composer" left="4" right="-178" bottom="-8" height="24" follows="left|right|bottom" max_length_bytes="1023" commit_on_focus_lost="false"/>
<combo_box name="local_volume" right="-80" bottom="-8" width="90" height="24" follows="right|bottom"><combo_box.item label="Whisper" value="0"/><combo_box.item label="Say" value="1"/><combo_box.item label="Shout" value="2"/></combo_box>
<button name="local_send" label="Send" right="-4" bottom="-8" width="68" height="24" follows="right|bottom"/>
</panel>
</tab_container></floater>)",error);
    const auto panel=mConversationsFloater ? std::optional(mConversationsFloater->id()) : std::nullopt;
    if (!panel) return false;
    mCommunicationPanel=*panel;
    mConversationsFloater->onClose([this]
    {
        stopCommunicationTyping();
        mConversationsRectangle=mTree.get(mCommunicationPanel)->params.rect;
        if (mActiveFloater==mConversationsFloater.get()) mActiveFloater=nullptr;
    });
    mResidentPicker=LLVKFloater::createXml(mTree,*mDialogFactory,mRoot,R"(<floater name="avatarpicker" title="Choose Resident" positioning="cascading" legacy_header_height="18" can_close="true" can_resize="true" width="500" height="350" min_width="400" min_height="200" layout="topleft" save_rect="true">
<tab_container name="ResidentChooserTabs" left="0" width="500" top="20" height="300" follows="all" tab_position="top">
<panel name="SearchPanel" label="Search" width="500" height="300" follows="all">
<line_editor name="recipient_id" label="Resident name or UUID" left="10" right="-60" top="24" height="23" follows="left|right|top" max_length_bytes="255" commit_on_focus_lost="false"/>
<button name="open_conversation" label="Go" right="-10" top="24" width="45" height="23" follows="right|top"/>
<combo_box name="resident_results" left="10" right="-10" top="56" height="24" follows="left|right|top"/>
<text name="resident_status" left="10" right="-10" top="88" height="40" follows="left|right|top" font="SansSerifSmall" word_wrap="true"/>
</panel></tab_container>
<button name="resident_im" label="OK" right="-80" bottom="-4" width="68" height="24" follows="right|bottom"/>
<button name="resident_cancel" label="Cancel" right="-4" bottom="-4" width="68" height="24" follows="right|bottom"/>
</floater>)",error);
    if (!mResidentPicker)
    {
        mConversationsFloater.reset(); mCommunicationPanel=0;
        return false;
    }
    mResidentPicker->onClose([this]
    {
        ++mResidentQuery;
        mResidentPickerRectangle=mTree.get(mResidentPicker->id())->params.rect;
        if (mActiveFloater==mResidentPicker.get()) mActiveFloater=nullptr;
    });
    if (!mTree.setComboValue(find("local_volume",*panel),LLSD("1"),error)) return false;
    if (!mTree.selectTabPanel(find("im_box_tab_container",*panel),find("nearby_chat",*panel),error)) return false;
    const auto bind=[&](const char* name,auto handler)
    {
        LLVKControl::Callback callback;
        const auto tag=mSessionOwner->snapshot().tag;
        callback.function=[this,tag,host=*panel,handler=std::move(handler)](auto id,const LLSD& value)
        {
            if (mCommunicationPanel==host && mSessionOwner && mSessionOwner->snapshot().state==LLVKSessionOwner::State::Connected &&
                mSessionOwner->snapshot().tag==tag && mCommunicationContext && mCommunicationContext->tag==tag) handler(id,value);
        };
        const auto field=find(name,*panel);
        mTree.setControlCommit(field ? field : find(name,mResidentPicker->id()),std::move(callback));
    };
    bind("im_box_tab_container",[this](auto,const LLSD& page)
    {
        stopCommunicationTyping();
        for (const auto& [id,conversation] : mConversations)
            if (mTree.get(conversation.page) && mTree.get(conversation.page)->params.name==page.asString()) mSelectedConversation=id;
        for (const auto& [id,conversation] : mGroupConversations)
            if (mTree.get(conversation.page) && mTree.get(conversation.page)->params.name==page.asString()) mSelectedGroup=id;
    });
    bind("local_send",[this](auto,const LLSD&) { sendCommunication(true); });
    bind("local_composer",[this](auto,const LLSD&) { sendCommunication(true); });
    bind("group_list",[this](auto id,const LLSD&)
    {
        mSelectedGroup=LLUUID(mTree.value(id).asString());
        refreshCommunications(mDialogError);
    });
    bind("group_join",[this](auto,const LLSD&)
    {
        showGroupConversation(LLUUID(mTree.value(find("group_list",mCommunicationPanel)).asString()),mDialogError);
    });
    const auto open=[this](auto,const LLSD&)
    {
        if (!mCommunicationContext || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected ||
            mSessionOwner->snapshot().tag!=mCommunicationContext->tag || !mResidentPicker) return;
        const auto picker=mResidentPicker->id();
        const auto text=mTree.value(find("recipient_id",picker)).asString();
        LLUUID recipient;
        ++mResidentQuery;
        mResidentResults.clear();
        mTree.replaceComboItems(find("resident_results",picker),{},mDialogError);
        if (recipient.set(text,false) && recipient.notNull())
        {
            selectConversation(recipient,mDialogError);
        }
        else
        {
            const bool started=mCommunications.search && mCommunications.search(mCommunicationContext->tag,mResidentQuery,text,mDialogError);
            mTree.setValue(find("resident_status",picker),LLSD(started ? "Searching..." : "Resident search unavailable"));
        }
    };
    bind("open_conversation",open); bind("recipient_id",open);
    bind("resident_im",[this](auto,const LLSD&)
    {
        if (!mResidentPicker || !mCommunicationContext) return;
        const auto tag=mCommunicationContext->tag;
        const LLUUID selected(mTree.value(find("resident_results",mResidentPicker->id())).asString());
        const auto found=std::find_if(mResidentResults.begin(),mResidentResults.end(),[&](const auto& resident) { return resident.id==selected; });
        if (found==mResidentResults.end() || !mCommunicationContext) return;
        const auto resident=*found;
        if (!mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected || mSessionOwner->snapshot().tag!=tag) return;
        if (!mResidentPicker->close(mDialogError)) return;
        if (selectConversation(resident.id,mDialogError))
        {
            mConversations[resident.id].name=resident.displayName+" ("+resident.username+")";
            refreshCommunications(mDialogError);
        }
    });
    bind("resident_cancel",[this](auto,const LLSD&) { if (mResidentPicker) mResidentPicker->close(mDialogError); });
    bind("disconnect",[this](auto,const LLSD&)
    {
        if (mSessionOwner && mCommunicationContext && mSessionOwner->snapshot().tag==mCommunicationContext->tag)
        { mSessionOwner->cancel(mCommunicationContext->tag); refreshSession(mDialogError); }
    });
    return true;
}

bool LLVKViewerUi::clearCommunications(std::string& error)
{
    error.clear();
    stopCommunicationTyping();
    mCommunicationContext.reset(); mConversations.clear(); mSelectedConversation.setNull(); mLocalTranscript.clear();
    mInstantReceivedReported=false; mInstantDisplayedReported=false;
    ++mResidentQuery; mResidentResults.clear();
    mGroupConversations.clear(); mSelectedGroup.setNull();
    if (mActiveFloater==mPeopleFloater.get()) mActiveFloater=nullptr;
    mPeopleFloater.reset();
    if (mActiveFloater==mResidentPicker.get()) mActiveFloater=nullptr;
    mResidentPicker.reset();
    if (mActiveFloater==mConversationsFloater.get()) mActiveFloater=nullptr;
    mConversationsFloater.reset(); mCommunicationPanel=0;
    mConversationsRectangle.reset(); mPeopleRectangle.reset(); mResidentPickerRectangle.reset();
    return true;
}

bool LLVKViewerUi::selectConversation(const LLUUID& recipient,std::string& error)
{
    if (!refreshCommunications(error)) return false;
    if (!mCommunicationContext || recipient.isNull()) { error="No connected conversation owner"; return false; }
    if (!ensureConversation(recipient,false,error)) return false;
    if (recipient!=mSelectedConversation) stopCommunicationTyping();
    mSelectedConversation=recipient;
    const auto page=conversationPanel(recipient);
    if (!showConversations(error) || !mTree.selectTabPanel(find("im_box_tab_container",mCommunicationPanel),page,error) ||
        !mTree.requestControlFocus(find("im_composer",page),true,error)) return false;
    return refreshCommunications(error);
}

LLVKWidgetTree::Id LLVKViewerUi::conversationPanel(const LLUUID& id,bool group) const
{
    const auto& conversations=group ? mGroupConversations : mConversations;
    const auto found=conversations.find(id);
    return found==conversations.end() ? 0 : found->second.page;
}

bool LLVKViewerUi::showDirectConversation(const LLUUID& recipient,std::string& error)
{
    return refreshCommunications(error) && selectConversation(recipient,error);
}

bool LLVKViewerUi::showGroupConversation(const LLUUID& id,std::string& error)
{
    if (!refreshCommunications(error)) return false;
    if (!mCommunicationContext || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected ||
        mSessionOwner->snapshot().tag!=mCommunicationContext->tag || id.isNull() || !mCommunications.groups)
    { error="No connected group conversation owner"; return false; }
    const auto groups=mCommunications.groups(mCommunicationContext->tag);
    const auto selected=std::find_if(groups.begin(),groups.end(),[&](const auto& group) { return group.id==id; });
    if (selected==groups.end() || !(selected->powers&(std::uint64_t(1)<<16)))
    { error="Group chat membership or permission is unavailable"; return false; }
    if (!ensureConversation(id,true,error)) return false;
    if (selected->state!=LLVKChatProtocol::Group::State::Joined && selected->state!=LLVKChatProtocol::Group::State::Joining &&
        (!mCommunications.joinGroup || !mCommunications.joinGroup(mCommunicationContext->tag,id,error))) return false;
    mSelectedGroup=id;
    const auto page=conversationPanel(id,true);
    if (!showConversations(error) || !mTree.selectTabPanel(find("im_box_tab_container",mCommunicationPanel),page,error) ||
        !mTree.requestControlFocus(page,true,error)) return false;
    return refreshCommunications(error);
}

bool LLVKViewerUi::ensureConversation(const LLUUID& id,bool group,std::string& error)
{
    auto& conversations=group ? mGroupConversations : mConversations;
    if (id.isNull() || !mCommunicationContext) { error="No connected conversation owner"; return false; }
    if (const auto found=conversations.find(id); found!=conversations.end() && found->second.page) return true;
    if (!conversations.contains(id) && conversations.size()>=128) { error="Native conversation limit reached"; return false; }
    const auto container=find("im_box_tab_container",mCommunicationPanel);
    const std::string name=(group ? "session_group_" : "session_im_")+id.asString();
    const std::string content=group ? R"(
<text name="group_title" left="4" right="-4" top="0" height="24" follows="left|right|top" font="SansSerif"/>
<button name="group_leave" label="Leave Chat" left="4" top="28" width="100" height="24"/>
<text name="group_status" left="4" right="-4" top="56" height="40" follows="left|right|top" word_wrap="true" font="SansSerifSmall"/>
<combo_box name="group_participants" left="4" right="-4" top="100" height="24" follows="left|right|top"/>
<button name="group_mute" label="Mute Text" left="4" top="128" width="100" height="24"/>
<button name="group_unmute" label="Allow Text" left="108" top="128" width="100" height="24"/>
<text_editor name="group_transcript" left="4" right="-4" top="156" bottom="-42" follows="all" read_only="true" word_wrap="true" max_length="131072" parse_urls="false" font="SansSerifSmall"/>
<line_editor name="group_composer" left="4" right="-80" bottom="-8" height="24" follows="left|right|bottom" max_length_bytes="1023" commit_on_focus_lost="false"/>
<button name="group_send" label="Send" right="-4" bottom="-8" width="68" height="24" follows="right|bottom"/>)" : R"(
<text_editor name="im_transcript" left="4" right="-4" top="4" bottom="-66" follows="all" read_only="true" word_wrap="true" max_length="131072" parse_urls="false" font="SansSerifSmall"/>
<text name="typing_status" left="4" right="-4" bottom="-42" height="20" follows="left|right|bottom" font="SansSerifSmall"/>
<line_editor name="im_composer" left="4" right="-80" bottom="-8" height="24" follows="left|right|bottom" max_length_bytes="1023" commit_on_focus_lost="false"/>
<button name="im_send" label="Send" right="-4" bottom="-8" width="68" height="24" follows="right|bottom"/>)";
    const auto page=mDialogFactory->construct(mTree,"<panel name='"+name+"' width='280' height='390' follows='all' layout='topleft'>"+content+"</panel>",container,error);
    if (!page) return false;
    const auto& reference=mTree.get(container)->tabContainer->tabs.front();
    auto button=mTree.get(reference.button)->button->params;
    button.label=U"[SESSION]"; button.selectedLabel=button.label;
    auto control=mTree.get(reference.button)->control->params;
    control.commit={}; control.validate={};
    LLVKWidgetTree::Params view; view.name="vtab_"+name; view.rect={0,0,115,20};
    const auto tab=mTree.createButton(view,control,button,container,error);
    if (!tab || !mTree.attachTabPanel(container,*page,*tab,error))
    {
        std::string cleanup;
        if (tab) mTree.erase(*tab,cleanup);
        mTree.erase(*page,cleanup);
        return false;
    }
    auto& conversation=conversations[id];
    conversation.page=*page; conversation.tab=*tab;
    if (conversation.name.empty()) conversation.name=id.asString();
    const auto bind=[&](const char* field,auto handler)
    {
        LLVKControl::Callback callback; callback.function=std::move(handler);
        mTree.setControlCommit(find(field,*page),std::move(callback));
    };
    const auto tag=mCommunicationContext->tag;
    const auto current=[this,tag,id,group,page=*page]
    {
        return mCommunicationContext && mCommunicationContext->tag==tag && mSessionOwner &&
            mSessionOwner->snapshot().state==LLVKSessionOwner::State::Connected &&
            mSessionOwner->snapshot().tag==tag && conversationPanel(id,group)==page;
    };
    if (group)
    {
        const auto send=[this,current,id](auto,const LLSD&) { if (current()) sendGroupCommunication(id); };
        bind("group_send",send); bind("group_composer",send);
        bind("group_leave",[this,current,id](auto,const LLSD&)
        {
            if (current() && mCommunications.leaveGroup &&
                mCommunications.leaveGroup(mCommunicationContext->tag,id,mDialogError))
                refreshCommunications(mDialogError);
        });
        const auto moderate=[this,current,id,page=*page](bool muted)
        {
            if (!current() || !mCommunications.moderateGroup || !mCommunications.groups) return;
            const LLUUID participant(mTree.value(find("group_participants",page)).asString());
            const auto groups=mCommunications.groups(mCommunicationContext->tag);
            const auto found=std::find_if(groups.begin(),groups.end(),[&](const auto& group) { return group.id==id; });
            if (found==groups.end() || found->state!=LLVKChatProtocol::Group::State::Joined || found->moderationPending ||
                !found->participants.contains(participant)) return;
            const auto self=found->participants.find(mCommunicationContext->agent);
            if (self==found->participants.end() || !self->second.moderator) return;
            if (mCommunications.moderateGroup(mCommunicationContext->tag,id,participant,muted,mDialogError))
                refreshCommunications(mDialogError);
        };
        bind("group_mute",[moderate](auto,const LLSD&) { moderate(true); });
        bind("group_unmute",[moderate](auto,const LLSD&) { moderate(false); });
    }
    else
    {
        const auto send=[this,current,id](auto,const LLSD&) { if (current()) sendCommunication(false,id); };
        bind("im_send",send); bind("im_composer",send);
        LLVKControl::Callback typing;
        typing.function=[this,current,id](auto,const LLSD&) { if (current()) communicationKeystroke(id); };
        mTree.setLineEditorKeystroke(find("im_composer",*page),std::move(typing));
    }
    return mTree.layoutTabPanels(container,*mTree.get(container)->tabContainer->layout,error);
}

bool LLVKViewerUi::refreshCommunications(std::string& error)
{
    error.clear();
    if (!mConnectedView) return true;
    const auto snapshot=mSessionOwner ? mSessionOwner->snapshot() : LLVKSessionOwner::Snapshot{};
    auto context=snapshot.state==LLVKSessionOwner::State::Connected && mCommunications.context ? mCommunications.context(snapshot.tag) : std::nullopt;
    if (context && context->tag!=snapshot.tag) context.reset();
    if (mCommunicationContext && (!context || context->tag!=mCommunicationContext->tag || context->agent!=mCommunicationContext->agent))
        if (!clearCommunications(error)) return false;
    mCommunicationContext=context;
    if (!mCommunicationPanel && (!context || !initializeCommunications(error))) return !context;
    const bool ready=bool(context);
    const auto now=mTree.time();
    if (mTypingRecipient.notNull())
    {
        if (!ready || !communicationFocused(conversationPanel(mTypingRecipient)) || !mTree.setting("FSSendTypingState").value_or(LLSD(true)).asBoolean() ||
            mTree.keyboardFocus()!=find("im_composer",conversationPanel(mTypingRecipient)) || now-mTypingLastKey>5.) stopCommunicationTyping();
        else if (mTypingAnnounced && now-mTypingLastSent>4. && mCommunications.direct)
        {
            std::string problem;
            if (mCommunications.direct(context->tag,mTypingRecipient,"",true,false,problem)) mTypingLastSent=now;
        }
    }
    for (auto& [id,conversation] : mConversations)
        if (conversation.typing && now>conversation.typingUntil) conversation.typing=false;
    const auto groups=ready && mCommunications.groups ? mCommunications.groups(snapshot.tag) : std::vector<LLVKChatProtocol::Group>{};
    const auto selectedGroup=std::find_if(groups.begin(),groups.end(),[&](const auto& group) { return group.id==mSelectedGroup; });
    const bool groupSelected=selectedGroup!=groups.end();
    using GroupState=LLVKChatProtocol::Group::State;
    const bool groupJoined=groupSelected && selectedGroup->state==GroupState::Joined;
    const bool groupAllowed=groupSelected && (selectedGroup->powers&(std::uint64_t(1)<<16));
    mTree.setEnabled(find("group_list",mCommunicationPanel),ready && !groups.empty());
    mTree.setEnabled(find("group_join",mCommunicationPanel),groupAllowed && selectedGroup->state!=GroupState::Joining &&
        (groupJoined || bool(mCommunications.joinGroup)));
    mTree.setValue(find("groups_unavailable",mCommunicationPanel),LLSD(!ready ? "Not Connected" :
        !mCommunications.groups ? "Groups are unavailable: no native groups service is connected." :
        groups.empty() ? "No group memberships received." : ""));
    if (ready && mCommunications.searchResult)
        if (auto result=mCommunications.searchResult(snapshot.tag); result && result->query==mResidentQuery)
        {
            if (result->residents.size()>100) { error="Resident result limit exceeded"; return false; }
            mResidentResults=std::move(result->residents);
            std::vector<LLVKWidgetTree::ComboItem> rows;
            for (const auto& resident : mResidentResults)
                rows.push_back({resident.displayName+" ("+resident.username+")",LLSD(resident.id.asString()),true});
            if (!mTree.replaceComboItems(find("resident_results",mResidentPicker->id()),std::move(rows),error)) return false;
            if (!mResidentResults.empty() && !mTree.setComboValue(find("resident_results",mResidentPicker->id()),LLSD(mResidentResults.front().id.asString()),error)) return false;
            mTree.setValue(find("resident_status",mResidentPicker->id()),LLSD(!result->error.empty() ? result->error : mResidentResults.empty() ? "No residents found" : ""));
        }
    for (const auto name : {"resident_results","resident_im"})
        mTree.setEnabled(find(name,mResidentPicker->id()),ready && !mResidentResults.empty());
    for (const auto name : {"recipient_id","open_conversation"})
        mTree.setEnabled(find(name,mResidentPicker->id()),ready);
    for (const auto name : {"local_composer","local_send","local_volume"})
        mTree.setEnabled(find(name,mCommunicationPanel),ready && bool(mCommunications.local));
    mTree.setEnabled(find("disconnect",mCommunicationPanel),ready);
    const auto status=ready ? context->name+" | "+context->regionName+" | "+errorString("connected","Connected") : errorString("NotConnected","Not Connected");
    mTree.setValue(find("connection_status",mCommunicationPanel),LLSD(status));
    if (!mTree.prepareLayoutStacks(mCommunicationPanel,0,error)) return false;
    bool localChanged=false;
    const auto append=[](std::string& history,const std::string& line)
    {
        history+=line+"\n";
        while (history.size()>65536)
        {
            const auto end=history.find('\n');
            history.erase(0,end==history.npos ? history.size() : end+1);
        }
    };
    if (ready && mCommunications.receive)
        for (const auto& message : mCommunications.receive(snapshot.tag))
        {
            if (mMuteList.muted(message.sender,message.name,LLVKMuteList::Text,context->agent)) continue;
            if (message.kind==LLVKChatProtocol::Message::Kind::Local)
            {
                if (message.audible!=1 || (message.chatType!=0 && message.chatType!=1 && message.chatType!=2) ||
                    mMuteList.muted(message.owner,"",LLVKMuteList::Text,context->agent)) continue;
                const auto verb=message.chatType==0 ? " "+errorString("whisper","whispers:") :
                    message.chatType==2 ? " "+errorString("shout","shouts:") : ":";
                append(mLocalTranscript,message.name+verb+" "+message.text); localChanged=true;
            }
            else if ((message.dialog==17 || message.dialog==13) && message.sender!=context->agent &&
                std::any_of(groups.begin(),groups.end(),[&](const auto& group) { return group.id==message.conversation && (group.state==GroupState::Joined || message.dialog==13); }))
            {
                const auto& group=*std::find_if(groups.begin(),groups.end(),[&](const auto& entry) { return entry.id==message.conversation; });
                if (mTree.setting("FSMuteAllGroups").value_or(LLSD(false)).asBoolean() ||
                    (mTree.setting("FSMuteGroupWhenNoticesDisabled").value_or(LLSD(false)).asBoolean() && !group.acceptNotices) ||
                    mMuteList.muted(group.id,group.name,LLVKMuteList::Text,context->agent)) continue;
                if (message.dialog==13 && mCommunications.joinGroup)
                {
                    std::string problem;
                    mCommunications.joinGroup(context->tag,group.id,problem);
                }
                if (!ensureConversation(message.conversation,true,error)) return false;
                auto& conversation=mGroupConversations[message.conversation];
                append(conversation.transcript,message.name+": "+message.text);
                if (!communicationFocused(conversation.page)) ++conversation.unread;
            }
            else if (message.recipient==context->agent && !message.fromGroup &&
                (message.dialog==0 || message.dialog==41 || message.dialog==42))
            {
                if (message.sender.isNull() || (!mConversations.contains(message.sender) && mConversations.size()>=128)) continue;
                if (!ensureConversation(message.sender,false,error)) return false;
                auto& conversation=mConversations[message.sender];
                conversation.name=message.name.empty() ? message.sender.asString() : message.name;
                if (message.dialog==0)
                {
                    append(conversation.transcript,conversation.name+": "+message.text);
                    if (!mInstantReceivedReported && mCommunications.diagnostic)
                    { mCommunications.diagnostic("im-conversation-received"); mInstantReceivedReported=true; }
                    conversation.typing=false;
                    if (!communicationFocused(conversation.page)) ++conversation.unread;
                }
                else { conversation.typing=message.dialog==41; conversation.typingUntil=now+9.; }
            }
        }
    if (localChanged && !mTree.setTextEditorText(find("local_transcript",mCommunicationPanel),mLocalTranscript,error)) return false;
    const auto publish=[&](Conversation& conversation,bool group)->bool
    {
        const auto page=conversation.page;
        if (!page) return true;
        if (communicationFocused(page)) conversation.unread=0;
        conversation.draft=mTree.value(find(group ? "group_composer" : "im_composer",page)).asString();
        const auto transcript=find(group ? "group_transcript" : "im_transcript",page);
        if (mTree.value(transcript).asString()!=conversation.transcript &&
            !mTree.setTextEditorText(transcript,conversation.transcript,error)) return false;
        const auto label=utf8str_to_wstring(conversation.name+(conversation.unread ? " ("+std::to_string(conversation.unread)+")" : ""));
        if (!mTree.setButtonLabel(conversation.tab,std::u32string(label.begin(),label.end()))) return false;
        if (!group && mTree.visibleInChain(page) && mConversationsFloater && !mConversationsFloater->minimized() &&
            !conversation.transcript.empty() && !mInstantDisplayedReported && mCommunications.diagnostic)
        { mCommunications.diagnostic("im-transcript-displayed"); mInstantDisplayedReported=true; }
        return true;
    };
    for (auto& [id,conversation] : mConversations)
    {
        for (const auto name : {"im_composer","im_send"})
            mTree.setEnabled(find(name,conversation.page),ready && bool(mCommunications.direct));
        mTree.setValue(find("typing_status",conversation.page),LLSD(conversation.typing ? conversation.name+" is typing..." : ""));
        if (!publish(conversation,false)) return false;
    }
    for (auto& [id,conversation] : mGroupConversations)
    {
        const auto page=conversation.page;
        if (!page) continue;
        const auto found=std::find_if(groups.begin(),groups.end(),[&](const auto& group) { return group.id==id; });
        const bool present=ready && found!=groups.end();
        const bool joined=present && found->state==GroupState::Joined;
        bool muted=false,canModerate=false;
        std::vector<LLVKWidgetTree::ComboItem> rows;
        if (present)
        {
            conversation.name=found->name;
            const auto self=found->participants.find(context->agent);
            muted=self!=found->participants.end() && self->second.textMuted;
            canModerate=joined && self!=found->participants.end() && self->second.moderator &&
                !found->moderationPending && bool(mCommunications.moderateGroup);
            if (joined) for (const auto& [participant,flags] : found->participants)
            {
                const auto known=mConversations.find(participant);
                auto label=participant==context->agent ? context->name : known!=mConversations.end() ? known->second.name : participant.asString();
                if (flags.moderator) label+=" (Moderator)";
                if (flags.textMuted) label+=" (Text muted)";
                rows.push_back({std::move(label),LLSD(participant.asString()),true});
            }
        }
        const auto list=find("group_participants",page);
        const auto selected=mTree.value(list);
        const auto& old=mTree.get(list)->combo->items;
        if (rows.size()!=old.size() || !std::equal(rows.begin(),rows.end(),old.begin(),
            [](const auto& first,const auto& second) { return first.label==second.label && first.value.asString()==second.value.asString(); }))
            if (!mTree.replaceComboItems(list,std::move(rows),error) || !mTree.setComboValue(list,selected,error)) return false;
        mTree.setEnabled(list,joined && !found->participants.empty());
        const LLUUID participant(mTree.value(list).asString());
        canModerate=canModerate && participant.notNull() && found->participants.contains(participant);
        for (const auto name : {"group_mute","group_unmute"}) mTree.setEnabled(find(name,page),canModerate);
        for (const auto name : {"group_composer","group_send"})
            mTree.setEnabled(find(name,page),joined && (found->powers&(std::uint64_t(1)<<16)) && !muted && bool(mCommunications.group));
        mTree.setEnabled(find("group_leave",page),present && found->state!=GroupState::Closed && bool(mCommunications.leaveGroup));
        mTree.setValue(find("group_title",page),LLSD(conversation.name));
        mTree.setValue(find("group_status",page),LLSD(!present ? "Group membership unavailable" : !found->error.empty() ? found->error :
            muted ? "Text muted by moderator" : found->state==GroupState::Joining ? "Joining..." : joined ? "Joined" : "Not joined"));
        if (!publish(conversation,true)) return false;
    }
    std::vector<LLVKWidgetTree::ComboItem> groupItems;
    for (const auto& group : groups)
    {
        const auto found=mGroupConversations.find(group.id);
        const auto unread=found==mGroupConversations.end() ? 0 : found->second.unread;
        groupItems.push_back({group.name+(unread ? " ("+std::to_string(unread)+")" : ""),LLSD(group.id.asString()),true});
    }
    const auto groupList=find("group_list",mCommunicationPanel);
    const auto& oldGroups=mTree.get(groupList)->combo->items;
    if (groupItems.size()!=oldGroups.size() || !std::equal(groupItems.begin(),groupItems.end(),oldGroups.begin(),
        [](const auto& first,const auto& second) { return first.label==second.label && first.value.asString()==second.value.asString(); }))
        if (!mTree.replaceComboItems(groupList,std::move(groupItems),error)) return false;
    if (groupSelected && !mTree.setComboValue(groupList,LLSD(mSelectedGroup.asString()),error)) return false;
    return refreshPeople(error);
}

void LLVKViewerUi::stopCommunicationTyping()
{
    if (mTypingRecipient.notNull() && mCommunicationContext && mCommunications.direct && mSessionOwner &&
        mSessionOwner->snapshot().state==LLVKSessionOwner::State::Connected && mSessionOwner->snapshot().tag==mCommunicationContext->tag &&
        mTree.setting("FSSendTypingState").value_or(LLSD(true)).asBoolean())
    {
        std::string problem;
        mCommunications.direct(mCommunicationContext->tag,mTypingRecipient,"",false,true,problem);
    }
    mTypingRecipient.setNull(); mTypingAnnounced=false;
}

void LLVKViewerUi::communicationKeystroke(const LLUUID& recipient)
{
    const auto page=conversationPanel(recipient);
    if (!mCommunicationContext || recipient.isNull() || !page || !communicationFocused(page) || !mCommunications.direct ||
        !mTree.setting("FSSendTypingState").value_or(LLSD(true)).asBoolean()) return;
    if (mTree.value(find("im_composer",page)).asString().empty()) { stopCommunicationTyping(); return; }
    const auto now=mTree.time();
    if (mTypingRecipient!=recipient)
    {
        stopCommunicationTyping();
        mTypingRecipient=recipient; mTypingStarted=now;
    }
    mTypingLastKey=now;
    if (!mTypingAnnounced && now-mTypingStarted>1.)
    {
        std::string problem;
        if (mCommunications.direct(mCommunicationContext->tag,mTypingRecipient,"",true,false,problem))
        { mTypingAnnounced=true; mTypingLastSent=now; }
    }
}

void LLVKViewerUi::sendGroupCommunication(const LLUUID& id)
{
    if (!mCommunicationContext || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected ||
        mSessionOwner->snapshot().tag!=mCommunicationContext->tag || id.isNull() || !mCommunications.group || !mCommunications.groups) return;
    const auto page=conversationPanel(id,true);
    if (!page) return;
    const auto groups=mCommunications.groups(mCommunicationContext->tag);
    const auto found=std::find_if(groups.begin(),groups.end(),[&](const auto& group) { return group.id==id; });
    if (found==groups.end() || found->state!=LLVKChatProtocol::Group::State::Joined || !(found->powers&(std::uint64_t(1)<<16))) return;
    const auto self=found->participants.find(mCommunicationContext->agent);
    if (self!=found->participants.end() && self->second.textMuted) return;
    const auto editor=find("group_composer",page);
    const auto text=mTree.value(editor).asString();
    if (text.empty() || !mCommunications.group(mCommunicationContext->tag,id,text,mDialogError)) return;
    auto& history=mGroupConversations[id].transcript;
    history+=mCommunicationContext->name+": "+text+"\n";
    while (history.size()>65536) history.erase(0,history.find('\n')+1);
    mTree.setTextEditorText(find("group_transcript",page),history,mDialogError);
    mTree.setValue(editor,LLSD(""));
    mGroupConversations[id].draft.clear();
}

void LLVKViewerUi::sendCommunication(bool local,const LLUUID& recipient)
{
    if (!mCommunicationContext || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected ||
        mSessionOwner->snapshot().tag!=mCommunicationContext->tag) return;
    const auto page=local ? mCommunicationPanel : conversationPanel(recipient);
    if (!page) return;
    const auto editor=find(local ? "local_composer" : "im_composer",page);
    const auto text=mTree.value(editor).asString();
    if (text.empty()) return;
    bool sent=false;
    if (local && mCommunications.local)
    {
        const auto mode=mTree.value(find("local_volume",mCommunicationPanel)).asString();
        sent=mCommunications.local(mCommunicationContext->tag,text,mode=="0" ? 0 : mode=="2" ? 2 : 1,mDialogError);
    }
    else if (!local && recipient.notNull() && mCommunications.direct)
    {
        stopCommunicationTyping();
        sent=mCommunications.direct(mCommunicationContext->tag,recipient,text,false,false,mDialogError);
        if (sent)
        {
            auto& history=mConversations[recipient].transcript;
            history+=mCommunicationContext->name+": "+text+"\n";
            while (history.size()>65536) history.erase(0,history.find('\n')+1);
            mTree.setTextEditorText(find("im_transcript",page),history,mDialogError);
            mConversations[recipient].draft.clear();
        }
    }
    if (sent) mTree.setValue(editor,LLSD(""));
}
