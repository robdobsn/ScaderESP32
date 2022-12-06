
export class ScaderRelayState {
    name: string = "";
    state: number = 0;
}

export class ScaderRelayStates {
    elems: Array<ScaderRelayState> = [];
}

export class ScaderShadeState {
    name: string = "";
    state: number = 0;
}

export class ScaderShadeStates {
    elems: Array<ScaderShadeState> = [];
}
export class ScaderState {
    ScaderRelays: ScaderRelayStates = {} as ScaderRelayStates;
    ScaderShades: ScaderShadeStates = {} as ScaderShadeStates;
}
