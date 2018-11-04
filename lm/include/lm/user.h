/*
    Lightmetrica - Copyright (c) 2018 Hisanari Otsu
    Distributed under MIT license. See LICENSE file for details.
*/

#pragma once

#include "detail/component.h"

LM_NAMESPACE_BEGIN(LM_NAMESPACE)

// ----------------------------------------------------------------------------

/*!
    \brief Initializes the renderer.
*/
LM_PUBLIC_API void init(const Json& prop = {});

/*!
    \brief Shutdown the renderer.
*/
LM_PUBLIC_API void shutdown();

/*!
    \brief Create an asset.
*/
LM_PUBLIC_API void asset(const std::string& name, const std::string& implKey, const Json& prop);

/*!
    \brief Create a primitive and add it to the scene.
*/
LM_PUBLIC_API void primitive(Mat4 transform, const Json& prop);

/*!
    \brief Create primitives from a model.
*/
LM_PUBLIC_API void primitives(Mat4 transform, const std::string& modelName);

/*!
    \brief Render image based on current configuration.
*/
LM_PUBLIC_API void render(const std::string& rendererName, const std::string& accelName, const Json& prop);

/*!
    \brief Save an image.
*/
LM_PUBLIC_API void save(const std::string& filmName, const std::string& outpath);

// ----------------------------------------------------------------------------

LM_NAMESPACE_BEGIN(detail)

/*!
    \brief Scoped guard of `init` and `shutdown` functions.
*/
class ScopedInit {
public:
    ScopedInit() { init(); }
    ~ScopedInit() { shutdown(); }
    LM_DISABLE_COPY_AND_MOVE(ScopedInit)
};

LM_NAMESPACE_END(detail)
LM_NAMESPACE_END(LM_NAMESPACE)