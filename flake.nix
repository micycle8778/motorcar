{
  description = "cs425 flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }:
    let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in
  {
    devShells.${system}.default = pkgs.mkShell {
      # disable default gcc flags
      NIX_HARDENING_ENABLE = "";

      nativeBuildInputs = with pkgs; [
        cmake
        clang-tools
        gcc
        just
        pkg-config
      ];

      buildInputs = with pkgs; [
        # deps for glfw
        wayland
        wayland-scanner
        wayland-protocols
        libxkbcommon
        xorg.libX11
        xorg.libXrandr
        xorg.libXinerama
        xorg.libXcursor
        xorg.libXi
        xorg.libXext
      ];

      shellHook = ''
        export LD_LIBRARY_PATH=${pkgs.wayland}/lib:$LD_LIBRARY_PATH
        export LD_LIBRARY_PATH=${pkgs.libxkbcommon}/lib:$LD_LIBRARY_PATH
        export PS1="(nix-shell) $PS1"
      '';
    };
  };
}
