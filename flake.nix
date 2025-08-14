{
  description = "High-performance EDL renderer with GPU acceleration support";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        
        # Package with GPU support
        edl2ffmpeg = pkgs.callPackage ./default.nix {
          enableCuda = system == "x86_64-linux";
          enableVaapi = pkgs.stdenv.isLinux;
          enableVideoToolbox = pkgs.stdenv.isDarwin;
        };
        
        # Development shell with all GPU libraries
        devShell = import ./shell.nix { inherit pkgs; };
      in
      {
        packages = {
          default = edl2ffmpeg;
          
          # Variant without GPU support
          edl2ffmpeg-minimal = pkgs.callPackage ./default.nix {
            enableCuda = false;
            enableVaapi = false;
            enableVideoToolbox = false;
          };
          
          # Variant with only VAAPI (for Intel/AMD GPUs)
          edl2ffmpeg-vaapi = pkgs.callPackage ./default.nix {
            enableCuda = false;
            enableVaapi = true;
            enableVideoToolbox = false;
          };
          
          # Variant with only CUDA (for NVIDIA GPUs)
          edl2ffmpeg-cuda = pkgs.callPackage ./default.nix {
            enableCuda = true;
            enableVaapi = false;
            enableVideoToolbox = false;
          };
        };
        
        devShells.default = devShell;
        
        apps.default = {
          type = "app";
          program = "${edl2ffmpeg}/bin/edl2ffmpeg";
        };
        
        # NixOS module for system-wide installation
        nixosModules.default = { config, lib, pkgs, ... }:
          with lib;
          let
            cfg = config.programs.edl2ffmpeg;
          in
          {
            options.programs.edl2ffmpeg = {
              enable = mkEnableOption "edl2ffmpeg EDL renderer";
              
              enableCuda = mkOption {
                type = types.bool;
                default = config.hardware.nvidia.modesetting.enable or false;
                description = "Enable NVIDIA CUDA/NVENC support";
              };
              
              enableVaapi = mkOption {
                type = types.bool;
                default = config.hardware.opengl.enable or false;
                description = "Enable Intel/AMD VAAPI support";
              };
            };
            
            config = mkIf cfg.enable {
              environment.systemPackages = [
                (pkgs.callPackage ./default.nix {
                  enableCuda = cfg.enableCuda;
                  enableVaapi = cfg.enableVaapi;
                  enableVideoToolbox = false; # Not on NixOS
                })
              ];
              
              # Enable VA-API driver
              hardware.opengl.extraPackages = mkIf cfg.enableVaapi (with pkgs; [
                intel-media-driver
                vaapiIntel
                libvdpau-va-gl
                vaapiVdpau
              ]);
            };
          };
      });
}