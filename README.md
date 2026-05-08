# Wordbooster

Adaptive Latin (French + English) word completion addon for **fcitx5**, designed for KDE Plasma 6 on Wayland.

The addon loads hunspell `fr` and `en` wordlists at startup, learns the user's vocabulary into a local SQLite database, and combines dictionary base frequency with personal usage history to score suggestions. It auto-capitalises sentence-start letters, supports a global on/off toggle, and integrates with fcitx5's native Wayland candidate window — no flashing, no surface recreation per keystroke.

## Why this exists

The original plan was to use `ibus-typing-booster` for global text completion. On KDE Plasma 6 Wayland it is **unusable**: the typing-booster engine recreates its candidate surface on every keystroke, producing a flashing window. fcitx5 uses an incremental shared panel that does not flash, but no fcitx5 addon currently provides adaptive Latin word completion (Presage was removed from nixpkgs in 2025-06; Enchant/Aspell do spell-correction, not completion). This addon fills that gap.

## Features

- Adaptive prefix completion in French and English from a single bilingual profile.
- Personal vocabulary scoring (`score = freq_dict + 10 × freq_history`) — your common words rise after ~10 uses.
- Auto-capitalisation after `.`, `!`, `?`, newline. Toggle with `Ctrl+Shift+A`.
- Global on/off toggle: `Ctrl+Alt+Space`.
- Word-internal apostrophe and hyphen support (`aujourd'hui`, `peut-être`).
- Local-only data: SQLite history at `$XDG_DATA_HOME/wordbooster/history.db`.
- Inspectable: `sqlite3 history.db "SELECT word, frequency FROM history ORDER BY frequency DESC LIMIT 20"`.

## Quick start

### NixOS / flake (recommended)

Add this repository to your `flake.nix` inputs and reference it from your fcitx5 configuration:

```nix
{
  description = "Configuration NixOS";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    wordbooster = {
      url = "github:Deimosra/wordbooster";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, wordbooster, ... }:
    nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      specialArgs = { inherit wordbooster; };
      modules = [
        ./configuration.nix
      ];
    };
}
```

Then in `configuration.nix` :

```nix
{ config, pkgs, wordbooster, ... }:

{
  i18n.inputMethod = {
    enable = true;
    type = "fcitx5";
    fcitx5.waylandFrontend = true;
    fcitx5.addons = [
      wordbooster.packages.${pkgs.system}.default
    ];
  };

  environment.systemPackages = with pkgs; [
    hunspellDicts.fr-any
    hunspellDicts.en-us
  ];
}
```

Then activate the IM in `fcitx5-configtool` (move **Word Booster** to the active list).

### Local development

```sh
nix develop
cmake -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

The learning history Database is stored at: `~/.local/share/wordbooster`

## Keyboard shortcuts

| Key                 | Action                                              |
| ------------------- | --------------------------------------------------- |
| `[A-Za-z]`          | Add letter to buffer (case preserved)               |
| `'` `-` (after a letter) | Add to buffer (compound French words)          |
| `Tab` / `Ctrl+Space`| Accept top suggestion                               |
| `Space`             | Commit raw buffer, insert space                     |
| `Backspace`         | Erase last character                                |
| `Return` / `Escape` | Commit raw buffer, consume the key                  |
| `.` `!` `?`         | Commit raw buffer, mark next word for auto-cap      |
| `Ctrl+Shift+A`      | Toggle auto-capitalisation                          |
| `Ctrl+Alt+Space`    | Toggle the addon globally on/off                    |

## Testing

```sh
ctest --test-dir build --output-on-failure
```

The pure modules (`Dictionary`, `HistoryStore`, `Suggester`, `text_utils`, `TypingSession`, `auto_cap`) are covered by GoogleTest. The fcitx5-bound `WordBoosterEngine` is validated by manual smoke tests on a running KDE Plasma 6 Wayland session.

## Known limitations

- KWin Wayland does not expose `zwp_input_method_v1`, so apps without GTK/Qt fcitx fallback (alacritty, foot) will not receive suggestions unless fcitx5 is registered as a KWin Virtual Keyboard.

## License

[LGPL-2.1+](./LICENSE) — same license as fcitx5.
