# VisuDemo / No title project

## Building the app

Download [leap SDK ](https://leap2.ultraleap.com/downloads/) (the Leap Motion Controller, not the 2)
(On windows : if you have issue, consider changeing  `ULTRALEAP_PATH_ROOT` in `CMakeLists.txt` to the path containing `leapsdk-config.cmake` )

in /build/
```cmd
cmake ..
```

```cmd
cmake --build . --config Release
```

# How to use

Keyboard short cut (on azerty keyboard) :
- `,` : lock camear angle 
- `z`, `q`, `s` , `d`, `lshift`, `space` : move arround
- `mousewheel` to change movement speed
- everything else can be controlled with the UI

Leap motion set up :
- In the UI, press the `Start Leap Capture` button
- for Attractor, you'll also need to switch the `lerping mode` to `leapmotion & hand`

Below are video presenting the possibilities :
- Raymarching :
- Attractor : https://www.youtube.com/watch?v=wo6vJYoP6qw

# Technical details

## Ray Marching

### General Idea

- Raymarching is similar to raytracing in that we launch rays from the camera, but instead of testing all intersections, we simply estimate a distance we can advance without hitting anything and step the ray
- We can make use of signed distance functions(SDF), which give the euclidian distance to the nearest surface for a given position, negative values being inside the object or something smaller but converging to the surface.
- For a sphere, we can simply take the distance to the center and subtract the radius
- Some operation can be performed on one or more SDF while conserving these properties:
    - union : min of distances
    - intersection : max of distances(must be orthogonal)
    - fold: mirror the position in it is in one half of a plane, ex absolute value to mirror around zero
- linear transformation : inverse transformation on position
- for rendering fractals, we mainly use the fold operation because it double the amount geometry using only one operation.
- The fractal shocased here is the Kaleidoscopic Iterated Function System (KIFS) family of fractals:
      - it fold one three axis, conveniently using abs on a vec3
      - then apply an orthogonal affine transformation
      - repeat
- here is a collection of signed distance functions to play with: https://jbaker.graphics/writings/DEC.html

### Rendering
- the shading use phong plus shadow rays
- we make soft shadows by using the distance function along the ray to conpute the exposure on a point
- ambient occlusion simply use the number of steps

## Attractors

### General Idea

For context, we would heavily recommend watching this video : https://www.youtube.com/watch?v=1L-x_DH3Uvg (The attractor part was initially an implementation. we mostly assume that you've watch the video and understood the basic principle).

In our implementation, points are only the GPU in an SSBO. The number of point can be changed when starting the proram with CLI arguments.

Position updated with a compute shader : `attractor.comp`. This compute shader does a few things :
1. For each point, for each attractor compute its transformed position. If it's the nearest on the depthmap, updates the depthmap and the "jump distance" texture. (this may cause concurrent writting, see below why we ignore this). Note that, when the camera is still and attractor functions didn't changed, points accumulate on the depthmap (on a side note this allows us to get hight resolution renders with only 500k points on the GPU)
2. choose randomly using weight (see below) an attractor and updates the point position with its transform

The actual rendering is done within the compute shader `attractor_shading.comp`. Using the informations obtained from the previous path, it generates the colored texture that is rendered.

### Weights

Idealy, we would wan't every computed point to end up as being on the surface on the fractal viewed from the camera. Because this is *obviously* impossible, we settles on a less but good enought alternative, having a somewhat uniform density of computed point. To explain this intuitively, some transformations actually contributes to the final fractal more than other, this what the weights are for.

The actual logic beind it was revealed to @pwouik in a dream while he was working on Romanesco's cabbage. 

Please note that the weights are actually important. While for most of randomly generated attractors, the attractors functions all contributes to same ammount to the final form so they could be ignored, having weights actually fasten convergende for Romanesco cabbage by a lot.

```cpp
//in attractor_renderer.cpp
float total_weight=0.0;
for(int i=0; i < matrix_per_attractor; i++){
    glm::mat3 m = glm::mat3(ubo_matrices[i]);
    total_weight+= glm::length(glm::cross(m[0],m[1]))+glm::length(glm::cross(m[1],m[2]))+glm::length(glm::cross(m[2],m[0]));
    ubo_weights[i] = total_weight;
}
for(int i=0; i < matrix_per_attractor; i++){
    ubo_weights[i] /= total_weight;
}
```

### Shading

#### general Idea

We write on a depthmap in the compute process.
We approximate normals using this depthmap. (It's actually imprecise because it assume sufficient point density so each neigbhor on the texture is close enought)
Then, using those normals, we can add :
- a simple phong
- handmades dubious SSAO implementation

#### Ambient Occlusion

##### Multiples AO ?

After consideration, actual SSAO as explained in the litterature seemed kinda sus to us (tbh we were intially too lazy to do the math requiered to sample points inside an half-sphere, even tho we eventually ended up implementing it but weren't satisfied by it). So we tried a few things, all with the same idea of accumalting depth difference to get an SSAO factor

Note that this brutforce approach produce a lot of useless computation which should probably be optimized by using some tilling approach and shared memory, but when we have millions points to handle it's actually not that much.

All of the 5 approach share a size and an intensity factor with different meaning in there context. In order to get visually coherent value those parameter have been compensated with hard coded magic number.

##### SSAO v1

AO factor <=> difference of exposition using the ratio of the difference of depth and distance to the point

##### SSAO v2

AO factor <=> how much the neighboring sampled point are closer to the camera ?

##### SSAO v3

AO factor <=> difference of exposition using the ratio of the difference of depth from the normal and distance to the point

##### SSAO v4

Inspired by the true SSAO implemtation, but compares distance of sampled point with the distance to the actual point (as this doesn't relies on the depthmap this could be acheived with less computation and 0 noise using only the normal)

##### SSAO v5

actual by the book implemtation of SSAO. See here : https://learnopengl.com/Advanced-Lighting/SSAO

#### race condition when writting on depthmap ?

We simply ignore them. (below an interpretation as to why we can do that)

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

**All math and assumption aside we can simply observe a decent result.**

## Leap Motion Integration

The project integrates Leap Motion for intuitive hand-based control of both the raymarching and attractor rendering modes. This allows users to explore and manipulate fractals using natural hand gestures.

### Raymarching Mode Controls

In raymarching mode, the Leap Motion controller enables:

- **Right hand (pinch gesture)**: Controls fractal parameters
  - Rotates the fractal based on hand orientation
  - Adjusts offset parameters based on hand position
  - Pinch gesture must be maintained for control

- **Left hand (pinch gesture)**: Controls camera/viewing position
  - Moves the fractal position based on hand velocity
  - Rotates the view according to hand orientation
  - Provides smooth transitions between positions with configurable smoothing factors

### Attractor Mode Controls

In attractor mode, the Leap Motion controller provides:

- For Attractor, you'll also need to switch the `lerping mode` to `leapmotion & hand`

- The right hand allows the user to explore the neighborhood of a given seed. Movement are smoothed with an exponential lerp. The seed can be changed bringing pinky fingers together.

- Left hand with pinch gesture: Controls camera movement
  - Adjusts camera position based on palm velocity
  - Changes view rotation based on palm orientation

- When both hands are used and pinky fingers are brought close together, a new random seed is generated, creating an entirely new fractal

### Implementation Details

- Movement is smoothed with exponential lerp functions to provide fluid transitions

- Seed changes occur when pinky fingers from both hands come close together

- The implementation maps various hand features to appropriate fractal parameters:
  - Finger positions and orientations
  - Palm orientation and velocity
  - Pinch detection between fingers

The Leap Motion integration allows for intuitive exploration of complex mathematical structures without requiring keyboard or mouse input, creating a more immersive and natural interaction experience.

## known issue

- `assert` macro expansion dependant on compiler. It was fixed in a ugly way using `#IFDEF`
- rng relies on the old `<cstdlib>` We should switch to `<random>` or smth else in order to access the seed at any given point in time (esp when generating knew attractors so we caneasily store a seed for attractory manually randomly generated after the program started) 
- The whole class hierarchie abstraction for attractor / attractor function / matrix overriding is kinda messy but it works. However it's to note that overriding matrices was though as a debug feature, and if this part is rewritten removing it wouldn't be a major issue.
- attractor : in some cases, `forceRedraw` isn't correctly updated so you have to move arround to clear depthmap
- attractor : in some cases, notably if the leap isn't correclty set up, the attractor functions will unpredictably degenerate when changing the lerping mode

# Perspective

## both

- We could store configuration in json, and add the ability to load / save configuration in text file. This includes :
    - raymarching parameters
    - attractors parameters (including global parameter such lerp value, number of function per attractor)
    - global settings (color)
- add the ability to take a screenshot : The original selling point of the project was to prompt users to get a **unique** fractal (involvment feeling) and then send them by mail so we get there contact information which the communication departement was interested into. While it's entirely possible to currently just take a screenshot and send it manually, we recommend saving textures as images and store in a separate text file a list of e mail address. This would heavily simplify the e mail sending process which could later be done manually or with a simple python script.
- Better controls (moving with left hand) with leap motion : While the general architecture for leapmotion controls is here, it should be ensured that it's impossible for a user to look at nothing. For instance :
    - ensure that camera moves on a sphere arround the fractal and always looks towards it
    - ensure that the camera is not too far / too close from the fractal
- some kuwahara filter or some other kind of post processing at the end because why not, it looks pretty

## Raymarching

- phong and shadow are not apparent by default because it didn't look "pretty", maybe find a way do do softer shading
- subsurface scattering conld be done to give a tranlucent effect by stepping a bit inside the fractal and computing scattering
- performance could be enhanced by doing cone marching: https://www.fulcrum-demo.org/wp-content/uploads/2012/04/Cone_Marching_Mandelbox_by_Seven_Fulcrum_LongVersion.pdf

## Attractor

- better normals computation : Currently normal are computed using `computeNormals` in `attractor_shading.comp` which average the cross product of a cross sampling of depth with only 5 points. This works assuming the attractor cloud is sufficiently dense on it's surface. However this could easily be improved with :
    - more sampling  (sample all depth in a 3x3 grid, or 5x5, maybe consider weighting the contribution to normals)
    - ignoring depth too extreme : if a sampled depth is too different, then it's likely that the cloud isn't dense enought and we should ignore that sampled point for normal approximation
- better controls : currently, the right hand allows the user to explore the neighborhood of a given seed. Movement are smoothed with an exponential lerp. The seed can be changed bringing pinky fingers together. While we acheived something satisfying, we would heavily recommend starting over the maths on this part to get have a better mapping between the fingers position and the neighborhood exploration.
- We tried some coloring strategy based on "which attractor did the point jumped to / is comming from" without much success but we still think there's some potential in this approach
- Optimisation : 
    - Currently we're doing : 1 point -> draw 3 points from attractor / keep 1. We could potentially compute 3, compute 9, draw 3+9=12, keep 1 randomly. Drawing multiple genration at once is something we tried but haven't extensively benchmarked. For some reasons it seemed to su that perf difference differ depending on computer so extensive testing will be requiered.
    - Lower `MAX_FUNC_PER_ATTRACTOR` in `attractor.comp` (may be irrelevant but get rid of useless memory in each kernel so maybe significant) (currently supports up to 10 function per attractor)
    - shared memory / tilling in SSAO as mentionned in part above


# note for IDE/env set up

## Linux
- ```mkdir build```
- ```cd build```
- ```cmake ..```
- ```cmake --build . --config Release```
- ```cd ..```
- ```XDG_SESSION_TYPE=x11 ./build/VisuDemo```(imgui multiframe not working on wayland)

## On windows / VCPKG / VS code

Even if CMake should installs relevant dependencies, we recommend using a package manger such as [VPCKG](https://github.com/microsoft/vcpkg)

For intellisense, update `c_cpp_properties` to contain :
```json
"includePath": [
    "${workspaceFolder}/**",
    "C:/vcpkg/installed/x64-windows/include/**",
    "C:/Program Files/Ultraleap/LeapSDK/include/**"
],
```

For compiling, you can use a `tasks.json` similar to this. Then run `ctrl+shift+p` in any `.cpp` file task > run task

```json
//if you're using VCPKG, update relevants paths
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "CMake Configure",
            "type": "shell",
            "command": "cmake",
            "args": [
                "-S ${fileDirname}/..",
                "-B ${fileDirname}/../build",
                "-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake", 
            ],
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": [],
            "detail": "Runs cmake to configure the project."
        },
        {
            "label": "CMake Build",
            "type": "shell",
            "command": "cmake",
            "args": [
                "--build",
                "${fileDirname}/../build",
                "--config",
                "Release"
            ],
            "group": {
                "kind": "build",
                "isDefault": false
            },
            "problemMatcher": [],
            "detail": "Builds the project in Release mode."
        },
        {
            "type": "cmake",
            "label": "CMake: clean",
            "command": "clean",
            "problemMatcher": [],
            "detail": "CMake template clean task"
        }
    ]
}
```
