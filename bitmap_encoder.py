#!/usr/bin/python3

import imageio.v3 as iio
import sys

img = iio.imread(sys.argv[1])

print("{", end="")

currentByte = -1
currentCount = 0
rle = False

if len(sys.argv) >= 3 and sys.argv[2] == "rle":
    rle = True

def write_byte(byte):
    global currentByte
    global currentCount

    if rle:
        if currentByte == byte:
            if currentCount < 256:
                currentCount += 1
            else:
                # Encoding 256 bytes
                currentCount -= 1
                print(f"0xff, {currentCount}, 0x{currentByte:x}, ", end="")
                currentCount = 0
        else:
            if currentByte == -1:
                pass
            elif currentCount == 1:
                if currentByte == 0xFF:
                    print("0xff, 0x0, 0xff, ", end="")
                else:
                    print(f"0x{currentByte:x}, ", end="")
            else:
                currentCount -= 1
                print(f"0xff, {currentCount}, 0x{currentByte:x}, ", end="")
            currentByte = byte
            currentCount = 1
    else:
        if byte != -1:
            print(f"{byte}, ", end="")

bits = 0
count = 8

for y in range(img.shape[1]):
    for x in range(img.shape[0]):
        pixel = img[y][x]
        count -= 1
        if pixel[0] == 255 and pixel[1] == 255 and pixel[2] == 255:
            # Foreground
            bits |= 1 << count
            pass
        else:
            # Background
            pass
        if count == 0:
            count = 8
            write_byte(bits)
            bits = 0

write_byte(-1)
print("};")
