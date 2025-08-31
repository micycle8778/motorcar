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
      ];

      shellHook = ''
        export PS1="(nix-shell) $PS1"
      '';
    };
  };
}
