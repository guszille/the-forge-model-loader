# Model Loader

Custom model loader built with [The Forge](https://github.com/ConfettiFX/The-Forge) framework. This project uses [Assimp](https://github.com/assimp/assimp) library to read a `.fbx` file from the disk and the **Forge Renderer** to draw it on the screen.

![Castle](./Art/Screenshots/CastleLoadedExample.png)

## Future Improvements

- Read model materials and textures;
- Adjust camera behavior to orbit around the model;
- Use the **Forge FileSystem** to avoid relative model paths.

## Build Instructions

This project was developed using C++, for Windows (x64) platform.

Build it using Visual Studio, it is recommended to use version 17 (**vs2022**). Before that, the `.exe` file will be inside `Solution/$(Platform)/$(Configuration)/ModelLoader`.

## Controls

- Press **W/A/S/D** keys to move the camera;
- Use **MOUSE** cursor to look around (pressing **LEFT MOUSE BUTTON** at the same time);
- Press **SPACE** key to reset the camera;
- Press **ESC** key to close the window.