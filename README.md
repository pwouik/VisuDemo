# Currently Working

## Shading

- improve depthmap based shading
    - approximat normal (->PHONG or whatever)
    - SSAO *: kinda doomed because we need to compute accurate normals ... (nvm computing normal seems doable)
- Based on fractal
    - store "distance from last jump" in a texture and convert 0-1 to a gradient color (done, I'll try some maprange clamping but doesn't seem to do a big dif)
    - color depending or which attractor it jumped to (bof à priori)

## Optimization

- Currently we're doing : 1 point -> draw 3 points from attractor / keep 1. We could potentially compute 3, compute 9, draw 3+9=12, keep 1 randomly - DONE / ~ -30% fps for 4 times the ammount of point. For some reasons perf difference differ on computer so extensive testing will be requiered
- Lower MAX_FUNC_PER_ATTRACTOR in render_attractor.comp (may be irrelevant but get rid of useless memory in each kernel so may be significant)



# VisuDemo

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

That's acceptable. We can fixe those 8% in our normal computation (not done yet) or shameless run a blur to fix it.
Well that's assuming atomic is really expensive tbf we didn't really benchmarked it.

We also note that this calcul heavily depends on the number of kernel on the GPU.

All math and assumption aside we can simply observe a decent result.





## Leap Motion Integration

acheiving world peace