// try doing a shift of a non-integer freq to see whether it works ok
frac = 0.5;
per = 256;
w = 10;
ww = 1-w:1+w;

// generate reference for fraction
p = 256;
x = cos(rng(0, 2*%pi*(p+frac), 1024));
xx = rfft(x);
fxx = xx(p+ww);

// we're going to shift this (original)
p = 40;
n = 256;
y = cos(rng(0, 2*%pi*(p+frac), n)+.3);
yy = rfft(y);
ph = exp(%i*.45);

// generate perfect target as a reference to compare against
y2 = cos(rng(0, 2*%pi*(p+frac*2), n)+.3+.45);
yy2 = rfft(y2);

// synthesize shifted version
ww2 = p-20:p+20;
yy3 = zeros(1,n/2+1);
for p = ww2
 yy3(p+ww-1) = yy3(p+ww-1) + yy(p)*fxx*ph;
end
// time domain
y3 = rifft(yy3);

// compare synthesized output to generated reference version
clf;
subplot(221);
plot(abs(yy2(ww2))); // reference
plot(abs(yy3(ww2)),'r'); // synthesized

subplot(222);
plot(y3-y2);

subplot(212);
plot(y2);
plot(y3,'r');

