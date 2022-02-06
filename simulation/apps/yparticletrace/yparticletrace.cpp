//
// LICENSE:
//
// Copyright (c) 2016 -- 2020 Fabio Pellacini
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

#include <yocto/yocto_commonio.h>
#include <yocto/yocto_image.h>
#include <yocto/yocto_math.h>
#include <yocto/yocto_sceneio.h>
#include <yocto/yocto_trace.h>
#include <yocto_particle/yocto_particle.h>
using namespace yocto;

#include <map>
#include <memory>

// construct a scene from io
void init_scene(trace_scene* scene, sceneio_scene* ioscene,
    trace_camera*& camera, sceneio_camera* iocamera,
    progress_callback progress_cb = {}) {
  // handle progress
  auto progress = vec2i{
      0, (int)ioscene->cameras.size() + (int)ioscene->environments.size() +
             (int)ioscene->materials.size() + (int)ioscene->textures.size() +
             (int)ioscene->shapes.size() + (int)ioscene->instances.size()};

  auto camera_map     = unordered_map<sceneio_camera*, trace_camera*>{};
  camera_map[nullptr] = nullptr;
  for (auto iocamera : ioscene->cameras) {
    if (progress_cb) progress_cb("convert camera", progress.x++, progress.y);
    auto camera          = add_camera(scene);
    camera->frame        = iocamera->frame;
    camera->lens         = iocamera->lens;
    camera->aspect       = iocamera->aspect;
    camera->film         = iocamera->film;
    camera->orthographic = iocamera->orthographic;
    camera->aperture     = iocamera->aperture;
    camera->focus        = iocamera->focus;
    camera_map[iocamera] = camera;
  }

  auto texture_map     = unordered_map<sceneio_texture*, trace_texture*>{};
  texture_map[nullptr] = nullptr;
  for (auto iotexture : ioscene->textures) {
    if (progress_cb) progress_cb("convert texture", progress.x++, progress.y);
    auto texture           = add_texture(scene);
    texture->hdr           = iotexture->hdr;
    texture->ldr           = iotexture->ldr;
    texture_map[iotexture] = texture;
  }

  auto material_map     = unordered_map<sceneio_material*, trace_material*>{};
  material_map[nullptr] = nullptr;
  for (auto iomaterial : ioscene->materials) {
    if (progress_cb) progress_cb("convert material", progress.x++, progress.y);
    auto material              = add_material(scene);
    material->emission         = iomaterial->emission;
    material->color            = iomaterial->color;
    material->specular         = iomaterial->specular;
    material->roughness        = iomaterial->roughness;
    material->metallic         = iomaterial->metallic;
    material->ior              = iomaterial->ior;
    material->spectint         = iomaterial->spectint;
    material->coat             = iomaterial->coat;
    material->transmission     = iomaterial->transmission;
    material->translucency     = iomaterial->translucency;
    material->scattering       = iomaterial->scattering;
    material->scanisotropy     = iomaterial->scanisotropy;
    material->trdepth          = iomaterial->trdepth;
    material->opacity          = iomaterial->opacity;
    material->thin             = iomaterial->thin;
    material->emission_tex     = texture_map.at(iomaterial->emission_tex);
    material->color_tex        = texture_map.at(iomaterial->color_tex);
    material->specular_tex     = texture_map.at(iomaterial->specular_tex);
    material->metallic_tex     = texture_map.at(iomaterial->metallic_tex);
    material->roughness_tex    = texture_map.at(iomaterial->roughness_tex);
    material->transmission_tex = texture_map.at(iomaterial->transmission_tex);
    material->translucency_tex = texture_map.at(iomaterial->translucency_tex);
    material->spectint_tex     = texture_map.at(iomaterial->spectint_tex);
    material->scattering_tex   = texture_map.at(iomaterial->scattering_tex);
    material->coat_tex         = texture_map.at(iomaterial->coat_tex);
    material->opacity_tex      = texture_map.at(iomaterial->opacity_tex);
    material->normal_tex       = texture_map.at(iomaterial->normal_tex);
    material_map[iomaterial]   = material;
  }

  auto shape_map     = unordered_map<sceneio_shape*, trace_shape*>{};
  shape_map[nullptr] = nullptr;
  for (auto ioshape : ioscene->shapes) {
    if (progress_cb) progress_cb("convert shape", progress.x++, progress.y);
    auto shape           = add_shape(scene);
    shape->points        = ioshape->points;
    shape->lines         = ioshape->lines;
    shape->triangles     = ioshape->triangles;
    shape->quads         = ioshape->quads;
    shape->quadspos      = ioshape->quadspos;
    shape->quadsnorm     = ioshape->quadsnorm;
    shape->quadstexcoord = ioshape->quadstexcoord;
    shape->positions     = ioshape->positions;
    shape->normals       = ioshape->normals;
    shape->texcoords     = ioshape->texcoords;
    shape->colors        = ioshape->colors;
    shape->radius        = ioshape->radius;
    shape->tangents      = ioshape->tangents;
    shape_map[ioshape]   = shape;
  }

  for (auto ioinstance : ioscene->instances) {
    if (progress_cb) progress_cb("convert instance", progress.x++, progress.y);
    auto instance      = add_instance(scene);
    instance->frame    = ioinstance->frame;
    instance->shape    = shape_map.at(ioinstance->shape);
    instance->material = material_map.at(ioinstance->material);
  }

  for (auto ioenvironment : ioscene->environments) {
    if (progress_cb)
      progress_cb("convert environment", progress.x++, progress.y);
    auto environment          = add_environment(scene);
    environment->frame        = ioenvironment->frame;
    environment->emission     = ioenvironment->emission;
    environment->emission_tex = texture_map.at(ioenvironment->emission_tex);
  }

  // done
  if (progress_cb) progress_cb("convert done", progress.x++, progress.y);

  // get camera
  camera = camera_map.at(iocamera);
}

void flatten_scene(sceneio_scene* ioscene) {
  for (auto ioinstance : ioscene->instances) {
    for (auto& position : ioinstance->shape->positions)
      position = transform_point(ioinstance->frame, position);
    for (auto& normal : ioinstance->shape->normals)
      normal = transform_normal(ioinstance->frame, normal);
    ioinstance->frame = identity3x4f;
  }
}

void init_ptscene(particle_scene* ptscene, sceneio_scene* ioscene,
    unordered_map<sceneio_shape*, particle_shape*>& ptshapemap,
    progress_callback                               progress_cb) {
  // handle progress
  auto progress = vec2i{0, (int)ioscene->instances.size()};

  // shapes
  static auto velocity = unordered_map<string, float>{
      {"floor", 0}, {"particles", 1}, {"cloth", 0}, {"collider", 0}};
  for (auto ioinstance : ioscene->instances) {
    if (progress_cb) progress_cb("convert instance", progress.x++, progress.y);
    auto ioshape    = ioinstance->shape;
    auto iomaterial = ioinstance->material;
    if (iomaterial->name == "particles") {
      auto ptshape = add_particles(
          ptscene, ioshape->points, ioshape->positions, ioshape->radius, 1, 1);
      ptshapemap[ioshape] = ptshape;
    } else if (ioinstance->material->name == "cloth") {
      auto nverts  = (int)ioshape->positions.size();
      auto ptshape = add_cloth(ptscene, ioshape->quads, ioshape->positions,
          ioshape->normals, ioshape->radius, 0.5, 1 / 8000.0,
          {nverts - 1, nverts - (int)sqrt(nverts)});
      ptshapemap[ioshape] = ptshape;
    } else if (ioinstance->material->name == "collider") {
      add_collider(ptscene, ioshape->triangles, ioshape->quads,
          ioshape->positions, ioshape->normals, ioshape->radius);
    } else if (ioinstance->material->name == "floor") {
      add_collider(ptscene, ioshape->triangles, ioshape->quads,
          ioshape->positions, ioshape->normals, ioshape->radius);
    } else {
      print_fatal("unknown material " + ioinstance->material->name);
    }
  }

  // done
  if (progress_cb) progress_cb("convert done", progress.x++, progress.y);
}

void update_ioscene(
    const unordered_map<sceneio_shape*, particle_shape*>& ptshapemap) {
  for (auto [ioshape, ptshape] : ptshapemap) {
    get_positions(ptshape, ioshape->positions);
    get_normals(ptshape, ioshape->normals);
  }
}

int main(int argc, const char* argv[]) {
  // options
  auto ptparams    = particle_params{};
  auto trparams    = trace_params{};
  auto camera_name = ""s;
  auto imfilename  = "out.hdr"s;
  auto filename    = "scene.json"s;

  // parse command line
  auto cli = make_cli("yscntrace", "Offline path tracing");
  add_option(cli, "--camera", camera_name, "Camera name.");
  add_option(cli, "--solver", ptparams.solver, "Solver", particle_solver_names);
  add_option(cli, "--frames", ptparams.frames, "Simulation frames.");
  add_option(cli, "--resolution", trparams.resolution, "Image resolution.");
  add_option(cli, "--samples", trparams.samples, "Number of samples.");
  add_option(
      cli, "--tracer", trparams.sampler, "Trace type.", trace_sampler_names);
  add_option(cli, "--output-image,-o", imfilename, "Image filename");
  add_option(cli, "scene", filename, "Scene filename", true);
  add_option(cli, "--extra,-e", ptparams.flag, "Extra Filter");
  parse_cli(cli, argc, argv);

  // scene loading
  auto ioscene_guard = std::make_unique<sceneio_scene>();
  auto ioscene       = ioscene_guard.get();
  auto ioerror       = ""s;
  if (!load_scene(filename, ioscene, ioerror, print_progress))
    print_fatal(ioerror);
  flatten_scene(ioscene);

  // simulate
  auto ptscene_guard = std::make_unique<particle_scene>();
  auto ptscene       = ptscene_guard.get();
  auto ptshapemap    = unordered_map<sceneio_shape*, particle_shape*>{};
  init_ptscene(ptscene, ioscene, ptshapemap, print_progress);

  // simulate
  simulate_frames(ptscene, ptparams, print_progress);

  // update scene
  update_ioscene(ptshapemap);

  // get camera
  auto iocamera = get_camera(ioscene, camera_name);

  // convert scene
  auto scene_guard = std::make_unique<trace_scene>();
  auto scene       = scene_guard.get();
  auto camera      = (trace_camera*)nullptr;
  init_scene(scene, ioscene, camera, iocamera, print_progress);

  // cleanup
  if (ioscene_guard) ioscene_guard.reset();
  if (ptscene_guard) ptscene_guard.reset();

  // build bvh
  auto bvh_guard = std::make_unique<trace_bvh>();
  auto bvh       = bvh_guard.get();
  init_bvh(bvh, scene, trparams, print_progress);

  // init renderer
  auto lights_guard = std::make_unique<trace_lights>();
  auto lights       = lights_guard.get();
  init_lights(lights, scene, trparams, print_progress);

  // fix renderer type if no lights
  if (lights->lights.empty() && is_sampler_lit(trparams)) {
    print_info("no lights presents, switching to eyelight shader");
    trparams.sampler = trace_sampler_type::eyelight;
  }

  // render
  auto render = trace_image(
      scene, camera, bvh, lights, trparams, print_progress, {});

  // save image
  print_progress("save image", 0, 1);
  if (!save_image(imfilename, render, ioerror)) print_fatal(ioerror);
  print_progress("save image", 1, 1);

  // done
  return 0;
}
