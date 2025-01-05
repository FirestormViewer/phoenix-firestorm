/**
 * @file llviewermenu.h
 * @brief Builds menus out of objects
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLVIEWERMENU_H
#define LL_LLVIEWERMENU_H

#include "llmenugl.h"
#include "llsafehandle.h"
#include "lltoggleablemenu.h" // <FS:Ansariel> FIRE-7893: Detach function on inspect self toast without function
#include "piemenu.h"

class LLMessageSystem;
class LLSD;
class LLUICtrl;
class LLView;
class LLParcelSelection;
class LLObjectSelection;
class LLSelectNode;
class LLViewerObject;

// [RLVa:KB] - Checked: RLVa-2.0.0
void set_use_wireframe(bool useWireframe);
// [/RLVa:KB]

void initialize_edit_menu();
void initialize_spellcheck_menu();
void initialize_volume_controls_callbacks(); //<FS:KC> Centralize a some of these volume panel callbacks
void init_menus();
void cleanup_menus();

void show_debug_menus(); // checks for if menus should be shown first.
void toggle_debug_menus();
void show_navbar_context_menu(LLView* ctrl, S32 x, S32 y);
void show_topinfobar_context_menu(LLView* ctrl, S32 x, S32 y);
void handle_reset_view();
void process_grant_godlike_powers(LLMessageSystem* msg, void**);

bool is_agent_mappable(const LLUUID& agent_id);

bool enable_god_full();
bool enable_god_liaison();
bool enable_god_basic();
void check_merchant_status(bool force = false);

void handle_object_touch();
bool enable_object_edit_gltf_material();
bool enable_object_open();
void handle_object_open();
void handle_object_tex_refresh(LLViewerObject* object, LLSelectNode* node);

bool visible_take_object();
bool tools_visible_take_object();
bool enable_object_take_copy();
bool enable_object_return();
bool enable_object_delete();

// Buy either contents or object itself
void handle_buy();
void handle_take(bool take_separate = false);
void handle_take_copy();
void handle_look_at_selection(const LLSD& param);
void handle_script_info();
// <FS:Ansariel> Option to try via exact position
//void handle_zoom_to_object(const LLUUID& object_id);
void handle_zoom_to_object(const LLUUID& object_id, const LLVector3d& object_pos = LLVector3d(-1.f, -1.f, -1.f));
// </FS:Ansariel> Option to try via exact position
void handle_object_return();
void handle_object_delete();
void handle_object_edit();
void handle_object_edit_gltf_material();

void handle_attachment_edit(const LLUUID& inv_item_id);
void handle_attachment_touch(const LLUUID& inv_item_id);
bool enable_attachment_touch(const LLUUID& inv_item_id);
void handle_selected_script_action(const std::string& action); // <FS> Script reset in edit floater

// <FS:Techwolf Lupindo> area search
// expose this function so other classes can call it
void handle_object_edit();
bool enable_bridge_function();
// <FS:Techwolf Lupindo>

void handle_buy_land();

// Takes avatar UUID, or if no UUID passed, uses last selected object
void handle_avatar_freeze(const LLSD& avatar_id);

// Takes avatar UUID, or if no UUID passed, uses last selected object
void handle_avatar_eject(const LLSD& avatar_id);

bool enable_freeze_eject(const LLSD& avatar_id);

// Can anyone take a free copy of the object?
// *TODO: Move to separate file
bool anyone_copy_selection(LLSelectNode* nodep);

// Is this selected object for sale?
// *TODO: Move to separate file
bool for_sale_selection(LLSelectNode* nodep);

void handle_toggle_flycam();

void handle_object_sit_or_stand();
void handle_object_sit(const LLUUID& object_id);
void handle_give_money_dialog();
bool enable_pay_object();
bool enable_buy_object();
bool handle_go_to();
bool update_grid_help();

// Convert strings to internal types
U32 render_type_from_string(std::string_view render_type);
U32 feature_from_string(std::string_view feature);
U64 info_display_from_string(std::string_view info_display);
// <FS:Techwolf Lupindo> export
bool enable_object_export();
// </FS:Techwolf Lupindo>

// <FS:Ansariel> Force HTTP features on SL
bool use_http_inventory();
bool use_http_textures();
// <FS:Ansariel>

// <FS:Ansariel> Avatar render more check for pie menu
bool check_avatar_render_mode(U32 mode);

// <FS:Ansariel> Texture refresh
void destroy_texture(const LLUUID& id);

class LLViewerMenuHolderGL : public LLMenuHolderGL
{
public:
    struct Params : public LLInitParam::Block<Params, LLMenuHolderGL::Params>
    {};

    LLViewerMenuHolderGL(const Params& p);

    virtual bool hideMenus();

    void setParcelSelection(LLSafeHandle<LLParcelSelection> selection);
    void setObjectSelection(LLSafeHandle<LLObjectSelection> selection);

    virtual const LLRect getMenuRect() const;

protected:
    LLSafeHandle<LLParcelSelection> mParcelSelection;
    LLSafeHandle<LLObjectSelection> mObjectSelection;
};

extern LLMenuBarGL*     gMenuBarView;
//extern LLView*            gMenuBarHolder;
extern LLMenuGL*        gEditMenu;
extern LLMenuGL*        gPopupMenuView;
extern LLViewerMenuHolderGL*    gMenuHolder;
extern LLMenuBarGL*     gLoginMenuBarView;

// Context menus in 3D scene
extern LLContextMenu        *gMenuAvatarSelf;
extern LLContextMenu        *gMenuAvatarOther;
extern LLContextMenu        *gMenuObject;
extern LLContextMenu        *gMenuAttachmentSelf;
extern LLContextMenu        *gMenuAttachmentOther;
extern LLContextMenu        *gMenuLand;
extern LLContextMenu        *gMenuMuteParticle;

// Needed to build menus when attachment site list available
extern LLMenuGL* gAttachSubMenu;
extern LLMenuGL* gDetachSubMenu;
extern LLMenuGL* gTakeOffClothes;
extern LLMenuGL* gDetachAvatarMenu;
extern LLMenuGL* gDetachHUDAvatarMenu;
extern LLContextMenu* gAttachScreenPieMenu;
extern LLContextMenu* gDetachScreenPieMenu;
extern LLContextMenu* gDetachHUDAttSelfMenu;
extern LLContextMenu* gAttachPieMenu;
extern LLContextMenu* gDetachPieMenu;
extern LLContextMenu* gDetachAttSelfMenu;
extern LLContextMenu* gAttachBodyPartPieMenus[9];
extern LLContextMenu* gDetachBodyPartPieMenus[9];

// <FS:Zi> Pie Menu
// Pie menus in 3D scene
extern PieMenu          *gPieMenuAvatarSelf;
extern PieMenu          *gPieMenuAvatarOther;
extern PieMenu          *gPieMenuObject;
extern PieMenu          *gPieMenuAttachmentSelf;
extern PieMenu          *gPieMenuAttachmentOther;
extern PieMenu          *gPieMenuLand;
extern PieMenu          *gPieMenuMuteParticle;

// Needed to build pie menus when attachment site list available
extern PieMenu* gPieAttachScreenMenu;
extern PieMenu* gPieDetachScreenMenu;
extern PieMenu* gPieAttachMenu;
extern PieMenu* gPieDetachMenu;
extern PieMenu* gPieAttachBodyPartMenus[PIE_MAX_SLICES];
extern PieMenu* gPieDetachBodyPartMenus[PIE_MAX_SLICES];
// <FS:Zi> Pie Menu

// <FS:Ansariel> FIRE-7893: Detach function on inspect self toast without function
extern LLToggleableMenu *gMenuInspectSelf;
extern LLContextMenu    *gInspectSelfDetachScreenMenu;
extern LLContextMenu    *gInspectSelfDetachMenu;
// </FS:Ansariel>

extern LLMenuItemCallGL* gAutorespondMenu;
extern LLMenuItemCallGL* gAutorespondNonFriendsMenu;

/*
// <FS:Zi> Dead code
extern LLMenuItemCallGL* gMutePieMenu;
extern LLMenuItemCallGL* gMuteObjectPieMenu;
extern LLMenuItemCallGL* gBuyPassPieMenu;
// </FS:Zi>
*/

#endif
