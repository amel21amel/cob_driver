/****************************************************************
 *
 * Copyright (c) 2010
 *
 * Fraunhofer Institute for Manufacturing Engineering	
 * and Automation (IPA)
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Project name: care-o-bot
 * ROS stack name: cob_drivers
 * ROS package name: cob_base_drive_chain
 * Description: This is a sample implementation of a can-bus with several nodes. In this case it implements the drive-chain of the Care-O-bot3 mobile base. yet, this can be used as template for using the generic_can and canopen_motor packages to implement arbitrary can-setups.
 *								
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *			
 * Author: Christian Connette, email:christian.connette@ipa.fhg.de
 * Supervised by: Christian Connette, email:christian.connette@ipa.fhg.de
 *
 * Date of creation: Feb 2010
 * ToDo: - Check whether motor status request in "setVelGearRadS" "setMotorTorque" make sense (maybe remove in "CanDriveHarmonica").
 *		 - move implementational details (can cmds) of configureElmoRecorder to CanDriveHarmonica (check whether CanDriveItf has to be adapted then)
 *		 - Check: what is the iRecordingGap, what is its unit
 *		 - Remove Mutex.h search for a Boost lib
 *
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above copyright
 *	   notice, this list of conditions and the following disclaimer in the
 *	   documentation and/or other materials provided with the distribution.
 *	 * Neither the name of the Fraunhofer Institute for Manufacturing 
 *	   Engineering and Automation (IPA) nor the names of its
 *	   contributors may be used to endorse or promote products derived from
 *	   this software without specific prior written permission.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as 
 * published by the Free Software Foundation, either version 3 of the 
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License LGPL for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public 
 * License LGPL along with this program. 
 * If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************/

// general includes
#include <math.h>
#include <unistd.h>

// Headers provided by other cob-packages
#include <cob_generic_can/CanESD.h>
#include <cob_generic_can/CanPeakSys.h>
#include <cob_generic_can/CanPeakSysUSB.h>
#include <cob_base_drive_chain/CanCtrlPltfCOb3.h>

#include <unistd.h>

//-----------------------------------------------

CanCtrlPltfCOb3::CanCtrlPltfCOb3(std::string iniDirectory)
{	
	sIniDirectory = iniDirectory;
	IniFile iniFile;
	iniFile.SetFileName(sIniDirectory + "Platform.ini", "PltfHardwareCoB3.h");

	// get max Joint-Velocities (in rad/s) for Steer- and Drive-Joint
	iniFile.GetKeyInt("Config", "NumberOfMotors", &m_iNumMotors, true);
	iniFile.GetKeyInt("Config", "NumberOfWheels", &m_iNumDrives, true);

	if(m_iNumMotors < 2 || m_iNumMotors > 8) {
		m_iNumMotors = 8;
		m_iNumDrives = 4;
	}

	// ------------- first of all set used CanItf
	m_pCanCtrl = NULL;

	// ------------- init hardware-specific vectors and set default values
	m_vpMotor.resize(m_iNumMotors);

	for(int i=0; i<m_iNumMotors; i++)
	{
		m_vpMotor[i] = NULL;
	}

	m_viMotorID.resize(m_iNumMotors);

//	m_viMotorID.resize(8);
	if(m_iNumMotors >= 1)
		m_viMotorID[0] = CANNODE_WHEEL1DRIVEMOTOR;
	if(m_iNumMotors >= 2)
		m_viMotorID[1] = CANNODE_WHEEL1STEERMOTOR;
	if(m_iNumMotors >= 3)
		m_viMotorID[2] = CANNODE_WHEEL2DRIVEMOTOR;
	if(m_iNumMotors >= 4)
		m_viMotorID[3] = CANNODE_WHEEL2STEERMOTOR;
	if(m_iNumMotors >= 5)
		m_viMotorID[4] = CANNODE_WHEEL3DRIVEMOTOR;
	if(m_iNumMotors >= 6)
		m_viMotorID[5] = CANNODE_WHEEL3STEERMOTOR;
	if(m_iNumMotors >= 7)
		m_viMotorID[6] = CANNODE_WHEEL4DRIVEMOTOR;
	if(m_iNumMotors == 8)
		m_viMotorID[7] = CANNODE_WHEEL4STEERMOTOR;

	// ------------- parameters
	m_Param.dCanTimeout = 7;

	if(m_iNumMotors >= 1)
		m_Param.iHasWheel1DriveMotor = 0;
	if(m_iNumMotors >= 2)
		m_Param.iHasWheel1SteerMotor = 0;
	if(m_iNumMotors >= 3)
		m_Param.iHasWheel2DriveMotor = 0;
	if(m_iNumMotors >= 4)
		m_Param.iHasWheel2SteerMotor = 0;
	if(m_iNumMotors >= 5)
		m_Param.iHasWheel3DriveMotor = 0;
	if(m_iNumMotors >= 6)
		m_Param.iHasWheel3SteerMotor = 0;
	if(m_iNumMotors >= 7)
		m_Param.iHasWheel4DriveMotor = 0;
	if(m_iNumMotors == 8)
		m_Param.iHasWheel4SteerMotor = 0;

	m_Param.iHasRelayBoard = 0;
	m_Param.iHasIOBoard = 0;
	m_Param.iHasUSBoard = 0;
	m_Param.iHasGyroBoard = 0;
	m_Param.iHasRadarBoard = 0;	

	m_bWatchdogErr = false;
	
	// ------------ CanIds
	
	// ------------ For CanOpen (Harmonica)
	// Wheel 1
	// can adresse motor 1
	if(m_iNumMotors >= 1)
	{
		m_CanOpenIDParam.TxPDO1_W1Drive = 0x182;
		m_CanOpenIDParam.TxPDO2_W1Drive = 0x282;
		m_CanOpenIDParam.RxPDO2_W1Drive = 0x302;
		m_CanOpenIDParam.TxSDO_W1Drive = 0x582;
		m_CanOpenIDParam.RxSDO_W1Drive = 0x602;
	}
	// can adresse motor 2
	if(m_iNumMotors >= 2)
	{
		m_CanOpenIDParam.TxPDO1_W1Steer = 0x181;
		m_CanOpenIDParam.TxPDO2_W1Steer = 0x281;
		m_CanOpenIDParam.RxPDO2_W1Steer = 0x301;
		m_CanOpenIDParam.TxSDO_W1Steer = 0x581;
		m_CanOpenIDParam.RxSDO_W1Steer = 0x601;
	}
	// Wheel 2
	// can adresse motor 7
	if(m_iNumMotors >= 3)
	{
		m_CanOpenIDParam.TxPDO1_W2Drive = 0x184;
		m_CanOpenIDParam.TxPDO2_W2Drive = 0x284;
		m_CanOpenIDParam.RxPDO2_W2Drive = 0x304;
		m_CanOpenIDParam.TxSDO_W2Drive = 0x584;
		m_CanOpenIDParam.RxSDO_W2Drive = 0x604;
	}
	// can adresse motor 8
	if(m_iNumMotors >= 4)
	{
		m_CanOpenIDParam.TxPDO1_W2Steer = 0x183;
		m_CanOpenIDParam.TxPDO2_W2Steer = 0x283;
		m_CanOpenIDParam.RxPDO2_W2Steer = 0x303;
		m_CanOpenIDParam.TxSDO_W2Steer = 0x583;
		m_CanOpenIDParam.RxSDO_W2Steer = 0x603;
	}
	// Wheel 3
	// can adresse motor 5
	if(m_iNumMotors >= 5)
	{
		m_CanOpenIDParam.TxPDO1_W3Drive = 0x188;
		m_CanOpenIDParam.TxPDO2_W3Drive = 0x288;
		m_CanOpenIDParam.RxPDO2_W3Drive = 0x308;
		m_CanOpenIDParam.TxSDO_W3Drive = 0x588;
		m_CanOpenIDParam.RxSDO_W3Drive = 0x608;
	}
	// can adresse motor 6
	if(m_iNumMotors >= 6)
	{
		m_CanOpenIDParam.TxPDO1_W3Steer = 0x187;
		m_CanOpenIDParam.TxPDO2_W3Steer = 0x287;
		m_CanOpenIDParam.RxPDO2_W3Steer = 0x307;
		m_CanOpenIDParam.TxSDO_W3Steer = 0x587;
		m_CanOpenIDParam.RxSDO_W3Steer = 0x607;
	}
	// Wheel 4
	// can adresse motor 3
	if(m_iNumMotors >= 7)
	{
		m_CanOpenIDParam.TxPDO1_W4Drive = 0x186;
		m_CanOpenIDParam.TxPDO2_W4Drive = 0x286;
		m_CanOpenIDParam.RxPDO2_W4Drive = 0x306;
		m_CanOpenIDParam.TxSDO_W4Drive = 0x586;
		m_CanOpenIDParam.RxSDO_W4Drive = 0x606;
	}
	// can adresse motor 4
	if(m_iNumMotors == 8)
	{
		m_CanOpenIDParam.TxPDO1_W4Steer = 0x185;
		m_CanOpenIDParam.TxPDO2_W4Steer = 0x285;
		m_CanOpenIDParam.RxPDO2_W4Steer = 0x305;
		m_CanOpenIDParam.TxSDO_W4Steer = 0x585;
		m_CanOpenIDParam.RxSDO_W4Steer = 0x605;
	}
}

//-----------------------------------------------
CanCtrlPltfCOb3::~CanCtrlPltfCOb3()
{

	if (m_pCanCtrl != NULL)
	{
		delete m_pCanCtrl;
	}

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		if (m_vpMotor[i] != NULL)
		{
			delete m_vpMotor[i];
		}
	}

}

//-----------------------------------------------
void CanCtrlPltfCOb3::readConfiguration()
{

	int iTypeCan = 0;
	int iMaxMessages = 0;

	DriveParam DriveParamW1DriveMotor;
	DriveParam DriveParamW1SteerMotor;
	DriveParam DriveParamW2DriveMotor;
	DriveParam DriveParamW2SteerMotor;
	DriveParam DriveParamW3DriveMotor;
	DriveParam DriveParamW3SteerMotor;
	DriveParam DriveParamW4DriveMotor;
	DriveParam DriveParamW4SteerMotor;
	
	std::string strTypeDrive;
	std::string strTypeSteer;

	double dScaleToMM;

	// read Platform.ini (Coupling of Drive/Steer for Homing)
	m_IniFile.SetFileName(sIniDirectory + "Platform.ini", "CanCtrlPltfCOb3.cpp");

	m_IniFile.GetKeyInt("Geom", "RadiusWheel", &m_Param.iRadiusWheelMM, true);
	m_IniFile.GetKeyInt("Geom", "DistSteerAxisToDriveWheelCenter", &m_Param.iDistSteerAxisToDriveWheelMM, true);

	if(m_iNumDrives >= 1)
		m_IniFile.GetKeyDouble("DrivePrms", "Wheel1SteerDriveCoupling", &m_Param.dWheel1SteerDriveCoupling, true);
	if(m_iNumDrives >= 2)
		m_IniFile.GetKeyDouble("DrivePrms", "Wheel2SteerDriveCoupling", &m_Param.dWheel2SteerDriveCoupling, true);
	if(m_iNumDrives >= 3)
		m_IniFile.GetKeyDouble("DrivePrms", "Wheel3SteerDriveCoupling", &m_Param.dWheel3SteerDriveCoupling, true);
	if(m_iNumDrives == 4)
		m_IniFile.GetKeyDouble("DrivePrms", "Wheel4SteerDriveCoupling", &m_Param.dWheel4SteerDriveCoupling, true);


	// read CanCtrl.ini
	m_IniFile.SetFileName(sIniDirectory + "CanCtrl.ini", "CanCtrlPltfCOb3.cpp");

	std::cout << "Can configuration of the platform:" << std::endl;
	
	// read Configuration of the Can-Network (CanCtrl.ini)
	m_IniFile.GetKeyInt("TypeCan", "Can", &iTypeCan, true);
	if (iTypeCan == 0)
	{
		sComposed = sIniDirectory;
		sComposed += "CanCtrl.ini";
		m_pCanCtrl = new CanPeakSys(sComposed.c_str());
		std::cout << "Uses CAN-Peak-Systems dongle" << std::endl;
	}
	else if (iTypeCan == 1)
	{
		sComposed = sIniDirectory;
		sComposed += "CanCtrl.ini";
		m_pCanCtrl = new CANPeakSysUSB(sComposed.c_str());
		std::cout << "Uses CAN-Peak-USB" << std::endl;
	}
	else if (iTypeCan == 2)
	{
		sComposed = sIniDirectory;
		sComposed += "CanCtrl.ini";
		m_pCanCtrl = new CanESD(sComposed.c_str(), false);
		std::cout << "Uses CAN-ESD-card" << std::endl;
	}

	// CanOpenId's ----- Default values (DESIRE)
	// Wheel 1
	// DriveMotor
	if(m_iNumDrives >= 1)
	{
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO1_W1Drive", &m_CanOpenIDParam.TxPDO1_W1Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO2_W1Drive", &m_CanOpenIDParam.TxPDO2_W1Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxPDO2_W1Drive", &m_CanOpenIDParam.RxPDO2_W1Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxSDO_W1Drive", &m_CanOpenIDParam.TxSDO_W1Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxSDO_W1Drive", &m_CanOpenIDParam.RxSDO_W1Drive, true);
	}
	// SteerMotor
	if(m_iNumDrives >= 1)
	{
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO1_W1Steer", &m_CanOpenIDParam.TxPDO1_W1Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO2_W1Steer", &m_CanOpenIDParam.TxPDO2_W1Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxPDO2_W1Steer", &m_CanOpenIDParam.RxPDO2_W1Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxSDO_W1Steer", &m_CanOpenIDParam.TxSDO_W1Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxSDO_W1Steer", &m_CanOpenIDParam.RxSDO_W1Steer, true);
	}
	
	// Wheel 2
	// DriveMotor
	if(m_iNumDrives >= 2)
	{
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO1_W2Drive", &m_CanOpenIDParam.TxPDO1_W2Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO2_W2Drive", &m_CanOpenIDParam.TxPDO2_W2Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxPDO2_W2Drive", &m_CanOpenIDParam.RxPDO2_W2Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxSDO_W2Drive", &m_CanOpenIDParam.TxSDO_W2Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxSDO_W2Drive", &m_CanOpenIDParam.RxSDO_W2Drive, true);
	}
	// SteerMotor
	if(m_iNumDrives >= 2)
	{
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO1_W2Steer", &m_CanOpenIDParam.TxPDO1_W2Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO2_W2Steer", &m_CanOpenIDParam.TxPDO2_W2Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxPDO2_W2Steer", &m_CanOpenIDParam.RxPDO2_W2Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxSDO_W2Steer", &m_CanOpenIDParam.TxSDO_W2Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxSDO_W2Steer", &m_CanOpenIDParam.RxSDO_W2Steer, true);
	}
	
	// Wheel 3
	// DriveMotor
	if(m_iNumDrives >= 3)
	{
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO1_W3Drive", &m_CanOpenIDParam.TxPDO1_W3Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO2_W3Drive", &m_CanOpenIDParam.TxPDO2_W3Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxPDO2_W3Drive", &m_CanOpenIDParam.RxPDO2_W3Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxSDO_W3Drive", &m_CanOpenIDParam.TxSDO_W3Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxSDO_W3Drive", &m_CanOpenIDParam.RxSDO_W3Drive, true);
	}
	// SteerMotor
	if(m_iNumDrives >= 3)
	{
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO1_W3Steer", &m_CanOpenIDParam.TxPDO1_W3Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO2_W3Steer", &m_CanOpenIDParam.TxPDO2_W3Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxPDO2_W3Steer", &m_CanOpenIDParam.RxPDO2_W3Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxSDO_W3Steer", &m_CanOpenIDParam.TxSDO_W3Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxSDO_W3Steer", &m_CanOpenIDParam.RxSDO_W3Steer, true);
	}
	
	// Wheel 4
	// DriveMotor
	if(m_iNumDrives >= 4)
	{
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO1_W4Drive", &m_CanOpenIDParam.TxPDO1_W4Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO2_W4Drive", &m_CanOpenIDParam.TxPDO2_W4Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxPDO2_W4Drive", &m_CanOpenIDParam.RxPDO2_W4Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxSDO_W4Drive", &m_CanOpenIDParam.TxSDO_W4Drive, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxSDO_W4Drive", &m_CanOpenIDParam.RxSDO_W4Drive, true);
	}
	// SteerMotor
	if(m_iNumDrives == 4)
	{
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO1_W4Steer", &m_CanOpenIDParam.TxPDO1_W4Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxPDO2_W4Steer", &m_CanOpenIDParam.TxPDO2_W4Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxPDO2_W4Steer", &m_CanOpenIDParam.RxPDO2_W4Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "TxSDO_W4Steer", &m_CanOpenIDParam.TxSDO_W4Steer, true);
		m_IniFile.GetKeyInt("CanOpenIDs", "RxSDO_W4Steer", &m_CanOpenIDParam.RxSDO_W4Steer, true);
	}

	// read configuration of the Drives (CanCtrl.ini)
	/* Drivemotor1-Parameters Old
	m_IniFile.GetKeyString("TypeDrive", "Drive1", &strTypeDrive, true);	
	m_IniFile.GetKeyInt(strTypeDrive.c_str(), "EncIncrPerRevMot", &(m_GearMotDrive1.iEncIncrPerRevMot), true);
	m_IniFile.GetKeyDouble(strTypeDrive.c_str(), "VelMeasFrqHz", &(m_GearMotDrive1.dVelMeasFrqHz), true);
	m_IniFile.GetKeyDouble(strTypeDrive.c_str(), "BeltRatio", &(m_GearMotDrive1.dBeltRatio), true);
	m_IniFile.GetKeyDouble(strTypeDrive.c_str(), "GearRatio", &(m_GearMotDrive1.dGearRatio), true);
	m_IniFile.GetKeyInt(strTypeDrive.c_str(), "Sign", &(m_GearMotDrive1.iSign), true);
	m_IniFile.GetKeyDouble(strTypeDrive.c_str(), "VelMaxEncIncrS", &(m_GearMotDrive1.dVelMaxEncIncrS), true);
	m_IniFile.GetKeyDouble(strTypeDrive.c_str(), "AccIncrS", &(m_GearMotDrive1.dAccIncrS2), true);
	m_IniFile.GetKeyDouble(strTypeDrive.c_str(), "DecIncrS", &(m_GearMotDrive1.dDecIncrS2), true);
	m_IniFile.GetKeyDouble(strTypeDrive.c_str(), "EncOffsetIncr",&(m_GearMotDrive1.iEncOffsetIncr),true);
	*/

	// "Drive Motor Type1" drive parameters	
	if(m_iNumDrives >= 1)
	{
		m_IniFile.GetKeyInt("Drive1", "EncIncrPerRevMot", &(m_GearMotDrive1.iEncIncrPerRevMot), true);
		m_IniFile.GetKeyDouble("Drive1", "VelMeasFrqHz", &(m_GearMotDrive1.dVelMeasFrqHz), true);
		m_IniFile.GetKeyDouble("Drive1", "BeltRatio", &(m_GearMotDrive1.dBeltRatio), true);
		m_IniFile.GetKeyDouble("Drive1", "GearRatio", &(m_GearMotDrive1.dGearRatio), true);
		m_IniFile.GetKeyInt("Drive1", "Sign", &(m_GearMotDrive1.iSign), true);
		m_IniFile.GetKeyDouble("Drive1", "VelMaxEncIncrS", &(m_GearMotDrive1.dVelMaxEncIncrS), true);
		m_IniFile.GetKeyDouble("Drive1", "AccIncrS", &(m_GearMotDrive1.dAccIncrS2), true);
		m_IniFile.GetKeyDouble("Drive1", "DecIncrS", &(m_GearMotDrive1.dDecIncrS2), true);
		m_IniFile.GetKeyInt("Drive1", "EncOffsetIncr",&(m_GearMotDrive1.iEncOffsetIncr),true);
		m_IniFile.GetKeyBool("Drive1", "IsSteering", &(m_GearMotDrive1.bIsSteer), true);
		m_IniFile.GetKeyDouble("Drive1", "CurrentToTorque", &(m_GearMotDrive1.dCurrentToTorque), false);
		m_IniFile.GetKeyDouble("Drive1", "CurrMax", &(m_GearMotDrive1.dCurrMax), false);
		m_IniFile.GetKeyInt("Drive1", "HomingDigIn", &(m_GearMotDrive1.iHomingDigIn), false);
	}

	// "Drive Motor Type2" drive parameters	
	if(m_iNumDrives >= 2)
	{
		m_IniFile.GetKeyInt("Drive2", "EncIncrPerRevMot", &(m_GearMotDrive2.iEncIncrPerRevMot), true);
		m_IniFile.GetKeyDouble("Drive2", "VelMeasFrqHz", &(m_GearMotDrive2.dVelMeasFrqHz), true);
		m_IniFile.GetKeyDouble("Drive2", "BeltRatio", &(m_GearMotDrive2.dBeltRatio), true);
		m_IniFile.GetKeyDouble("Drive2", "GearRatio", &(m_GearMotDrive2.dGearRatio), true);
		m_IniFile.GetKeyInt("Drive2", "Sign", &(m_GearMotDrive2.iSign), true);
		m_IniFile.GetKeyDouble("Drive2", "VelMaxEncIncrS", &(m_GearMotDrive2.dVelMaxEncIncrS), true);
		m_IniFile.GetKeyDouble("Drive2", "AccIncrS", &(m_GearMotDrive2.dAccIncrS2), true);
		m_IniFile.GetKeyDouble("Drive2", "DecIncrS", &(m_GearMotDrive2.dDecIncrS2), true);
		m_IniFile.GetKeyInt("Drive2", "EncOffsetIncr",&(m_GearMotDrive2.iEncOffsetIncr),true);
		m_IniFile.GetKeyBool("Drive2", "IsSteering", &(m_GearMotDrive2.bIsSteer), true);
		m_IniFile.GetKeyDouble("Drive2", "CurrentToTorque", &(m_GearMotDrive2.dCurrentToTorque), false);
		m_IniFile.GetKeyDouble("Drive2", "CurrMax", &(m_GearMotDrive2.dCurrMax), false);
		m_IniFile.GetKeyInt("Drive2", "HomingDigIn", &(m_GearMotDrive2.iHomingDigIn), false);
	}

	// "Drive Motor Type3" drive parameters	
	if(m_iNumDrives >= 3)
	{
		m_IniFile.GetKeyInt("Drive3", "EncIncrPerRevMot", &(m_GearMotDrive3.iEncIncrPerRevMot), true);
		m_IniFile.GetKeyDouble("Drive3", "VelMeasFrqHz", &(m_GearMotDrive3.dVelMeasFrqHz), true);
		m_IniFile.GetKeyDouble("Drive3", "BeltRatio", &(m_GearMotDrive3.dBeltRatio), true);
		m_IniFile.GetKeyDouble("Drive3", "GearRatio", &(m_GearMotDrive3.dGearRatio), true);
		m_IniFile.GetKeyInt("Drive3", "Sign", &(m_GearMotDrive3.iSign), true);
		m_IniFile.GetKeyDouble("Drive3", "VelMaxEncIncrS", &(m_GearMotDrive3.dVelMaxEncIncrS), true);
		m_IniFile.GetKeyDouble("Drive3", "AccIncrS", &(m_GearMotDrive3.dAccIncrS2), true);
		m_IniFile.GetKeyDouble("Drive3", "DecIncrS", &(m_GearMotDrive3.dDecIncrS2), true);
		m_IniFile.GetKeyInt("Drive3", "EncOffsetIncr",&(m_GearMotDrive3.iEncOffsetIncr),true);
		m_IniFile.GetKeyBool("Drive3", "IsSteering", &(m_GearMotDrive3.bIsSteer), true);
		m_IniFile.GetKeyDouble("Drive3", "CurrentToTorque", &(m_GearMotDrive3.dCurrentToTorque), false);
		m_IniFile.GetKeyDouble("Drive3", "CurrMax", &(m_GearMotDrive3.dCurrMax), false);
		m_IniFile.GetKeyInt("Drive3", "HomingDigIn", &(m_GearMotDrive3.iHomingDigIn), false);
	}

	// "Drive Motor Type4" drive parameters	
	if(m_iNumDrives == 4)
	{
		m_IniFile.GetKeyInt("Drive4", "EncIncrPerRevMot", &(m_GearMotDrive4.iEncIncrPerRevMot), true);
		m_IniFile.GetKeyDouble("Drive4", "VelMeasFrqHz", &(m_GearMotDrive4.dVelMeasFrqHz), true);
		m_IniFile.GetKeyDouble("Drive4", "BeltRatio", &(m_GearMotDrive4.dBeltRatio), true);
		m_IniFile.GetKeyDouble("Drive4", "GearRatio", &(m_GearMotDrive4.dGearRatio), true);
		m_IniFile.GetKeyInt("Drive4", "Sign", &(m_GearMotDrive4.iSign), true);
		m_IniFile.GetKeyDouble("Drive4", "VelMaxEncIncrS", &(m_GearMotDrive4.dVelMaxEncIncrS), true);
		m_IniFile.GetKeyDouble("Drive4", "AccIncrS", &(m_GearMotDrive4.dAccIncrS2), true);
		m_IniFile.GetKeyDouble("Drive4", "DecIncrS", &(m_GearMotDrive4.dDecIncrS2), true);
		m_IniFile.GetKeyInt("Drive4", "EncOffsetIncr",&(m_GearMotDrive4.iEncOffsetIncr),true);
		m_IniFile.GetKeyBool("Drive4", "IsSteering", &(m_GearMotDrive4.bIsSteer), true);
		m_IniFile.GetKeyDouble("Drive4", "CurrentToTorque", &(m_GearMotDrive4.dCurrentToTorque), false);
		m_IniFile.GetKeyDouble("Drive4", "CurrMax", &(m_GearMotDrive4.dCurrMax), false);
		m_IniFile.GetKeyInt("Drive4", "HomingDigIn", &(m_GearMotDrive4.iHomingDigIn), false);
	}

	// "Steer Motor Type1" drive parameters	
	if(m_iNumDrives >= 1)
	{
		m_IniFile.GetKeyInt("Steer1", "EncIncrPerRevMot", &(m_GearMotSteer1.iEncIncrPerRevMot), true);
		m_IniFile.GetKeyDouble("Steer1", "VelMeasFrqHz", &(m_GearMotSteer1.dVelMeasFrqHz), true);
		m_IniFile.GetKeyDouble("Steer1", "BeltRatio", &(m_GearMotSteer1.dBeltRatio), true);
		m_IniFile.GetKeyDouble("Steer1", "GearRatio", &(m_GearMotSteer1.dGearRatio), true);
		m_IniFile.GetKeyInt("Steer1", "Sign", &(m_GearMotSteer1.iSign), true);
		m_IniFile.GetKeyDouble("Steer1", "VelMaxEncIncrS", &(m_GearMotSteer1.dVelMaxEncIncrS), true);
		m_IniFile.GetKeyDouble("Steer1", "AccIncrS", &(m_GearMotSteer1.dAccIncrS2), true);
		m_IniFile.GetKeyDouble("Steer1", "DecIncrS", &(m_GearMotSteer1.dDecIncrS2), true);
		m_IniFile.GetKeyInt("Steer1", "EncOffsetIncr",&(m_GearMotSteer1.iEncOffsetIncr),true);
		m_IniFile.GetKeyBool("Steer1", "IsSteering", &(m_GearMotSteer1.bIsSteer), true);
		m_IniFile.GetKeyDouble("Steer1", "CurrentToTorque", &(m_GearMotSteer1.dCurrentToTorque), false);
		m_IniFile.GetKeyDouble("Steer1", "CurrMax", &(m_GearMotSteer1.dCurrMax), false);
		m_IniFile.GetKeyInt("Steer1", "HomingDigIn", &(m_GearMotSteer1.iHomingDigIn), false);
	}

	// "Steer Motor Type2" drive parameters	
	if(m_iNumDrives >= 2)
	{
		m_IniFile.GetKeyInt("Steer2", "EncIncrPerRevMot", &(m_GearMotSteer2.iEncIncrPerRevMot), true);
		m_IniFile.GetKeyDouble("Steer2", "VelMeasFrqHz", &(m_GearMotSteer2.dVelMeasFrqHz), true);
		m_IniFile.GetKeyDouble("Steer2", "BeltRatio", &(m_GearMotSteer2.dBeltRatio), true);
		m_IniFile.GetKeyDouble("Steer2", "GearRatio", &(m_GearMotSteer2.dGearRatio), true);
		m_IniFile.GetKeyInt("Steer2", "Sign", &(m_GearMotSteer2.iSign), true);
		m_IniFile.GetKeyDouble("Steer2", "VelMaxEncIncrS", &(m_GearMotSteer2.dVelMaxEncIncrS), true);
		m_IniFile.GetKeyDouble("Steer2", "AccIncrS", &(m_GearMotSteer2.dAccIncrS2), true);
		m_IniFile.GetKeyDouble("Steer2", "DecIncrS", &(m_GearMotSteer2.dDecIncrS2), true);
		m_IniFile.GetKeyInt("Steer2", "EncOffsetIncr",&(m_GearMotSteer2.iEncOffsetIncr),true);
		m_IniFile.GetKeyBool("Steer2", "IsSteering", &(m_GearMotSteer2.bIsSteer), true);
		m_IniFile.GetKeyDouble("Steer2", "CurrentToTorque", &(m_GearMotSteer2.dCurrentToTorque), false);
		m_IniFile.GetKeyDouble("Steer2", "CurrMax", &(m_GearMotSteer2.dCurrMax), false);
		m_IniFile.GetKeyInt("Steer2", "HomingDigIn", &(m_GearMotSteer2.iHomingDigIn), false);
	}

	// "Steer Motor Type3" drive parameters	
	if(m_iNumDrives >= 3)
	{
		m_IniFile.GetKeyInt("Steer3", "EncIncrPerRevMot", &(m_GearMotSteer3.iEncIncrPerRevMot), true);
		m_IniFile.GetKeyDouble("Steer3", "VelMeasFrqHz", &(m_GearMotSteer3.dVelMeasFrqHz), true);
		m_IniFile.GetKeyDouble("Steer3", "BeltRatio", &(m_GearMotSteer3.dBeltRatio), true);
		m_IniFile.GetKeyDouble("Steer3", "GearRatio", &(m_GearMotSteer3.dGearRatio), true);
		m_IniFile.GetKeyInt("Steer3", "Sign", &(m_GearMotSteer3.iSign), true);
		m_IniFile.GetKeyDouble("Steer3", "VelMaxEncIncrS", &(m_GearMotSteer3.dVelMaxEncIncrS), true);
		m_IniFile.GetKeyDouble("Steer3", "AccIncrS", &(m_GearMotSteer3.dAccIncrS2), true);
		m_IniFile.GetKeyDouble("Steer3", "DecIncrS", &(m_GearMotSteer3.dDecIncrS2), true);
		m_IniFile.GetKeyInt("Steer3", "EncOffsetIncr",&(m_GearMotSteer3.iEncOffsetIncr),true);
		m_IniFile.GetKeyBool("Steer3", "IsSteering", &(m_GearMotSteer3.bIsSteer), true);
		m_IniFile.GetKeyDouble("Steer3", "CurrentToTorque", &(m_GearMotSteer3.dCurrentToTorque), false);
		m_IniFile.GetKeyDouble("Steer3", "CurrMax", &(m_GearMotSteer3.dCurrMax), false);
		m_IniFile.GetKeyInt("Steer3", "HomingDigIn", &(m_GearMotSteer3.iHomingDigIn), false);
	}

	// "Steer Motor Type4" drive parameters	
	if(m_iNumDrives == 4)
	{
		m_IniFile.GetKeyInt("Steer4", "EncIncrPerRevMot", &(m_GearMotSteer4.iEncIncrPerRevMot), true);
		m_IniFile.GetKeyDouble("Steer4", "VelMeasFrqHz", &(m_GearMotSteer4.dVelMeasFrqHz), true);
		m_IniFile.GetKeyDouble("Steer4", "BeltRatio", &(m_GearMotSteer4.dBeltRatio), true);
		m_IniFile.GetKeyDouble("Steer4", "GearRatio", &(m_GearMotSteer4.dGearRatio), true);
		m_IniFile.GetKeyInt("Steer4", "Sign", &(m_GearMotSteer4.iSign), true);
		m_IniFile.GetKeyDouble("Steer4", "VelMaxEncIncrS", &(m_GearMotSteer4.dVelMaxEncIncrS), true);
		m_IniFile.GetKeyDouble("Steer4", "AccIncrS", &(m_GearMotSteer4.dAccIncrS2), true);
		m_IniFile.GetKeyDouble("Steer4", "DecIncrS", &(m_GearMotSteer4.dDecIncrS2), true);
		m_IniFile.GetKeyInt("Steer4", "EncOffsetIncr",&(m_GearMotSteer4.iEncOffsetIncr),true);
		m_IniFile.GetKeyBool("Steer4", "IsSteering", &(m_GearMotSteer4.bIsSteer), true);
		m_IniFile.GetKeyDouble("Steer4", "CurrentToTorque", &(m_GearMotSteer4.dCurrentToTorque), false);
		m_IniFile.GetKeyDouble("Steer4", "CurrMax", &(m_GearMotSteer4.dCurrMax), false);
		m_IniFile.GetKeyInt("Steer4", "HomingDigIn", &(m_GearMotSteer4.iHomingDigIn), false);
	}

	if(m_iNumDrives >= 1)
	{
		DriveParamW1DriveMotor.setParam(
			0,
			m_GearMotDrive1.iEncIncrPerRevMot,
			m_GearMotDrive1.dVelMeasFrqHz,
			m_GearMotDrive1.dBeltRatio,
			m_GearMotDrive1.dGearRatio,
			m_GearMotDrive1.iSign,
			m_GearMotDrive1.dVelMaxEncIncrS,
			m_GearMotDrive1.dAccIncrS2,
			m_GearMotDrive1.dDecIncrS2,
			m_GearMotDrive1.iEncOffsetIncr,
			m_GearMotDrive1.bIsSteer,
			m_GearMotDrive1.dCurrentToTorque,
			m_GearMotDrive1.dCurrMax,
			m_GearMotDrive1.iHomingDigIn);
	}
	
	if(m_iNumDrives >= 1)
	{
		DriveParamW1SteerMotor.setParam(
			1,
			m_GearMotSteer1.iEncIncrPerRevMot,
			m_GearMotSteer1.dVelMeasFrqHz,
			m_GearMotSteer1.dBeltRatio,
			m_GearMotSteer1.dGearRatio,
			m_GearMotSteer1.iSign,
			m_GearMotSteer1.dVelMaxEncIncrS,
			m_GearMotSteer1.dAccIncrS2,
			m_GearMotSteer1.dDecIncrS2,
			m_GearMotSteer1.iEncOffsetIncr,
			m_GearMotSteer1.bIsSteer,
			m_GearMotSteer1.dCurrentToTorque,
			m_GearMotSteer1.dCurrMax,
			m_GearMotSteer1.iHomingDigIn);
	}

	if(m_iNumDrives >= 2)
	{
		DriveParamW2DriveMotor.setParam(
			2,
			m_GearMotDrive2.iEncIncrPerRevMot,
			m_GearMotDrive2.dVelMeasFrqHz,
			m_GearMotDrive2.dBeltRatio,
			m_GearMotDrive2.dGearRatio,
			m_GearMotDrive2.iSign,
			m_GearMotDrive2.dVelMaxEncIncrS,
			m_GearMotDrive2.dAccIncrS2,
			m_GearMotDrive2.dDecIncrS2,
			m_GearMotDrive2.iEncOffsetIncr,
			m_GearMotDrive2.bIsSteer,
			m_GearMotDrive2.dCurrentToTorque,
			m_GearMotDrive2.dCurrMax,
			m_GearMotDrive2.iHomingDigIn);
	}
	
	if(m_iNumDrives >= 2)
	{
		DriveParamW2SteerMotor.setParam(
			3,
			m_GearMotSteer2.iEncIncrPerRevMot,
			m_GearMotSteer2.dVelMeasFrqHz,
			m_GearMotSteer2.dBeltRatio,
			m_GearMotSteer2.dGearRatio,
			m_GearMotSteer2.iSign,
			m_GearMotSteer2.dVelMaxEncIncrS,
			m_GearMotSteer2.dAccIncrS2,
			m_GearMotSteer2.dDecIncrS2,
			m_GearMotSteer2.iEncOffsetIncr,
			m_GearMotSteer2.bIsSteer,
			m_GearMotSteer2.dCurrentToTorque,
			m_GearMotSteer2.dCurrMax,
			m_GearMotSteer2.iHomingDigIn);
	}

	if(m_iNumDrives >= 3)
	{
		DriveParamW3DriveMotor.setParam(
			4,
			m_GearMotDrive3.iEncIncrPerRevMot,
			m_GearMotDrive3.dVelMeasFrqHz,
			m_GearMotDrive3.dBeltRatio,
			m_GearMotDrive3.dGearRatio,
			m_GearMotDrive3.iSign,
			m_GearMotDrive3.dVelMaxEncIncrS,
			m_GearMotDrive3.dAccIncrS2,
			m_GearMotDrive3.dDecIncrS2,
			m_GearMotDrive3.iEncOffsetIncr,
			m_GearMotDrive3.bIsSteer,
			m_GearMotDrive3.dCurrentToTorque,
			m_GearMotDrive3.dCurrMax,
			m_GearMotDrive3.iHomingDigIn);
	}
	
	if(m_iNumDrives >= 3)
	{
		DriveParamW3SteerMotor.setParam(
			5,
			m_GearMotSteer3.iEncIncrPerRevMot,
			m_GearMotSteer3.dVelMeasFrqHz,
			m_GearMotSteer3.dBeltRatio,
			m_GearMotSteer3.dGearRatio,
			m_GearMotSteer3.iSign,
			m_GearMotSteer3.dVelMaxEncIncrS,
			m_GearMotSteer3.dAccIncrS2,
			m_GearMotSteer3.dDecIncrS2,
			m_GearMotSteer3.iEncOffsetIncr,
			m_GearMotSteer3.bIsSteer,
			m_GearMotSteer3.dCurrentToTorque,
			m_GearMotSteer3.dCurrMax,
			m_GearMotSteer3.iHomingDigIn);
	}

	if(m_iNumDrives == 4)
	{
		DriveParamW4DriveMotor.setParam(
			6,
			m_GearMotDrive4.iEncIncrPerRevMot,
			m_GearMotDrive4.dVelMeasFrqHz,
			m_GearMotDrive4.dBeltRatio,
			m_GearMotDrive4.dGearRatio,
			m_GearMotDrive4.iSign,
			m_GearMotDrive4.dVelMaxEncIncrS,
			m_GearMotDrive4.dAccIncrS2,
			m_GearMotDrive4.dDecIncrS2,
			m_GearMotDrive4.iEncOffsetIncr,
			m_GearMotDrive4.bIsSteer,
			m_GearMotDrive4.dCurrentToTorque,
			m_GearMotDrive4.dCurrMax,
			m_GearMotDrive4.iHomingDigIn);
	}
	
	if(m_iNumDrives == 4)
	{
		DriveParamW4SteerMotor.setParam(
			7,
			m_GearMotSteer4.iEncIncrPerRevMot,
			m_GearMotSteer4.dVelMeasFrqHz,
			m_GearMotSteer4.dBeltRatio,
			m_GearMotSteer4.dGearRatio,
			m_GearMotSteer4.iSign,
			m_GearMotSteer4.dVelMaxEncIncrS,
			m_GearMotSteer4.dAccIncrS2,
			m_GearMotSteer4.dDecIncrS2,
			m_GearMotSteer4.iEncOffsetIncr,
			m_GearMotSteer4.bIsSteer,
			m_GearMotSteer4.dCurrentToTorque,
			m_GearMotSteer4.dCurrMax,
			m_GearMotSteer4.iHomingDigIn);
	}
	
	m_IniFile.GetKeyDouble("US", "ScaleToMM", &dScaleToMM, true);


	// read Platform.ini
	m_IniFile.SetFileName(sIniDirectory + "Platform.ini", "CanCtrlPltfCOb3.cpp");


	// ------ WHEEL 1 ------ //
	// --- Motor Wheel 1 Drive
	if(m_iNumDrives >= 1)
	{
		m_IniFile.GetKeyInt("Config", "Wheel1DriveMotor", &m_Param.iHasWheel1DriveMotor, true);
		if (m_Param.iHasWheel1DriveMotor == 0)
		{
			// No motor
			std::cout << "node Wheel1DriveMotor available = 0" << std::endl;
		}
		else
		{
			// Motor Harmonica
			std::cout << "Wheel1DriveMotor available = type version 2" << std::endl;
			m_vpMotor[0] = new CanDriveHarmonica();
			((CanDriveHarmonica*) m_vpMotor[0])->setCanOpenParam(
				m_CanOpenIDParam.TxPDO1_W1Drive, m_CanOpenIDParam.TxPDO2_W1Drive, m_CanOpenIDParam.RxPDO2_W1Drive,
				m_CanOpenIDParam.TxSDO_W1Drive, m_CanOpenIDParam.RxSDO_W1Drive);
			m_vpMotor[0]->setCanItf(m_pCanCtrl);
			m_vpMotor[0]->setDriveParam(DriveParamW1DriveMotor);
		}
	}

	// --- Motor Wheel 1 Steer
	if(m_iNumDrives >= 1)
	{
		m_IniFile.GetKeyInt("Config", "Wheel1SteerMotor", &m_Param.iHasWheel1SteerMotor, true);
		if (m_Param.iHasWheel1SteerMotor == 0)
		{
			// No motor
			std::cout << "node Wheel1SteerMotor available = 0" << std::endl;
		}
		else
		{
			// Motor Harmonica
			std::cout << "Wheel1SteerMotor available = type version 2" << std::endl;
			m_vpMotor[1] = new CanDriveHarmonica();
			((CanDriveHarmonica*) m_vpMotor[1])->setCanOpenParam(
				m_CanOpenIDParam.TxPDO1_W1Steer, m_CanOpenIDParam.TxPDO2_W1Steer, m_CanOpenIDParam.RxPDO2_W1Steer,
				m_CanOpenIDParam.TxSDO_W1Steer, m_CanOpenIDParam.RxSDO_W1Steer);
			m_vpMotor[1]->setCanItf(m_pCanCtrl);
			m_vpMotor[1]->setDriveParam(DriveParamW1SteerMotor);

		}
	}
	
	// ------ WHEEL 2 ------ //
	// --- Motor Wheel 2 Drive
	if(m_iNumDrives >= 2)
	{
		m_IniFile.GetKeyInt("Config", "Wheel2DriveMotor", &m_Param.iHasWheel2DriveMotor, true);
		if (m_Param.iHasWheel2DriveMotor == 0)
		{
			// No motor
			std::cout << "node Wheel2DriveMotor available = 0" << std::endl;
		}
		else
		{
			// Motor Harmonica
			std::cout << "Wheel2DriveMotor available = type version 2" << std::endl;
			m_vpMotor[2] = new CanDriveHarmonica();
			((CanDriveHarmonica*) m_vpMotor[2])->setCanOpenParam(
				m_CanOpenIDParam.TxPDO1_W2Drive, m_CanOpenIDParam.TxPDO2_W2Drive, m_CanOpenIDParam.RxPDO2_W2Drive,
				m_CanOpenIDParam.TxSDO_W2Drive, m_CanOpenIDParam.RxSDO_W2Drive);
			m_vpMotor[2]->setCanItf(m_pCanCtrl);
			m_vpMotor[2]->setDriveParam(DriveParamW2DriveMotor);
		}
	}

	// --- Motor Wheel 2 Steer
	if(m_iNumDrives >= 2)
	{
		m_IniFile.GetKeyInt("Config", "Wheel2SteerMotor", &m_Param.iHasWheel2SteerMotor, true);
		if (m_Param.iHasWheel2SteerMotor == 0)
		{
			// No motor
			std::cout << "node Wheel2SteerMotor available = 0" << std::endl;
		}
		else
		{
			// Motor Harmonica
			std::cout << "Wheel2SteerMotor available = type version 2" << std::endl;
			m_vpMotor[3] = new CanDriveHarmonica();
			((CanDriveHarmonica*) m_vpMotor[3])->setCanOpenParam(
				m_CanOpenIDParam.TxPDO1_W2Steer, m_CanOpenIDParam.TxPDO2_W2Steer, m_CanOpenIDParam.RxPDO2_W2Steer,
				m_CanOpenIDParam.TxSDO_W2Steer, m_CanOpenIDParam.RxSDO_W2Steer);
			m_vpMotor[3]->setCanItf(m_pCanCtrl);
			m_vpMotor[3]->setDriveParam(DriveParamW2SteerMotor);

		}
	}
	
	// ------ WHEEL 3 ------ //
	// --- Motor Wheel 3 Drive
	if(m_iNumDrives >= 3)
	{
		m_IniFile.GetKeyInt("Config", "Wheel3DriveMotor", &m_Param.iHasWheel3DriveMotor, true);
		if (m_Param.iHasWheel3DriveMotor == 0)
		{
			// No motor
			std::cout << "node Wheel3DriveMotor available = 0" << std::endl;
		}
		else
		{
			// Motor Harmonica
			std::cout << "Wheel3DriveMotor available = type version 2" << std::endl;
			m_vpMotor[4] = new CanDriveHarmonica();
			((CanDriveHarmonica*) m_vpMotor[4])->setCanOpenParam(
				m_CanOpenIDParam.TxPDO1_W3Drive, m_CanOpenIDParam.TxPDO2_W3Drive, m_CanOpenIDParam.RxPDO2_W3Drive,
				m_CanOpenIDParam.TxSDO_W3Drive, m_CanOpenIDParam.RxSDO_W3Drive);
			m_vpMotor[4]->setCanItf(m_pCanCtrl);
			m_vpMotor[4]->setDriveParam(DriveParamW3DriveMotor);
		}
	}

	// --- Motor Wheel 3 Steer
	if(m_iNumDrives >= 3)
	{
		m_IniFile.GetKeyInt("Config", "Wheel3SteerMotor", &m_Param.iHasWheel3SteerMotor, true);
		if (m_Param.iHasWheel3SteerMotor == 0)
		{
			// No motor
			std::cout << "node Wheel3SteerMotor available = 0" << std::endl;
		}
		else
		{
			// Motor Harmonica
			std::cout << "Wheel3SteerMotor available = type version 2" << std::endl;
			m_vpMotor[5] = new CanDriveHarmonica();
			((CanDriveHarmonica*) m_vpMotor[5])->setCanOpenParam(
				m_CanOpenIDParam.TxPDO1_W3Steer, m_CanOpenIDParam.TxPDO2_W3Steer, m_CanOpenIDParam.RxPDO2_W3Steer,
				m_CanOpenIDParam.TxSDO_W3Steer, m_CanOpenIDParam.RxSDO_W3Steer);
			m_vpMotor[5]->setCanItf(m_pCanCtrl);
			m_vpMotor[5]->setDriveParam(DriveParamW3SteerMotor);

		}
	}
	
	// ------ WHEEL 4 ------ //
	// --- Motor Wheel 4 Drive
	if(m_iNumDrives == 4)
	{
		m_IniFile.GetKeyInt("Config", "Wheel4DriveMotor", &m_Param.iHasWheel4DriveMotor, true);
		if (m_Param.iHasWheel4DriveMotor == 0)
		{
			// No motor
			std::cout << "node Wheel4DriveMotor available = 0" << std::endl;
		}
		else
		{
			// Motor Harmonica
			std::cout << "Wheel4DriveMotor available = type version 2" << std::endl;
			m_vpMotor[6] = new CanDriveHarmonica();
			((CanDriveHarmonica*) m_vpMotor[6])->setCanOpenParam(
				m_CanOpenIDParam.TxPDO1_W4Drive, m_CanOpenIDParam.TxPDO2_W4Drive, m_CanOpenIDParam.RxPDO2_W4Drive,
				m_CanOpenIDParam.TxSDO_W4Drive, m_CanOpenIDParam.RxSDO_W4Drive);
			m_vpMotor[6]->setCanItf(m_pCanCtrl);
			m_vpMotor[6]->setDriveParam(DriveParamW4DriveMotor);
		}
	}

	// --- Motor Wheel 4 Steer
	if(m_iNumDrives == 4)
	{
		m_IniFile.GetKeyInt("Config", "Wheel4SteerMotor", &m_Param.iHasWheel4SteerMotor, true);
		if (m_Param.iHasWheel4SteerMotor == 0)
		{
			// No motor
			std::cout << "node Wheel4SteerMotor available = 0" << std::endl;
		}
		else
		{
			// Motor Harmonica
			std::cout << "Wheel4SteerMotor available = type version 2" << std::endl;
			m_vpMotor[7] = new CanDriveHarmonica();
			((CanDriveHarmonica*) m_vpMotor[7])->setCanOpenParam(
				m_CanOpenIDParam.TxPDO1_W4Steer, m_CanOpenIDParam.TxPDO2_W4Steer, m_CanOpenIDParam.RxPDO2_W4Steer,
				m_CanOpenIDParam.TxSDO_W4Steer, m_CanOpenIDParam.RxSDO_W4Steer);
			m_vpMotor[7]->setCanItf(m_pCanCtrl);
			m_vpMotor[7]->setDriveParam(DriveParamW4SteerMotor);

		}
	}

	m_IniFile.GetKeyInt("Config", "GenericBufferLen", &iMaxMessages, true);


}

//-----------------------------------------------
int CanCtrlPltfCOb3::evalCanBuffer()
{
	bool bRet;
//	char cBuf[200];
	
	m_Mutex.lock();

	// as long as there is something in the can buffer -> read out next message
	while(m_pCanCtrl->receiveMsg(&m_CanMsgRec) == true)
	{
		bRet = false;
		// check for every motor if message belongs to it
		for (unsigned int i = 0; i < m_vpMotor.size(); i++)
		{
			// if message belongs to this motor write data (Pos, Vel, ...) to internal member vars
			bRet |= m_vpMotor[i]->evalReceivedMsg(m_CanMsgRec);
		}

		if (bRet == false)
		{
			std::cout << "evalCanBuffer(): Received CAN_Message with unknown identifier " << m_CanMsgRec.m_iID << std::endl;
		}		
	};
	

	m_Mutex.unlock();

	return 0;
}

//-----------------------------------------------
bool CanCtrlPltfCOb3::initPltf()
{	
	// read Configuration parameters from Inifile
	readConfiguration();
		
	// Vectors for drive objects and return values
	std::vector<bool> vbRetDriveMotor;
	std::vector<bool> vbRetSteerMotor;
	std::vector<CanDriveItf*> vpDriveMotor;
	std::vector<CanDriveItf*> vpSteerMotor;
	bool bHomingOk;

//	vbRetDriveMotor.assign(4,0);
//	vbRetSteerMotor.assign(4,0);
	vbRetDriveMotor.assign(m_iNumDrives,0);
	vbRetSteerMotor.assign(m_iNumDrives,0);

	// Homing is done on a wheel-module base (steering and driving needs to be synchronized) 
	// copy Motor-Pointer into Steer/Drive vector for more insight
	for(int i=0; i<=m_iNumMotors; i+=2)
		vpDriveMotor.push_back(m_vpMotor[i]);
//	vpDriveMotor.push_back(m_vpMotor[2]);
//	vpDriveMotor.push_back(m_vpMotor[4]);
//	vpDriveMotor.push_back(m_vpMotor[6]);
	for(int i=1; i<=m_iNumMotors; i+=2)
		vpSteerMotor.push_back(m_vpMotor[i]);
//	vpSteerMotor.push_back(m_vpMotor[3]);
//	vpSteerMotor.push_back(m_vpMotor[5]);
//	vpSteerMotor.push_back(m_vpMotor[7]);

	std::vector<double> vdFactorVel;
//	vdFactorVel.assign(4,0);
	vdFactorVel.assign(m_iNumDrives,0);
	double dhomeVeloRadS = -1.0;


	// Start can open network
	std::cout << "StartCanOpen" << std::endl;
	sendNetStartCanOpen();
	

	// initialize drives
	
	// 1st init watchdogs
	std::cout << "Initialization of Watchdogs" << std::endl;
	for(int i=0; i<m_iNumMotors; i++)
	{
		m_vpMotor[i]->startWatchdog(true);
	}
/*	m_vpMotor[0]->startWatchdog(true);
	m_vpMotor[1]->startWatchdog(true);
	m_vpMotor[2]->startWatchdog(true);
	m_vpMotor[3]->startWatchdog(true);
	m_vpMotor[4]->startWatchdog(true);
	m_vpMotor[5]->startWatchdog(true);
	m_vpMotor[6]->startWatchdog(true);
	m_vpMotor[7]->startWatchdog(true);
*/
	usleep(10000);
	
	// 2nd send watchdogs to bed while initializing drives
	for(int i=0; i<m_iNumMotors; i++)
	{
		m_vpMotor[i]->startWatchdog(false);
	}
/*	m_vpMotor[0]->startWatchdog(false);
	m_vpMotor[1]->startWatchdog(false);
	m_vpMotor[2]->startWatchdog(false);
	m_vpMotor[3]->startWatchdog(false);
	m_vpMotor[4]->startWatchdog(false);
	m_vpMotor[5]->startWatchdog(false);
	m_vpMotor[6]->startWatchdog(false);
	m_vpMotor[7]->startWatchdog(false);
*/
	usleep(100000);

	std::cout << "Initialization of Watchdogs done" << std::endl;


	// ---------------------- start homing procedurs
	// Perform homing of all wheels simultaneously
	// o.k. to avoid crashing hardware -> lets check that we have at least the 8 motors, like we have on cob
	if( (int)m_vpMotor.size() == m_iNumMotors )
	{
		// Initialize and start all motors
		for (int i = 0; i<m_iNumDrives; i++)
		{
			// init + start
			vbRetDriveMotor[i] = vpDriveMotor[i]->init();
			vbRetSteerMotor[i] = vpSteerMotor[i]->init();
			usleep(10000);
			vbRetDriveMotor[i] = vbRetDriveMotor[i] && vpDriveMotor[i]->start();
			vbRetSteerMotor[i] = vbRetSteerMotor[i] && vpSteerMotor[i]->start();
			usleep(10000);
			// output State / Errors
			if (vbRetDriveMotor[i] && vbRetSteerMotor[i])
				std::cout << "Initialization of Wheel "<< (i+1) << " OK" << std::endl;
			else if (!vbRetDriveMotor[i] && vbRetSteerMotor[i])
				std::cout << "Initialization of Wheel "<< (i+1) << " ERROR while initializing DRIVE-Motor" << std::endl;
			else if (vbRetDriveMotor[i] && !vbRetSteerMotor[i])
				std::cout << "Initialization of Wheel "<< (i+1) << " ERROR while initializing STEER-Motor" << std::endl;
			else
				std::cout << "Initialization of Wheel "<< (i+1) << " ERROR while initializing STEER- and DRIVE-Motor" << std::endl;
			// Just to be sure: Set vel to zero
			vpDriveMotor[i]->setGearVelRadS(0);
			vpSteerMotor[i]->setGearVelRadS(0);
		}	
		
		// perform homing only when ALL drives are ERROR-Free
//		if (vbRetDriveMotor[0] && vbRetDriveMotor[1] && vbRetDriveMotor[2] && vbRetDriveMotor[3] &&
//			vbRetSteerMotor[0] && vbRetSteerMotor[1] && vbRetSteerMotor[2] && vbRetSteerMotor[3])
		bHomingOk = true;
		for(int i=0; i<m_iNumDrives; i++)
		{
			if((vbRetDriveMotor[i] && vbRetSteerMotor[i]) == false)
				bHomingOk = false;
		}
		if(bHomingOk)
		{
			// Calc Compensation factor for Velocity:
			if(m_iNumDrives >= 1)
				vdFactorVel[0] = - m_Param.dWheel1SteerDriveCoupling + double(m_Param.iDistSteerAxisToDriveWheelMM) / double(m_Param.iRadiusWheelMM);
			if(m_iNumDrives >= 2)
				vdFactorVel[1] = - m_Param.dWheel2SteerDriveCoupling + double(m_Param.iDistSteerAxisToDriveWheelMM) / double(m_Param.iRadiusWheelMM);
			if(m_iNumDrives >= 3)
				vdFactorVel[2] = - m_Param.dWheel3SteerDriveCoupling + double(m_Param.iDistSteerAxisToDriveWheelMM) / double(m_Param.iRadiusWheelMM);
			if(m_iNumDrives == 4)
				vdFactorVel[3] = - m_Param.dWheel4SteerDriveCoupling + double(m_Param.iDistSteerAxisToDriveWheelMM) / double(m_Param.iRadiusWheelMM);

			// initialize homing procedure
			for (int i = 0; i<m_iNumDrives; i++)
				vpSteerMotor[i]->initHoming();						
			
			// make motors move
			for (int i = 0; i<m_iNumDrives; i++)
			{
				vpSteerMotor[i]->setGearVelRadS(dhomeVeloRadS);
				vpDriveMotor[i]->setGearVelRadS(dhomeVeloRadS * vdFactorVel[i]);
			}

			// wait at least 0.5 sec.
			usleep(500000);

			// Get rid of unnecessary can messages
			bool bRet;
			do
				bRet = m_pCanCtrl->receiveMsg(&m_CanMsgRec);
			while(bRet == true);

			// arm homing procedure
			for (int i = 0; i<m_iNumDrives; i++)
				vpSteerMotor[i]->IntprtSetInt(8, 'H', 'M', 1, 1);

			// wait until all steers are homed			
			bool bAllDone, bTimeOut=false;
			int iCnt = 0;
			do
			{
				// send request for homing status
				for (int i = 0; i<m_iNumDrives; i++)
					vpSteerMotor[i]->IntprtSetInt(4, 'H', 'M', 1, 0);
				
				// eval Can Messages
				evalCanBuffer();

				// set already homed wheels to zero velocity
				bAllDone = true;
				for (int i = 0; i<m_iNumDrives; i++)
				{
					if (vpSteerMotor[i]->getStatusLimitSwitch())
					{
						vpSteerMotor[i]->setGearVelRadS(0);
						vpDriveMotor[i]->setGearVelRadS(0);
					}
					bAllDone = bAllDone && vpSteerMotor[i]->getStatusLimitSwitch();
				}	

				// increment timeout counter
				if (iCnt++ > 1000) //cpc-ck has 500 for Cob 3.5 here
					bTimeOut = true;

				// Sleep: To avoid can overload
			usleep(20000);				
			}
			while(!bAllDone && !bTimeOut);

			// Output State
			if (bTimeOut)
			{
				for (int i=0;i<m_iNumDrives;i++)
				{
					vpSteerMotor[i]->setGearVelRadS(0);
					vpDriveMotor[i]->setGearVelRadS(0);
				}
				std::cout << "Error while Homing: Timeout while waiting for homing signal" << std::endl;
			}

			// update Can Buffer once more
			// read current can buffer and update position and velocity measurements
			evalCanBuffer();

			// Now make steers move to position: zero
			// this could be handled also by the elmos themselve
			// however this way synchronization of steering and driving is better
			double m_d0 = 2.5;
			if (bTimeOut == false)
			{
				do
				{	
					bAllDone = true;
					for (int i = 0; i<m_iNumDrives; i++)
					{
						// get current position of steer
						double dCurrentPosRad;
						vpSteerMotor[i]->getGearPosRad(&dCurrentPosRad);
						// P-Ctrl					
						double dDeltaPhi = 0.0 - dCurrentPosRad; 
						// check if steer is at pos zero
						if (fabs(dDeltaPhi) < 0.03)	//alter Wert=0.03
						{
							dDeltaPhi = 0.0;
						}
						else
						{
							bAllDone = false;	
						}
						double dVelCmd = m_d0 * dDeltaPhi;
						// set Outputs
						vpSteerMotor[i]->setGearVelRadS(dVelCmd);
						vpDriveMotor[i]->setGearVelRadS(dVelCmd*vdFactorVel[i]);
					}
					usleep(20000);
					evalCanBuffer();
				} while(!bAllDone);


				// Homing done. Wheels at position zero (+/- 0.5°)
				std::cout << "Wheels homed" << std::endl;
				for (int i=0;i<m_iNumDrives;i++)
				{
					vpSteerMotor[i]->setGearVelRadS(0);
					vpDriveMotor[i]->setGearVelRadS(0);
				}
			}
		}
	}
	// ---------------------- end homing procedure

	// homing done -> wake up watchdogs
	for(int i=0; i<m_iNumMotors; i++)
	{
		m_vpMotor[i]->startWatchdog(true);
	}
/*	m_vpMotor[0]->startWatchdog(true);
	m_vpMotor[1]->startWatchdog(true);
	m_vpMotor[2]->startWatchdog(true);
	m_vpMotor[3]->startWatchdog(true);
	m_vpMotor[4]->startWatchdog(true);
	m_vpMotor[5]->startWatchdog(true);
	m_vpMotor[6]->startWatchdog(true);
	m_vpMotor[7]->startWatchdog(true);
*/
//	return  (
//		vbRetDriveMotor[0] && vbRetDriveMotor[1] && vbRetDriveMotor[2] && vbRetDriveMotor[3] &&
//		vbRetSteerMotor[0] && vbRetSteerMotor[1] && vbRetSteerMotor[2] && vbRetSteerMotor[3]);
	bHomingOk = true;
	for(int i=0; i<m_iNumDrives; i++)
	{
		if((vbRetDriveMotor[i] && vbRetSteerMotor[i]) == false)
		{
			bHomingOk = false;
		}
	}
	return (bHomingOk);
}

//-----------------------------------------------
bool CanCtrlPltfCOb3::resetPltf()
{
	bool bRetMotor = true;
	bool bRet = true;
	
	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		bRetMotor = m_vpMotor[i]->start();
		if (bRetMotor == true)
		{
			m_vpMotor[i]->setGearVelRadS(0);
		}
		else
		{
			std::cout << "Resetting of Motor " << i << " failed" << std::endl;
		}

		bRet &= bRetMotor;
	}
	return(bRet);
}

//-----------------------------------------------
bool CanCtrlPltfCOb3::shutdownPltf()
{

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		m_vpMotor[i]->shutdown();
	}

	return true;
}

//-----------------------------------------------
bool CanCtrlPltfCOb3::isPltfError()
{

	bool bErrMotor = false;
	std::vector<bool> vbErrMotor;
	vbErrMotor.resize(m_vpMotor.size());

	// check all motors for errors
	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		vbErrMotor[i] = m_vpMotor[i]->isError();
		// if watchdog is not yet in error state but one motor is errorneous Log Out Error-Msg
		if( (m_bWatchdogErr == false) && (vbErrMotor[i] == true) )
		{
			std::cout << "Motor " << i << " error" << std::endl;
		}
		// check whether no motor was errorneous
		bErrMotor |= vbErrMotor[i];
	}

	// if no motor is in error state force Watchdog to "No Error" state
	if(bErrMotor == false)
		m_bWatchdogErr = false;
	else
		m_bWatchdogErr = true;

	if (m_bWatchdogErr) return true;


	// Check communication
	double dWatchTime = 0;

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		dWatchTime = m_vpMotor[i]->getTimeToLastMsg();

		if(dWatchTime <  m_Param.dCanTimeout)
		{
			m_bWatchdogErr = false;
		}
		if( (dWatchTime > m_Param.dCanTimeout) && (m_bWatchdogErr == false) )
		{
			std::cout << "timeout CAN motor " << i << std::endl;
			m_bWatchdogErr = true;
			return true;
		}
	}

	return false;
}

//-----------------------------------------------
bool CanCtrlPltfCOb3::startWatchdog(bool bStarted)
{

	bool bRet = true;

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		bRet = m_vpMotor[i]->startWatchdog(bStarted);
	}

	return (bRet);
}

//-----------------------------------------------
void CanCtrlPltfCOb3::sendNetStartCanOpen()
{
	CanMsg msg;

	msg.m_iID  = 0;
	msg.m_iLen = 2;
	msg.set(1,0,0,0,0,0,0,0);
	m_pCanCtrl->transmitMsg(msg, false);

	usleep(100000);
}


//-----------------------------------------------
// Motor Controlers
//-----------------------------------------------


//-----------------------------------------------
int CanCtrlPltfCOb3::setVelGearRadS(int iCanIdent, double dVelGearRadS)
{		
	m_Mutex.lock();

	// If an error was detected and processed in isPltfErr().
	if (m_bWatchdogErr == true)
	{
		// Error -> Stop motor driving
		dVelGearRadS = 0;
		// The error is detected and quitted in the funtion isPltfErr().
	}

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		// check if Identifier fits to availlable hardware
		if(iCanIdent == m_viMotorID[i])
		{
			m_vpMotor[i]->setGearVelRadS(dVelGearRadS);
		}
	}
	
	m_Mutex.unlock();
	
	return 0;
}

//-----------------------------------------------
int CanCtrlPltfCOb3::requestMotPosVel(int iCanIdent)
{
	m_Mutex.lock();

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		// check if Identifier fits to availlable hardware
		if(iCanIdent == m_viMotorID[i])
		{
			m_vpMotor[i]->requestPosVel();
		}
	}
	
	m_Mutex.unlock();

	return 0;
}

//-----------------------------------------------
void CanCtrlPltfCOb3::requestDriveStatus()
{
	m_Mutex.lock();

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		m_vpMotor[i]->requestStatus();
	}

	m_Mutex.unlock();
}

//-----------------------------------------------
int CanCtrlPltfCOb3::getGearPosVelRadS(int iCanIdent, double* pdAngleGearRad, double* pdVelGearRadS)
{

	// init default outputs
	*pdAngleGearRad = 0;
	*pdVelGearRadS = 0;

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		// check if Identifier fits to availlable hardware
		if(iCanIdent == m_viMotorID[i])
		{
			m_vpMotor[i]->getGearPosVelRadS(pdAngleGearRad, pdVelGearRadS);
		}
	}
	
	return 0;
}

//-----------------------------------------------
int CanCtrlPltfCOb3::getGearDeltaPosVelRadS(int iCanIdent, double* pdAngleGearRad,
										   double* pdVelGearRadS)
{

	// init default outputs
	*pdAngleGearRad = 0;
	*pdVelGearRadS = 0;

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		// check if Identifier fits to availlable hardware
		if(iCanIdent == m_viMotorID[i])
		{
			m_vpMotor[i]->getGearDeltaPosVelRadS(pdAngleGearRad, pdVelGearRadS);
		}
	}

	return 0;
}

//-----------------------------------------------
void CanCtrlPltfCOb3::getStatus(int iCanIdent, int* piStatus, int* piTempCel)
{
	// init default outputs
	*piStatus = 0;
	*piTempCel = 0;

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		// check if Identifier fits to availlable hardware
		if(iCanIdent == m_viMotorID[i])
		{
			m_vpMotor[i]->getStatus(piStatus, piTempCel);
		}
	}

}
//-----------------------------------------------
void CanCtrlPltfCOb3::requestMotorTorque()
{
	m_Mutex.lock();

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		m_vpMotor[i]->requestMotorTorque();
	}

	m_Mutex.unlock();
}	

//-----------------------------------------------
void CanCtrlPltfCOb3::getMotorTorque(int iCanIdent, double* pdTorqueNm)
{
	// init default outputs
	*pdTorqueNm = 0;

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		// check if Identifier fits to availlable hardware
		if(iCanIdent == m_viMotorID[i])
		{
			m_vpMotor[i]->getMotorTorque(pdTorqueNm);
		}
	}

}
//-----------------------------------------------
void CanCtrlPltfCOb3::setMotorTorque(int iCanIdent, double dTorqueNm)
{
	m_Mutex.lock();

	for(unsigned int i = 0; i < m_vpMotor.size(); i++)
	{
		// check if Identifier fits to availlable hardware
		if(iCanIdent == m_viMotorID[i])
		{
			m_vpMotor[i]->setMotorTorque(dTorqueNm);
		}
	}

	m_Mutex.unlock();
}

//-----------------------------------------------
// Read out Elmo-Recorder via CAN (cpc-pk)
//-----------------------------------------------

//-----------------------------------------------
int CanCtrlPltfCOb3::ElmoRecordings(int iFlag, int iParam, std::string sString) {
	int tempRet = 0;
	int bRet = 0;
	
	switch(iFlag) {
		case 0: //Flag = 0 means reset recorder and configure it
			for(unsigned int i = 0; i < m_vpMotor.size(); i++) {
				m_vpMotor[i]->setRecorder(0, iParam); //Configure Elmo Recorder with RecordingGap and start immediately
			}
			return 0;

		case 1: //Flag = 1 means start readout process, mustn't be called too early (while Rec is in process..)
			for(unsigned int i = 0; i < m_vpMotor.size(); i++) {
				if((tempRet = m_vpMotor[i]->setRecorder(1, iParam, sString)) > bRet) { 
					bRet = tempRet; //Query Readout of Index to Log Directory
				}
			}
			return bRet;
		
		case 99:
			for(unsigned int i = 0; i < m_vpMotor.size(); i++) {
				m_vpMotor[i]->setRecorder(99, 0); //Stop any ongoing SDO transfer and clear corresponding data.
			}
			return 0;
			
		case 100:
			for(unsigned int i = 0; i < m_vpMotor.size(); i++) {
				bRet += m_vpMotor[i]->setRecorder(2, 0); //Request state of transmission
			}
			return bRet;
		
		default:
			return -1;
	}
}
