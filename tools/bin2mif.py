import sys
import struct

def bin_to_mif(bin_path, mif_path, depth=16384, width=32):
    with open(bin_path, 'rb') as f:
        data = f.read()
    
    # Pad data to 4-byte alignment
    if len(data) % 4 != 0:
        data += b'\x00' * (4 - (len(data) % 4))
    
    num_words = len(data) // 4
    if num_words > depth:
        print(f"Error: Binary too large ({num_words} words) for MIF depth ({depth})")
        sys.exit(1)
    
    with open(mif_path, 'w') as f:
        f.write(f"-- Tyrian Bootloader MIF\n")
        f.write(f"-- Generated from {bin_path}\n\n")
        f.write(f"WIDTH={width};\n")
        f.write(f"DEPTH={depth};\n\n")
        f.write(f"ADDRESS_RADIX=DEC;\n")
        f.write(f"DATA_RADIX=HEX;\n\n")
        f.write(f"CONTENT BEGIN\n")
        
        # Write actual data
        for i in range(num_words):
            word = struct.unpack("<I", data[i*4:i*4+4])[0]
            f.write(f"    {i} : {word:08X};\n")
        
        # Pad remainder with NOP (addi x0, x0, 0 = 0x00000013)
        if num_words < depth:
            if num_words == depth - 1:
                f.write(f"    {num_words} : 00000013;\n")
            else:
                f.write(f"    [{num_words}..{depth-1}] : 00000013;\n")
        
        f.write(f"END;\n")
    
    print(f"MIF generated: {mif_path} ({num_words} words + {depth-num_words} padding)")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 bin2mif.py <input.bin> <output.mif>")
        sys.exit(1)
    bin_to_mif(sys.argv[1], sys.argv[2])
