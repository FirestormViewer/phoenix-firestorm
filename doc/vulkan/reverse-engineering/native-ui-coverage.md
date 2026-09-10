# Native UI investigation coverage ledger

Source revision: 3abd661f498329babaf87b49ffab910fcb5f0e6c; implementation checkpoint:
90af5a7220f1fec28e3c909c051b6ee7e062f10b. Branch: native-vulkan-ui. Historical GL
oracle remains 59108e15a1f8f94d2da7c674d937d19f5cf9450d; this ledger does not update it.

## Evidence and status

User amendment and clarification, 2026-09-10: existing GL-exclusive visual functions
cannot implement the native path, and the OpenGL implementation remains untouched.
Nonvisual functionality may be shared after audit; independently audited API-independent
third-party functionality is not categorically forbidden. Earlier suggestions to
reuse GL-dependent visual/CPU layout/font/image/mesh helpers or extract shared visual
services from GL are superseded. Source observations are not rewritten or granted closure.

This is an OPEN inventory, not an exhaustive trace or production UI implementation.
Discovery is separate from compilation, runtime reachability, local body inspection,
transitive closure, native implementation, runtime verification and measured parity.
No native implementation gate is closed by these tables. All UI helper targets and
registered callbacks need their NV-00 answers, including CPU-only responsibilities.

Reports containing local inspection records:

- [Parameters, fonts and atlases](native-ui-dependencies.md).
- [Image upload and state](native-ui-image-dependencies.md).
- [Font lifecycle and SVG](native-ui-font-lifecycle.md).
- [Frame traversal, coordinates and state](native-ui-frame-dependencies.md).
- [Vertex and shader submission](native-ui-submission-dependencies.md).
- [Construction, registries and parameter semantics](native-ui-construction-dependencies.md).
- [Viewer entry and pre-window UI lifecycle](native-ui-startup-dependencies.md).

Fresh RelWithDebInfo baseline build succeeded; recorded log:
logs/native-ui-baseline-build.log. The working generated build was subsequently
configured with LL_TESTS=ON. INTEGRATION_TEST_llheteromap built and ran successfully
(1/1); the new INTEGRATION_TEST_llinitparam reference probes built and ran
successfully (3/3), using MSVC/Windows SDK 10.0.26100, RelWithDebInfo, AVX2.
Test availability is a configuration responsibility, not a feature-scope limit.
Documentation validation checks link existence/line range, duplicate IDs and hygiene;
it does not prove source anchors identify exact functions or behavior is correct.
A subsequent isolated current-GL startup/capture exited0 and produced inspected login
controls plus a first-run notice. Device,inputs,hashes,artifacts and limitations are
recorded in [startup evidence](native-ui-startup-dependencies.md). This does not update
the historical oracle or establish native GPU validation or parity.
The isolated native startup diagnostic failed in pre-session default-font creation:
GL texture creation asserted with no GL context,then a fatal dialog prevented timed
quit until the external deadline terminated that test process. See the startup
report for exact evidence. Validation-layer discovery was not native session validation.

Generated Windows project reconciliation: every declaration source file in the
tables below has a ClCompile entry in llui.vcxproj or vulkanstorm-bin.vcxproj.
This was checked with MSBuild XML parsing and normalized absolute/relative paths.
It establishes project inclusion only, not conditional exclusion, preprocessor
reachability, object/link retention or runtime registration/execution.

## Registry declaration candidates

The following are individual source declarations found in indra/llui and
indra/newview, not a declaration that each is compiled or reachable. Conditional
guards, link retention, registrar scope, aliases, dynamic registrations and custom
builder targets require reconciliation with generated projects and runtime policy.
Each row remains OPEN for constructor, Params, initFromParams, postBuild, draw,
preparation side effects, input/callbacks, child registry and destructor closure.
Rows sharing a tag in different registries are intentionally separate.

Registry abbreviations: D=LLDefaultChildRegistry, M=MenuRegistry,
C=ContainerViewRegistry, S=ScrollContainerRegistry, V=StatViewRegistry,
L=LLLayoutStack::LayoutStackRegistry, P=PieChildRegistry.
Builder `default` means the declaration omits an explicit callback, not that its
transitive behavior has been inspected. These labels do not compress behavior
contracts: each class and overridden target still requires its own investigation.

### indra/llui

| Registry | Tag | Concrete type | Builder | Declaration |
|---|---|---|---|---|
| D | accordion | LLAccordionCtrl | default | [source](../../../indra/llui/llaccordionctrl.cpp#L48) |
| D | accordion_tab | LLAccordionCtrlTab | default | [source](../../../indra/llui/llaccordionctrltab.cpp#L49) |
| D | badge | LLBadge | default | [source](../../../indra/llui/llbadge.cpp#L36) |
| D | button | LLButton | default | [source](../../../indra/llui/llbutton.cpp#L59) |
| D | chat_editor | LLChatEntry | default | [source](../../../indra/llui/llchatentry.cpp#L32) |
| D | check_box | LLCheckBoxCtrl | default | [source](../../../indra/llui/llcheckboxctrl.cpp#L44) |
| D | combo_box | LLComboBox | default | [source](../../../indra/llui/llcombobox.cpp#L57) |
| D | icons_combo_box | LLIconsComboBox | default | [source](../../../indra/llui/llcombobox.cpp#L1414) |
| D | console | LLConsole | default | [source](../../../indra/llui/llconsole.cpp#L57) |
| D | container_view | LLContainerView | default | [source](../../../indra/llui/llcontainerview.cpp#L39) |
| C | stat_view | LLStatView | default | [source](../../../indra/llui/llcontainerview.cpp#L43) |
| C | panel | LLPanel | LLPanel::fromXML | [source](../../../indra/llui/llcontainerview.cpp#L44) |
| D | flat_list_view | LLFlatListView | default | [source](../../../indra/llui/llflatlistview.cpp#L34) |
| D | floater_view | LLFloaterView | default | [source](../../../indra/llui/llfloater.cpp#L2648) |
| D | folder_view_item | LLFolderViewItem | default | [source](../../../indra/llui/llfolderviewitem.cpp#L48) |
| D | icon | LLIconCtrl | default | [source](../../../indra/llui/lliconctrl.cpp#L42) |
| D | layout_stack | LLLayoutStack | default | [source](../../../indra/llui/lllayoutstack.cpp#L42) |
| L | layout_panel | LLLayoutPanel | default | [source](../../../indra/llui/lllayoutstack.cpp#L43) |
| D | line_editor | LLLineEditor | default | [source](../../../indra/llui/lllineeditor.cpp#L73) |
| D | menu_button | LLMenuButton | default | [source](../../../indra/llui/llmenubutton.cpp#L36) |
| M | menu_item | LLMenuItemGL | default | [source](../../../indra/llui/llmenugl.cpp#L112) |
| M | menu_item_separator | LLMenuItemSeparatorGL | default | [source](../../../indra/llui/llmenugl.cpp#L113) |
| M | menu_item_call | LLMenuItemCallGL | default | [source](../../../indra/llui/llmenugl.cpp#L114) |
| M | menu_item_check | LLMenuItemCheckGL | default | [source](../../../indra/llui/llmenugl.cpp#L115) |
| M | menu_item_tear_off | LLMenuItemTearOffGL | default | [source](../../../indra/llui/llmenugl.cpp#L117) |
| M | menu | LLMenuGL | default | [source](../../../indra/llui/llmenugl.cpp#L118) |
| D | menu | LLMenuGL | default | [source](../../../indra/llui/llmenugl.cpp#L120) |
| D | menu_bar | LLMenuBarGL | default | [source](../../../indra/llui/llmenugl.cpp#L3517) |
| D | context_menu | LLContextMenu | default | [source](../../../indra/llui/llmenugl.cpp#L4316) |
| M | context_menu | LLContextMenu | default | [source](../../../indra/llui/llmenugl.cpp#L4317) |
| D | multi_slider_bar | LLMultiSlider | default | [source](../../../indra/llui/llmultislider.cpp#L42) |
| D | multi_slider | LLMultiSliderCtrl | default | [source](../../../indra/llui/llmultisliderctrl.cpp#L46) |
| D | panel | LLPanel | LLPanel::fromXML | [source](../../../indra/llui/llpanel.cpp#L52) |
| D | progress_bar | LLProgressBar | default | [source](../../../indra/llui/llprogressbar.cpp#L43) |
| D | radio_group | LLRadioGroup | default | [source](../../../indra/llui/llradiogroup.cpp#L42) |
| D | scroll_bar | LLScrollbar | default | [source](../../../indra/llui/llscrollbar.cpp#L45) |
| D | scroll_container | LLScrollContainer | default | [source](../../../indra/llui/llscrollcontainer.cpp#L58) |
| S | scrolling_panel_list | LLScrollingPanelList | default | [source](../../../indra/llui/llscrollcontainer.cpp#L64) |
| S | container_view | LLContainerView | default | [source](../../../indra/llui/llscrollcontainer.cpp#L65) |
| S | panel | LLPanel | LLPanel::fromXML | [source](../../../indra/llui/llscrollcontainer.cpp#L66) |
| D | scrolling_panel_list | LLScrollingPanelList | default | [source](../../../indra/llui/llscrollingpanellist.cpp#L32) |
| D | scroll_list | LLScrollListCtrl | default | [source](../../../indra/llui/llscrolllistctrl.cpp#L67) |
| D | slider_bar | LLSlider | default | [source](../../../indra/llui/llslider.cpp#L39) |
| D | slider | LLSliderCtrl | default | [source](../../../indra/llui/llsliderctrl.cpp#L46) |
| D | spinner | LLSpinCtrl | default | [source](../../../indra/llui/llspinctrl.cpp#L49) |
| V | stat_bar | LLStatBar | default | [source](../../../indra/llui/llstatview.cpp#L61) |
| V | stat_view | LLStatView | default | [source](../../../indra/llui/llstatview.cpp#L62) |
| D | stat_view | LLStatView | default | [source](../../../indra/llui/llstatview.cpp#L64) |
| D | placeholder | LLPlaceHolderPanel | default | [source](../../../indra/llui/lltabcontainer.cpp#L193) |
| D | tab_container | LLTabContainer | default | [source](../../../indra/llui/lltabcontainer.cpp#L194) |
| D | text | LLTextBox | default | [source](../../../indra/llui/lltextbox.cpp#L40) |
| D | simple_text_editor | LLTextEditor | default | [source](../../../indra/llui/lltexteditor.cpp#L71) |
| D | time | LLTimeCtrl | default | [source](../../../indra/llui/lltimectrl.cpp#L42) |
| D | toggleable_menu | LLToggleableMenu | default | [source](../../../indra/llui/lltoggleablemenu.cpp#L33) |
| D | tooltip_view | LLToolTipView | default | [source](../../../indra/llui/lltooltip.cpp#L54) |
| D | tool_tip | LLToolTip | default | [source](../../../indra/llui/lltooltip.cpp#L139) |
| D | filter_editor | LLFilterEditor | default | [source](../../../indra/llui/llui.cpp#L80) |
| D | flyout_button | LLFlyoutButton | default | [source](../../../indra/llui/llui.cpp#L81) |
| D | search_editor | LLSearchEditor | default | [source](../../../indra/llui/llui.cpp#L82) |
| D | loading_indicator | LLLoadingIndicator | default | [source](../../../indra/llui/llui.cpp#L85) |
| D | toolbar | LLToolBar | default | [source](../../../indra/llui/llui.cpp#L86) |
| D | toolbar_vertical | LLToolBarVertical | default | [source](../../../indra/llui/llui.cpp#L87) |
| D | ui_ctrl | LLUICtrl | default | [source](../../../indra/llui/lluictrl.cpp#L40) |
| D | locate | LLUICtrlLocate | default | [source](../../../indra/llui/lluictrlfactory.cpp#L67) |
| D | view | LLView | default | [source](../../../indra/llui/llview.cpp#L90) |
| D | view_border | LLViewBorder | default | [source](../../../indra/llui/llviewborder.cpp#L33) |
| D | sun_moon_trackball | LLVirtualTrackball | default | [source](../../../indra/llui/llvirtualtrackball.cpp#L38) |
| D | window_shade | LLWindowShade | default | [source](../../../indra/llui/llwindowshade.cpp#L40) |
| D | xy_vector | LLXYVector | default | [source](../../../indra/llui/llxyvector.cpp#L43) |

Known comment-only search matches are not active declaration candidates:
[flyout button](../../../indra/llui/llflyoutbutton.cpp#L32),
[loading indicator](../../../indra/llui/llloadingindicator.cpp#L39),
[toolbar](../../../indra/llui/lltoolbar.cpp#L40), and the
[factory example](../../../indra/llui/lluictrlfactory.cpp#L142). The first three have
separate declaration sites in llui.cpp above; comments do not establish dormancy.

### indra/newview

| Registry | Tag | Concrete type | Builder | Declaration |
|---|---|---|---|---|
| D | fs_chat_history | FSChatHistory | default | [source](../../../indra/newview/fschathistory.cpp#L75) |
| D | fs_copytrans_inventory_drop_target | FSCopyTransInventoryDropTarget | default | [source](../../../indra/newview/fsdroptarget.cpp#L35) |
| D | fs_embedded_item_drop_target | FSEmbeddedItemDropTarget | default | [source](../../../indra/newview/fsdroptarget.cpp#L36) |
| D | fs_wearable_favorites_items_list | FSWearableFavoritesItemsList | default | [source](../../../indra/newview/fsfloaterwearablefavorites.cpp#L53) |
| D | fs_lsl_preproc_viewer | FSLSLPreProcViewer | default | [source](../../../indra/newview/fslslpreprocviewer.cpp#L31) |
| D | fs_nearby_chat_control | FSNearbyChatControl | default | [source](../../../indra/newview/fsnearbychatcontrol.cpp#L44) |
| D | fs_nearby_chat_voice_monitor | FSNearbyChatVoiceControl | default | [source](../../../indra/newview/fsnearbychatvoicemonitor.cpp#L32) |
| D | radar_list | FSRadarListCtrl | default | [source](../../../indra/newview/fsradarlistctrl.cpp#L34) |
| D | fs_scroll_list | FSScrollListCtrl | default | [source](../../../indra/newview/fsscrolllistctrl.cpp#L33) |
| D | fs_virtual_trackpad | FSVirtualTrackpad | default | [source](../../../indra/newview/fsvirtualtrackpad.cpp#L36) |
| D | avatar_icon | LLAvatarIconCtrl | default | [source](../../../indra/newview/llavatariconctrl.cpp#L50) |
| D | avatar_list | LLAvatarList | default | [source](../../../indra/newview/llavatarlist.cpp#L56) |
| D | block_list | LLBlockList | default | [source](../../../indra/newview/llblocklist.cpp#L36) |
| D | chat_history | LLChatHistory | default | [source](../../../indra/newview/llchathistory.cpp#L77) |
| D | text_chat | LLChatMsgBox | default | [source](../../../indra/newview/llchatmsgbox.cpp#L34) |
| D | chiclet_panel | LLChicletPanel | default | [source](../../../indra/newview/llchiclet.cpp#L65) |
| D | chiclet_notification | LLNotificationChiclet | default | [source](../../../indra/newview/llchiclet.cpp#L66) |
| D | chiclet_script | LLScriptChiclet | default | [source](../../../indra/newview/llchiclet.cpp#L67) |
| D | chiclet_offer | LLInvOfferChiclet | default | [source](../../../indra/newview/llchiclet.cpp#L68) |
| D | fs_chiclet_im_well | LLIMWellChiclet | default | [source](../../../indra/newview/llchiclet.cpp#L71) |
| D | fs_chiclet_im_p2p | LLIMP2PChiclet | default | [source](../../../indra/newview/llchiclet.cpp#L72) |
| D | fs_chiclet_im_group | LLIMGroupChiclet | default | [source](../../../indra/newview/llchiclet.cpp#L73) |
| D | fs_chiclet_im_adhoc | LLAdHocChiclet | default | [source](../../../indra/newview/llchiclet.cpp#L74) |
| D | color_swatch | LLColorSwatchCtrl | default | [source](../../../indra/newview/llcolorswatch.cpp#L46) |
| D | conversation_log_list | LLConversationLogList | default | [source](../../../indra/newview/llconversationloglist.cpp#L45) |
| D | conversation_view_session | LLConversationViewSession | default | [source](../../../indra/newview/llconversationview.cpp#L50) |
| D | conversation_view_participant | LLConversationViewParticipant | default | [source](../../../indra/newview/llconversationview.cpp#L603) |
| D | debug_view | LLDebugView | default | [source](../../../indra/newview/lldebugview.cpp#L51) |
| D | dnd_button | LLDragAndDropButton | default | [source](../../../indra/newview/lldndbutton.cpp#L32) |
| D | expandable_text | LLExpandableTextBox | default | [source](../../../indra/newview/llexpandabletextbox.cpp#L35) |
| D | favorites_bar | LLFavoritesBarCtrl | default | [source](../../../indra/newview/llfavoritesbar.cpp#L62) |
| D | panel_camera_item | LLPanelCameraItem | default | [source](../../../indra/newview/llfloatercamera.cpp#L53) |
| D | inventory_link_replace_drop_target | LLInventoryLinkReplaceDropTarget | default | [source](../../../indra/newview/llfloaterlinkreplace.cpp#L500) |
| D | profile_image | LLProfileImageCtrl | default | [source](../../../indra/newview/llfloaterprofiletexture.cpp#L45) |
| D | snapshot_floater_view | LLSnapshotFloaterView | default | [source](../../../indra/newview/llfloatersnapshot.cpp#L64) |
| D | overlap_panel | LLOverlapPanel | default | [source](../../../indra/newview/llfloateruipreview.cpp#L83) |
| D | group_icon | LLGroupIconCtrl | default | [source](../../../indra/newview/llgroupiconctrl.cpp#L34) |
| D | group_list | LLGroupList | default | [source](../../../indra/newview/llgrouplist.cpp#L53) |
| D | hint_popup | LLHintPopup | default | [source](../../../indra/newview/llhints.cpp#L146) |
| D | inventory_gallery_item | LLInventoryGalleryItem | default | [source](../../../indra/newview/llinventorygallery.cpp#L2796) |
| D | inventory_panel | LLInventoryPanel | default | [source](../../../indra/newview/llinventorypanel.cpp#L68) |
| D | recent_inventory_panel | LLInventoryRecentItemsPanel | default | [source](../../../indra/newview/llinventorypanel.cpp#L69) |
| D | favorites_inventory_panel | LLInventoryFavoritesItemsPanel | default | [source](../../../indra/newview/llinventorypanel.cpp#L70) |
| D | asset_filtered_inv_panel | LLAssetFilteredInventoryPanel | default | [source](../../../indra/newview/llinventorypanel.cpp#L71) |
| D | single_folder_inventory_panel | LLInventorySingleFolderPanel | default | [source](../../../indra/newview/llinventorypanel.cpp#L2909) |
| D | worn_inventory_panel | LLInventoryWornItemsPanel | default | [source](../../../indra/newview/llinventorypanel.cpp#L3275) |
| D | joystick_slide | LLJoystickAgentSlide | default | [source](../../../indra/newview/lljoystickbutton.cpp#L49) |
| D | joystick_turn | LLJoystickAgentTurn | default | [source](../../../indra/newview/lljoystickbutton.cpp#L50) |
| D | joystick_rotate | LLJoystickCameraRotate | default | [source](../../../indra/newview/lljoystickbutton.cpp#L51) |
| D | joystick_track | LLJoystickCameraTrack | default | [source](../../../indra/newview/lljoystickbutton.cpp#L52) |
| D | joystick_quat | LLJoystickQuaternion | default | [source](../../../indra/newview/lljoystickbutton.cpp#L53) |
| D | list_view | LLListView | default | [source](../../../indra/newview/lllistview.cpp#L36) |
| D | location_input | LLLocationInputCtrl | default | [source](../../../indra/newview/lllocationinputctrl.cpp#L188) |
| D | web_browser | LLMediaCtrl | default | [source](../../../indra/newview/llmediactrl.cpp#L74) |
| D | name_box | LLNameBox | default | [source](../../../indra/newview/llnamebox.cpp#L43) |
| D | name_editor | LLNameEditor | default | [source](../../../indra/newview/llnameeditor.cpp#L40) |
| D | name_list | LLNameListCtrl | default | [source](../../../indra/newview/llnamelistctrl.cpp#L48) |
| D | teleport_history_menu_item | LLTeleportHistoryMenuItem | default | [source](../../../indra/newview/llnavigationbar.cpp#L136) |
| D | pull_button | LLPullButton | default | [source](../../../indra/newview/llnavigationbar.cpp#L196) |
| D | net_map | LLNetMap | default | [source](../../../indra/newview/llnetmap.cpp#L97) |
| D | notification_list_view | LLNotificationListView | default | [source](../../../indra/newview/llnotificationlistview.cpp#L30) |
| D | outfit_gallery_item | LLOutfitGalleryItem | default | [source](../../../indra/newview/lloutfitgallery.cpp#L995) |
| D | output_monitor | LLOutputMonitorCtrl | default | [source](../../../indra/newview/lloutputmonitorctrl.cpp#L42) |
| D | nearby_voice_monitor | NearbyVoiceMonitor | default | [source](../../../indra/newview/lloutputmonitorctrl.cpp#L481) |
| D | profile_drop_target | LLProfileDropTarget | default | [source](../../../indra/newview/llpanelavatar.cpp#L59) |
| D | labeled_back_button | LLLabledBackButton | default | [source](../../../indra/newview/llpaneleditwearable.cpp#L422) |
| D | emoji_complete | LLPanelEmojiComplete | default | [source](../../../indra/newview/llpanelemojicomplete.cpp#L44) |
| D | settings_drop_target | LLSettingsDropTarget | default | [source](../../../indra/newview/llpanelenvironment.cpp#L132) |
| D | group_drop_target | LLGroupDropTarget | default | [source](../../../indra/newview/llpanelgroupnotices.cpp#L109) |
| D | inbox_inventory_panel | LLInboxInventoryPanel | default | [source](../../../indra/newview/llpanelmarketplaceinboxinventory.cpp#L54) |
| D | inbox_folder_view_folder | LLInboxFolderViewFolder | default | [source](../../../indra/newview/llpanelmarketplaceinboxinventory.cpp#L55) |
| D | inbox_folder_view_item | LLInboxFolderViewItem | default | [source](../../../indra/newview/llpanelmarketplaceinboxinventory.cpp#L56) |
| D | panel_inventory_object | LLPanelObjectInventory | default | [source](../../../indra/newview/llpanelobjectinventory.cpp#L1473) |
| D | places_inventory_panel | LLPlacesInventoryPanel | default | [source](../../../indra/newview/llplacesinventorypanel.cpp#L41) |
| D | script_editor | LLScriptEditor | default | [source](../../../indra/newview/llscripteditor.cpp#L41) |
| D | search_combo_box | LLSearchComboBox | default | [source](../../../indra/newview/llsearchcombobox.cpp#L34) |
| D | panel_container | LLSideTrayPanelContainer | default | [source](../../../indra/newview/llsidetraypanelcontainer.cpp#L30) |
| D | split_button | LLSplitButton | default | [source](../../../indra/newview/llsplitbutton.cpp#L43) |
| D | texture_picker | LLTextureCtrl | default | [source](../../../indra/newview/lltexturectrl.cpp#L1880) |
| D | thumbnail | LLThumbnailCtrl | default | [source](../../../indra/newview/llthumbnailctrl.cpp#L41) |
| D | toolbar_view | LLToolBarView | default | [source](../../../indra/newview/lltoolbarview.cpp#L54) |
| D | menu_holder | LLViewerMenuHolderGL | default | [source](../../../indra/newview/llviewermenu.cpp#L11456) |
| D | text_editor | LLViewerTextEditor | default | [source](../../../indra/newview/llviewertexteditor.cpp#L69) |
| D | wearable_items_list | LLWearableItemsList | default | [source](../../../indra/newview/llwearableitemslist.cpp#L856) |
| D | pie_menu | PieMenu | default | [source](../../../indra/newview/piemenu.cpp#L40) |
| P | pie_menu | PieMenu | default | [source](../../../indra/newview/piemenu.cpp#L43) |
| P | pie_slice | PieSlice | default | [source](../../../indra/newview/piemenu.cpp#L44) |
| P | pie_separator | PieSeparator | default | [source](../../../indra/newview/piemenu.cpp#L45) |

## Other root obligations

### Current investigation checkpoint

Completed local reads now include panel construction/background/input routing,
UI image nine-slice/plain/rotated geometry, solid/UI shader bodies, shader binding
and matrix synchronization, retained font geometry generation/replay, full font
render loop, glyph decoration, width measurement and advance/kerning bodies.
These are local-body records with outgoing obligations, NOT transitive closure.

The next nearby unresolved consumers are button/badge/scroll-list text reset and
matrix-scope callers; next unresolved helpers include parameter block/multiple
specializations, provider concrete implementations, glyph page generation setters,
FreeType/rounding contracts, shader loader/client-array/allocator functions and
settings/callback lifetimes. Panel-class injector and floater registration inventories
also remain unexpanded. Only test code/build registration and documentation have
changed on this branch; production renderer/UI behavior remains unchanged.

Checks actually performed: link/range/hygiene and unique record IDs; exact tag/type
declaration anchors; MSBuild source-file inclusion; exact54-entry nine-slice position
and UV table transcription against source. CPU runtime evidence additionally covers
the existing heterogeneous-cache test and the three parameter probes linked below.
No interactive native viewer, current-branch validation-layer run or measured
GL/native parity is implied.

### Unexpanded roots

These obligations are NOT resolved by the declaration tables and still need
individual symbol/edge expansion:

- LLPanel::fromXML custom class registry, callback-map lookup and factory fallback.
- Floater registries, typed build functors, dynamic menus/notifications and tooltips.
- Direct new/create<T>/child insertion paths, including controls without XUI tags.
- All concrete draw overrides and helper callbacks, not just tag-registered types.
- LLInitParam value specializations, parser registrations, settings and asset callbacks.
- Input/focus/capture, clipboard, IME, editing/undo, timers and asynchronous callbacks.
- CPU draw-time mutation/layout, text indexing, scroll state, invalidation and replay.
- Media surfaces, thumbnails/previews, HUD/world labels and auxiliary view scheduling.
- Process/static entry, backend selection, window creation and partial/normal teardown.
- Build/link/preprocessor/platform reconciliation and runtime constructor selection.
- Controlled GL reference startup/captures and tolerance qualification before parity.

The registration search is a discovery aid, not a substitute for the per-function
NV-00 contracts. No source-count or table-size percentage is a completion metric.