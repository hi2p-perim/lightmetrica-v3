/*
    Lightmetrica - Copyright (c) 2019 Hisanari Otsu
    Distributed under MIT license. See LICENSE file for details.
*/

#include <lm/lm.h>
using namespace lm::literals;

// ------------------------------------------------------------------------------------------------

// Simple ambient occlusion renderer
class Renderer_AO final : public lm::Renderer {
private:
    lm::Scene* scene_;
    lm::Film* film_;
    long long spp_;
    int rng_seed_ = 42;

public:
    virtual void construct(const lm::Json& prop) override {
        scene_ = lm::json::comp_ref<lm::Scene>(prop, "scene");
        film_ = lm::json::comp_ref<lm::Film>(prop, "output");
        spp_ = lm::json::value<long long>(prop, "spp");
    }

    virtual void render() const override {
        const auto [w, h] = film_->size();
        lm::parallel::foreach(w*h, [&](long long index, int threadId) -> void {
            thread_local lm::Rng rng(rng_seed_ + threadId);
            const int x = int(index % w);
            const int y = int(index / w);
            const auto ray = scene_->primary_ray({(x+.5_f)/w, (y+.5_f)/h}, film_->aspect_ratio());
            const auto hit = scene_->intersect(ray);
            if (!hit) {
                return;
            }
            auto V = 0_f;
            for (long long i = 0; i < spp_; i++) {
                const auto [n, u, v] = hit->geom.orthonormal_basis(-ray.d);
                const auto d = lm::math::sample_cosine_weighted(rng);
                V += scene_->intersect({hit->geom.p, u*d.x+v*d.y+n*d.z}, lm::Eps, .2_f) ? 0_f : 1_f;
            }
            V /= spp_;
            film_->set_pixel(x, y, lm::Vec3(V));
        });
    }
};

LM_COMP_REG_IMPL(Renderer_AO, "renderer::ao");

// ------------------------------------------------------------------------------------------------

// This example illustrates how to create custom renderer.
int main(int argc, char** argv) {
    try {
        // Initialize the framework
        lm::init();
        lm::parallel::init(lm::parallel::DefaultType, {
            #if LM_DEBUG_MODE
            {"numThreads", 1}
            #else
            {"numThreads", -1}
            #endif
        });
        lm::info();

        // Parse command line arguments
        const auto opt = lm::json::parse_positional_args<13>(argc, argv, R"({{
            "obj": "{}",
            "out": "{}",
            "spp": {},
            "w": {},
            "h": {},
            "eye": [{},{},{}],
            "lookat": [{},{},{}],
            "vfov": {}
        }})");

        // ----------------------------------------------------------------------------------------

        // Define assets

        // Film for the rendered image
        lm::asset("film1", "film::bitmap", {
            {"w", opt["w"]},
            {"h", opt["h"]}
        });

        // Pinhole camera
        lm::asset("camera1", "camera::pinhole", {
            {"film", lm::asset("film1")},
            {"position", opt["eye"]},
            {"center", opt["lookat"]},
            {"up", {0,1,0}},
            {"vfov", opt["vfov"]}
        });

        // OBJ model
        // Replace all materials to diffuse and use our checker texture
        lm::asset("obj1", "model::wavefrontobj", {
            {"path", opt["obj"]}
        });

        // ----------------------------------------------------------------------------------------

        // Define scene primitives

        // Camera
        lm::primitive(lm::Mat4(1), {
            {"camera", lm::asset("camera1")}
        });

        // Create primitives from model asset
        lm::primitive(lm::Mat4(1), {
            {"model", lm::asset("obj1")}
        });

        // ----------------------------------------------------------------------------------------

        // Render an image
        lm::build("accel::sahbvh");
        lm::render("renderer::ao", {
            {"output", lm::asset("film1")},
            {"spp", opt["spp"]}
        });

        // Save rendered image
        lm::save(lm::asset("film1"), opt["out"]);

        // Shutdown the framework
        lm::shutdown();
    }
    catch (const std::exception& e) {
        LM_ERROR("Runtime error: {}", e.what());
    }

    return 0;
}
