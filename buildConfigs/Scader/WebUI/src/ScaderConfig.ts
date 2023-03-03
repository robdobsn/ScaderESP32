
export class RelayConfig {
    name: string = "";
}

export class ShadesConfig {
    name: string = "";
}

export class DoorConfig {
    name: string = "";
}

export class ScaderConfig {
    NetMan: {
        EthEnabled: number
    } = {
        EthEnabled: 1
    }
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
        DoorOpenAngle: number,
        DoorClosedAngle: number,
        DoorRemainOpenTimeSecs: number,
        DoorTimeToOpenSecs: number,
        MotorOnTimeAfterMoveSecs: number,
        MaxMotorCurrentAmps: number,
    } = {
        enable: false,
        DoorOpenAngle: 150,
        DoorClosedAngle: 250,
        DoorRemainOpenTimeSecs: 30,
        DoorTimeToOpenSecs: 30,
        MotorOnTimeAfterMoveSecs: 30,
        MaxMotorCurrentAmps: 0.1,
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
    ScaderDoors: {
        enable: boolean,
        maxElems: number,
        name: string,
        elems: Array<DoorConfig>;
    } = {
        enable: false,
        maxElems: 2,
        name: "ScaderDoors",
        elems: [],
    }
    ScaderRFID: {
        enable: boolean,
        name: string,
    } = {
        enable: false,
        name: "ScaderRFID",
    }
}