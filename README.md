## capsense: capacitive sensor data processing library

Current algorithm being tested is Texas Instruments' derivative-integration method. See https://e2e.ti.com/blogs_/b/analogwire/posts/capacitive-sensing-simple-algorithm-for-proximity-sensing

# Purpose

* This handles the processing of raw capacitive data readings, determining button press events.
* It also has the support to receive Serial data from a remote capacitive sensor, which is how I'm using it currently.
