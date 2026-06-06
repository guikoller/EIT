function S = build_sensitivity_matrix(cfg, CP, MP)
%BUILD_SENSITIVITY_MATRIX Build sensitivity matrix S for LBP reconstruction.
%
%   S = eit_validation.build_sensitivity_matrix(cfg, CP, MP)
%
%   Inputs:
%     cfg - Configuration struct from get_config()
%     CP  - Current injection patterns (n_elec x n_inj)
%     MP  - Measurement patterns (n_meas x n_elec)
%
%   Output:
%     S - Sensitivity matrix (n_measurements x n_pixels)

    img_size = cfg.img_size;
    n_elec = cfg.n_elec;
    grid_extent = cfg.grid_extent;
    min_dist = cfg.min_dist;

    % Electrode positions on unit circle
    angles = linspace(0, 2*pi, n_elec+1);
    angles(end) = [];
    el_pos = [cos(angles)', sin(angles)'];

    % Reconstruction grid
    x = linspace(-grid_extent, grid_extent, img_size);
    y = linspace(-grid_extent, grid_extent, img_size);
    [X, Y] = meshgrid(x, y);
    px = X(:);
    py = Y(:);
    n_pixels = img_size^2;

    % Electrode fields
    eps_val = 1e-12;
    elec_field_x = zeros(n_elec, n_pixels);
    elec_field_y = zeros(n_elec, n_pixels);
    for e = 1:n_elec
        rx = px - el_pos(e, 1);
        ry = py - el_pos(e, 2);
        dist_clipped = max(sqrt(rx.^2 + ry.^2), min_dist);
        denom = dist_clipped.^2 + eps_val;
        elec_field_x(e, :) = (rx ./ denom)';
        elec_field_y(e, :) = (ry ./ denom)';
    end

    % Build sensitivity matrix
    n_inj = size(CP, 2);
    n_meas = size(MP, 1);
    S = zeros(n_inj * n_meas, n_pixels);

    fprintf('Building sensitivity matrix...\n');
    row = 0;
    for inj = 1:n_inj
        inj_pattern = CP(:, inj);
        E_inj_x = inj_pattern' * elec_field_x;
        E_inj_y = inj_pattern' * elec_field_y;
        for meas = 1:n_meas
            meas_pattern = MP(meas, :);
            E_meas_x = meas_pattern * elec_field_x;
            E_meas_y = meas_pattern * elec_field_y;
            sensitivity = E_inj_x .* E_meas_x + E_inj_y .* E_meas_y;
            maxabs = max(abs(sensitivity));
            if maxabs > 0
                sensitivity = sensitivity / maxabs;
            end
            row = row + 1;
            S(row, :) = sensitivity;
        end
    end
    fprintf('Sensitivity matrix: %dx%d\n', size(S, 1), size(S, 2));
end
