#include "llvkwidgettree.h"
#include "llsdutil.h"
#include "llstring.h"
#include <algorithm>
#include <cmath>
#include <numeric>

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createScrollList(const Params& view,
    const LLVKControl::Params& control,const ScrollListParams& params,Id parent,std::string& error)
{
    error.clear();
    if (params.rowPadding<0 || params.headingHeight<0 || params.columnPadding<0 || params.scrollbarSize<=0 ||
        params.columns.size()>128 || params.rows.size()>10000)
    { error="Native scroll list parameters exceed supported bounds"; return std::nullopt; }
    auto initial=control;
    initial.init={}; initial.initialValue.reset(); initial.valueSetting.reset();
    const auto id=createControl(view,initial,parent,error);
    if (!id) return std::nullopt;
    const auto discard=[&] { std::string ignored; if (get(*id)) erase(*id,ignored); };
    try
    {
        ScrollList state;
        state.params=std::make_shared<ScrollListParams>(params); state.columns=params.columns;
        state.lineHeight=static_cast<int>(std::ceil(control.font->metrics().lineHeight))+params.rowPadding;
        mNodes.at(*id).scrollList=std::move(state);
        Params child; child.name="Scrollbar"; child.rect={0,0,params.scrollbarSize,32};
        child.follows=Right|Top|Bottom;
        auto bar=params.scrollbar; bar.vertical=true; bar.stepSize=1;
        bar.changed=[this,owner=*id](Id,int position)
        { if (get(owner) && get(owner)->scrollList) mNodes.at(owner).scrollList->firstRow=position; };
        auto barControl=params.scrollbarControl;
        if (!barControl.font) barControl.font=control.font;
        const auto scrollbar=createScrollbar(child,barControl,bar,*id,error);
        if (!scrollbar) { discard(); return std::nullopt; }
        mNodes.at(*id).scrollList->scrollbar=*scrollbar;
        child.name="dig border"; child.mouseOpaque=false; child.visible=params.drawBorder;
        child.rect={0,0,view.rect.right-view.rect.left,view.rect.top-view.rect.bottom}; child.follows=Left|Right|Top|Bottom;
        const auto border=createBorder(child,params.border,*id,error);
        if (!border) { discard(); return std::nullopt; }
        mNodes.at(*id).scrollList->border=*border;
        if (!setScrollListRows(*id,params.rows,error)) { discard(); return std::nullopt; }
        mNodes.at(*id).control->params=control;
        if (control.valueSetting && mSettings.contains(*control.valueSetting)) selectScrollListValue(*id,mSettings.at(*control.valueSetting),true,error);
        else if (control.initialValue) selectScrollListValue(*id,*control.initialValue,true,error);
        applyControlSettings(*id);
        if (control.init.function) control.init.function(*id,control.init.parameter.value_or(LLSD()));
        if (!get(*id)) { error="Native list removed during initialization"; return std::nullopt; }
        return id;
    }
    catch (...) { discard(); throw; }
}

bool LLVKWidgetTree::setScrollListRows(Id id,std::vector<ListRow> rows,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->scrollList || rows.size()>10000) return false;
    std::size_t bytes=0,columns=get(id)->scrollList->columns.size();
    for (auto& row : rows)
    {
        if (row.cells.empty()) row.cells.push_back(row.value.asString());
        columns=std::max(columns,row.cells.size());
        for (const auto& cell : row.cells) bytes+=cell.size();
    }
    if (columns>128 || bytes>16*1024*1024) { error="Native list row data exceeds budget"; return false; }
    auto& state=*mNodes.at(id).scrollList;
    const auto before=state;
    while (state.columns.size()<columns)
    {
        const auto name=std::to_string(state.columns.size());
        state.columns.push_back({name,name});
    }
    state.rows=std::move(rows); state.anchor=state.hovered=-1;
    state.lineHeight=0;
    for (const auto& row : state.rows)
        for (std::size_t column=0; column<row.cells.size(); ++column)
        {
            const auto* style=column<row.styles.size() ? &row.styles[column] : nullptr;
            const auto font=style && style->font ? style->font : get(id)->control->params.font;
            const auto height=style && style->type==ListCellStyle::Type::Icon ?
                (style->image ? static_cast<int>(style->image->height()) : 0) : static_cast<int>(std::ceil(font->metrics().lineHeight));
            state.lineHeight=std::max(state.lineHeight,height+state.params->rowPadding);
        }
    if (!state.lineHeight) state.lineHeight=static_cast<int>(std::ceil(get(id)->control->params.font->metrics().lineHeight))+state.params->rowPadding;
    if (layoutScrollList(id,error))
    {
        if (get(id)->scrollList->sortColumns.empty())
        {
            const auto column=get(id)->scrollList->params->sortColumn;
            return column<0 || static_cast<std::size_t>(column)>=get(id)->scrollList->columns.size() ||
                sortScrollList(id,static_cast<std::size_t>(column),get(id)->scrollList->params->sortAscending,error);
        }
        const auto primary=get(id)->scrollList->sortColumns.back();
        return sortScrollList(id,primary.first,primary.second,error);
    }
    if (get(id) && get(id)->scrollList) mNodes.at(id).scrollList=before;
    return false;
}

bool LLVKWidgetTree::setScrollListCommitOnSelection(Id id,bool enabled)
{
    if (!get(id) || !get(id)->scrollList) return false;
    auto params=std::make_shared<ScrollListParams>(*get(id)->scrollList->params);
    params->commitOnSelection=enabled;
    mNodes.at(id).scrollList->params=std::move(params);
    return true;
}

bool LLVKWidgetTree::sortScrollList(Id id,std::size_t column,bool ascending,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->scrollList || column>=get(id)->scrollList->columns.size())
    { error="Native list sort column is missing"; return false; }
    auto& state=*mNodes.at(id).scrollList;
    auto order=state.sortColumns;
    std::erase_if(order,[column](const auto& entry) { return entry.first==column; });
    order.emplace_back(column,ascending);
    std::vector<std::size_t> indices(state.rows.size());
    std::iota(indices.begin(),indices.end(),0);
    std::stable_sort(indices.begin(),indices.end(),[&](auto first,auto second)
    {
        for (auto key=order.rbegin(); key!=order.rend(); ++key)
        {
            if (key->first>=state.rows[first].cells.size() || key->first>=state.rows[second].cells.size()) continue;
            const auto compared=LLStringUtil::compareDict(state.rows[first].cells[key->first],state.rows[second].cells[key->first]);
            if (compared) return key->second ? compared<0 : compared>0;
        }
        return false;
    });
    std::vector<ListRow> rows;
    rows.reserve(indices.size());
    int anchor=-1,hovered=-1;
    for (const auto index : indices)
    {
        if (static_cast<int>(index)==state.anchor) anchor=static_cast<int>(rows.size());
        if (static_cast<int>(index)==state.hovered) hovered=static_cast<int>(rows.size());
        rows.push_back(state.rows[index]);
    }
    state.rows=std::move(rows); state.sortColumns=std::move(order);
    state.anchor=anchor; state.hovered=hovered;
    return true;
}

bool LLVKWidgetTree::layoutScrollList(Id id,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->scrollList) return false;
    const auto state=*node->scrollList;
    const auto& params=*state.params;
    const auto width=node->params.rect.right-node->params.rect.left, height=node->params.rect.top-node->params.rect.bottom;
    const auto top=height-2-(params.heading ? params.headingHeight : 0);
    if (width<4 || top<2 || state.lineHeight<=0) { error="Native list has no content area"; return false; }
    const auto page=(top-2)/state.lineHeight;
    const bool scrollbar=std::int64_t(state.lineHeight)*state.rows.size()>top-2;
    const auto right=width-2-(scrollbar ? params.scrollbarSize : 0);
    if (right<2) { error="Native list has no space beside scrollbar"; return false; }
    std::vector<int> widths;
    const auto available=std::max(0,right-2-params.columnPadding*static_cast<int>(state.columns.size() ? state.columns.size()-1 : 0));
    int remaining=available,dynamic=0;
    for (const auto& column : state.columns)
    {
        if (!std::isfinite(column.relativeWidth) || column.relativeWidth>1.f || column.width<-1)
        { error="Invalid native list column width"; return false; }
        const auto size=column.width>=0 ? column.width : column.relativeWidth>=0 ? static_cast<int>(available*column.relativeWidth) : -1;
        widths.push_back(size);
        if (size>=0) remaining-=size; else ++dynamic;
    }
    for (auto& size : widths) if (size<0) size=dynamic ? std::max(0,remaining)/dynamic : 0;
    if (!setShape(state.scrollbar,{width-2-params.scrollbarSize,2,width-2,height-2},error) ||
        !setScrollDocumentSize(state.scrollbar,static_cast<int>(state.rows.size()),error) || !setScrollPageSize(state.scrollbar,page,error)) return false;
    setVisible(state.scrollbar,scrollbar);
    if (!get(id) || !get(id)->scrollList) return false;
    auto& current=*mNodes.at(id).scrollList;
    current.content={2,2,right,top}; current.pageLines=page; current.widths=std::move(widths);
    current.firstRow=get(state.scrollbar)->scrollbar->position;
    int left=2;
    for (std::size_t column=0; column<state.columns.size(); ++column)
    {
        if (params.heading && get(id)->scrollList->headers.size()<=column)
        {
            Params view; view.name="btn_"+state.columns[column].name;
            view.rect={0,0,1,std::max(1,params.headingHeight)};
            auto control=params.header ? params.header->control : LLVKControl::Params{};
            if (!control.font) control.font=get(id)->control->params.font;
            control.tabStop=false;
            auto button=params.header ? params.header->button : LLVKButton::Params{};
            const auto wide=utf8str_to_wstring(state.columns[column].label);
            button.label=std::u32string(wide.begin(),wide.end()); button.selectedLabel.reset();
            button.images.overlay.reset(); button.labelAlign=LLVKButton::Align::Left;
            button.overlayAlign=LLVKButton::Align::Right;
            button.click=LLVKControl::Callback{};
            button.click->function=[this,id,column](Id,const LLSD&)
            {
                if (!get(id) || !get(id)->scrollList) return;
                if (!get(id)->scrollList->params->canSort) return;
                const auto& sorted=get(id)->scrollList->sortColumns;
                bool ascending=true;
                for (const auto& key : sorted) if (key.first==column) ascending=key.second;
                if (!sorted.empty() && sorted.back().first==column) ascending=!ascending;
                std::string ignored;
                sortScrollList(id,column,ascending,ignored);
            };
            const auto header=createButton(view,control,button,id,error);
            if (!header || !get(id)) return false;
            mNodes.at(id).scrollList->headers.push_back(*header);
        }
        const int next=std::clamp(left+get(id)->scrollList->widths[column]+(column+1<state.columns.size() ? params.columnPadding : 0),left,right);
        if (column<get(id)->scrollList->headers.size())
        {
            const auto header=get(id)->scrollList->headers[column];
            if (!setShape(header,{left,top,next,top+params.headingHeight},error)) return false;
            setVisible(header,params.heading && next>left);
            if (!get(id) || !get(header)) return false;
            const auto& sorted=get(id)->scrollList->sortColumns;
            auto& button=*mNodes.at(header).button;
            button.images.overlay=params.header && !state.columns[column].label.empty() && !sorted.empty() && sorted.back().first==column ?
                (sorted.back().second ? params.header->ascendingImage : params.header->descendingImage) : nullptr;
        }
        left=next;
    }
    return true;
}

bool LLVKWidgetTree::selectScrollListValue(Id id,const LLSD& requested,bool selected,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->scrollList) return false;
    auto& state=*mNodes.at(id).scrollList;
    bool changed=false,found=false;
    if (selected && !state.params->multiSelect)
        for (auto& row : state.rows) { changed|=row.selected; row.selected=false; }
    for (std::size_t index=0; index<state.rows.size(); ++index)
    {
        auto& row=state.rows[index];
        if (!row.enabled) continue;
        const bool match=requested.isBinary() ? llsd_equals(row.value,requested) : row.value.asString()==requested.asString();
        if (!match) continue;
        changed|=row.selected!=selected; row.selected=selected; found=true;
        if (selected) state.anchor=static_cast<int>(index);
        break;
    }
    if (changed)
    {
        mNodes.at(id).control->dirty=true;
        if (state.params->commitOnSelection) dispatchControl(id,&LLVKControl::Params::commit);
    }
    return found;
}

bool LLVKWidgetTree::scrollListPointer(Id id,const PointerEvent& event,std::string& error)
{
    error.clear();
    if (!layoutScrollList(id,error)) return false;
    if (childrenPointer(id,event,error)) return true;
    if (!error.empty() || !get(id)) return error.empty();
    if (event.kind==PointerKind::LeftDown || event.kind==PointerKind::DoubleClick)
    {
        if (!requestControlFocus(id,true,error)) return false;
        if (!get(id) || !get(id)->scrollList) return true;
        if (!layoutScrollList(id,error)) return false;
    }
    const bool captured=mMouseCapture==id;
    if (event.kind==PointerKind::LeftUp && captured)
    {
        if (!setMouseCapture(0,error)) return false;
        if (!get(id) || !get(id)->scrollList) return true;
        if (!layoutScrollList(id,error)) return false;
    }
    auto state=*get(id)->scrollList;
    const auto& rect=state.content;
    const bool inside=event.x>=rect.left && event.x<rect.right && event.y>=rect.bottom && event.y<rect.top;
    const auto row=inside ? state.firstRow+(rect.top-1-event.y)/state.lineHeight : -1;
    const bool hit=row>=0 && static_cast<std::size_t>(row)<state.rows.size() && state.rows[row].enabled;
    int cell=-1,left=rect.left;
    for (std::size_t column=0; column<state.widths.size(); ++column)
    {
        if (event.x>=left && event.x<left+state.widths[column]+state.params->columnPadding)
        { cell=static_cast<int>(column); break; }
        left+=state.widths[column]+state.params->columnPadding;
    }
    if (state.params->selection==ScrollListParams::Selection::Row ||
        (state.params->selection==ScrollListParams::Selection::Header && cell==0)) cell=-1;
    const auto select=[&]()
    {
        if (!hit) return true;
        auto& current=*mNodes.at(id).scrollList;
        if (current.params->multiSelect && (event.modifiers&1) && current.anchor>=0)
        {
            const auto begin=std::min(row,current.anchor),end=std::max(row,current.anchor);
            for (int index=begin; index<=end; ++index) if (current.rows[index].enabled) current.rows[index].selected=true;
        }
        else if (current.params->multiSelect && (event.modifiers&2))
        { current.rows[row].selected=!current.rows[row].selected; if (current.rows[row].selected) current.anchor=row; }
        else
        {
            for (auto& item : current.rows) item.selected=false;
            current.rows[row].selected=true; current.anchor=row;
        }
        bool changed=false;
        for (std::size_t index=0; index<state.rows.size(); ++index) changed|=state.rows[index].selected!=current.rows[index].selected;
        current.rows[row].selectedCell=cell;
        if (changed) mNodes.at(id).control->dirty=true;
        return !changed || !current.params->commitOnSelection || dispatchControl(id,&LLVKControl::Params::commit);
    };
    if (event.kind==PointerKind::LeftDown || event.kind==PointerKind::DoubleClick)
    {
        if (!select() || !get(id)) return true;
        if (hit && !setMouseCapture(id,error)) return false;
        return true;
    }
    if (event.kind==PointerKind::Hover)
    {
        mNodes.at(id).scrollList->hovered=hit ? row : -1;
        mNodes.at(id).scrollList->hoveredCell=cell;
        if (mMouseCapture==id && !event.modifiers) return select();
        return inside;
    }
    if (event.kind==PointerKind::LeftUp)
    {
        if (!get(id)) return true;
        if (captured && !event.modifiers && !select()) return false;
        if (!get(id)) return true;
        if (inside) return dispatchControl(id,&LLVKControl::Params::commit);
    }
    return false;
}

bool LLVKWidgetTree::scrollListKey(Id id,ScrollKey key,LLVKLineEditor::Modifiers modifiers,std::string& error)
{
    error.clear();
    if (modifiers.control || modifiers.shift || modifiers.alt || !layoutScrollList(id,error)) return false;
    const auto state=*get(id)->scrollList;
    if (state.rows.empty()) return false;
    int current=-1;
    for (std::size_t index=0; index<state.rows.size(); ++index) if (state.rows[index].selected) { current=static_cast<int>(index); break; }
    int target=current;
    if (key==ScrollKey::Up || key==ScrollKey::Down)
    {
        const int direction=key==ScrollKey::Up ? -1 : 1;
        target=current<0 ? (direction>0 ? 0 : static_cast<int>(state.rows.size())-1) : current+direction;
        while (target>=0 && static_cast<std::size_t>(target)<state.rows.size() && !state.rows[target].enabled) target+=direction;
    }
    else if (key==ScrollKey::Home) target=0;
    else if (key==ScrollKey::End) target=static_cast<int>(state.rows.size())-1;
    else if (key==ScrollKey::PageUp || key==ScrollKey::PageDown)
        target=std::clamp(current+(key==ScrollKey::PageUp ? -1 : 1)*std::max(1,state.pageLines-1),0,static_cast<int>(state.rows.size())-1);
    else return (key==ScrollKey::Left || key==ScrollKey::Right) && current>=0;
    if (target<0 || static_cast<std::size_t>(target)>=state.rows.size() || !state.rows[target].enabled) return true;
    for (auto& row : mNodes.at(id).scrollList->rows) row.selected=false;
    if (!selectScrollListValue(id,state.rows[target].value,true,error)) return false;
    if (!get(id)) return true;
    const auto first=target<state.firstRow ? target : target>=state.firstRow+state.pageLines ? target-state.pageLines+1 : state.firstRow;
    setScrollPosition(state.scrollbar,first,true,error);
    if (!error.empty() || !get(id)) return error.empty();
    return !state.params->commitOnKeyboard || state.params->commitOnSelection || dispatchControl(id,&LLVKControl::Params::commit);
}