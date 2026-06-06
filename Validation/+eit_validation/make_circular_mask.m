function mask = make_circular_mask(img_size, radius_offset_px)
%MAKE_CIRCULAR_MASK Create a circular mask for EIT images.
%
%   mask = eit_validation.make_circular_mask(img_size, radius_offset_px)
%
%   Inputs:
%     img_size         - Image dimension (img_size x img_size)
%     radius_offset_px - Offset to add/subtract from default radius (default: 0)
%
%   Output:
%     mask - Logical mask (img_size x img_size), true inside circle

    if nargin < 2
        radius_offset_px = 0;
    end

    center = (img_size - 1) / 2;
    radius = (img_size / 2) + radius_offset_px;

    mask = true(img_size);
    for iy = 0:img_size-1
        for ix = 0:img_size-1
            if sqrt((ix - center)^2 + (iy - center)^2) > radius
                mask(iy + 1, ix + 1) = false;
            end
        end
    end
end
