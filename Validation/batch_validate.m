% =========================================================================
% BATCH VALIDATION: MCU (LBP) vs EIDORS (Gauss-Newton) and MATLAB (LBP)
%
% Processes all datamat_*.mat files (except reference datamat_1_0),
% reconstructs with EIDORS and MATLAB LBP, loads firmware CSV,
% computes metrics and generates comparison figures.
% =========================================================================
clear; close all; clc;

import eit_validation.*

%% ---- Configuration ----
cfg = get_config();
fprintf('Results directory: %s\n', cfg.results_dir);
fprintf('Mask radius offset: %.2f px\n', cfg.mask_radius_offset_px);

%% ---- Load reference dataset ----
dados_homo = load(fullfile(cfg.mat_dir, cfg.ref_file));
U_ref = dados_homo.Uel;
[n_meas, n_inj] = size(U_ref);
fprintf('Reference: %s (%d meas x %d inj = %d)\n', cfg.ref_file, n_meas, n_inj, n_meas*n_inj);

%% ---- Get patterns ----
if isfield(dados_homo, 'CurrentPattern')
    CP = double(dados_homo.CurrentPattern);
    if size(CP, 1) ~= cfg.n_elec && size(CP, 2) == cfg.n_elec
        CP = CP';
    end
else
    error('CurrentPattern not found in %s', cfg.ref_file);
end

if isfield(dados_homo, 'MeasPattern')
    MP = double(dados_homo.MeasPattern);
    if size(MP, 2) ~= cfg.n_elec && size(MP, 1) == cfg.n_elec
        MP = MP';
    end
else
    error('MeasPattern not found in %s', cfg.ref_file);
end

%% ---- Build sensitivity matrix ----
S = build_sensitivity_matrix(cfg, CP, MP);

%% ---- Create circular mask ----
circ_mask = make_circular_mask(cfg.img_size, cfg.mask_radius_offset_px);
fprintf('Circular mask: %d pixels\n', nnz(circ_mask));

%% ---- Setup EIDORS ----
[imdl, enable_eidors] = setup_eidors(cfg.n_elec, CP, MP, cfg.eidors_hp);

%% ---- List target files ----
mat_files = dir(fullfile(cfg.mat_dir, 'datamat_*.mat'));
mat_files(strcmpi({mat_files.name}, cfg.ref_file)) = [];
n_files = numel(mat_files);
fprintf('\nFound %d target datasets.\n\n', n_files);

%% ---- Pre-allocate results table ----
T = table('Size', [n_files 12], ...
    'VariableTypes', {'string', ...
        'double','double','double','double','double', ...
        'double','double','double','double','double', ...
        'string'}, ...
    'VariableNames', {'Dataset', ...
        'CC_E','SSIM_E','GRE_E','RMSE_E','PSNR_E', ...
        'CC_M','SSIM_M','GRE_M','RMSE_M','PSNR_M', ...
        'Photo'});

%% ---- Main loop ----
for k = 1:n_files
    mat_name = mat_files(k).name;
    [~, base, ~] = fileparts(mat_name);
    csv_name = [base '_LBP.csv'];
    photo_name = strrep(base, 'datamat', 'fantom');

    fprintf('=== [%2d/%d] %s ===\n', k, n_files, base);

    % Load target dataset
    dados_alvo = load(fullfile(cfg.mat_dir, mat_name));
    U_tgt = dados_alvo.Uel;

    % MATLAB LBP reconstruction
    matriz_matlab = reconstruct_lbp(S, U_ref, U_tgt, cfg.img_size);

    % EIDORS reconstruction
    if enable_eidors
        vh = U_ref(:);
        vi = U_tgt(:);
        opts_eid.resolution = cfg.img_size;

        img_eidors = inv_solve(imdl, vh, vi);
        slice_eidors = calc_slices(img_eidors, opts_eid);
        if size(slice_eidors, 1) ~= cfg.img_size || size(slice_eidors, 2) ~= cfg.img_size
            slice_eidors = imresize(slice_eidors, [cfg.img_size cfg.img_size], 'bilinear');
        end
        eidors_mask = ~isnan(slice_eidors);
    else
        slice_eidors = NaN(cfg.img_size, cfg.img_size);
        eidors_mask = circ_mask;
    end

    % Load firmware CSV
    csv_path = fullfile(cfg.csv_dir, csv_name);
    if ~isfile(csv_path)
        fprintf('  SKIP: CSV not found (%s)\n\n', csv_name);
        T.Dataset(k) = base;
        T{k, 2:11} = NaN;
        T.Photo(k) = "";
        continue;
    end
    matriz_mcu = readmatrix(csv_path);

    % Apply common mask
    common_mask = eidors_mask;
    fprintf('  Mask pixels: %d\n', sum(common_mask(:)));

    matriz_mcu(~common_mask) = NaN;
    matriz_matlab(~common_mask) = NaN;
    slice_eidors(~common_mask) = NaN;

    % Find photo
    photo_path = '';
    for ext = {'.jpg', '.png', '.bmp'}
        candidate = fullfile(cfg.photo_dir, [photo_name ext{1}]);
        if isfile(candidate)
            photo_path = candidate;
            break;
        end
    end

    % Normalize images
    mcu_norm = normalize_image(matriz_mcu);
    matlab_norm = normalize_image(matriz_matlab);
    eidors_norm = normalize_image(slice_eidors);

    % Compute metrics
    metrics_e = compute_metrics(mcu_norm, eidors_norm);
    metrics_m = compute_metrics(mcu_norm, matlab_norm);

    if enable_eidors
        fprintf('  MCU vs EIDORS: CC=%.4f  SSIM=%.4f  RMSE=%.4f\n', metrics_e.cc, metrics_e.ssim, metrics_e.rmse);
    end
    fprintf('  MCU vs MATLAB: CC=%.4f  SSIM=%.4f  RMSE=%.4f\n', metrics_m.cc, metrics_m.ssim, metrics_m.rmse);

    % Store results
    T.Dataset(k) = base;
    T.CC_E(k) = metrics_e.cc; T.SSIM_E(k) = metrics_e.ssim; T.GRE_E(k) = metrics_e.gre;
    T.RMSE_E(k) = metrics_e.rmse; T.PSNR_E(k) = metrics_e.psnr;
    T.CC_M(k) = metrics_m.cc; T.SSIM_M(k) = metrics_m.ssim; T.GRE_M(k) = metrics_m.gre;
    T.RMSE_M(k) = metrics_m.rmse; T.PSNR_M(k) = metrics_m.psnr;
    T.Photo(k) = photo_name;

    % Generate comparison figure
    fig_path = fullfile(cfg.results_dir, [base '_comparison.png']);
    plot_comparison(base, photo_path, mcu_norm, eidors_norm, matlab_norm, metrics_e, metrics_m, fig_path);
    fprintf('  Saved: %s\n\n', fig_path);
end

%% ---- Summary ----
fprintf('\n================================================================\n');
fprintf('  SUMMARY - MCU vs EIDORS (Gauss-Newton) and MATLAB (LBP)\n');
fprintf('================================================================\n');
disp(T(:, {'Dataset', 'CC_E', 'SSIM_E', 'CC_M', 'SSIM_M'}));

valid_rows_e = ~isnan(T.CC_E);
valid_rows_m = ~isnan(T.CC_M);
valid_rows = valid_rows_e | valid_rows_m;

fprintf('----------------------------------------------------------------\n');
fprintf('  AVERAGES:\n');
if any(valid_rows_e)
    fprintf('    EIDORS: CC=%.4f  SSIM=%.4f  RMSE=%.4f\n', ...
        mean(T.CC_E(valid_rows_e)), mean(T.SSIM_E(valid_rows_e)), mean(T.RMSE_E(valid_rows_e)));
end
if any(valid_rows_m)
    fprintf('    MATLAB: CC=%.4f  SSIM=%.4f  RMSE=%.4f\n', ...
        mean(T.CC_M(valid_rows_m)), mean(T.SSIM_M(valid_rows_m)), mean(T.RMSE_M(valid_rows_m)));
end
fprintf('================================================================\n');

%% ---- Export results ----
csv_out = fullfile(cfg.results_dir, 'validation_summary.csv');
writetable(T, csv_out);
fprintf('Table saved: %s\n', csv_out);

T_valid = T(valid_rows, :);
tex_out = fullfile(cfg.results_dir, 'metrics_summary.tex');
write_metrics_tex(tex_out, T_valid);
fprintf('LaTeX summary saved: %s\n', tex_out);

chart_out = fullfile(cfg.results_dir, 'summary_chart.png');
plot_summary(T, chart_out);
fprintf('Summary chart saved: %s\n', chart_out);

fprintf('\nDone! %d datasets processed.\n', n_files);
