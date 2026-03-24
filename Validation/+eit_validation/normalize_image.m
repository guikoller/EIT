function img_norm = normalize_image(img)
%NORMALIZE_IMAGE Normalize image to [0,1] range, ignoring NaN values.
%
%   img_norm = eit_validation.normalize_image(img)
%
%   Input:
%     img - Input image (may contain NaN values)
%
%   Output:
%     img_norm - Normalized image in [0,1] range (NaN preserved)

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
