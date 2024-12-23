
export class ScaderRelayState {
    name: string = "";
    state: number = 0;
}

export class ScaderRelayStates {
    module: string = "";
    mainsHz?: number = 0;
    elems: Array<ScaderRelayState> = [];
    pulseCount?: number = 0;
    constructor(obj: any) {
        if (obj) {
            if (obj.module)
                this.module = obj.module;
            this.elems = obj.elems;
            if (obj.pulseCount)
                this.pulseCount = obj.pulseCount;
        }
    }
}

export class ScaderShadeState {
    name: string = "";
    state: number = 0;
}

export class ScaderShadeStates {
    module: string = "";
    elems: Array<ScaderShadeState> = [];
    lux: Array<number> = [];
    constructor(obj: any) {
        if (obj) {
            if (obj.module)
                this.module = obj.module;
            this.elems = obj.elems;
            if (obj.lux)
                this.lux = obj.lux;
        }
    }
}

export class ScaderLEDPixStates {
    module: string = "";
}

export class ScaderDoorState {
    name: string = "";
    locked: string = 'K';
    open: string = 'K';
    closed?: string = 'K';
    toLockMs?: number = 0;
}

export class ScaderLockStates {
    module: string = "";
    elems: Array<ScaderDoorState> = [];
    bell: string = 'K';
    constructor(obj: any) {
        if (obj) {
            if (obj.module)
                this.module = obj.module;
            this.elems = obj.elems;
            this.bell = obj.bell;
        }
    }
}

export class ScaderOpenStatus {
    motorActive: boolean = false;
    inEnabled: boolean = false;
    outEnabled: boolean = false;
    inOutMode: string = "";
    kitButtonPressed: boolean = false;
    consButtonPressed: boolean = false;
    pirSenseInActive: boolean = false;
    pirSenseOutActive: boolean = false;
    doorOpenAngleDegs: number = 0;
    doorClosedAngleDegs: number = 0;
    doorCurAngle: number = 0;
    rawForceN: number = 0;
    measuredForceN: number = 0;
    forceOffsetN: number = 0;
    forceThresholdN: number = 0;
    timeBeforeCloseSecs: number = 0;
    doorStateCode: number = 0;
    doorStateStr: string = "";
}
export class ScaderOpenerStates {
    module: string = "";
    status: ScaderOpenStatus = {} as ScaderOpenStatus;
    constructor(obj: any) {
        if (obj) {
            if (obj.module)
                this.module = obj.module;
            this.status = obj.status;
        }
    }
}

export class ScaderPulseCounterStates {
    module: string = "";
    pulseCount?: number = 0;
    constructor(obj: any) {
        if (obj) {
            if (obj.module)
                this.module = obj.module;
            if (obj.pulseCount)
                this.pulseCount = obj.pulseCount;
        }
    }
}

export class ScaderElecMeterState {
    name: string = "";
    rmsCurrentA: number = 0;
    rmsPowerW: number = 0;
    totalKWh: number = 0;
}

export class ScaderElecMeterStates {
    module: string = "";
    elems: Array<ScaderElecMeterState> = [];
    constructor(obj: any) {
        if (obj) {
            if (obj.module)
                this.module = obj.module;
            this.elems = obj.elems;
        }
    }
}

export class ScaderState {
    ScaderRelays: ScaderRelayStates = {} as ScaderRelayStates;
    ScaderShades: ScaderShadeStates = {} as ScaderShadeStates;
    ScaderLocks: ScaderLockStates = {} as ScaderLockStates;
    ScaderLEDPix: ScaderLEDPixStates = {} as ScaderLEDPixStates;
    ScaderOpener: ScaderOpenerStates = {} as ScaderOpenerStates;
    ScaderPulseCounter: ScaderPulseCounterStates = {} as ScaderPulseCounterStates;
    ScaderElecMeters: ScaderElecMeterStates = {} as ScaderElecMeterStates;
}

export type ScaderStateType = 
    ScaderRelayStates | ScaderShadeStates | ScaderLockStates | ScaderLEDPixStates | ScaderOpenerStates | ScaderPulseCounterStates | ScaderElecMeterStates;
