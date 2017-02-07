#include "SimRadio.hpp"

#include <protobuf/grSim_Commands.pb.h>
#include <protobuf/grSim_Packet.pb.h>
#include <Network.hpp>
#include <stdexcept>
#include <Utils.hpp>

#include "firmware-common/robot2015/cpu/status.h"

using namespace std;
using namespace Packet;

static QHostAddress LocalAddress(QHostAddress::LocalHost);

SimRadio::SimRadio(bool blueTeam) : blueTeam(blueTeam) {
    _channel = blueTeam ? 1 : 0;
    if (!_socket.bind(RadioRxPort + _channel)) {
        throw runtime_error(QString("Can't bind to the %1 team's radio port.")
                                .arg(blueTeam ? "blue" : "yellow")
                                .toStdString());
    }
}

bool SimRadio::isOpen() const {
    return _socket.isValid();
}

void SimRadio::send(Packet::RadioTx& packet) {
    grSim_Packet simPacket;
    grSim_Commands* simRobotCommands = simPacket.mutable_commands();
    for (int x = 0; x < packet.robots_size(); x++) {
        const Packet::Robot& robot = packet.robots(x);
        grSim_Robot_Command* simRobot = simRobotCommands->add_robot_commands();
        simRobot->set_id(robot.uid());
        simRobot->set_veltangent(robot.control().yvelocity());
        simRobot->set_velnormal(-robot.control().xvelocity());
        simRobot->set_velangular(robot.control().avelocity() * 180.0 / M_PI_2);

        uint kick_strength = robot.control().kcstrength();
        switch (robot.control().shootmode()) {
            case Packet::Control::KICK:
                simRobot->set_kickspeedx(kick_strength);
                simRobot->set_kickspeedz(0);
                break;
            case Packet::Control::CHIP:
                simRobot->set_kickspeedx(kick_strength);
                simRobot->set_kickspeedz(kick_strength);
                break;
            default:
                break;
        }

        simRobot->set_spinner(robot.control().dvelocity() > 0);
        simRobot->set_wheelsspeed(false);
    }
    simRobotCommands->set_isteamyellow(!blueTeam);
    simRobotCommands->set_timestamp(RJ::timestamp());

    std::string out;
    simPacket.SerializeToString(&out);
    _socket.writeDatagram(&out[0], out.size(),
                                   QHostAddress(QHostAddress::LocalHost),
                                   SimCommandPort);
}

void SimRadio::receive() {
    for (int x = 0; x < 6; x++) {
        RadioRx packet;
        packet.set_robot_id(x);
        packet.set_hardware_version(RJ2015);
        packet.set_battery(100);
        
        for (int i = 0; i < 5; i++) {
            packet.add_motor_status(MotorStatus::Good);
        }
        packet.set_fpga_status(FpgaGood);
        packet.set_timestamp(RJ::timestamp());
        packet.set_kicker_status(Kicker_Charged | Kicker_Enabled | Kicker_I2C_OK);
        _reversePackets.push_back(packet);
    }
}

void SimRadio::switchTeam(bool blueTeam) {
    _socket.close();
    _channel = blueTeam ? 1 : 0;
    if (!_socket.bind(RadioRxPort + _channel)) {
        throw runtime_error(QString("Can't bind to the %1 team's radio port.")
                                .arg(blueTeam ? "blue" : "yellow")
                                .toStdString());
    }
}
