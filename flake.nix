{
  description = "A Nix-flake-based Nix development environment";

  inputs.nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0.1.*.tar.gz";

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forEachSupportedSystem = f: nixpkgs.lib.genAttrs supportedSystems (system: f {
        pkgs = import nixpkgs { inherit system; config.allowUnfree = true;};
      });
    in
    {
      devShells = forEachSupportedSystem ({ pkgs }: {
        pkgs.mkShell =  {
          packages = with pkgs; [
            # stm32cubemx
            (pkgs.callPackage /home/kamo/Projects/Telemetry-Module-copy/stm32cubemx.nix {})

            # (pkgs.callPackage ./stm32cubemx.nix {})
            # python311Full
            # python311Packages.pip
            (python311.withPackages (ps: with ps; [
                pip
                virtualenv
                # (ps.callPackage ./stm32pio.nix {})
            ]))
            # virtualenv
            platformio
            jdk17

            clang-tools
            gnumake
          ];
        };
        shellHook = ''
          echo hello world
          TEST=ajdsijiadsij
        '';
      });
    };
}

