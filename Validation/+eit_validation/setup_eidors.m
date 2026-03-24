function [imdl, enabled] = setup_eidors(n_elec, CP, MP, hp)
%SETUP_EIDORS Initialize EIDORS inverse model for Gauss-Newton reconstruction.
%
%   [imdl, enabled] = eit_validation.setup_eidors(n_elec, CP, MP, hp)
%
%   Inputs:
%     n_elec - Number of electrodes
%     CP     - Current injection patterns (n_elec x n_inj)
%     MP     - Measurement patterns (n_meas x n_elec)
%     hp     - Hyperparameter value for Tikhonov regularization
%
%   Outputs:
%     imdl    - EIDORS inverse model (empty if EIDORS not available)
%     enabled - true if EIDORS is available and configured

    enabled = true;
    imdl = [];

    % Try to load EIDORS if not already available
    if ~exist('mk_common_model', 'file')
        eidors_dir = getenv('EIDORS_DIR');
        if ~isempty(eidors_dir)
            startup_path = fullfile(eidors_dir, 'startup.m');
            if isfile(startup_path)
                fprintf('EIDORS_DIR found. Running: %s\n', startup_path);
                run(startup_path);
            end
        end

        % Try common local paths
        candidate_dirs = {
            fullfile(pwd, 'eidors'), ...
            fullfile(pwd, '..', 'eidors'), ...
            fullfile(pwd, '..', '..', 'eidors'), ...
            'C:\Users\g_kol\Downloads\eidors-v3.12-ng\eidors-v3.12-ng\eidors'
        };
        for i = 1:numel(candidate_dirs)
            startup_path = fullfile(candidate_dirs{i}, 'startup.m');
            if isfile(startup_path)
                fprintf('EIDORS found locally. Running: %s\n', startup_path);
                run(startup_path);
                break;
            end
        end
    end

    if ~exist('mk_common_model', 'file')
        enabled = false;
        fprintf('Warning: EIDORS not found. Running without EIDORS.\n');
        return;
    end

    fprintf('Configuring EIDORS model (Gauss-Newton + Tikhonov)...\n');

    mdl_base = mk_common_model('c2c2', n_elec);
    fmdl = mdl_base.fwd_model;

    % Override stimulation patterns to match data
    n_inj = size(CP, 2);
    clear stim_eidors;
    for i = 1:n_inj
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
    imdl.hyperparameter.value = hp;
    imdl.RtR_prior = @prior_tikhonov;
    imdl.jacobian_bkgnd.value = 1;

    fprintf('  Gauss-Newton (Tikhonov prior, hp=%.3f)\n', hp);
    fprintf('EIDORS model ready.\n');
end
