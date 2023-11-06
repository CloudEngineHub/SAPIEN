#include "sapien/component/physx/mesh.h"
#include "../../logger.h"
#include "sapien/component/physx/physx_system.h"
#include <filesystem>
#include <set>

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace fs = std::filesystem;

namespace sapien {
namespace component {

//////////////////// helpers ////////////////////

static Vertices loadVerticesFromMeshFile(std::string const &filename) {
  std::vector<float> vertices;
  Assimp::Importer importer;
  uint32_t flags = aiProcess_Triangulate | aiProcess_PreTransformVertices;
  importer.SetPropertyBool(AI_CONFIG_IMPORT_COLLADA_IGNORE_UP_DIRECTION, true);

  const aiScene *scene = importer.ReadFile(filename, flags);

  if (!scene) {
    logger::error(importer.GetErrorString());
    return {};
  }

  for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
    auto mesh = scene->mMeshes[i];
    for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
      auto vertex = mesh->mVertices[v];
      vertices.push_back(vertex.x);
      vertices.push_back(vertex.y);
      vertices.push_back(vertex.z);
    }
  }
  return Eigen::Map<Vertices>(vertices.data(), vertices.size() / 3, 3);
}

static std::tuple<Vertices, Triangles>
loadVerticesAndTrianglesFromMeshFile(std::string const &filename) {
  std::vector<float> vertices;
  std::vector<uint32_t> triangles;
  Assimp::Importer importer;
  uint32_t flags = aiProcess_Triangulate | aiProcess_PreTransformVertices;
  importer.SetPropertyInteger(AI_CONFIG_PP_PTV_ADD_ROOT_TRANSFORMATION, 1);

  const aiScene *scene = importer.ReadFile(filename, flags);

  if (!scene) {
    logger::error(importer.GetErrorString());
    throw std::runtime_error("failed to load scene from " + filename);
  }

  uint32_t vertexCount = 0;
  for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
    auto mesh = scene->mMeshes[i];
    for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
      auto vertex = mesh->mVertices[v];
      vertices.push_back(vertex.x);
      vertices.push_back(vertex.y);
      vertices.push_back(vertex.z);
    }
    for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
      for (uint32_t j = 0; j + 2 < mesh->mFaces[f].mNumIndices; ++j) {
        triangles.push_back(mesh->mFaces[f].mIndices[j] + vertexCount);
        triangles.push_back(mesh->mFaces[f].mIndices[j + 1] + vertexCount);
        triangles.push_back(mesh->mFaces[f].mIndices[j + 2] + vertexCount);
      }
    }
    vertexCount += mesh->mNumVertices;
  }
  Vertices vs = Eigen::Map<Vertices>(vertices.data(), vertices.size() / 3, 3);
  Triangles ts = Eigen::Map<Triangles>(triangles.data(), triangles.size() / 3, 3);

  return {vs, ts};
}

std::vector<std::vector<int>> splitMesh(aiMesh *mesh) {
  // build adjacency list
  logger::info("splitting mesh with {} vertices", mesh->mNumVertices);
  std::vector<std::set<int>> adj(mesh->mNumVertices);
  for (uint32_t i = 0; i < mesh->mNumFaces; ++i) {
    for (uint32_t j = 0; j < mesh->mFaces[i].mNumIndices; ++j) {
      uint32_t a = mesh->mFaces[i].mIndices[j];
      uint32_t b = mesh->mFaces[i].mIndices[(j + 1) % mesh->mFaces[i].mNumIndices];
      adj[a].insert(b);
      adj[b].insert(a);
    }
  }

  std::vector<std::vector<int>> groups;
  std::vector<int> visited(mesh->mNumVertices, 0);
  for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
    if (visited[i]) {
      continue;
    }

    // new group
    groups.emplace_back();
    groups.back().push_back(i);

    // traverse group
    visited[i] = 1;
    std::vector<int> q;
    q.push_back(i);
    while (!q.empty()) {
      int n = q.back();
      q.pop_back();
      for (auto m : adj[n]) {
        if (!visited[m]) {
          visited[m] = 1;
          groups.back().push_back(m);
          q.push_back(m);
        }
      }
    }
  }

  return groups;
}

static std::vector<Vertices> loadComponentVerticesFromMeshFileObj(std::string const &filename) {
  std::vector<Vertices> result;

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(filename, tinyobj::ObjReaderConfig())) {
    if (!reader.Error().empty()) {
      throw std::runtime_error("TinyObjReader: " + reader.Error());
    }
  }

  if (!reader.Warning().empty()) {
    logger::warn("TinyObjReader: {}", reader.Warning());
  }

  auto &attrib = reader.GetAttrib();
  auto &shapes = reader.GetShapes();

  // Loop over shapes
  for (size_t s = 0; s < shapes.size(); s++) {

    std::vector<float> vertices;

    // Loop over faces(polygon)
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

      // Loop over vertices in the face.
      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
        tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
        tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
        tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
        vertices.push_back(vx);
        vertices.push_back(vy);
        vertices.push_back(vz);
      }
      index_offset += fv;
    }
    result.push_back(Eigen::Map<Vertices>(vertices.data(), vertices.size() / 3, 3));
  }
  return result;
}

static std::vector<Vertices> loadComponentVerticesFromMeshFileAssimp(std::string const &filename) {
  Assimp::Importer importer;

  uint32_t flags =
      aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_RemoveComponent;

  importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
                              aiComponent_NORMALS | aiComponent_TEXCOORDS | aiComponent_COLORS |
                                  aiComponent_TANGENTS_AND_BITANGENTS | aiComponent_MATERIALS |
                                  aiComponent_TEXTURES);

  importer.SetPropertyBool(AI_CONFIG_IMPORT_COLLADA_IGNORE_UP_DIRECTION, true);
  const aiScene *scene = importer.ReadFile(filename, flags);
  if (!scene) {
    logger::error(importer.GetErrorString());
    return {};
  }
  logger::info("Found {} meshes", scene->mNumMeshes);

  std::vector<Vertices> result;
  for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
    auto mesh = scene->mMeshes[i];
    auto vertexGroups = splitMesh(mesh);

    logger::info("Decomposed mesh {} into {} components", i + 1, vertexGroups.size());
    for (auto &g : vertexGroups) {
      logger::info("vertex count: {}", g.size());
      std::vector<float> vertices;
      for (auto v : g) {
        auto vertex = mesh->mVertices[v];
        vertices.push_back(vertex.x);
        vertices.push_back(vertex.y);
        vertices.push_back(vertex.z);
      }

      result.push_back(Eigen::Map<Vertices>(vertices.data(), vertices.size() / 3, 3));
    }
  }
  return result;
}

static std::vector<Vertices>
loadComponentVerticesFromMeshFile(std::string const &filename) {
  if (filename.ends_with(".obj") || filename.ends_with(".OBJ")) {
    return loadComponentVerticesFromMeshFileObj(filename);
  }
  return loadComponentVerticesFromMeshFileAssimp(filename);
}

//////////////////// helpers end ////////////////////

void PhysxConvexMesh::loadMesh(Vertices const &vertices) {
  mEngine = PhysxEngine::Get();

  PxConvexMeshDesc convexDesc;
  convexDesc.points.count = vertices.rows();
  convexDesc.points.stride = sizeof(float) * 3;
  convexDesc.points.data = vertices.data();
  convexDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;
  convexDesc.vertexLimit = 255;

  PxDefaultMemoryOutputStream buf;
  if (!PxCookConvexMesh(PxCookingParams(mEngine->getPxPhysics()->getTolerancesScale()), convexDesc,
                        buf)) {
    throw std::runtime_error("failed to add convex mesh from vertices");
  }

  PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
  mMesh = mEngine->getPxPhysics()->createConvexMesh(input);
}

PhysxConvexMesh::PhysxConvexMesh(Vertices const &vertices) { loadMesh(vertices); }

PhysxConvexMesh::PhysxConvexMesh(Vertices const &vertices, std::string const &filename)
    : PhysxConvexMesh(vertices) {
  mFilename = filename;
}

PhysxConvexMesh::PhysxConvexMesh(Vertices const &vertices, std::string const &filename, int part)
    : PhysxConvexMesh(vertices) {
  mFilename = filename;
  mPart = part;
}

PhysxConvexMesh::PhysxConvexMesh(std::string const &filename)
    : PhysxConvexMesh(loadVerticesFromMeshFile(filename), filename) {}

Vertices PhysxConvexMesh::getVertices() const {
  std::vector<float> vertices;
  vertices.reserve(mMesh->getNbVertices());
  auto gv = mMesh->getVertices();
  for (uint32_t i = 0; i < mMesh->getNbVertices(); ++i) {
    vertices.push_back(gv[i].x);
    vertices.push_back(gv[i].y);
    vertices.push_back(gv[i].z);
  }
  return Eigen::Map<Vertices>(vertices.data(), vertices.size() / 3, 3);
}

std::vector<std::shared_ptr<PhysxConvexMesh>>
PhysxConvexMesh::LoadByConnectedParts(std::string const &filename) {
  std::vector<std::shared_ptr<PhysxConvexMesh>> result;
  auto parts = loadComponentVerticesFromMeshFile(filename);
  for (uint32_t i = 0; i < parts.size(); ++i) {
    try {
      result.push_back(std::make_shared<PhysxConvexMesh>(parts[i], filename, i));
    } catch (std::runtime_error &err) {
      // PhysX should be giving a critical error already
      logger::warn("failed to load a component from file " + filename);
    }
  }

  if (result.size() == 0) {
    throw std::runtime_error(
        "failed to load mesh file " + filename +
        ". All connected components of the mesh are invalid collision shapes.");
  }

  return result;
}

Triangles PhysxConvexMesh::getTriangles() const {
  std::vector<uint32_t> indices;
  auto gi = mMesh->getIndexBuffer();
  for (uint32_t i = 0; i < mMesh->getNbPolygons(); ++i) {
    physx::PxHullPolygon polygon;
    mMesh->getPolygonData(i, polygon);
    for (int j = 0; j < int(polygon.mNbVerts) - 2; ++j) {
      indices.push_back(gi[polygon.mIndexBase]);
      indices.push_back(gi[polygon.mIndexBase + j + 1]);
      indices.push_back(gi[polygon.mIndexBase + j + 2]);
    }
  }
  return Eigen::Map<Triangles>(indices.data(), indices.size() / 3, 3);
}

void PhysxTriangleMesh::loadMesh(Vertices const &vertices, Triangles const &triangles) {
  mEngine = PhysxEngine::Get();

  PxTriangleMeshDesc meshDesc;
  meshDesc.points.count = vertices.rows();
  meshDesc.points.stride = sizeof(float) * 3;
  meshDesc.points.data = vertices.data();

  meshDesc.triangles.count = triangles.rows();
  meshDesc.triangles.stride = sizeof(uint32_t) * 3;
  meshDesc.triangles.data = triangles.data();

  PxDefaultMemoryOutputStream writeBuffer;
  if (!PxCookTriangleMesh(PxCookingParams(mEngine->getPxPhysics()->getTolerancesScale()), meshDesc,
                          writeBuffer)) {
    throw std::runtime_error("Failed to cook non-convex mesh");
  }
  PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
  mMesh = mEngine->getPxPhysics()->createTriangleMesh(readBuffer);
}

PhysxTriangleMesh::PhysxTriangleMesh(Vertices const &vertices, Triangles const &triangles) {
  loadMesh(vertices, triangles);
}

PhysxTriangleMesh::PhysxTriangleMesh(Vertices const &vertices, Triangles const &triangles,
                                     std::string const &filename)
    : PhysxTriangleMesh(vertices, triangles) {
  mFilename = filename;
}

PhysxTriangleMesh::PhysxTriangleMesh(std::string const &filename) {
  auto [vertices, triangles] = loadVerticesAndTrianglesFromMeshFile(filename);
  loadMesh(vertices, triangles);
  mFilename = filename;
}

Vertices PhysxTriangleMesh::getVertices() const {
  std::vector<float> vertices;
  vertices.reserve(3 * mMesh->getNbVertices());
  auto gv = mMesh->getVertices();
  for (uint32_t i = 0; i < mMesh->getNbVertices(); ++i) {
    vertices.push_back(gv[i].x);
    vertices.push_back(gv[i].y);
    vertices.push_back(gv[i].z);
  }
  return Eigen::Map<Vertices>(vertices.data(), vertices.size() / 3, 3);
}

Triangles PhysxTriangleMesh::getTriangles() const {
  std::vector<uint32_t> indices;
  indices.reserve(3 * mMesh->getNbTriangles());
  if (mMesh->getTriangleMeshFlags() & physx::PxTriangleMeshFlag::e16_BIT_INDICES) {
    auto gi = static_cast<const uint16_t *>(mMesh->getTriangles());
    for (uint32_t i = 0; i < mMesh->getNbTriangles(); ++i) {
      indices.push_back(gi[3 * i]);
      indices.push_back(gi[3 * i + 1]);
      indices.push_back(gi[3 * i + 2]);
    }
  } else {
    auto gi = static_cast<const uint32_t *>(mMesh->getTriangles());
    for (uint32_t i = 0; i < mMesh->getNbTriangles(); ++i) {
      indices.push_back(gi[3 * i]);
      indices.push_back(gi[3 * i + 1]);
      indices.push_back(gi[3 * i + 2]);
    }
  }

  return Eigen::Map<Triangles>(indices.data(), indices.size() / 3, 3);
}

static std::weak_ptr<PhysxConvexMesh> gCylinder;
std::shared_ptr<PhysxConvexMesh> PhysxConvexMesh::CreateCylinder() {
  auto c = gCylinder.lock();
  if (c) {
    return c;
  }
  constexpr int segments = 32;
  Vertices vertices;
  vertices.resize(segments * 2, 3);
  float step = std::numbers::pi_v<float> * 2.f / segments;
  for (int i = 0; i < segments; ++i) {
    vertices.row(2 * i) << 1.f, std::cos(step * i), std::sin(step * i);
    vertices.row(2 * i + 1) << -1.f, std::cos(step * i), std::sin(step * i);
  }
  gCylinder = c = std::make_shared<PhysxConvexMesh>(vertices);
  return c;
}

} // namespace component
} // namespace sapien
