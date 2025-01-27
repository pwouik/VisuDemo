# Currently Working

## Shading

Todo
- fine tune light (why does k_d needs to be ridiculously high currently ???)
- full screen support
- better normal computation

Maybe ?
- SSAO ? simple phong & jump dist seem enough
- coloring based on which attractor it jumps to (tends to do ugly noise so no)

## Optimization

- Currently we're doing : 1 point -> draw 3 points from attractor / keep 1. We could potentially compute 3, compute 9, draw 3+9=12, keep 1 randomly - DONE / ~ -30% fps for 4 times the ammount of point. For some reasons perf difference differ on computer so extensive testing will be requiered
- Lower MAX_FUNC_PER_ATTRACTOR in render_attractor.comp (may be irrelevant but get rid of useless memory in each kernel so maybe significant) (currently supports up to 10 function per attractor)

## Bugs

- overwrite matrix option kinda not working at some times for *reasons*



# VisuDemo

## Building the app

in /build/
```cmd
cmake ..
```

```cmd
cmake --build . --config Release
```

## Ray Marching

for context read : https://jbaker.graphics/writings/DEC.html

## Attractors

for context see https://www.youtube.com/watch?v=1L-x_DH3Uvg

### Moving points

Points are only the GPU in an SSBO
Position updated with a compute shader
used to be displayed by concurent writting of pixels on a texture



### Shading

#### general Idea

We write on a depthmap in the compute process.
We approximate normals using this depthmap. (It's actually imprecise because it assume sufficient point density so each neigbhor on the texture is close enought)
Then we just do a simple Phong Lighting based on those normals.

#### race condition when writting on depthmap ?

We simply ignore them.

An issue can occurs if 2 kernels want's to write on the same pixel. However, this is very unlikely.

We choose not to use atomic operation. Thanks to our advanced degree in advanced statistic, we noted that :
- 800x800 texutre => 640 000 pixel
- assuming the point cloud takes 25% of the screen => 160 000 pixel
- assuming we have 2048 kernels on the gpu
- assuming an even repartion of points from cloud to texture

For 1 kernel, the probabily to write on the same pixel than another kernel operating at the same time is : 2047/160000
By some ugly unholy approximation. We say that we have 2048*2047/160000 ~ 26 pixel per 2048 kernel which can conflit (doing the proper math with binomial law is left as an exercise to the reader)
Because in a conflit there's a 50% of the result being correct we divide it by 2.
The 2048 kernels will run until all 20 000 000 points are drawn, so 20 000 000 / 2048  = 1 000 times
With those approximation we get 13 000 pixels having improper depth. This is 8% of the cloud on the texture

Actually that's not accurate, because 8% is only the proportion of issues. But whenever a conflict occurs, it can be overwritten later. So with gross assumption with assume that the proportion of conflict making it out to the final depthmap texture is 160k pixel / 20m ~0.8% of write. So 0.8% of 8% = 0.64% of 160k points.

That's acceptable. We can maybe fixes those numbers in our normal computation (ie detect value to extreme to ignore them, not done yet) or shameless run a blur to fix it.
Well that's assuming atomic is really expensive tbf we didn't really benchmarked it.

We also note that this calcul heavily depends on the number of kernel on the GPU.

All math and assumption aside we can simply observe a decent result.


## Leap Motion Integration

acheiving world peace