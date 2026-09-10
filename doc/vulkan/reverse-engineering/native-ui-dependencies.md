# Native UI dependency investigation

Status: **OPEN; production UI not implemented or qualified by this record.**
Branch: native-vulkan-ui. Source: 3abd661f498329babaf87b49ffab910fcb5f0e6c.
Implementation sources match PR #41 (90af5a7220); the later commit establishes
the governing [invariants](../native_vulkan_invariants.md) and
[roadmap](../native_viewer_roadmap.md). Reverted experiments are not implementation
evidence. Source anchors below refer to this baseline.

## Scope and accounting

The task is every dependency required for the viewer UI to render fully, not
only functions whose names contain GL or login-only controls. Construction,
factory registration, inherited Params, settings signals, font metrics, layout,
input-triggered changes, images/media, world-facing UI products, submission,
presentation and teardown are all within scope. No function is closed by being
assigned to a generic UI category. Every outgoing dependency must be read and
recorded or remain explicitly unresolved. Native design choices below are local
answers, not approved replacements for still-open transitive contracts.

Current concrete root: LLUICtrl::Params::Params. A complete UI root inventory is
not yet established. Therefore this document is not an exhaustive dependency
list, even for the font subsystem. No runtime/parity evidence has been collected
on this branch. No implementation edit is authorized by an open record alone.

Status definitions: body-inspected means the local body was read; edges-open means
listed callees, compiler-generated operations or indirect targets remain unresolved;
contract-closed requires evidence for local and transitive behavior. Implemented,
runtime-validated and parity-validated are separate statuses, none implied by a
contract record. Standard-library and third-party API contracts must be stated,
not replaced with invented internal behavior.

## UI-PARAM-001: LLUICtrl::Params::Params

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L70).
Status: body-inspected, edges-open. No platform preprocessor branch in this body.

**1. What is the OpenGL function doing?** Initializers declare tab_stop=true,
chrome=false, requests_front=false; label/value and init/commit/validate/mouse-enter/
mouse-leave callback fields; control_name; font from getFontEmojiDefault(); halign,
valign and ignored length/type. The body aliases initial_value to value. The font
getter is evaluated during construction, before the Params consumer has decided
whether it will draw. There is no local guard around registry availability or
allocation failure. Base LLView::Params, LLInitParam template constructors and
unnamed enabled/visibility fields are separate construction obligations.

**2. How is this done in Vulkan?** Authored/default values and callback declarations
are CPU data. Font identity, metrics readiness and GPU glyph availability must be
distinct. The native constructor cannot invoke this GL getter; native font requests
must resolve through a CPU owner and publish GPU data only through native resources.

**3. Cleanest native implementation?** Preserve provided/default/alias semantics in
native declarations without inheriting GL-owning parameter types. Resolve font
requests during explicit preparation. The declaration framework and callback
representation cannot be finalized until the base/template/factory obligations are
closed; a pointer substitution is not sufficient.

Check to disconfirm: constructor trace showing any additional font/image/global
mutation in inherited or implicit initializers invalidates a claim of CPU-only
construction. Native tests must distinguish unprovided defaults from authored
values and cannot declare success solely because construction avoids one getter.

Outgoing obligations: LLView::Params; LLInitParam::Block/Optional/Ignored and
addSynonym; specialized font parameter construction; enabled/visibility parameter
construction; getFontEmojiDefault; callers constructing default/derived Params;
static child registration and factory default lookup. All remain open except for
the explicitly body-inspected callees below.

## UI-FONT-001: LLFontGL::getFontEmojiDefault(bool)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L1192).
Status: body-inspected, edges-open.

**1.** Two function-static pointer initializers execute in source order: getFont
for (Emoji, Default, 0), then getFont for (EmojiBW, Default, 0). Both initialize on
the first call reaching them, regardless of useBW. The true branch returns the
BW pointer; false returns color. Returning null from an initializer still completes
that static initialization; throwing during initialization has different C++ retry
semantics. The getter has no invalidation mechanism for either cached pointer.

**2.** Native selection needs the same requested family/size/style meaning and
defined availability/errors. CPU font resolution is not GPU rendering. It must
not depend on LLFontGL static pointers or sFontRegistry.

**3.** Explicit native font-service ownership with generation-valid references is
preferable to process-static raw pointers. Whether to preserve eager resolution of
both families, and resulting failure timing, requires caller/service contracts;
lazy selected-only resolution must not be called equivalent without that decision.

Discriminating checks: first color request with missing BW family; first BW request
with missing color family; repeated lookup after registry reset/destroy/recreation;
first call before registry creation. No such runtime checks executed here.

Outgoing: LLFontDescriptor three-argument constructor; LLFontGL::getFont;
language-managed function-static initialization, exception retry and lifetime;
registry lifecycle and caller use of returned null/dangling values remain open.

## UI-FONT-002: LLFontGL::getFont(const LLFontDescriptor&)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L1294).
Status: body-inspected, edges-open.

**1.** Unconditionally dereferences sFontRegistry and calls its getFont. The static
pointer is initialized to null at file scope. No local readiness/null check exists.
**2.** Native resolution requires an explicitly initialized service; null global
state is not a Vulkan API concept. **3.** Service readiness must be an owner-stage
precondition with typed failure, not a wrapper delegating to this helper.
Check: invoke only after a trace proves initialization; test a native missing-service
request independently. Outgoing: registry getFont and every sFontRegistry writer.

## UI-FONT-003: LLFontRegistry::getFont(const LLFontDescriptor&)

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L747).
Status: body-inspected, edges-open.

**1.** Find the exact requested descriptor in mFontMap using its comparator. A hit
returns its pointer, including null, without attempting creation or ASCII warming.
A miss calls createFont. Null return logs failure; nonnull return calls
generateASCIIglyphs before returning. No local catch/rollback handles ASCII failure.
**2.** Native request caching and CPU glyph preparation must preserve identity,
negative-cache and publication semantics where defined. GPU glyph readiness is
separate from a nonnull CPU font. **3.** Explicit cache entries with lifecycle and
failure state are preferable to a raw-pointer map; warming policy is finalized only
after createFont/generateASCIIglyphs and consumer contracts are closed.
Checks: exact hit versus alias miss; cached-null versus uncached failure; exception
during ASCII warmup and subsequent retry. Outgoing: comparator, createFont,
generateASCIIglyphs, log/fatal policy, std::map lifetime and entry invalidators.

## UI-FONT-004: LLFontDescriptor constructors

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L81) and
[llfontregistry.h](../../../indra/llrender/llfontregistry.h#L80).
Status: constructor bodies inspected; standard member construction contract only.

**1.** Default initializes style=0; strings/vectors default construct and tabnum's
member initializer is false. Three-argument constructor copies name/size/style,
leaving tabnum=false. Four-argument bool overload also sets tabnum. Four-argument
file-vector overload copies files, leaving tabnum=false. Five-argument vector
overload delegates to the four-argument vector overload then assigns collection
files. Allocations/copies may throw; no local catch or application mutation exists.
**2.** CPU typed font descriptors can retain the same fields without GL ownership.
**3.** Prefer immutable semantic keys with explicit face style and tabular policy;
file lists belong to resolved catalog data unless consumer identity requires them.
Check: each overload's field values and subsequent comparator equivalence.
Outgoing: standard string/vector copy/allocation; LLFontFileInfo copy semantics.

## UI-FONT-005: LLFontDescriptor::operator<

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L126).
Status: local branches inspected; CPU operation.

**1.** Compare name ascending; unequal names return immediately. Then style
ascending with immediate return on inequality. Then size ascending; greater size
returns false. Finally true iff lhs tabnum is true and rhs false. File lists and
collection lists are excluded from ordering/equivalence. **2.** This is a CPU key
ordering, not a GPU operation. **3.** Native cache keys need explicit semantic
identity; preserving alias equivalence versus changing it requires a documented
cache contract, not indiscriminate inclusion of every descriptor field.
Check: identical name/style/size and differing file lists compare equivalent;
tabnum=true sorts before false. Outgoing: string lexicographical comparisons.

## UI-FONT-006: removeSubString and findSubString

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L156).
Status: local bodies inspected; CPU operations.

**1.** removeSubString finds the first case-sensitive occurrence anywhere in the
string. Found: erase exactly that occurrence and return true. Not found: leave
string unchanged and return false. findSubString returns found/not-found without
mutation despite its nonconst reference parameter. Neither checks token boundaries
or suffix position. **2.** CPU normalization helpers. **3.** Preserve this exact
matching behavior for compatible authored aliases unless correcting it explicitly.
Checks: interior occurrence, two occurrences, wrong case, absent substring.
Outgoing: standard string find/erase, not a GL function.

## UI-FONT-007: LLFontDescriptor::normalize

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L191).
Status: body-inspected, caller-dependent tabnum handling recorded below.

**1.** Copy name/size/style; mask style to BOLD|ITALIC. Independently remove first
Small, Big, Medium, Large, Huge, Default occurrences in that order, overwriting size
on each match (Big maps to Large). If size is not TEMPLATE and empty, test Monospace,
then Scripting, then Cascadia without removing those names; each later test still
requires empty size. Remaining empty size becomes Default. Remove first Bold and
Italic occurrences and OR corresponding style bits. Construct the result using
both file vectors; this overload leaves tabnum=false. createFont explicitly restores
tabnum; matching/template lookups do not. Comments saying default Medium are not
the active code contract.

**2.** Native alias normalization is CPU preparation. **3.** Separate the authored
request from normalized catalog lookup so tabnum and synthetic drawing decorations
are not accidentally lost. Exact compatibility or an intentional normalization
correction must be tested at each caller, not inferred from this helper alone.
Checks: names containing multiple size/style words, TEMPLATE, empty size, tabnum.
Outgoing: constructors, getFontFiles/getFontCollectionFiles accessors and both
substring helpers. Actual call sites remain to be individually reconciled.

## UI-FONT-008: LLFontRegistry::createFont

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L531).
Status: body-inspected, edges-open. This is NOT a closed loading contract.

**1. Local decisions in execution order:**

1. Normalize request, restore requested tabnum and look up point size. Missing
   size warns and returns null without reaching final map insertion.
2. Make TEMPLATE descriptor and find closest template. Missing match warns and
   returns null, again before final insertion.
3. Make nearest exact key with requested size/tabnum. Existing nonnull font creates
   a new LLFontGL wrapper, stores original descriptor, shares the existing FreeType
   owner, calls setTabnum on that shared owner, inserts under original key, returns.
   Existing null does not take this branch.
4. Copy matched file and collection lists. If a default template matches, append
   its respective lists. Append dynamic ultimate fallback filenames to file list
   with auto hinting, zero flags/delta and weight -1.
5. Empty file list warns and returns null. Collection list alone does not bypass
   this gate.
6. Construct search paths in order: local fonts, system fonts, user-settings fonts,
   executable fonts. Darwin adds /Library/Fonts/, its Supplemental directory and
   system Supplemental directory. Path-producing functions remain open.
7. For each file, collection membership is filename equality against collection
   list. fallback flag is !is_first_found OR !mCreateGLTextures. Scale is 1.0 in
   either branch because fallback_scale is fixed to 1.0 here.
8. For each search path, concatenate path+filename, allocate LLFontGL, query
   collection face count or use 1. For each face, allocate a wrapper if null and
   call loadFace with point size plus file delta, vertical/horizontal DPI, weight,
   fallback flag, face index, hinting, flags and original tabnum.
9. Successful first face becomes result and clears is_first_found. Subsequent
   success adds a fallback with its character predicate, deletes the wrapper and
   sets it null. Failed load deletes the wrapper and sets it null. The fallback
   flag was computed outside the face loop; do not assume it changes after face 0.
10. At least one successful face breaks the search-path loop. Entirely unloaded
    file logs once and deletes any remaining wrapper. Continue to subsequent files.
11. Nonnull result receives the original descriptor; null result warns. Insert
    original key with result (including null) and return. No local exception guard.

**2.** Native catalog resolution, path search, font-byte/face ownership, fallback
selection and tabular metrics are CPU responsibilities. Glyph cache initialization
or upload reached through loadFace/warmup requires separate native resource stages.

**3.** An explicit resolver should retain immutable resolved faces and separate
request style from physically provided style. It must not invoke LLFontGL wrappers
or mutate a shared face's tabular mode while published consumers use it. Exact
fallback ordering and failure/cache publication must be determined from all owners
before implementing that design. Multi-face first-file behavior is a specific open
question, not a silently corrected assumption.

Checks: missing size/template/files; null cache entry; closest-face sharing and
tabular mutation; first collection with multiple faces; first path fails then later
path succeeds; predicate fallback ordering; allocation/load/warmup failure cleanup.

Outgoing: nameToSize; getClosestFontTemplate; getMatchingFontDesc; ultimate fallback
producer; path helpers; LLFontGL construction/destruction/getNumFaces/loadFace;
LLFontFreetype addFallbackFont/setTabnum; intrusive references; all file/FreeType/
bitmap-cache paths; logging macros and failure policy. These remain unresolved
until individually recorded, regardless of the detail of this local trace.

## UI-FONT-009: LLFontGL::generateASCIIglyphs

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L597).
Status: body-inspected, edges-open.

**1.** For each integer from 32 through 126 inclusive, call mFontFreetype->getGlyphInfo
with Grayscale; ignore returned pointer. A profiling scope surrounds the loop.
There is no local face/null test or exception handling. **2.** CPU metric warmup
and GPU glyph warmup are distinct native jobs. **3.** Native requests should list
the warming set explicitly and publish only successfully prepared data; do not
silently drop eager failure/availability behavior without a consumer decision.
Check: exactly 95 requests, order preserved, mixed hits/misses, early failed glyph.
Outgoing: getGlyphInfo, FreeType owner reference, profiling instrumentation.

## UI-FONT-010: LLFontGL::loadFace

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L95).
Status: body-inspected, edges-open.

**1.** If mFontFreetype compares equal to null, allocate a new LLFontFreetype and
assign its intrusive pointer. Then forward filename, point size, vertical/horizontal
DPI, weight, fallback, face index, hinting, flags and tabnum to loadFace; return its
bool. Existing owner is reused. **2.** Native face creation is a CPU service action,
but this target's loading calls into a GL atlas. **3.** Native ownership must split
those responsibilities instead of forwarding to the GL owner. Check: absent versus
existing owner, failed reload, outstanding shared owner references. Outgoing:
LLFontFreetype constructor/loadFace, LLPointer assignment/refcount/destruction.

## UI-FONT-011: LLFontGL::getNumFaces

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L108).
Status: body-inspected, edges-open.

**1.** Allocate the FreeType owner only when absent, then forward filename and
return its face count. The called implementation destroys any already-open face.
**2.** Native collection inspection should be CPU metadata work. **3.** Use a
separately owned temporary inspection face, not mutation of a published face;
this is a design choice requiring lifetime checks. Check: inspect with existing
face and retained font consumers. Outgoing: owner constructor/getNumFaces/refcount.

## UI-FONT-012: LLFontFreetype constructor

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L145).
Status: body-inspected, member initialization obligations open.

**1.** Allocate LLFontBitmapCache; set ascender/descender/lineHeight=0, fallback=false,
hinting=FORCE_AUTOHINT, face=null, render count=0, style=0, point size=0 and maximum
digit width=0. Body empty. Header default initializers and implicit members remain
part of the audit, not inferred from this initializer list. **2.** CPU owner and
native atlas owner can initialize independently. **3.** Defer GPU allocation to a
resource-ready phase rather than couple it to face construction. Check: constructor
allocation trace, failure unwind of partially constructed members. Outgoing: cache
constructor, base/member constructors and native allocation failure policy.

## UI-FONT-013: LLFontFreetype::loadFace

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L178).
Status: body-inspected, edges-open.

**1. Exact local path:**

1. Existing mFTFace is destroyed with FT_Done_Face and set null before loading.
2. Zero FT_Open_Args, obtain memory pointer/length from gFontManagerp->loadFont.
   Missing bytes return false. No manager-null guard exists.
3. Set FT_OPEN_MEMORY; FT_Open_Face uses index **0**, not the supplied face_n.
   Nonzero FT error returns false. The parameter cannot be assumed honored.
4. Store fallback, hinting, flags, weight and tabnum. If weight >=0, set wght axis,
   retain its success bool, then attempt opsz=point_size without testing its return.
5. Compute pixels_per_em=(point_size/72)*verticalDPI. FT_Set_Char_Size uses width=0,
   truncated S32(point_size*64), U32(horizontalDPI), U32(verticalDPI). Failure destroys
   face, nulls it and returns false.
6. Compute units-per-EM reciprocal and scale face bbox, ascender, negative descender
   and line height. Maximum character dimensions use ll_round(0.5+scaled bbox span).
   There is no local check for zero units-per-EM or invalid metric inputs.
7. Initialize bitmap cache with maximum dimensions, which resets previous images.
8. If face charmap absent, call FT_Set_Charmap(face, face->charmaps[0]); no local
   num_charmaps check and no return-value test.
9. If not fallback, add grayscale character/glyph zero. No result check. This path
   allocates/binds/uploads GL atlas storage through the insertion/cache functions.
10. Save filename/point size, set style NORMAL. Bold is set if actual face bold;
    else declared bold flag; else weight>=600 and successful variable wght setting.
    Italic is set only if actual face italic flag. Return true.

**2.** File bytes, collection metadata, variable axes and metrics are CPU work;
default-glyph generation and GPU availability are independently scheduled native
work. **3.** Immutable per-generation face configuration plus owned font bytes and
explicit glyph publication removes global-manager and GL-at-construction coupling.
Correctly selecting nonzero collection indices would differ from this implementation;
record it as an explicit correction, not assumed parity. Likewise derive metrics
from the source formula or prove an alternative rather than substitute hinted face
metrics silently.
Checks: supplied nonzero face index; absent manager/file/charmap; invalid size;
wght/opsz supported/unsupported; declared bold versus actual/variable bold; fallback
true versus false; replacement while prior users retain the font.
Outgoing: font manager loadFont, FT APIs and buffer lifetime, setVariationAxis,
ll_round, bitmap init, addGlyphFromFont, face/global lifetime. Not closed.

## UI-FONT-014: LLFontFreetype::getNumFaces

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L293).
Status: body-inspected, edges-open.

**1.** Destroy existing face and null it; ask global manager for bytes. Missing bytes
returns 0. Open memory face index 0; failure returns 0. On success read num_faces,
destroy/null face, return count. **2.** CPU collection inspection. **3.** Temporary
inspection state with explicit byte lease; do not invalidate a published font.
Check: missing file, rejected font, collection count, already-open face.
Outgoing: manager byte loading, FT_Open_Face/FT_Done_Face and lifetime contracts.

## UI-FONT-015: LLFontFreetype::addFallbackFont and setTabnum

Source: [addFallbackFont](../../../indra/llrender/llfontfreetype.cpp#L322),
[setTabnum](../../../indra/llrender/llfontfreetype.h#L159).
Status: individual local bodies inspected, owner/subscriber edges open.

**1. addFallbackFont:** emplace (intrusive face reference, character predicate)
into fallback vector, preserving insertion order. Allocation/predicate copy may
throw. **1. setTabnum:** assign bool only; it does not clear cached digit widths,
invalidate prepared text or emit a signal. **2.** Both are CPU configuration duties.
**3.** Native immutable fallback order and request tabular policy should be captured
in a font generation rather than mutate shared state. Check ordered predicate
evaluation and toggling tabnum after layouts were prepared. Outgoing: LLPointer
ownership and each predicate body/registered settings subscriber.

## UI-FONT-016: LLFontFreetype::getGlyphInfo

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L702).
Status: body-inspected, edges-open; const does not imply read-only behavior.

**1.** Get equal-range of character entries. Explicit glyph type searches that
range for matching mGlyphType; Unspecified chooses its first entry. Hit returns
pointer. Miss calls addGlyph with explicit type or Grayscale for Unspecified.
**2.** Native lookup can return prepared metrics/bitmap state, but missing data
must have an explicit preparation request. **3.** Distinguish pure lookup from
mutable preparation; published draw records cannot trigger font loading/upload.
Check two cached representations, insertion order for Unspecified, miss during
input measurement and during draw. Outgoing: map comparator/container behavior,
addGlyph and every caller relying on its side effects.

## UI-FONT-017: LLFontFreetype::addGlyph

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L465).
Status: body-inspected, edges-open.

**1.** Null head face returns null. Assert head is not fallback and type<Count.
Obtain primary character index. If zero:

1. If LLStringOps::isEmoji is true, iterate fallback vector in order. Reject missing
   predicates or predicates returning false. First nonzero fallback glyph index
   immediately calls addGlyphFromFont and returns its result.
2. Iterate fallback vector again: record indices with predicates for later, skipping
   them now. First predicate-free font with a glyph immediately inserts/returns.
3. Iterate recorded predicate-bearing fonts in order, **without invoking predicates**;
   first nonzero glyph inserts/returns. This can choose a font whose predicate
   rejected the character earlier.

After primary hit or all fallback misses, search head cache's character equal-range
for requested representation. Absent calls addGlyphFromFont(this, character,
glyph_index, type), possibly index zero. Present returns null (getGlyphInfo normally
handles that hit first). No recursive fallback search through a fallback's own list.

**2.** Native fallback is CPU policy with classifier/settings inputs explicitly
versioned. **3.** Preserve ordered three-pass semantics or record an approved
correction; do not flatten predicates into a generic 'supported codepoint' search.
Checks: primary glyph wins even for emoji; predicate false but third-pass glyph
exists; no glyph; duplicate representation; invalid/fallback head.
Outgoing: isEmoji; every fallback predicate; FT_Get_Char_Index; addGlyphFromFont;
assert/fatal configuration; vector/map and face lifetime.

## UI-FONT-018: LLFontFreetype::renderGlyph

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L736).
Status: body-inspected, external renderer hooks/fatal policy open.

**1.** Null face returns without incrementing counter. Start load flags from the
hinting enum's numeric value; OR FT_LOAD_COLOR only for Color request. Load glyph.
On failure: out-of-memory first invokes user warning and fatal logger; continuation
if any is governed by fatal policy. Construct diagnostic, warn once, retry with
FT_LOAD_COLOR **XOR toggled** (it can be enabled or disabled). Invalid outline,
invalid composite, or any retry error for classified emoji loads glyph zero with
forced auto hinting; failure there logs fatal. Any remaining nonzero error then
tries '?' glyph with toggled color flag. Assert-always final load success using
the original diagnostic. Finally assert-always successful FT_Render_Glyph with
global gFontRenderMode and increment mRenderGlyphCount.

**2.** Native CPU rasterization must declare requested versus actual representation,
fallback/error result and byte ownership. **3.** A result-bearing raster operation
with explicit retry policy is preferable to void mutation plus fatal global state;
error-policy changes need classification under NV-18. No exception may escape a
C callback into FreeType. Check each retry branch, grayscale request that toggles
color on, SVG hook errors and allocation failure. Outgoing: FT load/render/error
APIs, installed SVG hooks, classifier, warning/fatal/assert machinery, global render
mode writers. Not closed by successful rasterization of one emoji.

## UI-FONT-019: LLFontFreetype::addGlyphFromFont

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L563).
Status: body-inspected, edges-open.

**1. Exact local path:**

1. Null destination head face returns null; assert destination is not fallback.
   Ask source font to render requested representation/index/character.
2. Classify actual bitmap MONO/GRAY as Grayscale, BGRA as Color. Other modes leave
   Unspecified; the default branch calls llassert_always(true), which is not a
   rejecting condition. Read bitmap width/height.
3. Call nextOpenPos(width, output positions/type/page), ignoring its bool result.
   Invalid type can therefore leave position/page outputs undefined; this is a
   risk to investigate, not a native feature requirement.
4. Allocate glyph info with requested type; store character, atlas position/page,
   dimensions/bearings, hinting deltas and advances divided by 64. If destination
   tabnum && weight>0 && digit, update maximum digit width.
5. Insert glyph info. If actual type differs from requested, copy info, change its
   type to actual and insert that too. Metadata insertion precedes CPU pixel copy
   and GPU upload.
6. MONO allocates width*height temporary bytes and expands each source bit MSB-first
   using source pitch; GRAY uses source pointer/pitch directly. Call luminance-alpha
   subimage writer; free temporary expansion if allocated.
7. BGRA calls BGRA subimage writer with abs(pitch); other modes assert false.
8. Fetch GL/raw atlas page. Both present: setSubImage(raw,0,0,full GL width,full GL
   height), ignoring returned success. Otherwise assert false. Return glyph info.

**2.** Native glyph metadata, CPU pixels and sampled image readiness need distinct
states and atomic publication guarantees. **3.** Prepare an owned raster result,
reserve/copy atlas space, submit an upload, then publish a version with its GPU
dependency. This is a native ownership choice, not permission to call the GL method.
Checks: requested/actual type mismatch, pitch/mono expansion, null page, upload
failure, metadata visibility before pixels, and concurrent readers of old pages.
Outgoing: renderGlyph; nextOpenPos; glyph info constructors/copy; insertGlyphInfo;
both pixel writers; image getters; LLImageGL::setSubImage; allocator/assert helpers.

## UI-FONT-020: LLFontFreetype::insertGlyphInfo

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L721).
Status: body-inspected, consumer lifetime open.

**1.** Assert type<Count; find same-character/same-type entry. Hit deletes the old
glyph pointer and replaces it; miss inserts pair. No notification to pointers
retained elsewhere appears here. **2.** CPU glyph index publication. **3.** Native
immutable metadata or stable generation handles avoid untracked pointer replacement.
Check cached consumer references after replacement and allocation failure at insert.
Outgoing: glyph destruction, map allocation and all consumers retaining glyph pointers.

## UI-ATLAS-001: LLFontBitmapCache::init

Source: [llfontbitmapcache.cpp](../../../indra/llrender/llfontbitmapcache.cpp#L41).
Status: body-inspected, reset destruction edges open.

**1.** Call reset; assign max character width/height; calculate width*20; double an
integer starting at 2 until >= that product; clamp to 512 and assign square bitmap
dimensions. No local checked arithmetic/input bound. **2.** Native atlas sizing is
CPU policy constrained by device limits. **3.** Validate requested glyph dimensions
and arithmetic before allocating bounded native pages. Defined reference packing
can be compared independently; integer overflow must not be reproduced as policy.
Check tiny/large dimensions and generation change. Outgoing: reset, llmin.

## UI-ATLAS-002: LLFontBitmapCache::nextOpenPos

Source: [llfontbitmapcache.cpp](../../../indra/llrender/llfontbitmapcache.cpp#L84).
Status: body-inspected, allocation/GL edges open.

**1.** Type>=Count returns false without assigning outputs. Otherwise use independent
raw/GL page vectors and offsets for that type. If raw pages empty OR x+width+1>
bitmap width, test raw-empty OR y+2*maxCharHeight+2>bitmap height. True allocates
a new square power-of-two-capped page using the same width*20 loop, with type-specific
components; grayscale clears luminance=255/alpha=0, color has no explicit clear in
this body. Append new LLImageGL(raw,false,false), reset offsets to 1, bind texture
unit 0 to the GL image, set point filtering. Inner false moves x to 1 and y by
maxCharHeight+1. Then assign position and last raw page index, advance x by width+1,
increment generation and return true. Width alone is supplied; no actual glyph
height fit test occurs here. The LLImageGL(raw,false,false) constructor arguments
disable mipmaps and compression; they do NOT disable creation. That overload calls
createGLTexture(0,raw) immediately. The subsequent bind is an additional state
dependency. This signature was checked directly, not inferred from argument values.

**2.** Native packing, allocation and sampled-image setup are separate tasks with
explicit page versions and initialization. **3.** CPU packing must validate both
dimensions and preserve borders/sampling results; resource allocation is native,
with no ambient texture-unit binding. Old submitted pages remain retained until
completion. Check row/page boundaries exactly at inequality thresholds, type split,
zero/oversized glyphs, raw/GL vector partial allocation and color-page initialization.
Outgoing: getNumComponents, LLImageRaw constructor/clear, LLImageGL constructor,
getImageRaw/getImageGL/getNumBitmaps, gGL.getTexUnit(0)->bind, setFilteringOption,
intrusive vector lifetimes and GL allocation/upload helpers. Not transitively closed.

## UI-ATLAS-003: getImageRaw, getImageGL, getNumBitmaps and getNumComponents

Sources: [llfontbitmapcache.cpp](../../../indra/llrender/llfontbitmapcache.cpp#L62),
[llfontbitmapcache.h](../../../indra/llrender/llfontbitmapcache.h#L60).
Status: individual local bodies inspected; returned-pointer lifetime open.

**1. getImageRaw/getImageGL:** convert type to index; type>=Count OR index beyond
vector size returns null, otherwise return intrusive pointer's raw value. The ||
short circuit prevents indexing invalid types. **1. getNumBitmaps:** valid type
returns raw vector length cast U32, otherwise 0. **1. getNumComponents:** Grayscale
returns 2, Color returns 4; default asserts false then returns 2 if execution resumes.
**2.** Native typed image descriptors and CPU page lookup serve these duties.
**3.** Avoid implicit lifetime transfer through raw pointers; retain referenced
page versions. Checks: Count/Unspecified and out-of-range page; empty vector;
assert policy. Outgoing: enum definition, vector size/access, intrusive conversion.