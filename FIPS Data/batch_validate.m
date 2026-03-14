% =========================================================================
% VALIDAÇÃO EM LOTE: MCU (LBP) vs EIDORS (GN) e MATLAB (LBP)
%
% Percorre todos os datamat_*.mat (exceto a referência datamat_1_0),
% reconstrói com EIDORS (Gauss-Newton) e MATLAB LBP (S calculada localmente),
% carrega o CSV do firmware e a foto do fantoma,
% calcula métricas e gera figuras comparativas salvas em results/.
%
% Layout da figura por dataset:
%   Topo:   Dados de comparação MCU vs EIDORS
%   Meio:   Imagens – Phantom | MCU | EIDORS | MATLAB
%   Baixo:  Dados de comparação MCU vs MATLAB
% =========================================================================
clear; close all; clc;

%% ---- Paths ----
mat_dir      = fullfile(pwd, 'data_mat_files');
csv_dir      = fullfile(pwd, 'firmware_data');
photo_dir    = fullfile(pwd, 'target_photos');
results_dir  = fullfile(pwd, 'results');
if ~exist(results_dir, 'dir'), mkdir(results_dir); end

ref_file = 'datamat_1_0.mat';
img_size = 32;
n_elec   = 16;
grid_extent = 1.1;
min_dist    = 0.4;

%% ---- Load reference dataset ----
dados_homo = load(fullfile(mat_dir, ref_file));
U_ref = dados_homo.Uel;
[n_meas, n_inj] = size(U_ref);
n_total = n_meas * n_inj;
fprintf('Referência: %s  (%d meas x %d inj = %d)\n', ref_file, n_meas, n_inj, n_total);

%% ---- Electrode geometry (unit circle) ----
angles = linspace(0, 2*pi, n_elec+1); angles(end) = [];
el_pos = [cos(angles)', sin(angles)'];

%% ---- Reconstruction grid ----
x = linspace(-grid_extent, grid_extent, img_size);
y = linspace(-grid_extent, grid_extent, img_size);
[X, Y] = meshgrid(x, y);
px = X(:); py = Y(:);
n_pixels = img_size^2;

%% ---- Electrode fields ----
eps_val = 1e-12;
elec_field_x = zeros(n_elec, n_pixels);
elec_field_y = zeros(n_elec, n_pixels);
for e = 1:n_elec
    rx = px - el_pos(e,1);
    ry = py - el_pos(e,2);
    dist_clipped = max(sqrt(rx.^2 + ry.^2), min_dist);
    denom = dist_clipped.^2 + eps_val;
    elec_field_x(e,:) = (rx ./ denom)';
    elec_field_y(e,:) = (ry ./ denom)';
end

%% ---- Injection / measurement patterns ----
if isfield(dados_homo, 'CurrentPattern')
    CP = double(dados_homo.CurrentPattern);
    if size(CP,1) ~= n_elec && size(CP,2) == n_elec, CP = CP'; end
else
    error('CurrentPattern não encontrado em %s', ref_file);
end

if isfield(dados_homo, 'MeasPattern')
    MP = double(dados_homo.MeasPattern);
    if size(MP,2) ~= n_elec && size(MP,1) == n_elec, MP = MP'; end
else
    error('MeasPattern não encontrado em %s', ref_file);
end

n_inj_pat  = size(CP, 2);
n_meas_pat = size(MP, 1);
fprintf('Padrões: %d inj x %d meas = %d linhas em S\n', n_inj_pat, n_meas_pat, n_inj_pat*n_meas_pat);

%% ---- Build sensitivity matrix S (MATLAB LBP) ----
fprintf('Construindo matriz S...\n');
S = zeros(n_inj_pat * n_meas_pat, n_pixels);
row = 0;
for inj = 1:n_inj_pat
    inj_pattern = CP(:, inj);
    E_inj_x = inj_pattern' * elec_field_x;
    E_inj_y = inj_pattern' * elec_field_y;
    for meas = 1:n_meas_pat
        meas_pattern = MP(meas, :);
        E_meas_x = meas_pattern * elec_field_x;
        E_meas_y = meas_pattern * elec_field_y;
        sensitivity = E_inj_x .* E_meas_x + E_inj_y .* E_meas_y;
        maxabs = max(abs(sensitivity));
        if maxabs > 0, sensitivity = sensitivity / maxabs; end
        row = row + 1;
        S(row, :) = sensitivity;
    end
end
fprintf('Matriz S: %dx%d\n', size(S,1), size(S,2));

%% ---- Circular mask (same as firmware) ----
center = (img_size - 1) / 2;
radius = img_size / 2;
circ_mask = true(img_size);
for iy = 0:img_size-1
    for ix = 0:img_size-1
        if sqrt((ix-center)^2 + (iy-center)^2) > radius
            circ_mask(iy+1, ix+1) = false;
        end
    end
end

%% ---- EIDORS model setup ----
if ~exist('mk_common_model','file')
    error(['EIDORS não encontrado no path do MATLAB. ' ...
           'Execute eidors/startup.m antes de rodar este script.']);
end
fprintf('Configurando modelo EIDORS...\n');
mdl_base = mk_common_model('c2c2', n_elec);
fmdl = mdl_base.fwd_model;

% Override stimulation patterns to match our data
clear stim_eidors;
for i = 1:n_inj_pat
    stim_eidors(i).stimulation  = 'Amp';
    stim_eidors(i).stim_pattern = sparse(CP(:, i));
    stim_eidors(i).meas_pattern = sparse(MP);
end
fmdl.stimulation = stim_eidors;

% Remove meas_select (incompatible with custom patterns)
if isfield(fmdl, 'meas_select')
    fmdl = rmfield(fmdl, 'meas_select');
end

% Build inverse model with Gauss-Newton + Tikhonov 
imdl = eidors_obj('inv_model', 'FIPS_batch_GN');
imdl.fwd_model = fmdl;
imdl.reconst_type = 'difference';
imdl.solve = @inv_solve_diff_GN_one_step;
imdl.hyperparameter.value = 0.03;
imdl.RtR_prior = @prior_tikhonov;
imdl.jacobian_bkgnd.value = 1;
fprintf('EIDORS pronto (Gauss-Newton, Tikhonov prior, hp=0.03)\n');

%% ---- List target .mat files ----
mat_files = dir(fullfile(mat_dir, 'datamat_*.mat'));
mat_files(strcmpi({mat_files.name}, ref_file)) = [];
n_files = numel(mat_files);
fprintf('\nEncontrados %d datasets-alvo.\n\n', n_files);

%% ---- Pre-allocate results table ----
T = table('Size', [n_files 10], ...
    'VariableTypes', {'string','double','double','double','double','double','double','double','double','string'}, ...
    'VariableNames', {'Dataset','CC_E','SSIM_E','GRE_E','RMSE_E','CC_M','SSIM_M','GRE_M','RMSE_M','Photo'});

%% ---- Helper: MATLAB LBP reconstruction ----
reconstruct_lbp = @(U_tgt_mat) do_lbp(S, U_ref, U_tgt_mat, n_meas, n_inj, img_size, circ_mask);

%% ---- Main loop ----
for k = 1:n_files
    mat_name = mat_files(k).name;
    [~, base, ~] = fileparts(mat_name);
    csv_name   = [base '_LBP.csv'];
    photo_name = strrep(base, 'datamat', 'fantom');

    fprintf('=== [%2d/%d] %s ===\n', k, n_files, base);

    % ---- Load target .mat ----
    dados_alvo = load(fullfile(mat_dir, mat_name));
    U_tgt = dados_alvo.Uel;

    % ---- MATLAB LBP reconstruction ----
    matriz_matlab = reconstruct_lbp(U_tgt);
    matriz_matlab = rot90(matriz_matlab, 1);   % rotate 90° left
    matriz_matlab = -matriz_matlab;            % sign flip

    % ---- EIDORS reconstruction ----
    vh = U_ref(:);             % homogeneous (reference) — plain vector
    vi = U_tgt(:);             % inhomogeneous (target)  — plain vector
    img_rec = inv_solve(imdl, vh, vi);
    opts_eid.resolution = img_size;
    eidors_slice = calc_slices(img_rec, opts_eid);
    fprintf('  EIDORS slice size: %dx%d\n', size(eidors_slice,1), size(eidors_slice,2));
    % Resize EIDORS output to 32x32 grid if needed
    if size(eidors_slice,1) ~= img_size || size(eidors_slice,2) ~= img_size
        eidors_slice = imresize(eidors_slice, [img_size img_size], 'bilinear');
    end
    eidors_slice(~circ_mask) = NaN;

    % ---- Load firmware CSV ----
    csv_path = fullfile(csv_dir, csv_name);
    if ~isfile(csv_path)
        fprintf('  SKIP: CSV not found (%s)\n\n', csv_name);
        T.Dataset(k) = base;
        T.CC_E(k) = NaN; T.SSIM_E(k) = NaN; T.GRE_E(k) = NaN; T.RMSE_E(k) = NaN;
        T.CC_M(k) = NaN; T.SSIM_M(k) = NaN; T.GRE_M(k) = NaN; T.RMSE_M(k) = NaN;
        T.Photo(k) = "";
        continue;
    end
    matriz_mcu = readmatrix(csv_path);
    % Apply same orientation correction as single-file validation
    matriz_mcu = rot90(matriz_mcu, 1);
    matriz_mcu = -matriz_mcu;

    % ---- Find phantom photo ----
    photo_path = '';
    for ext = {'.jpg', '.png', '.bmp'}
        candidate = fullfile(photo_dir, [photo_name ext{1}]);
        if isfile(candidate), photo_path = candidate; break; end
    end

    % ---- Normalize all three images to [0,1] ----
    mn = min(matriz_matlab(:),[],'omitnan'); mx = max(matriz_matlab(:),[],'omitnan');
    matlab_norm = (matriz_matlab - mn) / (mx - mn + eps);

    mn = min(eidors_slice(:),[],'omitnan'); mx = max(eidors_slice(:),[],'omitnan');
    eidors_norm = (eidors_slice - mn) / (mx - mn + eps);

    mn = min(matriz_mcu(:),[],'omitnan'); mx = max(matriz_mcu(:),[],'omitnan');
    mcu_norm = (matriz_mcu - mn) / (mx - mn + eps);

    % ---- Metrics: MCU vs EIDORS ----
    valid_e = ~isnan(mcu_norm) & ~isnan(eidors_norm);
    v_mcu_e = mcu_norm(valid_e); v_eid = eidors_norm(valid_e);
    gre_e  = norm(v_mcu_e - v_eid, 2) / (norm(v_eid, 2) + eps);
    rmse_e = sqrt(mean((v_mcu_e - v_eid).^2));
    cc_e   = corr(v_mcu_e, v_eid);
    ssim_img_e = mcu_norm; ssim_img_e(isnan(ssim_img_e)) = 0;
    ssim_ref_e = eidors_norm; ssim_ref_e(isnan(ssim_ref_e)) = 0;
    ssim_e = ssim(ssim_img_e, ssim_ref_e);

    % ---- Metrics: MCU vs MATLAB ----
    valid_m = ~isnan(mcu_norm) & ~isnan(matlab_norm);
    v_mcu_m = mcu_norm(valid_m); v_mat = matlab_norm(valid_m);
    gre_m  = norm(v_mcu_m - v_mat, 2) / (norm(v_mat, 2) + eps);
    rmse_m = sqrt(mean((v_mcu_m - v_mat).^2));
    cc_m   = corr(v_mcu_m, v_mat);
    ssim_img_m = mcu_norm; ssim_img_m(isnan(ssim_img_m)) = 0;
    ssim_ref_m = matlab_norm; ssim_ref_m(isnan(ssim_ref_m)) = 0;
    ssim_m = ssim(ssim_img_m, ssim_ref_m);

    fprintf('  MCU vs EIDORS:  CC=%.4f  SSIM=%.4f  GRE=%.4f  RMSE=%.4f\n', cc_e, ssim_e, gre_e, rmse_e);
    fprintf('  MCU vs MATLAB:  CC=%.4f  SSIM=%.4f  GRE=%.4f  RMSE=%.4f\n', cc_m, ssim_m, gre_m, rmse_m);

    % ---- Store results ----
    T.Dataset(k) = base;
    T.CC_E(k) = cc_e; T.SSIM_E(k) = ssim_e; T.GRE_E(k) = gre_e; T.RMSE_E(k) = rmse_e;
    T.CC_M(k) = cc_m; T.SSIM_M(k) = ssim_m; T.GRE_M(k) = gre_m; T.RMSE_M(k) = rmse_m;
    T.Photo(k) = photo_name;

    % ---- Generate comparison figure (3 rows) ----
    %   Row 1: Comparison data MCU vs EIDORS (text panel)
    %   Row 2: Images – Phantom | MCU | EIDORS | MATLAB
    %   Row 3: Comparison data MCU vs MATLAB (text panel)
    has_photo = ~isempty(photo_path);
    n_img = 3 + has_photo;   % 4 columns if photo, 3 otherwise

    fig = figure('Name', base, 'Color', 'white', 'Visible', 'off', ...
                 'Units', 'pixels', 'Position', [50 50 n_img*260 700]);

    % -- Row 1: MCU vs EIDORS metrics --
    ax1 = subplot(3, n_img, 1:n_img);
    set(ax1, 'Visible', 'off');
    text(0.5, 0.5, ...
        sprintf('MCU vs EIDORS:   CC = %.4f (ideal=1)   |   SSIM = %.4f (ideal=1)   |   GRE = %.4f (ideal=0)   |   RMSE = %.4f (ideal=0)', ...
                cc_e, ssim_e, gre_e, rmse_e), ...
        'HorizontalAlignment', 'center', 'VerticalAlignment', 'middle', ...
        'FontSize', 13, 'FontWeight', 'bold', 'Color', [0.1 0.3 0.7]);

    % -- Row 2: Images --
    col = 0;
    if has_photo
        col = col + 1;
        subplot(3, n_img, n_img + col);
        imshow(imread(photo_path));
        title('Phantom', 'FontSize', 11, 'FontWeight', 'bold');
    end

    col = col + 1;
    subplot(3, n_img, n_img + col);
    plot_mcu = mcu_norm; plot_mcu(isnan(plot_mcu)) = -0.1;
    imagesc(plot_mcu); axis square; axis off; colorbar; colormap(jet);
    title('MCU (LBP f32)', 'FontSize', 11);

    col = col + 1;
    subplot(3, n_img, n_img + col);
    plot_eid = eidors_norm; plot_eid(isnan(plot_eid)) = -0.1;
    imagesc(plot_eid); axis square; axis off; colorbar;
    title('EIDORS (GN)', 'FontSize', 11);

    col = col + 1;
    subplot(3, n_img, n_img + col);
    plot_mat = matlab_norm; plot_mat(isnan(plot_mat)) = -0.1;
    imagesc(plot_mat); axis square; axis off; colorbar;
    title('MATLAB (LBP f64)', 'FontSize', 11);

    % -- Row 3: MCU vs MATLAB metrics --
    ax3 = subplot(3, n_img, 2*n_img+1 : 3*n_img);
    set(ax3, 'Visible', 'off');
    text(0.5, 0.5, ...
        sprintf('MCU vs MATLAB:   CC = %.4f (ideal=1)   |   SSIM = %.4f (ideal=1)   |   GRE = %.4f (ideal=0)   |   RMSE = %.4f (ideal=0)', ...
                cc_m, ssim_m, gre_m, rmse_m), ...
        'HorizontalAlignment', 'center', 'VerticalAlignment', 'middle', ...
        'FontSize', 13, 'FontWeight', 'bold', 'Color', [0.1 0.6 0.2]);

    sgtitle(strrep(base,'_','\_'), 'FontSize', 15, 'FontWeight', 'bold');

    fig_path = fullfile(results_dir, [base '_comparison.png']);
    exportgraphics(fig, fig_path, 'Resolution', 150);
    close(fig);
    fprintf('  Saved: %s\n\n', fig_path);
end

%% ---- Summary table ----
fprintf('\n');
fprintf('================================================================\n');
fprintf('  RESUMO – MCU vs EIDORS e MCU vs MATLAB                       \n');
fprintf('================================================================\n');
fprintf('  Métricas ideais: GRE=0  RMSE=0  CC=1  SSIM=1                 \n');
fprintf('----------------------------------------------------------------\n');
disp(T);

valid_rows = ~isnan(T.CC_E) & ~isnan(T.CC_M);
fprintf('----------------------------------------------------------------\n');
fprintf('  MÉDIAS (%d datasets):\n', sum(valid_rows));
fprintf('  --- MCU vs EIDORS ---\n');
fprintf('    CC   = %.4f  (ideal = 1)\n', mean(T.CC_E(valid_rows)));
fprintf('    SSIM = %.4f  (ideal = 1)\n', mean(T.SSIM_E(valid_rows)));
fprintf('    GRE  = %.4f  (ideal = 0)\n', mean(T.GRE_E(valid_rows)));
fprintf('    RMSE = %.4f  (ideal = 0)\n', mean(T.RMSE_E(valid_rows)));
fprintf('  --- MCU vs MATLAB ---\n');
fprintf('    CC   = %.4f  (ideal = 1)\n', mean(T.CC_M(valid_rows)));
fprintf('    SSIM = %.4f  (ideal = 1)\n', mean(T.SSIM_M(valid_rows)));
fprintf('    GRE  = %.4f  (ideal = 0)\n', mean(T.GRE_M(valid_rows)));
fprintf('    RMSE = %.4f  (ideal = 0)\n', mean(T.RMSE_M(valid_rows)));
fprintf('================================================================\n');

csv_out = fullfile(results_dir, 'validation_summary.csv');
writetable(T, csv_out);
fprintf('Tabela salva em: %s\n', csv_out);

%% ---- Summary bar chart (grouped: EIDORS vs MATLAB) ----
fig2 = figure('Name', 'Summary', 'Color', 'white', 'Visible', 'off', ...
              'Units', 'pixels', 'Position', [50 50 1200 600]);

vr = find(valid_rows);
labels = T.Dataset(vr);

subplot(1,2,1);
bar_data_cc = [T.CC_E(vr), T.CC_M(vr)];
b = bar(bar_data_cc); hold on;
b(1).FaceColor = [0.2 0.4 0.8];   % blue  = vs EIDORS
b(2).FaceColor = [0.2 0.7 0.3];   % green = vs MATLAB
yline(mean(T.CC_E(vr)), 'b--', sprintf('EIDORS avg=%.3f', mean(T.CC_E(vr))), 'LineWidth', 1.2);
yline(mean(T.CC_M(vr)), 'g--', sprintf('MATLAB avg=%.3f', mean(T.CC_M(vr))), 'LineWidth', 1.2);
set(gca, 'XTick', 1:numel(vr), 'XTickLabel', labels, 'XTickLabelRotation', 60, 'FontSize', 7);
ylabel('Pearson CC'); title('Correlation Coefficient per Dataset');
legend('vs EIDORS', 'vs MATLAB', 'Location', 'southeast');
ylim([min(0, min(bar_data_cc(:))-0.05) 1.05]); grid on;

subplot(1,2,2);
bar_data_ssim = [T.SSIM_E(vr), T.SSIM_M(vr)];
b = bar(bar_data_ssim); hold on;
b(1).FaceColor = [0.2 0.4 0.8];
b(2).FaceColor = [0.2 0.7 0.3];
yline(mean(T.SSIM_E(vr)), 'b--', sprintf('EIDORS avg=%.3f', mean(T.SSIM_E(vr))), 'LineWidth', 1.2);
yline(mean(T.SSIM_M(vr)), 'g--', sprintf('MATLAB avg=%.3f', mean(T.SSIM_M(vr))), 'LineWidth', 1.2);
set(gca, 'XTick', 1:numel(vr), 'XTickLabel', labels, 'XTickLabelRotation', 60, 'FontSize', 7);
ylabel('SSIM'); title('Structural Similarity per Dataset');
legend('vs EIDORS', 'vs MATLAB', 'Location', 'southeast');
ylim([min(0, min(bar_data_ssim(:))-0.05) 1.05]); grid on;

sgtitle('Batch Validation: MCU vs EIDORS e MATLAB', 'FontSize', 14, 'FontWeight', 'bold');
exportgraphics(fig2, fullfile(results_dir, 'summary_chart.png'), 'Resolution', 150);
close(fig2);
fprintf('Summary chart saved to: %s\n', fullfile(results_dir, 'summary_chart.png'));

fprintf('\nDone! %d datasets processed.\n', n_files);

%% ========================================================================
%  Local function: LBP reconstruction using pre-built S matrix
%  ========================================================================
function img = do_lbp(S, U_ref, U_tgt, n_meas, n_inj, img_size, circ_mask)
    n_total = n_meas * n_inj;
    delta_v = zeros(n_total, 1);
    idx = 1;
    for inj = 1:n_inj
        for meas = 1:n_meas
            delta_v(idx) = U_tgt(meas, inj) - U_ref(meas, inj);
            idx = idx + 1;
        end
    end
    n_use = min(size(S,1), length(delta_v));
    image_vec = S(1:n_use,:)' * delta_v(1:n_use);
    img = reshape(image_vec, [img_size, img_size]);
    img(~circ_mask) = NaN;
end
