
import sys, PIL.Image

def cc(r, g, b):
  return ( ((r >> 3) << 0) | ((g >> 3) << 5) | ((b >> 3) << 10) )

im = PIL.Image.open(sys.argv[1])
icon_base = int(sys.argv[2])

pal = sorted([x[1] for x in im.getcolors()], key=lambda x: (x[3], x[0], x[1], x[2]))
numicons = im.size[0] // 16

print("const uint8_t img[%d][%d][%d] = {" % (numicons, im.size[1], 16))
for n in range(numicons):
  print(" {")
  for r in range(im.size[1]):
    line = []
    for c in range(n*16, (n+1)*16):
      col = im.getpixel((c, r))
      line.append(pal.index(col) + icon_base)
    print("  {" + ",".join("0x%02x" % x for x in line) + "},")
  print(" },")
print("};")

print("const uint16_t pal[%d] = {" % len(pal))
print("  " + ",".join("0x%04x" % cc(x[0],x[1],x[2]) for x in pal))
print("};")

