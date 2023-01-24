
export class ScaderRelayState {
    name: string = "";
    state: number = 0;
}

export class ScaderRelayStates {
    module: string = "";
    elems: Array<ScaderRelayState> = [];
    constructor(obj: any) {
        if (obj) {
            if (obj.module)
                this.module = obj.module;
            this.elems = obj.elems;
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

export class ScaderDoorStates {
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
    isMoving: boolean = false;
    isOpen: boolean = false;
    inEnabled: boolean = false;
    outEnabled: boolean = false;
    isOverCurrent: boolean = false;
    inOutMode: string = "";
    kitButtonPressed: boolean = false;
    consButtonPressed: boolean = false;
    pirSenseInActive: boolean = false;
    pirSenseInTriggered: boolean = false;
    pirSenseOutActive: boolean = false;
    pirSenseOutTriggered: boolean = false;
    rotationAngleDegs: number = 0;
    stepperCurAngle: number = 0;
    timeBeforeCloseSecs: number = 0;
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
export class ScaderState {
    ScaderRelays: ScaderRelayStates = {} as ScaderRelayStates;
    ScaderShades: ScaderShadeStates = {} as ScaderShadeStates;
    ScaderDoors: ScaderDoorStates = {} as ScaderDoorStates;
    ScaderLEDPix: ScaderLEDPixStates = {} as ScaderLEDPixStates;
    ScaderOpener: ScaderOpenerStates = {} as ScaderOpenerStates;
}

export type ScaderStateType = 
    ScaderRelayStates | ScaderShadeStates | ScaderDoorStates | ScaderLEDPixStates | ScaderOpenerStates;
    