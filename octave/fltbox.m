%
% Octave function to prepare a figure with folding zones for sdrx
% downsampling filters. Used as a background to filter curves.
%
% Arguments:
%   fs    - Sampling frequency in kHz
%   dr    - Dynamic range in dB
%   f_cut - care band (must be < fs/2)
%   M     - Downsampling factor (must be >= 2)
%
function ret = fltbox(fs, dr, f_cut, M)
    mr = 0.5;            % max ripple in dB in the passband
    noise_floor = -120;  % noise floor for the graph relative to 0 dBFS

    f_folding = fs / M;

    % Prepare the graph
    clf;
    box on;
    hold on;
    grid on;
    axis([ 0 fs/2 noise_floor 5 ]);
    xlabel('Frequency [kHz]');
    ylabel('Filter attenuation [dB]');
    title_str = sprintf('Fs: %dkS/s, M: %d, dynamic range: %ddB, band of interest: 0 - %dkHz ', fs, M, dr, f_cut);
    title(title_str);

    % Plot care band
    plot([ 0, f_cut ],[ -mr, -mr ], "r-");
    plot([ f_cut, f_cut ],[ noise_floor, -mr ], "r-");
    %area(1:1:f_cut, -100 * ones([1 f_cut]), 'FaceColor', 'red');

    % Plot alias zones
    for zone = 0:(M-2)
        zone_folding = f_folding + (f_folding * floor(zone/2));
        if mod(zone, 2) == 0
            %sprintf('zone = %d (even) %d to %d', zone, zone_folding - f_cut, zone_folding)
            plot([ zone_folding - f_cut, zone_folding - f_cut ], [ -dr, 0 ], "r-");
            plot([ zone_folding - f_cut, zone_folding ], [ -dr, -dr ], "r-");
        else
            %sprintf('zone = %d (odd) %d to %d',  zone, zone_folding, zone_folding + f_cut)
            plot([ zone_folding, zone_folding + f_cut ], [ -dr, -dr ], "r-");
            plot([ zone_folding + f_cut, zone_folding + f_cut ], [ -dr, 0 ], "r-");
        end
    endfor

    % Care band
%    plot([ 0, f_cut ],[ -mr, -mr ], "r-");
%    plot([ f_cut, f_cut ],[ -100, -0.5 ], "r-");

    % First alias zone
%    plot([ 80-f_cut, 80-f_cut ],[ -dr, 5 ], "r-");
%    plot([ 80-f_cut, 80 ],[ -dr, -dr ], "r-");

    % Second alias zone
%    plot([ 80, 80+f_cut ],[ -dr, -dr ], "r-");
%    plot([ 80+f_cut, 80+f_cut ],[ -dr, 5 ], "r-");

    % Third alias zone
%    plot([ 160-f_cut, 160-f_cut ],[ -dr, 5 ], "r-");
%    plot([ 160-f_cut, 160 ],[ -dr, -dr ], "r-");

    % Fourth alias zone
%    plot([ 160, 160+f_cut ],[ -dr, -dr ], "r-");
%    plot([ 160+f_cut, 160+f_cut ],[ -dr, 5 ], "r-");

endfunction
