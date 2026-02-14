# Maintainer: zenakuten
pkgname=utcolor
pkgver=1.0.0
pkgrel=1
pkgdesc='UT2004 Colored Text Editor'
arch=('x86_64')
url='https://github.com/zenakuten/utcolor'
license=('MIT')
depends=('gcc-libs' 'sdl3')
makedepends=('cmake' 'git' 'ninja' 'sdl3')
source=("git+https://github.com/zenakuten/utcolor.git")
sha256sums=('SKIP')

build() {
    cd "$srcdir/$pkgname"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    cd "$srcdir/$pkgname"
    install -Dm755 build/utcolor "$pkgdir/usr/bin/utcolor"
}
