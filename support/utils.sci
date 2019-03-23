//--------------------------------------------------------------------------------------
function out = rng(start_val, end_val, num)
out = start_val + (end_val-start_val)*(0:num-1)/num;
endfunction

//--------------------------------------------------------------------------------------
function out = rfft(x)
[m n] = size(x);
n2 = 2^nextpow2(n);
pad_n = n2-n;
out = zeros(m, n2/2+1);
for a=1:m
  t = fft([x(a, :) zeros(1,pad_n)]);
  out(a, :) = t(1:(n2/2+1))/n2*2;
end
endfunction

//--------------------------------------------------------------------------------------
function out = rifft(x)
[m n] = size(x);
for a=1:m
  out(a,:) = real(ifft([x(a, :) conj(x(a, n-1:-1:2))]))*(n-1);
end
endfunction

//--------------------------------------------------------------------------------------
function out = angle(x)
out = atan(imag(x), real(x));
endfunction

//--------------------------------------------------------------------------------------
function out = lim_rng(x, min_x, max_x)
out = x;
i1 = find(x < min_x);
out(i1) = ones(1, length(i1))*min_x;
i1 = find(x > max_x);
out(i1) = ones(1, length(i1))*max_x;
endfunction

//--------------------------------------------------------------------------------------
function plotcplx(x, t)
    [lhs, rhs] = argn();
    if rhs == 2
        plot(real(x), imag(x), t);
    else
        plot(real(x), imag(x));
    end
endfunction

//--------------------------------------------------------------------------------------
function out = pkt_prob(n_bits, allowed_errs, berr)
t = binomial(1-berr, n_bits);
out = sum(t(length(t)-allowed_errs:length(t)));
endfunction

//--------------------------------------------------------------------------------------
function out = rev(x)
out = x(length(x):-1:1);
endfunction

//--------------------------------------------------------------------------------------
function out = firfilt(fir_coeff, x)
//
// FIR filter using FFTs for convolution
// note that x is padded with length(fir_coeff)-1 zeros at the start
//
// todo, could do really short filters faster by direct means

//
fir_coeff = fir_coeff(:).';
x = x(:).';
out = zeros(1, length(x)+length(fir_coeff)-1);

// number of coefficients
//n_coeff = max(length(fir_coeff), length(iir_coeff));
n_coeff = length(fir_coeff);

// fft length
t = max(min(max(n_coeff*4, 1024), length(out)), n_coeff);
fft_len = 2^nextpow2(t);
//printf("fft_len=%d\n", fft_len);

// number of samples output each iteration
stp = fft_len-n_coeff+1;

// 
ff = fft([fir_coeff zeros(1, fft_len-length(fir_coeff))]);
//ff = ff ./ fft([iir_coeff zeros(1, fft_len-length(iir_coeff))]);

// output index
idx = 0;

// src offset
offs = -length(fir_coeff)+1;
while 1
    src_idx = idx+offs;
    pad_start = max(-src_idx, 0);
    pad_end = max(src_idx+fft_len-length(x), 0);
    x_start = src_idx+pad_start+1;
    x_end = src_idx+fft_len-pad_end;
    if x_start > x_end ; break ; end;
    t = [zeros(1, pad_start) x(x_start:x_end) zeros(1, pad_end)];
    xx = fft(t);
    //printf("idx=%d, src_idx=%d, pad_start=%d, pad_end=%d, x_start=%d, x_end=%d, n=%d, out_end=%d\n", idx, src_idx, pad_start, pad_end, x_start, x_end, length(t), out_end);
    //printf("length(t)=%d, length(xx)=%d length(ff)=%d\n", length(t), length(xx), length(ff));
    t = ifft(xx .* ff);
    
    out_end = min(idx+stp, length(out));
    out(idx+1:out_end) = t(n_coeff:n_coeff+out_end-idx-1);

      idx = idx+stp;
    
    if idx >= length(out) ; break ; end
end


endfunction

//--------------------------------------------------------------------------------------
function out = genexp(cycles, len)
out = exp(%i*2*%pi*rng(0, cycles, len));
endfunction


//--------------------------------------------------------------------------------------
function out = to_cplx(x)
// use hilbert transform to turn "x" into complex
    t = firfilt(hilb(1023), x);
    out = x + %i*t(511+(1:length(x)));
endfunction

//--------------------------------------------------------------------------------------
function plot_fft_peak(x, bins)
    xx = rfft(x);
    [a b] = max(abs(xx));
    idx1 = max(b-bins, 1);
    idx2 = min(b+bins, length(xx));
    
    plot(idx1:idx2, real(xx(idx1:idx2)));
    plot(idx1:idx2, imag(xx(idx1:idx2)), 'r');

    t1 = abs(xx(idx1:idx2));
    
    // do fractional resolution also
    t3 = [];
    t3(1:2:length(t1)*2-1) = t1;
    t3(2:2:length(t1)*2-2) = abs(diff(xx(idx1:idx2)))*.78;
    
    plot(idx1:.5:idx2, t3(:), 'm');
    
    plot(idx1:idx2, t1, 'g');

    title(sprintf("peak: idx=%d, (blue) real=%f, (red) imag=%f, (green) abs=%f", b, real(xx(b)), imag(xx(b)), abs(xx(b))));
endfunction

//--------------------------------------------------------------------------------------
function out = randn(m, n)
out = rand(m, n, 'normal');
endfunction;

//--------------------------------------------------------------------------------------
function out = randn_cplx(m, n)
out = matrix(rand(m*n, 2, 'normal')*[1;%i], m, n);
endfunction;

//--------------------------------------------------------------------------------------
function out = std(x)
out = stdev(x);
endfunction;

//--------------------------------------------------------------------------------------
function out = get_pdf(x, bins)
// return normalized histogram of data in "x" with bins centred according
// to vector "bins" (except for first & last which extend from -inf & inf respectively)
// x: data to find histrogram of
// bins: vector of bin centres

t = (bins(1:$-1)+bins(2:$))/2;
out = zeros(1, length(bins));
out(1) = length(find(x <= t(1)));
out(length(bins)) = length(find(x > t($)));
for a = 1:length(t)-1
    out(a+1) = length(find(x > t(a) & x <= t(a+1)));   
end

out = out/sum(out);
endfunction;

//--------------------------------------------------------------------------------------
function pdf = rician_pdf(x, lambda)
// return pdf(x) of sqrt((X+lambda)^2 + X), where X has gaussian PDF
// to get properly scaled pdf:
//    pdf = rician_pdf(linspace(0, <limit>, <len>), <lambda>)*<limit>/<len>
//
pdf = exp(-(x.^2 + lambda.^2)/2) .* x .* besseli(0, lambda.* x);
endfunction

//--------------------------------------------------------------------------------------
function pdf = gaussian_pdf(x)
// to get properly scaled pdf:
//    pdf = gaussian_pdf(linspace(0, <limit>, <len>))*<limit>/<len>
//
pdf = 1/sqrt(2*%pi)*exp(-(x.^2)/2);
endfunction


//--------------------------------------------------------------------------------------
function [pdf, lim]=add_pdf(out_len, pdf_a, lim_a, pdf_b, lim_b)
//
// work out pdf of pdf_a+pdf_b given 2 input pdf's & limits
// assume sum(pdf_a)==1 & sum(pdf_b)==1 (or they are meant to since they are normalized
// anyway)
//
// could extend this to take variable arguments instead of just 2
//
// input:
//    out_len  : length of pdf to return
//    lim_a : vector containing [ min_a max_a ]  (only first & last elements are used)
//    pdf_a : probablity distribution
//    lim_b : same as for lim_a 
//    pdf_b : same as for pdf_a
//
// output:
//    pdf : resulting PDF
//    lim : 2 element vector containing limits
//    
    // only use the first & last elements
    lim_a = lim_a([1 $]);
    lim_b = lim_b([1 $]);

    // find new limits
    lim = lim_a + lim_b;

    // determine expected std & mean
    std_exp = sqrt(sum([std_pdf(pdf_a, lim_a) std_pdf(pdf_b, lim_b)].^2));
    mean_exp = mean_pdf(pdf_a, lim_a)+mean_pdf(pdf_b, lim_b);

    // rescale pdf's to be the same resolution
    pdf_a = rescale_pdf(out_len*diff(lim_a)/diff(lim), lim_a, pdf_a, lim_a);
    pdf_b = rescale_pdf(out_len*diff(lim_b)/diff(lim), lim_b, pdf_b, lim_b);
    
    // zero-pad for FFT (and make pwr of 2)
    total_n = 2^nextpow2(length(pdf_a)+length(pdf_b));
    a = [ pdf_a zeros(1, total_n-length(pdf_a))];
    b = [ pdf_b zeros(1, total_n-length(pdf_b))];

    // do the convolve
    pdf = real(ifft(fft(a).*fft(b)));
    pdf = pdf(1:length(pdf_a)+length(pdf_b)-1);
    
    //
    if length(pdf) ~= out_len
        pdf = interp1(1:length(pdf), pdf, linspace(1, length(pdf), out_len));
    end
    
    // normalize
    pdf = pdf/sum(pdf);
    
    // correct any error with mean & std
    std_out = std_pdf(pdf, lim);
    mean_out = mean_pdf(pdf, lim);
    lim = (lim-mean_out)*std_exp/std_out + mean_exp;
endfunction

//--------------------------------------------------------------------------------------
function out = mean_pdf(pdf, lim)
// find mean (first central moment) of "pdf" with limits "lim"
//
out = sum(linspace(lim(1), lim($), length(pdf)).*pdf)/sum(pdf);
endfunction

//--------------------------------------------------------------------------------------
function out = std_pdf(pdf, lim)
// find standard deviation (sqrt of second moment) of "pdf" with limits "lim"
//
m = mean_pdf(pdf, lim);
pdf = pdf / sum(pdf);
out = sqrt(sum(pdf.*(linspace(lim(1), lim($), length(pdf))-m).^2));
endfunction

//--------------------------------------------------------------------------------------
function pdf = rescale_pdf(out_len, out_lim, pdf, lim)
// rescale "pdf" with "lim" to out_len
//
// 
    // work out limits for output length
    t = (out_lim([1 $])-lim(1))/(lim($)-lim(1))*(length(pdf)-1);
    
    // adjust t2 ending position for non-integer length
    t(2) = t(2)*out_len/ceil(out_len);
    
    pdf = interp1(-1:length(pdf), [0 pdf 0], linspace(t(1), t(2), ceil(out_len)), 'linear', 0);

    // todo: correct mean & stddev

    pdf = pdf/sum(pdf);
endfunction

//--------------------------------------------------------------------------------------
function [pdf, lim] = sum_pdf_n(out_len, n, pdf_in, lim_in)
// sum "pdf" "n" times to itself
// work out for final pdf for X1 + X2 + X3 ..., where pdf(X) is given by "pdf" and
// all variables are independent
// careful that out_len is sufficiently large so as not to lose too much resolution
//
//

    // only use first & last elements (assume linear)
    lim_in = lim_in([1 $]);
    
    // work out final limits
    lim = lim_in*n;
    
    // find expected mean & std
    std_exp = std_pdf(pdf_in, lim_in)*sqrt(n);
    mean_exp = mean_pdf(pdf_in, lim_in)*n;
    
    // rescale in pdf to match output scale
    pdf_in = rescale_pdf(out_len/n, lim_in, pdf_in, lim_in);
        
    // find output length
    total_n = 2^nextpow2(length(pdf_in)*n);

    // do convolve
    pdf = real(ifft(fft([pdf_in zeros(1, total_n-length(pdf_in))]).^n));
    pdf = pdf(1:length(pdf_in)*n-n+1);

    // this happens unless out_len+1 is multiple of n
    if length(pdf) ~= out_len
        pdf = interp1(1:length(pdf), pdf, linspace(1, length(pdf), out_len));
    end

    // ensure output is normalized to 1
    pdf = pdf/sum(pdf);
    
    // match limits to expected std deviation & mean
    std_out = std_pdf(pdf, lim);
    mean_out = mean_pdf(pdf, lim);
    lim = (lim-mean_out)*std_exp/std_out + mean_exp;
endfunction

//--------------------------------------------------------------------------------------
function p = prob_a_gt_b_pdf(pdf_a, lim_a, pdf_b, lim_b)
// return P(X[pdf_a] > X[pdf_b])
//
    // only use the first & last elements of limits (assume linear scale)
    lim_a = lim_a([1 $]);
    lim_b = lim_b([1 $]);
    //printf("lim a:%d..%d, b:%d..%d\n", lim_a, lim_b);
    
    // samples per unit
    spu_a = (length(pdf_a)-1)/diff(lim_a);
    spu_b = (length(pdf_b)-1)/diff(lim_b);
    
    // rescale units so that both match
    spu = spu_a;
    if spu_a > spu_b
        pdf_b = rescale_pdf(spu_a*diff(lim_b)+1, lim_b, pdf_b, lim_b);
    elseif spu_b > spu_a
        pdf_a = rescale_pdf(spu_b*diff(lim_a)+1, lim_a, pdf_a, lim_a);
        spu = spu_b;
    end
    pdf_a = pdf_a/sum(pdf_a);
    pdf_b = pdf_b/sum(pdf_b);
        
    // make limits relative to "a" in samples(starting at 0)
    lim_b = [0 length(pdf_b)-1]+round((lim_b(1)-lim_a(1))*spu);
    lim_a = [0 length(pdf_a)-1];

    //plot(lim_a(1):lim_a(2), pdf_a, "b:");
    //plot(lim_b(1):lim_b(2), pdf_b, "r:");
    //printf("lim a:%d..%d, b:%d..%d, spu=%f\n", lim_a, lim_b, spu);
    
    // probability to return
    p = 0;

    // sum of b completely prior to a (t starts at 1)
    t = min(-lim_b(1), length(pdf_b));
    if t >= 1
        p = p + sum(pdf_b(1:t));
        //plot((0:t-1)+lim_b(1), pdf_b(1:t), 'g');
        //printf("a>b completely: %d..%d\n", 1, t);
    end
    
    // find indices of overlap (starting at 1)
    t = [max(lim_a(1),lim_b(1)) min(lim_a(2),lim_b(2))]+1;

    // find probability in the overlap
    if t(2) >= t(1)

        // cumulative probability of "a" starting from the right hand side
        cdf_a = cumsum(pdf_a($:-1:1));
        cdf_a = cdf_a($:-1:1);
        
        // find probability
        tb = t-lim_b(1);
        p = p + sum(pdf_b(tb(1):tb(2)) .* cdf_a(t(1):t(2)))

        //printf("a:%d..%d, b:%d..%d\n", t, tb);
        //plot((t(1):t(2))-1, pdf_a(t(1):t(2)), 'b');
        //plot((tb(1):tb(2))+lim_b(1)-1, pdf_b(tb(1):tb(2)), 'r');
    end    
endfunction


//--------------------------------------------------------------------------------------
function plot_pdf(pdf, lim, args)
// plot normalized pdf
//
    [lhs, rhs] = argn();
    if rhs < 3 ; args = ""; end
    pdf = pdf/sum(pdf);
    plot(linspace(lim(1), lim($), length(pdf)), pdf*length(pdf)/(lim($)-lim(1)), args);
endfunction

