# Currently Working

## Shading

- improve depthmap based shading
    - approximat normal (->PHONG or whatever)
    - SSAO
- Based on fractal
    - store "distance from last jump" in a texture and convert 0-1 to a gradient color
    - color depending or which attractor it jumped to (bof à priori)

## Optimization

- Currently we're doing : 1 point -> draw 3 points from attractor / keep 1. We could potentially compute 3, compute 9, draw 3+9=12, keep 1 randomly - DONE / ~ -30% fps for 3 times the ammount of point. (May even also draw first gen and previous ?). Remove Immobile count
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

poor man HBAO

## Leap Motion Integration

acheiving world peace