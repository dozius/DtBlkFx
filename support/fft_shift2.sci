// check what fft of various non-integer freqs look like
clf

per = rng(100, 500, 20);
ph = zeros(length(per), length(t));
mag = ph;
fxx = ph;

for a = 1:length(per)
    x = cos(rng(0, 2*%pi*(per(a)-.8), 1024));
    xx = rfft(x);

    t = per(a)+1-5:per(a)+1+5;
    fxx(a,:) = xx(t);
    ph(a,:) = angle(xx(t));
    mag(a,:) = abs(xx(t));
    subplot(211);
    plot(angle(xx(t)));
    subplot(212);
    plot(abs(xx));
end

subplot(211);
surf(mag);
subplot(212);
surf(ph);

