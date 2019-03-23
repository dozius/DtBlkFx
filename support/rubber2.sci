n = 100;
ratio = .5;
yscale = 100;

d = [ sin(linspace(0, 4*%pi, n))' sin(linspace(0, 2*%pi, n))' ];
//d = [ [3;4;5;6;7;8;9] [3;4;5;6;7;8;100] ];

clf;
plot(d);

len_mul = (1) ./ sum(log((yscale*diff(d, 1, 'r')).^2 + 1), 'r');

// index into each data set
idx = [1, 1];

// current value from each data set
val = d(1, :);

// current line integral length
len = [0, 0];

// current multiplier
mul = [0 0];

// current delta y
dy = [0, 0];

prvx = 0;
prvy = 0;
x = 0;
y = 0;

ix = 0;
iy = 0;
pix = 0;
piy = 0;
out = zeros(1, n);

col = ['b', 'r'];
for c=1:n
	while ix < c
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
		lfrac = (len(b)-len(a))*mul(b);
		x = idx(b)-lfrac;
		y = val(b)-lfrac*dy(b);
	    
		//plot([x idx(a)], [y val(a)], col(a));
		
		// interpolated values
		piy = iy;
		pix = ix;
		ix = x*mix_ratio + idx(a)*(1-mix_ratio);
		iy = y*mix_ratio + val(a)*(1-mix_ratio);
		//plot(ix, iy, 'x');
		
		// stop at the first to reach the end
		if idx(a) >= n then
			break;
		end
		
		// move to the next point on curve "a"	
		idx(a) = idx(a) + 1;
		cur = d(idx(a), a);
		dy(a) = cur - val(a);
		dl = log( ((cur-val(a))*yscale)^2 + 1 )*len_mul(a);
		len(a) = len(a) + dl;
		val(a) = cur;
		mul(a) = 1/dl;
	end
	
	if ix ~= pix then
		lfrac = (ix - c) / (ix - pix);
	else
		lfrac = 0;
	end
	
	out(c) = iy * (1-lfrac) + piy * lfrac;
	
	//plot(c, out(c), 'gx');
end
plot(d * [1-ratio;ratio], 'y');
plot(out, 'g');
