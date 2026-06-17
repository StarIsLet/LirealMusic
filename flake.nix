# SPDX-License-Identifier: AGPL-3.0-or-later
# Lireal Music - C++ audio visual rendering engine.
# Copyright (C) 2026 Lireal contributors

{
  description = "Lireal Music - cute C++20 audio visual MV generator";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        rec {
          lirealMusic = pkgs.stdenv.mkDerivation {
            pname = "lireal-music";
            version = "0.1.0";
            src = self;

            nativeBuildInputs = with pkgs; [
              cmake
              ffmpeg
              fftwFloat
              git
              opencv
              ninja
              pkg-config
              qt6.qtbase
              qt6.wrapQtAppsHook
            ];

            buildInputs = with pkgs; [
              ffmpeg
              fftwFloat
              opencv
              qt6.qtbase
            ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DLIREAL_ENABLE_ONNXRUNTIME=OFF"
              "-DLIREAL_ENABLE_WEBGPU=ON"
            ];

            installPhase = ''
              runHook preInstall

              install -Dm755 lireal "$out/bin/lireal"
              install -Dm755 lireal_tui "$out/bin/lireal_tui"
              install -Dm644 "$src/README.md" "$out/share/doc/lireal-music/README.md"
              install -Dm644 "$src/LICENSE" "$out/share/licenses/lireal-music/LICENSE"
              install -Dm644 "$src/assets/themes/sakura.qss" "$out/share/lireal-music/assets/themes/sakura.qss"

              if [ -f "$src/萝莉体 第二版/loli.ttf" ]; then
                install -Dm644 "$src/萝莉体 第二版/loli.ttf" "$out/share/lireal-music/萝莉体 第二版/loli.ttf"
              fi

              runHook postInstall
            '';

            qtWrapperArgs = [
              "--prefix" "PATH" ":" "${pkgs.lib.makeBinPath [ pkgs.ffmpeg ]}"
              "--set-default" "LIREAL_ASSET_ROOT" "$out/share/lireal-music"
            ];

            meta = with pkgs.lib; {
              description = "Cute C++20 Qt/OpenCV/FFmpeg music video generator";
              homepage = "https://github.com/StarIsLet/LirealMusic";
              license = licenses.agpl3Plus;
              platforms = platforms.linux;
              mainProgram = "lireal";
            };
          };

          default = lirealMusic;
        });

      apps = forAllSystems (system: {
        lirealMusic = {
          type = "app";
          program = "${self.packages.${system}.lirealMusic}/bin/lireal";
        };
        lirealTui = {
          type = "app";
          program = "${self.packages.${system}.lirealMusic}/bin/lireal_tui";
        };
        default = self.apps.${system}.lirealMusic;
      });

      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.mkShell {
            packages = with pkgs; [
              cmake
              ninja
              pkg-config
              gcc
              ffmpeg
              fftwFloat
              opencv
              qt6.qtbase
              qt6.wrapQtAppsHook
            ];
          };
        });
    };
}
