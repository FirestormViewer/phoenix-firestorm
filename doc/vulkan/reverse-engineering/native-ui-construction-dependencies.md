# UI construction and registry dependencies

Reference: 3abd661f498329babaf87b49ffab910fcb5f0e6c; implementation checkpoint
90af5a7220f1fec28e3c909c051b6ee7e062f10b. Investigation covers the compiled Windows
RelWithDebInfo source tree, with platform/configuration alternatives retained as
explicit obligations. Status throughout: local-body-inspected, edges-open, not
implemented, not runtime/parity validated. NV-00/NV-03/NV-12/NV-17 govern this work.

The [font/default root](native-ui-dependencies.md) already establishes that parameter
construction is not automatically GPU-neutral. This document records factory
decisions independently of the rendering path. None of the candidate CPU sharing
choices below authorizes reuse until its outgoing constructor/helper targets close.

## UI-FACTORY-001: getDefaultParams<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L120).
**1.** Return const reference to prototype in factory-owned heterogeneous map,
obtaining ParamDefaults<T::Params,0>; obtain may create an entry. No value copy.
**2.** Native construction consumes stable typed defaults; resolution is CPU work
but constructors can allocate GL fonts in reference. **3.** Prefer audited neutral
semantic defaults cached under CPU-service lifetime over a GL parameter facade;
factory destruction/reconfiguration must define reference invalidation.
Check repeated/specialized first access and reference lifetime. Outgoing: instance,
LLHeteroMap::obtain, ParamDefaults constructor/get and parameter constructors.

## UI-FACTORY-002: ParamDefaults<PARAM_BLOCK,DUMMY>::ParamDefaults

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L290).
**1.** mPrototype is default-constructed before body. Lookup registry tag by parameter
type. If found, default-construct another PARAM_BLOCK, loadWidgetTemplate into it,
fill prototype from it. Then cast prototype to base_block_t and fill from recursively
obtained ParamDefaults<base_block_t,DUMMY>. Most-specific template is therefore
offered before base defaults. Body does not clear provided bits or catch failures.
**2.** CPU default precedence plus backend-neutral resource identities. **3.** Prefer
preserving audited fill semantics in CPU types to separately reimplementing schema
inheritance; do not cache native device objects in schema defaults.
Check explicit versus absent values at each inheritance level, missing template,
base tag collisions and construction-time font lookup. Outgoing: each parameter
constructor, LLWidgetNameRegistry, loadWidgetTemplate, fillFrom, obtain and base type.

## UI-FACTORY-003: ParamDefaults<BaseBlock,DUMMY>::ParamDefaults

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L308).
**1.** Empty body terminates template recursion; member mBaseBlock still constructs.
**2.** CPU recursion terminator. **3.** Reuse only after BaseBlock construction and
destruction audit; an empty body does not establish no side effects.
Check exactly one base termination and member lifetime. Outgoing: BaseBlock ctor/dtor.

## UI-FACTORY-004: create<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L135).
**1.** Mutates caller params by fillFrom cached defaults, calls createWidgetImpl with
params/parent. If returned pointer nonnull invokes postBuild and ignores bool. Returns
pointer. No layout transformation or child XML traversal in this body.
**2.** Native CPU construction/action registration with explicit owner. **3.** Preserve
direct-construction policy separately from XML-construction failure policy; do not
invent universal postBuild rejection. Check false postBuild remains returned, params
provided flags/default merge and parent insertion order. Outgoing: defaults cache,
fillFrom, createWidgetImpl, every T postBuild/constructor and failure path.

## UI-FACTORY-005: createWidgetImpl<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L210).
**1.** validateBlock false warns with filename/type but still constructs new T(params).
Call widget->initFromParams. If parent nonnull, tab group is params value only if
isProvided(), otherwise S32_MAX; setCtrlParent. Return pointer. No local new failure
handling, deletion on init exception, or parenting-result test.
**2.** CPU model/layout/control/action initialization independent of GPU owners.
**3.** Audited native construction transaction with explicit parent ownership and
failure rollback; preserve defined validation policy rather than rejecting formerly
accepted declarations implicitly. Check invalid block, init failure, provided tab0
versus unspecified, and constructor-time callbacks. Outgoing: validateBlock,
T constructor/initFromParams, tab parameter wrappers, setCtrlParent, diagnostics.

## UI-FACTORY-006: defaultBuilder<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L236).
**1.** Copy cached defaults into params; construct LLXUIParser/readXUI with current
filename. If output node: copy params, T::setupParamsForExport(output,parent), copy
name to output, writeXUI against defaults/default_parse_rules. Then set from_xui=true,
T::applyXUILayout(params,parent), createWidgetImpl, createChildren using
T::child_registry_t::instance(). Finally widget nonnull and postBuild false -> delete
widget and return null. Child creation precedes postBuild; no local child success
aggregate. Export occurs before layout transformation. Exceptions have no local
transaction rollback.
**2.** Native CPU declaration parsing, layout and post-construction semantics.
**3.** Prefer a typed neutral declaration/layout pipeline with explicit prepared
results to invoking reference GL-owning controls. Keep per-type builder overrides
and failure rules explicit. Check export versus runtime rects, child postBuild before
parent postBuild, false parent postBuild destruction, parse failures and partial trees.
Outgoing: all T Params methods, parser read/write/lifetime, default_parse_rules,
layout/export static targets, child registry, construction/children/postBuild/dtor.

## UI-FACTORY-007: createFromFile<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L153).
**1.** Set widget=null; push filename. getLayeredXMLNode failure warns and jumps to
pop/return null. Else createFromXML(root,parent,filename,registry,null output). Nonnull
view dynamic_cast<T*>; cast failure warns, deleteView(view), nulls local view. Always
normal-flow pop then return typed pointer. Exception does not run that pop locally.
**2.** Native CPU file/declaration loading and typed root ownership. **3.** Scoped
diagnostic provenance and ownership, with preserved wrong-type destruction. Check
missing file, wrong type, nested loads, thrown callback and file stack recovery.
Outgoing: push/pop, layered XML, createFromXML, deleteView and T type hierarchy.

## UI-FACTORY-008: getDefaultWidget<T>

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L189).
**1.** Value-initialize T::Params, assign name from string_view converted to string,
return create<T>(params) with default parent null. **2.** CPU fallback-widget
construction. **3.** Audit every fallback type and lifetime; a missing declaration
must not conceal a native GL-resource construction. Check name ownership after input
expires and false postBuild route. Outgoing: T::Params, name assignment and create.

## UI-FACTORY-009: LLChildRegistry<DERIVED>::Register<T>::Register

Source: [lluictrlfactory.h](../../../indra/llui/lluictrlfactory.h#L313).
**1.** Base StaticRegistrar gets tag and supplied callback, or defaultBuilder<T> if
callback null. In body, if factory does not exist create it. registerWidget with
T type, T::Params type and tag. Schema registry block is commented out. Static
registration can instantiate factory before application entry.
**2.** Native CPU registry of audited constructors, not a shared GL draw callback
registry. **3.** Separate constructor targets by lifecycle while sharing audited
declaration names; global registration must itself remain backend-independent.
Check static order, custom builder selection, duplicate tags and factory recreation.
Outgoing: StaticRegistrar, singleton lifecycle, registerWidget and each T/custom
callback. Concrete registrations/conditional compilation inventory remains OPEN.

## UI-FACTORY-010: LLUICtrlFactory constructor

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L77).
**1.** Initialize dummy panel null; other members default-construct. No dummy panel
allocation in body. **2.** CPU service ownership. **3.** Prefer lifecycle-owned
factory to implicit graphics construction during static registration; determine
whether audited base/member construction permits sharing. Check before settings,
font service or window initialization. Outgoing: LLSimpleton/base and member ctors.

## UI-FACTORY-011: LLUICtrlFactory destructor

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L82).
**1.** Empty active body; dummy panel deliberately not deleted. Members still
destruct, including parameter cache; referenced UI/font values need ownership audit.
**2.** Native lifecycle teardown with callbacks disconnected before CPU/GPU owner
destruction. **3.** No process-lifetime leak as substitute for explicit ownership;
preserve externally visible teardown ordering while correcting only reviewed leaks.
Check partial init, normal close, cache references and dummy children. Outgoing:
member/base destructors, heterogeneous cached parameter destructors and dummy owner.

## UI-FACTORY-012: loadWidgetTemplate

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L89).
**1.** Build widgets/<tag>.xml with directory helper; findSkinnedFilenames(XUI,path).
Empty list returns. Take front as base_filename; empty front skips body. Else push
base filename directly onto file stack; layered XML success -> local parser reads
into supplied BaseBlock; failure warns. Pop afterward, no exception scope. This body
does not itself parse one file per path or define layering merge semantics.
**2.** CPU skin/localization/default resolution. **3.** Share audited path/XML/default
semantics with neutral parameters, not copy a single skin as native hardcoded style.
Check empty paths/front, layer conflict, parser failure and exception file stack.
Outgoing: directory helpers, LLXMLNode::getLayeredXMLNode, parser, BaseBlock targets.

## UI-FACTORY-013: createChildren

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L124).
**1.** Null node returns. Traverse firstChild/nextSibling in XML order. For each,
create empty output child if exporting. Call createFromXML(child,viewp,empty filename,
provided registry,outputChild). Failure -> lookup tag in default registry: known tag
warns invalid child of this parent, unknown warns could not create; both include
name attributes and line number. Continue siblings on failure. If output child has
no children, attributes or value, remove it regardless of creation result. No local
aggregate failure return, rollback or viewp null guard.
**2.** CPU hierarchical construction with parent-specific legal children and order.
**3.** Preserve registry eligibility and partial-tree policy in native preparation;
an XML tag inventory alone cannot choose the correct child constructor.
Check invalid registered child versus unknown, failed postBuild, empty export,
mutation during custom callback and sibling order. Outgoing: XML traversal/mutation,
createFromXML, parent/default registries, output deletion and diagnostics.

## UI-FACTORY-014: getLayeredXMLNode

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L184).
**1.** findSkinnedFilenames(XUI,filename,constraint); if empty use supplied filename
as only path, then return LLXMLNode::getLayeredXMLNode(root,paths) bool. Header
default constraint CURRENT_SKIN. **2.** CPU asset lookup and layered declarations.
**3.** Shared audited resolver with exact precedence and fallback, independent of
GPU image resource lookup. Check absolute input, current/default skin, locale layers,
empty and malformed files. Outgoing: directory resolver, layered XML merge/parser.

## UI-FACTORY-015: createFromXML

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L217).
**1.** Read node tag, lowercase it; registry.getValue. Missing callback returns null
before dummy-parent work. Null parent: lazily construct LLPanel::Params and create
dummy LLPanel, then use it as parent. Invoke chosen callback(node,parent,output).
Return view. filename argument is unused in body; no node null guard, callback
failure cleanup or dummy-allocation success check.
**2.** Native CPU tag dispatch with explicit root ownership. **3.** Native root
construction must preserve required parent-dependent layout without requiring a
GL-owning hidden panel; alternatives are audited neutral root or explicit root
context after panel behavior is known. Check uppercase tags, unknown tag, first
parentless request, nested callback and failed dummy creation.
Outgoing: XML names, lowercase, registry lookup, LLPanel Params/constructor/postBuild,
selected callback and dummy-panel lifetime. Not closed by defaultBuilder inspection.

## UI-FACTORY-016: getCurFileName

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L244).
**1.** Empty file stack -> empty string; otherwise copy back. **2.** CPU diagnostic
provenance. **3.** Scoped preparation context avoids cross-thread shared stack.
Check nested/empty state and concurrent caller obligations. Outgoing: string/container
ownership; relevant stack writers remain individually recorded.

## UI-FACTORY-017: pushFileName

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L250).
**1.** Resolve base-language skin XUI filename and push result, even if empty.
**2.** CPU diagnostic provenance. **3.** Keep base-language identity separate from
localized merged declaration sources. Check fallback and nested loads. Outgoing:
findSkinnedFilenameBaseLang, global directory service and allocation failure.

## UI-FACTORY-018: popFileName

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L257).
**1.** Unchecked vector.pop_back. **2.** CPU context scope exit. **3.** Prefer scoped
restoration that handles exceptions; underflow is not a compatibility requirement.
Check matching pushes on all normal/error returns. Outgoing: each stack caller.

## UI-FACTORY-019: setCtrlParent

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L263).
**1.** S32_MAX tab group replaced with parent->getLastTabGroup(); then
parent->addChild(view,tab_group). Does not inspect addChild result. **2.** CPU tree,
focus/tab and layout relationships. **3.** Explicit parent-owned native control tree
after auditing addChild's virtual dispatch and reparenting side effects.
Check omitted group versus explicit max/zero, rejection and existing parent.
Outgoing: getLastTabGroup, concrete addChild overrides and lifetime behavior.

## UI-FACTORY-020: copyName

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L271).
**1.** dest.setName(src.getName()->mString), no null checks. This copies XML element
tag, not its name attribute. **2.** CPU schema export. **3.** Use same audited XML
node operation if export remains supported; never confuse element tag with control
identity. Check element/attribute name difference. Outgoing: XML name storage/setter.

## UI-FACTORY-021: registerWidget

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L285).
**1.** Lookup existing tag by parameter-block type. Existing different name emits
stderr diagnostic then deliberately writes through null pointer; same name returns.
Otherwise defaultRegistrar.add(parameter type,name). widget_type is unused in active
body; schema/type registry calls commented out. **2.** CPU deterministic schema
identity validation. **3.** Typed registry errors instead of undefined null-write;
retain diagnostic meaning but not undefined behavior. Check same Params/same tag,
same Params/different tag and unused widget type. Outgoing: registry lookup/add,
type identity and static registration ordering.

## UI-FACTORY-022: saveToXML

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L207).
**1.** Active body returns0 without using arguments or writing a file. **2.** No GPU
work or implemented export result exists here. **3.** Do not invent native export
requirements from method name; separately trace defaultBuilder output-node export.
Check callers' expectations and no output side effect. Outgoing: callers only;
dormant stub is not proof that all export routes are dormant.

## UI-FACTORY-023: LLUICtrlLocate::Params::Params

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L55).
**1.** Base LLInitParam::Block<Params,LLUICtrl::Params> constructs, then set name
locate and tab_stop=false. **2.** CPU invisible layout marker defaults. **3.** Use
neutral control defaults after base closure; even an empty marker inherits font
construction effects. Check absent/overridden name/tab and base font accesses.
Outgoing: base constructor and parameter assignment/provided semantics.

## UI-FACTORY-024: LLUICtrlLocate constructor

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L62).
**1.** Delegates to LLUICtrl(p), empty body. **2.** CPU marker node/control semantics.
**3.** Share only audited neutral node behavior; absence of draw content does not
eliminate parent/focus/input ownership. Check construction/initialization via factory.
Outgoing: LLUICtrl constructor, implicit destructor and inherited virtual operations.

## UI-FACTORY-025: LLUICtrlLocate::draw

Source: [lluictrlfactory.cpp](../../../indra/llui/lluictrlfactory.cpp#L63).
**1.** Empty override; does not call LLUICtrl::draw or render children. **2.** Native
preparation emits no visual content for this target. **3.** Preserve absence of
rendered child traversal, not just transparent marker geometry. Check legal children
and direct versus parent-driven traversal. Outgoing: callers, child registry and
inherited lifecycle; no callee in draw body itself.

## UI-DEFAULT-001: LLHeteroMap::obtain<T>

Source: [llheteromap.h](../../../indra/llcommon/llheteromap.h#L42).
**1.** Lookup typeid(T). Missing -> new T(), capture typed deleter, emplace raw
pointer/deleter by type; use returned iterator without checking inserted bool.
Return dereferenced typed stored pointer. Object construction precedes insertion,
so same-type reentrant obtain has no in-progress marker. If construction reentrantly
inserts same type, outer newly allocated pointer is not deleted on emplace failure;
emplace exception also has no local pointer cleanup. No synchronization in body.
**2.** CPU typed-default cache; native GPU resources must not be hidden in default
values. **3.** Alternatives: eager typed defaults, guarded lazy cache, or existing
cache after audit. Guarded CPU-service-owned lazy cache best accommodates recursive
base defaults, with defined same-type cycle error and transactional insertion;
this is a candidate pending actual reentrancy callers, not an approved rewrite.
Check hit avoids construction; base-type recursion; same-type recursive construction;
constructor/emplace failure; factory teardown with references. Outgoing: T ctor/dtor,
type identity, allocation/map operations and deleter. No GPU effect proven absent
until every T is audited.

## UI-DEFAULT-002: LLHeteroMap::deleter<T>

Source: [llheteromap.h](../../../indra/llcommon/llheteromap.h#L70).
**1.** Cast void* to T* and delete it. **2.** CPU typed ownership teardown; T may own
resources/callbacks transitively. **3.** Keep deletion type explicit; native device
retirement cannot be delegated to arbitrary parameter destruction. Check concrete
parameter destructors and reference ownership. Outgoing: each cached T destructor.

## UI-DEFAULT-003: LLHeteroMap destructor

Source: [llheteromap.cpp](../../../indra/llcommon/llheteromap.cpp#L22).
**1.** Iterate unordered entries, call stored deleter(pointer), then null pointer.
Map destroys afterward. No semantic destruction ordering, reentrant-mutation guard
or exception isolation in body. **2.** CPU cache teardown after its users and callback
subscriptions end. **3.** Cache only independent neutral defaults or impose explicit
owner order; unordered destruction is not an adequate native resource dependency
graph. Check destructor reentrancy, cross-entry references and partial construction.
Outgoing: typed deleters, map/member destruction and caller factory lifecycle.

## UI-PARAM-002: BaseBlock constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L837).
**1.** mValidated=false, mParamProvided=false; body empty. **2.** CPU validation and
provenance flags. **3.** Share the audited value-state model if typed member layout
remains valid; no GPU allocation belongs here. Check fresh block flags. Outgoing:
member/default derived construction and virtual BaseBlock destruction. Local body
has no font lookup; derived Params constructors do.

## UI-PARAM-003: BaseBlock::validateBlock

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L204).
**1.** If not cached validated: get mostDerivedBlockDescriptor, iterate validation
list in its order, resolve const parameter from handle and call validation function.
First false optionally warns with getParamName then returns false without caching
failure. All succeed -> set mutable validated=true. Return flag. Already true skips
callbacks. **2.** CPU schema validation with explicit invalidation. **3.** Reuse
audited validators on neutral values rather than assume const validation is pure;
native constructor failure policy remains caller-specific.
Check repeated success skips callbacks, repeated failure retries, emit_errors=false,
mutation invalidation and callback side effects. Outgoing: virtual descriptor target,
handle lookup, each validation function and name/diagnostic helpers.

## UI-PARAM-004: BaseBlock::mergeBlock

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L451).
**1.** Iterate supplied descriptor's mAllParams. Resolve source at each handle.
If merge callback exists, resolve destination, assert enclosing offset equals handle,
OR-assign callback(dst,src,overwrite) into some_param_changed. All callbacks run,
including after true. No type/size compatibility check or rollback here; comment
requires same derived type. Return aggregate changed bool, not validation success.
**2.** CPU parameter merging; callback can copy resource-owning values. **3.** Preserve
registered typed merge semantics on neutral values. Replacing with dictionary overlay
would lose per-field merge behavior and provided flags; callback inventory must close
before reuse. Check callback ordering, null callback, unchanged values, failures and
typed block compatibility. Outgoing: handles, enclosing offsets, each merge callback,
descriptor initialization and copy/assignment owners.

## UI-PARAM-005: BaseBlock::fillFrom

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L901).
**1.** Returns false, no merge; source unused. Method is not virtual. **2.** CPU base
recursion termination. **3.** Preserve type-resolved dispatch in a neutral parameter
system; invoking this through BaseBlock is not a generic merge operation. Check
derived versus explicitly base-qualified calls. Outgoing: call sites/static type.

## UI-PARAM-006: Block<DERIVED,BASE>::fillFrom

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L2012).
**1.** Cast this to DERIVED*, invoke mergeBlock with this Block's static descriptor,
source and overwrite=false; return result. **2.** CPU typed default filling. **3.**
Share only with closed derived merge overrides and descriptors, not bare BaseBlock
interface. Check caller's static type and any overridden mergeBlock target.
Outgoing: derived merge target/getBlockDescriptor and descriptor lifetime.

## UI-PARAM-007: Block<DERIVED,BASE> constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L2023).
**1.** BASE constructs first, then BaseBlock::init(own descriptor, BASE descriptor,
sizeof(DERIVED)). **2.** CPU schema metadata construction and inheritance. **3.**
Use stable layout/descriptor lifetime; backend substitution cannot insert arbitrary
members/base classes without checking handle arithmetic. Check first/subsequent
instances, inherited sizes and constructor recursion. Outgoing: BASE constructor,
descriptor getters and BaseBlock::init state machine.

## UI-PARAM-008: BaseBlock::getHandleFromParam

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L182).
**1.** Reinterpret parameter and block addresses as const U8*, return difference.
**2.** CPU parameter-field identity. **3.** If existing descriptor scheme is retained,
prove same-object layout/offset validity; explicit typed accessors are an alternative
if native parameter representation changes. No GL handle relevance.
Check inherited block offsets and out-of-object input preconditions. Outgoing:
param_handle_t width/signedness, caller layout and C++ object-model constraints.

## UI-PARAM-009: BaseBlock::getParamFromHandle mutable

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L859).
**1.** Handle0 returns null. Other handle -> reinterpret U8*(this)+handle as Param*.
No bounds or dynamic-type validation. **2.** CPU descriptor lookup. **3.** Retain only
under checked descriptor/type contract, otherwise typed accessors; invalid address
behavior is not a compatibility mandate. Check0, valid inherited offset and mismatch.
Outgoing: offset producers/descriptor ownership and concrete Param layout.

## UI-PARAM-010: BaseBlock::getParamFromHandle const

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L867).
**1.** Reinterpret const U8*(this)+handle as const Param*, WITHOUT mutable overload's
handle0 special case. **2.** CPU descriptor access. **3.** Audit caller valid-handle
preconditions; do not infer equivalent null behavior from overload name. Check0 and
normal descriptors, validation callback inputs. Outgoing: same offset/layout contract.

## UI-PARAM-011: BaseBlock::paramChanged

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L876).
**1.** user_provided=true clears validated flag and sets paramProvided=true; false
leaves both unchanged. changed_param unused in this body. Virtual overrides may do
more. **2.** CPU provenance/validation invalidation. **3.** Explicit mutation semantics
must distinguish default changes from provided data, not invalidate or mark provided
indiscriminately without review. Check true/false transitions and override calls.
Outgoing: concrete virtual targets, callers and dependent validation caching.

## UI-PARAM-012: ChoiceBlock::fillFrom

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1853).
**1.** Derived-cast mergeBlock with own descriptor/source/overwrite=false. No active
choice comparison in this wrapper. **2.** CPU mutually exclusive parameter defaults.
**3.** Preserve distinction from mergeBlockParam eligibility logic below; generic
map filling is not equivalent. Check provided destination choice against direct fill.
Outgoing: derived mergeBlock and choice descriptor.

## UI-PARAM-013: ChoiceBlock::mergeBlockParam

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1858).
**1.** source_override = source_provided && (overwrite || !dest_provided). If override
OR source choice equals current choice, mergeBlock supplied descriptor/source/policy;
else false. **2.** CPU nested-choice merge eligibility. **3.** Retain source/destination
provenance as explicit metadata, not just values. Check all provided/overwrite flags
with equal/different choice. Outgoing: actual mergeBlock target/choice validity.

## UI-PARAM-014: ChoiceBlock::mergeBlock

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1870).
**1.** Set current choice to source choice, then base_block_t::mergeBlock using OWN
getBlockDescriptor(), not supplied block_data; propagate result. Active choice
assignment precedes per-field merges even when overwrite=false. **2.** CPU choice
state plus field data merge. **3.** Audit callback interactions before modeling this
as a simple tagged union overwrite. Check choice changes when no values merge and
supplied descriptor differs. Outgoing: base merge target/own descriptor and callbacks.

## UI-PARAM-015: ChoiceBlock::paramChanged

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1877).
**1.** Compute changed parameter handle. If differs from current: resolve old choice
with mutable lookup; if nonnull call old.setProvided(false), then set current handle
to changed one. Always call base paramChanged(changed,user_provided). Switching is
not guarded by user_provided; clearing old provided state itself notifies enclosing
block. **2.** CPU exclusive-choice/provenance propagation. **3.** Preserve callback
ordering with a coherent CPU mutation transaction; GPU recording never mutates these
states. Check first choice0, same/different choice, false-provided changes and nested
notifications. Outgoing: handle lookup, Param::setProvided, base override chain.

## UI-PARAM-016: BaseBlock::init

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L158).
**1.** Always replace descriptor.currentBlockPtr with this and maxParamOffset with
block_size. UNINITIALIZED: aggregate base metadata then set INITIALIZING.
INITIALIZING: set INITIALIZED. INITIALIZED: no further action. Thus transition to
INITIALIZED occurs on a subsequent init call, not at end of first construction.
No locking, recursion guard or rollback after failed first construction.
**2.** CPU schema initialization with layout constraints. **3.** Retaining this scheme
requires proven construction order and single-thread assumptions; an immutable
explicit schema is an alternative, but changing it requires every consumer's offset
contract. Check first/second construction, base propagation, failure mid-constructor
and simultaneous construction. Outgoing: aggregateBlockData, descriptor owners and
member TypedParam registration conditions.

## UI-PARAM-017: BlockDescriptor constructor

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L151).
**1.** max offset0, state UNINITIALIZED, current block pointer null; containers
default-construct. **2.** CPU descriptor state. **3.** Define lifetime independent of
device; once-built schema preferable to mutable current-instance pointers if
multi-thread native preparation is selected. Check initial flags/empty collections.
Outgoing: container members and lifetime/static initialization sites.

## UI-PARAM-018: BlockDescriptor::aggregateBlockData

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L108).
**1.** Insert base named map entries without overwrite; append base unnamed params,
validation list and all params in their existing order. Uses shared descriptor
pointers, no descriptor deep copy. **2.** CPU schema inheritance. **3.** Preserve
lookup precedence separately from all-parameter/validation traversal; a single map
cannot represent both. Check preexisting name collision and repeated aggregation.
Outgoing: descriptor smart-pointer lifetime, container allocation and init caller.

## UI-PARAM-019: BlockDescriptor::addParam

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L116).
**1.** Append incoming descriptor pointer to allParams before checks; copy back into
local smart pointer. Construct name string from char*. Handle cast to size_t > max
offset logs fatal. Empty name appends unnamed list, otherwise namedParams[name]
overwrites existing name. Nonnull validation callback appends(handle,callback) to
validation list. No rollback or duplicate removal from allParams/validation list.
**2.** CPU schema registration. **3.** Preserve name overriding and positional
validation as distinct semantics; enforce valid typed offsets before publication in
any new representation. Check duplicate name, unnamed, max boundary and null name
precondition. Outgoing: ParamDescriptor ownership, diagnostics and all consumers.

## UI-PARAM-020: Param constructor

Source: [llinitparam.cpp](../../../indra/llcommon/llinitparam.cpp#L46).
**1.** provided=false. Difference this minus enclosing block cast to U32 and masked
0x7fffffff; low field stores low16 bits, high field stores bits16..22 (mask0x007f0000).
Reconstructed offset therefore has23 bits, despite nearby '24 bits' comment. No
overflow/range/enclosing-object checks here. **2.** CPU embedded-field identity.
**3.** Keep parameter layout valid if reused; changing inheritance/member layout
requires coordination, not assuming pointers relocate automatically. Check offset
roundtrip for representative inherited blocks and upper bound. Outgoing: offset
type/bitfields, object-model assumptions and enclosingBlock.

## UI-PARAM-021: Param::getEnclosingBlockOffset

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L964).
**1.** Return (U32(high)<<16)|U32(low). **2.** CPU field lookup only. **3.** Retain as
audited arithmetic under offset limits or use typed accessors with coordinated schema
change. Check packed boundary values. Outgoing: constructor/assignment layout.

## UI-PARAM-022: Param::enclosingBlock

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L955).
**1.** Get byte address of this, subtract stored offset cast ptrdiff_t, reinterpret
as const BaseBlock*, const_cast and dereference. A const Param can thus mutate its
enclosing block. **2.** CPU provenance notification routing. **3.** Audit lifetime and
const mutation; never assume const access makes shared parameter caches concurrent.
Check copied block offsets, nested wrappers and invalid owner precondition.
Outgoing: offset decoding and concrete enclosing block identity/lifetime.

## UI-PARAM-023: Param::setProvided

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L936).
**1.** Assign provided bit, then enclosingBlock().paramChanged(*this,is_provided).
Notification occurs even if bit unchanged; false also calls virtual target.
**2.** CPU mutation propagation. **3.** Preserve exact notification order and
choice-block behavior, not just boolean storage. Check repeated true, false, nested
choices and callback invalidation. Outgoing: enclosingBlock and all overrides.

## UI-PARAM-024: Param assignment

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L942).
**1.** Copy provided bit only; preserve destination enclosing offset. Return this
reference; no paramChanged call. **2.** CPU copied value provenance anchored in new
owner. **3.** Preserve destination ownership identity; raw structure replacement
can break copied parameter blocks. Check different source/destination offsets and
validation caching after callers' assignments. Outgoing: wrapper assignments.

## UI-PARAM-025: scalar TypedParam constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1010).
**1.** Param(currentBlockPtr), named_value_t(value), then only descriptor state
INITIALIZING invokes init with validation/count/name. **2.** CPU typed default and
schema registration; named_value_t can transitively own font/image values. **3.**
Use neutral parameter value types before sharing constructor machinery, retaining
default/provided distinction. Check first/subsequent instance and GL font-pointer
default path. Outgoing: Param, named_value_t constructor, init and type specialization.

## UI-PARAM-026: scalar TypedParam::init

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1151).
**1.** Allocate shared ParamDescriptor with handle from currentBlockPtr, mergeWith,
deserializeParam, serializeParam, supplied validation, inspectParam and min/max;
block_descriptor.addParam(pointer,name). **2.** CPU typed schema callbacks.
**3.** Audit each instantiated value type; callback registration is not purity.
Check callback pointers match specialization, offset correctness and allocation
failure. Outgoing: descriptor ctor/refcount, handle computation, addParam, callbacks.

## UI-PARAM-027: scalar TypedParam::mergeWith

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1137).
**1.** Cast src/dst to same self_t. If source provided AND (overwrite OR destination
not provided): dst.set(src.getValue()), true; else false. This can report true even
when values equal; set clears named-value label rather than copying source label.
**2.** CPU default/value provenance merging. **3.** Retain semantic value and explicit
provided state; do not infer changed bool means bitwise inequality. Check full
provided/overwrite truth table, equal values and named alias loss. Outgoing: set,
getValue, isProvided and typed value copy semantics.

## UI-PARAM-028: scalar TypedParam::set

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1115).
**1.** clearValueName, setValue(val), setProvided(flag), default flag=true. No equality
test or rollback if value assignment fails. **2.** CPU semantic value mutation and
notification. **3.** Neutral resource identities separate from native image/font
publication; assign explicit value before notifying consumers. Check alias clearing,
flag=false, assignment failure and notification order. Outgoing: named_value_t
methods, Param::setProvided and concrete value owners.

## UI-PARAM-029: scalar TypedParam::deserializeParam

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1024).
**1.** Only empty remaining name range attempts value parse. If named values exist,
read string and resolve into typed value; success stores name, sets provided, true.
Otherwise try parser.readValue directly into stored value; success clears name,
sets provided, true. Else false. new_name argument unused. A failed named resolution
can be followed by direct parsing; parser cursor/side effects need separate audit.
**2.** CPU declaration decoding with typed fallback. **3.** Preserve alias/direct
resolution precedence with audited parser transactions, not ad hoc XML conversion.
Check known/unknown alias, direct numeric/text, residual name path and partially
modified value on failure. Outgoing: lookup methods, parser.readValue specialization,
value mutation/setProvided, name-range ownership.

## UI-PARAM-030: scalar TypedParam::serializeParam

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1052).
**1.** Build predicate: HAS_DEFAULT_VALUE if diff exists and values compare equal;
VALID from isValid; PROVIDED from anyProvided; EMPTY=false. Reject if rule false.
Nonempty name stack marks last.second=true. Nonempty stored key writes key only if
diff absent or diff key unequal. Empty key branch writes value only if diff absent
OR values compare EQUAL (not unequal). Failed direct write computes alias, writes
nonempty computed alias only if diff absent or diff stored alias differs. Return
last serialization result, initialized false. No mutation rollback for name stack.
**2.** CPU export; no Vulkan command required. **3.** Audit actual export callers and
predicate rules before sharing; do not silently 'fix' equal-value branch during
native migration or assume it represents intended diff semantics.
Check equal/different diff values, alias versus unnamed, failed direct writer and
name-stack mutation. Outgoing: ParamCompare<T/string>, predicate methods, value-name
lookup, parser.writeValue and concrete isValid/getValue. Runtime reachability open.

## UI-PARAM-031: scalar TypedParam::inspectParam

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1104).
**1.** parser.inspectValue<T>(stack,min,max,null), then if getPossibleValues nonnull
inspectValue<string> with that possible-values set. parameter argument unused.
**2.** CPU schema inspection/export. **3.** Preserve both concrete type and aliases,
not expose GPU-owner types as native schema. Check null/nonempty values and order.
Outgoing: parser inspector callbacks and named-value registry.

## UI-PARAM-032: ChoiceBlock constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1898).
**1.** BASE constructs; currentChoice0, then BaseBlock::init own/base descriptors
and sizeof(DERIVED). **2.** CPU exclusive parameter default selection. **3.** Check
actual first-choice initialization before assuming every constructed block always
has a selected alternative. Check first and subsequent instances and copied blocks.
Outgoing: base constructor, init and alternative construction below.

## UI-PARAM-033: ChoiceBlock::Alternative constructor

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1919).
**1.** TypedParam using DERIVED descriptor, name, default value, null validator and
0..1 count; preserve original value separately. Fetch current block pointer. ONLY
if descriptor INITIALIZING and currentChoice0 set choice to this parameter handle.
Subsequent instances with descriptor INITIALIZED do not select first alternative
in this body, despite nearby comment stating one always chosen.
**2.** CPU choice default/original value semantics. **3.** Resolve actual construction
and copy paths before adopting corrected deterministic choice behavior; observed
comment/body mismatch is investigation, not approved parity requirement.
Check first/second fresh block versus copied defaults; original-value ownership.
Outgoing: TypedParam and value copy, descriptor state, handle calculation.

## UI-PARAM-034: ChoiceBlock::Alternative::choose

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1936).
**1.** Cast enclosing block to ChoiceBlock and call paramChanged(*this,true) directly;
does NOT first set this alternative's provided bit. **2.** CPU choice activation.
**3.** Preserve distinction between chosen and provided; set(val) is a different
operation. Check choose with false provided, validation flag and previous choice.
Outgoing: enclosingBlock, ChoiceBlock::paramChanged and base notification.

## UI-PARAM-035: ChoiceBlock::Alternative::operator() const

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1961).
**1.** If enclosing block current-choice pointer equals this return stored value;
otherwise return mOriginalValue, even if stored value differs. Returns const reference.
**2.** CPU active/default choice query. **3.** Expose selected state separately from
original default in neutral schema; generic value reads can otherwise expose stale
inactive values. Check inactive after mutation, default/no choice, reference lifetime.
Outgoing: enclosingBlock/getCurrentChoice and named-value storage.

## UI-PARAM-036: ChoiceBlock::Alternative::isChosen

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1970).
**1.** Compare this pointer to enclosing ChoiceBlock current-choice pointer; not
provided flag. **2.** CPU branch condition for control state resolution. **3.** Preserve
choice versus provenance in native controls. Check none chosen, choose() and direct
set(). Outgoing: enclosingBlock and getCurrentChoice.

## UI-PARAM-037: ChoiceBlock::getCurrentChoice

Source: [llinitparam.h](../../../indra/llcommon/llinitparam.h#L1988).
**1.** const method calls base const getParamFromHandle(currentChoice); zero thus
reinterprets block address instead of yielding mutable-overload null. **2.** CPU
selection lookup. **3.** Define valid selected identity without reproducing invalid
pointer assumptions; confirm all caller behavior for initial zero.
Check0 and valid selected offset. Outgoing: const handle lookup and choice lifecycle.

## UI-REGISTRY-001: LLRegistry::Registrar::add

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L51).
**1.** map.insert(key,value); duplicate warns and false, otherwise true. Does not
replace old value. **2.** CPU constructor/callback registry. **3.** Preserve duplicate
policy per caller; this differs from replace and panel-class registration. Check
duplicate identical/different values and null function. Outgoing: value copy/owners,
key ordering, diagnostics and caller return handling.

## UI-REGISTRY-002: LLRegistry::Registrar::getValue mutable

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L83).
**1.** find key; hit returns pointer to stored value, miss null. A stored null pointer
value still yields nonnull pointer-to-value. **2.** CPU scoped target lookup. **3.**
Retain absent versus present-null distinction and stable registration lifetime.
Check null callback entry and mutation after lookup. Outgoing: map/value lifetime.

## UI-REGISTRY-003: LLRegistry::Registrar::getValue const

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L93).
**1.** Same find/miss behavior, const value pointer. **2.** CPU lookup. **3.** Const
reference does not freeze registry against other mutable users; native preparation
needs stable callback ownership. Check const pointer lifetime and absent/null values.
Outgoing: map comparator and registry mutation/lifetime.

## UI-REGISTRY-004: LLRegistry::getValue mutable

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L134).
**1.** Iterate active scopes in list order, return first nonnull value pointer; if
none, default registrar lookup. Does not skip a stored null callback. **2.** CPU
scoped callback binding. **3.** Resolve action targets while preparation scope is
valid and retain subscription lifetime, never late-lookup from GPU recording.
Check newer/older/default precedence and present-null masking. Outgoing: each scope's
lookup, active-list lifetime, scoped push/pop and callback invocation callers.

## UI-REGISTRY-005: LLRegistry::getValue const

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L144).
**1.** Same active-then-default traversal returning const pointer. **2.** CPU target
resolution. **3.** Same lifetime model as mutable lookup; constness does not snapshot
scope state. Check precedence with const caller. Outgoing: scopes/registrar lookup.

## UI-REGISTRY-006: LLRegistry::exists

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L154).
**1.** Return true at first active scope containing key; else default.exists(key).
Stored value content is irrelevant. **2.** CPU registration validation. **3.** Distinguish
entry presence from callable readiness in native error handling. Check present-null
and default shadowed entry. Outgoing: Registrar::exists and active scope ownership.

## UI-REGISTRY-007: LLRegistry::addScope

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L207).
**1.** Insert scope pointer at list beginning. No null/duplicate check. **2.** CPU
nested callback environment. **3.** Explicit preparation scope with deterministic
push/pop; GPU resources are not members of callback scope. Check double push and
newest-first lookup. Outgoing: list allocation, scope lifetime and callers.

## UI-REGISTRY-008: LLRegistry::removeScope

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L213).
**1.** Find first equal pointer; erase it if present, otherwise no-op. Duplicate
pushes need repeated pops. **2.** CPU callback-scope exit. **3.** Prefer balanced
scope lifetime with cancellation before referenced actions disappear.
Check absent scope, repeated pushes and destruction mid-lookup. Outgoing: list owners.

## UI-REGISTRY-009: ScopedRegistrar constructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L248).
**1.** Registrar base constructs; default push_scope=true invokes pushScope, false
does not. mListIt has no role in active push/pop implementation. **2.** CPU scope
registration. **3.** Native action scopes need explicit activation boundaries,
including constructors before application/window initialization. Check false/true
and singleton initialization recursion. Outgoing: Registrar ctor, pushScope.

## UI-REGISTRY-010: ScopedRegistrar destructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L256).
**1.** If singleton exists call popScope; otherwise no pop. Base/value map destruction
follows. Does not track whether this object ever pushed or pushed multiple times.
**2.** CPU subscription/callback teardown. **3.** Disconnect users before callable
storage destruction; do not infer scoped destructor removes duplicate registrations.
Check false-constructed scope, multiple pushes and singleton already gone.
Outgoing: instanceExists, popScope, Registrar/map/callable destructors.

## UI-REGISTRY-011: ScopedRegistrar::pushScope

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L264).
**1.** singleton.instance().addScope(this); may instantiate singleton. **2.** CPU
scope activation. **3.** Restrict activation to service lifetime; no GPU work.
Check first activation and double push. Outgoing: singleton and addScope.

## UI-REGISTRY-012: ScopedRegistrar::popScope

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L269).
**1.** singleton.instance().removeScope(this), even if caller previously never
pushed. **2.** CPU scope exit. **3.** Avoid creating destroyed services during native
teardown; audit explicit calls separately from destructor guard. Check no instance
and missing scope. Outgoing: singleton construction and removeScope.

## UI-REGISTRY-013: StaticRegistrar constructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L292).
**1.** If singleton.instance().exists(key), log fatal duplicate. Then add key/value
to mStaticScope; add result ignored. Entry is not in registrar object's own map.
**2.** CPU static target registration. **3.** Native startup must audit singleton
initialization and callback constructors before backend selection; merely not calling
draw does not prove safe globals. Check duplicate in dynamic/default/static scope
and stored callable capture lifetime. Outgoing: singleton, exists, static scope add.

## UI-REGISTRY-014: LLRegistrySingleton constructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L322).
**1.** Registry and singleton bases construct, mStaticScope=null. **2.** CPU service
initial state. **3.** Share only after LLSingleton initialization/teardown closure;
static scope is not yet available in this constructor body. Check first recursive
registration. Outgoing: both base constructors and later initSingleton callback.

## UI-REGISTRY-015: LLRegistrySingleton::initSingleton

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L326).
**1.** mStaticScope=new ScopedRegistrar(), whose default constructor pushes scope
via singleton.instance() before assignment completes. **2.** CPU initialization
with reentrant singleton access. **3.** Need proven singleton construction protocol;
replacing with a generic function-local static without analysis may recurse/fail.
Check first static registration and allocation failure. Outgoing: ScopedRegistrar,
LLSingleton initialization state machine and virtual derived overrides.

## UI-REGISTRY-016: LLRegistrySingleton destructor

Source: [llregistry.h](../../../indra/llcommon/llregistry.h#L331).
**1.** delete static scope; base destruction follows, no local pointer reset.
**2.** CPU target teardown. **3.** Native owner graph must resolve active scopes and
callback references before registry destruction. Check late ScopedRegistrar dtor,
partial init and callable destruction. Outgoing: scoped/base/map destructors.

## UI-PANEL-001: LLPanel::fromXML

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L369).
**1.** Start name='panel', read name attribute; read class attribute. Nonempty class:
createPanelClass(class), warn if null. If no panel, createFactoryPanel(name), assert
nonnull, return null if still null. If panel factory map nonempty push its address
onto BACK of sFactoryStack. Push panel commit then enable callback scopes. Call
initPanelXML using default LLPanel Params and IGNORE bool. Pop commit then enable
scope. If map currently nonempty pop factory stack back (condition recomputed).
Return panel even when initPanelXML false. No local exception scope restoration;
factory-map mutation during callbacks can change push/pop condition.
**2.** CPU typed panel construction with scoped actions and declaration loading.
**3.** Prefer explicit construction context/ownership over invoking GL-owning panels;
preserve class-before-factory precedence and defined partial-failure policy. Scoped
rollback is a candidate failure correction requiring reviewed behavior, not assumed
equivalence. Check injected class, missing class fallback, null factory return,
failed initPanelXML, callback exceptions and map empty/nonempty mutation.
Outgoing: XML getters, class registry, createFactoryPanel, getFactoryMap, deque/scopes,
defaults, initPanelXML and concrete injected ctor/init/postBuild/dtor targets.

## UI-PANEL-002: LLPanel::createFactoryPanel

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L852).
**1.** Iterate sFactoryStack BEGIN to END (not newest/back first). First map with
name invokes mCallback(mData), C-casts result to LLPanel*, returns immediately even
if null. No callback found -> construct LLPanel::Params, create<LLPanel>(params).
**2.** CPU context-specific panel factory. **3.** Preserve factory precedence separately
from callback registry's newest-first scopes. Alternatives: explicit ordered factory
context or audited current deque; native preparation benefits from immutable context
because callbacks can construct nested panels. Check overlapping names, null factory
return, reentrant construction and fallback postBuild before later XML initialization.
Outgoing: LLCallbackMap types/target lifetime, each factory function, Params and create.

## UI-PANEL-003: LLPanel::initPanelXML

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L491).
**1.** Copy default params. If mXMLFilename empty, read node filename/setXMLFilename.
Cache factory/node name/child registry; construct parser. Nonempty filename plus
output node: parse current node, export copied params via setupParamsForExport,
set output tag/write against defaults, return true WITHOUT loading reference,
initializing panel, constructing children, parenting or postBuild.
Nonempty filename without output: push filename; failed layered XML warns/returns
false WITHOUT pop. Success: parse reference into params with pushed provenance,
setShape(reference params rect), create referenced children, pop filename. Then
parse current node into params. Optional output writes copied export params.
Set from_xui=true; applyXUILayout(params,parent); initFromParams. Create current-node
children. Parent nonnull: use provided tab_group else parent's last group, addChild
AFTER children because parent may reshape. Call postBuild ignoring bool. Return true.
No local validation-block call, child success aggregate or rollback in this body.
**2.** CPU layered panel preparation, topology and parent-dependent layout/actions.
**3.** Preserve reference-then-inline child order and pre-parent initialization in a
native construction transaction. One generic defaultBuilder is not equivalent;
export-only early return and failure behavior need deliberate tests.
Check referenced/inline value precedence and duplicate children, missing reference,
export shortcut, tab-container reflow, false postBuild and scope/file stack state.
Outgoing: filename accessors, XML parser/merge, shape/layout, default/typed Params,
child registry, createChildren, virtual initFromParams/addChild/postBuild and exports.

## UI-PANEL-004: LLPanel::initFromParams

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L425).
**1.** set visible/enabled/focus-root/sound flags BEFORE LLUICtrl::initFromParams.
Provided visible_callback -> initCommitCallback/setVisibleCallback. Iterate localized
strings assigning UIStrings[name]=value. Set label/help topic/shape, parse follows,
tooltip/fromXUI, hover cursor. has_border -> addBorder. use_bounding_rect only if
provided. Set default tab group, mouse opaque, background visible/opaque/colors.
Assign GL background images from parameters. Assign checkpoint native image-name
strings from vk_image_name only if provided else empty; these existing fields do NOT
prove GL-free image lookup. Assign image overlays, accepts-badge. No else removes
existing border, no strings clear or old callback removal when omitted in this body.
**2.** CPU panel appearance/layout/action state plus versioned image identities.
**3.** Native prepared panel state must separate image resolution from GL owners and
preserve reinitialization semantics. Existing native-name fields are migration debt,
not closure. Check repeated initialization, omitted/provided background/border,
visible callback ordering, reference-child shape changes and fallback images.
Outgoing: every setter/virtual callback, LLUICtrl initialization, image ParamValue,
LLUIColor, addBorder and badge ownership. All remain open.

## UI-PANEL-005: LLRegisterPanelClass::addPanelClass

Source: [llpanel.h](../../../indra/llui/llpanel.h#L288).
**1.** map[tag]=std::function, replacing prior callable without diagnostic. **2.** CPU
panel class registration. **3.** Preserve actual overwrite policy, unlike generic
StaticRegistrar duplicate fatal. Audit capture destruction on replacement and static
order. Check duplicate tag/custom function. Outgoing: map/function ownership/callers.

## UI-PANEL-006: LLRegisterPanelClass::createPanelClass

Source: [llpanel.h](../../../indra/llui/llpanel.h#L293).
**1.** Find string_view tag; absent returns0, present invokes stored function with no
args, no empty-function check. **2.** CPU dynamic class construction. **3.** Selected
native lifecycle needs audited native/neutral constructor targets under these names,
not cast a GL control result. Check absent, empty function, null return and exception.
Outgoing: transparent hash/equality, each registered target/capture/lifetime.

## UI-PANEL-007: defaultPanelClassBuilder<T>

Source: [llpanel.h](../../../indra/llui/llpanel.h#L301).
**1.** new T() and return pointer; no Params argument, initFromParams or postBuild.
T's constructor can perform all of those itself. **2.** CPU panel class construction.
**3.** Audit each actual T default constructor independently from factory<T>(Params).
Check constructor-created children/callbacks before XML initialization. Outgoing:
each T constructor/base/member chain and allocation failure.

## UI-PANEL-008: LLPanelInjector<T> default-builder constructor

Source: [llpanel.h](../../../indra/llui/llpanel.h#L328).
**1.** singleton.addPanelClass(tag,&defaultPanelClassBuilder<T>). **2.** CPU static
class injection. **3.** Share tags, not unexamined constructors, across selected
lifecycle. Check static initialization and duplicate injection ordering.
Outgoing: singleton, addPanelClass and typed builder; injector inventory open.

## UI-PANEL-009: LLPanelInjector<T> custom-builder constructor

Source: [llpanel.h](../../../indra/llui/llpanel.h#L335).
**1.** singleton.addPanelClass(tag,func). T does not select a constructor in this
overload body. **2.** CPU explicit factory registration. **3.** Resolve supplied
callable target/captures; template type alone is not coverage. Check arbitrary
custom builder, null/empty callable and duplicate tags. Outgoing: singleton,
addPanelClass, function copy/destruction and actual callback target.

## UI-PANEL-010: LLPanel::Params constructor

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L70).
**1.** Base block/LLUICtrl Params constructs first. has_border=false, border unnamed;
background_visible=false, background_opaque=false; named color/image/overlay params;
min_width/min_height100; multiple string, filename, class, help_topic,
visible_callback, accepts_badge parameters. Add bg_visible synonym for background
visible, border_visible for has_border, title for inherited label. Omitted explicit
defaults use their typed default constructors, not inferred zero/null values.
**2.** CPU declaration defaults with neutral color/image/font identities. **3.**
Preserve schema and synonyms while removing GL-owning value resolution; independent
hardcoded native panel style would not preserve per-skin defaults.
Check absent/provided values, synonyms and member-construction effects.
Outgoing: base and every wrapper/value constructor, addSynonym and template loading.

## UI-PANEL-011: LLPanel constructor

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L96).
**1.** Construct LLUICtrl(p), LLBadgeHolder(accepts_badge). Initialize background
visible/opaque, colors/overlays, GL image pointers and checkpoint native image names
(provided vk_image_name else empty), default button/border null, label/help,
commit/enable registrars with push=false, filename, visible signal null. Body calls
addBorder if has_border. Header default argument is getDefaultParams(), so default
new T() constructors inheriting it can resolve fonts/images before body runs.
**2.** CPU panel state and child/action ownership, native image identities separated.
**3.** Audit default-argument and base construction before reuse; no late draw branch
can remove earlier GL image/font ownership. Check direct/default/explicit Params,
border child creation and registrar activation absence.
Outgoing: base classes, default params, image/color owners, registrars, addBorder.

## UI-PANEL-012: LLPanel destructor

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L126).
**1.** Delete mVisibleSignal; member and base destructors follow. Body does not
explicitly remove border or delete children; those are outgoing base responsibilities.
**2.** CPU callbacks and tree teardown with native image versions separately retired.
**3.** Disconnect async users before panel destruction; signal deletion alone does
not establish that callbacks cannot recreate UI. Check signal connections, border,
default-button raw pointer, factory scopes and partial construction.
Outgoing: signal, member image/registrar/string owners, LLBadgeHolder/LLUICtrl/LLView.

## UI-PANEL-013: LLPanel::addBorder(Params)

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L137).
**1.** Parameter copied by value; removeBorder, overwrite p.rect with local rect,
create<LLViewBorder>(p), store pointer, addChild(pointer) with default group. No
success check before addChild. **2.** CPU border child creation and paint ordering.
**3.** Retain border as native prepared child or explicit decoration only after child
focus/layout/ordering contract closes; do not replace it with a rectangle solely
because of its name. Check replace, creation failure, parent reshape and z-order.
Outgoing: Params copy, local rect, removeBorder, factory/LLViewBorder/addChild.

## UI-PANEL-014: LLPanel::addBorder()

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L146).
**1.** Construct LLViewBorder::Params, set border_thickness to LLPANEL_BORDER_WIDTH,
call Params overload. **2.** CPU border defaults. **3.** Preserve declared units and
skin override precedence, not hardcode a native pixel thickness prematurely.
Check constant and DPI/layout application. Outgoing: Params/defaults, assignment,
LLPANEL_BORDER_WIDTH definition and addBorder(Params).

## UI-PANEL-015: LLPanel::removeBorder

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L154).
**1.** Nonnull border: removeChild, delete border, set pointer null. Null no-op.
**2.** CPU tree invalidation/deletion; old prepared frames may still reference native
border geometry/images. **3.** Separate CPU child removal from completed-use native
retirement, with deterministic callback cancellation. Check removal side effects,
destructor reentry and prepared-frame lifetime. Outgoing: removeChild, border dtor.

## UI-PANEL-016: LLPanel::draw

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L203).
**1.** Always getDrawContext().alpha initially. If background visible, replace local
alpha with virtual getCurrentTransparency, obtain local rect. Opaque branch: nonnull
opaque image draws rect with opaque overlay % alpha; otherwise gl_rect_2d with opaque
color.get() % alpha. Nonopaque branch uses alpha image/overlay or alpha color rectangle.
Then nonvirtual updateDefaultBtn (empty at checkpoint), then qualified LLView::draw.
Local alpha is not pushed into a new child draw context by this body. Hidden background
does not call getCurrentTransparency; children still draw.
**2.** Native preparation chooses background contribution/resource/color, then child
painter order, retaining exact alpha source and any callback effects.
**3.** Prepared panel primitives plus prepared children, never invoke this GL draw
callback. Native image/solid branches must honor independent opacity/visibility;
an unconditional opacity multiplier on descendants would change the contract.
Check all four image/color branches, hidden background, transparency callback
invocation count, inherited child alpha and clipping. Outgoing: draw context, virtual
transparency, local rect, LLUIImage::draw, color get/operator%, gl_rect_2d, LLView::draw.

## UI-PANEL-017: LLPanel::updateDefaultBtn

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L245),
[nonvirtual declaration](../../../indra/llui/llpanel.h#L162).
**1.** Empty body. Search finds panel draw and qualified floater draw callers, no
alternate implementation. **2.** No native preparation work follows from this body.
**3.** Do not infer default-button mutation from method name; actual button state
mutation is traced at its real setters/input callbacks. Check call sites remain
nonvirtual and source unchanged. Outgoing: none in body; callers separately audited.

## UI-PANEL-018: LLPanel::getCtrlList

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L187).
**1.** Iterate immediate child list order, invoke view->isCtrl; true -> static_cast
LLUICtrl*, append to local vector. Does not recurse or filter visible/enabled.
**2.** CPU control traversal. **3.** Preserve direct-child semantics and lifetime
during subsequent callback loops. Check mixed children, hidden/disabled and virtual
isCtrl correctness. Outgoing: child list, isCtrl overrides, pointer lifetime.

## UI-PANEL-019: LLPanel::clearCtrls

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L166).
**1.** Copy getCtrlList result, then for each pointer setFocus(false), setEnabled(false),
clear(), in that order. List copy does not retain child lifetime through callbacks.
**2.** CPU control/focus/model mutation. **3.** Preserve ordering while defining
callback-safe native ownership; clear is not a GPU buffer clear. Check reentrant child
deletion, focus effects and each concrete clear target. Outgoing: getCtrlList and
virtual focus/enabled/clear targets plus panel overrides.

## UI-PANEL-020: LLPanel::setCtrlsEnabled

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L178).
**1.** Snapshot direct control pointer list, invoke each setEnabled(b), no recursion
or visible filter. **2.** CPU interactive state update. **3.** Same callback/lifetime
requirements as clearCtrls; derive visuals in preparation after state changes.
Check nested panels and callback mutation. Outgoing: getCtrlList and enabled targets.

## UI-PANEL-021: LLPanel::handleKeyHere

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L273).
**1.** Read current keyboard focus, dynamic-cast LLUICtrl. Escape regardless mask:
setFocus(false), return true. Shift-only Tab: if focused control, findRootMostFocusRoot,
if nonnull focusPrevItem(false). No-mask Tab similarly focusNextItem(false).
If not handled, focused control, Return and no mask: focused LLButton with
getCommitOnReturn true -> leave false for button handling; else visible+enabled
default button -> onCommit, true; else acceptsTextInput -> focus onCommit, true.
Otherwise return handled. No default-button action if focus is absent/non-LLUICtrl.
**2.** CPU input/focus/action dispatch, independent of GPU. **3.** Native interactive
controls need the same dispatch and action policy after auditing transitive service
callbacks; GLFW/Win32 key forwarding alone is insufficient.
Check exact modifiers, no focus, default disabled/invisible, Return-capturing button,
text field commit, cancellation and callback deletion. Outgoing: focus manager, RTTI,
focus traversal/setFocus, button flags, visible/enabled, acceptsTextInput/onCommit.

## UI-PANEL-022: LLPanel::setFocus

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L344).
**1.** b=true and !hasFocus -> qualified LLUICtrl::setFocus(true) first, then
focusFirstItem(). Else qualified LLUICtrl::setFocus(b). No refresh() call in this body
despite nearby refresh comment; transitive base behavior still needs inspection.
**2.** CPU focus ownership/traversal. **3.** Preserve preemptive focus ordering to
avoid reentrant loops; native painting consumes resolved focus state.
Check first focus, already-focused descendant, false and no valid first child.
Outgoing: hasFocus, base setFocus, focusFirstItem and callback effects.

## UI-PANEL-023: LLPanel::onVisibilityChange

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L337).
**1.** Qualified LLUICtrl::onVisibilityChange(new), then if mVisibleSignal invoke
with this and LLSD(bool). No equality test or lifetime guard in body after base call.
**2.** CPU visibility notification/action dispatch. **3.** Preserve base-before-panel
signal order with callback-safe native control lifetime. Check deletion/registration
during base callbacks and repeated same values. Outgoing: base callback, LLSD ctor,
signal target list and connection ownership.

## UI-PANEL-024: LLPanel::setDefaultBtn(pointer)

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L255).
**1.** Assign raw pointer, no focus/visual mutation. **2.** CPU Return-key target.
**3.** Native action target needs lifetime validity independent of prepared geometry;
changing visual default style is not mandated by this setter. Check null/deleted child.
Outgoing: pointer owner/deletion and handleKeyHere consumer.

## UI-PANEL-025: LLPanel::setDefaultBtn(name)

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L260).
**1.** getChild<LLButton>(id); if nonnull set pointer else null. getChild may have
fallback construction behavior; name lookup is not proven passive. **2.** CPU named
action resolution. **3.** Audit lookup/fallback lifecycle before sharing; native
default-button identity must not create a hidden GL control. Check missing/wrong
type/recursive child and fallback ownership. Outgoing: getChild<T>, pointer overload.

## UI-PANEL-026: LLPanel::setBorderVisible

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L359).
**1.** Nonnull border -> border.setVisible(b), otherwise no-op. Does not change
background or create border. **2.** CPU decoration visibility. **3.** Preserve
independent border/background state; prepare after callback-safe visibility changes.
Check null border and repeated visibility. Outgoing: LLViewBorder visibility target.

## UI-PANEL-027: LLPanel::refresh

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L249).
**1.** Empty virtual default. Comment says automatically called from setFocus;
LLPanel::setFocus body does not do so locally. **2.** No work in base, concrete
overrides remain CPU model/preparation obligations. **3.** Resolve actual callers
and overrides rather than preserving a comment as control flow.
Check call graph and derived implementations. Outgoing: no callee in base body.

## UI-PANEL-028: LLPanel::LocalizedString constructor

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L59).
**1.** Base block constructs, mandatory name/value fields register names without
explicit defaults. **2.** CPU panel localization schema. **3.** Preserve mandatory
validation and localized value ownership, not GPU text pre-rasterization.
Check missing name/value and layered duplicate string names. Outgoing: Block,
Mandatory<string> constructors/validators and parser targets.

## UI-PANEL-029: LLPanel::getDefaultParams

Source: [llpanel.cpp](../../../indra/llui/llpanel.cpp#L65).
**1.** Return factory getDefaultParams<LLPanel>() by const reference. **2.** CPU
panel defaults resolution with resource-neutral types required for native use.
**3.** Reference existing factory/default records; default argument invocation timing
is part of lifecycle, not lazy draw setup. Check first direct panel construction.
Outgoing: UI-FACTORY-001 and LLPanel Params chain, all still transitive-open.

## UI-OPACITY-001: LLUICtrl::getCurrentTransparency

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L1063).
**1.** alpha starts0. TT_DEFAULT -> current draw context alpha; TT_ACTIVE -> global
active transparency; TT_INACTIVE -> global inactive; TT_FADING -> inactive/2;
TT_FORCE_OPAQUE ->1. No default switch branch, unknown enum stays0. If override
callback present return callback(type,alpha), otherwise alpha. No clamping or
automatic inherited-alpha multiplication for nondefault cases.
**2.** CPU opacity policy and action-like callback invocation during preparation.
**3.** Resolve opacity before immutable draw generation, preserving when/where
callbacks run; do not multiply every node by parent opacity unconditionally.
Check each enum, globals, out-of-range callback return, callback mutation/reentry and
call count across background/text/image branches. Outgoing: draw context, global
setting writers, override setter/callable targets and concrete virtual overrides.

## UI-OPACITY-002: LLUICtrl::setTransparencyType

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L1103).
**1.** Assign enum without validation, notification or dirty invalidation in body.
**2.** CPU opacity-mode state. **3.** Native prepared-state invalidation must account
for setter usage without relying on nonexistent local notification.
Check repeated/invalid enum and next-frame versus retained replay result.
Outgoing: callers, enum definition, retained invalidation and getCurrentTransparency.

## UI-BUTTON-001: LLButton::draw

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L957).
Revision/configuration and status are the document's pinned Windows baseline;
local body inspected, outgoing edges OPEN, no implementation/runtime/parity closure.
**1.** First invocation constructs function-static LLCachedControl<bool> from UI
config setting EnableButtonFlashing with fallback true. Alpha uses draw context
when mUseDrawContextAlpha, otherwise virtual getCurrentTransparency. Focused button
reads keyboard space and, when mCommitOnReturn, Return state. Mouse capture causes
local mouse position lookup and pointInView. Cache enabled-chain bool, pressed=
keyboard OR captured-and-over OR forced, selected=getToggleState(). Initialize glow
off, highlight/glow colors white, glow blend ADD_WITH_ALPHA, image pointers null.

Selection and mutation order:

1. If flashing timer exists AND ((selected AND not flashing-in-progress AND not
	force-flashing) OR pressed), set mFlashing=false. flash=mFlashing AND cached setting.
2. Pressed AND displayPressed -> selected?pressedSelected:pressed image. Otherwise
	needsHighlight -> selected/unselected hover image if present, else corresponding
	ordinary image AND enable glow. Otherwise ordinary selected/unselected image.
3. Disabled-selected image, when present, overrides if (enabled AND tentative) OR
	(!enabled AND selected). Else disabled image overrides when present, disabled
	and unselected. Set imageGlow=image.
4. If mFlashing: flash AND flash image -> imageGlow=flash image. If timer exists,
	use alternate/normal flash color, enable glow, blend ALPHA; timer highlighted OR
	not flashing-in-progress -> flash color; else needsHighlight -> white highlight
	color; else flash color. Setting EnableButtonFlashing=false gates flash-image
	substitution, NOT all glow behavior in this body. Highlight with null image also
	enables glow.
5. If toggle signal exists invoke it(this,empty LLSD) then setToggleState(result).
	This occurs AFTER image selection and local selected/enabled snapshots, BEFORE
	text color, geometry size queries and overlay color. Reentrant callback may alter
	button/model/layout; no lifetime guard or snapshot restart here.
6. Label color uses cached enabled bool and newly queried toggle: enabled selected/
	unselected or disabled selected/unselected colors. SearchableControl highlighted
	overrides with highlight font color. Focused AND drawFocusBorder -> drawBorder
	using earlier selected image, focus color % alpha and focus flash width.
7. Update mCurGlowStrength with lerp and LLSmoothInterpolation.getInterpolant(.05).
	Glow enabled target: flashing+timer ? (highlighted OR not in-progress OR hover ?
	1:0) : hoverGlowStrength. Glow disabled target0. This mutates animation state on
	each draw invocation, not necessarily once per simulation frame.

Visual emission follows:

- Nonnull image: disabledColor=disabledImageColor with alpha halved if fadeWhenDisabled,
  else unchanged. ScaleImage -> draw local rect with enabled imageColor or disabledColor,
  each %alpha. If glowStrength>.01, set glow blend, drawSolid imageGlow at(0,0,width,
  height) with glowColor %(strength*alpha), then BT_ALPHA. Non-scaled image y=local
  height-image natural height; draw at(0,y) natural size, same colors; optional glow
  uses imageGlow natural size at same y and restores BT_ALPHA. Missing image logs
  debug and draws unfilled pink rectangle; it emits no glow even if strength>0.
- textLeft=left padding, textRight=width-right padding, textWidth=width-both paddings.
  Overlay exists -> getOverlayImageSize, local centerX/Y. Pressed+displayPressed
  shifts centerY--,centerX++; centerY+=bottomPad-topPad. Overlay tint starts ordinary,
  disabled overrides, else current toggle selects selected tint; multiply alpha.
  Positive rightDelta -> draw at(width-overlayWidth-rightDelta,centerY-overlayHeight/2)
  without reducing text width. Otherwise LEFT shifts textLeft and reduces textWidth
  by overlayWidth+spacing, draws at left padding; HCENTER centers without text-width
  change; RIGHT shifts textRight/reduces textWidth, draws at width-rightPad-overlayWidth;
  invalid alignment draws nothing. Half sizes use integer division.
- If getCurrentLabel() is nonempty, copy another getCurrentLabel(), trim wide string.
  x=right text edge for RIGHT, textLeft+textWidth/2 for HCENTER, textLeft otherwise.
  Pressed+displayPressed increments x. Font buffer renders trimmed label offset0,
  x, y=integer(height/2)+bottomVPad, labelColor%alpha, current HAlign/VCENTER,
  NORMAL, SOFT shadow iff dropShadowedText, S32_MAX chars,textWidth pixels,rightX=null,
  useEllipses/useFontColor. Store returned mLastDrawCharsCount. Empty original label
  skips rendering WITHOUT clearing previous count; whitespace-only label enters
  but becomes empty after trim. Cache caller obligations apply to both flags.
- CheckboxControlPanel exists -> setOrigin(0,0), reshape(current button width,height),
  invoke its virtual draw directly. Then qualified LLUICtrl::draw, whose child
  traversal and potential duplication of that panel remain unresolved.

**2.** Native CPU preparation must resolve input/model callbacks, animation state,
layout changes, image/material choice and text shaping into immutable ordered output.
Native GPU records consume resource versions, clip/alpha and geometry only.
**3.** Prefer explicit phased preparation with documented old-state/new-state reads
to either calling this GL draw or moving all callbacks arbitrarily before rendering.
The source's image-before-toggle/label-after-toggle split is a defined ordering to
test; correcting it requires explicit behavior review. Animation time advancement
must preserve per-view/repeated-draw policy, not assume once-per-frame equivalence.
Owned control handles are needed across reentrant action callbacks; retaining a raw
this/image pointer does not establish native callback lifetime safety.
Checks: toggle callback changes false->true and width; image uses old selection,
label/overlay use current selection. Test flashing setting false with active timer,
every image fallback, tentative state, focus/pressed combinations, .01 glow threshold,
both scale modes, every overlay placement, empty/trimmed label, ellipses/color flags,
checkbox side effects, repeated auxiliary draws and callback deletion/cancellation.
Outgoing: cached setting construction/subscription, draw context/transparency,
keyboard/focus/capture and coordinate helpers, toggle/value/LLSD signal, timer and
interpolation, color/image providers, getCurrentLabel/trim, font buffer render/reset,
drawBorder, checkbox panel layout/draw, base draw and all virtual overrides.

## UI-BUTTON-002: LLButton::setToggleState

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1348).
**1.** If new bool differs from getToggleState: setControlValue(bool) first (settings
callbacks), setValue(bool), setFlashing(false), autoResize(), fontBuffer.reset().
Equal state does nothing. No re-check after callbacks, transaction rollback or
lifetime guard. Callback arguments omitted for setFlashing resolve header defaults.
**2.** CPU model/settings/action mutation and layout invalidation; no Vulkan work
belongs in this setter. **3.** Preserve notification/value/resize order with
callback-safe native control ownership and invalidated prepared text. Do not share
reference autoResize without closing its font measurement/resource effects.
Check equal value suppresses all work, nested settings writes, resize text/geometry,
flashing cancellation and deleted owner. Outgoing: getToggleState,setControlValue,
virtual setValue,setFlashing/defaults,autoResize,font cache reset.

## UI-BUTTON-003: LLButton::getToggleState

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1343).
**1.** getValue().asBoolean(); no separate toggle field. **2.** CPU view-model query.
**3.** Preserve LLSD coercion/default semantics and virtual getValue target rather
than introduce divergent cached native toggle state. Check bool and other LLSD types.
Outgoing: getValue override/model and LLSD::asBoolean.

## UI-BUTTON-004: LLButton::drawBorder

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1329).
**1.** Null image returns. ScaleImage -> image.drawBorder(local rect,color,size).
Otherwise y=local height-image height and drawBorder(0,y,color,size), natural size.
**2.** CPU focus-decoration geometry and native image-coverage material.
**3.** Preserve image-shaped enlarged decoration, not generic outline; dimensions
and resources need prepared snapshots. Check scale/natural placement and null image.
Outgoing: rect/image dimensions, both LLUIImage border overloads and native lifetime.

## UI-BUTTON-005: LLButton::getOverlayImageSize

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L944).
**1.** Read image width then height into output references. factor=min(buttonWidth/
overlayWidth,buttonHeight/overlayHeight,1), scale and ll_round each extent. No null,
zero-size, finite or negative-size guard. **2.** CPU aspect-preserving fit, never
upscale valid positive inputs. **3.** Shared audited geometry can provide native
layout, with explicit invalid-asset policy rather than undefined division/casts.
Check non-square, downscale, natural fit, zero and negative extents. Outgoing:
virtual image dimensions, button rect, llmin/round and image availability timing.

## UI-BUTTON-006: LLButton::setHighlight

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L908).
**1.** Changed bool -> assign mNeedsHighlight and fontBuffer.reset; equal no-op.
**2.** CPU hover appearance/cache invalidation. **3.** Native state generation must
invalidate dependent text/material even if geometry parameters otherwise unchanged.
Check equal/changed value and retained text. Outgoing: buffer reset and setter callers.

## UI-BUTTON-007: LLButton::onMouseLeave

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L901).
**1.** Qualified LLUICtrl::onMouseLeave(x,y,mask) then setHighlight(false).
**2.** CPU pointer-event/callback propagation. **3.** Retain order and callback-safe
owner lifetime; GPU submission never invokes event methods. Check base callback
mutates highlight or deletes control. Outgoing: base callback and setHighlight.

## UI-BUTTON-008: LLButton::handleHover

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L917).
**1.** Enabled chain AND (no mouse capture OR captured by this) -> setHighlight(true).
Does not explicitly clear highlight when condition false. childrenHandleHover first;
if unhandled and mouse-down timer started, getHeldDownTime; when elapsed>=heldDelay
AND current frame count minus mouseDownFrame>=frameDelay, construct LLSD count with
postincrement mouseHeldDownCount, invoke held signal if nonnull. Count increments
even without signal. Then set window cursor to HAND if hoverHandCursor else ARROW,
debug log. Child-handled path skips held callback/cursor. Always return true.
**2.** CPU repeated-hover input and timer/action semantics. **3.** Native event loop
must preserve dual time/frame threshold and child precedence; a time-only repeat
timer would differ. Audit callback lifetime before cursor access after signal.
Check threshold equality, frame delta, captured elsewhere, disabled, child handled,
null held signal, repeated hover and deletion/reentry. Outgoing: enabled/capture,
child hover targets, timer/frame count, getHeldDownTime, LLSD/signal and window cursor.

## UI-BUTTON-009: LLButton::setUseEllipses

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L385).
**1.** Assign flag then fontBuffer.reset unconditionally, even if unchanged.
**2.** CPU text-layout policy/invalidation. **3.** Native prepared text needs an
explicit ellipsis input/version; for this setter the reference's omitted retained
cache key is compensated by reset. Check toggling flag with otherwise equal render
inputs. Outgoing: reset, setter callers and direct field writes still open.

## UI-BUTTON-010: LLButton::setUseFontColor

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L391).
**1.** Assign use-font-color then reset unconditionally. **2.** CPU color/grayscale
glyph policy and invalidation. **3.** Explicit native glyph-type selection/version;
reference buffer key omission is compensated for this setter only.
Check glyph request type after switch. Outgoing: reset, direct writes/callers.

## UI-BUTTON-011: LLButton::getCurrentLabel

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1487).
**1.** Return selected or unselected LLUIString const reference based on getToggleState.
No trimming here. **2.** CPU label selection and source-index ownership. **3.** Native
preparation must snapshot text after the appropriate model callback, retaining original
versus trimmed text distinction. Check changed toggle, reference lifetime, whitespace.
Outgoing: getToggleState and LLUIString content/formatting lifetime.

## UI-BUTTON-012: LLButton::setFont

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1435).
**1.** Store supplied font if nonnull, else LLFontGL::getFontSansSerif(); reset buffer.
No owned font reference is acquired here. **2.** CPU font identity resolution and
layout invalidation; native glyph GPU ownership separate. **3.** Native font handle
must survive asynchronous preparation and registry reset; fallback getter's GL font
chain remains open. Check null/explicit font and registry destruction.
Outgoing: default font getter, raw owner, reset and metric consumers.

## UI-BUTTON-013: LLButton::setDropShadowedText

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1526).
**1.** Assign flag, reset buffer. **2.** CPU decoration policy. **3.** Native text
preparation includes shadow mode and ordered glyph copies; no generic blur substitute.
Check same/changed flag. Outgoing: reset, render shadow selection and callers.

## UI-BUTTON-014: LLButton::autoResize

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1541).
**1.** resize(getCurrentLabel()), no mAutoResize guard in facade. **2.** CPU layout
measurement/invalidation. **3.** Native preparation needs audited CPU metrics even
for paths whose resize flag is false; do not call this GL-font-owning facade.
Check disabled autoresize still reaches measurement. Outgoing: current label/resize.

## UI-BUTTON-015: LLButton::resize

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1546).
**1.** ALWAYS mGLFont.getWidth(label.getWString().c_str()), then current width.
Only then mAutoResize gate. Enabled: minWidth=labelWidth+left/right padding. Overlay
present -> source width, factor=(buttonHeight-bottomPad-topPad)/sourceHeight WITHOUT
clamp<=1, round scaled overlay width. LEFT/RIGHT add overlayWidth+spacing; HCENTER
max(label minimum,overlayWidth+both paddings); unknown leaves label minimum. If old
width<minimum, reshape(minimum,current height). Never shrink here. RightDelta drawing
override is not considered by this layout method. Font measurement can rasterize/
allocate GL atlas resources even if mAutoResize=false; no font/image null-size guard.
**2.** CPU native measurement, minimum-width computation and tree reflow with text/
image metadata independent of GPU readiness. **3.** Separate metrics/layout from
glyph publication and preserve this fit rule, which differs from draw overlay fit's
no-upscale constraint. Do not equate draw size to resize minimum automatically.
Check flag false still measures, grow-only, overlay upscale/padding/zero height,
all alignments, rightDelta, and callbacks triggered by reshape.
Outgoing: LLUIString conversion, GLFont getWidth chain, image dimensions, rounding,
rect arithmetic, virtual reshape and parent/child invalidation.

## UI-BUTTON-016: LLButton::Params constructor

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L71).
**1.** Base UI control Params first. Explicit defaults: label_shadow=true,
auto_resize=false,use_ellipses=false,use_font_color=true,overlay alignment='center',
overlay-label spacing1,rightDelta0,overlay tint white alpha.75,disabled overlay white
alpha.3,selected overlay white; left/right padding global LLBUTTON_H_PAD; is_toggle
false,scale_image true,commit_on_return true,commit_on_capture_lost false,
display_pressed_state true,use_draw_context_alpha true,draw_focus_border true,
hover_hand_cursor false,button_flash_enable false. Named fields without explicit
values: selected label, ordinary/selected/hover/disabled/pressed images, overlay,
top/bottom image padding, all label colors,image/disabled image colors,flash/alternate
flash colors,bottom label pad,click/down/up/held/toggle callbacks,hover glow,badge,
right-mouse policy,held delay,flash count/rate,checkbox control. Wrapper constructors
decide implicit defaults. Add synonym toggle for is_toggle; changeDefault(initial_value,
LLSD(false)). image_flash member initialization is not explicitly named in this list;
its header/default wrapper remains an obligation.
**2.** CPU declaration schema/default policy, neutral font/image/action identities.
**3.** Preserve provided flags, globals and typed defaults; no device allocation in
new native declaration types. Check overridden skin defaults, implicit versus explicit
false, initial value provided state and first-time font construction.
Outgoing: base/wrapper/value constructors, changeDefault/addSynonym, globals and templates.

## UI-BUTTON-017: LLButton constructor

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L143).
**1.** LLUICtrl(p), LLBadgeOwner(getHandle()). Initialize frame/held count0,flashing
false,alternate flash false,glow0,highlight false; labels,font pointer,held time/frame
delays and all GL image owners from Params. Copy each checkpoint native alias only
if corresponding vk_image_name provided, else empty. Read hover images,label/image/
flash/overlay colors, overlay alignment via hAlignFromName,padding/spacing/rightDelta,
toggle/scale/shadow/autoresize/ellipsis/font-color flags,alignment,label padding,
hover glow and commit policies. fadeWhenDisabled=false,forcePressed=false; display
pressed from Params,last drawn count0; down/up/held/toggle signals null; alpha/focus/
cursor/right-mouse policy from Params; flash timer and checkbox panel null, checkbox
control string copied. Member initialization order follows header declarations;
timers/cache/base default constructors are separate dependencies.

Body sequence: flash enabled -> new LLFlashTimer(null callback,count if provided
else0,rate if provided else0); else copy flash count/rate into deferred fields.
Construct static cached UIButtonOrigHPad(setting fallback0) and static copy of
factory default Button Params. Absent selected label -> copy ordinary label.
If rect.right>=0 AND width>0 AND available padded width<font.getWidth(' '), replace
both horizontal pads with cached original pad; glyph measurement happens during
construction. Stop mouse-down timer.
Custom ordinary image compared to defaults: if disabled image equals default,
replace with ordinary image and enable fade; default pressed-selected -> ordinary.
Custom selected image: analogous disabled-selected replacement/fade and default
pressed -> selected. Then absent pressed -> selected and absent pressed-selected
-> ordinary regardless preceding comparisons. Warn if ordinary image null.
Provided click callback -> initCommitCallback/setCommitCallback; provided mouse
down/up/held -> initCommitCallback and respective setters; provided toggle ->
initEnableCallback/setIsToggledCallback. Provided badge -> initBadgeParams.
No callback-safe construction transaction or GL-free owner boundary in this body.
**2.** CPU native control construction, defaults, timer/action subscription and layout;
image/font resource versions publish separately. **3.** Replace GL-owning defaults
with audited semantic identities and inject lifecycle-owned CPU services. Late native
draw selection cannot remove these constructor dependencies. Timer/default-static
lifetimes must be reconciled with backend selection and partial failure.
Checks: first/default/custom-image construction, selected-label omission, narrow
button space metric, optional flash fields, callback resolution and badge failure.
Outgoing: all bases/members, Params/value comparisons, font/default registry,
cached settings, LLFlashTimer, handle, callbacks and badge construction/destruction.

## UI-BUTTON-018: LLButton destructor

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L329).
**1.** Delete button down/up/held/toggle signals in order; flash timer if nonnull
unset(), NOT direct delete. Members/bases subsequently destroy; commit signal owned
by base. Timer self-removal/deferred deletion cannot be inferred from unset name.
**2.** CPU callback/timer shutdown and control release; native GPU uses outlive CPU
cache owners as needed. **3.** Explicit cancellation/teardown graph, no resource reuse
until all uses complete. Check callback-held references, unset timing and partial init.
Outgoing: signal destruction, LLFlashTimer::unset, members/font cache/base owners.

## UI-BUTTON-019: LLButton::onCommit

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L345).
**1.** Down signal(this,empty LLSD) if present, then up signal. Sound flags separately
gate UISndClick and UISndClickRelease. Toggle if mIsToggle. Qualified LLUICtrl::onCommit
LAST, explicitly allowing destruction there. Earlier signals/toggle callbacks still
have no local lifetime guard. **2.** CPU programmatic click action with sound and
toggle ordering. **3.** Native actions preserve this route separately from keyboard
and physical mouse paths, not dispatch all through onCommit indiscriminately.
Check both sounds, toggle, event ordering and destruction at each callback boundary.
Outgoing: signals/LLSD, sound service, toggleState and base commit targets.

## UI-BUTTON-020: LLButton::handleUnicodeCharHere

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L719).
**1.** Space AND !keyboard.getKeyRepeated(space): optionally toggle, qualified
LLUICtrl::onCommit, handled=true. Other inputs false. No sound/button down/up signals
in this body. No local focus/enabled check; dispatch caller responsible.
**2.** CPU character activation. **3.** Preserve repeat and dispatch eligibility
independently of native key events; avoid duplicate activation from key+text input.
Check space repeat and base commit versus button onCommit path.
Outgoing: keyboard repeat, toggle, base commit, dispatch focus/enabled policy.

## UI-BUTTON-021: LLButton::handleKeyHere

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L737).
**1.** commitOnReturn AND key RETURN AND mask NONE AND !repeat: optional toggle,
handled=true then base commit. Else false. No button down/up or sound locally.
**2.** CPU keyboard action. **3.** Preserve exact modifier/repeat/parent propagation;
panel default-button logic is a different route. Check all gates and destruction.
Outgoing: keyboard repeat, toggle/base commit and parent key dispatch.

## UI-BUTTON-022: LLButton::handleMouseDown

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L755).
**1.** If childrenHandleMouseDown false: set capture=this before focus; tabStop AND
!chrome -> setFocus(true). Nonempty function name -> debug and UIUsage logCommand/
logControl(pathname). Qualified base handleMouseDown (also emits base mouse signal),
eventRecorder.updateMouseEventInfo(x,y,-55,-55,path), then button down signal with
empty LLSD. Start timer, save frame count cast S32, heldCount0. Sound MOUSE_DOWN
-> UISndClick. Child handled skips these. Always true; base return ignored.
**2.** CPU capture/focus/action/event recording and held-repeat start.
**3.** Native event ownership must preserve preemptive capture and separate base/
button signal signatures; callback deletion/reentry requires explicit lifetime model.
Check child handled, chrome/tabStop, both down signals, timer after callbacks and
capture-lost reentry. Outgoing: child dispatch, focus manager, usage/event services,
base/button callbacks, timer/frame and sound.

## UI-BUTTON-023: LLButton::handleMouseUp

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L796).
**1.** Has capture: resetMouseDownTimer BEFORE release capture to avoid extra
commit-on-capture-lost. Base handleMouseUp, recorder with -55 coordinates, button up
signal regardless release location. If pointInView: optional release sound, optional
toggle, base commit LAST. No capture -> childrenHandleMouseUp only. Always true.
No local enabled test or lifetime guard after up callbacks.
**2.** CPU release/action semantics with cancellation outside bounds. **3.** Preserve
timer-before-capture ordering and up-versus-commit distinction in native input.
Check release outside, capture loss, callbacks mutating geometry before hit test,
disabled transitions and deleted owner. Outgoing: timer reset,capture lost callback,
base/child dispatch, recorder, pointInView,toggle/sound/commit.

## UI-BUTTON-024: LLButton::handleRightMouseDown

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L843).
**1.** handleRightMouse AND child dispatch false -> capture this, tabStop&&!chrome
focus true, base handleRightMouseDown. Does not start button down timer/signals or
sound here. Always true even when handling flag false. **2.** CPU right-button
capture routing. **3.** Preserve consumed-result behavior and separate route.
Check flag false, child handled and focus callback. Outgoing: child/base/focus/capture.

## UI-BUTTON-025: LLButton::handleRightMouseUp

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L872).
**1.** handleRightMouse true: if capture release it; else child right-up dispatch.
Then base right-up ALWAYS within flag branch. No direct click/toggle/sound here.
Always true including flag false. **2.** CPU right-button release propagation.
**3.** Keep child and base dual dispatch when uncaptured, not left-up behavior.
Check captured/uncaptured/disabled flag, capture-lost callback and child mutation.
Outgoing: capture manager, child/base right-up and their callbacks.

## UI-BUTTON-026: LLButton::postBuild

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L469).
**1.** autoResize first. Nonempty checkboxControl -> createFromFile panel_button_checkbox.xml
with this parent and default child registry. Success reshape to button size, find
LLCheckBoxCtrl 'check_control'; found -> setControlName(name,null context), missing
warn. Panel creation failure warns. Always addBadgeToParentHolder then return base
postBuild bool. Does not fail solely for missing checkbox or prevent repeated panel
creation on repeated postBuild. **2.** CPU nested declarations/settings/actions/layout.
**3.** Native control construction must cover checkbox and badge child lifetimes,
not render only a button background/label. Check missing file/control, repeated
postBuild, setting lookup context and parent ownership.
Outgoing: autoResize, factory/panel/checkbox/settings, badge owner/base postBuild.

## UI-BUTTON-027: LLButton::dirtyRect

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L713).
**1.** Base dirtyRect then font buffer reset. **2.** CPU damage and retained-text
invalidation. **3.** Native prepared records need equivalent invalidation dependencies,
not just rect inequality. Check same-shape dirtiness and callback ordering.
Outgoing: base invalidation, reset, all callers/overrides.

## UI-BUTTON-028: LLButton::onVisibilityChange

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L501).
**1.** Reset font buffer BEFORE qualified base visibility notification; return void
base expression. **2.** CPU visibility/cache invalidation. **3.** Keep preparation
invalidation before callbacks that may query rendered state. Check hide/show/repeated.
Outgoing: reset/base and callbacks.

## UI-BUTTON-029: LLButton::setFlashing

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1363).
**1.** Assign force flag. Timer exists: assign flashing bool and startFlashing or
stopFlashing even if same bool. No timer: only changed bool assigns and frameTimer.reset.
Always assign alternate-color flag at end. No font-buffer reset locally.
**2.** CPU animation/timer policy. **3.** Native time-state must preserve restart
versus no-op distinctions and selection cancellation, independently of GPU frames.
Check same bool with/without timer, force and alternate transitions.
Outgoing: timer start/stop/reset, draw consumption, constructor and timer ownership.

## UI-BUTTON-030: LLButton::toggleState

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1382).
**1.** flipped=!getToggleState, setToggleState(flipped), return originally computed
flipped even if nested callback changes final model again. **2.** CPU action result.
**3.** Preserve result versus post-callback state distinction; avoid assuming returned
bool describes final model after reentrancy. Check nested toggle callbacks.
Outgoing: toggle getter/setter and model notifications.

## UI-BUTTON-031: LLButton::setLabel(string)

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1390).
**1.** Chained assignment selected=string then unselected=selected result; reset
font buffer, no autoResize locally. **2.** CPU localized label state/cache invalidation.
**3.** Native label model preserves argument formatting and assignment semantics,
not raw bytes only. Check both labels, arguments, UTF8 and later resize timing.
Outgoing: LLUIString assignment and buffer reset.

## UI-BUTTON-032: LLButton::setLabel(LLUIString)

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1396).
**1.** Chained copy selected then unselected; reset. **2.** CPU formatted-label
ownership. **3.** Preserve source text/arguments/cache copy semantics after LLUIString
audit. Check argument maps and independent future edits. Outgoing: assignment/reset.

## UI-BUTTON-033: LLButton::setLabel(LLStringExplicit)

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1402).
**1.** setLabelUnselected then setLabelSelected, hence two resets. **2.** CPU labels.
**3.** Native logical update can coalesce cache work only after proving no consumer
observes intermediate state. Check both label values and reset side effects.
Outgoing: both setters.

## UI-BUTTON-034: LLButton::setLabelArg

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1409).
**1.** unselected.setArg then selected.setArg, reset, return true unconditionally.
**2.** CPU localization/format invalidation. **3.** Native retains source/format
arguments and recomputes layout without GL; return is not proof a token existed.
Check missing token, unicode replacement and both variants. Outgoing: LLUIString/reset.

## UI-BUTTON-035: LLButton::setLabelUnselected

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1417).
**1.** Assign unselected label, reset unconditionally. **2.** CPU source mutation.
**3.** Invalidate prepared text even while selected if cache may later switch.
Check equal value/inactive label. Outgoing: assignment/reset.

## UI-BUTTON-036: LLButton::setLabelSelected

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1423).
**1.** Assign selected label, reset unconditionally. **2.** CPU source mutation.
**3.** Same ownership rule with selected variant. Check inactive mutation and toggle.
Outgoing: assignment/reset.

## UI-BUTTON-037: LLButton::onMouseCaptureLost

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1680).
**1.** commitOnCaptureLost AND mouseDownTimer started -> button up signal if present,
optional toggle, qualified base commit. ALWAYS resetMouseDownTimer afterward. Unlike
physical mouse-up, base commit is not last access to this object. Potential deletion
in callbacks is an open lifetime defect, not native behavior to emulate.
**2.** CPU capture-transfer action/cancellation. **3.** Native capture state updates
before notifications with stable/weak control identities; no dereference after an
action can destroy its owner. Preserve intended single-commit sequence while testing
normal release's pre-reset suppression separately from unexpected capture loss.
Check timer stopped/started, policy false/true, callback deletion/reentrant capture,
one versus duplicate toggle. Outgoing: timer flag, signals, toggle/base commit/reset.

## UI-BUTTON-038: LLButton::resetMouseDownTimer

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1787).
**1.** stop timer then reset timer. Does not reset held count or saved frame field.
**2.** CPU repeat/activation state. **3.** Preserve stopped status when resetting
elapsed time; a generic restart helper is not equivalent. Check started flag after
reset and next capture-loss handling. Outgoing: LLFrameTimer::stop/reset.

## UI-BUTTON-039: LLButton::handleDoubleClick

Source: [llbutton.cpp](../../../indra/llui/llbutton.cpp#L1793).
**1.** Return handleMouseDown(x,y,mask); no double-click-specific signal in this body.
**2.** CPU second-press input. **3.** Native input must not invoke both this and
another synthetic down for one double-click event. Check dispatch/capture sequence.
Outgoing: virtual mouse-down target and window event mapping.

## UI-CALLBACK-001: LLUICtrl::initCommitCallback

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L284).
**1.** Provided function: provided parameter -> bind(function,_1,cb.parameter), else
return function. No explicit function: read name, setFunctionName(name), scoped
CommitCallbackRegistry lookup. Found -> parameter provided binds(*function,_1,param),
else slot copy. Not found/nonempty name warns. Fallback default_commit_handler.
Explicit function path does not set function-name metadata. Parameter wrapper
capture/conversion remains a typed binding obligation, not assumed eager LLSD copy.
**2.** CPU native action resolution with optional parameter override and live source
control argument. **3.** Resolve audited action targets under preparation scope,
retain callable/connection lifetime, no GPU callback invocation or raw GL control
capture. Preserve named action metadata and missing-callback diagnostics.
Check explicit/named/missing/empty, provided parameter, scope override and deletion.
Outgoing: parameter wrappers, bind/slot semantics, registry, default handler and
setFunctionName/logging consumers. Every registered action target remains separate.

## UI-CALLBACK-002: LLUICtrl::initEnableCallback

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L313).
**1.** Provided function+parameter -> bind(function,THIS,param), no parameter ->
function. Otherwise EnableCallbackRegistry name lookup; found+parameter binds THIS,
else slot copy. Missing -> default_enable_handler, no missing-name warning here.
Unlike commit, bound parameter causes capture of construction-time control rather
than forwarding invocation's first argument. **2.** CPU eligibility/toggle query.
**3.** Native lifetime-safe control identity in bound predicate; preserve which
control is queried and default true behavior after missing resolution.
Check invoking with different control, provided override, empty function and scope.
Outgoing: registry, bind/slot, raw-this lifetime and default-enable handler.

## UI-CALLBACK-003: LLUICtrl::onCommit

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L434).
**1.** Only nonnull commit signal: nonempty function name logs command/control
pathname through UIUsage, then invoke signal(this,getValue()). No validation-signal
check or settings write in this body. No post-callback access. Empty signal pointer
means no value lookup/usage log. **2.** CPU action dispatch and usage telemetry.
**3.** Preserve separate validation/commit contracts; naming a signal ValidateBeforeCommit
does not establish invocation here. Native actions use stable owner/model snapshots.
Check missing signal, value mutation, usage metadata, callbacks destroying owner.
Outgoing: getValue virtual/model, signal slot list/combiner, UIUsage/pathname.

## UI-CALLBACK-004: LLUICtrl::setValue

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L459).
**1.** mViewModel->setValue(value), no local null guard or control-variable write.
**2.** CPU shared view-model mutation. **3.** Native model must preserve sharing and
concrete model effects, not conflate with settings setter. Check shared controls,
model derived type and lifetime. Outgoing: virtual model setter and model ownership.

## UI-CALLBACK-005: LLUICtrl::getValue

Source: [lluictrl.cpp](../../../indra/llui/lluictrl.cpp#L465).
**1.** Return mViewModel->getValue() as LLSD value. **2.** CPU model snapshot/query.
**3.** Audit LLSD/model copy semantics before treating as deeply immutable native
action data. Check shared model and callback changes. Outgoing: model getter/LLSD.

## UI-CAPTURE-001: LLFocusMgr::setMouseCapture

Source: [llfocusmgr.cpp](../../../indra/llui/llfocusmgr.cpp#L377).
**1.** Equal pointer no-op. Different: save old, assign new pointer FIRST. Debug
handling flag logs new name/null. Old nonnull -> virtual onMouseCaptureLost; no
reassignment afterward, so reentrant transfer can replace new pointer. No new
captor notification, retain handle, platform capture API or null validation here.
**2.** CPU capture ownership and transfer event. **3.** Native input manager exposes
stable control IDs/weak references, commits state before old-owner callback, preserves
reentrancy without resurrecting destroyed owners. Platform capture remains a separate
window dependency. Check same/null/reentrant transfers and old callback observing new.
Outgoing: raw pointer lifetime, getName, old virtual target, platform callers.

## UI-FLASH-001: LLFlashTimer constructor

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L30).
**1.** LLEventTimer(period), callback copy,tickCount0,flashing/highlighted/unset=false.
Stop embedded timer. Static cached FlashCount; mFlashCount=2*(count>0 ? count :
setting), no negative/overflow clamp here. If inherited period<=0 use cached FlashPeriod.
Connect FlashCount and FlashPeriod setting signals to raw this->onUpdateFlashSettings,
discard returned connections. Does not start timer after registration.
**2.** CPU timed animation plus live settings subscription. **3.** Native timer owner
must own/disconnect subscriptions and cancellation token, with bounded tick count and
explicit teardown even when stopped. Cannot reuse raw-this lifetime unexamined.
Check explicit/default period/count, stopped new timer, setting callbacks during
destruction and integer limits. Outgoing: base timer/tracker, cached controls/settings,
signal connections/bind and callback target.

## UI-FLASH-002: LLFlashTimer::onUpdateFlashSettings

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L67).
**1.** stopFlashing; count=2*max(config FlashCount,0); period=max(config FlashPeriod,0).
Does not restart; overrides explicit constructor values after either setting change.
**2.** CPU animation configuration transition. **3.** Preserve stop-on-update and
coupled reread behavior in native subscription, with lifetime-safe cancellation.
Check either setting change, zero period, large count and previously explicit values.
Outgoing: stop, config reads/global UI lifetime and setting dispatch.

## UI-FLASH-003: LLFlashTimer::unset

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L76).
**1.** unset=true, callback=null; does NOT start/stop timer, delete object or disconnect
setting signals. **2.** CPU cancellation request. **3.** Native destruction cannot
depend on next tick for stopped timers; use owner removal and explicit subscription
teardown. Existing stopped-timer lifetime is not a feature to emulate.
Check stopped versus running at unset, later setting update and next event loop.
Outgoing: event timer updateClass/tick and raw settings callbacks.

## UI-FLASH-004: LLFlashTimer::tick

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L82).
**1.** Toggle highlighted, callback if present, preincrement tick count and if
count>=flashCount stopFlashing; return unset. Callback may mutate timer state before
count increment/check. Does not guard unset before highlight mutation.
**2.** CPU time transition/action. **3.** Native scheduler needs stable timer ownership
during callbacks and explicit canceled state; preserve active animation sequence.
Check callback stop/start/unset, final tick, zero/negative count and owner deletion.
Outgoing: callback/stop and scheduler deletion policy.

## UI-FLASH-005: LLFlashTimer::startFlashing

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L99).
**1.** flashing=true,highlighted=true,eventTimer.start; does not reset tickCount or
unset flag locally. **2.** CPU animation activation. **3.** Preserve restart semantics
only after embedded timer start audit; native action restart must not silently reset
every field. Check repeated start and start-after-unset. Outgoing: timer start.

## UI-FLASH-006: LLFlashTimer::stopFlashing

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L106).
**1.** eventTimer.stop,flashing=false,highlighted=false,tickCount0; unset unchanged.
**2.** CPU animation stop. **3.** Native cancellation and visual reset distinct from
resource retirement. Check final-tick behavior and subsequent start.
Outgoing: embedded timer stop and draw queries.

## UI-FLASH-007: LLFlashTimer::isFlashingInProgress

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L114).
**1.** Return flashing flag. **2.** CPU appearance query. **3.** Snapshot state during
preparation without advancing time. Check stopped/unset/running combinations.
Outgoing: flag writers/lifetime.

## UI-FLASH-008: LLFlashTimer::isCurrentlyHighlighted

Source: [llflashtimer.cpp](../../../indra/llui/llflashtimer.cpp#L119).
**1.** Return highlight flag. **2.** CPU appearance query. **3.** Distinguish from
hover highlight and actual flash-image enable setting. Check tick/stop sequence.
Outgoing: flag writers/lifetime.

## UI-FLASH-009: LLFlashTimer destructor

Source: [llflashtimer.h](../../../indra/llui/llflashtimer.h#L47).
**1.** Empty body; callback member/base destruct. No stored settings connections
exist in class for local disconnect. **2.** CPU subscription/timer teardown.
**3.** Explicit native connection ownership required; late raw-this signal invocation
after scheduler deletion is a lifetime risk, not compatibility requirement.
Check settings update after final tick deletion. Outgoing: callback/base/tracker dtor,
signal lifetime and actual deletion paths.

## UI-EVENTTIMER-001: LLEventTimer(period) constructor

Source: [lleventtimer.cpp](../../../indra/llcommon/lleventtimer.cpp#L40).
**1.** Default embedded event timer construction, assign supplied period. Implicit
instance tracker/base initialization remains open. No range clamp in body.
**2.** CPU scheduler registration/time state. **3.** Native lifecycle-owned scheduler
must audit registration before sharing. Check negative/zero period and tracker state.
Outgoing: timer/tracker constructors and updateClass.

## UI-EVENTTIMER-002: LLEventTimer::updateClass

Source: [lleventtimer.cpp](../../../indra/llcommon/lleventtimer.cpp#L59).
**1.** Iterate instance_snapshot; read elapsed even if stopped. Started AND elapsed>
period (strict) -> reset timer BEFORE virtual tick; tick true -> delete &timer.
No catch/retain guard beyond snapshot semantics. Stopped unset LLFlashTimer never
gets tick via this body. **2.** CPU scheduler events and deletion. **3.** Native timer
ownership must separate active tick scheduling from cancellation/deletion; stable
iteration and reentrant removal need audited tracker semantics.
Check equality threshold, stopped cancel, callback deleting other timers, new timers
during iteration, tick true and callback exceptions. Outgoing: instance_snapshot,
elapsed/started/reset, every tick override and virtual destructor.

## UI-FRAMETIMER-001: LLFrameTimer::start

Source: [llframetimer.cpp](../../../indra/llcommon/llframetimer.cpp#L52).
**1.** reset(), then mStarted=true. **2.** CPU input-repeat time state. **3.** Retain
explicit start versus reset semantics in native scheduler; no rendering API work.
Check repeated start and previously stopped timer. Outgoing: reset/frame clock.
Runtime: existing harness test4 verifies start enables, after either prior state.

## UI-FRAMETIMER-002: LLFrameTimer::stop

Source: [llframetimer.cpp](../../../indra/llcommon/llframetimer.cpp#L58).
**1.** mStarted=false only. Does not freeze/replace timestamps in this body.
**2.** CPU active-state update. **3.** Preserve differences from pause semantics;
native input cancellation cannot accidentally restart on elapsed-time reset.
Check stopped flag and retained timestamps. Outgoing: elapsed getters and reset.
Runtime: test4 verifies stopped status; timestamp behavior not tested there.

## UI-FRAMETIMER-003: LLFrameTimer::reset

Source: [llframetimer.cpp](../../../indra/llcommon/llframetimer.cpp#L63).
**1.** mStartTime=sFrameTime,mExpiry=sFrameTime; started flag unchanged. **2.** CPU
reference-time reset. **3.** Explicit state/time separation; frame-clock epoch and
update schedule need closure for repeat parity. Check running/stopped resets.
Runtime: test4 passed both cases. Outgoing: static frame time initialization/update,
expiry/elapsed consumers. This is LLFrameTimer, not LLFlashTimer's embedded LLTimer.

## UI-TRACKER-001: unkeyed LLInstanceTracker constructor

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L486).
**1.** shared_ptr<T>(static_cast<T*>(this),no-op deleter), assign weak mSelf; lock
static set and emplace shared pointer. Shared ownership is of tracking control block,
NOT object storage. **2.** CPU lifetime observation registry. **3.** Suitable as an
audited observer only; native control/submission owners need actual lifetime retention
or explicit callback validity checks, not a misleading shared_ptr type.
Check stack/heap instances and getWeak expiration after destruction. Outgoing:
LockStatic/StaticData,set allocation,no-op deleter and base construction ordering.

## UI-TRACKER-002: unkeyed LLInstanceTracker destructor

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L498).
**1.** Lock static set, erase shared_ptr from mSelf.lock(). Does not invalidate an
already-held external shared_ptr control block by magic or defer C++ object deletion.
**2.** CPU registry removal. **3.** Native callback safety requires strong owning
objects or validated handles; observer snapshots cannot keep deleted instances alive.
Check pending snapshot and held strengthened pointer separately.
Outgoing: lock/set, mSelf weak destruction and explicit object deletion callers.

## UI-TRACKER-003: snapshot_of constructor (unkeyed)

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L413).
**1.** LockStatic member acquired before mData. Copy set shared_ptr entries into
vector of WEAK pointers, unlock after population. Windows owns LockStatic through
shared_ptr and reference member; non-Windows direct member. New registrations after
snapshot absent. No rendering/model snapshot copied, only tracking identities.
**2.** CPU stable enumeration membership with liveness observation. **3.** Native
scheduler can use equivalent snapshot semantics for mutation-tolerant traversal,
but GPU versions need real completion-owned resources, not observer pointers.
Check deletion/new creation after snapshot and platform member lifetime.
Outgoing: LockStatic, vector conversions, set ordering and iterator strengthening.

## UI-TRACKER-004: snapshot_of::strengthen (unkeyed)

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L403).
**1.** Return dynamic_pointer_cast<SUBCLASS>(weak.lock()). No object storage ownership
is acquired beyond no-op-deleter tracking pointer. **2.** CPU liveness/type filtering.
**3.** Do not confuse nonnull result with protection from arbitrary explicit deletion
inside callback. Check expired pointer/derived mismatch and held-pointer deletion.
Outgoing: std weak/dynamic cast semantics, tracker lifetime and consumers.

## UI-TRACKER-005: snapshot_of::dead_skipper (unkeyed)

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L407).
**1.** Return bool(pointer). **2.** CPU iterator filtering. **3.** Native enumeration
must recheck actual owner lifetime at action boundaries where explicit deletion is
allowed. Check empty/nonempty strengthened pointers. Outgoing: iterator composition.

## UI-TRACKER-006: snapshot_of::make_iterator (unkeyed)

Source: [llinstancetracker.h](../../../indra/llcommon/llinstancetracker.h#L429).
**1.** Construct filter iterator(dead_skipper, transform iterator(iter,strengthen),
transform iterator(end,strengthen)). Returns shared_ptr values for still-live matching
types. No list mutation during construction. **2.** CPU weak snapshot traversal.
**3.** Preserve skipped dead entries, while treating explicit owner destruction within
current callback separately. Check deleted future/current entries and end iterator.
Outgoing: Boost transform/filter iterator public contracts and helper targets.

## UI-MODEL-001: LLViewModel default constructor

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L39).
**1.** mDirty=false, LLSD member default construction/base refcount. **2.** CPU scalar
model. **3.** Reuse audited neutral data/lifetime if includes and concrete model types
are decoupled; class name alone does not establish GL-free transitive construction.
Check undefined initial LLSD and dirty false. Outgoing: LLSD/LLRefCount constructors.

## UI-MODEL-002: LLViewModel(value) constructor

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L45).
**1.** Dirty false initially, then setValue(value). Constructor virtual dispatch
resolves to LLViewModel setter, not later-derived override. Final dirty true.
**2.** CPU model initialization. **3.** Preserve construction-versus-later-set semantics;
native text model initial representation must not be inferred from overridden setter.
Check derived construction and initial dirty flag. Outgoing: base/LLSD/setValue.

## UI-MODEL-003: LLViewModel::setValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L52).
**1.** Assign LLSD then dirty=true even equal value. No signal invocation in body.
**2.** CPU model mutation. **3.** Preserve dirty semantics distinct from settings
notifications; prepared state may use generation after auditing consumers.
Check equal assignment/sharing and assignment failure. Outgoing: LLSD assignment.

## UI-MODEL-004: LLViewModel::getValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L58).
**1.** Return mValue as LLSD by value, no dirty reset. **2.** CPU snapshot query.
**3.** Audit LLSD value/copy-on-write semantics before cross-thread publication.
Check read preserves dirty. Outgoing: LLSD copy/lifetime.

## UI-MODEL-005: LLTextViewModel default constructor

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L66).
**1.** LLViewModel(false),updateFromDisplay=false; string/display default empty,
displayGeneration header default-1. Base constructor sets LLSD bool false and dirty
true; does not invoke derived UTF8/display synchronization. **2.** CPU text model
initial state. **3.** Native initial value versus display representation must be
explicit, not assume default constructor equivalent to setValue('').
Check initial value/display/string/dirty/generation. Outgoing: base/LLSD/string defaults.

## UI-MODEL-006: LLTextViewModel(value) constructor

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L73).
**1.** Base(value),updateFromDisplay=false; no derived setValue body invocation.
Stored LLSD supplied while display/string start empty, generation-1. **2.** CPU
initial representation policy. **3.** Audit actual caller initialization after
construction before claiming a text display bug or normalizing native behavior.
Check supplied nonempty value and subsequent explicit setValue. Outgoing: base/callers.

## UI-MODEL-007: LLTextViewModel::setValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L81).
**1.** Base setValue; string=value.asString; display=utf8str_to_wstring(string);
increment display generation; updateFromDisplay=false. No equality gate, callbacks
or GPU work locally. Failure midway can leave representations partially updated.
**2.** CPU text/model synchronization and layout invalidation. **3.** Native text
source indices and formatted display require versioned consistent representations;
share audited conversion policy rather than a different decoder silently.
Check same value, Unicode/invalid UTF8, non-string LLSD, generation wrap and failures.
Outgoing: LLSD/converters/string assignment and display consumers.

## UI-MODEL-008: LLTextViewModel::getEditableDisplay

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L92).
**1.** Dirty=true,increment generation,updateFromDisplay=true, return mutable wide
string reference. Actual mutation after return is not observed/incremented again.
**2.** CPU editing and deferred serialization. **3.** Native preparation cannot
publish this reference as immutable; snapshot at defined editing boundary.
Check no-op access, retained mutable reference and later model reads.
Outgoing: callers' mutation/lifetime and synchronization getter.

## UI-MODEL-009: LLTextViewModel::setDisplay

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L100).
**1.** Copy wide display,increment generation,dirty=true,updateFromDisplay=true.
Do not immediately update string or LLSD. **2.** CPU edit-buffer publication.
**3.** Explicit conversion boundary preserves editing efficiency and source indices;
GPU records consume copied glyph/layout data, not mutable display reference.
Check equal text still increments, dirty and delayed getValue synchronization.
Outgoing: wide-string assignment and consumers.

## UI-MODEL-010: updateFromDisplayIfNeeded

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L115).
**1.** If flag true, const_cast model, CLEAR flag first, convert wide display to
UTF8, assign string and LLSD via chained assignment. No generation/dirty change.
If conversion/assignment throws after flag clear, no local retry marker restoration.
**2.** CPU lazy serialization, including mutation through const query.
**3.** Native model snapshot creation must run on owner thread and publish coherent
value/display versions; const pointer is not thread-safety proof. Failure policy
must be explicit rather than inheriting a half-synchronized cache.
Check no-op repeated reads, conversion failure and retained display edits.
Outgoing: converter, string/LLSD assignment, actual callers and concurrency model.

## UI-MODEL-011: LLTextViewModel::getValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L130).
**1.** updateFromDisplayIfNeeded(this), return mValue. **2.** CPU serialized model
query with potential mutation. **3.** Owner-thread preparation before immutable
native publication, not concurrent recording-time reads. Check pending/no pending edits.
Outgoing: lazy helper and LLSD copy.

## UI-MODEL-012: LLTextViewModel::getStringValue

Source: [llviewmodel.cpp](../../../indra/llui/llviewmodel.cpp#L136).
**1.** Lazy update, return const string reference. **2.** CPU text access.
**3.** Native consumers retain copied/versioned text when owner can mutate/delete.
Check reference invalidation after edits and destructor. Outgoing: helper/lifetime.

## UI-TEXT-001: LLTextBase::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L1657).
**1.** reflow first; not read-only -> updateScrollFromCursor. With scroller, convert
its content-window rectangle to this control's coordinates. Without scroller, get
visible lines(mClipPartial), union their rects in order, using isEmpty to distinguish
first rectangle; translate by document view's left/bottom.
BGVisible: resolve current transparency, compute bg_rect=visibleTextRect and intersect
with text_rect if scroller; choose readOnly background else focused/writable background.
Actual gl_rect_2d draws TEXT_RECT, not computed bg_rect, with bgColor%alpha.
Searchable highlight: similarly computes optional-intersected bg_rect but draws
TEXT_RECT with highlight background color, without local alpha modulation.
shouldClip=mClip OR scroller!=null. Regardless shouldClip, if text_rect.top>2 subtract2
from top. Construct LLLocalClipRect(text_rect,shouldClip). Inside clip scope: drawChild
scroller if present else document view; drawHighlightedBackground; if highlightsDirty
refreshHighlights; nonempty highlights -> drawHighlightsBackground; drawSelectionBackground;
drawText; drawCursor. After clip scope, set document view visible-direct=false,
qualified LLUICtrl::draw, then visible-direct=true (not restore prior value).
No local visibility restoration guard after exceptions, no native snapshots and no
assumption reflow is purely computational. Child drawing precedes text/selection.
**2.** CPU native layout, scroll/highlight/cursor state preparation and explicit
ordered clipped contributions. GPU receives immutable glyph/image/decoration data.
**3.** Split preparation from execution while preserving the source ordering and
view policy; do not call GL-coupled segment/child draws. A snapshot of current text
without running required CPU mutations is incomplete. Background clip mismatch and
forced visible restoration need reference reachability tests, not silent correction.
Checks: scroller/no scroller, read-only/focus, top<=2/>2, mClip false, empty visible
lines, overlapping highlights/selection/text/cursor, document initial visibility,
reflow altering size and callbacks deleting child/view. Numeric/raster evidence pending.
Outgoing: reflow,scroll update,coordinate/visible-line/rect helpers,transparency/color,
search highlighting, clip stack, drawChild virtual targets, highlight refresh/draw,
selection/text/cursor, visible-direct/base draw and all retained/resource lifetimes.

## UI-TEXT-002: LLTextBase::reshape

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L1628).
**1.** Width/height changed OR global forceReshape: snapshot scroller isAtBottom if
present, base reshape; if scroller AND previously bottom AND trackEnd -> goToBottom.
Then updateRects before needsReflow (unconditional within branch). No change/no force
skips everything. Reflow remains deferred. **2.** CPU layout/scroll invalidation.
**3.** Native layout transaction preserves pre-reshape scroll anchoring and update
order; recording never calls reshape. Check shrink from top, trackEnd false, forced
same dimensions, scroller mutation in base callback and duplicate invalidation.
Outgoing: scroller state/scroll setter, base reshape and callbacks, updateRects,
needsReflow and global force policy.

## UI-TEXT-003: LLTextEditor::onCommit

Source: [lltexteditor.cpp](../../../indra/llui/lltexteditor.cpp#L2446).
**1.** setControlValue(getValue()) BEFORE qualified LLTextBase::onCommit. Text model
getValue can lazily serialize display; settings write can run callbacks before commit.
No local rollback/lifetime guard. **2.** CPU edit commit/settings/action flow.
**3.** Native text editing requires exact serialized value and ordered notifications,
not just native glyph output. Check pending display edit, settings callback mutation,
validation policy and destruction. Outgoing: model,getValue,setControlValue,base commit.

## UI-TEXT-004: LLTextEditor::setEnabled

Source: [lltexteditor.cpp](../../../indra/llui/lltexteditor.cpp#L2452).
**1.** readOnly=!enabled; if differs from mReadOnly, LLTextBase::setReadOnly then
updateSegments then updateAllowingLanguageInput. Does NOT invoke base enabled setter
or assign ordinary enabled flag locally. Equal read-only no-op.
**2.** CPU editability/style/IME policy. **3.** Native controls must preserve disabled
text editor as read-only interpretation, not generic disabled event suppression.
Check selection/copy/focus while read-only, segment rebuild, IME and repeated value.
Outgoing: readOnly setter,segment update/language input and caller enabled-chain logic.

## UI-TEXT-005: LLTextBase::reflow

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L1970).
**1.** Static cached FSFontLineSpacingAdjustment default0; ALWAYS updateSegments
before testing mReflowIndex==S32_MAX and returning. Pending reflow snapshots scroller
bottom status; cursor local rect and overlap with local bounds (follow_selection).
Convert saved cursor top/bottom relative to visibleTextRect.top. First visible line:
if in line list and scrollIndex not inside its [start,end), set scrollIndex to start.
Save scroll-index local rect in same top-relative coordinates.

While reflowIndex<S32_MAX: increment pass count; >2 logs debug/breaks, leaving
pending index for later invocation. Copy start index and reset reflowIndex=S32_MAX.
WordWrap -> reshape document to visible width/current document height, potentially
causing callbacks/invalidation. Initialize segment iterator at begin,segment offset0,
lineStart0,curTop0,lineCount0; availableWidth=visible width-mHPad,remaining=available.
If existing lines: upper_bound by line end against start index; found -> resume from
that line's start/logical line number/top, getSegmentAndOffset and erase suffix.
No found -> initial segment traversal remains while existing list is not erased.
lineHeight0,segmentLineOffset=lineCount+1.

For each segment: current index=start+offset; getNumChars(wordWrap?max(0,round(remaining)):
S32_MAX,segmentOffset,currentIndex-lineStart,S32_MAX,lineCount-segmentLineOffset).
getDimensionsF32(offset,count,width,height) returns forceNewline. lineHeight=max,
remaining-=segmentWidth,offset+=count. lastChar=start+offset. Actual width=ceil(available-
remaining), getLeftOffset(actualWidth), construct line rect(left,curTop,left+width,
curTop-lineHeight). If segment not exhausted: append line, new lineStart=lastChar,
curTop-=round(lineHeight*lineSpacingMult)+lineSpacingPixels+fontSpacingAdjustment,
reset remaining/height, keep segment and offset. If exhausted LAST segment: append,
advance curTop with same spacing, break before forceNewline lineCount increment.
Otherwise exhausted with more segments: forceNewline appends and resets line/spacing;
advance segment/reset offset; segmentLineOffset=forceNewline?lineCount+1:lineCount.
At iteration end increment logical lineCount only when forceNewline, not ordinary
word wrap. No local progress guard if a segment returns zero chars indefinitely.
After segment loop updateRects; invoke every segment's updateLayout(*this). These
can invalidate/reenter layout, producing another pass under the two-pass bound.

After passes: no mouse capture AND scroller -> previously at bottom AND trackEnd:
endOfDoc. Else hasSelection AND follow_selection: get new doc cursor rect, reconstruct
old saved cursor screen relation from new visibleTextRect.top, scrollToShowRect(new,old).
Else same anchoring for scrollIndex first-character rect. Finally updateCursorXPos.
No post-scroll reflow loop, callback lifetime/exception guard or document immutability.
**2.** CPU native line layout, inline controls and scroll/cursor anchoring. GPU
consumes settled layout versions; auxiliary views must not independently advance
editing/model state (NV-05/NV-12).
**3.** Preserve explicit phased bounded reflow with typed segment preparation and
stable control handles. Reuse audited neutral metrics/algorithms; no invocation of
GL-owning segment draws during recording. A generic paragraph engine is not equivalent
without source-index, logical-line and inline-element tests. Invalid no-progress
behavior needs deterministic error policy, not an infinite native loop.
Check suffix reflow, wrap versus forced newline logical numbering, inline resize
oscillation/pass limit, count0, actual-width rounding once, spacing setting, selection/
bottom/first-line anchoring, capture suppressing scroll and cursor-X reset.
Outgoing: cached setting,updateSegments,all cursor/line/segment lookup/rect helpers,
wordWrap/left offset,segment getNumChars/getDimensionsF32/updateLayout concrete targets,
document reshape/updateRects,endOfDoc/scrollToShowRect,clock-independent layout inputs.
Local body complete; all named transitive dependencies remain open.

## UI-TEXT-006: LLTextBase::drawCursor

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L801).
**1.** Read draw-context alpha even if cursor hidden. Gate focus AND app focus AND
!readOnly. Get wide text/pointer,cursor local rect shifted x-1,segment containing
cursor; no segment returns. Read blink elapsed; visible when elapsed<CURSOR_FLASH_DELAY
OR (S32(elapsed*2)&1). Visible: overwrite mode AND no selection -> segment dimensions
for one character, width=max(CURSOR_THICKNESS,segmentWidth); else fixed thickness.
Unbind texture,set cursorColor%alpha,draw filled cursor rectangle. If overwrite,
no selection and current character!='\n': fetch segment color/style font and render
one character in rect with RGB complement of segment text color and alpha=draw context,
LEFT/current textVAlign,NORMAL,NO_SHADOW. No local terminator/index bounds checks.
Still in blink-visible branch: calcScreenRect; IME position=(screen.left+cursor.left,
screen.bottom+cursor.top), multiply UI scale each axis then S32 cast. Under LL_SDL2
add cached SDL2IMEDefaultVerticalOffset to Y. window.setLanguageTextInput(position).
No platform IME positioning on blink-hidden branch; no coordinate restore needed.
**2.** CPU cursor blink/overwrite appearance and platform IME positioning; native
GPU paints prepared cursor and optional inverted glyph under correct clip/depth.
**3.** Separate platform input update from GPU recording, preserving tested timing
or explicitly reviewing correction to blink-gated positioning. Snapshot text/segment
font and source indices before rendering; generic caret triangle is insufficient.
Check app/control focus, read-only, exact blink boundaries, overwrite newline/end,
selection, wide glyph, scale truncation, SDL2 offset and IME moves across reflow.
Outgoing: context/focus,model text,cursor/segment lookup,blink timer/constant,keyboard,
dimensions/style/font render,rect/color,screen conversion and platform window method.

## UI-TEXT-007: LLTextBase::drawText

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L884).
**1.** Get text length; <=0 and empty label returns. Else useLabel -> label wide
length. Selection bounds default-1; if selection set min/max regardless keyboard focus.
Get visible line range; empty returns. lineStart=first line start; containing segment
missing returns. Spellcheck enabled AND model wide length>2: compute range start=lineStart,
end=getLineEnd(lastLine) (lastLine is loop-exclusive below; helper interpretation open).
Only changed start/end recomputes ranges; text/version invalidation depends on callers.

Spell recomputation: clear misspell ranges, iterate segments from start. Null or
segment.start>=end breaks. Noneditable skips. Editable segment starts block at its
full start (not clamped to visible start), end=min(segment.end,rangeEnd); combine
successive editable segments without explicit adjacency equality check. Find first
alphabetic character while <full text length, not merely segment end. While wordStart<
segmentEnd, extend from start+1 over isPartOfWord OR apostrophe with alnum neighbors;
condition reads next character around apostrophe under string bounds obligations.
wordEnd>segmentEnd breaks. Valid substring bounds -> UTF8 conversion; UTF8 BYTE
length>=3 AND !checkSpelling -> append [wordStart,wordEnd). Next wordStart=wordEnd+1,
skip non-part-of-word up to segmentEnd. Store checked start/end after all segments.
Spell gate false clears ranges but does not reset checked start/end here.

Set current segment,misspell iterator=lower_bound ranges by pair(lineStart,0).
For each visible line: nextStart=-1,lineEnd=textLength; next line exists -> nextStart=
getLineStart(next),lineEnd=nextStart. Float rect from line,replace right with document
width then translate document left/bottom. SegmentStart=lineStart; while <lineEnd:
advance segments with end<=segmentStart; exhaustion warns/returns. segmentEnd=min(lineEnd,
current.end),clippedEnd=segmentEnd-current.start. If ellipses AND clippedEnd==lineEnd
(relative compared to absolute, as written) AND last visible line AND more lines
exist, subtract2 from rect.right to force ellipsis.

For misspell ranges overlapping current segment portion: if spell timer not expired
AND cursor within inclusive range, advance range iterator and skip. Otherwise clamp
range to portion. getDimensions for prefix and misspelled span; add integer textRect.left
to prefix. periods=(spanWidth+3)/6; start+=spanWidth/2-periods*3,end=start+periods*6.
baseline=int(textRect.bottom)+int(segment style font descender). Set color bytes
(255,0,0,200). While start+1<end: line(start,baseline,start+2,baseline-2); if start+3<end,
line(start+2,baseline-3,start+4,baseline-1); start+=4. No draw-context alpha applied
locally. If range extends past segmentEnd break without advancing it; else advance.
Then rect.left=currentSegment.draw(relative start,clippedEnd,ABSOLUTE selectionLeft/
Right,rect); segmentStart=clippedEnd+segment.start. After line lineStart=nextStart.
No state restoration or callback-safe segment-set mutation guard in body.
**2.** CPU visible text/spellcheck range maintenance and prepared underline/glyph/image
segments with exact source indices/painter order. Native recording does not run
spellchecking or virtual GL draw methods.
**3.** Use typed segment preparation returning advance and ordered contributions,
with model/segment generations and owner-thread spell service. Preserve selection
index domains and byte-length spell eligibility; replacing with uniformly shaped
plain strings loses inline/style/action behavior. Cache-range quirks require actual
mutation-path tests before native invalidation design is finalized.
Checks: label mode,empty lines,selection without focus,editable/noneditable blocks,
Unicode three-byte words/apostrophes,range unchanged after edit,misspelling across
segment/line boundary,timer skip,relative-versus-absolute ellipsis condition,underlines
before content and segment draw return advance. GPU/numeric parity unmeasured.
Outgoing: model/label/selection,visible-line helpers,segment lookup/dimensions/draw,
spell timer/checker/UTF8 classification/conversion,style/font metrics,GL line/color,
all concrete segment callbacks and invalidation writers.

## UI-SEGMENT-001: LLNormalTextSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4213).
**1.** end-start>0 -> drawClippedSegment(segment.start+start,segment.start+end,
selection bounds,rect). Otherwise reset pre/selection/post font buffers and width
buffer, return rect.left. Selection bounds are not translated here.
**2.** CPU text-span preparation with absolute selection indices. **3.** Native
segment interface must make source domains explicit and invalidate empty spans;
do not keep stale retained glyphs for zero-length segments.
Check positive/zero/negative span and absolute selection. Outgoing: clipped draw/reset.

## UI-SEGMENT-002: LLNormalTextSegment::drawClippedSegment

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4230).
**1.** rightX=rect.left; invisible style returns before generation/cache update.
Read current draw-context alpha,text and editor text generation. Changed generation:
store new generation,reset three font buffers and width buffer. Font from style;
ordinary color=(readOnly?style readOnlyColor:style color) % (contextAlpha*styleAlpha).
useFontBuffers decides cached versus direct rendering for EACH span, with same inputs.
Before-selection when selectionStart>segmentStart: [segmentStart,min(selectionStart,
segmentEnd)). Font render LEFT/editor VAlign,NORMAL,style shadow,span length,rightX,
editor ellipses/color flags. Update rect.left to returned rightX.
Selection overlap when selectionStart<segmentEnd AND selectionEnd>segmentStart:
[max(selectionStart,segmentStart),min(selectionEnd,segmentEnd)), style selectedColor.get
DIRECTLY, NO context/style-alpha multiplication, NO_SHADOW. Otherwise same font/flags.
Update rect.left. After-selection when selectionEnd<segmentEnd:
[max(selectionEnd,segmentStart),segmentEnd), ordinary color/style shadow and same inputs.
Return rightX. With selection bounds -1/-1, only after-selection covers ordinary span.
No local clipping scope, geometry rollback or rightX availability guarantee beyond
font return contract. Cached buffers record rightX on generation path here.
**2.** CPU ordered span/style selection and glyph preparation, with separate selected
color semantics. **3.** Preserve span boundaries/alpha/shadow per contribution in
native prepared runs; one multiplied parent tint for all text is incorrect. Content
generation invalidates this segment's buffers, but style/ellipsis flag changes need
their own caller closure. Direct GL font rendering is not neutral preparation.
Check selection before/inside/after/absent, style invisible then visible, changed
generation, cached/direct branches, selected opacity and rightX across three spans.
Outgoing: style/getWText/editor generation,useFontBuffers,font render/buffers,
color/alpha,ellipsis/color setters and cache reset callers.

## UI-SEGMENT-003: LLNormalTextSegment::getDimensionsF32

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4526).
**1.** width=height=0. numChars>0 AND start+firstChar>=0: height=cached fontHeight,
text=getWText,font=style font,width+=widthBuffer.getWidth(font,text,start+firstChar,
numChars,noPadding=true). Always return false (does not force newline).
**2.** CPU span metrics. **3.** Preserve advance-only width versus render overhang;
native measurement must not publish GPU glyph pages synchronously.
Check zero/negative ranges,noPadding,height cache versus changed font and generation.
Outgoing: text/style,font height initialization,width buffer and caller invalidation.

## UI-SEGMENT-004: LLNormalTextSegment::getOffset

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4542).
**1.** style font.charFromPixelOffset(text.c_str,segmentStart+startOffset,float(localX),
F32_MAX,numChars,round). **2.** CPU caret/picking index. **3.** Native text layout must
preserve source-index and midpoint rounding policy, not add GPU text picking.
Check local x,negative/end bounds,round flag and Unicode. Outgoing: font hit-test helper.

## UI-SEGMENT-005: LLNormalTextSegment::getNumChars

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4552).
**1.** Get text; optional style image subtracts live image width from pixel budget,
clamped>=0. startOffset=segmentStart+segmentOffset; maxChars=min(input,segmentEnd-
startOffset). Wrap mode=lineOffset0?WORD_BOUNDARY_IF_POSSIBLE:ONLY_WORD_BOUNDARIES.
Compute textLength-startOffset; inconsistent bounds log info but do not abort/clamp.
font.maxDrawableChars(text.c_str+startOffset,float(budget),maxChars,wrapMode).
If result0 AND lineOffset0 AND maxChars>0 force1 character. lastInRun=startOffset+count;
if lastInRun<segmentEnd AND lastInRun>=getLength(),increment count for EOF marker.
line_ind unused. No null/style/index/negative pointer guard in body.
**2.** CPU line-fitting/source-progress contract. **3.** Native layout preserves
first-line-character progress and EOF sentinel independently of rendering. Whole-text
replacement with generic wrapping needs exact boundary/source-index checks.
Check too-wide first glyph,remaining line word boundary,style image budget,EOF marker,
negative offset and maxChars; invalid memory access is not native compatibility.
Outgoing: text/style/image/font maxDrawableChars,getLength,wrap enums and diagnostics.

## UI-SEGMENT-006: LLNormalTextSegment::updateLayout

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4626).
**1.** Base updateLayout(editor), reset all three font buffers and width buffer.
No font-height recomputation in this body. **2.** CPU layout/cache invalidation.
**3.** Native prepared generations must include geometry invalidation, not assume
cached metrics always survive reflow. Check repeated reflow and style change.
Outgoing: base helper,buffer resets,fontHeight writers.

## UI-SEGMENT-007: LLNormalTextSegment destructor

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4207).
**1.** Disconnect image-loaded connection; then member/base destruction. This is
explicitly stronger connection ownership than the flash timer's raw callbacks.
**2.** CPU subscription/retained text release. **3.** Native image/font callbacks
must use equivalent owned cancellation and completed-use GPU retirement separately.
Check loaded callback during teardown,last style owner and buffers in flight.
Outgoing: connection semantics,callback installers,style/buffer/base destructors.

## UI-SEGMENT-008: LLOnHoverChangeableTextSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4739).
**1.** Base normal draw first; if end==segmentEnd-segmentStart reset style to normal;
return base advance. No local cache reset with style assignment. **2.** CPU transient
hover-style lifecycle plus prepared glyph output. **3.** Native preparation must
consume current hovered style then perform source-equivalent reset at final portion,
not retain hover style indefinitely or reset before drawing.
Check multiline segment,last versus partial portion,repeated views and font changes.
Outgoing: normal draw,style ownership,hover caller and invalidation.

## UI-SEGMENT-009: LLOnHoverChangeableTextSegment::handleHover

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4751).
**1.** style=editor.skipLinkUnderline?normal:hovered; then return normal-segment
handleHover. Style changes even if base hit/link test ultimately false.
**2.** CPU pointer-driven text style. **3.** Preserve event-to-preparation ordering
and link eligibility separately; no GPU callback. Check skip-underlining and miss.
Outgoing: editor setting,normal hover,style lifetime/cached font metrics.

## UI-SEGMENT-010: LLInlineViewSegment constructor

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4762).
**1.** Base(start,end),view pointer,newline flag/four pads from Params; permitsEmoji=false.
**2.** CPU inline control identity/layout. **3.** Native prepared text must carry
embedded control ownership and document child ordering, not encode as glyph.
Check view null precondition,pads/flags and base segment range.
Outgoing: Params,view owner,base ctor and document linkage.

## UI-SEGMENT-011: LLInlineViewSegment destructor

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4774).
**1.** view->die(), no null check or explicit delete here. **2.** CPU inline child
deferred destruction. **3.** Audit die queue/handle invalidation before native owner
reuse; not assume immediate deletion. Check linked/unlinked child and callbacks.
Outgoing: concrete view die/destruction queue,base/member dtors.

## UI-SEGMENT-012: LLInlineViewSegment::getDimensionsF32

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4786).
**1.** firstChar0 AND numChars0: width0; forceNewLine -> default style font lineHeight,
return true; otherwise height0. All other inputs -> width=pads+viewWidth,height=pads+
viewHeight. Return false outside forced-empty branch. Default font lookup can create
GL owners while measuring an inline element with no text.
**2.** CPU inline metrics/newline policy and audited default font metrics.
**3.** Neutral metrics service plus explicit child layout; no GL-font measurement
facade. Check empty forced line,partial segment,padding and missing view/font.
Outgoing: LLStyle default font/lineHeight,view rect and Params semantics.

## UI-SEGMENT-013: LLInlineViewSegment::getNumChars

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4816).
**1.** forceNewLine AND line_ind0 ->0. Else lineOffset!=0 AND pixels<padded viewWidth
->0. Else return segmentEnd-segmentStart, ignoring segmentOffset/maxChars. First
item fits regardless width unless forced-line branch. **2.** CPU indivisible inline
element wrapping. **3.** Preserve logical line_ind versus current-line offset meaning.
Check equality width,oversize first item,forced-newline and nonzero segment offset.
Outgoing: view dimensions,range/line counters from reflow.

## UI-SEGMENT-014: LLInlineViewSegment::updateLayout

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4835).
**1.** editor.getDocRectFromDocIndex(start); view.setOrigin(rect.left+leftPad,
rect.bottom+bottomPad). **2.** CPU document child placement. **3.** Native inline
child origin settles before rendering/picking snapshot; preserve document coordinate
domain. Check scrolled document,baseline/padding and invalidation callbacks.
Outgoing: index-to-rect,view origin setter and child ownership.

## UI-SEGMENT-015: LLInlineViewSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4841).
**1.** Return rect.left+viewWidth+leftPad+rightPad. Does NOT draw view; start/end/
selection ignored. Child is drawn through document view earlier in parent text draw.
**2.** CPU text advance only; native child contributes via prepared document order.
**3.** Avoid duplicate embedded child emission or moving it after text merely because
it appears in segment sequence. Check child painter order and empty span.
Outgoing: viewWidth,document child traversal.

## UI-SEGMENT-016: LLInlineViewSegment::linkToDocument

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4853).
**1.** editor.addDocumentChild(view). **2.** CPU tree registration. **3.** Native
ownership must preserve document versus control parent and draw order.
Check already-parented view and failure. Outgoing: addDocumentChild/reparent callbacks.

## UI-SEGMENT-017: LLInlineViewSegment::unlinkFromDocument

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4848).
**1.** editor.removeDocumentChild(view), no delete here. **2.** CPU detachment.
**3.** Separate removal from destructor die and GPU retirement. Check detached view
lifetime and active input capture. Outgoing: removeDocumentChild/callbacks.

## UI-SEGMENT-018: LLLineBreakTextSegment::getDimensionsF32

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4877).
**1.** width0,height=cached fontHeight,return true regardless requested span.
**2.** CPU explicit newline metric. **3.** Native line-break node preserves empty
line height and logical source unit rather than omitting invisible content.
Check zero span and cached font changes. Outgoing: fontHeight constructors/writers.

## UI-SEGMENT-019: LLLineBreakTextSegment::getNumChars

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4884).
**1.** Return1 regardless budget/offset/maxChars/line. **2.** CPU newline consumption.
**3.** Explicit source sentinel handling, not glyph fit. Check reflow progress/bounds.
Outgoing: segment [pos,pos+1) constructor and caller ranges.

## UI-SEGMENT-020: LLLineBreakTextSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4888).
**1.** Return rect.left, no graphics. **2.** CPU zero visual advance. **3.** Preserve
newline layout/selection without manufacturing a visible glyph.
Check selection crossing newline. Outgoing: none in body,selection background caller.

## UI-SEGMENT-021: LLImageTextSegment::getDimensionsF32

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4914).
**1.** width0,height=style.font.lineHeight; style image pointer. numChars>0 AND
image nonnull -> width=imageWidth+IMAGE_HPAD3,height=max(fontHeight,imageHeight+3).
Return false. **2.** CPU image-as-text-unit metrics. **3.** Native inline image needs
logical extent/readiness independent of font atlas allocation.
Check missing image,zero chars,oversize image and style font. Outgoing: style/font/image.

## UI-SEGMENT-022: LLImageTextSegment::getNumChars

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4928).
**1.** Null image ->1. Else lineOffset0 OR numPixels>imageWidth+3 ->1, else0.
Strict > means exact fit on nonempty line rejects. Other parameters ignored.
**2.** CPU indivisible inline image wrapping. **3.** Preserve exact threshold and
missing-image consumption; zero geometry is not absent source unit.
Check first item,exact boundary and unavailable image. Outgoing: style/image width.

## UI-SEGMENT-023: LLImageTextSegment::draw

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L4962).
**1.** start>=0 AND end<=segmentLength: color=white%editor context alpha,style image.
Nonnull -> get height then width; textCenter=S32(rect.top-rect.height/2.f),imageBottom=
textCenter-imageHeight/2 integer division; image.draw(S32(rect.left),bottom,width,
height,color),return rect.left+imageWidth+3. Otherwise return0, NOT rect.left.
No end>start test; no style color/alpha/visibility or selection behavior here.
**2.** CPU inline image geometry/advance and native sampled image contribution.
**3.** Keep per-segment color/advance contract distinct from normal text; actual
missing-image return can reset next segment position and needs reference testing.
Check empty span,missing image,negative start,odd dimensions,selection and alpha.
Outgoing: style/image/context,rect math and image draw/provider/lifetime.

## UI-TEXT-008: LLTextBase::drawSelectionBackground

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L668).
**1.** Only hasSelection AND nonempty line info: getSelectionRects,unbind texture,
read selectedBGColor RGB; alpha=(focused?FOCUSED_SELECTION_BG_ALPHA:UNFOCUSED_SELECTION_BG_ALPHA)
*drawContextAlpha, ignoring selectedBGColor alpha. Get visibleDocumentRect.
Each selection rect: scroller -> translate(visible.left-content.left,
visible.bottom-content.bottom). No scroller: vDelta0,hDelta0; TOP v=visible.top-
content.top-vPad; VCENTER v=(max(visible.height-content.top,-content.bottom)+
visible.bottom-content.bottom)/2; BOTTOM v=visible.bottom-content.bottom; other0.
LEFT h=visible.left-content.left+hPad; HCENTER h=(max(visible.width-content.left,
-content.right)+visible.right-content.right)/2; RIGHT h=visible.right-content.right;
other0. Integer arithmetic/division. Translate h/v,draw rect with selection color.
No local scissor setup; caller text draw owns clip. **2.** CPU selection geometry
and typed opacity with native solid material. **3.** Preserve focus-opacity replacement,
alignment-specific positioning and draw order; don't reuse ordinary text tint alpha.
Check unfocused search selection,configured alpha ignored,all alignments,scroller,
odd division and empty rects. Outgoing: selection rect generation,focus/constants,
context,color/rect,visibleDocumentRect and GL solid primitive.

## UI-TEXT-009: LLTextBase::drawHighlightedBackground

Source: [lltextbase.cpp](../../../indra/llui/lltextbase.cpp#L737).
**1.** Empty line info skips. getHighlightedBgRects; empty result returns before
texture unbind. Unbind,getVisibleDocumentRect. For each rect+LLUIColor: scroller
translation=(visible.left-content.left,visible.bottom-content.bottom). Otherwise
vDelta0,hDelta0; TOP v=visible.top-content.top-vPad; VCENTER v=(max(visible.height-
content.top,-content.bottom)+visible.bottom-content.bottom)/2; BOTTOM v=visible.bottom-
content.bottom. LEFT h=visible.left-content.left+hPad; HCENTER h=(max(visible.width-
content.left,-content.right)+visible.right-content.right)/2; RIGHT h=visible.right-
content.right; unknown alignments retain0. Translate,draw with provided color directly,
no context opacity/focus replacement. **2.** CPU style-highlight geometry/color.
**3.** Native contribution keeps distinct opacity policy from selection; shared
geometry calculation is possible only with all arithmetic/alignment inputs preserved.
Check transparent highlight under fading parent,all alignment modes,scroller and
multiple ordered colors. Outgoing: highlighted rectangle/style generation,LLUIColor
live value,visible-document/rect helpers and primitive caller clip.

## Executed CPU probes

Date: 2026-09-10. Source under test remains the unchanged checkpoint-derived
llinitparam/llheteromap implementation. Test harness uses working branch changes to
[llcommon/CMakeLists.txt](../../../indra/llcommon/CMakeLists.txt) only for registration.
NV-00 bounded investigation; NV-17 CPU runtime evidence; NV-18 observed quirks are
NOT native compatibility mandates and no GL oracle/tolerance has been changed.

[llinitparam_test.cpp](../../../indra/llcommon/tests/llinitparam_test.cpp) contains:

- test1: eight source-provided/destination-provided/overwrite combinations, equal-value
  merge result and base-qualified no-op fill. All passed against real parameter APIs.
- test2: first descriptor construction versus subsequent fresh instance; copied
  block choice ownership; choose versus provided; switching exposes original value.
  All passed. This does not establish reachability in every actual UI Params type.
- test3: missing mandatory parameter, valid provided value, false-provided notification
  retaining cached success, explicit invalidation restoring validation failure. Passed.
  This characterizes current caching, not an instruction to reproduce stale validation.

After loading the established LL_BUILD environment, configured the working build
with `cmake -S indra -B build-vc170-64 -DLL_TESTS=ON` and built targets using
`cmake --build build-vc170-64 --config RelWithDebInfo --target INTEGRATION_TEST_llheteromap`
and `INTEGRATION_TEST_llinitparam`. Repository POST_BUILD harness actually executed
tests: heterogeneous cache1/1 passed; parameter probes3/3 passed. These test executables
are test infrastructure, not additional viewer executables or backend launchers.
Scope still includes remaining callbacks, native design/implementation and qualification.

Additional runtime checks in the same working build:
`INTEGRATION_TEST_llframetimer` passed4/4, including the added
[stop/reset state test](../../../indra/llcommon/tests/llframetimer_test.cpp).
`INTEGRATION_TEST_llinstancetracker` passed8/8 using its existing
[snapshot deletion tests](../../../indra/llcommon/tests/llinstancetracker_test.cpp).
These exercise actual repository implementations, not independent algorithm copies.
They do not verify LLFlashTimer settings-signal disconnection or button callback
deletion safety, and do not validate native GPU ownership.

## Existing cache test

[llheteromap_test.cpp](../../../indra/llcommon/tests/llheteromap_test.cpp#L123)
test1 uses three unrelated types, mutates/retrieves names, verifies construction
order and an unordered set of destructor effects. It does not test constructor or
emplace failures, same-type recursion, concurrency, dependent destructors or any
actual UI Params/font owners. Its result cannot close those edges.

## Coverage boundary

Registered locate target is locally identified; neither complete default-child
registry enumeration nor custom panel/floater/factory callback closure is claimed.
Next controlling dependency is parameter fill/validation and type-specific builders,
not a speculative generic native widget class. Documentation checks validate links
and identifiers only; runtime construction/effect tests are still required.