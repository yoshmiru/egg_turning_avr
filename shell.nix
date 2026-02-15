{ pkgs ? import <nixpkgs> {} }:

let
  # AVR用のツール一式（gcc, libc, binutils）が含まれるパッケージセット
  avrPkgs = pkgs.pkgsCross.avr.buildPackages;
in
pkgs.mkShell {
  nativeBuildInputs = [
    avrPkgs.gcc          # avr-gcc
    avrPkgs.binutils     # avr-objcopy, avr-size など
    pkgs.avrdude         # 書き込みツール
    pkgs.gnumake         # Makefile
  ];

  shellHook = ''
    echo "--- AVR Development Environment ---"
    echo "MCU: ATmega8"
    echo "Compiler: $(avr-gcc --version | head -n 1)"
    echo "-----------------------------------"
  '';
}
