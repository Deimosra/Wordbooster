#pragma once

#include "dictionary.h"
#include "history_store.h"
#include "suggester.h"
#include "typing_session.h"

#include <fcitx/addonfactory.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/instance.h>

#include <memory>
#include <string>

class WordBoosterEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit WordBoosterEngine(fcitx::Instance* instance);
    ~WordBoosterEngine() override = default;

    void keyEvent(const fcitx::InputMethodEntry& entry,
                  fcitx::KeyEvent& keyEvent) override;

    void reset(const fcitx::InputMethodEntry& entry,
               fcitx::InputContextEvent& event) override;

    std::vector<fcitx::InputMethodEntry>
    listInputMethods() override;

    bool isEnabled() const { return enabled_; }
    void toggleEnabled() { enabled_ = !enabled_; }

private:
    void updateUI(fcitx::InputContext* ic);
    void commit(fcitx::InputContext* ic, const std::string& text);
    void showStatus(fcitx::InputContext* ic, const std::string& msg);

    fcitx::Instance* instance_;
    Dictionary dict_;
    std::unique_ptr<HistoryStore> history_;
    std::unique_ptr<Suggester> suggester_;
    bool enabled_ = true;

    // Per-context state stored via InputContextProperty is overkill for
    // a first version — we use a single global session (single-user context).
    TypingSession session_;

    // Auto-capitalise the next word: true when the last committed character
    // is a sentence-ending punctuation (. ! ?) or \n (Return), or on first
    // word of a session. Reset after the first letter of the next word.
    bool nextWordCapitalized_ = true;

    // Master toggle for the auto-capitalise feature.
    // Ctrl+Shift+A toggles this at runtime (Ctrl+Shift, not Ctrl+Alt, to
    // avoid clashing with AltGr on European keyboards). Default: on.
    bool autoCapEnabled_ = true;
};

class WordBoosterEngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override;
};
