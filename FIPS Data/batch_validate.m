% =========================================================================
% VALIDAÇÃO EM LOTE: MCU (LBP) vs EIDORS (Gauss-Newton) e MATLAB (LBP)
%
% Percorre todos os datamat_*.mat (exceto a referência datamat_1_0),
% reconstrói com EIDORS (Gauss-Newton + Tikhonov) e MATLAB LBP
% (S calculada localmente), carrega o CSV do firmware e a foto do fantoma,
% calcula métricas e gera figuras comparativas salvas em results_matlab/.
%
% Layout da figura por dataset:
%   Topo:   Imagens – Phantom | MCU | EIDORS | MATLAB
%   Baixo:  Resumo das métricas de comparação
% =========================================================================
clear; close all; clc;

%% ---- Paths ----
mat_dir      = fullfile(pwd, 'data_mat_files');
csv_dir      = fullfile(pwd, 'firmware_data');
photo_dir    = fullfile(pwd, 'target_photos');
results_root = fullfile(pwd, 'results_matlab');
results_subdir = strtrim(getenv('EIT_RESULTS_SUBDIR'));
if isempty(results_subdir)
    results_dir = results_root;
else
    results_dir = fullfile(results_root, results_subdir);
end
if ~exist(results_dir, 'dir'), mkdir(results_dir); end
fprintf('Diretório de resultados: %s\n', results_dir);

ref_file = 'datamat_1_0.mat';
img_size = 32;
n_elec   = 16;
grid_extent = 1.1;
min_dist    = 0.4;

% ---- Mask controls ----
% Circular mask used as initial mask in LBP; final mask is EIDORS runtime mask.
mask_radius_offset_px = 0.0;   % increase/decrease to tune circumference
mask_radius_offset_px_env = getenv('EIT_MASK_RADIUS_OFFSET_PX');
if ~isempty(mask_radius_offset_px_env)
    tmp = str2double(mask_radius_offset_px_env);
    if ~isnan(tmp)
        mask_radius_offset_px = tmp;
    end
end
fprintf('Mask radius offset: %.2f px\n', mask_radius_offset_px);

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

% Validate data consistency
if n_meas ~= n_meas_pat || n_inj ~= n_inj_pat
    error(['Dimensões incompatíveis: U_ref é %dx%d mas patterns indicam %dx%d. ' ...
           'Verifique CurrentPattern e MeasPattern.'], n_meas, n_inj, n_meas_pat, n_inj_pat);
end

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

%% ---- Circular mask (for initial LBP, overridden by EIDORS mask later) ----
circ_mask = make_circular_mask(img_size, mask_radius_offset_px);
radius = (img_size / 2) + mask_radius_offset_px;
fprintf('Máscara circular: radius=%.2f px, %d pixels\n', radius, nnz(circ_mask));

%% ---- EIDORS model setup ----
enable_eidors = true;
if ~exist('mk_common_model','file')
    eidors_dir = getenv('EIDORS_DIR');
    if ~isempty(eidors_dir)
        startup_path = fullfile(eidors_dir, 'startup.m');
        if isfile(startup_path)
            fprintf('EIDORS_DIR encontrado. Rodando: %s\n', startup_path);
            run(startup_path);
        end
    end

    candidate_dirs = {
        fullfile(pwd, 'eidors'), ...
        fullfile(pwd, '..', 'eidors'), ...
        fullfile(pwd, '..', '..', 'eidors'), ...
        'C:\\Users\\g_kol\\Downloads\\eidors-v3.12-ng\\eidors-v3.12-ng\\eidors'
    };
    for i = 1:numel(candidate_dirs)
        startup_path = fullfile(candidate_dirs{i}, 'startup.m');
        if isfile(startup_path)
            fprintf('EIDORS encontrado localmente. Rodando: %s\n', startup_path);
            run(startup_path);
            break;
        end
    end
end
if ~exist('mk_common_model','file')
    enable_eidors = false;
    fprintf(['Aviso: EIDORS não encontrado no path do MATLAB. ' ...
             'Rodando em modo sem EIDORS (apenas MCU vs MATLAB).\n']);
else
    fprintf('Configurando modelo EIDORS (Gauss-Newton + Tikhonov)...\n');
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

    % Gauss-Newton + Tikhonov
    imdl = eidors_obj('inv_model', 'FIPS_GN');
    imdl.fwd_model = fmdl;
    imdl.reconst_type = 'difference';
    imdl.solve = @inv_solve_diff_GN_one_step;
    imdl.hyperparameter.value = 0.03;
    imdl.RtR_prior = @prior_tikhonov;
    imdl.jacobian_bkgnd.value = 1;
    fprintf('  Gauss-Newton (Tikhonov prior, hp=0.03)\n');
    fprintf('Modelo EIDORS pronto.\n');
end

%% ---- List target .mat files ----
mat_files = dir(fullfile(mat_dir, 'datamat_*.mat'));
mat_files(strcmpi({mat_files.name}, ref_file)) = [];
n_files = numel(mat_files);
fprintf('\nEncontrados %d datasets-alvo.\n\n', n_files);

%% ---- Pre-allocate results table ----
% Columns: Dataset, EIDORS metrics, MATLAB metrics, Photo
T = table('Size', [n_files 12], ...
    'VariableTypes', {'string', ...
        'double','double','double','double','double', ...  % EIDORS
        'double','double','double','double','double', ...  % MATLAB
        'string'}, ...
    'VariableNames', {'Dataset', ...
        'CC_E','SSIM_E','GRE_E','RMSE_E','PSNR_E', ...
        'CC_M','SSIM_M','GRE_M','RMSE_M','PSNR_M', ...
        'Photo'});

%% ---- Helper: MATLAB LBP reconstruction ----
reconstruct_lbp = @(U_tgt_mat) do_lbp(S, U_ref, U_tgt_mat, n_meas, n_inj, img_size, circ_mask);

% ---- Helper: SSIM (built-in if available, else global fallback) ----
use_builtin_ssim = exist('ssim','file') == 2;
if ~use_builtin_ssim
    fprintf('Aviso: função ssim() não encontrada. Usando SSIM global (fallback).\n');
end

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
    if enable_eidors
        vh = U_ref(:);             % homogeneous (reference)
        vi = U_tgt(:);             % inhomogeneous (target)
        opts_eid.resolution = img_size;

        img_eidors = inv_solve(imdl, vh, vi);
        slice_eidors = calc_slices(img_eidors, opts_eid);
        if size(slice_eidors,1) ~= img_size || size(slice_eidors,2) ~= img_size
            slice_eidors = imresize(slice_eidors, [img_size img_size], 'bilinear');
        end
        eidors_mask = ~isnan(slice_eidors);
        fprintf('  EIDORS: %dx%d\n', size(slice_eidors,1), size(slice_eidors,2));
    else
        slice_eidors = NaN(img_size, img_size);
        eidors_mask = circ_mask;
    end

    % ---- Load firmware CSV ----
    csv_path = fullfile(csv_dir, csv_name);
    if ~isfile(csv_path)
        fprintf('  SKIP: CSV not found (%s)\n\n', csv_name);
        T.Dataset(k) = base;
        T.CC_E(k) = NaN; T.SSIM_E(k) = NaN; T.GRE_E(k) = NaN; T.RMSE_E(k) = NaN; T.PSNR_E(k) = NaN;
        T.CC_M(k) = NaN; T.SSIM_M(k) = NaN; T.GRE_M(k) = NaN; T.RMSE_M(k) = NaN; T.PSNR_M(k) = NaN;
        T.Photo(k) = "";
        continue;
    end
    matriz_mcu = readmatrix(csv_path);

    % Enforce a common mask across all methods.
    common_mask = eidors_mask;
    fprintf('  Mask pixels: %d\n', sum(common_mask(:)));

    % Apply mask to all images
    matriz_mcu(~common_mask) = NaN;
    matriz_matlab(~common_mask) = NaN;
    slice_eidors(~common_mask) = NaN;

    % ---- Find phantom photo ----
    photo_path = '';
    for ext = {'.jpg', '.png', '.bmp'}
        candidate = fullfile(photo_dir, [photo_name ext{1}]);
        if isfile(candidate), photo_path = candidate; break; end
    end

    % ---- Normalize all images to [0,1] ----
    mcu_norm = normalize_image(matriz_mcu);
    matlab_norm = normalize_image(matriz_matlab);
    eidors_norm = normalize_image(slice_eidors);

    % ---- Metrics: MCU vs EIDORS and MCU vs MATLAB ----
    [cc_e, ssim_e, gre_e, rmse_e, psnr_e] = compute_all_metrics(mcu_norm, eidors_norm, use_builtin_ssim);
    [cc_m, ssim_m, gre_m, rmse_m, psnr_m] = compute_all_metrics(mcu_norm, matlab_norm, use_builtin_ssim);

    if enable_eidors
        fprintf('  MCU vs EIDORS: CC=%.4f  SSIM=%.4f  RMSE=%.4f  PSNR=%.2f dB\n', cc_e, ssim_e, rmse_e, psnr_e);
    end
    fprintf('  MCU vs MATLAB: CC=%.4f  SSIM=%.4f  RMSE=%.4f  PSNR=%.2f dB\n', cc_m, ssim_m, rmse_m, psnr_m);

    % ---- Store results ----
    T.Dataset(k) = base;
    T.CC_E(k) = cc_e; T.SSIM_E(k) = ssim_e; T.GRE_E(k) = gre_e; T.RMSE_E(k) = rmse_e; T.PSNR_E(k) = psnr_e;
    T.CC_M(k) = cc_m; T.SSIM_M(k) = ssim_m; T.GRE_M(k) = gre_m; T.RMSE_M(k) = rmse_m; T.PSNR_M(k) = psnr_m;
    T.Photo(k) = photo_name;

    % ---- Generate comparison figure ----
    % Layout: 2 rows
    %   Row 1: Images – Phantom | MCU | EIDORS | MATLAB
    %   Row 2: Metrics summary text
    has_photo = ~isempty(photo_path);
    n_img = 3 + has_photo;   % 4 columns if photo

    fig = figure('Name', base, 'Color', 'white', 'Visible', 'off', ...
                 'Units', 'pixels', 'Position', [50 50 n_img*220 500]);

    % -- Row 1: Images --
    col = 0;
    if has_photo
        col = col + 1;
        subplot(2, n_img, col);
        imshow(imread(photo_path));
        title('Phantom', 'FontSize', 10, 'FontWeight', 'bold');
    end

    col = col + 1;
    subplot(2, n_img, col);
    imagesc(mcu_norm, 'AlphaData', ~isnan(mcu_norm)); axis square; axis off; colorbar; colormap(jet); caxis([0 1]);
    set(gca, 'Color', [0 0 0]);
    title('MCU (LBP)', 'FontSize', 10);

    col = col + 1;
    subplot(2, n_img, col);
    imagesc(eidors_norm, 'AlphaData', ~isnan(eidors_norm)); axis square; axis off; colorbar; caxis([0 1]);
    set(gca, 'Color', [0 0 0]);
    title(sprintf('EIDORS (CC=%.3f)', cc_e), 'FontSize', 10);

    col = col + 1;
    subplot(2, n_img, col);
    imagesc(matlab_norm, 'AlphaData', ~isnan(matlab_norm)); axis square; axis off; colorbar; caxis([0 1]);
    set(gca, 'Color', [0 0 0]);
    title(sprintf('MATLAB (CC=%.3f)', cc_m), 'FontSize', 10);

    % -- Row 2: Metrics text with ideal values and units --
    ax2 = subplot(2, n_img, n_img+1 : 2*n_img);
    set(ax2, 'Visible', 'off');
    metrics_text = sprintf([ ...
        '                   MCU vs EIDORS       MCU vs MATLAB       Ideal\n' ...
        '  CC  [-1,1]       %+.4f              %+.4f              1\n' ...
        '  SSIM [0,1]       %.4f               %.4f               1\n' ...
        '  GRE  [0,inf)     %.4f               %.4f               0\n' ...
        '  RMSE [0,1]       %.4f               %.4f               0\n' ...
        '  PSNR [dB]        %.2f               %.2f               inf'], ...
        cc_e, cc_m, ssim_e, ssim_m, gre_e, gre_m, rmse_e, rmse_m, psnr_e, psnr_m);
    text(0.5, 0.5, metrics_text, ...
        'HorizontalAlignment', 'center', 'VerticalAlignment', 'middle', ...
        'FontSize', 10, 'FontName', 'FixedWidth');

    sgtitle(strrep(base,'_','\_'), 'FontSize', 12, 'FontWeight', 'bold');

    fig_path = fullfile(results_dir, [base '_comparison.png']);
    exportgraphics(fig, fig_path, 'Resolution', 150);
    close(fig);
    fprintf('  Saved: %s\n\n', fig_path);
end

%% ---- Summary table ----
fprintf('\n');
fprintf('================================================================\n');
fprintf('  RESUMO – MCU vs EIDORS (Gauss-Newton) e MATLAB (LBP)         \n');
fprintf('================================================================\n');
fprintf('  Métricas:                                                    \n');
fprintf('    CC   (Correlation Coefficient)  [-1,1]   ideal: 1          \n');
fprintf('    SSIM (Structural Similarity)    [0,1]    ideal: 1          \n');
fprintf('    GRE  (Gradient Relative Error)  [0,inf)  ideal: 0          \n');
fprintf('    RMSE (Root Mean Square Error)   [0,1]    ideal: 0          \n');
fprintf('    PSNR (Peak SNR)                 [dB]     ideal: inf        \n');
fprintf('----------------------------------------------------------------\n');
disp(T(:, {'Dataset','CC_E','SSIM_E','CC_M','SSIM_M'}));

valid_rows_e = ~isnan(T.CC_E);
valid_rows_m = ~isnan(T.CC_M);
valid_rows = valid_rows_e | valid_rows_m;

fprintf('----------------------------------------------------------------\n');
fprintf('  MÉDIAS:\n');
if any(valid_rows_e)
    fprintf('    EIDORS (GN): CC=%.4f  SSIM=%.4f  RMSE=%.4f  PSNR=%.2f dB\n', ...
        mean(T.CC_E(valid_rows_e)), mean(T.SSIM_E(valid_rows_e)), ...
        mean(T.RMSE_E(valid_rows_e)), mean(T.PSNR_E(valid_rows_e)));
end
if any(valid_rows_m)
    fprintf('    MATLAB LBP:  CC=%.4f  SSIM=%.4f  RMSE=%.4f  PSNR=%.2f dB\n', ...
        mean(T.CC_M(valid_rows_m)), mean(T.SSIM_M(valid_rows_m)), ...
        mean(T.RMSE_M(valid_rows_m)), mean(T.PSNR_M(valid_rows_m)));
end
fprintf('================================================================\n');

csv_out = fullfile(results_dir, 'validation_summary.csv');
writetable(T, csv_out);
fprintf('Tabela salva em: %s\n', csv_out);

%% ---- Export LaTeX macro summary (means/min/max) ----
T_valid = T(valid_rows, :);
tex_out = fullfile(results_dir, 'metrics_summary.tex');
write_metrics_tex(tex_out, T_valid);
fprintf('Resumo LaTeX salvo em: %s\n', tex_out);

%% ---- Summary bar chart ----
fig2 = figure('Name', 'Summary', 'Color', 'white', 'Visible', 'off', ...
              'Units', 'pixels', 'Position', [50 50 1200 500]);

vr = find(valid_rows_m);
labels = T.Dataset(vr);

if isempty(vr)
    ax = axes(fig2);
    set(ax, 'Visible', 'off');
    text(0.5, 0.5, 'Sem dados válidos para gráfico-resumo.', ...
        'HorizontalAlignment', 'center', 'VerticalAlignment', 'middle', ...
        'FontSize', 14, 'FontWeight', 'bold');
    exportgraphics(fig2, fullfile(results_dir, 'summary_chart.png'), 'Resolution', 150);
    close(fig2);
    fprintf('Summary chart saved to: %s\n', fullfile(results_dir, 'summary_chart.png'));
    fprintf('\nDone! %d datasets processed.\n', n_files);
    return;
end

% CC comparison
subplot(1,2,1);
bar_data_cc = [T.CC_E(vr), T.CC_M(vr)];
b = bar(bar_data_cc); hold on;
b(1).FaceColor = [0.2 0.4 0.8];   % blue  = EIDORS
b(2).FaceColor = [0.2 0.7 0.3];   % green = MATLAB
set(gca, 'XTick', 1:numel(vr), 'XTickLabel', labels, 'XTickLabelRotation', 60, 'FontSize', 8);
ylabel('CC [-1,1], ideal=1'); title('Correlation Coefficient (MCU vs each method)');
legend('EIDORS (GN)', 'MATLAB LBP', 'Location', 'southeast');
ylim([min(0, min(bar_data_cc(:),[],'omitnan')-0.05) 1.05]); grid on;

% SSIM comparison
subplot(1,2,2);
bar_data_ssim = [T.SSIM_E(vr), T.SSIM_M(vr)];
b = bar(bar_data_ssim); hold on;
b(1).FaceColor = [0.2 0.4 0.8];
b(2).FaceColor = [0.2 0.7 0.3];
set(gca, 'XTick', 1:numel(vr), 'XTickLabel', labels, 'XTickLabelRotation', 60, 'FontSize', 8);
ylabel('SSIM [0,1], ideal=1'); title('Structural Similarity (MCU vs each method)');
legend('EIDORS (GN)', 'MATLAB LBP', 'Location', 'southeast');
ylim([min(0, min(bar_data_ssim(:),[],'omitnan')-0.05) 1.05]); grid on;

sgtitle('Batch Validation: MCU vs EIDORS & MATLAB', 'FontSize', 14, 'FontWeight', 'bold');
exportgraphics(fig2, fullfile(results_dir, 'summary_chart.png'), 'Resolution', 150);
close(fig2);
fprintf('Summary chart saved to: %s\n', fullfile(results_dir, 'summary_chart.png'));

fprintf('\nDone! %d datasets processed.\n', n_files);

%% ========================================================================
%  Local function: LBP reconstruction using pre-built S matrix
%  ========================================================================
function img = do_lbp(S, U_ref, U_tgt, n_meas, n_inj, img_size, ~)
    delta_v = zeros(n_meas * n_inj, 1);
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
end

function mask = make_circular_mask(img_size, radius_offset_px)
    center = (img_size - 1) / 2;
    radius = (img_size / 2) + radius_offset_px;
    mask = true(img_size);
    for iy = 0:img_size-1
        for ix = 0:img_size-1
            if sqrt((ix-center)^2 + (iy-center)^2) > radius
                mask(iy+1, ix+1) = false;
            end
        end
    end
end

function s = compute_ssim(img_a, img_b, use_builtin_ssim)
    % Handle NaN by replacing with 0 (masked regions)
    img_a_clean = img_a;
    img_b_clean = img_b;
    img_a_clean(isnan(img_a)) = 0;
    img_b_clean(isnan(img_b)) = 0;

    if use_builtin_ssim
        s = ssim(img_a_clean, img_b_clean);
        return;
    end
    s = ssim_global(img_a_clean, img_b_clean);
end

function s = ssim_global(img_a, img_b)
    % Global SSIM fallback (no windowing). Assumes inputs are double in [0,1].
    a = double(img_a(:));
    b = double(img_b(:));

    % Remove any remaining NaN
    valid = ~isnan(a) & ~isnan(b);
    a = a(valid);
    b = b(valid);

    if isempty(a)
        s = NaN;
        return;
    end

    mu_a = mean(a);
    mu_b = mean(b);
    var_a = mean((a - mu_a).^2);
    var_b = mean((b - mu_b).^2);
    cov_ab = mean((a - mu_a) .* (b - mu_b));
    C1 = (0.01)^2;
    C2 = (0.03)^2;
    s = ((2*mu_a*mu_b + C1) * (2*cov_ab + C2)) / ((mu_a^2 + mu_b^2 + C1) * (var_a + var_b + C2));
end

function p = compute_psnr(rmse_val)
    if isnan(rmse_val)
        p = NaN;
    elseif rmse_val <= 0
        p = Inf;
    else
        % Inputs are normalized to [0,1], so max signal level is 1.
        p = 20 * log10(1 / rmse_val);
    end
end

function img_norm = normalize_image(img)
    % Normalize image to [0,1] range, ignoring NaN values
    valid = img(~isnan(img));
    if isempty(valid)
        img_norm = img;
        return;
    end
    minv = min(valid);
    maxv = max(valid);
    if maxv - minv < eps
        img_norm = zeros(size(img));
        img_norm(isnan(img)) = NaN;
    else
        img_norm = (img - minv) / (maxv - minv);
    end
end

function [cc, ssim_val, gre, rmse_val, psnr_val] = compute_all_metrics(ref_norm, tgt_norm, use_builtin_ssim)
    % Compute all metrics comparing ref (MCU) vs tgt (method)
    % Both inputs should be normalized to [0,1]
    valid_mask = ~isnan(ref_norm) & ~isnan(tgt_norm);
    a = ref_norm(valid_mask);
    b = tgt_norm(valid_mask);

    if isempty(a) || numel(a) < 2
        cc = NaN; ssim_val = NaN; gre = NaN; rmse_val = NaN; psnr_val = NaN;
        return;
    end

    % Correlation coefficient
    cc = corr(a(:), b(:));

    % SSIM
    ssim_val = compute_ssim(ref_norm, tgt_norm, use_builtin_ssim);

    % Gradient Relative Error (GRE)
    [gx_a, gy_a] = gradient(ref_norm);
    [gx_b, gy_b] = gradient(tgt_norm);
    grad_diff = sqrt((gx_a - gx_b).^2 + (gy_a - gy_b).^2);
    grad_ref  = sqrt(gx_a.^2 + gy_a.^2);
    valid_grad = valid_mask & (grad_ref > eps);
    if any(valid_grad(:))
        gre = mean(grad_diff(valid_grad) ./ grad_ref(valid_grad));
    else
        gre = NaN;
    end

    % RMSE
    rmse_val = sqrt(mean((a(:) - b(:)).^2));

    % PSNR
    psnr_val = compute_psnr(rmse_val);
end

function write_metrics_tex(tex_path, T_valid)
    n = height(T_valid);

    fid = fopen(tex_path, 'w');
    if fid < 0
        error('Não foi possível escrever: %s', tex_path);
    end

    fprintf(fid, '%% Auto-generated by batch_validate.m on %s\n', datestr(now, 31));
    fprintf(fid, '%% Metrics: CC[-1,1] SSIM[0,1] GRE[0,inf) RMSE[0,1] PSNR[dB]\n');
    fprintf(fid, '%% Ideal values: CC=1, SSIM=1, GRE=0, RMSE=0, PSNR=inf\n\n');
    fprintf(fid, '\\newcommand{\\NMatlabDatasets}{%d}\n\n', n);

    % EIDORS metrics
    fprintf(fid, '%% EIDORS (Gauss-Newton + Tikhonov)\n');
    fprintf(fid, '\\newcommand{\\EidorsCCMean}{%.4f}\n', mean(T_valid.CC_E, 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsCCMin}{%.4f}\n', min(T_valid.CC_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsCCMax}{%.4f}\n', max(T_valid.CC_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsSSIMMean}{%.4f}\n', mean(T_valid.SSIM_E, 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsSSIMMin}{%.4f}\n', min(T_valid.SSIM_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsSSIMMax}{%.4f}\n', max(T_valid.SSIM_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsGREMean}{%.4f}\n', mean(T_valid.GRE_E, 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsGREMin}{%.4f}\n', min(T_valid.GRE_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsGREMax}{%.4f}\n', max(T_valid.GRE_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsRMSEMean}{%.4f}\n', mean(T_valid.RMSE_E, 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsRMSEMin}{%.4f}\n', min(T_valid.RMSE_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsRMSEMax}{%.4f}\n', max(T_valid.RMSE_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsPSNRMean}{%.2f}\n', mean(T_valid.PSNR_E, 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsPSNRMin}{%.2f}\n', min(T_valid.PSNR_E, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\EidorsPSNRMax}{%.2f}\n\n', max(T_valid.PSNR_E, [], 'omitnan'));

    % MATLAB metrics
    fprintf(fid, '%% MATLAB LBP\n');
    fprintf(fid, '\\newcommand{\\MatlabCCMean}{%.4f}\n', mean(T_valid.CC_M, 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabCCMin}{%.4f}\n', min(T_valid.CC_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabCCMax}{%.4f}\n', max(T_valid.CC_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabSSIMMean}{%.4f}\n', mean(T_valid.SSIM_M, 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabSSIMMin}{%.4f}\n', min(T_valid.SSIM_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabSSIMMax}{%.4f}\n', max(T_valid.SSIM_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabGREMean}{%.4f}\n', mean(T_valid.GRE_M, 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabGREMin}{%.4f}\n', min(T_valid.GRE_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabGREMax}{%.4f}\n', max(T_valid.GRE_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabRMSEMean}{%.4f}\n', mean(T_valid.RMSE_M, 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabRMSEMin}{%.4f}\n', min(T_valid.RMSE_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabRMSEMax}{%.4f}\n', max(T_valid.RMSE_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabPSNRMean}{%.2f}\n', mean(T_valid.PSNR_M, 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabPSNRMin}{%.2f}\n', min(T_valid.PSNR_M, [], 'omitnan'));
    fprintf(fid, '\\newcommand{\\MatlabPSNRMax}{%.2f}\n', max(T_valid.PSNR_M, [], 'omitnan'));

    fclose(fid);
end
