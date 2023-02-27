#include "BasicScene.h"
#include <Eigen/src/Core/Matrix.h>
#include <edges.h>
#include <memory>
#include <per_face_normals.h>
#include <read_triangle_mesh.h>
#include <utility>
#include <vector>
#include "GLFW/glfw3.h"
#include "Mesh.h"
#include "PickVisitor.h"
#include "Renderer.h"
#include "ObjLoader.h"
#include "IglMeshLoader.h"

#include "igl/per_vertex_normals.h"
#include "igl/per_face_normals.h"
#include "igl/unproject_onto_mesh.h"
#include "igl/edge_flaps.h"
#include "igl/loop.h"
#include "igl/upsample.h"
#include "igl/AABB.h"
#include "igl/parallel_for.h"
#include "igl/shortest_edge_and_midpoint.h"
#include "igl/circulation.h"
#include "igl/edge_midpoints.h"
#include "igl/collapse_edge.h"
#include "igl/edge_collapse_is_valid.h"
#include "igl/write_triangle_mesh.h"




using namespace cg3d;




void BasicScene::swap_cameras()
{
    if (camera == camera1)
    {
        std::cout << "swap1" << std::endl;
        camera = camera2;
    }
    else
    {
        camera = camera1;
    }
    viewport->camera = camera;
}

void BasicScene::Init(float fov, int width, int height, float near, float far)
{
    camera1 = Camera::Create("", fov, double(width) / height, near, far);
    camera2 = Camera::Create("", fov, double(width) / height, near, far);
    camera = camera1;
    camera2->Translate({ 5, 55, 85 });
    AddChild(root = Movable::Create("root")); // a common (invisible) parent object for all the shapes
    auto daylight{ std::make_shared<Material>("daylight", "shaders/cubemapShader") };
    daylight->AddTexture(0, "textures/cubemaps/Daylight Box_", 3);
    auto background{ Model::Create("background", Mesh::Cube(), daylight) };
    AddChild(background);
    background->Scale(120, Axis::XYZ);
    background->SetPickable(false);
    background->SetStatic();


    auto program = std::make_shared<Program>("shaders/basicShader");
    auto program1 = std::make_shared<Program>("shaders/pickingShader");

    auto material{ std::make_shared<Material>("material", program) }; // empty material
    auto material1{ std::make_shared<Material>("material", program1) }; // empty material

    material->AddTexture(0, "textures/box0.bmp", 2);
    material1->AddTexture(0, "textures/bricks.jpg", 2);
    auto sphereMesh{ IglLoader::MeshFromFiles("sphere_igl", "data/sphere.obj") };
    auto cylMesh{ IglLoader::MeshFromFiles("cyl_igl","data/xcylinder.obj") };
    auto cubeMesh{ IglLoader::MeshFromFiles("cube_igl","data/cube_old.obj") };
    auto snakeMesh{ IglLoader::MeshFromFiles("snake_igl","data/snake1.obj") };
    auto planeMesh{ IglLoader::MeshFromFiles("plane_igl", "data/plane.obj") };
    sphere1 = Model::Create("sphere0", sphereMesh, material1);
    //plane = Model::Create("plane", planeMesh, material);
    //root->AddChild(plane);
    //plane->TranslateInSystem(camera->GetRotation(), {0, -5, 0});
    //plane->Scale(20);
    //plane->Rotate(3.14159/2, Axis::X);
    //Axis
    float scaleFactor = 1;
    cyls.push_back(Model::Create("cyl0", cylMesh, material));
    cyls[0]->Scale(scaleFactor, Axis::X);
    cyls[0]->SetCenter(Eigen::Vector3f(-0.8f * scaleFactor, 0, 0));
    root->AddChild(cyls[0]);

    for (int i = 1; i < number_of_cyls; i++)
    {
        cyls.push_back(Model::Create("cyl", cylMesh, material));
        cyls[i]->Scale(scaleFactor, Axis::X);
        cyls[i]->Translate(1.6f * scaleFactor, Axis::X);
        cyls[i]->SetCenter(Eigen::Vector3f(-0.8f * scaleFactor, 0, 0));
        cyls[i - 1]->AddChild(cyls[i]);
    }
    cyls[0]->Translate({ 0.8f * scaleFactor,0,0 });

    camera1->Translate({ 0, 0, 22 });
    camera1->Rotate(-0.3, Axis::X);
    //root->AddChild(sphere1);
    sphere1->Translate({ 0, 0, -5 });

}

void BasicScene::Update(const Program& program, const Eigen::Matrix4f& proj, const Eigen::Matrix4f& view, const Eigen::Matrix4f& model)
{
    Scene::Update(program, proj, view, model);
    
}

void BasicScene::MouseCallback(Viewport* viewport, int x, int y, int button, int action, int mods, int buttonState[])
{
    // note: there's a (small) chance the button state here precedes the mouse press/release event

    if (action == GLFW_PRESS) { // default mouse button press behavior
        PickVisitor visitor;
        visitor.Init();
        renderer->RenderViewportAtPos(x, y, &visitor); // pick using fixed colors hack
        auto modelAndDepth = visitor.PickAtPos(x, renderer->GetWindowHeight() - y);
        renderer->RenderViewportAtPos(x, y); // draw again to avoid flickering
        pickedModel = modelAndDepth.first ? std::dynamic_pointer_cast<Model>(modelAndDepth.first->shared_from_this()) : nullptr;
        pickedModelDepth = modelAndDepth.second;
        camera->GetRotation().transpose();
        xAtPress = x;
        yAtPress = y;

        if (pickedModel && !pickedModel->isPickable)
            pickedModel = nullptr; // for non-pickable models we need only pickedModelDepth for mouse movement calculations later

        if (pickedModel)
            pickedToutAtPress = pickedModel->GetTout();
        else
            cameraToutAtPress = camera->GetTout();
    }
}

void BasicScene::ScrollCallback(Viewport* viewport, int x, int y, int xoffset, int yoffset, bool dragging, int buttonState[])
{
    // note: there's a (small) chance the button state here precedes the mouse press/release event
    auto system = camera->GetRotation().transpose();
    if (pickedModel) {
        auto T = pickedModel;
        for (int i = 0; i < cyls.size(); i++)
        {
            if (pickedModel == cyls[i])
            {
                pickedModel = cyls[0];
            }
        }
        pickedModel->TranslateInSystem(system, { 0, 0, -float(yoffset) });
        pickedToutAtPress = pickedModel->GetTout();
        pickedModel = T;
    }
    else {
        camera->TranslateInSystem(system, { 0, 0, -float(yoffset) });
        cameraToutAtPress = camera->GetTout();
    }
}

void BasicScene::CursorPosCallback(Viewport* viewport, int x, int y, bool dragging, int* buttonState)
{
    if (dragging) {
        auto system = camera->GetRotation().transpose() * GetRotation();
        auto moveCoeff = camera->CalcMoveCoeff(pickedModelDepth, viewport->width);
        auto angleCoeff = camera->CalcAngleCoeff(viewport->width);
        auto T = pickedModel;
        if (pickedModel) {
            for (int i = 0; i < cyls.size(); i++)
            {
                if (pickedModel == cyls[i])
                {
                    pickedModel = cyls[0];
                }
            }
            //pickedModel->SetTout(pickedToutAtPress);
            if (buttonState[GLFW_MOUSE_BUTTON_RIGHT] != GLFW_RELEASE)
                pickedModel->TranslateInSystem(system, { float(yAtPress - y) / moveCoeff, float(xAtPress - x) / moveCoeff, 0 });
            if (buttonState[GLFW_MOUSE_BUTTON_MIDDLE] != GLFW_RELEASE)
                pickedModel->RotateInSystem(system, float(xAtPress - x) / angleCoeff, Axis::Z);
            if (buttonState[GLFW_MOUSE_BUTTON_LEFT] != GLFW_RELEASE) {
                pickedModel->RotateInSystem(system, float(xAtPress - x) / angleCoeff, Axis::Y);
                pickedModel->RotateInSystem(system, float(yAtPress - y) / angleCoeff, Axis::X);
            }
            pickedModel = T;
        }
        else {
            // camera->SetTout(cameraToutAtPress);
            if (buttonState[GLFW_MOUSE_BUTTON_RIGHT] != GLFW_RELEASE)
                root->TranslateInSystem(system, { -float(xAtPress - x) / moveCoeff / 10.0f, float(yAtPress - y) / moveCoeff / 10.0f, 0 });
            if (buttonState[GLFW_MOUSE_BUTTON_MIDDLE] != GLFW_RELEASE)
                root->RotateInSystem(system, float(x - xAtPress) / 180.0f, Axis::Z);
            if (buttonState[GLFW_MOUSE_BUTTON_LEFT] != GLFW_RELEASE) {
                root->RotateInSystem(system, float(x - xAtPress) / angleCoeff, Axis::Y);
                root->RotateInSystem(system, float(y - yAtPress) / angleCoeff, Axis::X);
            }
        }
        xAtPress = x;
        yAtPress = y;
    }
}



void BasicScene::KeyCallback(Viewport* viewport, int x, int y, int key, int scancode, int action, int mods)
{
    auto system = camera->GetRotation().transpose();

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) // NOLINT(hicpp-multiway-paths-covered)
        {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_UP:
            cyls[pickedIndex]->RotateInSystem(system, 0.1f, Axis::X);
            break;
        case GLFW_KEY_DOWN:
            cyls[pickedIndex]->RotateInSystem(system, -0.1f, Axis::X);
            break;
        case GLFW_KEY_LEFT:
            cyls[pickedIndex]->RotateInSystem(system, 0.1f, Axis::Z);
            break;
        case GLFW_KEY_RIGHT:
            cyls[pickedIndex]->RotateInSystem(system, -0.1f, Axis::Z);
            break;
        case GLFW_KEY_W:
            camera->TranslateInSystem(system, { 0, 0.1f, 0 });
            break;
        case GLFW_KEY_S:
            camera->TranslateInSystem(system, { 0, -0.1f, 0 });
            break;
        case GLFW_KEY_A:
            camera->TranslateInSystem(system, { -0.1f, 0, 0 });
            break;
        case GLFW_KEY_B:
            camera->TranslateInSystem(system, { 0, 0, 0.1f });
            break;
        case GLFW_KEY_F:
            camera->TranslateInSystem(system, { 0, 0, -0.1f });
            break;
        case GLFW_KEY_1:
            if (pickedIndex > 0)
                pickedIndex--;
            break;
        case GLFW_KEY_2:
            if (pickedIndex < cyls.size() - 1)
                pickedIndex++;
            break;
        case GLFW_KEY_N:
            swap_cameras();
            break;
        case GLFW_KEY_SPACE:
            IK = !IK;
            break;
        case GLFW_KEY_H:
            for (int i = 1; i < axis.size(); i++)
            {
                axis[i]->isHidden = !axis[i]->isHidden;
            }
            break;
        }
    }
}

