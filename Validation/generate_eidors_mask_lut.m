% =========================================================================
% Generate EIDORS mask LUT (32x32) for consistent MCU/PC masking
%
% Outputs:
%   - eidors_mask_32.csv                    (for MATLAB scripts)
%   - ../Firmware/src/algorithms/eidors_mask_32.h
%   - ../Firmware/src/algorithms/eidors_mask_32.c
% =========================================================================
clear; clc;

script_dir = fileparts(mfilename('fullpath'));
project_root = fullfile(script_dir, '..');
fips_dir = fullfile(project_root, 'FIPS Data');

img_size = 32;
out_csv = fullfile(fips_dir, 'eidors_mask_32.csv');
out_h   = fullfile(project_root, 'Firmware', 'src', 'algorithms', 'eidors_mask_32.h');
out_c   = fullfile(project_root, 'Firmware', 'src', 'algorithms', 'eidors_mask_32.c');

%% ---- EIDORS bootstrap ----
if ~exist('mk_common_model', 'file')
    eidors_dir = getenv('EIDORS_DIR');
    if ~isempty(eidors_dir)
        startup_path = fullfile(eidors_dir, 'startup.m');
        if isfile(startup_path)
            fprintf('EIDORS_DIR encontrado: %s\n', startup_path);
            run(startup_path);
        end
    end

    candidate_dirs = {fullfile(pwd, 'eidors'), fullfile(pwd, '..', 'eidors'), fullfile(pwd, '..', '..', 'eidors')};
    for i = 1:numel(candidate_dirs)
        startup_path = fullfile(candidate_dirs{i}, 'startup.m');
        if isfile(startup_path)
            fprintf('EIDORS encontrado localmente: %s\n', startup_path);
            run(startup_path);
            break;
        end
    end
end

if ~exist('mk_common_model', 'file')
    error(['EIDORS não encontrado no path do MATLAB. ' ...
           'Defina EIDORS_DIR ou execute eidors/startup.m antes.']);
end

%% ---- Build EIDORS mask on 32x32 grid ----
mdl = mk_common_model('c2c2', 16);
img = mk_image(mdl.fwd_model, 1);
opts.resolution = img_size;
sl = calc_slices(img, opts);

if size(sl,1) ~= img_size || size(sl,2) ~= img_size
    sl = imresize(sl, [img_size img_size], 'nearest');
end

mask = ~isnan(sl);
mask_u8 = uint8(mask);

%% ---- Save CSV for MATLAB ----
writematrix(mask_u8, out_csv);
fprintf('CSV salvo em: %s\n', out_csv);

%% ---- Emit C header/source for firmware ----
fid_h = fopen(out_h, 'w');
if fid_h < 0
    error('Falha ao criar header: %s', out_h);
end
fprintf(fid_h, '#ifndef EIDORS_MASK_32_H\n');
fprintf(fid_h, '#define EIDORS_MASK_32_H\n\n');
fprintf(fid_h, '#include <stdint.h>\n\n');
fprintf(fid_h, '#define EIDORS_MASK_SIZE 32u\n');
fprintf(fid_h, '#define EIDORS_MASK_PIXELS (EIDORS_MASK_SIZE * EIDORS_MASK_SIZE)\n\n');
fprintf(fid_h, 'extern const uint8_t g_eidors_mask_32[EIDORS_MASK_PIXELS];\n\n');
fprintf(fid_h, '#endif /* EIDORS_MASK_32_H */\n');
fclose(fid_h);

flat = reshape(mask_u8', 1, []); % row-major (y,x)

fid_c = fopen(out_c, 'w');
if fid_c < 0
    error('Falha ao criar source: %s', out_c);
end
fprintf(fid_c, '#include "eidors_mask_32.h"\n\n');
fprintf(fid_c, 'const uint8_t g_eidors_mask_32[EIDORS_MASK_PIXELS] = {\n');
for i = 1:numel(flat)
    if mod(i-1, 32) == 0
        fprintf(fid_c, '    ');
    end
    if i < numel(flat)
        fprintf(fid_c, '%u, ', flat(i));
    else
        fprintf(fid_c, '%u', flat(i));
    end
    if mod(i, 32) == 0
        fprintf(fid_c, '\n');
    end
end
fprintf(fid_c, '};\n');
fclose(fid_c);

fprintf('Header salvo em: %s\n', out_h);
fprintf('Source salvo em: %s\n', out_c);
fprintf('Done. Máscara válida: %d/%d pixels.\n', nnz(mask), numel(mask));
