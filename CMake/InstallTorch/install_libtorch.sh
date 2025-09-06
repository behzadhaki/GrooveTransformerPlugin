#!/bin/bash

set -e

# Set root installation directory based on the platform
if [[ "$(uname -s)" == "Darwin" ]]; then
  ROOT_INSTALL_DIR="/Library/Application Support/libtorch"  # macOS
  DEPLOY_ROOT="/Library/MusicTechnologyGroup/libtorch"      # Deploy directory
elif [[ "$(uname -s)" == "Linux" ]]; then
  ROOT_INSTALL_DIR="/opt/libtorch"  # Linux
  DEPLOY_ROOT="/opt/MusicTechnologyGroup/libtorch"  # Deploy directory for Linux
else
  echo "Unsupported platform. Exiting."
  exit 1
fi

# Determine version and platform-specific file name
if [[ "$(uname -s)" == "Darwin" ]]; then
  PLATFORM="MacOS"
  if [[ $(uname -m) == "arm64" ]]; then
    TORCH_VERSION="2.6.0"
    DEBUG_FILE="libtorch-macos-arm64-${TORCH_VERSION}.zip"
    RELEASE_FILE="libtorch-macos-arm64-${TORCH_VERSION}.zip"
  else
    TORCH_VERSION="2.2.0"
    DEBUG_FILE="libtorch-macos-x86_64-${TORCH_VERSION}.zip"
    RELEASE_FILE="libtorch-macos-x86_64-${TORCH_VERSION}.zip"
  fi
elif [[ "$(uname -s)" == "Linux" ]]; then
  TORCH_VERSION="2.6.0"
  PLATFORM="Linux"
  DEBUG_FILE="libtorch-shared-with-deps-${TORCH_VERSION}%2Bcpu.zip"
  RELEASE_FILE="libtorch-shared-with-deps-${TORCH_VERSION}%2Bcpu.zip"
else
  echo "Unsupported platform. Exiting."
  exit 1
fi

DEBUG_DIR="$ROOT_INSTALL_DIR/libtorch-${TORCH_VERSION}-Debug"
RELEASE_DIR="$ROOT_INSTALL_DIR/libtorch-${TORCH_VERSION}-Release"

DEBUG_DEPLOY_DIR="$DEPLOY_ROOT/${TORCH_VERSION}-Debug"
RELEASE_DEPLOY_DIR="$DEPLOY_ROOT/${TORCH_VERSION}-Release"

# Function to copy and fix dylibs (macOS only)
copy_and_fix_dylibs() {
  local source_dir=$1
  local deploy_dir=$2
  local torch_root_path=$3

  if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "Copying dylibs from $source_dir/libtorch/lib to $deploy_dir"

    # Create deploy directory
    mkdir -p "$deploy_dir"

    # Copy all dylibs
    cp -R "$source_dir/libtorch/lib"/*.dylib "$deploy_dir/" 2>/dev/null || true

    # Fix dylib paths
    echo "Fixing dylib install names and dependencies in $deploy_dir"
    cd "$deploy_dir"

    for dylib in *.dylib; do
      if [ -f "$dylib" ]; then
        echo "Processing $dylib"
        install_name_tool -id "@rpath/$dylib" "$dylib"
        install_name_tool -change "$torch_root_path/libtorch/lib/libtorch.dylib" "@rpath/libtorch.dylib" "$dylib" 2>/dev/null || true
        install_name_tool -change "$torch_root_path/libtorch/lib/libtorch_cpu.dylib" "@rpath/libtorch_cpu.dylib" "$dylib" 2>/dev/null || true
        install_name_tool -change "$torch_root_path/libtorch/lib/libc10.dylib" "@rpath/libc10.dylib" "$dylib" 2>/dev/null || true
      fi
    done

    echo "Dylib processing completed for $deploy_dir"
  fi
}

# Function to download and install libtorch
install_libtorch() {
  local build_type=$1
  local target_dir=$2
  local file_name=$3
  local deploy_dir=$4

  echo "Installing libtorch ($build_type) to $target_dir"

  # Remove existing directory
  if [[ -d "$target_dir" ]]; then
    echo "Removing existing directory: $target_dir"
    rm -rf "$target_dir"
  fi

  mkdir -p "$target_dir"

  # Download the file
  local download_url="https://download.pytorch.org/libtorch/cpu/$file_name"
  local temp_file="/tmp/$file_name"

  echo "Downloading $file_name from $download_url"
  curl -L -o "$temp_file" "$download_url"

  # Extract the archive
  echo "Extracting $file_name"
  unzip -q "$temp_file" -d "$target_dir"
  rm "$temp_file"

  echo "Libtorch ($build_type) installed successfully to $target_dir"

  # Copy and fix dylibs to deploy directory
  copy_and_fix_dylibs "$target_dir" "$deploy_dir" "$target_dir"
}

# Install Debug and Release versions
install_libtorch "Debug" "$DEBUG_DIR" "$DEBUG_FILE" "$DEBUG_DEPLOY_DIR"
install_libtorch "Release" "$RELEASE_DIR" "$RELEASE_FILE" "$RELEASE_DEPLOY_DIR"

# Move .so files to /usr/lib on Linux
if [[ "$(uname -s)" == "Linux" ]]; then
  echo "Moving .so files to /usr/lib..."
  find "$DEBUG_DIR" "$RELEASE_DIR" -type f -name "*.so*" -exec sudo cp {} /usr/lib/ \;
  echo ".so files have been moved to /usr/lib"
fi

# Output result
echo "Libtorch installations completed:"
echo "  Debug: $DEBUG_DIR"
echo "  Release: $RELEASE_DIR"
echo ""
echo "Dylibs deployed to:"
echo "  Debug: $DEBUG_DEPLOY_DIR"
echo "  Release: $RELEASE_DEPLOY_DIR"
echo ""
echo "Using torch version: $TORCH_VERSION for $(uname -m) architecture"