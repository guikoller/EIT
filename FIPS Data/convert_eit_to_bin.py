#!/usr/bin/env python3
"""
EIT .mat to .bin Converter
Converts MATLAB .mat files containing EIT measurement data to packed binary format
for efficient loading on STM32F769NI microcontroller.

Binary Format:
- Header (20 bytes):
  - magic: 0x45495442 ('EITB' in ASCII) - uint32
  - n_meas: number of measurements - uint16
  - n_inj: number of injections - uint16
  - image_size: reconstruction grid size - uint16
  - curr_pattern_rows: CurrentPattern rows - uint16
  - meas_pattern_rows: MeasPattern rows - uint16
  - reserved: padding - uint16
  - reserved2: padding - uint32
- Data:
  - Uel: voltage measurements - float32[n_meas * n_inj]
  - CurrentPattern: injection pattern matrix - int8[curr_pattern_rows * n_inj]
  - MeasPattern: measurement pattern matrix - int8[meas_pattern_rows * n_meas]
"""

import numpy as np
import scipy.io as sio
import struct
import os
from pathlib import Path

# ============================================================================
# CONFIGURATION - Adjust these parameters as needed
# ============================================================================
IMAGE_SIZE = 32          # Reconstruction grid size (32x32 or 64x64)
N_ELECTRODES = 16        # Number of electrodes
N_MEASUREMENTS = 16      # Expected number of measurements (16 electrodes)
N_INJECTIONS = 79        # Expected number of injection patterns

# File paths
MAT_FILES_DIR = "./data_mat_files"
OUTPUT_DIR = "./data_bin_files"  # Output to same directory
REFERENCE_FILE_NAME = "datamat_1_0"  # Reference file (without extension)

# Magic number for binary file identification
MAGIC_NUMBER = 0x45495442  # 'EITB' in ASCII


def load_data_from_mat(filepath):
    """Load Uel, CurrentPattern, and MeasPattern from .mat file.
    
    Args:
        filepath: Path to .mat file
        
    Returns:
        tuple: (uel, current_pattern, meas_pattern) or (None, None, None) if error
    """
    data = sio.loadmat(filepath)
    
    # Load Uel
    uel = None
    for key in ("Uel", "uel", "UEL"):
        if key in data:
            uel = data[key].astype(np.float32)
            print(f"  Found '{key}' with shape {uel.shape}")
            break
    
    if uel is None:
        print(f"  ERROR: No Uel data found")
        return None, None, None
    
    # Load CurrentPattern
    curr_pattern = None
    for key in ("CurrentPattern", "currentpattern", "current_pattern"):
        if key in data:
            curr_pattern = data[key].astype(np.int8)  # Use int8 for ±1 values
            print(f"  Found '{key}' with shape {curr_pattern.shape}")
            break
    
    if curr_pattern is None:
        print(f"  WARNING: No CurrentPattern found")
    
    # Load MeasPattern
    meas_pattern = None
    for key in ("MeasPattern", "measpattern", "meas_pattern"):
        if key in data:
            meas_pattern = data[key].astype(np.int8)  # Use int8 for ±1 values
            print(f"  Found '{key}' with shape {meas_pattern.shape}")
            break
    
    if meas_pattern is None:
        print(f"  WARNING: No MeasPattern found")
    
    return uel, curr_pattern, meas_pattern


def write_binary_file(uel, curr_pattern, meas_pattern, output_path, image_size=IMAGE_SIZE):
    """Write Uel, CurrentPattern, and MeasPattern to binary file with header.
    
    Args:
        uel: numpy array of shape (n_meas, n_inj)
        curr_pattern: numpy array of shape (n_inj, 2) or None
        meas_pattern: numpy array of shape (n_meas, 2) or None
        output_path: Output .bin file path
        image_size: Reconstruction image size for header
    """
    n_meas, n_inj = uel.shape
    
    # Validate dimensions
    if n_meas != N_MEASUREMENTS:
        print(f"  WARNING: Expected {N_MEASUREMENTS} measurements, got {n_meas}")
    if n_inj != N_INJECTIONS:
        print(f"  WARNING: Expected {N_INJECTIONS} injections, got {n_inj}")
    
    # Flatten Uel in C order (row-major)
    uel_flat = uel.flatten(order='C').astype(np.float32)
    
    # Get pattern dimensions (they are full matrices, not pair lists)
    curr_rows = curr_pattern.shape[0] if curr_pattern is not None else 0
    curr_cols = curr_pattern.shape[1] if curr_pattern is not None else 0
    meas_rows = meas_pattern.shape[0] if meas_pattern is not None else 0
    meas_cols = meas_pattern.shape[1] if meas_pattern is not None else 0
    
    with open(output_path, 'wb') as f:
        # Write header (20 bytes)
        header = struct.pack('<IHHHHHHI',
                            MAGIC_NUMBER,    # magic (4 bytes)
                            n_meas,          # n_meas (2 bytes)
                            n_inj,           # n_inj (2 bytes)
                            image_size,      # image_size (2 bytes)
                            curr_rows,       # curr_pattern_rows (2 bytes)
                            meas_rows,       # meas_pattern_rows (2 bytes)
                            0,               # reserved (2 bytes)
                            0)               # reserved2 (4 bytes)
        f.write(header)
        
        # Write Uel data
        f.write(uel_flat.tobytes())
        
        # Write CurrentPattern if present
        if curr_pattern is not None:
            curr_flat = curr_pattern.flatten(order='C').astype(np.int8)
            f.write(curr_flat.tobytes())
        
        # Write MeasPattern if present
        if meas_pattern is not None:
            meas_flat = meas_pattern.flatten(order='C').astype(np.int8)
            f.write(meas_flat.tobytes())
    
    file_size = os.path.getsize(output_path)
    print(f"  Written: {output_path} ({file_size} bytes)")
    print(f"  Header: n_meas={n_meas}, n_inj={n_inj}, image_size={image_size}")
    print(f"  Uel: {len(uel_flat)} float32 ({len(uel_flat) * 4} bytes)")
    if curr_pattern is not None:
        print(f"  CurrentPattern: {curr_rows}x{curr_cols} int8 ({curr_rows * curr_cols} bytes)")
    if meas_pattern is not None:
        print(f"  MeasPattern: {meas_rows}x{meas_cols} int8 ({meas_rows * meas_cols} bytes)")


def convert_all_mat_files():
    """Convert all .mat files in MAT_FILES_DIR to .bin format."""
    mat_dir = Path(MAT_FILES_DIR)
    
    if not mat_dir.exists():
        print(f"ERROR: Directory '{MAT_FILES_DIR}' does not exist!")
        return
    
    # Find all .mat files
    mat_files = sorted(mat_dir.glob("*.mat"))
    
    if not mat_files:
        print(f"No .mat files found in '{MAT_FILES_DIR}'")
        return
    
    print(f"Found {len(mat_files)} .mat files")
    print(f"Converting with IMAGE_SIZE={IMAGE_SIZE}\n")
    
    output_dir = Path(OUTPUT_DIR)
    output_dir.mkdir(exist_ok=True)
    
    converted_count = 0
    reference_found = False
    
    for mat_file in mat_files:
        print(f"Processing: {mat_file.name}")
        
        # Load data from .mat file
        uel, curr_pattern, meas_pattern = load_data_from_mat(str(mat_file))
        if uel is None:
            print(f"  SKIPPED: Could not load data\n")
            continue
        
        # Generate output filename
        bin_filename = mat_file.stem + ".bin"
        bin_path = output_dir / bin_filename
        
        # Write binary file
        try:
            write_binary_file(uel, curr_pattern, meas_pattern, str(bin_path), IMAGE_SIZE)
            converted_count += 1
            
            # Check if this is the reference file
            if mat_file.stem == REFERENCE_FILE_NAME:
                reference_found = True
                print(f"  *** REFERENCE FILE ***")
            
            print()
            
        except Exception as e:
            print(f"  ERROR writing binary: {e}\n")
            continue
    
    print("=" * 60)
    print(f"Conversion complete: {converted_count}/{len(mat_files)} files converted")
    
    if reference_found:
        print(f"✓ Reference file '{REFERENCE_FILE_NAME}.bin' created")
    else:
        print(f"⚠ WARNING: Reference file '{REFERENCE_FILE_NAME}.mat' not found!")
        print(f"  The firmware expects this file to exist.")
    
    print(f"\nBinary files saved to: {output_dir.absolute()}")
    print(f"Copy these .bin files to the SD card root directory.")


def verify_binary_file(bin_path):
    """Verify a binary file by reading and displaying its contents.
    
    Args:
        bin_path: Path to .bin file to verify
    """
    print(f"\nVerifying: {bin_path}")
    
    with open(bin_path, 'rb') as f:
        # Read header (20 bytes actual, not 24)
        header_bytes = f.read(20)
        if len(header_bytes) < 20:
            print("  ERROR: File too short for header")
            return
        
        magic, n_meas, n_inj, image_size, curr_rows, meas_rows, reserved, reserved2 = struct.unpack('<IHHHHHHI', header_bytes)
        
        print(f"  Magic: 0x{magic:08X} {'✓' if magic == MAGIC_NUMBER else '✗ INVALID'}")
        print(f"  n_meas: {n_meas}")
        print(f"  n_inj: {n_inj}")
        print(f"  image_size: {image_size}")
        print(f"  curr_pattern_rows: {curr_rows}")
        print(f"  meas_pattern_rows: {meas_rows}")
        
        # Read Uel data
        expected_floats = n_meas * n_inj
        uel_bytes = f.read(expected_floats * 4)
        
        if len(uel_bytes) != expected_floats * 4:
            print(f"  ERROR: Expected {expected_floats * 4} Uel bytes, got {len(uel_bytes)}")
            return
        
        uel_flat = np.frombuffer(uel_bytes, dtype=np.float32)
        uel = uel_flat.reshape((n_meas, n_inj))
        
        print(f"  Uel shape: {uel.shape}")
        print(f"  Uel range: [{np.min(uel):.6e}, {np.max(uel):.6e}]")
        print(f"  Uel mean: {np.mean(uel):.6e}")
        
        # Read CurrentPattern if present
        if curr_rows > 0:
            curr_size = curr_rows * n_inj
            curr_bytes = f.read(curr_size)
            if len(curr_bytes) == curr_size:
                curr_pattern = np.frombuffer(curr_bytes, dtype=np.int8).reshape((curr_rows, n_inj))
                print(f"  CurrentPattern shape: {curr_pattern.shape}")
        
        # Read MeasPattern if present
        if meas_rows > 0:
            meas_size = meas_rows * n_meas
            meas_bytes = f.read(meas_size)
            if len(meas_bytes) == meas_size:
                meas_pattern = np.frombuffer(meas_bytes, dtype=np.int8).reshape((meas_rows, n_meas))
                print(f"  MeasPattern shape: {meas_pattern.shape}")
        
        print(f"  ✓ File is valid")


if __name__ == "__main__":
    print("=" * 60)
    print("EIT .mat to .bin Converter")
    print("=" * 60)
    print(f"Configuration:")
    print(f"  IMAGE_SIZE = {IMAGE_SIZE}")
    print(f"  N_ELECTRODES = {N_ELECTRODES}")
    print(f"  N_MEASUREMENTS = {N_MEASUREMENTS}")
    print(f"  N_INJECTIONS = {N_INJECTIONS}")
    print(f"  REFERENCE_FILE = {REFERENCE_FILE_NAME}.mat")
    print("=" * 60)
    print()
    
    convert_all_mat_files()
    
    # Verify the reference file if it exists
    ref_bin_path = Path(OUTPUT_DIR) / f"{REFERENCE_FILE_NAME}.bin"
    if ref_bin_path.exists():
        verify_binary_file(str(ref_bin_path))
    
    print("\nDone!")
