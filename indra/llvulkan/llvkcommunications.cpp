#include "llvkviewerui.h"
#include <algorithm>

bool LLVKViewerUi::initializeCommunications(std::string& error)
{
    if (mCommunicationPanel) return true;
    const auto panel=mDialogFactory->construct(mTree,R"(<panel name="native_communications" follows="all" width="1024" height="740" layout="topleft" background_visible="true" background_opaque="true" bg_opaque_color="DkGray">
<text name="connection_status" left="12" top="8" right="-130" height="22" follows="left|right|top" font="SansSerif"/>
<button name="disconnect" label="Disconnect" right="-12" top="6" width="106" height="24" follows="right|top"/>
<layout_stack name="communication_columns" left="8" right="-8" top="40" bottom="-8" follows="all" orientation="horizontal" animate="false" border_size="8">
<layout_panel name="local_column" width="490" height="692" min_width="220" auto_resize="true" user_resize="true">
<text name="local_title" left="4" top="0" height="24" right="-4" follows="left|right|top" font="SansSerif">Nearby chat</text>
<text_editor name="local_transcript" left="4" right="-4" top="28" bottom="-42" follows="all" read_only="true" word_wrap="true" max_length="131072" parse_urls="false" font="SansSerifSmall"/>
<line_editor name="local_composer" left="4" right="-178" bottom="-8" height="24" follows="left|right|bottom" max_length_bytes="1023" commit_on_focus_lost="false"/>
<combo_box name="local_volume" right="-80" bottom="-8" width="90" height="24" follows="right|bottom"><combo_box.item label="Whisper" value="0"/><combo_box.item label="Say" value="1"/><combo_box.item label="Shout" value="2"/></combo_box>
<button name="local_send" label="Send" right="-4" bottom="-8" width="68" height="24" follows="right|bottom"/>
</layout_panel>
<layout_panel name="im_column" width="490" height="692" min_width="250" auto_resize="true" user_resize="true">
<combo_box name="conversation_list" left="4" right="-4" top="0" height="24" follows="left|right|top"/>
<line_editor name="recipient_id" label="Resident name or UUID" left="4" right="-80" top="32" height="24" follows="left|right|top" max_length_bytes="255" commit_on_focus_lost="false"/>
<button name="open_conversation" label="Find" right="-4" top="32" width="68" height="24" follows="right|top"/>
<combo_box name="resident_results" left="4" right="-80" top="64" height="24" follows="left|right|top"/>
<button name="resident_im" label="IM" right="-4" top="64" width="68" height="24" follows="right|top"/>
<text name="resident_status" left="4" right="-4" top="94" height="20" follows="left|right|top" font="SansSerifSmall"/>
<text_editor name="im_transcript" left="4" right="-4" top="120" bottom="-66" follows="all" read_only="true" word_wrap="true" max_length="131072" parse_urls="false" font="SansSerifSmall"/>
<text name="typing_status" left="4" right="-4" bottom="-42" height="20" follows="left|right|bottom" font="SansSerifSmall"/>
<line_editor name="im_composer" left="4" right="-80" bottom="-8" height="24" follows="left|right|bottom" max_length_bytes="1023" commit_on_focus_lost="false"/>
<button name="im_send" label="Send" right="-4" bottom="-8" width="68" height="24" follows="right|bottom"/>
</layout_panel>
<layout_panel name="group_column" width="320" height="692" min_width="250" auto_resize="true" user_resize="true">
<combo_box name="group_list" left="4" right="-4" top="0" height="24" follows="left|right|top"/>
<button name="group_join" label="Join" left="4" top="32" width="68" height="24" follows="left|top"/>
<button name="group_leave" label="Leave" left="80" top="32" width="68" height="24" follows="left|top"/>
<text name="group_status" left="4" right="-4" top="64" height="40" follows="left|right|top" font="SansSerifSmall" word_wrap="true"/>
<combo_box name="group_participants" left="4" right="-4" top="108" height="24" follows="left|right|top"/>
<button name="group_mute" label="Mute Text" left="4" top="140" width="100" height="24" follows="left|top"/>
<button name="group_unmute" label="Allow Text" left="112" top="140" width="100" height="24" follows="left|top"/>
<text_editor name="group_transcript" left="4" right="-4" top="172" bottom="-42" follows="all" read_only="true" word_wrap="true" max_length="131072" parse_urls="false" font="SansSerifSmall"/>
<line_editor name="group_composer" left="4" right="-80" bottom="-8" height="24" follows="left|right|bottom" max_length_bytes="1023" commit_on_focus_lost="false"/>
<button name="group_send" label="Send" right="-4" bottom="-8" width="68" height="24" follows="right|bottom"/>
</layout_panel>
</layout_stack></panel>)",mRoot,error);
    if (!panel) return false;
    mCommunicationPanel=*panel;
    if (!mTree.setComboValue(find("local_volume",*panel),LLSD("1"),error)) return false;
    const auto bind=[&](const char* name,auto handler)
    {
        LLVKControl::Callback callback;
        callback.function=std::move(handler);
        mTree.setControlCommit(find(name,*panel),std::move(callback));
    };
    bind("local_send",[this](auto,const LLSD&) { sendCommunication(true); });
    bind("local_composer",[this](auto,const LLSD&) { sendCommunication(true); });
    bind("im_send",[this](auto,const LLSD&) { sendCommunication(false); });
    bind("im_composer",[this](auto,const LLSD&) { sendCommunication(false); });
    LLVKControl::Callback typing;
    typing.function=[this](auto,const LLSD&) { communicationKeystroke(); };
    mTree.setLineEditorKeystroke(find("im_composer",*panel),std::move(typing));
    bind("group_send",[this](auto,const LLSD&) { sendGroupCommunication(); });
    bind("group_composer",[this](auto,const LLSD&) { sendGroupCommunication(); });
    const auto moderate=[this](bool muted)
    {
        const LLUUID participant(mTree.value(find("group_participants",mCommunicationPanel)).asString());
        if (!mCommunicationContext || participant.isNull() || mSelectedGroup.isNull() || !mCommunications.moderateGroup) return;
        std::string problem;
        if (!mCommunications.moderateGroup(mCommunicationContext->tag,mSelectedGroup,participant,muted,problem))
        { mDialogError=std::move(problem); return; }
        refreshCommunications(mDialogError);
    };
    bind("group_mute",[moderate](auto,const LLSD&) { moderate(true); });
    bind("group_unmute",[moderate](auto,const LLSD&) { moderate(false); });
    bind("group_participants",[this](auto,const LLSD&) { refreshCommunications(mDialogError); });
    bind("group_list",[this](auto id,const LLSD&)
    {
        if (mSelectedGroup.notNull()) mGroupConversations[mSelectedGroup].draft=mTree.value(find("group_composer",mCommunicationPanel)).asString();
        mSelectedGroup=LLUUID(mTree.value(id).asString());
        mTree.replaceComboItems(find("group_participants",mCommunicationPanel),{},mDialogError);
        if (mSelectedGroup.isNull()) return;
        auto& conversation=mGroupConversations[mSelectedGroup]; conversation.unread=0;
        mTree.setValue(find("group_composer",mCommunicationPanel),LLSD(conversation.draft));
        mTree.setTextEditorText(find("group_transcript",mCommunicationPanel),conversation.transcript,mDialogError);
        refreshCommunications(mDialogError);
    });
    bind("group_join",[this](auto,const LLSD&)
    {
        if (mCommunicationContext && mSelectedGroup.notNull() && mCommunications.joinGroup)
            mCommunications.joinGroup(mCommunicationContext->tag,mSelectedGroup,mDialogError);
        refreshCommunications(mDialogError);
    });
    bind("group_leave",[this](auto,const LLSD&)
    {
        if (mCommunicationContext && mSelectedGroup.notNull() && mCommunications.leaveGroup)
            mCommunications.leaveGroup(mCommunicationContext->tag,mSelectedGroup,mDialogError);
        refreshCommunications(mDialogError);
    });
    const auto open=[this](auto,const LLSD&)
    {
        if (!mCommunicationContext) return;
        const auto text=mTree.value(find("recipient_id",mCommunicationPanel)).asString();
        LLUUID recipient;
        ++mResidentQuery;
        mResidentResults.clear();
        mTree.replaceComboItems(find("resident_results",mCommunicationPanel),{},mDialogError);
        if (recipient.set(text,false) && recipient.notNull()) selectConversation(recipient,mDialogError);
        else
        {
            const bool started=mCommunications.search && mCommunications.search(mCommunicationContext->tag,mResidentQuery,text,mDialogError);
            mTree.setValue(find("resident_status",mCommunicationPanel),LLSD(started ? "Searching..." : "Resident search unavailable"));
        }
    };
    bind("open_conversation",open); bind("recipient_id",open);
    bind("resident_im",[this](auto,const LLSD&)
    {
        const LLUUID selected(mTree.value(find("resident_results",mCommunicationPanel)).asString());
        const auto found=std::find_if(mResidentResults.begin(),mResidentResults.end(),[&](const auto& resident) { return resident.id==selected; });
        if (found==mResidentResults.end() || !mCommunicationContext) return;
        const auto resident=*found;
        if (selectConversation(resident.id,mDialogError))
        {
            mConversations[resident.id].name=resident.displayName+" ("+resident.username+")";
            refreshCommunications(mDialogError);
        }
    });
    bind("conversation_list",[this](auto id,const LLSD&)
    {
        const LLUUID recipient(mTree.value(id).asString());
        if (recipient.notNull()) selectConversation(recipient,mDialogError);
    });
    bind("disconnect",[this](auto,const LLSD&)
    {
        if (mSessionOwner && mCommunicationContext && mSessionOwner->snapshot().tag==mCommunicationContext->tag)
        { mSessionOwner->cancel(mCommunicationContext->tag); refreshSession(mDialogError); }
    });
    return true;
}

bool LLVKViewerUi::clearCommunications(std::string& error)
{
    stopCommunicationTyping();
    mCommunicationContext.reset(); mConversations.clear(); mSelectedConversation.setNull(); mLocalTranscript.clear();
    ++mResidentQuery; mResidentResults.clear();
    mGroupConversations.clear(); mSelectedGroup.setNull();
    if (!mCommunicationPanel) return true;
    for (const auto name : {"local_composer","im_composer","recipient_id","typing_status","connection_status","resident_status","group_composer","group_status"})
        mTree.setValue(find(name,mCommunicationPanel),LLSD(""));
    for (const auto name : {"local_transcript","im_transcript","group_transcript"})
        if (!mTree.setTextEditorText(find(name,mCommunicationPanel),"",error)) return false;
    return mTree.replaceComboItems(find("conversation_list",mCommunicationPanel),{},error) &&
        mTree.replaceComboItems(find("group_participants",mCommunicationPanel),{},error) &&
        mTree.replaceComboItems(find("group_list",mCommunicationPanel),{},error) &&
        mTree.replaceComboItems(find("resident_results",mCommunicationPanel),{},error);
}

bool LLVKViewerUi::selectConversation(const LLUUID& recipient,std::string& error)
{
    if (!mCommunicationContext || recipient.isNull()) { error="No connected conversation owner"; return false; }
    if (!mConversations.contains(recipient) && mConversations.size()>=128) { error="Native conversation limit reached"; return false; }
    if (recipient!=mSelectedConversation) stopCommunicationTyping();
    if (mSelectedConversation.notNull())
        mConversations[mSelectedConversation].draft=mTree.value(find("im_composer",mCommunicationPanel)).asString();
    auto& conversation=mConversations[recipient];
    if (conversation.name.empty()) conversation.name=recipient.asString();
    mSelectedConversation=recipient; conversation.unread=0;
    mTree.setValue(find("im_composer",mCommunicationPanel),LLSD(conversation.draft));
    if (!mTree.setTextEditorText(find("im_transcript",mCommunicationPanel),conversation.transcript,error)) return false;
    return refreshCommunications(error);
}

bool LLVKViewerUi::refreshCommunications(std::string& error)
{
    error.clear();
    if (!mCommunicationPanel || !mConnectedView) return true;
    const auto snapshot=mSessionOwner ? mSessionOwner->snapshot() : LLVKSessionOwner::Snapshot{};
    const auto context=snapshot.state==LLVKSessionOwner::State::Connected && mCommunications.context ? mCommunications.context(snapshot.tag) : std::nullopt;
    if (context && mCommunicationContext && context->tag!=mCommunicationContext->tag && !clearCommunications(error)) return false;
    mCommunicationContext=context;
    const bool ready=bool(context);
    const auto now=mTree.time();
    if (mTypingRecipient.notNull())
    {
        if (!ready || !mTree.setting("FSSendTypingState").value_or(LLSD(true)).asBoolean() ||
            mTree.keyboardFocus()!=find("im_composer",mCommunicationPanel) || now-mTypingLastKey>5.) stopCommunicationTyping();
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
    bool groupMuted=false;
    if (groupSelected)
    {
        const auto self=selectedGroup->participants.find(context->agent);
        groupMuted=self!=selectedGroup->participants.end() && self->second.textMuted;
    }
    const bool groupJoined=groupSelected && selectedGroup->state==GroupState::Joined;
    const bool groupAllowed=groupSelected && (selectedGroup->powers&(std::uint64_t(1)<<16));
    const auto participantList=find("group_participants",mCommunicationPanel);
    const auto selectedParticipant=mTree.value(participantList);
    std::vector<LLVKWidgetTree::ComboItem> participants;
    bool moderator=false;
    if (groupJoined)
    {
        const auto self=selectedGroup->participants.find(context->agent);
        moderator=self!=selectedGroup->participants.end() && self->second.moderator;
        for (const auto& [id,participant] : selectedGroup->participants)
        {
            const auto known=mConversations.find(id);
            auto label=id==context->agent ? context->name : known!=mConversations.end() ? known->second.name : id.asString();
            if (participant.moderator) label+=" (Moderator)";
            if (participant.textMuted) label+=" (Text muted)";
            participants.push_back({std::move(label),LLSD(id.asString()),true});
        }
    }
    const auto& previousParticipants=mTree.get(participantList)->combo->items;
    if (participants.size()!=previousParticipants.size() || !std::equal(participants.begin(),participants.end(),previousParticipants.begin(),
        [](const auto& first,const auto& second) { return first.label==second.label && first.value.asString()==second.value.asString(); }))
    {
        if (!mTree.replaceComboItems(participantList,std::move(participants),error) ||
            !mTree.setComboValue(participantList,selectedParticipant,error)) return false;
    }
    mTree.setEnabled(participantList,groupJoined && !selectedGroup->participants.empty());
    const LLUUID participantId(mTree.value(participantList).asString());
    const bool canModerate=groupJoined && moderator && !selectedGroup->moderationPending && participantId.notNull() && bool(mCommunications.moderateGroup);
    mTree.setEnabled(find("group_mute",mCommunicationPanel),canModerate);
    mTree.setEnabled(find("group_unmute",mCommunicationPanel),canModerate);
    mTree.setEnabled(find("group_list",mCommunicationPanel),ready && !groups.empty());
    mTree.setEnabled(find("group_join",mCommunicationPanel),groupAllowed && !groupJoined && selectedGroup->state!=GroupState::Joining && bool(mCommunications.joinGroup));
    mTree.setEnabled(find("group_leave",mCommunicationPanel),groupSelected && selectedGroup->state!=GroupState::Closed && bool(mCommunications.leaveGroup));
    for (const auto name : {"group_composer","group_send"})
        mTree.setEnabled(find(name,mCommunicationPanel),groupJoined && groupAllowed && !groupMuted && bool(mCommunications.group));
    const auto groupStatus=!groupSelected ? "" : !selectedGroup->error.empty() ? selectedGroup->error : groupMuted ? "Text muted by moderator" :
        selectedGroup->state==GroupState::Joining ? "Joining..." : groupJoined ? "Joined | "+std::to_string(selectedGroup->participants.size())+" participants" : "Not joined";
    mTree.setValue(find("group_status",mCommunicationPanel),LLSD(groupStatus));
    if (ready && mCommunications.searchResult)
        if (auto result=mCommunications.searchResult(snapshot.tag); result && result->query==mResidentQuery)
        {
            if (result->residents.size()>100) { error="Resident result limit exceeded"; return false; }
            mResidentResults=std::move(result->residents);
            std::vector<LLVKWidgetTree::ComboItem> rows;
            for (const auto& resident : mResidentResults)
                rows.push_back({resident.displayName+" ("+resident.username+")",LLSD(resident.id.asString()),true});
            if (!mTree.replaceComboItems(find("resident_results",mCommunicationPanel),std::move(rows),error)) return false;
            if (!mResidentResults.empty() && !mTree.setComboValue(find("resident_results",mCommunicationPanel),LLSD(mResidentResults.front().id.asString()),error)) return false;
            mTree.setValue(find("resident_status",mCommunicationPanel),LLSD(!result->error.empty() ? result->error : mResidentResults.empty() ? "No residents found" : ""));
        }
    for (const auto name : {"resident_results","resident_im"})
        mTree.setEnabled(find(name,mCommunicationPanel),ready && !mResidentResults.empty());
    for (const auto name : {"local_composer","local_send","local_volume"})
        mTree.setEnabled(find(name,mCommunicationPanel),ready && bool(mCommunications.local));
    for (const auto name : {"im_composer","im_send"})
        mTree.setEnabled(find(name,mCommunicationPanel),ready && mSelectedConversation.notNull() && bool(mCommunications.direct));
    for (const auto name : {"recipient_id","open_conversation","conversation_list","disconnect"})
        mTree.setEnabled(find(name,mCommunicationPanel),ready);
    const auto status=ready ? context->name+" | "+context->regionName+" | "+errorString("connected","Connected") : errorString("NotConnected","Not Connected");
    mTree.setValue(find("connection_status",mCommunicationPanel),LLSD(status));
    const auto root=mTree.get(mRoot)->params.rect;
    if (!mTree.setShape(mCommunicationPanel,{0,0,root.right-root.left,std::max(0,root.top-root.bottom-19)},error) ||
        !mTree.prepareLayoutStacks(mCommunicationPanel,0,error)) return false;
    bool localChanged=false,directChanged=false,groupChanged=false;
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
                auto& conversation=mGroupConversations[message.conversation];
                append(conversation.transcript,message.name+": "+message.text);
                if (mSelectedGroup==message.conversation) groupChanged=true;
                else ++conversation.unread;
            }
            else if (message.recipient==context->agent && !message.fromGroup &&
                (message.dialog==0 || message.dialog==41 || message.dialog==42))
            {
                if (message.sender.isNull() || (!mConversations.contains(message.sender) && mConversations.size()>=128)) continue;
                auto& conversation=mConversations[message.sender];
                conversation.name=message.name.empty() ? message.sender.asString() : message.name;
                if (message.dialog==0)
                {
                    append(conversation.transcript,conversation.name+": "+message.text);
                    conversation.typing=false;
                    if (mSelectedConversation!=message.sender) ++conversation.unread;
                }
                else { conversation.typing=message.dialog==41; conversation.typingUntil=now+9.; }
                if (mSelectedConversation==message.sender) directChanged=true;
            }
        }
    if (localChanged && !mTree.setTextEditorText(find("local_transcript",mCommunicationPanel),mLocalTranscript,error)) return false;
    if (groupChanged && !mTree.setTextEditorText(find("group_transcript",mCommunicationPanel),mGroupConversations[mSelectedGroup].transcript,error)) return false;
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
    if (directChanged && !mTree.setTextEditorText(find("im_transcript",mCommunicationPanel),mConversations[mSelectedConversation].transcript,error)) return false;
    std::vector<LLVKWidgetTree::ComboItem> items;
    for (const auto& [id,conversation] : mConversations)
        items.push_back({conversation.name+(conversation.unread ? " ("+std::to_string(conversation.unread)+")" : ""),LLSD(id.asString()),true});
    const auto list=find("conversation_list",mCommunicationPanel);
    const auto& previous=mTree.get(list)->combo->items;
    const bool changed=items.size()!=previous.size() || !std::equal(items.begin(),items.end(),previous.begin(),
        [](const auto& first,const auto& second) { return first.label==second.label && first.value.asString()==second.value.asString(); });
    if (changed && !mTree.replaceComboItems(list,std::move(items),error)) return false;
    if (mSelectedConversation.notNull() && !mTree.setComboValue(find("conversation_list",mCommunicationPanel),LLSD(mSelectedConversation.asString()),error)) return false;
    mTree.setValue(find("typing_status",mCommunicationPanel),LLSD(mSelectedConversation.notNull() && mConversations[mSelectedConversation].typing ?
        mConversations[mSelectedConversation].name+" is typing..." : ""));
    return true;
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

void LLVKViewerUi::communicationKeystroke()
{
    if (!mCommunicationContext || mSelectedConversation.isNull() || !mCommunications.direct ||
        !mTree.setting("FSSendTypingState").value_or(LLSD(true)).asBoolean()) return;
    if (mTree.value(find("im_composer",mCommunicationPanel)).asString().empty()) { stopCommunicationTyping(); return; }
    const auto now=mTree.time();
    if (mTypingRecipient!=mSelectedConversation)
    {
        stopCommunicationTyping();
        mTypingRecipient=mSelectedConversation; mTypingStarted=now;
    }
    mTypingLastKey=now;
    if (!mTypingAnnounced && now-mTypingStarted>1.)
    {
        std::string problem;
        if (mCommunications.direct(mCommunicationContext->tag,mTypingRecipient,"",true,false,problem))
        { mTypingAnnounced=true; mTypingLastSent=now; }
    }
}

void LLVKViewerUi::sendGroupCommunication()
{
    if (!mCommunicationContext || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected ||
        mSessionOwner->snapshot().tag!=mCommunicationContext->tag || mSelectedGroup.isNull() || !mCommunications.group) return;
    const auto editor=find("group_composer",mCommunicationPanel);
    const auto text=mTree.value(editor).asString();
    if (text.empty() || !mCommunications.group(mCommunicationContext->tag,mSelectedGroup,text,mDialogError)) return;
    auto& history=mGroupConversations[mSelectedGroup].transcript;
    history+=mCommunicationContext->name+": "+text+"\n";
    while (history.size()>65536) history.erase(0,history.find('\n')+1);
    mTree.setTextEditorText(find("group_transcript",mCommunicationPanel),history,mDialogError);
    mTree.setValue(editor,LLSD(""));
}

void LLVKViewerUi::sendCommunication(bool local)
{
    if (!mCommunicationContext || !mSessionOwner || mSessionOwner->snapshot().state!=LLVKSessionOwner::State::Connected ||
        mSessionOwner->snapshot().tag!=mCommunicationContext->tag) return;
    const auto editor=find(local ? "local_composer" : "im_composer",mCommunicationPanel);
    const auto text=mTree.value(editor).asString();
    if (text.empty()) return;
    bool sent=false;
    if (local && mCommunications.local)
    {
        const auto mode=mTree.value(find("local_volume",mCommunicationPanel)).asString();
        sent=mCommunications.local(mCommunicationContext->tag,text,mode=="0" ? 0 : mode=="2" ? 2 : 1,mDialogError);
    }
    else if (!local && mSelectedConversation.notNull() && mCommunications.direct)
    {
        stopCommunicationTyping();
        sent=mCommunications.direct(mCommunicationContext->tag,mSelectedConversation,text,false,false,mDialogError);
        if (sent)
        {
            auto& history=mConversations[mSelectedConversation].transcript;
            history+=mCommunicationContext->name+": "+text+"\n";
            while (history.size()>65536) history.erase(0,history.find('\n')+1);
            mTree.setTextEditorText(find("im_transcript",mCommunicationPanel),history,mDialogError);
        }
    }
    if (sent) mTree.setValue(editor,LLSD(""));
}