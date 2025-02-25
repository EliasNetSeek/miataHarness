/**
 * @file	engine_configuration.cpp
 * @brief	Utility method related to the engine configuration data structure.
 *
 * @date Nov 22, 2013
 * @author Andrey Belomutskiy, (c) 2012-2020
 *
 * This file is part of rusEfi - see http://rusefi.com
 *
 * rusEfi is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * rusEfi is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "pch.h"


#include "speed_density.h"
#include "advance_map.h"
#include "flash_main.h"

#include "bench_test.h"

#if EFI_MEMS
#include "accelerometer.h"
#endif

#include "defaults.h"

#include "bmw_m73.h"
#include "bmw_n73.h"

#include "citroenBerlingoTU3JP.h"
#include "custom_engine.h"
#include "dodge_neon.h"
#include "dodge_ram.h"

#include "engine_template.h"

#include "ford_aspire.h"
#include "ford_1995_inline_6.h"

#include "honda_k_dbc.h"
#include "honda_600.h"
#include "honda.h"
#include "hyundai.h"

#include "GY6_139QMB.h"

#include "nissan_primera.h"
#include "nissan_vq.h"

#include "mazda_miata.h"
#include "mazda_miata_1_6.h"
#include "mazda_miata_na8.h"
#include "mazda_miata_vvt.h"
#include "mazda_626.h"
#include "m111.h"
#include "mercedes.h"

#include "gm_ls_4.h"
#include "gm_sbc.h"
#include "subaru.h"
#include "slingshot.h"
#include "test_engine.h"
#include "sachs.h"
#include "vw.h"
#include "vw_b6.h"
#include "chevrolet_camaro_4.h"
#include "toyota_jzs147.h"
#include "toyota_1NZ_FE.h"
#include "mitsubishi_3A92.h"
#include "mitsubishi_4G93.h"
#include "ford_festiva.h"
#include "boost_control.h"
#if EFI_IDLE_CONTROL
#include "idle_thread.h"
#endif /* EFI_IDLE_CONTROL */

#if EFI_ALTERNATOR_CONTROL
#include "alternator_controller.h"
#endif

#if EFI_ELECTRONIC_THROTTLE_BODY
#include "electronic_throttle.h"
#endif

#if EFI_HIP_9011
#include "hip9011.h"
#endif

#include "hardware.h"

#if EFI_PROD_CODE
#include "board.h"
#endif /* EFI_PROD_CODE */

#if EFI_EMULATE_POSITION_SENSORS
#include "trigger_emulator_algo.h"
#endif /* EFI_EMULATE_POSITION_SENSORS */

#if EFI_TUNER_STUDIO
#include "tunerstudio.h"
#endif

//#define TS_DEFAULT_SPEED 115200
#define TS_DEFAULT_SPEED 38400

/**
 * Current engine configuration. On firmware start we assign empty configuration, then
 * we copy actual configuration after reading settings from flash.
 * This is useful to compare old/current (activeConfiguration) and new/future (engineConfiguration) configurations in order to apply new settings.
 *
 * todo: place this field next to 'engineConfiguration'?
 */
static bool hasRememberedConfiguration = false;
#if EFI_ACTIVE_CONFIGURATION_IN_FLASH
#include "flash_int.h"
engine_configuration_s & activeConfiguration = reinterpret_cast<persistent_config_container_s*>(getFlashAddrFirstCopy())->persistentConfiguration.engineConfiguration;
// we cannot use this activeConfiguration until we call rememberCurrentConfiguration()
bool isActiveConfigurationVoid = true;
#else
static engine_configuration_s activeConfigurationLocalStorage;
engine_configuration_s & activeConfiguration = activeConfigurationLocalStorage;
#endif /* EFI_ACTIVE_CONFIGURATION_IN_FLASH */

void rememberCurrentConfiguration() {
#if ! EFI_ACTIVE_CONFIGURATION_IN_FLASH
	memcpy(&activeConfiguration, engineConfiguration, sizeof(engine_configuration_s));
#else
	isActiveConfigurationVoid = false;
#endif /* EFI_ACTIVE_CONFIGURATION_IN_FLASH */
    hasRememberedConfiguration = true;
}

static void wipeString(char *string, int size) {
	// we have to reset bytes after \0 symbol in order to calculate correct tune CRC from MSQ file
	for (int i = strlen(string) + 1; i < size; i++) {
		string[i] = 0;
	}
}

static void wipeStrings() {
	wipeString(engineConfiguration->engineMake, sizeof(vehicle_info_t));
	wipeString(engineConfiguration->engineCode, sizeof(vehicle_info_t));
	wipeString(engineConfiguration->vehicleName, sizeof(vehicle_info_t));
}

void onBurnRequest() {
	wipeStrings();

	incrementGlobalConfigurationVersion("burn");
}

// Weak link a stub so that every board doesn't have to implement this function
__attribute__((weak)) void boardOnConfigurationChange(engine_configuration_s* /*previousConfiguration*/) { }

/**
 * this is the top-level method which should be called in case of any changes to engine configuration
 * online tuning of most values in the maps does not count as configuration change, but 'Burn' command does
 *
 * this method is NOT currently invoked on ECU start - actual user input has to happen!
 * See preCalculate which is invoked BOTH on start and configuration change
 */
void incrementGlobalConfigurationVersion(const char * msg) {
    assertStackVoid("increment", ObdCode::STACK_USAGE_MISC, EXPECTED_REMAINING_STACK);
    if (!hasRememberedConfiguration) {
        firmwareError(ObdCode::OBD_PCM_Processor_Fault, "too early to invoke incrementGlobalConfigurationVersion %s", msg);
    }
	engine->globalConfigurationVersion++;
#if EFI_DEFAILED_LOGGING
	efiPrintf("set globalConfigurationVersion=%d", globalConfigurationVersion);
#endif /* EFI_DEFAILED_LOGGING */

	applyNewHardwareSettings();

	boardOnConfigurationChange(&activeConfiguration);

/**
 * All these callbacks could be implemented as listeners, but these days I am saving RAM
 */
	engine->preCalculate();
#if EFI_ALTERNATOR_CONTROL
	onConfigurationChangeAlternatorCallback(&activeConfiguration);
#endif /* EFI_ALTERNATOR_CONTROL */

#if EFI_BOOST_CONTROL
	onConfigurationChangeBoostCallback(&activeConfiguration);
#endif
#if EFI_ELECTRONIC_THROTTLE_BODY
	onConfigurationChangeElectronicThrottleCallback(&activeConfiguration);
#endif /* EFI_ELECTRONIC_THROTTLE_BODY */

#if EFI_ENGINE_CONTROL && EFI_PROD_CODE
	onConfigurationChangeBenchTest();
#endif

#if EFI_SHAFT_POSITION_INPUT
	onConfigurationChangeTriggerCallback();
#endif /* EFI_SHAFT_POSITION_INPUT */
#if EFI_EMULATE_POSITION_SENSORS && ! EFI_UNIT_TEST
	onConfigurationChangeRpmEmulatorCallback(&activeConfiguration);
#endif /* EFI_EMULATE_POSITION_SENSORS */

	engine->engineModules.apply_all([](auto & m) {
			m.onConfigurationChange(&activeConfiguration);
		});
	rememberCurrentConfiguration();
}

/**
 * @brief Sets the same dwell time across the whole getRpm() range
 * set dwell X
 */
void setConstantDwell(floatms_t dwellMs) {
	for (int i = 0; i < DWELL_CURVE_SIZE; i++) {
		config->sparkDwellRpmBins[i] = 1000 * i;
	}
	setArrayValues(config->sparkDwellValues, dwellMs);
}

void setWholeIgnitionIatCorr(float value) {
	setTable(config->ignitionIatCorrTable, value);
}

void setFuelTablesLoadBin(float minValue, float maxValue) {
	setLinearCurve(config->injPhaseLoadBins, minValue, maxValue, 1);
	setLinearCurve(config->veLoadBins, minValue, maxValue, 1);
	setLinearCurve(config->lambdaLoadBins, minValue, maxValue, 1);
}

void setWholeIatCorrTimingTable(float value) {
	setTable(config->ignitionIatCorrTable, value);
}

/**
 * See also crankingTimingAngle
 */
void setWholeTimingTable_d(angle_t value) {
	setTable(config->ignitionTable, value);
}

static void initTemperatureCurve(float *bins, float *values, int size, float defaultValue) {
	for (int i = 0; i < size; i++) {
		bins[i] = -40 + i * 10;
		values[i] = defaultValue; // this correction is a multiplier
	}
}

void prepareVoidConfiguration(engine_configuration_s *engineConfiguration) {
	efiAssertVoid(ObdCode::OBD_PCM_Processor_Fault, engineConfiguration != NULL, "ec NULL");
	efi::clear(engineConfiguration);

	engineConfiguration->clutchDownPinMode = PI_PULLUP;
	engineConfiguration->clutchUpPinMode = PI_PULLUP;
	engineConfiguration->brakePedalPinMode = PI_PULLUP;
}

void setDefaultBasePins() {
#if EFI_PROD_CODE
	// call overrided board-specific serial configuration setup, if needed (for custom boards only)
	// needed also by bootloader code
	setPinConfigurationOverrides();
#endif /* EFI_PROD_CODE */

	// set UART pads configuration based on the board
// needed also by bootloader code
#ifdef TS_SECONDARY_UxART_PORT
	engineConfiguration->binarySerialTxPin = Gpio::C10;
	engineConfiguration->binarySerialRxPin = Gpio::C11;
#endif // TS_SECONDARY_UxART_PORT

	engineConfiguration->tunerStudioSerialSpeed = TS_DEFAULT_SPEED;
	engineConfiguration->uartConsoleSerialSpeed = 115200;
}

// needed also by bootloader code
// at the moment bootloader does NOT really need SD card, this is a step towards future bootloader SD card usage
void setDefaultSdCardParameters() {
	engineConfiguration->isSdCardEnabled = true;
}

static void setDefaultWarmupIdleCorrection() {
	initTemperatureCurve(CLT_MANUAL_IDLE_CORRECTION, 1.0);

	float baseIdle = 30;

	setCurveValue(CLT_MANUAL_IDLE_CORRECTION, -40, 1.5);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION, -30, 1.5);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION, -20, 40.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION, -10, 40.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION,   0, 40.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION,  10, 40.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION,  20, 40.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION,  30, 40.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION,  40, 40.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION,  50, 37.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION,  60, 35.0 / baseIdle);
	setCurveValue(CLT_MANUAL_IDLE_CORRECTION,  70, 33.0 / baseIdle);
}

/**
 * see also setTargetRpmCurve()
 */
static void setDefaultIdleSpeedTarget() {
	copyArray(config->cltIdleRpmBins, {  -30, - 20,  -10,    0,   10,   20,   30,   40,   50,  60,  70,  80,  90, 100 , 110,  120 });
	copyArray(config->cltIdleRpm,     { 1350, 1350, 1300, 1200, 1150, 1100, 1050, 1000, 1000, 950, 950, 930, 900, 900, 1000, 1100 });
}

/**
 * see also setDefaultIdleSpeedTarget()
 */
void setTargetRpmCurve(int rpm) {
	setLinearCurve(config->cltIdleRpmBins, CLT_CURVE_RANGE_FROM, 140, 10);
	setLinearCurve(config->cltIdleRpm, rpm, rpm, 10);
}

void setDefaultGppwmParameters() {
	// Same config for all channels
	for (size_t i = 0; i < efi::size(engineConfiguration->gppwm); i++) {
		auto& cfg = engineConfiguration->gppwm[i];
		chsnprintf(engineConfiguration->gpPwmNote[i], sizeof(engineConfiguration->gpPwmNote[0]), "GPPWM%d", i);

		// Set default axes
		cfg.loadAxis = GPPWM_Zero;
		cfg.rpmAxis = GPPWM_Rpm;

		cfg.pin = Gpio::Unassigned;
		cfg.dutyIfError = 0;
		cfg.onAboveDuty = 60;
		cfg.offBelowDuty = 50;
		cfg.pwmFrequency = 250;

		for (size_t j = 0; j < efi::size(cfg.loadBins); j++) {
			uint8_t z = j * 100 / (efi::size(cfg.loadBins) - 1);
			cfg.loadBins[j] = z;

			// Fill some values in the table
			for (size_t k = 0; k < efi::size(cfg.rpmBins); k++) {
				cfg.table[j][k] = z;
			}

		}

		for (size_t j = 0; j < efi::size(cfg.rpmBins); j++) {
			cfg.rpmBins[j] = 1000 * j;
		}
	}
}

static void setDefaultEngineNoiseTable() {
	setRpmTableBin(engineConfiguration->knockNoiseRpmBins);

	engineConfiguration->knockSamplingDuration = 45;

	setArrayValues(engineConfiguration->knockBaseNoise, -20);
}

/**
 * @brief	Global default engine configuration
 * This method sets the global engine configuration defaults. These default values are then
 * overridden by engine-specific defaults and the settings are saved in flash memory.
 *
 * This method is invoked only when new configuration is needed:
 *  * recently re-flashed chip
 *  * flash version of configuration failed CRC check or appears to be older then FLASH_DATA_VERSION
 *  * 'rewriteconfig' command
 *  * 'set engine_type X' command
 *
 * This method should only change the state of the configuration data structure but should NOT change the state of
 * anything else.
 *
 * This method should NOT be setting any default pinout
 */
static void setDefaultEngineConfiguration() {
#if EFI_ENGINE_CONTROL
#if (! EFI_UNIT_TEST)
	efi::clear(persistentState.persistentConfiguration);
#endif
	prepareVoidConfiguration(engineConfiguration);

	setDefaultBaseEngine();
	setDefaultFuel();
	setDefaultIgnition();
	setDefaultCranking();

	// VVT closed loop, totally random values!
	engineConfiguration->auxPid[0].pFactor = 2;
	engineConfiguration->auxPid[0].iFactor = 0.005;
	engineConfiguration->auxPid[0].dFactor = 0;
	engineConfiguration->auxPid[0].offset = 33;
	engineConfiguration->auxPid[0].minValue = 10;
	engineConfiguration->auxPid[0].maxValue = 90;

	engineConfiguration->vvtOutputFrequency[0] = 300; // VVT solenoid control

	engineConfiguration->isCylinderCleanupEnabled = true;

	engineConfiguration->auxPid[1].minValue = 10;
	engineConfiguration->auxPid[1].maxValue = 90;

	engineConfiguration->turboSpeedSensorMultiplier = 1;

#if EFI_IDLE_CONTROL
	setDefaultIdleParameters();
#endif /* EFI_IDLE_CONTROL */

#if EFI_ELECTRONIC_THROTTLE_BODY
	setDefaultEtbParameters();
	setDefaultEtbBiasCurve();
#endif /* EFI_ELECTRONIC_THROTTLE_BODY */
#if EFI_BOOST_CONTROL
    setDefaultBoostParameters();
#endif

    // OBD-II default rate is 500kbps
    engineConfiguration->canBaudRate = B500KBPS;
    engineConfiguration->can2BaudRate = B500KBPS;

	engineConfiguration->mafSensorType = Bosch0280218037;
	setBosch0280218037(config);

	engineConfiguration->canSleepPeriodMs = 50;
	engineConfiguration->canReadEnabled = true;
	engineConfiguration->canWriteEnabled = true;

	// Don't enable, but set default address
	engineConfiguration->verboseCanBaseAddress = CAN_DEFAULT_BASE;

	engineConfiguration->sdCardLogFrequency = 50;

	engineConfiguration->mapMinBufferLength = 1;
	engineConfiguration->vvtActivationDelayMs = 6000;
	
	engineConfiguration->startCrankingDuration = 3;

	engineConfiguration->maxAcRpm = 5000;
	engineConfiguration->maxAcClt = 100;
	engineConfiguration->maxAcTps = 75;

	initTemperatureCurve(IAT_FUEL_CORRECTION_CURVE, 1);

	engineConfiguration->auxPid[0].minValue = 10;
	engineConfiguration->auxPid[0].maxValue = 90;

	engineConfiguration->alternatorControl.minValue = 0;
	engineConfiguration->alternatorControl.maxValue = 90;

	setLinearCurve(config->scriptCurve1Bins, 0, 100, 1);
	setLinearCurve(config->scriptCurve1, 0, 100, 1);

	setLinearCurve(config->scriptCurve2Bins, 0, 100, /*precision*/1);
	setLinearCurve(config->scriptCurve2, 30, 170, 1);

	setLinearCurve(config->scriptCurve3Bins, 0, 100, 1);
	setLinearCurve(config->scriptCurve4Bins, 0, 100, 1);
	setLinearCurve(config->scriptCurve5Bins, 0, 100, 1);
	setLinearCurve(config->scriptCurve6Bins, 0, 100, 1);

	setLinearCurve(config->alsIgnRetardLoadBins, 2, 10, /*precision*/1);
	setRpmTableBin(config->alsIgnRetardrpmBins);
	setLinearCurve(config->alsFuelAdjustmentLoadBins, 2, 10, /*precision*/1);
	setRpmTableBin(config->alsFuelAdjustmentrpmBins);
	setLinearCurve(engineConfiguration->fuelLevelBins, 0, 5);

	setDefaultWarmupIdleCorrection();

	setLinearCurve(engineConfiguration->map.samplingAngleBins, 800, 7000, 1);
	setLinearCurve(engineConfiguration->map.samplingAngle, 100, 130, 1);
	setLinearCurve(engineConfiguration->map.samplingWindowBins, 800, 7000, 1);
	setLinearCurve(engineConfiguration->map.samplingWindow, 50, 50, 1);

	setLinearCurve(config->vvtTable1LoadBins, 20, 120, 10);
	setRpmTableBin(config->vvtTable1RpmBins);
	setLinearCurve(config->vvtTable2LoadBins, 20, 120, 10);
	setRpmTableBin(config->vvtTable2RpmBins);
	setLinearCurve(config->scriptTable1LoadBins, 20, 120, 10);
	setRpmTableBin(config->scriptTable1RpmBins);
	setLinearCurve(config->scriptTable2LoadBins, 20, 120, 10);
	setRpmTableBin(config->scriptTable2RpmBins);
	setLinearCurve(config->scriptTable3LoadBins, 20, 120, 10);
	setRpmTableBin(config->scriptTable3RpmBins);
	setLinearCurve(config->scriptTable4LoadBins, 20, 120, 10);
	setRpmTableBin(config->scriptTable4RpmBins);

	setDefaultEngineNoiseTable();

    // is this same old setCommonNTCSensor?
	engineConfiguration->clt.config = {0, 23.8889, 48.8889, 9500, 2100, 1000, 1500};

    setCommonNTCSensor(&engineConfiguration->iat, 2700);

	// wow unit tests have much cooler setDefaultLaunchParameters method
	engineConfiguration->launchRpm = 3000;
// 	engineConfiguration->launchTimingRetard = 10;
	engineConfiguration->launchTimingRpmRange = 500;
    engineConfiguration->launchSpeedThreshold = 30;
	engineConfiguration->hardCutRpmRange = 500;

	engineConfiguration->engineSnifferRpmThreshold = 2500;
	engineConfiguration->sensorSnifferRpmThreshold = 2500;

	engineConfiguration->noAccelAfterHardLimitPeriodSecs = 3;

	/**
	 * Idle control defaults
	 */
	setDefaultIdleSpeedTarget();
	//	setTargetRpmCurve(1200);

	engineConfiguration->idleRpmPid.pFactor = 0.05;
	engineConfiguration->idleRpmPid.iFactor = 0.002;

	engineConfiguration->idleRpmPid.minValue = -20;
	engineConfiguration->idleRpmPid.maxValue = 20;
	/**
	 * between variation between different sensor and weather and fabrication tolerance
	 * five percent looks like a safer default
	 */
	engineConfiguration->idlePidDeactivationTpsThreshold = 5;

	engineConfiguration->idle.solenoidFrequency = 200;
	// set idle_position 50
	engineConfiguration->manIdlePosition = 50;
//	engineConfiguration->idleMode = IM_AUTO;
	engineConfiguration->idleMode = IM_MANUAL;

	engineConfiguration->useStepperIdle = false;

	setLinearCurve(config->iacCoastingRpmBins, 0, 8000, 1);

	setDefaultGppwmParameters();

#if !EFI_UNIT_TEST
	engineConfiguration->analogInputDividerCoefficient = 2;
#endif

	// performance optimization
	engineConfiguration->sensorChartMode = SC_OFF;

	setTPS1Calibration(convertVoltageTo10bitADC(0),
			convertVoltageTo10bitADC(5),
			convertVoltageTo10bitADC(5),
			convertVoltageTo10bitADC(0));

	engineConfiguration->tps2Min = convertVoltageTo10bitADC(0);
	engineConfiguration->tps2Max = convertVoltageTo10bitADC(5);
	engineConfiguration->tps2SecondaryMin = convertVoltageTo10bitADC(5);
	engineConfiguration->tps2SecondaryMax = convertVoltageTo10bitADC(0);

	engineConfiguration->idlePositionMin = PACK_MULT_VOLTAGE * 0;
	engineConfiguration->idlePositionMax = PACK_MULT_VOLTAGE * 5;
	engineConfiguration->wastegatePositionMin = PACK_MULT_VOLTAGE * 0;
	engineConfiguration->wastegatePositionMax = PACK_MULT_VOLTAGE * 5;
	engineConfiguration->tpsErrorDetectionTooLow = -10; // -10% open
	engineConfiguration->tpsErrorDetectionTooHigh = 110; // 110% open

	engineConfiguration->oilPressure.v1 = 0.5f;
	engineConfiguration->oilPressure.v2 = 4.5f;
	engineConfiguration->oilPressure.value1 = 0;
	engineConfiguration->oilPressure.value2 = 689.476f;	// 100psi = 689.476kPa

	engineConfiguration->mapLowValueVoltage = 0;
	// todo: start using this for custom MAP
	engineConfiguration->mapHighValueVoltage = 5;

	engineConfiguration->cylinderBore = 87.5;

	setBoschHDEV_5_injectors();

	setEgoSensor(ES_14Point7_Free);

	engineConfiguration->globalFuelCorrection = 1;
	engineConfiguration->adcVcc = 3.0;

	engineConfiguration->map.sensor.type = MT_MPX4250;

	engineConfiguration->baroSensor.type = MT_CUSTOM;
	engineConfiguration->baroSensor.lowValue = 0;
	engineConfiguration->baroSensor.highValue = 500;

#if EFI_PROD_CODE
	engineConfiguration->engineChartSize = 300;
#else
	// need more events for automated test
	engineConfiguration->engineChartSize = 400;
#endif

	engineConfiguration->isMapAveragingEnabled = true;
	engineConfiguration->isWaveAnalyzerEnabled = true;

	engineConfiguration->acIdleRpmBump = 50;
	engineConfiguration->acDelay = 0.5;
	engineConfiguration->acIdleExtraOffset = 15;

	/* these two are used for HIP9011 only
	 * Currently this is offset from fire event, not TDC */
	/* TODO: convert to offset from TDC */
	engineConfiguration->knockDetectionWindowStart = 15.0 + 5.0;
	engineConfiguration->knockDetectionWindowEnd = 15.0 + 45.0;

	engineConfiguration->triggerSimulatorRpm = 1200;

	engineConfiguration->alternatorPwmFrequency = 300;

	engineConfiguration->isAlternatorControlEnabled = false;

	engineConfiguration->driveWheelRevPerKm = 1000;
	engineConfiguration->finalGearRatio = 1;
	engineConfiguration->vssGearRatio = 3.73;
	engineConfiguration->vssToothCount = 21;

	engineConfiguration->mapErrorDetectionTooLow = 5;
	// todo: default limits should be hard-coded for each sensor type
	// https://github.com/rusefi/rusefi/issues/4030
	engineConfiguration->mapErrorDetectionTooHigh = 410;

	setLinearCurve(config->throttleEstimateEffectiveAreaBins, 0, 100);

	engineConfiguration->hip9011Gain = 1;
#endif // EFI_ENGINE_CONTROL
    #include "default_script.lua"
}

#ifdef CONFIG_RESET_SWITCH_PORT
// this pin is not configurable at runtime so that we have a reliable way to reset configuration
#define SHOULD_IGNORE_FLASH() (palReadPad(CONFIG_RESET_SWITCH_PORT, CONFIG_RESET_SWITCH_PIN) == 0)
#else
#define SHOULD_IGNORE_FLASH() (false)
#endif // CONFIG_RESET_SWITCH_PORT

// by default, do not ignore config from flash! use it!
#ifndef IGNORE_FLASH_CONFIGURATION
#define IGNORE_FLASH_CONFIGURATION false
#endif

void loadConfiguration() {
#ifdef CONFIG_RESET_SWITCH_PORT
	// initialize the reset pin if necessary
	palSetPadMode(CONFIG_RESET_SWITCH_PORT, CONFIG_RESET_SWITCH_PIN, PAL_MODE_INPUT_PULLUP);
#endif /* CONFIG_RESET_SWITCH_PORT */

#if ! EFI_ACTIVE_CONFIGURATION_IN_FLASH
	// Clear the active configuration so that registered output pins (etc) detect the change on startup and init properly
	prepareVoidConfiguration(&activeConfiguration);
#endif /* EFI_ACTIVE_CONFIGURATION_IN_FLASH */

#if EFI_INTERNAL_FLASH
	if (SHOULD_IGNORE_FLASH() || IGNORE_FLASH_CONFIGURATION) {
		engineConfiguration->engineType = DEFAULT_ENGINE_TYPE;
		resetConfigurationExt(engineConfiguration->engineType);
		writeToFlashNow();
	} else {
		// this call reads configuration from flash memory or sets default configuration
		// if flash state does not look right.
		readFromFlash();
	}
#else // not EFI_INTERNAL_FLASH
	// This board doesn't load configuration, initialize the default
	engineConfiguration->engineType = DEFAULT_ENGINE_TYPE;
	resetConfigurationExt(engineConfiguration->engineType);
#endif /* EFI_INTERNAL_FLASH */

	// Force any board configuration options that humans shouldn't be able to change
	setBoardConfigOverrides();
}

void resetConfigurationExt(configuration_callback_t boardCallback, engine_type_e engineType) {
	enginePins.reset(); // that's mostly important for functional tests
	/**
	 * Let's apply global defaults first
	 */
	setDefaultEngineConfiguration();

	// set initial pin groups
	setDefaultBasePins();

	if (boardCallback != nullptr) {
		boardCallback(engineConfiguration);
	}

#if EFI_PROD_CODE
	// call overrided board-specific configuration setup, if needed (for custom boards only)
	setBoardDefaultConfiguration();
	setBoardConfigOverrides();
#endif

	engineConfiguration->engineType = engineType;

	/**
	 * And override them with engine-specific defaults
	 */
	switch (engineType) {
	case engine_type_e::MINIMAL_PINS:
		// all basic settings are already set in prepareVoidConfiguration(), no need to set anything here
		// nothing to do - we do it all in setBoardDefaultConfiguration
		break;
#if EFI_UNIT_TEST
	case engine_type_e::TEST_ISSUE_366_BOTH:
		setTestEngineIssue366both();
		break;
	case engine_type_e::TEST_ISSUE_366_RISE:
		setTestEngineIssue366rise();
		break;
#endif // EFI_UNIT_TEST
#if HW_MICRO_RUSEFI
	case engine_type_e::MRE_VW_B6:
		setMreVwPassatB6();
		break;
	case engine_type_e::MRE_M111:
		setMreM111EngineConfiguration();
		break;
	case engine_type_e::MRE_SECONDARY_CAN:
		mreSecondaryCan();
		break;
	case engine_type_e::MRE_SUBARU_EJ18:
		setSubaruEJ18_MRE();
		break;
	case engine_type_e::ME17_9_MISC:
	    setSlingshot();
		break;
	case engine_type_e::MRE_BOARD_NEW_TEST:
		mreBoardNewTest();
		break;
	case engine_type_e::BMW_M73_MRE:
	case engine_type_e::BMW_M73_MRE_SLAVE:
		setEngineBMW_M73_microRusEfi();
		break;
	case engine_type_e::MRE_MIATA_94_MAP:
		setMiata94_MAP_MRE();
		break;
	case engine_type_e::FUEL_BENCH:
	    setBodyControlUnit();
		break;
	case engine_type_e::MRE_MIATA_NA6_MAP:
		setMiataNA6_MAP_MRE();
		break;
	case engine_type_e::MRE_BODY_CONTROL:
		mreBCM();
		break;
#endif // HW_MICRO_RUSEFI
#if HW_PROTEUS
    case engine_type_e::PROTEUS_M111:
        setProteusM111EngineConfiguration();
        break;
    case engine_type_e::WASTEGATE_PROTEUS_TEST:
        proteusDcWastegateTest();
        break;
    case engine_type_e::PROTEUS_HYUNDAI_PB:
        setProteusHyundaiPb();
		break;
    case engine_type_e::PROTEUS_NISSAN_VQ35:
        setProteusNissanVQ();
		break;
	case engine_type_e::PROTEUS_GM_LS_4:
		setProteusGmLs4();
		break;
	case engine_type_e::PROTEUS_VW_B6:
		setProteusVwPassatB6();
		break;
	case engine_type_e::PROTEUS_QC_TEST_BOARD:
		proteusBoardTest();
		break;
	case engine_type_e::PROTEUS_LUA_DEMO:
		proteusLuaDemo();
		break;
	case engine_type_e::PROTEUS_HARLEY:
		proteusHarley();
		break;
	case engine_type_e::PROTEUS_BMW_M73:
		setEngineBMW_M73_Proteus();
		break;
	case engine_type_e::MIATA_PROTEUS_TCU:
		setMiataNB2_Proteus_TCU();
		break;
	case engine_type_e::PROTEUS_HONDA_K:
		setProteusHondaElement2003();
		break;
	case engine_type_e::PROTEUS_HONDA_OBD2A:
		setProteusHondaOBD2A();
		break;
	case engine_type_e::PROTEUS_E65_6H_MAN_IN_THE_MIDDLE:
		setEngineProteusGearboxManInTheMiddle();
		break;
	case engine_type_e::PROTEUS_N73:
	case engine_type_e::PROTEUS_MIATA_NB2:
		setMiataNB2_Proteus();
		break;
	case engine_type_e::PROTEUS_SBC:
	    setProteusSbc();
        break;
#ifdef HARDWARE_CI
	case engine_type_e::PROTEUS_ANALOG_PWM_TEST:
		setProteusAnalogPwmTest();
		break;
#endif // HARDWARE_CI
#endif // HW_PROTEUS
#if HW_HELLEN
	case engine_type_e::HELLEN_128_MERCEDES_4_CYL:
		setHellenMercedes128_4_cyl();
		break;
	case engine_type_e::HELLEN_128_MERCEDES_6_CYL:
		setHellenMercedes128_6_cyl();
		break;
	case engine_type_e::HELLEN_128_MERCEDES_8_CYL:
		setHellenMercedes128_8_cyl();
		break;
	case engine_type_e::HELLEN_NB2:
		setMiataNB2_Hellen72();
		break;
	case engine_type_e::HELLEN_NB2_36:
		setMiataNB2_Hellen72_36();
		break;
	case engine_type_e::HELLEN_NA8_96:
		setHellenMiata96();
		break;
	case engine_type_e::HELLEN_NB1:
		setHellenNB1();
		break;
	case engine_type_e::HELLEN_121_NISSAN_4_CYL:
		setHellen121nissanQR();
		break;
	case engine_type_e::HELLEN_121_NISSAN_6_CYL:
		setHellen121nissanVQ();
		break;
	case engine_type_e::HELLEN_121_VAG_5_CYL:
	    setHellen121Vag_5_cyl();
        break;
	case engine_type_e::HELLEN_121_VAG_V6_CYL:
	    setHellen121Vag_v6_cyl();
        break;
	case engine_type_e::HELLEN_121_VAG_VR6_CYL:
	    setHellen121Vag_vr6_cyl();
        break;
	case engine_type_e::HELLEN_121_VAG_8_CYL:
	    setHellen121Vag_8_cyl();
        break;
	case engine_type_e::HELLEN_121_VAG_4_CYL:
	case engine_type_e::HELLEN_154_VAG:
		break;
	case engine_type_e::HELLEN_154_HYUNDAI_COUPE_BK1:
		setGenesisCoupeBK1();
		break;
	case engine_type_e::HELLEN_154_HYUNDAI_COUPE_BK2:
		setGenesisCoupeBK2();
		break;
	case engine_type_e::HELLEN_NA6:
		setHellenNA6();
		break;
	case engine_type_e::HELLEN_NA94:
		setHellenNA94();
		break;
	case engine_type_e::ALPHAX_8CHAN_SBC:
	    set8chanSbc();
        break;
	case engine_type_e::HELLEN_HONDA_BCM:
	    setHondaCivicBcm();
        break;
#endif // HW_HELLEN
#if HW_FRANKENSO
	case engine_type_e::DEFAULT_FRANKENSO:
		setFrankensoConfiguration();
		break;
	case engine_type_e::DISCOVERY_PDM:
	case engine_type_e::TEST_ENGINE:
		setTestCamEngineConfiguration();
		break;
	case engine_type_e::TEST_CRANK_ENGINE:
		setTestCrankEngineConfiguration();
		break;
	case engine_type_e::FRANKENSO_QA_ENGINE:
		setFrankensoBoardTestConfiguration();
		break;
	case engine_type_e::FRANKENSO_BMW_M73_F:
		setBMW_M73_TwoCoilUnitTest();
		break;
	case engine_type_e::BMW_M73_M:
		setEngineBMW_M73_Manhattan();
		break;
	case engine_type_e::DODGE_NEON_1995:
		setDodgeNeon1995EngineConfiguration();
		break;
	case engine_type_e::DODGE_NEON_2003_CRANK:
		setDodgeNeonNGCEngineConfiguration();
		break;
	case engine_type_e::FORD_ASPIRE_1996:
		setFordAspireEngineConfiguration();
		break;
	case engine_type_e::NISSAN_PRIMERA:
		setNissanPrimeraEngineConfiguration();
		break;
	case engine_type_e::FRANKENSO_MIATA_NA6_MAP:
		setMiataNA6_MAP_Frankenso();
		break;
	case engine_type_e::ETB_BENCH_ENGINE:
		setEtbTestConfiguration();
		break;
	case engine_type_e::L9779_BENCH_ENGINE:
		setL9779TestConfiguration();
		break;
	case engine_type_e::EEPROM_BENCH_ENGINE:
#if EFI_PROD_CODE
		setEepromTestConfiguration();
#endif
		break;
	case engine_type_e::TLE8888_BENCH_ENGINE:
		setTle8888TestConfiguration();
		break;
	case engine_type_e::FRANKENSO_MAZDA_MIATA_NA8:
		setFrankensoMazdaMiataNA8Configuration();
		break;
	case engine_type_e::MITSUBISHI_3A92:
	    setMitsubishi3A92();
	    break;
	case engine_type_e::TOYOTA_1NZ_FE:
	    setToyota1NZFE();
	    break;
	case engine_type_e::MITSUBISHI_4G93:
	    setMitsubishi4G93();
	    break;
	case engine_type_e::FORD_INLINE_6_1995:
		setFordInline6();
		break;
	case engine_type_e::GY6_139QMB:
		setGy6139qmbDefaultEngineConfiguration();
		break;
	case engine_type_e::HONDA_600:
		setHonda600();
		break;
	case engine_type_e::FORD_ESCORT_GT:
		setFordEscortGt();
		break;
	case engine_type_e::MIATA_1996:
		setFrankensteinMiata1996();
		break;
	case engine_type_e::CITROEN_TU3JP:
		setCitroenBerlingoTU3JPConfiguration();
		break;
	case engine_type_e::SUBARU_2003_WRX:
		setSubaru2003Wrx();
		break;
	case engine_type_e::DODGE_RAM:
		setDodgeRam1996();
		break;
	case engine_type_e::VW_ABA:
		setVwAba();
		break;
	case engine_type_e::FRANKENSO_MAZDA_MIATA_2003:
		setMazdaMiata2003EngineConfiguration();
		break;
	case engine_type_e::MAZDA_MIATA_2003_NA_RAIL:
		setMazdaMiata2003EngineConfigurationNaFuelRail();
		break;
	case engine_type_e::MAZDA_MIATA_2003_BOARD_TEST:
		setMazdaMiata2003EngineConfigurationBoardTest();
		break;
	case engine_type_e::TEST_ENGINE_VVT:
		setTestVVTEngineConfiguration();
		break;
	case engine_type_e::TEST_DC_WASTEGATE_DISCOVERY:
		setTestDcWastegateConfiguration();
		break;
	case engine_type_e::SACHS:
		setSachs();
		break;
	case engine_type_e::CAMARO_4:
		setCamaro4();
		break;
	case engine_type_e::TOYOTA_2JZ_GTE_VVTi:
		setToyota_2jz_vics();
		break;
	case engine_type_e::TEST_33816:
		setTest33816EngineConfiguration();
		break;
	case engine_type_e::TEST_100:
	case engine_type_e::TEST_101:
	case engine_type_e::TEST_102:
	case engine_type_e::TEST_ROTARY:
		setRotary();
		break;
#endif // HW_FRANKENSO
#ifdef HW_SUBARU_EG33
	case engine_type_e::SUBARUEG33_DEFAULTS:
		setSubaruEG33Defaults();
		break;
#endif //HW_SUBARU_EG33
	default:
		firmwareError(ObdCode::CUSTOM_UNEXPECTED_ENGINE_TYPE, "Unexpected engine type: %d", engineType);
	}
	applyNonPersistentConfiguration();
}

void emptyCallbackWithConfiguration(engine_configuration_s * engineConfiguration) {
	UNUSED(engineConfiguration);
}

void resetConfigurationExt(engine_type_e engineType) {
	resetConfigurationExt(&emptyCallbackWithConfiguration, engineType);
}

void validateConfiguration() {
	if (engineConfiguration->adcVcc > 5.0f || engineConfiguration->adcVcc < 1.0f) {
		engineConfiguration->adcVcc = 3.0f;
	}
	engine->preCalculate();
}

void applyNonPersistentConfiguration() {
#if EFI_PROD_CODE
	efiAssertVoid(ObdCode::CUSTOM_APPLY_STACK, hasLotsOfRemainingStack(), "apply c");
	efiPrintf("applyNonPersistentConfiguration()");
#endif

#if EFI_ENGINE_CONTROL
	engine->updateTriggerWaveform();
#endif // EFI_ENGINE_CONTROL
}

void setTwoStrokeOperationMode() {
	engineConfiguration->twoStroke = true;
}

void setCamOperationMode() {
	engineConfiguration->skippedWheelOnCam = true;
}

void setCrankOperationMode() {
	engineConfiguration->skippedWheelOnCam = false;
}

void commonFrankensoAnalogInputs() {
	/**
	 * VBatt
	 */
	engineConfiguration->vbattAdcChannel = EFI_ADC_14;
}

// These symbols are weak so that a board_configuration.cpp file can override them
__attribute__((weak)) void setBoardDefaultConfiguration() { }
__attribute__((weak)) void setBoardConfigOverrides() { }

__attribute__((weak)) void onBoardStandBy() { }

__attribute__((weak)) int getBoardMetaOutputsCount() { return 0; }
__attribute__((weak)) Gpio* getBoardMetaOutputs() { return nullptr; }
