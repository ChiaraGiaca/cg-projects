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

#include <yocto/yocto_color.h>
#include <yocto/yocto_commonio.h>
#include <yocto/yocto_geometry.h>
#include <yocto/yocto_image.h>
#include <yocto/yocto_math.h>
#include <yocto/yocto_sampling.h>
#include <yocto/yocto_sceneio.h>
#include <yocto/yocto_shape.h>
using namespace yocto;

#include <filesystem>
#include <memory>

#include "ext/perlin-noise/noise1234.h"

float noise(const vec3f& p) { return noise3(p.x, p.y, p.z); }
vec2f noise2(const vec3f& p) {
  return {noise(p + vec3f{0, 0, 0}), noise(p + vec3f{3, 7, 11})};
}
vec3f noise3(const vec3f& p) {
  return {noise(p + vec3f{0, 0, 0}), noise(p + vec3f{3, 7, 11}),
      noise(p + vec3f{13, 17, 19})};
}
float fbm(const vec3f& p, int octaves) {
  auto sum    = 0.0f;
  auto weight = 1.0f;
  auto scale  = 1.0f;
  for (auto octave = 0; octave < octaves; octave++) {
    sum += weight * fabs(noise(p * scale));
    weight /= 2;
    scale *= 2;
  }
  return sum;
}
float turbulence(const vec3f& p, int octaves) {
  auto sum    = 0.0f;
  auto weight = 1.0f;
  auto scale  = 1.0f;
  for (auto octave = 0; octave < octaves; octave++) {
    sum += weight * fabs(noise(p * scale));
    weight /= 2;
    scale *= 2;
  }
  return sum;
}
float ridge(const vec3f& p, int octaves) {
  auto sum    = 0.0f;
  auto weight = 0.5f;
  auto scale  = 1.0f;
  for (auto octave = 0; octave < octaves; octave++) {
    sum += weight * (1 - fabs(noise(p * scale))) * (1 - fabs(noise(p * scale)));
    weight /= 2;
    scale *= 2;
  }
  return sum;
}

/*SMOOTH VORONOI*/
float voronoi(vec3f vector) {
  auto  p     = vec3f{floor(vector.x), floor(vector.y), floor(vector.z)}; /*arrotondo i valori del mio vettore*/
  vec3f fract = {vector.x - floor(vector.x), vector.y - floor(vector.y), /*creo un vettore con le differenze di approssimazione (numeri dopo la virgola)*/
      vector.z - floor(vector.z)};

  float res = 0.0;
  for (int j = -1; j <= 1; j++)
    for (int i = -1; i <= 1; i++) {
      for (int k = -1; k <= 1; k++) {
        vec3f idx = {i, j, k};
        vec3f r   = idx - fract; /*senza random*/
        float d   = length(r);

        res += exp(-32.0 * d);
      }
    }
  return -(1.0 / 32.0) * log10(res);
}

/*creo un turbulence di smooth voronoi*/
float vturbulence(const vec3f& p, int octaves) {
  auto sum    = 0.0f;
  auto weight = 1.0f;
  auto scale  = 1.0f;
  for (auto octave = 0; octave < octaves; octave++) {
    sum += weight * fabs(voronoi(p * scale)); /*valore assoluto in float*/
    weight /= 2;
    scale *= 2;
  }
  return sum;
}

/*SPIKE NOISE: ho preso ispirazione dall'algoritmo del voronoi: da cell voronoi ho tolto il secondo for: non faccio la ricerca  dei vicini */
float spikeDistance(vec3f vector) {
  auto  rng   = make_rng(172784);
  auto  p     = vec3f{floor(vector.x), floor(vector.y), floor(vector.z)};
  vec3f fract = {vector.x - floor(vector.x), vector.y - floor(vector.y),
      vector.z - floor(vector.z)};
  vec3f mr;
  vec3f mb;
  float res = 8.0;

  for (int j = -1; j <= 1; j++)
    for (int i = -1; i <= 1; i++) {
      for (int k = -1; k <= 1; k++) {
        vec3f idx = {i, j, k};
        vec3f r   = idx - fract + rand3f(rng) * (p + idx);
        float d   = dot(r, r);

        if (d < res) {
          res = d;
          mr  = r;
          mb  = idx;
        }
      }
    }
  return res;
}

float getBorder(vec3f vector) { 
    float d = spikeDistance(vector); 
    return 1.0 - smoothstep(0.0, 0.05, d);
}


sceneio_instance* get_instance(sceneio_scene* scene, const string& name) {
  for (auto instance : scene->instances)
    if (instance->name == name) return instance;
  print_fatal("unknown instance " + name);
  return nullptr;
}

void add_polyline(sceneio_shape* shape, const vector<vec3f>& positions,
    const vector<vec4f>& colors, float thickness = 0.0001f) {
  auto offset = (int)shape->positions.size();
  shape->positions.insert(
      shape->positions.end(), positions.begin(), positions.end());
  shape->colors.insert(shape->colors.end(), colors.begin(), colors.end());
  shape->radius.insert(shape->radius.end(), positions.size(), thickness);
  for (auto idx = 0; idx < positions.size() - 1; idx++) {
    shape->lines.push_back({offset + idx, offset + idx + 1});
  }
}

void sample_shape(vector<vec3f>& positions, vector<vec3f>& normals,
    vector<vec2f>& texcoords, sceneio_shape* shape, int num) {
  auto triangles  = shape->triangles;
  auto qtriangles = quads_to_triangles(shape->quads);
  triangles.insert(triangles.end(), qtriangles.begin(), qtriangles.end());
  auto cdf = sample_triangles_cdf(triangles, shape->positions);
  auto rng = make_rng(19873991);
  for (auto idx = 0; idx < num; idx++) {
    auto [elem, uv] = sample_triangles(cdf, rand1f(rng), rand2f(rng));
    auto q          = triangles[elem];
    positions.push_back(interpolate_triangle(shape->positions[q.x],
        shape->positions[q.y], shape->positions[q.z], uv));
    normals.push_back(normalize(interpolate_triangle(
        shape->normals[q.x], shape->normals[q.y], shape->normals[q.z], uv)));
    if (!texcoords.empty()) {
      texcoords.push_back(interpolate_triangle(shape->texcoords[q.x],
          shape->texcoords[q.y], shape->texcoords[q.z], uv));
    } else {
      texcoords.push_back(uv);
    }
  }
}
/***Procedural Terrain** nella funzione `make_terrain()`:
  - creare il terrain spostando i vertici lungo la normale
  - l'altezza di ogni vertice va calcolata con un `ridge()` noise,
    che dovete implementare, moltiplicato per `1 - (pos - center) / size`
  - applicare poi i colori ai vertici in base all'altezza, `bottom`
    per il 33%, `middle` dal 33% al 66% e `top` il resto
  - alla fine calcolate vertex normals usando le funzioni in Yocto/Shape*/

struct terrain_params {
  float size    = 0.1f;
  vec3f center  = zero3f;
  float height  = 0.1f;
  float scale   = 10;
  int   octaves = 8;
  vec4f bottom  = srgb_to_rgb(vec4f{154, 205, 50, 255} / 255);
  vec4f middle  = srgb_to_rgb(vec4f{205, 133, 63, 255} / 255);
  vec4f top     = srgb_to_rgb(vec4f{240, 255, 255, 255} / 255);
};

void make_terrain(sceneio_scene* scene, sceneio_instance* instance,
    const terrain_params& params) {
  auto normal = instance->shape->normals;  /*vettore delle normali*/
  auto pos = instance->shape->positions;   /*vettore delle posizioni*/
  
  /*scorro il vettore delle posizioni per creare
  la forma della montagna*/
  for (int i = 0; i < pos.size(); i++) {
    auto            h = (1 - length(pos[i] - params.center) / params.size) *params.height* ridge(pos[i] * params.scale, params.octaves); /*scalo il noise*/
    pos[i] += normal[i] * h;
    /*coloro le parti della montagna a seconda dell'altezza del vertice*/
    if (pos[i].y <= 0.030) {
      instance->shape->colors.push_back(params.bottom);
    } else if (pos[i].y > 0.030 && pos[i].y <= 0.060) {
      instance->shape->colors.push_back(params.middle);
    } else {
      instance->shape->colors.push_back(params.top);
    }
    instance->shape->positions[i] = pos[i];
  }
  instance->shape->normals = compute_normals(
      instance->shape->quads, instance->shape->positions);
}
         
/***Procedural Dispalcement** nella funzione `make_displacement()`:
       - spostare i vertici lungo la normale per usando `turbulence()` noise,
         che dovete implementare
       - colorare ogni vertice in base all'altezza tra `bottom` e `top`
       - alla fine calcolate vertex normals usando le funzioni in Yocto/Shape*/
  struct displacement_params {
  float height  = 0.02f;
  float scale   = 50;
  int   octaves = 8;
  vec4f bottom  = srgb_to_rgb(vec4f{64, 224, 208, 255} / 255);
  vec4f top     = srgb_to_rgb(vec4f{244, 164, 96, 255} / 255);
};
     void make_displacement(sceneio_scene* scene, sceneio_instance* instance,
         const displacement_params& params) {
       auto pos     = instance->shape->positions;
       auto normals = instance->shape->normals;
       for (int i = 0; i < pos.size(); i++) {
         auto h                        = turbulence(pos[i] * params.scale, params.octaves) * params.height;
         pos[i]                        += normals[i] * h;
         /*coloro interpolando i vertici, perchè il colore dipende dall'altezza tra bottom e top*/
         instance->shape->colors.push_back(lerp(params.bottom, params.top, h/params.height));
         instance->shape->positions[i] = pos[i];
       }
       instance->shape->normals = compute_normals(
           instance->shape->quads, instance->shape->positions);
     }

/***Procedural Hair** nella funzione `make_hair()`:
  - generare `num` capelli a partire dalla superficie in input
  - ogni capello è una linea di `steps` segmenti
  - ogni capello inizia da una punto della superficie ed e' direzionato
    lungo la normale
  - il vertice successivo va spostato lungo la direzione del capello di
    `length/steps`, perturbato con `noise()` e spostato lungo l'asse y
    di una quantità pari alla gravità
  - il colore del capelli varia lungo i capelli da `bottom` a `top`
  - per semplicità abbiamo implementato la funzione `sample_shape()` per
    generare punti sulla superficie e `add_polyline()` per aggiungere un
    "capello" ad una shape
  - alla fine calcolate vertex tangents usando le funzioni in Yocto/Shape*/

struct hair_params {
  int   num      = 100000;
  int   steps    = 1;
  float lenght   = 0.02f;
  float scale    = 250;
  float strength = 0.01f;
  float gravity  = 0.0f;
  vec4f bottom   = srgb_to_rgb(vec4f{25, 25, 25, 255} / 255);
  vec4f top      = srgb_to_rgb(vec4f{244, 164, 96, 255} / 255);
};

void make_hair(sceneio_scene* scene, sceneio_instance* instance,
    sceneio_instance* hair, const hair_params& params) {
  hair->shape    = add_shape(scene, "hair");
  /*mi salvo la size di positions, perchè da lì inserirò i vertici in cui dovrò mettere i peli*/
  auto init = instance->shape->positions.size();
  
  /*inserisco tanti punti sulla superficie quanti capelli voglio inserire (params.num)*/
  sample_shape(
      instance->shape->positions, instance -> shape -> normals, instance -> shape -> texcoords, instance -> shape, params.num);
  
  /*ciclo sui numeri relativi a quanti capelli io debba inserire, partendo dalla size che mi ero salvata*/
  for (int i = init; i < instance -> shape -> positions.size(); i++) {
    auto pos = instance->shape->positions[i];
    auto v_pos   = vector<vec3f>{};
    auto v_color = vector<vec4f>{};
    v_pos.push_back(pos);
    v_color.push_back(params.bottom);
    
    /*ogni capello è formato da steps segmenti*/
    for (int j = 0; j < params.steps; j++) {
      auto final =
          noise3(pos * params.scale) * params.strength +
          (params.lenght / params.steps) * instance->shape->normals[i] + pos;
      final -= vec3f{0, params.gravity, 0}; /*la gravità punta verso il basso*/
      instance->shape->normals[i] = normalize(final - pos);
      v_pos.push_back(final);
      v_color.push_back(lerp(params.bottom, params.top, (float) (j + 1) / (float) params.steps));
      pos = final;  
    }
    add_polyline(hair->shape, v_pos, v_color);  
  }
  instance->shape->normals = compute_tangents(instance -> shape -> lines, instance->shape->positions);  

}

struct grass_params {
  int num = 10000;
};

/***Procedural Grass** nella funzione `trace_texcoord()`:
  - generare `num` fili d'erba instanziando i modelli dati in input
  - per ogni modello, le instanze vanno salvate in `object->instance`
  - per ogni filo d'erba, scegliere a random l'oggetto da instanziare e
    metterlo sulla superficie in un punto samplato e orientato lungo la normale
  - per dare variazione transformare l'instanza applicando, in questo ordine,
    uno scaling random tra 0.9 e 1.0, una rotazione random attorno all'asse
    z tra 0.1 e 0.2, e una rotazione random attorno all'asse y tra 0 e 2 pi.*/

void make_grass(sceneio_scene* scene, sceneio_instance* object,
    const vector<sceneio_instance*>& grasses, const grass_params& params) {
  auto init = object->shape->positions.size();
  sample_shape(object->shape->positions, object->shape->normals,
      object->shape->texcoords, object->shape, params.num);

  /*inizializzo un generatore random di numeri*/
  auto rng = make_rng(0);

  for (int i = init; i < object->shape->positions.size(); i++) {
    auto istanza = add_instance(scene); /*creo l'istanza nella scena*/
    sceneio_instance* erba =
        grasses[rand1i(rng, grasses.size())]; /*scelgo a random l'oggetto di
                                                 tipo grasses da instanziare*/
   
    istanza->shape    = erba->shape;
    istanza->material = erba->material;
    istanza->frame *= translation_frame(object->shape->positions[i]) *
                      scaling_frame(vec3f{rand3f(rng) * 0.1f + 0.9f}) *
                      rotation_frame(
                          istanza->frame.y, rand1f(rng) * 2.0f * pif) *
                      rotation_frame(istanza->frame.z, rand1f(rng)* 0.1f + 0.1f);
  }
}
 
/*SMOOTH VORONOI*/
struct voronoi_params {
  float height  = 0.05f;
  float scale   = 80;
  int   octaves = 8;
  vec4f bottom  = srgb_to_rgb(vec4f{255, 0, 0, 255} / 255);
  vec4f top     = srgb_to_rgb(vec4f{0, 255, 0, 255} / 255);
};
void make_voronoi(sceneio_scene* scene, sceneio_instance* instance, const voronoi_params& params) {
  auto pos     = instance->shape->positions;
  auto normals = instance->shape->normals;
  for (int i = 0; i < pos.size(); i++) {
    auto h = vturbulence(pos[i] * params.scale, params.octaves) * params.height;
    pos[i] += normals[i] * h;
    instance->shape->colors.push_back(
        lerp(params.bottom, params.top, h / params.height));
    instance->shape->positions[i] = pos[i];
  }
  instance->shape->normals = compute_normals(
      instance->shape->quads, instance->shape->positions);
}

/*SPIKE NOISE: ho implementato un noise 
che riproduce degli spuntoni colorati su una sfera*/
struct spikenoise_params {
  float height  = 0.05f;
  float scale   = 50;
  int   octaves = 8;
  vec4f bottom  = srgb_to_rgb(vec4f{143, 0, 255, 255} / 255);
  vec4f top     = srgb_to_rgb(vec4f{51, 221, 255, 255} / 255);
};

void make_spikenoise(sceneio_scene* scene, sceneio_instance* instance,
    const spikenoise_params& params) {
  auto pos     = instance->shape->positions;
  auto normals = instance->shape->normals;
  
  for (int i = 0; i < pos.size(); i++) {
    auto h = getBorder(pos[i] * params.scale) * params.height;
    pos[i] += normals[i] * h;
    instance->shape->colors.push_back(
        lerp(params.bottom, params.top, h / params.height));
    instance->shape->positions[i] = pos[i];
  }
  instance->shape->normals = compute_normals(
      instance->shape->quads, instance->shape->positions);
}



int main(int argc, const char* argv[]) {
  // command line parameters
  auto terrain      = ""s;
  auto tparams      = terrain_params{};
  auto displacement = ""s;
  auto dparams      = displacement_params{};
  auto hair         = ""s;
  auto hairbase     = ""s;
  auto hparams      = hair_params{};
  auto grass        = ""s;
  auto grassbase    = ""s;
  auto gparams      = grass_params{};
  auto output       = "out.json"s;
  auto filename     = "scene.json"s;
  auto voronoi = ""s;
  auto vparams      = voronoi_params{};
  auto spikenoise    = ""s;
  auto sparams      = spikenoise_params{};
 


  // parse command line
  auto cli = make_cli("yscenegen", "Make procedural scenes");
  add_option(cli, "--terrain", terrain, "terrain object");
  add_option(cli, "--displacement", displacement, "displacement object");
  add_option(cli, "--hair", hair, "hair object");
  add_option(cli, "--hairbase", hairbase, "hairbase object");
  add_option(cli, "--grass", grass, "grass object");
  add_option(cli, "--grassbase", grassbase, "grassbase object");
  add_option(cli, "--hairnum", hparams.num, "hair number");
  add_option(cli, "--hairlen", hparams.lenght, "hair length");
  add_option(cli, "--hairstr", hparams.strength, "hair strength");
  add_option(cli, "--hairgrav", hparams.gravity, "hair gravity");
  add_option(cli, "--hairstep", hparams.steps, "hair steps");
  add_option(cli, "--output,-o", output, "output scene");
  add_option(cli, "scene", filename, "input scene", true);
  add_option(cli, "--voronoi", voronoi, "voronoi object");
  add_option(cli, "--spikenoise", spikenoise, "spikenoise object");
  parse_cli(cli, argc, argv);

  // load scene
  auto scene_guard = std::make_unique<sceneio_scene>();
  auto scene       = scene_guard.get();
  auto ioerror     = ""s;
  if (!load_scene(filename, scene, ioerror, print_progress))
    print_fatal(ioerror);

  // create procedural geometry
  if (terrain != "") {
    make_terrain(scene, get_instance(scene, terrain), tparams);
  }
  if (displacement != "") {
    make_displacement(scene, get_instance(scene, displacement), dparams);
  }
  if (hair != "") {
    make_hair(scene, get_instance(scene, hairbase), get_instance(scene, hair),
        hparams);
  }
  if (grass != "") {
    auto grasses = vector<sceneio_instance*>{};
    for (auto instance : scene->instances)
      if (instance->name.find(grass) != scene->name.npos)
        grasses.push_back(instance);
    make_grass(scene, get_instance(scene, grassbase), grasses, gparams);
  }
  if (voronoi != "") {
    make_voronoi(scene, get_instance(scene, voronoi), vparams);
  }
  
  if (spikenoise != "") {
    make_spikenoise(scene, get_instance(scene, spikenoise), sparams);
  }

  // make a directory if needed
  if (!make_directory(path_dirname(output), ioerror)) print_fatal(ioerror);
  if (!scene->shapes.empty()) {
    if (!make_directory(path_join(path_dirname(output), "shapes"), ioerror))
      print_fatal(ioerror);
  }
  if (!scene->textures.empty()) {
    if (!make_directory(path_join(path_dirname(output), "textures"), ioerror))
      print_fatal(ioerror);
  }

  // save scene
  if (!save_scene(output, scene, ioerror, print_progress)) print_fatal(ioerror);

  // done
  return 0;
}
}
