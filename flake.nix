{
  description = "wordbooster — adaptive word completion addon for fcitx5";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        inherit (pkgs.lib) cleanSourceWith;
        src = cleanSourceWith {
          src = ./.;
          filter = name: type:
            let base = baseNameOf (toString name);
            in !(type == "directory" && (base == "build" || base == ".git"));
        };
      in {
        # Build target
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "wordbooster";
          version = "0.1.0";
          inherit src;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
            kdePackages.extra-cmake-modules
          ];

          buildInputs = with pkgs; [
            fcitx5
            sqlite
            icu
            hunspellDicts.fr-any
            hunspellDicts.en-us
            gtest
          ];

          cmakeFlags = [
            "-DENABLE_TESTS=ON"
          ];

          doCheck = true;
        };

        # Development shell — `nix develop`
        devShells.default = pkgs.mkShell {
          name = "wordbooster-dev";

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            pkg-config
            kdePackages.extra-cmake-modules
            gcc
            gdb
            clang-tools
          ];

          buildInputs = with pkgs; [
            fcitx5
            sqlite
            icu
            hunspellDicts.fr-any
            hunspellDicts.en-us
            gtest
          ];

          shellHook = ''
            export CMAKE_PREFIX_PATH="${pkgs.fcitx5}:${pkgs.kdePackages.extra-cmake-modules}:$CMAKE_PREFIX_PATH"
            echo "wordbooster dev shell"
            echo "Build:  cmake -B build -G Ninja && cmake --build build"
            echo "Test:   ctest --test-dir build --output-on-failure"
          '';
        };
      });
}
