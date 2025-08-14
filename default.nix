{ lib
, stdenv
, fetchFromGitHub
, cmake
, pkg-config
, ffmpeg-full
, nlohmann_json
, cudaPackages ? null
, libva ? null
, libvdpau ? null
, darwin ? null
, enableCuda ? stdenv.isLinux && cudaPackages != null
, enableVaapi ? stdenv.isLinux && libva != null
, enableVideoToolbox ? stdenv.isDarwin
}:

stdenv.mkDerivation rec {
  pname = "edl2ffmpeg";
  version = "1.0.0";
  
  # For local development, use current directory
  src = ./.;
  
  # For packaging from GitHub
  # src = fetchFromGitHub {
  #   owner = "blackbird-aslough";
  #   repo = "edl2ffmpeg";
  #   rev = "v${version}";
  #   sha256 = "0000000000000000000000000000000000000000000000000000";
  # };
  
  nativeBuildInputs = [
    cmake
    pkg-config
  ];
  
  buildInputs = [
    ffmpeg-full
    nlohmann_json
  ] ++ lib.optionals enableCuda [
    cudaPackages.cuda_nvcc
    cudaPackages.cuda_cudart
    cudaPackages.cuda_cccl
  ] ++ lib.optionals enableVaapi [
    libva
    libvdpau
  ] ++ lib.optionals enableVideoToolbox [
    darwin.apple_sdk.frameworks.CoreMedia
    darwin.apple_sdk.frameworks.VideoToolbox
    darwin.apple_sdk.frameworks.CoreVideo
  ];
  
  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DBUILD_TESTS=ON"
    "-DENABLE_SIMD=ON"
    "-DENABLE_GPU=ON"
    "-DUSE_SYSTEM_FFMPEG=ON"
  ] ++ lib.optionals enableCuda [
    "-DCUDA_TOOLKIT_ROOT_DIR=${cudaPackages.cuda_nvcc}"
  ];
  
  # Enable parallel building
  enableParallelBuilding = true;
  
  # Run tests during build
  doCheck = true;
  checkPhase = ''
    runHook preCheck
    ctest --output-on-failure -j $NIX_BUILD_CORES
    runHook postCheck
  '';
  
  meta = with lib; {
    description = "High-performance EDL (Edit Decision List) renderer using direct FFmpeg integration";
    longDescription = ''
      edl2ffmpeg is an open-source tool that renders EDL files to compressed video
      output by directly linking with FFmpeg libraries, eliminating the performance
      overhead of pipe-based communication. It features a clean, extensible
      architecture designed for maximum performance with support for SIMD
      optimizations and lazy evaluation of compositor instructions.
    '';
    homepage = "https://github.com/blackbird-aslough/edl2ffmpeg";
    license = licenses.mit; # Update when license is determined
    maintainers = with maintainers; [ ]; # Add maintainers here
    platforms = platforms.linux ++ platforms.darwin;
    mainProgram = "edl2ffmpeg";
  };
}