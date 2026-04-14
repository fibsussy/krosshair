# Maintainer: fibsussy <noahlykins@gmail.com>
pkgname=krosshair
pkgver=0.1.0
pkgrel=1
pkgdesc="Crosshair overlay for games on linux using Vulkan"
arch=('x86_64' 'aarch64')
url="https://github.com/fibsussy/krosshair"
license=('GPL3')
depends=('vulkan-icd-loader' 'libgl' 'libx11')
makedepends=('gcc' 'make' 'vulkan-headers')
options=('!debug')

build() {
    cd "$startdir"
    make
}

package() {
    cd "$startdir"

    install -Dm755 lib/krosshair.so "$pkgdir/usr/lib/krosshair/krosshair.so"
    install -Dm644 krosshair.json "$pkgdir/usr/share/vulkan/implicit_layer.d/krosshair.json"
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"

    install -dm755 "$pkgdir/usr/share/krosshair/crosshairs"
    cp -r crosshairs/* "$pkgdir/usr/share/krosshair/crosshairs/" || true
}