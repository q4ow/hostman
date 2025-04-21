# Release Guide for Hostman

This document provides guidelines for creating releases of the hostman utility, including binary distributions for multiple platforms and AUR packages.

## Table of Contents

1. [Version Management](#version-management)
2. [Building Binaries](#building-binaries)
   - [Linux (x86_64)](#linux-x86_64)
   - [Linux (ARM64)](#linux-arm64)
   - [macOS](#macos)
   - [Windows](#windows)
3. [Creating Release Packages](#creating-release-packages)
4. [AUR Packages](#aur-packages)
   - [hostman Package (Prebuilt Binary)](#hostman-package-prebuilt-binary)
   - [hostman-git Package (Build from Source)](#hostman-git-package-build-from-source)
5. [Release Checklist](#release-checklist)

## Version Management

1. Update the version number in `CMakeLists.txt`:
   ```cmake
   project(hostman VERSION X.Y.Z LANGUAGES C)
   ```

2. Update the CHANGELOG.md with the new version's changes:
   ```markdown
   ## [X.Y.Z] - YYYY-MM-DD
   
   ### Added
   - New feature 1
   - New feature 2
   
   ### Changed
   - Change 1
   - Change 2
   
   ### Fixed
   - Fix 1
   - Fix 2
   ```

3. Commit the changes:
   ```bash
   git add CMakeLists.txt CHANGELOG.md
   git commit -m "Bump version to X.Y.Z"
   git tag -a vX.Y.Z -m "Version X.Y.Z"
   git push origin main --tags
   ```

## Building Binaries

### Linux (x86_64)

```bash
# Create a build directory
mkdir -p release/linux-x86_64
cd release/linux-x86_64

# Configure with CMake
cmake ../.. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Create the package
tar -czvf hostman-X.Y.Z-linux-x86_64.tar.gz hostman LICENSE README.md
```

### Linux (ARM64)

```bash
# Set up cross-compilation environment
export CC=aarch64-linux-gnu-gcc

# Create a build directory
mkdir -p release/linux-arm64
cd release/linux-arm64

# Configure with CMake
cmake ../.. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Create the package
tar -czvf hostman-X.Y.Z-linux-arm64.tar.gz hostman LICENSE README.md
```

### macOS

```bash
# Create a build directory
mkdir -p release/macos
cd release/macos

# Configure with CMake
cmake ../.. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(sysctl -n hw.ncpu)

# Create the package
tar -czvf hostman-X.Y.Z-macos.tar.gz hostman LICENSE README.md
```

### Windows

```bash
# Using MinGW (on Windows)
mkdir -p release\windows
cd release\windows

# Configure with CMake
cmake ..\.. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# Build
mingw32-make

# Create the package
7z a hostman-X.Y.Z-windows.zip hostman.exe LICENSE README.md
```

## Creating Release Packages

For each platform, create a release package that includes:

1. The binary executable
2. README.md
3. LICENSE
4. CHANGELOG.md
5. A simple installation script (optional)

Example installation script for Linux:

```bash
#!/bin/bash
# install.sh
mkdir -p ~/.local/bin
cp hostman ~/.local/bin/
echo "Installation complete. Make sure ~/.local/bin is in your PATH."
```

## AUR Packages

### hostman Package (Prebuilt Binary)

Create a PKGBUILD for the prebuilt binary:

```bash
# PKGBUILD for hostman
pkgname=hostman
pkgver=X.Y.Z
pkgrel=1
pkgdesc="A simple file host manager for various image hosting services"
arch=('x86_64')
url="https://github.com/yourusername/hostman"
license=('MIT')
depends=('libcurl' 'sqlite' 'openssl' 'cjson')
source=("$url/releases/download/v$pkgver/$pkgname-$pkgver-linux-x86_64.tar.gz")
sha256sums=('HASH_VALUE')

package() {
  cd "$srcdir"
  
  # Install binary
  install -Dm755 hostman "$pkgdir/usr/bin/hostman"
  
  # Install documentation
  install -Dm644 README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
  install -Dm644 CHANGELOG.md "$pkgdir/usr/share/doc/$pkgname/CHANGELOG.md"
  
  # Install license
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
```

### hostman-git Package (Build from Source)

Create a PKGBUILD for building from source:

```bash
# PKGBUILD for hostman-git
pkgname=hostman-git
pkgver=X.Y.Z.r$(git rev-list --count HEAD).g$(git rev-parse --short HEAD)
pkgrel=1
pkgdesc="A simple file host manager for various image hosting services (git version)"
arch=('x86_64' 'aarch64')
url="https://github.com/yourusername/hostman"
license=('MIT')
depends=('curl' 'sqlite' 'openssl' 'cjson')
makedepends=('cmake' 'git')
provides=('hostman')
conflicts=('hostman')
source=("git+$url.git")
sha256sums=('SKIP')

pkgver() {
  cd "$srcdir/hostman"
  printf "%s.r%s.g%s" "$(grep -Po '(?<=VERSION )[0-9.]+' CMakeLists.txt)" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cd "$srcdir/hostman"
  mkdir -p build
  cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make
}

package() {
  cd "$srcdir/hostman/build"
  
  # Install binary
  install -Dm755 hostman "$pkgdir/usr/bin/hostman"
  
  # Install documentation
  install -Dm644 ../README.md "$pkgdir/usr/share/doc/$pkgname/README.md"
  install -Dm644 ../CHANGELOG.md "$pkgdir/usr/share/doc/$pkgname/CHANGELOG.md"
  
  # Install license
  install -Dm644 ../LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
```

## Publishing to AUR

1. Clone the AUR package:
   ```bash
   git clone ssh://aur@aur.archlinux.org/hostman.git
   # or for the -git package
   git clone ssh://aur@aur.archlinux.org/hostman-git.git
   ```

2. Update the PKGBUILD and .SRCINFO:
   ```bash
   cd hostman  # or hostman-git
   # Edit PKGBUILD
   makepkg --printsrcinfo > .SRCINFO
   git add PKGBUILD .SRCINFO
   git commit -m "Update to version X.Y.Z"
   git push
   ```

## Release Checklist

Before creating a release, check the following:

1. [ ] Code passes all tests
2. [ ] Version numbers are updated in all files
3. [ ] CHANGELOG.md is updated
4. [ ] Dependencies are correctly listed in documentation and package files
5. [ ] Binary builds successfully on all target platforms
6. [ ] README.md is up to date with current features

After creating a release:

1. [ ] Upload binary packages to GitHub Releases
2. [ ] Update AUR packages with new version
3. [ ] Announce the new release on relevant channels
4. [ ] Check for any reported issues with the new release