
export class ScaderRelayState {
    name: string = "";
    state: number = 0;
}

export class ScaderRelayStates {
    elems: Array<ScaderRelayState> = [];
    constructor(obj: any) {
        if (obj) {
            this.elems = obj.elems;
        }
    }
}

export class ScaderShadeState {
    name: string = "";
    state: number = 0;
}

export class ScaderShadeStates {
    elems: Array<ScaderShadeState> = [];
    constructor(obj: any) {
        if (obj) {
            this.elems = obj.elems;
        }
    }
}

export class ScaderDoorState {
    name: string = "";
    locked: string = 'K';
    open: string = 'K';
    closed?: string = 'K';
    toLockMs?: number = 0;
}

export class ScaderDoorStates {
    elems: Array<ScaderDoorState> = [];
    bell: string = 'K';
    constructor(obj: any) {
        if (obj) {
            this.elems = obj.elems;
            this.bell = obj.bell;
        }
    }
}
export class ScaderState {
    ScaderRelays: ScaderRelayStates = {} as ScaderRelayStates;
    ScaderShades: ScaderShadeStates = {} as ScaderShadeStates;
    ScaderDoors: ScaderDoorStates = {} as ScaderDoorStates;
}
