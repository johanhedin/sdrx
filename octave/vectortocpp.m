% Function to print a Octave vector as a C++ STL float vector

function str = vectortocpp(vec, name, precision)

if nargin < 2
    name = 'a';
end
if nargin < 3
    precision = 16;
end

str = sprintf('static const std::vector<float> %s = {\n', name);
for i = 1:length(vec)
    if vec(i) > 0
        str = [ str sprintf(' ') ];
    end
    str = [ str sprintf( ['    %1.' int2str(precision) 'ff'], vec(i)) ];
    if i ~= length(vec)
        str = [ str sprintf(',') ];
    end
    str = [ str sprintf('\n') ];
end
str = [ str sprintf('};\n') ];
