#include "engine.h"

#include "text_utils.h"

#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/candidatelist.h>
#include <fcitx/addonmanager.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx/inputmethodentry.h>

#include <filesystem>

namespace {

// Modifier-only or navigation-only keys that must NOT flush the preedit
// buffer when pressed alone. fcitx5 sometimes delivers a press event for
// a modifier on its own (e.g. user holds Shift before pressing the next
// letter), and treating it as "any other key" would commit the partial
// word and break typing of capitalised words.
bool isModifierOrNavSym(uint32_t sym) {
    switch (sym) {
        case FcitxKey_Shift_L: case FcitxKey_Shift_R:
        case FcitxKey_Control_L: case FcitxKey_Control_R:
        case FcitxKey_Alt_L: case FcitxKey_Alt_R:
        case FcitxKey_Meta_L: case FcitxKey_Meta_R:
        case FcitxKey_Super_L: case FcitxKey_Super_R:
        case FcitxKey_Hyper_L: case FcitxKey_Hyper_R:
        case FcitxKey_Caps_Lock: case FcitxKey_Shift_Lock:
        case FcitxKey_Num_Lock: case FcitxKey_Scroll_Lock:
        case FcitxKey_ISO_Level3_Shift: // AltGr
        case FcitxKey_ISO_Level5_Shift:
            return true;
        default:
            return false;
    }
}

}

static std::filesystem::path historyDbPath() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg) return std::filesystem::path(xdg) / "wordbooster" / "history.db";

    const char* home = std::getenv("HOME");
    if (home) return std::filesystem::path(home) / ".local/share" / "wordbooster" / "history.db";

    return std::filesystem::path("/tmp") / "wordbooster" / "history.db";
}

WordBoosterEngine::WordBoosterEngine(fcitx::Instance* instance)
    : instance_(instance) {

    const char* home = std::getenv("HOME");
    std::vector<std::string> candidates;
    if (home) {
        candidates.push_back(std::string(home) + "/.nix-profile/share/hunspell/fr_FR.dic");
        candidates.push_back(std::string(home) + "/.nix-profile/share/hunspell/fr-toutesvariantes.dic");
        candidates.push_back(std::string(home) + "/.nix-profile/share/hunspell/en_US.dic");
    }
    candidates.push_back("/run/current-system/sw/share/hunspell/fr_FR.dic");
    candidates.push_back("/run/current-system/sw/share/hunspell/en_US.dic");

    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            dict_.loadWordlist(p);
        }
    }

    history_ = std::make_unique<HistoryStore>(historyDbPath());
    suggester_ = std::make_unique<Suggester>(dict_, *history_);
}

std::vector<fcitx::InputMethodEntry>
WordBoosterEngine::listInputMethods() {
    // Bilingual fr+en addon. LangCode is left empty so the IM is not hidden
    // by fcitx5-configtool's "Only Show Current Language" filter for users
    // whose system locale is en_*.
    std::vector<fcitx::InputMethodEntry> result;
    result.emplace_back("wordbooster", "Word Booster", "", "wordbooster");
    result.back().setLabel("WB").setIcon("fcitx-keyboard");
    return result;
}

void WordBoosterEngine::keyEvent(const fcitx::InputMethodEntry&,
                                   fcitx::KeyEvent& event) {
    if (event.isRelease()) return;
    if (!enabled_) return;

    auto* ic = event.inputContext();
    const auto key = event.key();

    // Modifier-only press (Shift, Ctrl, Alt, CapsLock, AltGr, …) — do
    // nothing. fcitx5 sends a key event for the modifier itself before
    // sending the chorded key (e.g. Shift then 'A'); treating the bare
    // modifier as "any other key" would prematurely flush the preedit
    // and corrupt words typed with leading capitals or AltGr letters.
    if (isModifierOrNavSym(key.sym())) {
        return;
    }

    // Ctrl+Alt+Space → toggle engine on/off
    if (key.check(FcitxKey_space, fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt})) {
        toggleEnabled();
        event.filterAndAccept();
        return;
    }

    // Ctrl+Shift+A → toggle auto-capitalise feature
    // Using Ctrl+Shift (not Ctrl+Alt) to avoid AltGr conflict on European keyboards.
    {
        auto st = key.states();
        bool hasCtrl = st.test(fcitx::KeyState::Ctrl);
        bool hasShift = st.test(fcitx::KeyState::Shift);
        auto sym = key.sym();
        bool isA = (sym == FcitxKey_a || sym == FcitxKey_A);
        if (hasCtrl && hasShift && isA) {
            autoCapEnabled_ = !autoCapEnabled_;
            showStatus(ic, autoCapEnabled_ ? "AutoCap: ON" : "AutoCap: OFF");
            event.filterAndAccept();
            return;
        }
    }

    // Tab or Ctrl+Space → accept top suggestion (rendered with user's casing)
    bool isTabAccept = key.check(FcitxKey_Tab);
    bool isCtrlSpaceAccept = key.check(FcitxKey_space, fcitx::KeyState::Ctrl);
    if ((isTabAccept || isCtrlSpaceAccept) && !session_.empty()) {
        auto suggestions = suggester_->suggest(session_.lookupPrefix(), 1);
        if (!suggestions.empty()) {
            auto rendered = session_.renderSuggestion(suggestions[0].word);
            commit(ic, rendered);
            event.filterAndAccept();
            return;
        }
    }

    // Backspace — remove last char from session
    if (key.check(FcitxKey_BackSpace)) {
        if (!session_.empty()) {
            session_.backspace();
            updateUI(ic);
            event.filterAndAccept();
        }
        return;
    }

    // Space → commit raw buffer, let space pass through to app (don't consume).
    if (key.check(FcitxKey_space)) {
        if (!session_.empty()) {
            commit(ic, session_.rawText());
        }
        return;
    }

    // Return / Escape → commit raw buffer, consume the key.
    if (key.check(FcitxKey_Return)) {
        if (!session_.empty()) {
            commit(ic, session_.rawText());
            event.filterAndAccept();
        }
        if (autoCapEnabled_) {
            nextWordCapitalized_ = true;
        }
        return;
    }
    if (key.check(FcitxKey_Escape)) {
        if (!session_.empty()) {
            commit(ic, session_.rawText());
            event.filterAndAccept();
        }
        return;
    }

    // Any chord involving Ctrl or Alt (alone or together): flush preedit and
    // let the key reach the app. Covers Ctrl+C (copy), Alt+Tab (window
    // switch), and AltGr+E (€) — all of which the user expects to act on the
    // application, not on the booster. Ctrl+Alt+Space is already handled
    // above (toggle), and Ctrl+Shift+A is handled separately (auto-cap).
    auto states = key.states();
    bool hasCtrl = states.test(fcitx::KeyState::Ctrl);
    bool hasAlt  = states.test(fcitx::KeyState::Alt);
    if (hasCtrl || hasAlt) {
        if (!session_.empty()) {
            commit(ic, session_.rawText());
        }
        return; // do NOT filterAndAccept — let the chord reach the app
    }

    // Printable ASCII letter (any case) → accumulate in session.
    auto sym = key.sym();
    bool isLower = (sym >= FcitxKey_a && sym <= FcitxKey_z);
    bool isUpper = (sym >= FcitxKey_A && sym <= FcitxKey_Z);
    if (isLower || isUpper) {
        char c = isLower ? static_cast<char>('a' + (sym - FcitxKey_a))
                         : static_cast<char>('A' + (sym - FcitxKey_A));

        // Auto-capitalise first letter of a new word after sentence end.
        // If the user explicitly types an upper-case letter, trust their
        // intent and treat as override (don't convert).
        // Can be disabled entirely via Ctrl+Shift+A toggle.
        bool isFirstLetterOfWord = session_.empty();
        if (autoCapEnabled_ && nextWordCapitalized_ && isFirstLetterOfWord && isLower) {
            c = static_cast<char>('A' + (c - 'a'));
        }
        nextWordCapitalized_ = false;

        session_.append(c);
        updateUI(ic);
        event.filterAndAccept();
        return;
    }

    // BUG-4/5 fix: apostrophe (`'`) and hyphen-minus (`-`) are word-internal
    // characters in French ("aujourd'hui", "peut-être"). Treat them as part
    // of the word, but only after at least one letter has been typed (so a
    // bare leading apostrophe still passes through to the app for quoting).
    if ((sym == FcitxKey_apostrophe || sym == FcitxKey_minus) && !session_.empty()) {
        char c = (sym == FcitxKey_apostrophe) ? '\'' : '-';
        session_.append(c);
        updateUI(ic);
        event.filterAndAccept();
        return;
    }

    // . ! ? → commit buffer, mark sentence end for auto-cap, let punctuation
    // reach the app so it gets inserted.
    if (sym == FcitxKey_period || sym == FcitxKey_exclam || sym == FcitxKey_question) {
        if (!session_.empty()) {
            commit(ic, session_.rawText());
        }
        if (autoCapEnabled_) {
            nextWordCapitalized_ = true;
        }
        return;
    }

    // Any other key → flush session, pass key through
    if (!session_.empty()) {
        commit(ic, session_.rawText());
    }
}

void WordBoosterEngine::reset(const fcitx::InputMethodEntry&,
                                fcitx::InputContextEvent& event) {
    auto* ic = event.inputContext();
    session_.clear();
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    if (autoCapEnabled_) {
        nextWordCapitalized_ = true;
    }
}

void WordBoosterEngine::updateUI(fcitx::InputContext* ic) {
    fcitx::Text preedit;
    preedit.append(session_.rawText(), fcitx::TextFormatFlag::Underline);
    ic->inputPanel().setClientPreedit(preedit);

    auto suggestions = suggester_->suggest(session_.lookupPrefix(), 1);
    if (!suggestions.empty()) {
        auto candList = std::make_unique<fcitx::CommonCandidateList>();
        candList->setSelectionKey(
            fcitx::KeyList{fcitx::Key(FcitxKey_space, fcitx::KeyState::Ctrl)});

        for (auto& s : suggestions) {
            auto rendered = session_.renderSuggestion(s.word);
            candList->append<fcitx::DisplayOnlyCandidateWord>(fcitx::Text(rendered));
        }
        ic->inputPanel().setCandidateList(std::move(candList));
    } else {
        ic->inputPanel().setCandidateList(nullptr);
    }

    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void WordBoosterEngine::showStatus(fcitx::InputContext* ic, const std::string& msg) {
    auto candList = std::make_unique<fcitx::CommonCandidateList>();
    candList->append<fcitx::DisplayOnlyCandidateWord>(fcitx::Text(msg));
    ic->inputPanel().setCandidateList(std::move(candList));
    ic->inputPanel().setClientPreedit(fcitx::Text{});
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

void WordBoosterEngine::commit(fcitx::InputContext* ic, const std::string& text) {
    history_->recordWord(text_utils::toLower(text));
    ic->commitString(text);
    session_.clear();
    ic->inputPanel().reset();
    ic->updatePreedit();
    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
}

fcitx::AddonInstance* WordBoosterEngineFactory::create(fcitx::AddonManager* manager) {
    return new WordBoosterEngine(manager->instance());
}

FCITX_ADDON_FACTORY(WordBoosterEngineFactory);
