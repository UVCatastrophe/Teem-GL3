#include "renderer.h"

/*Constructors*/
Camera::Camera(double width, double height){
    this->scrnWidth = width;
    this->scrnHeight = height;
    
    this->center = glm::vec3(0.0f,0.0f,0.0f);
    this->eye = glm::vec3(3.0f,0.0f,0.0f);
    this->up = glm::vec3(0.0f,1.0f,0.0f);
    
    this->fov = 1.0f;
    this->near_plane = 1.0f;
    this->far_plane = 1000.0f;
    
}

Camera::Camera(double width, double height,glm::vec3 center, glm::vec3 eye,
               glm::vec3 up){
    
    this->scrnWidth = width;
    this->scrnHeight = height;
    
    this->center = center;
    this->eye = eye;
    this->up = up;
    
    this->fov = 1.0f;
    this->near_plane = 1.0f;
    this->far_plane = 1000.0f;
}

Camera::Camera(double width, double height,glm::vec3 center, glm::vec3 eye,
               glm::vec3 up, float fov, float near_plane, float far_plane){
    
    this->scrnWidth = width;
    this->scrnHeight = height;
    
    this->center = center;
    this->eye = eye;
    this->up = up;
    
    this->fov = fov;
    this->near_plane = near_plane;
    this->far_plane = far_plane;
}

void Camera::update_view(){
    view = glm::lookAt(eye,center,up);
}

void Camera::update_proj(){
    proj = glm::perspective(fov, ((float) scrnWidth)/((float)scrnHeight),
                            near_plane, far_plane);
}

void Camera::mouseClicked(int button, int action, int mods){
    glfwGetCursorPos (w, &(last_x), &(last_y));
    mouseButton = button;
    
    //Else, set up the mode for rotating/zooming
    if(action == GLFW_PRESS){
        isDown = true;
    }
    else if(action == GLFW_RELEASE){
        isDown = false;
    }
    
    if(last_x <= scrnWidth*.1)
        mode = UI_POS_LEFT;
    else if(last_x >= scrnWidth*.9)
        mode = UI_POS_RIGHT;
    else if(last_y <= scrnHeight*.1)
        mode = UI_POS_BOTTOM;
    else if(last_y >= scrnHeight*.9)
        mode = UI_POS_TOP;
    else
        mode = UI_POS_CENTER;
}

void Camera::mouseMoved(double x, double y){
    //If zooming/rotating is not occuring
    if(!isDown){
        return;
    }
    
    float x_diff = last_x - x;
    float y_diff = last_y - y;
    
    //Standard (middle of the screen mode)
    if(mode == UI_POS_CENTER){
        
        //Left-Click (Rotate)
        if(mouseButton == GLFW_MOUSE_BUTTON_1){
            rotate_diff(glm::vec3(-x_diff,y_diff,0.0f));
        }
        
        //Right-Click (Translate)
        else if(mouseButton == GLFW_MOUSE_BUTTON_2){
            translate_diff(glm::vec3(-x_diff,y_diff,0.0f));
        }
        
    }
    
    //Zooming Mode
    else if(mode == UI_POS_LEFT){
        
        //FOV zoom
        if(mouseButton == GLFW_MOUSE_BUTTON_1){
            fov += (-y_diff / scrnHeight);
            update_proj();
        }
        else if(ui.mouseButton == GLFW_MOUSE_BUTTON_2)
            translate_diff(glm::vec3(0.0f,0.0f,y_diff));
        
    }
    
    //modify u only
    else if(mode == UI_POS_TOP){
        if(mouseButton == GLFW_MOUSE_BUTTON_1){
            if(x_diff != 0.0) //Can't rotate by 0
                rotate_diff(glm::vec3(-x_diff,0.0f,0.0f));
        }
        else if(mouseButton == GLFW_MOUSE_BUTTON_2)
            translate_diff(glm::vec3(-x_diff,0.0f,0.0f));
    }
    
    //modify v only
    else if(mode == UI_POS_RIGHT){
        if(mouseButton == GLFW_MOUSE_BUTTON_1){
            if(y_diff != 0.0)
                rotate_diff(glm::vec3(0.0f,y_diff,0.0f));
        }
        else if(mouseButton == GLFW_MOUSE_BUTTON_2)
            translate_diff(glm::vec3(0.0f,y_diff,0.0f));
    }
    
    //modify w only
    else if(mode == UI_POS_BOTTOM){
        if(mouseButton == GLFW_MOUSE_BUTTON_1){
            float angle = (x_diff*3.1415*2) / scrnWidth;
            glm::mat4 rot = glm::rotate(glm::mat4(),angle,
                                        eye-center);
            eye = glm::vec3(rot * glm::vec4(eye,0.0));
            if(!fixUp)
                up = glm::vec3(rot*glm::vec4(up,0.0));
            update_view();
        }
        
    }
    
    last_x = x;
    last_y = y;

}

void Camera::keyPressed(int key,int scancode,int action,int mods){
    //TODO: add a reset key
    bool shift = (key == GLFW_KEY_LEFT_SHIFT) || (key == GLFW_KEY_RIGHT_SHIFT);
    if(shift && action == GLFW_PRESS){
        shift_click = true;
    }
    else if(shift && action == GLFW_RELEASE){
        shift_click = false;
    }
}

void Camera::scrollActive(double x, double y){
    return;
}

void Camera::rotate_diff(glm::vec3 diff){
    glm::mat4 inv = glm::inverse(view);
    glm::vec4 invV = inv * glm::vec4(diff,0.0);
    
    glm::vec3 norm = glm::cross(center-eye,glm::vec3(invV));
    float angle = (glm::length(diff) * 2*3.1415 ) / scrnWidth;
    
    //Create a rotation matrix around norm.
    glm::mat4 rot = glm::rotate(glm::mat4(),angle,glm::normalize(norm));
    //Align the origin with the look-at point of the camera.
    glm::mat4 trans = glm::translate(glm::mat4(), -center);
    pos = glm::vec3(glm::inverse(trans)*rot*trans*glm::vec4(eye,1.0));
    
    if(!fixUp)
        up = glm::vec3(rot*glm::vec4(up,0.0));
    
    update_view();
}

void Camera::translate_diff(glm::vec3 diff){
    glm::mat4 inv = glm::inverse(view);
    glm::vec4 invV = inv * glm::vec4(diff,0.0);
    
    glm::mat4 trans = glm::translate(glm::mat4(),
                                     glm::vec3(invV));
    center = glm::vec3(trans*glm::vec4(center,1.0));
    eye = glm::vec3(trans*glm::vec4(eye,1.0));
    update_view();
}

//Renderer
void Renderer::render_all(){
    for(int i = 0; i < shaders.length; i++){
        
    }
}