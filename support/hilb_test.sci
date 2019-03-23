t = cos(2*%pi*linspace(0, 5.7, 1024));
lh = 555;
th = firfilt(hilb(lh), t);

x = t-%i*th(.5+lh/2+(1:length(t)));

clf;
subplot(211);
plot(real(x));
plot(imag(x),'r');

subplot(212);
plot(t);
tt = rfft(t)*%i;
plot(real(rifft(tt)),'r');

