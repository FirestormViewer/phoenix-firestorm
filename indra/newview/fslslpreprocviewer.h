/**
 * @file fslslpreprocviewer.h
 * @brief Specialized LLScriptEditor class for displaying LSL preprocessor output
 *
 * $LicenseInfo:firstyear=2016&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2016 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_LSLPREPROCVIEWER_H
#define FS_LSLPREPROCVIEWER_H

#include "llscripteditor.h"

class FSLSLPreProcViewer : public LLScriptEditor
{
public:

    struct Params : public LLInitParam::Block<Params, LLScriptEditor::Params>
    {
        Params()
        {}
    };

    virtual ~FSLSLPreProcViewer() {};

    bool    handleKeyHere(KEY key, MASK mask ) override;
    bool    handleUnicodeCharHere(llwchar uni_char) override;

    bool    canCut() const override { return false; }
    bool    canPaste() const override { return false; }
    bool    canUndo() const override { return false; }
    bool    canRedo() const override { return false; }
    bool    canPastePrimary() const override { return false; }
    bool    canDoDelete() const override { return false; }

protected:
    friend class LLUICtrlFactory;
    FSLSLPreProcViewer(const Params& p);
};

#endif // FS_LSLPREPROCVIEWER_H
