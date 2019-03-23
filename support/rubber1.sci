n = 7;
ratio = .5;
yscale = 1;

d = [ [3;4;5;6;7;8;9] [3;4;5;6;7;8;20]+5 ];

clf;
plot(d);

total_len = sum(sqrt(diff(d, 1, 'r').^2 + 1), 'r');

// index into each data set
idx = [0, 0];

// current value from each data set
val = [0, 0];

// current line integral length
len = [0, 0];

// current delta
dl = [1 1];

// current delta y
dy = [0, 0];

prvx = 0;
prvy = 0;
x = 0;
y = 0;

out = zeros(1, n);

while idx(1)<=n & idx(2)<=n

	// which curve should we use
	if len(1) < len(2) then
		a = 1;
		b = 2;
		mix_ratio = ratio;
	else        
		a = 2;
		b = 1;
		mix_ratio = 1-ratio;
	end

	// interpolate point on curve "b"
    lfrac = (len(b)-len(a))/dl(b);
    prvx = x;
    prvy = y;
    x = idx(b)-lfrac;
    y = val(b)-lfrac*dy(b);

	plot([x idx(a)], [y val(a)]);
	
    // interpolated values
    ix = x*mix_ratio + idx(a)*(1-mix_ratio);
    iy = y*mix_ratio + val(a)*(1-mix_ratio);
    
	// move to the next point on curve "a"
	if idx(a) < n then
		idx(a) = idx(a) + 1;
		cur = d(idx(a), a);
		dy(a) = cur - val(a);
		dl(a) = sqrt( (dy(a)*yscale)^2 + 1 )/total_len(a);
		len(a) = len(a) + dl(a);
		val(a) = cur;
	end
end

halt;






stp0 = sum(sqrt(diff(d0).^2 + xscale))/((n-1)*8);
stp1 = sum(sqrt(diff(d1).^2 + xscale))/((n-1)*8);
out = zeros(1, n);

sum0 = 0;
targ0 = 0;
idx0 = 1;
sum1 = 0;
targ1 = 0;
idx1 = 1;

x = 0;
y = 0;
dx = 1;

for a=1:n
    while x <= a & (idx0 <= n | idx1 <= n)
		while sum0 <= targ0 & idx0 < n
			prvsum0 = sum0;
			prv0 = d0(idx0);
			idx0 = idx0+1;
			sum0 = sum0 + sqrt( (d0(idx0)-prv0)^2 + xscale );
			diff0 = sum0 - prvsum0;
		end
		r = (sum0-targ0) / diff0;
		x0 = idx0 - r;
		y0 = prv0*r + d0(idx0)*(1-r);

		while sum1 <= targ1 & idx1 < n
			prvsum1 = sum1;
			prv1 = d1(idx1);
			idx1 = idx1+1;
			sum1 = sum1 + sqrt( (d1(idx1)-prv1)^2 + xscale );
			diff1 = sum1 - prvsum1;
		end
		r = (sum1-targ1) / diff1;
		x1 = idx1 - r;
		y1 = prv1*r + d1(idx1)*(1-r);

		plot([x0 x1], [y0 y1], 'g');
		targ0 = targ0 + stp0;
		targ1 = targ1 + stp1;
        prvx = x;
        prvy = y;
		x = x0*ratio+x1*(1-ratio);
		y = y0*ratio+y1*(1-ratio);
		dx = x-prvx;
	end	
	r = (x-a)/dx;
	out(a) = prvy*r + y*(1-r);
end

plot(out, 'y');
plot(d0*ratio+d1*(1-ratio), 'g');