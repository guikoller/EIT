"""
EIT Reconstruction Validation Script
=====================================

Computes the standard GREIT (Graz consensus Reconstruction algorithm for EIT)
figures of merit to quantitatively evaluate LBP and D-bar image quality.

Reference:
    Adler et al., "GREIT: a unified approach to 2D linear EIT reconstruction
    of lung images", Physiol. Meas. 30 (2009) S35–S55.

Metrics computed per target dataset:
  1. Amplitude Response (AR)  — fraction of total amplitude inside the true target
  2. Position Error     (PE)  — Euclidean distance between reconstruction CoG and true target centre
  3. Resolution         (RES) — ratio of reconstructed area to domain area (quarter-amplitude area)
  4. Shape Deformation  (SD)  — fractional area difference between reconstruction and its best-fit circle
  5. Ringing            (RNG) — normalised overshoot amplitude outside the target region

Additionally:
  - Relative Image Error  (RIE) compared to the Python reference reconstruction
  - Correlation Coefficient (CC) with the Python reference reconstruction

Usage:
    python validate_reconstruction.py
"""

import os
import sys
import numpy as np
import scipy.io as sio
import matplotlib.pyplot as plt
from matplotlib.patches import Circle
from dataclasses import dataclass
from typing import Optional, Tuple, List

# ---------------------------------------------------------------------------
#  Import the existing EIT helper functions from eit.py
# ---------------------------------------------------------------------------
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from eit import (load_mat_all, setup_geometry,
                 calculate_sensitivity_matrix_patterns,
                 select_delta_v_ordered, reconstruct_lbp)


# ===================================================================
#  Ground-truth target definitions (from your phantom photos)
# ===================================================================
#
#  Each entry maps a dataset filename to the true target(s) inside the
#  circular domain (radius = 1).  Positions are in normalised coordinates.
#
#  Convention:
#    cx, cy   — centre of the target object
#    r        — approximate radius of the target object
#
#  *** YOU MUST FILL THESE IN from your phantom photos ***
#  Measure the approximate target centre & radius relative to the unit-disk.
#  Example: a target halfway between centre and electrode 1 (rightward) with
#           diameter ~0.3 of the domain → cx=0.5, cy=0.0, r=0.15
#
@dataclass
class TargetInfo:
    cx: float          # centre x in [-1, 1]
    cy: float          # centre y in [-1, 1]
    radius: float      # target radius in domain units
    description: str = ""

# fmt: off
# ---- EDIT THIS DICTIONARY ----
# Key = target .mat filename (without path), Value = list of TargetInfo
KNOWN_TARGETS = {
    # Single-target datasets — estimate from fantom photos
    "datamat_1_1.mat": [TargetInfo(cx= 0.50, cy= 0.00, radius=0.15, description="1 target right of centre")],
    "datamat_1_2.mat": [TargetInfo(cx=-0.50, cy= 0.00, radius=0.15, description="1 target left of centre")],
    "datamat_1_3.mat": [TargetInfo(cx= 0.00, cy= 0.50, radius=0.15, description="1 target top")],
    "datamat_1_4.mat": [TargetInfo(cx= 0.00, cy=-0.50, radius=0.15, description="1 target bottom")],

    # Multi-target (dataset 2): two targets — fill in from fantom_2_X photos
    "datamat_2_1.mat": [TargetInfo(cx= 0.40, cy= 0.40, radius=0.12),
                         TargetInfo(cx=-0.40, cy=-0.40, radius=0.12)],

    # Add more as needed...
}
# fmt: on

REFERENCE_FILE = "datamat_1_0.mat"  # homogeneous reference (no target)


# ===================================================================
#  Helper: create binary ground-truth mask from TargetInfo list
# ===================================================================
def make_gt_mask(targets: List[TargetInfo], X, Y) -> np.ndarray:
    """Return a boolean mask (same shape as X) that is True inside any target."""
    mask = np.zeros(X.shape, dtype=bool)
    for t in targets:
        dist = np.sqrt((X - t.cx)**2 + (Y - t.cy)**2)
        mask |= (dist <= t.radius)
    return mask


def target_centre_of_mass(targets: List[TargetInfo]) -> Tuple[float, float]:
    """Amplitude-weighted centroid of all targets (equal weight per target)."""
    cx = np.mean([t.cx for t in targets])
    cy = np.mean([t.cy for t in targets])
    return cx, cy


# ===================================================================
#  GREIT figures of merit
# ===================================================================
def amplitude_response(image: np.ndarray, gt_mask: np.ndarray, domain_mask: np.ndarray) -> float:
    """AR = sum(|image| inside target) / sum(|image| inside domain)."""
    img = np.where(domain_mask, np.abs(image), 0.0)
    total = np.sum(img)
    if total < 1e-30:
        return 0.0
    return float(np.sum(img[gt_mask]) / total)


def position_error(image: np.ndarray, X: np.ndarray, Y: np.ndarray,
                   domain_mask: np.ndarray,
                   true_cx: float, true_cy: float) -> float:
    """PE = Euclidean distance between image centre-of-gravity and true target centre."""
    img = np.where(domain_mask, np.abs(image), 0.0)
    total = np.sum(img)
    if total < 1e-30:
        return np.nan
    cog_x = float(np.sum(img * X) / total)
    cog_y = float(np.sum(img * Y) / total)
    return float(np.sqrt((cog_x - true_cx)**2 + (cog_y - true_cy)**2))


def resolution(image: np.ndarray, domain_mask: np.ndarray) -> float:
    """RES = fraction of domain area where |image| > 0.25 * max |image|.
    Smaller is better (sharper reconstruction)."""
    img = np.where(domain_mask, np.abs(image), 0.0)
    peak = np.max(img)
    if peak < 1e-30:
        return 0.0
    quarter_mask = img > (0.25 * peak)
    return float(np.sum(quarter_mask) / np.sum(domain_mask))


def shape_deformation(image: np.ndarray, X: np.ndarray, Y: np.ndarray,
                      domain_mask: np.ndarray) -> float:
    """SD = 1 - (area of intersection with best-fit circle) / (area of union with best-fit circle).
    Lower is better (more circular reconstruction)."""
    img = np.where(domain_mask, np.abs(image), 0.0)
    peak = np.max(img)
    if peak < 1e-30:
        return 1.0
    recon_mask = img > (0.25 * peak)
    area = np.sum(recon_mask)
    if area == 0:
        return 1.0

    # Centre of gravity
    total = np.sum(img[recon_mask])
    cog_x = np.sum(img[recon_mask] * X[recon_mask]) / total
    cog_y = np.sum(img[recon_mask] * Y[recon_mask]) / total

    # Best-fit circle radius: radius of circle with same area
    dx = X[1, 1] - X[0, 0]  # pixel spacing
    pixel_area = dx * dx
    equiv_r = np.sqrt(area * pixel_area / np.pi)

    # Circle mask
    dist = np.sqrt((X - cog_x)**2 + (Y - cog_y)**2)
    circ_mask = dist <= equiv_r

    intersection = np.sum(recon_mask & circ_mask)
    union = np.sum(recon_mask | circ_mask)
    if union == 0:
        return 1.0
    return float(1.0 - intersection / union)


def ringing(image: np.ndarray, gt_mask: np.ndarray, domain_mask: np.ndarray) -> float:
    """RNG = sum of |negative values| outside target / sum of |all values| inside domain.
    Measures artefact overshoot. Lower is better."""
    img = np.where(domain_mask, image, 0.0)
    outside = domain_mask & (~gt_mask)
    total_abs = np.sum(np.abs(img))
    if total_abs < 1e-30:
        return 0.0
    # Overshoot: opposite-sign pixels outside target (if target produces positive bump,
    # negative values outside are ringing, and vice-versa)
    target_sign = np.sign(np.sum(img[gt_mask]))
    if target_sign == 0:
        return 0.0
    # Ringing is the opposite-sign energy outside the target region
    outside_vals = img[outside]
    rng_vals = outside_vals[np.sign(outside_vals) == -target_sign]
    return float(np.sum(np.abs(rng_vals)) / total_abs)


# ===================================================================
#  Cross-validation metrics (firmware vs. Python reference)
# ===================================================================
def _normalise(img: np.ndarray, mask: np.ndarray) -> np.ndarray:
    """Scale domain pixels to [−1, 1] (by max-abs), zeros outside."""
    v = np.where(mask, img, 0.0)
    mx = np.max(np.abs(v))
    if mx < 1e-30:
        return v
    return v / mx


def relative_image_error(img_test: np.ndarray, img_ref: np.ndarray,
                         mask: np.ndarray) -> float:
    """RIE = ||test − ref||₂ / ||ref||₂ after normalising both to [−1,1]."""
    t = _normalise(img_test, mask)
    r = _normalise(img_ref, mask)
    norm_ref = np.linalg.norm(r)
    if norm_ref < 1e-30:
        return np.nan
    return float(np.linalg.norm(t - r) / norm_ref)


def correlation_coefficient(img_a: np.ndarray, img_b: np.ndarray,
                            mask: np.ndarray) -> float:
    """Pearson correlation coefficient between two images (inside domain only)."""
    a = img_a[mask].flatten()
    b = img_b[mask].flatten()
    if len(a) < 2:
        return np.nan
    cc = np.corrcoef(a, b)[0, 1]
    return float(cc)


# ===================================================================
#  D-bar reconstruction (Python reference for cross-validation)
# ===================================================================
def reconstruct_dbar_python(S, delta_v, image_size, mask,
                            k_grid_size=16, k_radius=4.0, n_iter=3):
    """
    Minimal D-bar reconstruction (same algorithm as the firmware).
    Returns the blended (LBP + D-bar) image, matching the firmware output.
    """
    n_pixels = S.shape[1]
    # Step 1: back-projection (same as LBP)
    backproj = S.T @ delta_v
    backproj_img = backproj.reshape(image_size, image_size)

    # k-grid
    K_N = k_grid_size
    dk = (2.0 * k_radius) / K_N
    kvals = -k_radius + (np.arange(K_N) + 0.5) * dk
    KX, KY = np.meshgrid(kvals, kvals)

    # Normalised spatial coordinates
    centre = (image_size - 1) / 2.0
    inv_size = 2.0 / image_size
    xs = (np.arange(image_size) - centre) * inv_size
    ys = (np.arange(image_size) - centre) * inv_size
    XX, YY = np.meshgrid(xs, ys)

    # Domain mask (circular)
    radius = image_size / 2.0
    circ = np.sqrt((np.arange(image_size)[:, None] - centre)**2 +
                   (np.arange(image_size)[None, :] - centre)**2) <= radius

    # Step 2: Scattering transform t(k) — Born approximation
    t = np.zeros((K_N, K_N), dtype=complex)
    for iy in range(K_N):
        for ix in range(K_N):
            kx = kvals[ix]
            ky = kvals[iy]
            phase = 2.0 * (kx * XX + ky * YY)
            kernel = np.exp(1j * phase)
            vals = backproj_img * circ
            t[iy, ix] = np.sum(vals * kernel)

    # Step 3: D-bar fixed-point iteration
    mu = np.ones((K_N, K_N), dtype=complex)
    norm_factor = dk**2 / (4.0 * np.pi**2)

    for _ in range(n_iter):
        mu_new = np.ones((K_N, K_N), dtype=complex)
        for iy in range(K_N):
            for ix in range(K_N):
                kx = kvals[ix]
                ky = kvals[iy]
                s = 0.0 + 0.0j
                for jy in range(K_N):
                    for jx in range(K_N):
                        if jy == iy and jx == ix:
                            continue
                        dkx = kvals[jx] - kx
                        dky = kvals[jy] - ky
                        denom = dkx**2 + dky**2
                        if denom < 1e-12:
                            continue
                        s += t[jy, jx] * mu[jy, jx] / denom
                mu_new[iy, ix] = 1.0 + norm_factor * s
        mu = mu_new

    # Step 4: Inverse transform — recover image
    dbar_image = np.zeros((image_size, image_size))
    for py in range(image_size):
        y_n = ys[py]
        for px in range(image_size):
            if not circ[py, px]:
                continue
            x_n = xs[px]
            phase = -2.0 * (KX * x_n + KY * y_n)
            kernel = np.exp(1j * phase)
            val = np.sum(mu * kernel).real
            dbar_image[py, px] = val * dk**2 / (4.0 * np.pi**2) - 1.0

    # Step 5: Blend 50/50 with LBP (same as firmware)
    blended = np.where(circ, 0.5 * backproj_img + 0.5 * dbar_image, np.nan)
    return blended


# ===================================================================
#  Main validation pipeline
# ===================================================================
def run_validation():
    # ---- Configuration ----
    data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data_mat_files")
    n_electrodes = 16
    image_size = 32      # must match firmware EIT_IMAGE_SIZE
    min_dist = 0.4
    grid_extent = 1.1

    # ---- Load reference ----
    ref_path = os.path.join(data_dir, REFERENCE_FILE)
    ref = load_mat_all(ref_path)
    U_ref = ref["Uel"]
    CurrentPattern = ref.get("CurrentPattern", None)
    MeasPattern = ref.get("MeasPattern", None)

    # ---- Geometry & sensitivity matrix ----
    el_pos, X, Y, domain_mask = setup_geometry(n_electrodes, image_size, grid_extent)
    S, (n_inj, n_meas) = calculate_sensitivity_matrix_patterns(
        el_pos, X, Y, CurrentPattern=CurrentPattern, MeasPattern=MeasPattern, min_dist=min_dist
    )

    # ---- Collect all target datasets ----
    all_mat_files = sorted([f for f in os.listdir(data_dir)
                            if f.startswith("datamat_") and f.endswith(".mat")
                            and f != REFERENCE_FILE])

    print(f"\n{'='*80}")
    print(f"EIT Reconstruction Validation — {len(all_mat_files)} datasets")
    print(f"{'='*80}\n")

    # ---- Results table ----
    results = []

    for mat_file in all_mat_files:
        tgt_path = os.path.join(data_dir, mat_file)
        tgt = load_mat_all(tgt_path)
        if "Uel" not in tgt:
            print(f"  SKIP {mat_file}: no Uel found")
            continue

        U_tgt = tgt["Uel"]
        delta_v_full = U_tgt - U_ref
        n_inj_use = min(delta_v_full.shape[1], n_inj)

        try:
            delta_v = select_delta_v_ordered(delta_v_full, n_inj_use)
        except ValueError as e:
            print(f"  SKIP {mat_file}: {e}")
            continue

        if S.shape[0] != delta_v.size:
            # Rebuild S with fallback
            S_fb, _ = calculate_sensitivity_matrix_patterns(
                el_pos, X, Y, CurrentPattern=None, MeasPattern=None, min_dist=min_dist)
            if S_fb.shape[0] != delta_v.size:
                print(f"  SKIP {mat_file}: dimension mismatch S={S_fb.shape[0]} vs dv={delta_v.size}")
                continue
            S_use = S_fb
        else:
            S_use = S

        # ---- Python LBP reconstruction ----
        lbp_raw = reconstruct_lbp(S_use, delta_v)
        lbp_img = lbp_raw.reshape(image_size, image_size)
        lbp_img[~domain_mask] = np.nan

        # ---- Python D-bar reconstruction ----
        dbar_img = reconstruct_dbar_python(S_use, delta_v, image_size, domain_mask)

        # ---- Compute metrics ----
        row = {"dataset": mat_file}

        has_gt = mat_file in KNOWN_TARGETS
        if has_gt:
            targets = KNOWN_TARGETS[mat_file]
            gt_mask = make_gt_mask(targets, X, Y) & domain_mask
            true_cx, true_cy = target_centre_of_mass(targets)

            for algo_name, img in [("LBP", lbp_img), ("D-bar", dbar_img)]:
                img_clean = np.where(domain_mask, np.nan_to_num(img, nan=0.0), 0.0)
                ar  = amplitude_response(img_clean, gt_mask, domain_mask)
                pe  = position_error(img_clean, X, Y, domain_mask, true_cx, true_cy)
                res = resolution(img_clean, domain_mask)
                sd  = shape_deformation(img_clean, X, Y, domain_mask)
                rng = ringing(img_clean, gt_mask, domain_mask)

                row[f"{algo_name}_AR"]  = ar
                row[f"{algo_name}_PE"]  = pe
                row[f"{algo_name}_RES"] = res
                row[f"{algo_name}_SD"]  = sd
                row[f"{algo_name}_RNG"] = rng

        # Cross-validation: LBP vs D-bar
        lbp_c = np.where(domain_mask, np.nan_to_num(lbp_img, nan=0.0), 0.0)
        dbar_c = np.where(domain_mask, np.nan_to_num(dbar_img, nan=0.0), 0.0)
        row["CC_LBP_vs_Dbar"] = correlation_coefficient(lbp_c, dbar_c, domain_mask)
        row["RIE_Dbar_vs_LBP"] = relative_image_error(dbar_c, lbp_c, domain_mask)

        results.append(row)

    # ---- Print results table ----
    print(f"\n{'='*110}")
    print(f"{'Dataset':<20} │ {'Algo':<6} │ {'AR':>6} │ {'PE':>6} │ {'RES':>6} │ {'SD':>6} │ {'RNG':>6} │ {'CC(L/D)':>7} │ {'RIE(D/L)':>8}")
    print(f"{'─'*20}─┼{'─'*8}┼{'─'*8}┼{'─'*8}┼{'─'*8}┼{'─'*8}┼{'─'*8}┼{'─'*9}┼{'─'*10}")

    for row in results:
        ds = row["dataset"]
        cc_ld = row.get("CC_LBP_vs_Dbar", np.nan)
        rie_dl = row.get("RIE_Dbar_vs_LBP", np.nan)

        has_greit = "LBP_AR" in row
        if has_greit:
            for algo in ["LBP", "D-bar"]:
                ar  = row.get(f"{algo}_AR",  np.nan)
                pe  = row.get(f"{algo}_PE",  np.nan)
                res = row.get(f"{algo}_RES", np.nan)
                sd  = row.get(f"{algo}_SD",  np.nan)
                rng = row.get(f"{algo}_RNG", np.nan)
                cc_str  = f"{cc_ld:7.4f}" if algo == "LBP" else "       "
                rie_str = f"{rie_dl:8.4f}" if algo == "LBP" else "        "
                print(f"{ds:<20} │ {algo:<6} │ {ar:6.3f} │ {pe:6.3f} │ {res:6.3f} │ {sd:6.3f} │ {rng:6.3f} │ {cc_str} │ {rie_str}")
        else:
            print(f"{ds:<20} │ {'—':<6} │ {'  —':>6} │ {'  —':>6} │ {'  —':>6} │ {'  —':>6} │ {'  —':>6} │ {cc_ld:7.4f} │ {rie_dl:8.4f}")

    print(f"{'='*110}\n")

    # ---- Interpretation guide ----
    print("GREIT Figures of Merit — Interpretation:")
    print("  AR  (Amplitude Response) : fraction of signal inside target region  [higher = better, ideal ≈ 1]")
    print("  PE  (Position Error)     : distance between CoG and true centre     [lower = better, ideal = 0]")
    print("  RES (Resolution)         : fraction of domain above 25% amplitude   [lower = better = sharper]")
    print("  SD  (Shape Deformation)  : 1 - IoU with best-fit circle             [lower = better, 0 = perfect circle]")
    print("  RNG (Ringing)            : opposite-sign artefact outside target     [lower = better, ideal = 0]")
    print()
    print("Cross-validation:")
    print("  CC  (Correlation Coeff)  : Pearson r between LBP and D-bar          [closer to 1 = consistent]")
    print("  RIE (Relative Img Error) : ||D-bar − LBP||/||LBP||                  [lower = more similar]")

    # ---- Summary statistics ----
    if any("LBP_AR" in r for r in results):
        print(f"\n{'─'*60}")
        print("Mean GREIT scores across datasets with known targets:\n")
        for algo in ["LBP", "D-bar"]:
            for metric in ["AR", "PE", "RES", "SD", "RNG"]:
                key = f"{algo}_{metric}"
                vals = [r[key] for r in results if key in r and not np.isnan(r[key])]
                if vals:
                    print(f"  {algo:<6} {metric:<4}: mean = {np.mean(vals):.4f},  std = {np.std(vals):.4f}")
        print()

    # ---- Visual comparison plots ----
    plot_datasets = [r["dataset"] for r in results if "LBP_AR" in r][:4]  # first 4 with GT
    if not plot_datasets:
        plot_datasets = [r["dataset"] for r in results][:4]

    fig, axes = plt.subplots(len(plot_datasets), 2, figsize=(10, 5 * len(plot_datasets)),
                             squeeze=False)
    fig.suptitle("EIT Reconstruction Validation — LBP vs D-bar", fontsize=14, y=1.01)

    for idx, mat_file in enumerate(plot_datasets):
        tgt_path = os.path.join(data_dir, mat_file)
        tgt = load_mat_all(tgt_path)
        U_tgt = tgt["Uel"]
        delta_v_full = U_tgt - U_ref
        n_inj_use = min(delta_v_full.shape[1], n_inj)
        delta_v = select_delta_v_ordered(delta_v_full, n_inj_use)

        S_use = S if S.shape[0] == delta_v.size else calculate_sensitivity_matrix_patterns(
            el_pos, X, Y, CurrentPattern=None, MeasPattern=None, min_dist=min_dist)[0]

        lbp_raw = reconstruct_lbp(S_use, delta_v).reshape(image_size, image_size)
        lbp_raw[~domain_mask] = np.nan
        dbar_raw = reconstruct_dbar_python(S_use, delta_v, image_size, domain_mask)

        for col, (title, img) in enumerate([("LBP", lbp_raw), ("D-bar", dbar_raw)]):
            ax = axes[idx, col]
            v = np.nanmax(np.abs(img))
            if v < 1e-30:
                v = 1.0
            im = ax.imshow(img, cmap='bwr', origin='lower',
                           extent=[-grid_extent, grid_extent, -grid_extent, grid_extent],
                           vmin=-v, vmax=v)
            ax.add_artist(Circle((0, 0), 1, color='k', fill=False, lw=1.5))
            ax.plot(el_pos[:, 0], el_pos[:, 1], 'ko', ms=4)

            # Draw ground truth circle(s) if available
            if mat_file in KNOWN_TARGETS:
                for t in KNOWN_TARGETS[mat_file]:
                    ax.add_artist(Circle((t.cx, t.cy), t.radius,
                                        color='lime', fill=False, lw=2, ls='--'))

            ax.set_title(f"{mat_file}\n{title}", fontsize=10)
            ax.set_aspect('equal')
            ax.set_xlim(-1.3, 1.3)
            ax.set_ylim(-1.3, 1.3)
            fig.colorbar(im, ax=ax, shrink=0.8)

    plt.tight_layout()
    plt.savefig(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "validation_results.png"), dpi=150, bbox_inches='tight')
    print("Saved visual comparison to: validation_results.png")
    plt.show()


if __name__ == "__main__":
    run_validation()
