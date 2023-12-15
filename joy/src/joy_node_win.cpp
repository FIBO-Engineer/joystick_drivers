/*
The MIT License (MIT)

Copyright (c) 2016 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "ros/ros.h"
#include <diagnostic_updater/diagnostic_updater.h>
#include <sensor_msgs/Joy.h>


#include "SDL.h"
#include "SDL_gamecontroller.h"


class Joystick
{
  ros::NodeHandle _joyNodeHandle;
  ros::NodeHandle _joyNodeHandlePrivate;
  bool _sticky_buttons;
  bool _default_trig_val;
  bool _ackermann;
  double _deadzone;
  double _autorepeat_rate;   // in Hz.  0 for no repeat.
  double _coalesce_interval; // Defaults to 100 Hz rate limit.
  double _unscaled_deadzone;
  int _pub_count;
  int _vid_param;
  int _pid_param;
  int _vid_current;
  int _pid_current;
  ros::Publisher _joystickPublisher;
  std::string _mappingsFile;

  SDL_Joystick* _gameController = nullptr;
  int16_t _gameControllerIndex;

  public:
  Joystick() 
  : _joyNodeHandle()
  , _gameControllerIndex(-1)
  , _joyNodeHandlePrivate("~")
    {}

  ///\brief Opens joystick port, reads from port and publishes while node is active
  int main(int argc, char **argv)
  {
    _joyNodeHandlePrivate.param<int>("vid", _vid_param, 0);
    _joyNodeHandlePrivate.param<int>("pid", _pid_param, 0);
    _joyNodeHandlePrivate.param<double>("deadzone", _deadzone, 0.05);
    _joyNodeHandlePrivate.param<double>("autorepeat_rate", _autorepeat_rate, 0);
    _joyNodeHandlePrivate.param<double>("coalesce_interval", _coalesce_interval, 0.001);
    _joyNodeHandlePrivate.param<bool>("default_trig_val", _default_trig_val,false);
    _joyNodeHandlePrivate.param<bool>("sticky_buttons", _sticky_buttons, false);
    _joyNodeHandlePrivate.param<std::string>("mappings", _mappingsFile, "");

    if (_coalesce_interval < 0)
    {
      ROS_WARN("joy_node: coalesce_interval (%f) less than 0, setting to 0.", _coalesce_interval);
      _coalesce_interval = 0;
    }

    if (_coalesce_interval != 0 && (_autorepeat_rate > 1 / _coalesce_interval))
    {
      ROS_WARN("joy_node: autorepeat_rate (%f Hz) > 1/coalesce_interval (%f Hz) does not make sense. Timing behavior is not well defined.", _autorepeat_rate, 1/_coalesce_interval);
    }

    if (_deadzone >= 1)
    {
      ROS_WARN("joy_node: deadzone greater than 1 was requested. The semantics of deadzone have changed. It is now related to the range [-1:1] instead of [-32767:32767]. For now I am dividing your deadzone by 32767, but this behavior is deprecated so you need to update your launch file.");
      _deadzone /= 32767;
    }

    if (_deadzone > 0.9)
    {
      ROS_WARN("joy_node: deadzone (%f) greater than 0.9, setting it to 0.9", _deadzone);
      _deadzone = 0.9;
    }

    if (_deadzone < 0)
    {
      ROS_WARN("joy_node: deadzone_ (%f) less than 0, setting to 0.", _deadzone);
      _deadzone = 0;
    }

    if (_autorepeat_rate < 0)
    {
      ROS_WARN("joy_node: autorepeat_rate (%f) less than 0, setting to 0.", _autorepeat_rate);
      _autorepeat_rate = 0;
    }

    _joystickPublisher = _joyNodeHandle.advertise<sensor_msgs::Joy>("joy", 1); 


    // Parameter conversions
    double autorepeat_interval = 1 / _autorepeat_rate;
    double scale = -1. / (1. - _deadzone) / 32767.;
    _unscaled_deadzone = 32767. * _deadzone;

    // Rate is measured in Hz
    ros::Rate loop_rate(100);
    ros::Rate nojoy_rate(1);
    sensor_msgs::Joy joy_msg;
    sensor_msgs::Joy last_published_joy_msg; // used for sticky buttons option
    sensor_msgs::Joy sticky_buttons_joy_msg; // used for sticky buttons option

    joy_msg.buttons.resize(SDL_CONTROLLER_BUTTON_MAX, 0);
    last_published_joy_msg.buttons.resize(SDL_CONTROLLER_BUTTON_MAX, 0);
    sticky_buttons_joy_msg.buttons.resize(SDL_CONTROLLER_BUTTON_MAX, 0);

    joy_msg.axes.resize(SDL_CONTROLLER_AXIS_MAX, 0);
    last_published_joy_msg.axes.resize(SDL_CONTROLLER_AXIS_MAX, 0);
    sticky_buttons_joy_msg.axes.resize(SDL_CONTROLLER_AXIS_MAX, 0);

    //Initialize SDL
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0)
    {
        ROS_ERROR( "SDL could not initialize! SDL Error: %s\n", SDL_GetError() );

        // todo - is this a recoverable error?
        return 0;
    }

    if (!_mappingsFile.empty())
    {
      if (SDL_GameControllerAddMappingsFromFile(_mappingsFile.c_str()) < 0)
      {
          ROS_WARN( "SDL could not initialize mapping file.\n%s\n SDL Error: %s\n", _mappingsFile.c_str(), SDL_GetError() );
          // not fatal.
      }
    }

    while (ros::ok())
    {

      if (_gameController == nullptr)
      {
        SDL_JoystickUpdate();
        int nJoysticks = SDL_NumJoysticks();
        for (int i = 0; i < nJoysticks; i++) 
        {
          if (SDL_IsGameController(i)) 
          {
            _gameController = SDL_JoystickOpen(i);
            if (_gameController == nullptr)
            {
              ROS_ERROR( "SDL reported a game controller, but would not open it! SDL Error: %s\n", SDL_GetError() );
            }
            else
            {
              _gameControllerIndex = i;
              break;
            }
          }
        }
      }

      if (_gameController != nullptr)
      {
          SDL_Event e;
          while (SDL_PollEvent(&e))
          {
            std::cout << e.type << std::endl;
            // ROS_WARN("Triggered");
            switch (e.type)
            {
              case SDL_JOYAXISMOTION:
              {
                  //Motion on controller 0
                  if (e.jaxis.which == _gameControllerIndex)
                  {     
                    float value = e.jaxis.value;                   
                    if (value > _unscaled_deadzone)
                    {
                      value -= _unscaled_deadzone;
                    }
                    else if (value < -_unscaled_deadzone)
                    {
                      value += _unscaled_deadzone;
                    }
                    else
                    {
                      value = 0;
                    }

                    joy_msg.axes[e.jaxis.axis] = value * scale;
                    ROS_WARN("Axis triggered");
                  }
              }
              break;

              case SDL_JOYBUTTONDOWN:
              {
                  joy_msg.buttons[e.jbutton.button] = 1.0;
                  // joy_msg.buttons[e.cbutton.button] = 1.0;
                  // ROS_WARN("Button Down %d", e.jbutton.button);

              }
              break;

              case SDL_JOYBUTTONUP:
              {
                  joy_msg.buttons[e.jbutton.button] = 0.0;
                  // ROS_WARN("Button Up %d", e.jbutton.button);
              }
              break;

              case SDL_JOYHATMOTION:
              {
                  if(e.jhat.which == _gameControllerIndex)
                  {
                      double jvalue = e.jhat.value;
                      ROS_WARN("e.jhat.hat = %d", e.jhat.hat);
                      if(jvalue == SDL_HAT_CENTERED){
                        joy_msg.buttons[10] = 0.0;
                        joy_msg.buttons[11] = 0.0;
                        joy_msg.buttons[12] = 0.0;
                        joy_msg.buttons[13] = 0.0;
                        ROS_WARN("DPad Released");
                      }
                      if(jvalue == SDL_HAT_UP){
                        joy_msg.buttons[10] = 1.0;
                        ROS_WARN("DPad Up");
                      }
                      if(jvalue == SDL_HAT_DOWN){
                        joy_msg.buttons[11] = 1.0;
                        ROS_WARN("DPad Down");
                      }
                      if(jvalue == SDL_HAT_LEFT){
                        joy_msg.buttons[12] = 1.0;
                        ROS_WARN("DPad Left");
                      }
                      if(jvalue == SDL_HAT_RIGHT){
                        joy_msg.buttons[13] = 1.0;
                        ROS_WARN("DPad Right");
                      }
                  }
                  // joy_msg.buttons[e.jhat.hat] = 1.0;
                  // ROS_WARN("DPad Down %d", e.jhat.value);
              }
              break;

              case SDL_JOYDEVICEREMOVED:
              {
                if (e.jaxis.which == _gameControllerIndex)
                {     
                  _gameControllerIndex = -1;
                  SDL_JoystickClose(_gameController);
                  _gameController = nullptr;
                }
              }
              break;
            }
          }

          joy_msg.header.stamp = ros::Time().now();
          _joystickPublisher.publish(joy_msg);

          SDL_JoystickUpdate();
      }

      ros::spinOnce();

      if (_gameController != nullptr)
      {
        loop_rate.sleep();
      }
      else
      {
        nojoy_rate.sleep();
      }
    }

    if (_gameController != nullptr)
    {
      SDL_JoystickClose( _gameController );
      _gameController = nullptr;
    }

    return 0;
  }
  uint32_t getScaledDeadzone(uint32_t val)
  {
    if (val > _unscaled_deadzone)
    {
      val -= _unscaled_deadzone;
    }
    else if (val < -_unscaled_deadzone)
    {
      val += _unscaled_deadzone;
    }
    else
    {
      val = 0;
    }
    return val;
  }

};


int main(int argc, char **argv)
{
  ros::init(argc, argv, "joy_node");
 
  Joystick j;
  return j.main(argc, argv);
}