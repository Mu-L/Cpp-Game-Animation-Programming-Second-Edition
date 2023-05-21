/* glTF model, ready to draw */
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <vulkan/vulkan.h>
#include <tiny_gltf.h>

#include "Texture.h"
#include "GltfNode.h"

#include "VkRenderData.h"

class GltfModel {
  public:
    bool loadModel(VkRenderData &renderData, VkGltfRenderData& gltfRenderData,
      std::string modelFilename, std::string textureFilename);
    void draw(VkRenderData &renderData, VkGltfRenderData& gltfRenderData);
    void cleanup(VkRenderData &renderData, VkGltfRenderData& gltfRenderData);

    void uploadVertexBuffers(VkRenderData& renderData, VkGltfRenderData& gltfRenderData);
    void applyVertexSkinning(VkRenderData &renderData, VkGltfRenderData& gltfRenderData);
    void uploadIndexBuffer(VkRenderData& renderData, VkGltfRenderData& gltfRenderData);
    std::shared_ptr<VkMesh> getSkeleton(bool enableSkinning);
    int getJointMatrixSize();
    std::vector<glm::mat4> getJointMatrices();


private:
    void createVertexBuffers(VkRenderData& renderData, VkGltfRenderData& gltfRenderData);
    void createIndexBuffer(VkRenderData& renderData, VkGltfRenderData& gltfRenderData);
    int getTriangleCount();

    void getSkeletonPerNode(std::shared_ptr<GltfNode> treeNode, bool enableSkinning);

    void getJointData();
    void getWeightData();
    void getInvBindMatrices();
    void getNodes(std::shared_ptr<GltfNode> treeNode);
    void getNodeData(std::shared_ptr<GltfNode> treeNode, glm::mat4 parentNodeMatrix);

    std::vector<glm::tvec4<uint16_t>> mJointVec{};
    std::vector<glm::vec4> mWeightVec{};
    std::vector<glm::mat4> mInverseBindMatrices{};
    std::vector<glm::mat4> mJointMatrices{};

    std::vector<int> mAttribAccessors{};
    std::vector<int> mNodeToJoint{};

    std::vector<glm::vec3> mAlteredPositions{};

    std::shared_ptr<GltfNode> mRootNode = nullptr;

    std::shared_ptr<tinygltf::Model> mModel = nullptr;

    std::shared_ptr<VkMesh> mSkeletonMesh = nullptr;

    std::map<std::string, GLint> attributes =
      {{"POSITION", 0}, {"NORMAL", 1}, {"TEXCOORD_0", 2}, {"JOINTS_0", 3}, {"WEIGHTS_0", 4}};
};
