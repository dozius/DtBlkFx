j = sqrt(-1);
per1 = 73.6;
shift = 9;
fft_len = 1024;

bin1 = fft_len/per1;
per2 = fft_len/(bin1+shift);

t = 0:2047;
x1 = sin( t/per1*2*pi+1.5 );


x2 = sin( t/per2*2*pi+1.5 );
plot(t, x2, 'r-');
hold on;

tshift = 155;
t2 = (0:1023)+tshift;
xx1 = afft(x1(1+t2));
xx3 = [ zeros(shift, 1); xx1 ];
xx3 = xx3(1:513);
xx3 = xx3*exp(j * 2*pi*tshift*shift/fft_len);
x3 = aifft(xx3);

plot(t2, x3, 'g-');
hold off;
