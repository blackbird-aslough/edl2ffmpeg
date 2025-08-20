#!/usr/bin/env bash
# Wrapper for ftv_toffmpeg using the full container
# This uses the complete container with all dependencies

set -e

# Use the full container ID or tag
DOCKER_IMAGE="${FTV_DOCKER_IMAGE:-ftv_toffmpeg:full}"

# Force x86_64 platform for Apple Silicon Macs
DOCKER_ARGS=(--platform linux/amd64)

# Function to get absolute path (macOS compatible)
get_absolute_path() {
    if [[ -e "$1" ]]; then
        echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
    else
        # For non-existent files, resolve the directory
        local dir="$(dirname "$1")"
        local base="$(basename "$1")"
        if [[ -d "$dir" ]]; then
            echo "$(cd "$dir" && pwd)/$base"
        else
            echo "$1"
        fi
    fi
}

# Parse arguments to find file paths that need to be mounted
MOUNT_PATHS=()
ALL_ARGS=("$@")

# Function to add a path to mount list
add_mount_path() {
    local path="$1"
    local abs_path=$(get_absolute_path "$path")
    
    # Get the directory that needs to be mounted
    local mount_dir=$(dirname "$abs_path")
    
    # Check if already mounted
    local already_mounted=false
    for mounted in "${MOUNT_PATHS[@]}"; do
        if [[ "$mounted" == "$mount_dir" ]] || [[ "$mount_dir" == "$mounted"/* ]]; then
            already_mounted=true
            break
        fi
    done
    
    if [[ "$already_mounted" == "false" ]] && [[ -d "$mount_dir" ]]; then
        MOUNT_PATHS+=("$mount_dir")
    fi
}

# Process arguments
for i in "${!ALL_ARGS[@]}"; do
    arg="${ALL_ARGS[$i]}"
    next_arg=""
    if [[ $((i+1)) -lt ${#ALL_ARGS[@]} ]]; then
        next_arg="${ALL_ARGS[$((i+1))]}"
    fi
    
    case "$arg" in
        --logo|--kill|--tmp|--edl|--captions-embed|--progress|--report)
            if [[ -n "$next_arg" ]] && [[ "$next_arg" != -* ]]; then
                add_mount_path "$next_arg"
            fi
            ;;
        --logo=*|--kill=*|--tmp=*|--edl=*|--captions-embed=*|--progress=*|--report=*)
            value="${arg#*=}"
            if [[ -n "$value" ]]; then
                add_mount_path "$value"
            fi
            ;;
        -*)
            ;;
        *)
            if [[ "$arg" == */* ]] || [[ -e "$arg" ]] || [[ "$arg" == *.* ]]; then
                add_mount_path "$arg"
            fi
            ;;
    esac
done

# Add mounts (skip /tmp as it will be handled separately)
for mount in "${MOUNT_PATHS[@]}"; do
    if [[ "$mount" != "/tmp" ]]; then
        DOCKER_ARGS+=(-v "$mount:$mount:ro")
    fi
done

# Always mount current directory to /work for Linux compatibility (writable for output)
CURRENT_DIR="$(pwd)"
DOCKER_ARGS+=(-v "$CURRENT_DIR:/work:rw")

# Mount temp directory for output files
# Always mount /tmp as writable for temporary files
DOCKER_ARGS+=(-v "/tmp:/tmp:rw")

# If TMPDIR is set and different from /tmp, mount it too
if [[ -n "$TMPDIR" ]] && [[ "$TMPDIR" != "/tmp" ]]; then
    DOCKER_ARGS+=(-v "$TMPDIR:$TMPDIR:rw")
fi

# Setup for real-time output
DOCKER_ARGS+=(-t)
[[ -t 0 ]] && DOCKER_ARGS+=(-i)
[[ -t 2 ]] && DOCKER_ARGS+=(-e "PYTHONUNBUFFERED=1")

# Check Docker
if ! docker info >/dev/null 2>&1; then
    echo "Error: Docker Desktop is not running" >&2
    echo "Please start Docker Desktop and try again" >&2
    exit 1
fi

# Check image - use docker images to check as inspect can have issues
if ! docker images --format "{{.Repository}}:{{.Tag}}" | grep -q "^${DOCKER_IMAGE}$"; then
    echo "Error: Docker image '$DOCKER_IMAGE' not found" >&2
    echo "To get the full container, run on your Linux server:" >&2
    echo "  docker save 2b6c0dd49d0f | gzip > ftv_full.tar.gz" >&2
    echo "Then copy and load it here:" >&2
    echo "  scp server:ftv_full.tar.gz ." >&2
    echo "  docker load < ftv_full.tar.gz" >&2
    exit 1
fi


# Run container with seccomp workaround for ftv_ffmpeg_read
exec docker run --rm \
    --security-opt seccomp=unconfined \
    "${DOCKER_ARGS[@]}" \
    -w "/work" \
    "$DOCKER_IMAGE" \
    bash -c '
        # Create a stub libseccomp that returns success for all operations
        cat > /tmp/libseccomp_stub.c << "EOF"
#include <stdarg.h>
#include <stdlib.h>

struct scmp_version {
    unsigned int major;
    unsigned int minor;
    unsigned int micro;
};

void *seccomp_init(unsigned int def_action) { return malloc(1); }
int seccomp_rule_add(void *ctx, unsigned int action, int syscall, unsigned int arg_cnt, ...) { return 0; }
int seccomp_rule_add_exact(void *ctx, unsigned int action, int syscall, unsigned int arg_cnt, ...) { return 0; }
int seccomp_rule_add_array(void *ctx, unsigned int action, int syscall, unsigned int arg_cnt, void *arg_array) { return 0; }
int seccomp_load(void *ctx) { return 0; }
void seccomp_release(void *ctx) { if (ctx) free(ctx); }
int seccomp_reset(void *ctx, unsigned int def_action) { return 0; }
const struct scmp_version *seccomp_version(void) {
    static struct scmp_version v = {.major = 2, .minor = 5, .micro = 5};
    return &v;
}
int seccomp_arch_add(void *ctx, unsigned int arch_token) { return 0; }
int seccomp_arch_remove(void *ctx, unsigned int arch_token) { return 0; }
int seccomp_arch_exist(const void *ctx, unsigned int arch_token) { return 0; }
int seccomp_arch_native(void) { return 0x40000003; }
int seccomp_attr_get(const void *ctx, unsigned int attr, unsigned int *value) { 
    if (value) *value = 0;
    return 0; 
}
int seccomp_attr_set(void *ctx, unsigned int attr, unsigned int value) { return 0; }
int seccomp_export_bpf(const void *ctx, int fd) { return 0; }
int seccomp_export_pfc(const void *ctx, int fd) { return 0; }
EOF
        gcc -shared -fPIC /tmp/libseccomp_stub.c -o /tmp/libseccomp.so.2 || {
            echo "Error: Failed to compile seccomp stub library" >&2
            exit 1
        }
        
        # Create wrapper that bypasses sudo and preserves LD_PRELOAD
        cat > /tmp/ftv_ffmpeg_read_wrapper << "WRAPPER"
#!/bin/bash
export LD_PRELOAD=/tmp/libseccomp.so.2
exec /usr/bin/ftv_ffmpeg_read "$@"
WRAPPER
        chmod +x /tmp/ftv_ffmpeg_read_wrapper
        
        # Override PATH and LD_PRELOAD to use our stubs
        export PATH=/tmp:$PATH
        export LD_PRELOAD=/tmp/libseccomp.so.2
        
        # Run ftv_toffmpeg
        exec ftv_toffmpeg "$@"
    ' -- "$@"