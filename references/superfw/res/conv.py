
import sys, PIL.Image

def cc(r, g, b):
  return ( ((r >> 3) << 0) | ((g >> 3) << 5) | ((b >> 3) << 10) )

im = PIL.Image.open(sys.argv[1])

pal = [x[1] for x in im.getcolors()]

print("const uint8_t img[%d][%d] = {" % (im.size[1], im.size[0]))
for r in range(im.size[1]):
  line = []
  for c in range(im.size[0]):
    col = im.getpixel((c, r))
    line.append(pal.index(col))
  print(" {" + ",".join(str(x) for x in line) + "},")
print("};")

print("const uint16_t pal[%d] = {" % len(pal))
print("  " + ",".join("0x%04x" % cc(x[0],x[1],x[2]) for x in pal))
print("};")

