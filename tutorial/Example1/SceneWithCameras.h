#pragma once

#include "SceneWithImGui.h"
#include "CamModel.h"
#include "igl/AABB.h"


#define GAME_START 0;
#define STAGE_START 1;
#define DURING_STAGE 2;
#define STAGE_CLEAR 3;
#define GAME_PAUSE 4;
#define STAGE_FAILED 5;
class SceneWithCameras : public cg3d::SceneWithImGui
{
private:
    std::shared_ptr<Movable> root;
    std::shared_ptr<cg3d::Model> cube1, cube2, cube3, cube4, snake, plane, background, background2;
    std::shared_ptr<cg3d::Model> stage1_background, stage2_background, stage3_background;
    int curr_price;
    std::vector<std::shared_ptr<cg3d::Model>> cyls;
    std::vector<std::shared_ptr<cg3d::Model>> prices;
    std::vector<std::shared_ptr<cg3d::Model>> enemies;
    std::vector<std::shared_ptr<cg3d::Model>> bullets;
    std::vector<std::shared_ptr<cg3d::Model>> ammo;
    std::vector<igl::AABB<Eigen::MatrixXf, 3>> boxes;
    std::vector<float> rotation_list;
    std::shared_ptr<cg3d::Material> carbon;
    bool animate = false;
    void BuildImGui() override;
    std::vector<std::shared_ptr<cg3d::Camera>> camList{2};
    int number_of_cyls = 16;
    cg3d::Viewport* viewport = nullptr;
    int to_rotate = 1;
    int points = 0, hp = 100;
    bool to_animate = false;
    float scaleFactor = 0.5;
    int col_start, col_finish;
    int turn = 0;
    float speed = 1.0f;
    bool enemies_shoot = false;
    bool pause = false;
    int state = GAME_START;
    std::clock_t timer;
    int total_prices = 0;
    int stage = 1;
    int enemies_killed = 0;

    
    std::vector<std::shared_ptr<cg3d::Model>> enemy_bullets;
    std::vector<std::shared_ptr<cg3d::Model>> enemy_ammo;

    Eigen::Matrix4f init_transform;
    Eigen::VectorXi EMAP;
    Eigen::MatrixXi OF, F, E, EF, EI;
    Eigen::VectorXi EQ;
    Eigen::MatrixXd N, C, C2, OV, V, T;


public:

    SceneWithCameras(std::string name, cg3d::Display* display);
    void Init(float fov, int width, int height, float near, float far);
    void Update(const cg3d::Program& program, const Eigen::Matrix4f& proj, const Eigen::Matrix4f& view, const Eigen::Matrix4f& model) override;
    void KeyCallback(cg3d::Viewport* _viewport, int x, int y, int key, int scancode, int action, int mods) override;
    void AddViewportCallback(cg3d::Viewport* _viewport) override;
    void ViewportSizeCallback(cg3d::Viewport* _viewport) override;
    void Animate() override;
    Eigen::Vector3d get_center(Eigen::AlignedBox3d B, Eigen::Matrix4d Transformation);
    bool OBB_intersect(std::shared_ptr<cg3d::Model> M1, std::shared_ptr<cg3d::Model> M2, Eigen::AlignedBox3d box1, Eigen::AlignedBox3d box2);
    bool collision(std::shared_ptr<cg3d::Model> M1, std::shared_ptr<cg3d::Model> M2);
    float ABS(float f) { return f >= 0 ? f : -1 * f; };
    double ABS(double d) { return d >= 0 ? d : -1 * d; };

private:
    inline bool IsActive() const { return animate; };
    inline void SetActive(bool _isActive = true) { animate = _isActive; }
    void LoadObjectFromFileDialog();
    void SetCamera(int index);
    static std::shared_ptr<CamModel> CreateCameraWithModel(int width, int height, float fov, float near, float far, const std::shared_ptr<cg3d::Material>& material);
    static void DumpMeshData(const Eigen::IOFormat& simple, const cg3d::MeshData& data) ;
    void print_tips();
    std::vector<Eigen::Vector3f> tips_position();
    Eigen::Vector3f get_position(std::shared_ptr<cg3d::Model> m);
    void init_prices(int number_of_prices);
    void init_enemies(int number_of_enemies);
    void init_ammo(int number_of_bullets);
    void init_rotation_list();
    void update_rotation_list(float angle);
    void shoot();
    void enemy_shoot(int i);
    void animate_bullets();
    void animate_enemies();
    void animate_cyls();
    void animate_prices();
    void self_collision();
    void init_stage(int stage);
    void init_stage1();
    void init_stage2();
    void init_stage3();
    void remove_from_vector(std::vector<std::shared_ptr<cg3d::Model>>* v, int i);
    void play_background_music() override;
};

