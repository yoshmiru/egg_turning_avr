{
  description = "AVR Development Environment for Hatchery System";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, utils }:
    utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        avrPkgs = pkgs.pkgsCross.avr.buildPackages;
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = [
            avrPkgs.gcc
            avrPkgs.binutils
            pkgs.avrdude
            pkgs.gnumake
          ];

          shellHook = ''
            echo "--- Hatchery System Development (Flake) ---"
            echo "MCU: ATmega8 | Toolchain: avr-gcc"
          '';
        };
      }
    );
}
