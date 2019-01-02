/*
    Lightmetrica - Copyright (c) 2018 Hisanari Otsu
    Distributed under MIT license. See LICENSE file for details.
*/

#pragma once

#include "component.h"

LM_NAMESPACE_BEGIN(LM_NAMESPACE)

/*!
    \addtogroup model
    @{
*/

/*!
    \brief 3D model format.
    Aggregates multiple meshes and materials generated by the model.
*/
class Model : public Component {
public:
    /*!
        \brief Create primitives from underlying components.
    */
    using CreatePrimitiveFunc = std::function<void(Component* mesh, Component* material, Component* light)>;
    virtual void createPrimitives(const CreatePrimitiveFunc& createPrimitive) const = 0;
};

/*!
    @}
*/

LM_NAMESPACE_END(LM_NAMESPACE)
