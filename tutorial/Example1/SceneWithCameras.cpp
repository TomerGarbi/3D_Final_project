#pragma comment(lib, "winmm.lib")
#include "SceneWithCameras.h"

#include <utility>

#include "ObjLoader.h"
#include "AutoMorphingModel.h"
#include "SceneWithImGui.h"
#include "CamModel.h"
#include "Visitor.h"
#include "Utility.h"

#include <Windows.h>
#include "imgui.h"
#include "file_dialog_open.h"
#include "GLFW/glfw3.h"
#include "IglMeshLoader.h"

#include "igl/dqs.h"

#include <Windows.h>
#include <mmsystem.h>
#include <thread>

using namespace cg3d;

void SceneWithCameras::BuildImGui()
{
    int flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    bool* pOpen = nullptr;

    ImGui::Begin("Menu", pOpen, flags);
    ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetWindowSize(ImVec2(0, 0), ImGuiCond_Always);

    switch (state)
    {
    case(0):
        ImGui::Text("Welcome");
        ImGui::Text("Start game or select stage");
        if (ImGui::Button("start game"))
        {
            state = STAGE_START;
            PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
        }
        ImGui::Text("Select stage: ");
        for (int i = 1; i <= 3; i++) {
            ImGui::SameLine(0);
            std::string s = "stage ";
            std::string n = std::to_string(i);
            std::string b = s + n;
            if (ImGui::Button(b.c_str()))
            {
                state = STAGE_START;
                init_stage(i);
                PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
            }
        }
        break;

    case(1):
        ImGui::Text("Ready?");
        if (ImGui::Button("Start!"))
        {
            state = DURING_STAGE;
            pause = false;
            PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
        }
        break;
    case(2):
        ImGui::SameLine();
        if (ImGui::Button("pause"))
        {
            pause = true;
            state = GAME_PAUSE;
            PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
        }

        ImGui::Text("Camera: ");
        for (int i = 0; i < camList.size(); i++) {
            ImGui::SameLine(0);
            bool selectedCamera = camList[i] == camera;
            if (selectedCamera)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(std::to_string(i + 1).c_str()))
                SetCamera(i);

            if (selectedCamera)
                ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (ImGui::Button("Center"))
            camera->SetTout(Eigen::Affine3f::Identity());
        ImGui::Text("points: %d/%d", points, total_prices);
        ImGui::Text("hp: %d/%d", hp, 100);
        ImGui::Text("ammo: %d/%d", ammo.size(), 5);
        break;
        
    case(3):
        if (stage == 3)
        {
            ImGui::Text("Game complited:");
            
            if (ImGui::Button("restart"))
            {
                stage = 1;
                init_stage(stage);
                state = STAGE_START;
                PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
            }
            if (ImGui::Button("exit"))
            {
                PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
                exit(0);
            }
        }
        else
        {
            ImGui::Text("Stage Clear");
            ImGui::Text("points: %d/%d", points, total_prices);
            ImGui::Text("hp: %d/%d", hp, 100);
            ImGui::Text("ammo: %d/%d", ammo.size(), 5);
            ImGui::Text("Continue or select stage: ");
            if (ImGui::Button("Continue"))
            {
                PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
                stage++;
                init_stage(stage);
                state = STAGE_START;
            }
        }
        break;

    case(4):
        ImGui::Text("Game Paused");
        if (ImGui::Button("resume"))
        {
            PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
            state = DURING_STAGE;
            pause = false;
        }
        if (ImGui::Button("exit"))
            exit(0);
        break;
    case(5):
        ImGui::Text("Stage Failed");
        if (ImGui::Button("restart stage"))
        {
            PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
            state = STAGE_START;
            init_stage(stage);
        }
        if (ImGui::Button("exit"))
        {
            PlaySound("data/music/button_click.wav", NULL, SND_ASYNC);
            exit(0);
        }
        break;
    };

    
    
    
    
    
    ImGui::End();
}

void SceneWithCameras::DumpMeshData(const Eigen::IOFormat& simple, const MeshData& data)
{
    std::cout << "vertices mesh: " << data.vertices.format(simple) << std::endl;
    std::cout << "faces mesh: " << data.faces.format(simple) << std::endl;
    std::cout << "vertex normals mesh: " << data.vertexNormals.format(simple) << std::endl;
    std::cout << "texture coordinates mesh: " << data.textureCoords.format(simple) << std::endl;
}

SceneWithCameras::SceneWithCameras(std::string name, Display* display) : SceneWithImGui(std::move(name), display)
{
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 5.0f;
}

void SceneWithCameras::SetCamera(int index)
{
    camera = camList[index];
    viewport->camera = camera;
}

void SceneWithCameras::Init(float fov, int width, int height, float nearf, float farf)
{
    // create the basic elements of the scene
    AddChild(root = Movable::Create("root")); // a common (invisible) parent object for all the shapes
    auto program = std::make_shared<Program>("shaders/basicShader"); // TODO: TAL: replace with hard-coded basic program
    auto program2 = std::make_shared<Program>("shaders/phongShader"); // TODO: TAL: replace with hard-coded basic program
    auto program3 = std::make_shared<Program>("shaders/phongShader2"); // TODO: TAL: replace with hard-coded basic program
    carbon = std::make_shared<Material>("carbon", program3); // default material
    carbon->AddTexture(0, "textures/carbon.jpg", 2);

    
    // create the camera objects
    camList.resize(camList.capacity());
    camList[0] = Camera::Create("camera0", fov, float(width) / float(height), nearf, farf);
    for (int i = 1; i < camList.size(); i++) {
        auto camera = Camera::Create("", fov, double(width) * 1.2 / height, nearf, farf);
        auto model = ObjLoader::ModelFromObj(std::string("camera") + std::to_string(i), "data/camera.obj", carbon);
        model->isHidden = true;
        root->AddChild(camList[i] = CamModel::Create(*camera, *model));
        
    }
    
    camList[0]->Translate({0.0f, 30.0f, 35.f});
    camList[0]->RotateByDegree(-45, Axis::X);
    
    
    
    camera = camList[0];

    auto bricks{std::make_shared<Material>("bricks", program)};
    auto grass{std::make_shared<Material>("grass", program2)};
    auto daylight{std::make_shared<Material>("daylight", "shaders/cubemapShader")};
    auto city{std::make_shared<Material>("city", "shaders/cubemapShader")};
    auto woods{std::make_shared<Material>("city", "shaders/cubemapShader")};
    

    bricks->AddTexture(0, "textures/bricks.jpg", 2);
    grass->AddTexture(0, "textures/grass.bmp", 2);
    daylight->AddTexture(0, "textures/cubemaps/Daylight Box_", 3);
    city->AddTexture(0, "textures/cubemaps/Yokohoma Box_", 3);
    woods->AddTexture(0, "textures/cubemaps/Storeforsen_", 3);
    

    


    auto sphereMesh{ IglLoader::MeshFromFiles("sphere_igl", "data/sphere.obj") };
    auto cylMesh{ IglLoader::MeshFromFiles("cyl_igl","data/xcylinder.obj") };
    auto cubeMesh{ IglLoader::MeshFromFiles("cube_igl","data/cube_old.obj") };
    auto snakeMesh{ IglLoader::MeshFromFiles("snake_igl","data/snake1.obj") };
    auto planeMesh{ IglLoader::MeshFromFiles("plane_igl", "data/plane.obj") };
    
    
    

    
    auto blank{std::make_shared<Material>("blank", program)};
    auto snake{Model::Create("snake", snakeMesh, blank)};

   
    
    float platform_scale = 20;

    cube1 = Model::Create("plane", Mesh::Cube(), bricks);
    cube2 = Model::Create("plane", Mesh::Cube(), bricks);
    cube3 = Model::Create("plane", Mesh::Cube(), bricks);
    cube4 = Model::Create("plane", Mesh::Cube(), bricks);
    stage1_background = Model::Create("plane", Mesh::Cube(), daylight);
    stage2_background = Model::Create("plane", Mesh::Cube(), woods);
    stage3_background = Model::Create("plane", Mesh::Cube(), city);

    cube1->Scale(platform_scale);
    cube2->Scale(platform_scale);
    cube3->Scale(platform_scale);
    cube4->Scale(platform_scale);

    stage1_background->Scale(120);
    stage2_background->Scale(120);
    stage3_background->Scale(120);


    stage1_background->RotateByDegree(35, Axis::X);
    stage2_background->RotateByDegree(35, Axis::X);
    stage3_background->RotateByDegree(35, Axis::X);
    

    stage1_background->SetPickable(false);
    stage2_background->SetPickable(false);
    stage3_background->SetPickable(false);
    

    cube1->Translate({platform_scale / 2, -platform_scale / 2, -platform_scale / 2});
    cube2->Translate({ -platform_scale / 2, -platform_scale / 2, -platform_scale / 2 });
    cube3->Translate({ platform_scale / 2, -platform_scale / 2, platform_scale / 2 });
    cube4->Translate({ -platform_scale / 2, -platform_scale / 2, platform_scale / 2 });
    root->AddChildren({cube1, cube2, cube3, cube4, stage1_background, stage2_background, stage3_background});
    
    
    cyls.push_back(Model::Create("cyl0", Mesh::Cylinder(), grass));
    cyls[0]->Scale(scaleFactor, Axis::X);
    cyls[0]->SetCenter(Eigen::Vector3f(-0.8f * scaleFactor, 0, 0));
    root->AddChild(cyls[0]);
    cyls[0]->cyls_index = 0;

    for (int i = 1; i < number_of_cyls; i++)
    {
        cyls.push_back(Model::Create("cyl", Mesh::Cylinder(), grass));
        cyls[i]->Scale(scaleFactor, Axis::X);
        cyls[i]->Translate(1.6f * scaleFactor, Axis::X);
        cyls[i]->SetCenter(Eigen::Vector3f(-0.8f * scaleFactor, 0, 0));
        cyls[i]->cyls_index = i;
        cyls[i-1]->AddChild(cyls[i]);
    }
    cyls[0]->Translate({25.0f, 0.8f, 0.0f });
    cyls[0]->Scale(4, Axis::YZ);
    cyls[0]->AddChild(camList[1]);
    camList[1]->Translate({ 5.0f, 3.0f, 0.0f });
    camList[1]->RotateByDegree(90, Axis::Y);
    camList[1]->RotateByDegree(-22.5, Axis::X);
    
 

    for (int i = 0; i < cyls.size(); i++)
    {
        auto mesh = cyls[i]->GetMeshList();
        auto V = mesh[0]->data[0].vertices;
        auto F = mesh[0]->data[0].faces;
        cyls[i]->obb_tree.init(V, F);
    }
    init_transform = cyls[0]->GetTransform();
    init_stage(1);

    
    //std::thread t([this] { this->play_background_music(); });
    //t.join();
    

    // reset the root becuse of weired scaling
    root->RotateByDegree(90, Axis::Z);
    root->RotateByDegree(-90, Axis::Z);

    
}

void SceneWithCameras::Update(const Program& program, const Eigen::Matrix4f& proj, const Eigen::Matrix4f& view, const Eigen::Matrix4f& model)
{
    Scene::Update(program, proj, view, model);
    
}

void SceneWithCameras::Animate()
{
    if(!pause)
    {
        if (to_animate)
        {
            animate_cyls();
            switch (turn)
            {
            case(0):
                animate_bullets();
                break;
            case(1):
                self_collision();
                animate_prices();
                break;
            case(2):
                animate_enemies();
                break;
            };
            turn = (turn + 1) % 3;
        }
    }
    
    to_animate = true;

}

void SceneWithCameras::LoadObjectFromFileDialog()
{
    std::string filename = igl::file_dialog_open();
    if (filename.length() == 0) return;

    auto shape = Model::Create(filename, carbon);
}

void SceneWithCameras::KeyCallback(Viewport* _viewport, int x, int y, int key, int scancode, int action, int mods)
{
   
    Eigen::Matrix3f m;
    m << 1, 0, 0,
        0, 1, 0,
        0, 0, 1;
    auto system = m;
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {

        switch (key)
        {
        case GLFW_KEY_UP:
            cyls[0]->RotateInSystem(system, 0.1f, Axis::Y);
            cyls[1]->RotateInSystem(system, -0.1f, Axis::Y);

            break;
        case GLFW_KEY_DOWN:
            cyls[0]->RotateInSystem(system, -0.1f, Axis::Y);
            cyls[1]->RotateInSystem(system, 0.1f, Axis::Y);
            break;
        case GLFW_KEY_LEFT:
            cyls[0]->RotateInSystem(system, 0.1f, Axis::Y);
            cyls[1]->RotateInSystem(system, -0.1f, Axis::Y);
            //update_rotation_list(0.05f);
            break;
        case GLFW_KEY_RIGHT:
            cyls[0]->RotateInSystem(system, -0.1f, Axis::Y);
            cyls[1]->RotateInSystem(system, 0.1f, Axis::Y);
            //update_rotation_list(-0.05f);
            break;
        case GLFW_KEY_P:
            state = GAME_PAUSE;
            pause = true;
            break;

        case GLFW_KEY_SPACE:
            shoot();
          

            
            
            break;
        }
        // keys 1-9 are objects 1-9 (objects[0] - objects[8]), key 0 is object 10 (objects[9])
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
            if (int index; (index = (key - GLFW_KEY_1 + 10) % 10) < camList.size())
                SetCamera(index);
        }
    }

    //SceneWithImGui::KeyCallback(nullptr, x, y, key, scancode, action, mods);
}

void SceneWithCameras::ViewportSizeCallback(Viewport* _viewport)
{
    for (auto& cam: camList)
        cam->SetProjection(float(_viewport->width) / float(_viewport->height));

    // note: we don't need to call Scene::ViewportSizeCallback since we are setting the projection of all the cameras
}

void SceneWithCameras::AddViewportCallback(Viewport* _viewport)
{
    viewport = _viewport;

    Scene::AddViewportCallback(viewport);
}



std::vector<Eigen::Vector3f> SceneWithCameras::tips_position()
{
    Eigen::Vector3f l = { 1.6f * scaleFactor, 0.0f, 0.0f };
    Eigen::Vector3f p = { 0.0f, 0.0f, 0.0f };
    std::vector<Eigen::Vector3f> tips;
    tips.push_back(Eigen::Vector3f(0, 0, 0));
    for (int i = 0; i < cyls.size(); i++)
    {
        p = p + cyls[i]->GetRotation() * l;
        tips.push_back(p);
    }
    return tips;
}


void SceneWithCameras::print_tips()
{
    std::vector<Eigen::Vector3f> tips = tips_position();
    for (int i = 0; i < tips.size(); i++)
    {
        std::cout << tips[i].transpose() << std::endl;
    }
}



Eigen::Vector3d SceneWithCameras::get_center(Eigen::AlignedBox3d B, Eigen::Matrix4d transformation)
{
    auto C = B.center();
    Eigen::Vector4d temp = transformation * Eigen::Vector4d{ C(0), C(1), C(2), 1 };
    return { temp(0), temp(1), temp(2) };
}

bool SceneWithCameras::OBB_intersect(std::shared_ptr<cg3d::Model> M1, std::shared_ptr<cg3d::Model> M2, Eigen::AlignedBox3d box1, Eigen::AlignedBox3d box2)
{
    // set parameters
    Eigen::Vector3d C0 = get_center(box1, M1->GetAggregatedTransform().cast<double>()),
        C1 = get_center(box2, M2->GetAggregatedTransform().cast<double>());
    Eigen::Matrix3d A = M1->GetRotation().cast<double>();
    Eigen::Matrix3d B = M2->GetRotation().cast<double>();

    Eigen::Matrix3d C = A.transpose() * B;
    Eigen::RowVector3d A0 = A.col(0).transpose(), A1 = A.col(1).transpose(), A2 = A.col(2).transpose();
    Eigen::RowVector3d B0 = B.col(0).transpose(), B1 = B.col(1).transpose(), B2 = B.col(2).transpose();
    Eigen::Vector3d D = C1 - C0;
    Eigen::Matrix4d scale1 = M1->GetScaling(M1->GetAggregatedTransform()).matrix().cast<double>();
    Eigen::Matrix4d scale2 = M2->GetScaling(M2->GetAggregatedTransform()).matrix().cast<double>();

    double a0 = box1.sizes()(0) * scale1(0, 0) / 2, a1 = box1.sizes()(1) * scale1(1, 1) / 2, a2 = box1.sizes()(2) * scale1(2, 2) / 2,
        b0 = box2.sizes()(0) * scale2(0, 0) / 2, b1 = box2.sizes()(1) * scale2(1, 1) / 2, b2 = box2.sizes()(2) * scale2(2, 2) / 2;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            C(i, j) = ABS(C(i, j));
    // test if the boxes intersects
    if (
           (ABS((A0 * D)(0)) <= a0 + b0 * C(0, 0) + b1 * C(0, 1) + b2 * C(0, 2))         // A0
        && (ABS((A1 * D)(0)) <= a1 + b0 * C(1, 0) + b1 * C(1, 1) + b2 * C(1, 2))         // A1
        && (ABS((A2 * D)(0)) <= a2 + b0 * C(2, 0) + b1 * C(2, 1) + b2 * C(2, 2))         // A2
        && (ABS((B0 * D)(0)) <= b0 + a0 * C(0, 0) + a1 * C(0, 1) + a2 * C(0, 2))         // B0
        && (ABS((B1 * D)(0)) <= b1 + a0 * C(1, 0) + a1 * C(1, 1) + a2 * C(1, 2))         // B1
        && (ABS((B2 * D)(0)) <= b2 + a0 * C(2, 0) + a1 * C(2, 1) + a2 * C(2, 2))         // B2
        && (ABS(C(1, 0) * (A2 * D)(0) - C(2, 0) * (A1 * D)(0)) <= a1 * C(2, 0) + a2 * C(1, 0) + b1 * C(0, 2) + b2 * C(0, 1))        // A0 x B0
        && (ABS(C(1, 1) * (A2 * D)(0) - C(2, 1) * (A1 * D)(0)) <= a1 * C(2, 1) + a2 * C(1, 1) + b0 * C(0, 2) + b2 * C(0, 0))        // A0 x B1
        && (ABS(C(1, 2) * (A2 * D)(0) - C(2, 2) * (A1 * D)(0)) <= a1 * C(2, 2) + a2 * C(1, 2) + b0 * C(0, 1) + b1 * C(0, 0))        // A0 x B2
        && (ABS(C(2, 0) * (A0 * D)(0) - C(0, 0) * (A2 * D)(0)) <= a0 * C(2, 0) + a2 * C(0, 0) + b1 * C(1, 2) + b2 * C(1, 1))        // A1 x B0
        && (ABS(C(2, 1) * (A0 * D)(0) - C(0, 1) * (A2 * D)(0)) <= a0 * C(2, 1) + a2 * C(0, 1) + b0 * C(1, 2) + b2 * C(1, 0))        // A1 x B1
        && (ABS(C(2, 2) * (A0 * D)(0) - C(0, 2) * (A2 * D)(0)) <= a0 * C(2, 2) + a2 * C(0, 2) + b0 * C(1, 1) + b1 * C(1, 0))        // A1 x B2
        && (ABS(C(0, 0) * (A1 * D)(0) - C(1, 0) * (A0 * D)(0)) <= a0 * C(1, 0) + a1 * C(0, 0) + b1 * C(2, 2) + b2 * C(2, 1))        // A2 x B0
        && (ABS(C(0, 1) * (A1 * D)(0) - C(1, 1) * (A0 * D)(0)) <= a0 * C(1, 1) + a1 * C(0, 1) + b0 * C(2, 2) + b2 * C(2, 0))        // A2 x B1
        && (ABS(C(0, 2) * (A1 * D)(0) - C(1, 2) * (A0 * D)(0)) <= a0 * C(1, 2) + a1 * C(0, 2) + b0 * C(2, 1) + b1 * C(2, 0)))       // A2 x B2
        return true;
    else
        return false;
}


bool SceneWithCameras::collision(std::shared_ptr<cg3d::Model> M1, std::shared_ptr<cg3d::Model> M2)

{
    auto tree1 = M1->obb_tree;
    auto tree2 = M2->obb_tree;
    if (OBB_intersect(M1, M2, tree1.m_box, tree2.m_box))
    {
        return true;
    }
    return false;
}


void SceneWithCameras::init_prices(int number_of_prices)
{
    while (!prices.empty())
    {
        root->RemoveChild(prices[prices.size() - 1]);
        prices.pop_back();
    }
    

    auto sphereMesh{ IglLoader::MeshFromFiles("sphere_igl", "data/sphere.obj") };

    for (int i = 0; i < number_of_prices; i++)
    {
        auto p = Model::Create("price", sphereMesh, carbon);
        p->obb_tree.init(p->GetMeshList()[0]->data[0].vertices, p->GetMeshList()[0]->data[0].faces);
        p->isHidden = true;
        root->AddChild(p);
        float x = std::rand(), y = std::rand(), sx = std::rand(), sy = std::rand(), max = RAND_MAX;
        sx = sx / max > 0.5 ? 1 : -1;
        sy = sy / max > 0.5 / 2 ? 1 : -1;
        x = sx * 15 * x / max;
        y = sy * 15 * y / max;
        p->Translate({ x, 0.8f, y });
        p->Scale(0.5, Axis::Z);
        prices.push_back(p);
    }
    curr_price = prices.size() - 1;
    prices[curr_price]->isHidden = false;
    total_prices = prices.size();
}


void SceneWithCameras::init_enemies(int number_of_enemies)
{

    while (!enemies.empty())
    {
        root->RemoveChild(enemies[enemies.size() - 1]);
        enemies.pop_back();
    }


    auto program = std::make_shared<Program>("shaders/phongShader"); 
    auto bricks{ std::make_shared<Material>("bricks", program) };
    auto knightMesh{ IglLoader::MeshFromFiles("knight_igl", "data/decimated-knight.off") };

    for (int i = 0; i < number_of_enemies; i++)
    {
        auto p = Model::Create("enemy", knightMesh, bricks);
        p->obb_tree.init(p->GetMeshList()[0]->data[0].vertices, p->GetMeshList()[0]->data[0].faces);
        root->AddChild(p);
        p->Scale(4);
        p->SetCenter({ 0.4f, 0.4f, 0.4f });
        
        float x = std::rand(), y = std::rand(), sx = std::rand(), sy = std::rand(), max = RAND_MAX;
        sx = sx / max > 0.5 ? 1 : -1;
        sy = sy / max > 0.5 / 2 ? 1 : -1;
        x = sx * 15 * x / max;
        y = sy * 15 * y / max;
        
        p->Translate({ x, 0.0, y });
        enemies.push_back(p);
    }
    

}


void SceneWithCameras::init_rotation_list()
{
    for (int i = 0; i < number_of_cyls; i++)
    {
        rotation_list.push_back(0.0f);
    }
}

void SceneWithCameras::update_rotation_list(float angle)
{
    if(!rotation_list.empty())
        rotation_list.pop_back();
    {
        rotation_list.insert(rotation_list.begin(), angle);
    }
    
}


void SceneWithCameras::init_ammo(int number_of_bullets)
{
    if (ammo.size() == 0)
    {
        auto sphereMesh{ IglLoader::MeshFromFiles("sphere_igl", "data/sphere.obj") };
        for (int i = 0; i < number_of_bullets; i++)
        {
            auto b = Model::Create("bullet", sphereMesh, carbon);
            b->isHidden = true;
            b->obb_tree.init(b->GetMeshList()[0]->data[0].vertices, b->GetMeshList()[0]->data[0].faces);
            b->Scale(0.6);
            ammo.push_back(b);

        }
    }

}


void SceneWithCameras::shoot()
{
    if (!ammo.empty())
    {
        auto b = ammo[ammo.size() - 1];
        ammo.pop_back();
        b->SetTransform(cyls[0]->GetTransform());
        b->isHidden = false;
        root->AddChild(b);
        bullets.insert(bullets.begin(), b);
        PlaySound("data/music/laser.wav", NULL, SND_ASYNC);
    }
    else
    {

    }
    
}

void SceneWithCameras::animate_bullets()
{
    for (int i = 0; i < bullets.size(); i++)
    {
        Eigen::Vector3f pos = get_position(bullets[i]);
        
        if (pos.norm() > 50)
        {
            for (int j = bullets.size() - 1; j >= i; j--)
            {
                auto b = bullets[j];
                b->isHidden = true;
                bullets.pop_back();
                ammo.push_back(b);
            }
            break;
        }
        else 
        { 
            for (int j = 0; j < enemies.size(); j++)
            {     
                if (!bullets[i]->isHidden && !(enemies[j]->fall || enemies[j]->isHidden) && collision(bullets[i], enemies[j]))
                {
                    bullets[i]->isHidden = true;
                    enemies[j]->fall = true;
                    PlaySound("data/music/hit.wav", NULL,SND_ASYNC);
                    auto program = std::make_shared<Program>("shaders/phongShader");
                    auto bricks{ std::make_shared<Material>("bricks", program) };
                    auto knightMesh{ IglLoader::MeshFromFiles("knight_igl", "data/decimated-knight.off") };
                    auto p = Model::Create("enemy", knightMesh, bricks);
                    p->obb_tree.init(p->GetMeshList()[0]->data[0].vertices, p->GetMeshList()[0]->data[0].faces);
                    root->AddChild(p);
                    p->Scale(4);
                    p->SetCenter({ 0.4f, 0.4f, 0.4f });

                    float x = std::rand(), y = std::rand(), sx = std::rand(), sy = std::rand(), max = RAND_MAX;
                    sx = sx / max > 0.5 ? 1 : -1;
                    sy = sy / max > 0.5 / 2 ? 1 : -1;
                    x = sx * 15 * x / max;
                    y = sy * 15 * y / max;

                    p->Translate({ x, 0.0, y });
                    enemies.push_back(p);

                }
                else
                {
                    auto R = bullets[i]->GetRotation().transpose();
                    bullets[i]->TranslateInSystem(R, { -0.5f, 0.0f, 0.0f });
                }               
            }
        }               
    }
}


void SceneWithCameras::animate_enemies()
{
    for (int i = 0; i < enemies.size(); i++)
    {
        for (int j = 0; j < cyls.size(); j += 8)
        {
            if (enemies[i]->fall)
            {
                Eigen::Vector3f angles = enemies[i]->GetRotation().eulerAngles(2, 0, 2);
                std::cout << angles << std::endl;
                if (angles(2) >= 3.1415 / 2.0f)
                {
                    enemies[i]->fall = false;
                    enemies[i]->isHidden = true;
                }
                enemies[i]->RotateByDegree(5, Axis::Z);
                enemies[i]->Translate({0.0f, -0.02f, 0.0f});
            }
            else if (!(enemies[i]->fall || enemies[i]->isHidden) && timer + 4000 < clock() && collision(enemies[i], cyls[j]))
            {
                PlaySound("data/music/snake_col.wav", NULL, SND_ASYNC);
                hp -= 35;
                timer = std::clock();
                if (hp <= 0)
                {
                    PlaySound("data/music/stage_failed.wav", NULL, SND_ASYNC);
                    if (hp <= 0)
                    {
                        hp = 0;
                        pause = true;
                        state = STAGE_FAILED;
                    }
                }
            }
            
        }
        if (stage >= 2)
        {
            Eigen::Vector3f snake_pos = get_position(cyls[0]);
            Eigen::Vector3f enemy_pos = get_position(enemies[i]);

            float cross_prod = (snake_pos(0) * enemy_pos(0) + snake_pos(2) * enemy_pos(2));
            cross_prod = cross_prod / std::sqrt(snake_pos(0) * snake_pos(0) + snake_pos(2) * snake_pos(2));
            cross_prod = cross_prod / std::sqrt(enemy_pos(0) * enemy_pos(0) + enemy_pos(2) * enemy_pos(2));
            float angle = std::acos(cross_prod);
            if (angle > 0.3 || angle < -0.3)
            {
                enemies[i]->RotateByDegree(angle / 5.0f, Axis::Y);
            }

            if (enemy_pos(2) > 14 && enemies[i]->direction > 0)
            {
                enemies[i]->direction = -1;
            }
            if (enemy_pos(2) < -14 && enemies[i]->direction < 0)
            {

                enemies[i]->direction = -1;
            }
            enemies[i]->Translate({ 0.0f, 0.0f, 0.05f * enemies[i]->direction });

            if (stage == 3)
            {
                if (std::clock() % 3000 == 0)
                {
                    enemy_shoot(i);
                }
            }
        }
        
        
    }
}


void SceneWithCameras::animate_cyls()
{
    Eigen::Matrix3f R = cyls[0]->GetRotation().transpose();
    cyls[0]->TranslateInSystem(R, { -0.05f * speed, 0.0f, 0.0f });
    
    // Rotate cyls
    for (int i = 1; i < number_of_cyls; i++)
    {
        to_rotate = i;
        auto cyl1 = cyls[to_rotate];
        Eigen::Vector3f r1, r2;
        r1 = { 1.0f, 0.0f, 0.0f };
        r2 = cyl1->Tout.rotation() * Eigen::Vector3f(1.0f, 0.0f, 0.0f);
        Eigen::Quaternionf q = Eigen::Quaternionf::FromTwoVectors(r2, r1);
        q = q.slerp(0.9f, Eigen::Quaternionf::Identity());
        cyl1->Rotate(q);
        if (to_rotate < number_of_cyls - 1)
        {
            cyls[to_rotate + 1]->Rotate(q.inverse());
        }

    }

    /*for (int i = 0; i < rotation_list.size(); i++)
    {
        Eigen::Matrix3f R = cyls[i]->GetRotation().transpose();
        cyls[i]->TranslateInSystem(R, { -0.05f, 0.0f, 0.0f });;
        cyls[i]->Rotate(rotation_list[i], Axis::Z);
    }

    update_rotation_list(0.0f);*/
    // Translate cyls
   /* Eigen::Matrix3f R_p, R_s;
    int i_p = 0, i_s = 1;
    while (i_p < cyls.size())
    {
        R_p = cyls[i_p]->GetRotation().transpose();
        cyls[i_p]->TranslateInSystem(R_p, { -0.01f, 0.0f, 0.0f });
        if (i_s < cyls.size())
        {
            cyls[i_s]->TranslateInSystem(R_p, { 0.01f, 0.0f, 0.0f });

        }
        i_p++;
        i_s++;
    }*/
}


void SceneWithCameras::animate_prices()
{
    if (prices.size() == 0)
    {
        pause = true;
        PlaySound("data/music/stage_win.wav", NULL, SND_ASYNC);
        state = STAGE_CLEAR;
    }
    if (curr_price >= 0 && collision(prices[curr_price], cyls[0]))
    {
        PlaySound("data/music/collect.wav", NULL, SND_ASYNC);
        auto program2 = std::make_shared<Program>("shaders/phongShader");
        auto grass{ std::make_shared<Material>("grass", program2) };
        auto c = Model::Create("cyl", Mesh::Cylinder(), grass);
        c->Scale(scaleFactor, Axis::X);
        c->Translate(1.6f * scaleFactor, Axis::X);
        c->SetCenter(Eigen::Vector3f(-0.8f * scaleFactor, 0, 0));
        cyls[cyls.size() - 1]->AddChild(c);
        cyls.push_back(c);
        number_of_cyls = cyls.size();
        prices[curr_price]->isHidden = true;
        prices.pop_back();
        curr_price--;
        speed += 0.1;
        hp = hp + 10;
        if (hp > 100)
        {
            hp = 100;
        }
        points += 1;
        if (curr_price >= 0)
        {
            prices[curr_price]->isHidden = false;
        }
    }
}



Eigen::Vector3f SceneWithCameras::get_position(std::shared_ptr<cg3d::Model> m)
{
    Eigen::Vector4f initial_pos = { 0.0f, 0.0f, 0.0f, 1.0f };
    Eigen::Vector4f current_pos = m->GetTransform() * initial_pos;
    Eigen::Vector3f pos = root->GetRotation() * Eigen::Vector3f({ current_pos(0), current_pos(1), current_pos(2) });
    return pos;
}



void SceneWithCameras::init_stage(int stage)
{
    cyls[0]->SetTransform(init_transform);
    pause = true;

    switch (stage)
    {
    case 1:
        init_stage1();
        break;
    case 2: 
        init_stage2();
        break;
    case 3:
        init_stage3();
        break;
    }
}

void SceneWithCameras::init_stage1()
{
    stage = 1;
    hp = 100;
    points = 0;
    stage1_background->isHidden = false;
    stage2_background->isHidden = true;
    stage3_background->isHidden = true;
    

    init_enemies(1);
    init_prices(5);
    init_rotation_list();
    init_ammo(5);
}
void SceneWithCameras::init_stage2()
{
    stage = 2;
    hp = 100;
    points = 0;
    stage1_background->isHidden = true;
    stage2_background->isHidden = false;
    stage3_background->isHidden = true;


    init_enemies(2);
    init_prices(6);
    init_rotation_list();
    init_ammo(5);
}
void SceneWithCameras::init_stage3()
{
    stage = 3;
    hp = 100;
    points = 0;
    stage1_background->isHidden = true;
    stage2_background->isHidden = true;
    stage3_background->isHidden = false;


    init_enemies(3);
    init_prices(8);
    init_rotation_list();
    init_ammo(5);
}



void SceneWithCameras::enemy_shoot(int i)
{
    auto e = enemies[i];
    Eigen::Matrix3f R = e->GetRotation();

}


void SceneWithCameras::self_collision()
{
    if (timer + 3000 < std::clock())
    {
        for (int i = 2; i < cyls.size(); i += 1)
        {

            if (timer + 3000 < std::clock() && collision(cyls[0], cyls[i]))
            {

                PlaySound("data/music/snake_col.wav", NULL, SND_ASYNC);
                hp -= 50;
                timer = std::clock();
                if (hp <= 0)
                {
                    PlaySound("data/music/stage_failed.wav", NULL, SND_ASYNC);
                    hp = 0;
                    pause = true;
                    state = STAGE_FAILED;
                }
            }
        }

    }
    
}


void SceneWithCameras::remove_from_vector(std::vector<std::shared_ptr<cg3d::Model>> *v, int i)
{
    if (i < 0 || i >= v->size())
    {
        std::cout << "out of range" << std::endl;
    }
    else
    {
        std::vector<std::shared_ptr<cg3d::Model>> stack;
        int to_pop = v->size() - 1 - i;
        
        while (to_pop >= 0)
        {
            std::shared_ptr<cg3d::Model> p = (*v)[v->size() - 1];
            v->pop_back();
            
            stack.push_back(p);
        }
        stack.pop_back();
        while (!stack.empty())
        {
            std::shared_ptr<cg3d::Model> p = stack[stack.size() - 1];
            stack.pop_back();

            v->push_back(p);
        }

    }
}



void SceneWithCameras::play_background_music()
{
    PlaySound("data/music/background.wav", NULL, SND_LOOP | SND_ASYNC);
}