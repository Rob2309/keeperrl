#pragma once

#include "fx_base.h"
#include "vertex.h"

namespace fx {

struct DrawBuffers {
  struct Element {
    int firstVertex;
    int numVertices;
    TextureName texName;
  };

  void clear();
  // TODO: span would be useful
  void add(const DrawParticle*, int count);
  bool empty() const {
    return elements.empty();
  }

  vector<Vertex> vertices;

  // TODO: group elements by TextureName
  vector<Element> elements;
};
}
