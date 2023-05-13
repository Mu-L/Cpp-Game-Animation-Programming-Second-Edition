#include <algorithm>

#include <imgui_impl_glfw.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/spline.hpp>

#include "OGLRenderer.h"
#include "Logger.h"

OGLRenderer::OGLRenderer(GLFWwindow *window) {
  mRenderData.rdWindow = window;
}

bool OGLRenderer::init(unsigned int width, unsigned int height) {
  /* required for perspective */
  mRenderData.rdWidth = width;
  mRenderData.rdHeight = height;

  /* initalize GLAD */
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    Logger::log(1, "%s error: failed to initialize GLAD\n", __FUNCTION__);
    return false;
  }

  if (!GLAD_GL_VERSION_4_6) {
    Logger::log(1, "%s error: failed to get at least OpenGL 4.6\n", __FUNCTION__);
    return false;
  }

  GLint majorVersion, minorVersion;
  glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
  glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
  Logger::log(1, "%s: OpenGL %d.%d initializeed\n", __FUNCTION__, majorVersion, minorVersion);

  if (!mFramebuffer.init(width, height)) {
    Logger::log(1, "%s error: could not init Framebuffer\n", __FUNCTION__);
    return false;
  }
  Logger::log(1, "%s: framebuffer succesfully initialized\n", __FUNCTION__);

  if (!mTex.loadTexture("textures/crate.png")) {
    Logger::log(1, "%s: texture loading failed\n", __FUNCTION__);
    return false;
  }
  Logger::log(1, "%s: texture successfully loaded\n", __FUNCTION__);

  mVertexBuffer.init();
  Logger::log(1, "%s: vertex buffer successfully created\n", __FUNCTION__);

  mUniformBuffer.init();
  Logger::log(1, "%s: uniform buffer successfully created\n", __FUNCTION__);

  if (!mBasicShader.loadShaders("shader/basic.vert", "shader/basic.frag")) {
    Logger::log(1, "%s: basic shader loading failed\n", __FUNCTION__);
    return false;
  }
  if (!mLineShader.loadShaders("shader/line.vert", "shader/line.frag")) {
    Logger::log(1, "%s: line shader loading failed\n", __FUNCTION__);
    return false;
  }
  Logger::log(1, "%s: shaders succesfully loaded\n", __FUNCTION__);

  mUserInterface.init(mRenderData);
  Logger::log(1, "%s: user interface initialized\n", __FUNCTION__);

  /* add backface culling and depth test already here */
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glLineWidth(3.0);

  mModel = std::make_unique<Model>();

  mModelMesh = std::make_unique<OGLMesh>();
  Logger::log(1, "%s: model mesh storage initialized\n", __FUNCTION__);

  mAllMeshes = std::make_unique<OGLMesh>();
  Logger::log(1, "%s: global mesh storage initialized\n", __FUNCTION__);

  mFrameTimer.start();

  return true;
}

void OGLRenderer::setSize(unsigned int width, unsigned int height) {
  /* handle minimize */
  if (width == 0 || height == 0) {
    return;
  }

  mRenderData.rdWidth = width;
  mRenderData.rdHeight = height;

  mFramebuffer.resize(width, height);
  glViewport(0, 0, width, height);

  Logger::log(1, "%s: resized window to %dx%d\n", __FUNCTION__, width, height);
}

void OGLRenderer::handleKeyEvents(int key, int scancode, int action, int mods) {
}

void OGLRenderer::handleMouseButtonEvents(int button, int action, int mods) {
  /* forward to ImGui */
  ImGuiIO& io = ImGui::GetIO();
  if (button >= 0 && button < ImGuiMouseButton_COUNT) {
    io.AddMouseButtonEvent(button, action == GLFW_PRESS);
  }

  /* hide from application if above ImGui window */
  if (io.WantCaptureMouse) {
    return;
  }

  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    mMouseLock = !mMouseLock;

    if (mMouseLock) {
      glfwSetInputMode(mRenderData.rdWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      /* enable raw mode if possible */
      if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(mRenderData.rdWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
      }
    } else {
      glfwSetInputMode(mRenderData.rdWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
  }
}

void OGLRenderer::handleMousePositionEvents(double xPos, double yPos) {
  /* forward to ImGui */
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent((float)xPos, (float)yPos);

  /* hide from application if above ImGui window */
  if (io.WantCaptureMouse) {
    return;
  }

  /* calculate relative movement from last position */
  int mouseMoveRelX = static_cast<int>(xPos) - mMouseXPos;
  int mouseMoveRelY = static_cast<int>(yPos) - mMouseYPos;

  if (mMouseLock) {
    mRenderData.rdViewAzimuth += mouseMoveRelX / 10.0;
    /* keep between 0 and 360 degree */
    if (mRenderData.rdViewAzimuth < 0.0) {
      mRenderData.rdViewAzimuth += 360.0;
    }
    if (mRenderData.rdViewAzimuth >= 360.0) {
      mRenderData.rdViewAzimuth -= 360.0;
    }

    mRenderData.rdViewElevation -= mouseMoveRelY / 10.0;
    /* keep between -89 and +89 degree */
    if (mRenderData.rdViewElevation > 89.0) {
      mRenderData.rdViewElevation = 89.0;
    }
    if (mRenderData.rdViewElevation < -89.0) {
      mRenderData.rdViewElevation = -89.0;
    }
  }

  /* save old values*/
  mMouseXPos = static_cast<int>(xPos);
  mMouseYPos = static_cast<int>(yPos);
}

void OGLRenderer::handleMovementKeys() {
  mRenderData.rdMoveForward = 0;
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_W) == GLFW_PRESS) {
    mRenderData.rdMoveForward += 1;
  }
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_S) == GLFW_PRESS) {
    mRenderData.rdMoveForward -= 1;
  }

  mRenderData.rdMoveRight = 0;
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_A) == GLFW_PRESS) {
    mRenderData.rdMoveRight -= 1;
  }
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_D) == GLFW_PRESS) {
    mRenderData.rdMoveRight += 1;
  }

  mRenderData.rdMoveUp = 0;
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_E) == GLFW_PRESS) {
    mRenderData.rdMoveUp += 1;
  }
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_Q) == GLFW_PRESS) {
    mRenderData.rdMoveUp -= 1;
  }

  /* speed up movement with shift */
  if ((glfwGetKey(mRenderData.rdWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
      (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) {
    mRenderData.rdMoveForward *= 4;
    mRenderData.rdMoveRight *= 4;
    mRenderData.rdMoveUp *= 4;
  }
}

void OGLRenderer::draw() {
  /* handle minimize */
  while (mRenderData.rdWidth == 0 || mRenderData.rdHeight == 0) {
    glfwGetFramebufferSize(mRenderData.rdWindow, &mRenderData.rdWidth, &mRenderData.rdHeight);
    glfwWaitEvents();
  }

  /* get time difference for movement */
  double tickTime = glfwGetTime();
  mRenderData.rdTickDiff = tickTime - mLastTickTime;

  mRenderData.rdFrameTime = mFrameTimer.stop();
  mFrameTimer.start();

  handleMovementKeys();

  mAllMeshes->vertices.clear();

  /* draw to framebuffer */
  mFramebuffer.bind();

  glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mMatrixGenerateTimer.start();
  mProjectionMatrix = glm::perspective(
    glm::radians(static_cast<float>(mRenderData.rdFieldOfView)),
    static_cast<float>(mRenderData.rdWidth) / static_cast<float>(mRenderData.rdHeight),
    0.01f, 50.0f);

  mViewMatrix = mCamera.getViewMatrix(mRenderData);
  mRenderData.rdMatrixGenerateTime = mMatrixGenerateTimer.stop();

  mUploadToUBOTimer.start();
  mUniformBuffer.uploadUboData(mViewMatrix, mProjectionMatrix);
  mRenderData.rdUploadToUBOTime = mUploadToUBOTimer.stop();

  /* reset all values to zero when UI button is pressed */
  if (mRenderData.rdResetAnglesAndInterp) {
    mRenderData.rdResetAnglesAndInterp = false;

    mRenderData.rdRotXAngle = { 0, 0 };
    mRenderData.rdRotYAngle = { 0, 0 };
    mRenderData.rdRotZAngle = { 0, 0 };

    mRenderData.rdInterpValue = 0.0f;

    mRenderData.rdSplineStartVertex = glm::vec3(-4.0f, 1.0f, -2.0f);
    mRenderData.rdSplineStartTangent = glm::vec3(-10.0f, -8.0f, 8.0f);
    mRenderData.rdSplineEndVertex = glm::vec3(4.0f, 2.0f, -2.0f);
    mRenderData.rdSplineEndTangent = glm::vec3(-6.0f, 5.0f, -6.0f);

    mRenderData.rdDrawWorldCoordArrows = true;
    mRenderData.rdDrawModelCoordArrows = true;
    mRenderData.rdDrawSplineLines = true;
  }

  /* create quaternion from angles  */
  for (int i = 0; i < 2; ++i) {
    mQuatModelOrientation[i] = glm::normalize(glm::quat(glm::vec3(
      glm::radians(static_cast<float>(mRenderData.rdRotXAngle[i])),
      glm::radians(static_cast<float>(mRenderData.rdRotYAngle[i])),
      glm::radians(static_cast<float>(mRenderData.rdRotZAngle[i]))
    )));
    mQuatModelOrientationConjugate[i] = glm::conjugate(mQuatModelOrientation[i]);
  }

  /* interpolate between the two quaternions */
  mQuatMix = glm::slerp(mQuatModelOrientation[0],mQuatModelOrientation[1],
    mRenderData.rdInterpValue);
  mQuatMixConjugate = glm::conjugate(mQuatMix);

  /* position cube on current spline position */
  glm::vec3 interpolatedPosition = glm::hermite(
    mRenderData.rdSplineStartVertex, mRenderData.rdSplineStartTangent,
    mRenderData.rdSplineEndVertex, mRenderData.rdSplineEndTangent,
    mRenderData.rdInterpValue);

  /* draw a static coordinate system */
  mCoordArrowsMesh.vertices.clear();
  if (mRenderData.rdDrawWorldCoordArrows) {
    mCoordArrowsMesh = mCoordArrowsModel.getVertexData();
    std::for_each(mCoordArrowsMesh.vertices.begin(), mCoordArrowsMesh.vertices.end(),
      [=](auto &n){
        n.color /= 2.0f;
    });
    mAllMeshes->vertices.insert(mAllMeshes->vertices.end(),
      mCoordArrowsMesh.vertices.begin(), mCoordArrowsMesh.vertices.end());
  }

  mStartPosArrowMesh.vertices.clear();
  mEndPosArrowMesh.vertices.clear();
  mQuatPosArrowMesh.vertices.clear();
  if (mRenderData.rdDrawModelCoordArrows) {
    /* start position arrow */
    mStartPosArrowMesh = mArrowModel.getVertexData();
    std::for_each(mStartPosArrowMesh.vertices.begin(), mStartPosArrowMesh.vertices.end(),
      [=](auto &n){
        glm::quat position = glm::quat(0.0f, n.position.x, n.position.y, n.position.z);
        glm::quat newPosition =
          mQuatModelOrientation[0] * position * mQuatModelOrientationConjugate[0];
        n.position.x = newPosition.x;
        n.position.y = newPosition.y;
        n.position.z = newPosition.z;
        n.position += n.position + mRenderData.rdSplineStartVertex;
        n.color = glm::vec3(0.0f, 0.8f, 0.8f);
    });
    mAllMeshes->vertices.insert(mAllMeshes->vertices.end(),
      mStartPosArrowMesh.vertices.begin(), mStartPosArrowMesh.vertices.end());

    /* end position arrow */
    mEndPosArrowMesh = mArrowModel.getVertexData();
    std::for_each(mEndPosArrowMesh.vertices.begin(), mEndPosArrowMesh.vertices.end(),
      [=](auto &n){
        glm::quat position = glm::quat(0.0f, n.position.x, n.position.y, n.position.z);
        glm::quat newPosition =
          mQuatModelOrientation[1] * position * mQuatModelOrientationConjugate[1];
        n.position.x = newPosition.x;
        n.position.y = newPosition.y;
        n.position.z = newPosition.z;
        n.position += n.position + mRenderData.rdSplineEndVertex;
        n.color = glm::vec3(0.8f, 0.8f, 0.0f);
    });
    mAllMeshes->vertices.insert(mAllMeshes->vertices.end(),
      mEndPosArrowMesh.vertices.begin(), mEndPosArrowMesh.vertices.end());

    /* draw an arrow to show quaternion orientation changes */
    mQuatPosArrowMesh = mArrowModel.getVertexData();
    std::for_each(mQuatPosArrowMesh.vertices.begin(), mQuatPosArrowMesh.vertices.end(),
      [=](auto &n){
        glm::quat position = glm::quat(0.0f, n.position.x, n.position.y, n.position.z);
        glm::quat newPosition =
          mQuatMix * position * mQuatMixConjugate;
        n.position.x = newPosition.x;
        n.position.y = newPosition.y;
        n.position.z = newPosition.z;
        n.position += n.position + interpolatedPosition;
    });
    mAllMeshes->vertices.insert(mAllMeshes->vertices.end(),
      mQuatPosArrowMesh.vertices.begin(), mQuatPosArrowMesh.vertices.end());
  }

  /* draw spline */
  mSplineMesh.vertices.clear();
  if (mRenderData.rdDrawSplineLines) {
    mSplineMesh = mSplineModel.createVertexData(25,
      mRenderData.rdSplineStartVertex, mRenderData.rdSplineStartTangent,
      mRenderData.rdSplineEndVertex, mRenderData.rdSplineEndTangent);
    mAllMeshes->vertices.insert(mAllMeshes->vertices.end(),
    mSplineMesh.vertices.begin(), mSplineMesh.vertices.end());
  }

  /* draw the model itself */
  *mModelMesh = mModel->getVertexData();
  mRenderData.rdTriangleCount = mModelMesh->vertices.size() / 3;
  std::for_each(mModelMesh->vertices.begin(), mModelMesh->vertices.end(),
    [=](auto &n){
      glm::quat position = glm::quat(0.0f, n.position.x, n.position.y, n.position.z);
      glm::quat newPosition =
        mQuatMix * position * mQuatMixConjugate;
      n.position.x = newPosition.x;
      n.position.y = newPosition.y;
      n.position.z = newPosition.z;
      n.position += n.position + interpolatedPosition;
  });
  mAllMeshes->vertices.insert(mAllMeshes->vertices.end(),
    mModelMesh->vertices.begin(), mModelMesh->vertices.end());

  /* upload vertex data */
  mUploadToVBOTimer.start();
  mVertexBuffer.uploadData(*mAllMeshes);
  mRenderData.rdUploadToVBOTime = mUploadToVBOTimer.stop();

  mLineIndexCount =
    mStartPosArrowMesh.vertices.size() + mEndPosArrowMesh.vertices.size() +
    mQuatPosArrowMesh.vertices.size() + mCoordArrowsMesh.vertices.size() +
    mSplineMesh.vertices.size();

  /* draw the lines first */
  if (mLineIndexCount > 0) {
    mLineShader.use();
    mVertexBuffer.bindAndDraw(GL_LINES, 0, mLineIndexCount);
  }

  /* draw the model last */
  mBasicShader.use();
  mTex.bind();
  mVertexBuffer.bindAndDraw(GL_TRIANGLES, mLineIndexCount, mRenderData.rdTriangleCount * 3);
  mTex.unbind();

  mFramebuffer.unbind();

  /* blit color buffer to screen */
  mFramebuffer.drawToScreen();

  mUIGenerateTimer.start();
  mUserInterface.createFrame(mRenderData);
  mRenderData.rdUIGenerateTime = mUIGenerateTimer.stop();

  mUIDrawTimer.start();
  mUserInterface.render();
  mRenderData.rdUIDrawTime = mUIDrawTimer.stop();

  mLastTickTime = tickTime;
}

void OGLRenderer::cleanup() {
  mUserInterface.cleanup();
  mLineShader.cleanup();
  mBasicShader.cleanup();
  mTex.cleanup();
  mVertexBuffer.cleanup();
  mUniformBuffer.cleanup();
  mFramebuffer.cleanup();
}
