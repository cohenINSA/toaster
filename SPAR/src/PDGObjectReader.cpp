/* 
 * File:   PDGObjectReader.cpp
 * Author: gmilliez
 * 
 * Created on December 12, 2014, 2:23 PM
 */

#include "SPAR/PDGObjectReader.h"

PDGObjectReader::PDGObjectReader(ros::NodeHandle& node) {
    std::cout << "[SPAR] Initializing PDGObjectReader" << std::endl;

    // Starts listening to the topic
    sub_ = node.subscribe("/PDG/objectList", 1, &PDGObjectReader::objectStateCallBack, this);
}

void PDGObjectReader::objectStateCallBack(const PDG::ObjectList::ConstPtr& msg) {
    std::cout << "[SPAR] new data for object received" << std::endl;

    Object* curObject;
    for (unsigned int i = 0; i < msg->objectList.size(); i++) {

        // If this human is not assigned we have to allocate data.
        if (lastConfig_[msg->objectList[i].meEntity.id] == NULL)
            curObject = new Object(msg->objectList[i].meEntity.id);
        else
            curObject = lastConfig_[msg->objectList[i].meEntity.id];

        std::vector<double> robOrientation;
        bg::model::point<double, 3, bg::cs::cartesian> robPosition;

        curObject->setTime(msg->objectList[i].meEntity.time);

        robPosition.set<0>(msg->objectList[i].meEntity.positionX);
        robPosition.set<1>(msg->objectList[i].meEntity.positionY);
        robPosition.set<2>(msg->objectList[i].meEntity.positionZ);
        curObject->setPosition(robPosition);

        robOrientation.push_back(msg->objectList[i].meEntity.orientationRoll);
        robOrientation.push_back(msg->objectList[i].meEntity.orientationPitch);
        robOrientation.push_back(msg->objectList[i].meEntity.orientationYaw);
        curObject->setOrientation(robOrientation);

        if (lastConfig_[msg->objectList[i].meEntity.id] == NULL)
            lastConfig_[curObject->getId()] = curObject;
    }
}

bool PDGObjectReader::isPresent(unsigned int id) {
    timeval curTime;
    gettimeofday(&curTime, NULL);
    unsigned long now = curTime.tv_sec * pow(10, 9) + curTime.tv_usec;
    unsigned long timeThreshold = pow(10, 9);
    //std::cout << "current time: " << now <<  "  human time: " << m_LastTime << std::endl;
    long timeDif = lastConfig_[id]->getTime() - now;
    //std::cout << "time dif: " << timeDif << std::endl;

    if (fabs(timeDif) < timeThreshold)
        return true;
    else
        return false;
}
