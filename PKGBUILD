# Maintainer: zenakuten
pkgname=utcolor
pkgver=1.0.0
pkgrel=1
pkgdesc='UT2004 Colored Text Editor'
arch=('x86_64')
url='https://github.com/zenakuten/utcolor'
license=('MIT')
depends=('gcc-libs')
makedepends=('cmake' 'git' 'curl' 'zip' 'unzip' 'tar' 'pkg-config' 'ninja')
source=("git+https://github.com/zenakuten/utcolor.git")
sha256sums=('SKIP')

prepare() {
    cd "$srcdir"
    if [ ! -d vcpkg ]; then
        git clone https://github.com/microsoft/vcpkg.git
        ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
    fi
}

build() {
    cd "$srcdir/$pkgname"
    cmake -B build \
        -DCMAKE_TOOLCHAIN_FILE="$srcdir/vcpkg/scripts/buildsystems/vcpkg.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    cd "$srcdir/$pkgname"
    install -Dm755 build/utcolor "$pkgdir/usr/bin/utcolor"
}
