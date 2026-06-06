function plot_summary(T, out_path)
%PLOT_SUMMARY Generate per-group boxplots for MCU vs. EIDORS/GN comparison.
%
%   eit_validation.plot_summary(T, out_path)
%
%   Inputs:
%     T        - Results table with columns: Dataset, CC_E, SSIM_E, ...
%     out_path - Output file path

    valid_rows = ~isnan(T.CC_E);
    T_v = T(valid_rows, :);

    fig = figure('Name', 'Summary', 'Color', 'white', 'Visible', 'off', ...
                 'Units', 'pixels', 'Position', [50 50 960 440]);

    if isempty(T_v)
        ax = axes(fig);
        set(ax, 'Visible', 'off');
        text(0.5, 0.5, 'No valid data for summary chart.', ...
            'HorizontalAlignment', 'center', 'VerticalAlignment', 'middle', ...
            'FontSize', 14, 'FontWeight', 'bold', 'Units', 'normalized');
        exportgraphics(fig, out_path, 'Resolution', 150);
        close(fig);
        return;
    end

    % Extract group numbers from dataset names 'datamat_X_Y'
    n_ds = height(T_v);
    grp = zeros(n_ds, 1);
    for k = 1:n_ds
        tok = regexp(char(T_v.Dataset(k)), 'datamat_(\d+)_', 'tokens');
        if ~isempty(tok)
            grp(k) = str2double(tok{1}{1});
        end
    end

    n_grp = 8;
    g_labels = arrayfun(@(g) sprintf('D%d', g), 1:n_grp, 'UniformOutput', false);

    cc_by_grp   = cell(1, n_grp);
    ssim_by_grp = cell(1, n_grp);
    for g = 1:n_grp
        idx = grp == g;
        cc_by_grp{g}   = T_v.CC_E(idx);
        ssim_by_grp{g} = T_v.SSIM_E(idx);
    end

    col_face = [0.55 0.75 0.98];   % box fill – steel blue
    col_edge = [0.10 0.28 0.62];   % box border – dark navy
    col_med  = [0.90 0.35 0.10];   % median line – warm orange
    col_wsk  = [0.30 0.30 0.30];   % whisker / outlier

    % ─── Subplot 1: CC ────────────────────────────────────────────────
    ax1 = subplot(1, 2, 1);
    hold(ax1, 'on');

    patch('Parent', ax1, ...
          'XData', [0.5 5.5 5.5 0.5], 'YData', [-1.22 -1.22 1.22 1.22], ...
          'FaceColor', [0.82 0.95 0.82], 'EdgeColor', 'none', 'FaceAlpha', 0.50);
    patch('Parent', ax1, ...
          'XData', [5.5 8.5 8.5 5.5], 'YData', [-1.22 -1.22 1.22 1.22], ...
          'FaceColor', [0.98 0.82 0.82], 'EdgeColor', 'none', 'FaceAlpha', 0.50);
    text(ax1, 0.3235, 1.04, 'Scenarios 1–5', 'Units', 'normalized', 'HorizontalAlignment', 'center', ...
         'FontSize', 8.5, 'FontWeight', 'bold', 'Color', [0 0.35 0], 'Clipping', 'off');
    text(ax1, 0.7941, 1.04, 'Scenarios 6–8', 'Units', 'normalized', 'HorizontalAlignment', 'center', ...
         'FontSize', 8.5, 'FontWeight', 'bold', 'Color', [0.55 0 0], 'Clipping', 'off');

    for g = 1:n_grp
        d = cc_by_grp{g};
        if isempty(d), continue; end
        draw_box(ax1, g, d, col_face, col_edge, col_med, col_wsk);
    end

    yline(ax1, 0, '--', 'Color', [0.35 0.35 0.35], 'LineWidth', 1.2, ...
          'Label', 'CC = 0', 'LabelHorizontalAlignment', 'left', ...
          'FontSize', 7.5, 'Color', [0.35 0.35 0.35]);
    yline(ax1, 1, ':', 'Color', [0.60 0.60 0.60], 'LineWidth', 0.9);
    set(ax1, 'XTick', 1:n_grp, 'XTickLabel', g_labels, 'FontSize', 9.5, ...
        'XLim', [0.25 n_grp+0.75], 'YLim', [-1 1], ...
        'GridColor', [0.82 0.82 0.82], 'GridAlpha', 0.9);
    ylabel(ax1, 'CC  [−1; 1]', 'FontSize', 10);
    title(ax1, 'Correlation Coefficient (MCU vs. EIDORS/GN)', 'FontSize', 10, 'Visible', 'off');
    grid(ax1, 'on');
    box(ax1, 'on');

    % ─── Subplot 2: SSIM ──────────────────────────────────────────────
    ax2 = subplot(1, 2, 2);
    hold(ax2, 'on');

    patch('Parent', ax2, ...
          'XData', [0.5 5.5 5.5 0.5], 'YData', [-0.06 -0.06 1.22 1.22], ...
          'FaceColor', [0.82 0.95 0.82], 'EdgeColor', 'none', 'FaceAlpha', 0.50);
    patch('Parent', ax2, ...
          'XData', [5.5 8.5 8.5 5.5], 'YData', [-0.06 -0.06 1.22 1.22], ...
          'FaceColor', [0.98 0.82 0.82], 'EdgeColor', 'none', 'FaceAlpha', 0.50);
    text(ax2, 0.3235, 1.04, 'Scenarios 1–5', 'Units', 'normalized', 'HorizontalAlignment', 'center', ...
         'FontSize', 8.5, 'FontWeight', 'bold', 'Color', [0 0.35 0], 'Clipping', 'off');
    text(ax2, 0.7941, 1.04, 'Scenarios 6–8', 'Units', 'normalized', 'HorizontalAlignment', 'center', ...
         'FontSize', 8.5, 'FontWeight', 'bold', 'Color', [0.55 0 0], 'Clipping', 'off');

    for g = 1:n_grp
        d = ssim_by_grp{g};
        if isempty(d), continue; end
        draw_box(ax2, g, d, col_face, col_edge, col_med, col_wsk);
    end

    yline(ax2, 1, ':', 'Color', [0.60 0.60 0.60], 'LineWidth', 0.9);
    set(ax2, 'XTick', 1:n_grp, 'XTickLabel', g_labels, 'FontSize', 9.5, ...
        'XLim', [0.25 n_grp+0.75], 'YLim', [0 1], ...
        'GridColor', [0.82 0.82 0.82], 'GridAlpha', 0.9);
    ylabel(ax2, 'SSIM  [0; 1]', 'FontSize', 10);
    title(ax2, 'Structural Similarity (MCU vs. EIDORS/GN)', 'FontSize', 10, 'Visible', 'off');
    grid(ax2, 'on');
    box(ax2, 'on');



    exportgraphics(fig, out_path, 'Resolution', 150);
    close(fig);
end

% ─────────────────────────────────────────────────────────────────────────
function draw_box(ax, x_pos, d, col_face, col_edge, col_med, col_wsk)
%DRAW_BOX Draw a single Tukey boxplot at position x_pos on axes ax.
    bw = 0.30;
    q  = local_pctile(d, [25 50 75]);
    q1 = q(1);  med = q(2);  q3 = q(3);
    iqr_v = q3 - q1;

    lo_fence = q1 - 1.5 * iqr_v;
    hi_fence = q3 + 1.5 * iqr_v;
    inliers  = d(d >= lo_fence & d <= hi_fence);
    if isempty(inliers)
        wlo = q1;  whi = q3;
    else
        wlo = min(inliers);  whi = max(inliers);
    end
    outliers = d(d < wlo | d > whi);

    box_h = max(q3 - q1, 1e-6);
    rectangle('Parent', ax, 'Position', [x_pos - bw, q1, 2*bw, box_h], ...
              'FaceColor', col_face, 'EdgeColor', col_edge, 'LineWidth', 1.2);
    line(ax, [x_pos-bw, x_pos+bw], [med, med], ...
         'Color', col_med, 'LineWidth', 2.5);
    line(ax, [x_pos, x_pos], [q3, whi], 'Color', col_wsk, 'LineWidth', 1.0);
    line(ax, [x_pos, x_pos], [q1, wlo], 'Color', col_wsk, 'LineWidth', 1.0);
    line(ax, [x_pos-bw/2, x_pos+bw/2], [whi, whi], 'Color', col_wsk, 'LineWidth', 1.0);
    line(ax, [x_pos-bw/2, x_pos+bw/2], [wlo, wlo], 'Color', col_wsk, 'LineWidth', 1.0);
    if ~isempty(outliers)
        plot(ax, x_pos * ones(numel(outliers), 1), outliers(:), 'o', ...
             'MarkerEdgeColor', col_edge, 'MarkerFaceColor', [1 1 1], 'MarkerSize', 5, ...
             'LineWidth', 0.9);
    end
end

% ─────────────────────────────────────────────────────────────────────────
function q = local_pctile(v, pcts)
%LOCAL_PCTILE Compute percentiles without Statistics Toolbox.
    x = sort(v(~isnan(v)));
    n = numel(x);
    if n == 0
        q = nan(size(pcts));
        return;
    end
    if n == 1
        q = x(ones(size(pcts)));
        return;
    end
    p_nodes = linspace(0, 100, n);
    q = interp1(p_nodes, x, pcts, 'linear', 'extrap');
end
