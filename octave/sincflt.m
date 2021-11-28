% Function to create a low pass sinc filter with a window function.
%
% fcut must be less than fs/2.
% N must be a odd number.
%
% List of window functions here: https://octave.sourceforge.io/signal/overview.html#Window_Functions

function ret = sincflt(N, fs, fcut, win, varargin)
    fcut_norm = fcut / fs;
    w1 = window(win, N, varargin{:}).';
    N2 = 0.5 * (N -1);
    h1 = sinc((-N2:N2) * 2 * fcut_norm);
    h = h1 .* w1;
    k1 = 1 / sum(h);
    h = h * k1;
    ret = h;
endfunction
