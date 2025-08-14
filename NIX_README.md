# Nix Development Environment for edl2ffmpeg

This project includes Nix configurations for reproducible development environments and builds with full GPU acceleration support.

## Quick Start

### Using Nix Shell (Traditional)

```bash
# Enter development environment
nix-shell

# Build the project
./build-nix.sh

# Or manually:
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Using Nix Flakes (Recommended)

```bash
# Enter development environment
nix develop

# Build the project
./build-nix.sh

# Or build directly with Nix
nix build
```

### Using direnv (Automatic Environment)

```bash
# Install direnv if not already installed
nix-env -iA nixpkgs.direnv

# Allow direnv for this directory
direnv allow

# The environment will now load automatically when you cd into the directory
```

## Available Nix Files

### `shell.nix`
Traditional Nix shell configuration providing:
- GCC 13 compiler
- CMake and Make
- FFmpeg libraries with GPU support
- nlohmann_json
- Development tools (gdb, valgrind, perf)
- GPU acceleration libraries:
  - NVIDIA CUDA/NVENC (auto-detected)
  - Intel/AMD VAAPI
  - macOS VideoToolbox frameworks

### `flake.nix`
Modern Nix flake providing:
- Reproducible development environment
- Package definition for edl2ffmpeg with GPU variants
- Multiple package outputs:
  - `default`: Full GPU support
  - `edl2ffmpeg-minimal`: No GPU acceleration
  - `edl2ffmpeg-vaapi`: Intel/AMD GPU only
  - `edl2ffmpeg-cuda`: NVIDIA GPU only
- NixOS module for system integration
- Additional development tools (clang, lldb, profilers)
- CI/CD helpers

### `default.nix`
Package definition for building edl2ffmpeg as a Nix package with configurable GPU support:
- `enableCuda`: Enable NVIDIA CUDA/NVENC support
- `enableVaapi`: Enable Intel/AMD VAAPI support
- `enableVideoToolbox`: Enable macOS VideoToolbox support

### `.envrc`
direnv configuration that automatically loads the Nix environment and provides helpful aliases.

## Building with Nix

### Development Build

```bash
# Using shell.nix
nix-shell --run "./build-nix.sh --debug"

# Using flake
nix develop -c ./build-nix.sh --debug
```

### Release Build

```bash
# Using shell.nix
nix-shell --run "./build-nix.sh --release"

# Using flake
nix develop -c ./build-nix.sh --release

# Or build as a Nix package
nix build
./result/bin/edl2ffmpeg --help
```

### Clean Build

```bash
./build-nix.sh --clean --release
```

## Development Workflow

1. **Enter the Nix environment:**
   ```bash
   nix-shell  # or `nix develop` for flakes
   ```

2. **Build the project:**
   ```bash
   ./build-nix.sh
   ```

3. **Run tests:**
   ```bash
   cd build && ctest
   ```

4. **Debug with GDB:**
   ```bash
   gdb ./build/edl2ffmpeg
   ```

5. **Check for memory leaks:**
   ```bash
   valgrind --leak-check=full ./build/edl2ffmpeg input.json output.mp4
   ```

6. **Profile performance:**
   ```bash
   perf record ./build/edl2ffmpeg input.json output.mp4
   perf report
   ```

## Environment Variables

The Nix environment sets:
- `PKG_CONFIG_PATH` - For finding FFmpeg and GPU libraries
- `NIX_CFLAGS_COMPILE` - Include paths for dependencies
- `NIX_LDFLAGS` - Library paths for linking
- `CUDA_PATH` - CUDA toolkit location (if available)
- `LIBVA_DRIVER_NAME` - Intel GPU driver selection

## Troubleshooting

### FFmpeg not found
The Nix environment includes `ffmpeg-full`. If CMake can't find FFmpeg, ensure you're inside the Nix shell.

### Missing dependencies
All dependencies are provided by Nix. If something is missing, check that you're using the Nix environment.

### Build fails outside Nix
The project is designed to work with system dependencies too, but the Nix environment ensures all dependencies are available with correct versions.

## Adding Dependencies

To add new dependencies:

1. Edit `shell.nix` or `flake.nix`
2. Add the package to `buildInputs`
3. Exit and re-enter the Nix environment

Example:
```nix
buildInputs = with pkgs; [
  # ... existing inputs ...
  opencv4  # Add OpenCV for image processing
];
```

## CI/CD Integration

For GitHub Actions or other CI systems:

```yaml
- uses: cachix/install-nix-action@v22
- run: nix build
- run: nix develop -c ./build-nix.sh --release
```

## GPU Acceleration

### Checking GPU Support

When entering the Nix shell, you'll see GPU detection status:

```
GPU Acceleration Support:
✓ NVIDIA GPU detected (NVENC/NVDEC available)
✓ VAAPI: 2.20.0
  └─ GPU device found at /dev/dri/renderD128
✓ VideoToolbox: Available (macOS built-in)
```

### Building with Specific GPU Support

```bash
# Full GPU support (default)
nix build

# NVIDIA only
nix build .#edl2ffmpeg-cuda

# Intel/AMD only
nix build .#edl2ffmpeg-vaapi

# No GPU acceleration
nix build .#edl2ffmpeg-minimal
```

### Using GPU Acceleration

```bash
# Auto-detect best GPU
./edl2ffmpeg input.json output.mp4 --hw-accel auto --hw-encode --hw-decode

# Use NVIDIA specifically
./edl2ffmpeg input.json output.mp4 --hw-accel nvenc --hw-encode

# Use Intel/AMD VAAPI
./edl2ffmpeg input.json output.mp4 --hw-accel vaapi --hw-encode
```

### NixOS System Integration

Add to your NixOS configuration:

```nix
{
  programs.edl2ffmpeg = {
    enable = true;
    enableCuda = true;  # If you have NVIDIA GPU
    enableVaapi = true; # If you have Intel/AMD GPU
  };
}
```

## Tips

- Use `nix-shell --pure` for a pure environment without system packages
- The flake provides better reproducibility and caching
- direnv makes the environment automatic
- The build script handles common build configurations
- GPU libraries are automatically included based on your system
- CUDA support is only enabled on x86_64-linux with NVIDIA GPU