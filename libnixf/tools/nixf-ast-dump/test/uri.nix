# RUN: nixf-ast-dump < %s | FileCheck %s

#      CHECK: List {{.*}}
# CHECK-NEXT:  Token {{.*}} [
# CHECK-NEXT:  ListBody 459
# CHECK-NEXT:   Token 4 x:x
# CHECK-NEXT:   Token 47 https://svn.cs.uu.nl:12443/repos/trace/trunk
# CHECK-NEXT:   Token 80 http://www2.mplayerhq.hu/MPlayer/releases/fonts/font-arial-iso-8859-1.tar.bz2
# CHECK-NEXT:   Token 73 http://losser.st-lab.cs.uu.nl/~armijn/.nix/gcc-3.3.4-static-nix.tar.gz
# CHECK-NEXT:   Token 112 http://fpdownload.macromedia.com/get/shockwave/flash/english/linux/7.0r25/install_flash_player_7_linux.tar.gz
# CHECK-NEXT:   Token 91 https://ftp5.gwdg.de/pub/linux/archlinux/extra/os/x86_64/unzip-6.0-14-x86_64.pkg.tar.zst
# CHECK-NEXT:   Token 52 ftp://ftp.gtk.org/pub/gtk/v1.2/gtk+-1.2.10.tar.gz
# CHECK-NEXT:  Token 2 ]
# CHECK-NEXT: EOF 1
# CHECK-NEXT:  Token 1
[ x:x
  https://svn.cs.uu.nl:12443/repos/trace/trunk
  http://www2.mplayerhq.hu/MPlayer/releases/fonts/font-arial-iso-8859-1.tar.bz2
  http://losser.st-lab.cs.uu.nl/~armijn/.nix/gcc-3.3.4-static-nix.tar.gz
  http://fpdownload.macromedia.com/get/shockwave/flash/english/linux/7.0r25/install_flash_player_7_linux.tar.gz
  https://ftp5.gwdg.de/pub/linux/archlinux/extra/os/x86_64/unzip-6.0-14-x86_64.pkg.tar.zst
  ftp://ftp.gtk.org/pub/gtk/v1.2/gtk+-1.2.10.tar.gz
]
