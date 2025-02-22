#pragma once

// Assimp.
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Forge Interfaces.
#include <Application/Interfaces/IApp.h>
#include <Application/Interfaces/ICameraController.h>
#include <Application/Interfaces/IFont.h>
#include <Application/Interfaces/IProfiler.h>
#include <Application/Interfaces/IScreenshot.h>
#include <Application/Interfaces/IUI.h>

#include <Game/Interfaces/IScripting.h>

#include <Utilities/Interfaces/IFileSystem.h>
#include <Utilities/Interfaces/IToolFileSystem.h>
#include <Utilities/Interfaces/ILog.h>
#include <Utilities/Interfaces/ITime.h>
#include <Utilities/RingBuffer.h>

// Forge Renderer.
#include <Graphics/Interfaces/IGraphics.h>

#include <Resources/ResourceLoader/Interfaces/IResourceLoader.h>

// Forge Math.
#include <Utilities/Math/MathTypes.h>

#include <Utilities/Interfaces/IMemory.h>
