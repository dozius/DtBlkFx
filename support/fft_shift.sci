// find the freq-domain convolution shape for non-integer frequencies
// and check how well it works for synthesizing

frac = .5;
per = 256;
w = 4;
t = per+1-w:per+1+w;

ph_offs = rng(0, 2*%pi, 10);
ph = zeros(length(ph_offs), length(t));
mag = ph;
fxx = ph;
err = zeros(length(ph_offs), 1024);
xx2 = zeros(1, 513);
for a = 1:length(ph_offs)
    x = cos(rng(0, 2*%pi*(per+frac), 1024)+ph_offs(a));
    xx = rfft(x);
    fxx(a,:) = xx(t);
    ph(a,:) = angle(xx(t));
    mag(a,:) = abs(xx(t));
    xx2(t) = xx(t);
    err(a,:) = x-rifft(xx2);
end

clf
subplot(211);
surf(mag);
subplot(212);
surf(ph);

clf
xx2 = zeros(1, 257);
ph_offs = 1.5;
per = 4;
x = cos(rng(0, 2*%pi*(per+frac), (length(xx2)-1)*2)+ph_offs);
xx2(per+1-w:per+1+w) = fxx(1,:).*exp(%i * ph_offs);
x2 = rifft(xx2);
err = abs(x-x2);
subplot(211);
plot(err);
subplot(212);
plot(x);
plot(x2,'r');


