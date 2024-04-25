{
  description = "A Nix-flake-based Nix development environment";

  inputs.nixpkgs.url = "https://flakehub.com/f/NixOS/nixpkgs/0.1.*.tar.gz";
  # inputs.stm32cubemx.url = "path:./stm32cubemx";

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forEachSupportedSystem = f: nixpkgs.lib.genAttrs supportedSystems (system: f {
        pkgs = import nixpkgs { inherit system; config.allowUnfree = true;};
      });
    in
    {
      devShells = forEachSupportedSystem ({ pkgs }: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            # stm32cubemx
            (pkgs.callPackage /home/kamo/Projects/Telemetry-Module/stm32cubemx.nix {}) # nixos has an old version of cubemx
            (python311.withPackages (ps: with ps; [
                pip
            ]))
            # platformio
            jdk17
            # stlink #stlink needs to have udev rules installed (in nixos it's services.udev... [pkgs.stlink])
            picocom

            clang-tools
            gnumake
            binutils
          ];
          shellHook = ''

            if [ ! -d ".env" ]; then
              echo "installing..."
              python3 -m venv .env
              source ".env/bin/activate"
              pip install stm32pio
              echo "done"
            fi

            source .env/bin/activate
          '';   
        };
      });
    };
}

