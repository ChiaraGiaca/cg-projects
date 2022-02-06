//
// Implementation for Yocto/Grade.
//

//
// LICENSE:
//
// Copyright (c) 2020 -- 2020 Fabio Pellacini
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

#include "yocto_colorgrade.h"

#include <yocto/yocto_color.h>
#include <yocto/yocto_sampling.h>

#include <iostream>
using namespace std;

// -----------------------------------------------------------------------------
// COLOR GRADING FUNCTIONS
// -----------------------------------------------------------------------------
namespace yocto {

image<vec4f> grade_image(const image<vec4f>& img, const grade_params& params) {
  // PUT YOUR CODE HERE
  auto graded = img;
  auto size   = graded.imsize();
  auto w      = size[0];
  auto h      = size[1];
  auto rng    = make_rng(172784);

  //ci metto di più a fare prima w e poi h -> cache misses
  for (int i = 0; i < w; i++) {
    for (int j = 0; j < h; j++) {
      auto c = xyz(graded[{i, j}]); //trovo il mio colore e ignoro l'opacità
      
      // Tone mapping
      c *= pow(2, params.exposure);  // exposure compensation
      if (params.filmic) {
        c *= 0.6;
        c = (pow(c, 2) * 2.51 + c * 0.03) /
            (pow(c, 2) * 2.43 + c * 0.59 + 0.14);  // filmic correction
      }
      if (params.srgb) {
        c = pow(c, 1 / 2.2);  // srgb color space
      }
      c = clamp(c, 0, 1);

      graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};

      // Color tint
      c              = c * params.tint;
      graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};

      // Saturation
      auto g         = (c.x + c.y + c.z) / 3;
      c              = g + (c - g) * (params.saturation * 2);
      graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};

      // Contrast
      c              = gain(c, 1 - params.contrast);
      graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};

      // Vignette
      auto vr    = 1 - params.vignette;
      auto ij    = vec2f{(float)j, (float)i};  // inizializzo ij e size1 come un vec2f
      auto size1 = vec2f{(float)h, (float)w} /
                   2;  
      float r = length(ij - size1) /
                length(size1);  /*posso fare le operazioni su vec2f dato che
                                length li prende come parametri*/

      c              = c * (1 - smoothstep(vr, 2 * vr, r));
      graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};

      // Film grain
      c              = c + (rand1f(rng) - 0.5) * params.grain;
      graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};

      // Mosaic effect
      if (params.mosaic != 0) {
        int i1         = i - i % params.mosaic; //aggiorno i valori degli indici
        int j1         = j - j % params.mosaic;
        c              = xyz(graded[{i1, j1}]);
        graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
      }
    }
  }
   //Grid effect
    for (int i = 0; i < w; i++) {
      for (int j = 0; j < h; j++) {
          /*messo in un altro for perchè interferisce con Mosaic,
          in quanto i due filtri agiscono non solo sul pixel attuale ma anche sui circostanti*/
          auto c = xyz(graded[{i, j}]);
        if (params.grid != 0) {
          if (i % params.grid == 0 || j % params.grid == 0) {
            c = c * 0.5;
          }
          graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
        }

      
      }
    }

    //FILTRI AGGIUNTIVI

     for (int i = 0; i < w; i++) {
      for (int j = 0; j < h; j++) {
        
        // Filtro seppia
        auto c = xyz(graded[{i, j}]);
        if (params.seppia) {
          auto oR = (c.x * .393) + (c.y * .769) + (c.z * .189);
          auto oG = (c.x * .349) + (c.y * .686) + (c.z * .168);
          auto oB = (c.x * .272) + (c.y * .534) + (c.z * .131);

          c              = gain(c, 1 - 0.6);
          graded[{i, j}] = vec4f{(float)oR, (float)oG, (float)oB, 1};
        }

        /*Sunset: ho cercato di giocare con le colorazioni rossastre e cupe del tramonto nella stanza, 
        insieme al riflesso del sole sugli oggetti e sulle
        piante vicino alla finestra*/
        if (params.sunset) {
          c *= params.exposure;
          c        = gain(c, 1 - params.contrast);
          auto var= (0.5*c.x + c.y + c.z) / 3;
          c      = var + (c - var) * (params.saturation * 2);
          
          graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
        
        }

        /*Vintage movie: ho creato questo effetto pensando ai colori soffusi e poco accesi dei film vintage*/
        if (params.vintage) {
          c              = gain(c, 1 - params.contrast / 2);
          auto g         = (c.x * 0.5 + c.y + c.z) / 2;
          c              = g + (c - g) * (params.saturation * 2.5);
          c              = c * params.tint;
          if (j >= 0 && j <= h /9) {
            c.x = 0;
            c.y = 0;
            c.z = 0;
          }
          if (j >= h - h/9 && j <= h) {
            c.x = 0;
            c.y = 0;
            c.z = 0;
          
          }
          graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
        }
       
        /*Grayscale and red: ho creato l'effetto volendo giocare sul rosso acceso sopra uno sfondo bianco e nero*/
        if (params.red) {
          auto val       = (c.x + c.y + c.z) / 3;
          c        = val + (c - val) * (0.7 * 2);
          if (c.x >= 0.40 && c.z <= 0.40 && c.y <= 0.40) {
            graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
          } 
          else {
            graded[{i, j}] = vec4f{val, val, val, 1};
          }
        }

      }
    }

     /*I quattro cicli for che seguono, fanno parte dello stesso filtro: ho modificato l'immagine in quattro parti diverse, 
     per creare un filtro Pop Art*/
  
     //giallo
     for (int i = 0; i < w/2; i++) {
       for (int j = 0; j < h/2; j++) {
         auto c = xyz(graded[{i, j}]);
         if (params.effect) {
           c = gain(c, 1 + params.contrast);
           auto g = (c.x + c.y + c.z) / 3;
           c      = g + (c - g) * 0; 
           if (c.x >= 0.2 && c.x <= 1 && c.y >= 0.2 && c.y <= 1 && c.z >= 0.2 && c.z <= 1) {
             c.x = 0.94;
             c.y = 0.82;
             c.z = 0.18;
           }
             else {
               c.x = 0;
               c.y = 0;
               c.z = 0;
             
           }
           
           graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
          }

        }
      }
     
     //azzurro
     for (int i = w/2; i < w; i++) {
       for (int j = h/2; j < h; j++) {
         auto c = xyz(graded[{i, j}]);
         if (params.effect) {
           c      = gain(c, 1 + params.contrast);
           auto g = (c.x + c.y + c.z) / 3;
           c      = g + (c - g) * 0;
           if (c.x >= 0.2 && c.x <= 1 && c.y >= 0.2 && c.y <= 1 && c.z >= 0.2 &&
               c.z <= 1) {
             c.x = 0.15;
             c.y = 0.73;
             c.z = 0.77;
           } else {
             c.x = 0;
             c.y = 0;
             c.z = 0;
           }

           graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
         }
       }
     }

     //verde
     for (int i = w / 2; i < w; i++) {
       for (int j = 0; j < h/2; j++) {
         auto c = xyz(graded[{i, j}]);
         if (params.effect) {
           c      = gain(c, 1 + params.contrast);
           auto g = (c.x + c.y + c.z) / 3;
           c      = g + (c - g) * 0;
           if (c.x >= 0.2 && c.x <= 1 && c.y >= 0.2 && c.y <= 1 && c.z >= 0.2 &&
               c.z <= 1) {
             c.x = 0.15;
             c.y = 0.78;
             c.z = 0.12;
           } else {
             c.x = 0;
             c.y = 0;
             c.z = 0;
           }

           graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
         }
       }
     }
    
     //rosso
     for (int i = 0; i < w/2; i++) {
       for (int j = h/2; j < h; j++) {
         auto c = xyz(graded[{i, j}]);
         if (params.effect) {
           c      = gain(c, 1 + params.contrast);
           auto g = (c.x + c.y + c.z) / 3;
           c      = g + (c - g) * 0; 
           if (c.x >= 0.2 && c.x <= 1 && c.y >= 0.2 && c.y <= 1 && c.z >= 0.2 &&
               c.z <= 1) {
             c.x = 1;
             c.y = 0.11;
             c.z = 0.32;
           } else {
             c.x = 0;
             c.y = 0;
             c.z = 0;
           }

           graded[{i, j}] = vec4f{c.x, c.y, c.z, 1};
         }
       }
     }



   
    return graded;
}

}  // namespace yocto