function metrics = compute_metrics(ref_norm, tgt_norm)
%COMPUTE_METRICS Compute all validation metrics between two images.
%
%   metrics = eit_validation.compute_metrics(ref_norm, tgt_norm)
%
%   Inputs:
%     ref_norm - Reference image (normalized to [0,1])
%     tgt_norm - Target image (normalized to [0,1])
%
%   Output:
%     metrics - Struct with fields: cc, ssim, gre, rmse, psnr
%
%   Metrics:
%     CC   - Correlation Coefficient [-1,1], ideal: 1
%     SSIM - Structural Similarity [0,1], ideal: 1
%     GRE  - Gradient Relative Error [0,inf), ideal: 0
%     RMSE - Root Mean Square Error [0,1], ideal: 0
%     PSNR - Peak Signal-to-Noise Ratio [dB], ideal: inf

    valid_mask = ~isnan(ref_norm) & ~isnan(tgt_norm);
    a = ref_norm(valid_mask);
    b = tgt_norm(valid_mask);

    if isempty(a) || numel(a) < 2
        metrics.cc = NaN;
        metrics.ssim = NaN;
        metrics.gre = NaN;
        metrics.rmse = NaN;
        metrics.psnr = NaN;
        return;
    end

    % Correlation Coefficient
    metrics.cc = corr(a(:), b(:));

    % SSIM
    metrics.ssim = compute_ssim(ref_norm, tgt_norm);

    % Gradient Relative Error (GRE)
    [gx_a, gy_a] = gradient(ref_norm);
    [gx_b, gy_b] = gradient(tgt_norm);
    grad_diff = sqrt((gx_a - gx_b).^2 + (gy_a - gy_b).^2);
    grad_ref = sqrt(gx_a.^2 + gy_a.^2);
    valid_grad = valid_mask & (grad_ref > eps);
    if any(valid_grad(:))
        metrics.gre = mean(grad_diff(valid_grad) ./ grad_ref(valid_grad));
    else
        metrics.gre = NaN;
    end

    % RMSE
    metrics.rmse = sqrt(mean((a(:) - b(:)).^2));

    % PSNR
    if metrics.rmse <= 0
        metrics.psnr = Inf;
    else
        metrics.psnr = 20 * log10(1 / metrics.rmse);
    end
end

function s = compute_ssim(img_a, img_b)
    % Handle NaN by replacing with 0 (masked regions)
    img_a_clean = img_a;
    img_b_clean = img_b;
    img_a_clean(isnan(img_a)) = 0;
    img_b_clean(isnan(img_b)) = 0;

    % Use built-in ssim if available
    if exist('ssim', 'file') == 2
        s = ssim(img_a_clean, img_b_clean);
        return;
    end

    % Fallback: global SSIM
    a = double(img_a_clean(:));
    b = double(img_b_clean(:));

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
    s = ((2*mu_a*mu_b + C1) * (2*cov_ab + C2)) / ...
        ((mu_a^2 + mu_b^2 + C1) * (var_a + var_b + C2));
end
