#include "ros/ros.h"
#include "ros/package.h"
#include "std_msgs/String.h"
//std
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>
#include <vector>
#include <time.h>
#include <map>
//toaster_msgs
#include "toaster_msgs/Entity.h"
#include "toaster_msgs/Object.h"
#include "toaster_msgs/AreaList.h"
#include "toaster_msgs/ObjectListStamped.h"
#include "toaster_msgs/HumanListStamped.h"
#include "toaster_msgs/FactList.h"
#include "toaster_msgs/RobotListStamped.h"
#include "toaster_msgs/Joint.h"
#include "toaster_msgs/Empty.h"
#include "toaster_msgs/Scale.h"

#include "markerCreator.h"

#include <tinyxml.h>
#include <tf/transform_listener.h>

//nameMarker rpoportionnal scale

class Run {
    visualization_msgs::MarkerArray area_list;
    visualization_msgs::MarkerArray obj_list;
    visualization_msgs::MarkerArray human_list;
    visualization_msgs::MarkerArray robot_list;
    visualization_msgs::MarkerArray arrow_list;

    std::vector<toaster_msgs::Fact> factList;

    //a vector to store allready treated marker's name and an id counter
    std::vector<std::string> name_list;
    int id_cpt;
    float objectNameScale_;
    float humanNameScale_;
    float robotNameScale_;
    float areaNameScale_;
    bool printNames_;

    //a vector to store marker's color
    std::map<std::string, std::vector<float> > color_map;
    std::map<std::string, double> agentMoving_map;


    //subscribers
    ros::Subscriber sub_objList;
    ros::Subscriber sub_areaList;
    ros::Subscriber sub_humanList;
    ros::Subscriber sub_robotList;
    ros::Subscriber sub_factList;
    ros::Subscriber sub_agentFactList;

    //publishers
    ros::Publisher pub_obj;
    ros::Publisher pub_area;
    ros::Publisher pub_human;
    ros::Publisher pub_robot;
    ros::Publisher pub_movingTwrd;

    TiXmlDocument listObj;
    TiXmlDocument listHuman;
    TiXmlDocument listJoint;
    TiXmlDocument listRobot;

public:

    /**
     * Constructor of the run class for toaster_visualizer
     */
    Run(ros::NodeHandle& node) {
        name_list = std::vector<std::string>();
        id_cpt = 1;
        printNames_ = true;

        objectNameScale_ = 0.2;
        humanNameScale_ = 0.3;
        robotNameScale_ = 0.3;
        areaNameScale_ = 0.3;

        area_list = visualization_msgs::MarkerArray();
        obj_list = visualization_msgs::MarkerArray();
        human_list = visualization_msgs::MarkerArray();
        robot_list = visualization_msgs::MarkerArray();
        arrow_list = visualization_msgs::MarkerArray();

        //definition of subscribers
        sub_objList = node.subscribe("/pdg/objectList", 1000, &Run::chatterCallbackObjList, this);
        sub_areaList = node.subscribe("/area_manager/areaList", 1000, &Run::chatterCallbackAreaList, this);
        sub_humanList = node.subscribe("/pdg/humanList", 1000, &Run::chatterCallbackHumanList, this);
        sub_robotList = node.subscribe("/pdg/robotList", 1000, &Run::chatterCallbackRobotList, this);
        sub_factList = node.subscribe("/pdg/factList", 1000, &Run::chatterCallbackFactList, this);
        sub_agentFactList = node.subscribe("/agent_monitor/factList", 1000, &Run::chatterCallbackAgentFactList, this);

        //definition of publishers
        pub_obj = node.advertise<visualization_msgs::MarkerArray>("/toaster_visualizer/marker_object", 1000);
        pub_area = node.advertise<visualization_msgs::MarkerArray>("/toaster_visualizer/marker_area", 1000);
        pub_human = node.advertise<visualization_msgs::MarkerArray>("/toaster_visualizer/marker_human", 1000);
        pub_robot = node.advertise<visualization_msgs::MarkerArray>("/toaster_visualizer/marker_robot", 1000);
        pub_movingTwrd = node.advertise<visualization_msgs::MarkerArray>("/toaster_visualizer/marker_motion", 1000);


        // ********************************** Services ****************************************//


        // **************************************** definition function of rviz markers ********************************************************

        openXmlFile("/src/list_obj.xml", listObj);
        openXmlFile("/src/list_human.xml", listHuman);
        openXmlFile("/src/list_human_joints.xml", listJoint);
        openXmlFile("/src/list_robot.xml", listRobot);
    }

    bool openXmlFile(std::string fileName, TiXmlDocument& doc)
    {
      std::stringstream path;
      path << ros::package::getPath("toaster_visualizer") << fileName;
      doc = TiXmlDocument(path.str());

      if (!doc.LoadFile()) {
          ROS_WARN_ONCE("Error while loading xml file");
          ROS_WARN_ONCE("error # %d", doc.ErrorId());
          ROS_WARN_ONCE("%s", doc.ErrorDesc());
      }
    }

    bool switchNamePrint(toaster_msgs::Empty::Request &req, toaster_msgs::Empty::Response &res) {
        ROS_INFO("[toaster_visu] switching name print");
        printNames_ = !printNames_;

        return true;
    }

    bool scaleName(toaster_msgs::Scale::Request &req, toaster_msgs::Scale::Response &res) {
        ROS_INFO("[toaster_visu] scaling printed names");

        objectNameScale_ = req.objectScale;
        humanNameScale_ = req.humanScale;
        robotNameScale_ = req.robotScale;
        areaNameScale_ = req.areaScale;

        return true;
    }


    // **************************************** definition function of rviz markers ********************************************************

  bool isActivated(std::string id)
  {
    bool active = false;
    for (unsigned int i = 0; i < factList.size(); i++)
    {
      if(factList[i].subjectId == id)
      {
        if(factList[i].stringValue == "active")
          active = true;
      }
    }
    return active;
  }


    // ******************************************************** id generator for markers  ********************************************************

    /**
     * In our context names are identifier so we need to create an unique identifier for each input name
     * @param name		name of target
     * @return id		new identifier or target's identifier if his name already have been assigned to an identifier
     */
    int id_generator(std::string name) {
        if (std::find(name_list.begin(), name_list.end(), name) != name_list.end()) {
            return std::find(name_list.begin(), name_list.end(), name) - name_list.begin() + 1;
        } else {
            name_list.push_back(name);
            return id_cpt++;
        }
    }

    bool isSeating(const toaster_msgs::Human hum) {
        //Find the head:
        unsigned int i = 0;
        for (i = 0; i < hum.meAgent.skeletonNames.size(); i++) {
            if (hum.meAgent.skeletonNames[i] == "head") {
                break;
            }
        }
        if (i == hum.meAgent.skeletonNames.size()) {
            return false;
        } else {
            if (hum.meAgent.skeletonJoint[i].meEntity.pose.position.z < 1.4)
                return true;
            else
                return false;
        }
    }


    // ******************************************************** adjustment functions ********************************************************

    /**
     * Function to modify a marker's color
     * @param marker		name of target marker
     * @param r 			value of red coefficient
     * @param g 			value of green coefficient
     * @param b 			value of blue coefficient
     * @return marker 		new marker with input modifications
     */
    visualization_msgs::Marker setColor(visualization_msgs::Marker marker, double r, double g, double b) {
        marker.color.r = r;
        marker.color.g = g;
        marker.color.b = b;

        return marker;
    }

    /**
     * Function to modify a marker's position
     * @param marker		name of target marker
     * @param x 			coordinates of marker's base in the x direction
     * @param y 			coordinates of marker's base in the y direction
     * @param z 			coordinates of marker's base in the z direction
     * @return marker 		new marker with input modifications
     */
    visualization_msgs::Marker setPosition(visualization_msgs::Marker marker, float x, float y, float z) {
        marker.pose.position.x = x;
        marker.pose.position.y = y;
        marker.pose.position.z = z;

        return marker;
    }

    /**
     * Function to modify a marker's size
     * @param marker		name of target marker
     * @param x 			size of the marker in the x direction
     * @param y 			size of the marker in the y direction
     * @param z 			size of the marker in the z direction
     * @return marker 		new marker with input modifications
     */
    visualization_msgs::Marker setSize(visualization_msgs::Marker marker, float x, float y, float z) {
        marker.scale.x = x;
        marker.scale.y = y;
        marker.scale.z = z;

        return marker;
    }

    /**
     * Function to give and register a random color to input marker
     * @param marker		name of target marker
     * @return marker 		new marker with input modifications
     */
    visualization_msgs::Marker setRandomColor(visualization_msgs::Marker marker) {
        if (color_map.find(marker.ns) == color_map.end()) {
            std::vector<float> color_list;
            color_list.push_back(static_cast<float> (rand()) / static_cast<float> (RAND_MAX));
            color_list.push_back(static_cast<float> (rand()) / static_cast<float> (RAND_MAX));
            color_list.push_back(static_cast<float> (rand()) / static_cast<float> (RAND_MAX));
            color_map[marker.ns] = color_list;
        }

        marker.color.r = (color_map[marker.ns])[0];
        marker.color.g = (color_map[marker.ns])[1];
        marker.color.b = (color_map[marker.ns])[2];
        return marker;
    }

    //******************************************************** emission/reception ********************************************************

    //reception

    /**
     * CallBack creating markers based on received toaster_msgs and adding then to obj_list
     * @param msg			reference to receive toaster_msgs::ObjectList
     * @return 			void
     */
    void chatterCallbackObjList(const toaster_msgs::ObjectListStamped::ConstPtr& msg) //toaster object list reception
    {
        obj_list.markers.clear();

        for (int i = 0; i < msg->objectList.size(); i++) {
            visualization_msgs::Marker m = MarkerCreator::defineObj(msg->objectList[i].meEntity.pose,
                                                      msg->objectList[i].meEntity.name,
                                                      isActivated(msg->objectList[i].meEntity.id),
                                                      id_generator(msg->objectList[i].meEntity.name),
                                                      listObj);

            if (printNames_) {
                visualization_msgs::Marker mn = MarkerCreator::defineName(m);
                //mn = setColor(mn, 0.0, 0.0, 1.0);
                mn = setSize(mn, 0, 0, objectNameScale_);

                obj_list.markers.push_back(mn);
            }
            obj_list.markers.push_back(m);

            ROS_DEBUG("obj %d", m.id);
        }
        // extra object for environment
        geometry_msgs::Pose p;
        p.position.x = 0.3;
        p.position.y = 0.3;
        p.position.z = -0.05;
        p.orientation.w = 1.0;
        visualization_msgs::Marker m = MarkerCreator::defineObj(p, "env", false, id_generator("env"), listObj);
        obj_list.markers.push_back(m);
    }

    /**
     * CallBack creating markers based on received toaster_msgs and adding then to area_list
     * There is two different types of area markers so this function manages both circle and polygon types
     * @param msg			reference to receive toaster_msgs::AreaList
     * @return 			void
     */
    void chatterCallbackAreaList(const toaster_msgs::AreaList::ConstPtr& msg) {
        area_list.markers.clear();

        for (int i = 0; i < msg->areaList.size(); i++) {

            if (msg->areaList[i].isCircle == true) //circle case
            {
                visualization_msgs::Marker m = MarkerCreator::defineCircle(msg->areaList[i].center,
                        msg->areaList[i].ray, msg->areaList[i].height, msg->areaList[i].name, id_generator(msg->areaList[i].name));

                m = setRandomColor(m);

                visualization_msgs::Marker mn = MarkerCreator::defineName(m);
                mn = setSize(mn, 0.0, 0.0, 0.3);
                mn = setPosition(mn, mn.pose.position.x, mn.pose.position.y, mn.pose.position.z + 0.5);

                area_list.markers.push_back(m);
                area_list.markers.push_back(mn);

                ROS_DEBUG("circle %d", m.id);
            } else // polygon case
            {
                visualization_msgs::MarkerArray m = MarkerCreator::definePolygon(msg->areaList[i].poly, msg->areaList[i].name, msg->areaList[i].zmin, msg->areaList[i].zmax);

               for(int i =0 ; i<3 ; i++)
                m.markers[i] = setRandomColor(m.markers[i]);

                visualization_msgs::MarkerArray mn ;
                for(int i =0 ; i<3 ; i++)
                { mn.markers.push_back(MarkerCreator::defineName(m.markers[i]));
                mn.markers.push_back(setSize(mn.markers[i], 0.0, 0.0, 0.1));}

                double posx = 0.0;
                double posy = 0.0;
                double posz = 0.0;
				for (int j = 0; j<3; j++)
               { for (int i = 0; i < m.markers[j].points.size(); i++) {
                    posx = posx + m.markers[j].points[i].x;
                    posy = posy + m.markers[j].points[i].y;
                    posz = posz + msg->areaList[i].zmin;
                }

                mn.markers[j] = setPosition(mn.markers[j], posx / m.markers[j].points.size(), posy / m.markers[j].points.size(),  + posz / m.markers[j].points.size());

                area_list.markers.push_back(m.markers[j]);
                area_list.markers.push_back(mn.markers[j]);
              }
               // ROS_DEBUG("poly %d", m.id);
            }

        }

    }
    /**
     * CallBack creating markers based on received toaster_msgs and adding then to robot_list
     * Robots can be represented by a single unarticulated mesh or by multiple meshs for an articulated model
     * @param msg			reference to receive toaster_msgs::RobotList
     * @return 			void
     */
    void chatterCallbackRobotList(const toaster_msgs::RobotListStamped::ConstPtr& msg) {
        robot_list.markers.clear();

        for (int i = 0; i < msg->robotList.size(); i++) {
            //non articulated robot
            visualization_msgs::Marker m = MarkerCreator::defineRobot(msg->robotList[i].meAgent.meEntity.pose,
                    1.0, msg->robotList[i].meAgent.meEntity.name,
                    id_generator(msg->robotList[i].meAgent.meEntity.name),
                    listRobot);

            if (printNames_) {
                visualization_msgs::Marker mn = MarkerCreator::defineName(m);
                mn = setPosition(mn, mn.pose.position.x, mn.pose.position.y, mn.pose.position.z + 1);
                mn = setSize(mn, 0, 0, robotNameScale_);

                //If the robot is moving, we intensify its name color
                std::map<std::string, double>::const_iterator it = agentMoving_map.find(msg->robotList[i].meAgent.meEntity.id);
                if (it != agentMoving_map.end())
                    mn = setColor(mn, 0.4 + it->second * 0.6, 0.0, 0.0);
                else
                    mn = setColor(mn, 0.2, 0.0, 0.0);


                robot_list.markers.push_back(mn);
            }

            robot_list.markers.push_back(m);

            ROS_DEBUG("robot %d", m.id);
        }
    }

    /**
     * CallBack creating markers based on received toaster_msgs and adding then to human_list
     * Humans can be represented by a single unarticulated mesh or by multiple meshs for an articulated model
     * @param msg			reference to receive toaster_msgs::HumanList
     * @return 			void
     */
    void chatterCallbackHumanList(const toaster_msgs::HumanListStamped::ConstPtr& msg) {
        human_list.markers.clear();

        for (int i = 0; i < msg->humanList.size(); i++) {
            //non articulated human
            visualization_msgs::Marker m = MarkerCreator::defineHuman(msg->humanList[i].meAgent.meEntity.pose,
                    1.0, msg->humanList[i].meAgent.meEntity.name,
                    id_generator(msg->humanList[i].meAgent.meEntity.name),
                    listHuman);

            if (printNames_) {
                visualization_msgs::Marker mn = MarkerCreator::defineName(m);
                mn = setPosition(mn, mn.pose.position.x, mn.pose.position.y, mn.pose.position.z + 1);
                mn = setSize(mn, 0, 0, humanNameScale_);

                //If the human is moving, we intensify its color
                std::map<std::string, double>::const_iterator it = agentMoving_map.find(msg->humanList[i].meAgent.meEntity.id);
                if (it != agentMoving_map.end())
                    mn = setColor(mn, 0.0, 0.4 + it->second * 0.6, 0.0);
                else
                    mn = setColor(mn, 0.0, 0.2, 0.0);

                human_list.markers.push_back(mn);
            }

            if (msg->humanList[i].meAgent.skeletonJoint.size() == 0) {
                human_list.markers.push_back(m);

                ROS_DEBUG("human %d", m.id);
            } else //articulated human
            {
                std::vector<toaster_msgs::Joint> joints = msg->humanList[i].meAgent.skeletonJoint;
                visualization_msgs::Marker markerTempo;

                int scale = 1;

                for (int y = 0; y < joints.size(); y++) {
                    ROS_DEBUG("joint");

                    std::string name = msg->humanList[i].meAgent.skeletonJoint[y].meEntity.name;

                    //frame id
                    markerTempo.header.frame_id = "map";

                    //namespace
                    markerTempo.ns = name;
                    markerTempo.id = id_generator(joints[y].meEntity.name); //creation of an unique id based on marker's name

                    //action
                    markerTempo.action = visualization_msgs::Marker::ADD;

                    // Pose
                    markerTempo.pose = joints[y].meEntity.pose;

                    //color
                    markerTempo.color.r = 1.0;
                    markerTempo.color.g = 1.0;
                    markerTempo.color.b = 1.0;
                    markerTempo.color.a = 1.0;

                    //scale
                    markerTempo.scale.x = 0.1 * scale;
                    markerTempo.scale.y = 0.1 * scale;
                    markerTempo.scale.z = 0.1 * scale;

                    //type of marker
                    markerTempo.type = 3;

                    TiXmlHandle hdl(&listJoint);
                    TiXmlElement *elem = hdl.FirstChildElement().FirstChildElement().Element();

                    std::string name_obj;
                    std::string mesh_r;

                    while (elem) //for each element of the xml file
                    {
                        name_obj = elem->Attribute("name");
                        mesh_r = elem->Attribute("mesh_resource");
                        elem = elem->NextSiblingElement();

                        if (name_obj.compare(name) == 0) //if there is a 3d model related to this object
                        {


                            markerTempo.scale.x = scale;
                            markerTempo.scale.y = scale;
                            markerTempo.scale.z = scale;


                            markerTempo.color.r = 0.0;
                            markerTempo.color.g = 0.0;
                            markerTempo.color.b = 0.0;
                            markerTempo.color.a = 0.0;

                            markerTempo.type = visualization_msgs::Marker::MESH_RESOURCE; //use it as mesh

                            if (isSeating(msg->humanList[i]) && name == "base") {
                                markerTempo.mesh_resource = "package://toaster_visualizer/mesh/toaster_humans/mocapMorse/baseSeat.dae"; //using 3d human model
                            } else {
                                markerTempo.mesh_resource = mesh_r;
                            }
                            markerTempo.mesh_use_embedded_materials = true;

                            elem = NULL;
                        }
                    }

                    markerTempo.lifetime = ros::Duration(1.0);

                    human_list.markers.push_back(markerTempo);
                }
            }
        }
    }

    void chatterCallbackFactList(const toaster_msgs::FactList::ConstPtr& msg)
    {
      factList.clear();
      for(unsigned int i = 0; i < msg->factList.size(); i++)
      {
        factList.push_back(msg->factList[i]);
      }
    }

    void chatterCallbackAgentFactList(const toaster_msgs::FactList::ConstPtr& msg) {
        agentMoving_map.clear();
        arrow_list.markers.clear();

        for (int iFact = 0; iFact < msg->factList.size(); iFact++) {
            if (msg->factList[iFact].property == "IsMoving")
                agentMoving_map[msg->factList[iFact].subjectId] = msg->factList[iFact].confidence;
            else if (msg->factList[iFact].property == "IsMovingToward") {

                bool foundSub = false;
                bool foundTarg = false;
                visualization_msgs::Marker sub;
                visualization_msgs::Marker targ;
                visualization_msgs::Marker arrow;

                //Let's look for the subject position:
                for (unsigned int i = 0; i < human_list.markers.size(); i++) {
                    if (human_list.markers[i].ns.compare(msg->factList[iFact].subjectId) == 0) {
                        sub = human_list.markers[i];
                        foundSub = true;
                        break;
                    }
                }

                if (foundSub) {
                    for (unsigned int i = 0; i < human_list.markers.size(); i++) {

                        if (human_list.markers[i].ns.compare(msg->factList[iFact].targetId) == 0) {

                            targ = human_list.markers[i];
                            foundTarg = true;
                            break;
                        }
                    }

                    if (!foundTarg) {
                        for (unsigned int i = 0; i < robot_list.markers.size(); i++) {
                            if (robot_list.markers[i].ns.compare(msg->factList[iFact].targetId) == 0) {
                                targ = robot_list.markers[i];
                                foundTarg = true;
                                break;
                            }
                        }
                        if (!foundTarg) {
                            for (unsigned int i = 0; i < obj_list.markers.size(); i++) {
                                if (obj_list.markers[i].ns.compare(msg->factList[iFact].targetId) == 0) {
                                    targ = obj_list.markers[i];
                                    foundTarg = true;
                                    break;
                                }
                            }
                            if (!foundTarg) {
                                continue;
                            }
                        }
                    }


                } else {
                    for (unsigned int i = 0; i < robot_list.markers.size(); i++) {
                        if (robot_list.markers[i].ns.compare(msg->factList[iFact].subjectId) == 0) {
                            sub = human_list.markers[i];
                            foundSub = true;
                            break;
                        }
                    }

                    if (foundSub) {
                        for (unsigned int i = 0; i < human_list.markers.size(); i++) {
                            if (human_list.markers[i].ns.compare(msg->factList[iFact].targetId) == 0) {
                                targ = human_list.markers[i];
                                foundTarg = true;
                                break;
                            }
                        }
                        if (!foundTarg) {
                            for (unsigned int i = 0; i < robot_list.markers.size(); i++) {
                                if (robot_list.markers[i].ns.compare(msg->factList[iFact].targetId) == 0) {
                                    targ = robot_list.markers[i];
                                    foundTarg = true;
                                    break;
                                }
                            }
                            if (!foundTarg) {
                                for (unsigned int i = 0; i < obj_list.markers.size(); i++) {
                                    if (obj_list.markers[i].ns.compare(msg->factList[iFact].targetId) == 0) {
                                        targ = human_list.markers[i];
                                        foundTarg = true;
                                        break;
                                    }
                                }
                                if (!foundTarg) {
                                    continue;
                                }
                            }
                        }
                    }
                }

                // Create arrow
                if (foundTarg && foundSub) {
                  bool distance = msg->factList[iFact].subProperty.compare("distance") == 0;
                  std::ostringstream nameSpace;
                  std::string subtype = "distance";
                  if (!distance)
                      subtype = "direction";
                  nameSpace << sub.ns << " MvTwd " << subtype << targ.ns;
                  arrow = MarkerCreator::defineArrow(sub, targ, msg->factList[iFact].confidence,
                                                    distance, id_generator(nameSpace.str()));
                  arrow_list.markers.push_back(arrow);
                }
            }
        }
    }

    /**
     * Function sending all marker list to rviz
     * @return 			void
     */
    void send() {
        pub_area.publish(area_list);
        pub_obj.publish(obj_list);
        pub_human.publish(human_list);
        pub_robot.publish(robot_list);
        pub_movingTwrd.publish(arrow_list);

        ros::spinOnce();
    }
};

/**
 * Main function using class run
 */
int main(int argc, char **argv) {
    ros::init(argc, argv, "Run");
    ros::NodeHandle node;
    ROS_INFO("[toaster-visu] launched");



    Run c = Run(node);


    ros::Rate loop_rate(30);

    ros::ServiceServer switch_name_print;
    switch_name_print = node.advertiseService("toaster_visualizer/switch_name_print", &Run::switchNamePrint, &c);


    ros::ServiceServer name_scale;
    name_scale = node.advertiseService("toaster_visualizer/scale_name", &Run::scaleName, &c);


    while (ros::ok()) {
        c.send();
        loop_rate.sleep();
    }

    return 0;
}
