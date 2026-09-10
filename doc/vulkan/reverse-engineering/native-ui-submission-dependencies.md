# UI vertex and shader submission dependencies

Source: 3abd661f498329babaf87b49ffab910fcb5f0e6c, implementation 90af5a7220.
Status: local source investigation, transitive coverage OPEN. This continues
[frame/state traversal](native-ui-frame-dependencies.md). Native design obligations
are not implemented APIs or parity claims. Every buffer/shader/helper edge named
below needs its own closed record before relying on it as a native contract.

## UI-SUBMIT-001: LLRender::begin

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1573).
**1.** Only if new mode differs: old LINES/TRIANGLES/POINTS flushes; otherwise
nonzero pending count logs fatal; assign new mode. Same mode performs no operation.
**2.** Native primitive topology and batch boundaries are explicit. **3.** Prepare
ordered geometry batches and validate primitive completeness, not emulate begin/end.
Checks: same mode, each old mode, pending counts, failure continuation. Outgoing:
flush, fatal policy and topology enum map; edges open.

## UI-SUBMIT-002: LLRender::end

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1592).
**1.** Count0 returns. Otherwise modes other than LINES/TRIANGLES/POINTS OR count>2048
flush; other cases remain pending. **2.** Native batch completion need not match
each legacy end. **3.** Preserve painter/state order while aggregating compatible
native draws; end is not proof that GL work already submitted. Checks:2048/2049,
strip/independent primitives and empty batch. Outgoing: flush and state-changing callers.

## UI-SUBMIT-003: LLRender::flush

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1609).
**1.** Debug check; count0 skips body. Nonzero: assert shader pointer; if UI-offset
stack nonempty increment UI-call count and add original count to UI-vertex count.
Copy count locally. TRIANGLES count%3 truncates incomplete tail and warns; LINES
count%2 likewise. Set mCount=0 before further calls to avoid drawArrays reentry.
Buffer present: read bound shader attribute mask. Retained-list pointer present:
genBuffer, append buffer/mode/count/unit0 texture and modelview/projection/texture0
matrices to list. Otherwise bufferfromCache. In both cases call drawBuffer. Buffer
absent logs fatal. Finally resetStriders(adjusted count). No local allocation/error
unwind restores pending count or retained records.
**2.** Native CPU batches and GPU submission are separated; topology, vertices,
resource versions and matrices are explicit immutable inputs. **3.** Completion-owned
vertex storage and descriptor leases, with ordered batching and deliberate retained
UI caching, replace global-state capture. Do not preserve undefined cache collisions
or incomplete geometry as native feature requirements.
Checks: incomplete tail, retained versus cached path, empty/no buffer, missing shader,
recursive flush and state changes before physical draw. Outgoing: genBuffer,
bufferfromCache, drawBuffer, resetStriders, LLVertexBufferData construction and
retained-list replay/destruction, debug/profiling/fatal. All remain open.

## UI-SUBMIT-004: LLRender::bufferfromCache

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1683).
**1.** Hash count*LLVector4a position bytes; append UV bytes if mask includes
TEXCOORD0 and color bytes if COLOR. Finalize XXH64 and lookup by digest. Hit returns
buffer and updates timestamp. Miss genBuffer, insert buffer+now, increment static
miss count. Only miss_count>1024 triggers cleanup/reset0; erase entries last touched
strictly more than1 second ago, retain others. Return buffer. Key does not explicitly
include attribute mask, count, topology or matrices, and no byte equality fallback
checks hash collisions. No cleanup on cache hit.
**2.** Native geometry reuse is a CPU cache with completion-safe resource ownership.
**3.** Cache identity must include required ABI/content and verify correctness;
eviction drops logical ownership, not resources still in GPU use. Timestamp policy
is not a GPU lifetime proof. Checks: identical/different optional streams, hash
collision handling,1024/1025 misses,1-second boundary and retained references.
Outgoing: HBXXH64 implementation/API, genBuffer, cache entry destructor/refcount,
clock and collection thread ownership. Edges open.

## UI-SUBMIT-005: LLRender::genBuffer

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1758).
**1.** Allocate LLVertexBuffer(attribute_mask), allocateBuffer(count,0) without
testing result. Non-Apple device flag -> setBuffer before upload. Always position
data; conditional TEXCOORD0 and COLOR uploads. Under LL_DARWIN compile guard call
unmapBuffer, then unbind and return pointer. Device flag and compile guard are
distinct controls, not interchangeable platform assumptions. **2.** Native vertex
upload owns byte ranges and readiness. **3.** Explicit initialized buffer version
with bounded staging, no GL VBO abstraction reuse. Checks: failed allocateBuffer,
Apple/non-Apple and attribute combinations. Outgoing: constructor/allocation,
setBuffer, each setter, unmapBuffer/unbind and data strides. Open.

## UI-SUBMIT-006: LLRender::drawBuffer

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1797).
**1.** vb->setBuffer then vb->drawArrays(mode,0,count), no null check. **2.** Native
recording binds specified geometry and draws under explicit pipeline state.
**3.** Draw packet retains buffer/pipeline/resource versions rather than reading
ambient shader state. Check geometry availability and state synchronization ordering.
Outgoing: both VBO methods and buffer lifetime.

## UI-SUBMIT-007: LLRender::resetStriders

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1803).
**1.** Copy vertex/UV/color at supplied index to element0 of corresponding streams,
then count0. Carries current attributes into next batch; index is adjusted primitive
count after incomplete-tail handling. **2.** Native preparation captures current
attributes at vertex emission. **3.** Explicit vertex values make carry state local
to preparation, not recording. Checks: flushed complete/incomplete/empty batch and
next vertex inherited attributes. Outgoing: stream extent and element copy semantics.

## UI-SUBMIT-008: LLRender::vertex3f

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1812).
**1.** If count>2048: POINTS flush; TRIANGLES flush only count%3==0; LINES only even
count; other modes no switch action. If count>4094 afterward, return without adding.
Empty UI-offset stack stores x/y/z; nonempty stores (position+offset.back)*scale.back
component-wise. Increment count and copy just-written vertex/color/UV to next slot.
No independent scale-stack check here. **2.** CPU native vertex preparation with
explicit transform order and bounded output. **3.** Use coherent prepared geometry
buffers; capacity failure must be explicit rather than silently missing geometry.
Checks: all topology/capacity boundaries, offset then scale, inherited attributes,
stack mismatch and resulting raster position. Outgoing: flush, vector math, streams.

## UI-SUBMIT-009: vertexBatchPreTransformed(positions,count)

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1848).
**1.** If existing count+batch>4094 return unchanged. Loop copies each position at
current slot, increments count, copies prior UV/color into new current slot. After
loop if count>0 copy previous vertex into current slot. No UI transform applied.
**2.** Already-prepared native vertex input. **3.** Distinguish transformed and local
inputs explicitly; avoid double transforms. Checks:0/negative/excess count and
retained current attributes. Outgoing: bounds/array/strider semantics and callers.

## UI-SUBMIT-010: vertexBatchPreTransformed(positions,uvs,count)

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1870).
**1.** Same capacity early return; copy position and UV at current slot, increment,
propagate color into new slot. If final count>0 propagate last position and UV into
current slot. **2.** Native prepared geometry with inherited color. **3.** Capture
color as explicit data, keep transform convention typed. Checks: boundaries,
empty batch and per-stream source length. Outgoing: array/strider and consumers.

## UI-SUBMIT-011: vertexBatchPreTransformed(positions,uvs,colors,count)

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1894).
**1.** Capacity check/return; copy all three streams for each vertex, increment.
If final count>0 propagate last position/UV/color into current slot. No transform.
**2.** Native explicit vertex packet, used by glyph rendering. **3.** Retain geometry
and color precision/normalization through the native ABI. Checks: font batches,
capacity and zero/excess inputs. Outgoing: striders, font producers, flush consumers.

## UI-SUBMIT-012: LLRender::setLineWidth

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1510).
**1.** Width>1 clamps to smooth max if glIsEnabled(LINE_SMOOTH), otherwise aliased
max. Changed width OR mDirty: if current topology LINES/LINE_STRIP flush, assign
width and call glLineWidth. Other modes do not flush here. No lower/finite clamp.
**2.** Native stroke rendering must satisfy width/coverage requirements on selected
device. **3.** Native geometric strokes or supported wideLines require a reviewed
choice; this source does not expand lines in shaders/CPU. Checks: smooth flag,
device max, <=1/invalid widths, topology and pending geometry. Outgoing: GL query,
maximum-width initialization, flush and GL line rasterization contract.

## UI-SUBMIT-013: LLRender::setSceneBlendType

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1407).
**1.** Dispatch ALPHA -> sourceAlpha/oneMinusSourceAlpha; ADD -> one/one;
ADD_WITH_ALPHA -> sourceAlpha/one; MULT -> destColor/zero; MULT_ALPHA -> destAlpha/
zero; MULT_X2 -> destColor/sourceColor; REPLACE -> one/zero. Unknown logs fatal.
All valid choices call the two-factor overload. **2.** Native explicit blend
equations/factors. **3.** Include both alpha and color factors in pipeline identity,
not assume conventional premultiplied blending for every UI asset.
Checks: each enum and actual destination alpha results. Outgoing: blendFunc and
factor lookup/fatal policy; callers select which contract applies.

## UI-SUBMIT-014: LLRender::blendFunc(two factors)

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1438).
**1.** Assert both factors<BF_UNDEF. If any cached color/alpha source/destination
factor differs, set all four caches from the two arguments, then flush, then
glBlendFunc with mapped GL factors. Equal cache no-op. CPU caches update before
pending geometry physically draws; GL factors update afterward. **2.** Native
draw packets need the state in effect when their geometry was prepared. **3.**
Explicit ordered state avoids this split CPU/GPU timing; no call translation.
Checks: pending draws, same factors, invalid enums, color/alpha agreement.
Outgoing: factor table, flush/reentrant consumers, GL blending and assertions.

## UI-SUBMIT-015: LLRender::blendFunc(four factors)

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1454).
**1.** Assert four factors valid; any difference assigns all four caches, flushes,
then glBlendFuncSeparate with mapped values. Equal state no-op. **2.** Native
separate color/alpha blend state. **3.** Preserve per-contribution blend contract
with explicit pipeline key and order. Checks: differing alpha only and queued work.
Outgoing: mappings, flush, GL API and every caller selecting custom blends.

## UI-SUBMIT-016: LLRender::beginList

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1549).
**1.** Existing list logs fatal; assert bound shader is gUIProgram; flush pending
geometry then assign caller list pointer. **2.** Native retained prepared geometry.
**3.** Retained records must own every geometry/image/font version and explicit
state, not snapshot GL names. Checks: nested lists, null list, shader mismatch,
pending geometry outside versus inside list. Outgoing: flush and caller list lifetime.

## UI-SUBMIT-017: LLRender::endList

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1560).
**1.** Nonnull list: flush then set list null. Otherwise assert false. **2.** Native
prepared-record publication boundary. **3.** Atomic publish after preparation, with
failure rollback and completed-use retirement for old records. Check flush failure,
empty list and replay after invalidation. Outgoing: flush, retained list owners.

## UI-VBO-001: LLVertexBuffer::allocateBuffer

Source: [llvertexbuffer.cpp](../../../indra/llrender/llvertexbuffer.cpp#L1256).
**1.** Inputs are U32; local nverts<0 OR nindices<0 condition cannot detect negative
signed inputs already converted. Initialize success=true; bitwise-AND-assign
updateNumVerts then updateNumIndices, so both run even if the first fails. Return
combined bool. **2.** Native checked byte/element allocation. **3.** Validate inputs
before unsigned conversion and publish only consistent vertex/index ownership.
Checks: allocation failure at either stage, zero elements and signed caller casts.
Outgoing: both update methods, pool selection/allocation, error logging; open.

## UI-VBO-002: LLVertexBuffer::setBuffer

Source: [llvertexbuffer.cpp](../../../indra/llrender/llvertexbuffer.cpp#L1679).
**1.** Debug check; mapped -> warn once and _unmapBuffer. Assert mapped region lists
empty, shader bound, shader attribute mask subset of VBO type mask. Different VBO
name binds ARRAY_BUFFER, updates current cache and calls setupVertexBuffer. Same
VBO but changed mask calls setupVertexBuffer and updates sLastMask. Different index
buffer binds ELEMENT_ARRAY_BUFFER and updates current cache. Debug check at end.
**2.** Native geometry binding and vertex-format/pipeline compatibility. **3.**
Validate ABI at preparation/pipeline creation and bind explicit initialized versions;
no hidden upload on buffer binding. Checks: mapped recovery, mask mismatch, changed
buffer/same mask, same buffer/changed mask, changed index. Outgoing: unmap, setup,
shader mask producer, GL binding/global caches and error policy. Open.

## UI-VBO-003: LLVertexBuffer::drawArrays(mode,first,count)

Source: [llvertexbuffer.cpp](../../../indra/llrender/llvertexbuffer.cpp#L942).
**1.** Assert range within vertex count, own buffer/index names equal cached bound
names; gGL.syncMatrices; debug checks around glDrawArrays(sGLMode[mode],first,count).
No local mode range check. **2.** Native draw with explicit geometry, pipeline and
uniforms. **3.** Validate ranges and capture matrices before recording; retain
resources through all uses. Checks: range/mode, shader matrix changes and bindings.
Outgoing: syncMatrices, mode table, GL draw contract and diagnostics. Open.

## UI-VBO-004: LLVertexBuffer::unbind

Source: [llvertexbuffer.cpp](../../../indra/llrender/llvertexbuffer.cpp#L980).
**1.** Debug checks; bind ARRAY_BUFFER0 and ELEMENT_ARRAY_BUFFER0, then set both
cached names0. **2.** Native explicit subsequent bindings, no required unbind analog.
**3.** Eliminate ambient binding dependencies while preserving queued-work order.
Checks: vertex attribute state and pending buffers at callers. Outgoing: GL binding
semantics, debug checks and current-state consumers.

## UI-SHADER-001: interface/uiV.glsl main

Source: [uiV.glsl](../../../indra/newview/app_settings/shaders/class1/interface/uiV.glsl#L37).
**1.** gl_Position=MVP*vec4(position,1); output UV=(textureMatrix0*vec4(texcoord0,0,1)).xy;
output color=diffuse_color. Inputs vec3/vec2/vec4 respectively. No branch, lighting,
gamma or alpha mutation. **2.** Native vertex shader uses explicit vertex/uniform ABI
and coherent viewport/clip-depth convention. **3.** Preserve independent texture
matrix and position transform, rather than bake only translation and lose transformed
UV behavior. Checks: both matrices nonidentity, normalized byte color, clipping and
position orientation. Outgoing: loader-inserted prelude/variant, attribute/uniform
binding, interpolation and rasterization configuration; these remain open.

## UI-SHADER-002: interface/uiF.glsl main

Source: [uiF.glsl](../../../indra/newview/app_settings/shaders/class1/interface/uiF.glsl#L34).
**1.** frag_color=vertex_color*texture(diffuseMap,UV.xy). No discard, gamma conversion,
premultiplication, depth write or conditional. Sampler and attachment state determine
additional behavior. **2.** Native sampled color multiplication, with typed source
encoding/alpha and explicit blend/output format. **3.** Minimal shader plus accurate
resource interpretation and pipeline state; adding 'standard' gamma/alpha handling
would change results. Checks: coverage/colored images, opacity, sampler modes and
attachment encoding. Outgoing: generated prelude, sampler binding, texture swizzle,
blend equation, target format and color-write mask. Not a complete shader pipeline.

## UI-SHADER-003: gUIProgram construction in loadShadersInterface

Source: [llviewershadermgr.cpp](../../../indra/newview/llviewershadermgr.cpp#L3249).
This is a local block record, NOT closure of loadShadersInterface.
**1.** Only prior success=true enters block. Name UI Shader, clear shader file list,
append uiV vertex then uiF fragment files, set level from interface shader setting,
assign success=createShader(). **2.** Native complete shader variant/build manifest.
**3.** Reproducible source/prelude/ABI/pipeline validation, not compile these raw files
alone. Check prior failure, shader level and compile/link failure propagation.
Outgoing: createShader, loader/features/uniform/attribute binding and owning function's
preceding/following branches; all remain open.

## UI-SHADER-004: LLGLSLShader::bind()

Source: [llglslshader.cpp](../../../indra/llrender/llglslshader.cpp#L1049).
**1.** Assert-always program!=0, ALWAYS gGL.flush. If current GL program differs:
existing shader -> readProfileQuery; VBO unbind; glUseProgram; assign current name
and pointer; placeProfileQuery; setupClientArrays(attribute mask). Regardless name
change, uniformsDirty -> ShaderMgr.updateShaderUniforms(this), then false. Final
asserts current pointer nonnull and program name equal. Same program skips pointer
assignment/client arrays but still flushes and may update uniforms.
**2.** Native explicit pipeline/material/uniform snapshot, no global shader bind.
**3.** Ordered preparation establishes state boundaries; record immutable draws
without deferring current-color/resource reads. Reusing a pipeline must not eliminate
required CPU preparation updates. Check same/different program, pending geometry,
dirty uniforms, profiling and callback failure before dirty flag clear.
Outgoing: flush, VBO unbind, profiling queries, setupClientArrays, virtual manager
uniform update and GL program lifetime. Open.

## UI-SHADER-005: LLGLSLShader::unbind

Source: [llglslshader.cpp](../../../indra/llrender/llglslshader.cpp#L1101).
**1.** gGL.flush, VBO unbind, current shader profile read if pointer, glUseProgram0,
clear current name/pointer. No default shader selection. **2.** Native recording
has no implicit unbound material state; draw validity explicit. **3.** Finish prepared
batch while resources valid, then next draw supplies its own pipeline.
Check pending geometry, null current pointer, profiling and invalid next draw.
Outgoing: flush, VBO, profile read and callers.

## UI-SHADER-006: LLGLSLShader::uniform4f(index,x,y,z,w)

Source: [llglslshader.cpp](../../../indra/llrender/llglslshader.cpp#L1406).
**1.** Assert current shader pointer==this. If program0 no further action. index
outside uniform array warns once/asserts/returns. Nonnegative GL uniform location:
find cache by LOCATION, form LLVector4. Missing cache OR shouldChange(old,new) ->
glUniform4f then cache assignment. Negative location skips. No gGL.flush in body.
Cache mutation occurs after GL call; no local GL error/result handling.
**2.** Native per-draw uniform snapshots and upload ownership. **3.** Store quantized
color in immutable draw data; shared mutable uniform storage cannot stand in for
packet capture. Reference batching relies on callers' flush/bind boundaries, not
this uniform setter. Check missing/negative/out-of-range, same/changed vec,
shouldChange tolerance, pending vertices and shader switch.
Outgoing: shouldChange, mUniform producer, LLVector4/cached-value equality and callers'
ordering; these remain open. No claim that every direct uniform caller is safe.

## UI-SHADER-007: LLGLSLShader::getUniformLocation(index)

Source: [llglslshader.cpp](../../../indra/llrender/llglslshader.cpp#L1735).
**1.** Default-1. Program present: out-of-range warns once/returns-1; otherwise
return mUniform[index]. Program absent ->-1. **2.** Native reflected ABI validation,
not GL uniform locations. **3.** Validate required/optional fields at pipeline
creation. Callers must distinguish -1/0/positive explicitly.
Check absent program, valid location0, missing-1 and invalid index. Outgoing: map
population and all boolean/nonnegative caller tests.

## UI-SHADER-008: LLGLSLShader::getUniformLocation(name)

Source: [llglslshader.cpp](../../../indra/llrender/llglslshader.cpp#L1709).
**1.** Default-1. Program present + hashed-name map hit: under gDebugGL compare
cached location to glGetUniformLocation with error checks; mismatch fatal. Return
cached location. Missing program/name ->-1, no GL query except debug hit validation.
**2.** Native typed reflected resource interface. **3.** Compile/reflection ABI checks
replace runtime GL name lookup while preserving optional-uniform decisions.
Check map miss, debug mismatch and location0. Outgoing: hashed-string map/hash/name,
uniform registration and diagnostics.

## UI-SHADER-009: LLRender::syncMatrices

Source: [llrender.cpp](../../../indra/llrender/llrender.cpp#L1004).
**1.** Debug checks. Name table maps modelview, projection, texture0..3. Read shader
pointer; no shader -> skip body. Function-static cached MVP, inverse modelview,
normal, with hash sentinels0xffffffff. With shader:

- Modelview hash differs from shader's: current modelview; if global cached MVP
	modelview hash differs, compute inverse. Upload modelview and set shader hash.
	NORMAL_MATRIX location>-1: if normal cache hash differs, transpose cached inverse
	and update normal hash; select indices0,1,2,4,5,6,8,9,10 and upload mat3.
	INVERSE_MODELVIEW_MATRIX tests location as BOOL (0 false,-1 true), then calls
	matrix4 upload if true. mvp_done=true even if no MVP uniform. MVP location>-1:
	if either MVP source hash differs, cache projection*modelview and update both
	cache hashes; upload cached MVP.
- Projection hash differs from shader's: current projection; inverse-projection
	and identity uniforms also use BOOL location tests. If true compute/upload inverse
	projection or identity. Upload projection and set shader projection hash. If
	!mvp_done and MVP location>-1: condition compares cached MVP MODELVIEW hash against
	current PROJECTION hash, OR cached projection hash against projection hash. If
	true recompute projection*modelview and store actual modelview/projection hashes;
	then upload cached MVP. This comparison is recorded as written, not normalized
	to an assumed intended condition.
- For each texture matrix mode, if renderer hash differs from shader hash upload
	current stack matrix and assign shader hash.
- If shader features hasLighting OR calculatesLighting OR calculatesAtmospherics,
	call syncLightState. Otherwise no lighting sync here.

Static cached inverse has no separate identity/hash; its recomputation guard uses
MVP modelview hash, which only updates when MVP recomputed. Shader creation/hash
invalidation and per-thread renderer use remain essential unresolved dependencies.
**2.** CPU transforms and native explicit uniform snapshots, with coherent clip
depth/Y/packing conversion; optional lighting belongs to actual selected material.
**3.** Prefer view/draw versioned transform data and deterministic derived matrices
over global hash caches. Do not reproduce invalid-location or stale-cache hazards
as feature requirements; isolate any defined observable correction for review.
Checks: only modelview changes, only projection changes, shader switches, optional
uniform location0/-1, hash collisions/wrap/reset, singular matrices, texture0
transform and lighting-feature toggles. CPU and raster consumer evidence required.
Outgoing: shader hash initialization/mutators, matrix stack/hash setters, GLM inverse/
multiply/transpose/value_ptr, uniformMatrix3fv/4fv, getUniformLocation and
syncLightState. Full local body inspected, all named transitive edges remain open.

## UI-RETAIN-001: LLVertexBufferData default constructor

Source: [llvertexbuffer.h](../../../indra/llrender/llvertexbuffer.h#L62).
**1.** VBO null, mode/count/texture name0, projection/modelview/texture0 identities.
**2.** Native retained draw record must be valid or explicitly empty. **3.** Resource
leases/pipeline and draw ranges replace GL names; zero record is not drawable.
Check default draw rejects null VBO. Outgoing: LLPointer, GLM identity and consumers.

## UI-RETAIN-002: LLVertexBufferData parameter constructor

Source: [llvertexbuffer.h](../../../indra/llrender/llvertexbuffer.h#L71).
**1.** Retain VBO pointer via LLPointer; copy U8 mode/U32 count/raw texture name.
Assign mProjection=model_view and mModelView=projection (SWAPPED relative to names
and parameters), texture0 unchanged. No texture owner retained. No null/range checks.
**2.** Native immutable retained geometry plus explicit transform/resource snapshots.
**3.** Use typed view/draw fields and versioned image leases; raw name reuse is not
lifetime safety. Determine whether swapped fields affect reachable drawWithMatrix
before defining a correction; normal draw() ignores stored matrices.
Check parameter-to-field mapping, VBO lifetime, texture replacement, mode truncation
and consumers. Outgoing: refcount, matrix copy, draw/drawWithMatrix and texture owner.

## UI-RETAIN-003: LLVertexBufferData::draw

Source: [llvertexbuffer.cpp](../../../indra/llrender/llvertexbuffer.cpp#L648).
**1.** Null VBO assert/return. Nonzero texture name -> bindManual(TT_TEXTURE,name),
zero -> unbind (white-texture semantics). VBO setBuffer then drawArrays(mode,0,count).
No shader, blend, clip, matrices, sampler owner or color-uniform restoration here.
**2.** Native retained geometry replay under explicit current view/material policy.
**3.** Prepared record includes resource versions and every state required by its
consumer; caller-provided transforms must be intentional, not ambient leftovers.
Check current versus stored matrices, zero white texture, replaced texture name,
shader attribute mismatch and clipped replay. Outgoing: bindManual/unbind, VBO
setBuffer/drawArrays, caller material/clip/matrix state and texture lifetime.

## UI-RETAIN-004: LLVertexBufferData::drawWithMatrix

Source: [llvertexbuffer.cpp](../../../indra/llrender/llvertexbuffer.cpp#L610).
**1.** Null VBO assert/return. Bind raw texture or white as draw(). Switch MODELVIEW,
push/load stored modelview; PROJECTION push/load stored projection; TEXTURE0 push/load
stored texture0. VBO bind/draw. Pop texture0, switch projection/pop, switch modelview/
pop. Leaves current matrix mode MODELVIEW, not prior mode. No exception unwinding.
**2.** Native retained draw with captured transform snapshot. **3.** Separate replay
under current view from replay under captured view; do not use one ambiguous record
with swapped matrix names. Reachability and intended reference output remain open.
Check nonidentity distinct matrices, prior mode, stack limits and null VBO.
Outgoing: texture binding, matrix stack/load/hash, VBO, ctor field mapping and callers.

## UI-FONTVB-001: LLFontVertexBuffer constructor

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L36),
[members](../../../indra/llrender/llfontvertexbuffer.h#L105).
**1.** Empty body; member list constructs empty; chars/offset/limits/x/y/rightX0,
font null, alignment LEFT/BASELINE, style NORMAL, shadow NO_SHADOW, scaleX/Y1,
DPI0, resolution/cache generations0; color and origin use type defaults.
**2.** CPU retained text-preparation cache. **3.** Cache identity should explicitly
include content/layout/font versions and keep GPU versions separately leased.
Check first render regenerates due empty list and member defaults.
Outgoing: container/color/origin constructors and field lifetime.

## UI-FONTVB-002: LLFontVertexBuffer destructor

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L40).
**1.** reset(), then members destruct. **2.** CPU cache release with GPU lifetime
separate. **3.** Retire native resources by completed use, not cache destruction.
Check last VBO owner/pending submission. Outgoing: reset/list/record destructors.

## UI-FONTVB-003: LLFontVertexBuffer::reset

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L45).
**1.** Clear buffer list only. Does not reset chars, rightX or last-input fields.
**2.** CPU geometry invalidation. **3.** Explicit invalid cache state with old GPU
versions still completion-owned. Check next empty-text render, list ownership and
callers responsible for text changes. Outgoing: records/VBO deletion and all reset
call sites. A raw text pointer/content change alone is not detected by this method.

## UI-FONTVB-004: LLFontVertexBuffer::render(integer rect)

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L52).
**1.** Convert each LLRect edge to float, delegate rect-float overload. Input
max_pixels argument is NOT forwarded/used; rect-float path uses rect width instead.
**2.** CPU text extent/layout input. **3.** Preserve actual overload policy, not
signature-based expectation. Check max_pixels differs from rect width and large
integer-to-float precision. Outgoing: rect construction and next overload.

## UI-FONTVB-005: LLFontVertexBuffer::render(float rect)

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L70).
**1.** x=left. y TOP->top, VCENTER->centerY, BASELINE/BOTTOM/default->bottom.
Delegate point overload with max_pixels=S32(rect.width), supplied other inputs.
**2.** CPU text alignment reference point and width budget. **3.** Preserve fractional
rect-to-integer budget truncation and invalid enum fallback. Check each alignment,
fractional/negative widths and subsequent font alignment adjustment.
Outgoing: rect center/width methods, numeric cast and point overload.

## UI-FONTVB-006: LLFontVertexBuffer::render(point)

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L107).
**1.** Font display disabled -> return S32(text.length), no rightX/cache mutation.
Buffer collection disabled -> directly return fontp->render(all args). Otherwise:
empty list -> genBuffers. Nonempty list compares x,y,font pointer,color,halign,valign,
begin offset,max chars,max pixels,style,shadow,font global scaleX/Y,vertical/horizontal
DPI,integer current origin,resolution generation,font cache generation; any difference
-> genBuffers. Else renderBuffers, and if rightX pointer set it to cached rightX.
Return mChars. No font null guard on enabled path. No text content, text pointer,
use_ellipses,use_color,current depth,shader,clip or matrices in local cache key.
Header explicitly assigns text-change reset responsibility to caller. No local
equivalent statement closes other omitted-state caller obligations.
**2.** CPU cached glyph geometry/layout result with current view depth/clip policy.
**3.** Native cache should use explicit content and font/layout versions; replay
resources require all-use completion. Do not reuse reference incomplete keys as
native correctness proof. Preserve defined display-disabled return policy separately.
Check each key input independently, text change+caller reset, ellipses/color-mode
changes, rightX newly requested after unrequested generation, depth/matrix replay,
empty output and font cache generation changes during rasterization.
Outgoing: font render, genBuffers/replay, font cache generation accessor, globals
and their writers, actual caller reset policy/retained-resource lifetime.

## UI-FONTVB-007: LLFontVertexBuffer::genBuffers

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L166).
**1.** Clear old list. Cache font generation BEFORE render because rendering may
add glyphs/change atlas dimensions. beginList(&list); mChars=font.render(args);
endList. Copy font/input/style/shadow, global scale/DPI/origin/resolution fields
AFTER render. Only if rightX pointer present copy its value to cached rightX;
otherwise prior cached value retained. Capturing list also draws immediately via
LLRender::flush; generation is not merely CPU geometry creation. No exception-safe
endList/list rollback or retention of last font raw pointer here.
**2.** Native CPU layout/raster request, GPU upload publication, immutable text
draw capture and execution are distinct. **3.** Version font/atlas before/after
preparation and retry/invalidate as needed without recording mutable atlas geometry;
retain old native versions through all consumers. Right edge is explicit layout
output independent of whether one caller requested it.
Check atlas growth during render, exceptions, no glyph output, rightX optional and
state mutations during font render. Outgoing: font full render, begin/endList,
record owners, cache-generation semantics and callers.

## UI-FONTVB-008: LLFontVertexBuffer::renderBuffers

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L218).
**1.** Flush pending vertices; unit0 enable TT_TEXTURE; push UI matrix, load UI
identity. gGL.translatef(0,0,font current depth) modifies CURRENT regular matrix mode,
not UI translation; set BT_ALPHA. Iterate records in list order calling draw(),
not drawWithMatrix(). Pop UI matrix. No regular matrix push/pop, blend restore,
shader bind or explicit sampler restore in this body. Caller/font code must supply
expected regular matrix mode and lifetime; repeated depth addition cannot be assumed
restored by popUIMatrix.
**2.** Native retained text under explicit view/depth/blend policy. **3.** Resolve
current font depth into draw transform without mutating shared camera matrices;
resource/clip/color snapshots remain explicit. Do not copy potential state leak as
native design. Check nonzero depth, repeated replay, incoming matrix mode, pending
geometry and changed outer shader/clip. Outgoing: enable/flush, UI stack, translatef,
blend, record draw, caller matrix scope and font-depth writers.

## UI-FONTDRAW-001: LLFontGL::render(point,full arguments)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L156).
**1.** Inputs are LLWString/source-code-unit offset, local x/y, color/alignment/style/
shadow, max characters/pixels, optional rightX, ellipsis and color-glyph flags.
Display disabled -> S32(full string length), no rightX update. Empty string ->0.
Otherwise enable texture unit0; max_pixels==S32_MAX preserves sentinel, else
scaled_max_pixels=llceil(float(max_pixels)*sScaleX). Added style=(requested style OR
descriptor style) AND NOT FreeType style. For shadow!=NO_SHADOW get HSL luminance,
strength=clamp_rescale(lum,.35,.6,0,1); lum<.35 disables shadow.

State and positioning, in source order:

- Push UI matrix/load UI identity; origin=(floor(font integer origin X*scaleX),
  floor(originY*scaleY)). Regular gGL.translatef(0,0,sCurDepth), with no regular
  matrix push/pop in this body. UI pop does not undo regular depth translation.
- max_chars==-1 -> max_chars=length=stringLength-begin_offset; otherwise
  length=min(stringLength-begin_offset,max_chars). No begin_offset bounds check.
- Set BT_ALPHA; curX=x*scaleX+originX, curY=y*scaleY+originY.
- TOP subtract ceil(FreeType ascender); BOTTOM add ceil(descender); VCENTER subtract
  ceil((ceil(ascender)-ceil(descender))/2); BASELINE/default no vertical adjustment.
- RIGHT subtract min(scaled_max_pixels,ll_round(getWidthF32(subrange,length)*scaleX));
  HCENTER subtract same integer expression DIVIDED BY2 as integer; LEFT/default none.
- Set renderX/Y=current positions, startX=ll_round(curX). Fetch font bitmap cache;
  compute inverse width/height ONCE before later lazy glyph queries can grow atlas.
- If use_ellipses: stringWidth=ll_round(getWidthF32(subrange,max_chars)*scaleX).
  If >scaled_max_pixels, construct four-dot string via UTF8 conversion, subtract
  ll_round(getWidthF32(four dots)) WITHOUT explicit *scaleX in this expression,
  clamp budget>=0 and set draw_ellipses. Four dots reserve padding; three are drawn.
- Setup nextGlyph=null, thread_local position/UV/color arrays180 entries (30 quads),
  textColor=LLColor4U(float color), emojiColor=(255,255,255,textColor.alpha).
  bitmap=(Grayscale,-1), glyphCount0, lastChar=wstr[begin_offset] even if length<=0.

For each index from begin_offset to begin_offset+length exclusive:

1. Read character; use prefetched nextGlyph then clear it, otherwise lazy
	FreeType.getGlyphInfo(character,Grayscale if !use_color else Color). Null logs
	fatal then breaks. Capture returned bitmap entry.
2. If bitmap entry differs OR character differs from lastChar, emit any queued
	glyph quads via TRIANGLES/pretransformed(position,UV,color,count*6)/end and clear
	local glyphCount. Set bitmap entry, fetch cache GL page, bind it, set lastChar.
	Thus different characters trigger this branch even on the same page.
3. If startX+scaledMaxPixels < curX+glyph.xBearing+glyph.width, break before drawing
	character; equal edge fits. Texture switching/lazy glyph work already occurred.
4. Tabular centering only when isTabnum AND fontWeight>0 AND glyph char '0'..'9'
	AND maxDigitWidth>0: xOffset=(maxDigitWidth-glyph.rawXAdvance)*.5; otherwise0.
5. UV left=xBitmap*invWidth, right=(xBitmap+width)*invWidth,
	top=(yBitmap+height+PAD_UVY)*invHeight, bottom=(yBitmap-PAD_UVY)*invHeight.
	Screen left=ll_round(renderX+xBearing+xOffset), right=left+width;
	top=ll_round(renderY+yBearing), bottom=top-height. No center-based glyph snapping.
6. If glyphCount>=30 emit current batch and reset count BEFORE drawGlyph. Use
	textColor for grayscale bitmap, emojiColor for all other bitmap types. drawGlyph
	receives added style, possibly modified shadow and shadow strength; it can append
	multiple quads per character and must respect buffer capacity.
7. Increment charsDrawn; curX+=FreeType.getXAdvance(glyph); curY+=glyph.yAdvance.
	Read wstr[index+1] even beyond requested subrange (at full string end this uses
	string terminator access). If nonzero and <LAST_CHAR_FULL, lazy fetch next glyph
	with same type request and add getXKerning(current,next) to curX.
8. ll_round curX after kerning, DO NOT round curY; update renderX/renderY.

After loop always begin TRIANGLES/emit glyphCount*6/end, including count0. If rightX
provided, set (curX-originX)/scaleX. UNDERLINE in added style: floor descender,
unbind TT_TEXTURE, LINES from(startX,curY-descender) to(curX,curY-descender), end;
no explicit color change, so current GL vertex color state remains a dependency.
If draw_ellipses, initialize static three-dot wide string and recursively render at
x=(curX-originX)/scaleX,y=original y, color, LEFT/original valign, ADDED style,
modified shadow, maxChars=S32_MAX, ORIGINAL max_pixels, same rightX, ellipses=false,
same use_color. Recursive return ignored; it may overwrite rightX but charsDrawn
returned excludes ellipsis characters. Finally pop UI matrix, return charsDrawn.
No exception restoration, null FreeType/cache checks, finite/scale-zero checks or
offset validation local to this body.

**2.** Native CPU text layout, source indices, fallback/raster requests and glyph
geometry preparation; immutable atlas versions publish only when upload-ready. Draw
execution consumes explicit transform/clip/blend/material and separate grayscale
coverage versus color glyph encoding. Width queries remain CPU work but reference
queries may rasterize/allocate; they cannot be presumed neutral.
**3.** Prefer a coherent CPU font/layout service with content+font generation output,
versioned glyph pages and ordered native glyph/shadow/underline contributions.
Alternatives of translating this loop's GL calls or reusing a GL owner are forbidden;
replacing layout with a different shaper without matching metrics/source indices is
an unapproved behavior change. Atlas growth, depth scopes and optional rightX need
explicit producer/consumer contracts; undefined inputs are not compatibility targets.
Checks: display-off/empty, offset/subrange and -1 count, exact pixel edge, all alignments,
odd horizontal-center widths, scale!=1 ellipsis, dark/bright shadows, glyph-type
fallback and emoji alpha, tabular digits, kerning past subrange, atlas growth mid-loop,
each batch boundary, underline color, recursive rightX/char count and depth state.
No numeric/image tolerance or runtime closure is established here.
Outgoing: every font/bitmap/getWidthF32/drawGlyph helper, LLColor4U conversion/HSL,
UTF8 conversion/string access, ll_round/ceil/floor/clamp_rescale, texture binding,
GL batches, regular/UI matrices, style constants/PAD_UVY, globals and all caller
offset/lifetime/matrix preconditions. Full local body inspected; edges remain OPEN.

## UI-FONTDRAW-002: LLFontGL::render(integer rect)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L123).
**1.** Float-convert all rectangle edges and delegate rect-float overload with all
other arguments. **2.** CPU layout input conversion. **3.** Retain input precision
contract; native viewport pixels are not automatically these local units.
Check large integer precision and rect orientation. Outgoing: rect/next overload.

## UI-FONTDRAW-003: LLFontGL::render(float rect)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L130).
**1.** x=left; TOP y=top, VCENTER y=centerY, BASELINE/BOTTOM/default y=bottom.
Call full point render with maxPixels=S32(rect.width) and remaining arguments.
**2.** CPU alignment reference/budget. **3.** Preserve later font metric alignment,
not treat rect top as final glyph top. Check fractional width/center and invalid
alignment. Outgoing: rect accessors, cast and full render.

## UI-FONTDRAW-004: LLFontGL::render(short point overload)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L463).
**1.** Delegate full point with LEFT,BASELINE,NORMAL,NO_SHADOW and header defaults
for limits/rightX/flags. **2.** CPU default text policy. **3.** Resolve header defaults
explicitly before caller migration. Check default limits and glyph color mode.
Outgoing: full render and signature/default arguments (not closed by body alone).

## UI-FONTDRAW-005: LLFontGL::renderUTF8(full point)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L468).
**1.** utf8str_to_wstring(text), then full render with SAME begin_offset and limits;
does not translate a UTF8 byte offset. **2.** CPU text decoding/source-index contract.
**3.** Native APIs need explicit source index domain; preserve malformed-input policy
after converter audit, not assume UTF8 byte positions match glyph/code-point indices.
Check multibyte input, offset and invalid sequences. Outgoing: conversion/full render.

## UI-FONTDRAW-006: LLFontGL::renderUTF8(short integer point)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L473).
**1.** Convert x/y to float, delegate UTF8 with LEFT/BASELINE/NORMAL/NO_SHADOW and
remaining defaults. **2.** CPU default positioning/decoding. **3.** Keep default and
index semantics explicit. Check integer precision and default color glyph mode.
Outgoing: UTF8 overload/header defaults.

## UI-FONTDRAW-007: LLFontGL::renderUTF8(integer point with style)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L478).
**1.** Float-convert x/y, delegate with supplied alignment/style/shadow and defaults
for remaining parameters. **2.** CPU typed text request. **3.** No special integer
pixel snap beyond delegated implementation. Check style/default limits.
Outgoing: full UTF8/header defaults.

## UI-FONTDRAW-008: LLFontGL::getCacheGeneration

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L117).
**1.** FreeType->getFontBitmapCache then cache->getCacheGeneration, no null guard.
**2.** CPU font/atlas version query. **3.** Native geometry must identify actual page
version/metrics changes, not reuse a counter without defining every mutation.
Check cache reset/growth/wrap and missing owner. Outgoing: both accessors and all
generation writers/lifetime. Caller may query before or after rasterization.

## UI-SHADER-010: shouldChange

Source: [llglslshader.cpp](../../../indra/llrender/llglslshader.cpp#L88).
**1.** Return LLVector4 inequality. No local tolerance/rescale comparison.
**2.** CPU uniform-cache identity. **3.** Native immutable uniform snapshots can
reuse equal values after exact comparison contract; not infer fuzzy comparison.
Check component differences, NaN and signed zero via vector operator definition.
Outgoing: LLVector4::operator!= and cached-value consumers.

## UI-FONTDRAW-009: LLFontGL::renderTriangle

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L1425).
**1.** Write six vertices, paired UV corners and identical color to supplied arrays:
RT,LT,LB,RT,LB,RB. All position z0. slant_amt argument is NOT used; no body access
to font owner, GL state, allocator or global cache. Method constness alone was not
the evidence for this local purity; complete body was inspected. Caller-provided
array size/validity and math/value setters remain preconditions.
**2.** CPU neutral glyph quad preparation. **3.** Prefer a small pure geometry
producer writing a native vertex span over creating a Vulkan counterpart of every
GL method. Preserve this exact corner order and attributes; no synthetic italic
skew can be inferred from unused slant_amt. Existing actual italic face shapes are
separate font selection/rasterization behavior.
Check six positions/UV/colors, nonzero slant leaves output unchanged, valid output
capacity and input aliasing. Outgoing: LLVector4a::set, LLVector2::set, LLColor4U
assignment, LLRectf field data and pointer preconditions. Local body CPU-only;
transitive value/helper contract still open, no native implementation yet.

## UI-FONTDRAW-010: LLFontGL::drawGlyph

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L1461).
**1.** slantOffset=ITALIC ? -ascender*.2 :0; passes it to renderTriangle (unused by
that callee). BOLD branch wins over every shadow mode: two passes, screen rect copy
translated horizontally by pass*BOLD_OFFSET; emit with foreground color, increment
glyphCount each. Else SOFT shadow: shadowColor from global sShadowColor, overwrite
alpha with U8(foregroundAlpha*strength*DROP_SHADOW_SOFT_STRENGTH), then emit five
rect copies offset in order(-1,-1),(1,-1),(1,1),(-1,1),(0,-2), increment each;
emit unshifted foreground/increment. Else DROP_SHADOW: shadowColor global, alpha=
U8(foregroundAlpha*strength), emit offset(1,-1), then foreground, increment each.
Else one unshifted foreground/increment. For each emission pass array pointers at
glyphCount*6 before increment. No bounds/finite checks and no geometry batch flush
inside; caller must reserve enough entries for branch's whole character.
**2.** CPU glyph decoration geometry and quantized colors, native ordered draw
contributions to same coverage image. **3.** Prepare coherent character batches
with checked capacity, retaining exact offset/alpha/painter order. Do not replace
soft shadow with a blur filter or combine bold+shadow without approved change.
Checks: BOLD+every shadow selects2 quads; soft6, hard2, normal1; ITALIC alone no
synthetic skew due writer; alpha truncation/global shadow RGB and type conversion;
mixed branch batch boundaries and capacity. Source's 30-quad batching claims need
validation with actual uniform style/shadow per render, not arbitrary mixtures.
Outgoing: ascender getter, style/offset/strength constants, LLRectf copy/translate,
global shadow color conversion, renderTriangle and caller capacity/lifetime.

## UI-METRICS-001: LLFontFreetype::getXAdvance(character)

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L343).
**1.** Null face ->0. Lazy getGlyphInfo(character,Unspecified). Nonnull glyph:
tabnum AND input char digit AND maxDigitWidth>0 -> maxDigitWidth (NO weight test),
else raw advance. Null glyph: map.find(char0), if found return that glyph raw
advance; otherwise bitmapCache.maxCharWidth as float. No type choice on glyph0 map
fallback and no cache/null-entry guard. **2.** CPU glyph advance/fallback metrics.
**3.** Audit actual callers and overload distinction; neutral font metrics service
must not silently enforce the pointer overload's extra weight condition everywhere.
Check null face, digit/weight variants, missing glyph fallback and cache population.
Outgoing: lazy glyph lookup, map/value lifetime, max-width accessor/initialization.

## UI-METRICS-002: LLFontFreetype::getXAdvance(glyph)

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L373).
**1.** Null face ->0. tabnum AND weight>0 AND glyph char digit AND maxDigitWidth>0
-> maxDigitWidth, otherwise glyph.rawXAdvance. No glyph null guard with live face.
**2.** CPU layout advance. **3.** Preserve typed glyph/font relationship and caller
fallback policy, not an unconditional max-digit substitution.
Check weight0/positive, tabnum, digits and null-glyph preconditions. Outgoing: glyph
metadata producer, face lifetime, maxDigitWidth/weight/tabnum writers.

## UI-METRICS-003: LLFontFreetype::getXKerning(characters)

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L389).
**1.** Null face ->0; otherwise lazy getGlyphInfo left then right with Unspecified;
delegate pointer overload, including null results. **2.** CPU pair metrics with
possible glyph rasterization in reference. **3.** Native metrics must separate lazy
CPU glyph acquisition from native GPU publication, preserving lookup order/fallback.
Check missing glyphs and cache changes during pair lookup. Outgoing: getGlyphInfo,
pointer kerning, face owner.

## UI-METRICS-004: LLFontFreetype::getXKerning(glyphs)

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L402).
**1.** Null face ->0. Left/right indices start0. For nonnull left: tabnum AND weight>0
AND digit ->0 immediately; otherwise index=left glyph index. Same for nonnull right.
FT_Vector delta (not locally initialized); llverify(!FT_Get_Kerning(current face,
indices,FT_KERNING_UNFITTED,&delta)). If both infos nonnull, deltaDiff=left.rsbDelta-
right.lsbDelta; >32 -> correction-1; <-31 ->+1; otherwise0. Return delta.x/64 plus
correction. Always uses current mFTFace, not a face selected from each fallback glyph.
No local FT error return branch apart from llverify policy; failure output must be
checked against actual API contract before describing determinism.
**2.** CPU kerning and hinter side-bearing correction. **3.** Preserve FreeType
metric mode and strict thresholds under audited face/glyph identity; changing to
different shaping/kerning requires explicit equivalent consumer evidence.
Check deltaDiff -32/-31/32/33, null glyph indices0, tabular digits with max width0,
fallback glyph indices and FT failure. Outgoing: FT_Get_Kerning version/face contract,
llverify, glyph side-bearing producer and current face ownership.

## UI-METRICS-005: LLFontFreetype::getAscenderHeight

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L333).
**1.** Return stored mAscender. **2.** CPU loaded-face metric. **3.** Retain explicit
metric units/generation from loadFace; no Vulkan counterpart necessary.
Check loaded/reset value. Outgoing: constructor/loadFace/metric writers.

## UI-METRICS-006: LLFontFreetype::getDescenderHeight

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L338).
**1.** Return mDescender. **2.** CPU metric. **3.** Preserve sign/units from producer,
not assume raw FT negative descender. Check loadFace-derived value and vertical
alignment consumers. Outgoing: metric initialization/reset writers.

## UI-METRICS-007: LLFontFreetype::getLineHeight

Source: [llfontfreetype.cpp](../../../indra/llrender/llfontfreetype.cpp#L328).
**1.** Return mLineHeight. **2.** CPU metric. **3.** Keep distinct from LLFontGL's
ceil(ascender/scale)+ceil(descender/scale) line height. Check difference from those
derived sums for real fonts. Outgoing: loadFace/field writers and callers.

## UI-METRICS-008: LLFontGL::getAscenderHeight

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L484).
**1.** FreeType ascender/sScaleY, no null/zero guard. **2.** CPU local-pixel metric.
**3.** Native font metrics expose explicit scaled/unscaled units, not reuse raw
device metric as local layout. Check DPI/UI scale and zero precondition.
Outgoing: metric getter and scale writers.

## UI-METRICS-009: LLFontGL::getDescenderHeight

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L489).
**1.** FreeType descender/sScaleY. **2.** CPU local-pixel metric. **3.** Same coherent
scale contract as ascender; negative/zero scales remain invalid-input obligations.
Check scale changes. Outgoing: getter/scale lifetime.

## UI-METRICS-010: LLFontGL::getLineHeight

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L494).
**1.** ceil(ascender/sScaleY)+ceil(descender/sScaleY); does not call FreeType line
height getter. **2.** CPU rounded line layout. **3.** Preserve separate rounding,
not ceil of total or face line gap. Check fractional metrics and scale.
Outgoing: metric getters, llceil, scale and line-height consumers.

## UI-METRICS-011: LLFontGL::getWidthF32(wchars,offset,max,no_padding)

Source: [llfontgl.cpp](../../../indra/llrender/llfontgl.cpp#L540).
**1.** curX0; maxIndex=begin_offset+max_chars (signed addition with no overflow guard).
nextGlyph=null, padding0. Loop while index<maxIndex and wchars[index]!=0. Use
prefetched glyph or lazy getGlyphInfo(char,Unspecified); no null-result check before
pointer advance/metadata reads. advance=getXAdvance(glyph). If padding enabled,
padding=max(0,oldPadding-advance,float(width+xBearing)-advance). curX+=advance.
Always read nextChar=wchars[index+1]. Only if next index<begin_offset+max_chars AND
nextChar nonzero AND <LAST_CHAR_FULL: getGlyphInfo next Unspecified and add kerning.
Round curX after every iteration. After loop add padding if enabled. Return
curX/sScaleX. Unlike render, kerning is gated by requested subrange. Glyph queries
may create atlas/GL resources even though this is a width query. No display-font
gate, max_chars==-1 special case, pointer/offset/zero-scale guard here.
**2.** CPU width/layout metrics with exact advance, padding and per-character rounding;
native GPU glyph publication must be independent of measurement.
**3.** Audited CPU font metric service with explicit text bounds and font versions;
preserve padding and rounding contract, define invalid-input handling rather than
emulate signed overflow. Measurement and render equations must be compared, not
assumed identical from name. Check no_padding, overhanging glyph, subrange kerning,
empty/terminator, max sentinel with nonzero offset, zero/negative max, scale and
fallback glyph cache effects. Outgoing: getGlyphInfo/advance/kerning, rounding/max,
glyph metadata, callers' actual text bounds and font/scale lifetime.

## UI-WIDTHCACHE-001: LLFontWidthBuffer::reset

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L253).
**1.** Font null, offset/maxChars0, noPadding=false, width=-1, scaleX/Y1, DPI0,
resolution/cache generations0. Unlike vertex-buffer reset, clears all cached key
fields too. **2.** CPU width-cache invalidation. **3.** Native layout cache content
version must be explicit; do not rely only on raw font pointer identity.
Check invalid sentinel and reset callers after text edits. Outgoing: cache consumers.

## UI-WIDTHCACHE-002: LLFontWidthBuffer::getWidth

Source: [llfontvertexbuffer.cpp](../../../indra/llrender/llfontvertexbuffer.cpp#L268).
**1.** Null font OR text pointer ->0, no cache mutation. Collection disabled ->
direct font.getWidthF32. Otherwise recalc if width<0 OR font,offset,maxChars,
noPadding,global scaleX/Y,DPI,resolution generation,font cache generation differ.
Recalc: width=font.getWidthF32, then cache keys/global values and font generation
AFTER measurement; else return old width. No text pointer/content key. Header
requires caller reset after text changes. Width<0 result causes next recalc.
**2.** CPU versioned layout-result caching. **3.** Prefer text-content generation plus
font/layout identity; explicit metadata version can avoid invalidating widths for
unrelated GPU atlas changes once equivalent metric behavior is proved.
Check each key, text reset, failed/negative measurement, generation changed during
measurement and null call followed by cache reuse. Outgoing: font measurement,
generation/globals and all caller invalidation sites.

## Evidence boundary

The restored RelWithDebInfo viewer was rebuilt successfully from this branch's
unchanged implementation sources; log: logs/native-ui-baseline-build.log. This
refreshes the executable source provenance only. No UI runtime or pixel comparison
is established by the build, and no native production code has been added here.