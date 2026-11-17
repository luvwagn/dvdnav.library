#!/bin/sh
# The DVD-Video image dvdnav.library's tests run on (tests/dvd_test.c),
# made with ffmpeg, spumux and dvdauthor (dvdauthor package) and mkisofs
# (genisoimage). Kept as tests/fixtures/dvd/test.iso.xz, so the tests need
# none of these: run this only to change the disc.
#
#   VMGM: first play -> root menu, a still (pause inf) with two buttons:
#         1 at (100,200)-(320,280) plays title 1, 2 at (400,200)-(620,280)
#         plays title 2; left/right move between them
#   title 1: two chapters, 2 s each; title 2: one chapter, 2 s
#   PAL 720 x 576 4:3; two sound tracks: AC-3 5.1 "en" (0x80), then MPEG-1
#   layer II stereo "de" (0xC0); volume AMCE_TEST
#
# dvdauthor leaves the buttons' colour table empty, so the highlight
# colours are set afterwards in the menu's NAV packs: 0xFEDC9876 for a
# selected button, 0x32107654 for an activated one (colour group 1).
#
#   tests/mkdvd.sh out-dir     -> out-dir/test.iso
set -e
out=${1:-build/dvdgen}
mkdir -p "$out"
cd "$out"
ff="ffmpeg -v error -y"
$ff -f lavfi -i "color=c=navy:s=720x576:d=1,drawbox=x=100:y=200:w=220:h=80:c=white@1:t=fill,drawbox=x=400:y=200:w=220:h=80:c=white@1:t=fill" \
    -f lavfi -i anullsrc=r=48000:cl=stereo -t 1 -target pal-dvd -b:v 1500k -shortest menu_bg.mpg
$ff -f lavfi -i "color=c=black@0:s=720x576,format=rgba" -frames:v 1 \
    -vf "drawbox=x=100:y=200:w=220:h=80:c=red@1:t=6,drawbox=x=400:y=200:w=220:h=80:c=red@1:t=6" hl.png
$ff -f lavfi -i "color=c=black@0:s=720x576,format=rgba" -frames:v 1 \
    -vf "drawbox=x=100:y=200:w=220:h=80:c=yellow@1:t=6,drawbox=x=400:y=200:w=220:h=80:c=yellow@1:t=6" sel.png
cat > menu.xml <<'EOF'
<subpictures><stream><spu start="00:00:00.00" force="yes" highlight="hl.png" select="sel.png">
<button name="b1" x0="100" y0="200" x1="320" y1="280" right="b2"/>
<button name="b2" x0="400" y0="200" x1="620" y1="280" left="b1"/>
</spu></stream></subpictures>
EOF
spumux -v 0 menu.xml < menu_bg.mpg > menu.mpg 2> spumux.log
for i in 1 2 3; do
    $ff -f lavfi -i "testsrc=s=720x576:r=25:d=2" -f lavfi -i "sine=f=$((400 * i)):r=48000:d=2" \
        -map 0:v -map 1:a -map 1:a -target pal-dvd -b:v 800k \
        -c:a:0 ac3 -ac:a:0 6 -b:a:0 192k -c:a:1 mp2 -ac:a:1 2 -b:a:1 128k t$i.mpg
done
cat > dvd.xml <<'EOF'
<dvdauthor dest="DVD" jumppad="yes">
 <vmgm>
  <fpc>jump vmgm menu 1;</fpc>
  <menus>
   <video format="pal" aspect="4:3"/>
   <pgc entry="title">
    <button name="b1">jump title 1;</button>
    <button name="b2">jump title 2;</button>
    <vob file="menu.mpg" pause="inf"/>
   </pgc>
  </menus>
 </vmgm>
 <titleset>
  <menus><pgc><post>jump vmgm menu 1;</post></pgc></menus>
  <titles>
   <video format="pal" aspect="4:3"/>
   <audio lang="en"/>
   <audio lang="de"/>
   <pgc>
    <vob file="t1.mpg" chapters="0"/>
    <vob file="t2.mpg" chapters="0"/>
    <post>call vmgm menu 1;</post>
   </pgc>
   <pgc>
    <vob file="t3.mpg" chapters="0"/>
    <post>call vmgm menu 1;</post>
   </pgc>
  </titles>
 </titleset>
</dvdauthor>
EOF
rm -rf DVD
VIDEO_FORMAT=PAL dvdauthor -x dvd.xml > dvdauthor.log 2>&1
mkisofs -quiet -dvd-video -V AMCE_TEST -o test.iso DVD
# the button colours, in every NAV pack whose PCI has a highlight
python3 - test.iso <<'EOF'
import struct, sys
p = sys.argv[1]
d = bytearray(open(p, 'rb').read())
n = 0
for i in range(0, len(d), 2048):
    b = i + 38                               # after the pack and system headers
    if d[b:b + 4] == b'\x00\x00\x01\xbf' and d[b + 6] == 0:
        hli = b + 7 + 0x60                   # the PCI's highlight information
        if d[hli:hli + 2] != b'\x00\x00':
            d[hli + 22:hli + 30] = struct.pack('>II', 0xFEDC9876, 0x32107654)
            n += 1
open(p, 'wb').write(d)
print('button colours set in %d NAV packs' % n)
EOF
