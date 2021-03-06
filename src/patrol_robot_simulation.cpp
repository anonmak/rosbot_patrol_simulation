#include <PatrolManager.h>
#include <darknet_ros_msgs/BoundingBoxes.h>
#include <ros/ros.h>
#include <rosbot_patrol_simulation/EspTrigger.h>
#include <std_msgs/Bool.h>
#include <stdio.h>
#include <vector>
#include <yaml-cpp/yaml.h>

using namespace std;

string params_path, email_from, email_to;
bool room_reached = {0};          //flags defining succes of task
bool starting_poit_reached = {0};
ros::Time last_email_sent;
ros::NodeHandle *nh_ptr;          //pointer for node handle to make our method makeSpin publish to cmd_vel topic 

vector<string> room_names{};
vector<double> x_coordinates{};
vector<double> y_coordinates{};
vector<double> th_coordinates{};

void sendMail(const char *to, const char *from, const char *subject,
              const char *message) {
  ROS_INFO("Sending mail!!!");
  FILE *mailpipe = popen("/usr/lib/sendmail -t", "w");
  if (mailpipe != NULL) {
    fprintf(mailpipe, "To: %s\n", to);
    fprintf(mailpipe, "From: %s\n", from);
    fprintf(mailpipe, "Subject: %s\n\n", subject);
    fwrite(message, 1, strlen(message), mailpipe);
    fwrite(".\n", 1, 2, mailpipe);
    pclose(mailpipe);
  } else {
    perror("Failed to invoke sendmail");
  }
}

void saveConfigFiles(string path) {
  ROS_INFO("path is : %s",path.c_str());
  YAML::Node conf_file = YAML::LoadFile(path.c_str());
  for (int i = 0; i < conf_file["rooms_num"].as<int>(); i++) {
    x_coordinates.push_back(
        conf_file["rooms"]["room" + to_string(i)]["x"].as<double>());
    y_coordinates.push_back(
        conf_file["rooms"]["room" + to_string(i)]["y"].as<double>());
    th_coordinates.push_back(
        conf_file["rooms"]["room" + to_string(i)]["angle"].as<double>());
    room_names.push_back(
        conf_file["rooms"]["room" + to_string(i)]["name"].as<std::string>());
  }
}

void darknetCallback(const darknet_ros_msgs::BoundingBoxes &bb_msg) {
  

  int size = bb_msg.bounding_boxes.size(); // ros msgs are mapped onto std::vector thats we can use size method
  for (int i = 0; i < size; i++) {
    
    if ((bb_msg.bounding_boxes[i].Class == "fire hydrant") or
        (bb_msg.bounding_boxes[i].Class == "person")) {
      ROS_INFO("Found fire hydrant or person/people");

      ros::Time time_now = ros::Time::now();
      if (time_now - last_email_sent  >= ros::Duration(10)) { //to prevent sending too many emails - causing to be marked as spam source
        sendMail(email_to.c_str(), email_from.c_str(), "rosbot patrol node","I have found something strange it could be invader");
        last_email_sent = time_now;
      }
    }
  }
}

void espCallback(const rosbot_patrol_simulation::EspTrigger &trigger_msg) {
  bool spin_made;

  if (trigger_msg.move == 1) {
    int esp_id = trigger_msg.id;
    PatrolManager pn(*nh_ptr);

    room_reached = pn.moveToGoal(room_names[esp_id], x_coordinates[esp_id],
                                 y_coordinates[esp_id], th_coordinates[esp_id]);
    if (room_reached) {
      ROS_INFO("I've reached destination");
    }

    spin_made = pn.makeSpin(M_PI , 0);
    spin_made = pn.makeSpin(M_PI , 0);
    if (spin_made) {
      ROS_INFO("I've just scanned room");
    }

    starting_poit_reached = pn.moveToGoal(room_names[0], x_coordinates[0],
                                          y_coordinates[0], th_coordinates[0]);
    if (starting_poit_reached) {
      ROS_INFO("I've reached starting point");
    }
  }
}

int main(int argc, char *argv[]) {
  ros::init(argc, argv, "patrol_robot_simulation");
  ros::NodeHandle nh("~");
  nh_ptr = &nh;
  nh.getParam("path_to_params", params_path);
  nh.getParam("email_to", email_to);
  nh.getParam("email_from", email_from);
  printf("path : %s",params_path.c_str());
  saveConfigFiles(params_path);
  

  ros::Subscriber sub_esp = nh.subscribe("/motion_trigger", 1, espCallback);
  ros::Subscriber sub_darknet =
      nh.subscribe("/darknet_ros/bounding_boxes", 10, darknetCallback);

  ros::Rate loop_rate(10);
  last_email_sent = ros::Time::now();  
  while (ros::ok) {

    ros::spinOnce();
    loop_rate.sleep();
  }

  return 0;
}
