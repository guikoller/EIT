function img = reconstruct_lbp(S, U_ref, U_tgt, img_size)
%RECONSTRUCT_LBP LBP reconstruction using pre-built sensitivity matrix.
%
%   img = eit_validation.reconstruct_lbp(S, U_ref, U_tgt, img_size)
%
%   Inputs:
%     S        - Sensitivity matrix (n_measurements x n_pixels)
%     U_ref    - Reference voltage measurements (n_meas x n_inj)
%     U_tgt    - Target voltage measurements (n_meas x n_inj)
%     img_size - Image dimension (img_size x img_size)
%
%   Output:
%     img - Reconstructed image (img_size x img_size)

    [n_meas, n_inj] = size(U_ref);

    % Compute delta voltage
    delta_v = zeros(n_meas * n_inj, 1);
    idx = 1;
    for inj = 1:n_inj
        for meas = 1:n_meas
            delta_v(idx) = U_tgt(meas, inj) - U_ref(meas, inj);
            idx = idx + 1;
        end
    end

    % Backproject
    n_use = min(size(S, 1), length(delta_v));
    image_vec = S(1:n_use, :)' * delta_v(1:n_use);
    img = reshape(image_vec, [img_size, img_size]);

    % Apply standard transformations
    img = rot90(img, 1);   % rotate 90 degrees left
    img = -img;            % sign flip
end
