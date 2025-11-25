# Install script for directory: C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Assimp")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "libassimp6.0.2-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/lib/Debug/assimp-vc145-mtd.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/lib/Release/assimp-vc145-mt.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/lib/MinSizeRel/assimp-vc145-mt.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/lib/RelWithDebInfo/assimp-vc145-mt.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "libassimp6.0.2" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/bin/Debug/assimp-vc145-mtd.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/bin/Release/assimp-vc145-mt.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/bin/MinSizeRel/assimp-vc145-mt.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/bin/RelWithDebInfo/assimp-vc145-mt.dll")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "assimp-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/assimp" TYPE FILE FILES
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/anim.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/aabb.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/ai_assert.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/camera.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/color4.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/color4.inl"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/code/../include/assimp/config.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/ColladaMetaData.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/commonMetaData.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/defs.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/cfileio.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/light.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/material.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/material.inl"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/matrix3x3.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/matrix3x3.inl"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/matrix4x4.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/matrix4x4.inl"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/mesh.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/ObjMaterial.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/pbrmaterial.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/GltfMaterial.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/postprocess.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/quaternion.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/quaternion.inl"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/scene.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/metadata.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/texture.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/types.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/vector2.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/vector2.inl"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/vector3.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/vector3.inl"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/version.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/cimport.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/AssertHandler.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/importerdesc.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Importer.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/DefaultLogger.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/ProgressHandler.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/IOStream.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/IOSystem.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Logger.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/LogStream.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/NullLogger.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/cexport.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Exporter.hpp"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/DefaultIOStream.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/DefaultIOSystem.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/ZipArchiveIOSystem.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/SceneCombiner.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/fast_atof.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/qnan.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/BaseImporter.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Hash.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/MemoryIOWrapper.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/ParsingUtils.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/StreamReader.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/StreamWriter.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/StringComparison.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/StringUtils.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/SGSpatialSort.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/GenericProperty.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/SpatialSort.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/SkeletonMeshBuilder.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/SmallVector.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/SmoothingGroups.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/SmoothingGroups.inl"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/StandardShapes.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/RemoveComments.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Subdivision.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Vertex.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/LineSplitter.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/TinyFormatter.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Profiler.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/LogAux.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Bitmap.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/XMLTools.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/IOStreamBuffer.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/CreateAnimMesh.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/XmlParser.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/BlobIOSystem.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/MathFunctions.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Exceptional.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/ByteSwapper.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Base64.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "assimp-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/assimp/Compiler" TYPE FILE FILES
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Compiler/pushpack1.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Compiler/poppack1.h"
    "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/code/../include/assimp/Compiler/pstdint.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/bin/Debug/assimp-vc145-mtd.pdb")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/bin/Release/assimp-vc145-mt.pdb")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/bin/MinSizeRel/assimp-vc145-mt.pdb")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/bin/RelWithDebInfo/assimp-vc145-mt.pdb")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/Unoryuto/Downloads/assimp-6.0.2/assimp-6.0.2/bulid/fbxbuild/code/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
