import numpy as np
import scipy.io as sio
import matplotlib.pyplot as plt


def load_mat_all(filepath):
    """Load Uel (Uel/uel/UEL) and optional CurrentPattern / MeasPattern from .mat file.
    Returns dict with keys: 'Uel', 'CurrentPattern', 'MeasPattern' if available.
    """
    data = sio.loadmat(filepath)
    out = {}
    # Uel
    for key in ("Uel", "uel", "UEL"):
        if key in data:
            out["Uel"] = data[key].astype(float)
            break
    # CurrentPattern
    for key in ("CurrentPattern", "current_pattern", "Current_pattern", "currpat"):
        if key in data:
            out["CurrentPattern"] = np.array(data[key], dtype=float)
            break
    # MeasPattern
    for key in ("MeasPattern", "Meas_pattern", "measpattern", "MeasPatternList"):
        if key in data:
            out["MeasPattern"] = np.array(data[key], dtype=float)
            break
    return out


def setup_geometry(n_electrodes=16, image_size=64, grid_extent=1.1):
    angles = np.linspace(0, 2 * np.pi, n_electrodes, endpoint=False)
    el_pos = np.vstack([np.cos(angles), np.sin(angles)]).T
    x = np.linspace(-grid_extent, grid_extent, image_size)
    y = np.linspace(-grid_extent, grid_extent, image_size)
    X, Y = np.meshgrid(x, y)
    R = np.sqrt(X**2 + Y**2)
    mask = R <= 1.0
    return el_pos, X, Y, mask


def calculate_sensitivity_matrix_patterns(el_pos, X, Y, CurrentPattern=None, MeasPattern=None, min_dist=0.08):
    """
    Build sensitivity matrix S using CurrentPattern (n_elec x n_inj) and MeasPattern (n_meas x n_elec).
    If patterns are None -> fallback to adjacent-injection + adjacent-measurement heuristic.
    Returns S (n_inj * n_meas, n_pixels), and a tuple (n_inj, n_meas).
    """
    n_elec = el_pos.shape[0]
    nx, ny = X.shape
    n_pixels = nx * ny

    px = X.flatten()
    py = Y.flatten()

    # Precompute electrode fields (simple analytic field)
    eps = 1e-12
    elec_field = np.zeros((n_elec, 2, n_pixels), dtype=float)  # (elec, comp, pixel)
    for e in range(n_elec):
        rx = px - el_pos[e, 0]
        ry = py - el_pos[e, 1]
        dist = np.sqrt(rx * rx + ry * ry)
        dist_clipped = np.maximum(dist, min_dist)
        E_x = rx / (dist_clipped ** 2 + eps)
        E_y = ry / (dist_clipped ** 2 + eps)
        elec_field[e, 0, :] = E_x
        elec_field[e, 1, :] = E_y

    # Decide patterns
    if (CurrentPattern is None) or (MeasPattern is None):
        # fallback heuristic: adjacent injection and adjacent measurement
        print("Patterns not found: using adjacent-pair heuristic (fallback).")
        # build simple CurrentPattern (n_elec x n_inj) where col k: +1 at k, -1 at k+1
        n_inj = n_elec
        CurrentPattern = np.zeros((n_elec, n_inj), dtype=float)
        for k in range(n_inj):
            CurrentPattern[k, k] = 1.0
            CurrentPattern[(k + 1) % n_elec, k] = -1.0
        # meas pattern: each row is difference between meas and meas+1 -> n_meas = n_elec
        n_meas = n_elec
        MeasPattern = np.zeros((n_meas, n_elec), dtype=float)
        for m in range(n_meas):
            MeasPattern[m, m] = 1.0
            MeasPattern[m, (m + 1) % n_elec] = -1.0
    else:
        # use provided patterns
        # canonicalize shapes: CurrentPattern expected (n_elec, n_inj)
        CP = np.asarray(CurrentPattern)
        MP = np.asarray(MeasPattern)
        # Some .mat store as transposed shapes, attempt to fix common cases:
        if CP.shape[0] != n_elec and CP.shape[1] == n_elec:
            CP = CP.T
        if MP.shape[1] != n_elec and MP.shape[0] == n_elec:
            MP = MP.T
        CurrentPattern = CP
        MeasPattern = MP
        n_inj = CurrentPattern.shape[1]
        n_meas = MeasPattern.shape[0]

    # Verify dims
    n_inj = CurrentPattern.shape[1]
    n_meas = MeasPattern.shape[0]
    S = np.zeros((n_inj * n_meas, n_pixels), dtype=float)

    print(f"Building S using n_elec={n_elec}, n_inj={n_inj}, n_meas={n_meas}, n_pixels={n_pixels} ...")
    row = 0
    # Pre-normalization guard: if a pattern is all zeros, skip or warn
    for inj in range(n_inj):
        inj_pattern = CurrentPattern[:, inj].astype(float)  # shape (n_elec,)
        if np.allclose(inj_pattern, 0):
            # skip? keep zeros row block
            E_inj = np.zeros((2, n_pixels), dtype=float)
        else:
            # linear combination of electrode fields
            # tensordot(inj_pattern, elec_field, axes=(0,0)) -> (2, n_pixels)
            E_inj = np.tensordot(inj_pattern, elec_field, axes=(0, 0))

        for meas in range(n_meas):
            meas_pattern = MeasPattern[meas, :].astype(float)  # shape (n_elec,)
            if np.allclose(meas_pattern, 0):
                E_meas = np.zeros((2, n_pixels), dtype=float)
            else:
                E_meas = np.tensordot(meas_pattern, elec_field, axes=(0, 0))

            sensitivity = E_inj[0, :] * E_meas[0, :] + E_inj[1, :] * E_meas[1, :]
            maxabs = np.max(np.abs(sensitivity))
            if maxabs > 0:
                sensitivity = sensitivity / maxabs
            S[row, :] = sensitivity
            row += 1

    print(f"S built: shape {S.shape}")
    return S, (n_inj, n_meas)


def select_delta_v_ordered(delta_v_full, n_inj):
    """
    General selection/ordering:
    - delta_v_full shape: (n_meas, n_total_inj) usually
    - selects first n_inj columns and flattens in injection-major order:
      for inj in 0..n_inj-1: for meas in 0..n_meas-1 -> delta_v[meas, inj]
    """
    n_meas, n_total_inj = delta_v_full.shape
    if n_inj > n_total_inj:
        raise ValueError("n_inj requested is larger than available injections.")
    subset = delta_v_full[:, :n_inj]  # shape (n_meas, n_inj)
    vec = subset.T.flatten()  # (n_inj * n_meas,)
    return vec


def reconstruct_lbp(S, delta_v):
    if S.shape[0] != delta_v.size:
        raise ValueError(f"Dimension mismatch: S rows {S.shape[0]} vs delta_v length {delta_v.size}")
    delta_sigma = S.T @ delta_v
    return delta_sigma


def plot_reconstruction(image_data, X, el_pos, mask):
    fig, ax = plt.subplots(figsize=(6, 6))
    v = np.nanmax(np.abs(image_data))
    im = ax.imshow(image_data, cmap='bwr', origin='lower',
                   extent=[X.min(), X.max(), X.min(), X.max()],
                   vmin=-v, vmax=v)
    circle = plt.Circle((0, 0), 1, color='black', fill=False, linewidth=2)
    ax.add_artist(circle)
    ax.plot(el_pos[:, 0], el_pos[:, 1], 'ko', markersize=8)
    for i in range(el_pos.shape[0]):
        ax.text(el_pos[i, 0] * 1.12, el_pos[i, 1] * 1.12, str(i + 1),
                ha='center', va='center', fontsize=10)
    ax.set_title('Linear Back-Projection (with patterns)')
    ax.set_aspect('equal')
    ax.set_xlim(-1.2, 1.2)
    ax.set_ylim(-1.2, 1.2)
    fig.colorbar(im, ax=ax, label='Conductivity')
    plt.show()


def main():
    # user config
    ref_file = "./data_mat_files/datamat_1_0.mat"
    tgt_file = "./data_mat_files/datamat_4_3.mat"
    n_electrodes = 16
    image_size = 32
    n_inj_use = None  # None -> use as many injections as available in data
    min_dist = 0.4

    print("Loading .mat (reference) ...")
    ref = load_mat_all(ref_file)
    print("Loading .mat (target) ...")
    tgt = load_mat_all(tgt_file)

    if "Uel" not in ref or "Uel" not in tgt:
        raise RuntimeError("Uel not found in one of the files.")

    U_ref = ref["Uel"]
    U_tgt = tgt["Uel"]
    print(f"U_ref shape: {U_ref.shape}, U_tgt shape: {U_tgt.shape}")

    delta_v_full = U_tgt - U_ref  # shape (n_meas, n_total_inj)
    print("Computed delta_v_full (target - reference).")

    # Use patterns from reference if present, otherwise fallback
    CurrentPattern = ref.get("CurrentPattern", None)
    MeasPattern = ref.get("MeasPattern", None)
    if CurrentPattern is not None:
        print(f"CurrentPattern shape: {CurrentPattern.shape}")
    if MeasPattern is not None:
        print(f"MeasPattern shape: {MeasPattern.shape}")

    # determine how many injections to use
    n_total_inj = delta_v_full.shape[1]
    if n_inj_use is None:
        n_inj_use = n_total_inj
    else:
        n_inj_use = min(n_inj_use, n_total_inj)

    # geometry
    el_pos, X, Y, mask = setup_geometry(n_electrodes=n_electrodes, image_size=image_size)
    print(f"el_pos {el_pos.shape}, grid {X.shape}")

    # build S using patterns (or fallback)
    S, (n_inj_patterns, n_meas_patterns) = calculate_sensitivity_matrix_patterns(
        el_pos, X, Y, CurrentPattern=CurrentPattern, MeasPattern=MeasPattern, min_dist=min_dist
    )

    # sanity: determine expected rows in S that correspond to the amount of delta_v we will use
    expected_rows = n_inj_use * delta_v_full.shape[0]  # n_meas from data
    if S.shape[0] != expected_rows:
        # it's possible that MeasPattern used a different n_meas; try to adapt ordering accordingly:
        print("Warning: S rows != n_inj_used * n_meas_from_data.")
        print(f"S.rows={S.shape[0]}, n_inj_use={n_inj_use}, n_meas_data={delta_v_full.shape[0]}")
        # If MeasPattern produced different n_meas, but Uel has n_meas rows,
        # we will assume delta_v ordering should follow (inj, meas_data_index).
        # For safety, ensure select ordering matches S rows length:
        # If lengths differ, we will rebuild S with fallback to match data shape.
        # Simpler approach: rebuild S with heuristic fallback using n_meas = delta_v_full.shape[0]
        S, _ = calculate_sensitivity_matrix_patterns(el_pos, X, Y,
                                                    CurrentPattern=None, MeasPattern=None, min_dist=min_dist)
        print("Rebuilt S using fallback adjacent patterns (to match measurement count).")

    # select and order delta_v
    delta_v_subset = select_delta_v_ordered(delta_v_full, n_inj_use)
    print(f"Using first {n_inj_use} injections -> delta_v_subset length = {delta_v_subset.size}")

    if S.shape[0] != delta_v_subset.size:
        raise RuntimeError(f"Shape mismatch after ordering: S rows {S.shape[0]} != delta_v_subset {delta_v_subset.size}")

    # reconstruct
    print("Reconstructing (LBP) ...")
    delta_sigma = reconstruct_lbp(S, delta_v_subset)

    image = delta_sigma.reshape(image_size, image_size)
    image[~mask] = np.nan

    plot_reconstruction(image, X, el_pos, mask)


if __name__ == "__main__":
    main()
