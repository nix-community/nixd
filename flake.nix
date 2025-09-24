{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

    flake-parts.url = "github:hercules-ci/flake-parts";

    flake-root.url = "github:srid/flake-root";

    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      nixpkgs,
      flake-parts,
      treefmt-nix,
      ...
    }@inputs:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
        inputs.flake-parts.flakeModules.easyOverlay
        inputs.flake-root.flakeModule
      ];
      perSystem =
        { config, pkgs, ... }:
        let
          inherit (pkgs)
            nixVersions
            llvmPackages_19
            callPackage
            stdenv
            ;
          nixComponents = nixVersions.nixComponents_2_30;
          llvmPackages = llvmPackages_19;
          nixf = callPackage ./libnixf {
            inherit llvmPackages nixComponents;
          };
          nixt = callPackage ./libnixt { inherit nixComponents; };
          nixd = callPackage ./nixd {
            inherit nixComponents nixf nixt;
            inherit llvmPackages;
          };
          nixdMono = callPackage ./. { inherit nixComponents llvmPackages; };
          nixdLLVM = nixdMono.override { stdenv = if stdenv.isDarwin then stdenv else llvmPackages.stdenv; };
          regressionDeps = with pkgs; [
            clang-tools
            lit
            nixfmt-rfc-style
          ];
          shellOverride = old: {
            nativeBuildInputs = old.nativeBuildInputs ++ regressionDeps;
            shellHook = ''
              export PATH="${pkgs.clang-tools}/bin:$PATH"
              export NIX_SRC=${nixComponents.src}
              export NIX_PATH=nixpkgs=${nixpkgs}
            '';
            hardeningDisable = [ "fortify" ];
          };
          treefmtEval = treefmt-nix.lib.evalModule pkgs ./treefmt.nix;
        in
        {
          packages.default = nixd;
          overlayAttrs = {
            inherit (config.packages) nixd;
          };
          packages = {
            inherit nixd nixf nixt;
          };

          devShells.llvm = nixdLLVM.overrideAttrs shellOverride;

          devShells.default = nixdMono.overrideAttrs shellOverride;

          devShells.nvim = pkgs.mkShell {
            nativeBuildInputs = [
              nixd
              pkgs.nixfmt-rfc-style
              pkgs.git
              (import ./nixd/docs/editors/nvim-lsp.nix { inherit pkgs; })
            ];
            inputsFrom = [ config.flake-root.devShell ];
            shellHook = ''
              echo -e "\n\033[1;31mDuring the first time nixd launches, the flake inputs will be fetched from the internet, this is rather slow.\033[0m"
              echo -e "\033[1;34mEntering the nvim test environment...\033[0m"
              cd $FLAKE_ROOT
              export GIT_REPO=https://github.com/nix-community/nixd.git
              export EXAMPLES_PATH=nixd/docs/examples
              export WORK_TEMP=/tmp/NixOS_Home-Manager
              if [ -d "$WORK_TEMP" ]; then
              	rm -rf $WORK_TEMP
              fi
              mkdir -p $WORK_TEMP
              cp -r $EXAMPLES_PATH/NixOS_Home-Manager/* $WORK_TEMP/ 2>/dev/null
              if [[ $? -ne 0 ]]; then
              	export GIT_DIR=$WORK_TEMP/.git
              	export GIT_WORK_TREE=/tmp/NixOS_Home-Manager
              	git init
              	git config core.sparseCheckout true
              	git remote add origin $GIT_REPO
              	echo "$EXAMPLES_PATH/NixOS_Home-Manager/" >$GIT_DIR/info/sparse-checkout
              	git pull origin main
              	cp $GIT_WORK_TREE\/$EXAMPLES_PATH/NixOS_Home-Manager/* $GIT_WORK_TREE 2>/dev/null
              	rm -rf $GIT_WORK_TREE/nixd
              fi
              cd $WORK_TEMP
              echo -e "\033[1;32mNow, you can edit the nix file by running the following command:\033[0m"
              echo -e "\033[1;33m'nvim-lsp flake.nix'\033[0m"
              echo -e "\033[1;34mEnvironment setup complete.\033[0m"
            '';
          };
          devShells.vscodium = pkgs.mkShell {
            nativeBuildInputs = [
              nixd
              pkgs.nixfmt-rfc-style
              (import ./nixd/docs/editors/vscodium.nix { inherit pkgs; })
            ];
          };
          formatter = treefmtEval.config.build.wrapper;
        };
      systems = nixpkgs.lib.systems.flakeExposed;
    };
}
