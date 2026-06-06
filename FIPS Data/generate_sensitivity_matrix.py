"""
Generate pre-computed sensitivity matrix for EIT reconstruction on STM32F769
Saves S matrix as binary file to be loaded into SDRAM
"""
import numpy as np
import scipy.io as sio
import struct
from pathlib import Path


def load_patterns_from_mat(filepath):
    """Load CurrentPattern and MeasPattern from reference .mat file"""
    data = sio.loadmat(filepath)
    
    # Extract patterns
    current_pattern = None
    meas_pattern = None
    
    for key in ("CurrentPattern", "current_pattern"):
        if key in data:
            current_pattern = np.array(data[key], dtype=np.float32)
            break
    
    for key in ("MeasPattern", "Meas_pattern", "measpattern"):
        if key in data:
            meas_pattern = np.array(data[key], dtype=np.float32)
            break
    
    return current_pattern, meas_pattern


def setup_geometry(n_electrodes=16, image_size=32, grid_extent=1.1):
    """Setup electrode positions and reconstruction grid"""
    angles = np.linspace(0, 2 * np.pi, n_electrodes, endpoint=False)
    el_pos = np.vstack([np.cos(angles), np.sin(angles)]).T
    
    x = np.linspace(-grid_extent, grid_extent, image_size)
    y = np.linspace(-grid_extent, grid_extent, image_size)
    X, Y = np.meshgrid(x, y)
    
    R = np.sqrt(X**2 + Y**2)
    mask = R <= 1.0
    
    return el_pos, X, Y, mask


def calculate_sensitivity_matrix(el_pos, X, Y, CurrentPattern, MeasPattern, min_dist=0.4):
    """
    Build sensitivity matrix S using CurrentPattern and MeasPattern
    Returns S as (n_measurements, n_pixels) float32 array
    """
    n_elec = el_pos.shape[0]
    nx, ny = X.shape
    n_pixels = nx * ny
    
    px = X.flatten()
    py = Y.flatten()
    
    print(f"Computing electrode fields for {n_elec} electrodes...")
    # Precompute electrode fields
    eps = 1e-12
    elec_field = np.zeros((n_elec, 2, n_pixels), dtype=np.float32)
    
    for e in range(n_elec):
        rx = px - el_pos[e, 0]
        ry = py - el_pos[e, 1]
        dist = np.sqrt(rx * rx + ry * ry)
        dist_clipped = np.maximum(dist, min_dist)
        
        E_x = rx / (dist_clipped ** 2 + eps)
        E_y = ry / (dist_clipped ** 2 + eps)
        
        elec_field[e, 0, :] = E_x
        elec_field[e, 1, :] = E_y
    
    # Canonicalize pattern shapes
    CP = np.asarray(CurrentPattern, dtype=np.float32)
    MP = np.asarray(MeasPattern, dtype=np.float32)
    
    if CP.shape[0] != n_elec and CP.shape[1] == n_elec:
        CP = CP.T
    if MP.shape[1] != n_elec and MP.shape[0] == n_elec:
        MP = MP.T
    
    n_inj = CP.shape[1]
    n_meas = MP.shape[0]
    
    print(f"Building sensitivity matrix: {n_elec} electrodes, {n_inj} injections, {n_meas} measurements")
    print(f"Matrix size: ({n_inj * n_meas}, {n_pixels}) = {n_inj * n_meas * n_pixels * 4 / 1024 / 1024:.2f} MB")
    
    S = np.zeros((n_inj * n_meas, n_pixels), dtype=np.float32)
    
    row = 0
    for inj in range(n_inj):
        if (inj + 1) % 10 == 0:
            print(f"  Processing injection {inj + 1}/{n_inj}...")
        
        inj_pattern = CP[:, inj]
        
        if np.allclose(inj_pattern, 0):
            E_inj = np.zeros((2, n_pixels), dtype=np.float32)
        else:
            E_inj = np.tensordot(inj_pattern, elec_field, axes=(0, 0))
        
        for meas in range(n_meas):
            meas_pattern = MP[meas, :]
            
            if np.allclose(meas_pattern, 0):
                E_meas = np.zeros((2, n_pixels), dtype=np.float32)
            else:
                E_meas = np.tensordot(meas_pattern, elec_field, axes=(0, 0))
            
            # Dot product of electric fields
            sensitivity = E_inj[0, :] * E_meas[0, :] + E_inj[1, :] * E_meas[1, :]
            
            # Normalize
            maxabs = np.max(np.abs(sensitivity))
            if maxabs > 0:
                sensitivity = sensitivity / maxabs
            
            S[row, :] = sensitivity
            row += 1
    
    print(f"Sensitivity matrix built: shape {S.shape}")
    return S, n_inj, n_meas


def save_sensitivity_matrix(S, n_inj, n_meas, image_size, output_path):
    """
    Save sensitivity matrix as binary file with header
    Format:
    - Header (32 bytes):
      - magic: uint32 (0x53454E53 = 'SENS')
      - n_measurements: uint32 (n_inj * n_meas)
      - n_pixels: uint32 (image_size * image_size)
      - image_size: uint32
      - n_inj: uint32
      - n_meas: uint32
      - reserved: uint32[2]
    - Data: float32 array (n_measurements * n_pixels elements)
    """
    n_measurements = S.shape[0]
    n_pixels = S.shape[1]
    
    with open(output_path, 'wb') as f:
        # Write header
        header = struct.pack('<8I',
            0x53454E53,      # magic 'SENS'
            n_measurements,  # total measurements
            n_pixels,        # pixels in image
            image_size,      # image size (32)
            n_inj,           # number of injections
            n_meas,          # number of measurements per injection
            0,               # reserved
            0                # reserved
        )
        f.write(header)
        
        # Write matrix data (row-major, C order)
        S.astype(np.float32).tofile(f)
    
    file_size = Path(output_path).stat().st_size
    print(f"\nSaved sensitivity matrix to: {output_path}")
    print(f"File size: {file_size / 1024 / 1024:.2f} MB")
    print(f"Header: 32 bytes")
    print(f"Data: {n_measurements} x {n_pixels} float32 = {n_measurements * n_pixels * 4} bytes")


def verify_sensitivity_matrix(filepath):
    """Load and verify the saved sensitivity matrix"""
    with open(filepath, 'rb') as f:
        # Read header
        header_data = f.read(32)
        magic, n_measurements, n_pixels, image_size, n_inj, n_meas, _, _ = struct.unpack('<8I', header_data)
        
        print(f"\nVerifying sensitivity matrix:")
        print(f"  Magic: 0x{magic:08X} ({'SENS' if magic == 0x53454E53 else 'INVALID'})")
        print(f"  Measurements: {n_measurements} ({n_inj} inj × {n_meas} meas)")
        print(f"  Pixels: {n_pixels} ({image_size}×{image_size})")
        
        # Read matrix data
        S = np.fromfile(f, dtype=np.float32).reshape(n_measurements, n_pixels)
        print(f"  Matrix shape: {S.shape}")
        print(f"  Value range: [{S.min():.3e}, {S.max():.3e}]")
        print(f"  Non-zero elements: {np.count_nonzero(S)} ({100*np.count_nonzero(S)/S.size:.1f}%)")
        
        return magic == 0x53454E53


def main():
    # Configuration
    ref_file = "./data_mat_files/datamat_1_0.mat"
    output_file = "./data_bin_files/sensitivity_matrix.bin"
    
    n_electrodes = 16
    image_size = 32
    grid_extent = 1.1
    min_dist = 0.4
    
    print("=" * 60)
    print("EIT Sensitivity Matrix Generator")
    print("=" * 60)
    
    # Load patterns from reference file
    print(f"\nLoading patterns from: {ref_file}")
    current_pattern, meas_pattern = load_patterns_from_mat(ref_file)
    
    if current_pattern is None or meas_pattern is None:
        print("ERROR: Could not find CurrentPattern or MeasPattern in reference file")
        return
    
    print(f"CurrentPattern shape: {current_pattern.shape}")
    print(f"MeasPattern shape: {meas_pattern.shape}")
    
    # Setup geometry
    print(f"\nSetting up geometry...")
    el_pos, X, Y, mask = setup_geometry(n_electrodes, image_size, grid_extent)
    print(f"Electrode positions: {el_pos.shape}")
    print(f"Grid: {X.shape}")
    print(f"Pixels inside mask: {np.sum(mask)} / {mask.size}")
    
    # Calculate sensitivity matrix
    print(f"\nCalculating sensitivity matrix...")
    S, n_inj, n_meas = calculate_sensitivity_matrix(
        el_pos, X, Y, current_pattern, meas_pattern, min_dist
    )
    
    # Save to file
    print(f"\nSaving to: {output_file}")
    save_sensitivity_matrix(S, n_inj, n_meas, image_size, output_file)
    
    # Verify
    if verify_sensitivity_matrix(output_file):
        print("\n✓ Sensitivity matrix saved and verified successfully!")
    else:
        print("\n✗ Verification failed!")


if __name__ == "__main__":
    main()
