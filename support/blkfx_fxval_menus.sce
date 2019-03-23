// work out some values for the menus

// this is for the constant shifts
v = linspace(0.5, 1, 1e4);
lin_ = 2048/32768;
exp_raise = 8;
exp_scale = (1-lin_)/(2^exp_raise);
v2 = (v-0.5)*2;
r = (2^(v2*exp_raise)-1)*exp_scale + v2*lin_;
r = r*22050;
sh = interp1(r, v, [0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 500, 1000]);
sh = [1-sh($:-1:2) sh];

printf("Const shift values\n");
for i=1:length(sh)
  printf("%ff, ", sh(i));
end
printf("\n");

// this is for the resize

printf("\nresize values\n");
// r = lerp(f, -2, 2)^3 = (f*4-2)^3
// (r^(1/3)+2)/4 = f
r = [.1 .25 .5 1 2 4 7.9999];
v = (r.^(1/3)+2)/4;
v = [1-v($:-1:1) v]; // negative is symmetric
v(1) = 0; // force this one to 0
v = [v/2 0.5+v/2];  // now we want 2 of them for the time reverse
//v = v/2; // split range into 2
//v = [1-v($:-1:1) v 2-v($:-1:1) 1+v];
for i=1:length(v)
  printf("%ff, ", v(i));
end
printf("\n");

