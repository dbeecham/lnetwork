{
  inputs = {
    nixpkgs = {
      type = "github";
      owner = "NixOS";
      repo = "nixpkgs";
      rev = "c30bbcfae7a5cbe44ba4311e51d3ce24b5c84e1b";
    };
  };

  outputs = { self, nixpkgs }: {
    defaultPackage.x86_64-linux = nixpkgs.legacyPackages.x86_64-linux.stdenv.mkDerivation {

      name = "lnetwork";
      src = ./.;

      depsBuildBuild = [
        nixpkgs.legacyPackages.x86_64-linux.ragel
        nixpkgs.legacyPackages.x86_64-linux.kconfig-frontends
        nixpkgs.legacyPackages.x86_64-linux.pkgconfig
      ];

      buildInputs = [ 
        nixpkgs.legacyPackages.x86_64-linux.sqlite
        nixpkgs.legacyPackages.x86_64-linux.libnl
      ];

      installFlags = [ "DESTDIR=$(out)" "PREFIX=/" ];

    };
  };
}
