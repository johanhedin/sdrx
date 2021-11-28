%
% Octave function to prepare a figure with attenuation zones for sdrx
% channelization filters. Used as a background to filter curves.
%
% Arguments:
%   fs     - Sampling frequency in kHz
%   f_pass - High frequency of the pass band (must be < fs/2)
%   f_rej  - Low frequency of the reject band (must be > f_pass and < fs/2)
%   rej    - Attenuation in reject band in dB
%
function ret = fltboxch(fs, f_pass, f_rej, rej)
    mr = 1;             % max ripple in dB in the passband
    noise_floor = -120;  % noise floor for the graph relative to 0 dBFS

    % Prepare the graph
    clf;
    box on;
    hold on;
    grid on;
    axis([ 0 fs/2 noise_floor 5 ]);
    xlabel('Frequency [kHz]');
    ylabel('Filter attenuation [dB]');
    title_str = sprintf('Fs: %dkS/s, pass band: 0 - %dkHz, stop band: %dkHz - %dkHz, rejection: %ddB ',
                        fs, f_pass, f_rej, fs/2, rej);
    title(title_str);

    % Plot pass band
    plot([ 0, f_pass ],[ -mr, -mr ], "r-");
    plot([ f_pass, f_pass ],[ noise_floor, -mr ], "r-");

    % Plot reject band
    plot([ f_rej, f_rej ],[ -rej, 0 ], "r-");
    plot([ f_rej, fs/2 ],[ -rej, -rej ], "r-");
endfunction
