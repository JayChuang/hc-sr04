# About
My hc-sr04 driver on Raspberry Pi 3.

I use a timer to send trigger pulse(10us) every 1 second, and an interrupt handler to monitor the status of echo signal.

The default trigger pin is 27 and echo pin is 17, you can change it easily.

# Build
Firstly, you have to get linux header or souce code for rpi, I cloned one from https://github.com/raspberrypi/linux.

Make sure your kernel header version is same as your pi's kernel version, and modify KERNELDIR to your own path in Makefile.

Then type
```
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- 
```

# Usage
Install module
```
sudo insmod ./hc-sr04.ko 
```

Enable timer to send trigger signal periodically
```
echo -n 1 | sudo tee /dev/hc-sr04
```

Read node to get distance
```
sudo cat /dev/hc-sr04
```

The result shows the time difference between various echo signal. Divide it by 58 to get in cm unit.
