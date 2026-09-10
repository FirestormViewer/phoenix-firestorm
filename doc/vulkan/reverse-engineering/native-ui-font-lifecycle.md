# UI font configuration and lifetime dependencies

Source revision: 3abd661f498329babaf87b49ffab910fcb5f0e6c. Status: OPEN.
This continues [font lookup/glyph insertion](native-ui-dependencies.md) and links
to [image allocation/upload](native-ui-image-dependencies.md). Records distinguish
each named function's local behavior; no record implies transitive closure or
production readiness. Question 2/3 answers are conditional native design obligations
until every listed dependency is resolved. No reference code is changed.

## UI-FONT-L01: LLFontGL::initClass

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L896).

**1.** Write vertical DPI=floor(screenDPI*yScale), horizontal DPI=floor(screenDPI*
xScale), scales and app directory. If no registry: allocate with create-gl flag and
size modifier; parse selected file under install fonts. Failure: try user-settings
fonts. Second failure: find skinned fonts.xml files; empty list logs fatal; iterate
all found paths and warn for each parse failure. Registry already exists: reset it,
without reparsing requested file or updating its construction flags/size modifier.
Then call loadDefaultFonts, ignoring bool return. No local rollback of globals on
parse/reset/font failure. **2.** Native initialization/reconfiguration is a CPU
configuration transaction plus separately published GPU resources. **3.** Candidate
catalog/font generations should be complete before replacing live prepared state;
decide explicitly which old failure semantics are retained or corrected.
Checks: first versus repeated init; file change with existing registry; both failed
paths; empty/all-failed fallback list; changed DPI/size modifier/create-gl flag.
Outgoing: llfloor; directory/skin search; registry constructor/parse/reset;
loadDefaultFonts; allocation/fatal behavior and global initialization callers.

## UI-FONT-L02: LLFontGL::loadDefaultFonts

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L970).

**1.** Initialize succ=true, then bitwise-AND-assignment with nonnull results of
SansSerifSmall, SansSerif, SansSerifBig, SansSerifHuge, SansSerifBold, Monospace,
Scripting and OCRA getters, in that order. Every expression is evaluated even after
succ becomes false; return accumulated bool. Comment saying not to call this during
initClass conflicts with the actual initClass call and is not the behavior.
**2.** Native required-font availability checking. **3.** Explicit required request
set with ordered failures/preparation, not reliance on static getter side effects.
Checks: early missing font does not skip later requests. Outgoing: each named getter
must receive its own descriptor/static-lifetime trace; profiling scope.

## UI-FONT-L03: LLFontGL::loadCommonFonts

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L989).

**1.** Request SansSerif Small/BOLD, Large/BOLD, Huge/BOLD, then Monospace Medium/0.
Ignore all results. **2.** CPU/glyph warmup requirements. **3.** Declare these requests
at service preparation only after callers' readiness expectations are known.
Checks: all requests execute, nulls ignored locally. Outgoing: descriptor construction,
getFont, profiling, and calling stages.

## UI-FONT-L04: LLFontGL::destroyDefaultFonts

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L1001).

**1.** Delete registry, then set global pointer null. No function-static getter
pointers are reset in this body. **2.** Native service shutdown invalidates handles
while retaining resources needed by live readers/submissions. **3.** Explicit owner
generations and cancellation before registry retirement. Checks: cached getters or
prepared text after destroy/recreate. Outgoing: registry destructor and all retained
raw pointer consumers; no assumption they are safe.

## UI-FONT-L05: LLFontGL::destroyAllGL

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L1009).

**1.** If registry exists call destroyGL, otherwise no-op. Registry remains allocated.
**2.** Native GPU detach distinct from CPU font service destruction. **3.** Invalidate
sampled-image publication and retire GPU versions by completion; CPU metrics can
survive only with an explicit consumer contract. Check null registry and later use
of glyph metadata after GL names were destroyed. Outgoing: registry destroyGL.

## UI-FONT-L06: LLFontRegistry constructor

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L265).

**1.** Store create-gl flag and size modifier; cache LLWindow::getDynamicFallbackFontList
once during construction. **2.** Native platform font discovery produces CPU catalog
inputs. **3.** Audit the platform implementation and own its results without linking
GL window owners merely to call this helper. Check platform-specific discovery,
failure, directory changes and timing. Outgoing: LLWindow platform dispatch and
Fontconfig/OS/filesystem helpers; all remain open.

## UI-FONT-L07: LLFontRegistry::parseFontInfo

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L278).

**1.** Parse one full filename through LLXMLNode::parseFile. Failure returns false;
null/wrong root warns and false. Read unused root name attribute. Root fonts calls
init_from_xml, ORs its bool into initially-false success and returns it. The older
skin-path search implementation is under #if0, not active. **2.** Structured CPU
catalog parsing. **3.** Keep source provenance and parsing outcomes explicit; do not
claim validation stronger than the actual parser provides. Checks: malformed file,
wrong root and root with unknown/empty children. Outgoing: XML parser, init_from_xml,
file loading and error policy.

## UI-FONT-L08: currentOsName

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L337).

**1.** Preprocessor selects Windows, else Mac for LL_DARWIN, else Linux for LL_LINUX,
else empty string. **2.** CPU catalog platform key. **3.** Native configuration should
provide the exact supported key, preserving case. Check each compiled configuration;
not all are executed by a Windows build. Outgoing: build definitions.

## UI-FONT-L09: font_desc_init_from_xml

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L350).

**1.** If node tag is font: optional name replaces descriptor name; optional
font_style uses getStyleFromString; size becomes TEMPLATE. Iterate all children,
read name for each. File child: text content is filename; defaults auto hinting,
flags0, weight-1, delta0 and empty predicate. Optional functor is copied. Lowercase
font_hinting: default/force_auto/no_hinting select enum; unknown stays auto. Lowercase
flags: only exact bold sets BOLD. Optional F32 delta/S32 weight parse return values
are ignored. Optional load_collection parses initially-false bool; true adds to
collection list. Every file also adds to ordinary list. OS child whose name equals
currentOsName recursively processes its children; other children ignored. Return
true unconditionally; recursion's return is not tested.
**2.** CPU catalog parsing with explicit defaults and platform applicability.
**3.** Preserve authored order and actual accepted syntax; stricter rejection or
additional style support is a declared correction, not inferred equivalence.
Checks: unknown hinting/flags/predicate, failed numeric/bool parsing, nested matching
and nonmatching OS nodes, collection flag and ordering. Outgoing: XML accessors,
getStyleFromString, currentOsName, file-info constructors and predicate lookup.

## UI-FONT-L10: LLFontDescriptor::addFontFile

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L250).

**1.** Look up predicate name in static map; append file info with matching function
or null when absent. It does not reject unknown predicates. **2.** CPU fallback
declaration. **3.** Native predicate identity and enabled state should be explicit
data, with approved unknown-name behavior. Check unknown/empty/known names.
Outgoing: static map initialization, each predicate target, LLFontFileInfo constructor.

## UI-FONT-L11: LLFontDescriptor::addFontCollectionFile

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L256).

**1.** Same predicate lookup behavior as addFontFile, but appends to collection list.
**2.** CPU declaration of collection membership. **3.** Preserve relation to ordinary
file list and its filename matching; do not treat it as a separately loaded ordered
face list without examining createFont. Check duplicate filenames with differing
metadata/predicates. Outgoing: predicate map/function and file-info construction.

## UI-FONT-L12: init_from_xml

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L449).

**1.** Iterate children/read name. Font child: default descriptor; call descriptor
parser; normalize even before testing its success. If success, lookup matching
template. No match inserts normalized key/null. Match copies prior ordinary and
collection lists, prepends new respective lists, copies prior descriptor, sets
merged lists, erases old key and inserts new/null. It does not delete a nonnull
pointer that might have been stored under the erased key here. Font_size child:
only successful name AND F32 size parsing writes size+registry modifier. Unknown
children ignored. Return true unconditionally.
**2.** CPU ordered catalog merge. **3.** Native candidate catalog construction with
explicit template/resource separation avoids storing null resources as definitions.
Checks: duplicate templates, file prepend order, malformed/negative/nonfinite sizes,
unknown nodes and parse after fonts already exist. Outgoing: parser/normalize/matching,
descriptor copies, map resource ownership and XML conversion behavior.

## UI-FONT-L13: LLFontRegistry::nameToSize

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L520).

**1.** Exact string lookup; found writes referenced output size and true; miss false
without writing output. **2.** CPU lookup. **3.** Native optional/result type makes
miss explicit without an uninitialized output. Check miss leaves sentinel unchanged
and case sensitivity. Outgoing: map lookup/string comparison only.

## UI-FONT-L14: LLFontRegistry::getMatchingFontDesc

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L770).

**1.** Normalize argument, find map entry, return pointer to stored key or null.
Normalization drops tabnum as previously traced. **2.** CPU template lookup.
**3.** Keep stable immutable catalog keys; pointer validity must not cross map erase/
rebuild. Check table mutation after pointer acquisition and tabnum requests.
Outgoing: normalize, comparator/map lookup and callers retaining key pointer.

## UI-FONT-L15: bitCount

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L782).

**1.** Initialize count0; independently test masks1,2,4,8,16,32,64,128 and increment
once for each set bit; return U32 count. **2.** CPU style scoring. **3.** Native
implementation may use an equivalent fixed-width population count. Check all256
input values; no GPU/resource dependency. Outgoing: none beyond integer operations.

## UI-FONT-L16: LLFontRegistry::getClosestFontTemplate

Source: [llfontregistry.cpp](../../../indra/llrender/llfontregistry.cpp#L805).

**1.** Return exact matching descriptor if found. Else normalize, scan mFontMap in
its comparator order. Skip non-TEMPLATE, wrong name, or any style bit not requested.
First candidate becomes best. Later candidate with greater matched-bit count
replaces best and continues. Otherwise, any candidate matching BOLD replaces best;
the code does not explicitly require equal bit counts despite the tie-break comment.
Return best or null. **2.** CPU nearest-template policy. **3.** Preserve actual
iteration/scoring behavior or document a correction; generic distance sorting is
not an established equivalent. Checks: exact match, zero candidates, all style
subsets, comparator ordering and bold fallback branch. Outgoing: matching/normalize,
isTemplate/getters, bitCount and comparator.

## UI-FONT-L17: LLFontManager::initClass

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L65).

**1.** If global manager absent allocate one; otherwise no-op. **2.** CPU font-library
service setup. **3.** Explicit application-owned initialization, with ownership and
ready state established before faces. Check repeated init/failed construction.
Outgoing: manager constructor, global pointer writers and allocation/fatal policy.

## UI-FONT-L18: LLFontManager constructor

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L80).

**1.** FT_Init_FreeType writes global gFTLibrary. Error logs fatal, then calls
FT_Done_FreeType if execution resumes. ENABLE_OT_SVG_SUPPORT is defined in this
translation unit: construct four callbacks (OnInit, OnFree, OnRender,
OnPresetGlypthSlot) and call FT_Property_Set(ot-svg,svg-hooks), ignoring its result.
**2.** Native FreeType setup is CPU library initialization, including renderer hooks.
**3.** Own a library and installed-hook state explicitly; reject unavailable required
capability before exposing a ready service. Do not reuse the GL manager singleton.
Checks: FT initialization/property failure, SVG module absence and exception paths.
Outgoing: FreeType initialization/property contracts, four hooks, fatal logger.

## UI-FONT-L19: LLFontManager::loadFont

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L1055).

**1.** Set output size0 in try block. Cache filename hit increments mRefs, writes
stored size cast long and returns cached string buffer. Miss reads LLFile::getContents;
empty -> null. Assert content size<long max; set output size, allocate LoadedFont
with a copied string, insert shared owner into map, return cached buffer. bad_alloc
invokes out-of-memory message and fatal log, then returns null if allowed to resume.
Other exceptions are not caught. The reference count field is incremented here;
this function does not evict/release according to it.
**2.** CPU byte ownership and filename/content identity. **3.** Shared immutable byte
leases must outlive every FreeType memory face; budget and invalidation need explicit
policy instead of incidental process-global retention. Check empty/missing/replaced
file, repeated filename and allocation failure. Outgoing: LLFile IO, LoadedFont
constructor/string copy, map ownership, warning/fatal routines and face lifetime.

## UI-FONT-L20: ll::fonts::LoadedFont constructor

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L1037).

**1.** Copy byte string into mAddress; assign filename, size and refs=1. **2.** CPU
byte record. **3.** Native immutable storage with actual RAII users, not a separate
unused logical count. Check byte retention and copy/allocation failure. Outgoing:
standard strings and the owning map's lifetime.

## UI-FONT-L21: LLFontFreetype::setVariationAxis

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L965).

**1.** Null face false. FT_Get_MM_Var failure false. Scan axes for tag built from
axis_tag[0..3], with no length check. Match: clamp requested value to min/max divided
by65536, break. No match: free descriptor, warn once, false. Allocate coordinates
array; call FT_Get_Var_Design_Coordinates without checking result; overwrite selected
coordinate with truncated fixed value; call setter; delete array/free descriptor.
Setter error warns/false, otherwise debug log/true. Allocation exception after
descriptor creation lacks local descriptor cleanup.
**2.** CPU variable-face configuration. **3.** RAII descriptor/coordinate ownership,
validated tags and explicit getter/setter errors; preserve wght and opsz ordering
and coordinate values for successful cases. Checks: missing tag, short tag,
clamp boundaries, failed getter/setter, allocation failure and two-axis updates.
Outgoing: FreeType APIs, llclamp, logging/fatal and caller-specified tags.

## UI-FONT-L22: LLFontFreetype::setSubImageBGRA

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L888).

**1.** Get color raw page; assert nonfallback. Missing page asserts then false;
assert components4. Missing data pointer false. For each row, source row=height-1-row,
source offset=row*width*4; destination uses page width and x/y. Pack U32 as
A<<24 | B<<16 | G<<8 | R. The stride parameter is unused; there is no bounds
validation or image-data lock in this body. Return true. **2.** Native CPU pixel
conversion must preserve channel order, alpha and row orientation separately.
**3.** Sized row/slice views with checked extents and declared premultiplication;
no unchecked reinterpretation. Checks: padded/negative pitch, zero dimensions,
destination bounds and endian assumption; unsafe paths are not parity mandates.
Outgoing: bitmap cache/raw data access and source renderer representation.

## UI-FONT-L23: LLFontFreetype::setSubImageLuminanceAlpha

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L926).

**1.** Get grayscale raw page; assert nonfallback. Missing page asserts/returns.
Acquire LLImageDataLock; assert two components and data pointer. Missing source or
destination returns. Zero stride becomes width. For each row read reversed source
row times stride and copy coverage only into destination alpha channel; luminance
is left unchanged. No rectangle bounds check here. **2.** CPU coverage placement.
**3.** Checked native atlas copy under documented ownership, preserving white
luminance from initialization and separate source/destination orientation.
Checks: row pitch/zero pitch, rectangle boundaries, null pointers and lock lifetime.
Outgoing: raw getters/cache, LLImageDataLock and backing allocation initialization.

## UI-FONT-L24: LLFontFreetype::resetBitmapCache

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L814).

**1.** Delete every glyph-info pointer, clear map, reset bitmap cache and max-digit
width=0. If not fallback, add glyph zero immediately using current face (null face
returns from insertion). **2.** Native invalidation of metrics/glyph resources.
**3.** Replace generations only after preparing required state; retain old runs until
CPU/GPU users complete. Check metadata users, default glyph after clear, fallback
and null-face cases. Outgoing: pointer deletion, cache reset, addGlyphFromFont.

## UI-FONT-L25: LLFontFreetype::reset

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L791).

**1.** resetBitmapCache first, then loadFace with stored file/size/weight/fallback/
hinting/flags/tabnum and face index0; ignore bool. For nonfallback head, empty
fallback list warns; otherwise recursively reset every fallback. Fallback owners
do not recurse here. **2.** CPU font reconfiguration plus native image invalidation.
**3.** Avoid destroying active face/resources before replacement validity is known;
record failure behavior changes explicitly. Check load failure, repeated/shared
fallbacks and default glyph work before face reload. Outgoing: resetBitmapCache,
loadFace, fallback graph and its ownership/cycle assumptions.

## UI-FONT-L26: LLFontFreetype destructor

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L161).

**1.** If face exists FT_Done_Face; set null. Delete every glyph info, clear map,
delete bitmap cache. Fallback vector's intrusive references release implicitly
after the body. **2.** Native CPU/GPU owner teardown. **3.** Byte/library lifetime
must exceed face lifetime; GPU versions retire separately after submissions.
Check head/fallback destruction order and outstanding runs. Outgoing: FreeType face
destruction, glyph/cache destruction, LLPointer unref and library cleanup order.

## UI-FONT-L27: LLFontManager::cleanupClass

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L73).

**1.** Delete global manager then null it; does not null gFTLibrary. **2.** CPU library
shutdown. **3.** Explicit service owner with no live faces before library release.
Check outstanding faces and repeated cleanup. Outgoing: manager destructor and
all caller orderings.

## UI-FONT-L28: LLFontManager destructor and unloadAllFonts

Source: [destructor](../../../indra/llrender/llfontfreetype.cpp#L101),
[unloadAllFonts](../../../indra/llrender/llfontfreetype.cpp#L1092).

**1. Destructor:** FT_Done_FreeType(global library), then unloadAllFonts.
**1. unloadAllFonts:** clear the shared-owner map, releasing cached byte strings.
Neither function traverses live LLFontFreetype owners to establish they are gone.
**2.** Native CPU library/byte cleanup. **3.** Retained library/bytes through face
ownership; teardown ordering proved at service boundary. Check face callbacks during
FT library teardown and subsequent owner destruction; no safety claim from a cleared
map. Outgoing: FreeType destruction callback behavior and shared/string destruction.

## UI-SVG-001: LLFontFreeTypeSvgRenderer::OnInit

Source: [llfontfreetypesvg.cpp](../../../indra/llrender/llfontfreetypesvg.cpp#L56).

**1.** Write shared driver-state pointer=null and return FT_Err_Ok. Glyph state is
not allocated here. **2.** CPU hook initialization. **3.** Native hook state should
have explicit lifetime per library/face/glyph as appropriate, not assume GL state.
Check callback pointer validity and driver init contract. Outgoing: FreeType hook API.

## UI-SVG-002: LLFontFreeTypeSvgRenderer::OnFree

Source: [llfontfreetypesvg.cpp](../../../indra/llrender/llfontfreetypesvg.cpp#L66).

**1.** Empty body; ignores shared state pointer. **2.** CPU hook cleanup contract.
**3.** Native implementation must release whatever state it owns; copying this no-op
is correct only if it owns none. Check init/free pairing and cached glyph teardown.
Outgoing: external hook ordering and glyph finalizer.

## UI-SVG-003: LLFontFreeTypeSvgRenderer::OnDataFinalizer

Source: [llfontfreetypesvg.cpp](../../../indra/llrender/llfontfreetypesvg.cpp#L71).

**1.** Cast object to glyph slot, get its generic-data pointer, clear generic data
and finalizer, delete render-data struct. Its NSVGimage field is a raw pointer with
no destructor shown, so deleting the struct does not itself call nsvgDelete.
**2.** CPU glyph cache retirement. **3.** RAII parse-image ownership must cover success,
failure and destruction without rendering. Check preset-only and failed-render
finalization. Outgoing: FreeType generic finalizer protocol, parsed-image lifetime.

## UI-SVG-004: LLFontFreeTypeSvgRenderer::OnPresetGlypthSlot

Source: [llfontfreetypesvg.cpp](../../../indra/llrender/llfontfreetypesvg.cpp#L84).

**1.** Read FT SVG document from slot->other. Assert existing cached state's glyph
matches when cache=true. Allocate per-slot render data/finalizer if absent. cache=false
sets glyph index and error=Ok. Assert absent parsed image unless cache=true; if no
image allocate length+1 temp document, copy and terminate, nsvgParse(px, DPI0), then
delete temp. Parse-null records/returns Invalid_SVG_Document.

Nonidentity matrix or **positive** x/y delta records/returns Unimplemented_Feature.
Negative translation is not rejected, despite the zero-translation comment.
Width or height exactly0 makes both units_per_EM. Compute scale=min(x_ppem/floor(width),
y_ppem/floor(height)), store scale; bitmap width/rows truncate floored dimensions
times scale; left=(x_ppem-width)/2, top=face ascender/64, pitch=width*4, mode=BGRA.
Set metrics width/height; horizontal bearingX0 and bearingY=-bitmap_top; vertical
bearings from previous metrics/advances. If vertical advance0, synthesize rows*1.2*64.
Return Ok without an explicit final reset of cached error. No finite/bounds checks
or C++ exception containment in this callback.

**2.** Native SVG glyph preparation is CPU geometry/raster work governed by FreeType
metrics/transform conventions, independent of Vulkan submission. **3.** Owned parsed
documents and validated dimensions/transforms, with explicit supported capability
and error conversion. Do not carry unsupported-transform approximations or unbounded
allocation across merely because the reference lacks guards.
Checks: cache true/false sequence, glyph change, malformed SVG, missing face/document,
zero/fractional dimensions, positive/negative delta and matrix transforms, allocation
failure. Outgoing: FT SVG document API, generic slot lifecycle, NanoSVG parser,
math/conversion, allocation and callback error propagation.

## UI-SVG-005: LLFontFreeTypeSvgRenderer::OnRender

Source: [llfontfreetypesvg.cpp](../../../indra/llrender/llfontfreetypesvg.cpp#L176).

**1.** Read slot generic data without null check, assert error Ok; nonzero error
returns it. Create NanoSVG rasterizer without null check, rasterize image into
FreeType-provided buffer using stored scale and bitmap dimensions/pitch, delete
rasterizer and parsed image, set parsed image=null. Iterate rows and pitch/4 pixels;
read RGBA bytes and pack A<<24 | (R*A/255)<<16 | (G*A/255)<<8 | B*A/255 into U32.
Set slot format=BITMAP, pixel mode=BGRA, return Ok. It does not set num_grays here.
**2.** Native CPU raster output is premultiplied BGRA with explicit byte layout;
Vulkan upload/sampling/blend is a separate consumer contract. **3.** Preserve
premultiplication and coordinate output on supported inputs; contain allocation/
parser errors and retain state safely across callbacks. Sharing a neutral SVG
library is possible after its API/support contract is audited; do not invoke the
GL font renderer object as the native service.
Checks: pixel channels/alpha rounding, zero-alpha output, buffer pitch, early error,
rasterizer allocation failure and repeat render/preset/finalizer lifecycle. Outgoing:
NanoSVG parser/rasterizer contracts, FreeType-owned buffer and caller blend policy.