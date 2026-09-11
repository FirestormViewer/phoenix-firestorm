#ifndef LLVKLINEEDITOR_H
#define LLVKLINEEDITOR_H

#include "llvkfont.h"
#include "llvklabel.h"
#include <functional>

class LLVKLineEditor final
{
public:
    enum class Key { Left, Right, Home, End, PageUp, PageDown, Backspace, Delete, Up, Down, Return, Escape, Insert };
    struct Modifiers { bool shift = false, control = false, alt = false; };
    struct Preedit
    {
        std::u32string text, overwritten;
        std::vector<std::size_t> positions;
        std::vector<bool> standouts;
    };
    struct Params
    {
        std::string defaultText;
        std::size_t maximumBytes = 4096;
        std::size_t maximumCharacters = 0;
        std::int32_t width = 0, leftPadding = 0, rightPadding = 0;
        float scaleX = 1.f;
        bool allowEmoji = true;
        bool password = false;
        bool selectOnFocus = false;
        bool tabularNumbers = false;
        bool replaceNewlinesWithSpaces = true;
    };
    static std::optional<LLVKLineEditor> create(std::shared_ptr<LLVKFont> font, const Params& params,
        const std::optional<std::string>& initial, const LLVKLabel::Context& context, std::string& error);
    bool assign(const std::string& text, bool limit, bool focused, const LLVKLabel::Context& context, std::string& error);
    bool setCursor(std::size_t position, std::string& error);
    bool resize(std::int32_t width, std::string& error);
    void setPassword(bool password) noexcept { mParams.password = password; }
    bool selectAll(std::string& error);
    bool setSelection(std::size_t start, std::size_t end, std::string& error);
    void deselect();
    bool clear(std::string& error);
    bool recall(const std::string& text, const LLVKLabel::Context& context, std::string& error);
    bool revert(const LLVKLabel::Context& context, std::string& error);
    bool insert(char32_t character, bool overwrite, bool allowRemoval, bool& limited, std::string& error);
    bool eraseSelection(std::string& error);
    bool paste(std::u32string_view input, bool replaceSelection, unsigned& limitSignals, std::string& error);
    bool erase(std::size_t begin, std::size_t count, std::string& error);
    void startSelection() noexcept;
    bool extendSelection(std::size_t position, std::string& error);
    std::size_t wordPosition(std::size_t position, bool forward) const;
    void finishSelection() noexcept { mSelecting = false; }
    void endSelection() noexcept { if (mSelecting) { mSelecting = false; mSelectionEnd = mCursor; } }
    void resumeSelection() noexcept { mSelecting = true; }
    bool point(std::int32_t localX, bool shift, const std::function<bool(std::u32string_view)>& validator, std::string& error);
    bool selectWord(std::size_t oldStart, std::size_t oldEnd,
        const std::function<bool(std::u32string_view)>& validator, std::string& error);
    bool scrollPointer(std::int32_t localX, std::size_t increment, std::string& error);
    bool resetPreedit(bool removeSelection, std::string& error);
    bool updatePreedit(std::u32string_view text, const std::vector<std::size_t>& segments,
        const std::vector<bool>& standouts, std::size_t caret, bool overwrite, std::string& error);
    bool markPreedit(std::size_t position, std::size_t length, bool overwrite, std::string& error);
    const Preedit& preedit() const noexcept { return mPreedit; }
    bool hasPreedit() const noexcept { return mPreedit.positions.size() > 1; }
    std::optional<std::int32_t> pixelPosition(std::size_t position, std::string& error) const;
    std::optional<std::size_t> hitTest(std::int32_t localX, std::string& error) const;
    const std::string& text() const noexcept { return mText; }
    const std::u32string& display() const noexcept { return mDisplay; }
    std::size_t cursor() const noexcept { return mCursor; }
    std::size_t scroll() const noexcept { return mScroll; }
    std::int32_t leftEdge() const noexcept { return mLeft; }
    std::int32_t rightEdge() const noexcept { return mRight; }
    std::size_t selectionStart() const noexcept { return mSelectionStart; }
    std::size_t selectionEnd() const noexcept { return mSelectionEnd; }
    bool selecting() const noexcept { return mSelecting; }
    bool dirty() const noexcept { return mText != mPrevious; }
    void resetDirty() { mPrevious = mText; }
private:
    bool replace(std::size_t begin, std::size_t count, std::u32string_view replacement,
                 std::size_t cursor, std::string& error);
    Params mParams;
    std::shared_ptr<LLVKFont> mFont;
    LLVKLabel mSource;
    std::string mText, mPrevious;
    std::u32string mDisplay;
    std::size_t mCursor = 0, mScroll = 0, mSelectionStart = 0, mSelectionEnd = 0;
    std::int32_t mLeft = 0, mRight = 0;
    bool mSelecting = false;
    Preedit mPreedit;
};

#endif