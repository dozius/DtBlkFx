function out = simshift(n, data)
//simshift
//  n: number of samples
//  data : [frq phase amp]
//  cycles: number of cycles (frequency)
//  phase: phase to generate data

[m t] = size(data);
frq = round(data(:, 1)*64)/64; // freq rounding matches generated shift coefficients
phase = data(:, 2);
amp = data(:, 3);

out = zeros(1, n);
for a=1:m
  // generate component
  t = cos(rng(0, 2*%pi*frq(a), n) + phase(a)) * amp(a);
  
  // filter to simulate shift coefficients
  tt = rfft(t);
  tc = round(frq(a))+1;
  //tt(1:tc-5) = zeros(1, tc-5);
  //tt(tc+5:length(tt)) = zeros(1, length(tt)-(tc+5)+1);
  
  // add to output
  out = out + rifft(tt);
end

endfunction



