#ifndef _RENDERER_H_
#define _RENDERER_H_

//Holds camera data and methods for updating the camera
class Camera{
    typedef enum {UI_POS_LEFT, UI_POS_TOP, UI_POS_RIGHT,
        UI_POS_BOTTOM, UI_POS_CENTER } ui_position;
    
public:
    Camera::Camera(double width, doulble height);
    Camera::Camera(double width, doulble height, glm::vec3 center,
                   glm::vec3 eye, glm::vec3 up);
    Camera::Camera(double width, doulble height,
                   glm::vec3 center, glm::vec3 eye, glm::vec3 up,
                   float fov, float near_plane, float far_plane);
    
    /*--------UI State-----------*/
    bool isDown = false; //Mouse is down.
    GLuint mouseButton; //Which mouse button is down
    //0 for all, 1 for fov, 2 for just X, 3 for just y, 4 for just z
    ui_position mode;
    
    bool shift_click = false;
    
    //The last screen space coordinates that a ui event occured at
    double last_x;
    double last_y;
    
    bool fixUp = false;
    
    /*------Camera State--------*/
    //Location at which the camera is pointed
    glm::vec3 center;
    //The loaction of the eyepoint of the camera
    glm::vec3 eye;
    //Up vector
    glm::vec3 up;
    
    float fov;
    float near_plane;
    float far_plane;
    
    /*-----Camera Matricies (Derived from camera state)-----*/
    glm::mat4 view;
    glm::mat4 proj;

    void update_view();
    void update_proj();
    void update_cam(){ update_view(); update_proj(); };
    
    void mouseClicked(int button,
                      int action, int mods);
    void mouseMoved(double x, double y);
    void keyPressed(int key,int scancode,int action,int mods);
    void scrollActive(double x , double y);
    
private:
    
    void rotate_diff(glm::vec3 diff);
    void translate_diff(glm::vec3 diff);
    
};

class Renderer{
public:
    std::vector<Shader*> shaders;
    Camera cam;
    
    void render_all();
    
private:
    /*External framebuffer (if allocated)*/
    GLuint fbo_ext = -1;
    GLuint rbo_ext_color;
    GLuint rbo_ext_depth;
};