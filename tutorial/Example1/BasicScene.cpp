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




Eigen::Matrix3f BasicScene::angle_X_to_matrix(float angle)
{
    Eigen::Matrix3f R = Eigen::Matrix3f::Zero();
    R(0, 0) = 1;
    R(1, 1) = std::cos(angle);
    R(2, 2) = std::cos(angle);
    R(1, 2) = std::sin(angle) * -1;
    R(2, 1) = std::sin(angle);
    return R;
}

Eigen::Matrix3f BasicScene::angle_Z_to_matrix(float angle)
{
    Eigen::Matrix3f R = Eigen::Matrix3f::Zero();
    R(0, 0) = std::cos(angle);
    R(1, 1) = std::cos(angle);
    R(2, 2) = 1;
    R(0, 1) = std::sin(angle) * -1;
    R(1, 0) = std::sin(angle);
    return R;
}


using namespace cg3d;

void BasicScene::Init(float fov, int width, int height, float near, float far)
{
    camera = Camera::Create( "camera", fov, float(width) / height, near, far);
    
    AddChild(root = Movable::Create("root")); // a common (invisible) parent object for all the shapes
    auto daylight{std::make_shared<Material>("daylight", "shaders/cubemapShader")}; 
    daylight->AddTexture(0, "textures/cubemaps/Daylight Box_", 3);
    auto background{Model::Create("background", Mesh::Cube(), daylight)};
    AddChild(background);
    background->Scale(120, Axis::XYZ);
    background->SetPickable(false);
    background->SetStatic();

 
    auto program = std::make_shared<Program>("shaders/phongShader");
    auto program1 = std::make_shared<Program>("shaders/pickingShader");
    
    auto material{ std::make_shared<Material>("material", program)}; // empty material
    auto material1{ std::make_shared<Material>("material", program1)}; // empty material

    material->AddTexture(0, "textures/box0.bmp", 2);
    auto sphereMesh{IglLoader::MeshFromFiles("sphere_igl", "data/sphere.obj")};
    auto cylMesh{IglLoader::MeshFromFiles("cyl_igl","data/xcylinder.obj")};
    auto cubeMesh{IglLoader::MeshFromFiles("cube_igl","data/cube_old.obj")};
    sphere1 = Model::Create( "sphere",sphereMesh, material);    
    
    //Axis
    Eigen::MatrixXd vertices(6,3);
    vertices << -1,0,0,1,0,0,0,-1,0,0,1,0,0,0,-1,0,0,1;
    Eigen::MatrixXi faces(3,2);
    faces << 0,1,2,3,4,5;
    Eigen::MatrixXd vertexNormals = Eigen::MatrixXd::Ones(6,3);
    Eigen::MatrixXd textureCoords = Eigen::MatrixXd::Ones(6,2);
    std::shared_ptr<Mesh> coordsys = std::make_shared<Mesh>("coordsys",vertices,faces,vertexNormals,textureCoords);
    axis.push_back(Model::Create("axis",coordsys,material1));
    axis[0]->mode = 1;   
    axis[0]->Scale(2 * link_length, Axis::XYZ);
    root->AddChild(axis[0]);
    float scaleFactor = 1; 
    cyls.push_back( Model::Create("cyl",cylMesh, material));
    cyls[0]->Scale(scaleFactor,Axis::X);
    cyls[0]->SetCenter(Eigen::Vector3f(-0.8f*scaleFactor,0,0));
    root->AddChild(cyls[0]);
   
    for(int i = 1; i < number_of_cyls; i++)
    { 
        cyls.push_back( Model::Create("cyl", cylMesh, material));
        cyls[i]->Scale(scaleFactor,Axis::X);   
        cyls[i]->Translate(1.6f*scaleFactor,Axis::X);
        cyls[i]->SetCenter(Eigen::Vector3f(-0.8f*scaleFactor,0,0));
        cyls[i-1]->AddChild(cyls[i]);

        axis.push_back(Model::Create("axis", coordsys, material1));
        axis[i]->mode = 1;
        axis[i]->Scale(2 * link_length, Axis::XYZ);
        axis[i]->Translate({ 0.8f * scaleFactor, 0, 0});
        cyls[i - 1]->AddChild(axis[i]);
    }
    cyls[0]->Translate({0.8f*scaleFactor,0,0});
    root->Rotate(3.141592f / 2, Axis::Z);

    auto morphFunc = [](Model* model, cg3d::Visitor* visitor) {
        return model->meshIndex;
    };
  

    camera->Translate(22, Axis::Z);
    root->AddChild(sphere1);
    sphere1->Translate({0, -5, 0 });
    std::cout << Eigen::Vector3f({3.0, 4.0, 0.0}).norm() << std::endl;

}

void BasicScene::Update(const Program& program, const Eigen::Matrix4f& proj, const Eigen::Matrix4f& view, const Eigen::Matrix4f& model)
{
    Scene::Update(program, proj, view, model);
    program.SetUniform4f("lightColor", 0.8f, 0.3f, 0.0f, 0.5f);
    program.SetUniform4f("Kai", 1.0f, 0.3f, 0.6f, 1.0f);
    program.SetUniform4f("Kdi", 0.5f, 0.5f, 0.0f, 1.0f);
    program.SetUniform1f("specular_exponent", 5.0f);
    program.SetUniform4f("light_position", 0.0, 15.0f, 0.0, 1.0f);
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

        // if (pickedModel)
        //     debug("found ", pickedModel->isPickable ? "pickable" : "non-pickable", " model at pos ", x, ", ", y, ": ",
        //           pickedModel->name, ", depth: ", pickedModelDepth);
        // else
        //     debug("found nothing at pos ", x, ", ", y);

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
        pickedModel->TranslateInSystem(system, {0, 0, -float(yoffset)});
        pickedToutAtPress = pickedModel->GetTout();
        pickedModel = T;
    } else {
        camera->TranslateInSystem(system, {0, 0, -float(yoffset)});
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
                pickedModel->TranslateInSystem(system, {float(yAtPress - y) / moveCoeff, float(xAtPress - x) / moveCoeff, 0});
            if (buttonState[GLFW_MOUSE_BUTTON_MIDDLE] != GLFW_RELEASE)
                pickedModel->RotateInSystem(system, float(xAtPress - x) / angleCoeff, Axis::Z);
            if (buttonState[GLFW_MOUSE_BUTTON_LEFT] != GLFW_RELEASE) {
                pickedModel->RotateInSystem(system, float(xAtPress - x) / angleCoeff, Axis::Y);
                pickedModel->RotateInSystem(system, float(yAtPress - y) / angleCoeff, Axis::X);
            }
            pickedModel = T;
        } else {
           // camera->SetTout(cameraToutAtPress);
            if (buttonState[GLFW_MOUSE_BUTTON_RIGHT] != GLFW_RELEASE)
                root->TranslateInSystem(system, {-float(xAtPress - x) / moveCoeff/10.0f, float( yAtPress - y) / moveCoeff/10.0f, 0});
            if (buttonState[GLFW_MOUSE_BUTTON_MIDDLE] != GLFW_RELEASE)
                root->RotateInSystem(system, float(x - xAtPress) / 180.0f, Axis::Z);
            if (buttonState[GLFW_MOUSE_BUTTON_LEFT] != GLFW_RELEASE) {
                root->RotateInSystem(system, float(x - xAtPress) / angleCoeff, Axis::Y);
                root->RotateInSystem(system, float(y - yAtPress) / angleCoeff, Axis::X);
            }
        }
        xAtPress =  x;
        yAtPress =  y;
    }
}


Eigen::Matrix3f mat_from_quat(Eigen::Quaternionf q)
{
    Eigen::Matrix3f R = Eigen::Matrix3f::Zero();
    Eigen::Vector4f coeff = q.coeffs();
    float w = coeff(0), x = coeff(1), y = coeff(2), z = coeff(3);
    R(0, 0) = w * w + x * x - y * y - z * z;
    R(0, 1) = 2 * x * y - 2 * w * z;
    R(0, 2) = 2 * w * y + 2 * x * z;
    R(1, 0) = 2 * w * z + 2 * x * y;
    R(1, 1) = w * w - x * x + y * y - z * z;
    R(1, 2) = 2 * y * z - 2 * w * x;
    R(2, 0) = 2 * x * z - 2 * w * y;
    R(2, 1) = 2 * w * x + 2 * y * z;
    R(2, 2) = w * w - x * x - y * y + z * z;
    return R;
}

void BasicScene::Animate()
{
    Eigen::Vector3f D = GetSpherePos();
    if (D.norm() > number_of_cyls * link_length)
    {
        std::cout << "destination too far!" << std::endl;
        IK = false;
    }
    else
    {
        // perform CCD
        for (int i = cyls.size() - 1; i >= 0; i--)
        {
            std::vector<Eigen::Vector3f> tips = tips_position();
            Eigen::Vector3f E = tips[tips.size() - 1];
            Eigen::Vector3f R = tips[i] - cyls[i]->GetRotation() * Eigen::Vector3f({ 1.6f, 0.0f, 0.0f});
            Eigen::Vector3f RD = D - R, RE = E - R; 
            double x = RD.transpose() * RE;
            x = x / RD.norm();
            x = x / RE.norm();
            double angle = std::acos(x);
            if (!std::isnan(angle))
            {
                //cyls[i]->Rotate(-1 * angle / 20, RD.cross(RE));
            }
            Eigen::Quaternionf quat = Eigen::Quaternionf::FromTwoVectors(RE, RD);
            quat = quat.slerp(0.92f, Eigen::Quaternionf::Identity());
            cyls[i]->Rotate(quat);
            if ((E - D).norm() <= 0.05f)
            {
                IK = false;
                std::cout << "reached" << std::endl;
                break;
            }
        }
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
                camera->TranslateInSystem(system, {0, 0.1f, 0});
                break;
            case GLFW_KEY_S:
                camera->TranslateInSystem(system, {0, -0.1f, 0});
                break;
            case GLFW_KEY_A:
                camera->TranslateInSystem(system, {-0.1f, 0, 0});
                break;
            case GLFW_KEY_B:
                camera->TranslateInSystem(system, {0, 0, 0.1f});
                break;
            case GLFW_KEY_F:
                camera->TranslateInSystem(system, {0, 0, -0.1f});
                break;
            case GLFW_KEY_1:
                if( pickedIndex > 0)
                  pickedIndex--;
                break;
            case GLFW_KEY_2:
                if(pickedIndex < cyls.size()-1)
                    pickedIndex++;
                break;
            case GLFW_KEY_N:
                if (pickedIndex < cyls.size() - 1)
                {
                    pickedIndex++;
                }
                else 
                {
                    pickedIndex = 0;
                }
                break;
            case GLFW_KEY_SPACE:
                IK = !IK;
                break;
            case GLFW_KEY_D:
                std::cout << "sphere position: " << GetSpherePos().transpose() << std::endl;
                break;
            case GLFW_KEY_H:
                for (int i = 1; i < axis.size(); i++)
                {
                    axis[i]->isHidden = !axis[i]->isHidden;
                }
            case GLFW_KEY_T:
                print_tips();
                break;
            case GLFW_KEY_P:
                Eigen::Vector3f angles = cyls[0]->GetRotation().eulerAngles(2, 0, 2);
                if (pickedModel)
                {      
                    for (int i = 0; i < cyls.size(); i++)
                    {
                        if (pickedModel == cyls[i])
                        {
                            angles = cyls[i]->GetRotation().eulerAngles(2, 0, 2);
                        }
                    }
                }
                std::cout << "Z: \n" << angle_Z_to_matrix(angles(0)) << "\n" << std::endl;
                std::cout << "X: \n" << angle_X_to_matrix(angles(1)) << "\n" << std::endl;
                std::cout << "Z: \n" << angle_Z_to_matrix(angles(2)) << "\n" << std::endl;
                
                break;
        }
    }
}

Eigen::Vector3f BasicScene::GetSpherePos()
{
    Eigen::Vector4f initial_pos = { 0.0f, 0.0f, 0.0f, 1.0f };
    Eigen::Vector4f current_pos = sphere1->GetTransform() * initial_pos;
    return root->GetRotation() * Eigen::Vector3f({current_pos(0), current_pos(1), current_pos(2)});
}



void BasicScene::print_tips()
{
    std::vector<Eigen::Vector3f> tips = tips_position();
    for (int i = 0; i < tips.size(); i++)
    {
        std::cout << tips[i].transpose() << std::endl;
    }
}

std::vector<Eigen::Vector3f> BasicScene::tips_position()
{
    Eigen::Vector3f l = { 1.6f, 0.0f, 0.0f };
    Eigen::Vector3f p = { 0.0f, 0.0f, 0.0f };
    std::vector<Eigen::Vector3f> tips;
    for (int i = 0; i < cyls.size(); i++)
    {
        p = p + cyls[i]->GetRotation() * l;
        tips.push_back(p);
    }
    return tips;
}



void FABRIK(Eigen::Vector3f dest, std::vector<Eigen::Vector3f> joints)
{
    std::vector<Eigen::Vector3f> p(joints.size());
}