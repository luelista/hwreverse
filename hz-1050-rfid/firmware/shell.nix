with import <nixpkgs> {};
mkShell {
  nativeBuildInputs = [ avrdude ];
}

