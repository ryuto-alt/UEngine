#include "MeshRenderer.h"

namespace UnoEngine {

MeshRenderer::MeshRenderer(Mesh* mesh, Material* material)
    : mesh_(mesh)
    , material_(material) {
}

} // namespace UnoEngine
