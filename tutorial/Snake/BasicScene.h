#pragma once
#include "AutoMorphingModel.h"
#include "Scene.h"

#include <memory>
#include <utility>

class BasicScene : public cg3d::Scene
{
public:
    explicit BasicScene(std::string name, cg3d::Display* display) : Scene(std::move(name), display) {};
    void Init(float fov, int width, int height, float near, float far);
    void Update(const cg3d::Program& program, const Eigen::Matrix4f& proj, const Eigen::Matrix4f& view, const Eigen::Matrix4f& model) override;
    void MouseCallback(cg3d::Viewport* viewport, int x, int y, int button, int action, int mods, int buttonState[]) override;
    void ScrollCallback(cg3d::Viewport* viewport, int x, int y, int xoffset, int yoffset, bool dragging, int buttonState[]) override;
    void CursorPosCallback(cg3d::Viewport* viewport, int x, int y, bool dragging, int* buttonState)  override;
    void KeyCallback(cg3d::Viewport* viewport, int x, int y, int key, int scancode, int action, int mods) override;
    void swap_cameras();

private:
    cg3d::Viewport* viewport = nullptr;
    std::shared_ptr<Movable> root;
    std::shared_ptr<cg3d::Camera> camera1, camera2;
    std::shared_ptr<cg3d::Model> sphere1, cube, snake, plane;
    std::shared_ptr<cg3d::AutoMorphingModel> autoCube;
    std::vector<std::shared_ptr<cg3d::Model>> cyls, axis;
    int pickedIndex = 0;
    int tipIndex = 0;
    Eigen::VectorXi EMAP;
    Eigen::MatrixXi F, E, EF, EI;
    Eigen::VectorXi EQ;
    
    Eigen::MatrixXd V, C, N, T, points, edges, colors;
    double link_length = 1.6;
    int number_of_cyls = 4;
};
