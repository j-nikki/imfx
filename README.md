# imfx

Performs user-specified image manipulation with given images. Writes resultant image to stdout.

## Installation

Tested on a 64-bit Artix Linux using GCC 12.1, CMake 3.23, and Conan 1.48.

1. build<br>
`conan install -if build --build outdated . && cmake -B build --preset release && cmake --build build`
2. install<br>
`sudo cmake --install build`<br>*note: default prefix (`/usr/local`) can be overridden with `--prefix <dir>`*

## Usage

<pre><b>SYNOPSIS</b>
  imfx &lt;cmd&gt; &lt;imgs&gt;...

<b>EXAMPLES</b>
  imfx 0.ft(1920x1080) img    <i># fit img into 1920x1080 area</i>
  imfx 0.fl(1920x1080) img    <i># fill 1920x1080 area with img</i>
  imfx 0.pi(1) img0 img1      <i># place img1 amid img0</i>
  imfx 0.gb(425) img          <i># blur img with gaussian kernel σ=4.25</i>

  <i># put img amid a blurred background cover</i>
  imfx 0.fl(1920x1080).gb(425).pi(0.ft(1920x1080)) img</pre>
