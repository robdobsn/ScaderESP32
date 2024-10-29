
export class RelayConfig {
    name: string = "";
    isDimmable?: boolean = false;
}

export class ElecMeterConfig {
    name: string = "";
    calibADCToAmps: number = 0.05;
}

export class ShadesConfig {
    name: string = "";
}

export class DoorConfig {
    name: string = "";
}

export class PulseCounterConfig {
    name: string = "";
}

export class ScaderConfig {
    ScaderCommon: {
        name: string,
        hostname: string
    } = {
        name: "Scader",
        hostname: "scader"
    }
    ScaderRelays: {
        enable: boolean,
        maxElems: number,
        name: string,
        elems: Array<RelayConfig>;
        relays?: Array<RelayConfig>;
    } = {
        enable: false,
        maxElems: 24,
        name: "ScaderRelays",
        elems: []
    }
    ScaderCat: {
        enable: boolean
    } = {
        enable: false,
    }
    ScaderOpener: {
        enable: boolean,
        DoorOpenAngleDegs: number,
        DoorClosedAngleDegs: number,
        ForceOffsetN: number,
        ForceThresholdN: number,
        DoorRemainOpenTimeSecs: number,
        DoorTimeToOpenSecs: number,
        MotorOnTimeAfterMoveSecs: number,
        MaxMotorCurrentAmps: number
        // Old properties - here to allow removal with delete
        doorClosedAngleOffsetDegrees?: number,
        doorSwingAngleDegrees?: number,
        DoorClosedAngle?: number,
        DoorOpenAngle?: number,
    } = {
        enable: false,
        DoorOpenAngleDegs: 150,
        DoorClosedAngleDegs: 250,
        ForceOffsetN: 0,
        ForceThresholdN: 5,
        DoorRemainOpenTimeSecs: 30,
        DoorTimeToOpenSecs: 30,
        MotorOnTimeAfterMoveSecs: 30,
        MaxMotorCurrentAmps: 0.1
    }
    ScaderLEDPix: {
        enable: boolean
    } = {
        enable: false,
    }
    ScaderShades: {
        enable: boolean,
        enableLightLevels: boolean,
        maxElems: number,
        name: string,
        elems: Array<ShadesConfig>
    } = {
        enable: false,
        enableLightLevels: false,
        maxElems: 5,
        name: "ScaderShades",
        elems: [],
    }
    ScaderLocks: {
        enable: boolean,
        maxElems: number,
        name: string,
        elems: Array<DoorConfig>;
    } = {
        enable: false,
        maxElems: 2,
        name: "ScaderLocks",
        elems: [],
    }
    ScaderRFID: {
        enable: boolean,
        name: string,
    } = {
        enable: false,
        name: "ScaderRFID",
    }
    ScaderPulseCounter: {
        enable: boolean,
        name: string,
    } = {
        enable: false,
        name: "ScaderPulseCounter",
    }
    ScaderElecMeters: {
        enable: boolean,
        maxElems: number,
        name: string,
        mainsVoltage: number,
        elems: Array<ElecMeterConfig>;
    } = {
        enable: false,
        maxElems: 4,
        name: "ScaderElecMeters",
        mainsVoltage: 240,
        elems: [],
    }
}
