{
  description = "edl2ffmpeg - High-performance EDL renderer using direct FFmpeg integration";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        
        edl2ffmpeg = pkgs.stdenv.mkDerivation {
          pname = "edl2ffmpeg";
          version = "1.0.0";
          
          src = ./.;
          
          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];
          
          buildInputs = with pkgs; [
            ffmpeg-full
            nlohmann_json
          ];
          
          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_TESTS=ON"
          ];
          
          # Run tests as part of the build
          doCheck = true;
          checkPhase = ''
            ctest --output-on-failure
          '';
          
          meta = with pkgs.lib; {
            description = "High-performance EDL renderer using direct FFmpeg integration";
            homepage = "https://github.com/blackbird-aslough/edl2ffmpeg";
            license = licenses.mit; # Update when license is determined
            maintainers = [ ];
            platforms = platforms.linux ++ platforms.darwin;
          };
        };
      in
      {
        packages = {
          default = edl2ffmpeg;
          edl2ffmpeg = edl2ffmpeg;
        };
        
        devShells.default = pkgs.mkShell {
          inputsFrom = [ edl2ffmpeg ];
          
          buildInputs = with pkgs; [
            # Additional development tools
            gdb
            valgrind
            perf-tools
            git
            
            # Optional tools
            clang_16
            lldb
            clang-tools # clang-format, clang-tidy
            
            # Profiling
            gperftools
            hotspot
            
            # Code analysis
            cppcheck
            include-what-you-use
          ];
          
          shellHook = ''
            echo "edl2ffmpeg development environment"
            echo ""
            echo "Available compilers:"
            echo "  - gcc ${pkgs.gcc.version}"
            echo "  - clang ${pkgs.clang_16.version}"
            echo ""
            echo "Build commands:"
            echo "  mkdir -p build && cd build"
            echo "  cmake .. -DCMAKE_BUILD_TYPE=Debug    # For development"
            echo "  cmake .. -DCMAKE_BUILD_TYPE=Release  # For performance"
            echo "  make -j$(nproc)"
            echo ""
            echo "Development commands:"
            echo "  ctest                    # Run tests"
            echo "  valgrind ./edl2ffmpeg    # Memory leak detection"
            echo "  gdb ./edl2ffmpeg         # Debugging"
            echo "  perf record ./edl2ffmpeg # Performance profiling"
            echo ""
            
            # Set default compiler to GCC
            export CC="${pkgs.gcc}/bin/gcc"
            export CXX="${pkgs.gcc}/bin/g++"
            
            # For clang users, uncomment these instead:
            # export CC="${pkgs.clang_16}/bin/clang"
            # export CXX="${pkgs.clang_16}/bin/clang++"
          '';
        };
        
        # CI/CD helpers
        checks = {
          build = edl2ffmpeg;
          
          # Format check
          format = pkgs.runCommand "format-check" {
            nativeBuildInputs = [ pkgs.clang-tools ];
          } ''
            # This would check formatting if .clang-format exists
            touch $out
          '';
        };
        
        # Nix app for direct execution
        apps.default = flake-utils.lib.mkApp {
          drv = edl2ffmpeg;
        };
      });
}