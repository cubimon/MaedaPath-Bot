#include <iostream>
#include <chrono>
#include <thread>

#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <X11/Xlib.h>
#include <X11/keysym.h>

// answer is 156145

enum class State {
  STARTED,
  END_FOUND,
  STOPPED
};

cv::VideoCapture cap;
cv::Rect crop_rect(5, 350, 607, 405);
Display* display;
Window root_window;

void MoveMouse(int x, int y) {
  XWarpPointer(display, root_window, root_window, 0, 0, 0, 0, x, y);
  XFlush(display);
}

bool IsKeyPressed(KeySym ks) {
  static char keys[32];
  XQueryKeymap(display, keys);
  KeyCode kc = XKeysymToKeycode(display, ks);
  return !!(keys[kc >> 3] & (1 << (kc & 7)));
}

void GetGameWindow(cv::Mat& game_window) {
  static cv::Mat screen;
  cap.read(screen);
  game_window = cv::Mat(screen, crop_rect);
}

std::pair<State, cv::Point> GetTargetPos(cv::Mat& image) {
  int first_y = 175;
  int block_count = 6;
  int block_size = 30;
  cv::Vec3b target_color(204, 204, 204);
  cv::Vec3b end_color(164, 148, 138);
  cv::Vec3b valid_color(0, 255, 0);
  cv::Vec3b invalid_color(0, 0, 255);
  int block_x = 10;
  int buffer_x = 25;
  for (int col = 0; col < 15; col++) {
      int block_x = 10 + block_size * col;
      for (int block_idx = 0; block_idx < block_count; block_idx++) {
          int block_y = first_y + block_idx * block_size;
          auto pixel = image.at<cv::Vec3b>(cv::Point(block_x, block_y));
          auto safe_pixel = image.at<cv::Vec3b>(cv::Point(
                block_x + buffer_x,
                block_y));
          if (pixel == end_color) {
              auto global_pos = cv::Point(
                  crop_rect.x + block_x,
                  crop_rect.y + block_y);
              std::cout << "stopped" << std::endl;
              return std::make_pair(State::END_FOUND, global_pos);
          }
          if (pixel == target_color && safe_pixel == target_color) {
              auto global_pos = cv::Point(
                  crop_rect.x + block_x,
                  crop_rect.y + block_y);
              image.at<cv::Vec3b>(block_y, block_x) = invalid_color;
              image.at<cv::Vec3b>(block_y, block_x + buffer_x) = invalid_color;
              return std::make_pair(State::STARTED, global_pos);
          }
      }
  }
  return std::make_pair(State::STARTED, cv::Point(0, 0));
}

int main(int argc, char* argv[]) {
  // init
  display = XOpenDisplay(":0");
  root_window = XRootWindow(display, 0);
  cap.set(cv::VideoCaptureProperties::CAP_PROP_BUFFERSIZE, 3);
  cv::Mat game_window;

  State state = State::STOPPED;
  int iterations = 0;
  auto start = std::chrono::high_resolution_clock::now();
  while (true) {
    if (IsKeyPressed(XK_1)) {
      if (state == State::STOPPED) {
        std::cout << "started" << std::endl;
      }
      cap = cv::VideoCapture(
          "ximagesrc ! video/x-raw,framerate=60/1 ! videoconvert ! appsink");
      state = State::STARTED;
    }
    if (IsKeyPressed(XK_2)) {
      if (state == State::STARTED) {
        std::cout << "stopped" << std::endl;
      }
      cap.release();
      state = State::STOPPED;
    }
    if (IsKeyPressed(XK_q)) {
      break;
    }
    if (state != State::STOPPED) {
      GetGameWindow(game_window);
      auto [new_state, point] = GetTargetPos(game_window);
      state = new_state;
      if (point.x != 0 && point.y != 0) {
        std::cout << "target point " << point << std::endl;
        MoveMouse(point.x, point.y);
      }
      cv::imshow("frame", game_window);
      cv::waitKey(1);
      if (state != State::END_FOUND) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        MoveMouse(0, 0);
      }
    }
    iterations++;
    auto end = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>
        (end - start).count() > 1000) {
      start = end;
      std::cout << "iterations/sec: " << iterations << std::endl;
      iterations = 0;
    }
  }

  // end
  XCloseDisplay(display);

  return 0;
}
