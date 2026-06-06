function plot_comparison(base_name, photo_path, mcu_norm, eidors_norm, matlab_norm, metrics_e, metrics_m, out_path)
%PLOT_COMPARISON Generate comparison figure for a single dataset.
%
%   eit_validation.plot_comparison(base_name, photo_path, mcu_norm, eidors_norm, matlab_norm, metrics_e, metrics_m, out_path)
%
%   Inputs:
%     base_name    - Dataset name (for title)
%     photo_path   - Path to phantom photo (empty if not available)
%     mcu_norm     - MCU image (normalized)
%     eidors_norm  - EIDORS image (normalized)
%     matlab_norm  - MATLAB image (normalized)
%     metrics_e    - Metrics struct for MCU vs EIDORS
%     metrics_m    - Metrics struct for MCU vs MATLAB
%     out_path     - Output file path

    has_photo = ~isempty(photo_path) && isfile(photo_path);
    n_img = 3 + has_photo;

    fig = figure('Name', base_name, 'Color', 'white', 'Visible', 'off', ...
                 'Units', 'pixels', 'Position', [50 50 n_img*220 500]);

    % Row 1: Images
    col = 0;
    if has_photo
        col = col + 1;
        subplot(2, n_img, col);
        imshow(imread(photo_path));
        title('Phantom', 'FontSize', 10, 'FontWeight', 'bold');
    end

    col = col + 1;
    subplot(2, n_img, col);
    imagesc(mcu_norm, 'AlphaData', ~isnan(mcu_norm));
    axis square; axis off; colorbar; colormap(jet); caxis([0 1]);
    set(gca, 'Color', [0 0 0]);
    title('MCU (LBP)', 'FontSize', 10);

    col = col + 1;
    subplot(2, n_img, col);
    imagesc(eidors_norm, 'AlphaData', ~isnan(eidors_norm));
    axis square; axis off; colorbar; caxis([0 1]);
    set(gca, 'Color', [0 0 0]);
    title(sprintf('EIDORS (CC=%.3f)', metrics_e.cc), 'FontSize', 10);

    col = col + 1;
    subplot(2, n_img, col);
    imagesc(matlab_norm, 'AlphaData', ~isnan(matlab_norm));
    axis square; axis off; colorbar; caxis([0 1]);
    set(gca, 'Color', [0 0 0]);
    title(sprintf('MATLAB (CC=%.3f)', metrics_m.cc), 'FontSize', 10);

    % Row 2: Metrics text
    ax2 = subplot(2, n_img, n_img+1 : 2*n_img);
    set(ax2, 'Visible', 'off');
    metrics_text = sprintf([ ...
        '                   MCU vs EIDORS       MCU vs MATLAB       Ideal\n' ...
        '  CC  [-1,1]       %+.4f              %+.4f              1\n' ...
        '  SSIM [0,1]       %.4f               %.4f               1\n' ...
        '  GRE  [0,inf)     %.4f               %.4f               0\n' ...
        '  RMSE [0,1]       %.4f               %.4f               0\n' ...
        '  PSNR [dB]        %.2f               %.2f               inf'], ...
        metrics_e.cc, metrics_m.cc, ...
        metrics_e.ssim, metrics_m.ssim, ...
        metrics_e.gre, metrics_m.gre, ...
        metrics_e.rmse, metrics_m.rmse, ...
        metrics_e.psnr, metrics_m.psnr);
    text(0.5, 0.5, metrics_text, ...
        'HorizontalAlignment', 'center', 'VerticalAlignment', 'middle', ...
        'FontSize', 10, 'FontName', 'FixedWidth');

    sgtitle(strrep(base_name, '_', '\_'), 'FontSize', 12, 'FontWeight', 'bold');

    exportgraphics(fig, out_path, 'Resolution', 150);
    close(fig);
end
