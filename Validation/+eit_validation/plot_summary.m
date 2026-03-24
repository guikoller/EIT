function plot_summary(T, out_path)
%PLOT_SUMMARY Generate summary bar chart comparing methods.
%
%   eit_validation.plot_summary(T, out_path)
%
%   Inputs:
%     T        - Results table with columns: Dataset, CC_E, SSIM_E, CC_M, SSIM_M
%     out_path - Output file path

    valid_rows = ~isnan(T.CC_M);
    vr = find(valid_rows);

    fig = figure('Name', 'Summary', 'Color', 'white', 'Visible', 'off', ...
                 'Units', 'pixels', 'Position', [50 50 1200 500]);

    if isempty(vr)
        ax = axes(fig);
        set(ax, 'Visible', 'off');
        text(0.5, 0.5, 'No valid data for summary chart.', ...
            'HorizontalAlignment', 'center', 'VerticalAlignment', 'middle', ...
            'FontSize', 14, 'FontWeight', 'bold');
        exportgraphics(fig, out_path, 'Resolution', 150);
        close(fig);
        return;
    end

    labels = T.Dataset(vr);

    % CC comparison
    subplot(1, 2, 1);
    bar_data_cc = [T.CC_E(vr), T.CC_M(vr)];
    b = bar(bar_data_cc);
    hold on;
    b(1).FaceColor = [0.2 0.4 0.8];   % blue = EIDORS
    b(2).FaceColor = [0.2 0.7 0.3];   % green = MATLAB
    set(gca, 'XTick', 1:numel(vr), 'XTickLabel', labels, ...
        'XTickLabelRotation', 60, 'FontSize', 8);
    ylabel('CC [-1,1], ideal=1');
    title('Correlation Coefficient (MCU vs each method)');
    legend('EIDORS (GN)', 'MATLAB LBP', 'Location', 'southeast');
    ylim([min(0, min(bar_data_cc(:), [], 'omitnan') - 0.05) 1.05]);
    grid on;

    % SSIM comparison
    subplot(1, 2, 2);
    bar_data_ssim = [T.SSIM_E(vr), T.SSIM_M(vr)];
    b = bar(bar_data_ssim);
    hold on;
    b(1).FaceColor = [0.2 0.4 0.8];
    b(2).FaceColor = [0.2 0.7 0.3];
    set(gca, 'XTick', 1:numel(vr), 'XTickLabel', labels, ...
        'XTickLabelRotation', 60, 'FontSize', 8);
    ylabel('SSIM [0,1], ideal=1');
    title('Structural Similarity (MCU vs each method)');
    legend('EIDORS (GN)', 'MATLAB LBP', 'Location', 'southeast');
    ylim([min(0, min(bar_data_ssim(:), [], 'omitnan') - 0.05) 1.05]);
    grid on;

    sgtitle('Batch Validation: MCU vs EIDORS & MATLAB', 'FontSize', 14, 'FontWeight', 'bold');
    exportgraphics(fig, out_path, 'Resolution', 150);
    close(fig);
end
