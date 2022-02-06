//
// Implementation for Yocto/Particle.
//

//
// LICENSE:
//
// Copyright (c) 2020 -- 2020 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//

#include "yocto_particle.h"

#include <yocto/yocto_geometry.h>
#include <yocto/yocto_sampling.h>
#include <yocto/yocto_shape.h>

#include <unordered_set>

#include <stdexcept> /*per risolvere il problema di invalid argument*/

// -----------------------------------------------------------------------------
// SIMULATION DATA AND API
// -----------------------------------------------------------------------------
namespace yocto {

// cleanup
particle_scene::~particle_scene() {
  for (auto shape : shapes) delete shape;
  for (auto collider : colliders) delete collider;
}

// Scene creation
particle_shape* add_shape(particle_scene* scene) {
  return scene->shapes.emplace_back(new particle_shape{});
}
particle_collider* add_collider(particle_scene* scene) {
  return scene->colliders.emplace_back(new particle_collider{});
}
particle_shape* add_particles(particle_scene* scene, const vector<int>& points,
    const vector<vec3f>& positions, const vector<float>& radius, float mass,
    float random_velocity) {
  auto shape               = add_shape(scene);
  shape->points            = points;
  shape->initial_positions = positions;
  shape->initial_normals.assign(shape->positions.size(), {0, 0, 1});
  shape->initial_radius = radius;
  shape->initial_invmass.assign(
      positions.size(), 1 / (mass * positions.size()));
  shape->initial_velocities.assign(positions.size(), {0, 0, 0});
  shape->emit_rngscale = random_velocity;
  // avoid crashes
  shape->positions = shape->initial_positions;
  shape->normals   = shape->normals;
  shape->radius    = shape->initial_radius;
  return shape;
}
particle_shape* add_cloth(particle_scene* scene, const vector<vec4i>& quads,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<float>& radius, float mass, float coeff,
    const vector<int>& pinned) {
  auto shape               = add_shape(scene);
  shape->quads             = quads;
  shape->initial_positions = positions;
  shape->initial_normals   = normals;
  shape->initial_radius    = radius;
  shape->initial_invmass.assign(
      positions.size(), 1 / (mass * positions.size()));
  shape->initial_velocities.assign(positions.size(), {0, 0, 0});
  shape->initial_pinned = pinned;
  shape->spring_coeff   = coeff;
  // avoid crashes
  shape->positions = shape->initial_positions;
  shape->normals   = shape->normals;
  shape->radius    = shape->initial_radius;
  return shape;
}
particle_collider* add_collider(particle_scene* scene,
    const vector<vec3i>& triangles, const vector<vec4i>& quads,
    const vector<vec3f>& positions, const vector<vec3f>& normals,
    const vector<float>& radius) {
  auto collider       = add_collider(scene);
  collider->quads     = quads;
  collider->triangles = triangles;
  collider->positions = positions;
  collider->normals   = normals;
  collider->radius    = radius;
  return collider;
}

// Set shapes
void set_velocities(
    particle_shape* shape, const vec3f& velocity, float random_scale) {
  shape->emit_velocity = velocity;
  shape->emit_rngscale = random_scale;
}

// Get shape properties
void get_positions(particle_shape* shape, vector<vec3f>& positions) {
  positions = shape->positions;
}
void get_normals(particle_shape* shape, vector<vec3f>& normals) {
  normals = shape->normals;
}

}  // namespace yocto


// -----------------------------------------------------------------------------
// SIMULATION DATA AND API
// -----------------------------------------------------------------------------
namespace yocto {

// Init simulation
void init_simulation(particle_scene* scene, const particle_params& params) {
  auto rng = make_rng(params.seed);
  /*COPY INITIAL VALUES*/
  for (auto& shape : scene->shapes) {
    shape->positions  = shape->initial_positions;
    shape->normals    = shape->initial_normals;
    shape->velocities = shape->initial_velocities;
    shape->invmass    = shape->initial_invmass;
    shape->radius     = shape->initial_radius;
    auto init_pinned  = shape->initial_pinned;

    /*inizializzo il vettore delle forze*/
    for (int i = 0; i < shape->invmass.size(); i++) {
      shape->forces.push_back({0, 0, 0});
    }

    /*SETUP PINNED*/
    for (auto& vertex : init_pinned) {
      shape->invmass[vertex] = 0;
    }

    /*INITIALIZE VELOCITIES*/
    for (int i = 0; i < shape->velocities.size(); i++) {
      shape->velocities[i] += sample_sphere(rand2f(rng)) *
                              (shape->emit_rngscale) * rand1f(rng);
    }

    /*MAKE SPRINGS*/
    if (shape->spring_coeff > 0) {
      if (!shape->quads.empty()) {
        for (auto& edge : get_edges(shape->quads)) {
          shape->springs.push_back({edge.x, edge.y,
              distance(shape ->positions[edge.x], shape ->positions[edge.y]),
              shape->spring_coeff});
        }
        /*make diagonal*/
        for (auto& quad : shape->quads) {
          shape->springs.push_back({quad.x, quad.z,
              distance(shape->positions[quad.x], shape->positions[quad.z]),
              shape->spring_coeff});
          shape->springs.push_back({quad.w, quad.y,
              distance(shape->positions[quad.w], shape->positions[quad.y]),
              shape->spring_coeff});
        }

      } else if (!shape->triangles.empty()) {
        for (auto& edge : get_edges(shape->triangles)) {
          shape->springs.push_back({edge.x, edge.y,
              distance(shape ->positions[edge.x], shape ->positions[edge.y]),
              shape->spring_coeff});
        }
      }
    }

    /*INITIALIZE COLLIDERS BVH: costruisco il bvh*/
    for (auto& collider : scene->colliders) {
      if (!collider->quads.empty()) {
        collider->bvh = make_quads_bvh(
            collider->quads, collider->positions, collider->radius);
      } else if (!collider->triangles.empty()) {
        collider->bvh = make_triangles_bvh(
            collider->triangles, collider->positions, collider->radius);
      }
    }
  }
}


// check if a point is inside a collider
bool collide_collider(particle_collider* collider, const vec3f& position,
    vec3f& hit_position, vec3f& hit_normal) {
  auto ray = ray3f{position, vec3f{0, 1, 0}};
  if (!collider->quads.empty()) {
    auto isec = intersect_quads_bvh(
        collider->bvh, collider->quads, collider->positions, ray);
    if (!isec.hit) return false;
    auto q       = collider->quads[isec.element];
    hit_position = interpolate_quad(collider->positions[q.x],
        collider->positions[q.y], collider->positions[q.z],
        collider->positions[q.w], isec.uv);
    hit_normal   = normalize(
        interpolate_quad(collider->normals[q.x], collider->normals[q.y],
            collider->normals[q.z], collider->normals[q.w], isec.uv));

  } else if (!collider->triangles.empty()) {
    auto isec = intersect_triangles_bvh(
        collider->bvh, collider->triangles, collider->positions, ray);
    if (!isec.hit) return false;
    auto t       = collider->triangles[isec.element];
    hit_position = interpolate_triangle(collider->positions[t.x],
        collider->positions[t.y], collider->positions[t.z], isec.uv);
    hit_normal   = normalize(interpolate_triangle(collider->normals[t.x],
        collider->normals[t.y], collider->normals[t.z], isec.uv));
  }

  return dot(hit_normal, ray.d) > 0;
}

// simulate mass-spring
void simulate_massspring(particle_scene* scene, const particle_params& params) {
  /*SAVE OLD POSITIONS*/
  for (auto& particle : scene->shapes) {
    particle->old_positions = particle->positions;
  }
  /*COMPUTE DYNAMICS*/
  for (auto s = 0; s < params.mssteps; s++) {
    auto ddt = params.deltat / params.mssteps;
    /*compute forces*/
    for (auto& particle : scene->shapes) {
      /*ciclo sulla size di invmass per prendere gli indici di tutti i vettori*/
      for (int i = 0; i < particle->invmass.size(); i++) {
        if (particle->invmass[i] != 0) {
          particle->forces[i] = vec3f{0, -params.gravity, 0} /
                                particle->invmass[i];
        }
        /*per il vento, inserisco un flag di controllo e cambio le forze, 
        settando a un valore pari a 4 l'intensità del vento*/
        if (params.flag) {
          particle->forces[i] = vec3f{4, 0, 0} /
                                particle->invmass[i];
        }

      }

      for (auto& spring : particle->springs) {
        auto& particle0 = spring.vert0;
        auto& particle1 = spring.vert1;

        auto invmass = particle->invmass[particle0] +
                       particle->invmass[particle1];
        if (!invmass) continue;
        auto delta_pos = particle->positions[particle1] -
                         particle->positions[particle0];
        auto spring_dir = normalize(delta_pos);
        auto spring_len = length(delta_pos);
        auto force      = spring_dir * (spring_len / spring.rest - 1) /
                     (spring.coeff * invmass);  // we take invcoeff in [0,1]

        auto delta_vel = particle->velocities[particle1] -
                         particle->velocities[particle0];
        force += dot(delta_vel / spring.rest, spring_dir) * spring_dir /
                 (spring.coeff * 1000 * invmass);
        particle->forces[particle0] += force;
        particle->forces[particle1] -= force;
      }
    }

    /*update state*/
    for (auto particle : scene->shapes) {
      /*update velocity and positions using Euler's method*/
      for (int i = 0; i < particle->invmass.size(); i++) {
        if (particle->invmass[i] != 0) {
          particle->velocities[i] += ddt * particle->forces[i] *
                                     particle->invmass[i];
          particle->positions[i] += ddt * particle->velocities[i];
        }
        
      }
    }
  }
  /*HANDLE COLLISIONS*/
  for (auto particle : scene->shapes) {
      for (auto i = 0; i < particle->invmass.size(); i++) {
      if (!particle->invmass[i]) continue;
      for (auto collider : scene->colliders) {
        auto hitpos     = zero3f;
        auto hit_normal = zero3f;

        if (collide_collider(
                collider, particle->positions[i], hitpos, hit_normal)) {
          particle->positions[i] = hitpos + hit_normal * 0.005;   
          auto projection        = dot(particle->velocities[i], hit_normal);
          particle->velocities[i] =
              (particle->velocities[i] - projection * hit_normal) *
                  (1 - params.bounce.x) -
              projection * hit_normal * (1 - params.bounce.y);
          
        }
      }
    }
  }
  // VELOCITY FILTER
  for (auto& particle : scene->shapes) {
    for (int i = 0; i < particle->invmass.size(); i++) {
      if (!particle->invmass[i]) continue;
      particle->velocities[i] *= (1 - params.dumping * params.deltat);
      /*damping*/
      if (length(particle->velocities[i]) < params.minvelocity) /*sleeping*/
        particle->velocities[i] = {0, 0, 0};
    }
  }
  // RECOMPUTE NORMALS
  for (auto& particle : scene->shapes) {
    if (!particle->quads.empty()) {
      particle->normals = compute_normals(particle->quads, particle->positions);
    } else if (!particle->triangles.empty()) {
      particle->normals = compute_normals(
          particle->triangles, particle->positions);
    }
  }
 
}


// simulate pbd
void simulate_pbd(particle_scene* scene, const particle_params& params) {
  /*SAVE OLD POSITIONS*/
  for (auto& particle : scene->shapes) {
    particle->old_positions = particle->positions;

    /*PREDICT POSITIONS*/
    for (int i = 0; i < particle->invmass.size(); i++) {
      if (!particle->invmass[i]) continue;
      particle->velocities[i] += vec3f{0, -params.gravity, 0} * params.deltat;
      particle->positions[i] += particle->velocities[i] * params.deltat;
    }

    /*COMPUTE COLLISIONS*/
    particle->collisions.clear();
    for (int i = 0; i < particle->invmass.size(); i++) {
      if (!particle->invmass[i]) continue;
      for (auto collider : scene->colliders) {
        auto hit_position = zero3f, hit_normal = zero3f;
        if (!collide_collider(
                collider, particle->positions[i], hit_position, hit_normal)) {
          continue;
        }
        particle->collisions.push_back({i, hit_position, hit_normal});
      }
    }
    // SOLVE CONSTRAINTS (vincoli)
    for (int i = 0; i < params.pdbsteps; i++) {
      for (auto& spring : particle->springs) {
        auto particle0 = spring.vert0;
        auto particle1 = spring.vert1;  
        auto invmass   = particle->invmass[particle0] +
                       particle->invmass[particle1];
        if (!invmass) continue;
        auto dir = particle->positions[particle1] -
                   particle->positions[particle0];
        auto len = length(dir);
        dir /= len;
        auto lambda = (1 - spring.coeff) * (len - spring.rest) / invmass;
        particle->positions[particle0] += particle->invmass[particle0] *
                                          lambda * dir;
        particle->positions[particle1] -= particle->invmass[particle1] *
                                          lambda * dir;
      }
      for (auto& collision : particle->collisions) {
        auto particle1 = collision.vert;
        if (!particle->invmass[particle1]) continue;
        auto projection = dot(
            particle->positions[particle1] - collision.position,
            collision.normal);
        if (projection >= 0) continue;
        particle->positions[particle1] += -projection * collision.normal;
      }
    }

    // COMPUTE VELOCITIES
    for (int i = 0; i < particle->invmass.size(); i++) {
      if (!particle->invmass[i]) continue;
      particle->velocities[i] =
          (particle->positions[i] - particle->old_positions[i]) / params.deltat;
    }

    // VELOCITY FILTER
    for (int i = 0; i < particle->invmass.size(); i++) {
      if (!particle->invmass[i]) continue;
      particle->velocities[i] *= (1 - params.dumping * params.deltat);
      if (length(particle->velocities[i]) < params.minvelocity) {
        particle->velocities[i] = {0, 0, 0};
      }
    }
    // RECOMPUTE NORMALS
    if (!particle->quads.empty()) {
      particle->normals = compute_normals(particle->quads, particle->positions);
    } else if (!particle->triangles.empty()) {
      particle->normals = compute_normals(
          particle->triangles, particle->positions);
    }
  }

}

// Simulate one step
void simulate_frame(particle_scene* scene, const particle_params& params) {
  switch (params.solver) {
    case particle_solver_type::mass_spring:
      return simulate_massspring(scene, params);
    case particle_solver_type::position_based:
      return simulate_pbd(scene, params);
    default: throw std::invalid_argument("unknown solver");
  }
}

// Simulate the whole sequence
void simulate_frames(particle_scene* scene, const particle_params& params,
    progress_callback progress_cb) {
  // handle progress
  auto progress = vec2i{0, 1 + (int)params.frames};

  if (progress_cb) progress_cb("init simulation", progress.x++, progress.y);
  init_simulation(scene, params);

  for (auto idx = 0; idx < params.frames; idx++) {
    if (progress_cb) progress_cb("simulate frames", progress.x++, progress.y);
    simulate_frame(scene, params);
  }

  if (progress_cb) progress_cb("simulate frames", progress.x++, progress.y);
}

}  // namespace yocto
