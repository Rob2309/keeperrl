#include "fx_draw_buffers.h"

#include "fx_vec.h"
#include "fx_particle_system.h"

namespace fx {

void DrawBuffers::clear() {
  vertices.clear();
  elements.clear();
}

void DrawBuffers::add(const DrawParticle* particles, int count) {
  if (count == 0 || !particles)
    return;

  PROFILE;
  auto& first = particles[0];
  if (elements.empty())
    elements.emplace_back(Element{0, 0, first.texName});

  // TODO: sort particles by texture ?
  for (int n = 0; n < count; n++) {
    auto& quad = particles[n];
    auto* last = &elements.back();
    if (last->texName != quad.texName) {
      Element new_elem{(int)vertices.size(), 0, quad.texName};
      elements.emplace_back(new_elem);
      last = &elements.back();
    }
    last->numVertices += 6;

    vertices.push_back(Vertex { { quad.positions[0].x, quad.positions[0].y }, { quad.texCoords[0].x, quad.texCoords[0].y }, { quad.color.r, quad.color.g, quad.color.b, quad.color.a } });
    vertices.push_back(Vertex { { quad.positions[1].x, quad.positions[1].y }, { quad.texCoords[1].x, quad.texCoords[1].y }, { quad.color.r, quad.color.g, quad.color.b, quad.color.a } });
    vertices.push_back(Vertex { { quad.positions[2].x, quad.positions[2].y }, { quad.texCoords[2].x, quad.texCoords[2].y }, { quad.color.r, quad.color.g, quad.color.b, quad.color.a } });
    
    vertices.push_back(Vertex { { quad.positions[2].x, quad.positions[2].y }, { quad.texCoords[2].x, quad.texCoords[2].y }, { quad.color.r, quad.color.g, quad.color.b, quad.color.a } });
    vertices.push_back(Vertex { { quad.positions[0].x, quad.positions[0].y }, { quad.texCoords[0].x, quad.texCoords[0].y }, { quad.color.r, quad.color.g, quad.color.b, quad.color.a } });
    vertices.push_back(Vertex { { quad.positions[3].x, quad.positions[3].y }, { quad.texCoords[3].x, quad.texCoords[3].y }, { quad.color.r, quad.color.g, quad.color.b, quad.color.a } });
  }
}
}
