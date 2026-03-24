function cfg = get_config()
%GET_CONFIG Returns configuration struct for EIT validation.
%
%   cfg = eit_validation.get_config()
%
%   Environment variables (optional):
%     EIT_RESULTS_SUBDIR       - Subdirectory for results (default: '')
%     EIT_MASK_RADIUS_OFFSET_PX - Mask radius offset in pixels (default: 0.0)

    script_dir = fileparts(mfilename('fullpath'));
    validation_dir = fileparts(script_dir);  % parent of +eit_validation

    % Paths
    cfg.validation_dir = validation_dir;
    cfg.fips_dir = fullfile(validation_dir, '..', 'FIPS Data');
    cfg.mat_dir = fullfile(cfg.fips_dir, 'data_mat_files');
    cfg.csv_dir = fullfile(cfg.fips_dir, 'firmware_data');
    cfg.photo_dir = fullfile(cfg.fips_dir, 'target_photos');

    % Results directory
    results_root = fullfile(validation_dir, 'results_matlab');
    results_subdir = strtrim(getenv('EIT_RESULTS_SUBDIR'));
    if isempty(results_subdir)
        cfg.results_dir = results_root;
    else
        cfg.results_dir = fullfile(results_root, results_subdir);
    end
    if ~exist(cfg.results_dir, 'dir')
        mkdir(cfg.results_dir);
    end

    % Reference file
    cfg.ref_file = 'datamat_1_0.mat';

    % Image parameters
    cfg.img_size = 32;
    cfg.n_elec = 16;
    cfg.grid_extent = 1.1;
    cfg.min_dist = 0.4;

    % Mask parameters
    cfg.mask_radius_offset_px = 0.0;
    offset_env = getenv('EIT_MASK_RADIUS_OFFSET_PX');
    if ~isempty(offset_env)
        tmp = str2double(offset_env);
        if ~isnan(tmp)
            cfg.mask_radius_offset_px = tmp;
        end
    end

    % EIDORS hyperparameters
    cfg.eidors_hp = 0.03;
end
