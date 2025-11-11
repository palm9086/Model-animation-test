#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/animator.h>
#include <learnopengl/model_animation.h>
#include <learnopengl/animation.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stb_image.h>

#include <fstream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <map>

using namespace std;

unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma = false);

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

static bool fileExists(const std::string& path) {
 std::ifstream f(path.c_str());
 return f.good();
}

static bool canLoadAnimation(const std::string& path) {
 Assimp::Importer importer;
 const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);
 return scene && scene->mRootNode;
}

const unsigned int SCR_WIDTH =1000;
const unsigned int SCR_HEIGHT =800;

Camera camera(glm::vec3(0.0f,0.0f,3.0f));
float lastX = SCR_WIDTH /2.0f;
float lastY = SCR_HEIGHT /2.0f;
bool firstMouse = true;
float deltaTime =0.0f;
float lastFrame =0.0f;

enum AnimState { IDLE, RIFLE_IDLE, RUNNING, GRAB, PUTAWAY };

bool HasFinished(Animator& animator, Animation* anim) {
 if (!anim) return true;
 return animator.m_CurrentTime >= anim->GetDuration() -0.05f;
}

glm::mat4 GetBoneMatrix(Model& model, Animator& animator, const std::string& boneName) {
 std::map<std::string, BoneInfo> const* idMapPtr = nullptr;
 if (animator.m_CurrentAnimation) idMapPtr = &animator.m_CurrentAnimation->GetBoneIDMap();

 int index = -1;
 std::string usedName = boneName;

 if (idMapPtr) {
 auto itA = idMapPtr->find(boneName);
 if (itA != idMapPtr->end()) index = itA->second.id;
 else {
 std::string needle = boneName;
 std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
 for (auto &p : *idMapPtr) {
 std::string keyLower = p.first;
 std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
 if (keyLower.find(needle) != std::string::npos) { index = p.second.id; usedName = p.first; break; }
 }
 }
 }

 if (index == -1) {
 auto &modelMap = model.GetBoneInfoMap();
 auto it = modelMap.find(boneName);
 if (it != modelMap.end()) { index = it->second.id; usedName = it->first; }
 else {
 std::string needle = boneName;
 std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
 for (auto &p : modelMap) {
 std::string keyLower = p.first;
 std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
 if (keyLower.find(needle) != std::string::npos) { index = p.second.id; usedName = p.first; break; }
 }
 }
 }

 if (index == -1) return glm::mat4(1.0f);

 auto matrices = animator.GetFinalBoneMatrices();
 if (index >=0 && index < (int)matrices.size()) {
 glm::mat4 offset = glm::mat4(1.0f);
 auto &modelMap = model.GetBoneInfoMap();
 auto it = modelMap.find(usedName);
 if (it != modelMap.end()) offset = it->second.offset;

 glm::mat4 finalMat = matrices[index];
 glm::mat4 globalTransform = finalMat * glm::inverse(offset);
 return globalTransform;
 }
 return glm::mat4(1.0f);
}

int main() {
 // Initialize window
 glfwInit();
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
 glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
 glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
 glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

 GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Neco Gun Attach", NULL, NULL);
 if (!window) { glfwTerminate(); return -1; }
 glfwMakeContextCurrent(window);
 glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
 glfwSetCursorPosCallback(window, mouse_callback);
 glfwSetScrollCallback(window, scroll_callback);
 glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
 if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { glfwTerminate(); return -1; }

 stbi_set_flip_vertically_on_load(true);
 glEnable(GL_DEPTH_TEST);

 Shader shader("anim_model.vs", "anim_model.fs");

 std::string modelPath = FileSystem::getPath("resources/objects/necoarc/neco.dae");
 std::string gunPath = FileSystem::getPath("resources/objects/gun/fullgun.dae");
 std::string idlePath = FileSystem::getPath("resources/objects/necoarc/Breathing Idle.dae");
 std::string grabPath = FileSystem::getPath("resources/objects/necoarc/grab.dae");
 std::string rifleIdlePath = FileSystem::getPath("resources/objects/necoarc/Rifle Idle.dae");
 std::string putAwayPath = FileSystem::getPath("resources/objects/necoarc/put away.dae");
 std::string runPath = FileSystem::getPath("resources/objects/necoarc/Rifle Run.dae");
 std::string runStopPath = FileSystem::getPath("resources/objects/necoarc/Rifle Run To Stop.dae");

 if (!fileExists(modelPath)) { glfwTerminate(); return -1; }

 Model neco(modelPath);
 Model* gun = fileExists(gunPath) ? new Model(gunPath) : nullptr;

 // Keep only these gun attachment variables
 std::string handBone = "hand.R";
 std::string legBone = "legu.R";

 glm::vec3 gunScale(0.8f);
 // Per-attachment transforms
 glm::vec3 gunRotHand(90.0f,90.0f,180.0f);
 glm::vec3 gunOffsetHand(0.5f,1.5f,0.0f);
 glm::vec3 gunRotLeg(0.0f, -60.0f, -90.0f);
 glm::vec3 gunOffsetLeg(0.25f,1.25f,1.0f);

 auto loadAnim = [&](const std::string& path) -> Animation* {
 if (fileExists(path) && canLoadAnimation(path)) return new Animation(path, &neco);
 return nullptr;
 };

 Animation* animIdle = loadAnim(idlePath);
 Animation* animGrab = loadAnim(grabPath);
 Animation* animRifleIdle = loadAnim(rifleIdlePath);
 Animation* animPutAway = loadAnim(putAwayPath);
 Animation* animRun = loadAnim(runPath);
 Animation* animRunStop = loadAnim(runStopPath);

 Animator animator(animIdle);

 bool hasGun = false;
 bool isRunning = false;
 bool grabInProgress = false;
 bool putAwayInProgress = false;
 bool runStopInProgress = false;
 AnimState state = IDLE;

 bool prevKey3 = false;
 bool prevKey1 = false;

 while (!glfwWindowShouldClose(window)) {
 float currentFrame = static_cast<float>(glfwGetTime());
 deltaTime = currentFrame - lastFrame;
 lastFrame = currentFrame;

 processInput(window);

 bool key3 = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;
 bool key1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;

 if (key3 && !prevKey3) {
 if (state == RIFLE_IDLE && animRun) {
 animator.PlayAnimation(animRun, nullptr,0.0f,0.0f,1.0f);
 isRunning = true; state = RUNNING;
 } else if (state == RUNNING && animRunStop && !runStopInProgress) {
 animator.PlayAnimation(animRunStop, nullptr,0.0f,0.0f,1.0f);
 runStopInProgress = true;
 }
 }

 if (key1 && !prevKey1) {
 if (state == RUNNING && animRunStop && !runStopInProgress) {
 animator.PlayAnimation(animRunStop, nullptr,0.0f,0.0f,1.0f);
 runStopInProgress = true;
 }
 }

 prevKey3 = key3; prevKey1 = key1;

 if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
 if (!hasGun && !grabInProgress && state == IDLE && animGrab) { animator.PlayAnimation(animGrab, nullptr,0.0f,0.0f,1.0f); grabInProgress = true; state = GRAB; }
 else if (hasGun && !putAwayInProgress && state == RIFLE_IDLE && animPutAway) { animator.PlayAnimation(animPutAway, nullptr,0.0f,0.0f,1.0f); putAwayInProgress = true; state = PUTAWAY; }
 }

 if (HasFinished(animator, animator.m_CurrentAnimation)) {
 if (grabInProgress && animator.m_CurrentAnimation == animGrab) { grabInProgress = false; hasGun = true; animator.PlayAnimation(animRifleIdle, nullptr,0.0f,0.0f,1.0f); state = RIFLE_IDLE; }
 else if (putAwayInProgress && animator.m_CurrentAnimation == animPutAway) { putAwayInProgress = false; hasGun = false; animator.PlayAnimation(animIdle, nullptr,0.0f,0.0f,1.0f); state = IDLE; }
 else if (runStopInProgress && animator.m_CurrentAnimation == animRunStop) { runStopInProgress = false; isRunning = false; animator.PlayAnimation(animRifleIdle, nullptr,0.0f,0.0f,1.0f); state = RIFLE_IDLE; }
 else if (state == RUNNING) { animator.PlayAnimation(animRun, nullptr,0.0f,0.0f,1.0f); }
 }

 animator.UpdateAnimation(deltaTime);

 glClearColor(0.05f,0.05f,0.05f,1.0f);
 glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

 shader.use();
 glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT,0.1f,100.0f);
 glm::mat4 view = camera.GetViewMatrix();
 shader.setMat4("projection", projection);
 shader.setMat4("view", view);

 auto transforms = animator.GetFinalBoneMatrices();
 for (int i =0; i < (int)transforms.size(); ++i)
 shader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

 glm::mat4 modelMat(1.0f);
 modelMat = glm::translate(modelMat, glm::vec3(0.0f, -0.4f,0.0f));
 modelMat = glm::scale(modelMat, glm::vec3(0.5f));
 shader.setMat4("model", modelMat);
 neco.Draw(shader);

 if (gun) {
 glm::mat4 boneMat = (hasGun || grabInProgress || putAwayInProgress || isRunning) ? GetBoneMatrix(neco, animator, handBone) : GetBoneMatrix(neco, animator, legBone);
 glm::mat4 gunMat = modelMat * boneMat;
 bool attachToHand = (hasGun || grabInProgress || putAwayInProgress || isRunning);
 glm::vec3 curGunRot = attachToHand ? gunRotHand : gunRotLeg;
 glm::vec3 curGunOffset = attachToHand ? gunOffsetHand : gunOffsetLeg;

 gunMat = glm::translate(gunMat, curGunOffset);
 gunMat = glm::rotate(gunMat, glm::radians(curGunRot.x), glm::vec3(1,0,0));
 gunMat = glm::rotate(gunMat, glm::radians(curGunRot.y), glm::vec3(0,1,0));
 gunMat = glm::rotate(gunMat, glm::radians(curGunRot.z), glm::vec3(0,0,1));
 gunMat = glm::scale(gunMat, gunScale);

 shader.setMat4("model", gunMat);
 for (size_t i =0; i < gun->meshes.size(); ++i) gun->meshes[i].Draw(shader);
 }

 glfwSwapBuffers(window);
 glfwPollEvents();
 }

 delete animIdle; delete animGrab; delete animRifleIdle; delete animPutAway; delete animRun; delete animRunStop;
 if (gun) delete gun;

 glfwTerminate();
 return 0;
}

void processInput(GLFWwindow* window) {
 if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
 if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard(FORWARD, deltaTime);
 if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard(BACKWARD, deltaTime);
 if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard(LEFT, deltaTime);
 if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow*, int width, int height) { glViewport(0,0, width, height); }

void mouse_callback(GLFWwindow*, double xpos, double ypos) {
 if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
 float xoffset = (float)xpos - lastX;
 float yoffset = lastY - (float)ypos;
 lastX = (float)xpos; lastY = (float)ypos;
 camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow*, double, double yoffset) { camera.ProcessMouseScroll((float)yoffset); }

unsigned int TextureFromFile(const char* path, const std::string& directory, bool gamma) {
 std::string filename = directory + "/" + std::string(path);
 unsigned int textureID; glGenTextures(1, &textureID);
 int width, height, nrComponents; unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents,0);
 if (data) {
 GLenum format = GL_RGB; if (nrComponents ==1) format = GL_RED; else if (nrComponents ==3) format = GL_RGB; else if (nrComponents ==4) format = GL_RGBA;
 GLenum internalFormat = gamma ? (nrComponents ==4 ? GL_SRGB_ALPHA : GL_SRGB) : format;
 glBindTexture(GL_TEXTURE_2D, textureID);
 glTexImage2D(GL_TEXTURE_2D,0, internalFormat, width, height,0, format, GL_UNSIGNED_BYTE, data);
 glGenerateMipmap(GL_TEXTURE_2D);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
 glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 stbi_image_free(data);
 } else { std::cout << "Texture failed to load at path: " << filename << std::endl; stbi_image_free(data); }
 return textureID;
}