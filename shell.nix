{ pkgs ? import <nixpkgs> { 
    config = {
      allowUnfree = true;  # Enable unfree packages for CUDA support
      cudaSupport = true;  # Enable CUDA support in packages
    };
  } 
}:

let
  # Detect if we're on Linux or macOS
  isLinux = pkgs.stdenv.isLinux;
  isDarwin = pkgs.stdenv.isDarwin;
  
  # Check if CUDA is available (only on Linux with NVIDIA GPU)
  cudaAvailable = isLinux && builtins.pathExists /dev/nvidia0;
  
  # Use regular ffmpeg-full which should have CUDA support when available
  ffmpeg-cuda = pkgs.ffmpeg-full;
in
pkgs.mkShell {
  # Use gcc13 stdenv
  stdenv = pkgs.gcc13Stdenv;
  
  # Native build inputs (available during build)
  nativeBuildInputs = with pkgs; [
    # Build tools
    cmake
    gnumake
    pkg-config
    ninja  # Alternative to make
  ];
  
  # Build inputs (libraries)
  buildInputs = with pkgs; [
    # FFmpeg with CUDA support if available
    ffmpeg-cuda
    
    # JSON library
    nlohmann_json
    
    # Development tools
    gdb
    git
    valgrind
    perf-tools
  ] ++ pkgs.lib.optionals isLinux [
    # Linux GPU acceleration libraries
    libva         # Intel/AMD VAAPI
    libvdpau      # NVIDIA VDPAU
    intel-media-driver  # Intel GPU driver
    vaapiIntel    # Intel VAAPI driver
  ] ++ pkgs.lib.optionals cudaAvailable [
    # NVIDIA CUDA (only if NVIDIA GPU detected)
    cudaPackages.cuda_nvcc
    cudaPackages.cuda_cudart
    cudaPackages.cuda_cccl
    cudaPackages.cudnn
    cudaPackages.cuda_nvrtc
    nv-codec-headers  # NVIDIA codec headers for FFmpeg
  ] ++ pkgs.lib.optionals isDarwin [
    # macOS frameworks (VideoToolbox is built-in)
    darwin.apple_sdk.frameworks.CoreMedia
    darwin.apple_sdk.frameworks.VideoToolbox
    darwin.apple_sdk.frameworks.CoreVideo
    darwin.apple_sdk.frameworks.Metal
    darwin.apple_sdk.frameworks.MetalPerformanceShaders
  ];
  
  shellHook = ''
    echo "edl2ffmpeg development environment"
    echo ""
    echo "Checking environment..."
    
    # Verify tools are available
    if command -v gcc &> /dev/null; then
      echo "✓ GCC: $(gcc --version | head -n1)"
    else
      echo "✗ GCC not found!"
    fi
    
    if command -v cmake &> /dev/null; then
      echo "✓ CMake: $(cmake --version | head -n1)"
    else
      echo "✗ CMake not found!"
    fi
    
    if command -v pkg-config &> /dev/null && pkg-config --exists libavcodec; then
      echo "✓ FFmpeg: $(pkg-config --modversion libavcodec)"
    else
      echo "✗ FFmpeg libraries not found!"
    fi
    
    echo ""
    echo "GPU Acceleration Support:"
    
    # Check for NVIDIA GPU/CUDA
    if [ -e /dev/nvidia0 ] || command -v nvidia-smi &> /dev/null; then
      echo "✓ NVIDIA GPU detected (NVENC/NVDEC available)"
      if command -v nvcc &> /dev/null; then
        echo "  └─ CUDA toolkit: $(nvcc --version | grep release | sed 's/.*release //')"
      fi
      # Check FFmpeg CUDA support
      if ffmpeg -hwaccels 2>&1 | grep -q cuda; then
        echo "  └─ FFmpeg has CUDA support"
      fi
    else
      echo "✗ No NVIDIA GPU detected"
    fi
    
    # Check for Intel/AMD GPU (VAAPI)
    if pkg-config --exists libva 2>/dev/null; then
      echo "✓ VAAPI: $(pkg-config --modversion libva)"
      if [ -e /dev/dri/renderD128 ]; then
        echo "  └─ GPU device found at /dev/dri/renderD128"
      fi
    else
      echo "✗ VAAPI not available"
    fi
    
    # Check for macOS VideoToolbox
    if [ "$(uname)" = "Darwin" ]; then
      echo "✓ VideoToolbox: Available (macOS built-in)"
    fi
    
    echo ""
    echo "Build instructions:"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
    echo "  make -j$(nproc)"
    echo ""
    echo "Or use the build script:"
    echo "  ./build-nix.sh --clean"
    echo ""
  '';
  
  # Environment variables
  CXXFLAGS = "-I${pkgs.nlohmann_json}/include";
  PKG_CONFIG_PATH = pkgs.lib.concatStringsSep ":" ([
    "${ffmpeg-cuda}/lib/pkgconfig"
  ] ++ pkgs.lib.optionals isLinux [
    "${pkgs.libva}/lib/pkgconfig"
    "${pkgs.libvdpau}/lib/pkgconfig"
  ]);
  
  # Set CUDA paths if available
  CUDA_PATH = pkgs.lib.optionalString cudaAvailable "${pkgs.cudaPackages.cuda_nvcc}";
  CUDA_HOME = pkgs.lib.optionalString cudaAvailable "${pkgs.cudaPackages.cuda_nvcc}";
  
  # Enable GPU support in FFmpeg
  LIBVA_DRIVER_NAME = pkgs.lib.optionalString isLinux "iHD"; # Intel driver
  
  # Add CUDA library paths
  LD_LIBRARY_PATH = pkgs.lib.optionalString cudaAvailable 
    "${pkgs.cudaPackages.cuda_cudart}/lib:${pkgs.cudaPackages.cuda_nvrtc}/lib";
}