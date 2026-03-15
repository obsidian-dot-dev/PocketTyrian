import os
import struct
import sys

def bundle_rom(src_dir, output_file):
    files = sorted([f for f in os.listdir(src_dir) if os.path.isfile(os.path.join(src_dir, f))])
    
    # Header: Magic(8 bytes), NumFiles(4 bytes)
    magic = b"TYRIANRM"
    num_files = len(files)
    
    header = magic + struct.pack("<I", num_files)
    
    # Entry Table: Filename(32 bytes), Offset(4 bytes), Size(4 bytes)
    # Total 40 bytes per entry
    
    entries = []
    current_offset = len(header) + num_files * 40
    
    entry_data = b""
    file_contents = b""
    
    for filename in files:
        full_path = os.path.join(src_dir, filename)
        size = os.path.getsize(full_path)
        
        # Ensure filename fits in 32 bytes
        name_bytes = filename.encode('ascii')[:31]
        name_field = name_bytes.ljust(32, b'\0')
        
        entry_data += name_field + struct.pack("<II", current_offset, size)
        
        with open(full_path, 'rb') as f:
            file_contents += f.read()
        
        current_offset += size
        print(f"Adding {filename}: offset={current_offset-size}, size={size}")

    with open(output_file, 'wb') as f:
        f.write(header)
        f.write(entry_data)
        f.write(file_contents)
    
    print(f"\nDone! Bundled {num_files} files into {output_file} ({len(header) + len(entry_data) + len(file_contents)} bytes)")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 bundle_rom.py <src_dir> <output_file>")
        sys.exit(1)
    
    bundle_rom(sys.argv[1], sys.argv[2])
